#include "temporal_filter.h"

static float prev[64];
static bool  primed = false;

void filter_init() {
    filter_reset();
}

void filter_reset() {
    memset(prev, 0, sizeof(prev));
    primed = false;
}

void filter_apply(float* pixels64, float alpha) {
    if (!primed) {
        // First frame: seed the filter
        memcpy(prev, pixels64, sizeof(prev));
        primed = true;
        return;
    }

    // IIR: y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
    float one_minus_alpha = 1.0f - alpha;
    for (int i = 0; i < 64; i++) {
        prev[i] = alpha * pixels64[i] + one_minus_alpha * prev[i];
        pixels64[i] = prev[i];
    }
}
