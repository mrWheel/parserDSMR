//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: src/dsmr_profile.c - built-in profile definitions and registry
//-- Date: 2026-11-14 · Build: 1
//--
//-- Profile identifiers: nl.dsmr.4.0, nl.dsmr.4.2, nl.dsmr.5.0, nl.dsmr.4
//-- (automatic 4.0/4.2 selection from the reported version), be.emucs.1.x
//-- and be.emucs.2.1.
//--
//-- Sources of the object tables: the P1 Companion Standards DSMR 4.0.4,
//-- 4.0.7, 4.2.2 and 5.0.2 (Netbeheer Nederland) and the eMUCs 1.7.1 and
//-- 2.1.1 documents (Fluvius), as referenced in projectPrompt.md section 8.
//-- The Dutch tables cover the objects required by projectPrompt.md section 5
//-- for the first release. The Belgian tables are an initial coverage pending
//-- the full verified OBIS inventory; fixtures carry the source and revision
//-- of every accepted object.
//-- ==========================================================================

#include "dsmr_profile.h"

#include <string.h>

//-- --------------------------------------------------------------------------
//-- Shared Dutch DSMR 4.x/5.0 field groups
//-- --------------------------------------------------------------------------

//-- Identity, version and timestamps.
static const dsmrFieldDefinition nlIdentityFields[] =
{
  { .obis = "1-3:0.2.8", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.1.1", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:1.0.0", .type = DSMR_FIELD_TYPE_TIMESTAMP, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.13.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.13.1", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Energy readings, tariff indicator and instantaneous totals.
static const dsmrFieldDefinition nlEnergyFields[] =
{
  { .obis = "1-0:1.8.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:1.8.2", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.2", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:1.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "0-0:96.14.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Aggregate counters, published by DSMR 5.0.
static const dsmrFieldDefinition nlAggregateFields[] =
{
  { .obis = "1-0:1.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
};

//-- Phase measurements for three-phase meters.
static const dsmrFieldDefinition nlPhaseFields[] =
{
  { .obis = "1-0:21.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:22.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:23.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:24.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:25.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:26.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:32.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 1, .groupCount = 1 },
  { .obis = "1-0:52.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 1, .groupCount = 1 },
  { .obis = "1-0:72.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 1, .groupCount = 1 },
  { .obis = "1-0:31.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 1, .groupCount = 1 },
  { .obis = "1-0:51.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 1, .groupCount = 1 },
  { .obis = "1-0:71.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 1, .groupCount = 1 },
};

//-- Power quality counters and the structured failure log.
static const dsmrFieldDefinition nlPowerQualityFields[] =
{
  { .obis = "0-0:96.7.21", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.7.9", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "1-0:99.97.0", .type = DSMR_FIELD_TYPE_LOG, .unit = NULL, .maxScale = 0, .groupCount = 0, .logPrefixGroups = 1, .logEntryGroups = 3 },
};

//-- M-Bus submeter objects. The channel digit is a wildcard so the entries
//-- cover channels 1 through 4. Units follow the declared device type at
//-- decode time; m3 is the default for gas and water readings.
static const dsmrFieldDefinition nlMbusFields[] =
{
  { .obis = "0-?:24.1.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-?:24.2.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "m3", .maxScale = 3, .groupCount = 1 },
  { .obis = "0-?:24.3.1", .type = DSMR_FIELD_TYPE_TIMESTAMP, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- --------------------------------------------------------------------------
//-- Belgian eMUCs field groups
//-- --------------------------------------------------------------------------

//-- Identity, version and timestamps. The version object is 0-0:96.1.4.
static const dsmrFieldDefinition beIdentityFields[] =
{
  { .obis = "0-0:96.1.4", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.1.1", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:1.0.0", .type = DSMR_FIELD_TYPE_TIMESTAMP, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.13.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.13.1", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Energy readings, tariff indicator and instantaneous totals.
static const dsmrFieldDefinition beEnergyFields[] =
{
  { .obis = "1-0:1.8.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:1.8.2", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.2", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:1.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "0-0:96.14.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Phase measurements. Belgian current readings keep two fractional digits.
static const dsmrFieldDefinition bePhaseFields[] =
{
  { .obis = "1-0:21.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:22.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:23.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:24.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:25.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:26.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:32.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 1, .groupCount = 1 },
  { .obis = "1-0:52.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 1, .groupCount = 1 },
  { .obis = "1-0:72.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 1, .groupCount = 1 },
  { .obis = "1-0:31.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 2, .groupCount = 1 },
  { .obis = "1-0:51.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 2, .groupCount = 1 },
  { .obis = "1-0:71.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 2, .groupCount = 1 },
};

//-- Power quality counters and the structured failure log.
static const dsmrFieldDefinition bePowerQualityFields[] =
{
  { .obis = "0-0:96.7.21", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.7.9", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "1-0:99.97.0", .type = DSMR_FIELD_TYPE_LOG, .unit = NULL, .maxScale = 0, .groupCount = 0, .logPrefixGroups = 2, .logEntryGroups = 4 },
};

//-- M-Bus submeter objects. Belgian gas readings use 0-n:24.2.3 and eMUCs
//-- 1.7.1 water readings use 0-n:24.2.1.
static const dsmrFieldDefinition beMbusFields[] =
{
  { .obis = "0-?:24.1.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-?:24.2.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "m3", .maxScale = 3, .groupCount = 1 },
  { .obis = "0-?:24.2.3", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "m3", .maxScale = 3, .groupCount = 1 },
  { .obis = "0-?:24.3.1", .type = DSMR_FIELD_TYPE_TIMESTAMP, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Belgian demand objects, defined for eMUCs 2.1.
static const dsmrFieldDefinition beDemandFields[] =
{
  { .obis = "1-0:1.4.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:1.6.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 2 },
  { .obis = "0-0:98.1.0", .type = DSMR_FIELD_TYPE_LOG, .unit = NULL, .maxScale = 0, .groupCount = 0, .logPrefixGroups = 1, .logEntryGroups = 2 },
};

//-- --------------------------------------------------------------------------
//-- Built-in profiles
//-- --------------------------------------------------------------------------

#define DSMR_ARRAY_COUNT(table) (uint16_t)(sizeof(table) / sizeof((table)[0]))

static const dsmrFieldBlock nl40Blocks[] =
{
  { nlIdentityFields, DSMR_ARRAY_COUNT(nlIdentityFields) },
  { nlEnergyFields, DSMR_ARRAY_COUNT(nlEnergyFields) },
  { nlPhaseFields, DSMR_ARRAY_COUNT(nlPhaseFields) },
  { nlPowerQualityFields, DSMR_ARRAY_COUNT(nlPowerQualityFields) },
  { nlMbusFields, DSMR_ARRAY_COUNT(nlMbusFields) },
};

static const dsmrFieldBlock nl42Blocks[] =
{
  { nlIdentityFields, DSMR_ARRAY_COUNT(nlIdentityFields) },
  { nlEnergyFields, DSMR_ARRAY_COUNT(nlEnergyFields) },
  { nlPhaseFields, DSMR_ARRAY_COUNT(nlPhaseFields) },
  { nlPowerQualityFields, DSMR_ARRAY_COUNT(nlPowerQualityFields) },
  { nlMbusFields, DSMR_ARRAY_COUNT(nlMbusFields) },
};

static const dsmrFieldBlock nl50Blocks[] =
{
  { nlIdentityFields, DSMR_ARRAY_COUNT(nlIdentityFields) },
  { nlAggregateFields, DSMR_ARRAY_COUNT(nlAggregateFields) },
  { nlEnergyFields, DSMR_ARRAY_COUNT(nlEnergyFields) },
  { nlPhaseFields, DSMR_ARRAY_COUNT(nlPhaseFields) },
  { nlPowerQualityFields, DSMR_ARRAY_COUNT(nlPowerQualityFields) },
  { nlMbusFields, DSMR_ARRAY_COUNT(nlMbusFields) },
};

static const dsmrFieldBlock be1xBlocks[] =
{
  { beIdentityFields, DSMR_ARRAY_COUNT(beIdentityFields) },
  { beEnergyFields, DSMR_ARRAY_COUNT(beEnergyFields) },
  { bePhaseFields, DSMR_ARRAY_COUNT(bePhaseFields) },
  { bePowerQualityFields, DSMR_ARRAY_COUNT(bePowerQualityFields) },
  { beMbusFields, DSMR_ARRAY_COUNT(beMbusFields) },
};

static const dsmrFieldBlock be21Blocks[] =
{
  { beIdentityFields, DSMR_ARRAY_COUNT(beIdentityFields) },
  { beEnergyFields, DSMR_ARRAY_COUNT(beEnergyFields) },
  { bePhaseFields, DSMR_ARRAY_COUNT(bePhaseFields) },
  { bePowerQualityFields, DSMR_ARRAY_COUNT(bePowerQualityFields) },
  { beMbusFields, DSMR_ARRAY_COUNT(beMbusFields) },
  { beDemandFields, DSMR_ARRAY_COUNT(beDemandFields) },
};

//-- UART defaults for every built-in profile are 115200 baud, 8 data bits,
//-- no parity and 1 stop bit. The parity encoding is 0 none, 1 odd, 2 even.
static const char *const nl40Versions[] = { "40", NULL };
static const char *const nl42Versions[] = { "42", NULL };
static const char *const nl50Versions[] = { "50", NULL };
static const char *const nl4Versions[] = { "40", "42", NULL };
static const char *const be1xVersions[] = { "50213", "50214", "50215", "50216", "50217", NULL };
static const char *const be21Versions[] = { "50221", NULL };

static const dsmrProfile profileNlDsmr40 =
{
  .profileId = "nl.dsmr.4.0",
  .countryCode = "NL",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = nl40Versions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 10000,
  .fieldBlocks = nl40Blocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(nl40Blocks),
  .autoProfiles = NULL,
};

static const dsmrProfile profileNlDsmr42 =
{
  .profileId = "nl.dsmr.4.2",
  .countryCode = "NL",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = nl42Versions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 10000,
  .fieldBlocks = nl42Blocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(nl42Blocks),
  .autoProfiles = NULL,
};

static const dsmrProfile profileNlDsmr50 =
{
  .profileId = "nl.dsmr.5.0",
  .countryCode = "NL",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = nl50Versions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 1000,
  .fieldBlocks = nl50Blocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(nl50Blocks),
  .autoProfiles = NULL,
};

//-- The automatic profile selects the 4.0 or 4.2 field definitions from the
//-- version field of a CRC-validated telegram. Its own field blocks stay
//-- empty because decoding always runs on the resolved profile.
static const dsmrProfile *const nl4AutoProfiles[] = { &profileNlDsmr40, &profileNlDsmr42, NULL };

static const dsmrProfile profileNlDsmr4 =
{
  .profileId = "nl.dsmr.4",
  .countryCode = "NL",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = nl4Versions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 10000,
  .fieldBlocks = NULL,
  .fieldBlockCount = 0,
  .autoProfiles = nl4AutoProfiles,
};

static const dsmrProfile profileBeEmucs1x =
{
  .profileId = "be.emucs.1.x",
  .countryCode = "BE",
  .versionObis = "0-0:96.1.4",
  .acceptedVersions = be1xVersions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 1000,
  .fieldBlocks = be1xBlocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(be1xBlocks),
  .autoProfiles = NULL,
};

static const dsmrProfile profileBeEmucs21 =
{
  .profileId = "be.emucs.2.1",
  .countryCode = "BE",
  .versionObis = "0-0:96.1.4",
  .acceptedVersions = be21Versions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 1000,
  .fieldBlocks = be21Blocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(be21Blocks),
  .autoProfiles = NULL,
};

static const dsmrProfile *const builtinProfiles[] =
{
  &profileNlDsmr40,
  &profileNlDsmr42,
  &profileNlDsmr50,
  &profileNlDsmr4,
  &profileBeEmucs1x,
  &profileBeEmucs21,
};

static const char *const builtinProfileIds[] =
{
  "nl.dsmr.4.0",
  "nl.dsmr.4.2",
  "nl.dsmr.5.0",
  "nl.dsmr.4",
  "be.emucs.1.x",
  "be.emucs.2.1",
  NULL,
};

const dsmrProfile *dsmrProfileLookup(const char *profileId)
{
  if (profileId == NULL)
  {
    return NULL;
  }

  for (size_t index = 0; index < (sizeof builtinProfiles / sizeof builtinProfiles[0]); index++)
  {
    if (strcmp(builtinProfiles[index]->profileId, profileId) == 0)
    {
      return builtinProfiles[index];
    }
  }

  return NULL;
}

const char *const *dsmrProfileListIds(void)
{
  return builtinProfileIds;
}

//-- Matches a received OBIS code against a definition code that may contain
//-- '?' as a single-digit wildcard.
static bool obisMatches(const char *definition, const char *received)
{
  while ((*definition != '\0') && (*received != '\0'))
  {
    if ((*definition != '?') && (*definition != *received))
    {
      return false;
    }

    definition++;
    received++;
  }

  return (*definition == '\0') && (*received == '\0');
}

const dsmrFieldDefinition *dsmrProfileFindField(const dsmrProfile *profile, const char *obis)
{
  if ((profile == NULL) || (obis == NULL))
  {
    return NULL;
  }

  for (uint16_t block = 0; block < profile->fieldBlockCount; block++)
  {
    const dsmrFieldDefinition *entries = profile->fieldBlocks[block].entries;
    uint16_t count = profile->fieldBlocks[block].count;

    for (uint16_t index = 0; index < count; index++)
    {
      if (obisMatches(entries[index].obis, obis))
      {
        return &entries[index];
      }
    }
  }

  return NULL;
}

//-- True when the text consists only of decimal digits.
static bool allDigits(const char *text)
{
  if (*text == '\0')
  {
    return false;
  }

  while (*text != '\0')
  {
    if ((*text < '0') || (*text > '9'))
    {
      return false;
    }

    text++;
  }

  return true;
}

//-- True when the version text has the same length as one accepted version
//-- and shares its leading digit, which identifies the protocol family for
//-- the built-in profiles ("4" or "5" for Dutch, "502" prefix length for
//-- Belgian five-digit values).
static bool sameFamilyVersion(const dsmrProfile *profile, const char *versionText)
{
  size_t receivedLength = strlen(versionText);

  for (uint8_t index = 0; profile->acceptedVersions[index] != NULL; index++)
  {
    const char *accepted = profile->acceptedVersions[index];

    if ((strlen(accepted) == receivedLength) && (accepted[0] == versionText[0]))
    {
      return true;
    }
  }

  return false;
}

const dsmrProfile *dsmrProfileResolveForVersion(const dsmrProfile *profile, const char *versionText, bool acceptUnknownMinorVersion)
{
  if ((profile == NULL) || (versionText == NULL))
  {
    return NULL;
  }

  for (uint8_t index = 0; profile->acceptedVersions[index] != NULL; index++)
  {
    if (strcmp(profile->acceptedVersions[index], versionText) == 0)
    {
      //-- An automatic profile hands decoding to the concrete profile that
      //-- belongs to the accepted version.
      if (profile->autoProfiles != NULL)
      {
        return profile->autoProfiles[index];
      }

      return profile;
    }
  }

  //-- Compatibility option: after framing and CRC validation an unrecognized
  //-- minor version inside the same family is accepted and the configured
  //-- profile table stays in force. The original version is reported.
  if (acceptUnknownMinorVersion && (profile->autoProfiles == NULL) && allDigits(versionText) && sameFamilyVersion(profile, versionText))
  {
    return profile;
  }

  return NULL;
}
