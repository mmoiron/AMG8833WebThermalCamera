#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

struct SystemConfig {
    int   normal_fps;
    int   idle_fps;
    int   idle_timeout_sec;
    bool  temporal_enabled;
    float alpha;
    float calibration_offset;
    bool  sta_enabled;
    char  sta_ssid[33];
    char  sta_password[65];
};

// Default values
void     config_init();
void     config_load();
void     config_save();
SystemConfig& config_get();

// Validation helpers
int   config_clamp_fps(int fps);
float config_clamp_alpha(float a);
float config_clamp_offset(float o);

#endif
