#ifndef MQTT_MOTION_H
#define MQTT_MOTION_H
#include "Device.h"
// Functions in MQTT_Ranger.cpp 
// Public functions:
extern char *macAddr;
extern char *ipAddr;
extern void mqtt_setup(const char *wusr, const char *wpw,
    const char *mqsrv, int mqport, const char* mqdev,
    const char *hdev, const char *hnm, 
    void (*ccb)(int, int),
    void (*dcb)(boolean, String));
extern void mqtt_loop();
extern void mqtt_ranger_set_dist(int);

#endif
