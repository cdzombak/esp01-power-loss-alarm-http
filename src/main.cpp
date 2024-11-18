#include <Arduino.h>
#include <ESP8266WiFi.h> // https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html
#include <ESP8266HTTPClient.h> // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/src/ESP8266HTTPClient.h

#include "config.h"
// #define DEBUG

const uint8_t ledPin = 1;
const uint8_t vccSensPin = 3;

// core program state:

int vccSensState = 2;
unsigned long lastVccCheckAt = 0;

// health ping mgmt:

const unsigned long healthPingInterval = 3 * 60 * 1000;
unsigned long lastHealthPingAt = healthPingInterval + 1;

// forward declarations:

int sendAlarm(const String& alarm);
int sendHealthPing();
void ledToggle();
void debug(const String& msg);

// program core:

void setup() {
    pinMode(vccSensPin, INPUT);

    #ifdef DEBUG
        Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
    #else
        pinMode(ledPin, OUTPUT);
    #endif

    debug("connecting to WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.hostname(CFG_HOSTNAME);
    WiFi.begin(CFG_WIFI_ESSID, CFG_WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        yield();
        delay(500);
        ledToggle();
        debug("... WiFi not connected ...");
    }
    debug("WiFi connected");

    WiFi.setSleep(true);
}

void loop() {
    if (millis() - lastVccCheckAt > 100) {
        lastVccCheckAt = millis();
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
            int httpResponseCode = sendAlarm(PWR_LOSS_NOTIF);
            if (httpResponseCode < 200 || httpResponseCode >= 300) {
                vccSensState = 1;
            }
        } else if (vccSensStateChanged && vccSensState == 5) {
            sendAlarm(PWR_RESTORE_NOTIF);
        }
    }

    if (millis() - lastHealthPingAt > healthPingInterval) {
        int httpResponseCode = sendHealthPing();
        if (httpResponseCode >= 200 && httpResponseCode < 300) {
            lastHealthPingAt = millis();
        }
    }

    ledToggle();
}

int sendAlarm(const String& alarm) {
    debug("alarm: " + alarm);
    WiFiClient client;
    HTTPClient http;
    http.begin(client, API_HOST, API_PORT, API_ALARM_PATH, API_HTTPS);
    http.addHeader("Authorization", API_AUTH_HDR_VALUE);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    int retv = http.POST(alarm);
    http.end();
    debug("alarm response: " + String(retv));
    return retv;
}

int sendHealthPing() {
    debug("health ping");
    WiFiClient client;
    HTTPClient http;
    http.begin(client, API_HOST, API_PORT, API_HEALTH_PING_PATH, API_HTTPS);
    http.addHeader("Authorization", API_AUTH_HDR_VALUE);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    int retv = http.POST("");
    http.end();
    debug("health response: " + String(retv));
    return retv;
}

unsigned long lastLedToggleAt = 0;
void ledToggle() {
    #ifndef DEBUG
    if (millis() - lastLedToggleAt > 250) {
        lastLedToggleAt = millis();
        digitalWrite(ledPin, !digitalRead(ledPin));
    }
    #endif
}

void debug(const String& msg) {
    #ifdef DEBUG
    Serial.println(msg);
    #endif
}
