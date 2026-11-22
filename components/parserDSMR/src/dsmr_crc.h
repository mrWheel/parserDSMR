//--    parserDSMR - CRC-16/ARC interface used by the P1 telegram trailer
//--    Copyright (C) 2026  Willem Aandewiel (mrWheel)
//--
//--    This program is free software: you can redistribute it and/or modify
//--    it under the terms of the GNU General Public License as published by
//--    the Free Software Foundation, either version 3 of the License, or
//--    (at your option) any later version.
//--
//--    This program is distributed in the hope that it will be useful,
//--    but WITHOUT ANY WARRANTY; without even the implied warranty of
//--    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//--    GNU General Public License for more details.
//--
//--    You should have received a copy of the GNU General Public License
//--    along with this program.  If not, see <https://www.gnu.org/licenses/>.

//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: src/dsmr_crc.h - CRC-16/ARC used by the P1 telegram trailer
//-- Date: 2026-11-21 · Build: 3
//--
//-- The CRC covers the original byte sequence from '/' through '!' inclusive,
//-- including the intervening CR LF bytes. The parameters are initial value
//-- 0x0000, polynomial 0x8005 in the reflected implementation value 0xA001,
//-- LSB-first input processing and final XOR 0x0000. The check value for the
//-- ASCII string "123456789" is 0xBB3D.
//-- ==========================================================================

#ifndef DSMR_CRC_H
#define DSMR_CRC_H

#include <stddef.h>
#include <stdint.h>

//-- Starts a CRC calculation.
uint16_t dsmrCrc16ArcInit(void);

//-- Feeds more bytes into a running CRC calculation.
uint16_t dsmrCrc16ArcUpdate(uint16_t crc, const uint8_t *data, size_t length);

//-- Convenience for a complete buffer.
uint16_t dsmrCrc16Arc(const uint8_t *data, size_t length);

#endif //-- DSMR_CRC_H
