// Minimal sketch for connection to ADS129n family. Load this script and open Tools/SerialMonitor. 
// You should see text like this
//  Device Type (ID Control Register): 62 Channels: 8
// If you see "Channels: 0" then check your wiring

#include "ads1298.h"
#include "adsCMD.h"
#include <SPI.h>  // include the SPI library:
int gMaxChan = 0; //maximum number of channels supported by ads129n = 4,6,8
int gIDval = 0; //Device ID : lower 5 bits of  ID Control Register 

int activeSerialPort = 0; //data will be sent to serial port that last sent commands. E.G. bluetooth or USB port
const int kPIN_LED = 13;//pin with in-built light - typically 13, 11 for Teensy 2.0. 

#if defined(__SAM3X8E__)
  #define isDUE  //Detect Arduino Due
  //#define WiredSerial SerialUSB //Use Due's Native port
  #define WiredSerial Serial
#else  
  #define WiredSerial Serial
#endif

void setup(){
  using namespace ADS1298;
  //prepare pins to be outputs or inputs
  //pinMode(PIN_SCLK, OUTPUT); //optional - SPI library will do this for us
  //pinMode(PIN_DIN, OUTPUT); //optional - SPI library will do this for us
  //pinMode(PIN_DOUT, INPUT); //optional - SPI library will do this for us
  pinMode(IPIN_CS, OUTPUT);
  pinMode(PIN_START, OUTPUT);
  pinMode(IPIN_DRDY, INPUT);
  pinMode(PIN_CLKSEL, OUTPUT);//*optional
  pinMode(IPIN_RESET, OUTPUT);//*optional
  pinMode(IPIN_PWDN, OUTPUT);//*optional
  //start Serial Peripheral Interface
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  #ifndef isDUE
  SPI.setClockDivider(SPI_CLOCK_DIV4); //http://forum.pjrc.com/threads/1156-Teensy-3-SPI-Basic-Clock-Questions
  #endif
  SPI.setDataMode(SPI_MODE1);
  //Start ADS1298
  delay(500); //wait for the ads129n to be ready - it can take a while to charge caps
  adc_send_command(SDATAC); // Send SDATAC Command (Stop Read Data Continuously mode)
  delay(10);
  // Determine model number and number of channels available
  gIDval = adc_rreg(ID); //lower 5 bits of register 0 reveal chip type
  switch (gIDval & B00011111 ) { //least significant bits reports channels
          case  B10000: //16
            gMaxChan = 4; //ads1294
            break;
          case B10001: //17
            gMaxChan = 6; //ads1296
            break; 
          case B10010: //18
            gMaxChan = 8; //ads1298
            break;
          case B11110: //30
            gMaxChan = 8; //ads1299
            break;
          default: 
            gMaxChan = 0;
  }
  //start serial port
  WiredSerial.begin(115200); //use native port on Due
  while (WiredSerial.read() >= 0) {} //http://forum.arduino.cc/index.php?topic=134847.0
  //while (!WiredSerial) ; //required by Leonardo http://arduino.cc/en/Serial/IfSerial (ads129n requires 3.3v signals, Leonardo is 5v)
  delay(200);  // Catch Due reset problem
  pinMode(kPIN_LED, OUTPUT); 
}

void loop()
{
  WiredSerial.print("Device Type (ID Control Register): "); WiredSerial.print(gIDval); WiredSerial.print(" Channels: "); WiredSerial.println(gMaxChan);
  digitalWrite(kPIN_LED, HIGH);   // turn the LED on (HIGH is the voltage level)
  if (gMaxChan > 0)
    delay(500); //long pause if OK
  else
    delay(50); //rapid blink if error
  digitalWrite(kPIN_LED, LOW);    // turn the LED off by making the voltage LOW
  delay(500); 
}
