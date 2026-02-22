#include "wifi_manager.h"
#include "config.h"
#include <ESP8266WiFi.h>

#define AP_SSID     "THERMAL_ESP"
#define AP_PASSWORD "thermal1234"
#define AP_CHANNEL  1

static bool sta_was_connected = false;

void wifi_init() {
    SystemConfig& cfg = config_get();

    if (cfg.sta_enabled && strlen(cfg.sta_ssid) > 0) {
        // AP + STA mode
        WiFi.mode(WIFI_AP_STA);
        Serial.printf("[WiFi] AP+STA mode, connecting to '%s'\n", cfg.sta_ssid);
        WiFi.begin(cfg.sta_ssid, cfg.sta_password);
    } else {
        // AP only
        WiFi.mode(WIFI_AP);
        Serial.println("[WiFi] AP-only mode");
    }

    // Configure AP
    WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1),
        IPAddress(192, 168, 4, 1),
        IPAddress(255, 255, 255, 0)
    );
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, 4);

    Serial.printf("[WiFi] AP started: %s @ %s\n",
        AP_SSID, WiFi.softAPIP().toString().c_str());
}

void wifi_update() {
    SystemConfig& cfg = config_get();
    if (!cfg.sta_enabled) return;

    bool connected = WiFi.isConnected();
    if (connected && !sta_was_connected) {
        Serial.printf("[WiFi] STA connected: %s\n", WiFi.localIP().toString().c_str());
    } else if (!connected && sta_was_connected) {
        Serial.println("[WiFi] STA disconnected, reconnecting...");
        WiFi.begin(cfg.sta_ssid, cfg.sta_password);
    }
    sta_was_connected = connected;
}

bool wifi_sta_connected() {
    return WiFi.isConnected();
}

String wifi_sta_ip() {
    if (WiFi.isConnected()) {
        return WiFi.localIP().toString();
    }
    return "";
}
