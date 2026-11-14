# DSMR/P1 Parser — Project Specification

**Date:** 2026-11-14 · **Build:** 3

## 1. Product and scope

The project shall deliver a reusable library that reads and parses P1 telegrams from smart meters. The first release shall be an ESP-IDF component developed in VS Code and built for the ESP32-S3 with ESP-IDF 6.0.2. A later Arduino library shall compile the same parser sources as C and expose the same C API to Arduino sketches.

The parser and ESP-IDF adapter shall be written in standard C. The portable core shall target ISO C11 and use standard C headers. The code shall use plain structs, enums, functions, and explicit control flow. The framing logic shall be a readable state machine. Protocol tables shall use ordinary C initializers. The code shall use Allman braces, lowerCamelCase for functions and variables, and English identifiers, comments, and diagnostics.

The first release shall support the Dutch and Belgian profiles listed below. The library shall read P1 telegrams and the submeter data published in those telegrams. P2, P3, GPRS, and raw M-Bus or OMS communication are outside the initial scope.

## 2. Protocol profiles

The protocol profile shall be determined when a parser instance is initialized. The configuration shall provide an ISO 3166-1 alpha-2 country code and a profile ID. Each profile shall define the expected telegram version, framing, CRC policy, UART defaults, and OBIS field definitions. Each instance shall maintain its own configuration and parsing state.

| Profile ID | Country | Reference revision | Reported telegram version | UART | CRC | Telegram interval |
|---|---|---|---|---|---|---|
| `nl.dsmr.4.0` | NL | DSMR 4.0.4 and 4.0.7 | `1-3:0.2.8(40)` | 115200 8N1 | Required | 10 s |
| `nl.dsmr.4.2` | NL | DSMR 4.2.2 | `1-3:0.2.8(42)` | 115200 8N1 | Required | 10 s |
| `nl.dsmr.5.0` | NL | DSMR 5.0.2 | `1-3:0.2.8(50)` | 115200 8N1 | Required | 1 s |
| `be.emucs.1.x` | BE | eMUCs 1.3 through 1.7.1 | `0-0:96.1.4`, values `50213` through `50217` | 115200 8N1 | Required | 1 s |
| `be.emucs.2.1` | BE | eMUCs 2.1.1 | `0-0:96.1.4(50221)` | 115200 8N1 | Required | 1 s |

The interval applies while the request signal enables output. A document revision and a telegram version shall be represented separately. For example, `40` is shared by the 4.0.4 and 4.0.7 documents. The Belgian value `50217` identifies a DSMR 5.0.2 base and eMUCs 1.7. The profile `nl.dsmr.4` shall also be available. It shall select the 4.0 or 4.2 field definitions from the version field of a CRC-validated telegram.

The built-in profiles shall check telegram versions strictly by default. An explicit compatibility option may accept an unrecognized minor version within the selected protocol family after validating framing and CRC. It shall report the original version and expose only fields with established semantics in the base profile. DSMR 5.0.2 shall be the initial supported 5.x revision. Additional revisions shall have their own verified rules and fixtures before being listed as supported.

The core shall support three CRC policies for profile definitions. `Required` requires a valid CRC. `Absent` requires the line ending to follow `!` immediately. `Optional` accepts an absent CRC and validates any supplied CRC in full. The result shall distinguish a verified CRC from an absent one. All built-in NL and BE profiles shall require CRC. DSMR 3.0, a possible later addition, uses 9600 baud, ends on `!\r\n`, and requires its own rules for multiline `24.3.0` readings.

| Country | Extension requirements |
|---|---|
| LU | Luxembourg P1 1.1.3 uses DSMR 5.0.2 as a base and adds an AES-128-GCM envelope. A separate transport decoder shall authenticate the payload before passing the plaintext telegram to the parser. A local profile shall describe the OBIS fields. |
| SE | A Swedish profile shall specify the meter or operator implementation, including its ASCII framing, CRC, timestamps, and national OBIS dataset. |
| FI | A Finnish profile shall specify the actual meter protocol. The national metering guidance describes an ASCII RJ12 interface and phase-specific measurements. Concrete framing, update intervals, and mappings shall be verified per implementation. |
| NO | Support for the Norwegian wired M-Bus HAN variant over RJ45 shall require a separate decoder. |

The profile registry shall allow additional definitions without changing the UART adapter or common ASCII framer. Each supported profile shall have a stable identifier and a reference document, a field table, and positive and negative fixtures. Transport formats with a different wire encoding shall have separate decoders.

## 3. Public C API

The public header shall declare the configuration, parser state, telegram result, error event, and operations. A parser instance shall be an ordinary C struct initialized by a function. The initial design shall use caller-owned state and workspace. The library shall report the workspace size needed for the selected limits and reject insufficient storage. It shall perform no dynamic allocation in the parser during reception or decoding. The configuration and workspace ownership and lifetime shall be documented.

The following example illustrates the intended C API and its initialization contract.

```c
#include "dsmr_parser.h"

dsmrParser parser;
uint8_t workspace[DSMR_DEFAULT_WORKSPACE_BYTES];
const dsmrParserConfig config =
{
    .countryCode = "NL",
    .profileId = "nl.dsmr.4.2"
};

if (dsmrParserInit(&parser, &config, workspace, sizeof workspace) == DSMR_OK)
{
    dsmrParserFeed(&parser, bytes, byteCount, monotonicTimeMs, onEvent, userData);
}
```

| Operation | Required behavior |
|---|---|
| `dsmrParserInit` | Resolves the profile, applies its defaults, validates limits and configuration, and initializes the state before any bytes are consumed. An unknown profile, country mismatch, or insufficient workspace yields a distinct error. |
| `dsmrParserFeed` | Consumes a length-delimited byte array. The same telegram may span any number of calls. A call may contain multiple telegrams. Events are delivered in arrival order. |
| `dsmrParserTick` | Applies timeouts using caller-supplied monotonic time, including while no bytes arrive. |
| `dsmrParserReset` | Discards the current partial telegram and returns the parser to its initial framing state with the same configuration. |
| `dsmrParserApplyConfig` | Validates and applies a new configuration to a live instance without reallocating the workspace. Resolves the profile again, checks the limits against the existing workspace, and applies the new values from the next telegram onward. A partial telegram in progress shall be discarded and the instance shall return to its initial framing state. A rejected configuration yields a distinct error and leaves the previous configuration fully active. The callback shall not call this on its own instance. |

The configuration shall be adjustable while the application runs. The application shall build a complete `dsmrParserConfig`, pass it to `dsmrParserApplyConfig` between callbacks, and the new values shall govern the next telegram. Every configuration value shall be adjustable at runtime, including `countryCode` and `profileId`. Switching `profileId` shall resolve the profile again and apply its field table, CRC policy, and defaults from the next telegram onward. Configuration changes shall take effect only between telegrams so that a decoded result always reflects one consistent configuration. The configuration remains caller-owned; the parser shall copy the values it needs during the call and shall not retain the caller's pointer.

The configuration shall declare which devices are attached to the meter's M-Bus interface. Each entry shall pair an M-Bus channel number with a device type — gas, water, heat, cooling, electricity, or other — as published by `0-n:24.1.0`. The device table shall be a caller-owned array with an explicit count, bounded by a documented maximum channel number. The decoder shall use the declared type to label each submeter record and to select the device-specific reading fields defined in section 5. A telegram value of `0-n:24.1.0` that contradicts the declared type shall be reported as a mismatch diagnostic. A channel published by the meter but absent from the table shall follow the unknown-field policy, and a declared channel that never appears shall simply remain absent in the result.

The following example illustrates the M-Bus device declaration and a dynamic configuration change.

```c
#include "dsmr_parser.h"

//-- Static description of the devices attached to the meter's M-Bus interface.
const dsmrMbusDevice mbusDevices[] =
{
    { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_GAS },
    { .channel = 2, .deviceType = DSMR_MBUS_DEVICE_WATER }
};

dsmrParserConfig config =
{
    .countryCode = "NL",
    .profileId = "nl.dsmr.5.0",
    .mbusDevices = mbusDevices,
    .mbusDeviceCount = sizeof mbusDevices / sizeof mbusDevices[0]
};

//-- Later, while the application runs, the water meter on channel 2 is
//-- replaced by a heat meter. The change applies to the next telegram and
//-- discards any partial frame in progress.
dsmrMbusDevice updatedDevices[] =
{
    { .channel = 1, .deviceType = DSMR_MBUS_DEVICE_GAS },
    { .channel = 2, .deviceType = DSMR_MBUS_DEVICE_HEAT }
};

config.mbusDevices = updatedDevices;
config.mbusDeviceCount = 2;

if (dsmrParserApplyConfig(&parser, &config) != DSMR_OK)
{
    //-- The previous configuration remains active.
}
```

Each complete, accepted telegram shall generate one event with a validated result. A rejected candidate shall generate a diagnostic containing the reason and the byte position or OBIS code where available. The result shall include the profile ID, reported version, meter identification, CRC status, and receive time. Length-delimited views shall remain valid for the duration of the callback. The caller shall copy data needed afterward. Each instance shall be owned by one task and shall disallow re-entry from its callback. Telegram fields shall start empty for each candidate. Applications may retain a copy of the last valid result.

## 4. Framing and validation

The parser shall read through explicit states for seeking a header, receiving the body, and validating the trailer. A leading slash at a valid header position shall start a candidate. The strict ASCII profiles shall require the identification line, blank line, data lines, and terminator with the prescribed CR LF boundaries. The header shall be preserved as meter identification. The field containing the reported protocol version shall govern version validation. The character `5` in the identification line has no role in version selection.

For CRC profiles, the parser shall compute CRC-16/ARC over the original byte sequence from `/` through `!` inclusive, including intervening CR LF bytes. The parameters shall be initial value `0x0000`, polynomial `0x8005` (reflected implementation value `0xA001`), LSB-first input processing, and final XOR `0x0000`. The four trailer hex digits encode the CRC with the most significant nibble first. The digits and trailing CR LF are outside the calculation. The check value for `123456789` shall be `0xBB3D`. An explicitly enabled LF compatibility mode shall still calculate CRC over the received bytes.

The parser shall publish a telegram only after the full trailer, CRC where required, telegram version, and field syntax are validated. Missing CRC, malformed trailer, CRC mismatch, invalid version, syntax error, size limit, and timeout shall be distinguishable. After a truncated or corrupt frame or a UART data-loss notification, the parser shall discard the partial frame and resume searching for a valid header. Input noise and a bad frame shall allow recovery on the next complete valid frame.

Configuration shall bound telegram size, line length, field groups, and repeated records. Inter-byte timeout and total frame timeout shall have separate settings. The transfer interval shall be independent of either timeout. The design shall use 12 KiB as an initial telegram buffer allowance for the 115200 baud, 1 Hz profiles, subject to validation against the object tables. That allowance is an implementation setting. The line limit shall accommodate the OBIS prefix, delimiters, and 2048 hex characters of a maximum-length Dutch text message. The Belgian demand-history line shall fit within the corresponding profile limits. Processing time shall grow linearly with input size.

## 5. OBIS decoding and output

Each decoded field shall be presentable as one line holding the OBIS code, the M-Bus channel where present, the value, and the unit. The test programs in section 6 shall use this one line per OBIS presentation.

Application programs shall determine their own field names. The configuration shall contain an application-owned label table pairing an OBIS code with an application label, for example `1-0:1.8.1` with `energyDeliveredTariff1`; another program may give the same OBIS the label `EnDelTar1`. The profile field table shall remain the single place defining syntax, datatype, and unit; the label table only adds application display names. The parser shall resolve every accepted field against the label table and shall expose the accepted telegram as an ordered array of received fields. Each entry shall hold the OBIS text, the M-Bus channel where present, the resolved label or a null pointer when the application registered no label, the value, and the unit. The label table shall be adjustable through `dsmrParserApplyConfig`. A field without a label shall remain reachable through its OBIS code by a result lookup operation.

```c
//-- Application-determined names. Another program may label the same
//-- OBIS codes differently, for example EnDelTar1.
static const dsmrFieldLabel fieldLabels[] =
{
    { .obis = "1-0:1.8.1", .name = "energyDeliveredTariff1" },
    { .obis = "1-0:1.8.2", .name = "energyDeliveredTariff2" },
    { .obis = "1-0:2.8.1", .name = "energyReturnedTariff1" },
    { .obis = "1-0:2.8.2", .name = "energyReturnedTariff2" }
};

const dsmrParserConfig config =
{
    .countryCode = "NL",
    .profileId = "nl.dsmr.4.2",
    .fieldLabels = fieldLabels,
    .fieldLabelCount = sizeof fieldLabels / sizeof fieldLabels[0]
};
```

```c
//-- One line per received field: the application name when known,
//-- the OBIS code otherwise.
static void printFields(const dsmrTelegramResult *result)
{
    for (uint16_t index = 0; index < result->fieldCount; index++)
    {
        const dsmrTelegramField *field = &result->fields[index];
        const char *identifier = (field->name != NULL) ? field->name : field->obis;
        char valueText[32];

        dsmrFormatValue(&field->value, valueText, sizeof valueText);
        printf("%s = %s %s\r\n", identifier, valueText, (field->unit != NULL) ? field->unit : "");
    }
}
```

```c
//-- Three-column list: OBIS, application name when known, value.
static void printFieldList(const dsmrTelegramResult *result)
{
    for (uint16_t index = 0; index < result->fieldCount; index++)
    {
        const dsmrTelegramField *field = &result->fields[index];
        char valueText[32];

        dsmrFormatValue(&field->value, valueText, sizeof valueText);
        printf("%-12s %-24s %s %s\r\n",
               field->obis,
               (field->name != NULL) ? field->name : "-",
               valueText,
               (field->unit != NULL) ? field->unit : "");
    }
}
```

```c
//-- Fields without a registered label are reached by OBIS code.
const dsmrTelegramField *field;

if (dsmrResultFindFieldByObis(&result, "0-0:96.14.0", &field) == DSMR_OK)
{
    //-- Use field->value ...
}
```

The parser shall read the full OBIS key and its ordered parenthesized groups. A field definition shall specify the accepted syntax, datatype, units, precision, group count, and application meaning. The complete key and channel shall participate in lookup. Optional OBIS suffixes shall be normalized only where the source specification permits it. Unknown fields with valid syntax shall be skipped by default and may be exposed as bounded raw data. Duplicates shall be rejected unless the profile explicitly permits repetition. Ordinary data-line order shall be flexible.

| Data | Required interpretation |
|---|---|
| Identity, version, and timestamps | Header, equipment ID such as `0-0:96.1.1`, reported version, and meter timestamp `0-0:1.0.0` when defined. IDs shall retain leading zeroes. |
| Energy and active power | Tariff readings `1-0:1.8.1`, `1-0:1.8.2`, `1-0:2.8.1`, `1-0:2.8.2`, tariff indicator `0-0:96.14.0`, and instantaneous totals `1-0:1.7.0` and `1-0:2.7.0`. Aggregate counters `1-0:1.8.0` and `1-0:2.8.0` shall be decoded where published. Import and export shall remain separate. |
| Phase measurements | Profile-defined phase voltages, currents and directional active powers. Belgian current readings shall preserve fractional precision where specified. |
| Power quality and messages | Power failure and phase event counters, the structured `1-0:99.97.0` failure log where specified, and profile-defined message fields. The decoder shall validate record counts and treat message payloads as data. |
| M-Bus submeters | Submeters shall be identified by channel and device type (`0-n:24.1.0`) together with profile-defined identification objects. Dutch readings use `0-n:24.2.1`. Belgian gas uses `0-n:24.2.3`, and eMUCs 1.7.1 water uses `0-n:24.2.1`. The decoder shall associate records after all lines have been read, retain the measurement time per channel, and support the water, gas, thermal and other readings defined by the selected profile. The device type declared in the configuration shall label each channel and shall select the device-specific readings; a published `0-n:24.1.0` value that contradicts the declared type shall yield a mismatch diagnostic. |
| Belgian demand | `1-0:1.4.0` average demand, `1-0:1.6.0` current-month maximum with timestamp, and `0-0:98.1.0` history where defined. Demand shall have dedicated output fields. The field table shall include profile-specific limiters and other generation-specific objects. |

The project shall deliver a full OBIS inventory for each supported profile, including differences between document revisions that share a reported version. For each field it shall record the published representation, unit, presence conditions, output type, and fixture coverage.

Decimal values shall be converted without locale-dependent parsing. An integer mantissa and scale, or an exactly scaled integer, shall preserve the published precision and allow overflow checks. The result shall distinguish an absent value, a valid zero, and a value explicitly marked invalid by the protocol. Timestamps shall retain their source value and the `S`/`W` indicator when present. The parser shall validate calendar dates. Any conversion to UTC shall follow documented profile-specific time zone and century rules. A missing measurement timestamp shall remain absent.

## 6. Component structure and distribution

- The portable modules shall cover stream framing, CRC, OBIS field decoding, profile definitions, and the public API. The source layout shall allow a reader to follow the path from UART bytes to the result callback. The project shall include a short walkthrough of one telegram through that path.
- The ESP-IDF UART adapter shall configure baud rate, data bits, parity, stop bits, and flow control from the profile. UART port, RX pin, RX polarity, and optional request GPIO and polarity shall be configurable. It shall initialize reception before activating request and shall release owned resources on stop. It shall support an externally driven request signal. The chosen P1 interface board and its signal conditioning, physical inversion, and level shifting shall be documented.
- The first release shall provide the ESP-IDF component build, `CMakeLists.txt`, `idf_component.yml`, and two ESP32-S3 test programs. Instructions shall cover the VS Code ESP-IDF extension, `idf.py set-target esp32s3`, `idf.py build`, and `idf.py -p PORT flash monitor`.
- The stored-telegram test program shall verify the parser without a physical P1 connection. It shall run from the USB console with no P1 wiring, embed the accepted fixtures from the host test set, and feed them to the parser through `dsmrParserFeed` so the full decode path is exercised without any UART. It shall print every field of each accepted telegram, one OBIS code per line, and shall report each rejected candidate with its reason. It shall also demonstrate a dynamic configuration change, for example replacing a device type in the M-Bus device table, switching `profileId`, or editing the field label table through `dsmrParserApplyConfig` between telegrams.
- The live UART test program shall do nothing other than wait for telegrams on the P1 port UART and display the parsed data. For each accepted telegram it shall print one line per OBIS field, holding the OBIS code, the M-Bus channel where present, the value, and the unit, followed by a short per-telegram summary with the profile ID, reported version, meter identification, and CRC status. It shall contain no other application logic such as networking or storage.
- The Arduino release shall provide `library.properties`, the shared C sources and public headers under `src/`, and an ESP32 example using `HardwareSerial` or `Stream`. The public headers shall provide C linkage when included from a sketch. The ESP-IDF UART adapter shall be outside the Arduino compilation path. Arduino CLI shall verify the examples on an ESP32-S3 target.

## 7. Acceptance criteria

- Host tests shall compile the portable code as C11 without ESP-IDF. Each profile shall be exercised with complete single-phase and three-phase fixtures where the specification provides both, including matrix coverage of the Belgian revisions and both Dutch 4.0 reference documents. Source and revision shall be recorded for each fixture. Anonymized fixtures or published examples with placeholder CRCs shall have independently calculated trailers.
- The same valid frame shall produce the same result when fed all at once, one byte at a time, or split across arbitrary chunks. Tests shall cover multiple frames per buffer, noise, truncation, bad CRC, bad endings, timeout, oversize input, and UART overrun, with verification that the next valid frame is recovered.
- Tests shall cover exact country and profile matching, Dutch version values 40, 42 and 50, the Belgian version fields, unknown minor versions, missing versions, and absent and invalid CRC under each CRC policy.
- Field tests shall cover arbitrary data-line order, multiple M-Bus channels, valid zero versus missing fields, units, overflow, missing phases, time change boundaries, long messages, and valid and inconsistent log or demand-history counts.
- Both ESP-IDF test programs shall build for the ESP32-S3. The live UART path shall pass a meter or byte-accurate replay run of at least 30 minutes at 1 Hz without loss of valid telegrams. The test report shall identify the input source and record received and accepted counts, maximum parsing time, and memory use.
- A test profile shall demonstrate that a new field can be added without altering common framing or UART reception. Future country and revision additions shall include references, positive and negative fixtures, and an updated compatibility matrix.
- The stored-telegram test program shall build and run without a physical P1 connection, parse every embedded fixture, and print each accepted field on its own OBIS line. Every fixture shall be accepted, and each negative fixture shall yield its designated rejection reason.
- The live UART test program shall be limited to UART reception and the one line per OBIS display. A replay of a valid 1 Hz telegram stream shall show every field of every accepted telegram, including each M-Bus channel with its declared device type.
- Applications shall determine field names through the label table. The same OBIS shall accept different labels in different programs, a field without a label shall remain reachable by OBIS lookup, and a single loop over the received-field array shall print either the label or the OBIS together with the value. The three-column OBIS, name, and value list shall be produced from the same array.
- A configuration applied through `dsmrParserApplyConfig` shall govern the next telegram without reinitializing the parser. Changing the M-Bus device table shall add, relabel, or remove channels, and switching `countryCode` or `profileId` shall switch the resolved profile, field table, and defaults. A rejected configuration shall leave the previous configuration fully active, and a partial telegram in progress at the moment of reconfiguration shall be discarded with the next complete valid telegram accepted normally.

## 8. References

- [mrWheel/dsmr2Lib, specs at commit 5e7f07d](https://github.com/mrWheel/dsmr2Lib/tree/5e7f07dec4d5ac310e0005ad107e2dd53ea0e708/specs). P1 Companion Standards 3.0, 4.0.4, 4.0.7, 4.2.2 and 5.0.2, plus the release notes. The protocol, data object and example telegram chapters define the wire format.
- Netbeheer Nederland, [DSMR 4.2.2 Final P1](https://www.netbeheernederland.nl/publicatie/dsmr-v422-final-p1) and [DSMR 5.0.2 P1 Companion Standard](https://www.netbeheernederland.nl/publicatie/dsmr-502-p1-companion-standard).
- Fluvius, [P1 profiles by meter generation](https://partner.fluvius.be/nl/technische-documenten/gebruikerspoort-specificaties/p1-poort), [eMUCs 1.7.1](https://partner.fluvius.be/sites/fluvius/files/2025-09/smart-metering-system-emucs-p1-v1-7.pdf) and [eMUCs 2.1.1](https://partner.fluvius.be/sites/fluvius/files/2025-09/digital-metering-system-emucs-p1-v2-1.pdf).
- Luxembourg, [E-Meter P1 Specification 1.1.3](https://assets-global.website-files.com/656e09f6343bad59e7e40991/65ec35264e335e168a630d43_P1_Specification_Lux_V1.1.3_2021.pdf), §3.2 for transport and security.
- Energiföretagen Sverige, [Branschrekommendation för lokalt kundgränssnitt för elmätare v2.0](https://www.energiforetagen.se/globalassets/energiforetagen/det-erbjuder-vi/publikationer/branschrekommendation-lokalt-granssnitt-v2_0-201912.pdf), including the comparison with Norwegian HAN and the proposed data representation.
- Finnish Energy, [Principles of electricity metering 2024](https://energia.fi/wp-content/uploads/2025/03/Principles-of-electricity-metering-2024.pdf), §2.13.
- [ESP-IDF UART driver for ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32s3/api-reference/peripherals/uart.html) and [Arduino Library Specification](https://arduino.github.io/arduino-cli/latest/library-specification/).
