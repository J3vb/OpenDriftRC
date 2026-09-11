// Arduino cores define these as macros (the ESP32 core keeps only
// constrain, AVR-style cores all five). Including the headers after them
// proves no member, parameter or constant name collides. The host shim is
// pulled in first so the standard headers it needs are past the macros.
#include <Arduino.h>
#include <Preferences.h>

#ifndef abs
#define abs(x) ((x) > 0 ? (x) : -(x))
#endif
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#ifndef constrain
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif
#define round(x) ((x) >= 0 ? (long)((x) + 0.5) : (long)((x) - 0.5))

#include "BatteryCompensation.h"
#include "BatterySense.h"
#include "Settings.h"
#include "BlackboxLogger.h"

int macroClashProbe()
{
    BatteryCompensation compensation;
    BatteryCompensation::Config config;

    config.enabled = true;
    config.sensorEnabled = true;

    compensation.configure(config);

    for(int i = 0; i < 100; i++)
    {
        compensation.update(8.4f, true, 1500, 0.02f);
    }

    return compensation.apply(1750);
}
