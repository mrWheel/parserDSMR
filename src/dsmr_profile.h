//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: src/dsmr_profile.h - profile lookup helpers for the core
//-- Date: 2026-11-14 · Build: 1
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
