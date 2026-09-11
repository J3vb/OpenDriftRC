#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "Settings.h"
#include "GyroController.h"
#include "RadioInput.h"
#include "BlackboxLogger.h"
#include "BatteryCompensation.h"
#include "BatterySense.h"


class WebConfigurator
{
public:

    WebConfigurator();

    void begin(
        Settings& settings,
        GyroController& gyro,
        RadioInput& steeringRadio,
        RadioInput& gainRadio,
        RadioInput& throttleRadio,
        BlackboxLogger& blackbox,
        BatteryCompensation& batteryComp,
        BatterySense& batterySense
    );

    void update();

    bool isRunning();


private:

    WebServer server;

    Settings* settings = nullptr;

    GyroController* gyro = nullptr;

    RadioInput* steeringRadio = nullptr;

    RadioInput* gainRadio = nullptr;

    RadioInput* throttleRadio = nullptr;

    BlackboxLogger* blackbox = nullptr;

    BatteryCompensation* batteryComp = nullptr;

    BatterySense* batterySense = nullptr;

    bool running = false;

    // A calibration typed into the form waits until the sense pin saved in
    // the same request has produced a reading.
    static constexpr unsigned long CALIBRATION_TIMEOUT_MS = 10000;

    bool calibrationPending = false;
    float pendingCalibrationVolts = 0.0f;
    uint8_t pendingCalibrationPin = 0;
    unsigned long pendingCalibrationSinceMs = 0;
    String calibrationStatus;

    void applyPendingCalibration();

    void handleRoot();

    void handleLiveStatus();

    void handleBatteryScript();

    void handleSave();

    void handleProfileCreate();

    void handleProfileActivate();

    void handleProfileDelete();

    void handleLogDownload();

    void handleLogClear();

    void handleNotFound();

    String input(
        const char* label,
        const char* name,
        String value,
        const char* type = "number",
        const char* step = "1"
    );

    // A required number input whose min/max mirror the firmware clamps.
    String inputRange(
        const char* label,
        const char* name,
        String value,
        const char* step,
        const char* minimum,
        const char* maximum
    );

    String checkbox(
        const char* label,
        const char* name,
        bool checked
    );

    int getIntArg(
        const char* name,
        int fallback
    );

    float getFloatArg(
        const char* name,
        float fallback
    );
};
