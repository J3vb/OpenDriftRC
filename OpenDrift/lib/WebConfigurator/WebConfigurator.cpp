#include "WebConfigurator.h"

#if defined(OPENDRIFT_INPUT_CRSF) && defined(OPENDRIFT_BOARD_AMOLED_164)
#include "AuxChannelOutputs.h"
#endif

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
    BatteryCompensation& batteryCompRef,
    BatterySense& batterySenseRef
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

    batteryComp =
        &batteryCompRef;

    batterySense =
        &batterySenseRef;

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
        "/battery.js",
        HTTP_GET,
        [this]()
        {
            handleBatteryScript();
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
        "/clear-log",
        HTTP_POST,
        [this]()
        {
            handleLogClear();
        }
    );

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

    html.reserve(30000);

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
    html += F(".profile{display:grid;grid-template-columns:1fr 96px 82px;gap:8px;align-items:center;background:#0b0d10;border:1px solid #33383f;border-radius:6px;padding:9px;margin:8px 0}.profile.active{border-color:#24a36b}.profile strong{display:block}.profile small{color:#aeb4bb}.profile form{margin:0}.profile button{margin:0;padding:9px 6px;font-size:13px}.profile .danger{background:#973b45}.create-profile{display:grid;grid-template-columns:1fr 150px;gap:10px;align-items:end}.create-profile button{margin:0;height:43px}");
    html += F("a{color:#65b7ff}@media(max-width:560px){.row,.status,.create-profile{grid-template-columns:1fr}.profile{grid-template-columns:1fr 1fr}.profile>div{grid-column:1/-1}}");
    html += F("</style></head><body><main>");
    html += F("<h1>OpenDrift</h1><div class='sub'>Web configurator</div>");

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
        html += F(" &middot; Battery comp ");
        html += profile->batteryCompEnabled ? F("on") : F("off");
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
    html += F(">333 Hz - supported servos only</option></select><p class='sub'>250 Hz supports a broader range of digital servos. Select 333 Hz only when the servo manufacturer explicitly supports it. A restart is required after changing this setting.</p>");
    html += F("<div class='row'>");
    html += input("Center pulse", "servoCenter", String(settings->getServoCenter()));
    html += input("Travel percent", "servoTravel", String(settings->getServoTravel()));
    html += input("Quiet band us", "servoQuiet", String(settings->getServoQuiet()), "number", "1");
    html += F("</div></div>");

    html += F("<div class='card'><h2>Physical Servo Endpoints</h2><p class='sub'>Status: <strong>");
    html += settings->isSteeringCalibrated() ? F("CALIBRATED") : F("NOT CALIBRATED");
    html += F("</strong>. These are the servo's physical PWM stops and the final hard limits for both driver and gyro movement. Position the wheels at each safe physical endpoint and capture it from the display or EdgeTX tool, or enter all three pulse values below.</p><div class='row'>");
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

    html += F("<div class='card'><h2>Battery Compensation</h2>");
    html += F("<p class='sub'>Scales throttle below full stick so a fresh pack feels like a partly used one. Full stick always passes through unchanged; brake and reverse are never touched. Settings are stored in the active profile. Needs the sense divider described in Hardware.md and the ESC driven by OpenDrift.</p>");
    #if !defined(OPENDRIFT_INPUT_CRSF)
    if(!settings->getThrottleOutputEnabled())
    {
        #if defined(OPENDRIFT_AMOLED_V2)
        html += F("<div class='pill' style='border-color:#c9a227'>GPIO 2 is in gain-input mode, so the ESC is not driven by OpenDrift and compensation has no effect. Switch GPIO 2 to throttle output and plug the ESC into it.</div>");
        #else
        html += F("<div class='pill' style='border-color:#c9a227'>GPIO 18 is in gain-input mode, so the ESC is not driven by OpenDrift and compensation has no effect. Switch GPIO 18 to throttle output and plug the ESC into it.</div>");
        #endif
    }
    #endif
    html += F("<div class='status'>");
    html += F("<div class='pill'>Pack: <strong id='batRaw'>--</strong> V raw &middot; <strong id='batFilt'>--</strong> V filtered</div>");
    html += F("<div class='pill'>Resting: <strong id='batRest'>--</strong> V &middot; applied <strong id='batComp'>--</strong>%</div>");
    html += F("<div class='pill'>Status: <strong id='batStatus'>checking...</strong></div>");
    html += F("<div class='pill'>Forward throttle: <strong id='batFwd'>--</strong></div>");
    html += F("</div>");
    html += F("<canvas id='batVoltGraph' width='720' height='230' style='width:100%;height:auto;margin-top:12px;background:#0b0d10;border:1px solid #33383f;border-radius:6px'></canvas>");
    html += F("<canvas id='batThrGraph' width='720' height='260' style='width:100%;height:auto;margin-top:10px;background:#0b0d10;border:1px solid #33383f;border-radius:6px'></canvas>");
    html += checkbox("Enable battery compensation", "batEnabled", settings->getBatteryCompEnabled());
    html += F("<div class='row'>");
    html += input("Start voltage (V, max compensation)", "batStartV", String(settings->getBatteryCompStartVoltage(), 1), "number", "0.1");
    html += input("End voltage (V, no compensation, the pack the car should feel like)", "batEndV", String(settings->getBatteryCompEndVoltage(), 1), "number", "0.1");
    html += input("Strength (% of the physical amount, 100 = feels exactly like the end voltage)", "batStrength", String(settings->getBatteryCompStrength()), "number", "1");
    html += F("<div><label>Curve type</label><select name='batCurve'><option value='0'");
    if(settings->getBatteryCompCurve() == 0) html += F(" selected");
    html += F(">Linear - fades evenly to zero at full stick</option><option value='1'");
    if(settings->getBatteryCompCurve() == 1) html += F(" selected");
    html += F(">Expo - fades early, no slope change at full stick</option><option value='2'");
    if(settings->getBatteryCompCurve() == 2) html += F(" selected");
    html += F(">Custom - full compensation up to the knee, then fade</option></select></div>");
    html += input("Custom knee (% throttle, Custom curve only)", "batKnee", String(settings->getBatteryCompKnee()), "number", "5");
    html += F("<div><label>Voltage filter (resting estimate)</label><select name='batFilterMs'>");
    {
        const int presets[] = {500, 1000, 2000, 5000, 10000};
        const char* labels[] = {"0.5 s", "1 s", "2 s", "5 s", "10 s"};
        for(int i = 0; i < 5; i++)
        {
            html += F("<option value='");
            html += String(presets[i]);
            html += F("'");
            if(settings->getBatteryCompFilterMs() == presets[i]) html += F(" selected");
            html += F(">");
            html += labels[i];
            html += F("</option>");
        }
    }
    html += F("</select></div>");
    html += input("Voltage drop rate (s)", "batDropS", String(settings->getBatteryCompDropMs() / 1000.0f, 1), "number", "0.5");
    html += input("Voltage recovery rate (s)", "batRiseS", String(settings->getBatteryCompRecoveryMs() / 1000.0f, 1), "number", "1");
    html += F("</div>");
    html += checkbox("Use resting voltage (sampled while the throttle is lifted) instead of the filtered voltage", "batResting", settings->getBatteryCompUseResting());
    html += F("<p class='sub'>Resting voltage ignores the sag of a short throttle burst, so the feel stays constant through a corner. The filtered voltage follows the drop and recovery rates and is always shown and logged.</p>");
    html += F("<h2>Battery Sense Hardware</h2><p class='sub'>Global settings. Pack + through a 47k/15k divider with 100 nF at the pin; see Hardware.md. Anything above 9.2 V is rejected, so a 3S pack disables compensation.</p><div class='row'>");
    html += F("<div><label>Sense pin</label><select name='batPin'><option value='0'");
    if(settings->getBatterySensePin() == 0) html += F(" selected");
    html += F(">Off</option>");
    for(uint8_t gpio = 5; gpio <= 8; gpio++)
    {
        if(!Settings::isBatterySensePinAllowed(gpio)) continue;
        html += F("<option value='");
        html += String(gpio);
        html += F("'");
        if(settings->getBatterySensePin() == gpio) html += F(" selected");
        html += F(">GPIO ");
        html += String(gpio);
        html += F("</option>");
    }
    html += F("</select></div>");
    html += input("Voltage scale (pack volts per volt at the pin)", "batScale", String(settings->getBatteryVoltageScale(), 3), "number", "0.001");
    html += input("Measured pack voltage (type the multimeter reading to calibrate the scale)", "batMeasured", "", "number", "0.01");
    html += F("<div><label>Pin reading</label><div class='pill'>");
    if(batterySense != nullptr && batterySense->hasSample())
    {
        html += String(batterySense->getPinMillivolts());
        html += F(" mV at the pin");
    }
    else
    {
        html += F("no sample");
    }
    html += F("</div></div></div>");
    html += checkbox("Throttle reversed (forward is below 1500 us on this radio/ESC)", "batThrRev", settings->getBatteryThrottleReversed());
    html += F("</div>");

    #if defined(OPENDRIFT_INPUT_CRSF) && defined(OPENDRIFT_BOARD_AMOLED_164)
    html += F("<div class='card'><h2>Auxiliary Channel Outputs</h2><p class='sub'>Route any CRSF channel to a standard 50 Hz receiver-style PWM signal. Outputs return to 1500 us on signal loss. GPIO is 3.3 V signal only: power accessories externally and connect a common ground.</p><div class='row'>");

    for(uint8_t gpio = 1; gpio <= 8; gpio++)
    {
        html += F("<div><label>GPIO ");
        html += String(gpio);

        if(gpio == settings->getBatterySensePin())
        {
            html += F("</label><div class='pill'>Reserved for battery sense</div></div>");
            continue;
        }

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
    html += input("Auto-off timeout ms", "wifiTimeout", String(settings->getWifiTimeout()));
    html += F("<p class='sub'>Auto-off counts only while no device is connected. A connected phone pauses the timer; a disconnect starts a fresh timeout.</p>");
    html += F("</div>");

    html += F("<div class='card'><h2>Blackbox</h2>");
    html += checkbox("Enable onboard logging", "blackboxEnabled", settings->getBlackboxEnabled());
    html += F("</div>");

    html += F("<button type='submit'>Save Settings</button></form>");

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

    html += F("</main><script>function updateLive(){fetch('/live-status',{cache:'no-store'}).then(r=>r.json()).then(s=>{document.getElementById('activeGain').textContent=Number(s.gain).toFixed(2);document.getElementById('gainOverride').textContent=s.override?'CH3 gain override active':'Saved gain active';if(window.batteryLive){window.batteryLive(s);}}).catch(()=>{});}updateLive();setInterval(updateLive,500);</script><script src='/battery.js'></script></body></html>");

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
    json.reserve(224);
    json += F("{\"gain\":");
    json += String(gyro->getGain(), 2);
    json += F(",\"pulse\":");
    json += String(gainRadio->getPulseWidth());
    json += F(",\"override\":");
    json += gainOverride ? F("true") : F("false");

    if(batteryComp != nullptr)
    {
        json += F(",\"vraw\":");
        json += String(batteryComp->getRawVolts(), 2);
        json += F(",\"vfilt\":");
        json += String(batteryComp->getFilteredVolts(), 2);
        json += F(",\"vrest\":");
        json += String(batteryComp->getRestingVolts(), 2);
        json += F(",\"comp\":");
        json += String(batteryComp->getCompensationPercent(), 1);
        json += F(",\"bstat\":\"");
        json += batteryComp->getFaultText();
        json += F("\",\"ben\":");
        json += batteryComp->getConfig().enabled ? F("true") : F("false");
        json += F(",\"brev\":");
        json += batteryComp->getConfig().throttleReversed ? F("true") : F("false");
        json += F(",\"binit\":");
        json += batteryComp->isInitialised() ? F("true") : F("false");
    }

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


namespace
{
    // Served from /battery.js so the page String stays small. Mirrors
    // BatteryCompensation::shapeWeight() and apply() for the live preview.
    const char BATTERY_SCRIPT[] PROGMEM = R"JS((function(){
var q=function(n){return document.querySelector("[name='"+n+"']");};
var live={vraw:0,vfilt:0,vrest:0,comp:0,stat:'',en:false,rev:false,init:false,seen:false};
function num(el,d){var v=parseFloat(el?el.value:'');return isNaN(v)?d:v;}
function cfg(){var c=q('batCurve');return{sv:num(q('batStartV'),8.4),ev:num(q('batEndV'),7.4),str:num(q('batStrength'),100),curve:c?parseInt(c.value,10)||0:0,knee:num(q('batKnee'),50),rest:!!(q('batResting')&&q('batResting').checked),en:!!(q('batEnabled')&&q('batEnabled').checked)};}
function weight(t,curve,knee){var k=curve===2?Math.min(90,Math.max(0,knee))/100:0;var u=(1-t)/(1-k);u=Math.max(0,Math.min(1,u));return curve===1?u*u:u;}
function compAt(c,v){var span=c.sv-c.ev;if(span<0.199)return 0;var x=Math.max(0,Math.min(1,(v-c.ev)/span));return (c.str/100)*(1-c.ev/c.sv)*x;}
function outUs(c,v,inUs){var fwd=inUs-1500;if(fwd<=0)return inUs;var t=fwd/500;return 1500+Math.round(500*t*(1-compAt(c,v)*weight(t,c.curve,c.knee)));}
function sourceVolts(c){if(!live.seen||!live.init)return null;return c.rest?live.vrest:live.vfilt;}
function frame(ctx,W,H,pad,title){ctx.clearRect(0,0,W,H);ctx.fillStyle='#0b0d10';ctx.fillRect(0,0,W,H);ctx.strokeStyle='#3b4148';ctx.lineWidth=1;ctx.strokeRect(pad.l,pad.t,W-pad.l-pad.r,H-pad.t-pad.b);ctx.fillStyle='#c8cdd2';ctx.font='14px system-ui,Arial,sans-serif';ctx.fillText(title,pad.l,pad.t-8);}
function label(ctx,text,x,y,align){ctx.fillStyle='#aeb4bb';ctx.font='12px system-ui,Arial,sans-serif';ctx.textAlign=align||'left';ctx.fillText(text,x,y);ctx.textAlign='left';}
function drawVolt(){var cv=document.getElementById('batVoltGraph');if(!cv)return;var ctx=cv.getContext('2d');var W=cv.width,H=cv.height,pad={l:52,r:16,t:28,b:34};var c=cfg();var vmin=6.9,vmax=8.5;var cmax=Math.max(5,compAt(c,c.sv)*100*1.15);
frame(ctx,W,H,pad,'Compensation at low throttle vs pack voltage');
var gx=function(v){return pad.l+(v-vmin)/(vmax-vmin)*(W-pad.l-pad.r);},gy=function(p){return H-pad.b-p/cmax*(H-pad.t-pad.b);};
for(var v=7.0;v<=8.4001;v+=0.2){ctx.strokeStyle='#1f2429';ctx.beginPath();ctx.moveTo(gx(v),pad.t);ctx.lineTo(gx(v),H-pad.b);ctx.stroke();label(ctx,v.toFixed(1)+' V',gx(v),H-pad.b+16,'center');}
for(var p=0;p<=cmax;p+=(cmax>20?10:5)){ctx.strokeStyle='#1f2429';ctx.beginPath();ctx.moveTo(pad.l,gy(p));ctx.lineTo(W-pad.r,gy(p));ctx.stroke();label(ctx,p.toFixed(0)+'%',pad.l-6,gy(p)+4,'right');}
ctx.strokeStyle=c.en?'#24a36b':'#5c6570';ctx.lineWidth=2.5;ctx.beginPath();for(var i=0;i<=160;i++){var vv=vmin+(vmax-vmin)*i/160;var y=gy(compAt(c,vv)*100);if(i===0)ctx.moveTo(gx(vv),y);else ctx.lineTo(gx(vv),y);}ctx.stroke();ctx.lineWidth=1;
ctx.strokeStyle='#65b7ff';ctx.setLineDash([4,4]);ctx.beginPath();ctx.moveTo(gx(c.ev),pad.t);ctx.lineTo(gx(c.ev),H-pad.b);ctx.moveTo(gx(c.sv),pad.t);ctx.lineTo(gx(c.sv),H-pad.b);ctx.stroke();ctx.setLineDash([]);label(ctx,'end '+c.ev.toFixed(1),gx(c.ev)+4,pad.t+14);label(ctx,'start '+c.sv.toFixed(1),gx(c.sv)-4,pad.t+14,'right');
var sv=sourceVolts(c);if(sv!==null){var x=gx(Math.max(vmin,Math.min(vmax,sv)));ctx.strokeStyle='#d24fd1';ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(x,pad.t);ctx.lineTo(x,H-pad.b);ctx.stroke();ctx.lineWidth=1;ctx.fillStyle='#d24fd1';ctx.beginPath();ctx.arc(x,gy(compAt(c,sv)*100),5,0,6.283);ctx.fill();label(ctx,'now '+sv.toFixed(2)+' V, '+(compAt(c,sv)*100).toFixed(1)+'%',x+8,gy(compAt(c,sv)*100)-8);}
}
function drawThr(){var cv=document.getElementById('batThrGraph');if(!cv)return;var ctx=cv.getContext('2d');var W=cv.width,H=cv.height,pad={l:52,r:16,t:28,b:34};var c=cfg();var sv=sourceVolts(c);var v=sv===null?c.sv:sv;
frame(ctx,W,H,pad,'Throttle in vs ESC out at '+(sv===null?'start voltage '+c.sv.toFixed(1)+' V (no live reading)':v.toFixed(2)+' V live'));
var gx=function(p){return pad.l+p/100*(W-pad.l-pad.r);},gy=function(p){return H-pad.b-p/100*(H-pad.t-pad.b);};
for(var p=0;p<=100;p+=25){ctx.strokeStyle='#1f2429';ctx.beginPath();ctx.moveTo(gx(p),pad.t);ctx.lineTo(gx(p),H-pad.b);ctx.moveTo(pad.l,gy(p));ctx.lineTo(W-pad.r,gy(p));ctx.stroke();label(ctx,p+'%',gx(p),H-pad.b+16,'center');label(ctx,p+'%',pad.l-6,gy(p)+4,'right');}
label(ctx,'stick',W-pad.r,H-pad.b+30,'right');
ctx.strokeStyle='#5c6570';ctx.setLineDash([5,5]);ctx.beginPath();ctx.moveTo(gx(0),gy(0));ctx.lineTo(gx(100),gy(100));ctx.stroke();ctx.setLineDash([]);
var plot=function(volts,color,width){ctx.strokeStyle=color;ctx.lineWidth=width;ctx.beginPath();for(var i=0;i<=100;i++){var inUs=1500+i*5;var o=(outUs(c,volts,inUs)-1500)/5;if(i===0)ctx.moveTo(gx(i),gy(o));else ctx.lineTo(gx(i),gy(o));}ctx.stroke();ctx.lineWidth=1;};
if(sv!==null&&Math.abs(sv-c.sv)>0.02){plot(c.sv,'#2c5a44',1.5);}
plot(v,c.en?'#24a36b':'#5c6570',2.5);
[25,50,75].forEach(function(p){var o=(outUs(c,v,1500+p*5)-1500)/5;ctx.fillStyle='#d24fd1';ctx.beginPath();ctx.arc(gx(p),gy(o),4,0,6.283);ctx.fill();label(ctx,p+'% -> '+o.toFixed(0)+'%',gx(p)+8,gy(o)+14);});
}
function redraw(){drawVolt();drawThr();}
window.batteryOut=function(v,inUs){return outUs(cfg(),v,inUs);};
function text(id,t){var el=document.getElementById(id);if(el)el.textContent=t;}
window.batteryLive=function(s){if(s.bstat===undefined)return;live.seen=true;live.vraw=Number(s.vraw);live.vfilt=Number(s.vfilt);live.vrest=Number(s.vrest);live.comp=Number(s.comp);live.stat=s.bstat;live.en=!!s.ben;live.rev=!!s.brev;live.init=!!s.binit;
text('batRaw',live.init||live.stat==='OUT OF RANGE'?live.vraw.toFixed(2):'--');text('batFilt',live.init?live.vfilt.toFixed(2):'--');text('batRest',live.init?live.vrest.toFixed(2):'--');text('batComp',live.comp.toFixed(1));
text('batStatus',live.stat!=='OK'?live.stat:(live.en?'ACTIVE':'DISABLED IN PROFILE'));text('batFwd',live.rev?'below 1500 us (reversed)':'above 1500 us');redraw();};
['batStartV','batEndV','batStrength','batCurve','batKnee','batResting','batEnabled'].forEach(function(n){var el=q(n);if(el){el.addEventListener('input',redraw);el.addEventListener('change',redraw);}});
redraw();
})();)JS";
}


void WebConfigurator::handleBatteryScript()
{
    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send_P(
        200,
        "application/javascript",
        BATTERY_SCRIPT
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

    settings->setBatteryCompEnabled(
        server.hasArg("batEnabled")
    );

    settings->setBatteryCompStartVoltage(
        getFloatArg(
            "batStartV",
            settings->getBatteryCompStartVoltage()
        )
    );

    settings->setBatteryCompEndVoltage(
        getFloatArg(
            "batEndV",
            settings->getBatteryCompEndVoltage()
        )
    );

    settings->setBatteryCompStrength(
        getIntArg(
            "batStrength",
            settings->getBatteryCompStrength()
        )
    );

    settings->setBatteryCompCurve(
        getIntArg(
            "batCurve",
            settings->getBatteryCompCurve()
        )
    );

    settings->setBatteryCompKnee(
        getIntArg(
            "batKnee",
            settings->getBatteryCompKnee()
        )
    );

    settings->setBatteryCompFilterMs(
        getIntArg(
            "batFilterMs",
            settings->getBatteryCompFilterMs()
        )
    );

    settings->setBatteryCompDropMs(
        (int)lroundf(
            getFloatArg(
                "batDropS",
                settings->getBatteryCompDropMs() / 1000.0f
            ) * 1000.0f
        )
    );

    settings->setBatteryCompRecoveryMs(
        (int)lroundf(
            getFloatArg(
                "batRiseS",
                settings->getBatteryCompRecoveryMs() / 1000.0f
            ) * 1000.0f
        )
    );

    settings->setBatteryCompUseResting(
        server.hasArg("batResting")
    );

    settings->setBatterySensePin(
        getIntArg(
            "batPin",
            settings->getBatterySensePin()
        )
    );

    settings->setBatteryVoltageScale(
        getFloatArg(
            "batScale",
            settings->getBatteryVoltageScale()
        )
    );

    settings->setBatteryThrottleReversed(
        server.hasArg("batThrRev")
    );

    // Calibration: a typed multimeter reading rescales the divider so the
    // live voltage matches it. Ignored when empty or without a sample.
    if(
        server.hasArg("batMeasured") &&
        batterySense != nullptr &&
        batterySense->hasSample() &&
        batterySense->getPinMillivolts() > 0
    )
    {
        float measured = server.arg("batMeasured").toFloat();

        if(measured >= 5.0f && measured <= 9.5f)
        {
            settings->setBatteryVoltageScale(
                measured * 1000.0f / (float)batterySense->getPinMillivolts()
            );
        }
    }

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

    settings->setWifiTimeout(
        getIntArg(
            "wifiTimeout",
            settings->getWifiTimeout()
        )
    );

    settings->setBlackboxEnabled(
        server.hasArg("blackboxEnabled")
    );

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
        "attachment; filename=opendrift-blackbox.csv"
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

    char line[800];
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
