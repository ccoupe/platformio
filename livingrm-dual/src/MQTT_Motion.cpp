// One day, this might be a class. For now, it's just C
/*
 * Handles all of the mqtt interactions for motion sensors
 * The topic stucture is Homie V3 compliant
 */
#include <Arduino.h>
#include <stdlib.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino_JSON.h>
#include "MQTT_Motion.h"

#define USE_ACTIVE_HOLD

// forward declares 
// i.e Private:
void mqtt_reconnect();
void mqtt_send_config();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_publish(char *topic, char *payload);
void mqtt_homie_pub(String topic, String payload, bool retain); 

char *wifi_id;
char *wifi_password;
char *mqtt_server;
int  mqtt_port;
char *mqtt_device;
String hdevice;
String hname;
String hlname;
String hpub;
String hpubst;
String hsub;           // .../active_hold/set
String hsubq;          // .../active_hold
void (*setDelayCBack)(int newval);
int  (*getDelayCBack)();
byte macaddr[6];
char *macAddr;
char *ipAddr;

WiFiClient espClient;
PubSubClient client(espClient);

static void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_id);

  WiFi.begin(wifi_id, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  WiFi.macAddress(macaddr);
  macAddr = (char *)malloc(20);
  itoa(macaddr[5], &macAddr[0], 16);
  macAddr[2] = ':';
  itoa(macaddr[4], &macAddr[3], 16);
  macAddr[5] = ':';
  itoa(macaddr[3], &macAddr[6], 16);
  macAddr[8] = ':';
  itoa(macaddr[2], &macAddr[9], 16);
  macAddr[11] = ':';
  itoa(macaddr[1], &macAddr[12], 16);
  macAddr[13] = ':';
  itoa(macaddr[0], &macAddr[14], 16);
  Serial.print("MAC: ");
  String mcs = String(macAddr);
  mcs.toUpperCase();
  //Serial.print(macAddr);
  Serial.print(mcs);
  Serial.print(" IP address: ");
  String ipaddr = WiFi.localIP().toString();
  Serial.println(ipaddr);
  ipAddr = strdup(ipaddr.c_str());
}

void mqtt_setup(char *wid, char *wpw, char *mqsrv, int mqport, char* mqdev,
    String hdev, char *hnm, int (*gcb)(), void (*scb)(int) ) {

  setDelayCBack = scb;
  getDelayCBack = gcb;
  wifi_id = wid;
  wifi_password = wpw;
  mqtt_server = mqsrv;
  mqtt_port = mqport;
  mqtt_device = mqdev;
  hdevice = hdev;
  hname = String(hnm);
 
  // Create "homie/"HDEVICE"/motionsensor/motion"
  hpub = "homie/" + hdevice + "/motionsensor/motion";

  // Create "homie/"HDEVICE"/motionsensor/motion/status"  
  hpubst = hpub + "/status";

  // Create "homie/"HDEVICE"/motionsensor/active_hold 
  hsubq = "homie/" + hdevice +"/motionsensor/active_hold";
 
   // Create "homie/"HDEVICE"/motionsensor/active_hold/set
  hsub = hsubq + "/set";
   
  // Sanitize hname -> hlname
  hlname = hname;
  hlname.replace(" ", "_");
  hlname.replace("\t", "_");

  /*hlname = strlwr(strdup(hname));
  int i = 0;
  for (i = 0; i < strlen(hlname); i++) {
    if (hlname[i] == ' ' || hlname[i] == '\t')
      hlname[i] = '_';
  }
  */

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback);
  mqtt_reconnect();

  // Create and Publish the infrastructure topics for Homie v3
  String hd = "homie/"+hdevice+"/";
  // "homie/"HDEVICE"/$homie" -> "3.0.1"
  mqtt_homie_pub(hd + "$homie", "3.0.1", true);
  Serial.println(hd);
  //"homie/"HDEVICE"/$name" -> hlname
  mqtt_homie_pub(hd + "$name", hlname, true);

  // "homie/"HDEVICE"/$state -> ready
  mqtt_homie_pub(hd + "$state", "ready", true);
  
  // "homie/"HDEVICE"/$mac" -> macAddr
  String ts = String(macAddr);
  ts.toUpperCase();
  mqtt_homie_pub(hd + "$mac", ts, true);
  
  // "homie/"HDEVICE"/$localip" -> ipAddr
  mqtt_homie_pub(hd + "$localip" , ipAddr, true);
  
  //"homie/"HDEVICE"/$nodes", -> 
  mqtt_homie_pub(hd + "$nodes", "motionsensor", true);

  // end device - begin node motionsensor
  
  // "homie/"HDEVICE"/motionsensor/$name" -> hname (Un sanitized)
  mqtt_homie_pub(hd + "$name", hname, true);
  
  // "homie/"HDEVICE"/motionsensor/$type" ->  "sensor"
  mqtt_homie_pub(hd + "$type", "sensor", true);
  
  // "homie/"HDEVICE"/motionsensor/$properties" -> "motion,active_hold"
#ifdef USE_ACTIVE_HOLD
  mqtt_homie_pub(hd + "$properties", "motion,active_hold", true);
#else
  mqtt_homie_pub(hd + "$properties", "motion", true);
#endif

  // Property 'motion' of 'motionsensor' node
  String ms = "homie/"+hdevice+"/motionsensor/motion/";
  // "homie/"HDEVICE"/motionsensor/motion/$name ->, Unsanitized hname
  mqtt_homie_pub(ms + "$name", hname, true); 

  // "homeie"HDEVICE"/motionsensor/motion/$datatype" -> "boolean"
  mqtt_homie_pub(ms +"$datatype", "boolean", true);

  // "homie/"HDEVICE"/sensor/motion/$settable" -> "false"
  mqtt_homie_pub(ms + "$settable", "false", true);

  // "homie/"HDEVICE"/sensor/motion/$name" -> Unsantized hname
  mqtt_homie_pub(ms + "$name", hname, true);

  // "homie/"HDEVICE"/sensor/motion/$retained" -> "true"
  mqtt_homie_pub(ms + "$retained", "false", true);
  
#ifdef USE_ACTIVE_HOLD
  // Property 'active_hold' of 'motionsensor' node
  String ah = "homie/"+hdevice+"/motionsensor/active_hold/";
  // "homie/"HDEVICE"/motionsensor/active_hold/$name", -> "active_hold" 
  mqtt_homie_pub(ah + "$name", "active_hold", true);  

  // "homie/"HDEVICE"/sensor/active_hold/$datatype" ->  "integer"
  mqtt_homie_pub(ah + "$datatype", "integer", true);

  // "homie/"HDEVICE"/sensor/active_hold/$settable" -> "true"
  mqtt_homie_pub(ah + "$settable", "true", true);

  // "homie/"HDEVICE/motionsensor/active_hold/$format");
  mqtt_homie_pub(ah + "$format", "5:3600", true);
  
  // "homie/"HDEVICE"/motionsensor/active_hold/$retained" -> "true"
  mqtt_homie_pub(ah + "$retained", "false", true);
  
  // set the value of active_hold property in MQTT server, don't retain
 
  mqtt_homie_pub(ah , String(getDelayCBack()), false);
#endif  
}

void mqtt_callback(char* topic, byte* payl, unsigned int length) {
  char payload[length+1];
  // convert byte[] to char[]
  int i;
  for (i = 0; i < length; i++) {
    payload[i] = (char)payl[i];
  }
  payload[i] = '\0';
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(" payload: ");
  Serial.println(payload);
#ifdef USE_ACTIVE_HOLD
  if (! strcmp(hsub.c_str(), topic)) { 
    int d = atoi(payload);
    if (d < 5)
      d = 5;
    else if (d > 3600)
      d = 3600;
     setDelayCBack(d);
   // echo the setting back on the hsubq topic
   char tmp[10];
   itoa(getDelayCBack(), tmp, 10);
   mqtt_homie_pub(hsubq, tmp, false);
  }
#endif
}

void mqtt_reconnect() {
  // Loop until we're reconnected
  int cnt = 1;
  int len = 5;
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_device)) {
      Serial.println("connected");
#ifdef USE_ACTIVE_HOLD 
      // Subscribe to <dev/node/property>/set
      client.subscribe(hsub.c_str());
      Serial.print("listening on topic ");
      Serial.println(hsub);
#endif
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      Serial.print(" try again in ");
      Serial.print(cnt * len);
      Serial.println(" seconds");
      // Wait X seconds before retrying
      delay((cnt * len) * 1000);
      cnt = cnt * 2;
      if (cnt > 32) 
        cnt = 32;
    }
  }
}

void mqtt_homie_pub(String topic, String payload, bool retain) {
#if 0
  Serial.print("pub ");
  Serial.print(topic);
  Serial.print(" ");
  Serial.println(payload);
#endif
  if (! client.publish(topic.c_str(), payload.c_str(), retain)) {
    int rc  = client.state();
    if (rc < 0) {
      Serial.print(rc);
      Serial.println(" dead connection, retrying");
      mqtt_reconnect();
    }
  }
 
}

void mqtt_homie_active(bool state) {
  if (state == true) {
    mqtt_homie_pub(hpub,"true", false);
    mqtt_homie_pub(hpubst,"active", false);
  } else {
    mqtt_homie_pub(hpub,"false", false);
    mqtt_homie_pub(hpubst,"inactive", false);    
  }
}
// Called from sketches loop()
void mqtt_loop() {
  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();

}
