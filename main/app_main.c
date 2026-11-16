//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: main/app_main.c - built-in test telegram replay demo
//-- Date: 2026-11-16 · Build: 3
//--
//-- Default application of the repository. It replays the built-in sample
//-- telegram through the parser from the USB console, without any P1 wiring,
//-- and logs the component version, one line per summary item and one line
//-- per decoded field.
//--
//-- Fields are named from the built-in default display names of the parser.
//-- Exactly two fields are renamed through dsmrParserSetFieldLabel so the
//-- monitor shows the defaults in use and a single-field override next to
//-- them. The application label table of the fixtures is deliberately not
//-- used here; examples/stored_telegram demonstrates the full-table swap
//-- through dsmrParserApplyConfig.
//-- ==========================================================================

#include <string.h>

#include "esp_log.h"

#include "dsmr_parser.h"
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
        ESP_LOGI(TAG, "  ch%u %-12s %-28s = %s %s",
                 (unsigned)field->mbusChannel,
                 field->obis,
                 (field->name != NULL) ? field->name : "-",
                 value,
                 (field->unit != NULL) ? field->unit : "");
      }
      else
      {
        ESP_LOGI(TAG, "  %-12s %-28s = %s %s",
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

//-- Single-field overrides on top of the built-in default names. Every
//-- other field keeps its library default, so the monitor shows both.
static void applyLabelOverrides(void)
{
  static const dsmrFieldLabel overrides[] =
  {
    { .obis = "1-0:1.8.2", .name = "energyDeliveredTariff2-changed" },
    { .obis = "1-0:2.8.2", .name = "energyReturnedTariff2-changed" },
  };

  for (size_t index = 0; index < sizeof overrides / sizeof overrides[0]; index++)
  {
    dsmrParserStatus status = dsmrParserSetFieldLabel(&parser, overrides[index].obis, overrides[index].name);

    if (status != DSMR_OK)
    {
      ESP_LOGE(TAG, "Label override for %s failed: %s", overrides[index].obis, dsmrStatusToString(status));
    }
  }
}

void app_main(void)
{
  //-- Version of the parser component as reported by the component itself.
  ESP_LOGI(TAG, "Component parserDSMR version [%s]", dsmrParserVersion());

  //-- No fieldLabels table: the built-in default display names are used.
  const dsmrParserConfig config =
  {
    .countryCode = "NL",
    .profileId = "nl.dsmr.5.0",
    .mbusDevices = mbusDevices,
    .mbusDeviceCount = (uint8_t)(sizeof mbusDevices / sizeof mbusDevices[0]),
    .unknownFieldPolicy = DSMR_UNKNOWN_FIELD_RAW,
  };

  ESP_LOGI(TAG, "Replaying built-in test telegram; no P1 meter is needed");

  dsmrParserStatus status = dsmrParserInit(&parser, &config, workspace, sizeof workspace);

  if (status != DSMR_OK)
  {
    ESP_LOGE(TAG, "Parser initialization failed: %s", dsmrStatusToString(status));
    return;
  }

  ESP_LOGI(TAG, "Using built-in default field names; two fields are overridden through dsmrParserSetFieldLabel");
  applyLabelOverrides();

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
