#pragma once

#include <stdint.h>

// Voltage-dependent throttle compensation. Pure arithmetic with no hardware
// or Arduino dependency so the same code builds and runs on a host.
class BatteryCompensation
{
public:

    enum Curve : int
    {
        CURVE_LINEAR = 0,
        CURVE_EXPO = 1,
        CURVE_CUSTOM = 2
    };

    enum Fault : int
    {
        FAULT_NONE = 0,
        FAULT_SENSOR_OFF = 1,
        FAULT_NO_SAMPLE = 2,
        FAULT_OUT_OF_RANGE = 3,
        FAULT_BAD_SETTINGS = 4
    };

    struct Config
    {
        bool enabled = false;
        float startVoltage = 8.4f;
        float endVoltage = 7.4f;
        int strengthPercent = 100;
        int curve = CURVE_LINEAR;
        int kneePercent = 50;
        float filterSeconds = 2.0f;
        float dropSeconds = 1.0f;
        float recoverySeconds = 10.0f;
        bool useResting = true;
        bool sensorEnabled = false;
        bool throttleReversed = false;
    };

    static constexpr float MIN_VALID_VOLTS = 5.5f;
    static constexpr float MAX_VALID_VOLTS = 9.2f;
    static constexpr float MIN_VOLTAGE_SPAN = 0.2f;
    static constexpr float MAX_DT_SECONDS = 0.1f;
    static constexpr float HEALTH_SLEW_PER_SECOND = 1.0f;
    static constexpr float LIFT_SETTLE_SECONDS = 0.25f;
    static constexpr int LIFT_BAND_US = 50;

    void configure(const Config& newConfig);

    const Config& getConfig() const;

    // Forget filter state, e.g. after the sense pin changes.
    void reset();

    // volts: pack voltage in volts. rawValid: false when no reading exists.
    // throttleInUs: the driver's raw throttle pulse. dtSeconds is clamped.
    void update(
        float volts,
        bool rawValid,
        int throttleInUs,
        float dtSeconds
    );

    // Maps the driver's throttle pulse to the ESC pulse, both in microseconds.
    int apply(int throttleInUs);

    // Fraction of the compensation that applies at a throttle position
    // (0..1). Shared by the host tests and mirrored by the web preview.
    static float shapeWeight(
        float throttle,
        int curve,
        int kneePercent
    );

    float getRawVolts() const;
    float getFilteredVolts() const;
    float getRestingVolts() const;
    float getSourceVolts() const;
    float getCompensation() const;
    float getCompensationPercent() const;
    float getHealth() const;
    int getLastInputUs() const;
    int getLastOutputUs() const;
    Fault getFault() const;
    const char* getFaultText() const;
    bool isInitialised() const;
    bool isLifted() const;

private:

    Config config;

    float rawVolts = 0.0f;
    float filteredVolts = 0.0f;
    float restingVolts = 0.0f;
    float health = 0.0f;
    float liftTimer = 0.0f;
    float compensation = 0.0f;
    float lastCompensationPercent = 0.0f;
    bool initialised = false;
    bool lifted = false;
    int lastInputUs = 1500;
    int lastOutputUs = 1500;
    Fault fault = FAULT_SENSOR_OFF;

    bool settingsValid() const;
    void updateCompensation();
};
