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
