#pragma once

#include <Arduino.h>


// Reads the pack voltage through a resistor divider on an ADC1 pin.
class BatterySense
{
public:

    // gpio 0 disables sensing. scale is pack volts per volt at the pin.
    // Returns true when the pin changed, so callers can reset their filters.
    bool configure(
        uint8_t gpio,
        float scale
    );

    // Takes at most one calibrated reading per interval. Returns true when a
    // new accepted sample is available.
    bool update();

    bool isEnabled() const;

    bool hasSample() const;

    float getVolts() const;

    uint32_t getPinMillivolts() const;

    uint8_t getPin() const;


private:

    static constexpr uint32_t SAMPLE_INTERVAL_MS = 20;

    // A pin released by another peripheral can carry a pull-down until the
    // ADC reconfigures it, so the first readings after a change are dropped.
    static constexpr uint8_t SETTLE_SAMPLES = 3;

    uint8_t pin = 0;
    float scale = 4.133f;
    uint32_t lastSampleMs = 0;
    uint32_t pinMillivolts = 0;
    float volts = 0.0f;
    bool sampleValid = false;
    uint8_t settleRemaining = 0;
};
