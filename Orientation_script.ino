#include<Wire.h>
const int MPU_address= 104; // the one from uni has 40, the one from Kevin has 104
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;

void setup(){
  Wire.begin();
  Wire.beginTransmission(MPU_address);
  Wire.write(0x6B);
  Wire.write(0);
  Serial.begin(115200);
  byte error = Wire.endTransmission();
  if(error) {
    Serial.print("ERROR, when trying to connect to the device. Error: "); Serial.println(error);
  }

  Wire.beginTransmission(MPU_address);
  Wire.write(0x1C);
  Wire.write(0b00000000); // set sensitivity to +- 2g
  Wire.endTransmission();


  /*
  for(int address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address "); Serial.print(address);
    }
  }
  */
  Serial.println("Setup done");
}

void loop(){

  // read acc data
  Wire.beginTransmission(MPU_address);
  Wire.write(0x3B);
  Wire.endTransmission();
  Wire.requestFrom(MPU_address, 6);
  while(Wire.available() < 6);
  int accX_raw = Wire.read() << 8 | Wire.read();
  int accY_raw = Wire.read() << 8 | Wire.read();
  int accZ_raw = Wire.read() << 8 | Wire.read();

  float accX = accX_raw / 16384;
  float accY = accY_raw / 16384;
  float accZ = accZ_raw / 16384;

  float total = accX + accY + accZ;
  int x_per = (accX / total) * 100;
  int y_per = (accY / total) * 100;
  int z_per = (accZ / total) * 100;

  Serial.print(x_per); Serial.print("    ");
  Serial.print(y_per); Serial.print("    ");
  Serial.print(z_per); Serial.println("    ");
  
  /*
  Wire.beginTransmission(MPU_address);
  Wire.write(0x3B);
  Wire.endTransmission();
  Wire.requestFrom(MPU_address,12,true);
  AcX=Wire.read()<<8|Wire.read();
  AcY=Wire.read()<<8|Wire.read();
  AcZ=Wire.read()<<8|Wire.read();
  GyX=Wire.read()<<8|Wire.read();
  GyY=Wire.read()<<8|Wire.read();
  GyZ=Wire.read()<<8|Wire.read();
  
  float total_Acc = sqrt(AcX * AcX + AcY * AcY + AcZ * AcZ);
  
  Serial.print("Accelerometer: ");
  Serial.print("X = "); Serial.print(AcX);
  Serial.print(" | Y = "); Serial.print(AcY);
  Serial.print(" | Z = "); Serial.println(AcZ);
  
  Serial.print("Gyroscope: ");
  Serial.print("X = "); Serial.print(GyX);
  Serial.print(" | Y = "); Serial.print(GyY);
  Serial.print(" | Z = "); Serial.println(GyZ);

  Serial.print("total acceleration: "); Serial.println(total_Acc);

  Serial.println(" ");
  delay(333);
  */
}
