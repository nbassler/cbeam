// Regression tests for the position model.
//
// The point of most of these is a single property: positions are stored as
// integer steps, so no amount of going back and forth through the millimetre
// display can move the carriage off the step it started on. That drift is the
// bug this model was rewritten to make impossible, and these are here so it
// stays impossible.

#include "config.h"
#include "model.h"
#include "stepdriver.h"

#include <QtTest>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {
// Stand in for the GUI's mm widget: the model emits a millimetre value,
// the widget renders it at CB_MM_DIGITS decimals, and the user commits
// that rounded value back. This is the round trip that used to lose steps.
double widgetQuantise(double mm) {
  const double scale = std::pow(10.0, CB_MM_DIGITS);

  return std::round(mm * scale) / scale;
}

// A driver that refuses to travel past a given step, standing in for an
// end stop until the GPIO backend exists.
class EndstopDriver : public StepDriver {
public:
  explicit EndstopDriver(int limit) : m_limit(limit) {}

  StepOutcome step(int steps) override {
    const int landed = std::min(m_at + steps, m_limit);
    const int taken = landed - m_at;

    m_at = landed;
    return {taken, taken == steps ? StepResult::Done : StepResult::Blocked};
  }

  const char *name() const override { return "end stop test"; }

private:
  int m_at = 0;
  int m_limit;
};

// Refuses every other request with Busy, as the real driver does while a
// waveform is still playing. Busy must cost nothing: no steps lost, no steps
// invented, and the move must still finish.
class BusyDriver : public StepDriver {
public:
  StepOutcome step(int steps) override {
    m_calls++;

    if (m_calls % 2 == 0)
      return {0, StepResult::Busy};

    m_emitted += std::abs(steps);
    return {steps, StepResult::Done};
  }

  const char *name() const override { return "busy test"; }

  int emitted() const { return m_emitted; }

private:
  int m_calls = 0;
  int m_emitted = 0;
};
} // namespace

class TestModel : public QObject {
  Q_OBJECT

private slots:

  void mmRoundTripReturnsToStart();
  void selfEchoHoldsStill();
  void everyStepSurvivesTheMmDisplay();
  void jogsAccumulateWhileMoving();
  void limitsRoundRatherThanTruncate();
  void targetsClampToTravel();
  void zeroShiftsWindowByPosition();
  void startupLimitsComeFromConfig();
  void endstopHaltsTravel();
  void travelRampsUpAndDown();
  void shortMoveStillLandsExactly();
  void stopDeceleratesRatherThanJumping();
  void busyDriverLosesNoSteps();
};

namespace {
// Start a move, recording the number of steps taken on each tick -- the
// speed profile, sampled.
//
// `strides` and `last` belong to the caller on purpose. Travel continues
// after this returns, so anything the lambda touches has to outlive the
// call; owning them here and handing back a copy would leave the
// connection writing into a dead stack frame.
void startProfiledMove(Model &m, int targetSteps, QList<int> &strides,
                       int &last) {
  last = m.stepCurrent();

  QObject::connect(&m, &Model::positionChanged,
                   [&strides, &last](int steps, double) {
                     strides << std::abs(steps - last);
                     last = steps;
                   });

  m.setTargetSteps(targetSteps);
  m.go();
}
} // namespace

// Type a value, press enter, type the value back: many times over, at
// distances that do not divide evenly into steps.
void TestModel::mmRoundTripReturnsToStart() {
  Model m;
  double shown = 0.0;

  connect(&m, &Model::targetChanged,
          [&shown](int, double mm) { shown = widgetQuantise(mm); });

  const double hops[] = {3.33, 7.77, 0.017, 41.6667, 1.0 / 3.0};

  for (int rep = 0; rep < 200; rep++)
    for (double h : hops) {
      m.setTargetMm(shown + h);
      m.setTargetMm(shown - h);
    }

  QCOMPARE(m.stepTarget(), 0);
}

// Feed the model its own displayed value over and over. A design that stored
// millimetres creeps here; one that stores steps cannot.
void TestModel::selfEchoHoldsStill() {
  Model m;
  double shown = 0.0;

  connect(&m, &Model::targetChanged,
          [&shown](int, double mm) { shown = widgetQuantise(mm); });

  m.setTargetMm(123.456); // primes `shown` with the quantised readback
  const int settled = m.stepTarget();

  for (int i = 0; i < 10000; i++)
    m.setTargetMm(shown);

  QCOMPARE(m.stepTarget(), settled);
}

// The load-bearing one. Every reachable step has to survive being rendered at
// CB_MM_DIGITS decimals and parsed back. If the display were too coarse, two
// steps would share an mm string and a round trip could land on the wrong one.
void TestModel::everyStepSurvivesTheMmDisplay() {
  const int lo = Model::stepsFromMm(CB_LLIM_MM);
  const int hi = Model::stepsFromMm(CB_ULIM_MM);

  for (int s = lo; s <= hi; s++) {
    const int back = Model::stepsFromMm(widgetQuantise(Model::mmFromSteps(s)));

    if (back != s)
      QFAIL(qPrintable(QString("step %1 renders as %2 mm, which reads "
                               "back as step %3")
                           .arg(s)
                           .arg(Model::mmFromSteps(s), 0, 'f', CB_MM_DIGITS)
                           .arg(back)));
  }
}

// A jog issued while an earlier one is still running must add to it, not
// replace it, or a quick double click on +1 travels a single step.
void TestModel::jogsAccumulateWhileMoving() {
  // Deliberately literal rather than CB_JOG_DEFAULTS: how far the GUI's
  // buttons happen to be set is a view concern, and retuning a jog column
  // should not be able to break this test.
  const int forward = 10;
  const int back = 1;

  Model m;

  for (int i = 0; i < 500; i++) {
    m.jog(+forward);
    m.jog(-back);
  }

  QCOMPARE(m.stepTarget(), 500 * (forward - back));
}

void TestModel::limitsRoundRatherThanTruncate() {
  Model m;

  m.setUlimMm(100.0);
  QCOMPARE(m.stepUlim(), Model::stepsFromMm(100.0));
  QVERIFY(std::abs(Model::mmFromSteps(m.stepUlim()) - 100.0) <=
          CB_MM_PER_STEP / 2.0);
}

void TestModel::targetsClampToTravel() {
  Model m;

  m.setTargetMm(9999.0);
  QCOMPARE(m.stepTarget(), m.stepUlim());

  m.setTargetMm(-9999.0);
  QCOMPARE(m.stepTarget(), m.stepLlim());
}

// Zero declares the present position to be zero. Limits are unchanged: they
// represent the physical travel range and are set manually by the user.
void TestModel::zeroShiftsWindowByPosition() {
  Model m;
  const int span = m.stepUlim() - m.stepLlim();

    // Kept short deliberately: travel runs in real time at CB_STEPS_PER_TICK
  // per CB_TICK_MS, and what is under test here is the arithmetic of zero(),
  // which does not care how far the carriage came.
  m.setTargetMm(2.0);
  m.go();
  QTRY_VERIFY_WITH_TIMEOUT(!m.isMoving(), 10000);

  const int arrived = m.stepCurrent();
  QCOMPARE(arrived, Model::stepsFromMm(2.0));

  const int llimBefore = m.stepLlim();
  const int ulimBefore = m.stepUlim();

  m.zero();

  // Position resets; limits are not touched (they represent the physical
  // travel range and are set manually by the user).
  QCOMPARE(m.stepCurrent(), 0);
  QCOMPARE(m.stepLlim(), llimBefore);
  QCOMPARE(m.stepUlim(), ulimBefore);
  QCOMPARE(m.stepUlim() - m.stepLlim(), span);
}

// The original clobbered the 400 mm ceiling with 500 mm before the window was
// even shown, by feeding step counts into a millimetre spin box.
void TestModel::startupLimitsComeFromConfig() {
  Model m;

  QVERIFY(std::abs(Model::mmFromSteps(m.stepLlim()) - CB_LLIM_MM) < 1e-9);
  QVERIFY(std::abs(Model::mmFromSteps(m.stepUlim()) - CB_ULIM_MM) < 1e-9);
}

// A driver that stops short must bring the model to rest where the hardware
// actually is, not where the software wanted it to be.
void TestModel::endstopHaltsTravel() {
  const int stopAt = 250;

  Model m(std::make_unique<EndstopDriver>(stopAt));
  QSignalSpy interrupted(&m, &Model::travelInterrupted);

  m.setTargetMm(300.0); // well past the end stop
  QVERIFY(m.stepTarget() > stopAt);

  m.go();
  QTRY_VERIFY_WITH_TIMEOUT(!m.isMoving(), 10000);

  QCOMPARE(m.stepCurrent(), stopAt);
  QCOMPARE(interrupted.count(), 1);

  // The target followed the carriage, so pressing Go again does not resume
  // grinding into the switch.
  QCOMPARE(m.stepTarget(), stopAt);
  m.go();
  QVERIFY(!m.isMoving());
}

// Travel eases in and out instead of jumping straight to cruise speed. The
// exactness guarantee has to survive the variable rate: a ramped move must
// still land on precisely the step it was asked for.
void TestModel::travelRampsUpAndDown() {
  // Must exceed the ramp distance in both directions -- 550 steps at the
  // current rates -- or the move never reaches cruise, the profile comes out
  // triangular, and the ceiling assertion below tests nothing.
  const int target = 1000;

  Model m;
  QList<int> strides;
  int last = 0;

  startProfiledMove(m, target, strides, last);
  QTRY_VERIFY_WITH_TIMEOUT(!m.isMoving(), 30000);

  QCOMPARE(m.stepCurrent(), target);
  QVERIFY(strides.size() > 4);

  const int peak = *std::max_element(strides.begin(), strides.end());

  // Cruise is the ceiling: no tick may exceed the configured top rate.
  const int ceiling =
      static_cast<int>(std::ceil(CB_RATE_CRUISE * CB_TICK_MS / 1000.0));
  QVERIFY2(peak <= ceiling,
           qPrintable(QString("peak %1 steps/tick exceeds cruise ceiling %2")
                          .arg(peak)
                          .arg(ceiling)));

  // Eased in, and eased out.
  QVERIFY2(strides.first() < peak, "move did not start below cruise speed");
  QVERIFY2(strides.last() < peak, "move did not slow down before its target");
}

// A move too short to ever reach cruise speed gets a triangular profile
// rather than a truncated trapezoid, and must still be exact.
void TestModel::shortMoveStillLandsExactly() {
  for (int target : {1, 2, 7, 40}) {
    Model m;
    QList<int> strides;
    int last = 0;

    startProfiledMove(m, target, strides, last);
    QTRY_VERIFY_WITH_TIMEOUT(!m.isMoving(), 30000);

    QCOMPARE(m.stepCurrent(), target);

    int total = 0;

    for (int s : strides)
      total += s;

    QCOMPARE(total, target); // no step invented, none dropped
  }
}

// Stop eases the carriage down rather than dropping it from cruise to nothing
// in one tick, which is the same jolt the ramp exists to avoid. It is not an
// emergency stop; for that, cut the motor supply.
void TestModel::stopDeceleratesRatherThanJumping() {
  Model m;
  QList<int> strides;
  int last = 0;

  startProfiledMove(m, 2000, strides, last);

  // Let it get up to cruise first -- past CB_RAMP_MS worth of ticks.
  const int rampTicks = CB_RAMP_MS / CB_TICK_MS;

  QTRY_VERIFY_WITH_TIMEOUT(strides.size() > rampTicks + 10, 10000);

  const int rateAtCruise = strides.last();
  const int whereStopped = m.stepCurrent();

  m.stop();

  // Still travelling: it has to shed speed before it can rest.
  QVERIFY2(m.isMoving(), "stop() halted instantly instead of decelerating");

  QTRY_VERIFY_WITH_TIMEOUT(!m.isMoving(), 10000);

  // It ran on a little, and was slower by the end than it was at cruise.
  QVERIFY2(m.stepCurrent() > whereStopped,
           "carriage did not coast at all after stop()");
  QVERIFY2(strides.last() < rateAtCruise,
           "carriage was still at full speed on its last tick");

  // Well short of where it had been asked to go.
  QVERIFY(m.stepCurrent() < 2000);

  // And it rests with the target on the carriage, so pressing Go again does
  // not resume the abandoned move.
  QCOMPARE(m.stepTarget(), m.stepCurrent());
}

// A driver that is intermittently busy paces travel without corrupting it.
// Busy means "nothing happened, ask again", so those steps have to come back
// round rather than being dropped -- or being counted as though they moved.
void TestModel::busyDriverLosesNoSteps() {
  const int target = 300;

  auto driver = std::make_unique<BusyDriver>();
  BusyDriver *watch = driver.get();

  Model m(std::move(driver));

  m.setTargetSteps(target);
  m.go();
  QTRY_VERIFY_WITH_TIMEOUT(!m.isMoving(), 30000);

  // Landed exactly, and the hardware was asked for exactly as many steps as
  // the carriage is now reported to have travelled.
  QCOMPARE(m.stepCurrent(), target);
  QCOMPARE(watch->emitted(), target);
}

QTEST_MAIN(TestModel)
#include "test_model.moc"
