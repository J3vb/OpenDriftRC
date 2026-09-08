#include "BatteryCompensation.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

int macroClashProbe();

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
                if(failures <= 40) \
                { \
                    std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                    std::printf(__VA_ARGS__); \
                    std::printf("\n"); \
                } \
            } \
        } while(0)

    const float DT = 0.02f;

    BatteryCompensation::Config baseConfig()
    {
        BatteryCompensation::Config config;

        config.enabled = true;
        config.sensorEnabled = true;
        config.startVoltage = 8.4f;
        config.endVoltage = 7.4f;
        config.strengthPercent = 100;
        config.curve = BatteryCompensation::CURVE_LINEAR;
        config.kneePercent = 50;
        config.filterSeconds = 2.0f;
        config.dropSeconds = 1.0f;
        config.recoverySeconds = 10.0f;
        config.useResting = true;
        config.throttleReversed = false;

        return config;
    }

    void run(
        BatteryCompensation& compensation,
        float volts,
        bool valid,
        int throttle,
        float seconds
    )
    {
        int steps = (int)std::lround(seconds / DT);

        for(int i = 0; i < steps; i++)
        {
            compensation.update(volts, valid, throttle, DT);
        }
    }

    struct CurveVariant
    {
        int curve;
        int knee;
        const char* name;
    };

    const CurveVariant CURVES[] = {
        {BatteryCompensation::CURVE_LINEAR, 50, "linear"},
        {BatteryCompensation::CURVE_EXPO, 50, "expo"},
        {BatteryCompensation::CURVE_CUSTOM, 0, "custom0"},
        {BatteryCompensation::CURVE_CUSTOM, 50, "custom50"},
        {BatteryCompensation::CURVE_CUSTOM, 90, "custom90"},
    };

    struct VoltagePair
    {
        float start;
        float end;
    };

    const VoltagePair PAIRS[] = {
        {8.4f, 7.4f},
        {8.4f, 7.0f},
        {8.0f, 7.8f},
        {7.4f, 7.2f},
    };

    void testSweep()
    {
        for(const VoltagePair& pair : PAIRS)
        {
            for(int tenth = 50; tenth <= 95; tenth++)
            {
                float volts = (float)tenth / 10.0f;

                for(int strength = 0; strength <= 100; strength += 25)
                {
                    for(const CurveVariant& variant : CURVES)
                    {
                        for(int reversed = 0; reversed <= 1; reversed++)
                        {
                            BatteryCompensation compensation;
                            BatteryCompensation::Config config = baseConfig();

                            config.startVoltage = pair.start;
                            config.endVoltage = pair.end;
                            config.strengthPercent = strength;
                            config.curve = variant.curve;
                            config.kneePercent = variant.knee;
                            config.throttleReversed = reversed == 1;

                            compensation.configure(config);
                            run(compensation, volts, true, 1500, 3.0f);

                            int fullForward = reversed ? 1000 : 2000;

                            CHECK(
                                compensation.apply(fullForward) == fullForward,
                                "full throttle not preserved: V=%.1f str=%d %s rev=%d -> %d",
                                (double)volts, strength, variant.name, reversed,
                                compensation.apply(fullForward)
                            );

                            int previous = -1;
                            bool belowEnd = volts <= pair.end;

                            for(int input = 1000; input <= 2000; input++)
                            {
                                int output = compensation.apply(input);
                                int forwardIn = reversed ? 1500 - input : input - 1500;
                                int forwardOut = reversed ? 1500 - output : output - 1500;

                                if(previous >= 0)
                                {
                                    CHECK(
                                        output >= previous,
                                        "non-monotonic: V=%.1f str=%d %s rev=%d in=%d out=%d prev=%d",
                                        (double)volts, strength, variant.name, reversed,
                                        input, output, previous
                                    );
                                }

                                previous = output;

                                if(forwardIn <= 0)
                                {
                                    CHECK(
                                        output == input,
                                        "brake/neutral changed: in=%d out=%d rev=%d",
                                        input, output, reversed
                                    );
                                }
                                else
                                {
                                    CHECK(
                                        forwardOut <= forwardIn && forwardOut >= 0,
                                        "output exceeds input: in=%d out=%d rev=%d",
                                        input, output, reversed
                                    );
                                }

                                if(belowEnd || strength == 0)
                                {
                                    CHECK(
                                        output == input,
                                        "compensation below end voltage: V=%.1f end=%.1f str=%d in=%d out=%d",
                                        (double)volts, (double)pair.end, strength, input, output
                                    );
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void testReferenceValues()
    {
        BatteryCompensation compensation;
        compensation.configure(baseConfig());
        run(compensation, 8.3f, true, 1500, 3.0f);

        CHECK(compensation.getFault() == BatteryCompensation::FAULT_NONE, "fault %s", compensation.getFaultText());
        CHECK(std::fabs(compensation.getHealth() - 1.0f) < 1e-5f, "health %.4f", (double)compensation.getHealth());
        CHECK(std::fabs(compensation.getRestingVolts() - 8.3f) < 1e-4f, "resting %.4f", (double)compensation.getRestingVolts());
        CHECK(std::fabs(compensation.getCompensation() - 0.10714f) < 1e-3f, "c at 8.3 V = %.5f", (double)compensation.getCompensation());
        CHECK(compensation.apply(1750) == 1737, "50%% stick at 8.3 V -> %d", compensation.apply(1750));
        CHECK(compensation.apply(2000) == 2000, "full stick -> %d", compensation.apply(2000));
        CHECK(compensation.apply(1500) == 1500, "neutral -> %d", compensation.apply(1500));
        CHECK(compensation.apply(1200) == 1200, "brake -> %d", compensation.apply(1200));
        CHECK(compensation.apply(2012) == 2000, "CRSF 2012 clamps to %d", compensation.apply(2012));
        CHECK(compensation.apply(988) == 988 || compensation.apply(988) == 1000, "CRSF 988 -> %d", compensation.apply(988));

        BatteryCompensation reversed;
        BatteryCompensation::Config config = baseConfig();
        config.throttleReversed = true;
        reversed.configure(config);
        run(reversed, 8.3f, true, 1500, 3.0f);

        for(int k = 0; k <= 500; k += 25)
        {
            CHECK(
                reversed.apply(1500 - k) == 3000 - compensation.apply(1500 + k),
                "reversed mirror at k=%d: %d vs %d",
                k, reversed.apply(1500 - k), compensation.apply(1500 + k)
            );
            CHECK(reversed.apply(1500 + k) == 1500 + k, "reversed brake side changed at %d", 1500 + k);
        }

        std::printf("sample points (8.3 V, defaults) for web preview cross-check:\n");
        for(int input = 1500; input <= 2000; input += 50)
        {
            int output = compensation.apply(input);
            std::printf("  in=%d out=%d comp=%.2f%%\n", input, output, (double)compensation.getCompensationPercent());
        }
        std::printf("shape weights at t=0.5: linear %.4f expo %.4f custom50 %.4f custom90 %.4f\n",
            (double)BatteryCompensation::shapeWeight(0.5f, BatteryCompensation::CURVE_LINEAR, 0),
            (double)BatteryCompensation::shapeWeight(0.5f, BatteryCompensation::CURVE_EXPO, 0),
            (double)BatteryCompensation::shapeWeight(0.5f, BatteryCompensation::CURVE_CUSTOM, 50),
            (double)BatteryCompensation::shapeWeight(0.5f, BatteryCompensation::CURVE_CUSTOM, 90));
    }

    void testFirstSampleInit()
    {
        BatteryCompensation compensation;
        compensation.configure(baseConfig());

        CHECK(!compensation.isInitialised(), "initialised before any sample");
        CHECK(compensation.getFault() == BatteryCompensation::FAULT_SENSOR_OFF || compensation.getFault() == BatteryCompensation::FAULT_NO_SAMPLE, "initial fault %s", compensation.getFaultText());
        CHECK(compensation.apply(1750) == 1750, "compensation before first sample");

        compensation.update(8.12f, true, 1500, DT);

        CHECK(compensation.isInitialised(), "not initialised after first valid sample");
        CHECK(compensation.getFilteredVolts() == 8.12f, "filtered init %.4f", (double)compensation.getFilteredVolts());
        CHECK(compensation.getRestingVolts() == 8.12f, "resting init %.4f", (double)compensation.getRestingVolts());
        CHECK(std::fabs(compensation.getHealth() - DT) < 1e-6f, "health after first sample %.4f", (double)compensation.getHealth());
    }

    void testHealthRamps()
    {
        BatteryCompensation compensation;
        compensation.configure(baseConfig());

        float previousHealth = 0.0f;

        for(int i = 0; i < 80; i++)
        {
            compensation.update(8.4f, true, 1500, DT);
            float health = compensation.getHealth();
            CHECK(health - previousHealth <= DT + 1e-6f && health >= previousHealth, "ramp-in step %d: %.4f -> %.4f", i, (double)previousHealth, (double)health);
            previousHealth = health;
        }

        CHECK(std::fabs(previousHealth - 1.0f) < 1e-5f, "health after 1.6 s = %.4f", (double)previousHealth);

        float fullCompensation = compensation.getCompensation();
        float previousCompensation = fullCompensation;

        for(int i = 0; i < 80; i++)
        {
            compensation.update(0.0f, true, 1500, DT);
            float compensationNow = compensation.getCompensation();
            CHECK(compensation.getFault() == BatteryCompensation::FAULT_OUT_OF_RANGE, "fault during open sense: %s", compensation.getFaultText());
            CHECK(previousCompensation - compensationNow <= fullCompensation * DT + 1e-6f && compensationNow <= previousCompensation, "fade step %d: %.5f -> %.5f", i, (double)previousCompensation, (double)compensationNow);
            previousCompensation = compensationNow;
        }

        CHECK(compensation.getHealth() == 0.0f, "health after fault %.4f", (double)compensation.getHealth());
        CHECK(compensation.apply(1750) == 1750, "compensation not zero after fault: %d", compensation.apply(1750));
        CHECK(compensation.getFilteredVolts() == 8.4f, "filter moved on invalid samples: %.4f", (double)compensation.getFilteredVolts());

        BatteryCompensation::Config config = baseConfig();
        config.enabled = false;
        BatteryCompensation toggled;
        toggled.configure(baseConfig());
        run(toggled, 8.4f, true, 1500, 2.0f);
        toggled.configure(config);
        float before = toggled.getCompensation();
        toggled.update(8.4f, true, 1500, DT);
        CHECK(before - toggled.getCompensation() <= before * DT + 1e-6f, "disable stepped: %.5f -> %.5f", (double)before, (double)toggled.getCompensation());
        run(toggled, 8.4f, true, 1500, 1.5f);
        CHECK(toggled.getCompensation() == 0.0f, "compensation after disable %.5f", (double)toggled.getCompensation());
        CHECK(toggled.getFault() == BatteryCompensation::FAULT_NONE, "disabled is not a fault: %s", toggled.getFaultText());

        BatteryCompensation sensorOff;
        sensorOff.configure(baseConfig());
        run(sensorOff, 8.4f, true, 1500, 2.0f);
        config = baseConfig();
        config.sensorEnabled = false;
        sensorOff.configure(config);
        sensorOff.update(0.0f, false, 1500, DT);
        CHECK(sensorOff.getFault() == BatteryCompensation::FAULT_SENSOR_OFF, "sensor off fault: %s", sensorOff.getFaultText());
        CHECK(sensorOff.getHealth() > 0.9f, "sensor off stepped health to %.3f", (double)sensorOff.getHealth());
        run(sensorOff, 0.0f, false, 1500, 1.5f);
        CHECK(sensorOff.getHealth() == 0.0f, "sensor off did not fade out");
    }

    void testBurstSag()
    {
        BatteryCompensation resting;
        resting.configure(baseConfig());
        run(resting, 8.20f, true, 1500, 3.0f);

        float before = resting.getCompensation();
        float worst = 0.0f;

        // 0.6 s full throttle burst sagging to 7.50 V, then release with a
        // 0.3 s recovery of the remaining 0.28 V dip.
        for(int i = 0; i < 150; i++)
        {
            float t = (float)i * DT;
            float volts;
            int throttle;

            if(t < 0.6f)
            {
                volts = 7.50f;
                throttle = 2000;
            }
            else
            {
                volts = 8.20f - 0.28f * std::exp(-(t - 0.6f) / 0.3f);
                throttle = 1500;
            }

            resting.update(volts, true, throttle, DT);

            float delta = std::fabs(resting.getCompensation() - before);
            if(delta > worst) worst = delta;
        }

        CHECK(worst * 100.0f < 0.5f, "resting mode swung %.3f %% during burst", (double)(worst * 100.0f));
        CHECK(std::fabs(resting.getRestingVolts() - 8.20f) < 0.03f, "resting drifted to %.3f", (double)resting.getRestingVolts());

        BatteryCompensation filtered;
        BatteryCompensation::Config config = baseConfig();
        config.useResting = false;
        filtered.configure(config);
        run(filtered, 8.20f, true, 1500, 3.0f);
        run(filtered, 7.50f, true, 2000, 0.6f);

        // Documents the trade-off: a 1 s drop tau follows about 45 % of the dip.
        CHECK(std::fabs(filtered.getFilteredVolts() - 7.88f) < 0.03f, "filtered mode after burst %.3f (expected ~7.88)", (double)filtered.getFilteredVolts());
    }

    void testRejections()
    {
        BatteryCompensation threeCell;
        threeCell.configure(baseConfig());
        run(threeCell, 11.1f, true, 1500, 2.0f);
        CHECK(threeCell.getFault() == BatteryCompensation::FAULT_OUT_OF_RANGE, "3S fault %s", threeCell.getFaultText());
        CHECK(!threeCell.isInitialised(), "3S initialised the filter");
        CHECK(threeCell.apply(1750) == 1750, "3S compensated");

        BatteryCompensation badSettings;
        BatteryCompensation::Config config = baseConfig();
        config.startVoltage = 7.5f;
        config.endVoltage = 7.4f;
        badSettings.configure(config);
        run(badSettings, 8.4f, true, 1500, 2.0f);
        CHECK(badSettings.getFault() == BatteryCompensation::FAULT_BAD_SETTINGS, "bad settings fault %s", badSettings.getFaultText());
        CHECK(badSettings.apply(1750) == 1750, "bad settings compensated");

        config.startVoltage = 7.4f;
        config.endVoltage = 7.4f;
        badSettings.configure(config);
        badSettings.update(8.4f, true, 1500, DT);
        CHECK(badSettings.apply(1750) == 1750 && std::isfinite(badSettings.getCompensation()), "zero span blew up");

        BatteryCompensation clampDt;
        clampDt.configure(baseConfig());
        clampDt.update(8.4f, true, 1500, DT);
        clampDt.update(8.4f, true, 1500, 5.0f);
        CHECK(clampDt.getHealth() <= BatteryCompensation::MAX_DT_SECONDS + DT + 1e-6f, "dt not clamped: health %.3f", (double)clampDt.getHealth());
        clampDt.update(7.0f, true, 1500, 5.0f);
        float maxMove = 1.4f * (1.0f - std::exp(-BatteryCompensation::MAX_DT_SECONDS / 1.0f));
        CHECK(8.4f - clampDt.getFilteredVolts() <= maxMove + 1e-4f, "stalled loop snapped filter to %.3f", (double)clampDt.getFilteredVolts());

        BatteryCompensation lift;
        lift.configure(baseConfig());
        run(lift, 8.4f, true, 1500, 2.0f);
        run(lift, 7.9f, true, 1560, 2.0f);
        CHECK(std::fabs(lift.getRestingVolts() - 8.4f) < 1e-4f, "resting moved while throttle at 1560: %.3f", (double)lift.getRestingVolts());
        run(lift, 7.9f, true, 1540, 0.2f);
        CHECK(std::fabs(lift.getRestingVolts() - 8.4f) < 1e-4f, "resting moved before settle time: %.3f", (double)lift.getRestingVolts());
        run(lift, 7.9f, true, 1540, 3.0f);
        CHECK(lift.getRestingVolts() < 8.1f, "resting did not follow after settle: %.3f", (double)lift.getRestingVolts());
    }
}


int main()
{
    testSweep();
    testReferenceValues();
    testFirstSampleInit();
    testHealthRamps();
    testBurstSag();
    testRejections();

    CHECK(macroClashProbe() < 1750 && macroClashProbe() > 1700, "macro clash probe %d", macroClashProbe());

    std::printf("%d checks, %d failures\n", checks, failures);

    return failures == 0 ? 0 : 1;
}
