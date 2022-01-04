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
//#define DISPLAY_U8
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
#ifdef RANGER
#ifndef RANGE_LOW
#define RANGE_LOW 10
#endif
#ifndef RANGE_HIGH
#define RANGE_HIGH 400
#endif
#ifndef EVERY
#define EVERY 60
#endif
#ifndef KEEP_ALIVE
#define KEEP_ALIVE 14400
#endif
#ifndef SAMPLE
#define SAMPLE 1
#endif
#ifndef SLOPH
#define SLOPH 1
#endif
#ifndef SLOPL
#define SLOPL 1
#endif
#ifndef AVERAGE
#define AVERAGE 1
#endif 
#ifndef REPORT_ALL
#define REPORT_ALL false
#endif 
#ifndef BOUNDS_LOW
#define BOUNDS_LOW 3
#endif
#ifndef BOUNDS_HIGH
#define BOUNDS_HIGH 580
#endif 
#ifdef DISPLAYG              // DISPLAYG AND RANGER defined
#ifndef GUIDE_LOW
#define GUIDE_LOW "Move Forward"
#endif
#ifndef GUIDE_HIGH
#define GUIDE_HIGH "Move Backward"
#endif
#ifndef GUIDE_TARGET
#define GUIDE_TARGET "Stop There"
#endif
#endif // Display within ranger (autoranger)
#endif // Ranger
#ifdef DISPLAYG
#ifndef SCREEN_COLOR
#define SCREEN_COLOR "BLACK"
#endif
#ifndef BLANK_AFTER  
#define BLANK_AFTER 300 
#endif
#ifndef SCROLL
#define SCROLL false
#endif
#endif
/* 
 * Ranger modes
 * ONCE - signal once when object stop withing given distance (+/-) 
 * CONTINOUS - single whenever object within distance (multiple signals)
 * FREE - signal continously - makes for a very busy MQTT
 */
#define RGR_ONCE  0    
#define RGR_GUIDE 1
#define RGR_FREE  2

// declare extern vars and functions. And Classes.
extern void display_init();
extern void doDisplay(boolean state, String strarg);
extern void displayV2(String jsonstr);
#endif
