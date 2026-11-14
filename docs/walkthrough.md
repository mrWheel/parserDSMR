# One telegram through the parser

**Date:** 2026-11-14 · **Build:** 1

This walkthrough follows the fixture `F1 nl.dsmr.4.2 single-phase` from UART
bytes to the result callback. It is the reading path from UART bytes to the
result callback required by projectPrompt.md section 6.

## 1. Bytes arrive

The application (or the ESP-IDF adapter, which runs a receive task) hands a
chunk of bytes to `dsmrParserFeed`. The parser applies the inter-byte and
frame timeouts for the elapsed time, then walks the bytes one by one.

## 2. Framing — `src/dsmr_framing.c`

The framer is a readable state machine with the states of section 4 of the
specification:

- `SEEK_HEADER`: idle until a `/` arrives. The `/` starts a candidate; the
  bytes are stored and enter the running CRC.
- `IN_BODY`: every byte is stored and enters the CRC through the `!`
  inclusive. A frame that exceeds the configured telegram limit reports
  `DSMR_FRAMING_FRAME_TOO_LONG` and resumes seeking.
- `TRAILER_DIGITS`: the four hex digits after `!` are collected most
  significant nibble first. Under the required policy a missing CRC
  (`!` directly followed by `CR`) reports `TRAILER_MISSING_CRC`; anything
  else malformed reports `TRAILER_MALFORMED`.
- `TRAILER_CR` / `TRAILER_LF`: the trailer closes on `CR LF` (a bare `LF`
  only in the explicit LF compatibility mode). The last byte completes the
  frame.

For F1 the framer stores 496 bytes, the CRC runs over the first 491
(`/` through `!` inclusive), and the trailer digits `90A6` are collected.

## 3. CRC — `src/dsmr_crc.c`

CRC-16/ARC: initial `0x0000`, reflected polynomial `0xA001`, LSB-first, no
final XOR. The check value for `123456789` is `0xBB3D`, verified by the host
test. `dsmrParser` compares the running value with the received trailer
value. A mismatch yields the `DSMR_DIAG_CRC_MISMATCH` diagnostic; the
result distinguishes a verified CRC from an absent one per CRC policy.

## 4. Profile and version — `src/dsmr_profile.c`

The candidate's version field is located while the buffer is still intact.
For F1 that is `1-3:0.2.8(42)`. `dsmrProfileResolveForVersion` accepts the
value against the configured profile, or selects the concrete 4.0/4.2 table
for the automatic `nl.dsmr.4` profile. Missing or unaccepted versions yield
`DSMR_DIAG_MISSING_VERSION` or `DSMR_DIAG_INVALID_VERSION`.

## 5. Line decoding — `src/dsmr_decode.c`

The decoder validates the strict ASCII structure: the identification line
(becoming the meter identification), the blank line, then one data line per
CR LF. Each line is split into the OBIS key and its ordered parenthesized
groups. The OBIS key is resolved against the profile field blocks; `?`
matches the M-Bus channel digit (`0-?:24.1.0`).

Values are converted in `src/dsmr_value.c` without locale dependence:
decimals keep an integer mantissa and a scale so the published precision and
the distinction between absent, valid zero, and protocol-invalid (`FF`)
survive; timestamps retain the source text and the `S`/`W` indicator and
have their calendar fields validated. Units come from the profile field
table, the single place defining syntax, datatype and unit.

## 6. M-Bus association and labels

M-Bus readings carry the channel in the OBIS code (`0-1:24.2.1` has channel
1). The channel must be declared in the configuration device table; a
published `0-n:24.1.0` value that contradicts the declared type produces a
`DSMR_DIAG_MBUS_TYPE_MISMATCH` diagnostic alongside the accepted result,
and an undeclared channel follows the unknown-field policy. Every accepted
field is resolved against the application label table; fields without a
label remain reachable through `dsmrResultFindFieldByObis`.

## 7. The result callback

After the full trailer, CRC, version and field syntax are validated, one
`DSMR_EVENT_TELEGRAM` event carries the `dsmrTelegramResult`: profile ID,
reported version, meter identification, CRC status, receive time, and the
ordered array of received fields. Views point into the parser workspace and
stay valid for the duration of the callback; the application copies what it
keeps. The stored telegram example prints each field as one OBIS line and
each rejected candidate with its reason.
