//--    parserDSMR - stored telegram test program that replays the embedded fixture set
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
//-- File: components/parserDSMR/examples/stored_telegram/main/app_main.c
//-- Date: 2026-11-21 · Build: 4
//--
//-- Stored telegram test program. It runs from the USB console with no P1
//-- wiring: every fixture of the host test set is embedded and fed to the
//-- parser through dsmrParserFeed, so the full decode path is exercised
//-- without any UART. Accepted telegrams print every field on its own OBIS
//-- line; rejected candidates report their reason. The program then
//-- demonstrates dynamic configuration changes through dsmrParserApplyConfig
//-- between telegrams.
//-- ==========================================================================

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "dsmr_parser.h"
#include "dsmr_fixtures.h"

static dsmrParser parser;
static uint8_t workspace[DSMR_DEFAULT_WORKSPACE_BYTES];

//-- Application-determined names. Another program may label the same OBIS
//-- codes differently.
static const dsmrFieldLabel fieldLabels[] =
{
  { .obis = "1-0:1.8.1", .name = "energyDeliveredTariff1" },
  { .obis = "1-0:1.8.2", .name = "energyDeliveredTariff2" },
  { .obis = "1-0:2.8.1", .name = "energyReturnedTariff1" },
  { .obis = "1-0:2.8.2", .name = "energyReturnedTariff2" },
  { .obis = "0-0:96.14.0", .name = "tariffIndicator" },
};

static const dsmrFieldLabel editedFieldLabels[] =
{
  { .obis = "1-0:1.8.1", .name = "EnDelTar1" },
  { .obis = "1-0:1.8.2", .name = "EnDelTar2" },
};

//-- One line per received field with OBIS, application name and value.
//-- The M-Bus channel is shown where present.
static void printFields(const dsmrTelegramResult *result)
{
  for (uint16_t index = 0; index < result->fieldCount; index++)
  {
    const dsmrTelegramField *field = &result->fields[index];
    char valueText[32];
    const char *value = valueText;

    if (field->value.kind == DSMR_VALUE_TEXT)
    {
      value = (field->value.text != NULL) ? field->value.text : "";
    }
    else if (dsmrFormatValue(&field->value, valueText, sizeof valueText) < 0)
    {
      printf("  could not format field %s\r\n", field->obis);
      continue;
    }

    //-- Fields the profile does not define show a question mark instead of
    //-- a label. Unknown codes are data, never a fault.
    const char *name = (field->name != NULL) ? field->name : ((field->classification == DSMR_FIELD_CLASS_UNKNOWN) ? "?" : "-");

    if (field->mbusChannel != 0u)
    {
      printf("  ch%u %-12s %-24s = %s %s\r\n", (unsigned)field->mbusChannel, field->obis, name, value, (field->unit != NULL) ? field->unit : "");
    }
    else
    {
      printf("  %-12s %-24s = %s %s\r\n", field->obis, name, value, (field->unit != NULL) ? field->unit : "");
    }
  }
}

static void printSummary(const dsmrTelegramResult *result)
{
  printf("  summary: profile=%s version=%s meter=%s crc=%s fields=%u\r\n",
         result->profileId,
         (result->reportedVersion != NULL) ? result->reportedVersion : "-",
         (result->meterIdentification != NULL) ? result->meterIdentification : "-",
         (result->crcStatus == DSMR_CRC_STATUS_VERIFIED) ? "verified" : "absent",
         (unsigned)result->fieldCount);
}

static void onEvent(const dsmrParser *parserInstance, const dsmrParserEvent *event, void *userData)
{
  (void)parserInstance;
  (void)userData;

  if (event->type == DSMR_EVENT_TELEGRAM)
  {
    printFields(event->result);
    printSummary(event->result);
  }
  else
  {
    printf("  rejected: %s (position %lu, obis %s)\r\n",
           dsmrDiagReasonToString(event->diagnostic->reason),
           (unsigned long)event->diagnostic->bytePosition,
           (event->diagnostic->obis != NULL) ? event->diagnostic->obis : "-");
  }
}

static dsmrParserConfig makeConfig(const dsmrFixture *fixture, const dsmrFieldLabel *labels, uint16_t labelCount)
{
  dsmrParserConfig config;

  memset(&config, 0, sizeof config);
  config.countryCode = fixture->countryCode;
  config.profileId = fixture->profileId;
  config.mbusDevices = fixture->mbusDevices;
  config.mbusDeviceCount = fixture->mbusDeviceCount;
  config.fieldLabels = labels;
  config.fieldLabelCount = labelCount;

  return config;
}

static void runFixture(const dsmrFixture *fixture, const dsmrFieldLabel *labels, uint16_t labelCount)
{
  dsmrParserConfig config = makeConfig(fixture, labels, labelCount);
  dsmrParserStatus status = dsmrParserInit(&parser, &config, workspace, sizeof workspace);

  printf("\r\n[%s] source: %s\r\n", fixture->id, fixture->source);

  if (status != DSMR_OK)
  {
    printf("  init failed: %s\r\n", dsmrStatusToString(status));
    return;
  }

  dsmrParserFeed(&parser, (const uint8_t *)fixture->telegram, strlen(fixture->telegram), 1000u, onEvent, NULL);

  if (fixture->needsTick)
  {
    dsmrParserTick(&parser, 1000u + DSMR_DEFAULT_INTER_BYTE_TIMEOUT_MS + 1000u, onEvent, NULL);
  }
}

//-- Demonstrates dynamic configuration: the water meter on channel 2 of the
//-- three-phase fixture is replaced by a heat meter, which must produce an
//-- M-Bus device type mismatch diagnostic on the next telegram.
static void demonstrateDynamicReconfiguration(void)
{
  static const dsmrMbusDevice heatDevices[] =
  {
    { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_GAS },
    { .channel = 2, .deviceType = DSMR_MBUS_DEVICE_HEAT },
  };

  const dsmrFixture *fixture = &dsmrFixtures[1];
  dsmrParserConfig config = makeConfig(fixture, fieldLabels, (uint16_t)(sizeof fieldLabels / sizeof fieldLabels[0]));

  printf("\r\n[dynamic] replacing the channel 2 water meter with a heat meter\r\n");

  if (dsmrParserInit(&parser, &config, workspace, sizeof workspace) != DSMR_OK)
  {
    printf("  init failed\r\n");
    return;
  }

  config.mbusDevices = heatDevices;
  config.mbusDeviceCount = 2;

  if (dsmrParserApplyConfig(&parser, &config) != DSMR_OK)
  {
    printf("  applyConfig failed\r\n");
    return;
  }

  dsmrParserFeed(&parser, (const uint8_t *)fixture->telegram, strlen(fixture->telegram), 2000u, onEvent, NULL);

  //-- Switch profileId and feed a telegram matching the new profile. The
  //-- declared channel 2 stays absent because this meter does not publish
  //-- it, which is a valid outcome.
  const dsmrFixture *singlePhase = &dsmrFixtures[0];

  printf("\r\n[dynamic] switching profileId to nl.dsmr.4.2 for the single-phase fixture\r\n");

  config.profileId = "nl.dsmr.4.2";

  if (dsmrParserApplyConfig(&parser, &config) != DSMR_OK)
  {
    printf("  applyConfig failed\r\n");
    return;
  }

  dsmrParserFeed(&parser, (const uint8_t *)singlePhase->telegram, strlen(singlePhase->telegram), 3000u, onEvent, NULL);

  printf("\r\n[dynamic] editing the field label table (short names)\r\n");

  config.fieldLabels = editedFieldLabels;
  config.fieldLabelCount = (uint16_t)(sizeof editedFieldLabels / sizeof editedFieldLabels[0]);

  if (dsmrParserApplyConfig(&parser, &config) != DSMR_OK)
  {
    printf("  applyConfig failed\r\n");
    return;
  }

  dsmrParserFeed(&parser, (const uint8_t *)singlePhase->telegram, strlen(singlePhase->telegram), 4000u, onEvent, NULL);
}

void app_main(void)
{
  printf("\r\nparserDSMR stored telegram test program\r\n");
  printf("workspace: %u bytes, fixtures: %u\r\n", (unsigned)sizeof workspace, (unsigned)DSMR_FIXTURE_COUNT);

  for (uint16_t index = 0; index < DSMR_FIXTURE_COUNT; index++)
  {
    runFixture(&dsmrFixtures[index], fieldLabels, (uint16_t)(sizeof fieldLabels / sizeof fieldLabels[0]));
  }

  demonstrateDynamicReconfiguration();

  printf("\r\nstored telegram run complete; no P1 connection is used.\r\n");

  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
