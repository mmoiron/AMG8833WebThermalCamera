#ifndef THERMAL_SENSOR_H
#define THERMAL_SENSOR_H

#include <Arduino.h>

bool  sensor_init();
bool  sensor_read(float* pixels64);  // fills 64 floats with °C values
float sensor_thermistor();           // on-chip thermistor reading

#endif
