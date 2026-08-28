#include "WiFiManager.h"




void WiFiManager::begin(
    const char* ssid,
    const char* password,
    bool startEnabled
)
{

    wifiSSID = ssid;

    wifiPassword = password;


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


    WiFi.softAP(
        wifiSSID,
        wifiPassword
    );


    enabled = true;


    noClientSince = millis();
    clientWasPresent = false;



    Serial.println(
        "WiFi Enabled"
    );

}





void WiFiManager::disable()
{

    if(!enabled)
        return;



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



void WiFiManager::setTimeout(
    unsigned long timeoutMs
)
{
    timeout =
        timeoutMs;
}
