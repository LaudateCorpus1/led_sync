// Derives from https://github.com/HarringayMakerSpace/ESP-Now
#include <Arduino.h>
#include "time.h"
#include <ESP8266WiFi.h>
extern "C" {
    #include <espnow.h>
     #include <user_interface.h>
}
#define WIFI_CHANNEL 4

// it seems that the mac address needs to be set before setup() is called
//      and the inclusion of user_interface.h facilitates that
//      presumably there is a hidden call to initVariant()

uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct __attribute__((packed)) DataStruct {
    char text[32];
    unsigned int time;
};
DataStruct receivedData;
DataStruct sendingData;

unsigned long lastSentMillis = 0;
unsigned long sendIntervalMillis = 3000;
bool friendFound = false;
bool friendNewlyFound = false;
bool quickRespond = false;
unsigned long friendTimeout = 5000;
unsigned long lastSeenFriendTime = 0;

void initVariant() {
}

void sendData() {
    sendingData.time = Time_GetTime();
    uint8_t byteArray[sizeof(sendingData)];
    memcpy(byteArray, &sendingData, sizeof(sendingData));
    esp_now_send(broadcastMac, byteArray, sizeof(sendingData)); // NULL means send to all peers
    Serial.println("Loop sent data");
}

void receiveCallBackFunction(uint8_t *senderMac, uint8_t *incomingData, uint8_t len) {
    if (!friendFound) {
      friendFound = true;
      friendNewlyFound = true;
      quickRespond = true;
    }
    lastSeenFriendTime = millis();
    memcpy(&receivedData, incomingData, sizeof(receivedData));
    if (abs(Time_GetTime() - receivedData.time) > 100) {
      quickRespond = true;
    }
    unsigned long avg = (Time_GetTime() + receivedData.time) / 2;

    Serial.print("Old time ");
    Serial.print(Time_GetTime());
    Serial.print(" New time ");
    Serial.print(receivedData.time);
    Serial.print(" Avg ");
    Serial.print(avg);
    Serial.println();

    Time_SetTime(avg);

    Serial.print("NewMsg ");
    Serial.print("MacAddr ");
    for (byte n = 0; n < 6; n++) {
        Serial.print(senderMac[n], HEX);
    }
    Serial.print("  MsgLen ");
    Serial.print(len);
    Serial.print("  Text ");
    Serial.print(receivedData.text);
    Serial.print("  Time ");
    Serial.print(receivedData.time);
    Serial.println();
}

void Radio_Update() {
  if (friendNewlyFound) {
    Serial.println("Found a friend!");
    friendNewlyFound = false;
  }
  if (quickRespond) {
    sendData();
    quickRespond = false;
  } else if (millis() > sendIntervalMillis && millis() - lastSentMillis >= sendIntervalMillis) {
    lastSentMillis = millis();
    sendData();
  }
  if (friendFound && lastSeenFriendTime > 0 && millis() - lastSeenFriendTime > friendTimeout) {
    Serial.println("Friend timeout!");
    friendFound = false;
  }
  // TODO delay to save battery.. I think?
}

void Radio_Init() {
    Serial.begin(115200);
    if (esp_now_init()!=0) {
        Serial.println("*** ESP_Now init failed");
        while(true) {};
    }
    // role set to COMBO so it can send and receive - not sure this is essential
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

    Serial.println("Starting Master");

    WiFi.mode(WIFI_STA); // Station mode for esp-now controller
    WiFi.disconnect();

    Serial.printf("This mac: %s, ", WiFi.macAddress().c_str());
    Serial.printf("target mac: %02x%02x%02x%02x%02x%02x", broadcastMac[0], broadcastMac[1], broadcastMac[2], broadcastMac[3], broadcastMac[4], broadcastMac[5]);
    Serial.printf(", channel: %i\n", WIFI_CHANNEL);

    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, NULL, 0);

    Serial.println("Setup finished");

    esp_now_register_recv_cb(receiveCallBackFunction);
}
