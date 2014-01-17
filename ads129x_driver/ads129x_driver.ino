/*
 * Text-mode driver for ADS129x 
 * for Arduino Due
 *
 * Copyright (c) 2013 by Adam Feuer <adam@adamfeuer.com>
 * Copyright (c) 2012 by Chris Rorden
 * Copyright (c) 2012 by Steven Cogswell and Stefan Rado 
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <SPI.h>

#include <stdlib.h>
#include "adsCommand.h"
#include "ads129x.h"
#include "SerialCommand.h"
#include "Base64.h"
#include "SpiDma.h"

#define VERSION "ADS129x driver v0.2"

#define BAUD_RATE  115200     // WiredSerial ignores this and uses the maximum rate
#define txActiveChannelsOnly  // reduce bandwidth: only send data for active data channels
#define WiredSerial SerialUSB // use Due's Native USB port

int maxChannels = 0;
int numActiveChannels = 0;
boolean gActiveChan[9]; // reports whether channels 1..9 are active
boolean isRdatac = false;
boolean base64Mode = true;

char hexDigits[] = "0123456789ABCDEF";
uint8_t serialBytes[200];
char sampleBuffer[1000];

SerialCommand serialCommand;  

void setup() {  
  WiredSerial.begin(115200); 
  pinMode(PIN_LED,OUTPUT);      // Configure the onboard LED for output
  digitalWrite(PIN_LED,LOW);    // default to LED off
  arduinoSetup();
  adsSetup();

  WiredSerial.println("Ready"); 

  // Setup callbacks for SerialCommand commands 
  serialCommand.addCommand("version",version_command);        // Echos the driver version number
  serialCommand.addCommand("status",status_command);        // Echos the driver version number
  serialCommand.addCommand("serialnumber",serialNumber_command);        // Echos the driver version number
  serialCommand.addCommand("ledon",ledOn_command);            // Turns Due onboad LED on
  serialCommand.addCommand("ledoff", ledOff_command);         // Turns Due onboard LED off
  serialCommand.addCommand("boardledoff", boardLedOff_command);  // Turns HackEEG GPIO LED off
  serialCommand.addCommand("boardledon", boardLedOn_command);    // Turns HackEEG GPIO LED on
  serialCommand.addCommand("wakeup", wakeup_command);         // Enter read data continuous mode
  serialCommand.addCommand("standby", standby_command);         // Enter read data continuous mode
  serialCommand.addCommand("reset", reset_command);         // Enter read data continuous mode
  serialCommand.addCommand("start", start_command);         // Enter read data continuous mode
  serialCommand.addCommand("stop", stop_command);         // Enter read data continuous mode
  serialCommand.addCommand("rdatac", rdatac_command);         // Enter read data continuous mode
  serialCommand.addCommand("sdatac", sdatac_command);         // Stop read data continuous mode
  serialCommand.addCommand("rdata", rdata_command);           // Read one sample of data from each channel
  serialCommand.addCommand("rreg", readRegister_command);     // Read ADS129x register, argument in hex, print contents in hex
  serialCommand.addCommand("wreg", writeRegister_command);    // Write ADS129x register, arguments in hex
  serialCommand.addCommand("base64", base64ModeOn_command);   // rdata commands send base64 encoded data - default
  serialCommand.addCommand("hex", hexModeOn_command);         // rdata commands send hex encoded data
  serialCommand.addCommand("help", help_command);             // Print list of commands
  serialCommand.setDefaultHandler(unrecognized);      // Handler for any command that isn't matched 

}

void loop() {  
  serialCommand.readSerial();
  sendSamples();
  sendSamples();
  sendSamples();
  sendSamples();
  sendSamples();
  sendSamples();
  sendSamples();
  sendSamples();
  sendSamples();
  sendSamples();
}

long hexToLong(char *digits) {
  using namespace std;
  char *error;
  long n = strtol(digits, &error, 16);
  if ( *error != 0 ) { 
    return -1; // error
  } 
  else {
    return n;
  }
}

void outputHexByte(int value) {
  int clipped = value & 0xff;
  char charValue[3];
  sprintf(charValue, "%02X", clipped);
  WiredSerial.print(charValue);
}

void encodeHex(char* output, char* input, int inputLen) {
  register int count = 0;
  for (register int i=0; i < inputLen; i++) {
    register uint8_t lowNybble = input[i] & 0x0f;
    register uint8_t highNybble = input[i] >> 4;
    output[count++] = hexDigits[highNybble];
    output[count++] = hexDigits[lowNybble];
  }
  output[count] = 0;
}

void version_command() {
  WiredSerial.println("200 Ok");
  WiredSerial.println(VERSION);
  digitalWrite(PIN_LED,LOW);
}

void status_command() {
  WiredSerial.println("200 Ok");
  WiredSerial.print("Max channels: "); 
  WiredSerial.println(maxChannels); 
  detectActiveChannels();
  WiredSerial.print("Number of active channels: "); 
  WiredSerial.println(numActiveChannels); 
  WiredSerial.println(""); 
}

void serialNumber_command() {
  WiredSerial.println("200 Ok");
  WiredSerial.println("Not implemented yet. "); 
}

void ledOn_command() {
  WiredSerial.println("200 Ok");
  WiredSerial.println("LED on"); 
  digitalWrite(PIN_LED,HIGH);  
}

void ledOff_command() {
  WiredSerial.println("200 Ok");
  WiredSerial.println("LED off"); 
  digitalWrite(PIN_LED,LOW);
}

void boardLedOn_command() {
  WiredSerial.println("200 Ok");
  WiredSerial.println("Board GPIO LED on"); 
  int state = adc_rreg(ADS1298::GPIO);
  state = state & 0xF7;
  state = state | 0x80;
  adc_wreg(ADS1298::GPIO, state);
}

void boardLedOff_command() {
  WiredSerial.println("200 Ok");
  WiredSerial.println("Board GPIO LED off"); 
  int state = adc_rreg(ADS1298::GPIO);
  state = state & 0x77;
  adc_wreg(ADS1298::GPIO, state);
}

void base64ModeOn_command() {
  WiredSerial.println("200 Ok");
  WiredSerial.println("Base64 mode on - rdata commands will send bas64 encoded data."); 
  base64Mode = true;
}

void hexModeOn_command() {
  WiredSerial.println("200 Ok");
  WiredSerial.println("Hex mode on - rdata commands will send hex encoded data"); 
  base64Mode = false;
}

void help_command() {
  WiredSerial.println("200 Ok");
  WiredSerial.println("Available commands: "); 
  serialCommand.printCommands();
  WiredSerial.println();
}

void readRegister_command() {
  using namespace ADS1298; 
  char *arg1; 
  arg1 = serialCommand.next();   
  if (arg1 != NULL) {
    long registerNumber = hexToLong(arg1);
    if (registerNumber >= 0) {
      int result = adc_rreg(registerNumber);
      WiredSerial.print("200 Ok");
      WiredSerial.print(" (Read Register "); 
      outputHexByte(registerNumber); 
      WiredSerial.print(") "); 
      WiredSerial.println();               
      outputHexByte(result);      
      WiredSerial.println();      

    } 
    else {
      WiredSerial.println("402 Error: expected hexidecimal digits."); 
    }
  } 
  else {
    WiredSerial.println("403 Error: register argument missing."); 
  }
}

void writeRegister_command() {
  char *arg1, *arg2; 
  arg1 = serialCommand.next();   
  arg2 = serialCommand.next();  
  if (arg1 != NULL) {
    if (arg2 != NULL) { 
      long registerNumber = hexToLong(arg1);
      long registerValue = hexToLong(arg2);
      if (registerNumber >= 0 && registerValue >= 0) {
        adc_wreg(registerNumber, registerValue);        
        WiredSerial.print("200 Ok"); 
        WiredSerial.print(" (Write Register "); 
        outputHexByte(registerNumber); 
        WiredSerial.print(" "); 
        outputHexByte(registerValue); 
        WiredSerial.print(") "); 
        WiredSerial.println();
      } 
      else {
        WiredSerial.println("402 Error: expected hexidecimal digits."); 
      }
    } 
    else {
      WiredSerial.println("404 Error: value argument missing."); 
    }
  } 
  else {
    WiredSerial.println("403 Error: register argument missing."); 
  }
}


void wakeup_command() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok ");
  adc_send_command(WAKEUP);
}

void standby_command() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok ");
  adc_send_command(STANDBY);
}

void reset_command() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok ");
  adc_send_command(RESET);
}

void start_command() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok ");
  adc_send_command(START);
}

void stop_command() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok ");
  adc_send_command(STOP);
}

void rdata_command() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok ");
  adc_send_command(RDATA);
  while (digitalRead(IPIN_DRDY) == HIGH);
  sendSample();
}

void rdatac_command() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok");
  WiredSerial.println("RDATAC mode on."); 
  detectActiveChannels();
  if (numActiveChannels > 0) { 
    isRdatac = true;
    adc_send_command(RDATAC);
  }
}

void sdatac_command() {
  using namespace ADS1298; 
  WiredSerial.println("200 Ok");
  WiredSerial.println("RDATAC mode off."); 
  isRdatac= false;
  adc_send_command(SDATAC);
}

// This gets set as the default handler, and gets called when no other command matches. 
void unrecognized(const char *command) {
  WiredSerial.println("Unrecognized command."); 
}


void detectActiveChannels() {  //set device into RDATAC (continous) mode -it will stream data
  if ((isRdatac) ||  (maxChannels < 1)) return; //we can not read registers when in RDATAC mode
  //Serial.println("Detect active channels: ");
  using namespace ADS1298; 
  numActiveChannels = 0;
  for (int i = 1; i <= maxChannels; i++) {
    delayMicroseconds(1); 
    int chSet = adc_rreg(CHnSET + i);
    gActiveChan[i] = ((chSet & 7) != SHORTED);
    if ( (chSet & 7) != SHORTED) numActiveChannels ++;   
    //WiredSerial.print("Active channels: ");
    //WiredSerial.println(numActiveChannels);
  }

}

//#define testSignal //use this to determine if your software is accurately measuring full range 24-bit signed data -8388608..8388607
#ifdef testSignal
int testInc = 1;
int testPeriod = 100;
byte testMSB, testLSB; 
#endif 

inline void sendSamples(void) { 
  if ((!isRdatac) || (numActiveChannels < 1) )  return;
  if (digitalRead(IPIN_DRDY) == HIGH) return; 
  sendSample();
}

// Use SAM3X DMA
inline void sendSample(void) { 
  digitalWrite(PIN_CS, LOW);
  register int numSerialBytes = (3 * (maxChannels+1)); //24-bits header plus 24-bits per channel
  uint8_t returnCode = spiRec(serialBytes, numSerialBytes);
  digitalWrite(PIN_CS, HIGH);
  register unsigned int count = 0;
  if (base64Mode == true) {
    base64_encode(sampleBuffer, (char *)serialBytes, numSerialBytes);
  } 
  else {
    encodeHex(sampleBuffer, (char *)serialBytes, numSerialBytes);
  }
  WiredSerial.println(sampleBuffer);
}

void adsSetup() { //default settings for ADS1298 and compatible chips
  using namespace ADS1298;
  // Send SDATAC Command (Stop Read Data Continuously mode)
  delay(1000); //pause to provide ads129n enough time to boot up...
  adc_send_command(SDATAC);
  delayMicroseconds(2);
  //delay(100); 
  int val = adc_rreg(ID) ;
  switch (val & B00011111 ) { //least significant bits reports channels
  case  B10000: //16
    maxChannels = 4; //ads1294
    break;
  case B10001: //17
    maxChannels = 6; //ads1296
    break;
  case B10010: //18
    maxChannels = 8; //ads1298
    break;
  case B11110: //30
    maxChannels = 8; //ads1299
    break;
  default: 
    maxChannels = 0;
  }
  if (maxChannels == 0) { //error mode
    while(1) { //loop forever 
      digitalWrite(PIN_LED, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(500);               // wait for a second`
      digitalWrite(PIN_LED, LOW);    // turn the LED off by making the voltage LOW
      delay(500); 
    } //while forever
  } //error mode
  // All GPIO set to output 0x0000: (floating CMOS inputs can flicker on and off, creating noise)
  adc_wreg(GPIO, 0);
  adc_wreg(CONFIG3,PD_REFBUF | CONFIG3_const);
  //FOR RLD: Power up the internal reference and wait for it to settle
  // adc_wreg(CONFIG3, RLDREF_INT | PD_RLD | PD_REFBUF | VREF_4V | CONFIG3_const);
  // delay(150);
  // adc_wreg(RLD_SENSP, 0x01);	// only use channel IN1P and IN1N
  // adc_wreg(RLD_SENSN, 0x01);	// for the RLD Measurement
  adc_wreg(CONFIG1,HIGH_RES_500_SPS);
  adc_wreg(CONFIG2, INT_TEST);	// generate internal test signals
  // Set the first two channels to input signal
  for (int i = 1; i <= 1; ++i) {
    //adc_wreg(CHnSET + i, ELECTRODE_INPUT | GAIN_1X); //report this channel with x12 gain
    //adc_wreg(CHnSET + i, ELECTRODE_INPUT | GAIN_12X); //report this channel with x12 gain
    adc_wreg(CHnSET + i, TEST_SIGNAL | GAIN_12X); //create square wave
    //adc_wreg(CHnSET + i,SHORTED); //disable this channel
  }
  for (int i = 2; i <= 2; ++i) {
    //adc_wreg(CHnSET + i, ELECTRODE_INPUT | GAIN_1X); //report this channel with x12 gain
    //adc_wreg(CHnSET + i, ELECTRODE_INPUT | GAIN_12X); //report this channel with x12 gain
    adc_wreg(CHnSET + i, TEST_SIGNAL | GAIN_12X); //create square wave
    //adc_wreg(CHnSET + i,SHORTED); //disable this channel
  }
  for (int i = 3; i <= 6; ++i) {
    adc_wreg(CHnSET + i, SHORTED); //disable this channel
  }
  for (int i = 7; i <= 8; ++i) {
    adc_wreg(CHnSET + i, ELECTRODE_INPUT | GAIN_1X); //report this channel with x12 gain
    //adc_wreg(CHnSET + i, ELECTRODE_INPUT | GAIN_12X); //report this channel with x12 gain
    //adc_wreg(CHnSET + i, TEST_SIGNAL | GAIN_12X); //create square wave
    //adc_wreg(CHnSET + i,SHORTED); //disable this channel
  }

  
  
  digitalWrite(PIN_START, HIGH);  
}

void arduinoSetup(){
  Serial.begin(BAUD_RATE); // for debugging
  pinMode(PIN_LED, OUTPUT);
  using namespace ADS1298;

  //prepare pins to be outputs or inputs
  //pinMode(PIN_SCLK, OUTPUT); //optional - SPI library will do this for us
  //pinMode(PIN_DIN, OUTPUT); //optional - SPI library will do this for us
  //pinMode(PIN_DOUT, INPUT); //optional - SPI library will do this for us
  //pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_START, OUTPUT);
  pinMode(IPIN_DRDY, INPUT);
  pinMode(PIN_CLKSEL, OUTPUT);// *optional
  pinMode(IPIN_RESET, OUTPUT);// *optional
  //pinMode(IPIN_PWDN, OUTPUT);// *optional
  digitalWrite(PIN_CLKSEL, HIGH); // internal clock
  //start Serial Peripheral Interface
  spiBegin(PIN_CS);
  spiInit(MSBFIRST, SPI_MODE1, SPI_CLOCK_DIVIDER);
  //Start ADS1298
  delay(500); //wait for the ads129n to be ready - it can take a while to charge caps
  digitalWrite(PIN_CLKSEL, HIGH);// *optional
  delay(10); // wait for oscillator to wake up  
  delay(1);
  digitalWrite(IPIN_PWDN, HIGH); // *optional - turn off power down mode
  digitalWrite(IPIN_RESET, HIGH);
  delay(1000);// *optional
  digitalWrite(IPIN_RESET, LOW);
  delay(1);// *optional
  digitalWrite(IPIN_RESET, HIGH);
  delay(1);  // *optional Wait for 18 tCLKs AKA 9 microseconds, we use 1 millisecond


} //setup()





