//#include <WiFi.h>
//#include <ESPmDNS.h>
//#include "mdns.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

#ifndef CubePuzzle_h
#define CubePuzzle_h

// Speed of Serial Communication
#define SERIAL_COMSPEED 115200

// Credentials for network SSID and PWD
#define SSID "ubilab"
#define PWD "Mo4ienei3thoo7wei4ei"

// Device can be found on the network using this name
#define NAME "Puzzle1"

// PWD for OTA
#define OTA_PWD "Cube"

// TCP Client connecting to this server
// IP or DNS name of TCP Server
#define SERVER_IP "10.0.0.2" 
// Port of server, should be >= 2000
#define SERVER_PORT 2000
WiFiClient client;


// MQTT server or DNS
#define MQTT_SERVER_IP "10.8.166.20"
// Standard port for MQTT
#define MQTT_PORT 1883
// RAW TCP client and pubsub class using it

WiFiClient mqttClient;
PubSubClient mqtt(mqttClient);

// Global cstring for message building
#define MSG_SIZE 50
char msg[MSG_SIZE] = {'\0'};

// Enum and global variable for puzzle state
enum PuzzleState {
  idle = 0,
  activ = 1,
  solved = 2,
  panelsolved = 3,
  dummy = 4,
  };
PuzzleState puzzleState = idle;
PuzzleState puzzleStatePrev = dummy;


// JSON dict of given size
StaticJsonDocument<3 * MSG_SIZE> dict;

void setupOTA();
void mqttCallback(char*, byte*, unsigned int);
const char * puzzleStateToStr(PuzzleState);
void puzzleStateChanged();
void puzzleActive();
void puzzleSolved();
void puzzleIdle();
void puzzlePanel();
const char * handleMsg(const char *, const char *);
void mqtt_publish(const char*, const char*, const char*);
void handleStream(Stream *);

#endif //CubePuzzle_h
