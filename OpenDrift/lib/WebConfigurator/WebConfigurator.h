#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "Settings.h"
#include "GyroController.h"
#include "RadioInput.h"
#include "BlackboxLogger.h"
#include "WiFiManager.h"


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
        WiFiManager& wifi
    );

    void update();

    bool isRunning();

    // True between a web restart request and the reset itself. The main
    // loop keeps calling update() while this holds, even if the access
    // point drops in the meantime.
    bool isRestartPending();


private:

    WebServer server;

    Settings* settings = nullptr;

    GyroController* gyro = nullptr;

    RadioInput* steeringRadio = nullptr;

    RadioInput* gainRadio = nullptr;

    RadioInput* throttleRadio = nullptr;

    BlackboxLogger* blackbox = nullptr;

    WiFiManager* wifi = nullptr;

    bool running = false;

    // A restart request is answered first and executed from update()
    // once the response has had time to leave the socket.
    static constexpr unsigned long RESTART_DELAY_MS = 500;

    unsigned long restartAtMs = 0;

    void handleRoot();

    void handleLiveStatus();

    void handleSave();

    void handleProfileCreate();

    void handleProfileActivate();

    void handleProfileDelete();

    void handleLogDownload();

    void handleLogClear();

    void handleRestart();

    void sendRestartPage(
        const char* heading,
        const char* ssid
    );

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
