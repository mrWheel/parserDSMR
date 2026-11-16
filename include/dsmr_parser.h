//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: include/dsmr_parser.h - public C API of the portable core
//-- Date: 2026-11-16 · Build: 5
//--
//-- Portable ISO C11 core. This header uses standard C headers only and has
//-- no ESP-IDF dependency. The same sources compile for the ESP-IDF component
//-- and for the later Arduino library.
//--
//-- Ownership model:
//--   The application owns the dsmrParser instance and the workspace array.
//--   The parser performs no dynamic allocation during reception or decoding.
//--   The application owns every string the dsmrParserConfig references. The
//--   parser copies the values it needs during dsmrParserInit,
//--   dsmrParserApplyConfig and dsmrParserSetFieldLabel and does not retain
//--   the caller's pointers.
//--   Views handed out in a dsmrTelegramResult or dsmrDiagnostic stay valid
//--   only for the duration of the event callback. Copy what you keep.
//--   One instance is owned by one task. Calling back into the parser from
//--   its own event callback is not allowed and yields DSMR_ERR_REENTRANT_CALL
//--   for feed, tick and data-loss notifications.
//-- ==========================================================================

#ifndef DSMR_PARSER_H
#define DSMR_PARSER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//-- --------------------------------------------------------------------------
//-- Documented limits
//-- --------------------------------------------------------------------------

//-- Initial telegram buffer allowance for the 115200 baud, 1 Hz profiles.
//-- Implementation setting, subject to validation against the object tables.
#define DSMR_MAX_TELEGRAM_BYTES 12288

//-- Accommodates the OBIS prefix, the delimiters and the 2048 hex characters
//-- of a maximum length Dutch text message.
#define DSMR_MAX_LINE_BYTES 2200

//-- Upper bound for the ordered received-field array.
#define DSMR_MAX_FIELDS 256
#define DSMR_DEFAULT_MAX_FIELDS 128

//-- M-Bus channels published on the P1 port are 0-1 through 0-4.
#define DSMR_MBUS_CHANNEL_MIN 1
#define DSMR_MBUS_CHANNEL_MAX 4
#define DSMR_MAX_MBUS_DEVICES DSMR_MBUS_CHANNEL_MAX

//-- Bounds for the application label table.
#define DSMR_MAX_FIELD_LABELS 64
#define DSMR_MAX_LABEL_OBIS_LEN 16
#define DSMR_MAX_LABEL_NAME_LEN 32

//-- Bounds for copied configuration strings.
#define DSMR_MAX_PROFILE_ID_LEN 32

//-- Default timeouts in milliseconds. The transfer interval of a profile is
//-- a separate, informational property and is independent of both timeouts.
#define DSMR_DEFAULT_INTER_BYTE_TIMEOUT_MS 1000
#define DSMR_DEFAULT_FRAME_TIMEOUT_MS 10000

//-- Byte position reported when the parser cannot attribute a diagnostic.
#define DSMR_POS_UNKNOWN 0xFFFFFFFFu

//-- Conservative workspace size for the default limits. Use
//-- dsmrParserWorkspaceSizeNeeded() for the exact size of a chosen
//-- configuration.
#define DSMR_DEFAULT_WORKSPACE_BYTES \
  (DSMR_MAX_TELEGRAM_BYTES + 8u + \
   DSMR_MAX_TELEGRAM_BYTES + 8u + \
   DSMR_DEFAULT_MAX_FIELDS * (uint32_t)sizeof(dsmrTelegramField) + 8u + \
   DSMR_MAX_MBUS_DEVICES * (uint32_t)sizeof(dsmrMbusDevice) + 8u + \
   DSMR_MAX_FIELD_LABELS * (uint32_t)sizeof(dsmrFieldLabel) + 8u + \
   DSMR_MAX_FIELD_LABELS * (DSMR_MAX_LABEL_OBIS_LEN + DSMR_MAX_LABEL_NAME_LEN) + 8u)

//-- --------------------------------------------------------------------------
//-- Result and error codes
//-- --------------------------------------------------------------------------

typedef enum
{
  DSMR_OK = 0,
  DSMR_ERR_NULL_ARGUMENT,
  DSMR_ERR_UNKNOWN_PROFILE,
  DSMR_ERR_COUNTRY_MISMATCH,
  DSMR_ERR_INSUFFICIENT_WORKSPACE,
  DSMR_ERR_INVALID_CONFIG,
  DSMR_ERR_NOT_INITIALIZED,
  DSMR_ERR_REENTRANT_CALL,
  DSMR_ERR_NOT_FOUND,
} dsmrParserStatus;

typedef enum
{
  DSMR_CRC_STATUS_VERIFIED = 0,
  DSMR_CRC_STATUS_ABSENT,
} dsmrCrcStatus;

typedef enum
{
  DSMR_DIAG_NONE = 0,
  DSMR_DIAG_MISSING_CRC,
  DSMR_DIAG_MALFORMED_TRAILER,
  DSMR_DIAG_CRC_MISMATCH,
  DSMR_DIAG_MISSING_VERSION,
  DSMR_DIAG_INVALID_VERSION,
  DSMR_DIAG_SYNTAX_ERROR,
  DSMR_DIAG_SIZE_LIMIT,
  DSMR_DIAG_TIMEOUT_INTER_BYTE,
  DSMR_DIAG_TIMEOUT_FRAME,
  DSMR_DIAG_DATA_LOSS,
  DSMR_DIAG_DUPLICATE_FIELD,
  DSMR_DIAG_MBUS_TYPE_MISMATCH,
  DSMR_DIAG_VALUE_OVERFLOW,
  DSMR_DIAG_INVALID_TIMESTAMP,
} dsmrDiagReason;

//-- --------------------------------------------------------------------------
//-- Values and timestamps
//-- --------------------------------------------------------------------------

typedef enum
{
  DSMR_VALUE_ABSENT = 0,
  DSMR_VALUE_NUMBER,
  DSMR_VALUE_TEXT,
} dsmrValueKind;

typedef struct
{
  dsmrValueKind kind;
  bool protocolInvalid;
  int64_t mantissa;
  uint8_t scale;
  const char *text;
} dsmrValue;

typedef struct
{
  bool valid;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  bool hasOffset;
  int16_t offsetMinutes;
  char dst;
} dsmrTimestamp;

//-- --------------------------------------------------------------------------
//-- M-Bus device declaration and application labels
//-- --------------------------------------------------------------------------

typedef enum
{
  DSMR_MBUS_DEVICE_GAS = 0,
  DSMR_MBUS_DEVICE_WATER,
  DSMR_MBUS_DEVICE_HEAT,
  DSMR_MBUS_DEVICE_COOLING,
  DSMR_MBUS_DEVICE_ELECTRICITY,
  DSMR_MBUS_DEVICE_OTHER,
} dsmrMbusDeviceType;

typedef struct
{
  uint8_t channel;
  dsmrMbusDeviceType deviceType;
} dsmrMbusDevice;

//-- Application override for one OBIS code. The table takes precedence over
//-- the built-in default display names carried by the profile field tables:
//-- an application only registers the few codes it wants to rename. A
//-- resolved field with neither an application label nor a built-in default
//-- keeps name == NULL and remains reachable by its OBIS code.
typedef struct
{
  const char *obis;
  const char *name;
} dsmrFieldLabel;

//-- --------------------------------------------------------------------------
//-- Profile and field definitions
//-- --------------------------------------------------------------------------

typedef enum
{
  DSMR_CRC_POLICY_REQUIRED = 0,
  DSMR_CRC_POLICY_ABSENT,
  DSMR_CRC_POLICY_OPTIONAL,
} dsmrCrcPolicy;

typedef enum
{
  DSMR_UNKNOWN_FIELD_SKIP = 0,
  DSMR_UNKNOWN_FIELD_RAW,
} dsmrUnknownFieldPolicy;

typedef enum
{
  DSMR_FIELD_TYPE_DECIMAL = 0,
  DSMR_FIELD_TYPE_INTEGER,
  DSMR_FIELD_TYPE_TEXT,
  DSMR_FIELD_TYPE_TIMESTAMP,
  DSMR_FIELD_TYPE_LOG,
} dsmrFieldType;

//-- One row of a profile field table. The field table is the single place
//-- defining syntax, datatype and unit for an OBIS code.
//--   obis             exact code, or '?' as a single-digit channel wildcard
//--                    such as "0-?:24.1.0" for M-Bus channels 1 through 4.
//--   groupCount       number of parenthesized value groups the line must
//--                    carry. 0 accepts any number. A DECIMAL with
//--                    groupCount 2 carries the value and a timestamp, as
//--                    used by the Belgian maximum demand field.
//--   logPrefixGroups  LOG only: groups before the repeating records,
//--                    including the record count group.
//--   logEntryGroups   LOG only: groups per repeating record.
//--   name             built-in default display name used when the
//--                    application registers no label for the code. The
//--                    application label table always wins; NULL yields no
//--                    default (application tables may leave it unset).
//--                    Kept last so positional initializers stay valid.
typedef struct
{
  const char *obis;
  dsmrFieldType type;
  const char *unit;
  uint8_t maxScale;
  uint8_t groupCount;
  uint8_t flags;
  uint8_t logPrefixGroups;
  uint8_t logEntryGroups;
  const char *name;
} dsmrFieldDefinition;

#define DSMR_FIELD_FLAG_REPEATABLE 0x01u

//-- A named run of field definitions. A profile is assembled from one or
//-- more blocks so shared object groups are written once.
typedef struct
{
  const dsmrFieldDefinition *entries;
  uint16_t count;
} dsmrFieldBlock;

typedef struct dsmrProfile
{
  const char *profileId;
  const char *countryCode;
  const char *versionObis;
  const char *const *acceptedVersions;
  dsmrCrcPolicy crcPolicy;
  uint32_t uartBaudRate;
  uint8_t uartDataBits;
  uint8_t uartParity;
  uint8_t uartStopBits;
  uint16_t telegramIntervalMs;
  const dsmrFieldBlock *fieldBlocks;
  uint16_t fieldBlockCount;
  const struct dsmrProfile *const *autoProfiles;
} dsmrProfile;

//-- Resolves a built-in profile by identifier, or NULL when unknown.
const dsmrProfile *dsmrProfileLookup(const char *profileId);

//-- NULL-terminated list of built-in profile identifiers.
const char *const *dsmrProfileListIds(void);

//-- --------------------------------------------------------------------------
//-- Configuration
//-- --------------------------------------------------------------------------

typedef struct
{
  const char *countryCode;
  const char *profileId;
  const dsmrProfile *customProfile;
  const dsmrMbusDevice *mbusDevices;
  uint8_t mbusDeviceCount;
  const dsmrFieldLabel *fieldLabels;
  uint16_t fieldLabelCount;
  uint16_t maxTelegramBytes;
  uint16_t maxLineBytes;
  uint16_t maxFields;
  uint32_t interByteTimeoutMs;
  uint32_t frameTimeoutMs;
  bool acceptUnknownMinorVersion;
  bool acceptLfLineEndings;
  dsmrUnknownFieldPolicy unknownFieldPolicy;
} dsmrParserConfig;

//-- --------------------------------------------------------------------------
//-- Telegram result and events
//-- --------------------------------------------------------------------------

//-- Classification of a received field. A field whose OBIS code is defined
//-- by the active profile is KNOWN. A field that the profile does not
//-- define is UNKNOWN: it is never a fault. The overview of European P1
//-- OBIS usage explicitly requires a universal parser to ignore unknown
//-- lines or keep them as raw data without rejecting the telegram, because
//-- manufacturers add extra diagnostic objects in the 96.x.x range.
typedef enum
{
  DSMR_FIELD_CLASS_KNOWN = 0,
  DSMR_FIELD_CLASS_UNKNOWN,
} dsmrFieldClass;

//-- One received field. name is the application label when one is
//-- registered for the code, else the built-in default display name from
//-- the profile field table, else NULL.
typedef struct
{
  const char *obis;
  uint8_t mbusChannel;
  const char *name;
  dsmrValue value;
  const char *unit;
  dsmrTimestamp timestamp;
  bool hasTimestamp;
  dsmrFieldClass classification;
} dsmrTelegramField;

typedef struct
{
  const char *profileId;
  const char *reportedVersion;
  const char *meterIdentification;
  dsmrCrcStatus crcStatus;
  uint32_t receiveTimeMs;

  //-- Read-only view on the complete accepted frame: header, body, the
  //-- trailer bytes and every original line ending, exactly as received.
  //-- The parser preserves this bounded copy before decoding modifies the
  //-- framing buffer in place. The view is exposed only with an accepted
  //-- telegram and stays valid only for the duration of the event callback.
  const uint8_t *rawBytes;
  uint16_t rawLength;

  const dsmrTelegramField *fields;
  uint16_t fieldCount;
} dsmrTelegramResult;

typedef struct
{
  dsmrDiagReason reason;
  uint32_t bytePosition;
  const char *obis;
} dsmrDiagnostic;

typedef enum
{
  DSMR_EVENT_TELEGRAM = 0,
  DSMR_EVENT_DIAGNOSTIC,
} dsmrEventType;

typedef struct
{
  dsmrEventType type;
  const dsmrTelegramResult *result;
  const dsmrDiagnostic *diagnostic;
} dsmrParserEvent;

struct dsmrParser_s;
typedef void (*dsmrEventCallback)(const struct dsmrParser_s *parser, const dsmrParserEvent *event, void *userData);

//-- --------------------------------------------------------------------------
//-- Parser internals exposed so the caller can own the instance.
//-- Applications shall not depend on the types and fields below.
//-- --------------------------------------------------------------------------

typedef enum
{
  DSMR_FRAMING_SEEK_HEADER = 0,
  DSMR_FRAMING_IN_BODY,
  DSMR_FRAMING_TRAILER_DIGITS,
  DSMR_FRAMING_TRAILER_CR,
  DSMR_FRAMING_TRAILER_LF,
} dsmrFramingState;

typedef struct
{
  dsmrFramingState state;
  uint8_t *buffer;
  size_t capacity;
  size_t length;
  size_t bodyLength;
  uint16_t runningCrc;
  uint16_t trailerCrc;
  uint8_t trailerDigits;
  bool crcPresent;
  bool frameActive;
} dsmrFraming;

typedef struct dsmrParser_s
{
  //-- Internal state. Do not touch from the application.
  bool initialized;
  bool inCallback;
  const dsmrProfile *profile;
  dsmrParserConfig config;
  char profileId[DSMR_MAX_PROFILE_ID_LEN];
  char countryCode[3];
  uint8_t *workspace;
  size_t workspaceSize;
  dsmrTelegramField *fieldArray;
  uint16_t fieldCapacity;
  uint8_t *rawCopy;
  dsmrFieldLabel *labelTable;
  uint16_t labelCount;
  char *labelPool;
  dsmrFraming framing;
  uint32_t lastByteTimeMs;
  uint32_t frameStartMs;
  bool hasLastByte;
} dsmrParser;

//-- --------------------------------------------------------------------------
//-- Operations
//-- --------------------------------------------------------------------------

//-- Reports the workspace size needed for the limits in the configuration.
size_t dsmrParserWorkspaceSizeNeeded(const dsmrParserConfig *config);

//-- Resolves the profile, applies its defaults, validates limits and
//-- configuration, and initializes the state before any bytes are consumed.
//-- An unknown profile, a country mismatch or insufficient workspace each
//-- yield a distinct error.
dsmrParserStatus dsmrParserInit(dsmrParser *parser, const dsmrParserConfig *config, uint8_t *workspace, size_t workspaceSize);

//-- Consumes a length-delimited byte array. The same telegram may span any
//-- number of calls and one call may contain multiple telegrams. Events are
//-- delivered in arrival order.
dsmrParserStatus dsmrParserFeed(dsmrParser *parser, const uint8_t *bytes, size_t byteCount, uint32_t monotonicTimeMs, dsmrEventCallback callback, void *userData);

//-- Applies the inter-byte and frame timeouts using caller-supplied monotonic
//-- time, including while no bytes arrive.
dsmrParserStatus dsmrParserTick(dsmrParser *parser, uint32_t monotonicTimeMs, dsmrEventCallback callback, void *userData);

//-- Discards the current partial telegram and returns the parser to its
//-- initial framing state with the same configuration.
void dsmrParserReset(dsmrParser *parser);

//-- Validates and applies a new configuration to a live instance without
//-- reallocating the workspace. A partial telegram is discarded and the
//-- instance returns to its initial framing state. A rejected configuration
//-- yields a distinct error and leaves the previous configuration fully
//-- active. The event callback shall not call this on its own instance.
dsmrParserStatus dsmrParserApplyConfig(dsmrParser *parser, const dsmrParserConfig *config);

//-- Sets, replaces or removes a single application label override on a live
//-- instance without rebuilding the label table. The override matches the
//-- exact OBIS code as received (concrete M-Bus channels included) and wins
//-- over the built-in default display name of the field. Passing a NULL name
//-- removes the override so the field falls back to its built-in default;
//-- removing an absent override succeeds. The parser copies both strings.
//-- Overrides added here live in the same storage as the configuration
//-- table and share the DSMR_MAX_FIELD_LABELS bound; a full table yields
//-- DSMR_ERR_INSUFFICIENT_WORKSPACE. The next dsmrParserApplyConfig restores
//-- the table from the configuration. The event callback shall not call this
//-- on its own instance.
dsmrParserStatus dsmrParserSetFieldLabel(dsmrParser *parser, const char *obis, const char *name);

//-- Reports an external data-loss event, such as a UART ring buffer overrun.
//-- The partial frame is discarded and a data-loss diagnostic is delivered.
dsmrParserStatus dsmrParserNotifyDataLoss(dsmrParser *parser, dsmrEventCallback callback, void *userData);

//-- Finds a field in a result by OBIS code. Views stay valid only while the
//-- result itself is valid.
dsmrParserStatus dsmrResultFindFieldByObis(const dsmrTelegramResult *result, const char *obis, const dsmrTelegramField **outField);

//-- Formats a value without locale dependence. Returns the number of
//-- characters written, excluding the terminator, or a negative value on
//-- invalid arguments.
int dsmrFormatValue(const dsmrValue *value, char *buffer, size_t bufferSize);

//-- Stable strings for diagnostics and logging.
const char *dsmrStatusToString(dsmrParserStatus status);
const char *dsmrDiagReasonToString(dsmrDiagReason reason);
const char *dsmrFieldClassToString(dsmrFieldClass classification);

//-- Library version string, e.g. "v0.9.0". The returned pointer is stable for
//-- the lifetime of the program and never NULL; the storage belongs to the
//-- parser, the caller must not free or modify it.
const char *dsmrParserVersion(void);

#ifdef __cplusplus
}
#endif

#endif //-- DSMR_PARSER_H
