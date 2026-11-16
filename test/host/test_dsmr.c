//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: test/host/test_dsmr.c - host tests for the portable C11 core
//-- Date: 2026-11-16 · Build: 6
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
  bool setFromCallback;
  dsmrParserStatus setLabelStatus;
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

    if (collector->setFromCallback)
    {
      collector->setLabelStatus = dsmrParserSetFieldLabel((dsmrParser *)parser, "1-0:1.8.1", "late");
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

static void testVersionString(void)
{
  //-- The getter must hand out the parser's own version without allocation.
  CHECK(dsmrParserVersion() != NULL, "dsmrParserVersion never NULL");
  CHECK(strcmp(dsmrParserVersion(), "v0.9.0") == 0, "dsmrParserVersion reports v0.9.0");
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

    //-- The automatic nl.dsmr.4 profile reports the concrete 4.0 or 4.2
    //-- profile resolved from the reported version; every other profile
    //-- reports itself.
    bool resolvedIdOk = (strcmp(collector.result.profileId, fixture->profileId) == 0) ||
                        ((strcmp(fixture->profileId, "nl.dsmr.4") == 0) &&
                         ((strcmp(collector.result.profileId, "nl.dsmr.4.0") == 0) || (strcmp(collector.result.profileId, "nl.dsmr.4.2") == 0)));
    CHECK(resolvedIdOk, "profile id reported in result");
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
  CHECK(unlabeled != NULL && unlabeled->name != NULL && strcmp(unlabeled->name, "tarInd") == 0, "field without application label falls back to the built-in default name");

  const dsmrTelegramField *found = NULL;
  CHECK(dsmrResultFindFieldByObis(&collector.result, "0-0:96.14.0", &found) == DSMR_OK && found == unlabeled, "lookup by OBIS code");
  CHECK(dsmrResultFindFieldByObis(&collector.result, "9-9:9.9.9", &found) == DSMR_ERR_NOT_FOUND && found == NULL, "lookup miss reported");
}

//-- The built-in default display names apply when the application registers
//-- no label, one registered label overrides just its own code, and unknown
//-- fields keep a null name. M-Bus wildcard rows supply their default too.
static void testDefaultFieldNames(void)
{
  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config;
  char frame[2048];

  memset(&config, 0, sizeof config);
  config.countryCode = "NL";
  config.profileId = "nl.dsmr.4.2";
  config.unknownFieldPolicy = DSMR_UNKNOWN_FIELD_RAW;

  size_t length = buildFrame(frame, sizeof frame,
                             "/TEST\r\n\r\n"
                             "1-3:0.2.8(42)\n"
                             "1-0:1.8.1(000001.000*kWh)\n"
                             "0-0:96.14.0(1)\n"
                             "1-0:32.7.0(230.00*V)\n"
                             "9-9:99.99.9(123)\n");

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for default names");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 1000, 0);
  CHECK(collector.telegramCount == 1, "default names telegram accepted");

  const dsmrTelegramField *delivered = findLocalField(&collector, "1-0:1.8.1");
  CHECK((delivered != NULL) && (delivered->name != NULL) && (strcmp(delivered->name, "enrgyDelvrdTar1") == 0), "known field gets its built-in default name");

  const dsmrTelegramField *tariff = findLocalField(&collector, "0-0:96.14.0");
  CHECK((tariff != NULL) && (tariff->name != NULL) && (strcmp(tariff->name, "tarInd") == 0), "tariff indicator gets its built-in default name");

  const dsmrTelegramField *voltage = findLocalField(&collector, "1-0:32.7.0");
  CHECK((voltage != NULL) && (voltage->name != NULL) && (strcmp(voltage->name, "voltL1") == 0), "phase voltage gets its built-in default name");

  const dsmrTelegramField *raw = findLocalField(&collector, "9-9:99.99.9");
  CHECK((raw != NULL) && (raw->name == NULL), "unknown field keeps a null name");

  //-- One registered application label overrides just that code; every
  //-- other code keeps its built-in default.
  static const dsmrFieldLabel singleOverride[] =
  {
    { .obis = "1-0:1.8.1", .name = "appPick" },
  };

  config.fieldLabels = singleOverride;
  config.fieldLabelCount = 1;
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init with a single override");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 2000, 0);
  CHECK(collector.telegramCount == 1, "override telegram accepted");

  delivered = findLocalField(&collector, "1-0:1.8.1");
  CHECK((delivered != NULL) && (delivered->name != NULL) && (strcmp(delivered->name, "appPick") == 0), "application label wins over the built-in default");
  tariff = findLocalField(&collector, "0-0:96.14.0");
  CHECK((tariff != NULL) && (tariff->name != NULL) && (strcmp(tariff->name, "tarInd") == 0), "unregistered code keeps its default name");

  //-- M-Bus wildcard rows (0-?:24.2.1) supply the default for the concrete
  //-- channel through the same definition lookup.
  const dsmrFixture *threePhase = &dsmrFixtures[1];
  dsmrParser mbusParser;
  Collector mbusCollector;

  CHECK(initForFixture(&mbusParser, threePhase, NULL, false), "init for default M-Bus names");
  collectorReset(&mbusCollector);
  feedAll(&mbusParser, &mbusCollector, threePhase->telegram, strlen(threePhase->telegram), 1500, 0);
  CHECK(mbusCollector.telegramCount == 1, "three-phase fixture accepted for default M-Bus names");

  const dsmrTelegramField *mbus = findLocalField(&mbusCollector, "0-1:24.2.1");
  CHECK((mbus != NULL) && (mbus->name != NULL) && (strcmp(mbus->name, "mbusRdng") == 0), "M-Bus wildcard row supplies the default name");
}

//-- dsmrParserSetFieldLabel renames, replaces and removes one OBIS code at
//-- run time without rebuilding the table, and reports its error cases.
static void testSetFieldLabel(void)
{
  const dsmrFixture *fixture = &dsmrFixtures[0];
  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config = makeConfig(fixture);

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for setter test");
  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 1000, 0);
  CHECK(collector.telegramCount == 1, "setter baseline telegram");

  const dsmrTelegramField *field = findLocalField(&collector, "1-0:1.8.1");
  CHECK((field != NULL) && (field->name != NULL) && (strcmp(field->name, "enrgyDelvrdTar1") == 0), "setter baseline shows the default name");

  CHECK(dsmrParserSetFieldLabel(&parser, "1-0:1.8.1", "gridImportTar1") == DSMR_OK, "override one field");
  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 2000, 0);
  field = findLocalField(&collector, "1-0:1.8.1");
  CHECK((field != NULL) && (field->name != NULL) && (strcmp(field->name, "gridImportTar1") == 0), "single field renamed");

  const dsmrTelegramField *untouched = findLocalField(&collector, "0-0:96.14.0");
  CHECK((untouched != NULL) && (untouched->name != NULL) && (strcmp(untouched->name, "tarInd") == 0), "other fields keep their defaults");

  //-- A NULL name drops the override and restores the default.
  CHECK(dsmrParserSetFieldLabel(&parser, "1-0:1.8.1", NULL) == DSMR_OK, "override removed");
  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 3000, 0);
  field = findLocalField(&collector, "1-0:1.8.1");
  CHECK((field != NULL) && (field->name != NULL) && (strcmp(field->name, "enrgyDelvrdTar1") == 0), "default name returns after removal");

  //-- Removing an override that was never set succeeds.
  CHECK(dsmrParserSetFieldLabel(&parser, "1-0:2.8.1", NULL) == DSMR_OK, "removing an absent override succeeds");

  //-- Replacing an existing entry reuses its slot; removal shifts the
  //-- remaining entries so none is lost.
  static const dsmrFieldLabel baseLabels[] =
  {
    { .obis = "1-0:1.8.1", .name = "first" },
    { .obis = "0-0:96.14.0", .name = "second" },
  };

  config = makeConfig(fixture);
  config.fieldLabels = baseLabels;
  config.fieldLabelCount = 2;
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for replace and shift");
  CHECK(dsmrParserSetFieldLabel(&parser, "1-0:1.8.1", NULL) == DSMR_OK, "remove first of two entries");
  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 4000, 0);
  field = findLocalField(&collector, "1-0:1.8.1");
  CHECK((field != NULL) && (field->name != NULL) && (strcmp(field->name, "enrgyDelvrdTar1") == 0), "removed entry fell back to its default");
  field = findLocalField(&collector, "0-0:96.14.0");
  CHECK((field != NULL) && (field->name != NULL) && (strcmp(field->name, "second") == 0), "shifted entry kept its label");

  //-- Argument validation.
  CHECK(dsmrParserSetFieldLabel(NULL, "1-0:1.8.1", "x") == DSMR_ERR_NULL_ARGUMENT, "null parser rejected");
  CHECK(dsmrParserSetFieldLabel(&parser, NULL, "x") == DSMR_ERR_NULL_ARGUMENT, "null obis rejected");
  CHECK(dsmrParserSetFieldLabel(&parser, "", "x") == DSMR_ERR_INVALID_CONFIG, "empty obis rejected");
  CHECK(dsmrParserSetFieldLabel(&parser, "1-0:12345678.7.8", "x") == DSMR_ERR_INVALID_CONFIG, "oversized obis rejected");
  CHECK(dsmrParserSetFieldLabel(&parser, "1-0:1.8.1", "012345678901234567890123456789012") == DSMR_ERR_INVALID_CONFIG, "oversized name rejected");

  dsmrParser fresh;
  memset(&fresh, 0, sizeof fresh);
  CHECK(dsmrParserSetFieldLabel(&fresh, "1-0:1.8.1", "x") == DSMR_ERR_NOT_INITIALIZED, "uninitialised parser rejected");

  //-- Re-entry from the event callback is refused like the other operations.
  dsmrParser reentry;
  dsmrParserConfig reentryConfig = makeConfig(fixture);

  CHECK(dsmrParserInit(&reentry, &reentryConfig, workspace, sizeof workspace) == DSMR_OK, "init for setter reentrancy");
  collectorReset(&collector);
  collector.setFromCallback = true;
  feedAll(&reentry, &collector, fixture->telegram, strlen(fixture->telegram), 5000, 0);
  CHECK(collector.telegramCount == 1, "telegram delivered for setter reentrancy");
  CHECK(collector.setLabelStatus == DSMR_ERR_REENTRANT_CALL, "setter from callback refused");

  //-- A full override table reports insufficient workspace and the parser
  //-- keeps decoding.
  char obisBuf[16];
  char nameBuf[32];

  for (uint16_t index = 0; index < DSMR_MAX_FIELD_LABELS + 2u; index++)
  {
    (void)snprintf(obisBuf, sizeof obisBuf, "9-9:1.%u.%u", (unsigned)(index / 100u), (unsigned)(index % 100u));
    (void)snprintf(nameBuf, sizeof nameBuf, "fill%u", (unsigned)index);

    if (dsmrParserSetFieldLabel(&parser, obisBuf, nameBuf) != DSMR_OK)
    {
      break;
    }
  }

  CHECK(dsmrParserSetFieldLabel(&parser, "9-9:2.0.0", "overflow") == DSMR_ERR_INSUFFICIENT_WORKSPACE, "full label table rejected");
  collectorReset(&collector);
  feedAll(&parser, &collector, fixture->telegram, strlen(fixture->telegram), 6000, 0);
  CHECK(collector.telegramCount == 1, "parser still decodes with a full label table");
}

//-- Every built-in field row carries a default name within the label bound,
//-- and the same OBIS code carries the same default in every profile.
static void testDefaultNameConsistency(void)
{
  static const char *seenObis[128];
  static const char *seenNames[128];
  uint16_t seenCount = 0;
  uint16_t rowCount = 0;

  const char *const *ids = dsmrProfileListIds();

  for (uint16_t idIndex = 0; ids[idIndex] != NULL; idIndex++)
  {
    const dsmrProfile *profile = dsmrProfileLookup(ids[idIndex]);
    CHECK(profile != NULL, "built-in profile id resolves");

    if (profile == NULL)
    {
      continue;
    }

    for (uint16_t block = 0; block < profile->fieldBlockCount; block++)
    {
      const dsmrFieldDefinition *entries = profile->fieldBlocks[block].entries;

      for (uint16_t entry = 0; entry < profile->fieldBlocks[block].count; entry++)
      {
        rowCount++;
        CHECK(entries[entry].name != NULL, "built-in field row carries a default name");

        if (entries[entry].name == NULL)
        {
          continue;
        }

        CHECK(strlen(entries[entry].name) < DSMR_MAX_LABEL_NAME_LEN, "default name fits the label bound");

        bool known = false;

        for (uint16_t prior = 0; prior < seenCount; prior++)
        {
          if (strcmp(seenObis[prior], entries[entry].obis) == 0)
          {
            known = true;
            CHECK(strcmp(seenNames[prior], entries[entry].name) == 0, "same obis code carries the same default in every profile");
            break;
          }
        }

        if (!known && (seenCount < (sizeof seenObis / sizeof seenObis[0])))
        {
          seenObis[seenCount] = entries[entry].obis;
          seenNames[seenCount] = entries[entry].name;
          seenCount++;
        }
      }
    }
  }

  CHECK(rowCount >= 150u, "all profile blocks were walked");
  CHECK(seenCount >= 90u, "the whole shared dictionary was seen");
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
  CHECK((raw != NULL) && (raw->classification == DSMR_FIELD_CLASS_UNKNOWN), "raw field is classified unknown, not a fault");

  const dsmrTelegramField *known = findLocalField(&collector, "1-0:1.8.1");
  CHECK((known != NULL) && (known->classification == DSMR_FIELD_CLASS_KNOWN), "profile field is classified known");

  //-- A malformed line of an unknown code must never reject the telegram.
  config.unknownFieldPolicy = DSMR_UNKNOWN_FIELD_SKIP;
  length = buildFrame(frame, sizeof frame,
                      "/TEST\r\n\r\n"
                      "1-3:0.2.8(42)\n"
                      "1-0:1.8.1(000001.000*kWh)\n"
                      "9-9:99.99.8\n"
                      "9-9:99.99.7(1.0*kWh)trailing\n");

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init skip policy for malformed unknown lines");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 3000, 0);
  CHECK(collector.telegramCount == 1 && collector.diagnosticCount == 0, "malformed unknown lines never reject the telegram");

  //-- A malformed line of a known code is still a syntax error.
  length = buildFrame(frame, sizeof frame,
                      "/TEST\r\n\r\n"
                      "1-3:0.2.8(42)\n"
                      "1-0:1.8.1(000001.000*kWh)\n"
                      "0-0:96.1.1\n");

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for malformed known line");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 4000, 0);
  CHECK(collector.telegramCount == 0 && collector.lastDiag == DSMR_DIAG_SYNTAX_ERROR, "malformed known line yields syntax error");
}

//-- The European DSMR-style profiles initialise for their own country and
//-- reject a mismatched country code.
static void testEuropeanProfiles(void)
{
  static const char *const ids[] =
  {
    "hu.dsmr.5.0", "se.dsmr.5.0", "fi.dsmr.5.0", "lt.dsmr.5.0",
    "ie.dsmr.5.0", "lu.dsmr.5.0", "ch.dsmr.5.0",
  };
  static const char *const countries[] =
  {
    "HU", "SE", "FI", "LT", "IE", "LU", "CH",
  };

  for (size_t index = 0; index < (sizeof ids / sizeof ids[0]); index++)
  {
    dsmrParser parser;
    dsmrParserConfig config;
    char label[64];

    memset(&config, 0, sizeof config);
    config.countryCode = countries[index];
    config.profileId = ids[index];

    snprintf(label, sizeof label, "%s initialises", ids[index]);
    CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, label);

    config.countryCode = "NL";
    CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_ERR_COUNTRY_MISMATCH, "european profile rejects wrong country");
  }

  const char *const *list = dsmrProfileListIds();
  uint16_t count = 0u;

  while (list[count] != NULL)
  {
    count++;
  }

  CHECK(count == 13, "profile list exposes thirteen built-in identifiers");
}

//-- The acceptance list of projectPrompt.md section 7 requires that every
//-- profile is exercised. Every built-in identifier must have at least one
//-- accepted fixture, so every supported country code is present in the
//-- shared fixture set that also drives the stored telegram test program.
static void testProfileFixtureCoverage(void)
{
  const char *const *list = dsmrProfileListIds();

  for (size_t index = 0; list[index] != NULL; index++)
  {
    bool covered = false;
    char label[64];

    for (uint16_t fixture = 0u; fixture < DSMR_FIXTURE_COUNT; fixture++)
    {
      if (dsmrFixtures[fixture].expectAccepted && (strcmp(dsmrFixtures[fixture].profileId, list[index]) == 0))
      {
        covered = true;
        break;
      }
    }

    snprintf(label, sizeof label, "accepted fixture exists for %s", list[index]);
    CHECK(covered, label);
  }
}

//-- A Hungarian style telegram: DSMR 5.0 version object, Hungarian-only
//-- objects decode with their units, and manufacturer extras remain unknown
//-- data instead of a fault. Luxembourg reports 42 on a 5.0.2 base.
static void testHungarianAndLuxembourgTelegrams(void)
{
  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config;
  char frame[2048];

  memset(&config, 0, sizeof config);
  config.countryCode = "HU";
  config.profileId = "hu.dsmr.5.0";
  config.unknownFieldPolicy = DSMR_UNKNOWN_FIELD_RAW;

  size_t length = buildFrame(frame, sizeof frame,
                             "/EON5\r\n\r\n"
                             "1-3:0.2.8(50)\r\n"
                             "0-0:1.0.0(251116104500S)\r\n"
                             "0-0:96.1.0(ABCD1234)\r\n"
                             "0-0:96.14.0(1)\r\n"
                             "1-0:1.8.0(000123.456*kWh)\r\n"
                             "1-0:15.8.0(000456.789*kWh)\r\n"
                             "1-0:14.7.0(50.01*Hz)\r\n"
                             "1-0:13.7.0(0.98)\r\n"
                             "1-0:5.8.0(000001.000*kVArh)\r\n"
                             "1-0:91.7.0(0.123*A)\r\n"
                             "0-0:96.1.1(00456CDE)\r\n"
                             "0-0:96.5.0(0000FFFF)\r\n"
                             "0-0:97.97.0(0000)\r\n");

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init Hungarian profile");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 1000, 0);
  CHECK(collector.telegramCount == 1 && collector.diagnosticCount == 0, "accept Hungarian telegram");
  CHECK(findLocalField(&collector, "1-0:15.8.0") != NULL, "Hungarian total active energy decodes");
  CHECK(findLocalField(&collector, "1-0:14.7.0") != NULL, "Hungarian frequency decodes");
  CHECK(findLocalField(&collector, "1-0:91.7.0") != NULL, "Hungarian neutral current decodes");
  CHECK(findLocalField(&collector, "0-0:96.5.0") != NULL, "Hungarian status word decodes as text");

  //-- A manufacturer object outside the table stays accepted raw data.
  length = buildFrame(frame, sizeof frame,
                      "/EON5\r\n\r\n"
                      "1-3:0.2.8(50)\r\n"
                      "1-0:1.8.0(000001.000*kWh)\r\n"
                      "0-0:96.7.22(3)\r\n");

  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 2000, 0);
  CHECK(collector.telegramCount == 1 && collector.diagnosticCount == 0, "unknown Hungarian object never rejects");
  const dsmrTelegramField *extra = findLocalField(&collector, "0-0:96.7.22");
  CHECK((extra != NULL) && (extra->classification == DSMR_FIELD_CLASS_UNKNOWN), "manufacturer extra classified unknown");

  //-- Luxembourg reports 42 on the DSMR 5.0.2 base per the Luxmetering
  //-- example telegram.
  static const dsmrMbusDevice luDevices[] =
  {
    { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_HEAT },
  };

  memset(&config, 0, sizeof config);
  config.countryCode = "LU";
  config.profileId = "lu.dsmr.5.0";
  config.mbusDevices = luDevices;
  config.mbusDeviceCount = 1;

  length = buildFrame(frame, sizeof frame,
                      "/Lux5M\r\n\r\n"
                      "1-3:0.2.8(42)\r\n"
                      "0-0:1.0.0(251116104500S)\r\n"
                      "0-0:42.0.0(5341473130333037)\r\n"
                      "0-0:96.1.0(464C463138393930)\r\n"
                      "1-0:1.8.0(001234.567*kWh)\r\n"
                      "1-0:1.7.0(001.234*kW)\r\n"
                      "1-0:9.7.0(001.234*kVA)\r\n"
                      "0-0:17.0.0(99.9*kVA)\r\n"
                      "0-0:96.3.10(1)\r\n"
                      "0-1:24.1.0(003)\r\n"
                      "0-1:24.2.1(251116104500S)(00012.345*m3)\r\n"
                      "0-1:24.4.0(0)\r\n");

  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init Luxembourg profile");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 3000, 0);
  CHECK(collector.telegramCount == 1 && collector.diagnosticCount == 0, "accept Luxembourg telegram reporting 42");
  CHECK(findLocalField(&collector, "1-0:9.7.0") != NULL, "Luxembourg apparent power decodes");
  CHECK(findLocalField(&collector, "0-0:17.0.0") != NULL, "Luxembourg SMAX threshold decodes");

  const dsmrTelegramField *mbus = findLocalField(&collector, "0-1:24.2.1");
  CHECK((mbus != NULL) && mbus->hasTimestamp, "Luxembourg M-Bus timestamp plus value decodes");
}

//-- Two device numberings are documented for 0-n:24.1.0. The Dutch DSMR
//-- numbering is 1=gas, 2=water, 3=heat; the European P1 OBIS overview and
//-- the Luxembourg example telegram use the EN 13757-3 numbering 3=gas,
//-- 4=heat, 7=water. A value that any numbering explains for the declared
//-- type must not produce the mismatch diagnostic; only a value that no
//-- numbering explains may.
static void testMbusDeviceNumberings(void)
{
  dsmrParser parser;
  Collector collector;
  dsmrParserConfig config;
  char frame[2048];
  size_t length;

  static const dsmrMbusDevice gasDevices[] = { { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_GAS } };
  static const dsmrMbusDevice waterDevices[] = { { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_WATER } };
  static const dsmrMbusDevice heatDevices[] = { { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_HEAT } };

  memset(&config, 0, sizeof config);
  config.countryCode = "HU";
  config.profileId = "hu.dsmr.5.0";
  config.mbusDevices = gasDevices;
  config.mbusDeviceCount = 1;

  length = buildFrame(frame, sizeof frame,
                      "/EON5\r\n\r\n"
                      "1-3:0.2.8(50)\r\n"
                      "0-1:24.1.0(003)\r\n");
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for gas on EN 3");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 1000, 0);
  CHECK((collector.telegramCount == 1) && (collector.typeMismatchCount == 0), "gas declared against EN 13757-3 value 3");

  config.mbusDevices = heatDevices;
  length = buildFrame(frame, sizeof frame,
                      "/EON5\r\n\r\n"
                      "1-3:0.2.8(50)\r\n"
                      "0-1:24.1.0(004)\r\n");
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for heat on EN 4");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 2000, 0);
  CHECK((collector.telegramCount == 1) && (collector.typeMismatchCount == 0), "heat declared against EN 13757-3 value 4");

  config.mbusDevices = waterDevices;
  length = buildFrame(frame, sizeof frame,
                      "/EON5\r\n\r\n"
                      "1-3:0.2.8(50)\r\n"
                      "0-1:24.1.0(007)\r\n");
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for water on EN 7");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 3000, 0);
  CHECK((collector.telegramCount == 1) && (collector.typeMismatchCount == 0), "water declared against EN 13757-3 value 7");

  length = buildFrame(frame, sizeof frame,
                      "/EON5\r\n\r\n"
                      "1-3:0.2.8(50)\r\n"
                      "0-1:24.1.0(002)\r\n");
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for water on DSMR 2");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 4000, 0);
  CHECK((collector.telegramCount == 1) && (collector.typeMismatchCount == 0), "water declared against DSMR value 2");

  length = buildFrame(frame, sizeof frame,
                      "/EON5\r\n\r\n"
                      "1-3:0.2.8(50)\r\n"
                      "0-1:24.1.0(009)\r\n");
  CHECK(dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK, "init for unexplained value");
  collectorReset(&collector);
  feedAll(&parser, &collector, frame, length, 5000, 0);
  CHECK((collector.telegramCount == 1) && (collector.typeMismatchCount == 1), "value no numbering explains yields mismatch on an accepted telegram");
}

//-- --------------------------------------------------------------------------
//-- Main
//-- --------------------------------------------------------------------------

int main(void)
{
  testCrcCheckValue();
  testVersionString();
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
  testDefaultFieldNames();
  testSetFieldLabel();
  testDefaultNameConsistency();
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
  testEuropeanProfiles();
  testProfileFixtureCoverage();
  testHungarianAndLuxembourgTelegrams();
  testMbusDeviceNumberings();

  printf("%d checks, %d failures\n", checks, failures);
  return (failures == 0) ? 0 : 1;
}
