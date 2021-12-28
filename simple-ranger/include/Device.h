#ifndef MY_DEVICE_H
#define MY_DEVICE_H

#define WIFI_ID "CJCNET"
#define WIFI_PASSWORD "LostAgain2"

#define MQTT_SERVER "192.168.1.3"
#define MQTT_PORT 1883
#define MQTT_DEVICE "esp_garage_ranger"

// For Homie compatible topic structure - These are stored in eeprom if WRITE_FLASH
#define HDEVICE "audi_stall"
#define HNAME "ESP32 Ranger"        // Display name - not used but needed
#define NSAMPLES 5                  // number of samples in moving average
#define SAMPLE_SEC 1                // sample every <n> seconds
#define LIMIT_LOW   20              // don't report if lower than, in centimeters
#define LIMIT_HIGH  450             // don't report if higher than, in centimeters
#define SENSITIVITY 2               // +/- in centimeters
#define REPORT_AVG  true           // report sample or average.

//  HC-SR04 ultrasonic sensorPin numbers
#define echoPin 23
#define trigPin 4
//#define WRITE_FLASH     // uncomment for one time use

extern void updateFrom(String host, int port, String path);
/* 
 * Ranger modes
 * ONCE - signal once when object stop withing given distance (+/-) 
 * CONTINOUS - single whenever object within distance (multiple signals)
 * FREE - signal continously - makes for a very busy MQTT

#define RGR_ONCE  0    
#define RGR_CONTINOUS 1
#define RGR_FREE  2
#define RGR_SNAP 3
 */
#endif
