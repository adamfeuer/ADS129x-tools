ADS129x-tools
=============

This is a collection of software for working with the TI ADS129x series of analog to digital
converter chips.

The ADS1299 is a 24-bit 8-channel ADC meant specifically for EEG, with a 24x programmable gain 
amplifier and much of the analog circuitry needed for EEG. It is capable of 16,000 samples
per second at 24 bit resolution.

http://www.ti.com/product/ads1299

The ADS1298 is a 24-bit, 8-channel ADC chip with SPI interface, and 12x programmable gain amplifiers,
meant for ECG and EEG:

http://www.ti.com/product/ads1298

The ADS1294 is a 4-channel version; ADS1296 is a 6 channel version.

These chips are ideally suited for digitizing biological signals.

Arduino Due drivers
===================

The ads129x-driver/ directory contains an Arduino sketch and associated C/C++ files that make up a driver
for ADS129x chips. So far it has only been tested on the ADS1299 and ADS1298, but should work on the other models.

The driver is a text-mode driver, so can be used without any client software - just open up a serial port
to the SAM3X8E native USB port (baud rate 115200, line endings NL+CR). The driver can read from the ADS129x
at 16,000 samples per second, and can send that data on to the host via the Arduino DUE's USB 2.0 High Speed
connection.

It can be configured to use the Arduino library's SPI software (without DMA), and can do 8,000 samples per second
in that configuration.

Samples are encoded using the base64 encoding by default. For more information about that, see this 
Wikipedia page:

http://en.wikipedia.org/wiki/Base64


Using the Driver
================

The commands that are available are:

* RREG - read register. Takes two hex digits as an argument (one byte, for example: FF). Argument 1 is the register to read.
Returns a single hex-encoded byte (for example, 0E) that represents the contents of the register.
* WREG - write register. Takes two hex-encoded bytes as arguments, separated by a space. Argument 1 is the register to write, argument 2 is the register value.
* RDATA - read one sample of data from the ADS129x chip. Returns 3 bytes of header, plus 3 bytes x number of channels (8 for ADS1298 or ADS1299), encoded using base64.
* SDATAC - stop read data continuous mode. 
* RDATAC - start read data continuous mode.
* VERSION - reports the driver version
* LEDON - turns on the Arduino Due onboard LED.
* LEDOFF - turns off the Arduino Due onboard LED.
* BOARDLEDON - turns on the HackEEG Shield LED. (HackEEG shield only) 
* BOARDLEDOFF - turns off the HackEEG Shield LED. (HackEEG shield only) 
* BASE64 - RDATA/RDATAC commands will encode data in base64.
* HEX - RDATA/RDATAC commands will encode data in hex.
* HELP - prints a list of available commands.

Commands can be given in lower or upper case.

See the chip datasheet for more information about configuring the ADS129x and reading data from it.

If the host program (the program that reads data from the driver) does not pull data from the serial or USB interface fast enough, the driver
will block on sending when the serial or USB buffers fill up. This will cause the driver to lose samples. 


Python Host Software
====================

The Python host software is designed to run on a laptop computer. Right now it is a rudimentary performance testing script. Using Python 3.x on
2012 Retina Macbook Pro, it can read 8,000 samples per second. Under PyPy, it can read 16,000 samples per second.

It requires the PySerial module.

Hardware
========

If you are looking for a full Arduino shield with analog input capability, you might
be interested in the [HackEEG Shield](https://github.com/adamfeuer/hackeeg-shield).
The HackEEG shield is compatible with the ADS1298 or ADS1299.

For prototyping software, the [ADS1298 breakout board](https://github.com/adamfeuer/ADS1298-breakout)
might be useful. It is designed for the ADS1298, but should also work with the ADS1294, ADS1296, or ADS1299.


Credits
=======

This software would not be possible without the help of many people. The following people contributed software to this driver:

* Chris Rorden (original ADS1298 driver)
* Stefan Rado (SerialCommand library)
* Steven Cogswell (SerialCommand library)
* Adam Rudd (Base64 library)
* William Greiman (SPI DMA library)
* Cristian Maglie (SPI DMA library)
* Bill Porter (SPI DMA library)

If I forgot to credit you, please let me know!

If you have questions, comments, or improvements, I would love to know them! Pull requests welcome!

cheers <br>
adam <br>
Adam Feuer <br>
adam@starcat.io<br>
[StarCat Biosignals](https://starcat.io)<br>
Seattle, WA, USA <br>
