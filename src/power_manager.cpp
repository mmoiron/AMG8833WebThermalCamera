#include "power_manager.h"
#include "config.h"

static int      client_count = 0;
static bool     idle_active  = false;
static uint32_t last_client_disconnect_ms = 0;

void power_init() {
    client_count = 0;
    idle_active  = true;  // start idle until first client
    last_client_disconnect_ms = millis();
}

void power_client_connected() {
    client_count++;
    idle_active = false;
    Serial.printf("[Power] Client connected (%d total)\n", client_count);
}

void power_client_disconnected() {
    if (client_count > 0) client_count--;
    if (client_count == 0) {
        last_client_disconnect_ms = millis();
    }
    Serial.printf("[Power] Client disconnected (%d remaining)\n", client_count);
}

void power_update() {
    if (client_count > 0) {
        idle_active = false;
        return;
    }

    SystemConfig& cfg = config_get();
    uint32_t elapsed = millis() - last_client_disconnect_ms;

    if (elapsed >= (uint32_t)(cfg.idle_timeout_sec * 1000)) {
        if (!idle_active) {
            idle_active = true;
            Serial.println("[Power] Entering idle mode");
        }
    }
}

bool power_is_idle() {
    return idle_active;
}

int power_active_fps() {
    SystemConfig& cfg = config_get();
    return idle_active ? cfg.idle_fps : cfg.normal_fps;
}

int power_client_count() {
    return client_count;
}
