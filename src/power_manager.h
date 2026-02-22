#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>

void power_init();
void power_client_connected();
void power_client_disconnected();
void power_update();          // call each loop iteration
bool power_is_idle();
int  power_active_fps();      // returns current effective FPS
int  power_client_count();

#endif
