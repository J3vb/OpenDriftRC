#include "GyroController.h"

#include <math.h>


namespace
{
    static constexpr uint8_t PHASE_IDLE = 0;
    static constexpr uint8_t PHASE_ENTRY = 1;
    static constexpr uint8_t PHASE_SETTLED = 2;
    static constexpr uint8_t PHASE_TRANSITION = 3;

    static constexpr float TRANSITION_SECONDS = 0.18f;
    static constexpr float THROTTLE_APPLY_SECONDS = 0.22f;
    static constexpr float THROTTLE_LIFT_SECONDS = 0.48f;
    static constexpr float MEMORY_GAIN_SCALE = 6.0f;

    static constexpr float HUNT_BASELINE_SECONDS = 0.45f;
    static constexpr float HUNT_MIN_HALF_PERIOD = 0.20f;
    static constexpr float HUNT_MAX_HALF_PERIOD = 0.75f;
    static constexpr float HUNT_RESIDUAL_THRESHOLD = 3.5f;
    static constexpr float HUNT_MIN_PEAK = 5.0f;
}


void GyroController::resetDynamicState()
{
    filteredYaw = 0.0f;
    previousFilteredYaw = 0.0f;
    filteredYawAcceleration = 0.0f;

    driftReferenceYaw = 0.0f;
    driftReferenceReady = false;
    driftDirection = 0;
    transitionTime = 0.0f;

    integralAccumulator = 0.0f;
    integralCorrection = 0;
    counterSteerCorrection = 0;

    steeringActivity = 0.0f;
    tailSlideBlend = 0.0f;
    lastSteeringCommand = 1500;
    steeringReady = false;

    throttleRate = 0.0f;
    throttleTransientTime = 0.0f;
    throttleApplyTime = 0.0f;
    throttleLiftTime = 0.0f;
    previousThrottleLevel = 0.0f;
    filteredThrottleLoadRate = 0.0f;
    throttleLiftBlend = 0.0f;
    lastThrottlePulse = 1500;
    throttleReady = false;

    controlPhase = PHASE_IDLE;
    settledBlend = 0.0f;
    transitionAuthorityBlend = 0.0f;

    huntBaselineYaw = 0.0f;
    huntHalfCyclePeak = 0.0f;
    huntAmplitude = 0.0f;
    huntCrossingAge = 0.0f;
    huntConfidence = 0.0f;
    huntSuppression = 0.0f;
    huntFrequency = 0.0f;
    huntResidualSign = 0;
    huntBaselineReady = false;

    predictedYawTelemetry = 0.0f;
    driftReferenceTelemetry = 0.0f;
    referenceErrorTelemetry = 0.0f;
    referenceLockTelemetry = 0.0f;
    throttlePredictionTelemetry = 0.0f;
    directCorrectionTelemetry = 0.0f;
    memoryFeedbackTelemetry = 0.0f;
    driverActivityTelemetry = 0.0f;
    throttlePredictionBlendTelemetry = 0.0f;
    throttleLiftBlendTelemetry = 0.0f;
    huntSuppressionTelemetry = 0.0f;
    huntFrequencyTelemetry = 0.0f;
    transitionAuthorityTelemetry = 0.0f;

    servoOutput = 1500;
    correctionOutput = 0;
}


bool GyroController::begin()
{
    gyroOffset = 0.0f;
    calibrated = false;
    lastUpdateMicros = 0;

    resetDynamicState();

    return true;
}


void GyroController::calibrate(float yawRate)
{
    gyroOffset = yawRate;
    calibrated = true;

    resetDynamicState();

    lastUpdateMicros = micros();
}


int GyroController::update(
    float yawRate,
    int steeringCommand,
    bool steeringSignal,
    int throttlePulse,
    bool throttleSignal
)
{
    uint32_t now = micros();
    float dt = 0.004f;

    if(lastUpdateMicros != 0)
    {
        dt =
            (now - lastUpdateMicros)
            /
            1000000.0f;

        dt = constrain(
            dt,
            0.001f,
            0.05f
        );
    }

    lastUpdateMicros = now;

    if(!calibrated)
    {
        calibrate(yawRate);
    }

    steeringCommand = constrain(
        steeringCommand,
        1000,
        2000
    );

    if(!steeringSignal)
    {
        steeringReady = false;
        steeringActivity = 0.0f;
        lastSteeringCommand = 1500;
    }
    else if(!steeringReady)
    {
        lastSteeringCommand = steeringCommand;
        steeringReady = true;
    }
    else
    {
        float steeringRate =
            fabsf(
                steeringCommand - lastSteeringCommand
            )
            /
            dt;

        float steeringAmount =
            1.0f - expf(-dt / 0.04f);

        steeringActivity +=
            (steeringRate - steeringActivity)
            *
            steeringAmount;

        lastSteeringCommand = steeringCommand;
    }

    float driverActivityBlend = constrain(
        (steeringActivity - 80.0f) / 1800.0f,
        0.0f,
        1.0f
    );

    float throttleLevel = 0.0f;

    if(throttleSignal)
    {
        throttlePulse = constrain(
            throttlePulse,
            900,
            2100
        );

        throttleLevel = constrain(
            fabsf(throttlePulse - 1500.0f) / 500.0f,
            0.0f,
            1.0f
        );

        if(!throttleReady)
        {
            lastThrottlePulse = throttlePulse;
            previousThrottleLevel = throttleLevel;
            filteredThrottleLoadRate = 0.0f;
            throttleReady = true;
        }
        else
        {
            int throttleDelta =
                throttlePulse - lastThrottlePulse;

            float rawThrottleRate =
                throttleDelta
                /
                dt;

            float rawThrottleLoadRate =
                (throttleLevel - previousThrottleLevel)
                /
                dt;

            float throttleAmount =
                1.0f - expf(-dt / 0.04f);

            throttleRate +=
                (rawThrottleRate - throttleRate)
                *
                throttleAmount;

            filteredThrottleLoadRate +=
                (
                    rawThrottleLoadRate
                    -
                    filteredThrottleLoadRate
                )
                *
                throttleAmount;

            float throttleLoadDelta =
                throttleLevel - previousThrottleLevel;

            bool applyingThrottle =
                throttleLoadDelta >= 0.012f ||
                filteredThrottleLoadRate >= 0.30f;

            bool liftingThrottle =
                previousThrottleLevel >= 0.06f &&
                (
                    throttleLoadDelta <= -0.012f ||
                    filteredThrottleLoadRate <= -0.22f
                );

            if(applyingThrottle)
            {
                throttleApplyTime =
                    THROTTLE_APPLY_SECONDS;
            }

            if(liftingThrottle)
            {
                throttleLiftTime =
                    THROTTLE_LIFT_SECONDS;
            }

            lastThrottlePulse = throttlePulse;
            previousThrottleLevel = throttleLevel;
        }
    }
    else
    {
        throttleReady = false;
        throttleRate = 0.0f;
        filteredThrottleLoadRate = 0.0f;
        previousThrottleLevel = 0.0f;
        throttleApplyTime = 0.0f;
        throttleLiftTime = 0.0f;
        throttleTransientTime = 0.0f;
    }

    throttleApplyTime = max(
        0.0f,
        throttleApplyTime - dt
    );

    throttleLiftTime = max(
        0.0f,
        throttleLiftTime - dt
    );

    throttleTransientTime = max(
        throttleApplyTime,
        throttleLiftTime
    );

    float throttleRateBlend = constrain(
        (fabsf(throttleRate) - 250.0f) / 3000.0f,
        0.0f,
        1.0f
    );

    float throttleApplyBlend = constrain(
        throttleApplyTime
        /
        THROTTLE_APPLY_SECONDS,
        0.0f,
        1.0f
    );

    throttleLiftBlend = constrain(
        throttleLiftTime
        /
        THROTTLE_LIFT_SECONDS,
        0.0f,
        1.0f
    );

    float throttlePredictionBlend = max(
        throttleRateBlend,
        max(
            throttleApplyBlend * 0.55f,
            throttleLiftBlend * 0.80f
        )
    );

    float correctedYaw =
        yawRate - gyroOffset;

    float yawMagnitude =
        fabsf(correctedYaw);

    if(yawMagnitude <= deadband)
    {
        correctedYaw = 0.0f;
    }
    else
    {
        correctedYaw =
            (correctedYaw > 0.0f ? 1.0f : -1.0f)
            *
            (yawMagnitude - deadband);
    }

    // One time-based gyro low-pass is the entire filtering chain.
    float baseFilterAmount =
        1.0f -
        constrain(
            smoothing,
            0.01f,
            0.99f
        );

    float filterAmount =
        1.0f -
        powf(
            1.0f - baseFilterAmount,
            dt / 0.02f
        );

    filterAmount = constrain(
        filterAmount,
        0.001f,
        1.0f
    );

    previousFilteredYaw = filteredYaw;

    filteredYaw +=
        (correctedYaw - filteredYaw)
        *
        filterAmount;

    float rawYawAcceleration =
        (filteredYaw - previousFilteredYaw)
        /
        dt;

    rawYawAcceleration = constrain(
        rawYawAcceleration,
        -4000.0f,
        4000.0f
    );

    float accelerationAmount =
        1.0f - expf(-dt / 0.035f);

    filteredYawAcceleration +=
        (rawYawAcceleration - filteredYawAcceleration)
        *
        accelerationAmount;

    // Throttle does not pretend to be vehicle speed or prescribe a turn
    // direction. It announces an upcoming chassis-load change, extending the
    // short yaw-acceleration look-ahead before the resulting motion arrives.
    // Moving toward neutral gets a longer envelope because track data shows
    // lift response developing over several hundred milliseconds.
    float predictionSeconds =
        0.003f
        +
        (
            predictionStrength
            /
            100.0f
        )
        *
        0.024f
        +
        throttlePredictionBlend
        *
        0.012f
        +
        throttleLiftBlend
        *
        0.010f
        +
        throttleLevel
        *
        0.003f;

    float predictionDelta = constrain(
        filteredYawAcceleration
        *
        predictionSeconds,
        -45.0f,
        45.0f
    );

    float predictedYaw =
        filteredYaw + predictionDelta;

    if(
        fabsf(filteredYaw) > 5.0f &&
        predictedYaw * filteredYaw < 0.0f
    )
    {
        predictedYaw = 0.0f;
    }

    float yawAbs = fabsf(filteredYaw);

    int8_t definiteDirection =
        filteredYaw > 12.0f
        ?
        1
        :
        (
            filteredYaw < -12.0f
            ?
            -1
            :
            0
        );

    bool directionChanged =
        definiteDirection != 0 &&
        driftDirection != 0 &&
        definiteDirection != driftDirection;

    if(directionChanged)
    {
        transitionTime = TRANSITION_SECONDS;
        driftReferenceReady = false;
        integralCorrection = 0;
    }

    if(definiteDirection != 0)
    {
        driftDirection = definiteDirection;
    }

    transitionTime = max(
        0.0f,
        transitionTime - dt
    );

    bool idle =
        yawAbs < 7.0f;

    float deliberateTransitionBlend =
        !idle
        ?
        constrain(
            (driverActivityBlend - 0.25f) / 0.75f,
            0.0f,
            1.0f
        )
        :
        0.0f;

    float directionTransitionBlend =
        constrain(
            transitionTime / TRANSITION_SECONDS,
            0.0f,
            1.0f
        );

    float transitionAuthorityTarget = max(
        deliberateTransitionBlend,
        directionTransitionBlend
    );

    float transitionAuthoritySeconds =
        transitionAuthorityTarget > transitionAuthorityBlend
        ?
        0.025f
        :
        0.16f;

    float transitionAuthorityAmount =
        1.0f - expf(-dt / transitionAuthoritySeconds);

    transitionAuthorityBlend +=
        (
            transitionAuthorityTarget
            -
            transitionAuthorityBlend
        )
        *
        transitionAuthorityAmount;

    transitionAuthorityBlend = constrain(
        transitionAuthorityBlend,
        0.0f,
        1.0f
    );

    float quietBlend =
        (1.0f - driverActivityBlend)
        *
        (1.0f - throttlePredictionBlend);

    bool driftActive =
        !idle &&
        definiteDirection != 0;

    float settledTarget =
        driftActive &&
        transitionTime <= 0.0f &&
        transitionAuthorityBlend < 0.20f
        ?
        quietBlend
        :
        0.0f;

    float settledTimeConstant =
        settledTarget > settledBlend
        ?
        0.20f
        :
        0.06f;

    float settledAmount =
        1.0f - expf(-dt / settledTimeConstant);

    settledBlend +=
        (settledTarget - settledBlend)
        *
        settledAmount;

    settledBlend = constrain(
        settledBlend,
        0.0f,
        1.0f
    );

    if(idle)
    {
        controlPhase = PHASE_IDLE;
        driftDirection = 0;
        driftReferenceReady = false;
        driftReferenceYaw = 0.0f;
        integralAccumulator = 0.0f;
        integralCorrection = 0;
    }
    else if(
        transitionTime > 0.0f ||
        transitionAuthorityBlend > 0.35f
    )
    {
        controlPhase = PHASE_TRANSITION;
    }
    else if(settledBlend > 0.55f)
    {
        controlPhase = PHASE_SETTLED;
    }
    else
    {
        controlPhase = PHASE_ENTRY;
    }

    bool huntCandidate =
        controlPhase == PHASE_SETTLED &&
        settledBlend >= 0.65f &&
        driverActivityBlend <= 0.20f &&
        transitionAuthorityBlend <= 0.15f &&
        throttlePredictionBlend <= 0.25f &&
        yawAbs >= 15.0f;

    if(!huntBaselineReady)
    {
        huntBaselineYaw = filteredYaw;
        huntBaselineReady = true;
    }

    float huntBaselineSeconds =
        huntCandidate
        ?
        HUNT_BASELINE_SECONDS
        :
        0.08f;

    float huntBaselineAmount =
        1.0f - expf(-dt / huntBaselineSeconds);

    huntBaselineYaw +=
        (filteredYaw - huntBaselineYaw)
        *
        huntBaselineAmount;

    float huntResidual =
        filteredYaw - huntBaselineYaw;

    huntCrossingAge = min(
        huntCrossingAge + dt,
        2.0f
    );

    huntConfidence = max(
        0.0f,
        huntConfidence - dt * 0.18f
    );

    if(huntCandidate)
    {
        huntHalfCyclePeak = max(
            huntHalfCyclePeak,
            fabsf(huntResidual)
        );

        int8_t residualSign =
            huntResidual >= HUNT_RESIDUAL_THRESHOLD
            ?
            1
            :
            (
                huntResidual <= -HUNT_RESIDUAL_THRESHOLD
                ?
                -1
                :
                0
            );

        if(residualSign != 0)
        {
            if(huntResidualSign == 0)
            {
                huntResidualSign = residualSign;
                huntCrossingAge = 0.0f;
                huntHalfCyclePeak = fabsf(huntResidual);
            }
            else if(residualSign != huntResidualSign)
            {
                bool validHalfCycle =
                    huntCrossingAge >= HUNT_MIN_HALF_PERIOD &&
                    huntCrossingAge <= HUNT_MAX_HALF_PERIOD &&
                    huntHalfCyclePeak >= HUNT_MIN_PEAK;

                if(validHalfCycle)
                {
                    float measuredFrequency =
                        1.0f / (2.0f * huntCrossingAge);

                    huntFrequency +=
                        (measuredFrequency - huntFrequency)
                        *
                        0.35f;

                    huntAmplitude +=
                        (huntHalfCyclePeak - huntAmplitude)
                        *
                        0.40f;

                    huntConfidence = min(
                        1.0f,
                        huntConfidence + 0.34f
                    );
                }
                else
                {
                    huntConfidence *= 0.65f;
                }

                huntResidualSign = residualSign;
                huntCrossingAge = 0.0f;
                huntHalfCyclePeak = fabsf(huntResidual);
            }
        }
    }
    else
    {
        huntResidualSign = 0;
        huntCrossingAge = 0.0f;
        huntHalfCyclePeak = 0.0f;
        huntAmplitude +=
            (0.0f - huntAmplitude)
            *
            (1.0f - expf(-dt / 0.35f));
    }

    float huntAmplitudeBlend = constrain(
        (huntAmplitude - HUNT_MIN_PEAK) / 18.0f,
        0.0f,
        1.0f
    );

    float huntSuppressionTarget =
        huntCandidate
        ?
        huntConfidence
        *
        huntAmplitudeBlend
        *
        settledBlend
        :
        0.0f;

    float huntSuppressionSeconds =
        huntSuppressionTarget > huntSuppression
        ?
        0.16f
        :
        0.65f;

    huntSuppression +=
        (
            huntSuppressionTarget
            -
            huntSuppression
        )
        *
        (
            1.0f
            -
            expf(-dt / huntSuppressionSeconds)
        );

    huntSuppression = constrain(
        huntSuppression,
        0.0f,
        1.0f
    );

    if(driftActive)
    {
        if(!driftReferenceReady)
        {
            driftReferenceYaw = filteredYaw;
            driftReferenceReady = true;
        }
        else
        {
            float holdStrength =
                holdBoost
                /
                100.0f;

            float referenceTimeConstant =
                0.045f
                +
                quietBlend
                *
                (
                    0.18f
                    +
                    2.20f
                    *
                    holdStrength
                );

            float referenceAmount =
                1.0f -
                expf(
                    -dt
                    /
                    referenceTimeConstant
                );

            driftReferenceYaw +=
                (filteredYaw - driftReferenceYaw)
                *
                referenceAmount;
        }
    }

    float referenceError =
        driftReferenceReady
        ?
        filteredYaw - driftReferenceYaw
        :
        0.0f;

    integralAccumulator = referenceError;

    float memoryCorrection =
        referenceError
        *
        integralGain
        *
        MEMORY_GAIN_SCALE
        *
        settledBlend;

    integralCorrection =
        constrain(
            (int)roundf(memoryCorrection),
            -integralLimit,
            integralLimit
        );

    // Experimental Tail Slide Speed is centered at 50. It changes only the
    // fast damping path while deliberate steering movement is present:
    // lower values add damping, 50 preserves the Open Beta baseline, and
    // higher values release damping. The 40% floor at 100 means it cannot
    // reverse or remove gyro correction, and settled drifts receive a
    // substantially smaller change in either direction.
    tailSlideBlend =
        ((tailSlideSpeed - 50) / 50.0f)
        *
        driverActivityBlend
        *
        (1.0f - 0.65f * settledBlend);

    tailSlideBlend = constrain(
        tailSlideBlend,
        -1.0f,
        1.0f
    );

    float directDampingScale =
        1.0f - 0.60f * tailSlideBlend;

    float transitionDirectScale =
        1.0f - 0.10f * transitionAuthorityBlend;

    float huntDirectScale =
        1.0f - 0.30f * huntSuppression;

    float directCorrection =
        predictedYaw
        *
        gyroGain
        *
        directDampingScale
        *
        transitionDirectScale
        *
        huntDirectScale;

    // Countersteer Assist is deliberately sourced from the slow learned
    // drift reference. It increases how much of a settled drift OpenDrift
    // carries without raising fast yaw damping or responding to chatter.
    float steadyAssistCorrection =
        driftReferenceReady
        ?
        driftReferenceYaw
        *
        gyroGain
        *
        (counterSteerAssist / 100.0f)
        *
        settledBlend
        *
        (1.0f - 0.75f * transitionAuthorityBlend)
        *
        (1.0f - 0.15f * huntSuppression)
        :
        0.0f;

    counterSteerCorrection =
        (int)roundf(steadyAssistCorrection);

    float baseCorrection =
        directCorrection
        +
        steadyAssistCorrection;

    int effectiveMaxCorrection =
        (int)roundf(
            maxCorrection
            *
            (1.0f - 0.12f * transitionAuthorityBlend)
        );

    effectiveMaxCorrection = constrain(
        effectiveMaxCorrection,
        0,
        maxCorrection
    );

    // There is no accumulating state to wind up. When direct damping has
    // saturated, memory may help it unwind but may not push farther into the
    // same limit.
    if(
        fabsf(baseCorrection) >= effectiveMaxCorrection &&
        integralCorrection * baseCorrection > 0.0f
    )
    {
        integralCorrection = 0;
    }

    int targetCorrection =
        constrain(
            (int)roundf(
                baseCorrection
                +
                integralCorrection
            ),
            -effectiveMaxCorrection,
            effectiveMaxCorrection
        );

    if(idle && correctedYaw == 0.0f)
    {
        targetCorrection = 0;
        filteredYawAcceleration = 0.0f;
    }

    correctionOutput = targetCorrection;

    servoOutput = constrain(
        1500 - correctionOutput,
        1000,
        2000
    );

    predictedYawTelemetry = predictedYaw;
    driftReferenceTelemetry = driftReferenceYaw;
    referenceErrorTelemetry = referenceError;
    referenceLockTelemetry = settledBlend;
    throttlePredictionTelemetry = throttlePredictionBlend;
    directCorrectionTelemetry = directCorrection;
    memoryFeedbackTelemetry = integralCorrection;
    driverActivityTelemetry = driverActivityBlend;
    throttlePredictionBlendTelemetry = throttlePredictionBlend;
    throttleLiftBlendTelemetry = throttleLiftBlend;
    huntSuppressionTelemetry = huntSuppression;
    huntFrequencyTelemetry = huntFrequency;
    transitionAuthorityTelemetry = transitionAuthorityBlend;

    return servoOutput;
}


void GyroController::setGain(float value)
{
    gyroGain = constrain(
        value,
        0.0f,
        10.0f
    );
}


float GyroController::getGain()
{
    return gyroGain;
}


void GyroController::setDeadband(float value)
{
    deadband = constrain(
        value,
        0.0f,
        100.0f
    );
}


float GyroController::getDeadband()
{
    return deadband;
}


void GyroController::setSmoothing(float value)
{
    smoothing = constrain(
        value,
        0.01f,
        1.0f
    );
}


float GyroController::getSmoothing()
{
    return smoothing;
}


void GyroController::setMaxCorrection(int value)
{
    maxCorrection = constrain(
        value,
        0,
        1000
    );
}


int GyroController::getMaxCorrection()
{
    return maxCorrection;
}


int GyroController::getCorrection()
{
    return correctionOutput;
}


void GyroController::setIntegralGain(float value)
{
    integralGain = constrain(
        value,
        0.0f,
        20.0f
    );

    if(integralGain <= 0.0f)
    {
        integralAccumulator = 0.0f;
        integralCorrection = 0;
    }
}


float GyroController::getIntegralGain()
{
    return integralGain;
}


void GyroController::setIntegralLimit(int value)
{
    integralLimit = constrain(
        value,
        0,
        500
    );

    if(integralLimit <= 0)
    {
        integralCorrection = 0;
    }
}


int GyroController::getIntegralLimit()
{
    return integralLimit;
}


int GyroController::getIntegralCorrection()
{
    return integralCorrection;
}


void GyroController::setHoldBoost(int value)
{
    holdBoost = constrain(
        value,
        0,
        100
    );
}


int GyroController::getHoldBoost()
{
    return holdBoost;
}

void GyroController::setCounterSteerAssist(int value)
{
    counterSteerAssist = constrain(value, 0, 100);

    if(counterSteerAssist <= 0)
    {
        counterSteerCorrection = 0;
    }
}

int GyroController::getCounterSteerAssist()
{
    return counterSteerAssist;
}

int GyroController::getCounterSteerCorrection()
{
    return counterSteerCorrection;
}

void GyroController::setTailSlideSpeed(int value)
{
    tailSlideSpeed = constrain(value, 0, 100);

    if(tailSlideSpeed == 50)
    {
        tailSlideBlend = 0.0f;
    }
}

int GyroController::getTailSlideSpeed()
{
    return tailSlideSpeed;
}

float GyroController::getTailSlideBlend()
{
    return tailSlideBlend;
}


void GyroController::setPredictionStrength(int value)
{
    predictionStrength = constrain(
        value,
        0,
        100
    );
}


int GyroController::getPredictionStrength()
{
    return predictionStrength;
}


float GyroController::getPredictedYaw()
{
    return predictedYawTelemetry;
}


float GyroController::getDriftReferenceYaw()
{
    return driftReferenceTelemetry;
}


float GyroController::getReferenceError()
{
    return referenceErrorTelemetry;
}


float GyroController::getReferenceLock()
{
    return referenceLockTelemetry;
}


float GyroController::getThrottlePrediction()
{
    return throttlePredictionTelemetry;
}


float GyroController::getDirectCorrection()
{
    return directCorrectionTelemetry;
}


float GyroController::getMemoryFeedback()
{
    return memoryFeedbackTelemetry;
}


float GyroController::getDriverActivityBlend()
{
    return driverActivityTelemetry;
}


float GyroController::getThrottlePredictionBlend()
{
    return throttlePredictionBlendTelemetry;
}


float GyroController::getThrottleLiftBlend()
{
    return throttleLiftBlendTelemetry;
}


float GyroController::getSteeringActivity()
{
    return steeringActivity;
}


float GyroController::getHuntSuppression()
{
    return huntSuppressionTelemetry;
}


float GyroController::getHuntFrequency()
{
    return huntFrequencyTelemetry;
}


float GyroController::getTransitionAuthorityBlend()
{
    return transitionAuthorityTelemetry;
}


int GyroController::getControlPhase()
{
    return controlPhase;
}


float GyroController::getSettledBlend()
{
    return settledBlend;
}


float GyroController::getThrottleTransient()
{
    return throttleTransientTime;
}


float GyroController::getFilteredYaw()
{
    return filteredYaw;
}


int GyroController::getServoOutput()
{
    return servoOutput;
}
