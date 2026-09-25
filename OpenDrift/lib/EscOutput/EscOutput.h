#pragma once

#include <Arduino.h>


// ESC pulses own a separate LEDC timer/channel from the steering driver so
// their different frame rates cannot affect one another.
class EscOutput
{
public:

    bool begin(
        int outputPin,
        int frequencyHz = 50
    );

    void end();

    void writeMicroseconds(
        float pulseUs
    );

    void configure(
        int centerPulse,
        bool reversed,
        int travelPercent,
        int quietBand
    );

    bool isActive() const;


private:

    static constexpr uint8_t LEDC_CHANNEL = 7;
    // ESP32-S3 LEDC supports at most 14-bit duty resolution in this Arduino
    // core: about 1.22 us at 50 Hz or 0.18 us at 333 Hz.
    static constexpr uint8_t LEDC_RESOLUTION_BITS = 14;

    int pin = -1;
    int frequency = 50;
    float ticksPerMicrosecond = 1.0f;
    float currentPulse = 1500.0f;
    int center = 1500;
    bool reversed = false;
    int travel = 100;
    int quiet = 0;
    bool active = false;
};
