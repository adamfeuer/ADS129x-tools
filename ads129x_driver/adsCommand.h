/*
 * adsCommand.h
 *
 * Copyright (c) 2013-2019 by Adam Feuer <adam@adamfeuer.com>
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

#ifndef _ADS_COMMAND_H
#define _ADS_COMMAND_H

#include "Arduino.h"

// constants define pins on Arduino 

// Arduino Due
// HackEEG Shield v1.5.0
const int IPIN_PWDN = 33;
const int PIN_CLKSEL = 48;
const int IPIN_RESET = 47;

const int PIN_START = 59;
const int IPIN_DRDY = 25;
const int PIN_CS = 4;
//const int PIN_DOUT = 11;  //SPI out
//const int PIN_DIN = 12;   //SPI in
//const int PIN_SCLK = 13;  //SPI clock

void adcWreg(int reg, int val);
void adcSendCommand(int cmd);
void adcSendCommandLeaveCsActive(int cmd);
int adcRreg(int reg);

#endif // _ADS_COMMAND_H
