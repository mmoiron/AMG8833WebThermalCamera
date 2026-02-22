#include "stats.h"

void stats_compute(const float* pixels64, FrameStats& out) {
    float mn  = pixels64[0];
    float mx  = pixels64[0];
    float sum = 0.0f;
    int   max_idx = 0;

    for (int i = 0; i < 64; i++) {
        float v = pixels64[i];
        sum += v;
        if (v < mn) mn = v;
        if (v > mx) {
            mx = v;
            max_idx = i;
        }
    }

    out.tmin = mn;
    out.tmax = mx;
    out.tmean = sum / 64.0f;
    out.hotspot_x = max_idx % 8;
    out.hotspot_y = max_idx / 8;
}
