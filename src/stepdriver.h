#ifndef STEPDRIVER_H
#define STEPDRIVER_H

// The seam between the position model and whatever actually moves the rail.
//
// Model deals only in step counts and never touches hardware itself, so the
// same model code runs against the simulation on a desktop and against the
// GPIO pins on the Pi.  Chosen at build time by the CBEAM_BACKEND option in
// CMakeLists.txt.
// What came of a request to move.
enum class StepResult {
  Done,    // the steps were emitted
  Busy,    // the hardware is still working on the last lot; ask again later
  Blocked, // it will not go further this way -- an end stop, most likely
};

struct StepOutcome {
  int taken = 0;
  StepResult result = StepResult::Done;
};

class StepDriver {
public:
  virtual ~StepDriver() = default;

  // Advance the carriage by `steps`, signed; negative travels toward the
  // lower limit.
  //
  // Returns how many steps were actually emitted and why, if that is fewer
  // than asked. Busy and Blocked both mean "nothing happened", but they need
  // opposite responses: Busy is ordinary back-pressure and the move should
  // continue, while Blocked means give up and re-target to where the carriage
  // really is, rather than carrying on with a position the hardware does not
  // share.
  virtual StepOutcome step(int steps) = 0;

  // Abandon anything still in flight, immediately. Called when travel ends,
  // so nothing can still be playing out after the GUI says it has stopped.
  virtual void abort() {}

  // Shown in the status bar, so it is never a mystery whether what is on
  // screen is driving a real rail.
  virtual const char *name() const = 0;

  // A NOTE FOR WHOEVER WRITES THE GPIO BACKEND
  //
  // Model calls step() from a QTimer, which is fine for animating a
  // simulated carriage but must never be what times real pulses. A Qt timer
  // rides on the Linux scheduler: its jitter is milliseconds, against a step
  // period of two, and that irregularity is audible in the motor and rough
  // on the mechanism.
  //
  // So a hardware driver should not emit one pulse per call. It should hand
  // the whole run of `steps` to something with hardware timing behind it --
  // DMA, PIO, or an outboard microcontroller -- and return when that is
  // done. At which point the speed profile in Model::tick() becomes the
  // wrong place for the ramp too, since the driver is generating the pulse
  // train and only it knows the true timing. Expect this interface to grow
  // an asynchronous form when that happens.
};

// Motion with nothing behind it: every step always succeeds.
class SimDriver : public StepDriver {
public:
  StepOutcome step(int steps) override { return {steps, StepResult::Done}; }

  const char *name() const override { return "simulation"; }
};

#endif // STEPDRIVER_H
