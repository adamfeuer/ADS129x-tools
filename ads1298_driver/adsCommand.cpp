
/* adsCommand.cpp
 *
 * send and receive commands from TI ADS129x chips. 
 */
#include "Arduino.h"   // use: Wprogram.h for Arduino versions prior to 1.0
#include "adsCommand.h"
#include "ads1298.h"
#include "SpiDma.h"

/*void wait_for_drdy(int interval)
{
	int i = 0;
	while (digitalRead(IPIN_DRDY) == HIGH) {
		if (i < interval) {
			continue;
		}
		i = 0;
	}
}*/

void adc_send_command(int cmd) {
   digitalWrite(PIN_CS, LOW);
   spiSend(cmd);
   delayMicroseconds(1);
   digitalWrite(PIN_CS, HIGH);
}

void adc_wreg(int reg, int val) {
   //see pages 40,43 of datasheet - 
   digitalWrite(PIN_CS, LOW);
   spiSend(ADS1298::WREG | reg);
   delayMicroseconds(2);
   spiSend(0);	// number of registers to be read/written – 1
   delayMicroseconds(2);
   spiSend(val);
   delayMicroseconds(1);
   digitalWrite(PIN_CS, HIGH);
}

int adc_rreg(int reg){
   uint8_t out = 0;
   digitalWrite(PIN_CS, LOW);
   spiSend(ADS1298::RREG | reg);
   delayMicroseconds(2);
   spiSend(0);	// number of registers to be read/written – 1
   delayMicroseconds(2);
   out = spiRec();
   delayMicroseconds(1);
   digitalWrite(PIN_CS, HIGH);
   return((int)out);
}
