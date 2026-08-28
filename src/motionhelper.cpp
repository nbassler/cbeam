#include "motionhelper.h"
#include "config.h"

#include <pigpiod_if2.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {
constexpr unsigned CB_HELPER_CHUNK_US = 50000;
constexpr int CB_HELPER_MAX_STEPS = 128;

bool isMoving(MotionState state) {
  return state == MotionState::Moving || state == MotionState::Stopping;
}

bool negativeDirection(int direction) {
  return (direction < 0) != CB_DIR_INVERT;
}
} // namespace

MotionHelper::MotionHelper() {
  const char *addr = std::getenv("PIGPIO_ADDR");
  const char *port = std::getenv("PIGPIO_PORT");

  m_pi = pigpio_start(addr, port);

  if (m_pi < 0)
    throw std::runtime_error(
        "cannot reach pigpiod. Start it with 'sudo systemctl start pigpiod' "
        "(or 'sudo pigpiod'), and check it is enabled at boot.");

  set_mode(m_pi, CB_PIN_PULSE, PI_OUTPUT);
  set_mode(m_pi, CB_PIN_DIR, PI_OUTPUT);
  gpio_write(m_pi, CB_PIN_PULSE, 0);
  gpio_write(m_pi, CB_PIN_DIR, 0);
  wave_clear(m_pi);

  m_thread = std::thread(&MotionHelper::plannerLoop, this);
}

MotionHelper::~MotionHelper() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_exit = true;
    m_cv.notify_all();
  }

  if (m_thread.joinable())
    m_thread.join();

  if (m_pi < 0)
    return;

  std::lock_guard<std::mutex> lock(m_mutex);
  wave_tx_stop(m_pi);
  clearQueuedLocked();
  wave_clear(m_pi);
  pigpio_stop(m_pi);
}

bool MotionHelper::moveAbs(int target, std::string &error) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state == MotionState::Fault) {
    error = m_detail.empty() ? "fault" : m_detail;
    return false;
  }

  if (!m_positionKnown || m_state == MotionState::Estopped) {
    error = "position_unknown";
    return false;
  }

  if (isMoving(m_state)) {
    error = "busy";
    return false;
  }

  m_target = target;
  m_detail.clear();
  m_rate = CB_RATE_START;
  m_planRate = CB_RATE_START;
  m_state = (m_target == m_position) ? MotionState::Idle : MotionState::Moving;
  m_cv.notify_all();
  return true;
}

bool MotionHelper::stop(std::string &error) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state == MotionState::Fault) {
    error = m_detail.empty() ? "fault" : m_detail;
    return false;
  }

  if (!isMoving(m_state))
    return true;

  const int planned = plannedPositionLocked();
  const int delta = planned - m_position;
  const int direction =
      delta == 0 ? (m_target >= m_position ? 1 : -1) : (delta > 0 ? 1 : -1);
  const double speed = std::max(CB_RATE_START, m_planRate);
  const double braking =
      (speed * speed - CB_RATE_START * CB_RATE_START) / (2.0 * CB_ACCEL);

  m_target = planned + direction * static_cast<int>(std::ceil(braking));
  m_state = MotionState::Stopping;
  m_detail.clear();
  m_cv.notify_all();
  return true;
}

bool MotionHelper::estop(std::string &error) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state == MotionState::Fault) {
    error = m_detail.empty() ? "fault" : m_detail;
    return false;
  }

  wave_tx_stop(m_pi);
  clearQueuedLocked();
  m_target = m_position;
  m_positionKnown = false;
  m_state = MotionState::Estopped;
  m_detail.clear();
  m_rate = CB_RATE_START;
  m_planRate = CB_RATE_START;
  m_cv.notify_all();
  return true;
}

bool MotionHelper::zeroHere(std::string &error) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (isMoving(m_state)) {
    error = "busy";
    return false;
  }

  if (m_state == MotionState::Fault) {
    error = m_detail.empty() ? "fault" : m_detail;
    return false;
  }

  m_position = 0;
  m_target = 0;
  m_positionKnown = true;
  m_state = MotionState::Idle;
  m_detail.clear();
  m_rate = CB_RATE_START;
  m_planRate = CB_RATE_START;
  return true;
}

DriverStatus MotionHelper::status() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return {m_position, m_target, m_state, m_positionKnown, m_detail};
}

void MotionHelper::plannerLoop() {
  std::unique_lock<std::mutex> lock(m_mutex);

  while (!m_exit) {
    retireFinishedLocked();

    if (isMoving(m_state) && m_positionKnown && m_queued.id < 0)
      queueNextWaveLocked();

    if (isMoving(m_state) && plannedPositionLocked() == m_target &&
        m_playing.id < 0 && m_queued.id < 0) {
      m_state = MotionState::Idle;
      m_rate = CB_RATE_START;
      m_planRate = CB_RATE_START;
    }

    m_cv.wait_for(lock, std::chrono::milliseconds(5));
  }
}

void MotionHelper::retireFinishedLocked() {
  if (m_playing.id < 0)
    return;

  if (wave_tx_busy(m_pi) == 0) {
    m_position += signedSteps(m_playing);
    m_rate = m_playing.endRate;
    wave_delete(m_pi, m_playing.id);
    m_playing = {};

    if (m_queued.id >= 0) {
      m_position += signedSteps(m_queued);
      m_rate = m_queued.endRate;
      wave_delete(m_pi, m_queued.id);
      m_queued = {};
    }

    m_planRate = m_rate;
    return;
  }

  if (m_queued.id >= 0 && wave_tx_at(m_pi) == m_queued.id) {
    m_position += signedSteps(m_playing);
    m_rate = m_playing.endRate;
    wave_delete(m_pi, m_playing.id);
    m_playing = m_queued;
    m_queued = {};
  }
}

bool MotionHelper::queueNextWaveLocked() {
  const int remaining = m_target - plannedPositionLocked();

  if (remaining == 0)
    return true;

  const int direction = remaining > 0 ? 1 : -1;

  if (m_playing.id < 0)
    gpio_write(m_pi, CB_PIN_DIR, negativeDirection(direction) ? 1 : 0);

  std::vector<gpioPulse_t> pulses;
  pulses.reserve(CB_HELPER_MAX_STEPS * 2);

  double rate = m_planRate;
  unsigned spanUs = 0;
  int count = 0;

  while (count < std::abs(remaining) && count < CB_HELPER_MAX_STEPS) {
    const int afterStep = std::abs(remaining) - (count + 1);
    const double accelerating = std::sqrt(rate * rate + 2.0 * CB_ACCEL);
    const double braking =
        std::sqrt(CB_RATE_START * CB_RATE_START + 2.0 * CB_ACCEL * afterStep);

    rate = std::max(CB_RATE_START,
                    std::min({CB_RATE_CRUISE, accelerating, braking}));

    const unsigned periodUs =
        std::max<unsigned>(static_cast<unsigned>(std::lround(1000000.0 / rate)),
                           CB_PULSE_MIN_US * 2u);
    const unsigned highUs = std::max(periodUs / 2u, CB_PULSE_MIN_US);
    const unsigned lowUs = periodUs - highUs;

    pulses.push_back({1u << CB_PIN_PULSE, 0u, highUs});
    pulses.push_back({0u, 1u << CB_PIN_PULSE, lowUs});

    spanUs += periodUs;
    count++;

    if (spanUs >= CB_HELPER_CHUNK_US)
      break;
  }

  if (count == 0)
    return true;

  wave_add_new(m_pi);

  if (wave_add_generic(m_pi, static_cast<int>(pulses.size()), pulses.data()) <
      0) {
    setFaultLocked("wave_add_failed");
    return false;
  }

  const int wave = wave_create(m_pi);

  if (wave < 0) {
    setFaultLocked("wave_create_failed");
    return false;
  }

  const int sent =
      m_playing.id < 0
          ? wave_send_once(m_pi, wave)
          : wave_send_using_mode(m_pi, wave, PI_WAVE_MODE_ONE_SHOT_SYNC);

  if (sent < 0) {
    wave_delete(m_pi, wave);
    setFaultLocked("wave_send_failed");
    return false;
  }

  const WaveSlot slot{wave, count, direction, rate};

  if (m_playing.id < 0)
    m_playing = slot;
  else
    m_queued = slot;

  m_planRate = rate;
  return true;
}

void MotionHelper::clearQueuedLocked() {
  if (m_queued.id >= 0) {
    wave_delete(m_pi, m_queued.id);
    m_queued = {};
  }

  if (m_playing.id >= 0) {
    wave_delete(m_pi, m_playing.id);
    m_playing = {};
  }
}

void MotionHelper::setFaultLocked(const std::string &detail) {
  wave_tx_stop(m_pi);
  clearQueuedLocked();
  m_target = m_position;
  m_positionKnown = false;
  m_state = MotionState::Fault;
  m_detail = detail;
  m_rate = CB_RATE_START;
  m_planRate = CB_RATE_START;
}

int MotionHelper::plannedPositionLocked() const {
  return m_position + signedSteps(m_playing) + signedSteps(m_queued);
}

int MotionHelper::signedSteps(const WaveSlot &slot) const {
  return slot.id < 0 ? 0 : slot.direction * slot.steps;
}
