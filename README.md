# AMG8833 Web Thermal Camera

ESP8266 (Wemos D1 Mini) firmware that reads an AMG8833 8x8 IR thermal sensor and streams data to a smartphone-friendly web interface via WiFi.

## Pinout

| Wemos D1 Mini | AMG8833  |
|---------------|----------|
| 3V3           | VIN      |
| GND           | GND      |
| D2 (GPIO4)    | SDA      |
| D1 (GPIO5)    | SCL      |

I2C runs at 400kHz. Use short wires (< 15cm). No external pull-ups needed (internal pull-ups used).

## Build & Flash

### Prerequisites

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html) or PlatformIO IDE (VS Code extension)

### Build

```bash
cd AMG8833WebThermalCamera
pio run
```

### Upload Firmware

```bash
pio run --target upload
```

### Upload Web Files (LittleFS)

```bash
pio run --target uploadfs
```

**Important**: Upload filesystem **before** first boot, or the web UI will not be served.

### Serial Monitor

```bash
pio device monitor
```

## Usage

1. Power the Wemos D1 Mini
2. Connect your phone to WiFi network **THERMAL_ESP** (password: `thermal1234`)
3. Open browser: **http://192.168.4.1**

### WiFi Station Mode (optional)

The ESP can also connect to your existing WiFi network:

1. In the web UI, expand "WiFi Station" section
2. Enable STA, enter your SSID and password, click Save
3. The ESP will connect and display its IP address
4. You can now access the camera from your local network

The Access Point always remains active as fallback.

## Architecture

```
ESP8266                          Browser
┌────────────────────┐          ┌──────────────────────┐
│ AMG8833 (I2C)      │          │ Interpolation         │
│ ↓                  │          │  Nearest/Bilinear/    │
│ Calibration offset │   WS     │  Bicubic              │
│ ↓                  │ binary   │ Ironbow colormap      │
│ Temporal IIR filter│ ──────→  │ Autoscale + smoothing │
│ ↓                  │ 280 B    │ Legend rendering       │
│ Statistics         │          │ Hotspot overlay       │
│ ↓                  │  REST    │ Responsive UI         │
│ WebSocket stream   │ ←──────  │ Config controls       │
└────────────────────┘  JSON    └──────────────────────┘
```

All heavy visualization (interpolation, colormap) runs in the browser. The ESP only streams raw 8x8 data + statistics.

## WebSocket Binary Frame (280 bytes)

| Offset | Size | Type    | Field               |
|--------|------|---------|---------------------|
| 0      | 4    | uint32  | timestamp_ms        |
| 4      | 1    | uint8   | current_fps         |
| 5      | 1    | uint8   | flags               |
| 6      | 4    | float32 | calibration_offset  |
| 10     | 4    | float32 | tmin                |
| 14     | 4    | float32 | tmax                |
| 18     | 4    | float32 | tmean               |
| 22     | 1    | uint8   | hotspot_x (0-7)     |
| 23     | 1    | uint8   | hotspot_y (0-7)     |
| 24     | 256  | float32 | pixels[64] row-major|

Flags: bit0 = temporal filter on, bit1 = idle mode active, bit2 = STA connected.

All multi-byte values are little-endian (ESP8266 native).

## Calibration

A global offset (-5.0 to +5.0 C, step 0.1) is added to every pixel before filtering and statistics. Adjustable from the UI slider.

The code is structured to support future multi-point calibration (gain + offset per pixel), but only simple offset is implemented.

## Power Management

| State  | Condition                          | FPS        |
|--------|------------------------------------|------------|
| Active | WebSocket clients connected        | normal_fps |
| Idle   | No clients for `idle_timeout_sec`  | idle_fps   |

Compile-time option `STOP_SENSOR_WHEN_IDLE` (default 0) can halt sensor reads entirely when idle.

## Configuration

All settings persist across reboots (stored in LittleFS as JSON).

REST API:
- `GET /api/config` — read current config
- `POST /api/config` — update config (partial JSON accepted)

## Performance Notes

- ESP8266 has ~80KB usable RAM. All buffers are statically allocated.
- AMG8833 maximum sample rate is 10 FPS.
- WebSocket payload is 280 bytes/frame. At 10 FPS = 2.8 KB/s.
- Bicubic interpolation may be slow on older phones. Bilinear is recommended default.
- Keep web files small. Browser caches them after first load.

## Project Structure

```
AMG8833WebThermalCamera/
├── platformio.ini
├── src/
│   ├── main.cpp              # Setup + main loop + pipeline
│   ├── config.h/cpp          # Config struct + LittleFS persistence
│   ├── thermal_sensor.h/cpp  # AMG8833 I2C driver
│   ├── temporal_filter.h/cpp # Per-pixel IIR filter
│   ├── stats.h/cpp           # Min/max/mean/hotspot
│   ├── power_manager.h/cpp   # Idle detection + FPS control
│   ├── wifi_manager.h/cpp    # AP + STA management
│   ├── webserver.h/cpp       # HTTP server + WebSocket
│   └── ws_protocol.h         # Binary payload struct
└── data/www/
    ├── index.html             # Web UI
    ├── style.css              # Styles
    └── app.js                 # Visualization engine
```
