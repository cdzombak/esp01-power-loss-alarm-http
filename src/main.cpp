#include <Arduino.h>
#include <ESP8266WiFi.h> // https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html
#include <ESP8266HTTPClient.h> // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/src/ESP8266HTTPClient.h

#include "config.h"

const uint8_t ledPin = 1;
const uint8_t vccSensPin = 3;

// core program state:

int vccSensState = 2;

WiFiClient wfClient;
HTTPClient httpClient;

// health ping mgmt:

const unsigned long healthPingInterval = 3 * 60 * 1000;
unsigned long lastHealthPingAt = 0;

// forward declarations:

int sendAlarm(const String& alarm);
int sendHealthPing();
void ledToggle();

// program core:

void setup() {
    pinMode(vccSensPin, INPUT);
    pinMode(ledPin, OUTPUT);

    WiFi.mode(WIFI_STA);
    WiFi.hostname(CFG_HOSTNAME);
    WiFi.begin(CFG_WIFI_ESSID, CFG_WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        yield();
        delay(500);
        ledToggle();
    }

    WiFi.setSleep(true);
}

void loop() {
    bool vccSensStateChanged = false;

    if (digitalRead(vccSensPin) == HIGH && vccSensState < 5) {
        if (vccSensState == 4) {
            vccSensStateChanged = true;
        }
        vccSensState++;
    } else if (digitalRead(vccSensPin) == LOW && vccSensState > 0) {
        if (vccSensState != 0) {
            vccSensStateChanged = true;
        }
        vccSensState--;
    }

    if (vccSensStateChanged && vccSensState == 0) {
        sendAlarm(PWR_LOSS_NOTIF);
    } else if (vccSensStateChanged && vccSensState == 5) {
        sendAlarm(PWR_RESTORE_NOTIF);
    }

    ESP.wdtFeed();

    if (millis() - lastHealthPingAt > healthPingInterval) {
        int httpResponseCode = sendHealthPing();
        if (httpResponseCode >= 200 && httpResponseCode < 300) {
            lastHealthPingAt = millis();
        }
    }

    ledToggle();
    delay(100);
}

int sendAlarm(const String& alarm) {
    httpClient.begin(wfClient, API_HOST, API_PORT, API_ALARM_PATH, API_HTTPS);
    httpClient.addHeader("Authorization", API_AUTH_HDR_VALUE);
    httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    int retv = httpClient.POST(alarm);
    httpClient.end();
    return retv;
}

int sendHealthPing() {
    httpClient.begin(wfClient, API_HOST, API_PORT, API_HEALTH_PING_PATH, API_HTTPS);
    httpClient.addHeader("Authorization", API_AUTH_HDR_VALUE);
    httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    int retv = httpClient.POST("");
    httpClient.end();
    return retv;
}

unsigned long lastLedToggleAt = 0;
void ledToggle() {
    if (millis() - lastLedToggleAt > 250) {
        lastLedToggleAt = millis();
        digitalWrite(ledPin, !digitalRead(ledPin));
    }
}
