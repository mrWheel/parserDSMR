//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: src/dsmr_decode.c - OBIS line decoding of a stored candidate
//-- Date: 2026-11-16 · Build: 4
//-- ==========================================================================

#include "dsmr_decode.h"

#include "dsmr_profile.h"
#include "dsmr_value.h"

#include <string.h>

#define DECODE_MAX_GROUPS 24

typedef struct
{
  char *buffer;
  size_t bodyLength;
  const dsmrDecodeInput *input;
  dsmrDecodeOutput *output;
  const dsmrProfile *profile;
} DecodeContext;

//-- Appends a non-rejecting diagnostic, for example an M-Bus device type
//-- mismatch on an otherwise accepted telegram.
static void addPendingDiagnostic(dsmrDecodeOutput *output, dsmrDiagReason reason, const char *obis, size_t position)
{
  if (output->pendingDiagCount < DSMR_DECODE_MAX_PENDING_DIAG)
  {
    dsmrDiagnostic *diag = &output->pendingDiag[output->pendingDiagCount++];
    diag->reason = reason;
    diag->obis = obis;
    diag->bytePosition = (position <= 0xFFFFFFFFu) ? (uint32_t)position : DSMR_POS_UNKNOWN;
  }
}

static void rejectWith(DecodeContext *ctx, dsmrDiagReason reason, const char *obis, size_t position)
{
  ctx->output->accepted = false;
  ctx->output->reason = reason;
  ctx->output->obis = obis;
  ctx->output->position = position;
}

//-- Returns true when the OBIS code ends with ":suffix".
static bool obisEndsWith(const char *obis, const char *suffix)
{
  const char *colon = strrchr(obis, ':');

  if (colon == NULL)
  {
    return false;
  }

  return (strcmp(colon + 1, suffix) == 0);
}

//-- Extracts the M-Bus channel from an OBIS code of the form "0-n:x.y.z".
//-- Returns 0 when the code is not channel-based.
static uint8_t channelFromObis(const char *obis)
{
  if ((obis[0] == '0') && (obis[1] == '-') && (obis[2] >= '1') && (obis[2] <= '9') && (obis[3] == ':'))
  {
    uint8_t channel = (uint8_t)(obis[2] - '0');

    if ((channel >= DSMR_MBUS_CHANNEL_MIN) && (channel <= DSMR_MBUS_CHANNEL_MAX))
    {
      return channel;
    }
  }

  return 0;
}

//-- Finds the declared device for a channel in the configuration table.
static const dsmrMbusDevice *findDeclaredDevice(const dsmrParserConfig *config, uint8_t channel)
{
  if ((config == NULL) || (config->mbusDevices == NULL))
  {
    return NULL;
  }

  for (uint8_t index = 0; index < config->mbusDeviceCount; index++)
  {
    if (config->mbusDevices[index].channel == channel)
    {
      return &config->mbusDevices[index];
    }
  }

  return NULL;
}

//-- True when a published 0-n:24.1.0 value is compatible with a declared
//-- device type. Two numberings are in use on the supported P1 ports. The
//-- Dutch and Belgian DSMR/eMUCS numbering is 1=gas, 2=water, 3=heat,
//-- 4=cooling, 5=electricity. The European P1 OBIS overview documents the
//-- EN 13757-3 M-Bus numbering for the same object (3=gas, 4=heat, 7=water
//-- in assets/Europese_P1_OBIS_Codes_Overzicht.pdf) and the Luxembourg
//-- E-Meter P1 Specification 1.1.3 example telegram publishes 3 for a gas
//-- channel and 7 for a water channel. Because the numberings overlap, a
//-- value that any numbering explains for the declared type is accepted;
//-- only a value that no numbering explains yields the mismatch
//-- diagnostic. Ambiguous values therefore never produce a false alarm.
static bool publishedValueMatchesDeviceType(int64_t published, dsmrMbusDeviceType declared)
{
  switch (declared)
  {
    case DSMR_MBUS_DEVICE_GAS:
    {
      return (published == 1) || (published == 3);
    }

    case DSMR_MBUS_DEVICE_WATER:
    {
      return (published == 2) || (published == 7);
    }

    case DSMR_MBUS_DEVICE_HEAT:
    {
      return (published == 3) || (published == 4);
    }

    case DSMR_MBUS_DEVICE_COOLING:
    {
      return (published == 4);
    }

    case DSMR_MBUS_DEVICE_ELECTRICITY:
    {
      return (published == 5);
    }

    default:
    {
      return false;
    }
  }
}

//-- Resolves the application label for an OBIS code, or NULL.
static const char *resolveLabel(const dsmrParserConfig *config, const char *obis)
{
  if ((config == NULL) || (config->fieldLabels == NULL))
  {
    return NULL;
  }

  for (uint16_t index = 0; index < config->fieldLabelCount; index++)
  {
    if (strcmp(config->fieldLabels[index].obis, obis) == 0)
    {
      return config->fieldLabels[index].name;
    }
  }

  return NULL;
}

//-- Reads one line starting at *position. Returns 1 on a line, 0 at the end
//-- of the body and -1 on a malformed line boundary. Strict profiles require
//-- CR LF boundaries; LF compatibility mode also accepts a bare LF. The
//-- decoder writes terminators into the buffer so views become strings.
static int readLine(DecodeContext *ctx, size_t *position, char **line, size_t *lineLength)
{
  if (*position >= ctx->bodyLength)
  {
    return 0;
  }

  size_t index = *position;

  while ((index < ctx->bodyLength) && (ctx->buffer[index] != '\n'))
  {
    index++;
  }

  if (index >= ctx->bodyLength)
  {
    return -1;
  }

  bool hadCr = (index > *position) && (ctx->buffer[index - 1] == '\r');

  if (!hadCr && !ctx->input->config->acceptLfLineEndings)
  {
    return -1;
  }

  size_t contentEnd = hadCr ? (index - 1) : index;

  ctx->buffer[index] = '\0';

  if (hadCr)
  {
    ctx->buffer[index - 1] = '\0';
  }

  *line = ctx->buffer + *position;
  *lineLength = contentEnd - *position;
  *position = index + 1;
  return 1;
}

//-- Splits a data line into its OBIS code and its ordered parenthesized
//-- groups. Returns the number of groups, or -1 on a syntax error. On a
//-- syntax error the OBIS code is still reported when the key itself is
//-- well formed, so the caller can apply the unknown-field policy to a
//-- malformed line of a code the profile does not define. An unknown line
//-- shall never reject the telegram by itself.
static int splitLine(char *line, const char **obis, char **groups, uint8_t groupCapacity)
{
  size_t index = 0;

  while (line[index] != '\0')
  {
    char character = line[index];
    bool allowed = ((character >= '0') && (character <= '9')) || (character == '-') || (character == ':') || (character == '.');

    if (!allowed)
    {
      break;
    }

    index++;
  }

  if (index == 0u)
  {
    return -1;
  }

  //-- The character at index is simultaneously the terminator of the OBIS
  //-- code and, when it is '(', the opening delimiter of the first value
  //-- group. It is consumed here; the groups loop below expects '(' only
  //-- between groups.
  char stoppedAt = line[index];
  line[index] = '\0';
  *obis = line;

  if (stoppedAt != '(')
  {
    return -1;
  }

  index++;

  uint8_t groupCount = 0;

  for (;;)
  {
    size_t valueStart = index;

    while ((line[index] != '\0') && (line[index] != ')'))
    {
      index++;
    }

    if (line[index] != ')')
    {
      return -1;
    }

    line[index] = '\0';

    if (groupCount >= groupCapacity)
    {
      return -1;
    }

    groups[groupCount++] = line + valueStart;
    index++;

    if (line[index] == '\0')
    {
      break;
    }

    if (line[index] != '(')
    {
      return -1;
    }

    index++;
  }

  return (int)groupCount;
}

//-- Adds a field to the ordered result array. Returns false when the array
//-- is full.
static bool addField(DecodeContext *ctx, const char *obis, uint8_t channel, const dsmrValue *value, const char *unit, const dsmrTimestamp *timestamp, dsmrFieldClass classification)
{
  if (ctx->output->fieldCount >= ctx->input->fieldCapacity)
  {
    return false;
  }

  dsmrTelegramField *field = &ctx->output->fields[ctx->output->fieldCount++];

  memset(field, 0, sizeof *field);
  field->obis = obis;
  field->mbusChannel = channel;
  field->name = resolveLabel(ctx->input->config, obis);
  field->value = *value;
  field->unit = unit;
  field->classification = classification;

  if (timestamp != NULL)
  {
    field->timestamp = *timestamp;
    field->hasTimestamp = true;
  }

  return true;
}

//-- True when the same OBIS code and channel are already present and the
//-- profile does not permit repetition.
static bool isDuplicate(DecodeContext *ctx, const char *obis, uint8_t channel, bool repeatable)
{
  if (repeatable)
  {
    return false;
  }

  for (uint16_t index = 0; index < ctx->output->fieldCount; index++)
  {
    if ((ctx->output->fields[index].mbusChannel == channel) && (strcmp(ctx->output->fields[index].obis, obis) == 0))
    {
      return true;
    }
  }

  return false;
}

//-- Applies the unknown-field policy to a line whose OBIS code the active
//-- profile does not define. Unknown fields are never a fault: they are
//-- skipped or kept as bounded raw text and classified as unknown.
static bool addUnknownField(DecodeContext *ctx, const char *obis, uint8_t channel, const char *text, size_t linePosition)
{
  if (ctx->input->config->unknownFieldPolicy == DSMR_UNKNOWN_FIELD_SKIP)
  {
    return true;
  }

  dsmrValue value;
  dsmrValueSetText(&value, (text != NULL) ? text : "");

  if (!addField(ctx, obis, channel, &value, NULL, NULL, DSMR_FIELD_CLASS_UNKNOWN))
  {
    rejectWith(ctx, DSMR_DIAG_SIZE_LIMIT, obis, linePosition);
    return false;
  }

  return true;
}

//-- Decodes one data line against the resolved profile.
static bool decodeLine(DecodeContext *ctx, char *line, size_t lineLength, size_t linePosition)
{
  const char *obis = NULL;
  char *groups[DECODE_MAX_GROUPS];
  int groupCount = splitLine(line, &obis, groups, DECODE_MAX_GROUPS);

  if (groupCount < 0)
  {
    //-- A well-formed OBIS key the profile does not define never rejects the
    //-- telegram; it follows the unknown-field policy. Only a known code or
    //-- a key that is not an OBIS code at all yields a syntax error.
    if ((obis != NULL) && (dsmrProfileFindField(ctx->profile, obis) == NULL))
    {
      return addUnknownField(ctx, obis, channelFromObis(obis), NULL, linePosition);
    }

    rejectWith(ctx, DSMR_DIAG_SYNTAX_ERROR, obis, linePosition);
    return false;
  }

  if (lineLength > ctx->input->config->maxLineBytes)
  {
    rejectWith(ctx, DSMR_DIAG_SIZE_LIMIT, obis, linePosition);
    return false;
  }

  uint8_t channel = channelFromObis(obis);
  const dsmrFieldDefinition *definition = dsmrProfileFindField(ctx->profile, obis);

  //-- A channel published by the meter but absent from the declared table
  //-- follows the unknown-field policy.
  if ((channel != 0u) && (findDeclaredDevice(ctx->input->config, channel) == NULL))
  {
    definition = NULL;
  }

  if (definition == NULL)
  {
    return addUnknownField(ctx, obis, channel, (groupCount > 0) ? groups[0] : "", linePosition);
  }

  if (isDuplicate(ctx, obis, channel, (definition->flags & DSMR_FIELD_FLAG_REPEATABLE) != 0u))
  {
    rejectWith(ctx, DSMR_DIAG_DUPLICATE_FIELD, obis, linePosition);
    return false;
  }

  bool optionalMbusTimestamp = (definition->type == DSMR_FIELD_TYPE_DECIMAL) &&
                               (groupCount == 2) &&
                               obisEndsWith(obis, "24.2.1");

  if ((definition->groupCount != 0u) && ((uint8_t)groupCount != definition->groupCount) && !optionalMbusTimestamp)
  {
    rejectWith(ctx, DSMR_DIAG_SYNTAX_ERROR, obis, linePosition);
    return false;
  }

  if ((uint8_t)groupCount > DECODE_MAX_GROUPS)
  {
    rejectWith(ctx, DSMR_DIAG_SIZE_LIMIT, obis, linePosition);
    return false;
  }

  dsmrValue value;
  dsmrValueSetAbsent(&value);
  dsmrTimestamp timestamp;
  memset(&timestamp, 0, sizeof timestamp);
  bool hasTimestamp = false;

  switch (definition->type)
  {
    case DSMR_FIELD_TYPE_DECIMAL:
    {
      //-- Data lines carry the unit as a *suffix inside the value group.
      //-- The unit itself comes from the profile field table, so the
      //-- suffix is stripped before the decimal conversion.
      char *valueText = optionalMbusTimestamp ? groups[1] : groups[0];
      char *timestampText = (groupCount == 2) ? (optionalMbusTimestamp ? groups[0] : groups[1]) : NULL;
      char *unitSuffix = strchr(valueText, '*');

      if (unitSuffix != NULL)
      {
        *unitSuffix = '\0';
      }

      int parseResult = dsmrValueParseDecimal(valueText, definition->maxScale, &value);

      if (parseResult < 0)
      {
        rejectWith(ctx, DSMR_DIAG_VALUE_OVERFLOW, obis, linePosition);
        return false;
      }

      if (parseResult == 0)
      {
        rejectWith(ctx, DSMR_DIAG_SYNTAX_ERROR, obis, linePosition);
        return false;
      }

      //-- M-Bus readings put the timestamp before the value; other decimal
      //-- fields with two groups put the timestamp after the value.
      if (timestampText != NULL)
      {
        dsmrValue ignored;

        if (dsmrTimestampParse(timestampText, &ignored, &timestamp) != true)
        {
          rejectWith(ctx, DSMR_DIAG_INVALID_TIMESTAMP, obis, linePosition);
          return false;
        }

        hasTimestamp = true;
      }

      break;
    }

    case DSMR_FIELD_TYPE_INTEGER:
    {
      int parseResult = dsmrValueParseInteger(groups[0], &value);

      if (parseResult < 0)
      {
        rejectWith(ctx, DSMR_DIAG_VALUE_OVERFLOW, obis, linePosition);
        return false;
      }

      if (parseResult == 0)
      {
        rejectWith(ctx, DSMR_DIAG_SYNTAX_ERROR, obis, linePosition);
        return false;
      }

      //-- A published 0-n:24.1.0 value that contradicts the declared device
      //-- type yields a mismatch diagnostic on an accepted telegram.
      if ((channel != 0u) && obisEndsWith(obis, "24.1.0") && !value.protocolInvalid && (value.kind == DSMR_VALUE_NUMBER))
      {
        const dsmrMbusDevice *declared = findDeclaredDevice(ctx->input->config, channel);

        if (declared != NULL)
        {
          if (!publishedValueMatchesDeviceType(value.mantissa, declared->deviceType))
          {
            addPendingDiagnostic(ctx->output, DSMR_DIAG_MBUS_TYPE_MISMATCH, obis, linePosition);
          }
        }
      }

      break;
    }

    case DSMR_FIELD_TYPE_TEXT:
    {
      dsmrValueSetText(&value, groups[0]);
      break;
    }

    case DSMR_FIELD_TYPE_TIMESTAMP:
    {
      if (dsmrTimestampParse(groups[0], &value, &timestamp) != true)
      {
        rejectWith(ctx, DSMR_DIAG_INVALID_TIMESTAMP, obis, linePosition);
        return false;
      }

      hasTimestamp = true;
      break;
    }

    case DSMR_FIELD_TYPE_LOG:
    {
      dsmrValue countValue;
      int parseResult = dsmrValueParseInteger(groups[0], &countValue);

      if ((parseResult != 1) || (countValue.kind != DSMR_VALUE_NUMBER) || countValue.protocolInvalid || (countValue.mantissa < 0))
      {
        rejectWith(ctx, DSMR_DIAG_SYNTAX_ERROR, obis, linePosition);
        return false;
      }

      size_t expected = (size_t)definition->logPrefixGroups + ((size_t)countValue.mantissa * (size_t)definition->logEntryGroups);

      bool validGroupCount = ((size_t)groupCount == expected);

      //-- Keep accepting the existing fixture layout of two prefix groups
      //-- and four groups per record alongside the DSMR 5.0 record layout.
      if (!validGroupCount && (strcmp(obis, "1-0:99.97.0") == 0) && (countValue.mantissa <= (int64_t)DECODE_MAX_GROUPS))
      {
        size_t legacyExpected = 2u + ((size_t)countValue.mantissa * 4u);
        validGroupCount = ((size_t)groupCount == legacyExpected);
      }

      if (!validGroupCount)
      {
        rejectWith(ctx, DSMR_DIAG_SYNTAX_ERROR, obis, linePosition);
        return false;
      }

      value = countValue;
      break;
    }

    default:
    {
      rejectWith(ctx, DSMR_DIAG_SYNTAX_ERROR, obis, linePosition);
      return false;
    }
  }

  if (!addField(ctx, obis, channel, &value, definition->unit, hasTimestamp ? &timestamp : NULL, DSMR_FIELD_CLASS_KNOWN))
  {
    rejectWith(ctx, DSMR_DIAG_SIZE_LIMIT, obis, linePosition);
    return false;
  }

  return true;
}

//-- Scans the candidate for the version field without altering the buffer.
//-- Copies the version text so the buffer stays intact for decoding. Returns
//-- false when the field is absent or malformed.
static bool scanVersion(const DecodeContext *ctx, char *out, size_t outSize)
{
  const char *buffer = ctx->buffer;
  size_t versionLength = strlen(ctx->profile->versionObis);
  size_t position = 0;

  out[0] = '\0';

  while (position < ctx->bodyLength)
  {
    size_t lineEnd = position;

    while ((lineEnd < ctx->bodyLength) && (buffer[lineEnd] != '\n'))
    {
      lineEnd++;
    }

    if (((lineEnd - position) > versionLength) && (strncmp(buffer + position, ctx->profile->versionObis, versionLength) == 0) && (buffer[position + versionLength] == '('))
    {
      size_t index = position + versionLength + 1;
      size_t written = 0;

      while ((index < lineEnd) && (buffer[index] != ')'))
      {
        if ((written + 1u) >= outSize)
        {
          return false;
        }

        out[written++] = buffer[index];
        index++;
      }

      if (index < lineEnd)
      {
        out[written] = '\0';
        return true;
      }
    }

    position = lineEnd + 1;
  }

  return false;
}

void dsmrDecodeTelegram(uint8_t *buffer, size_t bodyLength, const dsmrDecodeInput *input, dsmrDecodeOutput *output)
{
  DecodeContext ctx;

  memset(output, 0, sizeof *output);
  output->accepted = false;
  output->reason = DSMR_DIAG_NONE;
  output->position = 0;
  output->obis = NULL;
  output->resolvedProfile = NULL;
  output->reportedVersion = NULL;
  output->meterIdentification = NULL;
  output->fields = input->fieldArray;
  output->fieldCount = 0;
  output->pendingDiagCount = 0;

  ctx.buffer = (char *)buffer;
  ctx.bodyLength = bodyLength;
  ctx.input = input;
  ctx.output = output;
  ctx.profile = input->configuredProfile;

  if ((bodyLength < 2u) || (buffer[0] != '/'))
  {
    rejectWith(&ctx, DSMR_DIAG_SYNTAX_ERROR, NULL, 0);
    return;
  }

  size_t position = 0;
  char *line = NULL;
  size_t lineLength = 0;

  //-- The reported protocol version governs version validation. The scan
  //-- runs while the buffer is still intact; readLine replaces CR LF bytes
  //-- with terminators. Resolving the version before the field syntax check
  //-- makes the field table of the correct revision govern decoding.
  char versionText[16];

  if (!scanVersion(&ctx, versionText, sizeof versionText))
  {
    rejectWith(&ctx, DSMR_DIAG_MISSING_VERSION, ctx.profile->versionObis, 0);
    return;
  }

  const dsmrProfile *resolved = dsmrProfileResolveForVersion(input->configuredProfile, versionText, input->config->acceptUnknownMinorVersion);

  if (resolved == NULL)
  {
    rejectWith(&ctx, DSMR_DIAG_INVALID_VERSION, ctx.profile->versionObis, 0);
    return;
  }

  ctx.profile = resolved;
  output->resolvedProfile = resolved;

  //-- The header is preserved as meter identification.
  int lineResult = readLine(&ctx, &position, &line, &lineLength);

  if (lineResult != 1)
  {
    rejectWith(&ctx, DSMR_DIAG_SYNTAX_ERROR, NULL, 0);
    return;
  }

  output->meterIdentification = ctx.buffer + 1;

  //-- The identification line is followed by a blank line.
  lineResult = readLine(&ctx, &position, &line, &lineLength);

  if ((lineResult != 1) || (lineLength != 0u))
  {
    rejectWith(&ctx, DSMR_DIAG_SYNTAX_ERROR, NULL, position);
    return;
  }

  while (position < ctx.bodyLength)
  {
    size_t linePosition = position;
    lineResult = readLine(&ctx, &position, &line, &lineLength);

    if (lineResult == -1)
    {
      rejectWith(&ctx, DSMR_DIAG_SYNTAX_ERROR, NULL, linePosition);
      return;
    }

    if (lineResult == 0)
    {
      break;
    }

    if (lineLength == 0u)
    {
      rejectWith(&ctx, DSMR_DIAG_SYNTAX_ERROR, NULL, linePosition);
      return;
    }

    if (!decodeLine(&ctx, line, lineLength, linePosition))
    {
      return;
    }
  }

  //-- The reported version is taken from the decoded text field so leading
  //-- zeroes are retained and the view points into the telegram buffer. The
  //-- profile shall define its version object in the field table.
  for (uint16_t index = 0; index < output->fieldCount; index++)
  {
    if (strcmp(output->fields[index].obis, resolved->versionObis) == 0)
    {
      output->reportedVersion = output->fields[index].value.text;
      break;
    }
  }

  if (output->reportedVersion == NULL)
  {
    rejectWith(&ctx, DSMR_DIAG_MISSING_VERSION, resolved->versionObis, 0);
    return;
  }

  output->accepted = true;
}
