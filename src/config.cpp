#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static SystemConfig cfg;

static const char* CONFIG_PATH = "/config.json";

void config_init() {
    // Set defaults
    cfg.normal_fps        = 5;
    cfg.idle_fps          = 1;
    cfg.idle_timeout_sec  = 10;
    cfg.temporal_enabled  = false;
    cfg.alpha             = 0.3f;
    cfg.calibration_offset = 0.0f;
    cfg.sta_enabled       = false;
    memset(cfg.sta_ssid, 0, sizeof(cfg.sta_ssid));
    memset(cfg.sta_password, 0, sizeof(cfg.sta_password));
}

void config_load() {
    config_init();  // start with defaults

    if (!LittleFS.exists(CONFIG_PATH)) {
        Serial.println("[Config] No config file, using defaults");
        return;
    }

    File f = LittleFS.open(CONFIG_PATH, "r");
    if (!f) {
        Serial.println("[Config] Failed to open config file");
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[Config] Parse error: %s\n", err.c_str());
        return;
    }

    cfg.normal_fps        = config_clamp_fps(doc["normal_fps"] | 5);
    cfg.idle_fps          = config_clamp_fps(doc["idle_fps"] | 1);
    cfg.idle_timeout_sec  = constrain((int)(doc["idle_timeout_sec"] | 10), 1, 300);
    cfg.temporal_enabled  = doc["temporal_enabled"] | false;
    cfg.alpha             = config_clamp_alpha(doc["alpha"] | 0.3f);
    cfg.calibration_offset = config_clamp_offset(doc["calibration_offset"] | 0.0f);
    cfg.sta_enabled       = doc["sta_enabled"] | false;

    strlcpy(cfg.sta_ssid,     doc["sta_ssid"] | "",     sizeof(cfg.sta_ssid));
    strlcpy(cfg.sta_password, doc["sta_password"] | "",  sizeof(cfg.sta_password));

    Serial.println("[Config] Loaded from flash");
}

void config_save() {
    StaticJsonDocument<512> doc;

    doc["normal_fps"]        = cfg.normal_fps;
    doc["idle_fps"]          = cfg.idle_fps;
    doc["idle_timeout_sec"]  = cfg.idle_timeout_sec;
    doc["temporal_enabled"]  = cfg.temporal_enabled;
    doc["alpha"]             = cfg.alpha;
    doc["calibration_offset"] = cfg.calibration_offset;
    doc["sta_enabled"]       = cfg.sta_enabled;
    doc["sta_ssid"]          = cfg.sta_ssid;
    doc["sta_password"]      = cfg.sta_password;

    File f = LittleFS.open(CONFIG_PATH, "w");
    if (!f) {
        Serial.println("[Config] Failed to write config");
        return;
    }
    serializeJson(doc, f);
    f.close();
    Serial.println("[Config] Saved to flash");
}

SystemConfig& config_get() {
    return cfg;
}

int config_clamp_fps(int fps) {
    if (fps <= 1) return 1;
    if (fps <= 5) return 5;
    return 10;
}

float config_clamp_alpha(float a) {
    if (a < 0.05f) return 0.05f;
    if (a > 0.8f)  return 0.8f;
    return a;
}

float config_clamp_offset(float o) {
    if (o < -5.0f) return -5.0f;
    if (o >  5.0f) return  5.0f;
    return o;
}
