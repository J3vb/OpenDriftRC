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
        1000,
        2000
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



void ServoOutput::writeMicroseconds(int us)
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

    targetPulse = constrain(targetPulse, 900, 2100);

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
    currentPulse =
        constrain(
            centerPulse,
            1000,
            2000
        );

    servo.writeMicroseconds(
        currentPulse
    );
}



int ServoOutput::getPosition()
{
    return currentPulse;
}



void ServoOutput::configure(
    int centerPulseValue,
    bool reversedValue,
    int travelPercentValue,
    int quietBandValue,
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
}
