#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include "final_code_panel_header.h"

SoftwareSerial Kbus(D1, D0);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(100, D5, NEO_GRB + NEO_KHZ800);

uint8_t led_count[8] = {8, 10, 12, 10, 7, 14, 14, 8};
uint8_t led_sum = 0, led_sum_max = 0;
uint8_t panels = 0x81;
uint8_t analog[6] = {0};

uint8_t respTry = 0;
uint8_t respPanel = 0;
void checkKbusMessage(char *msg);
void checkKbusDevices(void);
void checkKbusDevice(uint8_t panelNr);

StaticJsonDocument<300> doc;

// ______________________________________________________________________
void setup() {
  Serial.begin(115200);
  Kbus.begin(4800);
  
  strip.begin();
  
  for (int x=0; x<100; x++) {
    strip.setPixelColor(x, 0, 0, 0);
  }

  strip.show();
  
  for(uint8_t i=0; i<sizeof(led_count); i++) {
    led_sum_max += led_count[i];
  }
  Kbus.println("D1=__");
  delay(50);
  Kbus.println("D2=__");
  delay(50);
  Kbus.println("D3=__");
  delay(50);
  Kbus.println("D4=__");
  delay(50);
  Kbus.println("D5=__");
  delay(50);
  Kbus.println("D6=__");
  delay(50);
  
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
    Serial.print(".");
    strip.setPixelColor(0, 0, 0, 50);
    strip.show();
    delay(250);
    strip.setPixelColor(0,0,0,0);
    strip.show();
    delay(250);
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

  
  delay(2000);
}


// ______________________________________________________________________
int8_t led = 0;
uint8_t curr_panel = 0;
char KbusBuffer[20] = {0};
uint8_t KbusPtr = 0;
void loop () {

  // MQTT Code
  // Needs to be called to check for external update request
  ArduinoOTA.handle();
  // Checks for MQTT subscriptions
  mqtt.loop();

  // Change state from idle to active afterwards cycle between
  if (puzzleState != puzzleStatePrev){
    // solved and active
    if (puzzleState == idle) {
      puzzleIdle();
      puzzleStatePrev = idle;
    } else if (puzzleState == activ) {
      puzzleActive();
      puzzleStatePrev = activ;
    } else if (puzzleState == solved){
      puzzleSolved();
      puzzleStatePrev = solved;
    }
  }
  // If at least one byte is available over serial
  if (Serial.available()) {
    handleStream(&Serial);
  }
  
  // If at least one byte is available over TCP
  if (client.available()) {
    handleStream(&client);
  }

  
  if (puzzleState == activ) {
  
    while(Kbus.available()) {
      KbusBuffer[KbusPtr] = Kbus.read();
      Serial.print(KbusBuffer[KbusPtr]);
      if(KbusBuffer[KbusPtr] == '\n') {
        checkKbusMessage(KbusBuffer);
        while(KbusPtr>0) {
          KbusBuffer[KbusPtr] = 0;
          KbusPtr--;
        }
      } else {
        KbusPtr++;
      }
    }
    
    if(led == 0) {
      panels = 0x81;
      led_sum = 0;
      curr_panel = 0;
      delay(500);
    }
  
    // Set new Panel and check if next panel exists
    if(led == led_sum) {
      if(panels&(1<<curr_panel)) {
        // Panel Exists, extend light and ask for next panel
        led_sum += led_count[curr_panel];
        curr_panel++;
        respTry = 4; // Up to 4 Retries of communication.
        checkKbusDevice(curr_panel);
      } else {
        // Panel does not exist, FAIL HERE!
      }
    }
    
    /*if(led == 8) { // When reaching 8 we should know which panels are there
      for(uint8_t i=1; i<8; i++) {
        if(panels&(1<<i)) led_sum += led_count[i];
        else break;
      }
    }*/
    int8_t n = 0;
  
    // Light has reached the end 
    if(led > led_sum_max) {
      // Puzzle finalized, do not do shit.
    } else if(led == led_sum_max) {
      for (int x=0; x<100; x++) {
        strip.setPixelColor(x, 0, 0x40, 0);
      }
      strip.show();
      Kbus.println("D1=__");
      delay(75);
      Kbus.println("D2=_1.");
      delay(75);
      Kbus.println("D3=_9.");
      delay(75);
      Kbus.println("D4=_5.");
      delay(75);
      Kbus.println("D5=_6.");
      delay(75);
      Kbus.println("D6=__");
      delay(75);
      led++;
      puzzleState = solved;
    } else if(led >= led_sum) {
      uint8_t col = 0x10;
      while(col) {
        col >>= 1;
        for (int x=0; x<led_sum; x++) {
          strip.setPixelColor(x, col, 0, 0);
        }
        led = 0;
        strip.show();
        delay(333);
      }
    }else { 
      // Light is still running
      for (int x=0; x<led_sum; x++) {
        strip.setPixelColor(x, 0);
        //else strip.setPixelColor(x, 0, 0x0f, 0);
      }
      while(n <= led && n < 10) {
        if(led-n < 100) strip.setPixelColor(led-n, (0xff>>n), 0, (0xff>>n));
        n++;
      }
      led = (led+1)%100;
      strip.show();
    }
    
    delay(50);
  } else {
    //Serial.println("Panel Puzzle is inactive");
    delay(1000);
  }
}

void checkKbusMessage(char *msg) {
  uint16_t tmp = 0;
  char thisChar = 0;
  if(*msg == 'X') { // Device available
    if(*(msg+2) == '!' && *(msg+1) >= '0' && *(msg+1) <= '9') panels |= (1<<(*(msg+1)-'0'));
  } else if(*msg == 'A') {
    if(*(msg+1) > '6' || *(msg+1) < '1') return;
    if(*(msg+2) == '=') {
      for(uint8_t i=0; i<3; i++) {
        tmp <<= 4;
        thisChar = *(msg+4+i);
        if(thisChar <= '9' && thisChar >= '0') tmp += thisChar - '0';
        if(thisChar <= 'F' && thisChar >= 'A') tmp += thisChar - 'A' + 10;
      }
      tmp >>= 6;
      analog[*(msg+1)-'1'] = (uint8_t)tmp;
      Serial.print("Panel ");
      Serial.print(*(msg+1));
      Serial.print(", Value: ");
      Serial.println(analog[*(msg+1)-'1']);
      switch(*(msg+1)) { // Check if the module is in the correct place.
        case '1':
          if(tmp >= 0 && tmp <= 1) panels |= (1<<(*(msg+1)-'0'));
          break;
        case '2':
          if(tmp >= 2 && tmp <= 4) panels |= (1<<(*(msg+1)-'0'));
          break;
        case '3':
          if(tmp >= 5 && tmp <= 7) panels |= (1<<(*(msg+1)-'0'));
          break;
        case '4':
          if(tmp >= 8 && tmp <= 10) panels |= (1<<(*(msg+1)-'0'));
          break;
        case '5':
          if(tmp >= 11 && tmp <= 13) panels |= (1<<(*(msg+1)-'0'));
          break;
        case '6':
          if(tmp >= 14 && tmp <= 16) panels |= (1<<(*(msg+1)-'0'));
          break;
          
      }
    }
  } else {
    // We got Blödsinn, retry if neccessary
    if(respTry > 0) {
      respTry--;
      checkKbusDevice(respPanel);
    }
  }
}


void checkKbusDevices(void) {
  panels = 0x81;
  Kbus.println("X1?");
  delay(100);
  Kbus.println("X2?");
  delay(100);
  Kbus.println("X3?");
  delay(100);
  Kbus.println("X4?");
  delay(100);
  Kbus.println("X5?");
  delay(100);
  Kbus.println("X6?");
  delay(100);
}

void checkKbusDevice(uint8_t panelNr) {
  if(panelNr < 1 || panelNr > 6) return;
  respPanel = panelNr;
  panels &= ~(1<<panelNr);
  Kbus.print("A");
  Kbus.print(panelNr);
  Kbus.println("?");
}

void handleStream(Stream * getter) {
  Serial.println("handleStrea");
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
  const char * returnMsg = handleMsg(msg, msg, "", "");
  // Write something back to stream only if wanted
  if (returnMsg[0] != '\0') {
    getter->println(returnMsg);
  }
}


/*
   Handle a received topic and message, may return some answer.
*/

const char * handleMsg(const char * state_msg, const char * topic, const char * data, const char * method_msg) {
  // strcmp returns zero on a match
  /*
  if (strcmp(topic, "1/panel/state") == 0 && strcmp(msg, "deactivate") == 0) {
    puzzleIdle();
  } else if (strcmp(topic, "1/panel/state") == 0 && strcmp(msg, "activate") == 0) {
    puzzleActive();
  } else if (strcmp(topic, "1/panel/state") == 0 && strcmp(msg, "solved_panel") == 0) {
    puzzleSolved();
  } else if (strcmp(topic, "1/panel/state") == 0 && strcmp(msg, "reset_panel") == 0) {
    puzzleIdle();
    puzzleState = idle;
  } else {
    return "Unknown command";
  }
  return "";
  */
  if (strcmp(topic, "1/panel/state") == 0 && strcmp(state_msg,"on") == 0 && strcmp(method_msg, "trigger") == 0 && puzzleStatePrev != activ) {
    puzzleActive();
  } else if (strcmp(topic, "1/panel/state")  == 0 && strcmp(state_msg, "off") == 0 && strcmp(method_msg, "trigger") == 0  && strcmp(data, "skipped") == 0 && puzzleStatePrev != solved){
    puzzleSolved();
  } else if (strcmp(topic, "1/panel/state")  == 0 && strcmp(state_msg, "off") == 0 && strcmp(method_msg, "trigger") == 0 && puzzleStatePrev != idle) {
    puzzleIdle();
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
  mqtt_publish("1/panel/state","status", "off");
  //mqtt_publish("game/puzzle1", "status", "off");
  puzzleState = idle;
  led = 0;
  led_sum = 0;
  led_sum_max = 0;
  panels = 0x81;
  for (int x=0; x<100; x++) {
    strip.setPixelColor(x, 0, 0, 0);
  }
  strip.show();

  for(uint8_t i=0; i<sizeof(led_count); i++) {
    led_sum_max += led_count[i];
  }
  Kbus.println("D1=__");
  delay(75);
  Kbus.println("D2=__");
  delay(75);
  Kbus.println("D3=__");
  delay(75);
  Kbus.println("D4=__");
  delay(75);
  Kbus.println("D5=__");
  delay(75);
  Kbus.println("D6=__");
  delay(75);
}


/*
   Puzzle changes to active state
*/

void puzzleActive() {
  puzzleState = activ;
  // Code to set puzzle into active state ...
  // Serial.println("Active State");
  puzzleStateChanged();
  mqtt_publish("1/panel/state","status", "on");
  //mqtt_publish("game/puzzle1", "status", "on");
}


/*
   Puzzle changes to solved state
*/

void puzzleSolved() {
  puzzleState = solved;
  // Code to set puzzle into solved state ...
  // Serial.println("Solved State");
  puzzleStateChanged();
  mqtt_publish("1/panel/state","status", "solved");
  mqtt_publish("game/puzzle1", "Solved", "");
  //mqtt_publish("1/panel/state", "status", "activate");
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
    const char* method_msg = dict["method"];
    const char* newState = dict["state"];
    const char* data_msg = dict["data"];
    if (!data_msg) {
      data_msg = "None";
    }
    if (!method_msg) {
      method_msg = "None";
    }
    // If key exists this is not NULL
    if (newState) {
      handleMsg(newState, topic, data_msg, method_msg);
    }
  }
}

void mqtt_publish(const char* topic, const char* method_, const char* state){
  doc["method"] = method_;
  doc["state"] = state;
  doc["data"] = 0;

  char JSONmessageBuffer[100];

  serializeJson(doc,JSONmessageBuffer, 100);
  mqtt.publish(topic, JSONmessageBuffer,true);
}
