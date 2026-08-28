// Regression tests for the position model.
//
// The point of most of these is a single property: positions are stored as
// integer steps, so no amount of going back and forth through the millimetre
// display can move the carriage off the step it started on. That drift is the
// bug this model was rewritten to make impossible, and these are here so it
// stays impossible.

#include "model.h"
#include "config.h"
#include "stepdriver.h"

#include <QtTest>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {
    // Stand in for the GUI's mm widget: the model emits a millimetre value,
    // the widget renders it at CB_MM_DIGITS decimals, and the user commits
    // that rounded value back. This is the round trip that used to lose steps.
    double widgetQuantise(double mm)
    {
        const double scale = std::pow(10.0, CB_MM_DIGITS);

        return std::round(mm * scale) / scale;
    }

    // A driver that refuses to travel past a given step, standing in for an
    // end stop until the GPIO backend exists.
    class EndstopDriver : public StepDriver {
public:

        explicit EndstopDriver(int limit) :
            m_limit(limit)
        {}

        int step(int steps) override
        {
            const int landed = std::min(m_at + steps, m_limit);
            const int taken  = landed - m_at;

            m_at = landed;
            return taken;
        }

        const char *name() const override
        {
            return "end stop test";
        }

private:

        int m_at = 0;
        int m_limit;
    };
}

class TestModel: public QObject {
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
};

// Type a value, press enter, type the value back: many times over, at
// distances that do not divide evenly into steps.
void TestModel::mmRoundTripReturnsToStart()
{
    Model  m;
    double shown = 0.0;

    connect(&m, &Model::targetChanged, [&shown](int, double mm) {
        shown = widgetQuantise(mm);
    });

    const double hops[] = { 3.33, 7.77, 0.017, 41.6667, 1.0 / 3.0 };

    for (int rep = 0; rep < 200; rep++)
        for (double h : hops)
        {
            m.setTargetMm(shown + h);
            m.setTargetMm(shown - h);
        }

    QCOMPARE(m.stepTarget(), 0);
}

// Feed the model its own displayed value over and over. A design that stored
// millimetres creeps here; one that stores steps cannot.
void TestModel::selfEchoHoldsStill()
{
    Model  m;
    double shown = 0.0;

    connect(&m, &Model::targetChanged, [&shown](int, double mm) {
        shown = widgetQuantise(mm);
    });

    m.setTargetMm(123.456); // primes `shown` with the quantised readback
    const int settled = m.stepTarget();

    for (int i = 0; i < 10000; i++)
        m.setTargetMm(shown);

    QCOMPARE(m.stepTarget(), settled);
}

// The load-bearing one. Every reachable step has to survive being rendered at
// CB_MM_DIGITS decimals and parsed back. If the display were too coarse, two
// steps would share an mm string and a round trip could land on the wrong one.
void TestModel::everyStepSurvivesTheMmDisplay()
{
    const int lo = Model::stepsFromMm(CB_LLIM_MM);
    const int hi = Model::stepsFromMm(CB_ULIM_MM);

    for (int s = lo; s <= hi; s++)
    {
        const int back = Model::stepsFromMm(
            widgetQuantise(Model::mmFromSteps(s)));

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
void TestModel::jogsAccumulateWhileMoving()
{
    // Deliberately literal rather than CB_JOG_DEFAULTS: how far the GUI's
    // buttons happen to be set is a view concern, and retuning a jog column
    // should not be able to break this test.
    const int forward = 10;
    const int back    = 1;

    Model m;

    for (int i = 0; i < 500; i++)
    {
        m.jog(+forward);
        m.jog(-back);
    }

    QCOMPARE(m.stepTarget(), 500 * (forward - back));
}

void TestModel::limitsRoundRatherThanTruncate()
{
    Model m;

    m.setUlimMm(100.0);
    QCOMPARE(m.stepUlim(), Model::stepsFromMm(100.0));
    QVERIFY(std::abs(Model::mmFromSteps(m.stepUlim()) - 100.0)
            <= CB_MM_PER_STEP / 2.0);
}

void TestModel::targetsClampToTravel()
{
    Model m;

    m.setTargetMm(9999.0);
    QCOMPARE(m.stepTarget(), m.stepUlim());

    m.setTargetMm(-9999.0);
    QCOMPARE(m.stepTarget(), m.stepLlim());
}

// Zero declares the present position to be zero and slides the travel window
// down with it. Shifting by the lower limit, as the original did, only
// happened to work while parked at that limit.
void TestModel::zeroShiftsWindowByPosition()
{
    Model     m;
    const int span = m.stepUlim() - m.stepLlim();

    // Kept short deliberately: travel runs in real time at CB_STEPS_PER_TICK
    // per CB_TICK_MS, and what is under test here is the arithmetic of zero(),
    // which does not care how far the carriage came.
    m.setTargetMm(2.0);
    m.go();
    QTRY_VERIFY_WITH_TIMEOUT(!m.isMoving(), 10000);

    const int arrived = m.stepCurrent();
    QCOMPARE(arrived, Model::stepsFromMm(2.0));

    m.zero();
    QCOMPARE(m.stepCurrent(), 0);
    QCOMPARE(m.stepUlim() - m.stepLlim(), span);
    QCOMPARE(m.stepLlim(), -arrived);
}

// The original clobbered the 400 mm ceiling with 500 mm before the window was
// even shown, by feeding step counts into a millimetre spin box.
void TestModel::startupLimitsComeFromConfig()
{
    Model m;

    QVERIFY(std::abs(Model::mmFromSteps(m.stepLlim()) - CB_LLIM_MM) < 1e-9);
    QVERIFY(std::abs(Model::mmFromSteps(m.stepUlim()) - CB_ULIM_MM) < 1e-9);
}

// A driver that stops short must bring the model to rest where the hardware
// actually is, not where the software wanted it to be.
void TestModel::endstopHaltsTravel()
{
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

QTEST_MAIN(TestModel)
#include "test_model.moc"
