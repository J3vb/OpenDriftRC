#include "BatterySense.h"


bool BatterySense::configure(
    uint8_t gpio,
    float newScale
)
{
    bool pinChanged = gpio != pin;
    bool scaleChanged = newScale != scale;

    if(!pinChanged && !scaleChanged)
    {
        return false;
    }

    scale = newScale;
    sampleValid = false;
    volts = 0.0f;
    pinMillivolts = 0;

    if(pinChanged)
    {
        pin = gpio;
        settleRemaining = SETTLE_SAMPLES;
        hasSampled = false;

        if(pin != 0)
        {
            pinMode(pin, INPUT);
            analogSetPinAttenuation(pin, ADC_11db);
        }
    }

    return true;
}


bool BatterySense::update()
{
    if(pin == 0)
    {
        sampleValid = false;
        return false;
    }

    uint32_t now = millis();

    if(hasSampled && now - lastSampleMs < SAMPLE_INTERVAL_MS)
    {
        return false;
    }

    lastSampleMs = now;
    hasSampled = true;

    if(settleRemaining > 0)
    {
        // Clear any pull left behind by a previous owner of the pin before
        // trusting a reading.
        pinMode(pin, INPUT);
        analogReadMilliVolts(pin);
        settleRemaining--;
        return false;
    }

    pinMillivolts = analogReadMilliVolts(pin);
    volts = (float)pinMillivolts * scale / 1000.0f;
    sampleValid = true;

    return true;
}


bool BatterySense::isEnabled() const
{
    return pin != 0;
}


bool BatterySense::hasSample() const
{
    return sampleValid;
}


float BatterySense::getVolts() const
{
    return volts;
}


uint32_t BatterySense::getPinMillivolts() const
{
    return pinMillivolts;
}


uint8_t BatterySense::getPin() const
{
    return pin;
}
