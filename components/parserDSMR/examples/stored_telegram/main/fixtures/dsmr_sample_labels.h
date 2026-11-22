//--    parserDSMR - application field label sample table based on the DSMR 5.0.2 object table
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
//-- File: components/parserDSMR/examples/stored_telegram/main/fixtures/dsmr_sample_labels.h - application field label sample table based on the DSMR 5.0.2 object table
//-- Date: 2026-11-21 · Build: 1
//-- ==========================================================================

//-- Application labels based on the DSMR 5.0.2 P1 object table, pages 20-23.

#ifndef DSMR_SAMPLE_LABELS_H
#define DSMR_SAMPLE_LABELS_H

#include "dsmr_parser.h"

static const dsmrFieldLabel dsmrSampleLabels[] =
{
  { .obis = "1-3:0.2.8", .name = "p1Version" },
  { .obis = "0-0:1.0.0", .name = "telegramTimestamp" },
  { .obis = "0-0:96.1.1", .name = "equipmentIdentifier" },
  { .obis = "1-0:1.8.1", .name = "energyDeliveredTariff1" },
  { .obis = "1-0:1.8.2", .name = "energyDeliveredTariff2" },
  { .obis = "1-0:2.8.1", .name = "energyReturnedTariff1" },
  { .obis = "1-0:2.8.2", .name = "energyReturnedTariff2" },
  { .obis = "0-0:96.14.0", .name = "electricityTariff" },
  { .obis = "1-0:1.7.0", .name = "powerDelivered" },
  { .obis = "1-0:2.7.0", .name = "powerReceived" },
  { .obis = "0-0:96.7.21", .name = "powerFailures" },
  { .obis = "0-0:96.7.9", .name = "longPowerFailures" },
  { .obis = "1-0:99.97.0", .name = "powerFailureEventLog" },
  { .obis = "1-0:32.32.0", .name = "voltageSagsL1" },
  { .obis = "1-0:52.32.0", .name = "voltageSagsL2" },
  { .obis = "1-0:72.32.0", .name = "voltageSagsL3" },
  { .obis = "1-0:32.36.0", .name = "voltageSwellsL1" },
  { .obis = "1-0:52.36.0", .name = "voltageSwellsL2" },
  { .obis = "1-0:72.36.0", .name = "voltageSwellsL3" },
  { .obis = "0-0:96.13.0", .name = "textMessage" },
  { .obis = "1-0:32.7.0", .name = "voltageL1" },
  { .obis = "1-0:52.7.0", .name = "voltageL2" },
  { .obis = "1-0:72.7.0", .name = "voltageL3" },
  { .obis = "1-0:31.7.0", .name = "currentL1" },
  { .obis = "1-0:51.7.0", .name = "currentL2" },
  { .obis = "1-0:71.7.0", .name = "currentL3" },
  { .obis = "1-0:21.7.0", .name = "powerDeliveredL1" },
  { .obis = "1-0:41.7.0", .name = "powerDeliveredL2" },
  { .obis = "1-0:61.7.0", .name = "powerDeliveredL3" },
  { .obis = "1-0:22.7.0", .name = "powerReceivedL1" },
  { .obis = "1-0:42.7.0", .name = "powerReceivedL2" },
  { .obis = "1-0:62.7.0", .name = "powerReceivedL3" },
  { .obis = "0-1:24.1.0", .name = "mbusDeviceType" },
  { .obis = "0-1:96.1.0", .name = "mbusEquipmentIdentifier" },
  { .obis = "0-1:24.2.1", .name = "mbusLast5MinuteReading" },
};

#endif
