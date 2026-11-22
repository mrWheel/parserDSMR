//--    parserDSMR - CRC-16/ARC calculation used by the P1 telegram trailer
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
//-- File: src/dsmr_crc.c - CRC-16/ARC implementation
//-- Date: 2026-11-21 · Build: 3
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
