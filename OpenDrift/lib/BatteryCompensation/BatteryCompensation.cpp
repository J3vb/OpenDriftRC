#include "BatteryCompensation.h"

#include <math.h>

namespace
{
    float clampFloat(float value, float low, float high)
    {
        return value < low ? low : (value > high ? high : value);
    }

    int clampInt(int value, int low, int high)
    {
        return value < low ? low : (value > high ? high : value);
    }

    float alphaFor(float dt, float tauSeconds)
    {
        if(tauSeconds <= 0.0f)
        {
            return 1.0f;
        }

        return 1.0f - expf(-dt / tauSeconds);
    }
}


void BatteryCompensation::configure(const Config& newConfig)
{
    config = newConfig;
    config.strengthPercent = clampInt(config.strengthPercent, 0, 100);
    config.curve = clampInt(config.curve, CURVE_LINEAR, CURVE_CUSTOM);
    config.kneePercent = clampInt(config.kneePercent, 0, 90);
    config.filterSeconds = clampFloat(config.filterSeconds, 0.05f, 60.0f);
    config.dropSeconds = clampFloat(config.dropSeconds, 0.05f, 60.0f);
    config.recoverySeconds = clampFloat(config.recoverySeconds, 0.05f, 120.0f);

    updateCompensation();
}


const BatteryCompensation::Config& BatteryCompensation::getConfig() const
{
    return config;
}


void BatteryCompensation::reset()
{
    rawVolts = 0.0f;
    filteredVolts = 0.0f;
    restingVolts = 0.0f;
    liftTimer = 0.0f;
    sinceRestingSeconds = 0.0f;
    initialised = false;
    lifted = false;
    fault = config.sensorEnabled ? FAULT_NO_SAMPLE : FAULT_SENSOR_OFF;

    updateCompensation();
}


void BatteryCompensation::update(
    float volts,
    bool rawValid,
    int throttleInUs,
    float dtSeconds,
    bool throttleValid
)
{
    float dt = clampFloat(dtSeconds, 0.0f, MAX_DT_SECONDS);

    rawVolts = rawValid ? volts : 0.0f;

    bool inRange =
        rawValid &&
        volts >= MIN_VALID_VOLTS &&
        volts <= MAX_VALID_VOLTS;

    bool valid = config.sensorEnabled && inRange;

    // A (re)start waits until any earlier compensation has faded out, so a
    // reading that differs from the previous sensor never steps the ESC.
    if(valid && !initialised && health <= 0.0f)
    {
        filteredVolts = volts;
        restingVolts = volts;
        sinceRestingSeconds = 0.0f;
        initialised = true;
    }

    if(valid && initialised)
    {
        float tau =
            volts < filteredVolts
            ? config.dropSeconds
            : config.recoverySeconds;

        filteredVolts += (volts - filteredVolts) * alphaFor(dt, tau);
    }

    int input =
        throttleValid
        ? clampInt(throttleInUs, 1000, 2000)
        : neutralUs;

    if(throttleValid)
    {
        learnNeutral(input, dt);
    }
    else
    {
        neutralCandidateUs = neutralUs;
        neutralStableSeconds = 0.0f;
    }

    int offset = input - neutralUs;

    lifted = offset >= -LIFT_BAND_US && offset <= LIFT_BAND_US;
    liftTimer = lifted ? liftTimer + dt : 0.0f;

    if(valid && initialised && liftTimer >= LIFT_SETTLE_SECONDS)
    {
        restingVolts +=
            (volts - restingVolts) * alphaFor(dt, config.filterSeconds);

        sinceRestingSeconds = 0.0f;
    }
    else
    {
        sinceRestingSeconds =
            clampFloat(sinceRestingSeconds + dt, 0.0f, STALE_RESTING_SECONDS);

        if(
            valid &&
            initialised &&
            sinceRestingSeconds >= STALE_RESTING_SECONDS
        )
        {
            restingVolts +=
                (filteredVolts - restingVolts) *
                alphaFor(dt, config.filterSeconds);
        }
    }

    if(!config.sensorEnabled)
    {
        fault = FAULT_SENSOR_OFF;
    }
    else if(rawValid && !inRange)
    {
        fault = FAULT_OUT_OF_RANGE;
    }
    else if(!rawValid || !initialised)
    {
        fault = FAULT_NO_SAMPLE;
    }
    else if(!settingsValid())
    {
        fault = FAULT_BAD_SETTINGS;
    }
    else
    {
        fault = FAULT_NONE;
    }

    float healthTarget =
        valid && initialised && config.enabled && settingsValid()
        ? 1.0f
        : 0.0f;

    float healthStep = HEALTH_SLEW_PER_SECOND * dt;

    health += clampFloat(healthTarget - health, -healthStep, healthStep);

    updateCompensation();
}


int BatteryCompensation::apply(int throttleInUs)
{
    int input = clampInt(throttleInUs, 1000, 2000);

    lastInputUs = input;

    int forward =
        config.throttleReversed
        ? neutralUs - input
        : input - neutralUs;

    int span =
        config.throttleReversed
        ? neutralUs - 1000
        : 2000 - neutralUs;

    if(forward <= 0 || span <= 0 || compensation <= 0.0f)
    {
        lastCompensationPercent = 0.0f;
        lastOutputUs = input;
        return input;
    }

    float throttle = (float)forward / (float)span;
    float weight = shapeWeight(throttle, config.curve, config.kneePercent);
    float reduction = compensation * weight;

    int forwardOut =
        (int)lroundf((float)forward * (1.0f - reduction));

    forwardOut = clampInt(forwardOut, 0, forward);

    lastCompensationPercent = 100.0f * reduction;

    lastOutputUs =
        config.throttleReversed
        ? neutralUs - forwardOut
        : neutralUs + forwardOut;

    return lastOutputUs;
}


void BatteryCompensation::clearApplied()
{
    lastCompensationPercent = 0.0f;
    lastInputUs = 1500;
    lastOutputUs = 1500;
}


float BatteryCompensation::shapeWeight(
    float throttle,
    int curve,
    int kneePercent
)
{
    float t = clampFloat(throttle, 0.0f, 1.0f);

    float knee =
        curve == CURVE_CUSTOM
        ? (float)clampInt(kneePercent, 0, 90) / 100.0f
        : 0.0f;

    float u = clampFloat((1.0f - t) / (1.0f - knee), 0.0f, 1.0f);

    return curve == CURVE_EXPO ? u * u : u;
}


float BatteryCompensation::getRawVolts() const
{
    return rawVolts;
}


float BatteryCompensation::getFilteredVolts() const
{
    return filteredVolts;
}


float BatteryCompensation::getRestingVolts() const
{
    return restingVolts;
}


float BatteryCompensation::getSourceVolts() const
{
    return config.useResting ? restingVolts : filteredVolts;
}


float BatteryCompensation::getCompensation() const
{
    return compensation;
}


float BatteryCompensation::getCompensationPercent() const
{
    return lastCompensationPercent;
}


float BatteryCompensation::getHealth() const
{
    return health;
}


int BatteryCompensation::getLastInputUs() const
{
    return lastInputUs;
}


int BatteryCompensation::getLastOutputUs() const
{
    return lastOutputUs;
}


int BatteryCompensation::getNeutralUs() const
{
    return neutralUs;
}


bool BatteryCompensation::isNeutralLearned() const
{
    return neutralLearned;
}


BatteryCompensation::Fault BatteryCompensation::getFault() const
{
    return fault;
}


const char* BatteryCompensation::getFaultText() const
{
    switch(fault)
    {
        case FAULT_NONE:
            return "OK";

        case FAULT_SENSOR_OFF:
            return "NO SENSOR";

        case FAULT_NO_SAMPLE:
            return "NO SAMPLE";

        case FAULT_OUT_OF_RANGE:
            return "OUT OF RANGE";

        case FAULT_BAD_SETTINGS:
            return "BAD SETTINGS";
    }

    return "UNKNOWN";
}


bool BatteryCompensation::isInitialised() const
{
    return initialised;
}


bool BatteryCompensation::isLifted() const
{
    return lifted;
}


bool BatteryCompensation::settingsValid() const
{
    return
        config.startVoltage - config.endVoltage >= MIN_VOLTAGE_SPAN - 0.001f &&
        config.startVoltage > 0.0f;
}


void BatteryCompensation::learnNeutral(int input, float dt)
{
    int offset = input - neutralCandidateUs;

    if(
        offset >= -NEUTRAL_STABLE_BAND_US &&
        offset <= NEUTRAL_STABLE_BAND_US
    )
    {
        neutralStableSeconds += dt;
    }
    else
    {
        neutralCandidateUs = input;
        neutralStableSeconds = 0.0f;
    }

    float required =
        neutralLearned
        ? NEUTRAL_RELEARN_SECONDS
        : NEUTRAL_LEARN_SECONDS;

    if(
        neutralCandidateUs >= NEUTRAL_MIN_US &&
        neutralCandidateUs <= NEUTRAL_MAX_US &&
        neutralStableSeconds >= required
    )
    {
        neutralUs = neutralCandidateUs;
        neutralLearned = true;
    }
}


// The base holds the last value computed from valid settings and a real
// voltage; health alone takes it to zero, so nothing ever steps.
void BatteryCompensation::updateCompensation()
{
    if(settingsValid() && initialised)
    {
        float source = getSourceVolts();
        float span = config.startVoltage - config.endVoltage;

        float position =
            clampFloat((source - config.endVoltage) / span, 0.0f, 1.0f);

        float maximum =
            ((float)config.strengthPercent / 100.0f) *
            (1.0f - config.endVoltage / config.startVoltage);

        compensationBase = maximum * position;
    }

    compensation = compensationBase * health;
}
