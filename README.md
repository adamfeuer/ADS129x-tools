# ADS129x-tools

This is a collection of software for working with the TI ADS129x series of analog to digital
converter chips.

The [TI ADS1299](http://www.ti.com/product/ads1299) is a 24-bit 8-channel ADC meant specifically for EEG, with 24x programmable gain amplifiers and much of the analog circuitry needed for EEG. It is capable of digitizing 16,000 samples per second at 24 bit resolution. The ADS1299-4 is a 4-channel version of the ADS1299; the ADS1299-6 is a 6-channel version.

The [TI ADS1298](http://www.ti.com/product/ads1298) is a 24-bit, 8-channel ADC chip with SPI interface, and 12x programmable gain amplifiers,
meant for ECG and EEG. The ADS1294 is a 4-channel version; ADS1296 is a 6 channel version. 

The ADS1299 and ADS1298 family of chips are ideally suited for digitizing biological signals.

## Arduino drivers

The ads129x-driver/ directory contains an Arduino sketch and associated C/C++ files that make up a driver for ADS129x chips. So far it has only been tested on the ADS1299 and ADS1298, but should work on the other models. This driver has been tested on the Arduino Due and Mega2560, but should also work on other Arduinos. The DMA mode can only be used on the Arduino Due.

The driver has a text-mode interface, so can be used without any client software – just open up a serial port to the SAM3X8E native USB port (line endings NL+CR). It also has a JSONLines mode for easy parsing by client programs and a MessagePack mode for efficient binary communication. 

In MessagePack mode, using the Arduino Due's native SPI DMA, driver can read from the ADS1299 at 16,000 samples per second, and can send that data on to the host via the Arduino Due's USB 2.0 High Speed connection at the same rate. 

By default the driver uses the Arduino library's software SPI (without DMA), and can read and send 8,000 samples per second in that configuration on the Arduino Due, either in JSON Lines mode or MessagePack mode. Other Arduinos will have lower performance.

When in MessagePack mode, MessagePack format is only used to transfer data in `rdata` and `rdatac` commands; all other communication takes place by JSON Lines.

In text mode, samples are encoded using the [base64](http://en.wikipedia.org/wiki/Base64) encoding by default.


## Installing

You must install the [ArduinoJson](https://arduinojson.org/v6/doc/installation/) library before compiling this driver.


## Using the Driver

The commands that are available are:

* RREG – read register. Takes two hex digits as an argument (one byte, for example: FF). Argument 1 is the register to read.
Returns a single hex-encoded byte (for example, 0E) that represents the contents of the register.
* WREG – write register. Takes two hex-encoded bytes as arguments, separated by a space. Argument 1 is the register to write, argument 2 is the register value.
* RDATA – read one sample of data from the ADS129x chip. Returns 3 bytes of header, plus 3 bytes x number of channels (8 for ADS1298 or ADS1299), encoded using base64 or packed hex in text mode, and JSON Lines byte array, or MessagePack byte array, depending on the protocol
* RDATAC – start read data continuous mode. Data is read into a buffer in the Arduino RAM, and streamed to the client one sample at a time, either in packed base64 format or packed hex format in text, JSON Lines byte array, or MessagePack byte array, depending on the protocol.
* SDATAC – stop read data continuous mode. 
* VERSION – reports the driver version
* SERIALNUMBER – reports the HackEEG serial number (UUID from the onboard 24AA256UID-I/SN I2S EEPROM)
* LEDON – turns on the Arduino Due onboard LED.
* LEDOFF – turns off the Arduino Due onboard LED.
* BOARDLEDON – turns on the HackEEG Shield LED. (HackEEG shield has a blue LED connected to ADS1299 GPIO4) 
* BOARDLEDOFF – turns off the HackEEG Shield LED. 
* BASE64 – RDATA/RDATAC commands will encode data in base64.
* TEXT – communication protocol switches to text. See the Communication Protocol section.
* JSONLINES – communication protocol switches from text to [JSONLines](http://jsonlines.org/). This is a text-oriented serialization format with libraries in many languages. See the Communication Protocol section.
* MESSAGEPACK – communication protocol switches from text to [MessagePack](https://msgpack.org) for `rdatac` data only. This is a concise binary serialization format with libraries in many languages. See the Communication Protocol section.
* HEX – RDATA commands will encode data in hexidecimal format.
* HELP – prints a list of available commands.



## General Operation

The ADS129x chips are configured by reading and writing registers. See the chip datasheet for more information about configuring the ADS129x and reading data from it.

If the host program (the program that reads data from the driver) does not pull data from the serial or USB interface fast enough, the driver
will block on sending when the serial or USB buffers fill up. This will cause the driver to lose samples. 


In most applications, the Python 3 usage will go something like this:

```python
#!/usr/bin/env python

SERIAL_PORT_PATH="/dev/cu.usbmodem14434401"  # your actual path to the Arduino Native serial port device goes here
import sys
import hackeeg
from hackeeg import ads1299

hackeeg = hackeeg.HackEEGBoard(SERIAL_PORT_PATH)
hackeeg.connect()
hackeeg.sdatac()
hackeeg.reset()
hackeeg.blink_board_led()
hackeeg.disable_all_channels()
sample_mode = ads1299.HIGH_RES_250_SPS | ads1299.CONFIG1_const
hackeeg.wreg(ads1299.CONFIG1, sample_mode)
test_signal_mode = ads1299.INT_TEST_4HZ | ads1299.CONFIG2_const
hackeeg.wreg(ads1299.CONFIG2, test_signal_mode)
hackeeg.enable_channel(7)
hackeeg.wreg(ads1299.CH7SET, ads1299.TEST_SIGNAL | ads1299.GAIN_1X)
hackeeg.rreg(ads1299.CH5SET)

# Unipolar mode - setting SRB1 bit sends mid-supply voltage to the N inputs
hackeeg.wreg(ads1299.MISC1, ads1299.SRB1)
# add channels into bias generation
hackeeg.wreg(ads1299.BIAS_SENSP, ads1299.BIAS8P)
hackeeg.rdatac()
hackeeg.start()

while True:
    result = hackeeg.read_response()
    status_code = result.get('STATUS_CODE')
    status_text = result.get('STATUS_TEXT')
    data = result.get(hackeeg.DataKey)
    if data:
        decoded_data = result.get(hackeeg.DecodedDataKey)
        if decoded_data:
            timestamp = decoded_data.get('timestamp')
            ads_gpio = decoded_data.get('ads_gpio')
            loff_statp = decoded_data.get('loff_statp')
            loff_statn = decoded_data.get('loff_statn')
            channel_data = decoded_data.get('channel_data')
            print(f"timestamp:{timestamp} | gpio:{ads_gpio} loff_statp:{loff_statp} loff_statn:{loff_statn} |   ",
                  end='')
            for channel_number, sample in enumerate(channel_data):
                print(f"{channel_number + 1}:{sample} ", end='')
            print()
        else:
            print(data)
        sys.stdout.flush()
```

## SPI DMA 

The driver running on the Arduino communicates with the ADS1299 chip via the SPI interface. For data rates from 250 to 8,192 samples per second, the driver can use the Arduino API's built in software SPI. This is simplest.

To use the 16,384 samples per second data rate, or to reduce CPU load on the Arduino Due, the driver can use the Arduino Due's [Atmel SAM3X8E](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-11057-32-bit-Cortex-M3-Microcontroller-SAM3X-SAM3A_Datasheet.pdf) CPU's built-in SPI DMA ([Direct Memory Access](https://en.wikipedia.org/wiki/Direct_memory_access)) controller. To enable this, change the following constants in `SpiDma.cpp` from this:

Arduino software SPI:

```
#define USE_ARDUINO_SPI_LIBRARY 1
#define USE_NATIVE_SAM3X_SPI 0
```

To this (HackEEG SPI DMA):

```
#define USE_ARDUINO_SPI_LIBRARY 0
#define USE_NATIVE_SAM3X_SPI 1
```

Using the SPI DMA offloads all SPI communication to the SAM3X8E DMA controller, freeing up the CPU to do other tasks like serial communication. 

## Communication Protocol

### Text mode

The driver starts up in this mode. This mode is easiest to use when quick communication with the board is needed without a client program. 

When the command TEXT is given, the driver communication protocol switches to text, and the response will be in text format. Commands can be given in lower or upper case, parameters separated by a space. Responses are given on one line, starting with a status code (200 for OK, errors are in the 300s and 400s.) Commands can be given in upper or lower case.

### JSON Lines mode

Give the JSONLINES command to switch to this mode.

When the command JSONLINES is given, the driver communication protocol switches to [JSON Lines](http://jsonlines.org/) format, and the response will be in JSON Lines format. Commands and Responses are a single map, with keys and values determine the command and parameters. The format is as follows (on separate lines for readability; in use, the entire JSON blob would be on its own line):

#### Commands:

```
{
    COMMAND: "<command-name>",
    PARAMETERS: [ <param1>, <param2>, ... ]
}
```

#### Responses:

```
{
    STATUS_CODE: <status-code>,
    STATUS_TEXT: "<status-text>",
    HEADERS: ["header1", "header2", ... ],
    DATA: [value1, value2, value3, ... ]
}
```

Here is an example exchange:

```
{"COMMAND" : "boardledon"}
{"STATUS_CODE" : 200, "STATUS_TEXT": "Ok" }

```

#### `rdat` and `rdatc` repsonses

For `rdata` and `rdatac` responses, in order to send data at high speeds, a special response format is used that is similar to the MessagePack format:

```
{
    C: <status-code>,
    D: "<base64-encoded byte-array>"
}
```

#### Software library

The Arduino driver uses the [ArduinoJson](https://arduinojson.org/) library for encoding and decoding JSON Lines data.


### MessagePack mode

Give the MESSAGEPACK command to switch to this mode.

When the command MESSAGEPACK is given, the driver communication protocol for `rdatac` responses ONLY switches to [MessagePack](https://msgpack.org) concise binary format. Commands are still given in JSON Lines format, and responses for all commands other than `rdatac` will be in JSON Lines. Responses for `rdatac` will be in MessagePack format. This mode is available to improve data transfer speed.

The format is as follows (on separate lines as JSON for readability, in use this would be packed as a binary structure):

#### Responses:

```
{
    C: <status-code>,
    T: "<status-text>",
    H: ["header1", "header2", ... ],
    D: [value1, value2, value3, ... ]
}
```

In MessagePack mode, status text is usually omitted except in the case of errors. Headers are optional and may or may not be provided.


#### Software library


The Arduino driver uses the [ArduinoJson](https://arduinojson.org/) library for encoding and decoding MessagePack data.



### Byte-array Format

The packed byte-array used for `rdata` and `rdatac` transfers has this format:

| position | function  | byte |
|----------|-----------|------|
| 00       | timestamp |    0 |
| 01       | timestamp |    1 |
| 02       | timestamp |    2 |
| 03       | timestamp |    3 |
| 04       | channel 1 |    0 |
| 05       | channel 1 |    1 |
| 06       | channel 1 |    2 |
| 07       | channel 2 |    0 |
| 08       | channel 2 |    1 |
| 09       | channel 2 |    2 |
| 10       | channel 3 |    0 |
| 11       | channel 3 |    1 |
| 12       | channel 3 |    2 |
| 13       | channel 4 |    0 |
| 14       | channel 4 |    1 |
| 15       | channel 4 |    2 |
| 16       | channel 5 |    0 |
| 17       | channel 5 |    1 |
| 18       | channel 5 |    2 |
| 19       | channel 6 |    0 |
| 20       | channel 6 |    1 |
| 21       | channel 6 |    2 |
| 22       | channel 7 |    0 |
| 23       | channel 7 |    1 |
| 24       | channel 7 |    2 |
| 25       | channel 8 |    0 |
| 26       | channel 8 |    1 |
| 27       | channel 8 |    2 |



## Python Host Software

The Python host software is designed to run on a laptop computer. There is a `hackeeg` driver Python module for communicating with the Arduino over the USB serial port, a command line client (`hackeeg_shell` and `hackeeg_shell.py`), and a demonstration and performance testing script (`hackeeg_test.py`). 

Using Python 3.6.5 on a 2017 Retina Macbook Pro, connected to an Arduino Due configured to use the SPI DMA included in the driver, the `hackeeg_test.py` program can read and transfer 8 channels of 24-bit resolution data at 16,384 samples per second, the maximum rate of the ADS1299 chip.

It requires the [PySerial](https://github.com/pyserial/pyserial) module.

## Hardware

If you are looking for a full Arduino shield with analog input capability, you might
be interested in the [HackEEG Shield](https://github.com/adamfeuer/hackeeg-shield).
The HackEEG shield is compatible with the ADS1298 or ADS1299.

For prototyping software, the [ADS1298 breakout board](https://github.com/adamfeuer/ADS1298-breakout)
might be useful. It is designed for the ADS1298, but should also work with the ADS1294, ADS1296, or ADS1299.


## Credits

This software would not be possible without the help of many people:

* Kendrick Shaw, Ace Medlock, and Eric Herman (parts of the ADS129x.h header file and some parts of the ADS129x driver, see [OpenHardwareExG project](https://github.com/OpenElectronicsLab/OpenHardwareExG) for more info.
* Chris Rorden (some parts of the ADS1298 driver)
* Stefan Rado (SerialCommand library)
* Steven Cogswell (SerialCommand library)
* William Greiman (SPI DMA library)
* Cristian Maglie (SPI DMA library)
* Bill Porter (SPI DMA library)
* Adam Rudd ([Base64](https://github.com/adamvr/arduino-base64) library)
* Benoît Blanchon ([ArduinoJson](https://arduinojson.org/) library)


If I forgot to credit you, please let me know!

If you have questions, comments, or improvements, I would love to know them! Pull requests welcome!

cheers <br/>
adam <br/>
<br/>
Adam Feuer <br/>
adam@starcat.io<br/>
[Starcat LLC](https://starcat.io)<br/>
Seattle, WA, USA <br/>