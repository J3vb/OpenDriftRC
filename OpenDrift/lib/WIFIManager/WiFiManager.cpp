#include "WiFiManager.h"

#include <stdio.h>
#include <string.h>




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
    bool clientPresent = hasClient();

    if(clientPresent)
    {
        if(!clientWasPresent)
        {
            Serial.println("WiFi client connected; auto-off paused");
        }

        clientWasPresent = true;
        noClientSince = 0;
        return;
    }

    if(clientWasPresent)
    {
        Serial.println("WiFi client disconnected; auto-off timer started");
        clientWasPresent = false;
        noClientSince = now;
    }
    else if(noClientSince == 0)
    {
        noClientSince = now;
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

    return (
        WiFi.softAPgetStationNum()
        > 0
    );

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
