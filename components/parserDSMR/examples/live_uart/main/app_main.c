//--    parserDSMR - live UART test program that displays one line per received OBIS field
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
//-- File: components/parserDSMR/examples/live_uart/main/app_main.c
//-- Date: 2026-11-21 · Build: 4
//--
//-- Live UART test program. It does nothing other than wait for telegrams
//-- on the P1 port UART and display the parsed data. For each accepted
//-- telegram it prints one line per OBIS field with the OBIS code, the
//-- M-Bus channel where present, the value and the unit, followed by a
//-- short per-telegram summary. It contains no other application logic.
//--
//-- Adjust the defines below to your board and meter. The P1 interface
//-- board must provide signal conditioning and level shifting; enable
//-- invertRx when the board presents the signal inverted.
//-- ==========================================================================

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "dsmr_parser.h"
#include "dsmr_uart_esp32.h"

//-- Meter selection: switch these two lines for another profile, for
//-- example "BE" and "be.emucs.2.1".
#define DSMR_EXAMPLE_COUNTRY "NL"
#define DSMR_EXAMPLE_PROFILE "nl.dsmr.5.0"

#define DSMR_EXAMPLE_UART UART_NUM_1
#define DSMR_EXAMPLE_RX_PIN 20
#define DSMR_EXAMPLE_TX_PIN 21
#define DSMR_EXAMPLE_REQUEST_GPIO (-1)
#define DSMR_EXAMPLE_INVERT_RX true

static dsmrParser parser;
static dsmrUart uart;
static uint8_t workspace[DSMR_DEFAULT_WORKSPACE_BYTES];

//-- Static description of the devices attached to the meter's M-Bus
//-- interface. Adjust to the meters actually connected.
static const dsmrMbusDevice mbusDevices[] =
{
  { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_GAS },
  { .channel = 2, .deviceType = DSMR_MBUS_DEVICE_WATER },
};

static void onEvent(const dsmrParser *parserInstance, const dsmrParserEvent *event, void *userData)
{
  (void)parserInstance;
  (void)userData;

  if (event->type == DSMR_EVENT_TELEGRAM)
  {
    const dsmrTelegramResult *result = event->result;

    for (uint16_t index = 0; index < result->fieldCount; index++)
    {
      const dsmrTelegramField *field = &result->fields[index];
      char valueText[32];

      dsmrFormatValue(&field->value, valueText, sizeof valueText);

      //-- Unknown OBIS codes are kept as raw data and flagged as unknown;
      //-- they are never reported as a fault.
      const char *classMark = (field->classification == DSMR_FIELD_CLASS_UNKNOWN) ? " (unknown)" : "";

      if (field->mbusChannel != 0u)
      {
        printf("ch%u %-12s %s %s%s\r\n", (unsigned)field->mbusChannel, field->obis, valueText, (field->unit != NULL) ? field->unit : "", classMark);
      }
      else
      {
        printf("    %-12s %s %s%s\r\n", field->obis, valueText, (field->unit != NULL) ? field->unit : "", classMark);
      }
    }

    printf("telegram: profile=%s version=%s meter=%s crc=%s\r\n",
           result->profileId,
           (result->reportedVersion != NULL) ? result->reportedVersion : "-",
           (result->meterIdentification != NULL) ? result->meterIdentification : "-",
           (result->crcStatus == DSMR_CRC_STATUS_VERIFIED) ? "verified" : "absent");
  }
  else
  {
    printf("rejected: %s (position %lu, obis %s)\r\n",
           dsmrDiagReasonToString(event->diagnostic->reason),
           (unsigned long)event->diagnostic->bytePosition,
           (event->diagnostic->obis != NULL) ? event->diagnostic->obis : "-");
  }
}

void app_main(void)
{
  const dsmrParserConfig config =
  {
    .countryCode = DSMR_EXAMPLE_COUNTRY,
    .profileId = DSMR_EXAMPLE_PROFILE,
    .mbusDevices = mbusDevices,
    .mbusDeviceCount = sizeof mbusDevices / sizeof mbusDevices[0],
  };

  dsmrParserStatus status = dsmrParserInit(&parser, &config, workspace, sizeof workspace);

  if (status != DSMR_OK)
  {
    printf("parser init failed: %s\r\n", dsmrStatusToString(status));
    return;
  }

  const dsmrUartConfig uartConfig =
  {
    .uartPort = DSMR_EXAMPLE_UART,
    .rxPin = DSMR_EXAMPLE_RX_PIN,
    .txPin = DSMR_EXAMPLE_TX_PIN,
    .requestGpio = DSMR_EXAMPLE_REQUEST_GPIO,
    .requestActiveHigh = true,
    .invertRx = DSMR_EXAMPLE_INVERT_RX,
    .requestLeadMs = 0,
  };

  esp_err_t result = dsmrUartStart(&uart, &parser, &uartConfig, onEvent, NULL);

  if (result != ESP_OK)
  {
    printf("uart start failed: %s\r\n", esp_err_to_name(result));
    return;
  }

  printf("waiting for telegrams on %s using profile %s\r\n", DSMR_EXAMPLE_PROFILE, DSMR_EXAMPLE_COUNTRY);

  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
