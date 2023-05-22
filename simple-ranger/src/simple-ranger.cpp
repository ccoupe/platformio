/*
 * Garage-Ranger is an HR-504 or JSN-SR04T ultrasonic sensor to meausre distance in cm.
 * It communicates with MQTT with a psuedo Homie structure,
 * that is not 'homie' - it only looks like it.
 * There is a matching Hubitat driver.
 * 
 * There are constants in device.h you need to look at.
 * 
 * The distance in cm is published to 'homie/audi_stall/ranger/distance'
 * Each 4 hours (14400 secs  we publish anyway.
 * Listen on  homie/audi_stall/ranger/distance/set for payloads:
 *   {"period": secs}
 *   Note this is Legacy. 
 * Listen on homie/audi_stall/control/cmd/set for payloads:
 *   {"cmd: "update", "url": "<string>"}
 *   {"period": secs}
 *   {"upper": value}
 *   {"lower": value}
* 
 * These only look like json!
 * There are two modes depending on the report average setting.
 * When true, the sensor value is sampled every 'sample_seconds' and averaged (default is 5
 * samples) The average is compared to the last value published to mqtt. If it is within a 
 * 'sensivity', say +/- 2 cm off the last value then nothing is publish. Essentially no change.
 * If it is within the upper and lower limits it is published to mqtt. 
 * This mode is the default. It smooths the samples and their reporting so temporary sensor
 * malfunctioning doesn't trigger publishing events as often. The ultrasonic sensors can be buggy.
 * 
 * When report average is false then
 * The sensor is read every 'period' seconds and compared to a moving average of the last 5
 * readings. If the sensor reading is within 1 cm of the average then if the value is between
 * the lower and higher limits then it is published to mqtt. The upper and lower limits
 * keep 'out-of-bounds' values from being reported. 
 * The average is always updated with the new sensor value.
 * 
 * If you think the two modes are similar then test them on your application.
 * 
*/
 

#include <Arduino.h>
#include <Wire.h>
#include "Device.h"
#include "MQTT_Ranger.h"
#include "Preferences.h"
#include "Update.h"

Preferences eeprom;
#define DelayToOff 2
unsigned int delaySeconds = DelayToOff;   // this variable can be set dynamically
unsigned every = DelayToOff;
unsigned fourHours = 60*60*4;
int last_d;

uint32_t target_d = 0;
boolean rgr_running = false;
uint32_t next_pub = 25;
uint32_t next_tick = 0;
int sample_cnt = 0;
int sample_avg = 0;
int idx = 0;
int last_value = 0;

// Settables: via eeprom and mqtt
int nSamples;
int *samples;               // used as an array
int sensitivity;
int limit_low;       
int limit_high;
boolean report_average;

// forward declare
int get_distance();

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;

void IRAM_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
}



void delay_set(int val) {
  // don't update flash unless value changes
  if (val != delaySeconds) {
    delaySeconds = val;
#ifdef USE_FLASH
    eeprom.begin("ranger",false);
    delaySeconds = eeprom.putUInt("DelayToOff", (uint) val);
    eeprom.end();
#endif  
    Serial.print("set delaySeconds to ");
    Serial.println(delaySeconds);
  } else {
    Serial.println("skipping flash writing");
  }
}

// Mqtt callback - parse json string the hacky way
void setRanger(String jstr) {
  if (jstr.startsWith("{\"period\"")) {
    int c = jstr.indexOf(":");
    jstr.remove(0,c+1);
    jstr.trim();
    int s = jstr.toInt();
    if (s > 0) {
      delay_set(s);
    }
  }
}

int get_distance() {
  float duration, distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(20);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = (duration*.0343)/2;
  Serial.print("Distance: ");
  Serial.println(distance);
  mqtt_loop();
  delay(200);
  return (int)distance;
}

void check_measurement(int d, boolean publish) {
  if (! report_average) {
    // is d 1cm +/- from the average? 
    if (d < last_value - sensitivity || d > last_value + sensitivity) {
      // It is not 'close' to average. Is it within reporting limits?
      if (d >= limit_low && d <= limit_high) {
        publish = true;
      }
    }
  }
  samples[idx++] = d;
  if (idx >= nSamples) idx = 0;
  int sample_sum = 0;
  for (int i = 0; i < nSamples; i++) {
    sample_sum += samples[i];
  }
  sample_avg = sample_sum / nSamples;
  if (report_average) {
    // is avg +/- sensitivity different enouch from last published value?
    if (sample_avg < last_value - sensitivity || sample_avg > last_value + sensitivity) {
      // is it within the limits
      if (sample_avg >= limit_low && sample_avg <= limit_high) {
        publish = true;
      }
    }
  }
  if (sample_cnt < nSamples) {
    sample_cnt++;
    return;  // don't publish until samples array is filled 
  } 
  Serial.println("d: "+String(d)+ " idx: "+String(idx)+" avg: "+String(sample_avg)+
    " sum: "+String(sample_sum));
  if (publish) {
    if (report_average) d = sample_avg;
    last_value = d;
     mqtt_ranger_set_dist(d);
  }
}


// Mqtt 'control' callback - parse json string the hacky way
// calls updateFrom() in updateOTA.cpp
void handleControl(String jstr) {
  int pos;
  String host;
  String portStr;
  String path;
  int port = 80;
  pos = jstr.indexOf("{\"cmd\":");
  if (pos >= 0) {
    pos = jstr.indexOf("\"url\":");
    if (pos >= 0) {
      String urls = jstr.substring(pos+7);
      int len = urls.length();
      String url = urls.substring(1, len-2);
      Serial.println("Have url " + url );
      if (url.startsWith("http://")) {
        pos = url.indexOf("/",9);
        host = url.substring(7,pos);
        path = url.substring(pos);
      } else {
        pos = url.indexOf("/");
        host = url.substring(0, pos);
        path = url.substring(host.length());
      }
      // Pick port number out of host string
      pos = host.indexOf(":");
      if (pos > 0) {
        portStr = host.substring(pos+1);
        //Serial.println("portstr: "+ portStr);
        host = host.substring(0,pos);
        port = portStr.toInt();
      }
      updateFrom(host, port, path);
    }
  }
}

void setup() {
  Serial.begin(115200);
  //Serial.println("Starting");
#ifdef WRITE_FLASH
  eeprom.begin("ranger",false);
  eeprom.clear();
  eeprom.putUInt("DelayToOff", DelayToOff);
  eeprom.putString("WIFI_ID",WIFI_ID);
  eeprom.putString("WIFI_PASSWORD",WIFI_PASSWORD);
  eeprom.putString("MQTT_SERVER",MQTT_SERVER);
  eeprom.putInt("MQTT_PORT",MQTT_PORT);
  eeprom.putString("MQTT_DEVICE",MQTT_DEVICE);
  eeprom.putString("HDEVICE",HDEVICE);
  eeprom.putString("HNAME", HNAME);
  eeprom.putInt("NSAMPLES",NSAMPLES);
  eeprom.putInt("LIMIT_LOW",LIMIT_LOW);
  eeprom.putInt("LIMIT_HIGH",LIMIT_HIGH);
  eeprom.putBool("REPORT_AVG",REPORT_AVG);
  eeprom.putInt("SENSITIVITY",SENSITIVITY);
  Serial.println("Device parameters written to EEPROM");
  return;
#else

  eeprom.begin("ranger",true);
  delaySeconds = eeprom.getUInt("DelayToOff", DelayToOff);
  Serial.println("DelayToOff: " + String(delaySeconds));

  wifi_id = eeprom.getString("WIFI_ID", WIFI_ID);
  Serial.println("WIFI_ID: " + wifi_id);
 
  wifi_password = eeprom.getString("WIFI_PASSWORD", WIFI_PASSWORD);
  Serial.println("WIFI_PASSWORD: "+wifi_password);

  mqtt_server = eeprom.getString("MQTT_SERVER", MQTT_SERVER);
  Serial.println("MQTT_SERVER: "+mqtt_server);

  mqtt_port = eeprom.getInt("MQTT_PORT", MQTT_PORT);
  Serial.println("MQTT_PORT: "+String(mqtt_port));

  mqtt_device = eeprom.getString("MQTT_DEVICE",MQTT_DEVICE);
  Serial.println("MQTT_DEVICE: "+mqtt_device);

  hdevice = eeprom.getString("HDEVICE",HDEVICE);
  Serial.println("HDEVICE: "+hdevice);

  hname = eeprom.getString("HNAME", HNAME);
  Serial.println("HNAME: "+hname);

  nSamples = eeprom.getInt("NSAMPLES",NSAMPLES);
  samples = (int *)malloc(sizeof(int) * nSamples);
  Serial.println("NSAMPLES: "+String(nSamples));

  sensitivity = eeprom.getInt("SENSITIVITY",SENSITIVITY);
  Serial.println("SENSITIVITY: "+String(sensitivity));

  limit_low = eeprom.getInt("LIMIT_LOW",LIMIT_LOW);
  Serial.println("LIMIT_LOW: "+String(limit_low));

  limit_high = eeprom.getInt("LIMIT_HIGH",LIMIT_HIGH);
  Serial.println("LIMIT_HIGH: "+String(limit_high));

  report_average = eeprom.getBool("REPORT_AVG",REPORT_AVG);
  Serial.println("REPORT_AVG: "+String(report_average));
  eeprom.end();
#endif  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  mqtt_setup(setRanger, handleControl); 

  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 1000000, true);

  // Start an alarm
  timerAlarmEnable(timer);

  // Does it takes a while to warm up to accurate readings?
  //int d = get_distance();
  //delaySeconds(60);

  /*
  // Need to prime the pump with the first measurement?
  int d = get_distance();
  check_measurement(d, false);
  mqtt_ranger_set_dist(d);
  */
}

void loop() {
#ifdef WRITE_FLASH
  return;
#endif
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {
    fourHours--;
    if (fourHours <= 0) {
      fourHours = 60*60*4;
      int d = get_distance();
      check_measurement(d, true);
    } else {
      every--;
      if (every <= 0) {
        // time to take a measurement
        int d = get_distance();
        check_measurement(d, false);
        // reset every timer
        every = delaySeconds;
      }
    }
  }
  mqtt_loop();

   
}
