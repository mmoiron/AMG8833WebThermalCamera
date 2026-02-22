#ifndef TEMPORAL_FILTER_H
#define TEMPORAL_FILTER_H

#include <Arduino.h>

void filter_init();
void filter_reset();
void filter_apply(float* pixels64, float alpha);

#endif
