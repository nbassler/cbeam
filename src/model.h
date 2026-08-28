#ifndef MODEL_H
#define MODEL_H

#include <QObject>
#include <QTimer>

#include <memory>

#include "stepdriver.h"

// Position state for the linear actuator.
//
// Every position held here is an integer step count.  Millimetres are computed
// on the way out to the GUI, and converted back to steps exactly once, at the
// moment the user edits an mm widget.  No mm value is ever stored, so repeated
// round trips through the GUI cannot accumulate rounding error: a move out and
// back always lands on the step it started from.
//
// Travel is simulated.  tick() advances the carriage a few steps at a time off
// a QTimer rather than looping, so the GUI stays responsive and a long move can
// be aborted with stop().
class Model : public QObject {
    Q_OBJECT

public:

    explicit Model(QObject *parent = nullptr); // simulated motion
    explicit Model(std::unique_ptr<StepDriver> driver,
                   QObject *parent = nullptr);

    const char *driverName() const { return m_driver->name(); }

    int  stepCurrent() const { return m_stepCurrent; }
    int  stepTarget() const { return m_stepTarget; }
    int  stepLlim() const { return m_stepLlim; }
    int  stepUlim() const { return m_stepUlim; }
    bool isMoving() const { return m_timer.isActive(); }

    static double mmFromSteps(int steps);
    static int    stepsFromMm(double mm); // nearest reachable step

    // Re-emit the full state.  Used once after the GUI has connected up, so
    // every widget starts out showing what the model actually holds.
    void publishAll();

public slots:

    void go();   // travel to the current target
    void stop(); // abort travel and hold here
    void zero(); // call the present position zero

    void setTargetSteps(int steps);
    void setTargetMm(double mm);
    void setLlimMm(double mm);
    void setUlimMm(double mm);
    void jog(int deltaSteps); // start a move deltaSteps away from here

signals:

    void positionChanged(int steps,
                         double mm);
    void targetChanged(int steps,
                       double mm);
    void limitsChanged(int loSteps,
                       int hiSteps,
                       double loMm,
                       double hiMm);
    void movingChanged(bool moving);

    // The driver stopped short of what was asked -- an end stop, in a hardware
    // build. Travel has already been halted by the time this fires.
    void travelInterrupted();

private:

    void tick();
    void halt();
    void publishLimits();
    int  clampToLimits(int steps) const;

    std::unique_ptr<StepDriver> m_driver;
    int m_stepCurrent;
    int m_stepTarget;
    int m_stepLlim;
    int m_stepUlim;
    QTimer m_timer;
};

#endif // MODEL_H
