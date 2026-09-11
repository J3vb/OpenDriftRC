#pragma once

#include <Arduino.h>
#include <Preferences.h>

class Settings
{
public:

    static constexpr uint8_t MAX_PROFILES = 12;
    static constexpr size_t PROFILE_NAME_LENGTH = 24;
    static constexpr size_t WIFI_SSID_LENGTH = 33;   // 32 characters + NUL
    static constexpr size_t BACKGROUND_NAME_LENGTH = 24;   // 23 characters + NUL

    struct DrivingProfile
    {
        uint32_t version = 10;
        char name[PROFILE_NAME_LENGTH] = {0};

        float gain = 1.5f;
        float deadband = 2.0f;
        float gyroSmoothing = 0.10f;
        float gyroIntegralGain = 0.0f;

        int32_t gyroMaxCorrection = 25;
        int32_t gyroIntegralLimit = 120;
        int32_t gyroHoldBoost = 0;
        int32_t predictionStrength = 0;
        int32_t radioSteeringTravel = 100;
        int32_t gyroCounterSteerAssist = 0;
        int32_t gyroTransitionSpeed = 50;
        int32_t gyroHuntStrength = 50;
    };

    bool begin();

    void update();

    // Write pending changes now instead of waiting for the deferred
    // save. Used right before a restart.
    void flush();

    // Erase every key in the open preferences namespace. The caller must
    // restart immediately: the in-memory copy is left as is and would be
    // written back by the next save.
    void factoryReset();

    static const char* defaultWifiSsid();

    // Gyro
    float getGain();
    void setGain(float value);

    float getDeadband();
    void setDeadband(float value);

    bool getGyroReverse();
    void setGyroReverse(bool value);

    int getGyroMaxCorrection();
    void setGyroMaxCorrection(int value);

    float getGyroSmoothing();
    void setGyroSmoothing(float value);

    // 0 = 24 Hz (QMI mode 0), 1 = 120 Hz (mode 3), 2 = hardware LPF off.
    uint8_t getGyroLpfMode();
    void setGyroLpfMode(uint8_t value);

    float getGyroIntegralGain();
    void setGyroIntegralGain(float value);

    int getGyroIntegralLimit();
    void setGyroIntegralLimit(int value);

    int getGyroHoldBoost();
    void setGyroHoldBoost(int value);

    int getGyroCounterSteerAssist();
    void setGyroCounterSteerAssist(int value);

    int getGyroTransitionSpeed();
    void setGyroTransitionSpeed(int value);

    int getPredictionStrength();
    void setPredictionStrength(int value);

    int getGyroHuntStrength();
    void setGyroHuntStrength(int value);

    // Servo
    int getServoCenter();
    void setServoCenter(int value);

    bool getServoReverse();
    void setServoReverse(bool value);

    int getServoTravel();
    void setServoTravel(int value);

    int getServoQuiet();
    void setServoQuiet(int value);

    uint16_t getControlLoopHz();
    void setControlLoopHz(uint16_t value);

    // WiFi
    bool getWifiEnabled();
    void setWifiEnabled(bool value);

    uint32_t getWifiTimeout();
    void setWifiTimeout(uint32_t value);

    // Access point name. The returned pointer is stable for the life of
    // the object and never empty: an unset or cleared value holds the
    // default "OpenDrift". A new name takes effect the next time the
    // access point starts.
    const char* getWifiSsid();
    void setWifiSsid(const String& value);
    static String sanitizeWifiSsid(const String& value);

    // Blackbox
    bool getBlackboxEnabled();
    void setBlackboxEnabled(bool value);

    // Display (AMOLED). Brightness in percent, steps of 10, 10-100.
    uint8_t getDisplayBrightness();
    void setDisplayBrightness(int value);

    // Seconds without a touch before the AMOLED dims. 0 = never.
    uint16_t getDisplayDimTimeout();
    void setDisplayDimTimeout(int value);

    // Rotates the rendered UI and the touch input by 180 degrees, for a
    // board mounted upside down. Purely cosmetic: the gyro has its own
    // reverse setting and must not be changed with this one.
    bool getDisplayFlip();
    void setDisplayFlip(bool value);

    // Name of the stored AMOLED background image, "" for the built-in
    // one. Letters, digits, - and _ only; the returned pointer is stable.
    const char* getBackgroundName();
    void setBackgroundName(const String& value);
    static String sanitizeBackgroundName(const String& value);

    // AMOLED theme. Text 0 = light text (default), 1 = dark text for light
    // backgrounds. Accent selects a preset for headers, buttons and
    // highlights; 0 keeps the original mixed colours.
    static constexpr uint8_t THEME_ACCENT_COUNT = 7;
    static const char* themeAccentName(uint8_t accent);
    uint8_t getThemeText();
    void setThemeText(int value);
    uint8_t getThemeAccent();
    void setThemeAccent(int value);

    // Radio
    int getSteeringMin();
    void setSteeringMin(int value);

    int getSteeringCenter();
    void setSteeringCenter(int value);

    int getSteeringMax();
    void setSteeringMax(int value);

    uint8_t getSteeringCalibrationMask();
    bool isSteeringCalibrated();
    int getSteeringCapturedPulse(uint8_t point);
    int getSteeringCapturedInputPulse(uint8_t point);
    bool captureSteeringCalibrationPoint(
        uint8_t point,
        int physicalPulse,
        int inputPulse = 0
    );
    bool confirmStoredSteeringCalibration();
    void clearSteeringCalibration();

    int getRadioSteeringTravel();
    void setRadioSteeringTravel(int value);

    int getGainMin();
    void setGainMin(int value);

    int getGainMax();
    void setGainMax(int value);

    float getChannel3GainMin();
    void setChannel3GainMin(float value);

    float getChannel3GainMax();
    void setChannel3GainMax(float value);

    bool getThrottleOutputEnabled();
    void setThrottleOutputEnabled(bool value);

    // CRSF auxiliary receiver-style PWM outputs. A value of zero disables
    // the GPIO; values 1-16 select the corresponding CRSF radio channel.
    uint8_t getAuxChannelForGpio(uint8_t gpio);
    void setAuxChannelForGpio(uint8_t gpio, uint8_t channel);

    // Driving profiles
    uint8_t getProfileCount();
    int8_t getActiveProfileIndex();
    const char* getActiveProfileName();
    const DrivingProfile* getProfile(uint8_t index);

    int8_t createProfile(const String& name);
    bool activateProfile(uint8_t index);
    bool deleteProfile(uint8_t index);

    // Stores a profile with the given values, replacing one of the same
    // name. Values are clamped to the setter ranges. Replacing the active
    // profile deactivates it, because save() would otherwise overwrite the
    // import with the live tune. Returns the index, -1 for an unusable
    // name, -2 when the list is full.
    int8_t importProfile(
        const DrivingProfile& incoming,
        bool& replaced
    );

private:

    Preferences prefs;

    bool dirty = false;

    unsigned long lastSave = 0;

    // Stored values

    float gain = 1.5f;

    float deadband = 2.0f;

    bool gyroReverse = false;

    int gyroMaxCorrection = 25;

    float gyroSmoothing = 0.10f;

    uint8_t gyroLpfMode = 0;

    float gyroIntegralGain = 0.0f;

    int gyroIntegralLimit = 120;

    int gyroHoldBoost = 0;

    int gyroCounterSteerAssist = 0;

    int gyroTransitionSpeed = 50;

    int predictionStrength = 0;

    int gyroHuntStrength = 50;

    int servoCenter = 1500;

    bool servoReverse = false;

    int servoTravel = 100;

    int servoQuiet = 0;

    uint16_t controlLoopHz = 250;

    bool wifiEnabled = true;

    uint32_t wifiTimeout = 40000;

    char wifiSsid[WIFI_SSID_LENGTH] = {0};

    bool blackboxEnabled = false;

    uint8_t displayBrightness = 100;

    uint16_t displayDimTimeout = 0;

    bool displayFlip = false;

    char backgroundName[BACKGROUND_NAME_LENGTH] = {0};

    uint8_t themeText = 0;

    uint8_t themeAccent = 0;

    int steeringMin = 1000;

    int steeringCenter = 1500;

    int steeringMax = 2000;

    uint8_t steeringCalibrationMask = 0;

    int steeringCapturedPulses[3] = {1000, 1500, 2000};

    int steeringCapturedInputPulses[3] = {1000, 1500, 2000};

    int radioSteeringTravel = 100;

    int gainMin = 1000;

    int gainMax = 2000;

    float channel3GainMin = 0.5f;

    float channel3GainMax = 3.0f;

    bool throttleOutputEnabled = false;

    uint8_t auxChannels[8] = {0};

    DrivingProfile profiles[MAX_PROFILES];

    uint8_t profileCount = 0;

    int8_t activeProfileIndex = -1;

    void save();

    void loadProfiles();
    void captureProfile(DrivingProfile& profile);
    void applyProfile(const DrivingProfile& profile);
    bool persistProfile(uint8_t index);
    String sanitizeProfileName(const String& name);
    void applyWifiSsid(const String& value);
};
