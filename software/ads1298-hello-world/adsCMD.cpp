
/* adsCMD.cpp
 * simple library to light an LED for a duration given in milliseconds
 */
#include "Arduino.h"   // use: Wprogram.h for Arduino versions prior to 1.0
#include "adsCMD.h"
#include "ads1298.h"
#include <SPI.h>  // include the SPI library:



void adc_send_command(int cmd)
{
	digitalWrite(IPIN_CS, LOW);
	SPI.transfer(cmd);
	delayMicroseconds(1);
	digitalWrite(IPIN_CS, HIGH);
}

void adc_wreg(int reg, int val)
{
  //see pages 40,43 of datasheet - 
	digitalWrite(IPIN_CS, LOW);
	SPI.transfer(ADS1298::WREG | reg);
	SPI.transfer(0);	// number of registers to be read/written – 1
	SPI.transfer(val);
	delayMicroseconds(1);
	digitalWrite(IPIN_CS, HIGH);
}

int adc_rreg(int reg){
  int out = 0;
  digitalWrite(IPIN_CS, LOW);
  SPI.transfer(ADS1298::RREG | reg);
  delayMicroseconds(5);
  SPI.transfer(0);	// number of registers to be read/written – 1
  delayMicroseconds(5);
  out = SPI.transfer(0);
  	delayMicroseconds(1);
  digitalWrite(IPIN_CS, HIGH);
  return(out);
}
