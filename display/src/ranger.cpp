/*
 * Mqtt-Ranger2 is an ultrasonic sensor to meausre distance in cm.
 * and a Waveshare 2" 320x240 OLED LCD. 
 * 
 * V1:
 * listening on topic homie/test_ranger/autoranger/mode/set
 * listening on topic homie/test_ranger/autoranger/distance/set
 * listening on topic homie/test_ranger/display/text/set
 * listening on topic homie/test_ranger/display/mode/set
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
 * V2: see Display.md
 *
 *    Font #6 is roughly 12 chars x 4 lines for 320px width 240 height
*/
 
#include <Arduino.h> 
#include <Wire.h>
#include "Device.h"
#include "MQTT_Ranger.h"


#define ACTIVE 1
#define INACTIVE 0
int state = INACTIVE;

#ifdef RANGER
// Forward declares (and externs?)
int get_distance();
boolean dist_display(int d, int target_d);
// Settable vars
int rgr_mode = RANGER_MODE;
int sample_rate = SAMPLE;
int every_rate = EVERY;
int every = EVERY;          // counts down.  When zero, report, reset to EVERY
int averageNum = AVERAGE;
unsigned keep_alive_rate = KEEP_ALIVE;
unsigned keep_alive = KEEP_ALIVE;
int nSamples = SAMPLE;
int slopL = SLOPL;
int slopH = SLOPH;
int boundsL = BOUNDS_LOW;
int boundsH = BOUNDS_HIGH;
int range_low = RANGE_LOW;
int range_high = RANGE_HIGH;
#ifdef DISPLAYG
const char *guide_low = GUIDE_LOW;
const char *guide_high = GUIDE_HIGH;
const char* guide_target = GUIDE_TARGET;
#endif
// statics because scoping needs them
int *samples;               // used as an array
uint32_t target_d = 0;
boolean rgr_running = false;
uint32_t next_pub = 25;
uint32_t next_tick = 0;
int sample_cnt = 0;
int sample_avg = 0;
int idx = 0;
int last_value = 0;
boolean report_average = true;
#endif

long lastMsg = 0;
char msg[50];
int value = 0;

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

// Want to publish to mqtt every 5 seconds or (so)
// beware of the 'cleverness' in tick counting and what could go 
// wrong. Hint: there are many ways to fail

#ifdef RANGER
void wait_for_pub(int d) {
  if (next_tick == isrCounter) {
    return;    
  }
  next_pub -= 1;
  next_tick = isrCounter;
  if (next_pub <= 0) {
    mqtt_ranger_set_dist(d);
    next_pub = 5;
  }
}
#endif

// Called from mqtt with distance to measure towards (guide mode)
// Modes Free, Once, Guide
// Every sample seconds - get sensor value.
//   Average it if not out-of-bounds pass value to
//     if everyNnt < 0, set everyCnt = EVERY, 
// Assuming the distance is not out of bounds and in guide post range and...
//
// Beware - this can/will be called re-entrantly to cancel. Should protect rgr_running var
// and the ISR stuff but that's a lot work and testing for those failures is difficult.
void doRanger(int rgrmode, int dist) {
#ifdef RANGER
  if (dist == 0) {   
    rgr_running = false;
    Serial.println("set to zero, stopping?");
    return;
  }
  Serial.println("Begin ranger");
  if (rgrmode == RGR_ONCE) {
    int d = get_distance();
    mqtt_ranger_set_dist(d);
  } else if (rgrmode == RGR_FREE) {
    int d;
    rgr_running = true;
    while (rgr_running == true) {
      char tmp[8];
      d = get_distance();
      itoa(d, tmp, 10);
      doDisplay(true, tmp);
      mqtt_ranger_set_dist(d);
    }
  } else if (rgrmode == RGR_TEST) {
    int d;
    while ((d = get_distance())) {
      char tmp[8];
      d = get_distance();
      itoa(d, tmp, 10);
      doDisplay(true, tmp);
      mqtt_ranger_set_dist(d);
      delay(1000);
        
    }
  } else if (rgrmode == RGR_GUIDE) {
    target_d = dist;
    rgr_running = true;
    // don't run for more than a few minutes
    uint32_t tend = isrCounter + (1 * 60);
    int d = get_distance();
    while (isrCounter < tend) {
      // check for canceled.
      if (rgr_running == false)
        break;
      if (d > 2 && d < 500) {
        if (dist_display(d, target_d) == true) {
          if (rgrmode == RGR_GUIDE)
            wait_for_pub(d);
          else {
            mqtt_ranger_set_dist(d);
            rgr_running = false;
            break;
          }
        }
      } else {
        doDisplay(true, "Over Here!");
      }
      d = get_distance();
    }
    Serial.println("end DoRanger");
    if (isrCounter >= tend) {
      doDisplay(true,"Timed Out");
      mqtt_ranger_set_dist(0);
    }
  }
#endif
}

#ifdef RANGER
#define ADJMIN 10
#define ADJMAX 10


boolean dist_display(int d, int target_d) {
  if (d > 400) {
    Serial.println("out of range");
    doDisplay(true, "Out of Range");
    return false;
  }
  if (d >= (target_d - ADJMIN) && d <= (target_d + ADJMAX)) {
    Serial.println("stop there");
    doDisplay(true, "Stop There");
    return true;
  }
  if (d < (target_d + ADJMAX)) {
    Serial.println("move back");
    doDisplay(true, "Move Back");
    return false;
  }
  if ( d > (target_d - ADJMIN)) {
    Serial.println("move forward");
    doDisplay(true, "Move Forward");
    return false;
  }
  return false; // should not get here!
}


int get_distance() {
  float duration, distance;
  digitalWrite(TRIGGERPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGERPIN, HIGH);
  delayMicroseconds(40);
  digitalWrite(TRIGGERPIN, LOW);

  duration = pulseIn(ECHOPIN, HIGH);
  distance = (duration*.0343)/2;
  //Serial.print("Distance: ");
  //Serial.println(distance);
  mqtt_loop();
  delay(200);
  return (int)distance;
}

void rangerOn() {

}

void rangerOff() {

}

int check_measurement(int d, boolean publish) {
   if (! report_average) {
    // is d 1cm +/- from the average? 
    if (d < last_value - slopL || d > last_value + slopH) {
      // It is not 'close' to average. Is it within reporting limits?
      if (d >= boundsL && d <= boundsH) {
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
    // is avg +/- sensitivity different enough from last published value?
    if (sample_avg < last_value - slopL || sample_avg > last_value + slopH) {
      // is it within the limits
      if (sample_avg >= boundsL && sample_avg <= boundsH) {
        publish = true;
      }
    }
  }
  if (sample_cnt < nSamples) {
    sample_cnt++;
    return -1;  // don't publish until samples array is filled 
  } 
  Serial.println("d: "+String(d)+ " idx: "+String(idx)+" avg: "+String(sample_avg)+
    " sum: "+String(sample_sum));
  if (publish) {
    if (report_average) d = sample_avg;
    last_value = d;
    return d;
  } else {
    return -1;
  }
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
#ifdef RANGER
  pinMode(TRIGGERPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
#endif
#ifdef DISPLAYG
  display_init();
#endif
  mqtt_setup(WIFI_ID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_DEVICE,
      HDEVICE, HNAME, doRanger, doDisplay);

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
#ifdef RANGER
  samples = (int *)malloc(nSamples * sizeof(int));
#endif
}

void loop() {
#ifdef RANGER
  int newd;
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {
#ifdef DISPLAYG
    displayLoop();
#endif
    keep_alive--;
    if (keep_alive <= 0) {
      keep_alive = keep_alive_rate;
      Serial.println("keep-alive");
      int d = get_distance();
      newd = check_measurement(d, true);
      if (newd > 0) {
        mqtt_ranger_set_dist(d);
      }
    } else {
      every--;
      if (every <= 0) {
        // time to take a measurement
        int d = get_distance();
        if (rgr_mode == RGR_FREE) {
          newd = check_measurement(d, false);
          if (newd > 0 ) {
#ifdef DISPLAYG
            doDisplay(true, String(d)+" "+String(sample_avg));
            //doDisplay(true, newd);
#endif
            mqtt_ranger_set_dist(newd);
          }
        } else if (rgr_mode == RGR_GUIDE ) {
          if (d <= range_low) {
            Serial.println(String(d)+" Move backward");
#ifdef DISPLAYG
            doDisplay(true, guide_low);
#endif
            mqtt_ranger_show_place(">");
          } else if (d >= range_high) {
            Serial.println(String(d)+" Move forward");
#ifdef DISPLAYG
            doDisplay(true, guide_high);
#endif
            mqtt_ranger_show_place("<");
          } else {
            Serial.println(String(d)+" Stay still");
#ifdef DISPLAYG
            doDisplay(true, guide_target);
#endif
            mqtt_ranger_show_place("=");
          }
        }
        // reset every timer
        every = every_rate;
      }
    }
  }
#else   // Must be DISPLAYG without RANGER
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {
    displayLoop();
  }
#endif
  mqtt_loop();
  
}

