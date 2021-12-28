#ifndef MQTT_RANGER_H
#define MQTT_RANGER_H
#include "Device.h"

// Functions in MQTT_Ranger.cpp 
// Public functions:
extern void mqtt_setup( void (*ccb)(String jstr), void (*ctb)(String) );
extern void mqtt_loop();
extern void mqtt_ranger_set_dist(int);

extern String wifi_id;
extern String wifi_password;
extern String mqtt_server;
extern int  mqtt_port;
extern String mqtt_device;
extern String hdevice;
extern String hname;
#endif
