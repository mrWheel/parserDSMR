//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: test/host/test_dsmr.c - host tests for the portable C11 core
//-- Date: 2026-11-14 · Build: 1
//--
//-- Compiles the portable core without ESP-IDF. Run with: make run
//-- Coverage follows the acceptance list of projectPrompt.md section 7.
//-- ==========================================================================

#include "dsmr_fixtures.h"
#include "dsmr_sample_labels.h"
#include "dsmr_sample_telegram.h"
#include "dsmr_parser.h"

#include "dsmr_crc.h"
#include "dsmr_value.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(condition, label)                                                    \
  do                                                                               \
  {                                                                                \
    checks++;                                                                      \
    if (!(condition))                                                              \
    {                                                                              \
      failures++;                                                                  \
      printf("FAIL %s:%d %s\n", __FILE__, __LINE__, (label));                      \
    }                                                                              \
  } while (0)

//-- --------------------------------------------------------------------------
//-- Event collector
//-- --------------------------------------------------------------------------

typedef struct
{
  dsmrTelegramResult result;
  dsmrTelegramField fields[DSMR_MAX_FIELDS];
  uint16_t telegramCount;
  uint16_t diagnosticCount;
  dsmrDiagReason lastDiag;
  const char *lastDiagObis;
  uint16_t typeMismatchCount;
  bool feedFromCallback;
  dsmrParserStatus reentrantStatus;
} Collector;

static void onEvent(const dsmrParser *parser, const dsmrParserEvent *event, void *userData)
{
  Collector *collector = (Collector *)userData;

  if (event->type == DSMR_EVENT_TELEGRAM)
  {
    collector->result = *event->result;
    memcpy(collector->fields, event->result->fields, (size_t)event->result->fieldCount * sizeof(dsmrTelegramField));
    collector->result.fields = collector->fields;
    collector->telegramCount++;

    //-- Re-entry from the callback must be refused.
    if (collector->feedFromCallback)
    {
      static const uint8_t noise[] = "x";
      collector->reentrantStatus = dsmrParserFeed((dsmrParser *)parser, noise, sizeof noise, 1000, onEvent, collector);
    }
  }
  else
  {
    collector->diagnosticCount++;
    collector->lastDiag = event->diagnostic->reason;
    collector->lastDiagObis = event->diagnostic->obis;

    if (event->diagnostic->reason == DSMR_DIAG_MBUS_TYPE_MISMATCH)
    {
      collector->typeMismatchCount++;
    }
  }
}

static void collectorReset(Collector *collector)
{
  memset(collector, 0, sizeof *collector);
}

//-- --------------------------------------------------------------------------
//-- Helpers
//-- --------------------------------------------------------------------------

static dsmrParserConfig makeConfig(const dsmrFixture *fixture)
{
  dsmrParserConfig config;
  memset(&config, 0, sizeof config);
  config.countryCode = fixture->countryCode;
  config.profileId = fixture->profileId;
  config.mbusDevices = fixture->mbusDevices;
  config.mbusDeviceCount = fixture->mbusDeviceCount;
  return config;
}

static uint8_t workspace[DSMR_DEFAULT_WORKSPACE_BYTES];

static bool feedAll(dsmrParser *parser, Collector *collector, const char *data, size_t length, uint32_t timeMs, size_t chunkSize)
{
  size_t offset = 0;
  uint32_t clock = timeMs;

  while (offset < length)
  {
    size_t size = length - offset;

    if ((chunkSize > 0u) && (size > chunkSize))
    {
      size = chunkSize;
    }

    if (dsmrParserFeed(parser, (const uint8_t *)(data + offset), size, clock, onEvent, collector) != DSMR_OK)
    {
      return false;
    }

    offset += size;
    clock += 1u;
  }

  return true;
}

static bool initForFixture(dsmrParser *parser, const dsmrFixture *fixture, dsmrParserConfig *override, bool overrideActive)
{
  dsmrParserConfig config = overrideActive ? *override : makeConfig(fixture);
  return dsmrParserInit(parser, &config, workspace, sizeof workspace) == DSMR_OK;
}

//-- Builds a complete frame around a body. Existing CR characters are
//-- dropped; LF endings are expanded to CR LF for the CRLF variant and kept
//-- as bare LF for the LF variant. The CRC is computed over the received
//-- byte sequence exactly as emitted.
static size_t buildFrameEx(char *out, size_t outSize, const char *body, bool useCrlf)
{
  size_t outLength = 0;
  const char *source = body;

  while ((*source != '\0') && ((outLength + 4u) < outSize))
  {
    if (*source == '\r')
    {
      source++;
      continue;
    }

    if (*source == '\n')
    {
      if (useCrlf)
      {
        out[outLength++] = '\r';
      }

      out[outLength++] = '\n';
      source++;
    }
    else
    {
      out[outLength++] = *source;
      source++;
    }
  }

  if ((outLength == 0u) || ((outLength + 8u) >= outSize))
  {
    return 0;
  }

  out[outLength++] = '!';

  uint16_t crc = dsmrCrc16Arc((const uint8_t *)out, outLength);

  if (useCrlf)
  {
    snprintf(out + outLength, outSize - outLength, "%04X\r\n", (unsigned int)crc);
    outLength += 7u;
  }
  else
  {
    snprintf(out + outLength, outSize - outLength, "%04X\n", (unsigned int)crc);
    outLength += 6u;
  }

  return outLength;
}

static size_t buildFrame(char *out, size_t outSize, const char *body)
{
  return buildFrameEx(out, outSize, body, true);
}

static const dsmrTelegramField *findLocalField(const Collector *collector, const char *obis)
{
  for (uint16_t index = 0; index < collector->result.fieldCount; index++)
  {
    if (strcmp(collector->result.fields[index].obis, obis) == 0)
    {
      return &collector->result.fields[index];
    }
  }

  return NULL;
}

//-- --------------------------------------------------------------------------
//-- Tests
//-- --------------------------------------------------------------------------

static void testCrcCheckValue(void)
{
  uint16_t crc = dsmrCrc16Arc((const uint8_t *)"123456789", 9);
  CHECK(crc == 0xBB3D, "CRC-16/ARC check value for 123456789 is 0xBB3D");
}

static void testSampleTelegram(void)
{
  dsmrParser parser;
  Collector collector;
  static const dsmrMbusDevice devices[] =
  {
    { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_HEAT },
  };
  const dsmrParserConfig config =
  {
    .countryCode = "NL",
    .profileId = "nl.dsmr.5.0",
    .mbusDevices = devices,
    .mbusDeviceCount = (uint8_t)(sizeof devices / sizeof devices[0]),
    .fieldLabels = dsmrSampleLabels,
    .fieldLabelCount = (uint16_t)(sizeof dsmrSampleLabels / sizeof dsmrSampleLabels[0]),
    .unknownFieldPolicy = DSMR_UNKNOWN_FIELD_RAW,
  };

  collectorReset(&collector);
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for sample DSMR 5.0 telegram");
  CHECK(feedAll(&parser, &collector, dsmrSampleTelegram, strlen(dsmrSampleTelegram), 1000u, 0u), "feed sample DSMR 5.0 telegram");

  if (collector.diagnosticCount > 0u)
  {
    printf("sample diagnostic: %s obis=%s\n",
           dsmrDiagReasonToString(collector.lastDiag),
           (collector.lastDiagObis != NULL) ? collector.lastDiagObis : "-");
  }

  CHECK(collector.telegramCount == 1u && collector.diagnosticCount == 0u, "accept sample DSMR 5.0 telegram");
  CHECK(collector.result.crcStatus == DSMR_CRC_STATUS_VERIFIED, "sample telegram CRC verified");
  CHECK(collector.result.fieldCount == sizeof dsmrSampleLabels / sizeof dsmrSampleLabels[0], "sample telegram exposes all expected fields");
  const dsmrTelegramField *mbusReading = findLocalField(&collector, "0-1:24.2.1");
  CHECK(mbusReading != NULL, "sample telegram decodes M-Bus reading");
  CHECK((mbusReading != NULL) && mbusReading->hasTimestamp, "M-Bus timestamp is parsed before the reading");
  CHECK(findLocalField(&collector, "1-0:32.7.0") != NULL, "sample telegram retains an unconfigured phase voltage field");

  for (uint16_t index = 0; index < collector.result.fieldCount; index++)
  {
    CHECK(collector.result.fields[index].name != NULL, "each sample field has an application label");
  }

  const dsmrTelegramField *voltage = findLocalField(&collector, "1-0:32.7.0");
  CHECK((voltage != NULL) && (voltage->name != NULL) && (strcmp(voltage->name, "voltageL1") == 0), "raw phase voltage has the DSMR sample label");
}

static void testInitErrors(void)
{
  dsmrParser parser;
  dsmrParserConfig config;

  memset(&config, 0, sizeof config);
  config.countryCode = "NL";
  config.profileId = "nl.dsmr.4.2";

  CHECK(dsmrParserInit(&parser, NULL, workspace, sizeof workspace) == DSMR_ERR_NULL_ARGUMENT, "null configuration rejected");
  CHECK(dsmrParserInit(NULL, &config, workspace, sizeof workspace) == DSMR_ERR_NULL_ARGUMENT, "null parser rejected");
  CHECK(dsmrParserInit(&parser, &config, NULL, sizeof workspace) == DSMR_ERR_NULL_ARGUMENT, "null workspace rejected");

  config.profileId = "xx.nope.1";
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_ERR_UNKNOWN_PROFILE, "unknown profile yields distinct error");

  config.profileId = "nl.dsmr.4.2";
  config.countryCode = "BE";
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_ERR_COUNTRY_MISMATCH, "country mismatch yields distinct error");

  config.countryCode = "NL";
  config.maxTelegramBytes = DSMR_MAX_TELEGRAM_BYTES;
  CHECK(dsmrParserInit(&parser, &config, workspace, 1024) == DSMR_ERR_INSUFFICIENT_WORKSPACE, "insufficient workspace yields distinct error");

  size_t needed = dsmrParserWorkspaceSizeNeeded(&config);
  CHECK(dsmrParserInit(&parser, &config, workspace, needed) == DSMR_OK, "reported workspace size is sufficient");

  dsmrParserConfig badLimits;
  memset(&badLimits, 0, sizeof badLimits);
  badLimits.countryCode = "NL";
  badLimits.profileId = "nl.dsmr.4.2";
  badLimits.maxTelegramBytes = DSMR_MAX_TELEGRAM_BYTES + 1u;
  CHECK(dsmrParserInit(&parser, &badLimits, workspace, sizeof workspace) == DSMR_ERR_INVALID_CONFIG, "limit above documented maximum rejected");
}

static void testAcceptedFixtures(void)
{
  for (uint16_t index = 0; index < DSMR_FIXTURE_COUNT; index++)
  {
    const dsmrFixture *fixture = &dsmrFixtures[index];

    if (!fixture->expectAccepted)
    {
      continue;
    }

    size_t length = strlen(fixture->telegram);
    dsmrParser parser;
    Collector collector;

    CHECK(initForFixture(&parser, fixture, NULL, false), "accepted fixture initializes parser");
    collectorReset(&collector);
    CHECK(feedAll(&parser, &collector, fixture->telegram, length, 1000, 0), "bulk feed succeeds");
    CHECK(collector.telegramCount == 1, "one telegram event per accepted frame");
    CHECK(collector.diagnosticCount == 0, "no diagnostics for accepted frame");
    CHECK(collector.result.fieldCount > 0, "accepted frame yields fields");
    CHECK(collector.result.crcStatus == DSMR_CRC_STATUS_VERIFIED, "accepted frame reports verified CRC");
    CHECK(collector.result.meterIdentification != NULL, "meter identification present");
    CHECK(strcmp(collector.result.profileId, fixture->profileId) == 0, "profile id reported in result");
    CHECK(collector.result.reportedVersion != NULL, "reported version present");

    //-- The raw view must be the accepted frame byte for byte, including
    //-- every original line ending and trailer byte. The accepted frame is
    //-- a suffix of the fed bytes: fixtures may prepend noise before the
    //-- header, which framing discards.
    CHECK((collector.result.rawBytes != NULL) && (collector.result.rawLength <= (uint16_t)length), "raw view fits inside the fed bytes");
    CHECK((collector.result.rawBytes != NULL) && (collector.result.rawLength > 0u) && (collector.result.rawBytes[0] == '/'), "raw view starts at the frame header");
    CHECK((collector.result.rawBytes != NULL) && (collector.result.rawLength <= (uint16_t)length) && (memcmp(collector.result.rawBytes, fixture->telegram + (length - collector.result.rawLength), collector.result.rawLength) == 0), "raw view matches the accepted frame byte for byte");

    //-- The same frame must give the same result one byte at a time and in
    //-- odd-sized chunks.
    uint16_t bulkFieldCount = collector.result.fieldCount;

    CHECK(initForFixture(&parser, fixture, NULL, false), "re-init for byte-wise feed");
    collectorReset(&collector);
    feedAll(&parser, &collector, fixture->telegram, length, 2000, 1);
    CHECK(collector.telegramCount == 1 && collector.result.fieldCount == bulkFieldCount, "byte-wise feed yields the same result");
    CHECK((collector.result.rawBytes != NULL) && (collector.result.rawLength <= (uint16_t)length) && (memcmp(collector.result.rawBytes, fixture->telegram + (length - collector.result.rawLength), collector.result.rawLength) == 0), "byte-wise feed yields the same raw view");

    CHECK(initForFixture(&parser, fixture, NULL, false), "re-init for chunked feed");
    collectorReset(&collector);
    feedAll(&parser, &collector, fixture->telegram, length, 3000, 7);
    CHECK(collector.telegramCount == 1 && collector.result.fieldCount == bulkFieldCount, "chunked feed yields the same result");
  }
}

static void testRejectedFixtures(void)
{
  for (uint16_t index = 0; index < DSMR_FIXTURE_COUNT; index++)
  {
    const dsmrFixture *fixture = &dsmrFixtures[index];

    if (fixture->expectAccepted)
    {
      continue;
    }

    size_t length = strlen(fixture->telegram);
    dsmrParser parser;
    Collector collector;

    CHECK(initForFixture(&parser, fixture, NULL, false), "rejected fixture initializes parser");
    collectorReset(&collector);
    CHECK(feedAll(&parser, &collector, fixture->telegram, length, 1000, 0), "feed succeeds for rejected frame");

    if (fixture->needsTick)
    {
      dsmrParserTick(&parser, 1000u + DSMR_DEFAULT_INTER_BYTE_TIMEOUT_MS + 1000u, onEvent, &collector);
    }

    char label[96];
    snprintf(label, sizeof label, "%s yields %s", fixture->id, dsmrDiagReasonToString(fixture->expectedReason));
    CHECK(collector.telegramCount == 0 && collector.diagnosticCount == 1 && collector.lastDiag == fixture->expectedReason, label);

    //-- Every rejected candidate must allow recovery on the next good frame.
    const dsmrFixture *recovery = &dsmrFixtures[0];
    CHECK(initForFixture(&parser, recovery, NULL, false), "re-init for recovery");
    collectorReset(&collector);
    feedAll(&parser, &collector, recovery->telegram, strlen(recovery->telegram), 50000, 0);
    CHECK(collector.telegramCount == 1, "next valid frame is accepted after rejection");
  }
}

static void testMultipleFramesPerBuffer(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[0];
  char buffer[2 * DSMR_MAX_TELEGRAM_BYTES];
  size_t length = strlen(fixture->telegram);

  memcpy(buffer, fixture->telegram, length);
  memcpy(buffer + length, fixture->telegram, length);

  dsmrParser parser;
  Collector collector;

  CHECK(initForFixture(&parser, fixture, NULL, false), "init for multi-frame test");
  collectorReset(&collector);
  CHECK(dsmrParserFeed(&parser, (const uint8_t *)buffer, length * 2u, 1000, onEvent, &collector) == DSMR_OK, "multi-frame feed");
  CHECK(collector.telegramCount == 2, "two telegrams in one buffer, delivered in arrival order");
}

static void testFrameTimeout(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[0];
  size_t half = strlen(fixture->telegram) / 2u;

  dsmrParser parser;
  Collector collector;

  CHECK(initForFixture(&parser, fixture, NULL, false), "init for frame timeout");
  collectorReset(&collector);

  //-- Keep the byte gap under the inter-byte timeout so the frame timeout
  //-- fires first.
  for (size_t offset = 0; offset < half; offset++)
  {
    uint32_t nowMs = 1000u + (uint32_t)offset * 600u;
    dsmrParserFeed(&parser, (const uint8_t *)(fixture->telegram + offset), 1, nowMs, onEvent, &collector);

    if (collector.diagnosticCount > 0)
    {
      break;
    }
  }

  CHECK(collector.diagnosticCount == 1 && collector.lastDiag == DSMR_DIAG_TIMEOUT_FRAME, "slow frame exceeds the total frame timeout");

  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 200000, 0);
  CHECK(collector.telegramCount == 1, "recovery after frame timeout");
}

static void testInterByteTimeout(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[0];
  size_t half = strlen(fixture->telegram) / 2u;

  dsmrParser parser;
  Collector collector;

  CHECK(initForFixture(&parser, fixture, NULL, false), "init for inter-byte timeout");
  collectorReset(&collector);
  dsmrParserFeed(&parser, (const uint8_t *)fixture->telegram, half, 1000, onEvent, &collector);
  CHECK(collector.telegramCount == 0 && collector.diagnosticCount == 0, "partial frame yields nothing yet");

  dsmrParserTick(&parser, 1000u + DSMR_DEFAULT_INTER_BYTE_TIMEOUT_MS + 1u, onEvent, &collector);
  CHECK(collector.diagnosticCount == 1 && collector.lastDiag == DSMR_DIAG_TIMEOUT_INTER_BYTE, "inter-byte timeout fires while idle");

  collectorReset(&collector);
  dsmrParserTick(&parser, 1000u + 3u * DSMR_DEFAULT_INTER_BYTE_TIMEOUT_MS, onEvent, &collector);
  CHECK(collector.diagnosticCount == 0, "no repeated timeout after restart");

  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 10000, 0);
  CHECK(collector.telegramCount == 1, "recovery after inter-byte timeout");
}

static void testOversizeAndRecovery(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[0];
  dsmrParser parser;
  Collector collector;
  char flood[DSMR_MAX_TELEGRAM_BYTES + 1024];

  CHECK(initForFixture(&parser, fixture, NULL, false), "init for oversize test");
  memset(flood, 'A', sizeof flood);
  flood[0] = '/';

  collectorReset(&collector);
  feedAll(&parser, &collector, flood, sizeof flood, 1000, 4096);
  CHECK(collector.telegramCount == 0 && collector.diagnosticCount == 1 && collector.lastDiag == DSMR_DIAG_SIZE_LIMIT, "oversize frame reports size limit");

  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 20000, 0);
  CHECK(collector.telegramCount == 1, "recovery after oversize frame");
}

static void testDataLoss(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[0];
  dsmrParser parser;
  Collector collector;

  CHECK(initForFixture(&parser, fixture, NULL, false), "init for data loss");
  collectorReset(&collector);

  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram) / 2u, 1000, 0);
  dsmrParserNotifyDataLoss(&parser, onEvent, &collector);
  CHECK(collector.diagnosticCount == 1 && collector.lastDiag == DSMR_DIAG_DATA_LOSS, "data loss notification reported");

  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 20000, 0);
  CHECK(collector.telegramCount == 1, "recovery after data loss");
}

static const dsmrFieldLabel testLabels[] =
{
  { .obis = "1-0:1.8.1", .name = "energyDeliveredTariff1" },
  { .obis = "1-0:1.8.2", .name = "energyDeliveredTariff2" },
  { .obis = "1-0:2.8.1", .name = "energyReturnedTariff1" },
  { .obis = "1-0:2.8.2", .name = "energyReturnedTariff2" },
};

static void testLabelsAndLookup(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[0];
  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config = makeConfig(fixture);

  config.fieldLabels = testLabels;
  config.fieldLabelCount = (uint16_t)(sizeof testLabels / sizeof testLabels[0]);

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init with label table");
  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 1000, 0);
  CHECK(collector.telegramCount == 1, "telegram with labels");

  const dsmrTelegramField *labeled = findLocalField(&collector, "1-0:1.8.1");
  CHECK(labeled != NULL, "labeled field present");

  if (labeled != NULL)
  {
    CHECK(labeled->name != NULL && strcmp(labeled->name, "energyDeliveredTariff1") == 0, "label resolved from application table");
    CHECK(labeled->unit != NULL && strcmp(labeled->unit, "kWh") == 0, "unit comes from the profile table");
    CHECK(labeled->value.kind == DSMR_VALUE_NUMBER && labeled->value.mantissa == 123456 && labeled->value.scale == 3, "decimal precision preserved");

    char text[32];
    dsmrFormatValue(&labeled->value, text, sizeof text);
    CHECK(strcmp(text, "123.456") == 0, "formatted value text");
  }

  const dsmrTelegramField *unlabeled = findLocalField(&collector, "0-0:96.14.0");
  CHECK(unlabeled != NULL && unlabeled->name == NULL, "field without label has null name");

  const dsmrTelegramField *found = NULL;
  CHECK(dsmrResultFindFieldByObis(&collector.result, "0-0:96.14.0", &found) == DSMR_OK && found == unlabeled, "lookup by OBIS code");
  CHECK(dsmrResultFindFieldByObis(&collector.result, "9-9:9.9.9", &found) == DSMR_ERR_NOT_FOUND && found == NULL, "lookup miss reported");
}

static const dsmrMbusDevice twoDevicesWater[] =
{
  { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_GAS },
  { .channel = 2, .deviceType = DSMR_MBUS_DEVICE_WATER },
};

static const dsmrMbusDevice twoDevicesHeat[] =
{
  { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_GAS },
  { .channel = 2, .deviceType = DSMR_MBUS_DEVICE_HEAT },
};

static void testApplyConfig(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[1]; //-- three-phase with gas and water
  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config = makeConfig(fixture);

  config.mbusDevices = twoDevicesWater;
  config.mbusDeviceCount = 2;
  config.fieldLabels = testLabels;
  config.fieldLabelCount = (uint16_t)(sizeof testLabels / sizeof testLabels[0]);

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init with two declared devices");

  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 1000, 0);
  CHECK(collector.telegramCount == 1 && collector.typeMismatchCount == 0, "consistent device table gives no mismatch");

  //-- Replace the water meter on channel 2 with a heat meter. The published
  //-- 0-1:24.1.0/0-2:24.1.0 values now contradict the declared type.
  config.mbusDevices = twoDevicesHeat;
  CHECK(dsmrParserApplyConfig(&parser, &config) == DSMR_OK, "device table change accepted");

  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 5000, 0);
  CHECK(collector.telegramCount == 1 && collector.typeMismatchCount == 1, "contradicting declaration yields mismatch diagnostic");

  //-- A rejected configuration leaves the previous configuration active.
  dsmrParserConfig broken = config;
  broken.profileId = "xx.nope.1";
  CHECK(dsmrParserApplyConfig(&parser, &broken) == DSMR_ERR_UNKNOWN_PROFILE, "rejected configuration reported");

  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 9000, 0);
  CHECK(collector.telegramCount == 1 && collector.typeMismatchCount == 1, "previous configuration still active");

  //-- A partial telegram in progress at the moment of reconfiguration is
  //-- discarded and the next complete telegram is accepted.
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram) / 2u, 12000, 0);
  CHECK(dsmrParserApplyConfig(&parser, &config) == DSMR_OK, "reconfigure between bytes");

  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 13000, 0);
  CHECK(collector.telegramCount == 1, "telegram after discarded partial frame accepted");
}

static void testAutoProfileSelection(void)
{
  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config;

  memset(&config, 0, sizeof config);
  config.countryCode = "NL";
  config.profileId = "nl.dsmr.4";
  config.mbusDevices = &twoDevicesWater[0];
  config.mbusDeviceCount = 1;

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init automatic profile");
  collectorReset(&collector);
  feedAll(&parser, &collector, dsmrFixtures[0].telegram, strlen(dsmrFixtures[0].telegram), 1000, 0);
  CHECK(collector.telegramCount == 1 && strcmp(collector.result.profileId, "nl.dsmr.4.2") == 0, "version 42 selects the 4.2 table");

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "re-init for 4.0 fixture");
  collectorReset(&collector);
  feedAll(&parser, &collector, dsmrFixtures[2].telegram, strlen(dsmrFixtures[2].telegram), 2000, 0);
  CHECK(collector.telegramCount == 1 && strcmp(collector.result.profileId, "nl.dsmr.4.0") == 0, "version 40 selects the 4.0 table");
}

static void testUnknownMinorVersionOption(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[12]; //-- unknown minor version 43
  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config = makeConfig(fixture);

  config.acceptUnknownMinorVersion = false;
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init strict version option");
  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 1000, 0);
  CHECK(collector.telegramCount == 0 && collector.lastDiag == DSMR_DIAG_INVALID_VERSION, "unknown minor version rejected by default");

  config.acceptUnknownMinorVersion = true;
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init permissive version option");
  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 2000, 0);
  CHECK(collector.telegramCount == 1, "unknown minor version accepted with the option");

  if (collector.telegramCount == 1)
  {
    CHECK(strcmp(collector.result.reportedVersion, "43") == 0, "original version is reported");
  }
}

static void testMbusChannelAndDeviceType(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[1];
  dsmrParser parser;
  Collector collector;

  CHECK(initForFixture(&parser, fixture, NULL, false), "init for M-Bus test");
  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 1000, 0);
  CHECK(collector.telegramCount == 1, "telegram with two M-Bus channels");

  const dsmrTelegramField *gasReading = NULL;
  const dsmrTelegramField *waterReading = NULL;

  for (uint16_t index = 0; index < collector.result.fieldCount; index++)
  {
    const dsmrTelegramField *field = &collector.result.fields[index];

    if (strcmp(field->obis, "0-1:24.2.1") == 0)
    {
      gasReading = field;
    }

    if (strcmp(field->obis, "0-2:24.2.1") == 0)
    {
      waterReading = field;
    }
  }

  CHECK(gasReading != NULL && gasReading->mbusChannel == 1, "gas reading carries channel 1");
  CHECK(waterReading != NULL && waterReading->mbusChannel == 2, "water reading carries channel 2");

  const dsmrTelegramField *gasTime = NULL;

  for (uint16_t index = 0; index < collector.result.fieldCount; index++)
  {
    if (strcmp(collector.result.fields[index].obis, "0-1:24.3.1") == 0)
    {
      gasTime = &collector.result.fields[index];
    }
  }

  CHECK(gasTime != NULL && gasTime->hasTimestamp && gasTime->timestamp.valid, "per-channel measurement time retained");
}

static void testValueStatesAndTimestamps(void)
{
  dsmrParser parser;
  Collector collector;
  char frame[2048];
  dsmrParserConfig config;

  memset(&config, 0, sizeof config);
  config.countryCode = "NL";
  config.profileId = "nl.dsmr.4.2";

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for value states");
  collectorReset(&collector);

  size_t length = buildFrame(frame, sizeof frame,
                             "/TEST\r\n\r\n"
                             "1-3:0.2.8(42)\n"
                             "1-0:1.8.1(0.000*kWh)\n"
                             "1-0:1.8.2()\n"
                             "1-0:2.8.1(FF)\n"
                             "0-0:1.0.0(180611100000S)\n");
  CHECK(length > 0, "value-state frame built");
  feedAll(&parser, &collector, frame, length, 1000, 0);
  CHECK(collector.telegramCount == 1, "value-state frame accepted");

  const dsmrTelegramField *zero = findLocalField(&collector, "1-0:1.8.1");
  const dsmrTelegramField *missing = findLocalField(&collector, "1-0:1.8.2");
  const dsmrTelegramField *invalid = findLocalField(&collector, "1-0:2.8.1");
  const dsmrTelegramField *stamp = findLocalField(&collector, "0-0:1.0.0");

  CHECK(zero != NULL && zero->value.kind == DSMR_VALUE_NUMBER && !zero->value.protocolInvalid && zero->value.mantissa == 0, "valid zero distinguished");
  CHECK(missing != NULL && missing->value.kind == DSMR_VALUE_ABSENT, "absent value distinguished");
  CHECK(invalid != NULL && invalid->value.protocolInvalid, "protocol-invalid value distinguished");
  CHECK(stamp != NULL && stamp->hasTimestamp && stamp->timestamp.valid && stamp->timestamp.year == 2018 && stamp->timestamp.dst == 'S', "timestamp parsed with S indicator");

  //-- An invalid calendar date must be rejected.
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "re-init for bad date");
  collectorReset(&collector);
  length = buildFrame(frame, sizeof frame,
                      "/TEST\r\n\r\n"
                      "1-3:0.2.8(42)\n"
                      "0-0:1.0.0(181332100000S)\n");
  feedAll(&parser, &collector, frame, length, 2000, 0);
  CHECK(collector.telegramCount == 0 && collector.lastDiag == DSMR_DIAG_INVALID_TIMESTAMP, "invalid calendar date rejected");
}

static void testFormatValue(void)
{
  dsmrValue value;
  char text[32];

  dsmrValueSetAbsent(&value);
  CHECK(dsmrFormatValue(&value, text, sizeof text) == 0 && text[0] == '\0', "absent formats empty");

  CHECK(dsmrValueParseDecimal("123.456", 3, &value) == 1, "parse 123.456");
  dsmrFormatValue(&value, text, sizeof text);
  CHECK(strcmp(text, "123.456") == 0, "format 123.456");

  CHECK(dsmrValueParseDecimal("-0.5", 1, &value) == 1, "parse -0.5");
  dsmrFormatValue(&value, text, sizeof text);
  CHECK(strcmp(text, "-0.5") == 0, "format negative fraction below one");

  CHECK(dsmrValueParseDecimal("0001", 0, &value) == 1, "parse 0001");
  dsmrFormatValue(&value, text, sizeof text);
  CHECK(strcmp(text, "1") == 0, "format integer");

  CHECK(dsmrValueParseDecimal("FF", 3, &value) == 1 && value.protocolInvalid, "parse FF marker");
  dsmrFormatValue(&value, text, sizeof text);
  CHECK(strcmp(text, "FF") == 0, "format invalid marker");

  CHECK(dsmrValueParseDecimal("1.2345", 3, &value) == 0, "scale beyond profile precision rejected");
  CHECK(dsmrValueParseDecimal("12a3", 3, &value) == 0, "malformed decimal rejected");
}

static void testLfCompatibility(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[0];

  //-- Re-frame the fixture with bare LF endings. The trailer CRC is
  //-- recomputed over the LF bytes, as a meter using LF endings would do.
  const char *bang = strrchr(fixture->telegram, '!');
  char lfBody[1024];
  size_t bodyLength = (size_t)(bang - fixture->telegram);

  memcpy(lfBody, fixture->telegram, bodyLength);
  lfBody[bodyLength] = '\0';

  char lfFrame[2048];
  size_t outLength = buildFrameEx(lfFrame, sizeof lfFrame, lfBody, false);
  CHECK(outLength > 0, "LF frame built");

  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config = makeConfig(fixture);

  config.acceptLfLineEndings = false;
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init strict LF mode");
  collectorReset(&collector);
  feedAll(&parser, &collector, lfFrame, outLength, 1000, 0);
  CHECK(collector.telegramCount == 0, "LF-only frame rejected by strict profiles");

  config.acceptLfLineEndings = true;
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init LF compatibility mode");
  collectorReset(&collector);
  feedAll(&parser, &collector, lfFrame, outLength, 2000, 0);
  CHECK(collector.telegramCount == 1 && collector.result.crcStatus == DSMR_CRC_STATUS_VERIFIED, "LF-only frame accepted with CRC over received bytes");
}

static void testReentrancyGuard(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[0];
  dsmrParser parser;
  Collector collector;

  CHECK(initForFixture(&parser, fixture, NULL, false), "init for reentrancy");
  collectorReset(&collector);
  collector.feedFromCallback = true;
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 1000, 0);
  CHECK(collector.telegramCount == 1, "telegram delivered");
  CHECK(collector.reentrantStatus == DSMR_ERR_REENTRANT_CALL, "re-entry from callback refused");
}

static const dsmrFieldDefinition customExtraFields[] =
{
  { .obis = "1-3:0.2.8", .type = DSMR_FIELD_TYPE_TEXT, .unit = NULL, .maxScale = 0, .groupCount = 1 },
  { .obis = "1-0:1.8.1", .type = DSMR_FIELD_TYPE_DECIMAL, .unit = "kWh", .maxScale = 3, .groupCount = 1 },
  { .obis = "0-0:96.3.1", .type = DSMR_FIELD_TYPE_INTEGER, .unit = NULL, .maxScale = 0, .groupCount = 1 },
};

static const dsmrFieldBlock customBlocks[] =
{
  { customExtraFields, (uint16_t)(sizeof customExtraFields / sizeof customExtraFields[0]) },
};

static const char *const customVersions[] = { "42", NULL };

static const dsmrProfile testCustomProfile =
{
  .profileId = "test.custom.1",
  .countryCode = "NL",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = customVersions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 1000,
  .fieldBlocks = customBlocks,
  .fieldBlockCount = (uint16_t)(sizeof customBlocks / sizeof customBlocks[0]),
  .autoProfiles = NULL,
};

//-- A test profile demonstrates that a new field can be added without
//-- altering the common framing or UART reception path.
static void testCustomProfileAddsField(void)
{
  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config;
  char frame[2048];

  memset(&config, 0, sizeof config);
  config.countryCode = "NL";
  config.profileId = "test.custom.1";
  config.customProfile = &testCustomProfile;

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init custom profile");
  collectorReset(&collector);

  size_t length = buildFrame(frame, sizeof frame,
                             "/TEST\r\n\r\n"
                             "1-3:0.2.8(42)\n"
                             "1-0:1.8.1(000001.000*kWh)\n"
                             "0-0:96.3.1(7)\n");
  feedAll(&parser, &collector, frame, length, 1000, 0);
  CHECK(collector.telegramCount == 1, "custom profile telegram accepted");

  const dsmrTelegramField *extra = findLocalField(&collector, "0-0:96.3.1");
  CHECK(extra != NULL && extra->value.kind == DSMR_VALUE_NUMBER && extra->value.mantissa == 7, "new field decoded through the shared framer");
}

static void testUnknownFieldPolicy(void)
{
  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config;
  char frame[2048];

  memset(&config, 0, sizeof config);
  config.countryCode = "NL";
  config.profileId = "nl.dsmr.4.2";
  config.unknownFieldPolicy = DSMR_UNKNOWN_FIELD_SKIP;

  size_t length = buildFrame(frame, sizeof frame,
                             "/TEST\r\n\r\n"
                             "1-3:0.2.8(42)\n"
                             "1-0:1.8.1(000001.000*kWh)\n"
                             "9-9:99.99.9(123)\n");

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init skip policy");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 1000, 0);
  CHECK(collector.telegramCount == 1 && findLocalField(&collector, "9-9:99.99.9") == NULL, "unknown field skipped by default");

  config.unknownFieldPolicy = DSMR_UNKNOWN_FIELD_RAW;
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init raw policy");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 2000, 0);

  const dsmrTelegramField *raw = findLocalField(&collector, "9-9:99.99.9");
  CHECK(collector.telegramCount == 1 && raw != NULL && raw->value.kind == DSMR_VALUE_TEXT, "unknown field exposed as bounded raw data");
}

//-- --------------------------------------------------------------------------
//-- Main
//-- --------------------------------------------------------------------------

int main(void)
{
  testCrcCheckValue();
  testSampleTelegram();
  testInitErrors();
  testAcceptedFixtures();
  testRejectedFixtures();
  testMultipleFramesPerBuffer();
  testFrameTimeout();
  testInterByteTimeout();
  testOversizeAndRecovery();
  testDataLoss();
  testLabelsAndLookup();
  testApplyConfig();
  testAutoProfileSelection();
  testUnknownMinorVersionOption();
  testMbusChannelAndDeviceType();
  testValueStatesAndTimestamps();
  testFormatValue();
  testLfCompatibility();
  testReentrancyGuard();
  testCustomProfileAddsField();
  testUnknownFieldPolicy();

  printf("%d checks, %d failures\n", checks, failures);
  return (failures == 0) ? 0 : 1;
}
