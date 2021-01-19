// One day, this might be a class. For now, it's just C
/*
 * Handles all of the mqtt interactions for ranger (there are many)
 * The topic stucture is Homie V3 semi-compliant
 */
#include <Arduino.h>
#include <stdlib.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino_JSON.h>
#include "MQTT_Ranger.h"

// forward declares 
// i.e Private:
void mqtt_reconnect();
void mqtt_send_config();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_publish(char *topic, char *payload);
void mqtt_homie_pub(String topic, String payload, bool retain); 

const char *wifi_id;
const char *wifi_password;
const char *mqtt_server;
int  mqtt_port;
const char *mqtt_device;
String hdevice;
String hname;
String hlname;
String hpub;
String hsub;
String hsubq;
String hpubst;             // ..ranger/$status <- publish to 
String hpubDistance;       // ..ranger/distance <- publish to
String hsubDistance;       // ..ranger/control/set -> subcribe to
//char *hsubMode;            // ..ranger/mode/set -> subscribe to
//char *hsubDspCmd;         // ../display/cmd/set  -> subscribe to
//char *hsubDspTxt;         // ../display/text/set -> subscribe to
void (*rgrCBack)(String); // does autorange to near newval
//void (*dspCBack)(boolean st, char *str);       

//int rgr_mode = RGR_ONCE;

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
  Serial.print(macAddr);
  Serial.print(" IP address: ");
  String ipaddr = WiFi.localIP().toString();
  Serial.println(ipaddr);
  ipAddr = strdup(ipaddr.c_str());
}

void mqtt_setup(const char *wid, const char *wpw, const char *mqsrv, int mqport, 
    const char* mqdev, const char *hdev, const char *hnm, void (*ccb)(String) ) {

  rgrCBack = ccb; 
  wifi_id = wid;
  wifi_password = wpw;
  mqtt_server = mqsrv;
  mqtt_port = mqport;
  mqtt_device = mqdev;
  hdevice = String(hdev);
  hname = String(hnm);

  // Create "homie/"HDEVICE"/ranger"
  hpub = "homie/" + hdevice + "/ranger";
  //hpub = (char *)malloc(6 + strlen(hdevice) + 22); // wastes a byte or two.
  //strcpy(hpub, "homie/");
  //strcat(hpub, hdevice);
  //strcat(hpub, "/ranger");
  
  // Create "homie/"HDEVICE"/ranger/status"
  hpubst = "homie/" + hdevice + "/ranger/status";
  //hpubst = (char *)malloc(strlen(hpub) + 8);
  //strcpy(hpubst, hpub);
  //strcat(hpubst, "/status");

  /*
  // Create "homie/"HDEVICE"/ranger/mode
  hsubq = (char *)malloc(6+strlen(hdevice)+30); // wastes a byte or two.
  strcpy(hsubq, "homie/");
  strcat(hsubq, hdevice);
  strcat(hsubq, "/ranger/mode");

  // Create "homie/"HDEVICE"/ranger/mode/set
  hsub = (char *)malloc(strlen(hsubq) + 6);
  strcpy(hsub, hsubq);
  strcat(hsub, "/set");
  hsubMode = strdup(hsub);
  */
  // Create "homie/"HDEVICE"/ranger/distance for publishing
  hpubDistance = "homie/" + hdevice + "/ranger/distance";
  //hpubDistance = (char *)malloc(strlen(hpub) + 12);
  //strcpy(hpubDistance, hpub);
  //strcat(hpubDistance, "/distance");

  // Create "homie/"HDEVICE"/ranger/distance/set" topic for subscribe 
  hsubDistance = "homie/" + hdevice + "/ranger/distance/set";
  //hsubDistance = (char *)malloc(strlen(hpubDistance) + 5);
  //strcpy(hsubDistance, hpubDistance);
  //strcat(hsubDistance, "/set");

  /*
  // create the subscribe topics for "homie/"HDEVICE"/display/cmd/set
  hsubDspCmd = (char *)malloc(6+strlen(hdevice)+20); // wastes a byte or two.
  strcpy(hsubDspCmd, "homie/");
  strcat(hsubDspCmd, hdevice);
  strcat(hsubDspCmd, "/display/mode/set");

  // create the subscribe topics for "homie/"HDEVICE"/display/text/set
  hsubDspTxt = (char *)malloc(6+strlen(hdevice)+20); // wastes a byte or two.
  strcpy(hsubDspTxt, "homie/");
  strcat(hsubDspTxt, hdevice);
  strcat(hsubDspTxt, "/display/text/set");
  */

  // Sanitize hname -> hlname
  hlname = hname;
  hlname.toLowerCase();
  hlname.replace(" ", "_");
  hlname.replace("\t", "_");
  /*
  hlname = strlwr(strdup(hname));
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
  /*
  char tmp[100] = {};
  char hpfix[30] = {};
  strcpy(hpfix,"homie/");
  strcpy(hpfix+6, hdevice);
  i = strlen(hpfix);
  char *p = tmp+i;
  strcpy(tmp,hpfix);
  */

  // "homie/"HDEVICE"/$homie" -> "3.0.1"
  String tmp = "homie/" + hdevice + "/$homie";
  //strcpy(p, "/$homie");
  mqtt_homie_pub(tmp, "3.0.1", true);
  
  //"homie/"HDEVICE"/$name" -> hlname
  tmp = "homie/" + hdevice + "/$name";
  mqtt_homie_pub(tmp, hlname, true);

  // "homie/"HDEVICE"/$state -> ready
  tmp = "homie/" + hdevice +  "/$state";
  mqtt_homie_pub(tmp, "ready", true);
  
  // "homie/"HDEVICE"/$mac" -> macAddr
  tmp = "homie/" + hdevice + "/$mac";
  mqtt_homie_pub(tmp, String(macAddr), true);
  
  // "homie/"HDEVICE"/$localip" -> ipAddr
  tmp = "homie/" + hdevice + "/$localip";
  mqtt_homie_pub(tmp, ipAddr, true);
  
  //"homie/"HDEVICE"/$nodes", -> 
  tmp = "homie/" + hdevice + "/$nodes";
  mqtt_homie_pub(tmp, "ranger", true);

  // begin pr - display
  
  // "homie/"HDEVICE"/ranger/$name" -> hname (Un sanitized)
  tmp = "homie/" + hdevice + "/ranger/$name";
  mqtt_homie_pub(tmp, hname, true);
  
  // "homie/"HDEVICE"/ranger/$type" ->  "sensor"
  tmp = "homie/" + hdevice +  "/ranger/$type";
  mqtt_homie_pub(tmp, "sensor", true);
  
  // "homie/"HDEVICE"/ranger/$properties" -> "distance"
  tmp = "homie/" + hdevice + "/ranger/$properties";
  mqtt_homie_pub(tmp, "distance", true);

  // Property 'distance' of 'ranger' node
  // "homie/"HDEVICE"/ranger/distance/$name ->, Unsanitized hname
  tmp = "homie/" + hdevice + "/ranger/distance/$name";
  mqtt_homie_pub(tmp, hname, true); 

  // "homie"HDEVICE"/ranger/distance/$datatype" -> "integer"
  tmp = "homie/" + hdevice + "/ranger/distance/$datatype";
  mqtt_homie_pub(tmp, "integer", true);

  // "homie"HDEVICE"/ranger/distance/$format" -> "65536"
  // That's 18.244 hours 
  tmp = "homie/" + hdevice + "/ranger/distance/$format";
  mqtt_homie_pub(tmp, "1:65536", true);
  
  // "homie/"HDEVICE"/ranger/cmd/$settable" -> "true"
  tmp = "homie/" + hdevice + "/ranger/distance/$settable";
  mqtt_homie_pub(tmp, "true", true);
  
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

  if (! strcmp(hsubDistance.c_str(), topic)) { 
    rgrCBack(String(payload));
  }
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
      /*
      // Subscribe to <dev/node/property>/set
      client.subscribe(hsubMode);
      Serial.print("listening on topic ");
      Serial.println(hsubMode);
      */
      // Subscribe to <dev/node/property>/set
      client.subscribe(hsubDistance.c_str());
      Serial.print("listening on topic ");
      Serial.println(hsubDistance);
      
      /*
      // Subscribe to <dev/node/property>/set
      Serial.print("listening on topic ");
      Serial.println(hsubDspTxt);
      client.subscribe(hsubDspTxt);
      // Subscribe to <dev/node/property>/set
      client.subscribe(hsubDspCmd);
      Serial.print("listening on topic ");
      Serial.println(hsubDspCmd);
      */
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      Serial.print(" try again in ");
      Serial.print(cnt * len);
      Serial.println(" seconds");
      // Wait X seconds before retrying
      delay((cnt * len) * 1000);
      cnt = cnt * 2;
      if (cnt > 256) 
        cnt = 256;
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

void mqtt_ranger_set_dist(int d) {
 char t[8];
 Serial.print("mqtt pub ");
 itoa(d, t, 10);
 Serial.print(hpubDistance);
 Serial.print(' ');
 Serial.println(t);
 mqtt_homie_pub(hpubDistance, t, false);
}

// Called from sketch's loop()
void mqtt_loop() {
  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();

}
