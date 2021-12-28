#include <Arduino.h>
#include "Update.h"
#include "MQTT_Ranger.h"
#include <WiFi.h>

extern WiFiClient espClient;
long contentLength = 0;
bool isValidContentType = false;

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
   return header.substring(strlen(headerName.c_str()));
}

// OTA update 
void updateFrom(String host, int port, String path) {
    Serial.println("host: " + host);
    Serial.println("port: " + String(port));
    Serial.println("path: " + path);
    String lmDateStr;

    if (espClient.connect(host.c_str(), port)) {
        Serial.println("Fetching Bin: " + String(path));
        espClient.print(String("GET ") + path + " HTTP/1.1\r\n" + 
            "Host: " + host + "\r\n" +
           "Cache-Control: no-cache\r\n" +
           "Connection: close\r\n\r\n");
        unsigned long timeout = millis();
        while (espClient.available() == 0) {
            if (millis() - timeout > 5000) {
                Serial.println("Client Timeout !");
                espClient.stop();
                return;
            }
        }
        while (espClient.available()) {
            // read line till /n
            String line = espClient.readStringUntil('\n');
            // remove space, to check if the line is end of headers
            line.trim();

            // if the the line is empty, then this is end of headers
            // break the while and feed the remaining `client` to the
            // Update.writeStream();
            if (!line.length()) {
                //headers ended
                break; // and get the OTA started
            }
            //Serial.println("Line: " + line);
 
            // Check if the HTTP Response is 200
            // else break and Exit Update
           if (line.startsWith("HTTP/1.")) {
                if (line.indexOf("200") < 0) {
                    Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
                    break;
                }
            }

            // extract headers here
            // Start with content length
            if (line.startsWith("Content-Length: ")) {
                contentLength = atol((getHeaderValue(line, "Content-Length: ")).c_str());
                Serial.println("Got " + String(contentLength) + " bytes from server");
            }

            // Next, the content type
            if (line.startsWith("Content-type: ")) {
               String contentType = getHeaderValue(line, "Content-type: ");
                Serial.println("Got " + contentType + " payload.");
                if (contentType == "application/octet-stream") {
                   isValidContentType = true;
                }
            }
            // Last-Modified probably not worth capturing.
            if (line.startsWith("Last-Modified: ")) {
                lmDateStr = getHeaderValue(line, "Last-Modified: ");
                Serial.println("Last-Modified: " + lmDateStr);
            }
        }
        // check contentLength and content type
        if (contentLength && isValidContentType) {
            // Check if there is enough to OTA Update
            bool canBegin = Update.begin(contentLength);

            if (canBegin) {
                Serial.println("Begin OTA. This may take 2 − 5 mins to complete. Things might be quite for a while.. Patience!");
                // No activity would appear on the Serial monitor
                // So be patient. This may take 2 - 5mins to complete
                size_t written = Update.writeStream(espClient);

                if (written == contentLength) {
                    Serial.println("Written : " + String(written) + " successfully");
                } else {
                    Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
                   // retry??
                    // execOTA();
                 }

                if (Update.end()) {
                    Serial.println("OTA done!");
                    if (Update.isFinished()) {
                        Serial.println("Update successfully completed. Rebooting.");
                        ESP.restart();
                        return;
                    } else {
                        Serial.println("Update not finished? Something went wrong!");
                   }
                } else {
                    Serial.println("Error Occurred. Error #: " + String(Update.getError()));
                }
            } else {
                // not enough space to begin OTA
                // Understand the partitions and space availability
                Serial.println("Not enough space to begin OTA");
                espClient.flush();
            }
        } else {
            Serial.println("Failed to connect " + host + ":" + String(port));
        }
    }
}
