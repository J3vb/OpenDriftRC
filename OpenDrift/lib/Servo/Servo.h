#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>


class ServoOutput
{
public:

    bool begin(
        int pin,
        int frequencyHz = 50
    );

    void end();

    void writeMicroseconds(int us);

    void center();

    int getPosition();

    void noteCommandPulse(int us);

    int getCommandPosition();

    void configure(
        int centerPulse,
        bool reversed,
        int travelPercent,
        int quietBand,
        int speedPercent,
        bool endpointCalibrationActive = false,
        int leftEndpointPulse = 1000,
        int calibratedCenterPulse = 1500,
        int rightEndpointPulse = 2000
    );


private:

    int computePulse(int us);

    Servo servo;

    int currentPulse = 1500;

    int commandPulse = 1500;

    int centerPulse = 1500;

    bool reversed = false;

    int travelPercent = 100;

    int quietBand = 0;

    // 100 = no limit. Below that the output moves at most maxRate us per
    // second, like the "servo speed" setting on commercial drift gyros.
    int speedPercent = 100;
    float maxRateUsPerSecond = 0.0f;
    float limitedPulse = 1500.0f;
    uint32_t lastWriteMicros = 0;
    bool limiterWasClipping = false;

    bool endpointCalibrationActive = false;

    int leftEndpointPulse = 1000;

    int rightEndpointPulse = 2000;

    bool active = false;
};
