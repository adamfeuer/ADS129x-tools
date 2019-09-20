# ADS129x-tools

This is a collection of software for working with the TI ADS129x series of analog to digital
converter chips.

The ADS1299 is a 24-bit 8-channel ADC meant specifically for EEG, with a 24x programmable gain 
amplifier and much of the analog circuitry needed for EEG. It is capable of digitizing 16,000 samples
per second at 24 bit resolution. The ADS1299-4 is a 4-channel version of the ADS1299; the ADS1299-6 is a 6-channel version.

http://www.ti.com/product/ads1299

The ADS1298 is a 24-bit, 8-channel ADC chip with SPI interface, and 12x programmable gain amplifiers,
meant for ECG and EEG:

http://www.ti.com/product/ads1298

The ADS1294 is a 4-channel version; ADS1296 is a 6 channel version. 

These chips are ideally suited for digitizing biological signals.

## Arduino drivers

The ads129x-driver/ directory contains an Arduino sketch and associated C/C++ files that make up a driver for ADS129x chips. So far it has only been tested on the ADS1299 and ADS1298, but should work on the other models. This driver has been tested on the Arduino Due and Mega2560, but should also work on other Arduinos. The DMA mode can only be used on the Arduino Due.

The driver has a text-mode interface, so can be used without any client software – just open up a serial port to the SAM3X8E native USB port (baud rate 115200, line endings NL+CR). It also has a JSONLines mode for easy parsing by client programs and a MessagePack mode for efficient binary communication. The driver can read from the ADS1299 at 16,000 samples per second, and can send that data on to the host via the Arduino Due's USB 2.0 High Speed connection.

It can be configured to use the Arduino library's software SPI (without DMA), and can do 8,000 samples per second in that configuration on the Arduino Due. Other Arduinos will have lower performance.

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
* HEX – RDATA commands will encode data in hex.
* HELP – prints a list of available commands.



## General Operation

The ADS129x chips are configured by reading and writing registers. See the chip datasheet for more information about configuring the ADS129x and reading data from it.

If the host program (the program that reads data from the driver) does not pull data from the serial or USB interface fast enough, the driver
will block on sending when the serial or USB buffers fill up. This will cause the driver to lose samples. 


In most applications, the usage will go something like this:

```
communication_protocol_message_pack()
board_led_on()
sleep(1)
board_led_off()
setup_ads1299()
rdatac()
while 1:
  my_data = data()
  my_display_data(my_data)

```

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


The Arduino driver uses the [ArduinoJson](https://arduinojson.org/) library for encoding and decoding JSON Lines data.


## Python Host Software

The Python host software is designed to run on a laptop computer. Right now it is a rudimentary performance testing script. Using Python 3.x on
2012 Retina Macbook Pro, it can read 8,000 samples per second. Under PyPy, it can read 16,000 samples per second.

It requires the PySerial module.

## Hardware

If you are looking for a full Arduino shield with analog input capability, you might
be interested in the [HackEEG Shield](https://github.com/adamfeuer/hackeeg-shield).
The HackEEG shield is compatible with the ADS1298 or ADS1299.

For prototyping software, the [ADS1298 breakout board](https://github.com/adamfeuer/ADS1298-breakout)
might be useful. It is designed for the ADS1298, but should also work with the ADS1294, ADS1296, or ADS1299.


## Credits

This software would not be possible without the help of many people. The following people helped with this driver:

* Kendrick Shaw, Ace Medlock, and Eric Herman (ADS1298.h header file and some parts of the ADS1298 driver, see [OpenHardwareExG project](https://github.com/OpenElectronicsLab/OpenHardwareExG) for more info
* Chris Rorden (some parts of the ADS1298 driver)
* Stefan Rado (SerialCommand library)
* Steven Cogswell (SerialCommand library)
* William Greiman (SPI DMA library)
* Cristian Maglie (SPI DMA library)
* Bill Porter (SPI DMA library)
* Adam Rudd (Base64 library)
* Benoît Blanchon ([ArduinoJson](https://arduinojson.org/) library)


If I forgot to credit you, please let me know!

If you have questions, comments, or improvements, I would love to know them! Pull requests welcome!

cheers <br>
adam <br>
Adam Feuer <br>
adam@starcat.io<br>
[Starcat LLC](https://starcat.io)<br>
Seattle, WA, USA <br>