#ifndef MQTT_MOTION_H
#define MQTT_MOTION_H

// Functions in mqtt_lib.cpp 
// Public functions:
extern void mqtt_setup(const char *wusr, const char *wpw,
    const char *mqsrv, int mqport, const char* mqdev,
    String hdev, const char *hnm, 
    int (*gcb)(),
    void (*scb)(int));
extern void mqtt_loop();
extern void mqtt_homie_active(bool state);


#endif
