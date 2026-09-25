#include "Servo.h"


bool ServoOutput::begin(
    int pin,
    int frequencyHz
)
{
    end();

    servo.setPeriodHertz(
        constrain(
            frequencyHz,
            50,
            333
        )
    );

    int channel =
        servo.attach(
        pin,
        900,
        2100
    );

    if(channel == 0)
    {
        return false;
    }

    active = true;


    center();


    Serial.print("Servo attached GPIO ");
    Serial.println(pin);


    return true;
}



void ServoOutput::end()
{
    if(!active)
    {
        return;
    }

    servo.detach();
    active = false;
}



int ServoOutput::computePulse(int us)
{
    us = constrain(us, 1000, 2000);

    int targetPulse = centerPulse;

    if(endpointCalibrationActive)
    {
        targetPulse = us <= 1500
            ? map(us, 1000, 1500, leftEndpointPulse, centerPulse)
            : map(us, 1500, 2000, centerPulse, rightEndpointPulse);
    }
    else
    {
        int correction = us - 1500;

        if(reversed)
        {
            correction = -correction;
        }

        correction = (correction * travelPercent) / 100;
        targetPulse = centerPulse + correction;
    }

    return constrain(targetPulse, 900, 2100);
}



void ServoOutput::writeMicroseconds(int us)
{
    int targetPulse =
        computePulse(us);

    uint32_t now = micros();

    if(speedPercent < 100)
    {
        float dt =
            lastWriteMicros == 0
            ? 0.004f
            : constrain((now - lastWriteMicros) / 1000000.0f, 0.0005f, 0.05f);

        float maxStep = maxRateUsPerSecond * dt;
        float delta = (float)targetPulse - limitedPulse;

        if(delta > maxStep)
        {
            delta = maxStep;
        }
        else if(delta < -maxStep)
        {
            delta = -maxStep;
        }

        limitedPulse += delta;
        targetPulse = (int)roundf(limitedPulse);
    }
    else
    {
        limitedPulse = (float)targetPulse;
    }

    lastWriteMicros = now;

    if(
        quietBand > 0 &&
        abs(targetPulse - currentPulse) <= quietBand
    )
    {
        return;
    }

    currentPulse =
        targetPulse;

    servo.writeMicroseconds(
        currentPulse
    );
}



void ServoOutput::center()
{
    // Failsafe and boot path: never slowed by the speed limit.
    currentPulse =
        constrain(
            centerPulse,
            900,
            2100
        );

    limitedPulse = (float)currentPulse;

    servo.writeMicroseconds(
        currentPulse
    );
}



int ServoOutput::getPosition()
{
    return currentPulse;
}



void ServoOutput::noteCommandPulse(int us)
{
    commandPulse =
        computePulse(us);
}



int ServoOutput::getCommandPosition()
{
    return commandPulse;
}



void ServoOutput::configure(
    int centerPulseValue,
    bool reversedValue,
    int travelPercentValue,
    int quietBandValue,
    int speedPercentValue,
    bool calibratedEndpointsActive,
    int leftEndpointValue,
    int calibratedCenterValue,
    int rightEndpointValue
)
{
    int candidateCenter = constrain(calibratedCenterValue, 900, 2100);
    int leftDelta = leftEndpointValue - candidateCenter;
    int rightDelta = rightEndpointValue - candidateCenter;

    endpointCalibrationActive =
        calibratedEndpointsActive &&
        abs(leftDelta) >= 10 &&
        abs(rightDelta) >= 10 &&
        leftDelta * rightDelta < 0;

    centerPulse = endpointCalibrationActive
        ? candidateCenter
        : constrain(centerPulseValue, 1000, 2000);

    leftEndpointPulse = constrain(leftEndpointValue, 900, 2100);
    rightEndpointPulse = constrain(rightEndpointValue, 900, 2100);

    reversed =
        reversedValue;

    travelPercent =
        constrain(
            travelPercentValue,
            1,
            100
        );

    if(travelPercentValue <= 0)
    {
        travelPercent = 100;
    }

    quietBand =
        constrain(
            quietBandValue,
            0,
            50
        );

    speedPercent =
        constrain(
            speedPercentValue,
            1,
            100
        );

    // 50 covers the full 1000 us span in about 0.15 s, 25 in about 0.6 s,
    // 10 in about 2 s. 100 is unlimited.
    maxRateUsPerSecond =
        200.0f + 2.5f * (float)speedPercent * (float)speedPercent;
}
