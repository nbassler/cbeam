#include "pigpiodriver.h"
#include "config.h"

#include <pigpiod_if2.h>

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {
// Enough headroom for a few ticks in flight without letting the daemon's
// waveform storage fill up if something stalls.
constexpr std::size_t kMaxQueuedWaves = 8;
} // namespace

PigpioDriver::PigpioDriver() {
  // Honour the usual pigpio environment variables so a daemon on another host
  // or port can be used without recompiling.
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

  if (cb_pin_connected(CB_PIN_ENDSTOP_LO))
    set_mode(m_pi, CB_PIN_ENDSTOP_LO, PI_INPUT);

  if (cb_pin_connected(CB_PIN_ENDSTOP_HI))
    set_mode(m_pi, CB_PIN_ENDSTOP_HI, PI_INPUT);

  // Anything left over from a previous run would otherwise still be queued.
  wave_clear(m_pi);

  m_name = "pigpio (GPIO " + std::to_string(CB_PIN_PULSE) + "/" +
           std::to_string(CB_PIN_DIR) + ")";
}

PigpioDriver::~PigpioDriver() {
  if (m_pi < 0)
    return;

  wave_tx_stop(m_pi);
  wave_clear(m_pi);
  gpio_write(m_pi, CB_PIN_PULSE, 0);
  pigpio_stop(m_pi);
}

const char *PigpioDriver::name() const { return m_name.c_str(); }

// An end stop that is engaged blocks travel further into it, but never travel
// away from it -- otherwise a triggered switch would strand the carriage.
bool PigpioDriver::endstopEngaged(int direction) const {
  const int pin = direction < 0 ? CB_PIN_ENDSTOP_LO : CB_PIN_ENDSTOP_HI;

  if (!cb_pin_connected(pin))
    return false;

  const int level = gpio_read(m_pi, pin);

  if (level < 0)
    return false; // unreadable; do not invent a stop

  return (level != 0) == CB_ENDSTOP_ACTIVE_HIGH;
}

void PigpioDriver::releaseFinishedWaves() {
  const int transmitting = wave_tx_at(m_pi);

  while (!m_waves.empty() && m_waves.front() != transmitting) {
    wave_delete(m_pi, m_waves.front());
    m_waves.pop_front();
  }
}

int PigpioDriver::step(int steps) {
  if (steps == 0)
    return 0;

  const int direction = steps > 0 ? 1 : -1;
  const int count = std::abs(steps);

  if (endstopEngaged(direction))
    return 0; // short count: Model halts and re-targets to where we are

  // test3.py's convention, kept: low drives positive, high drives negative.
  gpio_write(m_pi, CB_PIN_DIR, direction < 0 ? 1 : 0);

  releaseFinishedWaves();

  if (m_waves.size() >= kMaxQueuedWaves)
    return 0; // daemon is behind; let Model stop rather than pile up

  // Spread this tick's steps evenly across the tick. Model varies `steps` to
  // shape the ramp, so period follows from it and no rate is duplicated here.
  const unsigned totalUs = static_cast<unsigned>(CB_TICK_MS) * 1000u;
  const unsigned periodUs = std::max(totalUs / static_cast<unsigned>(count),
                                     static_cast<unsigned>(CB_PULSE_US) * 2u);
  const unsigned highUs = CB_PULSE_US;
  const unsigned lowUs = periodUs - highUs;

  std::vector<gpioPulse_t> pulses;
  pulses.reserve(static_cast<std::size_t>(count) * 2);

  for (int i = 0; i < count; i++) {
    pulses.push_back({1u << CB_PIN_PULSE, 0u, highUs});
    pulses.push_back({0u, 1u << CB_PIN_PULSE, lowUs});
  }

  wave_add_new(m_pi);

  if (wave_add_generic(m_pi, static_cast<int>(pulses.size()), pulses.data()) <
      0)
    return 0;

  const int wave = wave_create(m_pi);

  if (wave < 0)
    return 0;

  // ONE_SHOT_SYNC appends to whatever is already playing instead of cutting it
  // off, so consecutive ticks join without a seam in the pulse train.
  if (wave_send_using_mode(m_pi, wave, PI_WAVE_MODE_ONE_SHOT_SYNC) < 0) {
    wave_delete(m_pi, wave);
    return 0;
  }

  m_waves.push_back(wave);

  // Reported as done immediately: the pulses are queued in the daemon and will
  // play out on their own. Model's position therefore leads the carriage by up
  // to one tick, which is the price of not blocking the GUI thread for the
  // duration of every burst.
  return steps;
}
