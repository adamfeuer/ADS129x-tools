// text-mode driver for ADS1298 
// for Arduino Due
//
// Copright 2013 Adam Feuer
//
// Based on code by Chris Rorden (ADS1298),
// Steven Cogswell and Stefan Rado (SerialCommand).
//

#include <stdlib.h>
#include <SPI.h> 
#include "SerialCommand.h"
#include "ads1298.h"
#include "adsCommand.h"

#define VERSION "ADS1298 driver v0.1"

#define arduinoLed 13   // Arduino LED on board
#define BAUD_RATE  115200 // WiredSerial ignores this and uses the maximum rate
#define txActiveChannelsOnly //reduce bandwidth: only send data for active data channels
#define WiredSerial SerialUSB //use Due's Native port

int gMaxChan = 0;
int gNumActiveChan = 0;
int activeSerialPort = 0; //data will be sent to serial port that last sent commands. E.G. bluetooth or USB port
boolean gActiveChan [9]; // reports whether channels 1..9 are active
const int kPIN_LED = 13;//pin with light - typically 13. Only flashes if error status. n.b. on Teensy3, pin 13 is ALSO spi clock!
boolean isRDATAC = false;

char hexDigits[] = "0123456789ABCDEF";
unsigned char serialBytes[200];
char sampleDigits[400];

SerialCommand serialCommand;  

void setup() {  
  pinMode(arduinoLed,OUTPUT);      // Configure the onboard LED for output
  digitalWrite(arduinoLed,LOW);    // default to LED off
  arduinoSetup();
  adsSetup();

  WiredSerial.begin(115200); 
  WiredSerial.println("Ready"); 

  // Setup callbacks for SerialCommand commands 
  serialCommand.addCommand("version",version);     // Echos the driver version number
  serialCommand.addCommand("ledon",ledOn);         // Turns Due onboad LED on
  serialCommand.addCommand("ledoff", ledOff);      // Turns Due onboard LED off
  serialCommand.addCommand("rreg", readRegister);  // Read ADS129x register, argument in hex, print contents in hex
  serialCommand.addCommand("wreg", writeRegister); // Write ADS129x register, arguments in hex
  serialCommand.addCommand("rdata", rdata);        // Read one sample of data from each channel
  serialCommand.addCommand("rdatac", rdatac);      // Enter read data continuous mode
  serialCommand.addCommand("sdatac", sdatac);      // Stop read data continuous mode
  serialCommand.setDefaultHandler(unrecognized);   // Handler for command that isn't matched 

}

void loop() {  
   serialCommand.readSerial();
   sendSamples();
}

long hexToLong(char *digits) {
   using namespace std;
   char *error;
   long n = strtol(digits, &error, 16);
   if ( *error != 0 ) { 
      return -1; // error
   } else {
      return n;
   }
}

void outputHexByte(int value) {
  int clipped = value & 0xff;
  char charValue[3];
  sprintf(charValue, "%02X", clipped);
  WiredSerial.print(charValue);
}

void ledOn() {
  WiredSerial.println("200 Ok");
  WiredSerial.println("LED on"); 
  digitalWrite(arduinoLed,HIGH);  
}

void ledOff() {
  WiredSerial.println("200 Ok");
  WiredSerial.println("LED off"); 
  digitalWrite(arduinoLed,LOW);
}

void version() {
  WiredSerial.println("200 Ok");
  WiredSerial.println(VERSION);
  digitalWrite(arduinoLed,LOW);
}

void readRegister() {
  using namespace ADS1298; 
  char *arg1; 
  arg1 = serialCommand.next();   
  if (arg1 != NULL) {
      long registerNumber = hexToLong(arg1);
      if (registerNumber >= 0) {
        int result = adc_rreg(registerNumber);
        WiredSerial.print("200 Ok");
        WiredSerial.print(" (Read Register "); 
        WiredSerial.print(registerNumber); 
        WiredSerial.print(") "); 
        WiredSerial.println();               
        outputHexByte(result);      
        WiredSerial.println();      

      } else {
         WiredSerial.println("402 Error: expected hexidecimal digits."); 
      }
  } else {
    WiredSerial.println("403 Error: register argument missing."); 
  }
}

void writeRegister() {
  char *arg1, *arg2; 
  arg1 = serialCommand.next();   
  arg2 = serialCommand.next();  
  if (arg1 != NULL) {
    if (arg2 != NULL) { 
      long registerNumber = hexToLong(arg1);
      long registerValue = hexToLong(arg2);
      if (registerNumber >= 0 && registerValue > 0) {
        adc_wreg(registerNumber, registerValue);        
        WiredSerial.print("200 Ok"); 
        WiredSerial.print(" (Write Register "); 
        WiredSerial.print(registerNumber); 
        WiredSerial.print(" "); 
        WiredSerial.print(registerValue); 
        WiredSerial.print(") "); 
        WiredSerial.println();
                
      } else {
         WiredSerial.println("402 Error: expected hexidecimal digits."); 
      }
    } else {
      WiredSerial.println("404 Error: value argument missing."); 
    }
  } else {
    WiredSerial.println("403 Error: register argument missing."); 
  }
}

void rdata() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok ");
  adc_send_command(RDATA);
  while (digitalRead(IPIN_DRDY) == HIGH);
  sendSample();
}

void rdatac() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok");
  WiredSerial.println("RDATAC mode on."); 
  detectActiveChannels();
  if (gNumActiveChan > 0) { 
      isRDATAC = true;
      adc_send_command(RDATAC);
  }
}

void sdatac() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok");
  WiredSerial.println("RDATAC mode off."); 
  isRDATAC= false;
  adc_send_command(SDATAC);
}

// This gets set as the default handler, and gets called when no other command matches. 
void unrecognized(const char *command) {
  WiredSerial.println("Unrecognized command."); 
}


void detectActiveChannels() {  //set device into RDATAC (continous) mode -it will stream data
  if ((isRDATAC) ||  (gMaxChan < 1)) return; //we can not read registers when in RDATAC mode
  //Serial.println("Detect active channels: ");
  using namespace ADS1298; 
  gNumActiveChan = 0;
  for (int i = 1; i <= gMaxChan; i++) {
    delayMicroseconds(1); 
     int chSet = adc_rreg(CHnSET + i);
     gActiveChan[i] = ((chSet & 7) != SHORTED);
     if ( (chSet & 7) != SHORTED) gNumActiveChan ++;   
     //WiredSerial.print("Active channels: ");
     //WiredSerial.println(gNumActiveChan);
  }
  
}

//#define testSignal //use this to determine if your software is accurately measuring full range 24-bit signed data -8388608..8388607
#ifdef testSignal
  int testInc = 1;
  int testPeriod = 100;
  byte testMSB, testLSB; 
#endif 

void sendSamples(void) { 
    if ((!isRDATAC) || (gNumActiveChan < 1) )  return;
    if (digitalRead(IPIN_DRDY) == HIGH) return; 
    sendSample();
}

void sendSample(void) { 
    digitalWrite(IPIN_CS, LOW);
    register int numSerialBytes = (3 * (gMaxChan+1)); //24-bits header plus 24-bits per channel
    register unsigned int count = 0;
    for (register unsigned int i = 0; i < numSerialBytes; i++) { 
      serialBytes[i] =SPI.transfer(0); 
      register unsigned char lowNybble = serialBytes[i] & 0x0f;
      register unsigned char highNybble = (serialBytes[i] >> 4) & 0x0f;
      sampleDigits[count++]=hexDigits[lowNybble];
      sampleDigits[count++]=hexDigits[highNybble];
    }
    delayMicroseconds(1); 
    digitalWrite(IPIN_CS, HIGH);
    sampleDigits[count]=0;
    WiredSerial.println(sampleDigits);
}

void adsSetup() { //default settings for ADS1298 and compatible chips
        using namespace ADS1298;
        // All GPIO set to output 0x0000: (floating CMOS inputs can flicker on and off, creating noise)
	adc_wreg(GPIO, 0);
        adc_wreg(CONFIG3,PD_REFBUF | CONFIG3_const);
	//FOR RLD: Power up the internal reference and wait for it to settle
        // adc_wreg(CONFIG3, RLDREF_INT | PD_RLD | PD_REFBUF | VREF_4V | CONFIG3_const);
	// delay(150);
	// adc_wreg(RLD_SENSP, 0x01);	// only use channel IN1P and IN1N
	// adc_wreg(RLD_SENSN, 0x01);	// for the RLD Measurement
        adc_wreg(CONFIG1,HIGH_RES_1k_SPS);
        adc_wreg(CONFIG2, INT_TEST);	// generate internal test signals
	// Set the first two channels to input signal
	for (int i = 1; i <= 1; ++i) {
		adc_wreg(CHnSET + i, ELECTRODE_INPUT | GAIN_1X); //report this channel with x12 gain
		//adc_wreg(CHnSET + i, ELECTRODE_INPUT | GAIN_12X); //report this channel with x12 gain
		//adc_wreg(CHnSET + i, TEST_SIGNAL | GAIN_12X); //create square wave
                //adc_wreg(CHnSET + i,SHORTED); //disable this channel
	}
	for (int i = 2; i <= 2; ++i) {
		adc_wreg(CHnSET + i, TEST_SIGNAL | GAIN_12X); //create square wave
	}
	for (int i = 3; i <= 8; ++i) {
                adc_wreg(CHnSET + i,SHORTED); //disable this channel
	}
	digitalWrite(PIN_START, HIGH);  
}

void arduinoSetup(){
  Serial.begin(BAUD_RATE); // for debugging
  pinMode(kPIN_LED, OUTPUT);
  using namespace ADS1298;
  //prepare pins to be outputs or inputs
  //pinMode(PIN_SCLK, OUTPUT); //optional - SPI library will do this for us
  //pinMode(PIN_DIN, OUTPUT); //optional - SPI library will do this for us
  //pinMode(PIN_DOUT, INPUT); //optional - SPI library will do this for us
  pinMode(IPIN_CS, OUTPUT);
  pinMode(PIN_START, OUTPUT);
  pinMode(IPIN_DRDY, INPUT);
  pinMode(PIN_CLKSEL, OUTPUT);// *optional
  pinMode(IPIN_RESET, OUTPUT);// *optional
  //pinMode(IPIN_PWDN, OUTPUT);// *optional
  digitalWrite(PIN_CLKSEL, HIGH); // internal clock
  //start Serial Peripheral Interface
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE1);
  //Start ADS1298
  delay(500); //wait for the ads129n to be ready - it can take a while to charge caps
  digitalWrite(PIN_CLKSEL, HIGH);// *optional
  delay(1);digitalWrite(IPIN_PWDN, HIGH);// *optional - turn off power down mode
  digitalWrite(IPIN_RESET, HIGH);delay(100);// *optional
  digitalWrite(IPIN_RESET, LOW);delay(1);// *optional
  digitalWrite(IPIN_RESET, HIGH);delay(1);  // *optional Wait for 18 tCLKs AKA 9 microseconds, we use 1 millisec
  // Send SDATAC Command (Stop Read Data Continuously mode)
  delay(100); //pause to provide ads129n enough time to boot up...
  adc_send_command(SDATAC);
  int val = adc_rreg(ID) ; 
  switch (val & B00011111 ) { //least significant bits reports channels
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
  WiredSerial.begin(BAUD_RATE); // use native port on Due
  while (WiredSerial.read() >= 0) {} //http://forum.arduino.cc/index.php?topic=134847.0
  delay(200);  // Catch Due reset problem
  if (gMaxChan == 0) { //error mode
    while(1) { //loop forever 
      digitalWrite(kPIN_LED, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(500);               // wait for a second
      digitalWrite(kPIN_LED, LOW);    // turn the LED off by making the voltage LOW
      delay(500); 
    } //while forever
  } //error mode
  adsSetup(); //optional - sets up device - the PC can do this as well       
} //setup()




