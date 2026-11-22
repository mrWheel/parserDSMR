//--    parserDSMR - value and timestamp conversion interface of the portable core
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
//-- File: src/dsmr_value.h - value and timestamp conversion of the core
//-- Date: 2026-11-21 · Build: 3
//--
//-- Decimal values are converted without locale-dependent parsing. The
//-- mantissa and scale preserve the published precision and allow overflow
//-- checks. Timestamps keep their source text and the S/W indicator. Two
//-- digit years resolve as 20YY, the documented century rule of the core.
//-- ==========================================================================

#ifndef DSMR_VALUE_H
#define DSMR_VALUE_H

#include "dsmr_parser.h"

//-- Parses a decimal value such as "123.456", "-2.5", "0001" or "FF".
//-- An empty string yields DSMR_VALUE_ABSENT. The string "FF" (any case)
//-- marks a protocol-invalid value. Returns 1 on success, 0 on malformed
//-- text and -1 on mantissa overflow, so the caller can raise a distinct
//-- diagnostic.
int dsmrValueParseDecimal(const char *text, uint8_t maxScale, dsmrValue *value);

//-- Parses an integer value. Leading zeroes are kept by the caller as text
//-- when needed. Returns 1, 0 or -1 like the decimal parser.
int dsmrValueParseInteger(const char *text, dsmrValue *value);

//-- Stores a text value, such as an equipment identifier or a message.
void dsmrValueSetText(dsmrValue *value, const char *text);

//-- Leaves the value absent.
void dsmrValueSetAbsent(dsmrValue *value);

//-- Returns ten to the power of scale for a value's scale. Scales above the
//-- representable range clamp to the largest power of ten.
int64_t dsmrValuePow10(uint8_t scale);

//-- Parses a compact timestamp of the form YYMMDDHHMMSS with an optional
//-- signed offset and an optional S or W indicator. The raw text is stored in
//-- the value as text and the parsed fields are reported separately. Calendar
//-- fields are validated. Returns false when the text cannot be parsed or
//-- when the calendar date is invalid.
bool dsmrTimestampParse(const char *text, dsmrValue *value, dsmrTimestamp *timestamp);

#endif //-- DSMR_VALUE_H
