#ifndef APP_WEBSERVER_H
#define APP_WEBSERVER_H

#include <Arduino.h>

void webserver_init();
void webserver_loop();  // call from main loop — handles deferred WiFi restart
void webserver_broadcast(const uint8_t* data, size_t len);

#endif
