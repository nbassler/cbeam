# cbeam

[![build](https://github.com/nbassler/cbeam/actions/workflows/build.yml/badge.svg)](https://github.com/nbassler/cbeam/actions/workflows/build.yml)
[![release](https://img.shields.io/github/v/tag/nbassler/cbeam?label=release&sort=semver)](https://github.com/nbassler/cbeam/tags)
[![license](https://img.shields.io/github/license/nbassler/cbeam)](LICENSE)

Qt controller for a linear actuator — a stepper motor on a rail.

## Hardware

An [OpenBuilds C-Beam linear actuator](https://openbuilds.com), driven from a
Raspberry Pi over two GPIO lines: one pulse, one direction. Standard build is a
Tr8×8-4p leadscrew (4 start, 2 mm pitch, 8 mm of travel per revolution) with a
1.8° stepper at 200 full steps per revolution.

The app is meant to be built and run **on the Pi**, reached over `ssh -X`. There
is no client/server split.

The target is a **Raspberry Pi 4 Model B**, which settles how step pulses will
eventually be generated: `pigpio` clocks a pulse train out over DMA, immune to
scheduler jitter, and takes a per-pulse delay list — so the acceleration ramp
becomes part of the waveform rather than something a timer has to chase. That
option exists only up to the Pi 4; the Pi 5's RP1 moved the GPIO block and
pigpio does not work there.

## Build

```sh
rm -rf build/; cmake -S . -B build
cmake --build build
./build/cbeam
ctest --test-dir build --output-on-failure
```

Needs CMake ≥ 3.16 and **either Qt 6 or Qt 5** (Core, Gui, Widgets, Test):

```sh
sudo apt install cmake make g++ qt6-base-dev    # Debian 12+ / Pi OS Bookworm
sudo apt install cmake make g++ qtbase5-dev     # Debian 11  / Pi OS Bullseye
```

Qt 5 is supported because Pi OS Bullseye packages no Qt 6 at all — not an old
version, none — and reflashing a headless rig that is in use is not always an
option. Nothing in the sources is Qt-6-only.

On a 1 GB Pi, build with `-j2` rather than `--parallel`: g++ on Qt sources can
take several hundred MB per translation unit, and four at once will run the
machine out of memory.

| Option | Default | |
| --- | --- | --- |
| `CBEAM_BACKEND` | `sim` | `sim` simulates motion and runs anywhere. `gpio` would drive the pins in `src/config.h`, and is **not implemented** — configuring with it fails on purpose rather than silently building something that cannot move a rail. |
| `CBEAM_BUILD_TESTS` | `ON` | Set `OFF` to skip the test target. |
| `CBEAM_QT_VERSION` | `auto` | `auto` prefers Qt 6 and falls back to Qt 5. Force with `5` or `6`. The configure output names the version actually used — worth a glance on a machine that has both. |

The running backend is named in the status bar, so it is never a mystery
whether the window in front of you is driving real hardware.

## Packages

Tagging a release (`v*`) builds an ARM64 `.deb` for each Pi OS generation and
attaches them to the GitHub release:

| Asset | For | Links against |
| --- | --- | --- |
| `cbeam_X.Y.Z_arm64~bullseye.deb` | Pi OS 11 | Qt 5 |
| `cbeam_X.Y.Z_arm64~bookworm.deb` | Pi OS 12 | Qt 6 |
| `cbeam_X.Y.Z_arm64~trixie.deb` | Pi OS 13 | Qt 6 |

```sh
sudo apt install ./cbeam_0.0.3_arm64~bullseye.deb
```

Use `apt install ./file.deb` rather than `dpkg -i`, so Qt gets pulled in
automatically. The packages are **not** interchangeable: each is built inside a
container of its own Debian release, against that release's glibc and Qt.

The Qt dependency is never written down anywhere. `dpkg-shlibdeps` derives it
from the linked binary, so the same `CMakeLists.txt` produces a package
depending on `libqt5widgets5` on Bullseye and `libqt6widgets6` on Bookworm
without being told which is which.

To build one by hand:

```sh
cmake --build build && (cd build && cpack -G DEB)
```

That needs `dpkg-dev` for `dpkg-shlibdeps`.

## Version

The window title carries the version, resolved from `git describe`:

| Title shows | Means |
| --- | --- |
| `CBeam Controller v0.0.1` | exactly a tagged release, tree clean |
| `CBeam Controller v0.0.1+ga46ac93` | built past the tag |
| `CBeam Controller v0.0.1+ga46ac93.dirty` | uncommitted changes in the tree |
| `CBeam Controller 0.0.0+ga46ac93` | no tag reachable |
| `CBeam Controller unknown` | not a git checkout, or git unavailable |

`cmake/GitVersion.cmake` regenerates `version.h` on **every build**, not at
configure time, so the title cannot go stale after a commit. Regenerating is
free when nothing changed — `configure_file` leaves the file alone if the
content is identical, so it does not force a relink.

The header lands in the build tree and is never committed. To cut a release,
tag it; the next build picks the name up on its own:

```sh
git tag -a v0.0.2 -m "..."
```

If `ssh -X` gives you a blank or refused window, force the X11 backend —
Qt will otherwise try Wayland:

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

Travel follows a trapezoidal speed profile: it eases from `CB_RATE_START` up
to `CB_RATE_CRUISE` over `CB_RAMP_MS`, cruises, then sheds speed again into the
target. Moves too short to reach cruise get a triangular profile instead. The
deceleration falls out of one term in `Model::tick()` — speed is capped at
`sqrt(start² + 2·a·distance)`, the fastest you could still be going and stop in
what is left.

Two reasons it is there. A stepper asked to start at cruise can stall or lose
steps against the carriage's inertia; and a rail that slams to a halt at a
limit is alarming to stand next to, whereas one visibly slowing over its last
second reads as deliberate. `Stop` decelerates the same way rather than
cutting instantly — for a real emergency, cut the motor supply.

Sub-step remainders are carried between ticks rather than rounded away, so a
ramped move still lands on exactly the step it was asked for. There are tests
for that.

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
short on purpose. Run them in parallel — it cuts the wall time by about two
thirds, since the motion cases spend most of it waiting:

```sh
ctest --test-dir build -j8            # all of them
ctest --test-dir build -R Ramps       # one, by name (case sensitive)
ctest --test-dir build -N             # list without running
```

Each QTest slot is registered as its own ctest case, so a failure names itself
instead of pointing at the whole binary. `tests/CMakeLists.txt` discovers the
list by reading the source, and a change to `test_model.cpp` triggers a
reconfigure — a new test function registers itself with no CMake edit, and one
cannot silently go unregistered.

## CI

`.github/workflows/build.yml` builds and tests on x86, and produces ARM64
binaries for Pi OS as downloadable artifacts.

The ARM jobs are **not** cross-compiled. They run on GitHub's arm64 runners,
free for public repositories, inside a Debian container matching the Pi OS
generation — `bookworm` and `trixie` are both built, so there is a binary ready
whichever the Pi turns out to be. The container matters: a binary linked
against the runner's own newer glibc will not load on Pi OS.

Two things the workflow has to get right, both easy to trip over:

- `fetch-depth: 0`, or there are no tags and every binary claims to be
  `0.0.0+g…`.
- git is installed **before** `actions/checkout`. Without it, checkout falls
  back to a tarball download, leaving no `.git` at all and stamping the
  version `unknown`.

## Style

Stock LLVM style — clang-format's own default. `.clang-format` sets only
`BasedOnStyle: LLVM` and the language standard, deliberately: every line added
there would be a house rule someone has to learn, and the point of a canonical
style is that it is the same everywhere.

```sh
cmake --build build --target format      # reformat in place
clang-format --dry-run --Werror src/*.cpp src/*.h tests/*.cpp   # just check
```

CI checks it on every push, from a `debian:trixie` container rather than the
runner image. clang-format's output shifts a little between major versions, so
an unpinned check would start failing on its own schedule when the runner
updates. Trixie ships clang-format 19, which is what a Debian 13 desktop has
too — local and CI agree by construction.

This replaced a 250-setting `uncrustify.cfg` that nothing ran: the tree had
drifted 14% of its lines away from it. A style config only stays true if
something checks it.

## Calibration

Currently **248 steps/cm** (24.8 steps/mm, 0.040323 mm/step), in
[src/config.h](src/config.h) as `CB_STEPS_PER_CM`. Full 400 mm travel is
therefore 9920 steps, matching `src_rpi/test2.py`.

This was measured, and it disagrees with the theory by 0.8%. The measurement
wins for now, but the gap is unexplained.

The theory: a C-Beam's Tr8×8-4p leadscrew moves 8 mm per revolution, and a 1.8°
stepper takes 200 steps per revolution, so 200 ÷ 8 = 25 steps/mm = **250**
steps/cm exactly. The measurement: driving ~30 cm at 250 reproducibly
overshoots, which is what a true rate below 250 looks like — 7500 steps at an
actual 248 steps/cm travels 30.24 cm, 2.4 mm long. That matches, so 248 stands.

What is odd is the *size* of the gap. 0.8% is roughly fifty times the lead
error of a rolled ACME screw, so manufacturing tolerance does not cover it, and
no standard lead or microstep setting lands on 24.8 steps/mm.

### Settling it

A measurement at one distance cannot separate a scale error from a fixed
offset: 2.4 mm of backlash or datum error looks exactly like 0.8% over 30 cm.
Measuring several distances does separate them. Always approach from the same
direction, so backlash is not in play:

| Steps commanded | Nominal at 250 | If the rate is really 248 |
| ---: | ---: | ---: |
| 2500 | 100 mm | 100.8 mm |
| 5000 | 200 mm | 201.6 mm |
| 7500 | 300 mm | 302.4 mm |

If the error grows in proportion — roughly 0.8, 1.6, 2.4 mm — the rate really
is 248. If it is the *same* few mm at every distance, the rate is 250 and
something is adding a constant offset. Either way it is worth 3.2 mm over full
travel.

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

## License

MIT — see [LICENSE](LICENSE).
