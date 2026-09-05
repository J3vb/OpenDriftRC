#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>


class WiFiManager
{

public:

    void begin(
        const char* ssid,
        const char* password,
        bool startEnabled = true,
        const char* hostname = "opendrift"
    );


    void update();



    void enable();


    void disable();



    bool isEnabled();


    bool hasClient();

    // Stations currently associated with the access point.
    uint8_t getClientCount();

    // Hostname without suffix, e.g. "opendrift".
    const char* getHostname();

    // Fully qualified local name, e.g. "opendrift.local".
    String getLocalName();

    // Name the running access point was started with. Empty while
    // disabled. The configured name can change while the AP is up; it
    // only takes effect on the next enable().
    const char* getActiveSsid();

    // True while the AP is running under a different name than the one
    // currently configured.
    bool isSsidChangePending();

    void setTimeout(
        unsigned long timeoutMs
    );



private:

    const char* wifiSSID = nullptr;

    const char* wifiPassword = nullptr;

    const char* wifiHostname = "opendrift";

    String localName;

    DNSServer dnsServer;

    bool mdnsRunning = false;

    static constexpr size_t SSID_LENGTH = 33;

    char activeSsid[SSID_LENGTH] = {0};


    bool enabled = false;


    unsigned long noClientSince = 0;

    bool clientWasPresent = false;


    unsigned long timeout =
        40000;

    // A station that connects, gets its address or drops off holds the
    // auto-off timer for this long, so a slow handshake or a laptop that
    // briefly reconnects cannot be cut off halfway.
    static constexpr unsigned long STATION_GRACE_MS = 30000;

    volatile unsigned long lastStationEventMs = 0;

    static WiFiManager* eventTarget;

    static void onWifiEvent(
        arduino_event_id_t event,
        arduino_event_info_t info
    );
};
