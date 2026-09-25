#pragma once

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

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

    // True only after DHCP has assigned an address. Merely associating with
    // the access point is not enough for the web configurator to be usable.
    bool hasClient();

    String getLocalName();
    void setTimeout(unsigned long timeoutMs);

private:
    bool startAccessPoint();
    bool restartDhcpServer();
    void restartAccessPoint();
    void updateClientCounts();

    const char* wifiSSID = nullptr;
    const char* wifiPassword = nullptr;
    const char* wifiHostname = "opendrift";

    bool mdnsRunning = false;
    bool enabled = false;

    uint8_t associatedClientCount = 0;
    uint8_t ipClientCount = 0;
    unsigned long lastHealthCheck = 0;
    unsigned long dhcpPendingSince = 0;
    uint8_t dhcpRecoveryAttempts = 0;

    unsigned long noClientSince = 0;
    bool clientWasPresent = false;
    unsigned long timeout = 40000;

    static constexpr unsigned long HEALTH_CHECK_INTERVAL_MS = 250;
    static constexpr unsigned long DHCP_LEASE_TIMEOUT_MS = 6000;
};
