#pragma once

#include <Arduino.h>


class GyroController
{
public:

    bool begin();

    int update(
        float yawRate,
        int steeringCommand = 1500,
        bool steeringSignal = false,
        int throttlePulse = 1500,
        bool throttleSignal = false
    );

    void calibrate(float yawRate);

    void setGain(float gain);
    float getGain();

    void setDeadband(float deadband);
    float getDeadband();

    void setSmoothing(float smoothing);
    float getSmoothing();

    void setMaxCorrection(int correction);
    int getMaxCorrection();
    int getCorrection();

    // Drift Memory is equilibrium-error feedback, never an integral of raw
    // yaw. Drift Memory Limit caps that feedback.
    void setIntegralGain(float value);
    float getIntegralGain();
    void setIntegralLimit(int value);
    int getIntegralLimit();
    int getIntegralCorrection();

    // Hold Assist controls how firmly a quiet drift reference is retained.
    void setHoldBoost(int value);
    int getHoldBoost();

    // Adds only steady-state countersteer from the learned drift reference.
    // Zero preserves the base response; 100 adds up to one additional
    // copy of the steady direct correction without increasing fast damping.
    void setCounterSteerAssist(int value);
    int getCounterSteerAssist();
    int getCounterSteerCorrection();

    // Experimental RC1 rotation-speed trim. While the driver is actively
    // moving the steering, lower values add damping and higher values let
    // commanded chassis rotation pass more freely. 50 preserves RC1 exactly.
    void setTailSlideSpeed(int value);
    int getTailSlideSpeed();
    float getTailSlideBlend();

    void setPredictionStrength(int value);
    int getPredictionStrength();

    float getPredictedYaw();
    float getDriftReferenceYaw();
    float getReferenceError();
    float getReferenceLock();
    float getThrottlePrediction();
    float getDirectCorrection();
    float getMemoryFeedback();
    float getDriverActivityBlend();
    float getThrottlePredictionBlend();
    float getThrottleLiftBlend();
    float getSteeringActivity();
    float getHuntSuppression();
    float getHuntFrequency();
    float getTransitionAuthorityBlend();

    int getControlPhase();
    float getSettledBlend();
    float getThrottleTransient();

    float getFilteredYaw();
    int getServoOutput();


private:

    float gyroGain = 1.5f;
    float gyroOffset = 0.0f;
    float deadband = 2.0f;
    float smoothing = 0.10f;
    int maxCorrection = 250;

    float integralGain = 0.0f;
    int integralLimit = 120;
    int holdBoost = 0;
    int counterSteerAssist = 0;
    int tailSlideSpeed = 50;
    int predictionStrength = 0;

    float filteredYaw = 0.0f;
    float previousFilteredYaw = 0.0f;
    float filteredYawAcceleration = 0.0f;

    float driftReferenceYaw = 0.0f;
    bool driftReferenceReady = false;
    int8_t driftDirection = 0;
    float transitionTime = 0.0f;

    float integralAccumulator = 0.0f;
    int integralCorrection = 0;
    int counterSteerCorrection = 0;

    float steeringActivity = 0.0f;
    float tailSlideBlend = 0.0f;
    int lastSteeringCommand = 1500;
    bool steeringReady = false;

    float throttleRate = 0.0f;
    float throttleTransientTime = 0.0f;
    float throttleApplyTime = 0.0f;
    float throttleLiftTime = 0.0f;
    float previousThrottleLevel = 0.0f;
    float filteredThrottleLoadRate = 0.0f;
    float throttleLiftBlend = 0.0f;
    int lastThrottlePulse = 1500;
    bool throttleReady = false;

    uint8_t controlPhase = 0;
    float settledBlend = 0.0f;
    float transitionAuthorityBlend = 0.0f;

    float huntBaselineYaw = 0.0f;
    float huntHalfCyclePeak = 0.0f;
    float huntAmplitude = 0.0f;
    float huntCrossingAge = 0.0f;
    float huntConfidence = 0.0f;
    float huntSuppression = 0.0f;
    float huntFrequency = 0.0f;
    int8_t huntResidualSign = 0;
    bool huntBaselineReady = false;

    float predictedYawTelemetry = 0.0f;
    float driftReferenceTelemetry = 0.0f;
    float referenceErrorTelemetry = 0.0f;
    float referenceLockTelemetry = 0.0f;
    float throttlePredictionTelemetry = 0.0f;
    float directCorrectionTelemetry = 0.0f;
    float memoryFeedbackTelemetry = 0.0f;
    float driverActivityTelemetry = 0.0f;
    float throttlePredictionBlendTelemetry = 0.0f;
    float throttleLiftBlendTelemetry = 0.0f;
    float huntSuppressionTelemetry = 0.0f;
    float huntFrequencyTelemetry = 0.0f;
    float transitionAuthorityTelemetry = 0.0f;

    int servoOutput = 1500;
    int correctionOutput = 0;

    bool calibrated = false;
    uint32_t lastUpdateMicros = 0;

    void resetDynamicState();
};
