#ifndef STEPDRIVER_H
#define STEPDRIVER_H

// The seam between the position model and whatever actually moves the rail.
//
// Model deals only in step counts and never touches hardware itself, so the
// same model code runs against the simulation on a desktop and against the
// GPIO pins on the Pi.  Chosen at build time by the CBEAM_BACKEND option in
// CMakeLists.txt.
class StepDriver {
public:

    virtual ~StepDriver() = default;

    // Advance the carriage by `steps`, signed; negative travels toward the
    // lower limit.  Returns the number of steps actually taken, which may be
    // fewer than asked for.
    //
    // A short count is how a driver says "I stopped early" -- an end stop
    // engaged, most likely.  The model then halts and re-targets to wherever
    // it really ended up, rather than carrying on with a position it believes
    // but the hardware does not share.  That is the whole reason this returns
    // a count instead of void.
    virtual int step(int steps) = 0;

    // Shown in the status bar, so it is never a mystery whether what is on
    // screen is driving a real rail.
    virtual const char *name() const = 0;
};

// Motion with nothing behind it: every step always succeeds.
class SimDriver : public StepDriver {
public:

    int step(int steps) override
    {
        return steps;
    }

    const char *name() const override
    {
        return "simulation";
    }
};

#endif // STEPDRIVER_H
