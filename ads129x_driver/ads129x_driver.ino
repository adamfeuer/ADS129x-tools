/*
   Driver for TI ADS129x
   for Arduino Due and Arduino Mega2560

   Copyright (c) 2013-2019 by Adam Feuer <adam@adamfeuer.com>

   This library is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <SPI.h>
#include <stdlib.h>
#include <ArduinoJson.h>
#include "adsCommand.h"
#include "ads129x.h"
#include "SerialCommand.h"
#include "JsonCommand.h"
#include "Base64.h"
#include "SpiDma.h"


#define BAUD_RATE 2000000     // WiredSerial ignores this and uses the maximum rate
#define WiredSerial SerialUSB // use the Arduino Due's Native USB port

#define SPI_BUFFER_SIZE 200
#define OUTPUT_BUFFER_SIZE 1000

#define TEXT_MODE 0
#define JSONLINES_MODE 1
#define MESSAGEPACK_MODE 2

#define RESPONSE_OK 200
#define RESPONSE_BAD_REQUEST 400
#define UNRECOGNIZED_COMMAND 406
#define RESPONSE_ERROR 500
#define RESPONSE_NOT_IMPLEMENTED 501
#define RESPONSE_NO_ACTIVE_CHANNELS 502

const char *STATUS_TEXT_OK = "Ok";
const char *STATUS_TEXT_BAD_REQUEST = "Bad request";
const char *STATUS_TEXT_ERROR = "Error";
const char *STATUS_TEXT_NOT_IMPLEMENTED = "Not Implemented";
const char *STATUS_TEXT_NO_ACTIVE_CHANNELS = "No Active Channels";

int protocol_mode = TEXT_MODE;
int max_channels = 0;
int num_active_channels = 0;
boolean active_channels[9]; // reports whether channels 1..9 are active
int num_spi_bytes = 0;
int num_timestamped_spi_bytes = 0;
boolean is_rdatac = false;
boolean base64_mode = true;

char hexDigits[] = "0123456789ABCDEF";

// microseconds timestamp
#define TIMESTAMP_SIZE_IN_BYTES 4
union {
    char timestamp_bytes[TIMESTAMP_SIZE_IN_BYTES];
    unsigned long timestamp;
} timestamp_union;

// sample number counter
#define SAMPLE_NUMBER_SIZE_IN_BYTES 4
union {
    char sample_number_bytes[SAMPLE_NUMBER_SIZE_IN_BYTES];
    unsigned long sample_number = 0;
} sample_number_union;

// SPI input buffer
uint8_t spi_bytes[SPI_BUFFER_SIZE];
uint8_t spi_data_available;

// char buffer to send via USB
char output_buffer[OUTPUT_BUFFER_SIZE];

const char *hardware_type = "unknown";
const char *board_name = "HackEEG";
const char *maker_name = "Starcat LLC";
const char *driver_version = "v0.3.0";


const char *json_rdatac_header= "{\"C\":200,\"D\":\"";
const char *json_rdatac_footer= "\"}";

uint8_t messagepack_rdatac_header[] = { 0x82, 0xa1, 0x43, 0xcc, 0xc8, 0xa1, 0x44, 0xc4};
size_t messagepack_rdatac_header_size = sizeof(messagepack_rdatac_header);

SerialCommand serialCommand;
JsonCommand jsonCommand;

void arduinoSetup();
void adsSetup();
void detectActiveChannels();
void unrecognized(const char *);
void unrecognizedJsonLines(const char *);

void nopCommand(unsigned char unused1, unsigned char unused2);
void versionCommand(unsigned char unused1, unsigned char unused2);
void statusCommand(unsigned char unused1, unsigned char unused2);
void serialNumberCommand(unsigned char unused1, unsigned char unused2);
void textCommand(unsigned char unused1, unsigned char unused2);
void jsonlinesCommand(unsigned char unused1, unsigned char unused2);
void messagepackCommand(unsigned char unused1, unsigned char unused2);
void ledOnCommand(unsigned char unused1, unsigned char unused2);
void ledOffCommand(unsigned char unused1, unsigned char unused2);
void boardLedOffCommand(unsigned char unused1, unsigned char unused2);
void boardLedOnCommand(unsigned char unused1, unsigned char unused2);
void wakeupCommand(unsigned char unused1, unsigned char unused2);
void standbyCommand(unsigned char unused1, unsigned char unused2);
void resetCommand(unsigned char unused1, unsigned char unused2);
void startCommand(unsigned char unused1, unsigned char unused2);
void stopCommand(unsigned char unused1, unsigned char unused2);
void rdatacCommand(unsigned char unused1, unsigned char unused2);
void sdatacCommand(unsigned char unused1, unsigned char unused2);
void rdataCommand(unsigned char unused1, unsigned char unused2);
void base64ModeOnCommand(unsigned char unused1, unsigned char unused2);
void hexModeOnCommand(unsigned char unused1, unsigned char unused2);
void helpCommand(unsigned char unused1, unsigned char unused2);
void readRegisterCommand(unsigned char unused1, unsigned char unused2);
void writeRegisterCommand(unsigned char unused1, unsigned char unused2);
void readRegisterCommandDirect(unsigned char register_number);
void writeRegisterCommandDirect(unsigned char register_number, unsigned char register_value);


void setup() {
    WiredSerial.begin(BAUD_RATE);
    pinMode(PIN_LED, OUTPUT);     // Configure the onboard LED for output
    digitalWrite(PIN_LED, LOW);   // default to LED off

    protocol_mode = TEXT_MODE;
    arduinoSetup();
    adsSetup();

    // Setup callbacks for SerialCommand commands
    serialCommand.addCommand("nop", nopCommand);                     // No operation (does nothing)
    serialCommand.addCommand("micros", microsCommand);               // Returns number of microseconds since the program began executing
    serialCommand.addCommand("version", versionCommand);             // Echos the driver version number
    serialCommand.addCommand("status", statusCommand);               // Echos the driver status
    serialCommand.addCommand("serialnumber", serialNumberCommand);   // Echos the board serial number (UUID from the onboard 24AA256UID-I/SN I2S EEPROM)
    serialCommand.addCommand("text", textCommand);                   // Sets the communication protocol to text
    serialCommand.addCommand("jsonlines", jsonlinesCommand);         // Sets the communication protocol to JSONLines
    serialCommand.addCommand("messagepack", messagepackCommand);     // Sets the communication protocol to MessagePack
    serialCommand.addCommand("ledon", ledOnCommand);                 // Turns Arduino Due onboard LED on
    serialCommand.addCommand("ledoff", ledOffCommand);               // Turns Arduino Due onboard LED off
    serialCommand.addCommand("boardledoff", boardLedOffCommand);     // Turns HackEEG ADS1299 GPIO4 LED off
    serialCommand.addCommand("boardledon", boardLedOnCommand);       // Turns HackEEG ADS1299 GPIO4 LED on
    serialCommand.addCommand("wakeup", wakeupCommand);               // Send the WAKEUP command
    serialCommand.addCommand("standby", standbyCommand);             // Send the STANDBY command
    serialCommand.addCommand("reset", resetCommand);                 // Reset the ADS1299
    serialCommand.addCommand("start", startCommand);                 // Send START command
    serialCommand.addCommand("stop", stopCommand);                   // Send STOP command
    serialCommand.addCommand("rdatac", rdatacCommand);               // Enter read data continuous mode, clear the ringbuffer, and read new data into the ringbuffer
    serialCommand.addCommand("sdatac", sdatacCommand);               // Stop read data continuous mode; ringbuffer data is still available
    serialCommand.addCommand("rdata", rdataCommand);                 // Read one sample of data from each active channel
    serialCommand.addCommand("rreg", readRegisterCommand);           // Read ADS129x register, argument in hex, print contents in hex
    serialCommand.addCommand("wreg", writeRegisterCommand);          // Write ADS129x register, arguments in hex
    serialCommand.addCommand("base64", base64ModeOnCommand);         // RDATA commands send base64 encoded data - default
    serialCommand.addCommand("hex", hexModeOnCommand);               // RDATA commands send hex encoded data
    serialCommand.addCommand("help", helpCommand);                   // Print list of commands
    serialCommand.setDefaultHandler(unrecognized);                   // Handler for any command that isn't matched

    // Setup callbacks for JsonCommand commands
    jsonCommand.addCommand("nop", nopCommand);                       // No operation (does nothing)
    jsonCommand.addCommand("micros", microsCommand);                 // Returns number of microseconds since the program began executing
    jsonCommand.addCommand("ledon", ledOnCommand);                   // Turns Arduino Due onboard LED on
    jsonCommand.addCommand("ledoff", ledOffCommand);                 // Turns Arduino Due onboard LED off
    jsonCommand.addCommand("boardledoff", boardLedOffCommand);       // Turns HackEEG ADS1299 GPIO4 LED off
    jsonCommand.addCommand("boardledon", boardLedOnCommand);         // Turns HackEEG ADS1299 GPIO4 LED on
    jsonCommand.addCommand("status", statusCommand);                 // Returns the driver status
    jsonCommand.addCommand("reset", resetCommand);                   // Reset the ADS1299
    jsonCommand.addCommand("start", startCommand);                   // Send START command
    jsonCommand.addCommand("stop", stopCommand);                     // Send STOP command
    jsonCommand.addCommand("rdatac", rdatacCommand);                 // Enter read data continuous mode, clear the ringbuffer, and read new data into the ringbuffer
    jsonCommand.addCommand("sdatac", sdatacCommand);                 // Stop read data continuous mode; ringbuffer data is still available
    jsonCommand.addCommand("serialnumber", serialNumberCommand);     // Returns the board serial number (UUID from the onboard 24AA256UID-I/SN I2S EEPROM)
    jsonCommand.addCommand("text", textCommand);                     // Sets the communication protocol to text
    jsonCommand.addCommand("jsonlines", jsonlinesCommand);           // Sets the communication protocol to JSONLines
    jsonCommand.addCommand("messagepack", messagepackCommand);       // Sets the communication protocol to MessagePack
    jsonCommand.addCommand("rreg", readRegisterCommandDirect);       // Read ADS129x register
    jsonCommand.addCommand("wreg", writeRegisterCommandDirect);      // Write ADS129x register
    jsonCommand.addCommand("rdata", rdataCommand);                   // Read one sample of data from each active channel
    jsonCommand.setDefaultHandler(unrecognizedJsonLines);            // Handler for any command that isn't matched

    WiredSerial.println("Ready");
}

void loop() {
    switch (protocol_mode) {
        case TEXT_MODE:
            serialCommand.readSerial();
            break;
        case JSONLINES_MODE:
        case MESSAGEPACK_MODE:
            jsonCommand.readSerial();
            break;
        default:
            // do nothing
            ;
    }
    send_samples();
}

long hex_to_long(char *digits) {
    using namespace std;
    char *error;
    long n = strtol(digits, &error, 16);
    if (*error != 0) {
        return -1; // error
    } else {
        return n;
    }
}

void output_hex_byte(int value) {
    int clipped = value & 0xff;
    char charValue[3];
    sprintf(charValue, "%02X", clipped);
    WiredSerial.print(charValue);
}

void encode_hex(char *output, char *input, int input_len) {
    register int count = 0;
    for (register int i = 0; i < input_len; i++) {
        register uint8_t low_nybble = input[i] & 0x0f;
        register uint8_t highNybble = input[i] >> 4;
        output[count++] = hexDigits[highNybble];
        output[count++] = hexDigits[low_nybble];
    }
    output[count] = 0;
}

void send_response_ok() {
    send_response(RESPONSE_OK, STATUS_TEXT_OK);
}

void send_response_error() {
    send_response(RESPONSE_ERROR, STATUS_TEXT_ERROR);
}

void send_response(int status_code, const char *status_text) {
    switch (protocol_mode) {
        case TEXT_MODE:
            char response[128];
            sprintf(response, "%d %s", status_code, status_text);
            WiredSerial.println(response);
            break;
        case JSONLINES_MODE:
            jsonCommand.sendJsonLinesResponse(status_code, (char *) status_text);
            break;
        case MESSAGEPACK_MODE:
            // all responses are in JSON Lines, MessagePack mode is only for sending samples
            jsonCommand.sendJsonLinesResponse(status_code, (char *) status_text);
            break;
        default:
            // unknown protocol
            ;
    }
}

void send_jsonlines_data(int status_code, char data, char *status_text) {
    StaticJsonDocument<1024> doc;
    JsonObject root = doc.to<JsonObject>();
    root[STATUS_CODE_KEY] = status_code;
    root[STATUS_TEXT_KEY] = status_text;
    root[DATA_KEY] = data;
    serializeJson(doc, WiredSerial);
    WiredSerial.println();
    doc.clear();
}

void versionCommand(unsigned char unused1, unsigned char unused2) {
    send_response(RESPONSE_OK, driver_version);
}

void statusCommand(unsigned char unused1, unsigned char unused2) {
    detectActiveChannels();
    if (protocol_mode == TEXT_MODE) {
        WiredSerial.println("200 Ok");
        WiredSerial.print("Driver version: ");
        WiredSerial.println(driver_version);
        WiredSerial.print("Board name: ");
        WiredSerial.println(board_name);
        WiredSerial.print("Board maker: ");
        WiredSerial.println(maker_name);
        WiredSerial.print("Hardware type: ");
        WiredSerial.println(hardware_type);
        WiredSerial.print("Max channels: ");
        WiredSerial.println(max_channels);
        WiredSerial.print("Number of active channels: ");
        WiredSerial.println(num_active_channels);
        WiredSerial.println();
        return;
    }
    StaticJsonDocument<1024> doc;
    JsonObject root = doc.to<JsonObject>();
    root[STATUS_CODE_KEY] = STATUS_OK;
    root[STATUS_TEXT_KEY] = STATUS_TEXT_OK;
    JsonObject status_info = root.createNestedObject(DATA_KEY);
    status_info["driver_version"] = driver_version;
    status_info["board_name"] = board_name;
    status_info["maker_name"] = maker_name;
    status_info["hardware_type"] = hardware_type;
    status_info["max_channels"] = max_channels;
    status_info["active_channels"] = num_active_channels;
    switch (protocol_mode) {
        case JSONLINES_MODE:
        case MESSAGEPACK_MODE:
            jsonCommand.sendJsonLinesDocResponse(doc);
            break;
        default:
            // unknown protocol
            ;
    }
}

void nopCommand(unsigned char unused1, unsigned char unused2) {
    send_response_ok();
}

void microsCommand(unsigned char unused1, unsigned char unused2) {
    unsigned long microseconds = micros();
    if (protocol_mode == TEXT_MODE) {
        send_response_ok();
        WiredSerial.println(microseconds);
        return;
    }
    StaticJsonDocument<1024> doc;
    JsonObject root = doc.to<JsonObject>();
    root[STATUS_CODE_KEY] = STATUS_OK;
    root[STATUS_TEXT_KEY] = STATUS_TEXT_OK;
    root[DATA_KEY] = microseconds;
    switch (protocol_mode) {
        case JSONLINES_MODE:
        case MESSAGEPACK_MODE:
            jsonCommand.sendJsonLinesDocResponse(doc);
            break;
        default:
            // unknown protocol
            ;
    }
}

void serialNumberCommand(unsigned char unused1, unsigned char unused2) {
    send_response(RESPONSE_NOT_IMPLEMENTED, STATUS_TEXT_NOT_IMPLEMENTED);
}

void textCommand(unsigned char unused1, unsigned char unused2) {
    protocol_mode = TEXT_MODE;
    send_response_ok();
}

void jsonlinesCommand(unsigned char unused1, unsigned char unused2) {
    protocol_mode = JSONLINES_MODE;
    send_response_ok();
}

void messagepackCommand(unsigned char unused1, unsigned char unused2) {
    protocol_mode = MESSAGEPACK_MODE;
    send_response_ok();
}

void ledOnCommand(unsigned char unused1, unsigned char unused2) {
    digitalWrite(PIN_LED, HIGH);
    send_response_ok();
}

void ledOffCommand(unsigned char unused1, unsigned char unused2) {
    digitalWrite(PIN_LED, LOW);
    send_response_ok();
}

void boardLedOnCommand(unsigned char unused1, unsigned char unused2) {
    int state = adcRreg(ADS129x::GPIO);
    state = state & 0xF7;
    state = state | 0x80;
    adcWreg(ADS129x::GPIO, state);
    send_response_ok();
}

void boardLedOffCommand(unsigned char unused1, unsigned char unused2) {
    int state = adcRreg(ADS129x::GPIO);
    state = state & 0x77;
    adcWreg(ADS129x::GPIO, state);
    send_response_ok();
}

void base64ModeOnCommand(unsigned char unused1, unsigned char unused2) {
    base64_mode = true;
    send_response(RESPONSE_OK, "Base64 mode on - rdata command will respond with base64 encoded data.");
}

void hexModeOnCommand(unsigned char unused1, unsigned char unused2) {
    base64_mode = false;
    send_response(RESPONSE_OK, "Hex mode on - rdata command will respond with hex encoded data");
}

void helpCommand(unsigned char unused1, unsigned char unused2) {
    if (protocol_mode == JSONLINES_MODE ||  protocol_mode == MESSAGEPACK_MODE) {
        send_response(RESPONSE_OK, "Help not available in JSON Lines or MessagePack modes.");
    } else {
        WiredSerial.println("200 Ok");
        WiredSerial.println("Available commands: ");
        serialCommand.printCommands();
        WiredSerial.println();
    }
}

void readRegisterCommand(unsigned char unused1, unsigned char unused2) {
    using namespace ADS129x;
    char *arg1;
    arg1 = serialCommand.next();
    if (arg1 != NULL) {
        long registerNumber = hex_to_long(arg1);
        if (registerNumber >= 0) {
            int result = adcRreg(registerNumber);
            WiredSerial.print("200 Ok");
            WiredSerial.print(" (Read Register ");
            output_hex_byte(registerNumber);
            WiredSerial.print(") ");
            WiredSerial.println();
            output_hex_byte(result);
            WiredSerial.println();
        } else {
            WiredSerial.println("402 Error: expected hexidecimal digits.");
        }
    } else {
        WiredSerial.println("403 Error: register argument missing.");
    }
    WiredSerial.println();
}

void readRegisterCommandDirect(unsigned char register_number, unsigned char unused1) {
    using namespace ADS129x;
    if (register_number >= 0 and register_number <= 255) {
        unsigned char result = adcRreg(register_number);
        StaticJsonDocument<1024> doc;
        JsonObject root = doc.to<JsonObject>();
        root[STATUS_CODE_KEY] = STATUS_OK;
        root[STATUS_TEXT_KEY] = STATUS_TEXT_OK;
        root[DATA_KEY] = result;
        jsonCommand.sendJsonLinesDocResponse(doc);
    } else {
        send_response_error();
    }
}

void writeRegisterCommand(unsigned char unused1, unsigned char unused2) {
    char *arg1, *arg2;
    arg1 = serialCommand.next();
    arg2 = serialCommand.next();
    if (arg1 != NULL) {
        if (arg2 != NULL) {
            long registerNumber = hex_to_long(arg1);
            long registerValue = hex_to_long(arg2);
            if (registerNumber >= 0 && registerValue >= 0) {
                adcWreg(registerNumber, registerValue);
                WiredSerial.print("200 Ok");
                WiredSerial.print(" (Write Register ");
                output_hex_byte(registerNumber);
                WiredSerial.print(" ");
                output_hex_byte(registerValue);
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
    WiredSerial.println();
}


void writeRegisterCommandDirect(unsigned char register_number, unsigned char register_value) {
    if (register_number >= 0 && register_value >= 0) {
        adcWreg(register_number, register_value);
        send_response_ok();
    } else {
        send_response_error();
    }
}

void wakeupCommand(unsigned char unused1, unsigned char unused2) {
    using namespace ADS129x;
    adcSendCommand(WAKEUP);
    send_response_ok();
}

void standbyCommand(unsigned char unused1, unsigned char unused2) {
    using namespace ADS129x;
    adcSendCommand(STANDBY);
    send_response_ok();
}

void resetCommand(unsigned char unused1, unsigned char unused2) {
    using namespace ADS129x;
    adcSendCommand(RESET);
    adsSetup();
    send_response_ok();
}

void startCommand(unsigned char unused1, unsigned char unused2) {
    using namespace ADS129x;
    adcSendCommand(START);
    sample_number_union.sample_number = 0;
    send_response_ok();
}

void stopCommand(unsigned char unused1, unsigned char unused2) {
    using namespace ADS129x;
    adcSendCommand(STOP);
    send_response_ok();
}

void rdataCommand(unsigned char unused1, unsigned char unused2) {
    using namespace ADS129x;
    while (digitalRead(IPIN_DRDY) == HIGH);
    adcSendCommandLeaveCsActive(RDATA);
    if (protocol_mode == TEXT_MODE) {
        send_response_ok();
    }
    send_sample();
}

void rdatacCommand(unsigned char unused1, unsigned char unused2) {
    using namespace ADS129x;
    detectActiveChannels();
    if (num_active_channels > 0) {
        is_rdatac = true;
        adcSendCommand(RDATAC);
        send_response_ok();
    } else {
        send_response(RESPONSE_NO_ACTIVE_CHANNELS, STATUS_TEXT_NO_ACTIVE_CHANNELS);
    }
}

void sdatacCommand(unsigned char unused1, unsigned char unused2) {
    using namespace ADS129x;
    is_rdatac = false;
    adcSendCommand(SDATAC);
    using namespace ADS129x;
    send_response_ok();
}

// This gets set as the default handler, and gets called when no other command matches.
void unrecognized(const char *command) {
    WiredSerial.println("406 Error: Unrecognized command.");
    WiredSerial.println();
}

// This gets set as the default handler for jsonlines and messagepack, and gets called when no other command matches.
void unrecognizedJsonLines(const char *command) {
    StaticJsonDocument<1024> doc;
    JsonObject root = doc.to<JsonObject>();
    root[STATUS_CODE_KEY] = UNRECOGNIZED_COMMAND;
    root[STATUS_TEXT_KEY] = "Unrecognized command";
    jsonCommand.sendJsonLinesDocResponse(doc);
}

void detectActiveChannels() {  //set device into RDATAC (continous) mode -it will stream data
    if ((is_rdatac) || (max_channels < 1)) return; //we can not read registers when in RDATAC mode
    //Serial.println("Detect active channels: ");
    using namespace ADS129x;
    num_active_channels = 0;
    for (int i = 1; i <= max_channels; i++) {
        delayMicroseconds(1);
        int chSet = adcRreg(CHnSET + i);
        active_channels[i] = ((chSet & 7) != SHORTED);
        if ((chSet & 7) != SHORTED) num_active_channels++;
    }
}

void drdy_interrupt() {
    spi_data_available = 1;
}

inline void send_samples(void) {
    if (!is_rdatac) return;
    if (spi_data_available) {
        spi_data_available = 0;
        receive_sample();
        send_sample();
    }
}

inline void receive_sample() {
    digitalWrite(PIN_CS, LOW);
    delayMicroseconds(10);
    memset(spi_bytes, 0, sizeof(spi_bytes));
    timestamp_union.timestamp = micros();
    spi_bytes[0] = timestamp_union.timestamp_bytes[0];
    spi_bytes[1] = timestamp_union.timestamp_bytes[1];
    spi_bytes[2] = timestamp_union.timestamp_bytes[2];
    spi_bytes[3] = timestamp_union.timestamp_bytes[3];
    spi_bytes[4] = sample_number_union.sample_number_bytes[0];
    spi_bytes[5] = sample_number_union.sample_number_bytes[1];
    spi_bytes[6] = sample_number_union.sample_number_bytes[2];
    spi_bytes[7] = sample_number_union.sample_number_bytes[3];

    uint8_t returnCode = spiRec(spi_bytes + TIMESTAMP_SIZE_IN_BYTES + SAMPLE_NUMBER_SIZE_IN_BYTES, num_spi_bytes);

    digitalWrite(PIN_CS, HIGH);
    sample_number_union.sample_number++;
}

inline void send_sample(void) {
    switch (protocol_mode) {
        case JSONLINES_MODE:
            WiredSerial.write(json_rdatac_header);
            base64_encode(output_buffer, (char *) spi_bytes, num_timestamped_spi_bytes);
            WiredSerial.write(output_buffer);
            WiredSerial.write(json_rdatac_footer);
            WiredSerial.write("\n");
            break;
        case TEXT_MODE:
            if (base64_mode) {
                base64_encode(output_buffer, (char *) spi_bytes, num_timestamped_spi_bytes);
            } else {
                encode_hex(output_buffer, (char *) spi_bytes, num_timestamped_spi_bytes);
            }
            WiredSerial.println(output_buffer);
            break;
        case MESSAGEPACK_MODE:
            send_sample_messagepack(num_timestamped_spi_bytes);
            break;
    }
}


inline void send_sample_json(int num_bytes) {
    StaticJsonDocument<1024> doc;
    JsonObject root = doc.to<JsonObject>();
    root[STATUS_CODE_KEY] = STATUS_OK;
    root[STATUS_TEXT_KEY] = STATUS_TEXT_OK;
    JsonArray data = root.createNestedArray(DATA_KEY);
    copyArray(spi_bytes, num_bytes, data);
    jsonCommand.sendJsonLinesDocResponse(doc);
}


inline void send_sample_messagepack(int num_bytes) {
    WiredSerial.write(messagepack_rdatac_header, messagepack_rdatac_header_size);
    WiredSerial.write((uint8_t) num_bytes);
    WiredSerial.write(spi_bytes, num_bytes);
}

void adsSetup() { //default settings for ADS1298 and compatible chips
    using namespace ADS129x;
    // Send SDATAC Command (Stop Read Data Continuously mode)
    spi_data_available = 0;
    attachInterrupt(digitalPinToInterrupt(IPIN_DRDY), drdy_interrupt, FALLING);
    adcSendCommand(SDATAC);
    delay(1000); //pause to provide ads129n enough time to boot up...
    // delayMicroseconds(2);
    delay(100);
    int val = adcRreg(ID);
    switch (val & B00011111) {
        case B10000:
            hardware_type = "ADS1294";
            max_channels = 4;
            break;
        case B10001:
            hardware_type = "ADS1296";
            max_channels = 6;
            break;
        case B10010:
            hardware_type = "ADS1298";
            max_channels = 8;
            break;
        case B11110:
            hardware_type = "ADS1299";
            max_channels = 8;
            break;
        case B11100:
            hardware_type = "ADS1299-4";
            max_channels = 4;
            break;
        case B11101:
            hardware_type = "ADS1299-6";
            max_channels = 6;
            break;
        default:
            max_channels = 0;
    }
    num_spi_bytes = (3 * (max_channels + 1)); //24-bits header plus 24-bits per channel
    num_timestamped_spi_bytes = num_spi_bytes + TIMESTAMP_SIZE_IN_BYTES + SAMPLE_NUMBER_SIZE_IN_BYTES;
    if (max_channels == 0) { //error mode
        while (1) {
            digitalWrite(PIN_LED, HIGH);
            delay(500);
            digitalWrite(PIN_LED, LOW);
            delay(500);
        }
    } //error mode

    // All GPIO set to output 0x0000: (floating CMOS inputs can flicker on and off, creating noise)
    adcWreg(GPIO, 0);
    adcWreg(CONFIG3,PD_REFBUF | CONFIG3_const);
    digitalWrite(PIN_START, HIGH);
}

void arduinoSetup() {
    pinMode(PIN_LED, OUTPUT);
    using namespace ADS129x;
    // prepare pins to be outputs or inputs
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
    digitalWrite(IPIN_PWDN, HIGH); // *optional - turn off power down mode
    digitalWrite(IPIN_RESET, HIGH);
    delay(1000);
    digitalWrite(IPIN_RESET, LOW);
    delay(1);
    digitalWrite(IPIN_RESET, HIGH);
    delay(1);  // *optional Wait for 18 tCLKs AKA 9 microseconds, we use 1 millisecond
} 
