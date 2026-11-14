//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: src/dsmr_crc.c - CRC-16/ARC implementation
//-- Date: 2026-11-14 · Build: 1
//-- ==========================================================================

#include "dsmr_crc.h"

uint16_t dsmrCrc16ArcInit(void)
{
  return 0x0000;
}

uint16_t dsmrCrc16ArcUpdate(uint16_t crc, const uint8_t *data, size_t length)
{
  for (size_t index = 0; index < length; index++)
  {
    crc ^= (uint16_t)data[index];

    for (uint8_t bit = 0; bit < 8; bit++)
    {
      if ((crc & 0x0001u) != 0u)
      {
        crc = (uint16_t)((crc >> 1) ^ 0xA001u);
      }
      else
      {
        crc = (uint16_t)(crc >> 1);
      }
    }
  }

  return crc;
}

uint16_t dsmrCrc16Arc(const uint8_t *data, size_t length)
{
  return dsmrCrc16ArcUpdate(dsmrCrc16ArcInit(), data, length);
}
