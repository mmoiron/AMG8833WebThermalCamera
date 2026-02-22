#include "webserver.h"
#include "config.h"
#include "power_manager.h"
#include "wifi_manager.h"
#include "temporal_filter.h"

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

static AsyncWebServer  server(80);
static AsyncWebSocket  ws("/ws");

// Deferred WiFi restart — avoids killing the HTTP response
static bool     wifi_restart_pending = false;
static uint32_t wifi_restart_at_ms   = 0;

// Buffer for POST body (config JSON is small, <256 bytes)
static uint8_t  post_body_buf[512];
static size_t   post_body_len = 0;
static bool     post_body_ready = false;

// ── WebSocket events ────────────────────────────────

static void onWsEvent(AsyncWebSocket* srv, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected from %s\n",
                client->id(), client->remoteIP().toString().c_str());
            power_client_connected();
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client #%u disconnected\n", client->id());
            power_client_disconnected();
            break;

        case WS_EVT_ERROR:
            Serial.printf("[WS] Client #%u error\n", client->id());
            break;

        case WS_EVT_DATA:
            // No incoming data expected from clients
            break;

        default:
            break;
    }
}

// ── REST API: GET /api/config ───────────────────────

static void handleGetConfig(AsyncWebServerRequest* request) {
    SystemConfig& cfg = config_get();
    StaticJsonDocument<512> doc;

    doc["normal_fps"]        = cfg.normal_fps;
    doc["idle_fps"]          = cfg.idle_fps;
    doc["idle_timeout_sec"]  = cfg.idle_timeout_sec;
    doc["temporal_enabled"]  = cfg.temporal_enabled;
    doc["alpha"]             = cfg.alpha;
    doc["calibration_offset"] = cfg.calibration_offset;
    doc["sta_enabled"]       = cfg.sta_enabled;
    doc["sta_ssid"]          = cfg.sta_ssid;
    // Don't expose password
    doc["sta_ip"]            = wifi_sta_ip();
    doc["sta_connected"]     = wifi_sta_connected();
    doc["clients"]           = power_client_count();
    doc["idle"]              = power_is_idle();
    doc["version"]           = VERSION_STR;

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

// ── REST API: POST /api/config ──────────────────────

// Body handler: accumulates incoming data
static void handlePostConfigBody(AsyncWebServerRequest* request,
                                 uint8_t* data, size_t len, size_t index, size_t total)
{
    if (index == 0) {
        post_body_len = 0;
        post_body_ready = false;
    }
    if (index + len <= sizeof(post_body_buf)) {
        memcpy(post_body_buf + index, data, len);
        post_body_len = index + len;
    }
    if (index + len == total) {
        post_body_ready = true;
    }
}

// Request handler: called after body is complete
static void handlePostConfigRequest(AsyncWebServerRequest* request) {
    if (!post_body_ready || post_body_len == 0) {
        request->send(400, "application/json", "{\"error\":\"no body\"}");
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, post_body_buf, post_body_len);
    if (err) {
        Serial.printf("[Web] POST /api/config parse error: %s\n", err.c_str());
        request->send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }

    Serial.println("[Web] POST /api/config received");

    SystemConfig& cfg = config_get();
    bool need_wifi_restart = false;

    if (doc.containsKey("normal_fps"))
        cfg.normal_fps = config_clamp_fps(doc["normal_fps"]);

    if (doc.containsKey("idle_fps"))
        cfg.idle_fps = config_clamp_fps(doc["idle_fps"]);

    if (doc.containsKey("idle_timeout_sec"))
        cfg.idle_timeout_sec = constrain((int)doc["idle_timeout_sec"], 1, 300);

    if (doc.containsKey("temporal_enabled")) {
        bool new_val = doc["temporal_enabled"];
        if (new_val != cfg.temporal_enabled) {
            cfg.temporal_enabled = new_val;
            if (!new_val) filter_reset();
        }
    }

    if (doc.containsKey("alpha"))
        cfg.alpha = config_clamp_alpha(doc["alpha"]);

    if (doc.containsKey("calibration_offset"))
        cfg.calibration_offset = config_clamp_offset(doc["calibration_offset"]);

    if (doc.containsKey("sta_enabled")) {
        bool new_val = doc["sta_enabled"];
        if (new_val != cfg.sta_enabled) {
            cfg.sta_enabled = new_val;
            need_wifi_restart = true;
        }
    }

    if (doc.containsKey("sta_ssid")) {
        const char* ssid = doc["sta_ssid"];
        if (strcmp(ssid, cfg.sta_ssid) != 0) {
            strlcpy(cfg.sta_ssid, ssid, sizeof(cfg.sta_ssid));
            need_wifi_restart = true;
        }
    }

    if (doc.containsKey("sta_password")) {
        const char* pass = doc["sta_password"];
        strlcpy(cfg.sta_password, pass, sizeof(cfg.sta_password));
        need_wifi_restart = true;
    }

    config_save();

    // Send response BEFORE restarting WiFi
    request->send(200, "application/json", "{\"ok\":true}");
    Serial.println("[Web] POST /api/config OK");

    if (need_wifi_restart) {
        wifi_restart_pending = true;
        wifi_restart_at_ms = millis() + 500;
        Serial.println("[Web] WiFi restart scheduled in 500ms");
    }
}

// ── Init ────────────────────────────────────────────

void webserver_init() {
    // Serve static files from LittleFS — no cache to avoid stale JS
    server.serveStatic("/", LittleFS, "/www/")
        .setDefaultFile("index.html")
        .setCacheControl("no-cache, no-store, must-revalidate");

    // API endpoints
    server.on("/api/config", HTTP_GET, handleGetConfig);

    // POST config — request handler processes after body is accumulated
    server.on("/api/config", HTTP_POST,
        handlePostConfigRequest,
        nullptr,
        handlePostConfigBody);

    // WebSocket
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.begin();
    Serial.println("[Web] Server started on port 80");
}

void webserver_loop() {
    if (wifi_restart_pending && millis() >= wifi_restart_at_ms) {
        wifi_restart_pending = false;
        Serial.println("[Web] Applying deferred WiFi restart");
        wifi_init();
    }
}

void webserver_broadcast(const uint8_t* data, size_t len) {
    ws.binaryAll(const_cast<uint8_t*>(data), len);
    ws.cleanupClients();
}
