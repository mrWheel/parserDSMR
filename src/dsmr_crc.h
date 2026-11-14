//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: src/dsmr_crc.h - CRC-16/ARC used by the P1 telegram trailer
//-- Date: 2026-11-14 · Build: 1
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
