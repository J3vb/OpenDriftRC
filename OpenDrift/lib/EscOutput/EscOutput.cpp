#include "EscOutput.h"

#include <esp32-hal-ledc.h>


bool EscOutput::begin(
    int outputPin,
    int frequencyHz
)
{
    end();

    pin = outputPin;
    frequency = constrain(frequencyHz, 50, 333);

    uint32_t configuredFrequency =
        ledcSetup(
            LEDC_CHANNEL,
            frequency,
            LEDC_RESOLUTION_BITS
        );

    if(configuredFrequency == 0)
    {
        pin = -1;
        return false;
    }

    frequency = (int)configuredFrequency;
    ticksPerMicrosecond =
        ((float)(1UL << LEDC_RESOLUTION_BITS) * frequency)
        /
        1000000.0f;

    ledcAttachPin(
        pin,
        LEDC_CHANNEL
    );

    active = true;
    writeMicroseconds(center);

    Serial.print("ESC PWM attached GPIO ");
    Serial.println(pin);

    return true;
}


void EscOutput::end()
{
    if(!active)
    {
        return;
    }

    ledcWrite(
        LEDC_CHANNEL,
        0
    );

    ledcDetachPin(pin);

    active = false;
    pin = -1;
}


void EscOutput::writeMicroseconds(
    float pulseUs
)
{
    if(!active)
    {
        return;
    }

    float correction =
        pulseUs - 1500.0f;

    if(reversed)
    {
        correction = -correction;
    }

    correction =
        (correction * travel) / 100;

    float target =
        constrain(
            center + correction,
            1000.0f,
            2000.0f
        );

    if(
        quiet > 0 &&
        fabsf(target - currentPulse) <= quiet
    )
    {
        return;
    }

    currentPulse = target;

    constexpr uint32_t maxDuty =
        (1UL << LEDC_RESOLUTION_BITS) - 1UL;

    uint32_t duty =
        (uint32_t)roundf(
            currentPulse * ticksPerMicrosecond
        );

    duty = min(duty, maxDuty);

    ledcWrite(
        LEDC_CHANNEL,
        duty
    );
}


void EscOutput::configure(
    int centerPulse,
    bool reversedValue,
    int travelPercent,
    int quietBand
)
{
    center = constrain(centerPulse, 1000, 2000);
    reversed = reversedValue;
    travel = constrain(travelPercent, 1, 100);
    quiet = constrain(quietBand, 0, 50);
}


bool EscOutput::isActive() const
{
    return active;
}
