// One day, this might be a class. For now, it's just C
/*
 * Handles all of the mqtt interactions for ranger (there are many)
 * The topic stucture is Homie V3 semi-compliant
 */
#include <Arduino.h>
#include <stdlib.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Device.h"
#include "MQTT_Ranger.h"

#define USE_ACTIVE_HOLD

// forward declares 
// i.e Private:
void mqtt_reconnect();
void mqtt_send_config();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_publish(char *topic, char *payload);
void mqtt_homie_pub(String topic, String payload, bool retain); 
void controlCb(String topic, char *payload);
extern void updateFrom(String host, int port, String path); 

String wifi_id;
String wifi_password;
String mqtt_server;
int  mqtt_port;
String mqtt_device;
String hdevice;
String hname;
String hlname;
String hpub;
String hsub;
String hsubq;
//String hpubst;             // ..ranger/$status <- publish to 
#ifdef RANGER
String hpubDistance;       // ..ranger/distance <- publish to
String hsubDistance;       // ..ranger/distance/set -> subcribe to
String hsubRgrSet;           // ..ranger/cmd/set -> subscribe to
#endif
#ifdef DISPLAYG
String hsubDspCmd;         // ../display/cmd/set  -> subscribe to
String hsubDspTxt;         // ../display/text/set -> subscribe to
//String hsubCmdSet;          // ../control/cmd/set -> subscribe to
#endif
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

  WiFi.begin(wifi_id.c_str(), wifi_password.c_str());

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
      const char* mqdev, const char *hdev, const char *hnm,
      void (*ccb)(int, int), void (*dcb)(boolean, String) ) {

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
#ifdef RANGER
  // Create "homie/"HDEVICE"/ranger"
  hpub = "homie/" + hdevice + "/ranger";

  // Create "homie/"HDEVICE"/autoranger/status" 
  //hpubst =  "homie/" + hdevice + "/ranger/status";

  // Create "homie/"HDEVICE"/ranger/cmd
  hsubq = "homie/"+ hdevice + "/ranger/cmd";

  // Create "homie/"HDEVICE"/ranger/cmd/set
  hsubRgrSet = "homie/" + hdevice + "/ranger/cmd/set";
 
  // Create "homie/"HDEVICE"/ranger/distance for publishing
  hpubDistance = "homie/" + hdevice + "/ranger/distance";
 
  // Create "homie/"HDEVICE"/ranger/distance/set topic for subscribe 
  hsubDistance = "homie/" + hdevice + "/ranger/distance/set";
#endif  
#ifdef DISPLAYG
  // create the subscribe topics for "homie/"HDEVICE"/display/cmd/set
  //hsubDspCmd = "homie/" + hdevice + "/display/mode/set";

  // create the subscribe topics for "homie/"HDEVICE"/display/text/set
  hsubDspTxt = "homie/" + hdevice + "/display/text/set";

  // creat the subscribe topic for "home/"HDEVICE"/control/cmd/set
  //hsubCmdSet = "homie/" + hdevice + "/control/cmd/set";
#endif

  // Sanitize hname -> hlname
  hlname = String(hname);
  hlname.toLowerCase();
  hlname.replace(" ", "_");
  hlname.replace("\t", "_");
 
  setup_wifi();
  client.setServer(mqtt_server.c_str(), mqtt_port);
  client.setCallback(mqtt_callback);
  mqtt_reconnect();
  
  // Create and Publish the infrastructure topics for Homie v3
  
  // "homie/"HDEVICE"/$homie" -> "3.0.1"
  tmp = "homie/" + hdevice + "/$homie";
  mqtt_homie_pub(tmp, "3.0.1", true);
  
  //"homie/"HDEVICE"/$name" -> hlname
   tmp = "homie/" + hdevice + "/$name";
  mqtt_homie_pub(tmp, hlname, true);

  // "homie/"HDEVICE"/$state -> ready
  tmp = "homie/" + hdevice + "/$state";
  mqtt_homie_pub(tmp, "ready", true);
  
  // "homie/"HDEVICE"/$mac" -> macAddr
  tmp = "homie/" + hdevice + "/$mac";
  String mc = String(macAddr);
  mc.toUpperCase();
  mqtt_homie_pub(tmp, mc, true);
  
  // "homie/"HDEVICE"/$localip" -> ipAddr
  tmp = "homie/" + hdevice + "/$localip";
  mqtt_homie_pub(tmp, ipAddr, true);
  
  //"homie/"HDEVICE"/$nodes", -> 
  tmp = "homie/" + hdevice + "/$nodes";
#if defined(RANGER) && defined(DISPLAYG)
  mqtt_homie_pub(tmp, "ranger,display", true);
#elif defined(RANGER)
  mqtt_homie_pub(tmp, "ranger", true);
#elif defined(DISPLAYG)
  mqtt_homie_pub(tmp, "display", true);
#endif

  // -------------- begin node control ---------------------
#ifdef RANGER
  // "homie/"HDEVICE"/control/$name" -> hname (Un sanitized)
  tmp = "homie/" + hdevice + "/control/$name";
  mqtt_homie_pub(tmp, hname, true);
  
  // "homie/"HDEVICE"/control/$type" ->  "sensor"
  tmp = "homie/" + hdevice + "/control/$type";
  mqtt_homie_pub(tmp, "control", true);
  
  // "homie/"HDEVICE"/control/$properties" -> "cmd, text"
   tmp = "homie/" + hdevice + "/control/$properties";
  mqtt_homie_pub(tmp, "cmd", true);

  // Property 'cmd' of 'control' node
  // "homie/"HDEVICE"/control/cmd/$name ->, Unsanitized hname
   tmp = "homie/" + hdevice + "/control/cmd/$name";
  mqtt_homie_pub(tmp, hname, true); 

  // "homeie"HDEVICE"/control/cmd/$datatype" -> "string"
  tmp = "homie/" + hdevice + "/control/cmd/$datatype";
  mqtt_homie_pub(tmp, "string", true);

  // "homie/"HDEVICE"/control/cmd/$settable" -> "false"
  tmp = "homie/" + hdevice + "/control/cmd/$settable";
   mqtt_homie_pub(tmp, "false", true);

  // "homie/"HDEVICE"/control/cmd/$name" -> Unsantized hname
  tmp = "homie/" + hdevice + "/control/cmd/$name";
  mqtt_homie_pub(tmp, hname, true);

  // "homie/"HDEVICE"/control/cmd/$retained" -> "true"
  tmp = "homie/" + hdevice + "/control/cmd/$retained";
  mqtt_homie_pub(tmp, "false", true);
  // ----------- end node control ------------------

  // begin node autoranger
  // end node - autoranger
#endif
#ifdef DISPLAYG
  // begin node - display
  // "homie/"HDEVICE"/display/$name" -> hname (Un sanitized)
  tmp = "homie/" + hdevice + "/display/$name";
  mqtt_homie_pub(tmp, hname, true);
  
  // "homie/"HDEVICE"/display/$type" ->  "sensor"
  tmp = "homie/" + hdevice + "/display/$type";
  mqtt_homie_pub(tmp, "sensor", true);
  
  // "homie/"HDEVICE"/display/$properties" -> "cmd, text"
   tmp = "homie/" + hdevice + "/display/$properties";
  mqtt_homie_pub(tmp, "cmd,text", true);

  // Property 'cmd' of 'display' node
  // "homie/"HDEVICE"/display/cmd/$name ->, Unsanitized hname
   tmp = "homie/" + hdevice + "/display/cmd/$name";
  mqtt_homie_pub(tmp, hname, true); 

  // "homeie"HDEVICE"/display/cmd/$datatype" -> "string"
  tmp = "homie/" + hdevice + "/display/cmd/$datatype";
  mqtt_homie_pub(tmp, "string", true);

  // "homie/"HDEVICE"/display/cmd/$settable" -> "false"
  tmp = "homie/" + hdevice + "/display/cmd/$settable";
   mqtt_homie_pub(tmp, "false", true);

  // "homie/"HDEVICE"/display/cmd/$name" -> Unsantized hname
  tmp = "homie/" + hdevice + "/display/cmd/$name";
  mqtt_homie_pub(tmp, hname, true);

  // "homie/"HDEVICE"/display/cmd/$retained" -> "true"
  tmp = "homie/" + hdevice + "/display/cmd/$retained";
  mqtt_homie_pub(tmp, "false", true);
  
  // Property 'text' of 'display' node
  // "homie/"HDEVICE"/display/text/$name", -
  tmp = "homie/" + hdevice + "/display/text/$name";
  mqtt_homie_pub(tmp, hname, true);  

  // "homie/"HDEVICE"/display/text/$datatype" ->  "strimg"
  tmp = "homie/" + hdevice + "/display/text/$datatype";
  mqtt_homie_pub(tmp, "string", true);

  // "homie/"HDEVICE"/display/text/$settable" -> "true"
  tmp = "homie/" + hdevice + "/display/text/$settable";
  mqtt_homie_pub(tmp, "true", true);

  // "homie/"HDEVICE"/display/text/$retained" -> "true"
  tmp = "homie/" + hdevice + "/display/text/$retained";
  mqtt_homie_pub(tmp, "false", true);
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


  if (! strcmp(hsubRgrSet.c_str(), topic)) {
     controlCb(topic, payload);    // in this file, for now.
  } else if (! strcmp(hsubDistance.c_str(), topic)) { 
    int d = atoi(payload);
    if (d < 0)
      d = 0;
    else if (d > 3600)
      d = 3600;
    rgrCBack(rgr_mode, d);
  } else if (! strcmp(hsubDspCmd.c_str(), topic)) {
      controlCb(topic, payload);    // in this file, for now.
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
    if (client.connect(mqtt_device.c_str())) {
      Serial.println("connected");
#ifdef RANGER
      // Subscribe to <dev/node/property>/set
      client.subscribe(hsubRgrSet.c_str());
      Serial.print("listening on topic ");
      Serial.println(hsubRgrSet);
      
      // Subscribe to <dev/node/property>/set
      client.subscribe(hsubDistance.c_str());
      Serial.print("listening on topic ");
      Serial.println(hsubDistance);
#endif
#ifdef DISPLAYG
      // Subscribe to <dev>/display/text/set
      client.subscribe(hsubDspTxt.c_str());
      Serial.print("listening on topic ");
      Serial.println(hsubDspTxt);
     
      // Subscribe to <dev>/display/cmd>/set
      client.subscribe(hsubDspCmd.c_str());
      Serial.print("listening on topic ");
      Serial.println(hsubDspCmd);

      // subscribe to <device>/display/text/set
      //client.subscribe(hsubCmdSet.c_str());
      //Serial.print("listening on topic ");
      //Serial.println(hsubCmdSet);
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

void mqtt_ranger_set_dist(int d) {
 char t[8];
 Serial.print("mqtt pub ");
 itoa(d, t, 10);
 Serial.print(hpubDistance);
 Serial.print(" ");
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

void tryUpdate(String url) {
    Serial.println("Update from "+url);
    return;
    String host;
    int port = 0;
    String path;
    if (url.length() ) {
      // break it down.
      int sl = url.indexOf("//");
      if (sl > 4) {
        url = url.substring(sl+2);
        int portpos = url.indexOf(":");
        int trslh = url.indexOf("/");
        if (portpos > 0) {
          port = url.substring(portpos+1,trslh).toInt();
        } else {
          port = 2345; //Better default?
          portpos = trslh;
        }
        host = url.substring(0, portpos);
        path = url.substring(trslh);
      }
      if (host.length() > 2 && port > 1000  && path.length() > 6) {
        Serial.print("Start update from "+String(host)+":"+String(port));
        Serial.println(" path: "+String(path));
        updateFrom(host, port, path);  
        // no return if things work, nothing to do if they don't
      }
    }
}

// should be pointed to the key/value pairs
void processSettingsJson(DynamicJsonDocument doc) {
#ifdef RANGER
  if (doc["mode"]) {
    const char *mode = doc["mode"];
    if (! strcmp(mode, "guide")) {

    }
    else if (! strcmp(mode, "once")) {

    }
    else if (! strcmp(mode, "free")) {

    }
  }
  int every = EVERY;             // report every <n> seconds 
  if (doc["every"]) {
    every = doc["every"];
  }

  int keep_alive = KEEP_ALIVE ;    // default 4 hrs in (4 * 50 * 60)
  if (doc["keep-alive"]) {
    keep_alive = doc["keep-alive"];
  }           

  int sample = SAMPLE;             // read sensor every n seconds
  if (doc["sample"]) {
    sample = doc["sample"];
  }

  int range_low = RANGE_LOW;
  if (doc["range-low"])  range_low = doc["range-low"];

  int range_high = RANGE_HIGH;
  if (doc["range-high"])  range_high = doc["range-high"];
  
  int slopH = SLOPH;
  if (doc["slopH"]) slopH = doc["slopH"];

  int slopL = SLOPL;
  if (doc["slopL"]) slopH = doc["slopL"];

  int average = AVERAGE;
  if (doc["average"]) average = doc["average"];

  boolean report_all = REPORT_ALL;
  if (doc["report-all"]) report_all = doc["report_all"];

  int bounds_low = BOUNDS_LOW;
  if (doc["bounds_low"]) bounds_low = doc["bounds-low"];

  int bounds_high = BOUNDS_HIGH;
  if (doc["bounds_high"]) bounds_high = doc["bounds-high"];

#ifdef DISPLAYG
  const char* guide_low = GUIDE_LOW;
  if (doc["guide-low"]) guide_low = doc["guide-low"];

  const char* guide_high = GUIDE_HIGH;
  if (doc["guide_high"]) guide_high = doc["guide_high"];

  const char* guide_target = GUIDE_TARGET;
  if (doc["guide-target"]) guide_target = doc["guide-target"];

#endif // DISPLAY and 
#endif // RANGER
  // Now we handle DISPLAY settings
#ifdef DISPLAYG
  const char *screen_color = SCREEN_COLOR;
  if (doc["color"]) screen_color = doc["color"];

  int blank_after = BLANK_AFTER;
  if (doc["blank-after"]) blank_after = doc["blank-after"];

  boolean do_scroll = SCROLL;
  if (doc["scroll"]) do_scroll = doc["scroll"];
#endif
}

void processTextJson(DynamicJsonDocument doc) {
   
}

// parse json (cmds)
void controlCb(String topic, char *payload) {
#ifdef DISPLAYG
  if (payload[0]  != '{')
    // not json - assume plain text to display
    doDisplay(true, payload);
#endif
  const size_t capacity = JSON_OBJECT_SIZE(2) + 180;
  DynamicJsonDocument doc(capacity);

  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.f_str());
  }
  const char *cmd = doc["cmd"];
  if (cmd) { 
    if (! strcmp(cmd, "update")) { // "update"
      tryUpdate(doc["url"]);
    } else if (! strcmp(cmd, "off")) {
#ifdef DISPLAYG
      displayOff();
#endif
#ifdef RANGER
      rangerOff();
#endif
    } else if (! strcmp(cmd, "on")) {
#ifdef DISPLAYG
      displayOn();
#endif
#ifdef RANGER
      rangerOn();
#endif
    }
    return;
  }
  const char *text = doc["text"];
  if (text) {
    processTextJson(doc);
    return;
  }
  const char *settings = doc["settings"];
  if (settings) {
    processSettingsJson(doc["settings"]);
    return;
  }

}