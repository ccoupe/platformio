/*
 * Mqtt-Ranger2 is an ultrasonic sensor to meausre distance in cm.
 * and a Waveshare 2" 320x240 OLED LCD. 
 * 
 * 
 * listening on topic homie/test_ranger/autoranger/mode/set
 * listening on topic homie/test_ranger/autoranger/distance/set
 * listening on topic homie/test_ranger/display/text/set
 * listening on topic homie/test_ranger/display/mode/set
 * 
 * Font #6 is rouchly 16 chars x 4 lines in 320px width 240 height
*/
 
#include <Arduino.h> 
#include <Wire.h>
#include "Device.h"
#include "MQTT_Ranger.h"


#define ACTIVE 1
#define INACTIVE 0
int state = INACTIVE;

// Forward declares (and externs?)
void myterm(int x, int y, String ln);
int get_distance();
boolean dist_display(int d, int target_d);
//void doDisplay(boolean state, char *argstr);
void doDisplay(boolean state, String argstr);

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


int32_t target_d = 0;
boolean rgr_running = false;
uint32_t next_pub = 1;
uint32_t next_tick = 0;

// Want to publish to mqtt every 5 seconds or (so)
// beware of the 'cleverness' in tick counting and what could go 
// wrong. Hint: there are many ways to fail

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

// Called from mqtt with distance to measure towards
// anything over 400cm/13ft is too far to care about
// Beware - this can/will be called re-entrantly to cancel. Should protect rgr_running var
// and the ISR stuff but that's a lot work and testing for those failures is difficult.
void doRanger(int rgrmode, int dist) {
  //int next_pub = 0;
  if (dist == 0) {   
    rgr_running = false;
    Serial.println("set to zero, stopping?");
    return;
  }
  Serial.println("Begin ranger");
  if (rgrmode == RGR_SNAP) {
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
  } else {
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
          if (rgrmode == RGR_CONTINOUS)
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
}


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
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = (duration*.0343)/2;
  Serial.print("Distance: ");
  Serial.println(distance);
  mqtt_loop();
  delay(200);
  return (int)distance;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  display_init();

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
}

void loop() {
  mqtt_loop();
  //delay(200);
  //doRanger(RGR_CONTINOUS, 75);  //test
   
}

