#ifndef CB_CONFIG_H
#define CB_CONFIG_H

// Hardware and motion configuration for the cbeam linear actuator.
//
// Steps are the single source of truth throughout this program.  Millimetres
// are a display convenience only and are never stored anywhere.

// Travel limits in millimetres.  Converted to steps once, at startup.
constexpr double CB_LLIM_MM = 0.0;
constexpr double CB_ULIM_MM = 400.0;

// Measured calibration of the rail: 248 steps per centimetre, i.e. 24.8
// steps/mm, i.e. 0.040323 mm/step.  Kept in the form it was measured in and
// divided down here, rather than stored pre-divided, so the number in this
// file is the one that can be checked against the hardware.
//
// This agrees with src_rpi/test2.py (9920 steps / 400 mm) and confirms the
// 400 mm travel: 400 mm * 24.8 = 9920 steps.
//
// The Qt app previously used 0.040 mm/step (25 steps/mm), which was wrong by
// 0.8% -- 3.2 mm over full travel.
constexpr double CB_STEPS_PER_CM = 248.0;
constexpr double CB_MM_PER_STEP  = 10.0 / CB_STEPS_PER_CM;

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
// last half second before its target reads as deliberate rather than runaway.
constexpr double CB_RATE_CRUISE = 500.0; // matches the 2 ms pulse in test3.py
constexpr double CB_RATE_START  = 50.0;  // rate at both ends of a move
constexpr int    CB_RAMP_MS     = 500;   // time from start rate up to cruise

// Acceleration follows from the rates and the ramp time, in steps/s^2.
constexpr double CB_ACCEL = (CB_RATE_CRUISE - CB_RATE_START)
                            / (CB_RAMP_MS / 1000.0);

// How often the simulated carriage is advanced and the display refreshed.
//
// This paces the *view*, not the motor. A Qt timer is nowhere near accurate
// enough to time step pulses -- see the note in stepdriver.h.
constexpr int CB_TICK_MS = 20;

// Starting increments for the three jog button columns, in steps. Each column
// is retunable at runtime, which is the point: set one to the increment a
// measurement needs and then click it repeatedly without retyping anything.
// At 248 steps/cm the defaults are roughly 0.04, 0.4 and 4 mm.
constexpr int CB_JOG_COLUMNS     = 3;
constexpr int CB_JOG_DEFAULTS[3] = { 1, 10, 100 };

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

inline constexpr bool cb_pin_connected(int pin)
{
    return pin >= 0;
}

// Outputs to the stepper driver.  These two are live on the rail today and
// match src_rpi/test3.py.
constexpr int CB_PIN_PULSE = 17; // one rising edge per step
constexpr int CB_PIN_DIR   = 27; // low = positive travel, high = negative

// Inputs -- all three unwired at present.
//
// End-stop switches, for protection: these must be able to abort a move
// regardless of where the software thinks the carriage is.
constexpr int CB_PIN_ENDSTOP_LO = CB_PIN_NC;
constexpr int CB_PIN_ENDSTOP_HI = CB_PIN_NC;

// External trigger, to start a move from outside the GUI.
constexpr int CB_PIN_TRIGGER = CB_PIN_NC;

// Input polarity: true means the line reads high when the switch is engaged
// or the trigger is asserted.  Unverified against the hardware -- normally
// closed end stops would want false here, which is also the safer wiring
// because a severed cable then reads as "engaged".  Measure before trusting.
constexpr bool CB_ENDSTOP_ACTIVE_HIGH = true;
constexpr bool CB_TRIGGER_ACTIVE_HIGH = true;

// Debounce window for the switch and trigger inputs.
constexpr int CB_INPUT_DEBOUNCE_MS = 20;

#endif // CB_CONFIG_H
