#include <Adafruit_NeoPixel.h>
#include<Wire.h>
//#include <ESP8266WiFi.h>

//char *ssid = "BV9900Pro";
char *ssid = "Mi 11 Lite";
char *password = "ff799896820f";
const uint8_t MPU_address= 104; // the one from uni has 40, the one from Kevin has 104

Adafruit_NeoPixel strip = Adafruit_NeoPixel(67, D5, NEO_GRB + NEO_KHZ800);

uint8_t upside, upside_old = -1, upside_duration;

int snake[70] = {0};
int reset = 21;

uint8_t sequence[6] = {2,6,4,3,5,1};
uint8_t seq_cntr = 0;
uint8_t moved = 0;

// ______________________________________________________________________
void setup() {
  Wire.begin();
  Serial.begin(115200);

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
  Serial.println("Setup done");
}


// ______________________________________________________________________
int16_t accDefault[3]= {0};
int16_t moveThres = 8196;
void loop () {
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
  } else if(upside_duration < 20-1) {
    upside_duration++;
  } else if(upside_duration == 20-1) {
    upside_duration++;
    Serial.print("Cube is now on side ");
    Serial.println(upside);

    if(seq_cntr < 6 && moved == 1) {
      if(upside == sequence[seq_cntr]) {
        right(upside-1);
        seq_cntr++;
        if(seq_cntr == 6) Serial.println("Cube has been solved!");
      } else {
        wrong();
        seq_cntr = 0;
      }
    }
    
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
