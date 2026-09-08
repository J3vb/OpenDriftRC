// Runs the real Settings code against a fake NVS store to prove that saved
// profiles of every earlier layout survive the version 11 migration and
// that the battery fields persist and clamp.
#include "Settings.h"

#include <cstdio>
#include <map>
#include <string>

unsigned long fakeMillis = 0;

static std::map<std::string, FakePreferencesNamespace> fakeStores;

FakePreferencesNamespace& fakePreferencesStore(const char* name)
{
    return fakeStores[name];
}

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

    const char* NAMESPACE = "OpenDrift";

    struct LayoutV10
    {
        uint32_t version;
        char name[24];
        float gain;
        float deadband;
        float gyroSmoothing;
        float gyroIntegralGain;
        int32_t gyroMaxCorrection;
        int32_t gyroIntegralLimit;
        int32_t gyroHoldBoost;
        int32_t predictionStrength;
        int32_t radioSteeringTravel;
        int32_t gyroCounterSteerAssist;
        int32_t gyroTransitionSpeed;
        int32_t gyroHuntStrength;
    };

    struct LayoutV7
    {
        uint32_t version;
        char name[24];
        float gain;
        float deadband;
        float gyroSmoothing;
        float gyroIntegralGain;
        int32_t gyroMaxCorrection;
        int32_t gyroIntegralLimit;
        int32_t gyroHoldBoost;
        int32_t predictionStrength;
        int32_t radioSteeringTravel;
        int32_t gyroCounterSteerAssist;
        int32_t gyroTransitionSpeed;
        int32_t gyroHuntSensitivity;
        int32_t gyroHuntStrength;
    };

    struct LayoutV5
    {
        uint32_t version;
        char name[24];
        float gain;
        float deadband;
        float gyroSmoothing;
        float gyroIntegralGain;
        int32_t gyroMaxCorrection;
        int32_t gyroIntegralLimit;
        int32_t gyroHoldBoost;
        int32_t predictionStrength;
        int32_t radioSteeringTravel;
        int32_t gyroCounterSteerAssist;
        int32_t gyroTransitionSpeed;
    };

    static_assert(sizeof(LayoutV10) == 76, "test layout V10");
    static_assert(sizeof(LayoutV7) == 80, "test layout V7");
    static_assert(sizeof(LayoutV5) == 72, "test layout V5");
    static_assert(sizeof(Settings::DrivingProfile) == 116, "current layout");

    template<typename T>
    void putBlob(const char* key, const T& blob)
    {
        std::vector<uint8_t> bytes((const uint8_t*)&blob, (const uint8_t*)&blob + sizeof(T));
        fakePreferencesStore(NAMESPACE)[key] = bytes;
    }

    template<typename T>
    void putScalar(const char* key, T value)
    {
        std::vector<uint8_t> bytes((const uint8_t*)&value, (const uint8_t*)&value + sizeof(T));
        fakePreferencesStore(NAMESPACE)[key] = bytes;
    }

    LayoutV10 makeV10(uint32_t version, const char* name, int32_t maxCorrection, int32_t hunt)
    {
        LayoutV10 blob = {};
        blob.version = version;
        std::snprintf(blob.name, sizeof(blob.name), "%s", name);
        blob.gain = 1.7f;
        blob.deadband = 3.0f;
        blob.gyroSmoothing = 0.05f;
        blob.gyroIntegralGain = 0.1f;
        blob.gyroMaxCorrection = maxCorrection;
        blob.gyroIntegralLimit = 90;
        blob.gyroHoldBoost = 10;
        blob.predictionStrength = 20;
        blob.radioSteeringTravel = 95;
        blob.gyroCounterSteerAssist = 30;
        blob.gyroTransitionSpeed = 55;
        blob.gyroHuntStrength = hunt;
        return blob;
    }

    void seedStore()
    {
        fakeStores.clear();

        putBlob("prof0", makeV10(10, "Asphalt", 40, 60));
        putBlob("prof1", makeV10(9, "Carpet", 60, 65));
        putBlob("prof2", makeV10(8, "P-Tile", 370, 70));
        putBlob("prof3", makeV10(6, "Low Grip", 370, 99));

        LayoutV7 v7 = {};
        v7.version = 7;
        std::snprintf(v7.name, sizeof(v7.name), "%s", "High Grip");
        v7.gain = 2.1f;
        v7.deadband = 4.0f;
        v7.gyroSmoothing = 0.02f;
        v7.gyroMaxCorrection = 500;
        v7.gyroIntegralLimit = 80;
        v7.predictionStrength = 5;
        v7.radioSteeringTravel = 100;
        v7.gyroTransitionSpeed = 50;
        v7.gyroHuntSensitivity = 40;
        v7.gyroHuntStrength = 70;
        putBlob("prof4", v7);

        LayoutV5 v5 = {};
        v5.version = 5;
        std::snprintf(v5.name, sizeof(v5.name), "%s", "Old Five");
        v5.gain = 1.2f;
        v5.deadband = 2.0f;
        v5.gyroSmoothing = 0.1f;
        v5.gyroMaxCorrection = 250;
        v5.gyroIntegralLimit = 120;
        v5.radioSteeringTravel = 100;
        v5.gyroTransitionSpeed = 50;
        putBlob("prof5", v5);

        std::vector<uint8_t> garbage(50, 0xAB);
        fakePreferencesStore(NAMESPACE)["prof6"] = garbage;

        putScalar<uint8_t>("profCnt", 7);
        putScalar<int8_t>("profAct", 1);
    }

    void checkProfileDefaults(const Settings::DrivingProfile* profile, const char* label)
    {
        CHECK(profile->version == 11, "%s version %u", label, (unsigned)profile->version);
        CHECK(profile->batteryCompEnabled == 0, "%s battery enabled seeded", label);
        CHECK(profile->batteryCompStartVoltage == 8.4f, "%s start %.2f", label, (double)profile->batteryCompStartVoltage);
        CHECK(profile->batteryCompEndVoltage == 7.4f, "%s end %.2f", label, (double)profile->batteryCompEndVoltage);
        CHECK(profile->batteryCompStrength == 100, "%s strength %d", label, (int)profile->batteryCompStrength);
        CHECK(profile->batteryCompCurve == 0, "%s curve %d", label, (int)profile->batteryCompCurve);
        CHECK(profile->batteryCompKnee == 50, "%s knee %d", label, (int)profile->batteryCompKnee);
        CHECK(profile->batteryCompFilterMs == 2000, "%s filter %d", label, (int)profile->batteryCompFilterMs);
        CHECK(profile->batteryCompDropMs == 1000, "%s drop %d", label, (int)profile->batteryCompDropMs);
        CHECK(profile->batteryCompRecoveryMs == 10000, "%s recovery %d", label, (int)profile->batteryCompRecoveryMs);
        CHECK(profile->batteryCompUseResting == 1, "%s resting %d", label, (int)profile->batteryCompUseResting);
    }

    void testMigration()
    {
        seedStore();

        Settings settings;
        settings.begin();

        CHECK(settings.getProfileCount() == 6, "profile count %d", settings.getProfileCount());

        const Settings::DrivingProfile* asphalt = settings.getProfile(0);
        const Settings::DrivingProfile* carpet = settings.getProfile(1);
        const Settings::DrivingProfile* ptile = settings.getProfile(2);
        const Settings::DrivingProfile* lowGrip = settings.getProfile(3);
        const Settings::DrivingProfile* highGrip = settings.getProfile(4);
        const Settings::DrivingProfile* oldFive = settings.getProfile(5);

        CHECK(asphalt && std::string(asphalt->name) == "Asphalt", "profile 0 name");
        CHECK(carpet && std::string(carpet->name) == "Carpet", "profile 1 name");
        CHECK(ptile && std::string(ptile->name) == "P-Tile", "profile 2 name");
        CHECK(lowGrip && std::string(lowGrip->name) == "Low Grip", "profile 3 name");
        CHECK(highGrip && std::string(highGrip->name) == "High Grip", "profile 4 name");
        CHECK(oldFive && std::string(oldFive->name) == "Old Five", "profile 5 name");

        if(!(asphalt && carpet && ptile && lowGrip && highGrip && oldFive))
        {
            return;
        }

        CHECK(asphalt->gyroMaxCorrection == 40, "v10 max correction kept: %d", (int)asphalt->gyroMaxCorrection);
        CHECK(carpet->gyroMaxCorrection == 30, "v9 max correction converted: %d", (int)carpet->gyroMaxCorrection);
        CHECK(ptile->gyroMaxCorrection == 37, "v8 max correction converted: %d", (int)ptile->gyroMaxCorrection);
        CHECK(lowGrip->gyroMaxCorrection == 37, "v6 max correction converted: %d", (int)lowGrip->gyroMaxCorrection);
        CHECK(lowGrip->gyroHuntStrength == 50, "v6 hunt strength defaulted: %d", (int)lowGrip->gyroHuntStrength);
        CHECK(asphalt->gyroHuntStrength == 60 && carpet->gyroHuntStrength == 65 && ptile->gyroHuntStrength == 70, "hunt strength copied");
        CHECK(highGrip->gyroMaxCorrection == 50 && highGrip->gyroHuntStrength == 70, "v7 fields: max %d hunt %d", (int)highGrip->gyroMaxCorrection, (int)highGrip->gyroHuntStrength);
        CHECK(oldFive->gain == 1.2f, "v5 gain %.2f", (double)oldFive->gain);
        CHECK(asphalt->gain == 1.7f && asphalt->gyroTransitionSpeed == 55 && asphalt->gyroCounterSteerAssist == 30, "v10 tune copied");

        checkProfileDefaults(asphalt, "Asphalt");
        checkProfileDefaults(highGrip, "High Grip");
        checkProfileDefaults(oldFive, "Old Five");

        for(int i = 0; i < 6; i++)
        {
            char key[8];
            std::snprintf(key, sizeof(key), "prof%d", i);
            CHECK(fakePreferencesStore(NAMESPACE)[key].size() == 116, "%s rewritten as %zu bytes", key, fakePreferencesStore(NAMESPACE)[key].size());
        }

        CHECK(settings.getActiveProfileIndex() == 1, "active index %d", settings.getActiveProfileIndex());
        // Boot keeps the live tune from the global keys; activation applies the profile.
        CHECK(settings.activateProfile(1), "activate 1");
        CHECK(settings.getGain() == 1.7f && settings.getGyroMaxCorrection() == 30, "active profile applied: gain %.2f max %d", (double)settings.getGain(), settings.getGyroMaxCorrection());
        CHECK(!settings.getBatteryCompEnabled() && settings.getBatteryCompStrength() == 100, "battery defaults live");
        CHECK(settings.getBatterySensePin() == 0 && settings.getBatteryVoltageScale() == 4.133f && !settings.getBatteryThrottleReversed(), "hardware defaults");
    }

    void testPersistence()
    {
        seedStore();

        Settings settings;
        settings.begin();

        settings.setBatteryCompEnabled(true);
        settings.setBatteryCompStartVoltage(8.23f);
        settings.setBatteryCompEndVoltage(7.5f);
        settings.setBatteryCompStrength(80);
        settings.setBatteryCompCurve(2);
        settings.setBatteryCompKnee(60);
        settings.setBatteryCompFilterMs(4000);
        settings.setBatteryCompDropMs(700);
        settings.setBatteryCompRecoveryMs(20000);
        settings.setBatteryCompUseResting(false);
        settings.setBatterySensePin(8);
        settings.setBatteryVoltageScale(4.2f);
        settings.setBatteryThrottleReversed(true);

        CHECK(settings.getBatteryCompStartVoltage() == 8.2f, "start rounded to tenths: %.3f", (double)settings.getBatteryCompStartVoltage());
        CHECK(settings.getBatteryCompFilterMs() == 5000, "filter snapped to preset: %d", settings.getBatteryCompFilterMs());

        fakeMillis = 5000;
        settings.update();

        FakePreferencesNamespace& store = fakePreferencesStore(NAMESPACE);
        const char* keys[] = {"batEnabled", "batStartV", "batEndV", "batStrength", "batCurve", "batKnee", "batFilterMs", "batDropMs", "batRiseMs", "batResting", "batPin", "batScale", "batThrRev"};
        for(const char* key : keys)
        {
            CHECK(store.count(key) == 1, "key %s not saved", key);
            CHECK(std::strlen(key) <= 15, "key %s longer than NVS allows", key);
        }

        Settings reloaded;
        reloaded.begin();

        CHECK(reloaded.getBatteryCompEnabled(), "enabled not persisted");
        CHECK(reloaded.getBatteryCompStartVoltage() == 8.2f, "start not persisted: %.2f", (double)reloaded.getBatteryCompStartVoltage());
        CHECK(reloaded.getBatteryCompEndVoltage() == 7.5f, "end not persisted");
        CHECK(reloaded.getBatteryCompStrength() == 80, "strength not persisted");
        CHECK(reloaded.getBatteryCompCurve() == 2, "curve not persisted");
        CHECK(reloaded.getBatteryCompKnee() == 60, "knee not persisted");
        CHECK(reloaded.getBatteryCompFilterMs() == 5000, "filter not persisted");
        CHECK(reloaded.getBatteryCompDropMs() == 700, "drop not persisted");
        CHECK(reloaded.getBatteryCompRecoveryMs() == 20000, "recovery not persisted");
        CHECK(!reloaded.getBatteryCompUseResting(), "resting toggle not persisted");
        CHECK(reloaded.getBatterySensePin() == 8, "pin not persisted: %d", reloaded.getBatterySensePin());
        CHECK(reloaded.getBatteryVoltageScale() == 4.2f, "scale not persisted");
        CHECK(reloaded.getBatteryThrottleReversed(), "reversed not persisted");

        const Settings::DrivingProfile* carpet = reloaded.getProfile(1);
        CHECK(carpet && carpet->batteryCompEnabled == 1 && carpet->batteryCompStrength == 80 && carpet->batteryCompKnee == 60, "active profile did not capture battery fields");

        const Settings::DrivingProfile* asphalt = reloaded.getProfile(0);
        CHECK(asphalt && asphalt->batteryCompEnabled == 0 && asphalt->batteryCompStrength == 100, "inactive profile changed");

        CHECK(reloaded.activateProfile(0), "activate 0");
        CHECK(!reloaded.getBatteryCompEnabled() && reloaded.getBatteryCompStrength() == 100 && reloaded.getBatteryCompUseResting(), "switching profile did not switch battery settings");
        CHECK(reloaded.getBatterySensePin() == 8 && reloaded.getBatteryThrottleReversed(), "hardware settings changed with profile");

        CHECK(reloaded.activateProfile(1), "activate 1");
        CHECK(reloaded.getBatteryCompEnabled() && reloaded.getBatteryCompStrength() == 80 && reloaded.getBatteryCompDropMs() == 700, "switching back did not restore");
    }

    void testClamping()
    {
        seedStore();

        Settings::DrivingProfile wild = {};
        wild.version = 11;
        std::snprintf(wild.name, sizeof(wild.name), "%s", "Wild");
        wild.gain = 1.5f;
        wild.batteryCompEnabled = 7;
        wild.batteryCompStartVoltage = 9.9f;
        wild.batteryCompEndVoltage = 1.0f;
        wild.batteryCompStrength = 999;
        wild.batteryCompCurve = 7;
        wild.batteryCompKnee = 200;
        wild.batteryCompFilterMs = 3333;
        wild.batteryCompDropMs = 1;
        wild.batteryCompRecoveryMs = 99999;
        wild.batteryCompUseResting = 5;
        putBlob("prof0", wild);

        Settings settings;
        settings.begin();
        CHECK(settings.activateProfile(0), "activate wild");
        CHECK(settings.getBatteryCompEnabled(), "enabled from nonzero");
        CHECK(settings.getBatteryCompStartVoltage() == 8.4f, "start clamped %.2f", (double)settings.getBatteryCompStartVoltage());
        CHECK(settings.getBatteryCompEndVoltage() == 7.0f, "end clamped %.2f", (double)settings.getBatteryCompEndVoltage());
        CHECK(settings.getBatteryCompStrength() == 100, "strength clamped %d", settings.getBatteryCompStrength());
        CHECK(settings.getBatteryCompCurve() == 2, "curve clamped %d", settings.getBatteryCompCurve());
        CHECK(settings.getBatteryCompKnee() == 90, "knee clamped %d", settings.getBatteryCompKnee());
        CHECK(settings.getBatteryCompFilterMs() == 2000, "filter snapped %d", settings.getBatteryCompFilterMs());
        CHECK(settings.getBatteryCompDropMs() == 500, "drop clamped %d", settings.getBatteryCompDropMs());
        CHECK(settings.getBatteryCompRecoveryMs() == 30000, "recovery clamped %d", settings.getBatteryCompRecoveryMs());
        CHECK(settings.getBatteryCompUseResting(), "resting from nonzero");

        settings.setBatterySensePin(3);
        CHECK(settings.getBatterySensePin() == 0, "disallowed pin accepted");
        settings.setBatterySensePin(5);
        CHECK(settings.getBatterySensePin() == 5, "GPIO 5 rejected");
        settings.setBatteryCompCurve(9);
        CHECK(settings.getBatteryCompCurve() == 2, "curve setter clamp");
        settings.setBatteryCompStartVoltage(1.0f);
        CHECK(settings.getBatteryCompStartVoltage() == 7.0f, "start setter clamp");
        settings.setBatteryVoltageScale(0.0f);
        CHECK(settings.getBatteryVoltageScale() == 1.0f, "scale setter clamp");

        CHECK(Settings::isBatterySensePinAllowed(0), "pin 0");
        CHECK(Settings::isBatterySensePinAllowed(5) && Settings::isBatterySensePinAllowed(8), "pins 5 and 8");
        CHECK(!Settings::isBatterySensePinAllowed(4) && !Settings::isBatterySensePinAllowed(9) && !Settings::isBatterySensePinAllowed(18), "pins 4, 9, 18");
    }
}


int main()
{
    testMigration();
    testPersistence();
    testClamping();

    std::printf("%d checks, %d failures\n", checks, failures);

    return failures == 0 ? 0 : 1;
}
