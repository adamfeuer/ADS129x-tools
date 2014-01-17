/*
* 
*  Use the I2C bus with EEPROM 24LC64 
*  Sketch:    eeprom.pde
*  
*  Author: hkhijhe
*  Date: 01/10/2010
* 
*   
*/

#include <Wire.h> //I2C library
#define WirePort Wire

void i2c_eeprom_write_byte( int deviceaddress, unsigned int eeaddress, byte data ) {
  int rdata = data;
  WirePort.beginTransmission(deviceaddress);
  WirePort.write((int)(eeaddress >> 8)); // MSB
  WirePort.write((int)(eeaddress & 0xFF)); // LSB
  WirePort.write(rdata);
  WirePort.endTransmission();
}

// WARNING: address is a page address, 6-bit end will wrap around
// also, data can be maximum of about 30 bytes, because the Wire library has a buffer of 32 bytes
void i2c_eeprom_write_page( int deviceaddress, unsigned int eeaddresspage, byte* data, byte length ) {
  WirePort.beginTransmission(deviceaddress);
  WirePort.write((int)(eeaddresspage >> 8)); // MSB
  WirePort.write((int)(eeaddresspage & 0xFF)); // LSB
  byte c;
  for ( c = 0; c < length; c++)
    WirePort.write(data[c]);
  WirePort.endTransmission();
}

byte i2c_eeprom_read_byte( int deviceaddress, unsigned int eeaddress ) {
  byte rdata = 0xFF;
  WirePort.beginTransmission(deviceaddress);
  WirePort.write((int)(eeaddress >> 8)); // MSB
  WirePort.write((int)(eeaddress & 0xFF)); // LSB
  WirePort.endTransmission();
  WirePort.requestFrom(deviceaddress,1);
  if (WirePort.available()) rdata = WirePort.read();
  return rdata;
}

// maybe let's not read more than 30 or 32 bytes at a time!
void i2c_eeprom_read_buffer( int deviceaddress, unsigned int eeaddress, byte *buffer, int length ) {
  WirePort.beginTransmission(deviceaddress);
  WirePort.write((int)(eeaddress >> 8)); // MSB
  WirePort.write((int)(eeaddress & 0xFF)); // LSB
  WirePort.endTransmission();
  WirePort.requestFrom(deviceaddress,length);
  int c = 0;
  for ( c = 0; c < length; c++ )
    if (WirePort.available()) buffer[c] = WirePort.read();
}




void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting.");
  char somedata[] = "this is data from the eeprom"; // data to write
  WirePort.begin(); // initialise the connection
  i2c_eeprom_write_page(0x50, 0, (byte *)somedata, sizeof(somedata)); // write to EEPROM 

  delay(10); //add a small delay

  Serial.println("Memory written");
}

void loop() 
{
  int addr=0; //first address
  byte b = i2c_eeprom_read_byte(0x50, 0); // access the first address from the memory

  while (b!=0) 
  {
    Serial.print((char)b); //print content to serial port
    addr++; //increase address
    b = i2c_eeprom_read_byte(0x50, addr); //access an address from the memory
  }
  Serial.println(" ");
  delay(2000);

}
