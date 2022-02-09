#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>


SoftwareSerial Kbus(D3, D0);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(100, D5, NEO_GRB + NEO_KHZ800);

uint8_t led_count[8] = {8, 10, 12, 10, 7, 14, 14, 8};
uint8_t led_sum = 0, led_sum_max = 0;
uint8_t panels = 0x81;

void checkKbusMessage(char *msg);
void checkKbusDevices(void);

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

  // Set new Panel and check if next panel exists
  if(led == led_sum) {
    if(panels&(1<<curr_panel)) {
      led_sum += led_count[curr_panel];
      Kbus.print('X');
      Kbus.print((char)('1'+curr_panel));
      Kbus.println('?');
      
    }
    curr_panel++;
  }
  
  if(led == 8) { // When reaching 8 we should know which panels are there
    for(uint8_t i=1; i<8; i++) {
      if(panels&(1<<i)) led_sum += led_count[i];
      else break;
    }
  }
  int8_t n = 0;

  // Light has reached the end
  if(led == led_sum_max) {
    
  // Light has Crashed
    
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
  // Light is still running
  } else {
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
  if(led == 0) {
    //solved = 1 - solved;
    panels = 0x81;
    led_sum = 0;
    curr_panel = 0;
    Serial.println(panels, HEX);
    //checkKbusDevices();
    delay(500);
  }
}

void checkKbusMessage(char *msg) {
  if(*msg == 'X') { // Device available
    if(*(msg+2) == '!' && *(msg+1) >= '0' && *(msg+1) <= '9') panels |= (1<<(*(msg+1)-'0'));
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
