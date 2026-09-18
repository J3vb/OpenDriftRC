#include "Settings.h"

#include <stdio.h>

static const char DEFAULT_WIFI_SSID[] = "OpenDrift";

namespace
{
    int legacyMaxCorrectionToPercent(int value)
    {
        // Legacy values used center-to-endpoint microseconds. The current
        // percentage covers the complete endpoint-to-endpoint correction.
        return constrain((value + 5) / 10, 0, 100);
    }

    int centerSpanPercentToFullSpanPercent(int value)
    {
        return constrain((value + 1) / 2, 0, 100);
    }

    struct DrivingProfileV8
    {
        uint32_t version;
        char name[Settings::PROFILE_NAME_LENGTH];
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

    struct DrivingProfileV1
    {
        uint32_t version;
        char name[Settings::PROFILE_NAME_LENGTH];
        float gain;
        float deadband;
        float gyroSmoothing;
        float gyroIntegralGain;
        int32_t gyroMaxCorrection;
        int32_t gyroAttackSpeed;
        int32_t gyroReturnSpeed;
        int32_t gyroIntegralLimit;
        int32_t gyroHoldBoost;
        int32_t gyroAntiWobble;
        int32_t gyroHuntDamping;
        int32_t steeringDamper;
        int32_t radioSteeringTravel;
    };

    struct DrivingProfileV2
    {
        uint32_t version;
        char name[Settings::PROFILE_NAME_LENGTH];
        float gain;
        float deadband;
        float gyroSmoothing;
        float gyroIntegralGain;
        int32_t gyroMaxCorrection;
        int32_t gyroAttackSpeed;
        int32_t gyroReturnSpeed;
        int32_t gyroIntegralLimit;
        int32_t gyroHoldBoost;
        int32_t gyroAntiWobble;
        int32_t gyroHuntDamping;
        int32_t steeringDamper;
        int32_t radioSteeringTravel;
        int32_t gyroCounterSteerAssist;
    };

    struct DrivingProfileV3
    {
        uint32_t version;
        char name[Settings::PROFILE_NAME_LENGTH];
        float gain;
        float deadband;
        float gyroSmoothing;
        float gyroIntegralGain;
        int32_t gyroMaxCorrection;
        int32_t gyroAttackSpeed;
        int32_t gyroReturnSpeed;
        int32_t gyroIntegralLimit;
        int32_t gyroHoldBoost;
        int32_t gyroAntiWobble;
        int32_t gyroHuntDamping;
        int32_t steeringDamper;
        int32_t radioSteeringTravel;
        int32_t gyroCounterSteerAssist;
        int32_t gyroTailSlideSpeed;
    };

    struct DrivingProfileV4
    {
        uint32_t version;
        char name[Settings::PROFILE_NAME_LENGTH];
        float gain;
        float deadband;
        float gyroSmoothing;
        float gyroIntegralGain;
        int32_t gyroMaxCorrection;
        int32_t gyroAttackSpeed;
        int32_t gyroReturnSpeed;
        int32_t gyroIntegralLimit;
        int32_t gyroHoldBoost;
        int32_t gyroAntiWobble;
        int32_t gyroHuntDamping;
        int32_t steeringDamper;
        int32_t radioSteeringTravel;
        int32_t gyroCounterSteerAssist;
        int32_t gyroTailSlideSpeed;
    };

    struct DrivingProfileV5
    {
        uint32_t version;
        char name[Settings::PROFILE_NAME_LENGTH];
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

    struct DrivingProfileV6
    {
        uint32_t version;
        char name[Settings::PROFILE_NAME_LENGTH];
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
    };

    struct DrivingProfileV7
    {
        uint32_t version;
        char name[Settings::PROFILE_NAME_LENGTH];
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

    constexpr size_t largerSize(size_t left, size_t right)
    {
        return left > right ? left : right;
    }

    constexpr size_t LARGEST_STORED_PROFILE_SIZE =
        largerSize(
            largerSize(
                largerSize(
                    sizeof(Settings::DrivingProfile),
                    sizeof(DrivingProfileV8)
                ),
                largerSize(
                    sizeof(DrivingProfileV7),
                    sizeof(DrivingProfileV6)
                )
            ),
            largerSize(
                largerSize(
                    sizeof(DrivingProfileV5),
                    sizeof(DrivingProfileV4)
                ),
                largerSize(
                    sizeof(DrivingProfileV3),
                    largerSize(
                        sizeof(DrivingProfileV2),
                        sizeof(DrivingProfileV1)
                    )
                )
            )
        );
}

bool Settings::begin()
{
    #if defined(OPENDRIFT_ROUND_LOG51_TUNE)
    // Private round-display recovery build. Keep its calibration and tuning
    // isolated from both the normal CRSF and PWM firmware namespaces.
    bool storageReady = prefs.begin("OpenDriftR51", false);
    #elif defined(OPENDRIFT_INPUT_CRSF)
    // Keep experimental CRSF tuning completely separate from the RC1 PWM
    // build, even when both firmwares are flashed onto the same board.
    bool storageReady = prefs.begin("OpenDriftCRSF", false);
    #else
    bool storageReady = prefs.begin("OpenDrift", false);
    #endif

    if(!storageReady)
    {
        Serial.println("Settings: NVS open failed");
    }

    gain = constrain(
        prefs.getFloat(
            "gain",
            1.5f
        ),
        0.0f,
        6.0f
    );

    deadband = constrain(
        prefs.getFloat(
            "deadband",
            2.0f
        ),
        0.0f,
        100.0f
    );

    gyroReverse = prefs.getBool(
        "gyroRev",
        false
    );

    bool maxCorrectionUsesFullSpan =
        prefs.getBool("maxSpanV1", false);

    if(prefs.isKey("gyroMaxPct"))
    {
        gyroMaxCorrection = constrain(
            prefs.getInt("gyroMaxPct", 50),
            0,
            100
        );

        if(!maxCorrectionUsesFullSpan)
        {
            gyroMaxCorrection =
                centerSpanPercentToFullSpanPercent(
                    gyroMaxCorrection
                );
        }
    }
    else if(prefs.isKey("gyroMax"))
    {
        gyroMaxCorrection = legacyMaxCorrectionToPercent(
            prefs.getInt("gyroMax", 250)
        );
        prefs.putInt("gyroMaxPct", gyroMaxCorrection);
    }
    else
    {
        gyroMaxCorrection = 25;
    }

    if(!maxCorrectionUsesFullSpan)
    {
        prefs.putInt("gyroMaxPct", gyroMaxCorrection);
        prefs.putBool("maxSpanV1", true);
    }

    gyroSmoothing = constrain(
        prefs.getFloat(
            "gyroSmooth",
            0.10f
        ),
        0.0f,
        1.0f
    );

    gyroLpfMode = constrain(
        prefs.getUChar("gyroLpf", 0),
        0,
        2
    );

    gyroIntegralGain = constrain(
        prefs.getFloat("gyroIGain", 0.0f),
        0.0f,
        20.0f
    );

    gyroIntegralLimit = constrain(
        prefs.getInt("gyroILim", 120),
        0,
        500
    );

    gyroHoldBoost = constrain(
        prefs.getInt("gyroHold", 0),
        0,
        100
    );

    gyroCounterSteerAssist = constrain(
        prefs.getInt("counterAssist", 0),
        0,
        100
    );

    if(prefs.isKey("tailSpeedC"))
    {
        gyroTransitionSpeed = constrain(
            prefs.getInt("tailSpeedC", 50),
            0,
            100
        );
    }
    else
    {
        int legacyTailSlideSpeed = prefs.getInt("tailSpeed", 0);

        // Experimental v3 used 0 as the proven response and 100 as the
        // maximum release. Preserve that exact behavior in the centered
        // scale, where old 0 -> new 50 and old 100 -> new 100.
        gyroTransitionSpeed = constrain(
            50 + legacyTailSlideSpeed / 2,
            50,
            100
        );

        prefs.putInt("tailSpeedC", gyroTransitionSpeed);
    }

    if(prefs.isKey("prediction"))
    {
        predictionStrength = prefs.getInt("prediction", 0);
    }
    else
    {
        predictionStrength = prefs.getInt("gyroHunt", 0);
        prefs.putInt("prediction", predictionStrength);
    }

    predictionStrength = constrain(predictionStrength, 0, 100);

    gyroHuntStrength = constrain(
        prefs.getInt("huntStrength", 50),
        0,
        100
    );

    const char* retiredKeys[] = {
        "gyroAttack", "gyroReturn", "gyroWob", "gyroHunt",
        "strDamp", "huntSense", "terrainAssist"
    };

    for(const char* key : retiredKeys)
    {
        if(prefs.isKey(key)) prefs.remove(key);
    }

    servoCenter = constrain(
        prefs.getInt(
            "center",
            1500
        ),
        1000,
        2000
    );

    servoReverse = prefs.getBool(
        "reverse",
        false
    );

    servoTravel = constrain(
        prefs.getInt(
            "travel",
            100
        ),
        1,
        100
    );

    servoQuiet = constrain(
        prefs.getInt("quiet", 0),
        0,
        50
    );

    controlLoopHz =
        prefs.getUShort("loopHz", 250) == 333
        ? 333
        : 250;

    wifiEnabled = prefs.getBool(
        "wifi",
        true
    );

    wifiTimeout = prefs.getULong(
        "timeout",
        40000
    );

    if(wifiTimeout != 0)
    {
        wifiTimeout =
            constrain(
                wifiTimeout,
                5000UL,
                3600000UL
            );
    }

    applyWifiSsid(
        prefs.getString(
            "wifiSsid",
            ""
        )
    );

    blackboxEnabled = prefs.getBool(
        "blackbox",
        false
    );

    displayBrightness = constrain(
        prefs.getUChar(
            "dispBright",
            100
        ),
        10,
        100
    );

    displayDimTimeout = constrain(
        prefs.getUShort(
            "dispDimS",
            0
        ),
        0,
        600
    );

    displayFlip = prefs.getBool(
        "dispFlip",
        false
    );

    {
        String storedBackground =
            sanitizeBackgroundName(
                prefs.getString(
                    "bgName",
                    ""
                )
            );

        snprintf(
            backgroundName,
            sizeof(backgroundName),
            "%s",
            storedBackground.c_str()
        );
    }

    themeText = constrain(
        prefs.getUChar(
            "thmText",
            0
        ),
        0,
        1
    );

    themeAccent = constrain(
        prefs.getUChar(
            "thmAccent",
            0
        ),
        0,
        THEME_ACCENT_COUNT - 1
    );

    // v1.0.7b stored receiver input endpoints under the steering keys. They
    // cannot safely be reused as physical servo stops, so only the new servo
    // endpoint schema is accepted as calibrated.
    applyFallbackSteeringEndpoints();
    steeringCapturedPulses[0] = constrain(prefs.getInt("servoCalL", steeringMin), 900, 2100);
    steeringCapturedPulses[1] = constrain(prefs.getInt("servoCalC", steeringCenter), 900, 2100);
    steeringCapturedPulses[2] = constrain(prefs.getInt("servoCalR", steeringMax), 900, 2100);
    steeringCapturedInputPulses[0] = constrain(prefs.getInt("servoInL", 1000), 800, 2200);
    steeringCapturedInputPulses[1] = constrain(prefs.getInt("servoInC", 1500), 800, 2200);
    steeringCapturedInputPulses[2] = constrain(prefs.getInt("servoInR", 2000), 800, 2200);
    steeringCalibrationMask = prefs.getBool("servoEndV1", false)
        ? (prefs.getUChar("servoCalM", 0) & 0x07)
        : 0;

    // Only a complete, valid capture set becomes the live endpoints. A
    // partial or rejected capture stays visible on the endpoint pages but
    // the servo keeps using the fallback geometry until it is completed.
    if(
        steeringCalibrationMask == 0x07 &&
        steeringStopsValid(
            steeringCapturedPulses[0],
            steeringCapturedPulses[1],
            steeringCapturedPulses[2]
        )
    )
    {
        steeringMin = steeringCapturedPulses[0];
        steeringCenter = steeringCapturedPulses[1];
        steeringMax = steeringCapturedPulses[2];
    }
    else if(steeringCalibrationMask == 0x07)
    {
        steeringCalibrationMask = 0;
    }

    radioSteeringTravel = constrain(
        prefs.getInt("strTravel", 100),
        0,
        100
    );

    gainMin = constrain(
        prefs.getInt(
            "gainMin",
            1000
        ),
        800,
        2200
    );

    gainMax = constrain(
        prefs.getInt(
            "gainMax",
            2000
        ),
        800,
        2200
    );

    channel3GainMin = constrain(
        prefs.getFloat("ch3GainLo", 0.5f),
        0.0f,
        6.0f
    );

    channel3GainMax = constrain(
        prefs.getFloat("ch3GainHi", 3.0f),
        0.0f,
        6.0f
    );

    if(channel3GainMax < channel3GainMin)
    {
        channel3GainMax = channel3GainMin;
    }

    throttleOutputEnabled = prefs.getBool(
        "thrOut",
        false
    );

    for(uint8_t index = 0; index < 8; index++)
    {
        char key[10];

        snprintf(
            key,
            sizeof(key),
            "auxCh%u",
            index + 1
        );

        auxChannels[index] = constrain(
            prefs.getUChar(key, 0),
            0,
            16
        );
    }

    #if defined(OPENDRIFT_ROUND_LOG51_TUNE)
    // Seed the proven blackbox-51 tune once. Subsequent UI, web, or EdgeTX
    // adjustments persist normally and are not overwritten on each boot.
    // Steering and servo calibration values are deliberately left untouched.
    if(!prefs.getBool("log51Preset", false))
    {
        gain = 1.85f;
        deadband = 2.0f;
        // 37% on the full-span scale preserves the old 74% authority.
        gyroMaxCorrection = 37;
        gyroSmoothing = 0.01f;
        gyroIntegralGain = 0.0f;
        gyroIntegralLimit = 120;
        gyroHoldBoost = 0;
        gyroCounterSteerAssist = 95;
        predictionStrength = 30;
        servoQuiet = 4;
        gyroTransitionSpeed = 25;
        gyroHuntStrength = 75;
        controlLoopHz = 333;

        prefs.putFloat("gain", gain);
        prefs.putFloat("deadband", deadband);
        prefs.putInt("gyroMaxPct", gyroMaxCorrection);
        prefs.putFloat("gyroSmooth", gyroSmoothing);
        prefs.putFloat("gyroIGain", gyroIntegralGain);
        prefs.putInt("gyroILim", gyroIntegralLimit);
        prefs.putInt("gyroHold", gyroHoldBoost);
        prefs.putInt("counterAssist", gyroCounterSteerAssist);
        prefs.putInt("prediction", predictionStrength);
        prefs.putInt("quiet", servoQuiet);
        prefs.putInt("tailSpeedC", gyroTransitionSpeed);
        prefs.putInt("huntStrength", gyroHuntStrength);
        prefs.putUShort("loopHz", controlLoopHz);
        prefs.putBool("log51Preset", true);
    }
    #endif

    loadProfiles();

    return storageReady;
}

void Settings::update()
{
    if(
        dirty &&
        millis() - lastSave > 1000
    )
    {
        save();
    }
}

void Settings::flush()
{
    if(dirty)
    {
        save();
    }
}

void Settings::factoryReset()
{
    prefs.clear();
    dirty = false;
}

const char* Settings::defaultWifiSsid()
{
    return DEFAULT_WIFI_SSID;
}

void Settings::save()
{
    prefs.putFloat(
        "gain",
        gain
    );

    prefs.putFloat(
        "deadband",
        deadband
    );

    prefs.putBool(
        "gyroRev",
        gyroReverse
    );

    prefs.putInt(
        "gyroMaxPct",
        gyroMaxCorrection
    );

    prefs.putFloat(
        "gyroSmooth",
        gyroSmoothing
    );

    prefs.putUChar(
        "gyroLpf",
        gyroLpfMode
    );

    prefs.putFloat(
        "gyroIGain",
        gyroIntegralGain
    );

    prefs.putInt(
        "gyroILim",
        gyroIntegralLimit
    );

    prefs.putInt(
        "gyroHold",
        gyroHoldBoost
    );

    prefs.putInt(
        "counterAssist",
        gyroCounterSteerAssist
    );

    prefs.putInt(
        "tailSpeedC",
        gyroTransitionSpeed
    );

    prefs.putInt(
        "prediction",
        predictionStrength
    );

    prefs.putInt(
        "huntStrength",
        gyroHuntStrength
    );

    prefs.putInt(
        "center",
        servoCenter
    );

    prefs.putBool(
        "reverse",
        servoReverse
    );

    prefs.putInt(
        "travel",
        servoTravel
    );

    prefs.putInt(
        "quiet",
        servoQuiet
    );

    prefs.putUShort(
        "loopHz",
        controlLoopHz
    );

    prefs.putBool(
        "wifi",
        wifiEnabled
    );

    prefs.putULong(
        "timeout",
        wifiTimeout
    );

    prefs.putString(
        "wifiSsid",
        wifiSsid
    );

    prefs.putBool(
        "blackbox",
        blackboxEnabled
    );

    prefs.putUChar(
        "dispBright",
        displayBrightness
    );

    prefs.putUShort(
        "dispDimS",
        displayDimTimeout
    );

    prefs.putBool(
        "dispFlip",
        displayFlip
    );

    prefs.putString(
        "bgName",
        backgroundName
    );

    prefs.putUChar(
        "thmText",
        themeText
    );

    prefs.putUChar(
        "thmAccent",
        themeAccent
    );

    prefs.putBool("servoEndV1", true);
    prefs.putInt("servoCalL", steeringCapturedPulses[0]);
    prefs.putInt("servoCalC", steeringCapturedPulses[1]);
    prefs.putInt("servoCalR", steeringCapturedPulses[2]);
    prefs.putUChar("servoCalM", steeringCalibrationMask & 0x07);
    prefs.putInt("servoInL", steeringCapturedInputPulses[0]);
    prefs.putInt("servoInC", steeringCapturedInputPulses[1]);
    prefs.putInt("servoInR", steeringCapturedInputPulses[2]);

    prefs.putInt(
        "strTravel",
        radioSteeringTravel
    );

    prefs.putInt(
        "gainMin",
        gainMin
    );

    prefs.putInt(
        "gainMax",
        gainMax
    );

    prefs.putFloat(
        "ch3GainLo",
        channel3GainMin
    );

    prefs.putFloat(
        "ch3GainHi",
        channel3GainMax
    );

    prefs.putBool(
        "thrOut",
        throttleOutputEnabled
    );

    for(uint8_t index = 0; index < 8; index++)
    {
        char key[10];

        snprintf(
            key,
            sizeof(key),
            "auxCh%u",
            index + 1
        );

        prefs.putUChar(
            key,
            auxChannels[index]
        );
    }

    if(
        activeProfileIndex >= 0 &&
        activeProfileIndex < profileCount
    )
    {
        captureProfile(
            profiles[activeProfileIndex]
        );

        persistProfile(
            activeProfileIndex
        );
    }

    prefs.putUChar(
        "profCnt",
        profileCount
    );

    prefs.putChar(
        "profAct",
        activeProfileIndex
    );

    dirty = false;

    lastSave = millis();
}

// --------------------
// Gyro
// --------------------

float Settings::getGain()
{
    return gain;
}

void Settings::setGain(float value)
{
    gain = constrain(value, 0.0f, 6.0f);
    dirty = true;
}

float Settings::getDeadband()
{
    return deadband;
}

void Settings::setDeadband(float value)
{
    deadband = constrain(value, 0.0f, 100.0f);
    dirty = true;
}

bool Settings::getGyroReverse()
{
    return gyroReverse;
}

void Settings::setGyroReverse(bool value)
{
    gyroReverse = value;
    dirty = true;
}

int Settings::getGyroMaxCorrection()
{
    return gyroMaxCorrection;
}

void Settings::setGyroMaxCorrection(int value)
{
    gyroMaxCorrection =
        constrain(
            value,
            0,
            100
        );

    dirty = true;
}

float Settings::getGyroSmoothing()
{
    return gyroSmoothing;
}

void Settings::setGyroSmoothing(float value)
{
    gyroSmoothing =
        constrain(
            value,
            0.0f,
            1.0f
        );

    dirty = true;
}

uint8_t Settings::getGyroLpfMode()
{
    return gyroLpfMode;
}

void Settings::setGyroLpfMode(uint8_t value)
{
    gyroLpfMode = constrain(value, 0, 2);
    dirty = true;
}

float Settings::getGyroIntegralGain()
{
    return gyroIntegralGain;
}

void Settings::setGyroIntegralGain(float value)
{
    gyroIntegralGain =
        constrain(
            value,
            0.0f,
            20.0f
        );

    dirty = true;
}

int Settings::getGyroIntegralLimit()
{
    return gyroIntegralLimit;
}

void Settings::setGyroIntegralLimit(int value)
{
    gyroIntegralLimit =
        constrain(
            value,
            0,
            500
        );

    dirty = true;
}

int Settings::getGyroHoldBoost()
{
    return gyroHoldBoost;
}

int Settings::getGyroCounterSteerAssist()
{
    return gyroCounterSteerAssist;
}

void Settings::setGyroCounterSteerAssist(int value)
{
    gyroCounterSteerAssist = constrain(value, 0, 100);
    dirty = true;
}

int Settings::getGyroTransitionSpeed()
{
    return gyroTransitionSpeed;
}

void Settings::setGyroTransitionSpeed(int value)
{
    gyroTransitionSpeed = constrain(value, 0, 100);
    dirty = true;
}

void Settings::setGyroHoldBoost(int value)
{
    gyroHoldBoost =
        constrain(
            value,
            0,
            100
        );

    dirty = true;
}

int Settings::getPredictionStrength()
{
    return predictionStrength;
}

void Settings::setPredictionStrength(int value)
{
    predictionStrength =
        constrain(
            value,
            0,
            100
        );

    dirty = true;
}

int Settings::getGyroHuntStrength()
{
    return gyroHuntStrength;
}

void Settings::setGyroHuntStrength(int value)
{
    gyroHuntStrength = constrain(value, 0, 100);
    dirty = true;
}


// --------------------
// Servo
// --------------------

int Settings::getServoCenter()
{
    return servoCenter;
}

bool Settings::setServoCenter(int value)
{
    int clamped =
        constrain(
            value,
            1000,
            2000
        );

    if(servoCenter == clamped)
    {
        return true;
    }

    if(isSteeringCalibrated())
    {
        return false;
    }

    // The fallback endpoints describe the geometry the servo uses while
    // uncalibrated, so they follow every servo setting change.
    portENTER_CRITICAL(&settingsMux);
    servoCenter = clamped;
    applyFallbackSteeringEndpoints();
    portEXIT_CRITICAL(&settingsMux);

    dirty = true;

    return true;
}

bool Settings::getServoReverse()
{
    return servoReverse;
}

bool Settings::setServoReverse(bool value)
{
    if(servoReverse == value)
    {
        return true;
    }

    // Reverse stays usable after calibration: the captured left and right
    // stops swap sides, so the wheels turn the other way and the
    // calibration survives.
    portENTER_CRITICAL(&settingsMux);

    if(isSteeringCalibrated())
    {
        int swap = steeringMin;
        steeringMin = steeringMax;
        steeringMax = swap;

        steeringCapturedPulses[0] = steeringMin;
        steeringCapturedPulses[2] = steeringMax;
        servoReverse = value;
    }
    else
    {
        servoReverse = value;
        applyFallbackSteeringEndpoints();
    }

    portEXIT_CRITICAL(&settingsMux);

    dirty = true;

    return true;
}

int Settings::getServoTravel()
{
    return servoTravel;
}

bool Settings::setServoTravel(int value)
{
    int clamped =
        constrain(
            value,
            1,
            100
        );

    if(servoTravel == clamped)
    {
        return true;
    }

    if(isSteeringCalibrated())
    {
        return false;
    }

    portENTER_CRITICAL(&settingsMux);
    servoTravel = clamped;
    applyFallbackSteeringEndpoints();
    portEXIT_CRITICAL(&settingsMux);

    dirty = true;

    return true;
}

int Settings::getServoQuiet()
{
    return servoQuiet;
}

void Settings::setServoQuiet(int value)
{
    servoQuiet =
        constrain(
            value,
            0,
            50
        );

    dirty = true;
}

uint16_t Settings::getControlLoopHz()
{
    return controlLoopHz;
}

void Settings::setControlLoopHz(uint16_t value)
{
    controlLoopHz = value == 333 ? 333 : 250;
    dirty = true;
}

// --------------------
// WiFi
// --------------------

bool Settings::getWifiEnabled()
{
    return wifiEnabled;
}

void Settings::setWifiEnabled(bool value)
{
    wifiEnabled = value;
    dirty = true;
}

uint32_t Settings::getWifiTimeout()
{
    return wifiTimeout;
}

void Settings::setWifiTimeout(uint32_t value)
{
    // Zero keeps the access point running for as long as the car is on.
    wifiTimeout =
        value == 0
        ? 0UL
        : constrain(
            value,
            5000UL,
            3600000UL
        );

    dirty = true;
}

const char* Settings::getWifiSsid()
{
    // Always hand out the member buffer so callers that keep the pointer
    // (WiFiManager) see later changes. Lazily fill the default in case
    // this runs before begin().
    if(wifiSsid[0] == 0)
    {
        applyWifiSsid("");
    }

    return wifiSsid;
}

void Settings::setWifiSsid(const String& value)
{
    applyWifiSsid(value);
    dirty = true;
}

void Settings::applyWifiSsid(const String& value)
{
    String clean = sanitizeWifiSsid(value);

    // The buffer is never left empty: an empty or fully rejected name
    // restores the default, so the access point always has a valid SSID.
    snprintf(
        wifiSsid,
        sizeof(wifiSsid),
        "%s",
        clean.length() > 0 ? clean.c_str() : DEFAULT_WIFI_SSID
    );
}

// Same character set as profile names so the value is safe inside the
// web form without escaping. Length is capped at the 32-byte SSID limit.
String Settings::sanitizeWifiSsid(
    const String& requestedName
)
{
    String name = requestedName;
    name.trim();

    String clean;
    clean.reserve(WIFI_SSID_LENGTH - 1);

    for(
        size_t i = 0;
        i < name.length() &&
        clean.length() < WIFI_SSID_LENGTH - 1;
        i++
    )
    {
        char value = name.charAt(i);

        if(
            isAlphaNumeric(value) ||
            value == ' ' ||
            value == '-' ||
            value == '_' ||
            value == '.'
        )
        {
            clean += value;
        }
    }

    clean.trim();

    return clean;
}

// --------------------
// Blackbox
// --------------------

bool Settings::getBlackboxEnabled()
{
    return blackboxEnabled;
}

void Settings::setBlackboxEnabled(bool value)
{
    blackboxEnabled = value;
    dirty = true;
}

uint8_t Settings::getDisplayBrightness()
{
    return displayBrightness;
}

void Settings::setDisplayBrightness(int value)
{
    // Round to the nearest 10 so the display's -/+ buttons and the web
    // select always agree, then keep the panel readable.
    int rounded =
        ((value + 5) / 10) * 10;

    displayBrightness =
        constrain(
            rounded,
            10,
            100
        );

    dirty = true;
}

uint16_t Settings::getDisplayDimTimeout()
{
    return displayDimTimeout;
}

void Settings::setDisplayDimTimeout(int value)
{
    displayDimTimeout =
        constrain(
            value,
            0,
            600
        );

    dirty = true;
}

bool Settings::getDisplayFlip()
{
    return displayFlip;
}

void Settings::setDisplayFlip(bool value)
{
    displayFlip = value;

    dirty = true;
}

const char* Settings::getBackgroundName()
{
    return backgroundName;
}

void Settings::setBackgroundName(const String& value)
{
    String clean =
        sanitizeBackgroundName(value);

    snprintf(
        backgroundName,
        sizeof(backgroundName),
        "%s",
        clean.c_str()
    );

    dirty = true;
}

const char* Settings::themeAccentName(
    uint8_t accent
)
{
    static const char* const names[THEME_ACCENT_COUNT] =
    {
        "MIXED",
        "CYAN",
        "BLUE",
        "MAGENTA",
        "AMBER",
        "GREEN",
        "WHITE"
    };

    return accent < THEME_ACCENT_COUNT ? names[accent] : names[0];
}

uint8_t Settings::getThemeText()
{
    return themeText;
}

void Settings::setThemeText(int value)
{
    themeText =
        constrain(
            value,
            0,
            1
        );

    dirty = true;
}

uint8_t Settings::getThemeAccent()
{
    return themeAccent;
}

void Settings::setThemeAccent(int value)
{
    themeAccent =
        constrain(
            value,
            0,
            THEME_ACCENT_COUNT - 1
        );

    dirty = true;
}

// Same rules as Backgrounds::sanitizeName so a stored name always maps to
// a valid file name and is safe inside the web page.
String Settings::sanitizeBackgroundName(
    const String& value
)
{
    String name = value;
    name.trim();

    String clean;
    clean.reserve(BACKGROUND_NAME_LENGTH - 1);

    for(
        size_t i = 0;
        i < name.length() &&
        clean.length() < BACKGROUND_NAME_LENGTH - 1;
        i++
    )
    {
        char character = name.charAt(i);

        if(
            isAlphaNumeric(character) ||
            character == '-' ||
            character == '_'
        )
        {
            clean += character;
        }
    }

    return clean;
}

// --------------------
// Radio
// --------------------

int Settings::getSteeringMin()
{
    return steeringMin;
}

void Settings::setSteeringMin(int value)
{
    int clamped =
        constrain(
            value,
            900,
            2100
        );

    if(steeringMin == clamped)
    {
        return;
    }

    portENTER_CRITICAL(&settingsMux);

    steeringMin = clamped;
    steeringCapturedPulses[0] = clamped;
    steeringCalibrationMask = 0;

    portEXIT_CRITICAL(&settingsMux);

    dirty = true;
}

int Settings::getSteeringCenter()
{
    return steeringCenter;
}

void Settings::setSteeringCenter(int value)
{
    int clamped =
        constrain(
            value,
            900,
            2100
        );

    if(steeringCenter == clamped)
    {
        return;
    }

    portENTER_CRITICAL(&settingsMux);

    steeringCenter = clamped;
    steeringCapturedPulses[1] = clamped;
    steeringCalibrationMask = 0;

    portEXIT_CRITICAL(&settingsMux);

    dirty = true;
}

int Settings::getSteeringMax()
{
    return steeringMax;
}

void Settings::setSteeringMax(int value)
{
    int clamped =
        constrain(
            value,
            900,
            2100
        );

    if(steeringMax == clamped)
    {
        return;
    }

    portENTER_CRITICAL(&settingsMux);

    steeringMax = clamped;
    steeringCapturedPulses[2] = clamped;
    steeringCalibrationMask = 0;

    portEXIT_CRITICAL(&settingsMux);

    dirty = true;
}

uint8_t Settings::getSteeringCalibrationMask()
{
    return steeringCalibrationMask & 0x07;
}

bool Settings::isSteeringCalibrated()
{
    int leftDelta = steeringMin - steeringCenter;
    int rightDelta = steeringMax - steeringCenter;

    return
        getSteeringCalibrationMask() == 0x07 &&
        abs(leftDelta) >= 10 &&
        abs(rightDelta) >= 10 &&
        leftDelta * rightDelta < 0;
}

void Settings::getSteeringCalibration(
    SteeringCalibration& out
)
{
    portENTER_CRITICAL(&settingsMux);

    int leftDelta = steeringMin - steeringCenter;
    int rightDelta = steeringMax - steeringCenter;

    out.calibrated =
        (steeringCalibrationMask & 0x07) == 0x07 &&
        abs(leftDelta) >= 10 &&
        abs(rightDelta) >= 10 &&
        leftDelta * rightDelta < 0;

    out.min = steeringMin;
    out.center = steeringCenter;
    out.max = steeringMax;
    out.inputMin = steeringCapturedInputPulses[0];
    out.inputCenter = steeringCapturedInputPulses[1];
    out.inputMax = steeringCapturedInputPulses[2];

    portEXIT_CRITICAL(&settingsMux);
}

int Settings::getSteeringCapturedPulse(
    uint8_t point
)
{
    if(point >= 3)
    {
        return 1500;
    }

    return steeringCapturedPulses[point];
}

int Settings::getSteeringCapturedInputPulse(
    uint8_t point
)
{
    if(point >= 3)
    {
        return 1500;
    }

    return steeringCapturedInputPulses[point];
}

bool Settings::captureSteeringCalibrationPoint(
    uint8_t point,
    int physicalPulse,
    int inputPulse
)
{
    if(
        point >= 3 ||
        physicalPulse < 900 ||
        physicalPulse > 2100
    )
    {
        return false;
    }

    // The mask and the endpoints move together so the control task never
    // sees a complete calibration paired with the previous endpoints.
    bool accepted = true;

    portENTER_CRITICAL(&settingsMux);

    int previousPulse = steeringCapturedPulses[point];
    int previousInputPulse = steeringCapturedInputPulses[point];

    steeringCapturedPulses[point] = physicalPulse;

    if(inputPulse >= 800 && inputPulse <= 2200)
    {
        steeringCapturedInputPulses[point] = inputPulse;
    }

    steeringCalibrationMask |= (1U << point);

    if((steeringCalibrationMask & 0x07) == 0x07)
    {
        if(
            steeringStopsValid(
                steeringCapturedPulses[0],
                steeringCapturedPulses[1],
                steeringCapturedPulses[2]
            )
        )
        {
            steeringMin = steeringCapturedPulses[0];
            steeringCenter = steeringCapturedPulses[1];
            steeringMax = steeringCapturedPulses[2];
        }
        else
        {
            // Keep the two known-good captures and make the rejected position
            // visibly incomplete on both the display and radio tool. The
            // rejected pulse is dropped so it can never be saved as a stop.
            steeringCapturedPulses[point] = previousPulse;
            steeringCapturedInputPulses[point] = previousInputPulse;
            steeringCalibrationMask &= ~(1U << point);
            accepted = false;
        }
    }

    portEXIT_CRITICAL(&settingsMux);

    dirty = true;

    return accepted;
}

bool Settings::steeringStopsValid(
    int min,
    int center,
    int max
)
{
    int leftDelta = min - center;
    int rightDelta = max - center;

    return
        abs(leftDelta) >= 10 &&
        abs(rightDelta) >= 10 &&
        leftDelta * rightDelta < 0;
}

bool Settings::setStoredSteeringEndpoints(
    int min,
    int center,
    int max
)
{
    min = constrain(min, 900, 2100);
    center = constrain(center, 900, 2100);
    max = constrain(max, 900, 2100);

    if(!steeringStopsValid(min, center, max))
    {
        return false;
    }

    // All three stops and the mask move in one critical section, so the
    // control task never sees a half-applied set or a cleared mask.
    portENTER_CRITICAL(&settingsMux);

    steeringMin = min;
    steeringCenter = center;
    steeringMax = max;
    steeringCapturedPulses[0] = min;
    steeringCapturedPulses[1] = center;
    steeringCapturedPulses[2] = max;
    steeringCalibrationMask = 0x07;

    portEXIT_CRITICAL(&settingsMux);

    dirty = true;

    return true;
}

bool Settings::confirmStoredSteeringCalibration()
{
    portENTER_CRITICAL(&settingsMux);

    bool validCalibration =
        steeringStopsValid(steeringMin, steeringCenter, steeringMax);

    if(validCalibration)
    {
        steeringCapturedPulses[0] = steeringMin;
        steeringCapturedPulses[1] = steeringCenter;
        steeringCapturedPulses[2] = steeringMax;
        steeringCalibrationMask = 0x07;
    }
    else
    {
        steeringCalibrationMask = 0;
    }

    portEXIT_CRITICAL(&settingsMux);

    dirty = true;

    return validCalibration;
}

void Settings::applyFallbackSteeringEndpoints()
{
    int fallbackOffset = (500 * constrain(servoTravel, 1, 100)) / 100;

    steeringCenter = servoCenter;
    steeringMin = constrain(
        servoCenter + (servoReverse ? fallbackOffset : -fallbackOffset),
        900,
        2100
    );
    steeringMax = constrain(
        servoCenter + (servoReverse ? -fallbackOffset : fallbackOffset),
        900,
        2100
    );
    steeringCapturedPulses[0] = steeringMin;
    steeringCapturedPulses[1] = steeringCenter;
    steeringCapturedPulses[2] = steeringMax;
}

void Settings::clearSteeringCalibration()
{
    portENTER_CRITICAL(&settingsMux);

    steeringCalibrationMask = 0;
    applyFallbackSteeringEndpoints();
    steeringCapturedInputPulses[0] = 1000;
    steeringCapturedInputPulses[1] = 1500;
    steeringCapturedInputPulses[2] = 2000;

    portEXIT_CRITICAL(&settingsMux);

    dirty = true;
}

int Settings::getRadioSteeringTravel()
{
    return radioSteeringTravel;
}

void Settings::setRadioSteeringTravel(int value)
{
    radioSteeringTravel =
        constrain(
            value,
            0,
            100
        );

    dirty = true;
}

int Settings::getGainMin()
{
    return gainMin;
}

void Settings::setGainMin(int value)
{
    gainMin =
        constrain(
            value,
            800,
            2200
        );

    dirty = true;
}

int Settings::getGainMax()
{
    return gainMax;
}

void Settings::setGainMax(int value)
{
    gainMax =
        constrain(
            value,
            800,
            2200
        );

    dirty = true;
}

float Settings::getChannel3GainMin()
{
    return channel3GainMin;
}

void Settings::setChannel3GainMin(float value)
{
    channel3GainMin = constrain(value, 0.0f, 6.0f);

    if(channel3GainMax < channel3GainMin)
    {
        channel3GainMax = channel3GainMin;
    }

    dirty = true;
}

float Settings::getChannel3GainMax()
{
    return channel3GainMax;
}

void Settings::setChannel3GainMax(float value)
{
    channel3GainMax = constrain(value, 0.0f, 6.0f);

    if(channel3GainMin > channel3GainMax)
    {
        channel3GainMin = channel3GainMax;
    }

    dirty = true;
}

bool Settings::getThrottleOutputEnabled()
{
    return throttleOutputEnabled;
}

void Settings::setThrottleOutputEnabled(bool value)
{
    throttleOutputEnabled = value;
    dirty = true;
}

uint8_t Settings::getAuxChannelForGpio(uint8_t gpio)
{
    if(gpio < 1 || gpio > 8)
    {
        return 0;
    }

    return auxChannels[gpio - 1];
}

void Settings::setAuxChannelForGpio(
    uint8_t gpio,
    uint8_t channel
)
{
    if(gpio < 1 || gpio > 8)
    {
        return;
    }

    auxChannels[gpio - 1] = constrain(
        channel,
        (uint8_t)0,
        (uint8_t)16
    );

    dirty = true;
}

// --------------------
// Driving profiles
// --------------------

uint8_t Settings::getProfileCount()
{
    return profileCount;
}

int8_t Settings::getActiveProfileIndex()
{
    return activeProfileIndex;
}

const char* Settings::getActiveProfileName()
{
    if(
        activeProfileIndex < 0 ||
        activeProfileIndex >= profileCount
    )
    {
        return "Current Tune";
    }

    return profiles[activeProfileIndex].name;
}

const Settings::DrivingProfile* Settings::getProfile(
    uint8_t index
)
{
    if(index >= profileCount)
    {
        return nullptr;
    }

    return &profiles[index];
}

int8_t Settings::createProfile(
    const String& requestedName
)
{
    if(profileCount >= MAX_PROFILES)
    {
        return -1;
    }

    String name =
        sanitizeProfileName(requestedName);

    if(name.length() == 0)
    {
        return -1;
    }

    for(uint8_t i = 0; i < profileCount; i++)
    {
        if(name.equalsIgnoreCase(profiles[i].name))
        {
            return -1;
        }
    }

    if(dirty)
    {
        save();
    }

    DrivingProfile& profile =
        profiles[profileCount];

    profile = DrivingProfile();

    name.toCharArray(
        profile.name,
        PROFILE_NAME_LENGTH
    );

    captureProfile(profile);

    uint8_t newIndex = profileCount;

    profileCount++;
    activeProfileIndex = newIndex;

    persistProfile(newIndex);

    prefs.putUChar(
        "profCnt",
        profileCount
    );

    prefs.putChar(
        "profAct",
        activeProfileIndex
    );

    return activeProfileIndex;
}

int8_t Settings::importProfile(
    const DrivingProfile& incoming,
    bool& replaced
)
{
    replaced = false;

    String name =
        sanitizeProfileName(String(incoming.name));

    if(name.length() == 0)
    {
        return -1;
    }

    if(dirty)
    {
        save();
    }

    int8_t index = -1;

    for(uint8_t i = 0; i < profileCount; i++)
    {
        if(name.equalsIgnoreCase(profiles[i].name))
        {
            index = i;
            break;
        }
    }

    if(index < 0)
    {
        if(profileCount >= MAX_PROFILES)
        {
            return -2;
        }

        index = profileCount;
        profileCount++;
    }
    else
    {
        replaced = true;

        if(activeProfileIndex == index)
        {
            activeProfileIndex = -1;
        }
    }

    DrivingProfile& profile =
        profiles[index];

    profile = DrivingProfile();

    name.toCharArray(
        profile.name,
        PROFILE_NAME_LENGTH
    );

    profile.gain = constrain(incoming.gain, 0.0f, 6.0f);
    profile.deadband = constrain(incoming.deadband, 0.0f, 100.0f);
    profile.gyroSmoothing = constrain(incoming.gyroSmoothing, 0.0f, 1.0f);
    profile.gyroIntegralGain = constrain(incoming.gyroIntegralGain, 0.0f, 20.0f);
    profile.gyroMaxCorrection = constrain(incoming.gyroMaxCorrection, 0, 100);
    profile.gyroIntegralLimit = constrain(incoming.gyroIntegralLimit, 0, 500);
    profile.gyroHoldBoost = constrain(incoming.gyroHoldBoost, 0, 100);
    profile.predictionStrength = constrain(incoming.predictionStrength, 0, 100);
    profile.radioSteeringTravel = constrain(incoming.radioSteeringTravel, 0, 100);
    profile.gyroCounterSteerAssist = constrain(incoming.gyroCounterSteerAssist, 0, 100);
    profile.gyroTransitionSpeed = constrain(incoming.gyroTransitionSpeed, 0, 100);
    profile.gyroHuntStrength = constrain(incoming.gyroHuntStrength, 0, 100);

    persistProfile(index);

    prefs.putUChar(
        "profCnt",
        profileCount
    );

    prefs.putChar(
        "profAct",
        activeProfileIndex
    );

    return index;
}

bool Settings::activateProfile(
    uint8_t index
)
{
    if(index >= profileCount)
    {
        return false;
    }

    if(dirty)
    {
        save();
    }

    activeProfileIndex = index;

    applyProfile(
        profiles[index]
    );

    dirty = true;
    save();

    return true;
}

bool Settings::deleteProfile(
    uint8_t index
)
{
    if(index >= profileCount)
    {
        return false;
    }

    if(dirty)
    {
        save();
    }

    bool deletedActive =
        activeProfileIndex == index;

    for(uint8_t i = index; i + 1 < profileCount; i++)
    {
        profiles[i] = profiles[i + 1];
    }

    uint8_t previousLast =
        profileCount - 1;

    profiles[previousLast] = DrivingProfile();
    profileCount--;

    if(deletedActive)
    {
        activeProfileIndex = -1;
    }
    else if(activeProfileIndex > index)
    {
        activeProfileIndex--;
    }

    for(uint8_t i = 0; i < profileCount; i++)
    {
        persistProfile(i);
    }

    char key[16];

    snprintf(
        key,
        sizeof(key),
        "prof%u",
        previousLast
    );

    prefs.remove(key);

    prefs.putUChar(
        "profCnt",
        profileCount
    );

    prefs.putChar(
        "profAct",
        activeProfileIndex
    );

    return true;
}

void Settings::loadProfiles()
{
    uint8_t storedCount = constrain(
        (int)prefs.getUChar("profCnt", 0),
        0,
        (int)MAX_PROFILES
    );

    uint8_t loadedCount = 0;

    int8_t newIndexFor[MAX_PROFILES];

    for(uint8_t i = 0; i < MAX_PROFILES; i++)
    {
        newIndexFor[i] = -1;
    }

    for(uint8_t i = 0; i < storedCount; i++)
    {
        char key[16];

        snprintf(
            key,
            sizeof(key),
            "prof%u",
            i
        );

        uint8_t buffer[LARGEST_STORED_PROFILE_SIZE] = {};

        size_t storedSize = prefs.getBytesLength(key);

        if(
            storedSize < sizeof(uint32_t) ||
            storedSize > sizeof(buffer) ||
            prefs.getBytes(key, buffer, storedSize) != storedSize
        )
        {
            continue;
        }

        uint32_t version = 0;
        memcpy(&version, buffer, sizeof(version));

        switch(version)
        {
            case 10:
            case 9:
            case 8:
            {
                if(storedSize != sizeof(DrivingProfile))
                {
                    break;
                }

                DrivingProfile stored = {};
                memcpy(&stored, buffer, sizeof(stored));

                if(stored.name[0] == '\0')
                {
                    break;
                }

                DrivingProfile& profile = profiles[loadedCount];
                profile = stored;
                profile.version = 10;
                profile.name[PROFILE_NAME_LENGTH - 1] = '\0';

                if(version == 9)
                {
                    profile.gyroMaxCorrection =
                        centerSpanPercentToFullSpanPercent(
                            stored.gyroMaxCorrection
                        );
                }
                else if(version == 8)
                {
                    profile.gyroMaxCorrection =
                        legacyMaxCorrectionToPercent(
                            stored.gyroMaxCorrection
                        );
                }

                newIndexFor[i] = (int8_t)loadedCount;
                loadedCount++;
                break;
            }
            case 7:
            {
                if(storedSize != sizeof(DrivingProfileV7))
                {
                    break;
                }

                DrivingProfileV7 legacy = {};
                memcpy(&legacy, buffer, sizeof(legacy));

                if(legacy.name[0] == '\0')
                {
                    break;
                }

                DrivingProfile& profile = profiles[loadedCount];
                profile = DrivingProfile();
                memcpy(profile.name, legacy.name, PROFILE_NAME_LENGTH);
                profile.name[PROFILE_NAME_LENGTH - 1] = '\0';
                profile.gain = legacy.gain;
                profile.deadband = legacy.deadband;
                profile.gyroSmoothing = legacy.gyroSmoothing;
                profile.gyroIntegralGain = legacy.gyroIntegralGain;
                profile.gyroMaxCorrection = legacyMaxCorrectionToPercent(legacy.gyroMaxCorrection);
                profile.gyroIntegralLimit = legacy.gyroIntegralLimit;
                profile.gyroHoldBoost = legacy.gyroHoldBoost;
                profile.predictionStrength = legacy.predictionStrength;
                profile.radioSteeringTravel = legacy.radioSteeringTravel;
                profile.gyroCounterSteerAssist = legacy.gyroCounterSteerAssist;
                profile.gyroTransitionSpeed = legacy.gyroTransitionSpeed;
                profile.gyroHuntStrength = legacy.gyroHuntStrength;
                newIndexFor[i] = (int8_t)loadedCount;
                loadedCount++;
                break;
            }
            case 6:
            {
                if(storedSize != sizeof(DrivingProfileV6))
                {
                    break;
                }

                DrivingProfileV6 legacy = {};
                memcpy(&legacy, buffer, sizeof(legacy));

                if(legacy.name[0] == '\0')
                {
                    break;
                }

                DrivingProfile& profile = profiles[loadedCount];
                profile = DrivingProfile();
                memcpy(profile.name, legacy.name, PROFILE_NAME_LENGTH);
                profile.name[PROFILE_NAME_LENGTH - 1] = '\0';
                profile.gain = legacy.gain;
                profile.deadband = legacy.deadband;
                profile.gyroSmoothing = legacy.gyroSmoothing;
                profile.gyroIntegralGain = legacy.gyroIntegralGain;
                profile.gyroMaxCorrection = legacyMaxCorrectionToPercent(legacy.gyroMaxCorrection);
                profile.gyroIntegralLimit = legacy.gyroIntegralLimit;
                profile.gyroHoldBoost = legacy.gyroHoldBoost;
                profile.predictionStrength = legacy.predictionStrength;
                profile.radioSteeringTravel = legacy.radioSteeringTravel;
                profile.gyroCounterSteerAssist = legacy.gyroCounterSteerAssist;
                profile.gyroTransitionSpeed = legacy.gyroTransitionSpeed;
                profile.gyroHuntStrength = 50;
                newIndexFor[i] = (int8_t)loadedCount;
                loadedCount++;
                break;
            }
            case 5:
            {
                if(storedSize != sizeof(DrivingProfileV5))
                {
                    break;
                }

                DrivingProfileV5 legacy = {};
                memcpy(&legacy, buffer, sizeof(legacy));

                if(legacy.name[0] == '\0')
                {
                    break;
                }

                DrivingProfile& profile = profiles[loadedCount];
                profile = DrivingProfile();
                memcpy(profile.name, legacy.name, PROFILE_NAME_LENGTH);
                profile.name[PROFILE_NAME_LENGTH - 1] = '\0';
                profile.gain = legacy.gain;
                profile.deadband = legacy.deadband;
                profile.gyroSmoothing = legacy.gyroSmoothing;
                profile.gyroIntegralGain = legacy.gyroIntegralGain;
                profile.gyroMaxCorrection = legacyMaxCorrectionToPercent(legacy.gyroMaxCorrection);
                profile.gyroIntegralLimit = legacy.gyroIntegralLimit;
                profile.gyroHoldBoost = legacy.gyroHoldBoost;
                profile.predictionStrength = legacy.predictionStrength;
                profile.radioSteeringTravel = legacy.radioSteeringTravel;
                profile.gyroCounterSteerAssist = legacy.gyroCounterSteerAssist;
                profile.gyroTransitionSpeed = legacy.gyroTransitionSpeed;
                newIndexFor[i] = (int8_t)loadedCount;
                loadedCount++;
                break;
            }
            case 4:
            {
                if(storedSize != sizeof(DrivingProfileV4))
                {
                    break;
                }

                DrivingProfileV4 legacy = {};
                memcpy(&legacy, buffer, sizeof(legacy));

                if(legacy.name[0] == '\0')
                {
                    break;
                }

                DrivingProfile& profile = profiles[loadedCount];
                profile = DrivingProfile();
                memcpy(profile.name, legacy.name, PROFILE_NAME_LENGTH);
                profile.name[PROFILE_NAME_LENGTH - 1] = '\0';
                profile.gain = legacy.gain;
                profile.deadband = legacy.deadband;
                profile.gyroSmoothing = legacy.gyroSmoothing;
                profile.gyroIntegralGain = legacy.gyroIntegralGain;
                profile.gyroMaxCorrection = legacyMaxCorrectionToPercent(legacy.gyroMaxCorrection);
                profile.gyroIntegralLimit = legacy.gyroIntegralLimit;
                profile.gyroHoldBoost = legacy.gyroHoldBoost;
                profile.predictionStrength = legacy.gyroHuntDamping;
                profile.radioSteeringTravel = legacy.radioSteeringTravel;
                profile.gyroCounterSteerAssist = legacy.gyroCounterSteerAssist;
                profile.gyroTransitionSpeed = legacy.gyroTailSlideSpeed;
                newIndexFor[i] = (int8_t)loadedCount;
                loadedCount++;
                break;
            }
            case 3:
            {
                if(storedSize != sizeof(DrivingProfileV3))
                {
                    break;
                }

                DrivingProfileV3 legacy = {};
                memcpy(&legacy, buffer, sizeof(legacy));

                if(legacy.name[0] == '\0')
                {
                    break;
                }

                DrivingProfile& profile = profiles[loadedCount];
                profile = DrivingProfile();
                memcpy(profile.name, legacy.name, PROFILE_NAME_LENGTH);
                profile.name[PROFILE_NAME_LENGTH - 1] = '\0';
                profile.gain = legacy.gain;
                profile.deadband = legacy.deadband;
                profile.gyroSmoothing = legacy.gyroSmoothing;
                profile.gyroIntegralGain = legacy.gyroIntegralGain;
                profile.gyroMaxCorrection = legacyMaxCorrectionToPercent(legacy.gyroMaxCorrection);
                profile.gyroIntegralLimit = legacy.gyroIntegralLimit;
                profile.gyroHoldBoost = legacy.gyroHoldBoost;
                profile.predictionStrength = legacy.gyroHuntDamping;
                profile.radioSteeringTravel = legacy.radioSteeringTravel;
                profile.gyroCounterSteerAssist = legacy.gyroCounterSteerAssist;
                profile.gyroTransitionSpeed = constrain(
                    50 + legacy.gyroTailSlideSpeed / 2,
                    50,
                    100
                );
                newIndexFor[i] = (int8_t)loadedCount;
                loadedCount++;
                break;
            }
            case 2:
            {
                if(storedSize != sizeof(DrivingProfileV2))
                {
                    break;
                }

                DrivingProfileV2 legacy = {};
                memcpy(&legacy, buffer, sizeof(legacy));

                if(legacy.name[0] == '\0')
                {
                    break;
                }

                DrivingProfile& profile = profiles[loadedCount];
                profile = DrivingProfile();
                memcpy(profile.name, legacy.name, PROFILE_NAME_LENGTH);
                profile.name[PROFILE_NAME_LENGTH - 1] = '\0';
                profile.gain = legacy.gain;
                profile.deadband = legacy.deadband;
                profile.gyroSmoothing = legacy.gyroSmoothing;
                profile.gyroIntegralGain = legacy.gyroIntegralGain;
                profile.gyroMaxCorrection = legacyMaxCorrectionToPercent(legacy.gyroMaxCorrection);
                profile.gyroIntegralLimit = legacy.gyroIntegralLimit;
                profile.gyroHoldBoost = legacy.gyroHoldBoost;
                profile.predictionStrength = legacy.gyroHuntDamping;
                profile.radioSteeringTravel = legacy.radioSteeringTravel;
                profile.gyroCounterSteerAssist = legacy.gyroCounterSteerAssist;
                profile.gyroTransitionSpeed = 50;
                newIndexFor[i] = (int8_t)loadedCount;
                loadedCount++;
                break;
            }
            case 1:
            {
                if(storedSize != sizeof(DrivingProfileV1))
                {
                    break;
                }

                DrivingProfileV1 legacy = {};
                memcpy(&legacy, buffer, sizeof(legacy));

                if(legacy.name[0] == '\0')
                {
                    break;
                }

                DrivingProfile& profile = profiles[loadedCount];
                profile = DrivingProfile();
                memcpy(profile.name, legacy.name, PROFILE_NAME_LENGTH);
                profile.name[PROFILE_NAME_LENGTH - 1] = '\0';
                profile.gain = legacy.gain;
                profile.deadband = legacy.deadband;
                profile.gyroSmoothing = legacy.gyroSmoothing;
                profile.gyroIntegralGain = legacy.gyroIntegralGain;
                profile.gyroMaxCorrection = legacyMaxCorrectionToPercent(legacy.gyroMaxCorrection);
                profile.gyroIntegralLimit = legacy.gyroIntegralLimit;
                profile.gyroHoldBoost = legacy.gyroHoldBoost;
                profile.predictionStrength = legacy.gyroHuntDamping;
                profile.radioSteeringTravel = legacy.radioSteeringTravel;
                profile.gyroCounterSteerAssist = 0;
                profile.gyroTransitionSpeed = 50;
                newIndexFor[i] = (int8_t)loadedCount;
                loadedCount++;
                break;
            }
            default:
                break;
        }
    }

    profileCount = loadedCount;

    for(uint8_t i = 0; i < profileCount; i++)
    {
        clampProfile(profiles[i]);
        persistProfile(i);
    }

    for(uint8_t i = profileCount; i < storedCount; i++)
    {
        char key[16];

        snprintf(
            key,
            sizeof(key),
            "prof%u",
            i
        );

        prefs.remove(key);
    }

    int storedActive =
        prefs.getChar("profAct", -1);

    activeProfileIndex =
        storedActive >= 0 &&
        storedActive < storedCount
        ?
        newIndexFor[storedActive]
        :
        -1;

    prefs.putUChar("profCnt", profileCount);
    prefs.putChar("profAct", activeProfileIndex);
}

void Settings::clampProfile(
    DrivingProfile& profile
)
{
    profile.gain = constrain(profile.gain, 0.0f, 6.0f);
    profile.deadband = constrain(profile.deadband, 0.0f, 100.0f);
    profile.gyroSmoothing = constrain(profile.gyroSmoothing, 0.0f, 1.0f);
    profile.gyroIntegralGain = constrain(profile.gyroIntegralGain, 0.0f, 20.0f);
    profile.gyroMaxCorrection = constrain(profile.gyroMaxCorrection, 0, 100);
    profile.gyroIntegralLimit = constrain(profile.gyroIntegralLimit, 0, 500);
    profile.gyroHoldBoost = constrain(profile.gyroHoldBoost, 0, 100);
    profile.predictionStrength = constrain(profile.predictionStrength, 0, 100);
    profile.radioSteeringTravel = constrain(profile.radioSteeringTravel, 0, 100);
    profile.gyroCounterSteerAssist = constrain(profile.gyroCounterSteerAssist, 0, 100);
    profile.gyroTransitionSpeed = constrain(profile.gyroTransitionSpeed, 0, 100);
    profile.gyroHuntStrength = constrain(profile.gyroHuntStrength, 0, 100);
}

void Settings::captureProfile(
    DrivingProfile& profile
)
{
    profile.version = 10;
    profile.gain = gain;
    profile.deadband = deadband;
    profile.gyroSmoothing = gyroSmoothing;
    profile.gyroIntegralGain = gyroIntegralGain;
    profile.gyroMaxCorrection = gyroMaxCorrection;
    profile.gyroIntegralLimit = gyroIntegralLimit;
    profile.gyroHoldBoost = gyroHoldBoost;
    profile.predictionStrength = predictionStrength;
    profile.radioSteeringTravel = radioSteeringTravel;
    profile.gyroCounterSteerAssist = gyroCounterSteerAssist;
    profile.gyroTransitionSpeed = gyroTransitionSpeed;
    profile.gyroHuntStrength = gyroHuntStrength;
}

void Settings::applyProfile(
    const DrivingProfile& profile
)
{
    portENTER_CRITICAL(&settingsMux);

    gain = constrain(profile.gain, 0.0f, 6.0f);
    deadband = constrain(profile.deadband, 0.0f, 100.0f);
    gyroSmoothing = constrain(profile.gyroSmoothing, 0.0f, 1.0f);
    gyroIntegralGain = constrain(profile.gyroIntegralGain, 0.0f, 20.0f);
    gyroMaxCorrection = constrain(profile.gyroMaxCorrection, 0, 100);
    gyroIntegralLimit = constrain(profile.gyroIntegralLimit, 0, 500);
    gyroHoldBoost = constrain(profile.gyroHoldBoost, 0, 100);
    predictionStrength = constrain(profile.predictionStrength, 0, 100);
    radioSteeringTravel = constrain(profile.radioSteeringTravel, 0, 100);
    gyroCounterSteerAssist = constrain(profile.gyroCounterSteerAssist, 0, 100);
    gyroTransitionSpeed = constrain(profile.gyroTransitionSpeed, 0, 100);
    gyroHuntStrength = constrain(profile.gyroHuntStrength, 0, 100);

    portEXIT_CRITICAL(&settingsMux);
}

bool Settings::persistProfile(
    uint8_t index
)
{
    if(index >= profileCount)
    {
        return false;
    }

    char key[16];

    snprintf(
        key,
        sizeof(key),
        "prof%u",
        index
    );

    return prefs.putBytes(
        key,
        &profiles[index],
        sizeof(DrivingProfile)
    ) == sizeof(DrivingProfile);
}

String Settings::sanitizeProfileName(
    const String& requestedName
)
{
    String name = requestedName;
    name.trim();

    String clean;
    clean.reserve(PROFILE_NAME_LENGTH - 1);

    for(
        size_t i = 0;
        i < name.length() &&
        clean.length() < PROFILE_NAME_LENGTH - 1;
        i++
    )
    {
        char value = name.charAt(i);

        if(
            isAlphaNumeric(value) ||
            value == ' ' ||
            value == '-' ||
            value == '_' ||
            value == '.'
        )
        {
            clean += value;
        }
    }

    clean.trim();

    return clean;
}
