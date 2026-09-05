#include "WiFiManager.h"

#include <stdio.h>
#include <string.h>


WiFiManager* WiFiManager::eventTarget = nullptr;


// Runs on the WiFi event task. It only counts stations and stamps a
// timestamp, so nothing here touches the access point or the settings.
void WiFiManager::onWifiEvent(
    arduino_event_id_t event,
    arduino_event_info_t info
)
{
    (void)info;

    switch(event)
    {
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            if(eventTarget != nullptr && eventTarget->eventStationCount < 16)
            {
                eventTarget->eventStationCount++;
            }
            break;

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            if(eventTarget != nullptr && eventTarget->eventStationCount > 0)
            {
                eventTarget->eventStationCount--;
            }
            break;

        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            break;

        default:
            return;
    }

    if(eventTarget != nullptr)
    {
        eventTarget->lastStationEventMs = millis();
    }
}




void WiFiManager::begin(
    const char* ssid,
    const char* password,
    bool startEnabled,
    const char* hostname
)
{

    wifiSSID = ssid;

    wifiPassword = password;

    wifiHostname = hostname;

    localName = String(hostname) + ".local";

    eventTarget = this;

    WiFi.onEvent(
        onWifiEvent
    );


    if(startEnabled)
    {
        enable();
    }

}





void WiFiManager::enable()
{

    if(enabled)
        return;

    if(
        wifiSSID == nullptr ||
        wifiPassword == nullptr
    )
    {
        Serial.println(
            "WiFi credentials not set"
        );

        return;
    }



    WiFi.mode(
        WIFI_AP
    );


    // Capture the name this AP actually starts with. wifiSSID points at
    // the live setting, which the web configurator can change while the
    // AP is running; that change only applies on the next enable().
    snprintf(
        activeSsid,
        sizeof(activeSsid),
        "%s",
        wifiSSID
    );

    WiFi.softAP(
        activeSsid,
        wifiPassword
    );


    // mDNS serves opendrift.local to iOS, macOS, Windows and Linux clients.
    mdnsRunning = MDNS.begin(wifiHostname);

    if(mdnsRunning)
    {
        MDNS.addService("http", "tcp", 80);
    }

    // Android browsers do not resolve .local through mDNS. The access point
    // is already the DHCP-assigned DNS server, so answer the same name here.
    // Only this one name is answered; every other query gets NXDOMAIN.
    dnsServer.start(
        53,
        localName,
        WiFi.softAPIP()
    );


    enabled = true;


    noClientSince = millis();
    clientWasPresent = false;
    lastStationEventMs = 0;
    eventStationCount = 0;



    Serial.println(
        "WiFi Enabled"
    );

    Serial.printf(
        "WiFi AP: %s\n",
        activeSsid
    );

    Serial.printf(
        "mDNS %s: http://%s/\n",
        mdnsRunning ? "OK" : "FAILED",
        localName.c_str()
    );

}





void WiFiManager::disable()
{

    if(!enabled)
        return;



    dnsServer.stop();

    if(mdnsRunning)
    {
        MDNS.end();
        mdnsRunning = false;
    }

    activeSsid[0] = 0;


    WiFi.softAPdisconnect(
        true
    );


    WiFi.mode(
        WIFI_OFF
    );


    enabled = false;
    noClientSince = 0;
    clientWasPresent = false;
    lastStationEventMs = 0;
    eventStationCount = 0;



    Serial.println(
        "WiFi Disabled"
    );

}





void WiFiManager::update()
{

    if(!enabled)
        return;


    dnsServer.processNextRequest();



    unsigned long now = millis();

    // A station that is still handshaking, fetching its address, or
    // reconnecting after a brief drop is not in the station list yet, but
    // its events are. Treat recent activity as presence.
    unsigned long stationEventMs = lastStationEventMs;

    bool stationActivity =
        stationEventMs != 0 &&
        now - stationEventMs < STATION_GRACE_MS;

    bool clientPresent =
        hasClient() ||
        stationActivity;

    if(clientPresent)
    {
        clientWasPresent = true;
        noClientSince = 0;
        return;
    }

    if(clientWasPresent)
    {
        clientWasPresent = false;
        noClientSince = now;
    }
    else if(noClientSince == 0)
    {
        noClientSince = now;
    }



    // The WiFi page is where someone goes to connect. Do not switch off
    // while it is on screen; the timer starts fresh once it is left.
    if(autoOffHold)
    {
        noClientSince = now;
        return;
    }

    if(
        timeout > 0 &&
        now - noClientSince
        > timeout
    )
    {

        disable();

    }

}





bool WiFiManager::hasClient()
{
    return getClientCount() > 0;
}



uint8_t WiFiManager::getClientCount()
{
    if(!enabled)
    {
        return 0;
    }

    uint8_t reported =
        WiFi.softAPgetStationNum();

    uint8_t counted =
        getEventClientCount();

    return counted > reported ? counted : reported;
}



uint8_t WiFiManager::getEventClientCount()
{
    int8_t count = eventStationCount;

    return count > 0 ? (uint8_t)count : 0;
}



void WiFiManager::holdAutoOff(
    bool hold
)
{
    autoOffHold = hold;
}





bool WiFiManager::isEnabled()
{

    return enabled;

}



const char* WiFiManager::getHostname()
{
    return wifiHostname;
}



String WiFiManager::getLocalName()
{
    return localName;
}



const char* WiFiManager::getActiveSsid()
{
    return activeSsid;
}



bool WiFiManager::isSsidChangePending()
{
    return
        enabled &&
        wifiSSID != nullptr &&
        strcmp(activeSsid, wifiSSID) != 0;
}



void WiFiManager::setTimeout(
    unsigned long timeoutMs
)
{
    timeout =
        timeoutMs;
}
