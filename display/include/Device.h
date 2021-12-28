#ifndef MY_DEVICE_H
#define MY_DEVICE_H

#define WIFI_ID "CJCNET"
#define WIFI_PASSWORD "LostAgain2"

#define MQTT_SERVER "192.168.1.3"
#define MQTT_PORT 1883
#define MQTT_DEVICE "esp32_test_ranger"

// For Homie compatible topic structure
#define HDEVICE "test_ranger"
#define HNAME "ESP32 in TrumpyBear"        // Display name is OK

//  HC-SR04 ultrasonic sensorPin numbers
#define echoPin 23
#define trigPin 4

// Define display 
#define DISPLAY_U8
//#define LCD160X
//#define TFT_SPI 


#if defined(DISPLAY_U8)
#define DISPLAY_COLUMNS 8
#define DISPLAY_LINES 2
#elif defined(LCD160X) 
#define LCD_I2C_ADDR 0x27
#define DISPLAY_COLUMNS 16
#define DISPLAY_LINES 2
#elif defined(TFT_SPI)
#define TFT_HEIGHT 320
#define TFT_WIDTH 240// rotated(1), font4 x2, appox char counts
#define DISPLAY_COLUMNS 12
#define DISPLAY_LINES 4
#endif
/* 
 * Ranger modes
 * ONCE - signal once when object stop withing given distance (+/-) 
 * CONTINOUS - single whenever object within distance (multiple signals)
 * FREE - signal continously - makes for a very busy MQTT
 */
#define RGR_ONCE  0    
#define RGR_CONTINOUS 1
#define RGR_FREE  2
#define RGR_SNAP 3

// declare extern vars and functions. And Classes.
extern void display_init();
extern void doDisplay(boolean state, String strarg);
extern void displayV2(String jsonstr);
#endif
