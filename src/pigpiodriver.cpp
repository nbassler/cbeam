#include "pigpiodriver.h"
#include "config.h"

#include <pigpiod_if2.h>

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <vector>

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

  abort();
  wave_clear(m_pi);
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

// Only waves that have already played may be deleted. Their DMA control
// blocks are live until then, and freeing a wave that is still queued lets the
// daemon reuse the slot underneath a transmission that has not happened yet.
void PigpioDriver::releaseWave() {
  if (m_wave >= 0) {
    wave_delete(m_pi, m_wave);
    m_wave = -1;
  }
}

void PigpioDriver::abort() {
  if (m_pi < 0)
    return;

  wave_tx_stop(m_pi);
  releaseWave();
  gpio_write(m_pi, CB_PIN_PULSE, 0);
}

StepOutcome PigpioDriver::step(int steps) {
  if (steps == 0)
    return {0, StepResult::Done};

  // Nothing is ever queued behind what is playing. If the previous burst is
  // still going, emit nothing and let Model offer the same steps again next
  // tick.
  //
  // Queuing was the original design and it was wrong twice over. It let the
  // daemon recycle waveform ids underneath transmissions that had not
  // happened yet, which duplicated pulses and drove the carriage far past
  // where it was asked to go. And it meant Stop did not stop: pulses banked
  // in the daemon play out regardless of what the GUI thinks, even into a
  // controller that has since been powered down.
  if (wave_tx_busy(m_pi) != 0)
    return {0, StepResult::Busy};

  releaseWave(); // the previous burst has finished; its slot can go back

  const int direction = steps > 0 ? 1 : -1;
  const int count = std::abs(steps);

  if (endstopEngaged(direction))
    return {0, StepResult::Blocked};

  // test3.py's convention by default: low drives positive, high negative.
  // CB_DIR_INVERT swaps it without touching this file.
  const bool negative = (direction < 0) != CB_DIR_INVERT;

  gpio_write(m_pi, CB_PIN_DIR, negative ? 1 : 0);

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
    return {0, StepResult::Blocked};

  const int wave = wave_create(m_pi);

  if (wave < 0)
    return {0, StepResult::Blocked};

  // Plain one-shot, not a synchronised chain: by construction nothing else is
  // playing, so there is nothing to chain onto.
  if (wave_send_once(m_pi, wave) < 0) {
    wave_delete(m_pi, wave);
    return {0, StepResult::Blocked};
  }

  m_wave = wave;
  return {steps, StepResult::Done};
}
