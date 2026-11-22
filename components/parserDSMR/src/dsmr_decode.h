//--    parserDSMR - OBIS line decoding interface of the portable core
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
//-- File: src/dsmr_decode.h - OBIS line decoding
//-- Date: 2026-11-21 · Build: 3
//--
//-- The decoder walks the stored candidate line by line. It validates the
//-- identification line, the blank line and the CR LF boundaries of every
//-- data line, resolves each OBIS code against the profile field table and
//-- fills the ordered received-field array. Telegram fields start empty for
//-- each candidate.
//-- ==========================================================================

#ifndef DSMR_DECODE_H
#define DSMR_DECODE_H

#include "dsmr_parser.h"

#define DSMR_DECODE_MAX_PENDING_DIAG 4

typedef struct
{
  const dsmrProfile *configuredProfile;
  const dsmrParserConfig *config;
  dsmrTelegramField *fieldArray;
  uint16_t fieldCapacity;
} dsmrDecodeInput;

typedef struct
{
  bool accepted;
  dsmrDiagReason reason;
  size_t position;
  const char *obis;
  const dsmrProfile *resolvedProfile;
  const char *reportedVersion;
  const char *meterIdentification;
  dsmrTelegramField *fields;
  uint16_t fieldCount;
  dsmrDiagnostic pendingDiag[DSMR_DECODE_MAX_PENDING_DIAG];
  uint8_t pendingDiagCount;
} dsmrDecodeOutput;

//-- Decodes the stored candidate. The buffer is writable: the decoder places
//-- terminators so every view into the telegram stays a plain C string. The
//-- body region is buffer[0 .. bodyLength), which starts with '/' and ends
//-- just before the '!'. On rejection output->accepted is false and reason,
//-- position and obis describe the failure. Accepted M-Bus device type
//-- mismatches do not reject the telegram; they are reported through
//-- pendingDiag in addition to the accepted result.
void dsmrDecodeTelegram(uint8_t *buffer, size_t bodyLength, const dsmrDecodeInput *input, dsmrDecodeOutput *output);

#endif //-- DSMR_DECODE_H
