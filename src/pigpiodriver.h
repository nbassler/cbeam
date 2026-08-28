#ifndef PIGPIODRIVER_H
#define PIGPIODRIVER_H

#include <string>

#include "stepdriver.h"

// Drives the stepper through pigpio, talking to the pigpiod daemon.
//
// Why pigpio and not libgpiod or plain writes: pulses are emitted as a DMA
// waveform, so their timing is set by hardware rather than by when Linux next
// schedules us. Toggling a pin from userspace at 500 Hz has millisecond
// jitter against a 2 ms period, which is audible in the motor and hard on the
// mechanism. This costs nothing extra to do properly.
//
// Why the daemon (pigpiod_if2) and not the in-process pigpio library: the
// latter needs root, and running a GUI as root over ssh -X is its own misery.
// pigpiod runs as root; this connects to it as an ordinary user.
//
// The rate is not set here. Model hands over however many steps belong in one
// tick, and those are spread evenly across the tick -- so the acceleration
// ramp arrives already encoded in the step count, and this class stays a dumb
// pulse emitter.
class PigpioDriver : public StepDriver {
public:
  // Throws std::runtime_error if pigpiod is not reachable, so a failure to
  // reach the hardware is loud rather than a rail that silently does nothing.
  PigpioDriver();
  ~PigpioDriver() override;

  PigpioDriver(const PigpioDriver &) = delete;
  PigpioDriver &operator=(const PigpioDriver &) = delete;

  StepOutcome step(int steps) override;
  void abort() override;
  const char *name() const override;

private:
  bool endstopEngaged(int direction) const;
  void retireFinished();

  int m_pi = -1;

  // Exactly two waveform slots: the one playing, and at most one chained
  // behind it. The second exists because a waveform lasts one tick, so
  // without something already queued the motor stands still in the gap
  // between a burst ending and the next timer tick delivering the next --
  // a 50 Hz stutter that is audible and loses steps at speed.
  //
  // This is bounded queuing, unlike the unbounded version it replaced: never
  // more than one tick of pulses is banked, and abort() calls wave_tx_stop,
  // which discards it immediately. Stop still stops.
  int m_playing = -1;
  int m_queued = -1;
  std::string m_name;
};

#endif // PIGPIODRIVER_H
