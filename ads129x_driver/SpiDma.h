/*
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
#ifndef SPI_DMA_H
#define SPI_DMA_H

// SPI clock divider - 1-255, divides 84Mhz system clock 
// 21 = 4 Mhz
// 6 = 14 Mhz
// 5 = 16.8 Mhz
// 4 = 21 Mhz
// ADS1299 needs the SPI clock to be 20Mhz or less

#define SPI_CLOCK_DIVIDER 5

void spiBegin(uint8_t csPin);

void spiInit(uint8_t bitOrder, uint8_t spiMode, uint8_t spiClockDivider);

uint8_t spiRec();

uint8_t spiRec(uint8_t *buf, size_t len);

void spiSend(uint8_t b);

void spiSend(const uint8_t *buf, size_t len);

#endif  // SPI_DMA_H
