#include "WebConfigurator.h"
#include "../../include/Version.h"

#include <esp_system.h>

#if defined(OPENDRIFT_INPUT_CRSF) && defined(OPENDRIFT_BOARD_AMOLED_164)
#include "AuxChannelOutputs.h"
#endif

namespace
{
    // Hand-rolled JSON in the same style as /live-status. Values arrive
    // already rendered so numbers, booleans and quoted strings share one
    // path.
    void appendJsonField(
        String& json,
        const char* key,
        const String& rawValue
    )
    {
        json += F(",\"");
        json += key;
        json += F("\":");
        json += rawValue;
    }

    String jsonBool(
        bool value
    )
    {
        return value ? String(F("true")) : String(F("false"));
    }

    String jsonString(
        const char* value
    )
    {
        String quoted;

        quoted.reserve(strlen(value) + 2);

        quoted += '"';
        quoted += value;
        quoted += '"';

        return quoted;
    }

    const char* resetReasonText(
        esp_reset_reason_t reason
    )
    {
        switch(reason)
        {
            case ESP_RST_POWERON: return "power-on";
            case ESP_RST_EXT: return "external reset";
            case ESP_RST_SW: return "software restart";
            case ESP_RST_PANIC: return "crash (panic)";
            case ESP_RST_INT_WDT: return "interrupt watchdog";
            case ESP_RST_TASK_WDT: return "task watchdog";
            case ESP_RST_WDT: return "watchdog";
            case ESP_RST_DEEPSLEEP: return "deep sleep";
            case ESP_RST_BROWNOUT: return "brownout (power dip)";
            case ESP_RST_SDIO: return "sdio";
            default: return "unknown";
        }
    }

    String uptimeText()
    {
        unsigned long seconds =
            millis() / 1000UL;

        char text[24];

        snprintf(
            text,
            sizeof(text),
            "%lu:%02lu:%02lu",
            seconds / 3600UL,
            (seconds / 60UL) % 60UL,
            seconds % 60UL
        );

        return String(text);
    }
}

WebConfigurator::WebConfigurator()
:
server(80)
{

}



void WebConfigurator::begin(
    Settings& settingsRef,
    GyroController& gyroRef,
    RadioInput& steeringRadioRef,
    RadioInput& gainRadioRef,
    RadioInput& throttleRadioRef,
    BlackboxLogger& blackboxRef,
    WiFiManager& wifiRef,
    ServoOutput& steeringServoRef,
    Backgrounds& backgroundsRef
)
{
    settings =
        &settingsRef;

    gyro =
        &gyroRef;

    steeringRadio =
        &steeringRadioRef;

    gainRadio =
        &gainRadioRef;

    throttleRadio =
        &throttleRadioRef;

    blackbox =
        &blackboxRef;

    wifi =
        &wifiRef;

    steeringServo =
        &steeringServoRef;

    backgrounds =
        &backgroundsRef;

    server.on(
        "/",
        HTTP_GET,
        [this]()
        {
            handleRoot();
        }
    );

    server.on(
        "/save",
        HTTP_POST,
        [this]()
        {
            handleSave();
        }
    );

    server.on(
        "/live-status",
        HTTP_GET,
        [this]()
        {
            handleLiveStatus();
        }
    );

    server.on(
        "/create-profile",
        HTTP_POST,
        [this]()
        {
            handleProfileCreate();
        }
    );

    server.on(
        "/activate-profile",
        HTTP_POST,
        [this]()
        {
            handleProfileActivate();
        }
    );

    server.on(
        "/delete-profile",
        HTTP_POST,
        [this]()
        {
            handleProfileDelete();
        }
    );

    server.on(
        "/blackbox.csv",
        HTTP_GET,
        [this]()
        {
            handleLogDownload();
        }
    );

    server.on(
        "/settings.json",
        HTTP_GET,
        [this]()
        {
            handleSettingsExport();
        }
    );

    server.on(
        "/clear-log",
        HTTP_POST,
        [this]()
        {
            handleLogClear();
        }
    );

    server.on(
        "/restart",
        HTTP_POST,
        [this]()
        {
            handleRestart();
        }
    );

    server.on(
        "/factory-reset",
        HTTP_POST,
        [this]()
        {
            handleFactoryReset();
        }
    );

    server.on(
        "/capture-endpoint",
        HTTP_POST,
        [this]()
        {
            handleEndpointCapture();
        }
    );

    server.on(
        "/reset-endpoints",
        HTTP_POST,
        [this]()
        {
            handleEndpointReset();
        }
    );

    #if defined(OPENDRIFT_BOARD_AMOLED_164)
    // The second handler receives the multipart file in chunks while the
    // request is parsed; the first one answers once it is complete.
    server.on(
        "/upload-background",
        HTTP_POST,
        [this]()
        {
            handleBackgroundUpload();
        },
        [this]()
        {
            handleBackgroundUploadChunk();
        }
    );

    server.on(
        "/use-background",
        HTTP_POST,
        [this]()
        {
            handleBackgroundUse();
        }
    );

    server.on(
        "/delete-background",
        HTTP_POST,
        [this]()
        {
            handleBackgroundDelete();
        }
    );
    #endif

    server.onNotFound(
        [this]()
        {
            handleNotFound();
        }
    );

    server.begin();

    running = true;

    Serial.println(
        "Web configurator started"
    );
}



void WebConfigurator::update()
{
    // Checked before the running guard so a WiFi auto-off inside the
    // delay window cannot strand a requested restart.
    if(
        restartAtMs != 0 &&
        (long)(millis() - restartAtMs) >= 0
    )
    {
        if(settings != nullptr)
        {
            // Erasing here, microseconds before the reset, means no
            // deferred save, display press or CRSF write can put the
            // in-memory settings back into flash.
            if(factoryResetPending)
            {
                settings->factoryReset();

                #if defined(OPENDRIFT_BOARD_AMOLED_164)
                if(backgrounds != nullptr)
                {
                    backgrounds->eraseAll();
                }
                #endif
            }
            else
            {
                settings->flush();
            }
        }

        Serial.println(
            factoryResetPending
            ? "Factory reset requested from web configurator"
            : "Restart requested from web configurator"
        );

        Serial.flush();

        esp_restart();
    }

    if(!running)
    {
        return;
    }

    server.handleClient();
}



bool WebConfigurator::isRunning()
{
    return running;
}



bool WebConfigurator::isRestartPending()
{
    return restartAtMs != 0;
}



void WebConfigurator::handleRoot()
{
    if(settings == nullptr)
    {
        server.send(
            503,
            "text/plain",
            "Settings unavailable"
        );

        return;
    }

    String html;

    html.reserve(20000);

    html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
    html += F("<title>OpenDrift Config</title><style>");
    html += F("body{font-family:system-ui,Arial,sans-serif;margin:0;background:#101214;color:#f5f5f5}");
    html += F("main{max-width:760px;margin:0 auto;padding:18px}");
    html += F("h1{font-size:28px;margin:8px 0 2px}h2{font-size:18px;margin:22px 0 10px}");
    html += F(".sub{color:#aeb4bb;margin-bottom:20px}.card{border:1px solid #33383f;border-radius:8px;padding:14px;margin:12px 0;background:#171a1f}");
    html += F("label{display:block;font-size:13px;color:#c8cdd2;margin:12px 0 5px}input,select{width:100%;box-sizing:border-box;background:#0b0d10;color:#fff;border:1px solid #3b4148;border-radius:6px;padding:10px;font-size:16px}");
    html += F("input[type=checkbox]{width:auto;transform:scale(1.3);margin-right:8px}.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}");
    html += F(".status{display:grid;grid-template-columns:1fr 1fr;gap:8px}.pill{background:#0b0d10;border:1px solid #33383f;border-radius:6px;padding:10px}");
    html += F("button{width:100%;padding:13px 16px;border:0;border-radius:6px;background:#24a36b;color:#fff;font-size:17px;font-weight:700;margin-top:16px}");
    html += F("button.secondary{background:#3b4148}button.danger{background:#973b45}.warn{color:#e5a733}.ok{color:#24a36b}.bad{color:#e5484d}");
    html += F(".endpoints{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:12px}.endpoints button{margin:0;padding:10px 6px;font-size:13px}.endpoints small{font-weight:400}");
    html += F(".profile{display:grid;grid-template-columns:1fr 96px 82px;gap:8px;align-items:center;background:#0b0d10;border:1px solid #33383f;border-radius:6px;padding:9px;margin:8px 0}.profile.active{border-color:#24a36b}.profile strong{display:block}.profile small{color:#aeb4bb}.profile form{margin:0}.profile button{margin:0;padding:9px 6px;font-size:13px}.profile .danger{background:#973b45}.create-profile{display:grid;grid-template-columns:1fr 150px;gap:10px;align-items:end}.create-profile button{margin:0;height:43px}");
    html += F("a{color:#65b7ff}@media(max-width:560px){.row,.status,.create-profile{grid-template-columns:1fr}.profile,.endpoints{grid-template-columns:1fr 1fr}.profile>div{grid-column:1/-1}}");
    html += F("</style></head><body><main>");
    html += F("<h1>OpenDrift</h1><div class='sub'>Web configurator &middot; ");
    html += F(OPENDRIFT_VERSION_STRING);
    html += F("</div>");

    html += F("<div class='card'><h2>Live Radio</h2><div class='status'>");
    html += F("<div class='pill'>Steering: ");
    html += String(steeringRadio->getPulseWidth());
    html += steeringRadio->hasSignal() ? F(" OK") : F(" NO SIGNAL");
    html += F("</div><div class='pill'>Gain: ");
    html += String(gainRadio->getPulseWidth());
    html += gainRadio->hasSignal() ? F(" OK") : F(" NO SIGNAL");
    html += F("</div><div class='pill'>Active gain: <strong id='activeGain'>");
    html += gyro != nullptr ? String(gyro->getGain(), 2) : F("--");
    html += F("</strong><br><small id='gainOverride'>Checking gain source...</small>");
    html += F("</div><div class='pill'>Throttle: ");
    html += String(throttleRadio->getPulseWidth());
    html += throttleRadio->hasSignal() ? F(" OK") : F(" NO SIGNAL");
    #if defined(OPENDRIFT_INPUT_CRSF)
    #if defined(OPENDRIFT_CRSF_OOPS_SWAPPED_PINS)
    html += F("</div><div class='pill'>CRSF OOPS: receiver TX to GPIO 17 / RX to GPIO 18");
    #elif defined(OPENDRIFT_AMOLED_V2)
    html += F("</div><div class='pill'>CRSF: GPIO 1 RX / 2 TX");
    #else
    html += F("</div><div class='pill'>CRSF: GPIO 17 RX / 18 TX");
    #endif
    #else
    #if defined(OPENDRIFT_AMOLED_V2)
    html += F("</div><div class='pill'>GPIO 2: ");
    #else
    html += F("</div><div class='pill'>GPIO 18: ");
    #endif
    html += settings->getThrottleOutputEnabled()
        ? F("THROTTLE OUT")
        : F("GAIN INPUT");
    #endif
    html += F("</div></div></div>");

    html += F("<div class='card'><h2>Driving Profiles</h2><p class='sub'>Active: <strong>");
    html += settings->getActiveProfileName();
    html += F("</strong>. Active profiles automatically keep trackside tune changes.</p>");

    for(uint8_t i = 0; i < settings->getProfileCount(); i++)
    {
        const Settings::DrivingProfile* profile =
            settings->getProfile(i);

        if(profile == nullptr)
        {
            continue;
        }

        html += F("<div class='profile");

        if(settings->getActiveProfileIndex() == i)
        {
            html += F(" active");
        }

        html += F("'><div><strong>");
        html += profile->name;
        html += F("</strong><small>Gain ");
        html += String(profile->gain, 2);
        html += F(" &middot; Prediction ");
        html += String(profile->predictionStrength);
        html += F(" &middot; Hold ");
        html += String(profile->gyroHoldBoost);
        html += F(" &middot; Countersteer ");
        html += String(profile->gyroCounterSteerAssist);
        html += F(" &middot; Transition speed ");
        html += String(profile->gyroTransitionSpeed);
        html += F(" &middot; Anti Wobble ");
        html += String(profile->gyroHuntStrength);
        html += F("</small></div>");

        html += F("<form method='post' action='/activate-profile'><input type='hidden' name='profile' value='");
        html += String(i);
        html += F("'><button type='submit'>Activate</button></form>");

        html += F("<form method='post' action='/delete-profile' onsubmit=\"return confirm('Delete this profile?')\"><input type='hidden' name='profile' value='");
        html += String(i);
        html += F("'><button class='danger' type='submit'>Delete</button></form></div>");
    }

    if(settings->getProfileCount() < Settings::MAX_PROFILES)
    {
        html += F("<form class='create-profile' method='post' action='/create-profile'><div><label>New profile name</label><input name='name' type='text' maxlength='23' required placeholder='Example: P-tile'></div><button type='submit'>Create from current tune</button></form>");
    }
    else
    {
        html += F("<p class='sub'>Profile limit reached. Delete one to create another.</p>");
    }

    html += F("</div>");

    html += F("<form method='post' action='/save'>");

    html += F("<div class='card'><h2>Drive &amp; Limits</h2><div class='row'>");
    html += input("Saved gain (fallback)", "gain", String(settings->getGain(), 2), "number", "0.01");
    html += input("Deadband", "deadband", String(settings->getDeadband(), 2), "number", "1");
    html += input("Max correction (% full steering span)", "gyroMax", String(settings->getGyroMaxCorrection()), "number", "1");
    html += F("<p class='sub'>This is the gyro's maximum endpoint-to-endpoint authority. 50% can move from center to one calibrated endpoint; 100% can override one endpoint all the way to the other. Physical endpoint calibration remains the final hard limit.</p>");
    html += F("</div>");
    html += checkbox("Reverse gyro correction", "gyroReverse", settings->getGyroReverse());
    html += F("</div>");

    html += F("<div class='card'><h2>OpenDrift v1.0 Response</h2><div class='row'>");
    html += input("Smoothing", "gyroSmoothing", String(settings->getGyroSmoothing(), 2), "number", "0.01");
    html += F("<label>Gyro sensor LPF</label><select name='gyroLpfMode'><option value='0'");
    if(settings->getGyroLpfMode() == 0) html += F(" selected");
    html += F(">24 Hz - original</option><option value='1'");
    if(settings->getGyroLpfMode() == 1) html += F(" selected");
    html += F(">120 Hz - low latency</option><option value='2'");
    if(settings->getGyroLpfMode() == 2) html += F(" selected");
    html += F(">Off - raw bandwidth</option></select>");
    html += input("Prediction strength (0-100)", "predictionStrength", String(settings->getPredictionStrength()), "number", "1");
    html += input("Anti Wobble (0-100)", "huntStrength", String(settings->getGyroHuntStrength()), "number", "1");
    html += F("<p class='sub'>Anti Wobble controls the depth of OpenDrift's narrow, phase-aware wheel-wobble notch. Start at 50. Raise it only if a repeating wheel oscillation remains; lower it if steering begins to feel soft or unnatural. Zero bypasses the notch and 100 applies its maximum depth.</p>");
    html += F("</div></div>");

    html += F("<div class='card'><h2>Transition Response</h2><p class='sub'>Transition Speed follows the complete chassis direction change. 50 is neutral; lower values add damping for slower transitions and higher values release damping for faster transitions. It never changes the Max Correction ceiling. Compare 25, 50, and 75 at the same tune.</p><div class='row'>");
    html += input("Transition speed (0-100)", "transitionSpeed", String(settings->getGyroTransitionSpeed()), "number", "1");
    html += F("</div></div>");

    html += F("<div class='card'><h2>Drift Assist</h2><p class='sub'>Countersteer Assist changes only the steady steering workload. Zero preserves the base v1.0 response; higher values let OpenDrift carry more of a settled drift.</p><div class='row'>");
    html += input("Countersteer assist (0-100)", "counterSteerAssist", String(settings->getGyroCounterSteerAssist()), "number", "1");
    html += input("Hold assist (0-100)", "gyroHoldBoost", String(settings->getGyroHoldBoost()), "number", "1");
    html += input("Drift memory", "gyroIGain", String(settings->getGyroIntegralGain(), 2), "number", "0.01");
    html += input("Memory limit (us)", "gyroILimit", String(settings->getGyroIntegralLimit()), "number", "1");
    html += F("</div></div>");

    html += F("<div class='card'><h2>Servo</h2>");
    html += checkbox("Reverse servo", "servoReverse", settings->getServoReverse());
    html += F("<label>Control and servo rate</label><select name='controlLoopHz'><option value='250'");
    if(settings->getControlLoopHz() == 250) html += F(" selected");
    html += F(">250 Hz - broad servo compatibility</option><option value='333'");
    if(settings->getControlLoopHz() == 333) html += F(" selected");
    html += F(">333 Hz - supported servos only</option></select><p class='sub'>250 Hz supports a broader range of digital servos. Select 333 Hz only when the servo manufacturer explicitly supports it. A restart is required after changing this setting: save first, then restart.</p>");
    html += F("<button type='submit' form='restartForm' class='secondary'>Restart OpenDrift</button>");
    html += F("<div class='row'>");
    html += input("Center pulse", "servoCenter", String(settings->getServoCenter()));
    html += input("Travel percent", "servoTravel", String(settings->getServoTravel()));
    html += input("Quiet band us", "servoQuiet", String(settings->getServoQuiet()), "number", "1");
    html += F("</div></div>");

    // Same states and colours as the display's endpoint page.
    bool endpointsSaved =
        settings->isSteeringCalibrated();

    if(endpointsSaved)
    {
        endpointCaptureError = false;
    }

    bool steeringSignal =
        steeringRadio != nullptr &&
        steeringRadio->hasSignal();

    uint8_t endpointMask =
        settings->getSteeringCalibrationMask();

    html += F("<div class='card' id='endpoints'><h2>Physical Servo Endpoints</h2><p class='sub'>Status: <strong class='");
    html += endpointsSaved
        ? F("ok")
        : (
            (!steeringSignal || endpointCaptureError)
            ? F("bad")
            : F("")
        );
    html += F("'>");
    html += endpointsSaved
        ? F("CALIBRATED")
        : (
            !steeringSignal
            ? F("NO STEERING SIGNAL")
            : (
                endpointCaptureError
                ? F("INVALID - RETRY")
                : (
                    endpointMask != 0
                    ? F("CAPTURE REMAINING")
                    : F("CAPTURE ALL 3")
                )
            )
        );
    html += F("</strong>. These are the servo's physical PWM stops and the final hard limits for both driver and gyro movement. Steer the wheels to each safe physical stop with the transmitter, then capture it here, on the display, or in the EdgeTX tool. Entering all three pulse values by hand also works.</p>");
    html += F("<p class='sub'>Servo now: <strong id='servoPulse'>");
    html += steeringServo != nullptr
        ? String(steeringServo->getPosition())
        : String(F("--"));
    html += F("</strong> us &middot; steering signal <strong id='steeringSignal'>");
    html += steeringSignal ? F("OK") : F("NONE");
    html += F("</strong></p><div class='endpoints'>");

    static const char* const endpointLabels[3] =
    {
        "Capture left",
        "Capture center",
        "Capture right"
    };

    for(uint8_t point = 0; point < 3; point++)
    {
        bool captured =
            (endpointMask & (1U << point)) != 0;

        html += F("<button type='submit' form='captureEndpoint");
        html += String(point);
        html += captured ? F("'>") : F("' class='danger'>");
        html += endpointLabels[point];

        if(captured)
        {
            html += F("<br><small>");
            html += String(settings->getSteeringCapturedPulse(point));
            html += F(" us</small>");
        }

        html += F("</button>");
    }

    html += F("<button type='submit' form='resetEndpoints' class='secondary'>Reset calibration</button></div><div class='row'>");
    html += input("Max left", "steeringMin", String(settings->getSteeringMin()));
    html += input("Center", "steeringCenter", String(settings->getSteeringCenter()));
    html += input("Max right", "steeringMax", String(settings->getSteeringMax()));
    html += input("Steering travel percent", "radioSteeringTravel", String(settings->getRadioSteeringTravel()), "number", "1");
    html += F("</div></div>");

    html += F("<div class='card'><h2>Gain Channel Calibration</h2><div class='row'>");
    #if defined(OPENDRIFT_INPUT_CRSF)
    #if defined(OPENDRIFT_CRSF_OOPS_SWAPPED_PINS)
    html += F("Personal swapped-pin build: CRSF channel 3 controls gyro gain. GPIO 16 drives the steering servo. GPIO 15 actively outputs neutral throttle during failsafe and passes throttle only after a valid neutral hold. Receiver TX feeds GPIO 17; receiver RX connects to GPIO 18.");
    #elif defined(OPENDRIFT_AMOLED_V2)
    html += F("CRSF channel 3 controls gyro gain. GPIO 15 drives the steering servo. GPIO 16 actively outputs neutral throttle during failsafe and passes throttle only after a valid neutral hold. Receiver TX feeds GPIO 1; receiver RX connects to GPIO 2.");
    #else
    html += F("CRSF channel 3 controls gyro gain. GPIO 15 drives the steering servo. GPIO 16 actively outputs neutral throttle during failsafe and passes throttle only after a valid neutral hold.");
    #endif
    html += F("</div>");
    #else
    html += input("Gain low", "gainMin", String(settings->getGainMin()));
    html += input("Gain high", "gainMax", String(settings->getGainMax()));
    html += F("</div>");
    html += checkbox(
        #if defined(OPENDRIFT_AMOLED_V2)
        "Use GPIO 2 as throttle output instead of gyro gain input",
        #else
        "Use GPIO 18 as throttle output instead of gyro gain input",
        #endif
        "throttleOutputEnabled",
        settings->getThrottleOutputEnabled()
    );
    #endif
    html += F("<div class='row'>");
    html += input("CH3 gain minimum", "channel3GainMin", String(settings->getChannel3GainMin(), 2), "number", "0.05");
    html += input("CH3 gain maximum", "channel3GainMax", String(settings->getChannel3GainMax(), 2), "number", "0.05");
    html += F("</div><p class='sub'>Maps the full Channel 3 control range to gyro gain. Defaults are 0.50 to 3.00; both ends support 0.00 to 6.00.</p>");
    html += F("</div>");

    #if defined(OPENDRIFT_INPUT_CRSF) && defined(OPENDRIFT_BOARD_AMOLED_164)
    html += F("<div class='card'><h2>Auxiliary Channel Outputs</h2><p class='sub'>Route any CRSF channel to a standard 50 Hz receiver-style PWM signal. Outputs return to 1500 us on signal loss. GPIO is 3.3 V signal only: power accessories externally and connect a common ground.</p><div class='row'>");

    for(uint8_t gpio = 1; gpio <= 8; gpio++)
    {
        html += F("<div><label>GPIO ");
        html += String(gpio);

        if(!AuxChannelOutputs::isPinAvailable(gpio))
        {
            html += F("</label><div class='pill'>Reserved for CRSF UART</div></div>");
            continue;
        }

        html += F("</label><select name='auxGpio");
        html += String(gpio);
        html += F("'><option value='0'");

        uint8_t selectedChannel =
            settings->getAuxChannelForGpio(gpio);

        if(selectedChannel == 0)
        {
            html += F(" selected");
        }

        html += F(">Disabled</option>");

        for(uint8_t channel = 1; channel <= 16; channel++)
        {
            html += F("<option value='");
            html += String(channel);
            html += F("'");

            if(selectedChannel == channel)
            {
                html += F(" selected");
            }

            html += F(">CRSF Channel ");
            html += String(channel);
            html += F("</option>");
        }

        html += F("</select></div>");
    }

    html += F("</div><p class='sub'>Mappings take effect immediately after Save Settings. Multiple GPIOs may mirror the same channel.</p></div>");
    #endif

    html += F("<div class='card'><h2>WiFi</h2>");
    html += checkbox("Enable WiFi on boot", "wifiEnabled", settings->getWifiEnabled());
    html += input("Network name (SSID)", "wifiSsid", String(settings->getWifiSsid()), "text", "");
    html += F("<p class='sub'>1-32 letters, numbers, spaces, - _ . Give each car its own name when several OpenDrift boards share a track. A new name applies after a restart, or the next time WiFi is switched on from the display.</p>");

    if(
        wifi != nullptr &&
        wifi->isSsidChangePending()
    )
    {
        // The active name is sanitized to the same character set as the
        // form values, so it is safe to inline.
        html += F("<p class='sub warn'>Rename pending: the access point still broadcasts <strong>");
        html += wifi->getActiveSsid();
        html += F("</strong>. Restart to switch to the new name.</p>");
        html += F("<button type='submit' form='restartForm' class='secondary'>Restart OpenDrift</button>");
    }
    html += input("Auto-off timeout ms", "wifiTimeout", String(settings->getWifiTimeout()));
    html += F("<p class='sub'>Auto-off counts only while no device is connected. A connected device pauses the timer, and a device that is connecting, getting its address, or reconnecting after a drop holds it for 30 seconds more. A disconnect then starts a fresh timeout. 0 never switches WiFi off.</p>");
    html += F("</div>");

    #if defined(OPENDRIFT_BOARD_AMOLED_164)
    html += F("<div class='card'><h2>Display</h2><label>Brightness</label><select name='displayBrightness'>");

    for(uint8_t percent = 10; percent <= 100; percent += 10)
    {
        html += F("<option value='");
        html += String(percent);
        html += F("'");

        if(settings->getDisplayBrightness() == percent)
        {
            html += F(" selected");
        }

        html += F(">");
        html += String(percent);
        html += F("%</option>");
    }

    html += F("</select><p class='sub'>Applies right after Save Settings. The System page on the display has the same control.</p>");
    html += input("Dim after idle (seconds, 0 = never)", "displayDimTimeout", String(settings->getDisplayDimTimeout()), "number", "1");
    html += F("<p class='sub'>After this many seconds without a touch the AMOLED drops to a tenth of its brightness, up to 600 seconds. The first touch only wakes the screen. Off by default.</p></div>");

    #endif

    html += F("<div class='card'><h2>Blackbox</h2>");
    html += checkbox("Enable onboard logging", "blackboxEnabled", settings->getBlackboxEnabled());
    html += F("</div>");

    html += F("<button type='submit'>Save Settings</button></form>");
    html += F("<p class='sub'><a href='/settings.json'>Export settings (JSON)</a> downloads every setting, the endpoint calibration and all profiles as one backup file.</p>");

    // The capture and reset buttons live inside the settings form above,
    // which cannot nest another form, so they target these through their
    // form attribute.
    for(uint8_t point = 0; point < 3; point++)
    {
        html += F("<form id='captureEndpoint");
        html += String(point);
        html += F("' method='post' action='/capture-endpoint'><input type='hidden' name='point' value='");
        html += String(point);
        html += F("'></form>");
    }

    html += F("<form id='resetEndpoints' method='post' action='/reset-endpoints' onsubmit=\"return confirm('Clear the physical endpoint calibration? The servo returns to the plain center and travel map until all three points are captured again.')\"></form>");

    html += F("<div class='card'><h2>Blackbox Log</h2>");

    if(!settings->getBlackboxEnabled())
    {
        html += F("<p class='sub'>Logging disabled. Enable onboard logging and save settings to record logs.</p>");
    }
    else if(blackbox != nullptr && blackbox->isReady())
    {
        html += F("<p class='sub'>Binary records in PSRAM: ");
        html += String(blackbox->getRecordCount());
        html += F(" &middot; used: ");
        html += String(blackbox->getSize() / 1024);
        html += F(" / ");
        html += String(blackbox->getCapacityBytes() / 1024);
        html += F(" KB &middot; duration: ");
        html += String(blackbox->getDurationMs() / 60000);
        html += F("m ");
        html += String((blackbox->getDurationMs() / 1000) % 60);
        html += F("s");

        if(blackbox->isFull())
        {
            html += F(" &middot; retaining newest records");
        }

        if(blackbox->getOverwrittenRows() > 0)
        {
            html += F(" &middot; overwritten: ");
            html += String(blackbox->getOverwrittenRows());
        }

        html += F("</p><p class='sub'>Stage-one logger: records stay entirely in volatile PSRAM. No internal flash writes occur. Download converts the binary records to CSV; power cycling clears the log.</p>");
        html += F("<a href='/blackbox.csv'>Download CSV</a>");
        html += F("<form method='post' action='/clear-log'><button type='submit'>Clear RAM Log</button></form>");
    }
    else
    {
        html += F("<p class='sub'>PSRAM log buffer unavailable.</p>");
    }

    html += F("</div>");

    #if defined(OPENDRIFT_BOARD_AMOLED_164)
    // Outside the settings form on purpose: the Use and Delete buttons
    // are forms of their own, and forms cannot nest.
    html += F("<div class='card' id='backgrounds'><h2>Backgrounds</h2>");

    if(backgrounds == nullptr || !backgrounds->isReady())
    {
        html += F("<p class='sub bad'>Background storage is not available on this board.</p></div>");
    }
    else
    {
        const char* activeBackground =
            settings->getBackgroundName();

        html += F("<p class='sub'>Active: <strong>");
        html += activeBackground[0] != 0 ? activeBackground : "Built-in";
        html += F("</strong> &middot; stored: ");
        html += String((int)backgrounds->getCount());
        html += F(" of ");
        html += String((int)Backgrounds::MAX_BACKGROUNDS);
        html += F(" &middot; free: ");
        html += String((unsigned long)(backgrounds->getFreeBytes() / 1024));
        html += F(" KB</p>");

        html += F("<div class='profile");

        if(activeBackground[0] == 0)
        {
            html += F(" active");
        }

        html += F("'><div><strong>Built-in</strong><small>The image compiled into the firmware</small></div><form method='post' action='/use-background'><input type='hidden' name='name' value=''><button type='submit'>Use</button></form><div></div></div>");

        // Names are sanitized to letters, digits, - and _ so they are safe
        // inside attributes without escaping.
        for(uint8_t i = 0; i < backgrounds->getCount(); i++)
        {
            const char* name =
                backgrounds->getName(i);

            html += F("<div class='profile");

            if(strcmp(name, activeBackground) == 0)
            {
                html += F(" active");
            }

            html += F("'><div><strong>");
            html += name;
            html += F("</strong><small>456 x 280 &middot; 250 KB</small></div>");
            html += F("<form method='post' action='/use-background'><input type='hidden' name='name' value='");
            html += name;
            html += F("'><button type='submit'>Use</button></form>");
            html += F("<form method='post' action='/delete-background' onsubmit=\"return confirm('Delete this background?')\"><input type='hidden' name='name' value='");
            html += name;
            html += F("'><button class='danger' type='submit'>Delete</button></form></div>");
        }

        html += F("<label>Image file</label><input id='bgFile' type='file' accept='image/*'>");
        html += F("<label>Name (letters, digits, - and _)</label><input id='bgName' type='text' maxlength='23' placeholder='Example: track-night'>");
        html += F("<button type='button' class='secondary' onclick='uploadBackground()'>Convert and upload</button>");
        html += F("<p class='sub' id='bgStatus'>Any JPG or PNG. Your browser scales and crops it to 456 x 280 and converts it to the panel's pixel format, so the board only stores 250 KB per image and holds up to 16. Upload at the bench, not while driving: it writes flash.</p>");
        html += F("</div>");
    }
    #endif

    // The settings form above cannot contain another form, so the restart
    // form lives here and the restart buttons elsewhere on the page point
    // at it through their form attribute.
    html += F("<div class='card'><h2>System</h2><p class='sub'>Last reset: <strong>");
    html += resetReasonText(esp_reset_reason());
    html += F("</strong> &middot; up ");
    html += uptimeText();
    html += F(". A crash or watchdog here means the board rebooted on its own; check the serial monitor for the backtrace.</p>");
    html += F("<p class='sub'>Restart applies a changed control rate and a pending WiFi name. Steering is uncontrolled for a few seconds while OpenDrift boots, and the RAM blackbox log is lost.</p>");
    html += F("<form id='restartForm' method='post' action='/restart' onsubmit=\"return confirm('Restart OpenDrift now? Steering is uncontrolled for a few seconds, the RAM blackbox log is lost, and unsaved edits on this page are discarded. Save first if you changed anything.')\"><button type='submit' class='secondary'>Restart OpenDrift</button></form>");
    html += F("<p class='sub'><a href='/settings.json'>Export settings (JSON)</a> before a factory reset to keep a copy of the tune and profiles.</p>");
    html += F("<p class='sub'>Factory reset erases everything this firmware has stored on the board and restarts with defaults.</p>");
    html += F("<form method='post' action='/factory-reset' onsubmit=\"return confirm('Factory reset erases EVERYTHING stored on this board: gyro tune, all driving profiles, physical endpoint calibration, servo center, travel and direction, GPIO and aux channel mappings, WiFi name and options, logging settings, and every uploaded background. OpenDrift restarts with defaults and the WiFi name ");
    html += Settings::defaultWifiSsid();
    html += F(". Continue?')\"><button type='submit' class='danger'>Factory reset</button></form>");
    html += F("</div>");

    html += F("</main><script>function updateLive(){fetch('/live-status',{cache:'no-store'}).then(r=>r.json()).then(s=>{document.getElementById('activeGain').textContent=Number(s.gain).toFixed(2);document.getElementById('gainOverride').textContent=s.override?'CH3 gain override active':'Saved gain active';document.getElementById('servoPulse').textContent=s.servo;document.getElementById('steeringSignal').textContent=s.steering?'OK':'NONE';}).catch(()=>{});}updateLive();setInterval(updateLive,500);");

    #if defined(OPENDRIFT_BOARD_AMOLED_164)
    // Scale and crop to 456 x 280, pack RGB565 little-endian, and post the
    // raw pixels as a multipart file named <name>.rgb. The board never has
    // to decode an image format.
    html += F("function uploadBackground(){var f=document.getElementById('bgFile').files[0];var n=document.getElementById('bgName').value.trim();var st=document.getElementById('bgStatus');if(!f||!n){st.textContent='Choose an image and give it a name.';return;}var img=new Image();img.onload=function(){URL.revokeObjectURL(img.src);var c=document.createElement('canvas');c.width=456;c.height=280;var x=c.getContext('2d');var s=Math.max(456/img.width,280/img.height);var w=img.width*s,h=img.height*s;x.drawImage(img,(456-w)/2,(280-h)/2,w,h);var d=x.getImageData(0,0,456,280).data;var out=new Uint8Array(456*280*2);for(var i=0,j=0;i<d.length;i+=4,j+=2){var v=((d[i]&248)<<8)|((d[i+1]&252)<<3)|(d[i+2]>>3);out[j]=v&255;out[j+1]=v>>8;}var fd=new FormData();fd.append('image',new Blob([out]),n+'.rgb');st.textContent='Uploading 250 KB...';fetch('/upload-background',{method:'POST',body:fd}).then(function(r){return r.text().then(function(t){if(r.ok){location.href='/?r='+Date.now()+'#backgrounds';}else{st.textContent=t;}});}).catch(function(){st.textContent='Upload failed. Stay on the OpenDrift network and try again.';});};img.onerror=function(){st.textContent='The browser could not read that image.';};img.src=URL.createObjectURL(f);}");
    #endif

    html += F("</script></body></html>");

    server.send(
        200,
        "text/html",
        html
    );
}


void WebConfigurator::handleLiveStatus()
{
    if(
        settings == nullptr ||
        gyro == nullptr ||
        gainRadio == nullptr
    )
    {
        server.send(
            503,
            "application/json",
            "{\"error\":\"unavailable\"}"
        );

        return;
    }

    bool gainOverride = gainRadio->hasSignal();

    #if !defined(OPENDRIFT_INPUT_CRSF)
    gainOverride =
        gainOverride &&
        !settings->getThrottleOutputEnabled();
    #endif

    String json;
    json.reserve(128);
    json += F("{\"gain\":");
    json += String(gyro->getGain(), 2);
    json += F(",\"pulse\":");
    json += String(gainRadio->getPulseWidth());
    json += F(",\"override\":");
    json += gainOverride ? F("true") : F("false");
    json += F(",\"steering\":");
    json += (steeringRadio != nullptr && steeringRadio->hasSignal()) ? F("true") : F("false");
    json += F(",\"servo\":");
    json += steeringServo != nullptr
        ? String(steeringServo->getPosition())
        : String(0);
    json += F("}");

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json",
        json
    );
}


void WebConfigurator::handleSave()
{
    if(settings == nullptr)
    {
        server.send(
            503,
            "text/plain",
            "Settings unavailable"
        );

        return;
    }

    settings->setGain(
        getFloatArg(
            "gain",
            settings->getGain()
        )
    );

    settings->setDeadband(
        getFloatArg(
            "deadband",
            settings->getDeadband()
        )
    );

    settings->setGyroReverse(
        server.hasArg("gyroReverse")
    );

    settings->setGyroMaxCorrection(
        getIntArg(
            "gyroMax",
            settings->getGyroMaxCorrection()
        )
    );

    settings->setGyroSmoothing(
        getFloatArg(
            "gyroSmoothing",
            settings->getGyroSmoothing()
        )
    );

    settings->setGyroLpfMode(
        getIntArg(
            "gyroLpfMode",
            settings->getGyroLpfMode()
        )
    );

    settings->setGyroIntegralGain(
        getFloatArg(
            "gyroIGain",
            settings->getGyroIntegralGain()
        )
    );

    settings->setGyroIntegralLimit(
        getIntArg(
            "gyroILimit",
            settings->getGyroIntegralLimit()
        )
    );

    settings->setGyroHoldBoost(
        getIntArg(
            "gyroHoldBoost",
            settings->getGyroHoldBoost()
        )
    );

    settings->setGyroCounterSteerAssist(
        getIntArg(
            "counterSteerAssist",
            settings->getGyroCounterSteerAssist()
        )
    );

    settings->setGyroTransitionSpeed(
        getIntArg(
            "transitionSpeed",
            settings->getGyroTransitionSpeed()
        )
    );

    settings->setPredictionStrength(
        getIntArg(
            "predictionStrength",
            settings->getPredictionStrength()
        )
    );

    settings->setGyroHuntStrength(
        getIntArg(
            "huntStrength",
            settings->getGyroHuntStrength()
        )
    );

    settings->setServoReverse(
        server.hasArg("servoReverse")
    );

    settings->setServoCenter(
        getIntArg(
            "servoCenter",
            settings->getServoCenter()
        )
    );

    settings->setServoTravel(
        getIntArg(
            "servoTravel",
            settings->getServoTravel()
        )
    );

    settings->setServoQuiet(
        getIntArg(
            "servoQuiet",
            settings->getServoQuiet()
        )
    );

    settings->setControlLoopHz(
        getIntArg(
            "controlLoopHz",
            settings->getControlLoopHz()
        )
    );

    int requestedSteeringMin =
        getIntArg(
            "steeringMin",
            settings->getSteeringMin()
        );

    int requestedSteeringCenter =
        getIntArg(
            "steeringCenter",
            settings->getSteeringCenter()
        );

    int requestedSteeringMax =
        getIntArg(
            "steeringMax",
            settings->getSteeringMax()
        );

    bool steeringCalibrationChanged =
        requestedSteeringMin != settings->getSteeringMin() ||
        requestedSteeringCenter != settings->getSteeringCenter() ||
        requestedSteeringMax != settings->getSteeringMax();

    settings->setSteeringMin(requestedSteeringMin);
    settings->setSteeringCenter(requestedSteeringCenter);
    settings->setSteeringMax(requestedSteeringMax);

    if(steeringCalibrationChanged)
    {
        settings->confirmStoredSteeringCalibration();
    }

    settings->setRadioSteeringTravel(
        getIntArg(
            "radioSteeringTravel",
            settings->getRadioSteeringTravel()
        )
    );

    settings->setGainMin(
        getIntArg(
            "gainMin",
            settings->getGainMin()
        )
    );

    settings->setGainMax(
        getIntArg(
            "gainMax",
            settings->getGainMax()
        )
    );

    settings->setChannel3GainMin(
        getFloatArg(
            "channel3GainMin",
            settings->getChannel3GainMin()
        )
    );

    settings->setChannel3GainMax(
        getFloatArg(
            "channel3GainMax",
            settings->getChannel3GainMax()
        )
    );

    #if !defined(OPENDRIFT_INPUT_CRSF)
    settings->setThrottleOutputEnabled(
        server.hasArg("throttleOutputEnabled")
    );
    #endif

    #if defined(OPENDRIFT_INPUT_CRSF) && defined(OPENDRIFT_BOARD_AMOLED_164)
    for(uint8_t gpio = 1; gpio <= 8; gpio++)
    {
        if(!AuxChannelOutputs::isPinAvailable(gpio))
        {
            continue;
        }

        char argument[12];

        snprintf(
            argument,
            sizeof(argument),
            "auxGpio%u",
            gpio
        );

        settings->setAuxChannelForGpio(
            gpio,
            getIntArg(
                argument,
                settings->getAuxChannelForGpio(gpio)
            )
        );
    }
    #endif

    settings->setWifiEnabled(
        server.hasArg("wifiEnabled")
    );

    if(server.hasArg("wifiSsid"))
    {
        settings->setWifiSsid(
            server.arg("wifiSsid")
        );
    }

    settings->setWifiTimeout(
        getIntArg(
            "wifiTimeout",
            settings->getWifiTimeout()
        )
    );

    settings->setBlackboxEnabled(
        server.hasArg("blackboxEnabled")
    );

    #if defined(OPENDRIFT_BOARD_AMOLED_164)
    settings->setDisplayBrightness(
        getIntArg(
            "displayBrightness",
            settings->getDisplayBrightness()
        )
    );

    settings->setDisplayDimTimeout(
        getIntArg(
            "displayDimTimeout",
            settings->getDisplayDimTimeout()
        )
    );
    #endif

    if(gyro != nullptr)
    {
        gyro->setGain(
            settings->getGain()
        );

        gyro->setDeadband(
            settings->getDeadband()
        );

        gyro->setSmoothing(
            settings->getGyroSmoothing()
        );

        gyro->setMaxCorrection(
            settings->getGyroMaxCorrection() * 10
        );

        gyro->setIntegralGain(
            settings->getGyroIntegralGain()
        );

        gyro->setIntegralLimit(
            settings->getGyroIntegralLimit()
        );

        gyro->setHoldBoost(
            settings->getGyroHoldBoost()
        );

        gyro->setCounterSteerAssist(
            settings->getGyroCounterSteerAssist()
        );

        gyro->setTransitionSpeed(
            settings->getGyroTransitionSpeed()
        );

        gyro->setPredictionStrength(
            settings->getPredictionStrength()
        );

        gyro->setHuntStrength(
            settings->getGyroHuntStrength()
        );
    }

    server.sendHeader(
        "Location",
        "/"
    );

    server.send(
        303
    );
}



void WebConfigurator::handleProfileCreate()
{
    if(
        settings == nullptr ||
        !server.hasArg("name")
    )
    {
        server.send(
            400,
            "text/plain",
            "Profile name required"
        );

        return;
    }

    if(settings->createProfile(server.arg("name")) < 0)
    {
        server.send(
            400,
            "text/plain",
            "Could not create profile. Use a unique name and check the profile limit."
        );

        return;
    }

    server.sendHeader("Location", "/");
    server.send(303);
}


void WebConfigurator::handleProfileActivate()
{
    if(
        settings == nullptr ||
        !server.hasArg("profile")
    )
    {
        server.send(400, "text/plain", "Profile required");
        return;
    }

    int index = server.arg("profile").toInt();

    if(
        index < 0 ||
        index >= settings->getProfileCount() ||
        !settings->activateProfile(index)
    )
    {
        server.send(404, "text/plain", "Profile not found");
        return;
    }

    server.sendHeader("Location", "/");
    server.send(303);
}


void WebConfigurator::handleProfileDelete()
{
    if(
        settings == nullptr ||
        !server.hasArg("profile")
    )
    {
        server.send(400, "text/plain", "Profile required");
        return;
    }

    int index = server.arg("profile").toInt();

    if(
        index < 0 ||
        index >= settings->getProfileCount() ||
        !settings->deleteProfile(index)
    )
    {
        server.send(404, "text/plain", "Profile not found");
        return;
    }

    server.sendHeader("Location", "/");
    server.send(303);
}



void WebConfigurator::handleLogDownload()
{
    if(
        settings != nullptr &&
        !settings->getBlackboxEnabled()
    )
    {
        server.send(
            503,
            "text/plain",
            "Blackbox logging disabled"
        );

        return;
    }

    if(
        blackbox == nullptr ||
        !blackbox->isReady()
    )
    {
        server.send(
            503,
            "text/plain",
            "Blackbox log unavailable"
        );

        return;
    }

    server.sendHeader(
        "Content-Disposition",
        "attachment; filename=opendrift-blackbox-" OPENDRIFT_VERSION ".csv"
    );

    server.setContentLength(
        CONTENT_LENGTH_UNKNOWN
    );

    server.send(
        200,
        "text/csv",
        ""
    );

    server.sendContent(
        blackbox->getCsvHeader()
    );
    server.sendContent("\n");

    size_t recordCount =
        blackbox->getRecordCount();

    char line[672];
    String chunk;
    chunk.reserve(8192);

    for(size_t index = 0; index < recordCount; index++)
    {
        size_t length =
            blackbox->formatCsvRecord(
                index,
                line,
                sizeof(line)
            );

        if(length == 0)
        {
            continue;
        }

        if(chunk.length() + length > 8192)
        {
            server.sendContent(chunk);
            chunk = "";

            if(!server.client().connected())
            {
                return;
            }
        }

        chunk.concat(line, length);

        if((index & 0x7F) == 0)
        {
            delay(0);
        }
    }

    if(chunk.length() > 0)
    {
        server.sendContent(chunk);
    }

    server.sendContent("");
}



void WebConfigurator::handleLogClear()
{
    if(
        settings != nullptr &&
        !settings->getBlackboxEnabled()
    )
    {
        server.send(
            503,
            "text/plain",
            "Blackbox logging disabled"
        );

        return;
    }

    if(
        blackbox == nullptr ||
        !blackbox->isReady()
    )
    {
        server.send(
            503,
            "text/plain",
            "Blackbox log unavailable"
        );

        return;
    }

    blackbox->clear();

    server.sendHeader(
        "Location",
        "/"
    );

    server.send(
        303
    );
}



void WebConfigurator::handleSettingsExport()
{
    if(settings == nullptr)
    {
        server.send(
            503,
            "text/plain",
            "Settings unavailable"
        );

        return;
    }

    // Keys are the web form field names so a future import can post the
    // same values straight back through /save. No escaping is needed: the
    // WiFi name and profile names are sanitized to letters, digits, space
    // and - _ . on the way in.
    String json;

    json.reserve(6144);

    json += F("{\"schema\":1,\"version\":\"" OPENDRIFT_VERSION "\",\"build\":\"" OPENDRIFT_BUILD_NAME "\"");

    appendJsonField(json, "gain", String(settings->getGain(), 2));
    appendJsonField(json, "deadband", String(settings->getDeadband(), 2));
    appendJsonField(json, "gyroReverse", jsonBool(settings->getGyroReverse()));
    appendJsonField(json, "gyroMax", String(settings->getGyroMaxCorrection()));
    appendJsonField(json, "gyroSmoothing", String(settings->getGyroSmoothing(), 2));
    appendJsonField(json, "gyroLpfMode", String((int)settings->getGyroLpfMode()));
    appendJsonField(json, "predictionStrength", String(settings->getPredictionStrength()));
    appendJsonField(json, "huntStrength", String(settings->getGyroHuntStrength()));
    appendJsonField(json, "transitionSpeed", String(settings->getGyroTransitionSpeed()));
    appendJsonField(json, "counterSteerAssist", String(settings->getGyroCounterSteerAssist()));
    appendJsonField(json, "gyroHoldBoost", String(settings->getGyroHoldBoost()));
    appendJsonField(json, "gyroIGain", String(settings->getGyroIntegralGain(), 2));
    appendJsonField(json, "gyroILimit", String(settings->getGyroIntegralLimit()));

    appendJsonField(json, "servoReverse", jsonBool(settings->getServoReverse()));
    appendJsonField(json, "controlLoopHz", String((int)settings->getControlLoopHz()));
    appendJsonField(json, "servoCenter", String(settings->getServoCenter()));
    appendJsonField(json, "servoTravel", String(settings->getServoTravel()));
    appendJsonField(json, "servoQuiet", String(settings->getServoQuiet()));

    appendJsonField(json, "steeringMin", String(settings->getSteeringMin()));
    appendJsonField(json, "steeringCenter", String(settings->getSteeringCenter()));
    appendJsonField(json, "steeringMax", String(settings->getSteeringMax()));
    appendJsonField(json, "radioSteeringTravel", String(settings->getRadioSteeringTravel()));

    appendJsonField(json, "gainMin", String(settings->getGainMin()));
    appendJsonField(json, "gainMax", String(settings->getGainMax()));
    appendJsonField(json, "channel3GainMin", String(settings->getChannel3GainMin(), 2));
    appendJsonField(json, "channel3GainMax", String(settings->getChannel3GainMax(), 2));
    appendJsonField(json, "throttleOutputEnabled", jsonBool(settings->getThrottleOutputEnabled()));

    // Emitted on every build so the file layout does not depend on the
    // firmware variant that wrote it.
    for(uint8_t gpio = 1; gpio <= 8; gpio++)
    {
        char key[12];

        snprintf(
            key,
            sizeof(key),
            "auxGpio%u",
            gpio
        );

        appendJsonField(json, key, String((int)settings->getAuxChannelForGpio(gpio)));
    }

    appendJsonField(json, "wifiEnabled", jsonBool(settings->getWifiEnabled()));
    appendJsonField(json, "wifiSsid", jsonString(settings->getWifiSsid()));
    appendJsonField(json, "wifiTimeout", String(settings->getWifiTimeout()));
    appendJsonField(json, "blackboxEnabled", jsonBool(settings->getBlackboxEnabled()));
    appendJsonField(json, "displayBrightness", String((int)settings->getDisplayBrightness()));
    appendJsonField(json, "displayDimTimeout", String((int)settings->getDisplayDimTimeout()));

    json += F(",\"endpointCalibration\":{\"calibrated\":");
    json += jsonBool(settings->isSteeringCalibrated());
    json += F(",\"mask\":");
    json += String((int)settings->getSteeringCalibrationMask());
    json += F(",\"servoPulse\":[");

    for(uint8_t point = 0; point < 3; point++)
    {
        if(point > 0)
        {
            json += ',';
        }

        json += String(settings->getSteeringCapturedPulse(point));
    }

    json += F("],\"inputPulse\":[");

    for(uint8_t point = 0; point < 3; point++)
    {
        if(point > 0)
        {
            json += ',';
        }

        json += String(settings->getSteeringCapturedInputPulse(point));
    }

    json += F("]},\"profiles\":{\"active\":");
    json += String((int)settings->getActiveProfileIndex());
    json += F(",\"activeName\":");
    json += jsonString(settings->getActiveProfileName());
    json += F(",\"items\":[");

    bool firstProfile = true;

    for(uint8_t i = 0; i < settings->getProfileCount(); i++)
    {
        const Settings::DrivingProfile* profile =
            settings->getProfile(i);

        if(profile == nullptr)
        {
            continue;
        }

        if(!firstProfile)
        {
            json += ',';
        }

        firstProfile = false;

        json += F("{\"name\":");
        json += jsonString(profile->name);
        appendJsonField(json, "gain", String(profile->gain, 2));
        appendJsonField(json, "deadband", String(profile->deadband, 2));
        appendJsonField(json, "gyroSmoothing", String(profile->gyroSmoothing, 2));
        appendJsonField(json, "gyroIntegralGain", String(profile->gyroIntegralGain, 2));
        appendJsonField(json, "gyroMaxCorrection", String((int)profile->gyroMaxCorrection));
        appendJsonField(json, "gyroIntegralLimit", String((int)profile->gyroIntegralLimit));
        appendJsonField(json, "gyroHoldBoost", String((int)profile->gyroHoldBoost));
        appendJsonField(json, "predictionStrength", String((int)profile->predictionStrength));
        appendJsonField(json, "radioSteeringTravel", String((int)profile->radioSteeringTravel));
        appendJsonField(json, "gyroCounterSteerAssist", String((int)profile->gyroCounterSteerAssist));
        appendJsonField(json, "gyroTransitionSpeed", String((int)profile->gyroTransitionSpeed));
        appendJsonField(json, "gyroHuntStrength", String((int)profile->gyroHuntStrength));
        json += '}';
    }

    json += F("]}}");

    server.sendHeader(
        "Content-Disposition",
        "attachment; filename=opendrift-settings-" OPENDRIFT_VERSION ".json"
    );

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json",
        json
    );
}



void WebConfigurator::handleRestart()
{
    if(settings == nullptr)
    {
        server.send(
            503,
            "text/plain",
            "Settings unavailable"
        );

        return;
    }

    // Persist anything still waiting for the deferred save so a change
    // made moments ago cannot be lost by the reset.
    settings->flush();

    // The configured name is the one the access point uses after boot.
    sendRestartPage(
        "Restarting",
        settings->getWifiSsid()
    );

    restartAtMs =
        millis() + RESTART_DELAY_MS;
}



void WebConfigurator::handleFactoryReset()
{
    if(settings == nullptr)
    {
        server.send(
            503,
            "text/plain",
            "Settings unavailable"
        );

        return;
    }

    // Nothing is flushed here on purpose: the deferred restart erases
    // the namespace and the board boots with defaults.
    factoryResetPending = true;

    sendRestartPage(
        "Factory reset",
        Settings::defaultWifiSsid()
    );

    restartAtMs =
        millis() + RESTART_DELAY_MS;
}



void WebConfigurator::handleEndpointCapture()
{
    if(
        settings == nullptr ||
        steeringRadio == nullptr ||
        steeringServo == nullptr
    )
    {
        server.send(
            503,
            "text/plain",
            "Endpoint capture unavailable"
        );

        return;
    }

    int point =
        getIntArg(
            "point",
            -1
        );

    if(point < 0 || point > 2)
    {
        server.send(
            400,
            "text/plain",
            "Invalid endpoint"
        );

        return;
    }

    // Same gate and same inputs as the display and the EdgeTX tool: the
    // servo's current position is only meaningful while the transmitter
    // is steering it, and the captured pulse must be a sane servo value.
    if(!steeringRadio->hasSignal())
    {
        endpointCaptureError = true;
    }
    else
    {
        int pulse =
            steeringServo->getPosition();

        if(pulse < 900 || pulse > 2100)
        {
            endpointCaptureError = true;
        }
        else
        {
            endpointCaptureError =
                !settings->captureSteeringCalibrationPoint(
                    (uint8_t)point,
                    pulse,
                    steeringRadio->getPulseWidth()
                );
        }
    }

    server.sendHeader(
        "Location",
        "/#endpoints"
    );

    server.send(
        303
    );
}



void WebConfigurator::handleEndpointReset()
{
    if(settings == nullptr)
    {
        server.send(
            503,
            "text/plain",
            "Settings unavailable"
        );

        return;
    }

    settings->clearSteeringCalibration();

    endpointCaptureError = false;

    server.sendHeader(
        "Location",
        "/#endpoints"
    );

    server.send(
        303
    );
}



#if defined(OPENDRIFT_BOARD_AMOLED_164)
void WebConfigurator::handleBackgroundUploadChunk()
{
    if(backgrounds == nullptr)
    {
        return;
    }

    HTTPUpload& upload =
        server.upload();

    switch(upload.status)
    {
        case UPLOAD_FILE_START:
        {
            // The browser sends <name>.rgb; beginUpload() validates the
            // name and refuses when the list or the partition is full.
            String name =
                upload.filename;

            int dot =
                name.lastIndexOf('.');

            if(dot > 0)
            {
                name = name.substring(0, dot);
            }

            backgroundUploadOk =
                backgrounds->beginUpload(
                    name.c_str()
                );

            break;
        }

        case UPLOAD_FILE_WRITE:
            if(backgroundUploadOk)
            {
                backgroundUploadOk =
                    backgrounds->writeUpload(
                        upload.buf,
                        upload.currentSize
                    );
            }
            break;

        case UPLOAD_FILE_END:
            if(backgroundUploadOk)
            {
                backgroundUploadOk =
                    backgrounds->endUpload();
            }
            else
            {
                backgrounds->abortUpload();
            }
            break;

        default:
            backgrounds->abortUpload();
            backgroundUploadOk = false;
            break;
    }
}



void WebConfigurator::handleBackgroundUpload()
{
    if(
        backgrounds == nullptr ||
        !backgrounds->isReady()
    )
    {
        server.send(
            503,
            "text/plain",
            "Background storage is not available"
        );

        return;
    }

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    if(backgroundUploadOk)
    {
        backgroundUploadOk = false;

        server.send(
            200,
            "text/plain",
            "OK"
        );

        return;
    }

    const char* error =
        backgrounds->getUploadError();

    server.send(
        400,
        "text/plain",
        error[0] != 0 ? error : "No image was received"
    );
}



void WebConfigurator::handleBackgroundUse()
{
    if(settings == nullptr)
    {
        server.send(
            503,
            "text/plain",
            "Settings unavailable"
        );

        return;
    }

    String name =
        Backgrounds::sanitizeName(
            server.arg("name")
        );

    // An unknown name selects the built-in image rather than leaving a
    // dangling choice behind.
    if(
        name.length() > 0 &&
        (
            backgrounds == nullptr ||
            !backgrounds->exists(name.c_str())
        )
    )
    {
        name = "";
    }

    settings->setBackgroundName(
        name
    );

    server.sendHeader(
        "Location",
        "/#backgrounds"
    );

    server.send(
        303
    );
}



void WebConfigurator::handleBackgroundDelete()
{
    if(
        settings == nullptr ||
        backgrounds == nullptr
    )
    {
        server.send(
            503,
            "text/plain",
            "Background storage is not available"
        );

        return;
    }

    String name =
        Backgrounds::sanitizeName(
            server.arg("name")
        );

    if(name.length() > 0)
    {
        backgrounds->remove(
            name.c_str()
        );

        if(strcmp(settings->getBackgroundName(), name.c_str()) == 0)
        {
            settings->setBackgroundName(
                ""
            );
        }
    }

    server.sendHeader(
        "Location",
        "/#backgrounds"
    );

    server.send(
        303
    );
}
#endif



void WebConfigurator::sendRestartPage(
    const char* heading,
    const char* ssid
)
{
    String html;

    html.reserve(1200);

    html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><meta http-equiv='refresh' content='12;url=/'>");
    html += F("<title>OpenDrift</title><style>body{font-family:system-ui,Arial,sans-serif;margin:0;background:#101214;color:#f5f5f5}main{max-width:760px;margin:0 auto;padding:18px}h1{font-size:28px;margin:8px 0 2px}p{color:#aeb4bb;font-size:16px}a{color:#65b7ff}</style></head><body><main><h1>");
    html += heading;
    html += F("</h1><p>OpenDrift is restarting. Rejoin the WiFi network <strong>");
    html += ssid;
    html += F("</strong> in about 10 seconds. This page reloads by itself once you are back on the network, or <a href='/'>reload it</a> yourself.</p>");
    html += F("<p>The RAM blackbox log does not survive a restart.</p></main></body></html>");

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "text/html",
        html
    );
}



void WebConfigurator::handleNotFound()
{
    server.sendHeader(
        "Location",
        "/"
    );

    server.send(
        302
    );
}



String WebConfigurator::input(
    const char* label,
    const char* name,
    String value,
    const char* type,
    const char* step
)
{
    String html;

    html += F("<div><label>");
    html += label;
    html += F("</label><input name='");
    html += name;
    html += F("' type='");
    html += type;
    html += F("' step='");
    html += step;
    html += F("' value='");
    html += value;
    html += F("'></div>");

    return html;
}



String WebConfigurator::checkbox(
    const char* label,
    const char* name,
    bool checked
)
{
    String html;

    html += F("<label><input name='");
    html += name;
    html += F("' type='checkbox'");

    if(checked)
    {
        html += F(" checked");
    }

    html += F(">");
    html += label;
    html += F("</label>");

    return html;
}



int WebConfigurator::getIntArg(
    const char* name,
    int fallback
)
{
    if(!server.hasArg(name))
    {
        return fallback;
    }

    return server.arg(name).toInt();
}



float WebConfigurator::getFloatArg(
    const char* name,
    float fallback
)
{
    if(!server.hasArg(name))
    {
        return fallback;
    }

    return server.arg(name).toFloat();
}
