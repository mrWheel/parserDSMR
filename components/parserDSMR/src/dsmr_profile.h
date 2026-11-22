//--    parserDSMR - profile lookup helpers for the portable core
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
//-- File: src/dsmr_profile.h - profile lookup helpers for the core
//-- Date: 2026-11-21 · Build: 3
//-- ==========================================================================

#ifndef DSMR_PROFILE_H
#define DSMR_PROFILE_H

#include "dsmr_parser.h"

//-- Finds the definition for a received OBIS code. A definition may use '?'
//-- as a single-digit wildcard, for example "0-?:24.1.0" for the M-Bus
//-- channels 1 through 4. Returns NULL when the profile does not define the
//-- code.
const dsmrFieldDefinition *dsmrProfileFindField(const dsmrProfile *profile, const char *obis);

//-- Resolves the profile that governs decoding for a reported version text.
//-- For an automatic profile such as nl.dsmr.4 the concrete profile is
//-- selected from the version. Returns NULL when the version is not accepted
//-- and the compatibility option is off. When the option is on and the text
//-- is all digits with the same length as an accepted version of the same
//-- leading digit, the configured profile stays in force and the original
//-- version is reported.
const dsmrProfile *dsmrProfileResolveForVersion(const dsmrProfile *profile, const char *versionText, bool acceptUnknownMinorVersion);

#endif //-- DSMR_PROFILE_H
