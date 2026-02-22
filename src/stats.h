#ifndef STATS_H
#define STATS_H

#include <Arduino.h>

struct FrameStats {
    float   tmin;
    float   tmax;
    float   tmean;
    uint8_t hotspot_x;  // 0-7
    uint8_t hotspot_y;  // 0-7
};

void stats_compute(const float* pixels64, FrameStats& out);

#endif
