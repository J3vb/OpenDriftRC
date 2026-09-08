// Arduino.h defines these as macros. Including the module header after
// them proves no member or parameter name collides with them.
#define abs(x) ((x) > 0 ? (x) : -(x))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define round(x) ((x) >= 0 ? (long)((x) + 0.5) : (long)((x) - 0.5))

#include "BatteryCompensation.h"

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
