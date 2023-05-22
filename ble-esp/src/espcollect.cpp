
/*
 * 
 * This detects advertising messages of BLE devices and compares it with stored MAC addresses. 
 * If one matches, it sends an MQTT message to swithc something

   Copyright <2017> <Andreas Spiess>

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
   
   Based on Neil Kolban's example file: https://github.com/nkolban/ESP32_BLE_Arduino
 */

#include "BLEDevice.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "BLEBeacon.h"
//#include <LinkedList.h>


static BLEAddress *pServerAddress;

#define LED 22

BLEScan* pBLEScan;
BLEClient*  pClient;

String myIPAddr;
char mqttTopic[60];
char mqttPayload[320];
/*
// TODO enum
#define CmdIgn  0
#define CmdAdd  1
#define CmdDel  2
#define CmdUpd  3
const char *MqttCmds[4];

void myinit() {
  MqttCmds[CmdIgn] = "ign";
  MqttCmds[CmdAdd] = "add"; 
  MqttCmds[CmdDel] = "del";
  MqttCmds[CmdUpd] = "upd";
}

class BleDevice {
  public:
  char      id[18];
  uint      update;
  uint8_t   cmd;
  int8_t    rssi;
  bool      addComplete;

  BleDevice() {}
  BleDevice(const char *arg_addr, int arg_rssi, int time_in) {
      // copy and convert lower case a-f to A-F
      if (arg_addr) {
        for (int i = 0; i < 17; i++) {
          int c = (int)arg_addr[i];
          if (c >= 'a' && c <= 'f') 
            c = c - 32;
          id[i] = (char)c;
        }
      } else {
        id[0] = '\0';
      }
      id[17]= '\0';               // its a c_str too, not on the heap!
      update = time_in;
      addComplete = false;
      cmd = CmdIgn;
      rssi = 0;
  }

  ~BleDevice() {
  }

 bool equals(BleDevice *dev) {
    return  strncmp(id, dev->id, 17) == 0;
  }

 };

LinkedList<BleDevice*> devList = LinkedList<BleDevice*>();
LinkedList<int> deleteList = LinkedList<int>();
*/
const char* ssid = "CJCNET";
const char* password = "LostAgain2";
const char* mqtt_server = "192.168.1.7";  // change for your own MQTT broker address
#define TOPIC "esp32ble"  // Change for your own topic


WiFiClient espClient;
PubSubClient mqttClient(espClient);
void MQTTcallback(char* topic, byte* payload, unsigned int length);


void mqtt_begin() {
  //btStop();
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int entry = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - entry >= 15000) esp_restart();
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected from");
  Serial.println();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(MQTTcallback);
  myIPAddr = WiFi.localIP().toString();
  Serial.println(myIPAddr);

  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("ESP8266Client", "", "")) {
      Serial.println("connected");
      return;
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void fmt_payload(char *outp, int len, uint8_t *in) {
  int i;
  for (i = 0; i < len; i++) {
    byte nib1 = (in[i] >> 4) & 0x0F;
    byte nib2 = (in[i] >> 0) & 0x0F;
    outp[i*3+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    outp[i*3+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    outp[i*3+2] = ' ';
  }
  outp[i*3]='\0';
}

void fmt_addr(char *outp, BLEAdvertisedDevice ent) {
  pServerAddress = new BLEAddress(ent.getAddress());

  const char * addr = pServerAddress->toString().c_str();
  for (int i = 0; i < 17; i++) {
    int c = (int)addr[i];
    if (c >= 'a' && c <= 'f') 
            c = c - 32;
    outp[i] = (char)c;
  }
  outp[17]= '\0'; 
}

// Format topic and payload strings from a BleDevice and its flags.
// Publis topic and payload.
void mqtt_devTbl(BLEAdvertisedDevice ent) {
    sprintf(mqttTopic, "%s/%s/", TOPIC, myIPAddr.c_str());
    fmt_addr(mqttTopic+strlen(mqttTopic),ent);

    strcpy(mqttPayload, "{\"id\":\"");
    fmt_addr(mqttPayload+strlen(mqttPayload), ent);

    char *p = mqttPayload+strlen(mqttPayload);
    if (ent.haveRSSI()) {
      sprintf(p, "\", \"rssi\":%d", ent.getRSSI());
      p = mqttPayload+strlen(mqttPayload);
    }

    if (ent.haveName()) {
      sprintf(p, ", \"name\":\"%s\"", ent.getName().c_str());
      p = mqttPayload+strlen(mqttPayload);
    }
   
    if (ent.haveAppearance()) {
      sprintf(p, ", \"apr\": 0x%.4x", ent.getAppearance());
      p = mqttPayload+strlen(mqttPayload);
    }
    int paylen = ent.getPayloadLength();
    if (paylen) {
      sprintf(p, ", \"paylen\":%d", paylen);
      p = mqttPayload+strlen(mqttPayload);

      strcpy(p, ", \"payload\":\"");
      p = mqttPayload+strlen(mqttPayload);
      fmt_payload(p, ent.getPayloadLength(), ent.getPayload());
      strcat(p, "\"");
      p = mqttPayload+strlen(mqttPayload);
    } 

    if (ent.haveName()) {
      sprintf(p, ", \"name\":\"%s\"",  ent.getName().c_str());
      p = mqttPayload+strlen(mqttPayload);
     }
    
    if (ent.haveManufacturerData()) {
      sprintf(p, ", \"mfgdata\":\"%s\"", ent.getManufacturerData().c_str());
      p = mqttPayload+strlen(mqttPayload);
    }
    
    if (ent.haveServiceUUID()) {
      // TODO: support multiple service UUIDs? 
      BLEUUID uuid = ent.getServiceUUID();
      sprintf(p, ", \"svcUUID\":\"%s\"", uuid.toString().c_str());
      p = mqttPayload+strlen(mqttPayload); 
    }

    if (ent.haveServiceData()) {
      sprintf(p, ", \"svcData\":\"%s\"", ent.getServiceData().c_str());
      p = mqttPayload+strlen(mqttPayload);
   }
   
    strcat(mqttPayload, "}");
    mqttClient.publish(mqttTopic, mqttPayload);
    mqttClient.loop();
    delay(250);  // .25 second
}

void mqtt_end() {
  mqttClient.disconnect();
  delay(100);
  WiFi.mode(WIFI_OFF);
  btStart();
  Serial.println("\nMqtt Finished");
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}


void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  
  myIPAddr = WiFi.localIP().toString();

  BLEDevice::init("");

  pClient  = BLEDevice::createClient();
  Serial.print(" - Created client for ");
  Serial.println(myIPAddr);
  pBLEScan = BLEDevice::getScan();
  //pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
}

void loop() {

  Serial.println();
  Serial.println("BLE Scan restarted.....");
  delay(1000);

  BLEScanResults scanResults = pBLEScan->start(2);

  int n = scanResults.getCount();

  // try connecting to each address
  for (int i = 0; i < n; i++) {

  }
 
  Serial.print("Sending ");
  Serial.print(n);
  Serial.println(" results to mqtt");
  // begin mqtt - turn off ble, turn on wifi, connect to mqtt
  if (n > 0) {
    mqtt_begin();
    // send each device state/cmd to mqtt
    for (int i = 0; i < n; i++) {
      mqtt_devTbl(scanResults.getDevice(i));
    }
    // end mqtt - disconnect mqtt, turn off wifi, turn on ble
    mqtt_end();
    
    for (int i = 0; i > 10; i++) {
      mqttClient.loop();
     delay(100);
    }
  }
  pBLEScan->clearResults();
  delay(40000); // 40 secs until the next scan
} // End of loop
