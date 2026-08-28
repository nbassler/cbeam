#include "model.h"
#include "config.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

Model::Model(QObject *parent) :
    Model(std::make_unique<SimDriver>(), parent)
{}

Model::Model(std::unique_ptr<StepDriver> driver, QObject *parent) :
    QObject(parent),
    m_driver(std::move(driver)),
    m_stepCurrent(0),
    m_stepTarget(0),
    m_stepLlim(stepsFromMm(CB_LLIM_MM)),
    m_stepUlim(stepsFromMm(CB_ULIM_MM))
{
    m_timer.setInterval(CB_TICK_MS);
    connect(&m_timer, &QTimer::timeout, this, &Model::tick);
}

double Model::mmFromSteps(int steps)
{
    return steps * CB_MM_PER_STEP;
}

int Model::stepsFromMm(double mm)
{
    return static_cast<int>(std::lround(mm / CB_MM_PER_STEP));
}

int Model::clampToLimits(int steps) const
{
    return qBound(m_stepLlim, steps, m_stepUlim);
}

void Model::setTargetSteps(int steps)
{
    m_stepTarget = clampToLimits(steps);

    // Emitted unconditionally, even when the target did not move.  The view
    // writes this value back into the slider and the mm box with their signals
    // blocked, which is what snaps a typed-in mm value onto the exact mm of the
    // nearest step, and is also what stops the echo from coming back here.
    emit targetChanged(m_stepTarget, mmFromSteps(m_stepTarget));
}

void Model::setTargetMm(double mm)
{
    // The only mm -> step conversion on the control path.
    setTargetSteps(stepsFromMm(mm));
}

void Model::setLlimMm(double mm)
{
    m_stepLlim = stepsFromMm(mm);

    if (m_stepLlim > m_stepUlim)
        m_stepUlim = m_stepLlim;
    publishLimits();
}

void Model::setUlimMm(double mm)
{
    m_stepUlim = stepsFromMm(mm);

    if (m_stepUlim < m_stepLlim)
        m_stepLlim = m_stepUlim;
    publishLimits();
}

void Model::publishLimits()
{
    emit limitsChanged(m_stepLlim, m_stepUlim,
                       mmFromSteps(m_stepLlim), mmFromSteps(m_stepUlim));

    // Pull the target back into the new window.  The carriage itself is left
    // where it is: narrowing the limits must not teleport hardware that is
    // already parked outside them.
    setTargetSteps(m_stepTarget);
}

void Model::zero()
{
    // Declare wherever we are now to be zero, and slide the whole travel window
    // down with us so the same physical span stays reachable.  Shifting by the
    // current position is the point of the button; shifting by the lower limit
    // (as this used to) only did the right thing while parked at that limit.
    const int offset = m_stepCurrent;

    m_stepCurrent -= offset; // 0, by construction
    m_stepTarget  -= offset;
    m_stepLlim    -= offset;
    m_stepUlim    -= offset;

    publishAll();
}

void Model::go()
{
    if (m_stepCurrent == m_stepTarget)
        return;

    if (!m_timer.isActive())
    {
        // Start of a fresh move: begin at the start rate with no carry left
        // over from whatever the last move was doing.
        m_rate     = CB_RATE_START;
        m_fraction = 0.0;

        m_timer.start();
        emit movingChanged(true);
    }
}

void Model::stop()
{
    if (!m_timer.isActive())
        return;

    // A controlled stop, not an instant one: dropping from cruise to nothing
    // in a single tick is exactly the jolt the ramp exists to avoid. Retarget
    // to the end of the braking distance -- rearranging v^2 = v0^2 + 2*a*d --
    // and let the profile in tick() decelerate into it, which takes at most
    // CB_RAMP_MS. For a genuine emergency, cut the motor supply.
    const int    direction = (m_stepTarget >= m_stepCurrent) ? 1 : -1;
    const double braking   = (m_rate * m_rate
                              - CB_RATE_START * CB_RATE_START)
                             / (2.0 * CB_ACCEL);

    setTargetSteps(m_stepCurrent
                   + direction * static_cast<int>(std::ceil(braking)));
}

// Come to rest where we are. The target follows the carriage, so a second
// press of Go does not silently resume a move that was just aborted.
void Model::halt()
{
    m_timer.stop();
    setTargetSteps(m_stepCurrent);
    emit movingChanged(false);
}

void Model::jog(int deltaSteps)
{
    // Measured from where we are headed, not from where we happen to be: a jog
    // issued while an earlier one is still running has to add to it, otherwise
    // a quick double click on +1 travels one step instead of two.
    const int base = isMoving() ? m_stepTarget : m_stepCurrent;

    setTargetSteps(base + deltaSteps);
    go();
}

void Model::tick()
{
    const int remaining = m_stepTarget - m_stepCurrent;

    if (remaining == 0)
    {
        halt();
        return;
    }

    const int    direction = remaining > 0 ? 1 : -1;
    const double distance  = std::abs(remaining);
    const double dt        = CB_TICK_MS / 1000.0;

    // Trapezoidal profile. Speed climbs at CB_ACCEL, but is never allowed
    // past what can still be shed before the target: from v^2 = v0^2 + 2*a*d,
    // the fastest we may be travelling with `distance` left to run is
    // sqrt(start^2 + 2*a*distance). That single term produces the whole
    // deceleration ramp, and it also handles moves too short to ever reach
    // cruise -- the profile just becomes a triangle.
    const double accelerating = m_rate + CB_ACCEL * dt;
    const double braking      = std::sqrt(CB_RATE_START * CB_RATE_START
                                          + 2.0 * CB_ACCEL * distance);

    m_rate = std::max(CB_RATE_START,
                      std::min({ CB_RATE_CRUISE, accelerating, braking }));

    // Carry the sub-step remainder between ticks rather than rounding it away,
    // so the profile does not quietly lose or gain steps.
    m_fraction += m_rate * dt;

    int stride = static_cast<int>(m_fraction);

    m_fraction -= stride;
    stride      = std::min(stride, static_cast<int>(distance));

    if (stride == 0)
        return; // still filling the accumulator; nothing to do this tick

    const int taken = m_driver->step(direction * stride);

    m_stepCurrent += taken;
    emit positionChanged(m_stepCurrent, mmFromSteps(m_stepCurrent));

    if (taken != direction * stride)
    {
        // The driver would not go the whole way. Abandon the rest of the move
        // rather than keep asking; grinding a carriage into an engaged end
        // stop is exactly what the switches are there to prevent.
        halt();
        emit travelInterrupted();
        return;
    }

    if (m_stepCurrent == m_stepTarget)
    {
        m_timer.stop();
        emit movingChanged(false);
    }
}

void Model::publishAll()
{
    emit positionChanged(m_stepCurrent, mmFromSteps(m_stepCurrent));
    emit limitsChanged(m_stepLlim, m_stepUlim,
                       mmFromSteps(m_stepLlim), mmFromSteps(m_stepUlim));
    emit targetChanged(m_stepTarget, mmFromSteps(m_stepTarget));
    emit movingChanged(isMoving());
}
