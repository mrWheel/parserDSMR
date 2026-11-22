//--    parserDSMR - ASCII telegram framing state machine interface
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
//-- File: src/dsmr_framing.h - ASCII framing state machine
//-- Date: 2026-11-21 · Build: 3
//--
//-- The framer walks the explicit states of projectPrompt.md section 4:
//-- seeking a header, receiving the body and validating the trailer. A
//-- leading slash at a valid header position starts a candidate. The framer
//-- stores the candidate and maintains the running CRC-16/ARC over the bytes
//-- from '/' through '!' inclusive. Timeouts and telegram content validation
//-- are the responsibility of the parser.
//-- ==========================================================================

#ifndef DSMR_FRAMING_H
#define DSMR_FRAMING_H

#include "dsmr_parser.h"

typedef enum
{
  DSMR_FRAMING_NONE = 0,
  DSMR_FRAMING_FRAME_COMPLETE,
  DSMR_FRAMING_FRAME_TOO_LONG,
  DSMR_FRAMING_TRAILER_MISSING_CRC,
  DSMR_FRAMING_TRAILER_MALFORMED,
} dsmrFramingOutcome;

//-- Binds the framer to a caller-owned telegram buffer and starts seeking.
void dsmrFramingInit(dsmrFraming *framing, uint8_t *buffer, size_t capacity);

//-- Discards the current candidate and returns to seeking a header.
void dsmrFramingRestart(dsmrFraming *framing);

//-- Feeds one received byte. On DSMR_FRAMING_FRAME_COMPLETE the caller reads
//-- the stored bytes from framing->buffer, the CRC-covered length from
//-- framing->bodyLength, the CRC itself from framing->runningCrc and the
//-- transmitted trailer digits from framing->trailerCrc and
//-- framing->crcPresent. The caller must then call dsmrFramingRestart.
dsmrFramingOutcome dsmrFramingByte(dsmrFraming *framing, dsmrCrcPolicy crcPolicy, bool acceptLfLineEndings, uint8_t byte);

#endif //-- DSMR_FRAMING_H
