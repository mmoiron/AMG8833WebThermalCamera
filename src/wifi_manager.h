#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void wifi_init();
void wifi_update();           // call periodically to monitor STA
bool wifi_sta_connected();
String wifi_sta_ip();

#endif
