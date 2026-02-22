#ifndef WS_PROTOCOL_H
#define WS_PROTOCOL_H

#include <stdint.h>

// WebSocket binary payload — packed to avoid alignment padding
struct __attribute__((packed)) ws_payload_t {
    uint32_t timestamp_ms;
    uint8_t  current_fps;
    uint8_t  flags;               // bit0: temporal_enabled
                                  // bit1: idle_mode_active
                                  // bit2: sta_connected
    float    calibration_offset;
    float    tmin;
    float    tmax;
    float    tmean;
    uint8_t  hotspot_x;
    uint8_t  hotspot_y;
    float    pixels[64];          // 8x8 grid, row-major
};
// Total size: 4+1+1+4+4+4+4+1+1+256 = 280 bytes

#define WS_FLAG_TEMPORAL_ENABLED  0x01
#define WS_FLAG_IDLE_ACTIVE       0x02
#define WS_FLAG_STA_CONNECTED     0x04

#endif
