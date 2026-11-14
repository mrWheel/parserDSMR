# parserDSMR — DSMR/P1 smart-meter telegram parser

**Date:** 2026-11-14 · **Build:** 2

parserDSMR is a reusable C library that reads and parses P1 telegrams from
Dutch and Belgian smart electricity meters. Point it at the P1 port of a
meter — or at a stored buffer of bytes — and it turns the raw serial stream
into structured, decoded fields you can act upon: energy delivered per
tariff, current power, voltage and current per phase, M-Bus gas/water/heat
readings, tariff indicator, text messages and more.

The portable core is plain ISO C11: no operating system, no dynamic
allocation, no locale dependence. The first release ships as an ESP-IDF
component built for the ESP32-S3 with ESP-IDF v6.0.2 (the component
manifest requires ESP-IDF >= 5.3). A later Arduino library will compile the
same `src/` sources and expose the same C API.

## Contents

- [Features](#features)
- [Installing the component](#installing-the-component)
- [Quick start](#quick-start)
- [What a P1 telegram looks like](#what-a-p1-telegram-looks-like)
- [How the parser works](#how-the-parser-works)
- [Supported profiles](#supported-profiles)
- [Configuring the parser](#configuring-the-parser)
- [Usage recipes](#usage-recipes)
- [API reference — portable core](#api-reference--portable-core)
- [API reference — ESP-IDF UART adapter](#api-reference--esp-idf-uart-adapter)
- [Building the sample programs](#building-the-sample-programs)
- [Host tests](#host-tests)
- [Telegram fixtures](#telegram-fixtures)
- [Hardware note](#hardware-note)
- [Directory structure](#directory-structure)
- [Status and pending work](#status-and-pending-work)
- [License](#license)

## Features

- **Portable ISO C11 core.** The parser under `src/` compiles with any
  C11 compiler; it uses only `<stdbool.h>`, `<stddef.h>` and `<stdint.h>`
  semantics and the standard library. The ESP-IDF dependency lives only in
  the optional UART adapter.
- **No dynamic allocation.** The application owns the parser instance and
  one workspace byte array; the parser carves everything it needs out of
  that buffer at `dsmrParserInit`/`dsmrParserApplyConfig` time. Nothing is
  allocated while bytes are received or telegrams are decoded.
- **Stream-oriented.** Bytes are handed in with `dsmrParserFeed`; a telegram
  may span any number of calls and one call may contain several telegrams.
- **Caller-supplied time.** Inter-byte and frame timeouts run on monotonic
  time you provide; `dsmrParserTick` keeps them alive between byte bursts.
- **Strict validation with clear diagnostics.** CRC-16/ARC verification,
  version acceptance, group-count and datatype checks, timestamp calendar
  validation, duplicate detection and M-Bus device-type cross-checks each
  report a distinct `dsmrDiagReason` with a byte position.
- **Exact numeric fidelity.** Decimal values are kept as an integer mantissa
  plus a scale, so published precision, valid zero, absent values and the
  protocol-invalid `FF` marker all survive the decode; formatting is locale
  independent.
- **M-Bus aware.** Channels 1–4 are declared by the application; readings
  are attached to their channel and the published device type is checked
  against the declaration.
- **Runtime reconfigurable.** `dsmrParserApplyConfig` swaps profile, M-Bus
  device table and label table on a live instance without reallocation; a
  rejected configuration leaves the previous one fully active.

## Installing the component

The component lives in `components/parserDSMR/`; its `src/`, `include/` and
`adapter/` entries are relative links to the shared sources at the
repository root. The component registers itself with
`idf_component_register()` and pulls in `esp_driver_uart`,
`esp_driver_gpio` and `esp_timer`.

The component is **not yet published** to the ESP Component Registry (the
manifest `idf_component.yml` is prepared for v0.1.0). Until then there are
two ways to use it in your own ESP-IDF project.

**Option A — reference the checkout with EXTRA_COMPONENT_DIRS.** Add the
repository's `components/` directory to the search path in your project's
top-level `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "/path/to/parserDSMR/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(myP1Reader)
```

**Option B — vendor the component.** Copy these four items into
`<your project>/components/parserDSMR/`, replacing the symlinked
directories with real copies (needed when symlinks do not survive your
distribution method):

```
components/parserDSMR/
├── CMakeLists.txt      copy from components/parserDSMR/CMakeLists.txt
├── src/                copy from the repository root src/
├── include/            copy from the repository root include/
└── adapter/            copy from the repository root adapter/
```

In both cases the application component then simply requires it:

```cmake
idf_component_register(
  SRCS "app_main.c"
  REQUIRES parserDSMR
)
```

Include the public headers with `#include "dsmr_parser.h"` (portable core)
and `#include "dsmr_uart_esp32.h"` (ESP-IDF UART adapter).

## Quick start

The smallest complete ESP-IDF application that reads a live P1 port and
prints one line per received field:

```c
//-- Quick start: read a DSMR 5.0 P1 port on UART1, RX on GPIO20.
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "dsmr_parser.h"
#include "dsmr_uart_esp32.h"

static dsmrParser parser;
static dsmrUart uart;
static uint8_t workspace[DSMR_DEFAULT_WORKSPACE_BYTES];

static void onEvent(const dsmrParser *parserInstance, const dsmrParserEvent *event, void *userData)
{
  (void)parserInstance;
  (void)userData;

  if (event->type == DSMR_EVENT_DIAGNOSTIC)
  {
    printf("rejected: %s (position %lu)\r\n",
           dsmrDiagReasonToString(event->diagnostic->reason),
           (unsigned long)event->diagnostic->bytePosition);
    return;
  }

  const dsmrTelegramResult *result = event->result;

  for (uint16_t index = 0; index < result->fieldCount; index++)
  {
    const dsmrTelegramField *field = &result->fields[index];
    char valueText[32];

    dsmrFormatValue(&field->value, valueText, sizeof valueText);
    printf("%-12s %s %s\r\n", field->obis, valueText, (field->unit != NULL) ? field->unit : "");
  }
}

void app_main(void)
{
  const dsmrParserConfig config =
  {
    .countryCode = "NL",
    .profileId = "nl.dsmr.5.0",
  };

  if (dsmrParserInit(&parser, &config, workspace, sizeof workspace) != DSMR_OK)
  {
    printf("parser init failed\r\n");
    return;
  }

  const dsmrUartConfig uartConfig =
  {
    .uartPort = UART_NUM_1,
    .rxPin = 20,
    .txPin = 21,
    .requestGpio = -1,    //-- request signal driven externally
    .invertRx = true,     //-- most P1 interface boards invert the signal
  };

  if (dsmrUartStart(&uart, &parser, &uartConfig, onEvent, NULL) != ESP_OK)
  {
    printf("uart start failed\r\n");
    return;
  }

  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
```

That is the whole program: the adapter owns the receive task, feeds bytes
to the parser and delivers every finished telegram (or rejection) to
`onEvent`. See [examples/live_uart](examples/live_uart) for the annotated
version and [examples/stored_telegram](examples/stored_telegram) for a
version that needs no wiring at all.

## What a P1 telegram looks like

The P1 port of a smart meter emits a human-readable telegram per transfer
interval. The annotated skeleton below is schematic (not a CRC-verified
frame):

```text
/SK5\2M550T-1014              header: manufacturer id + meter type
                              blank line
1-3:0.2.8(50)                 P1 protocol version (DSMR 5.0)
0-0:1.0.0(261114170412W)      timestamp: YYMMDDhhmmss + S/W (winter/summer)
0-0:96.1.1(45303038...)       meter identification, hex-encoded ASCII
1-0:1.8.1(001189.755*kWh)     energy delivered, tariff 1 (decimal + unit)
1-0:2.8.1(000213.728*kWh)     energy returned, tariff 1
0-0:96.14.0(0001)             active tariff indicator
1-0:1.7.0(02.906*kW)          current power delivered
1-0:32.7.0(233.5*V)           phase 1 voltage
1-0:99.97.0(1)(0-0:96.7.19)(241120143608W)(0000000284*s)
                              LOG field: 1 power-failure record
!                             trailer start
90A6                          CRC-16/ARC over everything from '/' through '!'
```

Key structure the parser enforces:

- The frame starts at the first `/` and ends with `!`, four hex CRC digits
  and `CR LF` (a bare `LF` only in the optional `acceptLfLineEndings`
  mode).
- The CRC is CRC-16/ARC (reflected polynomial `0xA001`, initial `0x0000`,
  no final XOR) computed over the bytes from `/` through `!` inclusive. The
  algorithm is check-verified against the standard `123456789` → `0xBB3D`
  vector by the host tests.
- Every data line is an OBIS code followed by parenthesized value groups,
  e.g. `1-0:1.8.1(001189.755*kWh)`. A `FF` value marks a protocol-invalid
  reading.
- M-Bus device readings use codes of the form `0-n:xx.x.x` where `n` is the
  channel (1–4); `0-n:24.1.0` publishes the device type on that channel.

The normative descriptions are in the DSMR P1 companion standards in
[assets/](assets/).

## How the parser works

One parser instance is a state machine driven from the outside; bytes only
move it forward when you call `dsmrParserFeed`, and time only moves it
forward when you call `dsmrParserTick` (or when the UART adapter does both
for you).

```text
dsmrParserFeed(bytes)
      │
      ▼
┌─────────────┐   '/' starts a candidate; bytes are stored in the
│  framing    │   workspace and enter the running CRC until '!' is
│ dsmr_framing│   passed inclusive; the trailer collects 4 hex digits,
└─────┬───────┘   CR and LF.
      │ frame complete
      ▼
┌─────────────┐   running CRC compared with the received trailer;
│     CRC     │   mismatch (or a missing CRC under the required
│  dsmr_crc   │   policy) rejects the candidate → DSMR_EVENT_DIAGNOSTIC
└─────┬───────┘
      │ CRC ok
      ▼
┌─────────────┐   the version field (1-3:0.2.8 or 0-0:96.1.4) is located
│ version /   │   and accepted against the configured profile; the
│  profile    │   automatic profile nl.dsmr.4 selects the concrete 4.0
│dsmr_profile │   or 4.2 table here.
└─────┬───────┘
      │ version accepted
      ▼
┌─────────────┐   each line is split into OBIS key + value groups and
│   decode    │   resolved against the profile field tables (the single
│ dsmr_decode │   place defining datatype and unit). Values are converted
│ dsmr_value  │   without locale dependence; timestamps are calendar-
└─────┬───────┘   validated; M-Bus channels are checked against the
      │           declared device table; fields are named from the label
      ▼           table.
┌─────────────┐   one DSMR_EVENT_TELEGRAM with the ordered field array,
│   events    │   or DSMR_EVENT_DIAGNOSTIC with reason + byte position.
└─────────────┘
```

Ownership rules that make this safe in embedded code:

- The application owns the `dsmrParser` instance, the workspace and every
  string the configuration references. The parser copies what it needs
  during `dsmrParserInit`/`dsmrParserApplyConfig` and never keeps caller
  pointers.
- Views handed out in a `dsmrTelegramResult` or `dsmrDiagnostic` (obis
  strings, text, unit, name, the field array) point into the parser
  workspace and stay valid **only for the duration of the event callback**.
  Copy what you want to keep.
- One instance belongs to one task. Calling back into the parser from its
  own event callback (feed, tick, data-loss notification) yields
  `DSMR_ERR_REENTRANT_CALL`.
- Rejected candidates produce a `DSMR_EVENT_DIAGNOSTIC`, never a partial
  `DSMR_EVENT_TELEGRAM`; the framer then resumes searching for the next
  `/`. A diagnostic such as `DSMR_DIAG_MBUS_TYPE_MISMATCH` is delivered
  *alongside* an accepted telegram because the telegram itself was valid.

A full narrated example of one telegram passing every stage is in
[docs/walkthrough.md](docs/walkthrough.md).

## Supported profiles

| Profile ID | Country | Reported version | UART | CRC | Transfer interval |
|---|---|---|---|---|---|
| `nl.dsmr.4.0` | NL | `1-3:0.2.8(40)` | 115200 8N1 | required | 10 s |
| `nl.dsmr.4.2` | NL | `1-3:0.2.8(42)` | 115200 8N1 | required | 10 s |
| `nl.dsmr.5.0` | NL | `1-3:0.2.8(50)` | 115200 8N1 | required | 1 s |
| `nl.dsmr.4` | NL | auto-selects 4.0 or 4.2 from the version field | 115200 8N1 | required | 10 s |
| `be.emucs.1.x` | BE | `0-0:96.1.4(50213…50217)` | 115200 8N1 | required | 1 s |
| `be.emucs.2.1` | BE | `0-0:96.1.4(50221)` | 115200 8N1 | required | 1 s |

The transfer interval is informational; the parser accepts whatever timing
the meter actually delivers, bounded by the configured timeouts. List the
built-in identifiers at run time with `dsmrProfileListIds()`.

The field tables (OBIS → datatype, unit, group count) per profile live in
`src/dsmr_profile.c`; the Belgian tables are initial coverage pending
verification against the published Fluvius documents (see Status below).
A meter that does not fit a built-in table can be described with a
`customProfile` (see [Configuring the parser](#configuring-the-parser)).

## Configuring the parser

Fill a `dsmrParserConfig` and pass it to `dsmrParserInit` (once) or
`dsmrParserApplyConfig` (at run time). Zero/NULL numeric and pointer fields
fall back to the defaults below.

| Field | Type | Meaning / default when zero or NULL |
|---|---|---|
| `countryCode` | `const char *` | Required. `"NL"` or `"BE"`; must match the profile's country, else `DSMR_ERR_COUNTRY_MISMATCH`. |
| `profileId` | `const char *` | Required unless `customProfile` is set. Must name a built-in profile, else `DSMR_ERR_UNKNOWN_PROFILE`. |
| `customProfile` | `const dsmrProfile *` | `NULL` = use `profileId`. Otherwise a full profile description you own; its `profileId`, `countryCode`, `versionObis` and `acceptedVersions` must be non-NULL. |
| `mbusDevices` / `mbusDeviceCount` | `const dsmrMbusDevice *` / `uint8_t` | Declared M-Bus channels 1–4, at most 4 entries, each channel at most once. `NULL`/0 = no M-Bus devices declared. |
| `fieldLabels` / `fieldLabelCount` | `const dsmrFieldLabel *` / `uint16_t` | Application-determined names per OBIS code, at most 64 entries; OBIS ≤ 15 characters, name ≤ 31 characters. `NULL`/0 = no names. |
| `maxTelegramBytes` | `uint16_t` | Telegram buffer limit; `0` → `DSMR_MAX_TELEGRAM_BYTES` (12288). May not exceed the hard maximum. |
| `maxLineBytes` | `uint16_t` | Longest single data line; `0` → `DSMR_MAX_LINE_BYTES` (2200). |
| `maxFields` | `uint16_t` | Upper bound of the received-field array; `0` → `DSMR_DEFAULT_MAX_FIELDS` (128); hard maximum 256. |
| `interByteTimeoutMs` | `uint32_t` | Maximum gap between bytes; `0` → 1000 ms. Exceeded → `DSMR_DIAG_TIMEOUT_INTER_BYTE`, partial frame discarded. |
| `frameTimeoutMs` | `uint32_t` | Maximum frame duration; `0` → 10000 ms. Exceeded → `DSMR_DIAG_TIMEOUT_FRAME`. |
| `acceptUnknownMinorVersion` | `bool` | `false`. When `true` (and the profile has no auto-select list), an all-digit minor version inside the same family — same digit count, same leading digit as an accepted version — is accepted and the configured table stays in force; the original version is reported. |
| `acceptLfLineEndings` | `bool` | `false`. When `true`, a bare `LF` closes lines and the trailer instead of strict `CR LF`. |
| `unknownFieldPolicy` | `dsmrUnknownFieldPolicy` | `DSMR_UNKNOWN_FIELD_SKIP` (0): lines without a table entry are ignored. `DSMR_UNKNOWN_FIELD_RAW`: they are kept as text fields so nothing published is lost. |

`dsmrParserInit` validates everything before consuming bytes: an unknown
profile, a country mismatch or insufficient workspace each yield a
distinct error code (see the API reference). Size your workspace with
`dsmrParserWorkspaceSizeNeeded(&config)`, or use
`DSMR_DEFAULT_WORKSPACE_BYTES` for a default configuration.

## Usage recipes

### Feeding bytes yourself (no UART adapter)

Any source of bytes works — a file, a log replay, a radio link:

```c
//-- Feed one stored telegram; events are delivered synchronously.
dsmrParserFeed(&parser, (const uint8_t *)telegram, strlen(telegram), 1000u, onEvent, NULL);
```

Bytes are consumed in arrival order across calls: a telegram split over
several `dsmrParserFeed` calls, or several telegrams in one call, are both
normal. `monotonicTimeMs` is your monotonic clock (milliseconds) at the
moment the chunk was taken; it drives the timeouts.

### Driving the timeouts while idle

When bytes can stop arriving mid-frame (a meter that resets its P1 port, a
stalled link), call `dsmrParserTick` periodically with the current time:

```c
//-- In a periodic task or timer, e.g. every 100 ms.
dsmrParserTick(&parser, xTaskGetTickCount() * portTICK_PERIOD_MS, onEvent, NULL);
```

Tick applies the inter-byte and frame timeouts and reports
`DSMR_DIAG_TIMEOUT_INTER_BYTE` / `DSMR_DIAG_TIMEOUT_FRAME` when a partial
frame goes stale. The UART adapter calls it for you between receives.

### Handling the two events

```c
static void onEvent(const dsmrParser *parserInstance, const dsmrParserEvent *event, void *userData)
{
  if (event->type == DSMR_EVENT_TELEGRAM)
  {
    const dsmrTelegramResult *result = event->result;
    //-- result->profileId, reportedVersion, meterIdentification,
    //-- crcStatus, receiveTimeMs, fields[], fieldCount
    //-- Valid only during this callback: copy what you keep.
  }
  else
  {
    const dsmrDiagnostic *d = event->diagnostic;
    printf("rejected: %s at byte %lu (obis %s)\r\n",
           dsmrDiagReasonToString(d->reason), (unsigned long)d->bytePosition,
           (d->obis != NULL) ? d->obis : "-");
  }
}
```

`receiveTimeMs` is the monotonic time passed to the `dsmrParserFeed` call
that delivered the frame's last byte. `meterIdentification` carries the
identification exactly as published (hex-encoded ASCII on Dutch meters).

### Extracting one value by OBIS code

```c
//-- Current power delivered, ignoring telegrams that do not carry it.
static void handlePower(const dsmrTelegramResult *result)
{
  const dsmrTelegramField *field = NULL;

  if ((dsmrResultFindFieldByObis(result, "1-0:1.7.0", &field) == DSMR_OK) &&
      (field->value.kind == DSMR_VALUE_NUMBER) && !field->value.protocolInvalid)
  {
    //-- Exact arithmetic: value == mantissa / 10^scale  (kW here).
    double kW = (double)field->value.mantissa / 1000.0;
    printf("power: %.3f %s\r\n", kW, (field->unit != NULL) ? field->unit : "");
  }
}
```

For display, `dsmrFormatValue` renders a value without locale dependence:
ABSENT → empty string, TEXT → the text, a protocol-invalid number →
`"FF"`, otherwise fixed-point notation honoring the scale
(`001189.755` → `"1189.755"`).

### M-Bus devices (gas, water, heat)

Declare what is attached; the parser fills in the channel per field and
cross-checks the published device type:

```c
static const dsmrMbusDevice mbusDevices[] =
{
  { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_GAS },
  { .channel = 2, .deviceType = DSMR_MBUS_DEVICE_WATER },
};

config.mbusDevices = mbusDevices;
config.mbusDeviceCount = sizeof mbusDevices / sizeof mbusDevices[0];
```

- A field like `0-1:24.2.1` arrives with `mbusChannel == 1`; direct
  electricity fields carry `mbusChannel == 0`.
- A published `0-n:24.1.0` value that contradicts your declaration yields
  `DSMR_DIAG_MBUS_TYPE_MISMATCH` on an otherwise accepted telegram.
- A channel you did not declare follows the unknown-field policy (skipped
  or kept raw).
- `dsmrResultFindFieldByObis(result, "0-1:24.2.1", &field)` finds it with
  the concrete channel digit.

### Naming fields for your application

Labels are yours to choose; the parser copies them and attaches them to
matching fields:

```c
static const dsmrFieldLabel fieldLabels[] =
{
  { .obis = "1-0:1.8.1", .name = "energyDeliveredTariff1" },
  { .obis = "1-0:1.8.2", .name = "energyDeliveredTariff2" },
  { .obis = "0-0:96.14.0", .name = "tariffIndicator" },
};

config.fieldLabels = fieldLabels;
config.fieldLabelCount = sizeof fieldLabels / sizeof fieldLabels[0];
```

Fields without a label keep `name == NULL` and remain reachable through
`dsmrResultFindFieldByObis`.

### Reconfiguring a running parser

Profile, M-Bus table and label table can be changed without reallocating
the workspace. A rejected configuration returns an error and leaves the
previous configuration fully active; an applied one discards any partial
telegram and restarts framing.

```c
//-- Swap the M-Bus table to match a heat meter on channel 2.
config.mbusDevices = heatDevices;
config.mbusDeviceCount = 2;

if (dsmrParserApplyConfig(&parser, &config) != DSMR_OK)
{
  //-- previous configuration still in force
}
```

Never call `dsmrParserApplyConfig` on the same instance from inside its own
event callback (re-entrancy is rejected where applicable and is undefined
behavior to attempt from the callback's own task in flight).

### Reporting external data loss

If the byte source itself dropped data (a UART ring buffer overrun you
detect outside the adapter), tell the parser so the stale partial frame is
discarded and the event is reported:

```c
dsmrParserNotifyDataLoss(&parser, onEvent, NULL);
```

This delivers `DSMR_DIAG_DATA_LOSS` and returns the framer to seeking.

### Resetting

`dsmrParserReset(&parser)` discards the current partial telegram and
returns the instance to its initial framing state, keeping the
configuration — useful after a meter reboot or a mode switch.

### Describing a meter with a custom profile

When no built-in profile fits, pass a `customProfile` you own. The unit of
description is the `dsmrFieldDefinition` row — the single place defining
syntax, datatype and unit for an OBIS code:

```c
static const dsmrFieldDefinition myFields[] =
{
  //-- obis            type                     unit   maxScale groupCount flags
  { "1-0:1.8.1", DSMR_FIELD_TYPE_DECIMAL,      "kWh", 3,       1,           0 },
  { "0-0:96.14.0", DSMR_FIELD_TYPE_INTEGER,   NULL,   0,       1,           0 },
  { "0-?:24.1.0", DSMR_FIELD_TYPE_INTEGER,     NULL,   0,       1,           0 }, //-- '?' = channels 1-4
  { "0-?:24.2.1", DSMR_FIELD_TYPE_DECIMAL,     "m3",   3,       2,           0 }, //-- timestamp + value
};

static const dsmrFieldBlock myBlock = { .entries = myFields, .count = sizeof myFields / sizeof myFields[0] };

static const char *const myVersions[] = { "62", NULL };

static const dsmrProfile myProfile =
{
  .profileId = "nl.custom.mymeter",
  .countryCode = "NL",
  .versionObis = "1-3:0.2.8",
  .acceptedVersions = myVersions,
  .crcPolicy = DSMR_CRC_POLICY_REQUIRED,
  .uartBaudRate = 115200u, .uartDataBits = 8, .uartParity = 0, .uartStopBits = 1,
  .telegramIntervalMs = 1000,
  .fieldBlocks = &myBlock,
  .fieldBlockCount = 1,
  .autoProfiles = NULL,
};
```

Notes: `groupCount` `0` accepts any number of groups; DECIMAL fields with
two groups carry a value and a timestamp (the Belgian maximum-demand field;
the M-Bus `0-n:24.2.1` layout with the timestamp *before* the value is
handled automatically); LOG fields add `logPrefixGroups` and
`logEntryGroups` and yield the record count as their value — the record
groups are validated but not expanded. A profile is assembled from one or
more `dsmrFieldBlock`s so shared object groups are written once. Repeatable
OBIS codes (two readings of the same code in one telegram, as published by
some meters) carry `DSMR_FIELD_FLAG_REPEATABLE`; otherwise a second
occurrence yields `DSMR_DIAG_DUPLICATE_FIELD`.

## API reference — portable core

All prototypes below are from [include/dsmr_parser.h](include/dsmr_parser.h)
and carry `extern "C"` linkage for C++ use. Every function is safe to call
from any task that owns the instance; none allocate.

### Operations

```c
size_t dsmrParserWorkspaceSizeNeeded(const dsmrParserConfig *config);
```
Returns the exact workspace size in bytes for the (effective) limits in
`config`. Use it instead of `DSMR_DEFAULT_WORKSPACE_BYTES` whenever you set
custom limits.

```c
dsmrParserStatus dsmrParserInit(dsmrParser *parser, const dsmrParserConfig *config,
                               uint8_t *workspace, size_t workspaceSize);
```
Resolves the profile, applies defaults, validates limits and configuration,
and initializes the state — all before any byte is consumed. Errors:
`DSMR_ERR_NULL_ARGUMENT`, `DSMR_ERR_UNKNOWN_PROFILE`,
`DSMR_ERR_COUNTRY_MISMATCH`, `DSMR_ERR_INVALID_CONFIG`,
`DSMR_ERR_INSUFFICIENT_WORKSPACE`.

```c
dsmrParserStatus dsmrParserFeed(dsmrParser *parser, const uint8_t *bytes, size_t byteCount,
                               uint32_t monotonicTimeMs, dsmrEventCallback callback, void *userData);
```
Consumes a length-delimited byte array. The same telegram may span any
number of calls; one call may contain multiple telegrams. Events are
delivered in arrival order, synchronously, before this call returns.
`DSMR_ERR_REENTRANT_CALL` when called from the instance's own callback.

```c
dsmrParserStatus dsmrParserTick(dsmrParser *parser, uint32_t monotonicTimeMs,
                               dsmrEventCallback callback, void *userData);
```
Applies the inter-byte and frame timeouts using caller-supplied monotonic
time, including while no bytes arrive.

```c
void dsmrParserReset(dsmrParser *parser);
```
Discards the current partial telegram and returns to the initial framing
state with the same configuration.

```c
dsmrParserStatus dsmrParserApplyConfig(dsmrParser *parser, const dsmrParserConfig *config);
```
Validates and applies a new configuration to a live instance without
reallocating the workspace. A partial telegram is discarded and framing
restarts. A rejected configuration yields a distinct error and leaves the
previous configuration fully active. Must not be called from the event
callback on the same instance.

```c
dsmrParserStatus dsmrParserNotifyDataLoss(dsmrParser *parser, dsmrEventCallback callback, void *userData);
```
Reports an external data-loss event (e.g. UART ring buffer overrun): the
partial frame is discarded and a `DSMR_DIAG_DATA_LOSS` diagnostic is
delivered.

```c
dsmrParserStatus dsmrResultFindFieldByObis(const dsmrTelegramResult *result, const char *obis,
                                          const dsmrTelegramField **outField);
```
Finds a field by exact OBIS string (M-Bus codes include the concrete
channel digit). Returns `DSMR_ERR_NOT_FOUND` and `*outField == NULL` when
absent. The returned view is valid only while the result is.

```c
int dsmrFormatValue(const dsmrValue *value, char *buffer, size_t bufferSize);
```
Locale-free rendering: returns the number of characters written excluding
the terminator, or negative on invalid arguments. ABSENT → `""`, TEXT →
the text, `FF` → `"FF"`, numbers → fixed-point honoring `scale`.

```c
const char *dsmrStatusToString(dsmrParserStatus status);
const char *dsmrDiagReasonToString(dsmrDiagReason reason);
```
Stable strings for diagnostics and logging.

```c
const dsmrProfile *dsmrProfileLookup(const char *profileId);
const char *const *dsmrProfileListIds(void);
```
Resolves a built-in profile by identifier (NULL when unknown) and returns
the NULL-terminated list of built-in identifiers.

### Result and error codes

`dsmrParserStatus`:

| Code | Meaning |
|---|---|
| `DSMR_OK` | Success. |
| `DSMR_ERR_NULL_ARGUMENT` | A required pointer was NULL (also: missing `countryCode`, or missing `profileId` without `customProfile`). |
| `DSMR_ERR_UNKNOWN_PROFILE` | `profileId` does not name a built-in profile. |
| `DSMR_ERR_COUNTRY_MISMATCH` | `countryCode` does not match the profile's country. |
| `DSMR_ERR_INSUFFICIENT_WORKSPACE` | Workspace smaller than `dsmrParserWorkspaceSizeNeeded`. |
| `DSMR_ERR_INVALID_CONFIG` | Limit or table violation (M-Bus channel/duplicate/count, label length/count, oversized limits, malformed `customProfile`). |
| `DSMR_ERR_NOT_INITIALIZED` | Used before a successful `dsmrParserInit`. |
| `DSMR_ERR_REENTRANT_CALL` | Feed/tick/data-loss called from the instance's own callback. |
| `DSMR_ERR_NOT_FOUND` | `dsmrResultFindFieldByObis` matched nothing. |

`dsmrDiagReason` (delivered in `dsmrDiagnostic`):

| Reason | Cause |
|---|---|
| `DSMR_DIAG_MISSING_CRC` | Required CRC absent (`!` directly followed by CR). |
| `DSMR_DIAG_MALFORMED_TRAILER` | Trailer digits/CR/LF structure invalid. |
| `DSMR_DIAG_CRC_MISMATCH` | Computed CRC ≠ received CRC. |
| `DSMR_DIAG_MISSING_VERSION` | Version field not present in the frame. |
| `DSMR_DIAG_INVALID_VERSION` | Version not accepted for the profile. |
| `DSMR_DIAG_SYNTAX_ERROR` | Malformed line, OBIS code or value group. |
| `DSMR_DIAG_SIZE_LIMIT` | Telegram/line/field limit exceeded. |
| `DSMR_DIAG_TIMEOUT_INTER_BYTE` | Gap between bytes exceeded `interByteTimeoutMs`. |
| `DSMR_DIAG_TIMEOUT_FRAME` | Frame exceeded `frameTimeoutMs`. |
| `DSMR_DIAG_DATA_LOSS` | Reported via `dsmrParserNotifyDataLoss`. |
| `DSMR_DIAG_DUPLICATE_FIELD` | Same OBIS+channel twice on a non-repeatable field. |
| `DSMR_DIAG_MBUS_TYPE_MISMATCH` | Published `0-n:24.1.0` contradicts the declaration (delivered with an accepted telegram). |
| `DSMR_DIAG_VALUE_OVERFLOW` | Numeric value too large for the field. |
| `DSMR_DIAG_INVALID_TIMESTAMP` | Timestamp failed calendar validation. |

`dsmrCrcStatus`: `DSMR_CRC_STATUS_VERIFIED`, `DSMR_CRC_STATUS_ABSENT`.

`dsmrEventType`: `DSMR_EVENT_TELEGRAM`, `DSMR_EVENT_DIAGNOSTIC`.

### Data types

```c
typedef struct
{
  dsmrValueKind kind;   //-- DSMR_VALUE_ABSENT / DSMR_VALUE_NUMBER / DSMR_VALUE_TEXT
  bool protocolInvalid; //-- the meter published FF: the reading is explicitly invalid
  int64_t mantissa;     //-- NUMBER: value == mantissa * 10^-scale
  uint8_t scale;        //-- NUMBER: number of published decimal digits
  const char *text;     //-- TEXT: view into the workspace, valid during the callback
} dsmrValue;
```

```c
typedef struct
{
  bool valid;
  uint16_t year; uint8_t month, day, hour, minute, second;
  bool hasOffset; int16_t offsetMinutes;  //-- UTC offset when published
  char dst;                               //-- source S/W indicator, 0 when absent
} dsmrTimestamp;
```

```c
typedef struct
{
  const char *obis;        //-- as published (M-Bus codes carry the concrete channel)
  uint8_t mbusChannel;     //-- 0 = direct/electricity, 1..4 = M-Bus channel
  const char *name;        //-- from your label table, or NULL
  dsmrValue value;
  const char *unit;        //-- from the profile table, or NULL (text/unknown fields)
  dsmrTimestamp timestamp; //-- valid when hasTimestamp
  bool hasTimestamp;
} dsmrTelegramField;
```

```c
typedef struct
{
  const char *profileId;        //-- resolved profile (including the auto-selected one)
  const char *reportedVersion;  //-- version text exactly as published, or NULL
  const char *meterIdentification; //-- as published (hex ASCII on Dutch meters), or NULL
  dsmrCrcStatus crcStatus;
  uint32_t receiveTimeMs;       //-- monotonic time of the frame's last byte
  const dsmrTelegramField *fields;
  uint16_t fieldCount;
} dsmrTelegramResult;
```

```c
typedef struct
{
  dsmrDiagReason reason;
  uint32_t bytePosition;   //-- DSMR_POS_UNKNOWN when not attributable
  const char *obis;        //-- offending OBIS code when known, else NULL
} dsmrDiagnostic;
```

```c
typedef struct
{
  dsmrEventType type;
  const dsmrTelegramResult *result;    //-- valid when type == DSMR_EVENT_TELEGRAM
  const dsmrDiagnostic *diagnostic;    //-- valid when type == DSMR_EVENT_DIAGNOSTIC
} dsmrParserEvent;

typedef void (*dsmrEventCallback)(const struct dsmrParser_s *parser,
                                  const dsmrParserEvent *event, void *userData);
```

Other enums: `dsmrMbusDeviceType` (`_GAS, _WATER, _HEAT, _COOLING,
_ELECTRICITY, _OTHER`), `dsmrFieldType` (`_DECIMAL, _INTEGER, _TEXT,
_TIMESTAMP, _LOG`), `dsmrCrcPolicy` (`_REQUIRED, _ABSENT, _OPTIONAL`),
`dsmrUnknownFieldPolicy` (`_SKIP, _RAW`). The `dsmrParser`, `dsmrFraming`
and `dsmrFieldDefinition`/`dsmrFieldBlock`/`dsmrProfile` structures are
documented above; the internals of `dsmrParser`/`dsmrFraming` must not be
touched by the application.

### Limits and defaults

| Constant | Value | Note |
|---|---|---|
| `DSMR_MAX_TELEGRAM_BYTES` | 12288 | Telegram buffer allowance (115200 baud, 1 Hz profiles). |
| `DSMR_MAX_LINE_BYTES` | 2200 | OBIS prefix + delimiters + 2048 hex chars of a Dutch text message. |
| `DSMR_MAX_FIELDS` / `DSMR_DEFAULT_MAX_FIELDS` | 256 / 128 | Received-field array bounds. |
| `DSMR_MBUS_CHANNEL_MIN` / `MAX`, `DSMR_MAX_MBUS_DEVICES` | 1 / 4, 4 | P1 publishes channels 0-1 … 0-4. |
| `DSMR_MAX_FIELD_LABELS` | 64 | Label table entries. |
| `DSMR_MAX_LABEL_OBIS_LEN` / `DSMR_MAX_LABEL_NAME_LEN` | 16 / 32 | Exclusive bounds (≤15 / ≤31 chars). |
| `DSMR_MAX_PROFILE_ID_LEN` | 32 | Copied configuration strings. |
| `DSMR_DEFAULT_INTER_BYTE_TIMEOUT_MS` | 1000 | |
| `DSMR_DEFAULT_FRAME_TIMEOUT_MS` | 10000 | |
| `DSMR_POS_UNKNOWN` | `0xFFFFFFFF` | Unattributable byte position. |
| `DSMR_DEFAULT_WORKSPACE_BYTES` | (macro) | Conservative size for default limits. |

## API reference — ESP-IDF UART adapter

[include/dsmr_uart_esp32.h](include/dsmr_uart_esp32.h) is outside the
Arduino compilation path; the portable core does not depend on it. The
adapter configures the UART from the resolved profile (baud rate, data
bits, parity, stop bits) and owns a receive task, so the parser event
callback runs on that task: one parser instance belongs to that task, and
the application must not feed the same parser from elsewhere while the
adapter runs.

```c
typedef struct
{
  uart_port_t uartPort;       //-- UART_NUM_0/1/2
  int rxPin;                  //-- ESP32 RX, wired to the meter's P1 transmit line
  int txPin;                  //-- ESP32 TX, passed to the UART driver; the P1 port is transmit-only from the meter
  int requestGpio;            //-- GPIO driving the meter's request signal; -1 = driven externally
  bool requestActiveHigh;     //-- polarity of the request signal
  bool invertRx;              //-- true when the interface board inverts the signal
  uint32_t requestLeadMs;     //-- request assertion lead time before reception (0 = none)
  uint32_t taskStackSize;     //-- 0 → 4096
  uint8_t taskPriority;       //-- 0 → 5
} dsmrUartConfig;
```

```c
esp_err_t dsmrUartStart(dsmrUart *uart, dsmrParser *parser, const dsmrUartConfig *config,
                        dsmrEventCallback callback, void *userData);
esp_err_t dsmrUartStop(dsmrUart *uart);
```

`dsmrUartStart` requires an already-initialized parser (its resolved
profile governs the line settings), installs the UART driver with a 4096
byte RX ring buffer, starts the receive task, and **only then** activates
the request signal. `dsmrUartStop` stops the task and releases the UART
driver and the request GPIO. Reception is always initialized before the
request signal is activated, so a meter that starts streaming immediately
is never missed.

## Building the sample programs

### Repository root — no-hardware test project

The repository root is a no-hardware test project. It replays the sample
telegram in `test/fixtures/dsmr_sample_telegram.h` and prints the decoded
fields in the monitor. It adds the component from `components/parserDSMR`,
which points to the shared source, adapter, and include directories. The
application labels in `test/fixtures/dsmr_sample_labels.h` are based on the
DSMR 5.0.2 P1 object table. Each line shows the OBIS code, label and value;
an unknown label is shown as `-`.

With the VS Code ESP-IDF extension, open the repository root as a project,
or on the command line:

```
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

`flash` is needed to run the new test application on the board; `build`
alone only creates the firmware image. The test application does not need
a P1 meter, but it does need an ESP32-S3 connected to the selected serial
port. (Flashing is done by the user; this project's convention is that the
developer never runs flash commands.)

### The dedicated examples

To run either dedicated example instead, open its folder as a project:

```
cd examples/live_uart
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

The same sequence applies to `examples/stored_telegram`. The stored
telegram program needs no P1 wiring: it replays the shared fixtures and
also demonstrates dynamic reconfiguration (M-Bus device table, profileId
and label table changes) through `dsmrParserApplyConfig`. `examples/live_uart`
waits for real telegrams on the configured UART and prints one line per
OBIS field; switch meter and profile by editing the defines at the top of
its `main/app_main.c`.

## Host tests

```
cd test/host
make run
```

The host build compiles the portable core as C11 without ESP-IDF, with
address and undefined behaviour sanitizers, and exercises the acceptance
list of projectPrompt.md section 7.

## Telegram fixtures

The fixtures in `test/fixtures/dsmr_fixtures.h` are shared by the host
tests and the stored telegram program. They are generated:

```
cd test/host
make fixtures
```

`test/fixtures/tools/generate_fixtures.py` computes every trailer CRC with
an independent CRC-16/ARC implementation. Source and revision are recorded
with each fixture. The Belgian fixtures are structured after the eMUCs
object tables and are marked pending verification against the published
Fluvius documents.

## Hardware note

The P1 port of the meter is an idle-high serial interface. Choose a P1
interface board that provides signal conditioning and level shifting to
the ESP32-S3 3.3 V I/O level. When the board presents the signal inverted,
set `invertRx` in `dsmrUartConfig`. The request signal is driven by the
adapter when `requestGpio` is set, and must be driven externally when
`requestGpio` is -1; reception is always initialized before the request
signal is activated.

## Directory structure

```
parserDSMR/
├── CMakeLists.txt            ESP-IDF no-hardware test project
├── components/parserDSMR/    ESP-IDF component used by the root test project
├── main/                     no-hardware sample replay application
├── idf_component.yml         manifest for the ESP Component Registry
├── LICENSE                   MIT
├── README.md                 this document
├── projectPrompt.md          product specification (authoritative)
├── include/                  public headers
│   ├── dsmr_parser.h         public C API of the portable core
│   └── dsmr_uart_esp32.h     ESP-IDF UART adapter
├── src/                      portable core (the future Arduino src/)
│   ├── dsmr_crc.c/.h         CRC-16/ARC over '/' through '!'
│   ├── dsmr_framing.c/.h     seek-header / body / trailer state machine
│   ├── dsmr_value.c/.h       locale-free decimal, integer and timestamp conversion
│   ├── dsmr_profile.c/.h     profile registry and field tables (NL, BE)
│   ├── dsmr_decode.c/.h      OBIS line splitting and field decoding
│   └── dsmr_parser.c         configuration, workspace, events, public API
├── adapter/
│   └── dsmr_uart_esp32.c     UART receive task, outside the Arduino path
├── examples/
│   ├── stored_telegram/      fixture replay from the USB console, no wiring
│   └── live_uart/            UART reception and one line per OBIS display
├── test/
│   ├── fixtures/             shared telegram set + CRC generator tool
│   └── host/                 host tests, compiled as C11 without ESP-IDF
├── assets/                   DSMR standard and companion-standard PDFs
└── docs/
    └── walkthrough.md        one telegram through the modules
```

## Status and pending work

- Full OBIS inventory per profile: the first-release field tables cover the
  objects required by projectPrompt.md section 5; the Belgian tables are
  initial coverage pending the verified inventory (see table comments in
  `src/dsmr_profile.c`).
- Luxembourg, Sweden, Finland and Norway need separate transport decoders
  and verified profiles before being listed.
- The Arduino packaging (`library.properties`, examples) follows the
  ESP-IDF release; `src/` is already the Arduino source directory and the
  public header provides C linkage.
- Publishing to the ESP Component Registry is pending (manifest prepared,
  v0.1.0).

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 Willem Aandewiel
(mrWheel).
