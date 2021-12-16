#include<Wire.h>
const uint8_t MPU_address= 104; // the one from uni has 40, the one from Kevin has 104

// Change the code to your liking, but make sure, that the length of the code is 6 and each number from 1 to 6 is contained in the code exactly once
const int code[6] = {2, 4, 3, 6, 5, 1};
int current_pos = 0;

void led_animation(int current_pose);
void led_side_solved_animation(int side_nr);
void led_side_unsolved_animation(int side_nr);
void reset_led_animation();
void led_win_animation();

uint8_t upside = 10;
uint8_t upside_old, upside_duration;
uint8_t upper_side = 10;
uint8_t upper_side_before = 10;
unsigned long last_upper_side_change = 0;
void setup(){
  Wire.begin();
  Serial.begin(115200);
  
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

  Serial.println("Setup done");
}

void loop(){
  upper_side_before = upper_side;
  upper_side = get_upper_cube_side(upside, upside_old, upside_duration);
  if(upper_side_before != upper_side){
    last_upper_side_change = millis();
  }
  

  if(current_pos == 6){
    Serial.println("The code has been entered correctly");
    led_win_animation();
  }else{
    // code recognition
    // depends on the algorithm to change the upside of the cube not instantly
    if(upper_side == code[current_pos]){
      current_pos += 1;
    }else if (current_pos >= 1 ){
      if(code[current_pos - 1] == upper_side){
        // nothing to do here
      } else if (millis() - last_upper_side_change > 2000) {
        current_pos = 0;
        reset_led_animation();
      }
    }
    led_animation(current_pos);
  }
  
    
  // Just some delay to not spam the terminal
  delay(500);
}

// instead of calling an animation method it could also just send a message or something

void led_animation(int current_pos){
  for(int i = 0; i < current_pos; i++){
    led_side_solved_animation(code[i]);
  }

  for(int i = current_pos; i < 6; i++){
    led_side_unsolved_animation(code[i]);
  }
}

void led_side_solved_animation(int side_nr){
  Serial.print("the following side is green: ");
  Serial.println(side_nr);
}

void led_side_unsolved_animation(int side_nr){
  Serial.print("the following side is blue: ");
  Serial.println(side_nr);
}

void reset_led_animation() {
  Serial.println("the cube is doing the -you-failed- animation");
}

void led_win_animation(){
  Serial.println("the cube is doing the -you-won- animation");
}



int get_upper_cube_side(uint8_t upside, uint8_t upside_old, uint8_t upside_duration) {
  // read acc data
  Wire.beginTransmission(MPU_address);
  Wire.write(0x3B);
  Wire.endTransmission();
  Wire.requestFrom(MPU_address, 6);
  while(Wire.available() < 6);
  int16_t accX_raw = Wire.read() << 8 | Wire.read();
  int16_t accY_raw = Wire.read() << 8 | Wire.read();
  int16_t accZ_raw = Wire.read() << 8 | Wire.read();

  // the three lowest bits are which axis is the highest
  // the bits 5 to 7 are if the axis is negative or positive
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

  // printing raw data
  /*Serial.print(accX_raw); Serial.print("\t");
  Serial.print(accY_raw); Serial.print("\t");
  Serial.print(accZ_raw); Serial.print("\t");
  Serial.print(accDir&0x08); Serial.println("\t");*/
  switch(accDir&0x07) { // ignore if axes are negative or positive
    case 0x04:
      upside = (accDir&0x40) ? 3 : 4;
      break;
    case 0x02:
      upside = (accDir&0x20) ? 2 : 5;
      break;
    case 0x01:
      upside = (accDir&0x10) ? 6 : 1;
      break;
    default:
      if(accDir&0x07) upside = 10;
      break;
  }

  if(upside != upside_old) {
    upside_duration = 0;
    upside_old = upside;
  } else if(upside_duration < 50-1) {
    upside_duration++;
  } else if(upside_duration == 50-1) {
    upside_duration++;
  }
  Serial.print("Cube is now on side ");
  Serial.println(upside);
  return upside;
}
