#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>

SoftwareSerial Kbus(D3, D0);
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

// ______________________________________________________________________
void setup() {
  Serial.begin(115200);
  Kbus.begin(4800);
  
  strip.begin();
  for (int x=0; x<100; x++) {
    strip.setPixelColor(x, 0, 0x40, 0);
  }
  strip.show();
  for (int x=0; x<100; x++) {
    strip.setPixelColor(x, 0, 0, 0);
  }

  for(uint8_t i=0; i<sizeof(led_count); i++) {
    led_sum_max += led_count[i];
  }
  Kbus.println("D1=__");
  Kbus.println("D2=__");
  Kbus.println("D3=__");
  Kbus.println("D4=__");
  Kbus.println("D5=__");
  Kbus.println("D6=__");
  
  Serial.println("Setup done");
  delay(2000);
}


// ______________________________________________________________________
int8_t led = 0;
uint8_t curr_panel = 0;
char KbusBuffer[20] = {0};
uint8_t KbusPtr = 0;
void loop () {
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
    Kbus.println("D2=_1.");
    Kbus.println("D3=_9.");
    Kbus.println("D4=_5.");
    Kbus.println("D5=_6.");
    Kbus.println("D6=__");
    led++;
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
      //if(led-n < 100 && solved == 1) strip.setPixelColor(led-n, (0xff>>n), 0x0f, (0xff>>n));
      n++;
    }
    led = (led+1)%100;
    strip.show();
  }
  
  delay(50);
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
