//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: src/dsmr_parser.c - public API, framing glue and event dispatch
//-- Date: 2026-11-14 · Build: 1
//--
//-- The parser owns no memory of its own. The workspace is carved into the
//-- telegram buffer, the field array, and the deep copies of the M-Bus
//-- device table and the label table. Every value the parser needs survives
//-- the dsmrParserInit and dsmrParserApplyConfig calls, so the caller's
//-- configuration and tables may go out of scope afterwards.
//-- ==========================================================================

#include "dsmr_parser.h"

#include "dsmr_decode.h"
#include "dsmr_framing.h"
#include "dsmr_profile.h"
#include "dsmr_value.h"

#include <stdio.h>
#include <string.h>

//-- --------------------------------------------------------------------------
//-- Small helpers
//-- --------------------------------------------------------------------------

static size_t align8(size_t value)
{
  return (value + 7u) & ~(size_t)7u;
}

static char toUpperChar(char character)
{
  if ((character >= 'a') && (character <= 'z'))
  {
    return (char)(character - ('a' - 'A'));
  }

  return character;
}

static void copyBounded(char *dst, size_t dstSize, const char *src)
{
  size_t length = strlen(src);

  if (length >= dstSize)
  {
    length = dstSize - 1u;
  }

  memcpy(dst, src, length);
  dst[length] = '\0';
}

static bool countryMatches(const char *provided, const char *expected)
{
  if ((provided == NULL) || (strlen(provided) != 2u))
  {
    return false;
  }

  return (toUpperChar(provided[0]) == expected[0]) && (toUpperChar(provided[1]) == expected[1]);
}

static uint16_t effectiveTelegramBytes(const dsmrParserConfig *config)
{
  return (config->maxTelegramBytes != 0u) ? config->maxTelegramBytes : DSMR_MAX_TELEGRAM_BYTES;
}

static uint16_t effectiveLineBytes(const dsmrParserConfig *config)
{
  return (config->maxLineBytes != 0u) ? config->maxLineBytes : DSMR_MAX_LINE_BYTES;
}

static uint16_t effectiveMaxFields(const dsmrParserConfig *config)
{
  return (config->maxFields != 0u) ? config->maxFields : DSMR_DEFAULT_MAX_FIELDS;
}

//-- --------------------------------------------------------------------------
//-- Workspace layout
//-- --------------------------------------------------------------------------

//-- Base padding for 8-byte alignment plus the six regions in order:
//-- telegram buffer, raw frame copy, field array, M-Bus copies, label
//-- structs, label pool. The raw frame copy holds the complete accepted
//-- frame so the raw view survives the in-place decoding.
static size_t workspaceBytesNeeded(uint16_t telegramBytes, uint16_t maxFields)
{
  size_t needed = 8u;

  needed += align8(telegramBytes);
  needed += align8(telegramBytes);
  needed += align8((size_t)maxFields * sizeof(dsmrTelegramField));
  needed += align8((size_t)DSMR_MAX_MBUS_DEVICES * sizeof(dsmrMbusDevice));
  needed += align8((size_t)DSMR_MAX_FIELD_LABELS * sizeof(dsmrFieldLabel));
  needed += (size_t)DSMR_MAX_FIELD_LABELS * (DSMR_MAX_LABEL_OBIS_LEN + DSMR_MAX_LABEL_NAME_LEN);

  return needed;
}

size_t dsmrParserWorkspaceSizeNeeded(const dsmrParserConfig *config)
{
  if (config == NULL)
  {
    return 0;
  }

  return workspaceBytesNeeded(effectiveTelegramBytes(config), effectiveMaxFields(config));
}

//-- --------------------------------------------------------------------------
//-- Configuration validation
//-- --------------------------------------------------------------------------

static const dsmrProfile *resolveProfile(const dsmrParserConfig *config, dsmrParserStatus *status)
{
  if (config->countryCode == NULL)
  {
    *status = DSMR_ERR_NULL_ARGUMENT;
    return NULL;
  }

  const dsmrProfile *profile = config->customProfile;

  if (profile == NULL)
  {
    if (config->profileId == NULL)
    {
      *status = DSMR_ERR_NULL_ARGUMENT;
      return NULL;
    }

    profile = dsmrProfileLookup(config->profileId);

    if (profile == NULL)
    {
      *status = DSMR_ERR_UNKNOWN_PROFILE;
      return NULL;
    }
  }
  else
  {
    if ((profile->profileId == NULL) || (profile->countryCode == NULL) || (profile->versionObis == NULL) || (profile->acceptedVersions == NULL))
    {
      *status = DSMR_ERR_INVALID_CONFIG;
      return NULL;
    }
  }

  if (!countryMatches(config->countryCode, profile->countryCode))
  {
    *status = DSMR_ERR_COUNTRY_MISMATCH;
    return NULL;
  }

  if ((config->mbusDeviceCount > DSMR_MAX_MBUS_DEVICES) || ((config->mbusDeviceCount > 0u) && (config->mbusDevices == NULL)))
  {
    *status = DSMR_ERR_INVALID_CONFIG;
    return NULL;
  }

  for (uint8_t index = 0; index < config->mbusDeviceCount; index++)
  {
    if ((config->mbusDevices[index].channel < DSMR_MBUS_CHANNEL_MIN) || (config->mbusDevices[index].channel > DSMR_MBUS_CHANNEL_MAX))
    {
      *status = DSMR_ERR_INVALID_CONFIG;
      return NULL;
    }

    for (uint8_t other = 0; other < index; other++)
    {
      if (config->mbusDevices[other].channel == config->mbusDevices[index].channel)
      {
        *status = DSMR_ERR_INVALID_CONFIG;
        return NULL;
      }
    }
  }

  if ((config->fieldLabelCount > DSMR_MAX_FIELD_LABELS) || ((config->fieldLabelCount > 0u) && (config->fieldLabels == NULL)))
  {
    *status = DSMR_ERR_INVALID_CONFIG;
    return NULL;
  }

  for (uint16_t index = 0; index < config->fieldLabelCount; index++)
  {
    if ((config->fieldLabels[index].obis == NULL) || (config->fieldLabels[index].name == NULL))
    {
      *status = DSMR_ERR_INVALID_CONFIG;
      return NULL;
    }

    if ((strlen(config->fieldLabels[index].obis) >= DSMR_MAX_LABEL_OBIS_LEN) || (strlen(config->fieldLabels[index].name) >= DSMR_MAX_LABEL_NAME_LEN))
    {
      *status = DSMR_ERR_INVALID_CONFIG;
      return NULL;
    }
  }

  if ((config->maxTelegramBytes > DSMR_MAX_TELEGRAM_BYTES) || (config->maxLineBytes > DSMR_MAX_LINE_BYTES) || (config->maxFields > DSMR_MAX_FIELDS))
  {
    *status = DSMR_ERR_INVALID_CONFIG;
    return NULL;
  }

  return profile;
}

//-- --------------------------------------------------------------------------
//-- Configuration application
//-- --------------------------------------------------------------------------

static void applyConfigToParser(dsmrParser *parser, const dsmrParserConfig *config, const dsmrProfile *profile)
{
  uint16_t telegramBytes = effectiveTelegramBytes(config);
  uint16_t maxFields = effectiveMaxFields(config);

  size_t pad = align8((size_t)(uintptr_t)parser->workspace) - (size_t)(uintptr_t)parser->workspace;
  uint8_t *region = parser->workspace + pad;

  uint8_t *telegramBuffer = region;
  region += align8(telegramBytes);

  uint8_t *rawCopy = region;
  region += align8(telegramBytes);

  dsmrTelegramField *fieldArray = (dsmrTelegramField *)region;
  region += align8((size_t)maxFields * sizeof(dsmrTelegramField));

  dsmrMbusDevice *mbusCopy = (dsmrMbusDevice *)region;
  region += align8((size_t)DSMR_MAX_MBUS_DEVICES * sizeof(dsmrMbusDevice));

  dsmrFieldLabel *labelCopy = (dsmrFieldLabel *)region;
  region += align8((size_t)DSMR_MAX_FIELD_LABELS * sizeof(dsmrFieldLabel));

  char *pool = (char *)region;

  for (uint8_t index = 0; index < config->mbusDeviceCount; index++)
  {
    mbusCopy[index] = config->mbusDevices[index];
  }

  for (uint16_t index = 0; index < config->fieldLabelCount; index++)
  {
    char *obisSlot = pool + ((size_t)index * (DSMR_MAX_LABEL_OBIS_LEN + DSMR_MAX_LABEL_NAME_LEN));
    char *nameSlot = obisSlot + DSMR_MAX_LABEL_OBIS_LEN;

    copyBounded(obisSlot, DSMR_MAX_LABEL_OBIS_LEN, config->fieldLabels[index].obis);
    copyBounded(nameSlot, DSMR_MAX_LABEL_NAME_LEN, config->fieldLabels[index].name);

    labelCopy[index].obis = obisSlot;
    labelCopy[index].name = nameSlot;
  }

  parser->config = *config;
  parser->config.maxTelegramBytes = telegramBytes;
  parser->config.maxLineBytes = effectiveLineBytes(config);
  parser->config.maxFields = maxFields;
  parser->config.interByteTimeoutMs = (config->interByteTimeoutMs != 0u) ? config->interByteTimeoutMs : DSMR_DEFAULT_INTER_BYTE_TIMEOUT_MS;
  parser->config.frameTimeoutMs = (config->frameTimeoutMs != 0u) ? config->frameTimeoutMs : DSMR_DEFAULT_FRAME_TIMEOUT_MS;
  parser->config.mbusDevices = (config->mbusDeviceCount > 0u) ? mbusCopy : NULL;
  parser->config.mbusDeviceCount = config->mbusDeviceCount;
  parser->config.fieldLabels = (config->fieldLabelCount > 0u) ? labelCopy : NULL;
  parser->config.fieldLabelCount = config->fieldLabelCount;

  copyBounded(parser->profileId, sizeof parser->profileId, (config->profileId != NULL) ? config->profileId : profile->profileId);
  copyBounded(parser->countryCode, sizeof parser->countryCode, config->countryCode);

  parser->profile = profile;
  parser->fieldArray = fieldArray;
  parser->fieldCapacity = maxFields;
  parser->rawCopy = rawCopy;

  dsmrFramingInit(&parser->framing, telegramBuffer, telegramBytes);
  parser->hasLastByte = false;
}

//-- --------------------------------------------------------------------------
//-- Event dispatch
//-- --------------------------------------------------------------------------

static void dispatchEvent(dsmrParser *parser, const dsmrParserEvent *event, dsmrEventCallback callback, void *userData)
{
  parser->inCallback = true;
  callback(parser, event, userData);
  parser->inCallback = false;
}

static void dispatchDiagnostic(dsmrParser *parser, dsmrDiagReason reason, uint32_t position, const char *obis, dsmrEventCallback callback, void *userData)
{
  dsmrDiagnostic diagnostic;
  dsmrParserEvent event;

  diagnostic.reason = reason;
  diagnostic.bytePosition = position;
  diagnostic.obis = obis;

  event.type = DSMR_EVENT_DIAGNOSTIC;
  event.result = NULL;
  event.diagnostic = &diagnostic;

  dispatchEvent(parser, &event, callback, userData);
}

//-- --------------------------------------------------------------------------
//-- Framing and decoding pipeline
//-- --------------------------------------------------------------------------

static void handleFrame(dsmrParser *parser, uint32_t receiveTimeMs, dsmrEventCallback callback, void *userData)
{
  dsmrFraming *framing = &parser->framing;
  dsmrCrcPolicy policy = parser->profile->crcPolicy;
  dsmrCrcStatus crcStatus;

  if (framing->bodyLength < 2u)
  {
    dispatchDiagnostic(parser, DSMR_DIAG_MALFORMED_TRAILER, DSMR_POS_UNKNOWN, NULL, callback, userData);
    return;
  }

  if ((policy == DSMR_CRC_POLICY_ABSENT) || !framing->crcPresent)
  {
    crcStatus = DSMR_CRC_STATUS_ABSENT;
  }
  else
  {
    if (framing->runningCrc != framing->trailerCrc)
    {
      dispatchDiagnostic(parser, DSMR_DIAG_CRC_MISMATCH, DSMR_POS_UNKNOWN, NULL, callback, userData);
      return;
    }

    crcStatus = DSMR_CRC_STATUS_VERIFIED;
  }

  //-- Preserve the complete received frame, including every original line
  //-- ending and trailer byte, before the decoder modifies the framing
  //-- buffer in place. The view reaches the application only with the
  //-- accepted result below.
  memcpy(parser->rawCopy, framing->buffer, framing->length);

  dsmrDecodeInput input;
  input.configuredProfile = parser->profile;
  input.config = &parser->config;
  input.fieldArray = parser->fieldArray;
  input.fieldCapacity = parser->fieldCapacity;

  dsmrDecodeOutput output;
  dsmrDecodeTelegram(framing->buffer, framing->bodyLength - 1u, &input, &output);

  if (!output.accepted)
  {
    uint32_t position = (output.position <= 0xFFFFFFFEu) ? (uint32_t)output.position : DSMR_POS_UNKNOWN;
    dispatchDiagnostic(parser, output.reason, position, output.obis, callback, userData);
    return;
  }

  dsmrTelegramResult result;
  result.profileId = output.resolvedProfile->profileId;
  result.reportedVersion = output.reportedVersion;
  result.meterIdentification = output.meterIdentification;
  result.crcStatus = crcStatus;
  result.receiveTimeMs = receiveTimeMs;
  result.rawBytes = parser->rawCopy;
  result.rawLength = (uint16_t)framing->length;
  result.fields = output.fields;
  result.fieldCount = output.fieldCount;

  dsmrParserEvent event;
  event.type = DSMR_EVENT_TELEGRAM;
  event.result = &result;
  event.diagnostic = NULL;

  dispatchEvent(parser, &event, callback, userData);

  //-- M-Bus device type mismatches are reported after the accepted result.
  for (uint8_t index = 0; index < output.pendingDiagCount; index++)
  {
    dispatchDiagnostic(parser, output.pendingDiag[index].reason, output.pendingDiag[index].bytePosition, output.pendingDiag[index].obis, callback, userData);
  }
}

static void checkTimeouts(dsmrParser *parser, uint32_t nowMs, dsmrEventCallback callback, void *userData)
{
  if (!parser->framing.frameActive || !parser->hasLastByte)
  {
    return;
  }

  uint32_t sinceLastByte = (uint32_t)(nowMs - parser->lastByteTimeMs);
  uint32_t sinceFrameStart = (uint32_t)(nowMs - parser->frameStartMs);

  if (sinceLastByte > parser->config.interByteTimeoutMs)
  {
    dispatchDiagnostic(parser, DSMR_DIAG_TIMEOUT_INTER_BYTE, DSMR_POS_UNKNOWN, NULL, callback, userData);
    dsmrFramingRestart(&parser->framing);
    parser->hasLastByte = false;
  }
  else if (sinceFrameStart > parser->config.frameTimeoutMs)
  {
    dispatchDiagnostic(parser, DSMR_DIAG_TIMEOUT_FRAME, DSMR_POS_UNKNOWN, NULL, callback, userData);
    dsmrFramingRestart(&parser->framing);
    parser->hasLastByte = false;
  }
}

//-- --------------------------------------------------------------------------
//-- Public operations
//-- --------------------------------------------------------------------------

dsmrParserStatus dsmrParserInit(dsmrParser *parser, const dsmrParserConfig *config, uint8_t *workspace, size_t workspaceSize)
{
  if ((parser == NULL) || (config == NULL) || (workspace == NULL))
  {
    return DSMR_ERR_NULL_ARGUMENT;
  }

  dsmrParserStatus status;
  const dsmrProfile *profile = resolveProfile(config, &status);

  if (profile == NULL)
  {
    return status;
  }

  size_t needed = dsmrParserWorkspaceSizeNeeded(config);

  if (workspaceSize < needed)
  {
    return DSMR_ERR_INSUFFICIENT_WORKSPACE;
  }

  memset(parser, 0, sizeof *parser);
  parser->workspace = workspace;
  parser->workspaceSize = workspaceSize;

  applyConfigToParser(parser, config, profile);
  parser->initialized = true;

  return DSMR_OK;
}

dsmrParserStatus dsmrParserFeed(dsmrParser *parser, const uint8_t *bytes, size_t byteCount, uint32_t monotonicTimeMs, dsmrEventCallback callback, void *userData)
{
  if ((parser == NULL) || (callback == NULL))
  {
    return DSMR_ERR_NULL_ARGUMENT;
  }

  if ((bytes == NULL) && (byteCount > 0u))
  {
    return DSMR_ERR_NULL_ARGUMENT;
  }

  if (!parser->initialized)
  {
    return DSMR_ERR_NOT_INITIALIZED;
  }

  if (parser->inCallback)
  {
    return DSMR_ERR_REENTRANT_CALL;
  }

  checkTimeouts(parser, monotonicTimeMs, callback, userData);

  for (size_t index = 0; index < byteCount; index++)
  {
    bool wasActive = parser->framing.frameActive;

    parser->lastByteTimeMs = monotonicTimeMs;
    parser->hasLastByte = true;

    dsmrFramingOutcome outcome = dsmrFramingByte(&parser->framing, parser->profile->crcPolicy, parser->config.acceptLfLineEndings, bytes[index]);

    if (!wasActive && parser->framing.frameActive)
    {
      parser->frameStartMs = monotonicTimeMs;
    }

    switch (outcome)
    {
      case DSMR_FRAMING_FRAME_COMPLETE:
      {
        handleFrame(parser, monotonicTimeMs, callback, userData);
        dsmrFramingRestart(&parser->framing);
        parser->hasLastByte = false;
        break;
      }

      case DSMR_FRAMING_FRAME_TOO_LONG:
      {
        dispatchDiagnostic(parser, DSMR_DIAG_SIZE_LIMIT, (uint32_t)parser->framing.length, NULL, callback, userData);
        dsmrFramingRestart(&parser->framing);
        parser->hasLastByte = false;
        break;
      }

      case DSMR_FRAMING_TRAILER_MISSING_CRC:
      {
        dispatchDiagnostic(parser, DSMR_DIAG_MISSING_CRC, DSMR_POS_UNKNOWN, NULL, callback, userData);
        dsmrFramingRestart(&parser->framing);
        parser->hasLastByte = false;
        break;
      }

      case DSMR_FRAMING_TRAILER_MALFORMED:
      {
        dispatchDiagnostic(parser, DSMR_DIAG_MALFORMED_TRAILER, DSMR_POS_UNKNOWN, NULL, callback, userData);
        dsmrFramingRestart(&parser->framing);
        parser->hasLastByte = false;
        break;
      }

      case DSMR_FRAMING_NONE:
      default:
      {
        break;
      }
    }
  }

  return DSMR_OK;
}

dsmrParserStatus dsmrParserTick(dsmrParser *parser, uint32_t monotonicTimeMs, dsmrEventCallback callback, void *userData)
{
  if ((parser == NULL) || (callback == NULL))
  {
    return DSMR_ERR_NULL_ARGUMENT;
  }

  if (!parser->initialized)
  {
    return DSMR_ERR_NOT_INITIALIZED;
  }

  if (parser->inCallback)
  {
    return DSMR_ERR_REENTRANT_CALL;
  }

  checkTimeouts(parser, monotonicTimeMs, callback, userData);
  return DSMR_OK;
}

void dsmrParserReset(dsmrParser *parser)
{
  if (parser == NULL)
  {
    return;
  }

  dsmrFramingRestart(&parser->framing);
  parser->hasLastByte = false;
}

dsmrParserStatus dsmrParserApplyConfig(dsmrParser *parser, const dsmrParserConfig *config)
{
  if ((parser == NULL) || (config == NULL))
  {
    return DSMR_ERR_NULL_ARGUMENT;
  }

  if (!parser->initialized)
  {
    return DSMR_ERR_NOT_INITIALIZED;
  }

  dsmrParserStatus status;
  const dsmrProfile *profile = resolveProfile(config, &status);

  if (profile == NULL)
  {
    return status;
  }

  size_t needed = dsmrParserWorkspaceSizeNeeded(config);

  if (needed > parser->workspaceSize)
  {
    return DSMR_ERR_INSUFFICIENT_WORKSPACE;
  }

  //-- A partial telegram in progress is discarded. The new values govern the
  //-- next telegram from the initial framing state.
  applyConfigToParser(parser, config, profile);
  return DSMR_OK;
}

dsmrParserStatus dsmrParserNotifyDataLoss(dsmrParser *parser, dsmrEventCallback callback, void *userData)
{
  if ((parser == NULL) || (callback == NULL))
  {
    return DSMR_ERR_NULL_ARGUMENT;
  }

  if (!parser->initialized)
  {
    return DSMR_ERR_NOT_INITIALIZED;
  }

  if (parser->inCallback)
  {
    return DSMR_ERR_REENTRANT_CALL;
  }

  dsmrFramingRestart(&parser->framing);
  parser->hasLastByte = false;

  dispatchDiagnostic(parser, DSMR_DIAG_DATA_LOSS, DSMR_POS_UNKNOWN, NULL, callback, userData);
  return DSMR_OK;
}

//-- --------------------------------------------------------------------------
//-- Result helpers
//-- --------------------------------------------------------------------------

dsmrParserStatus dsmrResultFindFieldByObis(const dsmrTelegramResult *result, const char *obis, const dsmrTelegramField **outField)
{
  if ((result == NULL) || (obis == NULL) || (outField == NULL))
  {
    return DSMR_ERR_NULL_ARGUMENT;
  }

  for (uint16_t index = 0; index < result->fieldCount; index++)
  {
    if (strcmp(result->fields[index].obis, obis) == 0)
    {
      *outField = &result->fields[index];
      return DSMR_OK;
    }
  }

  *outField = NULL;
  return DSMR_ERR_NOT_FOUND;
}

int dsmrFormatValue(const dsmrValue *value, char *buffer, size_t bufferSize)
{
  if ((value == NULL) || (buffer == NULL) || (bufferSize == 0u))
  {
    return -1;
  }

  switch (value->kind)
  {
    case DSMR_VALUE_ABSENT:
    {
      buffer[0] = '\0';
      return 0;
    }

    case DSMR_VALUE_TEXT:
    {
      copyBounded(buffer, bufferSize, (value->text != NULL) ? value->text : "");
      return (int)strlen(buffer);
    }

    case DSMR_VALUE_NUMBER:
    default:
    {
      if (value->protocolInvalid)
      {
        copyBounded(buffer, bufferSize, "FF");
        return (int)strlen(buffer);
      }

      int64_t divisor = dsmrValuePow10(value->scale);
      int64_t magnitude = (value->mantissa < 0) ? (-value->mantissa) : value->mantissa;
      int64_t integerPart = magnitude / divisor;
      int64_t fractionPart = magnitude % divisor;

      int written;

      if (value->scale == 0u)
      {
        written = snprintf(buffer, bufferSize, "%s%lld", (value->mantissa < 0) ? "-" : "", (long long)integerPart);
      }
      else
      {
        written = snprintf(buffer, bufferSize, "%s%lld.%0*llu", (value->mantissa < 0) ? "-" : "", (long long)integerPart, (int)value->scale, (unsigned long long)fractionPart);
      }

      if (written < 0)
      {
        return -1;
      }

      return (int)strlen(buffer);
    }
  }
}

const char *dsmrStatusToString(dsmrParserStatus status)
{
  switch (status)
  {
    case DSMR_OK: return "OK";
    case DSMR_ERR_NULL_ARGUMENT: return "null argument";
    case DSMR_ERR_UNKNOWN_PROFILE: return "unknown profile";
    case DSMR_ERR_COUNTRY_MISMATCH: return "country mismatch";
    case DSMR_ERR_INSUFFICIENT_WORKSPACE: return "insufficient workspace";
    case DSMR_ERR_INVALID_CONFIG: return "invalid configuration";
    case DSMR_ERR_NOT_INITIALIZED: return "not initialized";
    case DSMR_ERR_REENTRANT_CALL: return "reentrant call";
    case DSMR_ERR_NOT_FOUND: return "not found";
    default: return "unknown status";
  }
}

const char *dsmrDiagReasonToString(dsmrDiagReason reason)
{
  switch (reason)
  {
    case DSMR_DIAG_NONE: return "none";
    case DSMR_DIAG_MISSING_CRC: return "missing CRC";
    case DSMR_DIAG_MALFORMED_TRAILER: return "malformed trailer";
    case DSMR_DIAG_CRC_MISMATCH: return "CRC mismatch";
    case DSMR_DIAG_MISSING_VERSION: return "missing version";
    case DSMR_DIAG_INVALID_VERSION: return "invalid version";
    case DSMR_DIAG_SYNTAX_ERROR: return "syntax error";
    case DSMR_DIAG_SIZE_LIMIT: return "size limit";
    case DSMR_DIAG_TIMEOUT_INTER_BYTE: return "inter-byte timeout";
    case DSMR_DIAG_TIMEOUT_FRAME: return "frame timeout";
    case DSMR_DIAG_DATA_LOSS: return "data loss";
    case DSMR_DIAG_DUPLICATE_FIELD: return "duplicate field";
    case DSMR_DIAG_MBUS_TYPE_MISMATCH: return "M-Bus device type mismatch";
    case DSMR_DIAG_VALUE_OVERFLOW: return "value overflow";
    case DSMR_DIAG_INVALID_TIMESTAMP: return "invalid timestamp";
    default: return "unknown reason";
  }
}
