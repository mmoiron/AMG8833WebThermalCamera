#include <Arduino.h>
#include <LittleFS.h>

#include "config.h"
#include "thermal_sensor.h"
#include "temporal_filter.h"
#include "stats.h"
#include "power_manager.h"
#include "wifi_manager.h"
#include "webserver.h"
#include "ws_protocol.h"

// ── Static buffers ──────────────────────────────────

static float         raw_pixels[64];
static float         proc_pixels[64];
static FrameStats    frame_stats;
static ws_payload_t  payload;

static uint32_t last_frame_ms    = 0;
static uint32_t last_wifi_check  = 0;

// ── Processing pipeline ─────────────────────────────

static void process_and_stream() {
    SystemConfig& cfg = config_get();

    // 1) Read raw 8x8 frame
    if (!sensor_read(raw_pixels)) return;

    // Copy to processing buffer
    memcpy(proc_pixels, raw_pixels, sizeof(proc_pixels));

    // 2) Apply calibration offset
    if (cfg.calibration_offset != 0.0f) {
        for (int i = 0; i < 64; i++) {
            proc_pixels[i] += cfg.calibration_offset;
        }
    }

    // 3) Apply temporal IIR filter (optional)
    if (cfg.temporal_enabled) {
        filter_apply(proc_pixels, cfg.alpha);
    }

    // 4) Compute statistics
    stats_compute(proc_pixels, frame_stats);

    // 5) Build and stream payload
    payload.timestamp_ms      = millis();
    payload.current_fps       = (uint8_t)power_active_fps();
    payload.flags             = 0;
    if (cfg.temporal_enabled)   payload.flags |= WS_FLAG_TEMPORAL_ENABLED;
    if (power_is_idle())        payload.flags |= WS_FLAG_IDLE_ACTIVE;
    if (wifi_sta_connected())   payload.flags |= WS_FLAG_STA_CONNECTED;
    payload.calibration_offset = cfg.calibration_offset;
    payload.tmin              = frame_stats.tmin;
    payload.tmax              = frame_stats.tmax;
    payload.tmean             = frame_stats.tmean;
    payload.hotspot_x         = frame_stats.hotspot_x;
    payload.hotspot_y         = frame_stats.hotspot_y;
    memcpy(payload.pixels, proc_pixels, sizeof(proc_pixels));

    webserver_broadcast((const uint8_t*)&payload, sizeof(payload));
}

// ── Arduino setup ───────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== AMG8833 Web Thermal Camera ===");
    Serial.printf("Version: %s\n", VERSION_STR);

    // Mount filesystem
    if (!LittleFS.begin()) {
        Serial.println("[FS] LittleFS mount failed!");
    } else {
        Serial.println("[FS] LittleFS mounted");
    }

    // Init subsystems
    config_init();
    config_load();
    filter_init();
    power_init();
    wifi_init();

    // Init sensor
    if (!sensor_init()) {
        Serial.println("[FATAL] Sensor init failed — halting");
        while (true) {
            delay(1000);
            yield();
        }
    }

    webserver_init();

    Serial.println("[Main] Setup complete, entering main loop");
}

// ── Arduino loop ────────────────────────────────────

void loop() {
    uint32_t now = millis();

    // Update power manager
    power_update();

    // Determine frame interval based on active FPS
    int fps = power_active_fps();
    uint32_t interval_ms = 1000 / fps;

    // Frame acquisition
    if (now - last_frame_ms >= interval_ms) {
        last_frame_ms = now;

#if STOP_SENSOR_WHEN_IDLE
        if (!power_is_idle()) {
            process_and_stream();
        }
#else
        // Always acquire, even in idle (at reduced rate)
        if (power_client_count() > 0 || !power_is_idle()) {
            process_and_stream();
        }
#endif
    }

    // Periodic WiFi STA check (every 5 seconds)
    if (now - last_wifi_check >= 5000) {
        last_wifi_check = now;
        wifi_update();
    }

    yield();
}
