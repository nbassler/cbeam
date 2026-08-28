# cbeam

Qt 6 controller for a linear actuator — a stepper motor on a rail.

## Build

```sh
rm -rf build/; cmake -S . -B build
cmake --build build
./build/cbeam
ctest --test-dir build --output-on-failure
```

Needs Qt 6 (Core, Gui, Widgets, Test) and CMake ≥ 3.16. All of those are in
`qt6-base-dev`; there is no extra package for the tests.

| Option | Default | |
| --- | --- | --- |
| `CBEAM_BACKEND` | `sim` | `sim` simulates motion and runs anywhere. `gpio` would drive the pins in `src/config.h`, and is **not implemented** — configuring with it fails on purpose rather than silently building something that cannot move a rail. |
| `CBEAM_BUILD_TESTS` | `ON` | Set `OFF` to skip the test target. |

The running backend is named in the status bar, so it is never a mystery
whether the window in front of you is driving real hardware.

The app is meant to be built and run **on the Raspberry Pi**, reached over
`ssh -X` when it is being used. There is no client/server split. Build deps on
Pi OS:

```sh
sudo apt install cmake g++ qt6-base-dev
```

If `ssh -X` gives you a blank or refused window, force the X11 backend —
Qt 6 will otherwise try Wayland:

```sh
QT_QPA_PLATFORM=xcb ./build/cbeam
```

## Steps are the unit

Every position in [src/model.h](src/model.h) is an integer step count.
Millimetres are computed on the way out to the GUI and converted back to steps
exactly once, when the user commits an mm widget. No mm value is stored
anywhere.

This is deliberate. Storing millimetres, or converting steps → mm → steps
around the signal graph, loses a fraction of a step on every round trip, and
moving back and forth then fails to return to the position it started from.
With steps as the only state, that class of drift cannot occur.

The consequence for the GUI: whatever you type into an mm box is snapped to the
nearest reachable step and the box immediately redraws itself with that step's
exact value. Programmatic widget updates in
[src/mainwindow.cpp](src/mainwindow.cpp) are wrapped in a `QSignalBlocker`,
which is what stops the slider and the spin box from echoing each other.

## Motion

Travel is **simulated** — nothing touches the GPIO yet. `Model::tick()`
advances the carriage off a `QTimer` at the rate `src_rpi/test3.py` pulses
(500 steps/s), rather than looping, so the GUI stays responsive and `Stop`
can abort a move in progress.

`Model` never touches hardware itself. It drives a `StepDriver`
([src/stepdriver.h](src/stepdriver.h)), of which `SimDriver` is the only
implementation today. A driver's `step()` returns *how many steps it actually
took*, which is the hook the end-stop switches need: a short count means the
driver stopped early, and the model then halts and re-targets to where the
hardware really is instead of where it wanted to be. The tests exercise that
path with a fake end stop, so a GPIO backend has something to conform to
before any wiring exists.

## Tests

[tests/test_model.cpp](tests/test_model.cpp) guards the no-drift property. The
load-bearing case walks every one of the 9920 reachable steps, renders it at
the display precision and parses it back, asserting the step survives intact.
The rest cover the specific bugs this rewrite fixed — the clobbered 400 mm
ceiling, `zero()` shifting by the wrong variable, jogs not accumulating, and
truncated rather than rounded limits.

Travel runs in real time, so tests that move the carriage keep the distance
short on purpose.

## Calibration

Measured: **248 steps/cm** (24.8 steps/mm, 0.040323 mm/step), in
[src/config.h](src/config.h) as `CB_STEPS_PER_CM`. Full 400 mm travel is
therefore 9920 steps, which agrees with `src_rpi/test2.py`.

The mm readouts use 3 decimals. That is deliberate and load-bearing: step
spacing is 0.0403 mm, so 0.001 mm display resolution distinguishes every one of
the 9920 reachable steps with room to spare. Coarsening `CB_MM_DIGITS` to 2
would make adjacent steps share an mm string and reintroduce round-trip error.

The calibration is shown on the right of the status bar, generated from those
constants rather than written out, so it cannot go stale after a recalibration.

One caveat still open in that file: the end-stop and external-trigger pins are
`CB_PIN_NC`, and their polarity constants are guesses. Measure before wiring.

## Lock limits

The limit boxes and `Zero` start out locked (`CB_LOCK_LIMITS_AT_STARTUP`) and
are greyed until the tickbox is cleared. `Zero` sits in that group because it
redefines the travel window just as surely as editing a limit does — a stray
click silently renames every position on the rail, and that is not something to
discover halfway through a measurement.

## Jog columns

The three jog columns are retunable. Each has a spin box setting how many
**steps** a click travels, with the millimetre equivalent shown underneath;
the button faces follow whatever the box is set to. Defaults are 1, 10 and 100
steps (`CB_JOG_DEFAULTS`), roughly 0.04, 0.4 and 4 mm.

The point is repetition: dial a column to the increment a measurement needs,
then click it as many times as required without retyping a position. Steps are
the unit here too, so a hundred clicks of the same button land exactly a
hundred increments away.

The increment boxes stay live during travel — changing one only affects what
the next click does.

## src_rpi/

Standalone Python scripts that do drive the hardware; not used by the Qt app
and not maintained here. `test3.py` is the one in current use.

Note that its `stpcm = 1`, so its `dist` argument is really a **raw step
count**, not a distance: `python3 test3.py -50` means 50 steps in the negative
direction. Its `steps = int(...)` truncates, so any caller passing a
non-integer accumulates error move over move — pass integer step deltas only.

The scripts depend on `Adafruit_GPIO` / `Adafruit_SSD1306`, which are Python 2
era and will not install on current Pi OS. `luma.oled` or
`adafruit-circuitpython-ssd1306` are the live replacements.
