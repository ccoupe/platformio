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
String hsub;
String hsubq;
String hpubst;             // ..autoranger/$status <- publish to 
String hpubDistance;       // ..autoranger/distance <- publish to
String hsubDistance;       // ..autoranger/distance/set -> subcribe to
String hsubMode;            // ..autoranger/mode/set -> subscribe to
String hsubDspCmd;         // ../display/cmd/set  -> subscribe to
String hsubDspTxt;         // ../display/text/set -> subscribe to
void (*rgrCBack)(int mode, int newval); // does autorange to near newval
void (*dspCBack)(boolean st, String str);       

int rgr_mode = RGR_ONCE;

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

void mqtt_setup(char *wid, char *wpw, char *mqsrv, int mqport, char* mqdev,
    char *hdev, char *hnm, void (*ccb)(int, int), void (*dcb)(boolean, String) ) {

  rgrCBack = ccb;
  dspCBack = dcb;
  wifi_id = wid;
  wifi_password = wpw;
  mqtt_server = mqsrv;
  mqtt_port = mqport;
  mqtt_device = mqdev;
  hdevice = hdev;
  hname = hnm;
  String tmp;

  // Create "homie/"HDEVICE"/autoranger"
  hpub = "homie/" + hdevice + "/autoranger";
  /*
  hpub = (char *)malloc(6 + strlen(hdevice) + 22); // wastes a byte or two.
  strcpy(hpub, "homie/");
  strcat(hpub, hdevice);
  strcat(hpub, "/autoranger");
  */

  // Create "homie/"HDEVICE"/autoranger/status" 
  hpubst =  "homie/" + hdevice + "/autoranger/status";
  /*
  hpubst = (char *)malloc(strlen(hpub) + 8);
  strcpy(hpubst, hpub);
  strcat(hpubst, "/status");
  */

  // Create "homie/"HDEVICE"/autoranger/mode
  hsubq = "homie/"+ hdevice + "/autoranger/mode";
  /*
  hsubq = (char *)malloc(6+strlen(hdevice)+30); // wastes a byte or two.
  strcpy(hsubq, "homie/");
  strcat(hsubq, hdevice);
  strcat(hsubq, "/autoranger/mode");
  */

  // Create "homie/"HDEVICE"/autoranger/mode/set
  hsub = "homie/" + hdevice + "/autoranger/mode/set";
  /*
  hsub = (char *)malloc(strlen(hsubq) + 6);
  strcpy(hsub, hsubq);
  strcat(hsub, "/set");
  hsubMode = strdup(hsub);
  */

  // Create "homie/"HDEVICE"/autoranger/distance for publishing
  hpubDistance = "homie/" + hdevice + "/autoranger/distance";
  /*
  hpubDistance = (char *)malloc(strlen(hpub) + 12);
  strcpy(hpubDistance, hpub);
  strcat(hpubDistance, "/distance");
  */

  // Create "homie/"HDEVICE"/autoranger/distance/set topic for subscribe 
  hsubDistance = "homie/" + hdevice + "/autoranger/distance/set";
  /*
  hsubDistance = (char *)malloc(strlen(hpubDistance) + 5);
  strcpy(hsubDistance, hpubDistance);
  strcat(hsubDistance, "/set");
  */
  
  // create the subscribe topics for "homie/"HDEVICE"/display/cmd/set
  hsubDspCmd = "homie/" + hdevice + "/display/cmd/set";
  /*
  hsubDspCmd = (char *)malloc(6+strlen(hdevice)+20); // wastes a byte or two.
  strcpy(hsubDspCmd, "homie/");
  strcat(hsubDspCmd, hdevice);
  strcat(hsubDspCmd, "/display/mode/set");
  */

  // create the subscribe topics for "homie/"HDEVICE"/display/text/set
  hsubDspTxt = "homie/" + hdevice + "/display/text/set";
  /*
  hsubDspTxt = (char *)malloc(6+strlen(hdevice)+20); // wastes a byte or two.
  strcpy(hsubDspTxt, "homie/");
  strcat(hsubDspTxt, hdevice);
  strcat(hsubDspTxt, "/display/text/set");
  */

  // Sanitize hname -> hlname
  hlname = String(hname);
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
  // TODO: not unicode compatible, might work for utf-8 though
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
  //strcpy(p, "/$homie");
  tmp = "homie/" + hdevice + "/$homie";
  mqtt_homie_pub(tmp, "3.0.1", true);
  
  //"homie/"HDEVICE"/$name" -> hlname
  //strcpy(p, "/$name");
  tmp = "homie/" + hdevice + "/$name";
  mqtt_homie_pub(tmp, hlname, true);

  // "homie/"HDEVICE"/$state -> ready
  //strcpy(p, "/$state");
  tmp = "homie/" + hdevice + "/$state";
  mqtt_homie_pub(tmp, "ready", true);
  
  // "homie/"HDEVICE"/$mac" -> macAddr
  //strcpy(p, "/$mac");
  tmp = "homie/" + hdevice + "/$mac";
  String mc = String(macAddr);
  mc.toUpperCase();
  mqtt_homie_pub(tmp, mc, true);
  
  // "homie/"HDEVICE"/$localip" -> ipAddr
  //strcpy(p, "/$localip");
  tmp = "homie/" + hdevice + "/$localip";
  mqtt_homie_pub(tmp, ipAddr, true);
  
  //"homie/"HDEVICE"/$nodes", -> 
  //strcpy(p, "/$nodes");
  tmp = "homie/" + hdevice + "/$nodes";
  mqtt_homie_pub(tmp, "autoranger,display", true);

  // end node - autoranger
  // begin node - display
  
  // "homie/"HDEVICE"/display/$name" -> hname (Un sanitized)
  //strcpy(p, "/display/$name");
  tmp = "homie/" + hdevice + "/display/$name";
  mqtt_homie_pub(tmp, hname, true);
  
  // "homie/"HDEVICE"/display/$type" ->  "sensor"
  //strcpy(p, "/display/$type");
  tmp = "homie/" + hdevice + "/display/$type";
  mqtt_homie_pub(tmp, "sensor", true);
  
  // "homie/"HDEVICE"/display/$properties" -> "cmd, text"
  //strcpy(p, "/display/$properties");
  tmp = "homie/" + hdevice + "/display/$properties";
  mqtt_homie_pub(tmp, "cmd,text", true);

  // Property 'cmd' of 'display' node
  // "homie/"HDEVICE"/display/cmd/$name ->, Unsanitized hname
  //strcpy(p, "/display/cmd/$name");
  tmp = "homie/" + hdevice + "/display/cmd/$name";
  mqtt_homie_pub(tmp, hname, true); 

  // "homeie"HDEVICE"/display/cmd/$datatype" -> "string"
  //strcpy(p, "/display/cmd/$datatype");
  tmp = "homie/" + hdevice + "/display/cmd/$datatype";
  mqtt_homie_pub(tmp, "string", true);

  // "homie/"HDEVICE"/display/cmd/$settable" -> "false"
  //strcpy(p, "/display/cmd/$settable");
  tmp = "homie/" + hdevice + "/display/cmd/$settable";
   mqtt_homie_pub(tmp, "false", true);

  // "homie/"HDEVICE"/display/cmd/$name" -> Unsantized hname
  //strcpy(p, "/display/cmd/$name");
  tmp = "homie/" + hdevice + "/display/cmd/$name";
  mqtt_homie_pub(tmp, hname, true);

  // "homie/"HDEVICE"/display/cmd/$retained" -> "true"
  //strcpy(p,"/display/cmd/$retained");
  tmp = "homie/" + hdevice + "/display/cmd/$retained";
  mqtt_homie_pub(tmp, "false", true);
  
  // Property 'text' of 'display' node
  
  // "homie/"HDEVICE"/display/text/$name", -
  //strcpy(p, "/display/text/$name");
  tmp = "homie/" + hdevice + "/display/text/$name";
  mqtt_homie_pub(tmp, hname, true);  

  // "homie/"HDEVICE"/display/text/$datatype" ->  "strimg"
  //strcpy(p, "/display/text/$datatype");
  tmp = "homie/" + hdevice + "/display/text/$datatype";
  mqtt_homie_pub(tmp, "string", true);

  // "homie/"HDEVICE"/display/text/$settable" -> "true"
  //strcpy(p, "/display/text/$settable");
  tmp = "homie/" + hdevice + "/display/text/$settable";
  mqtt_homie_pub(tmp, "true", true);

  // "homie/"HDEVICE"/display/text/$retained" -> "true"
  //strcpy(p,"/display/text/$retained");
  tmp = "homie/" + hdevice + "/display/text/$retained";
  mqtt_homie_pub(tmp, "false", true);
  
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

  if (! strcmp(hsubMode.c_str(), topic)) {
    if (! strcmp(payload, "once")) {
      rgr_mode = RGR_ONCE;
    } else if (! strcmp(payload, "continous")) {
      rgr_mode = RGR_CONTINOUS;
    } else if (! strcmp(payload, "free")) {
      rgr_mode = RGR_FREE;
      rgrCBack(rgr_mode, 3600);
    } else if (! strcmp(payload, "snap")) {
      rgr_mode = RGR_SNAP;
      rgrCBack(rgr_mode, 3600);
    } else if (! strcmp(payload, "off")) {
      rgr_mode = RGR_ONCE;
      rgrCBack(rgr_mode, 0);
    } else {
      Serial.println("mode: bad payload");
      rgr_mode = RGR_ONCE;
    }
  } else if (! strcmp(hsubDistance.c_str(), topic)) { 
    int d = atoi(payload);
    if (d < 0)
      d = 0;
    else if (d > 3600)
      d = 3600;
    rgrCBack(rgr_mode, d);
  } else if (! strcmp(hsubDspCmd.c_str(), topic)) {
    if (!strcmp(payload, "on") || !strcmp(payload, "true")) {
      dspCBack(true, (char*)0);
    } else if ((! strcmp(payload, "off")) || (!strcmp(payload, "false" ))) {
      Serial.println("display set off");
      dspCBack(false, (char*)0);
    }
  } else if (! strcmp(hsubDspTxt.c_str(), topic)) {
    dspCBack(true, payload);
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

      // Subscribe to <dev/node/property>/set
      client.subscribe(hsubMode.c_str());
      Serial.print("listening on topic ");
      Serial.println(hsubMode);
      
      // Subscribe to <dev/node/property>/set
      client.subscribe(hsubDistance.c_str());
      Serial.print("listening on topic ");
      Serial.println(hsubDistance);

      // Subscribe to <dev/node/property>/set
      Serial.print("listening on topic ");
      Serial.println(hsubDspTxt);
      client.subscribe(hsubDspTxt.c_str());

      // Subscribe to <dev/node/property>/set
      client.subscribe(hsubDspCmd.c_str());
      Serial.print("listening on topic ");
      Serial.println(hsubDspCmd);

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
 Serial.print("mqtt pub");
 itoa(d, t, 10);
 Serial.print(hpubDistance);
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
