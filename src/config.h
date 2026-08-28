#ifndef CB_CONFIG_H
#define CB_CONFIG_H

// Hardware and motion configuration for the cbeam linear actuator.
//
// Steps are the single source of truth throughout this program.  Millimetres
// are a display convenience only and are never stored anywhere.

// Travel limits in millimetres.  Converted to steps once, at startup.
constexpr double CB_LLIM_MM = 0.0;
constexpr double CB_ULIM_MM = 400.0;

// Calibration of the rail, in steps per centimetre. Kept in the form it was
// measured in and divided down here rather than stored pre-divided, so the
// number in this file is the one that can be checked against the hardware.
//
// 248 is measured, and it disagrees with the theory by 0.8%. The measurement
// wins for now, but the gap has no explanation yet.
//
// The rig is an OpenBuilds C-Beam actuator: a Tr8*8-4p leadscrew (4 start,
// 2 mm pitch, so 8 mm of travel per revolution) turned by a 1.8 degree
// stepper at 200 full steps per revolution. That gives
//
//     200 steps/rev / 8 mm/rev = 25 steps/mm = 250 steps/cm = 0.0400 mm/step
//
// exactly. But driving ~30 cm at 250 reproducibly overshoots, which is what a
// true rate below 250 looks like: 7500 steps at an actual 248 steps/cm travels
// 30.24 cm, i.e. 2.4 mm long. That matches, and it is why this is 248.
//
// What has no explanation is the size of the gap. 0.8% is roughly fifty times
// the lead error of a rolled ACME screw, so "manufacturing tolerance" does not
// cover it, and no standard lead or microstep setting lands on 24.8 steps/mm.
// Something systematic is unaccounted for.
//
// A single-distance measurement cannot tell a scale error from a fixed offset
// -- 2.4 mm of backlash or datum error at one end looks identical to 0.8% over
// 30 cm. Measuring several distances from the same approach direction
// separates them: an error growing in proportion means the rate really is 248,
// while a constant error means the rate is 250 and something adds a fixed
// offset. See README.md.
constexpr double CB_STEPS_PER_CM = 248.0;
constexpr double CB_MM_PER_STEP = 10.0 / CB_STEPS_PER_CM;

// Decimal places used for every millimetre readout and every mm input widget.
// All of them must agree, otherwise a value written back from the model gets
// re-quantised by the widget it lands in.
constexpr int CB_MM_DIGITS = 3;

// Motion profile, in steps per second.
//
// Travel eases in and out rather than switching between stopped and full
// speed. Two reasons: a stepper asked to start at cruise rate can stall or
// lose steps against the inertia of the carriage, and a rail that slams to a
// halt at a limit is alarming to stand next to. Seeing it slow down over the
// last second before its target reads as deliberate rather than runaway.
//
// Note that the ramp time sets a ramp *distance*: at these rates a 1000 ms
// ramp spends 275 steps getting up to speed and another 275 slowing down, so
// moves shorter than 550 steps (about 22 mm) never reach cruise at all and
// come out triangular.
constexpr double CB_RATE_CRUISE = 500.0; // matches the 2 ms pulse in test3.py
constexpr double CB_RATE_START = 50.0;   // rate at both ends of a move
constexpr int CB_RAMP_MS = 1000;         // time from start rate up to cruise

// Acceleration follows from the rates and the ramp time, in steps/s^2.
constexpr double CB_ACCEL =
    (CB_RATE_CRUISE - CB_RATE_START) / (CB_RAMP_MS / 1000.0);

// How often the simulated carriage is advanced and the display refreshed.
//
// This paces the *view*, not the motor. A Qt timer is nowhere near accurate
// enough to time step pulses -- see the note in stepdriver.h.
constexpr int CB_TICK_MS = 20;

// Starting increments for the three jog button columns, in steps. Each column
// is retunable at runtime, which is the point: set one to the increment a
// measurement needs and then click it repeatedly without retyping anything.
// At 248 steps/cm the defaults are roughly 0.04, 0.4 and 4 mm.
constexpr int CB_JOG_COLUMNS = 3;
constexpr int CB_JOG_DEFAULTS[3] = {1, 10, 100};

// A jog of zero does nothing, and a negative one would flip the sign printed
// on the button, so the boxes floor at a single step. The ceiling only keeps
// the widget sane -- the model clamps every target to the travel limits
// regardless of how large an increment it is handed.
constexpr int CB_JOG_MIN = 1;
constexpr int CB_JOG_MAX = 9999;

// Whether the limits and the Zero button start out locked against stray
// clicks. Locked by default: unlocking is a deliberate act, and losing the
// travel window mid-experiment is worse than one extra click.
constexpr bool CB_LOCK_LIMITS_AT_STARTUP = true;

// ---------------------------------------------------------------------------
// GPIO pin assignments
//
// BCM numbering, matching gpiozero's default in src_rpi/*.py.  Nothing in the
// Qt app drives these yet -- motion is simulated -- but this is the one place
// the numbers should live once it does.
//
// A pin of CB_PIN_NC is not connected.  Deliberately an invalid BCM number so
// that using one unchecked fails loudly rather than toggling some innocent
// bystander pin.
constexpr int CB_PIN_NC = -1;

inline constexpr bool cb_pin_connected(int pin) { return pin >= 0; }

// Outputs to the stepper driver.  These two are live on the rail today and
// match src_rpi/test3.py.
constexpr int CB_PIN_PULSE = 17; // one rising edge per step
constexpr int CB_PIN_DIR = 27;   // low = positive travel, high = negative

// Inputs -- all three unwired at present.
//
// End-stop switches, for protection: these must be able to abort a move
// regardless of where the software thinks the carriage is.
constexpr int CB_PIN_ENDSTOP_LO = CB_PIN_NC;
constexpr int CB_PIN_ENDSTOP_HI = CB_PIN_NC;

// External trigger, to start a move from outside the GUI.
constexpr int CB_PIN_TRIGGER = CB_PIN_NC;

// Direction line polarity. false matches src_rpi/test3.py: the line is driven
// low to travel positive and high to travel negative. Flip this if a positive
// jog moves the carriage the wrong way -- which way round it ends up is a
// matter of how the motor is wired, not something that can be known here.
constexpr bool CB_DIR_INVERT = false;

// Input polarity: true means the line reads high when the switch is engaged
// or the trigger is asserted.  Unverified against the hardware -- normally
// closed end stops would want false here, which is also the safer wiring
// because a severed cable then reads as "engaged".  Measure before trusting.
constexpr bool CB_ENDSTOP_ACTIVE_HIGH = true;
constexpr bool CB_TRIGGER_ACTIVE_HIGH = true;

// Width of the high part of a step pulse, in microseconds. Stepper drivers
// generally want at least 2.5 us; 10 us is comfortably above that and still
// leaves the line low for the rest of the period even at full speed.
constexpr unsigned CB_PULSE_US = 10;

// Debounce window for the switch and trigger inputs.
constexpr int CB_INPUT_DEBOUNCE_MS = 20;

#endif // CB_CONFIG_H
