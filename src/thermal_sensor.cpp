#include "thermal_sensor.h"
#include <Wire.h>
#include <Adafruit_AMG88xx.h>

static Adafruit_AMG88xx amg;
static bool initialized = false;

bool sensor_init() {
    // Wemos D1 mini default I2C: D1=GPIO5 (SCL), D2=GPIO4 (SDA)
    Wire.begin(4, 5);
    Wire.setClock(400000);  // 400kHz I2C

    if (!amg.begin()) {
        Serial.println("[Sensor] AMG8833 not found!");
        return false;
    }

    Serial.println("[Sensor] AMG8833 initialized");
    initialized = true;
    return true;
}

bool sensor_read(float* pixels64) {
    if (!initialized) return false;
    amg.readPixels(pixels64);
    return true;
}

float sensor_thermistor() {
    if (!initialized) return 0.0f;
    return amg.readThermistor();
}
