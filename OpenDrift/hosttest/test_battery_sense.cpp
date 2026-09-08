// BatterySense against a fake ADC: sample interval, settle discards, scale.
#include "BatterySense.h"

#include <cmath>
#include <cstdio>

unsigned long fakeMillis = 0;
uint32_t fakeAdcMillivolts = 0;
int fakePinModeCalls = 0;

namespace
{
    int failures = 0;
    int checks = 0;

    #define CHECK(condition, ...) \
        do \
        { \
            checks++; \
            if(!(condition)) \
            { \
                failures++; \
                std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                std::printf(__VA_ARGS__); \
                std::printf("\n"); \
            } \
        } while(0)
}

int main()
{
    BatterySense sense;

    CHECK(!sense.isEnabled() && !sense.hasSample(), "default state");
    CHECK(!sense.update(), "disabled sense produced a sample");

    fakeAdcMillivolts = 2032;
    fakeMillis = 1000;

    CHECK(sense.configure(8, 4.133f), "pin change not reported");
    CHECK(sense.isEnabled() && sense.getPin() == 8, "pin not stored");
    CHECK(!sense.configure(8, 4.133f), "same pin reported as change");

    int accepted = 0;
    for(int i = 0; i < 3; i++)
    {
        fakeMillis += 20;
        if(sense.update()) accepted++;
    }
    CHECK(accepted == 0 && !sense.hasSample(), "settle samples were accepted (%d)", accepted);

    fakeMillis += 20;
    CHECK(sense.update(), "fourth sample not accepted");
    CHECK(sense.hasSample(), "no sample after settle");
    CHECK(std::fabs(sense.getVolts() - 8.398f) < 0.005f, "volts %.3f", (double)sense.getVolts());
    CHECK(sense.getPinMillivolts() == 2032, "pin mV %u", (unsigned)sense.getPinMillivolts());

    CHECK(!sense.update(), "sampled again inside the interval");
    fakeMillis += 19;
    CHECK(!sense.update(), "sampled at 19 ms");
    fakeMillis += 1;
    fakeAdcMillivolts = 1800;
    CHECK(sense.update(), "did not sample at 20 ms");
    CHECK(std::fabs(sense.getVolts() - 7.439f) < 0.005f, "volts after change %.3f", (double)sense.getVolts());

    sense.configure(8, 4.2f);
    fakeMillis += 20;
    sense.update();
    CHECK(std::fabs(sense.getVolts() - 7.56f) < 0.005f, "scale change not applied: %.3f", (double)sense.getVolts());

    CHECK(sense.configure(5, 4.2f), "pin change to 5 not reported");
    CHECK(!sense.hasSample(), "sample kept across pin change");
    accepted = 0;
    for(int i = 0; i < 4; i++)
    {
        fakeMillis += 20;
        if(sense.update()) accepted++;
    }
    CHECK(accepted == 1, "settle after pin change accepted %d", accepted);

    CHECK(sense.configure(0, 4.2f), "disable not reported");
    CHECK(!sense.isEnabled() && !sense.hasSample() && !sense.update(), "disabled state");

    std::printf("%d checks, %d failures\n", checks, failures);

    return failures == 0 ? 0 : 1;
}
