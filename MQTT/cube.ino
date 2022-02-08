#include <Adafruit_NeoPixel.h>
#include<Wire.h>
#include "CubePuzzle.h" // MQTT


//char *ssid = "BV9900Pro";
//char *ssid = "Mi 11 Lite";
//char *password = "ff799896820f";

const uint8_t MPU_address= 104; // the one from uni has 40, the one from Kevin has 104

Adafruit_NeoPixel strip = Adafruit_NeoPixel(67, D5, NEO_GRB + NEO_KHZ800);

uint8_t upside, upside_old = -1, upside_duration;

int snake[70] = {0};
int reset = 21;

uint8_t sequence[6] = {2,6,4,3,5,1};
uint8_t seq_cntr = 0;
uint8_t moved = 0;

// MQTT code variables

unsigned int min_time = 0;
unsigned int max_time = 0;

StaticJsonDocument<300> doc;

// ______________________________________________________________________
void setup() {
  
  Serial.begin(SERIAL_COMSPEED);
  Wire.begin();
  /*
  Serial.println("\nHowdy!");
  // initialize WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
  */

  Wire.beginTransmission(MPU_address);
  Wire.write(0x6B);
  Wire.write(0);
  byte error = Wire.endTransmission();
  if(error) {
    Serial.print("ERROR, when trying to connect to the device. Error: "); 
    Serial.println(error);
  }

  // set sensitivity to +- 2g
  Wire.beginTransmission(MPU_address);
  Wire.write(0x1C);
  Wire.write(0b00000000); 
  Wire.endTransmission();

  strip.begin();
  for (int x=0; x<67; x++) {
    strip.setPixelColor(x, 0);
  }
  strip.show();
  Serial.println("Setup except for MQTT done");

  /*********************
     WiFi Connection
   *********************/
  Serial.print("Connecting to ");
  Serial.println(SSID);
  // Set name passed to AP while connection
//  mdns::MDns myMdns();
  WiFi.hostname(NAME);
  // Connect to AP
  WiFi.begin(SSID, PWD);
  // Wait while not connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print IP
  Serial.println("\nWiFi connected.\nIP Adress: ");
  Serial.println(WiFi.localIP());


  /*********************
     multicast DNS
   *********************/
  // Set name of this device
  if (MDNS.begin(NAME)) {
    Serial.println("Setting up MDNS responder!");
  }
  // The service this device hosts (example)
  MDNS.addService("_escape", "_tcp", 2000);


  /*************************
     Connect to TCP server
   *************************/
  if (client.connect(SERVER_IP, SERVER_PORT)) {
    Serial.println("Connected to server");
  } else {
    Serial.println("Cannot connect to server");
  }

  // OTA setup in separate function
  setupOTA();

  /*************************
     MQTT Setup
   *************************/

  // Set IP and port of broker
  mqtt.setServer(MQTT_SERVER_IP, MQTT_PORT);
  // Set CB function to be called for all subscriptions
  mqtt.setCallback(mqttCallback);
  // Connect to broker using a unique ID (here the name)
  if (mqtt.connect(NAME)) {
    Serial.println("Connected to MQTT server");
    // Subscribe to a certain topic
    mqtt.subscribe("1/#");
  } else {
    Serial.println("Cannot connect to MQTT server");
  }

  // Indicate end of setup
  Serial.println("Setup for MQTT done!");

}


// ______________________________________________________________________
int16_t accDefault[3]= {0};
int16_t moveThres = 8196;
bool puzzle_active = false;
void loop () {

  // MQTT Code
  // Needs to be called to check for external update request
  ArduinoOTA.handle();
  // Checks for MQTT subscriptions
  mqtt.loop();

  // Change state from idle to active afterwards cycle between
  // solved and active
  if (puzzleState == idle) {
    puzzleIdle();
  } else if (puzzleState == activ) {
    puzzleActive();
  } else if (puzzleState == solved){
    puzzleSolved();
  }
  else if (puzzleState == panelsolved){
    puzzlePanel();
  }

  // If at least one byte is available over serial
  if (Serial.available()) {
    handleStream(&Serial);
  }
  
  // If at least one byte is available over TCP
  if (client.available()) {
    handleStream(&client);
  }

  if (puzzleState == activ){
    if(moved == 1) glimmer();
    // read acc data
    Wire.beginTransmission(MPU_address);
    Wire.write(0x3B);
    Wire.endTransmission();
    Wire.requestFrom(MPU_address, 6);
    while(Wire.available() < 6);
    int16_t accX_raw = Wire.read() << 8 | Wire.read();
    int16_t accY_raw = Wire.read() << 8 | Wire.read();
    int16_t accZ_raw = Wire.read() << 8 | Wire.read();
  
    uint8_t accDir = 0x70;
    if(accX_raw < 0) {
      accDir &= ~(0x40);
      accX_raw = -accX_raw;
    } 
    if(accY_raw < 0) {
      accDir &= ~(0x20);
      accY_raw = -accY_raw;
    } 
    if(accZ_raw < 0) {
      accDir &= ~(0x10);
      accZ_raw = -accZ_raw;
    }
  
    if(accX_raw/2 > accY_raw && accX_raw/2 > accZ_raw) accDir |= 0x04;
    else if(accY_raw/2 > accX_raw && accY_raw/2 > accZ_raw) accDir |= 0x02;
    else if(accZ_raw/2 > accX_raw && accZ_raw/2 > accY_raw) accDir |= 0x01;
  
    if(moved == 0 && accDefault[0] == 0 && accDefault[1] == 0 && accDefault[2] == 0) {
      accDefault[0] = accX_raw;
      accDefault[1] = accY_raw;
      accDefault[2] = accZ_raw;
    } else if(moved == 0 && (accX_raw-accDefault[0] > moveThres || accDefault[0]-accX_raw > moveThres)) {
        moved = 1;
        Serial.println("I was moved!!! *_*");
    }
  
    // printing raw data
    /*Serial.print(accX_raw); Serial.print("\t");
    Serial.print(accY_raw); Serial.print("\t");
    Serial.print(accZ_raw); Serial.print("\t");
    Serial.print(accDir&0x08); Serial.println("\t");*/
    switch(accDir&0x07) {
      case 0x04:
        upside = (accDir&0x40) ? 5 : 3;
        break;
      case 0x02:
        upside = (accDir&0x20) ? 4 : 2;
        break;
      case 0x01:
        upside = (accDir&0x10) ? 1 : 6;
        break;
      default:
        if(accDir&0x07) upside = 10;
        break;
    }
    if(upside != upside_old) {
      upside_duration = 0;
      upside_old = upside;
    } else if(upside_duration < 40-1) {
      upside_duration++;
    } else if(upside_duration == 40-1) {
      upside_duration++;
      Serial.print("Cube is now on side ");
      Serial.println(upside);
  
      if(seq_cntr < 6 && moved == 1) {
        if(upside == sequence[seq_cntr]) {
          right(upside-1);
          seq_cntr++;
          if(seq_cntr == 6){
            Serial.println("Cube has been solved!");
            puzzleState = solved;
          }
        } else {
          wrong();
          seq_cntr = 0;
        }
      }
  
    }
  }else{
    Serial.println("puzzle is inactive");
  }
  // Just some delay to not spam the terminal
  delay(50);
}


// ______________________________________________________________________
int y = 5;
int ydir = 3;
void glimmer() {
  y += ydir;
  if(y >= 70 || y <= 5) ydir = -ydir;
  for (int x=0; x<60; x++) {
    if (snake[x] == 0 and x%2 == 0) {
      strip.setPixelColor(x, y, 0, y);
    }
    if (snake[x] == 0 and x%2 != 0) {
      strip.setPixelColor(x, 75-y, 0, 75-y);
    }
  }
  strip.show();
}


// ______________________________________________________________________
int dickhead_counter = 1;
void wrong() {
  for (int z=0; z<7; z++) {
    for (int x=0; x<60; x++) {
      strip.setPixelColor(x, 50, 0, 0);
      snake[x] = 0;
    }
    strip.show();
    delay(100);

    for (int x=0; x<60; x++) {
      strip.setPixelColor(x, 0);
    }
    strip.show();
    delay(100);
  }
  delay(1000*dickhead_counter);
  dickhead_counter++;
  glimmer();
}


// ______________________________________________________________________
void right(int side) {
  /*
   
  for (int x=side*10; x<10*side+10; x++) {
    snake[x] = 1;
  }
  */
  int lstart = 0;
  int lend = 0;

  switch(side){
    case 0:
      lstart = 0;
      lend = 9;
      break;
    case 1:
      lstart = 10;
      lend = 21;
      break;
    case 2:
      lstart = 22;
      lend = 31;
      break;
    case 3:
      lstart = 32;
      lend = 38;
      break;
    case 4:
      lstart = 39;
      lend = 52;
      break;
    case 5:
      lstart = 53;
      lend = 66;
      break;
    default:
      Serial.println("Error");
  }

  for (int x=lstart; x<=lend; x++) {
    snake[x] = 1;
  }

  for (int y=0; y<70; y+=2) {
    for (int x=0; x<67; x++) {
      strip.setPixelColor(x, 0, y, 0);
    }
    strip.show();
    delay(50);
  }

  for (int y=70; y>=0; y-=5) {
    for (int x=0; x<67; x++) {
      strip.setPixelColor(x, 0, y, 0);
    }
    strip.show();
    delay(50);
  }

  for (int x=0; x<67; x++) {
    if (snake[x] == 1) {
      strip.setPixelColor(x, 0, 50, 0);
    }
  }
  strip.show();
}

/*
   Handles incoming data of a stream
   This can be any stream compliant class such as
   Serial, WiFiClient (TCP or UDP), File, etc.
*/

void handleStream(Stream * getter) {
  // Copy incoming data into msg cstring until newline is received
  snprintf(msg, MSG_SIZE, "%s", getter->readStringUntil('\n').c_str());
  // Some systems also send a carrige return, we want to strip it off
  for (size_t i = 0; i < MSG_SIZE; i++) {
    if (msg[i] == '\r') {
      msg[i] = '\0';
      break;
    }
  }
  // Handle given Msg
  const char * returnMsg = handleMsg(msg, msg);
  // Write something back to stream only if wanted
  if (returnMsg[0] != '\0') {
    getter->println(returnMsg);
  }
}


/*
   Handle a received topic and message, may return some answer.
*/

const char * handleMsg(const char * topic, const char * msg) {
  // strcmp returns zero on a match
  if (strcmp(topic, "1/cube/state") == 0 && strcmp(msg, "off") == 0) {
    puzzleIdle();
  } else if (strcmp(topic, "1/cube/state") == 0 && strcmp(msg, "on") == 0) {
    puzzleActive();
  } else if (strcmp(topic, "1/cube/state") == 0 && strcmp(msg, "solved") == 0) {
    puzzleSolved();
  } else if (strcmp(topic, "1/cube/mintime") == 0) {
    min_time = atoi(msg);
  } else if (strcmp(topic, "1/cube/maxtime") == 0) {
    max_time = atoi(msg);
  } else if (strcmp(topic, "1/panel/state") == 0 && strcmp(msg, "solved") == 0) {
    puzzlePanel();
  } else {
    return "Unknown command";
  }
  return "";
}


/*
   Puzzle changes to idle state
*/

void puzzleIdle() {
  puzzleState = idle;
  // Code to puzzle state idle...
  // Serial.println("Idle State");
  puzzleStateChanged();
  mqtt_publish("1/cube","status", "idle");
  mqtt_publish("game/puzzle1", "status", "idle");
}


/*
   Puzzle changes to active state
*/

void puzzleActive() {
  puzzleState = activ;
  // Code to set puzzle into active state ...
  // Serial.println("Active State");
  puzzleStateChanged();
  mqtt_publish("1/cube","status", "active");
  mqtt_publish("game/puzzle1", "status", "active");
}


/*
   Puzzle changes to solved state
*/

void puzzleSolved() {
  puzzleState = solved;
  // Code to set puzzle into solved state ...
  // Serial.println("Solved State");
  puzzleStateChanged();
  mqtt_publish("1/cube","status", "solved");
  mqtt_publish("game/puzzle1", "status", "solved");
  puzzleState = idle;
}


/*
   Panel puzzle changes to solved state
*/

void puzzlePanel() {
  puzzleState = panelsolved;
  // Code to set panel puzzle into solved state ...
  // Serial.println("Panel puzzle solved");
  puzzleStateChanged();
  mqtt_publish("1/panel","status", "panelsolved");
  mqtt_publish("game/puzzle1", "status", "panelsolved");
  puzzleState = idle;
}

/*
   Indicate to others that the state of the
   puzzle changed
*/

void puzzleStateChanged() {
  Serial.printf("Puzzle state changed to: %s\n", puzzleStateToStr(puzzleState));

  // Convert puzzlestate into JSON dict
  JsonObject obj = dict.to<JsonObject>();
  obj.clear();
  obj["state"] = puzzleStateToStr(puzzleState);
  // Serialize into msg cstring
  serializeJson(obj, msg);

  // If the TCP client is connected to the server, send JSON
  if (client.connected()) {
    client.println(msg);
  }
  // If the MQTT client is connected to broker, send JSON
  if (mqtt.connected()) {
    // Specify if msg should be retained
    bool retained = true;
    // Publish msg under given topic
    mqtt.publish("puzzle1/status", ""); //"test", retained);                                                                                                                                                                                                                                                                                                                                           , retained);
  }
}


/*
   Convert the ENUM to a string
*/

const char * puzzleStateToStr(PuzzleState puzzle) {
  switch (puzzle) {
    case activ: return "active";
    case idle: return "idle";
    case solved: return "solved";
    case panelsolved: return "panelsolved";
    default: return "Unknown state";
  }
}


/*
   Setup OTA Updates
*/

void setupOTA() {
  // Set name of this device (again)
  ArduinoOTA.setHostname(NAME);
  // Set a password to protect against uploads from other parties
  ArduinoOTA.setPassword(OTA_PWD);

  // CB function for when update starts
  ArduinoOTA.onStart([]() {
    Serial.println("Start updating");
  });
  // CB function for when update starts
  ArduinoOTA.onEnd([]() {
    Serial.println("Finished updating");
  });
  // CB function for progress
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Update at %u % \n", progress / (total / 100));
  });
  // CB function for when update was interrupted
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.println("Error updating");
    ESP.restart();
  });
  // Start OTA update service
  ArduinoOTA.begin();
}


/*
   Callback for MQTT messages on subscribed topics
*/

void mqttCallback(char* topic, byte* message, unsigned int length) {
  // Convert message as byte array to cstring
  unsigned int len = min((int)length, (int)(MSG_SIZE - 1));
  memcpy(&msg[0], message, len);
  msg[len] = '\0';
  // Print over serial
  Serial.printf("MQTT msg on topic: %s: %s\n", topic, &msg);

  // Try to deserialize the JSON msg
  DeserializationError error = deserializeJson(dict, msg);
  // Test if parsing succeeds.
  if (error) {
    Serial.println("error deserializing the msg");
  } else {
    Serial.println("Msg is of valid JSON format");
    // Try to extract new state
    const char* newState = dict["state"];
    // If key exists this is not NULL
    if (newState) {
      handleMsg(newState, topic);
    }
  }
}

void mqtt_publish(const char* topic, const char* method, const char* state){
  doc["method"] = method;
  doc["state"] = state;
  doc["data"] = 0;

  char JSONmessageBuffer[100];

  serializeJson(doc,JSONmessageBuffer, 100);
  mqtt.publish(topic, JSONmessageBuffer,true);
}
