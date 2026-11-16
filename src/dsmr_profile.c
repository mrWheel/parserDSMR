//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: src/dsmr_profile.c - built-in profile definitions and registry
//-- Date: 2026-11-16 · Build: 5
//--
//-- Profile identifiers: nl.dsmr.4.0, nl.dsmr.4.2, nl.dsmr.5.0, nl.dsmr.4
//-- (automatic 4.0/4.2 selection from the reported version), be.emucs.1.x
//-- and be.emucs.2.1, plus the European DSMR-style ports hu.dsmr.5.0,
//-- se.dsmr.5.0, fi.dsmr.5.0, lt.dsmr.5.0, ie.dsmr.5.0, lu.dsmr.5.0 and
//-- ch.dsmr.5.0.
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

//-- Phase measurements for three-phase meters. The codes follow the P1
//-- objects published by these meters and cross-checked against dsmr2Lib's
//-- fields2.h (power_delivered_l1 1-0:21.7.0, power_returned_l1 1-0:22.7.0,
//-- the 4x/6x pairs for L2/L3, current_l1 1-0:31.7.0, voltage_l1
//-- 1-0:32.7.0) and against this repository's verified sample fixture:
//-- delivered kW per phase on 21/41/61.7.0, returned kW per phase on
//-- 22/42/62.7.0, current on 31/51/71.7.0 and voltage on 32/52/72.7.0.
static const dsmrFieldDefinition nlPhaseFields[] =
{
  { .obis = "1-0:21.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:41.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:61.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:22.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:42.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:62.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:31.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:51.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:71.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:32.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 2, .groupCount = 1 },
  { .obis = "1-0:52.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 2, .groupCount = 1 },
  { .obis = "1-0:72.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 2, .groupCount = 1 },
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

//-- Phase measurements. Same objects as the Dutch phase group (cross-checked
//-- against dsmr2Lib and the verified fixtures); current and voltage accept
//-- up to two fractional digits, power up to three.
static const dsmrFieldDefinition bePhaseFields[] =
{
  { .obis = "1-0:21.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:41.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:61.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:22.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:42.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:62.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:31.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:51.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:71.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:32.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 2, .groupCount = 1 },
  { .obis = "1-0:52.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 2, .groupCount = 1 },
  { .obis = "1-0:72.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 2, .groupCount = 1 },
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
//-- Shared European field groups (HU, SE, FI, LT, IE)
//--
//-- Source: assets/Europese_P1_OBIS_Codes_Overzicht.pdf, the reference table
//-- of OBIS objects published on European RJ12 P1 ports for NL, BE, HU, SE,
//-- FI, LT and IE. ALGEMEEN objects are present in all listed countries,
//-- LAND CODE objects only in the named ones. The blocks below are
//-- composable so each country profile assembles exactly the objects its
//-- standard publishes. Decimal fields accept up to maxScale fraction
//-- digits so any published precision decodes without overflow.
//-- --------------------------------------------------------------------------

//-- Identity and version for the 1-3:0.2.8 family used outside Belgium.
//-- 0-0:96.1.4 replaces 1-3:0.2.8 only in Belgium, so these profiles carry
//-- the DSMR-style version object.
static const dsmrFieldDefinition euIdentityFields[] =
{
  { .obis = "1-3:0.2.8", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.1.1", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:1.0.0", .type = DSMR_FIELD_TYPE_TIMESTAMP, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.14.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- User text message, published by NL, BE, HU, SE and FI.
static const dsmrFieldDefinition euTextMessageFields[] =
{
  { .obis = "0-0:96.13.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Energy readings and instantaneous totals, published by all countries.
static const dsmrFieldDefinition euEnergyFields[] =
{
  { .obis = "1-0:1.8.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:1.8.2", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.2", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:1.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
};

//-- Aggregate counters over all tariffs: SE, FI, HU and LT. NL and BE
//-- publish only the per-tariff readings.
static const dsmrFieldDefinition euAggregateFields[] =
{
  { .obis = "1-0:1.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
};

//-- Aggregate reactive energy counters: SE, FI, HU and LT.
static const dsmrFieldDefinition euReactiveAggregateFields[] =
{
  { .obis = "1-0:3.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:4.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
};

//-- Per-tariff reactive energy readings: HU and LT.
static const dsmrFieldDefinition euReactiveTariffFields[] =
{
  { .obis = "1-0:3.8.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:3.8.2", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:4.8.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:4.8.2", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
};

//-- Instantaneous reactive power totals: SE, FI and HU.
static const dsmrFieldDefinition euReactivePowerFields[] =
{
  { .obis = "1-0:3.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:4.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:23.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:43.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:63.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:24.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:44.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:64.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
};

//-- Active power, current and voltage per phase. The active power objects
//-- are published on one- and three-phase meters alike; the L2/L3 objects
//-- appear on three-phase meters only.
static const dsmrFieldDefinition euPhaseFields[] =
{
  { .obis = "1-0:21.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:41.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:61.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:22.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:42.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:62.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:31.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:51.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:71.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:32.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 2, .groupCount = 1 },
  { .obis = "1-0:52.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 2, .groupCount = 1 },
  { .obis = "1-0:72.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "V", .maxScale = 2, .groupCount = 1 },
};

//-- Voltage sag and swell counters per phase: NL, BE, HU, SE and FI.
static const dsmrFieldDefinition euSagSwellFields[] =
{
  { .obis = "1-0:32.32.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "1-0:52.32.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "1-0:72.32.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "1-0:32.36.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "1-0:52.36.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "1-0:72.36.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Structured power failure log, same DSMR record layout: NL, BE, HU, SE
//-- and FI.
static const dsmrFieldDefinition euFailureLogField[] =
{
  { .obis = "1-0:99.97.0", .type = DSMR_FIELD_TYPE_LOG, .unit = NULL, .maxScale = 0, .groupCount = 0, .logPrefixGroups = 1, .logEntryGroups = 3 },
};

//-- Power failure counters used with the log.
static const dsmrFieldDefinition euPowerQualityFields[] =
{
  { .obis = "0-0:96.7.21", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.7.9", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- M-Bus submeter objects used by the 1-3:0.2.8 family countries that
//-- publish submeters (HU, SE, LT). The 24.2.1 reading carries a timestamp
//-- group and a value group with a unit suffix; the optional timestamp
//-- handling of the decoder accepts both layouts.
static const dsmrFieldDefinition euMbusFields[] =
{
  { .obis = "0-?:24.1.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-?:96.1.1", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-?:24.2.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "m3", .maxScale = 3, .groupCount = 1 },
  { .obis = "0-?:24.3.1", .type = DSMR_FIELD_TYPE_TIMESTAMP, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Legacy M-Bus equipment identifier, published alongside 0-n:96.1.1 by
//-- DSMR 4 style meters and by Hungarian meters.
static const dsmrFieldDefinition euMbusLegacyIdField[] =
{
  { .obis = "0-?:96.1.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Valve position of a coupled M-Bus gas or water meter: BE and HU.
static const dsmrFieldDefinition euMbusValveField[] =
{
  { .obis = "0-?:24.4.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Circuit breaker state (0=off, 1=on, 2=ready to switch on): BE, HU and LT.
static const dsmrFieldDefinition euBreakerField[] =
{
  { .obis = "0-0:96.3.10", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Actual power limit / limiter value: BE and HU. The Dutch style publishes
//-- it at 1-0:17.0.0; the European overview prints the same object for BE
//-- and HU at 0-0:17.0.0 in kW. Luxembourg publishes its threshold at
//-- 0-0:17.0.0 in kVA instead (luExtraFields).
static const dsmrFieldDefinition euLimiterFields[] =
{
  { .obis = "1-0:17.0.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "0-0:17.0.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
};

//-- Fuse / current limit value per phase: BE and HU.
static const dsmrFieldDefinition euCurrentLimitFields[] =
{
  { .obis = "1-0:31.4.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
};

//-- Meter internal status / fault flags as a hexadecimal bit mask: HU, LT
//-- and SE.
static const dsmrFieldDefinition euStatusField[] =
{
  { .obis = "0-0:96.5.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Error code register, filled only on internal meter errors: HU, SE and FI.
static const dsmrFieldDefinition euErrorRegisterField[] =
{
  { .obis = "0-0:97.97.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Internal status / factory flags (Sagemcom and Landis+Gyr style meters):
//-- BE, LT and IE.
static const dsmrFieldDefinition euStatusFlagsField[] =
{
  { .obis = "0-0:96.10.1", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Event code / last event in the log book: HU and LT.
static const dsmrFieldDefinition euEventCodeField[] =
{
  { .obis = "0-0:96.11.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Hungarian (MVM/E.ON) tariff 3 and 4 counters, published only on meters
//-- with more than two tariffs.
static const dsmrFieldDefinition huTariff34Fields[] =
{
  { .obis = "1-0:1.8.3", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:1.8.4", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.3", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.4", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
};

//-- Hungarian (ELMŰ/ÉMÁSZ) combined total active energy |(+A)| + |(-A)|.
static const dsmrFieldDefinition huTotalActiveField[] =
{
  { .obis = "1-0:15.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
};

//-- Hungarian reactive energy quadrant measurements on specific meters.
static const dsmrFieldDefinition huQuadrantFields[] =
{
  { .obis = "1-0:5.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:6.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:7.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:8.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
};

//-- Hungarian meter specifics: neutral current, frequency, power factors,
//-- numeric text message and the legacy split local date and time.
static const dsmrFieldDefinition huExtraFields[] =
{
  { .obis = "1-0:91.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:14.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "Hz", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:13.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = NULL, .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:33.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = NULL, .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:53.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = NULL, .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:73.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = NULL, .maxScale = 3, .groupCount = 1 },
  { .obis = "0-0:96.13.1", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  //-- Legacy split local clock of older Hungarian meters: local time hhmmss
  //-- and local date YYMMDD, published apart from 0-0:1.0.0.
  { .obis = "1-0:0.9.1", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "1-0:0.9.2", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "1-0:5.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:6.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:7.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:8.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVAr", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:51.4.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:71.4.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "A", .maxScale = 3, .groupCount = 1 },
  { .obis = "0-0:96.50.68", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:42.0.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  //-- E.ON Hungária reports the meter serial on 0-0:96.1.0. The exact code
  //-- precedes the "0-?:96.1.0" M-Bus wildcard so both resolve correctly.
  { .obis = "0-0:96.1.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Luxembourg (Luxmetering E-Meter P1 Specification 1.1.3): DSMR 5.0.2
//-- base, clear text when the AES-128-GCM transport layer is disabled. The
//-- readout list carries the version, the timestamp, the COSEM logical
//-- device name and the equipment identifier; no tariff indicator.
static const dsmrFieldDefinition luIdentityFields[] =
{
  { .obis = "1-3:0.2.8", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:1.0.0", .type = DSMR_FIELD_TYPE_TIMESTAMP, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:42.0.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "0-0:96.1.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- Luxembourg energy objects: totals only, no per-tariff readings.
static const dsmrFieldDefinition luEnergyFields[] =
{
  { .obis = "1-0:1.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:3.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:4.8.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVArh", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:1.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:2.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kW", .maxScale = 3, .groupCount = 1 },
};

//-- Luxembourg apparent power, the active threshold (SMAX) in kVA at
//-- 0-0:17.0.0 and the power failure counter of the readout list.
static const dsmrFieldDefinition luExtraFields[] =
{
  { .obis = "1-0:9.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVA", .maxScale = 3, .groupCount = 1 },
  { .obis = "1-0:10.7.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVA", .maxScale = 3, .groupCount = 1 },
  { .obis = "0-0:17.0.0", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kVA", .maxScale = 1, .groupCount = 1 },
  { .obis = "0-0:96.7.21", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- M-Bus device type only; Finnish and Irish meters publish the type
//-- object without submeter readings of their own on the P1 port.
static const dsmrFieldDefinition euMbusTypeOnlyFields[] =
{
  { .obis = "0-?:24.1.0", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

//-- COSEM logical device name, used as identity by Swiss style meters.
static const dsmrFieldDefinition euDeviceNameField[] =
{
  { .obis = "0-0:42.0.0", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
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
  { euMbusLegacyIdField, DSMR_ARRAY_COUNT(euMbusLegacyIdField) },
  { euSagSwellFields, DSMR_ARRAY_COUNT(euSagSwellFields) },
};

static const dsmrFieldBlock nl42Blocks[] =
{
  { nlIdentityFields, DSMR_ARRAY_COUNT(nlIdentityFields) },
  { nlEnergyFields, DSMR_ARRAY_COUNT(nlEnergyFields) },
  { nlPhaseFields, DSMR_ARRAY_COUNT(nlPhaseFields) },
  { nlPowerQualityFields, DSMR_ARRAY_COUNT(nlPowerQualityFields) },
  { nlMbusFields, DSMR_ARRAY_COUNT(nlMbusFields) },
  { euMbusLegacyIdField, DSMR_ARRAY_COUNT(euMbusLegacyIdField) },
  { euSagSwellFields, DSMR_ARRAY_COUNT(euSagSwellFields) },
};

static const dsmrFieldBlock nl50Blocks[] =
{
  { nlIdentityFields, DSMR_ARRAY_COUNT(nlIdentityFields) },
  { nlAggregateFields, DSMR_ARRAY_COUNT(nlAggregateFields) },
  { nlEnergyFields, DSMR_ARRAY_COUNT(nlEnergyFields) },
  { nlPhaseFields, DSMR_ARRAY_COUNT(nlPhaseFields) },
  { nlPowerQualityFields, DSMR_ARRAY_COUNT(nlPowerQualityFields) },
  { nlMbusFields, DSMR_ARRAY_COUNT(nlMbusFields) },
  { euSagSwellFields, DSMR_ARRAY_COUNT(euSagSwellFields) },
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
  { euSagSwellFields, DSMR_ARRAY_COUNT(euSagSwellFields) },
  { euBreakerField, DSMR_ARRAY_COUNT(euBreakerField) },
  { euLimiterFields, DSMR_ARRAY_COUNT(euLimiterFields) },
  { euCurrentLimitFields, DSMR_ARRAY_COUNT(euCurrentLimitFields) },
  { euMbusValveField, DSMR_ARRAY_COUNT(euMbusValveField) },
  { euStatusFlagsField, DSMR_ARRAY_COUNT(euStatusFlagsField) },
};

//-- --------------------------------------------------------------------------
//-- Country profile field assemblies
//-- --------------------------------------------------------------------------

//-- Hungary (E.ON Hungária P1 tájékoztató 2024-07-02, DSMR 5.x based): four
//-- active energy tariffs, quadrant reactive registers, frequency, power
//-- factors and the Hungarian extra objects. The exact identity codes in
//-- huExtraFields precede the M-Bus wildcard blocks.
static const dsmrFieldBlock huBlocks[] =
{
  { euIdentityFields, DSMR_ARRAY_COUNT(euIdentityFields) },
  { huExtraFields, DSMR_ARRAY_COUNT(huExtraFields) },
  { euTextMessageFields, DSMR_ARRAY_COUNT(euTextMessageFields) },
  { euEnergyFields, DSMR_ARRAY_COUNT(euEnergyFields) },
  { huTariff34Fields, DSMR_ARRAY_COUNT(huTariff34Fields) },
  { huTotalActiveField, DSMR_ARRAY_COUNT(huTotalActiveField) },
  { euAggregateFields, DSMR_ARRAY_COUNT(euAggregateFields) },
  { euReactiveAggregateFields, DSMR_ARRAY_COUNT(euReactiveAggregateFields) },
  { euReactiveTariffFields, DSMR_ARRAY_COUNT(euReactiveTariffFields) },
  { huQuadrantFields, DSMR_ARRAY_COUNT(huQuadrantFields) },
  { euReactivePowerFields, DSMR_ARRAY_COUNT(euReactivePowerFields) },
  { euPhaseFields, DSMR_ARRAY_COUNT(euPhaseFields) },
  { euBreakerField, DSMR_ARRAY_COUNT(euBreakerField) },
  { euLimiterFields, DSMR_ARRAY_COUNT(euLimiterFields) },
  { euCurrentLimitFields, DSMR_ARRAY_COUNT(euCurrentLimitFields) },
  { euPowerQualityFields, DSMR_ARRAY_COUNT(euPowerQualityFields) },
  { euSagSwellFields, DSMR_ARRAY_COUNT(euSagSwellFields) },
  { euFailureLogField, DSMR_ARRAY_COUNT(euFailureLogField) },
  { euStatusField, DSMR_ARRAY_COUNT(euStatusField) },
  { euErrorRegisterField, DSMR_ARRAY_COUNT(euErrorRegisterField) },
  { euEventCodeField, DSMR_ARRAY_COUNT(euEventCodeField) },
  { euMbusLegacyIdField, DSMR_ARRAY_COUNT(euMbusLegacyIdField) },
  { euMbusFields, DSMR_ARRAY_COUNT(euMbusFields) },
  { euMbusValveField, DSMR_ARRAY_COUNT(euMbusValveField) },
};

//-- Sweden (Energimyndigheten / Energiföretagen style DSMR 5 P1 port per
//-- the European OBIS overview): aggregates, reactive set, submeter
//-- readings, status and error register.
static const dsmrFieldBlock seBlocks[] =
{
  { euIdentityFields, DSMR_ARRAY_COUNT(euIdentityFields) },
  { euTextMessageFields, DSMR_ARRAY_COUNT(euTextMessageFields) },
  { euEnergyFields, DSMR_ARRAY_COUNT(euEnergyFields) },
  { euAggregateFields, DSMR_ARRAY_COUNT(euAggregateFields) },
  { euReactiveAggregateFields, DSMR_ARRAY_COUNT(euReactiveAggregateFields) },
  { euReactivePowerFields, DSMR_ARRAY_COUNT(euReactivePowerFields) },
  { euPhaseFields, DSMR_ARRAY_COUNT(euPhaseFields) },
  { euPowerQualityFields, DSMR_ARRAY_COUNT(euPowerQualityFields) },
  { euSagSwellFields, DSMR_ARRAY_COUNT(euSagSwellFields) },
  { euFailureLogField, DSMR_ARRAY_COUNT(euFailureLogField) },
  { euStatusField, DSMR_ARRAY_COUNT(euStatusField) },
  { euErrorRegisterField, DSMR_ARRAY_COUNT(euErrorRegisterField) },
  { euMbusFields, DSMR_ARRAY_COUNT(euMbusFields) },
};

//-- Finland: like Sweden without the meter status word and without M-Bus
//-- readings on the port.
static const dsmrFieldBlock fiBlocks[] =
{
  { euIdentityFields, DSMR_ARRAY_COUNT(euIdentityFields) },
  { euTextMessageFields, DSMR_ARRAY_COUNT(euTextMessageFields) },
  { euEnergyFields, DSMR_ARRAY_COUNT(euEnergyFields) },
  { euAggregateFields, DSMR_ARRAY_COUNT(euAggregateFields) },
  { euReactiveAggregateFields, DSMR_ARRAY_COUNT(euReactiveAggregateFields) },
  { euReactivePowerFields, DSMR_ARRAY_COUNT(euReactivePowerFields) },
  { euPhaseFields, DSMR_ARRAY_COUNT(euPhaseFields) },
  { euPowerQualityFields, DSMR_ARRAY_COUNT(euPowerQualityFields) },
  { euSagSwellFields, DSMR_ARRAY_COUNT(euSagSwellFields) },
  { euFailureLogField, DSMR_ARRAY_COUNT(euFailureLogField) },
  { euErrorRegisterField, DSMR_ARRAY_COUNT(euErrorRegisterField) },
  { euMbusTypeOnlyFields, DSMR_ARRAY_COUNT(euMbusTypeOnlyFields) },
};

//-- Lithuania: aggregates and per-tariff reactive readings, breaker state,
//-- status words and submeter readings; no text message or failure log.
static const dsmrFieldBlock ltBlocks[] =
{
  { euIdentityFields, DSMR_ARRAY_COUNT(euIdentityFields) },
  { euEnergyFields, DSMR_ARRAY_COUNT(euEnergyFields) },
  { euAggregateFields, DSMR_ARRAY_COUNT(euAggregateFields) },
  { euReactiveAggregateFields, DSMR_ARRAY_COUNT(euReactiveAggregateFields) },
  { euReactiveTariffFields, DSMR_ARRAY_COUNT(euReactiveTariffFields) },
  { euPhaseFields, DSMR_ARRAY_COUNT(euPhaseFields) },
  { euPowerQualityFields, DSMR_ARRAY_COUNT(euPowerQualityFields) },
  { euBreakerField, DSMR_ARRAY_COUNT(euBreakerField) },
  { euStatusField, DSMR_ARRAY_COUNT(euStatusField) },
  { euStatusFlagsField, DSMR_ARRAY_COUNT(euStatusFlagsField) },
  { euEventCodeField, DSMR_ARRAY_COUNT(euEventCodeField) },
  { euMbusFields, DSMR_ARRAY_COUNT(euMbusFields) },
};

//-- Ireland: the DSMR core kernel only, plus the factory status word.
static const dsmrFieldBlock ieBlocks[] =
{
  { euIdentityFields, DSMR_ARRAY_COUNT(euIdentityFields) },
  { euEnergyFields, DSMR_ARRAY_COUNT(euEnergyFields) },
  { euPhaseFields, DSMR_ARRAY_COUNT(euPhaseFields) },
  { euStatusFlagsField, DSMR_ARRAY_COUNT(euStatusFlagsField) },
  { euMbusTypeOnlyFields, DSMR_ARRAY_COUNT(euMbusTypeOnlyFields) },
};

//-- Luxembourg (Luxmetering 1.1.3): clear-text telegrams once the optional
//-- AES-128-GCM transport layer is disabled; totals only, apparent power,
//-- relay/breaker state and M-Bus gas/water/heat with valve position.
static const dsmrFieldBlock luBlocks[] =
{
  { luIdentityFields, DSMR_ARRAY_COUNT(luIdentityFields) },
  { luEnergyFields, DSMR_ARRAY_COUNT(luEnergyFields) },
  { luExtraFields, DSMR_ARRAY_COUNT(luExtraFields) },
  { euReactivePowerFields, DSMR_ARRAY_COUNT(euReactivePowerFields) },
  { euPhaseFields, DSMR_ARRAY_COUNT(euPhaseFields) },
  { euBreakerField, DSMR_ARRAY_COUNT(euBreakerField) },
  { euPowerQualityFields, DSMR_ARRAY_COUNT(euPowerQualityFields) },
  { euSagSwellFields, DSMR_ARRAY_COUNT(euSagSwellFields) },
  { euTextMessageFields, DSMR_ARRAY_COUNT(euTextMessageFields) },
  { euMbusLegacyIdField, DSMR_ARRAY_COUNT(euMbusLegacyIdField) },
  { euMbusFields, DSMR_ARRAY_COUNT(euMbusFields) },
  { euMbusValveField, DSMR_ARRAY_COUNT(euMbusValveField) },
};

//-- Switzerland (VSE Branchenempfehlung RL-DSP CH 2024, Anhang 3): P1 DSMR
//-- 5.0.2 is an allowed wire protocol; the annex lists the minimum data
//-- points. Reduced OBIS in the annex is published as full 1-0: keys.
static const dsmrFieldBlock chBlocks[] =
{
  { euIdentityFields, DSMR_ARRAY_COUNT(euIdentityFields) },
  { euDeviceNameField, DSMR_ARRAY_COUNT(euDeviceNameField) },
  { euEnergyFields, DSMR_ARRAY_COUNT(euEnergyFields) },
  { euAggregateFields, DSMR_ARRAY_COUNT(euAggregateFields) },
  { euReactiveAggregateFields, DSMR_ARRAY_COUNT(euReactiveAggregateFields) },
  { euReactivePowerFields, DSMR_ARRAY_COUNT(euReactivePowerFields) },
  { euPhaseFields, DSMR_ARRAY_COUNT(euPhaseFields) },
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

//-- The remaining countries publish a DSMR-style P1 port with the version
//-- object 1-3:0.2.8 (the European OBIS overview: "1-3:0.2.8 ... Gebruikt
//-- in: NL, HU, SE, FI, LT, IE"; 0-0:96.1.4 replaces it only in Belgium).
//-- The accepted version is the DSMR 5.0 reported value unless the national
//-- document states otherwise. Real-telegram fixtures remain pending;
//-- acceptUnknownMinorVersion covers same-family drift.
static const char *const dsmr5FamilyVersions[] = { "50", NULL };
//-- The Luxmetering E-Meter P1 Specification 1.1.3 publishes the P1 output
//-- version 1-3:0.2.8(42); the document never shows 50.
static const char *const luP1Versions[] = { "42", NULL };

static const dsmrProfile profileHuDsmr50 =
{
  .profileId = "hu.dsmr.5.0",
  .countryCode = "HU",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = dsmr5FamilyVersions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 10000,
  .fieldBlocks = huBlocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(huBlocks),
  .autoProfiles = NULL,
};

static const dsmrProfile profileSeDsmr50 =
{
  .profileId = "se.dsmr.5.0",
  .countryCode = "SE",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = dsmr5FamilyVersions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 1000,
  .fieldBlocks = seBlocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(seBlocks),
  .autoProfiles = NULL,
};

static const dsmrProfile profileFiDsmr50 =
{
  .profileId = "fi.dsmr.5.0",
  .countryCode = "FI",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = dsmr5FamilyVersions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 1000,
  .fieldBlocks = fiBlocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(fiBlocks),
  .autoProfiles = NULL,
};

static const dsmrProfile profileLtDsmr50 =
{
  .profileId = "lt.dsmr.5.0",
  .countryCode = "LT",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = dsmr5FamilyVersions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 1000,
  .fieldBlocks = ltBlocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(ltBlocks),
  .autoProfiles = NULL,
};

static const dsmrProfile profileIeDsmr50 =
{
  .profileId = "ie.dsmr.5.0",
  .countryCode = "IE",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = dsmr5FamilyVersions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 1000,
  .fieldBlocks = ieBlocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(ieBlocks),
  .autoProfiles = NULL,
};

//-- Luxembourg reports "42" on a DSMR 5.0.2 base in the Luxmetering
//-- example telegram; "50" is accepted as well. Clear text only: with the
//-- AES-128-GCM transport layer enabled a separate decoder must deliver
//-- the plaintext before this parser sees it.
static const dsmrProfile profileLuDsmr50 =
{
  .profileId = "lu.dsmr.5.0",
  .countryCode = "LU",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = luP1Versions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 10000,
  .fieldBlocks = luBlocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(luBlocks),
  .autoProfiles = NULL,
};

//-- Switzerland: the VSE Branchenempfehlung allows P1 DSMR 5.0.2 as a wire
//-- protocol at the Kundenschnittstelle; Anhang 3 defines the minimum data
//-- set. The customer interface ships encrypted and is activated on
//-- request; a plaintext capture uses the DSMR 5.0.2 framing.
static const dsmrProfile profileChDsmr50 =
{
  .profileId = "ch.dsmr.5.0",
  .countryCode = "CH",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = dsmr5FamilyVersions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 10000,
  .fieldBlocks = chBlocks,
  .fieldBlockCount = DSMR_ARRAY_COUNT(chBlocks),
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
  &profileHuDsmr50,
  &profileSeDsmr50,
  &profileFiDsmr50,
  &profileLtDsmr50,
  &profileIeDsmr50,
  &profileLuDsmr50,
  &profileChDsmr50,
};

static const char *const builtinProfileIds[] =
{
  "nl.dsmr.4.0",
  "nl.dsmr.4.2",
  "nl.dsmr.5.0",
  "nl.dsmr.4",
  "be.emucs.1.x",
  "be.emucs.2.1",
  "hu.dsmr.5.0",
  "se.dsmr.5.0",
  "fi.dsmr.5.0",
  "lt.dsmr.5.0",
  "ie.dsmr.5.0",
  "lu.dsmr.5.0",
  "ch.dsmr.5.0",
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
