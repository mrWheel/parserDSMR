#!/usr/bin/env python3
# parserDSMR - generator for the shared telegram fixture header.
# Date: 2026-11-14 · Build: 1
#
# Computes every trailer CRC independently of the C sources with a plain
# CRC-16/ARC implementation, then emits test/fixtures/dsmr_fixtures.h.
# Run from anywhere:  python3 -I generate_fixtures.py

import os

def crc16_arc(data: bytes) -> int:
    crc = 0x0000
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

def frame(body: str) -> str:
    """Complete a telegram with a computed trailer. The body may use bare LF
    or CRLF line endings; the result always uses CRLF."""
    payload = (body.replace("\r\n", "\n") + "!").replace("\n", "\r\n")
    crc = crc16_arc(payload.encode("ascii"))
    return payload + "%04X\r\n" % crc

def c_string(text: str, indent: str = "  ") -> str:
    lines = text.split("\r\n")
    parts = []
    for index, line in enumerate(lines):
        escaped = line.replace("\\", "\\\\").replace('"', '\\"')
        suffix = "\\r\\n" if index < len(lines) - 1 else ""
        if escaped == "" and suffix == "":
            continue
        parts.append('%s"%s%s"' % (indent, escaped, suffix))
    if not parts:
        parts.append('%s""' % indent)
    return "\n".join(parts)

# ---------------------------------------------------------------------------
# Accepted fixtures
# ---------------------------------------------------------------------------

f1_nl42_single = frame(
    "/EID18SE210031234\n"
    "\n"
    "1-3:0.2.8(42)\n"
    "0-0:96.1.1(1E2DC7000000A1B2C3D4E5F60718293A)\n"
    "0-0:1.0.0(180611100000S)\n"
    "1-0:1.8.1(000123.456*kWh)\n"
    "1-0:1.8.2(000000.000*kWh)\n"
    "1-0:2.8.1(000012.345*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "0-0:96.14.0(0001)\n"
    "1-0:1.7.0(000.123*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "0-0:96.7.21(00004)\n"
    "0-0:96.7.9(00002)\n"
    "1-0:99.97.0(2)(0-0:96.7.19)(180611235959S)(0000000002)(1)(2)(180612085859S)(0000000003)(2)(2)\n"
    "0-0:96.13.1()\n"
    "0-0:96.13.0()\n"
    "0-1:24.1.0(001)\n"
    "0-1:24.2.1(000012.345*m3)\n"
)

f2_nl50_three = frame(
    "/EID18SE210039876\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(1E2DC7000000B1B2C3D4E5F60718293B)\n"
    "0-0:1.0.0(180611100100S)\n"
    "1-0:1.8.0(000200.000*kWh)\n"
    "1-0:1.8.1(000100.000*kWh)\n"
    "1-0:1.8.2(000100.000*kWh)\n"
    "1-0:2.8.0(000050.000*kWh)\n"
    "1-0:2.8.1(000050.000*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "0-0:96.14.0(0002)\n"
    "1-0:1.7.0(001.234*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:21.7.0(000.100*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:23.7.0(000.050*kW)\n"
    "1-0:24.7.0(000.000*kW)\n"
    "1-0:25.7.0(000.080*kW)\n"
    "1-0:26.7.0(000.000*kW)\n"
    "1-0:32.8.0(229.5*V)\n"
    "1-0:52.8.0(230.1*V)\n"
    "1-0:72.8.0(228.9*V)\n"
    "1-0:31.7.0(000.5*A)\n"
    "1-0:51.7.0(000.2*A)\n"
    "1-0:71.7.0(000.4*A)\n"
    "0-0:96.7.21(00007)\n"
    "0-0:96.7.9(00001)\n"
    "0-1:24.1.0(001)\n"
    "0-1:24.2.1(000123.456*m3)\n"
    "0-1:24.3.1(180611230000S)\n"
    "0-2:24.1.0(002)\n"
    "0-2:24.2.1(000045.678*m3)\n"
    "0-2:24.3.1(180611230000S)\n"
)

f3_nl40_single = frame(
    "/ISk5\\2M530E1234567\n"
    "\n"
    "1-3:0.2.8(40)\n"
    "0-0:96.1.1(45334C33204330303030313233343536)\n"
    "0-0:1.0.0(180611100000S)\n"
    "1-0:1.8.1(000001.234*kWh)\n"
    "1-0:2.8.1(000000.000*kWh)\n"
    "0-0:96.14.0(0001)\n"
    "1-0:1.7.0(000.000*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "0-1:24.1.0(001)\n"
    "0-1:24.2.1(000000.123*m3)\n"
)

f4_be21_three = frame(
    "/52344C33204330303030313233\n"
    "\n"
    "0-0:96.1.4(50221)\n"
    "0-0:96.1.1(3153453231303030313233343536AB)\n"
    "0-0:1.0.0(180611100000+0100)\n"
    "1-0:1.8.1(000100.000*kWh)\n"
    "1-0:1.8.2(000050.000*kWh)\n"
    "1-0:2.8.1(000010.000*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "1-0:1.4.0(000.456*kW)\n"
    "1-0:1.6.0(003.210*kW)(180611120000+0100)\n"
    "0-0:98.1.0(1)(123456)(180611120000+0100)\n"
    "0-0:96.14.0(0001)\n"
    "1-0:1.7.0(001.234*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:31.7.0(000.54*A)\n"
    "1-0:51.7.0(000.21*A)\n"
    "1-0:71.7.0(000.43*A)\n"
    "1-0:32.8.0(230.2*V)\n"
    "1-0:52.8.0(229.8*V)\n"
    "1-0:72.8.0(231.1*V)\n"
    "0-1:24.1.0(001)\n"
    "0-1:24.2.3(000078.912*m3)\n"
    "0-1:24.3.1(180611230000+0100)\n"
)

f5_be1x_single = frame(
    "/52344C33204330303030393939\n"
    "\n"
    "0-0:96.1.4(50217)\n"
    "0-0:96.1.1(315345323130303039393938383736AB)\n"
    "0-0:1.0.0(180611100000+0100)\n"
    "1-0:1.8.1(000200.000*kWh)\n"
    "1-0:2.8.1(000020.000*kWh)\n"
    "0-0:96.14.0(0002)\n"
    "1-0:1.7.0(000.500*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
)

# ---------------------------------------------------------------------------
# Rejected fixtures, derived from the accepted set
# ---------------------------------------------------------------------------

def replace_crc(text: str, transform) -> str:
    head, crc = text.rsplit("!", 1)
    crc_digits = crc[:4]
    return head + "!" + transform(crc_digits) + "\r\n"

f6_bad_crc = replace_crc(f1_nl42_single, lambda c: c[:-1] + ("0" if c[-1] != "0" else "1"))

f7_truncated = (
    "/EID18SE210031234\r\n"
    "\r\n"
    "1-3:0.2.8(42)\r\n"
    "1-0:1.8.1(000123.456*kWh)\r\n"
    "1-0:1.7.0(000.123*kW)\r\n"
)

f8_no_version = (
    "/EID18SE210031234\r\n"
    "\r\n"
    "1-0:1.8.1(000123.456*kWh)\r\n"
    "1-0:1.7.0(000.123*kW)\r\n"
)
f8_no_version = frame(f8_no_version.replace("\r\n", "\n"))

f9_malformed_trailer = replace_crc(f1_nl42_single, lambda c: "ZZZZ")

f10_missing_crc = f1_nl42_single.rsplit("!", 1)[0] + "!\r\n"

f11_noise_prefix = "\r\nboot noise !!??\r\n" + f1_nl42_single

f12_duplicate = (
    "/EID18SE210031234\n"
    "\n"
    "1-3:0.2.8(42)\n"
    "1-0:1.8.1(000123.456*kWh)\n"
    "1-0:1.8.1(000123.999*kWh)\n"
)
f12_duplicate = frame(f12_duplicate.replace("\r\n", "\n"))

f13_body = f1_nl42_single.rsplit("!", 1)[0].replace("1-3:0.2.8(42)", "1-3:0.2.8(43)")
f13_unknown_minor = frame(f13_body)

# ---------------------------------------------------------------------------
# Emission
# ---------------------------------------------------------------------------

NL_SOURCE = "Synthetic single-phase telegram structured after the example " \
            "style of the DSMR 4.2.2 P1 Companion Standard; anonymized data, " \
            "trailer CRC computed with CRC-16/ARC."
NL50_SOURCE = "Synthetic three-phase telegram structured after the object " \
              "tables of the DSMR 5.0.2 P1 Companion Standard; anonymized " \
              "data, trailer CRC computed with CRC-16/ARC."
NL40_SOURCE = "Synthetic single-phase telegram structured after the object " \
              "tables of the DSMR 4.0.4 and 4.0.7 P1 Companion Standards; " \
              "anonymized data, trailer CRC computed with CRC-16/ARC."
BE21_SOURCE = "Synthetic three-phase telegram structured after the object " \
              "tables of the Fluvius eMUCs 2.1.1 document; anonymized data, " \
              "trailer CRC computed with CRC-16/ARC. Pending verification " \
              "against the published document."
BE1X_SOURCE = "Synthetic single-phase telegram structured after the object " \
              "tables of the Fluvius eMUCs 1.7.1 document; anonymized data, " \
              "trailer CRC computed with CRC-16/ARC. Pending verification " \
              "against the published document."
DERIVED_SOURCE = "Derived from the DSMR 4.2.2 style fixture by the edit " \
                 "named in the identifier; trailer handling as described by " \
                 "the negative case."

HEADER = '''//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: test/fixtures/dsmr_fixtures.h - shared telegram fixture set
//-- Date: 2026-11-14 · Build: 1
//--
//-- Generated by test/fixtures/tools/generate_fixtures.py. The trailer CRC of
//-- every accepted fixture is computed with an independent CRC-16/ARC
//-- implementation. The same header feeds the host tests and the stored
//-- telegram test program, so both exercise the identical byte stream.
//-- Source and revision are recorded with each fixture as required by
//-- projectPrompt.md section 7.
//-- ==========================================================================

#ifndef DSMR_FIXTURES_H
#define DSMR_FIXTURES_H

#include "dsmr_parser.h"

typedef struct
{
  const char *id;
  const char *source;
  const char *countryCode;
  const char *profileId;
  const char *telegram;
  const dsmrMbusDevice *mbusDevices;
  uint8_t mbusDeviceCount;
  bool expectAccepted;
  dsmrDiagReason expectedReason;
  bool needsTick;
} dsmrFixture;

'''

def emit(name, text):
    return "static const char %s[] =\n%s;\n\n" % (name, c_string(text))

def emit_devices(name, devices):
    if not devices:
        return ""
    entries = ", ".join("{ .channel = %d, .deviceType = %s }" % (ch, dtype) for ch, dtype in devices)
    return "static const dsmrMbusDevice %s[] = { %s };\n\n" % (name, entries)

rows = [
  ("fixtureF1", "f1Nl42SinglePhase", "F1 nl.dsmr.4.2 single-phase", NL_SOURCE, "NL", "nl.dsmr.4.2", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF2", "f2Nl50ThreePhase", "F2 nl.dsmr.5.0 three-phase", NL50_SOURCE, "NL", "nl.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS"), (2, "DSMR_MBUS_DEVICE_WATER")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF3", "f3Nl40SinglePhase", "F3 nl.dsmr.4.0 single-phase", NL40_SOURCE, "NL", "nl.dsmr.4.0", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF4", "f4Be21ThreePhase", "F4 be.emucs.2.1 three-phase", BE21_SOURCE, "BE", "be.emucs.2.1", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF5", "f5Be1xSinglePhase", "F5 be.emucs.1.x single-phase", BE1X_SOURCE, "BE", "be.emucs.1.x", None, True, "DSMR_DIAG_NONE", False),
  ("fixtureF6", "f6BadCrc", "F6 bad CRC", DERIVED_SOURCE, "NL", "nl.dsmr.4.2", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_CRC_MISMATCH", False),
  ("fixtureF7", "f7Truncated", "F7 truncated frame", DERIVED_SOURCE, "NL", "nl.dsmr.4.2", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_TIMEOUT_INTER_BYTE", True),
  ("fixtureF8", "f8NoVersion", "F8 missing version field", DERIVED_SOURCE, "NL", "nl.dsmr.4.2", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_MISSING_VERSION", False),
  ("fixtureF9", "f9MalformedTrailer", "F9 malformed trailer", DERIVED_SOURCE, "NL", "nl.dsmr.4.2", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_MALFORMED_TRAILER", False),
  ("fixtureF10", "f10MissingCrc", "F10 absent CRC under required policy", DERIVED_SOURCE, "NL", "nl.dsmr.4.2", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_MISSING_CRC", False),
  ("fixtureF11", "f11NoisePrefix", "F11 noise before a valid header", DERIVED_SOURCE, "NL", "nl.dsmr.4.2", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF12", "f12DuplicateField", "F12 duplicated data line", DERIVED_SOURCE, "NL", "nl.dsmr.4.2", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_DUPLICATE_FIELD", False),
  ("fixtureF13", "f13UnknownMinorVersion", "F13 unknown minor version 43", DERIVED_SOURCE, "NL", "nl.dsmr.4.2", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_INVALID_VERSION", False),
]

texts = {
  "fixtureF1": f1_nl42_single,
  "fixtureF2": f2_nl50_three,
  "fixtureF3": f3_nl40_single,
  "fixtureF4": f4_be21_three,
  "fixtureF5": f5_be1x_single,
  "fixtureF6": f6_bad_crc,
  "fixtureF7": f7_truncated,
  "fixtureF8": f8_no_version,
  "fixtureF9": f9_malformed_trailer,
  "fixtureF10": f10_missing_crc,
  "fixtureF11": f11_noise_prefix,
  "fixtureF12": f12_duplicate,
  "fixtureF13": f13_unknown_minor,
}

out = [HEADER]

for var, _, _, _, _, _, devices, _, _, _ in rows:
    out.append(emit_devices(var + "Devices", devices))
    out.append(emit(var, texts[var]))

out.append("static const dsmrFixture dsmrFixtures[] =\n{\n")
for var, _, label, source, country, profile, devices, accepted, reason, tick in rows:
    device_ptr = ("%sDevices" % var) if devices else "NULL"
    device_count = len(devices) if devices else 0
    out.append("  {\n")
    out.append("    .id = \"%s\",\n" % label)
    out.append("    .source = \"%s\",\n" % source)
    out.append("    .countryCode = \"%s\",\n" % country)
    out.append("    .profileId = \"%s\",\n" % profile)
    out.append("    .telegram = %s,\n" % var)
    out.append("    .mbusDevices = %s,\n" % device_ptr)
    out.append("    .mbusDeviceCount = %d,\n" % device_count)
    out.append("    .expectAccepted = %s,\n" % ("true" if accepted else "false"))
    out.append("    .expectedReason = %s,\n" % reason)
    out.append("    .needsTick = %s,\n" % ("true" if tick else "false"))
    out.append("  },\n")
out.append("};\n\n")
out.append("#define DSMR_FIXTURE_COUNT ((uint16_t)(sizeof dsmrFixtures / sizeof dsmrFixtures[0]))\n\n")
out.append("#endif //-- DSMR_FIXTURES_H\n")

target = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "dsmr_fixtures.h")
with open(target, "w", encoding="utf-8") as handle:
    handle.write("".join(out))

print("wrote", os.path.normpath(target))
