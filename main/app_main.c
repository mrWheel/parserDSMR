//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: main/app_main.c - built-in test telegram replay demo
//-- Date: 2026-11-16 · Build: 1
//--
//-- Default application of the repository. It replays the built-in sample
//-- telegram through the parser from the USB console, without any P1 wiring,
//-- and logs the component version, one line per summary item and one line
//-- per decoded field.
//-- ==========================================================================

#include <string.h>

#include "esp_log.h"

#include "dsmr_parser.h"
#include "dsmr_sample_labels.h"
#include "dsmr_sample_telegram.h"

static const char *TAG = "parserDSMR";

static dsmrParser parser;
static uint8_t workspace[DSMR_DEFAULT_WORKSPACE_BYTES];

static const dsmrMbusDevice mbusDevices[] =
{
  { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_HEAT },
};

static void onEvent(const dsmrParser *parserInstance, const dsmrParserEvent *event, void *userData)
{
  (void)parserInstance;
  (void)userData;

  if (event->type == DSMR_EVENT_TELEGRAM)
  {
    const dsmrTelegramResult *result = event->result;

    ESP_LOGI(TAG, "Accepted telegram:");
    ESP_LOGI(TAG, "  profile = %s", result->profileId);
    ESP_LOGI(TAG, "  version = %s", (result->reportedVersion != NULL) ? result->reportedVersion : "-");
    ESP_LOGI(TAG, "  meter   = %s", (result->meterIdentification != NULL) ? result->meterIdentification : "-");
    ESP_LOGI(TAG, "  crc     = %s", (result->crcStatus == DSMR_CRC_STATUS_VERIFIED) ? "verified" : "absent");
    ESP_LOGI(TAG, "  fields  = %u", (unsigned)result->fieldCount);

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
        ESP_LOGE(TAG, "Could not format field %s", field->obis);
        continue;
      }

      if (field->mbusChannel != 0u)
      {
        ESP_LOGI(TAG, "  ch%u %-12s %-24s = %s %s",
                 (unsigned)field->mbusChannel,
                 field->obis,
                 (field->name != NULL) ? field->name : "-",
                 value,
                 (field->unit != NULL) ? field->unit : "");
      }
      else
      {
        ESP_LOGI(TAG, "  %-12s %-24s = %s %s",
                 field->obis,
                 (field->name != NULL) ? field->name : "-",
                 value,
                 (field->unit != NULL) ? field->unit : "");
      }
    }
  }
  else
  {
    ESP_LOGE(TAG, "Rejected telegram: %s (position %lu, obis %s)",
             dsmrDiagReasonToString(event->diagnostic->reason),
             (unsigned long)event->diagnostic->bytePosition,
             (event->diagnostic->obis != NULL) ? event->diagnostic->obis : "-");
  }
}

void app_main(void)
{
  //-- Version of the parser component as reported by the component itself.
  ESP_LOGI(TAG, "Component parserDSMR version [%s]", dsmrParserVersion());

  const dsmrParserConfig config =
  {
    .countryCode = "NL",
    .profileId = "nl.dsmr.5.0",
    .mbusDevices = mbusDevices,
    .mbusDeviceCount = (uint8_t)(sizeof mbusDevices / sizeof mbusDevices[0]),
    .fieldLabels = dsmrSampleLabels,
    .fieldLabelCount = (uint16_t)(sizeof dsmrSampleLabels / sizeof dsmrSampleLabels[0]),
    .unknownFieldPolicy = DSMR_UNKNOWN_FIELD_RAW,
  };

  ESP_LOGI(TAG, "Replaying built-in test telegram; no P1 meter is needed");

  dsmrParserStatus status = dsmrParserInit(&parser, &config, workspace, sizeof workspace);

  if (status != DSMR_OK)
  {
    ESP_LOGE(TAG, "Parser initialization failed: %s", dsmrStatusToString(status));
    return;
  }

  status = dsmrParserFeed(&parser,
                          (const uint8_t *)dsmrSampleTelegram,
                          strlen(dsmrSampleTelegram),
                          1000u,
                          onEvent,
                          NULL);

  if (status != DSMR_OK)
  {
    ESP_LOGE(TAG, "Feeding test telegram failed: %s", dsmrStatusToString(status));
  }
}
