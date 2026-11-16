#!/usr/bin/env python3
# parserDSMR - generator for the shared telegram fixture set.
# Date: 2026-11-16 · Build: 3
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
# Automatic Dutch profile and European country fixtures
#
# Structured after the documents in assets/: the E.ON Hungaria P1 port
# user information leaflet 2024-07-02, the Luxmetering E-Meter P1
# Specification 1.1.3 (its Appendix A clear-text example telegram) and the
# VSE RL-DSP CH 2024 Anhang 3 minimum data points, cross-checked against
# the European P1 OBIS overview. The M-Bus channels below publish the
# EN 13757-3 device numbering printed in the overview and the Luxembourg
# example (3=gas, 4=heat, 7=water); the DSMR numbering stays accepted.
# ---------------------------------------------------------------------------

# F14 exercises the automatic nl.dsmr.4 profile with the DSMR 4.2 telegram.
f14_nl4_auto = f1_nl42_single

# --- Hungary (hu.dsmr.5.0) --------------------------------------------------

f15_hu_three = frame(
    "/EON5HU12345678\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(454F4E3132333435363738)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:42.0.0(EON HU MVM)\n"
    "0-0:96.1.0(HU31234567)\n"
    "0-0:96.14.0(0001)\n"
    "0-0:96.13.0(Szamlaadas rendben)\n"
    "1-0:0.9.1(104500)\n"
    "1-0:0.9.2(251116)\n"
    "1-0:1.8.0(000215.000*kWh)\n"
    "1-0:1.8.1(000100.000*kWh)\n"
    "1-0:1.8.2(000050.000*kWh)\n"
    "1-0:1.8.3(000025.000*kWh)\n"
    "1-0:1.8.4(000040.000*kWh)\n"
    "1-0:2.8.0(000020.000*kWh)\n"
    "1-0:2.8.1(000010.000*kWh)\n"
    "1-0:2.8.2(000005.000*kWh)\n"
    "1-0:2.8.3(000003.000*kWh)\n"
    "1-0:2.8.4(000002.000*kWh)\n"
    "1-0:15.8.0(000235.000*kWh)\n"
    "1-0:3.8.0(000015.000*kVArh)\n"
    "1-0:4.8.0(000012.000*kVArh)\n"
    "1-0:3.8.1(000010.000*kVArh)\n"
    "1-0:3.8.2(000005.000*kVArh)\n"
    "1-0:4.8.1(000008.000*kVArh)\n"
    "1-0:4.8.2(000004.000*kVArh)\n"
    "1-0:5.8.0(000001.000*kVArh)\n"
    "1-0:6.8.0(000002.000*kVArh)\n"
    "1-0:7.8.0(000003.000*kVArh)\n"
    "1-0:8.8.0(000004.000*kVArh)\n"
    "1-0:1.7.0(001.234*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:3.7.0(000.150*kVAr)\n"
    "1-0:4.7.0(000.070*kVAr)\n"
    "1-0:5.7.0(000.100*kVAr)\n"
    "1-0:6.7.0(000.020*kVAr)\n"
    "1-0:7.7.0(000.030*kVAr)\n"
    "1-0:8.7.0(000.040*kVAr)\n"
    "1-0:21.7.0(000.500*kW)\n"
    "1-0:41.7.0(000.400*kW)\n"
    "1-0:61.7.0(000.334*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:42.7.0(000.000*kW)\n"
    "1-0:62.7.0(000.000*kW)\n"
    "1-0:23.7.0(000.040*kVAr)\n"
    "1-0:43.7.0(000.050*kVAr)\n"
    "1-0:63.7.0(000.060*kVAr)\n"
    "1-0:24.7.0(000.010*kVAr)\n"
    "1-0:44.7.0(000.010*kVAr)\n"
    "1-0:64.7.0(000.010*kVAr)\n"
    "1-0:31.7.0(002.500*A)\n"
    "1-0:51.7.0(002.000*A)\n"
    "1-0:71.7.0(001.500*A)\n"
    "1-0:32.7.0(230.1*V)\n"
    "1-0:52.7.0(229.8*V)\n"
    "1-0:72.7.0(231.2*V)\n"
    "1-0:14.7.0(50.012*Hz)\n"
    "1-0:13.7.0(0.980)\n"
    "1-0:33.7.0(0.990)\n"
    "1-0:53.7.0(0.980)\n"
    "1-0:73.7.0(0.970)\n"
    "1-0:91.7.0(000.050*A)\n"
    "1-0:31.4.0(063.000*A)\n"
    "1-0:51.4.0(063.000*A)\n"
    "1-0:71.4.0(063.000*A)\n"
    "0-0:17.0.0(025.000*kW)\n"
    "0-0:96.50.68(1)\n"
    "0-0:96.3.10(1)\n"
    "0-0:96.5.0(0000)\n"
    "0-0:97.97.0()\n"
    "0-0:96.11.0(0)\n"
    "1-0:32.32.0(00004)\n"
    "1-0:32.36.0(00001)\n"
    "0-0:96.7.21(00004)\n"
    "0-0:96.7.9(00001)\n"
    "1-0:99.97.0(2)(0-0:96.7.19)(251115235959S)(0000000002)(1)(2)(251116085859S)(0000000003)(2)(2)\n"
    "0-1:24.1.0(003)\n"
    "0-1:96.1.0(5641533132333435363738)\n"
    "0-1:96.1.1(5641533132333435363738)\n"
    "0-1:24.2.1(000123.456*m3)\n"
    "0-1:24.3.1(251116104500S)\n"
    "0-1:24.4.0(1)\n"
    "0-0:96.7.22(00001)\n"
)

f16_hu_single = frame(
    "/EON5HU87654321\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(454F4E3837363534333231)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:96.1.0(HU87654321)\n"
    "0-0:96.14.0(0002)\n"
    "1-0:1.8.0(000321.000*kWh)\n"
    "1-0:1.8.1(000200.000*kWh)\n"
    "1-0:1.8.2(000121.000*kWh)\n"
    "1-0:2.8.0(000005.000*kWh)\n"
    "1-0:2.8.1(000005.000*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "1-0:15.8.0(000326.000*kWh)\n"
    "1-0:1.7.0(000.456*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:21.7.0(000.456*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:31.7.0(002.000*A)\n"
    "1-0:32.7.0(230.5*V)\n"
    "1-0:14.7.0(49.998*Hz)\n"
    "1-0:13.7.0(0.995)\n"
    "0-0:96.3.10(1)\n"
    "0-0:17.0.0(020.000*kW)\n"
    "0-0:96.7.21(00002)\n"
    "0-1:24.1.0(003)\n"
    "0-1:24.2.1(000045.678*m3)\n"
)

f17_body = f15_hu_three.rsplit("!", 1)[0].replace("1-3:0.2.8(50)", "1-3:0.2.8(51)")
f17_hu_bad_version = frame(f17_body)

# --- Sweden (se.dsmr.5.0) ----------------------------------------------------

f18_se_three = frame(
    "/ISB5SE12345678\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(4953425345313233343536)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:96.14.0(0001)\n"
    "0-0:96.13.0()\n"
    "1-0:1.8.0(001234.567*kWh)\n"
    "1-0:1.8.1(000800.000*kWh)\n"
    "1-0:1.8.2(000434.567*kWh)\n"
    "1-0:2.8.0(000012.345*kWh)\n"
    "1-0:2.8.1(000012.345*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "1-0:3.8.0(000002.000*kVArh)\n"
    "1-0:4.8.0(000001.000*kVArh)\n"
    "1-0:1.7.0(002.345*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:3.7.0(000.100*kVAr)\n"
    "1-0:4.7.0(000.050*kVAr)\n"
    "1-0:21.7.0(001.000*kW)\n"
    "1-0:41.7.0(000.800*kW)\n"
    "1-0:61.7.0(000.545*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:42.7.0(000.000*kW)\n"
    "1-0:62.7.0(000.000*kW)\n"
    "1-0:23.7.0(000.030*kVAr)\n"
    "1-0:43.7.0(000.040*kVAr)\n"
    "1-0:63.7.0(000.030*kVAr)\n"
    "1-0:24.7.0(000.010*kVAr)\n"
    "1-0:44.7.0(000.020*kVAr)\n"
    "1-0:64.7.0(000.020*kVAr)\n"
    "1-0:31.7.0(004.500*A)\n"
    "1-0:51.7.0(003.500*A)\n"
    "1-0:71.7.0(002.500*A)\n"
    "1-0:32.7.0(230.4*V)\n"
    "1-0:52.7.0(231.0*V)\n"
    "1-0:72.7.0(229.7*V)\n"
    "1-0:32.32.0(00001)\n"
    "1-0:32.36.0(00000)\n"
    "0-0:96.7.21(00003)\n"
    "0-0:96.7.9(00000)\n"
    "1-0:99.97.0(2)(0-0:96.7.19)(251115235959S)(0000000002)(1)(2)(251116085859S)(0000000003)(2)(2)\n"
    "0-0:96.5.0(0000)\n"
    "0-0:97.97.0()\n"
    "0-1:24.1.0(004)\n"
    "0-1:96.1.1(484541543132333435)\n"
    "0-1:24.2.1(000098.765*m3)\n"
    "0-1:24.3.1(251116104500S)\n"
    "0-2:24.1.0(007)\n"
    "0-2:96.1.1(574154313233343536)\n"
    "0-2:24.2.1(000012.345*m3)\n"
    "0-2:24.3.1(251116104500S)\n"
    "0-0:96.7.22(00002)\n"
)

f19_se_single = frame(
    "/ISB5SE87654321\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(49534253453837363534)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:96.14.0(0002)\n"
    "0-0:96.13.0()\n"
    "1-0:1.8.0(000500.000*kWh)\n"
    "1-0:1.8.1(000300.000*kWh)\n"
    "1-0:1.8.2(000200.000*kWh)\n"
    "1-0:2.8.0(000001.000*kWh)\n"
    "1-0:2.8.1(000001.000*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "1-0:3.8.0(000000.500*kVArh)\n"
    "1-0:4.8.0(000000.250*kVArh)\n"
    "1-0:1.7.0(000.750*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:3.7.0(000.050*kVAr)\n"
    "1-0:4.7.0(000.020*kVAr)\n"
    "1-0:21.7.0(000.750*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:31.7.0(003.200*A)\n"
    "1-0:32.7.0(230.0*V)\n"
    "1-0:32.32.0(00000)\n"
    "0-0:96.7.21(00001)\n"
    "0-0:96.7.9(00000)\n"
    "0-0:96.5.0(0000)\n"
    "0-1:24.1.0(004)\n"
    "0-1:24.2.1(000012.000*m3)\n"
    "0-1:24.3.1(251116104500S)\n"
)

f20_se_bad_crc = replace_crc(f18_se_three, lambda c: c[:-1] + ("0" if c[-1] != "0" else "1"))

# --- Finland (fi.dsmr.5.0) ---------------------------------------------------

f21_fi_three = frame(
    "/AID5FI12345678\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(41494446493132333435)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:96.14.0(0001)\n"
    "0-0:96.13.0()\n"
    "1-0:1.8.0(002345.000*kWh)\n"
    "1-0:1.8.1(001500.000*kWh)\n"
    "1-0:1.8.2(000845.000*kWh)\n"
    "1-0:2.8.0(000023.000*kWh)\n"
    "1-0:2.8.1(000023.000*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "1-0:3.8.0(000003.000*kVArh)\n"
    "1-0:4.8.0(000001.000*kVArh)\n"
    "1-0:1.7.0(003.000*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:3.7.0(000.200*kVAr)\n"
    "1-0:4.7.0(000.100*kVAr)\n"
    "1-0:21.7.0(001.200*kW)\n"
    "1-0:41.7.0(001.000*kW)\n"
    "1-0:61.7.0(000.800*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:42.7.0(000.000*kW)\n"
    "1-0:62.7.0(000.000*kW)\n"
    "1-0:23.7.0(000.060*kVAr)\n"
    "1-0:43.7.0(000.070*kVAr)\n"
    "1-0:63.7.0(000.070*kVAr)\n"
    "1-0:24.7.0(000.030*kVAr)\n"
    "1-0:44.7.0(000.030*kVAr)\n"
    "1-0:64.7.0(000.040*kVAr)\n"
    "1-0:31.7.0(005.500*A)\n"
    "1-0:51.7.0(004.500*A)\n"
    "1-0:71.7.0(003.500*A)\n"
    "1-0:32.7.0(230.6*V)\n"
    "1-0:52.7.0(229.9*V)\n"
    "1-0:72.7.0(231.4*V)\n"
    "1-0:32.32.0(00002)\n"
    "1-0:32.36.0(00000)\n"
    "0-0:96.7.21(00002)\n"
    "0-0:96.7.9(00000)\n"
    "1-0:99.97.0(2)(0-0:96.7.19)(251115235959S)(0000000002)(1)(2)(251116085859S)(0000000003)(2)(2)\n"
    "0-0:97.97.0()\n"
    "0-1:24.1.0(003)\n"
)

f22_fi_single = frame(
    "/AID5FI87654321\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(41494446493837363534)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:96.14.0(0002)\n"
    "0-0:96.13.0()\n"
    "1-0:1.8.0(000250.000*kWh)\n"
    "1-0:1.8.1(000150.000*kWh)\n"
    "1-0:1.8.2(000100.000*kWh)\n"
    "1-0:2.8.0(000000.500*kWh)\n"
    "1-0:2.8.1(000000.500*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "1-0:1.7.0(000.345*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:21.7.0(000.345*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:31.7.0(001.500*A)\n"
    "1-0:32.7.0(229.5*V)\n"
    "0-0:96.7.21(00000)\n"
    "0-1:24.1.0(003)\n"
)

f23_body = f22_fi_single.rsplit("!", 1)[0].replace("1-3:0.2.8(50)\r\n", "")
f23_fi_missing_version = frame(f23_body)

# --- Lithuania (lt.dsmr.5.0) -------------------------------------------------

f24_lt_three = frame(
    "/EGY5LT12345678\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(45475231323334353637)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:96.14.0(0001)\n"
    "1-0:1.8.0(000987.654*kWh)\n"
    "1-0:1.8.1(000600.000*kWh)\n"
    "1-0:1.8.2(000387.654*kWh)\n"
    "1-0:2.8.0(000032.100*kWh)\n"
    "1-0:2.8.1(000032.100*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "1-0:3.8.0(000004.000*kVArh)\n"
    "1-0:4.8.0(000002.000*kVArh)\n"
    "1-0:3.8.1(000003.000*kVArh)\n"
    "1-0:3.8.2(000001.000*kVArh)\n"
    "1-0:4.8.1(000002.000*kVArh)\n"
    "1-0:4.8.2(000000.000*kVArh)\n"
    "1-0:1.7.0(001.111*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:21.7.0(000.500*kW)\n"
    "1-0:41.7.0(000.300*kW)\n"
    "1-0:61.7.0(000.311*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:42.7.0(000.000*kW)\n"
    "1-0:62.7.0(000.000*kW)\n"
    "1-0:31.7.0(005.000*A)\n"
    "1-0:51.7.0(004.000*A)\n"
    "1-0:71.7.0(003.000*A)\n"
    "1-0:32.7.0(229.9*V)\n"
    "1-0:52.7.0(230.6*V)\n"
    "1-0:72.7.0(231.3*V)\n"
    "0-0:96.3.10(1)\n"
    "0-0:96.5.0(0F0F)\n"
    "0-0:96.10.1(00000001)\n"
    "0-0:96.11.0(0000)\n"
    "0-0:96.7.21(00001)\n"
    "0-0:96.7.9(00000)\n"
    "0-1:24.1.0(003)\n"
    "0-1:96.1.1(474153313233343536)\n"
    "0-1:24.2.1(000555.555*m3)\n"
    "0-1:24.3.1(251116104500S)\n"
    "0-2:24.1.0(007)\n"
    "0-2:96.1.1(5741544552313233)\n"
    "0-2:24.2.1(000077.777*m3)\n"
    "0-2:24.3.1(251116104500S)\n"
)

f25_lt_single = frame(
    "/EGY5LT87654321\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(45475238373635343332)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:96.14.0(0002)\n"
    "1-0:1.8.0(000111.000*kWh)\n"
    "1-0:1.8.1(000060.000*kWh)\n"
    "1-0:1.8.2(000051.000*kWh)\n"
    "1-0:2.8.0(000001.000*kWh)\n"
    "1-0:2.8.1(000001.000*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "1-0:3.8.0(000000.500*kVArh)\n"
    "1-0:4.8.0(000000.250*kVArh)\n"
    "1-0:3.8.1(000000.500*kVArh)\n"
    "1-0:4.8.1(000000.250*kVArh)\n"
    "1-0:1.7.0(000.234*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:21.7.0(000.234*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:31.7.0(001.000*A)\n"
    "1-0:32.7.0(230.2*V)\n"
    "0-0:96.3.10(0)\n"
    "0-0:96.5.0(0000)\n"
    "0-0:96.10.1(00000000)\n"
    "0-0:96.11.0(0)\n"
    "0-1:24.1.0(003)\n"
    "0-1:24.2.1(000022.222*m3)\n"
    "0-1:24.3.1(251116104500S)\n"
)

f26_lt_malformed = replace_crc(f25_lt_single, lambda c: "ZZZZ")

# --- Ireland (ie.dsmr.5.0) ---------------------------------------------------

f27_ie_three = frame(
    "/ESB5IE12345678\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(45534249453132333435)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:96.14.0(0001)\n"
    "1-0:1.8.1(000700.000*kWh)\n"
    "1-0:1.8.2(000300.000*kWh)\n"
    "1-0:2.8.1(000015.000*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "1-0:1.7.0(002.000*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:21.7.0(000.900*kW)\n"
    "1-0:41.7.0(000.700*kW)\n"
    "1-0:61.7.0(000.400*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:42.7.0(000.000*kW)\n"
    "1-0:62.7.0(000.000*kW)\n"
    "1-0:31.7.0(004.000*A)\n"
    "1-0:51.7.0(003.000*A)\n"
    "1-0:71.7.0(002.000*A)\n"
    "1-0:32.7.0(230.8*V)\n"
    "1-0:52.7.0(230.1*V)\n"
    "1-0:72.7.0(229.6*V)\n"
    "0-0:96.10.1(00000000)\n"
    "0-1:24.1.0(003)\n"
)

f28_ie_single = frame(
    "/ESB5IE87654321\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(45534249453837363534)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:96.14.0(0002)\n"
    "1-0:1.8.1(000400.000*kWh)\n"
    "1-0:1.8.2(000100.000*kWh)\n"
    "1-0:2.8.1(000002.000*kWh)\n"
    "1-0:2.8.2(000000.000*kWh)\n"
    "1-0:1.7.0(000.690*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:21.7.0(000.690*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "1-0:31.7.0(003.000*A)\n"
    "1-0:32.7.0(231.0*V)\n"
    "0-0:96.10.1(00000000)\n"
    "0-1:24.1.0(003)\n"
)

f29_ie_duplicate = frame(
    "/ESB5IE87654321\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "1-0:1.8.1(000400.000*kWh)\n"
    "1-0:1.8.1(000400.500*kWh)\n"
)

# --- Luxembourg (lu.dsmr.5.0) ------------------------------------------------
# F30 and F32 adapt the clear-text example telegram of the Luxmetering
# E-Meter P1 Specification 1.1.3 Appendix A. F32 keeps the padding
# timestamps (month 25) of the channels without an M-Bus device to show
# that calendar validation rejects them; F30 keeps only the valid rows.

f30_lu_three = frame(
    "/Lux5253694471_M\n"
    "\n"
    "1-3:0.2.8(42)\n"
    "0-0:1.0.0(200706104157S)\n"
    "0-0:42.0.0(53414731303330373930303032353734)\n"
    "1-0:1.8.0(000025.653*kWh)\n"
    "1-0:2.8.0(000000.040*kWh)\n"
    "1-0:3.8.0(000000.835*kvarh)\n"
    "1-0:4.8.0(0000063.781*kvarh)\n"
    "1-0:1.7.0(00.005*kW)\n"
    "1-0:2.7.0(00.000*kW)\n"
    "1-0:3.7.0(00.000*kvar)\n"
    "1-0:4.7.0(00.000*kvar)\n"
    "0-0:17.0.0(069.0*kVA)\n"
    "1-0:9.7.0(00.021*kVA)\n"
    "1-0:10.7.0(00.000*kVA)\n"
    "0-0:96.3.10(1)\n"
    "0-0:96.7.21(00099)\n"
    "1-0:32.32.0(00040)\n"
    "1-0:52.32.0(00003)\n"
    "1-0:72.32.0(00002)\n"
    "1-0:32.36.0(00000)\n"
    "1-0:52.36.0(00000)\n"
    "1-0:72.36.0(00000)\n"
    "0-0:96.13.0(0)\n"
    "1-0:32.7.0(233.0*V)\n"
    "1-0:52.7.0(000.0*V)\n"
    "1-0:72.7.0(001.0*V)\n"
    "1-0:31.7.0(000*A)\n"
    "1-0:51.7.0(000*A)\n"
    "1-0:71.7.0(000*A)\n"
    "1-0:21.7.0(00.005*kW)\n"
    "1-0:41.7.0(00.000*kW)\n"
    "1-0:61.7.0(00.000*kW)\n"
    "1-0:22.7.0(00.000*kW)\n"
    "1-0:42.7.0(00.000*kW)\n"
    "1-0:62.7.0(00.000*kW)\n"
    "1-0:23.7.0(00.000*kvar)\n"
    "1-0:43.7.0(00.000*kvar)\n"
    "1-0:63.7.0(00.000*kvar)\n"
    "1-0:24.7.0(00.000*kvar)\n"
    "1-0:44.7.0(00.000*kvar)\n"
    "1-0:64.7.0(00.000*kvar)\n"
    "0-1:24.1.0(003)\n"
    "0-1:96.1.0(464C4F318393930303630333535)\n"
    "0-1:24.2.1(200706103140S)(00000.006*m3)\n"
    "0-1:24.4.0(0)\n"
    "0-4:24.1.0(003)\n"
    "0-4:96.1.0(454C53333533353839393830333030)\n"
    "0-4:24.2.1(200706102900S)(00028.103*m3)\n"
    "0-4:24.4.0(1)\n"
)

f31_lu_single = frame(
    "/Lux5253694472_M\n"
    "\n"
    "1-3:0.2.8(42)\n"
    "0-0:1.0.0(251116104500S)\n"
    "0-0:42.0.0(53414731303330373930303032353735)\n"
    "0-0:96.1.0(464C4F3138393930)\n"
    "1-0:1.8.0(001234.567*kWh)\n"
    "1-0:2.8.0(000001.234*kWh)\n"
    "1-0:3.8.0(000002.345*kvarh)\n"
    "1-0:4.8.0(000003.456*kvarh)\n"
    "1-0:1.7.0(001.234*kW)\n"
    "1-0:2.7.0(000.000*kW)\n"
    "1-0:3.7.0(000.010*kvar)\n"
    "1-0:4.7.0(000.000*kvar)\n"
    "0-0:17.0.0(011.5*kVA)\n"
    "1-0:9.7.0(001.234*kVA)\n"
    "1-0:10.7.0(000.000*kVA)\n"
    "0-0:96.3.10(1)\n"
    "0-0:96.7.21(00007)\n"
    "1-0:32.7.0(230.5*V)\n"
    "1-0:31.7.0(005*A)\n"
    "1-0:21.7.0(001.234*kW)\n"
    "1-0:22.7.0(000.000*kW)\n"
    "0-1:24.1.0(003)\n"
    "0-1:96.1.0(474153313233343536)\n"
    "0-1:24.2.1(251116104500S)(00012.345*m3)\n"
    "0-1:24.4.0(1)\n"
)

f32_lu_padding_rejected = frame(
    "/Lux5253694471_M\n"
    "\n"
    "1-3:0.2.8(42)\n"
    "0-0:1.0.0(200706104157S)\n"
    "1-0:1.8.0(000025.653*kWh)\n"
    "0-1:24.1.0(003)\n"
    "0-1:24.2.1(200706103140S)(00000.006*m3)\n"
    "0-2:24.1.0(007)\n"
    "0-2:96.1.0()\n"
    "0-2:24.2.1(632525252525S)(00000.000)\n"
)

f33_body = f31_lu_single.rsplit("!", 1)[0].replace("1-3:0.2.8(42)", "1-3:0.2.8(43)")
f33_lu_bad_version = frame(f33_body)

# --- Switzerland (ch.dsmr.5.0) -----------------------------------------------
# F34 and F35 follow the minimum data points of VSE RL-DSP CH 2024 Anhang 3
# Tabelle 1 with the optional Tabelle 2 rows on F34; the resolutions printed
# in the annex (kWh/kW 0.01, V 1, A 0.01) are reproduced.

f34_ch_three = frame(
    "/CAC5CH12345678\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:96.1.1(43484142433132333435)\n"
    "0-0:42.0.0(43482D4D455445522D3031)\n"
    "0-0:1.0.0(251116104500S)\n"
    "1-0:1.8.0(002345.67*kWh)\n"
    "1-0:2.8.0(000034.56*kWh)\n"
    "1-0:1.8.1(002000.00*kWh)\n"
    "1-0:1.8.2(000345.67*kWh)\n"
    "1-0:2.8.1(000034.56*kWh)\n"
    "1-0:2.8.2(000000.00*kWh)\n"
    "1-0:3.8.0(000012.34*kVArh)\n"
    "1-0:4.8.0(000005.67*kVArh)\n"
    "1-0:1.7.0(001.23*kW)\n"
    "1-0:2.7.0(000.00*kW)\n"
    "1-0:21.7.0(000.60*kW)\n"
    "1-0:41.7.0(000.40*kW)\n"
    "1-0:61.7.0(000.23*kW)\n"
    "1-0:22.7.0(000.00*kW)\n"
    "1-0:42.7.0(000.00*kW)\n"
    "1-0:62.7.0(000.00*kW)\n"
    "1-0:23.7.0(000.05*kvar)\n"
    "1-0:43.7.0(000.05*kvar)\n"
    "1-0:63.7.0(000.05*kvar)\n"
    "1-0:24.7.0(000.02*kvar)\n"
    "1-0:44.7.0(000.02*kvar)\n"
    "1-0:64.7.0(000.02*kvar)\n"
    "1-0:31.7.0(005.34*A)\n"
    "1-0:51.7.0(004.21*A)\n"
    "1-0:71.7.0(003.10*A)\n"
    "1-0:32.7.0(231*V)\n"
    "1-0:52.7.0(230*V)\n"
    "1-0:72.7.0(229*V)\n"
)

f35_ch_single = frame(
    "/CAC5CH87654321\n"
    "\n"
    "1-3:0.2.8(50)\n"
    "0-0:42.0.0(43482D4D455445522D3032)\n"
    "0-0:1.0.0(251116104500S)\n"
    "1-0:1.8.0(001000.00*kWh)\n"
    "1-0:2.8.0(000000.00*kWh)\n"
    "1-0:1.7.0(000.50*kW)\n"
    "1-0:2.7.0(000.00*kW)\n"
    "1-0:32.7.0(230*V)\n"
    "1-0:31.7.0(002.15*A)\n"
)

f36_ch_bad_crc = replace_crc(f34_ch_three, lambda c: c[:-1] + ("0" if c[-1] != "0" else "1"))

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
NL4_AUTO_SOURCE = "Same telegram as F1 decoded through the automatic " \
                  "nl.dsmr.4 profile, which selects 4.0 or 4.2 from the " \
                  "reported version after CRC validation."
HU_SOURCE = "Synthetic Hungarian telegram structured after the E.ON " \
            "Hungaria P1 port user information leaflet 2024-07-02 " \
            "(assets/P1_port_felhasznaloi_interfesz_felhasznaloi_tajekoztato_" \
            " 20240702.pdf) and the Hungarian rows of " \
            "assets/Europese_P1_OBIS_Codes_Overzicht.pdf; DSMR 5 framing; " \
            "anonymized data, trailer CRC computed with CRC-16/ARC."
SE_SOURCE = "Synthetic Swedish telegram structured after the Swedish rows " \
            "of assets/Europese_P1_OBIS_Codes_Overzicht.pdf (aggregates, " \
            "reactive set, M-Bus heat and water, status and error " \
            "register); DSMR 5 framing; anonymized data, trailer CRC " \
            "computed with CRC-16/ARC."
FI_SOURCE = "Synthetic Finnish telegram structured after the Finnish rows " \
            "of assets/Europese_P1_OBIS_Codes_Overzicht.pdf; the Finnish " \
            "port publishes the M-Bus device type only; anonymized data, " \
            "trailer CRC computed with CRC-16/ARC."
LT_SOURCE = "Synthetic Lithuanian telegram structured after the Lithuanian " \
            "rows of assets/Europese_P1_OBIS_Codes_Overzicht.pdf " \
            "(per-tariff reactive set, breaker, status words, event code, " \
            "M-Bus gas and water); anonymized data, trailer CRC computed " \
            "with CRC-16/ARC."
IE_SOURCE = "Synthetic Irish telegram structured after the Irish rows of " \
            "assets/Europese_P1_OBIS_Codes_Overzicht.pdf (DSMR core kernel " \
            "plus the factory status word); anonymized data, trailer CRC " \
            "computed with CRC-16/ARC."
LU_EXAMPLE_SOURCE = "Adapted from the clear-text example telegram of the " \
                    "Luxmetering E-Meter P1 Specification 1.1.3 Appendix A " \
                    "(assets/SPEC - E-Meter_P1_specification_20210308.pdf); " \
                    "trailer CRC recomputed with CRC-16/ARC."
LU_SOURCE = "Synthetic Luxembourg telegram following the readout list of " \
            "the Luxmetering E-Meter P1 Specification 1.1.3 (clear text, " \
            "AES-128-GCM transport layer disabled); anonymized data, " \
            "trailer CRC computed with CRC-16/ARC."
CH_SOURCE = "Synthetic Swiss telegram following the minimum data points of " \
            "VSE RL-DSP CH 2024 Anhang 3 Tabelle 1/2 with the P1 DSMR 5.0.2 " \
            "wire protocol (assets/20250627-be-standards-kundenschnittstelle" \
            "-intelligente-messsysteme-anhang3 (1).pdf); anonymized data, " \
            "trailer CRC computed with CRC-16/ARC."

def derived(source: str) -> str:
    return "Derived from the " + source + " by the edit named in the " \
           "identifier; trailer handling as described by the negative case."

HEADER = '''//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: test/fixtures/dsmr_fixtures.h - shared telegram fixture set
//-- Date: 2026-11-16 · Build: 3
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
  ("fixtureF14", "f14Nl4Auto", "F14 nl.dsmr.4 auto selection", NL4_AUTO_SOURCE, "NL", "nl.dsmr.4", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF15", "f15HuThreePhase", "F15 hu.dsmr.5.0 three-phase", HU_SOURCE, "HU", "hu.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF16", "f16HuSinglePhase", "F16 hu.dsmr.5.0 single-phase", HU_SOURCE, "HU", "hu.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF17", "f17HuBadVersion", "F17 hu bad version 51", derived("Hungarian three-phase fixture"), "HU", "hu.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_INVALID_VERSION", False),
  ("fixtureF18", "f18SeThreePhase", "F18 se.dsmr.5.0 three-phase", SE_SOURCE, "SE", "se.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_HEAT"), (2, "DSMR_MBUS_DEVICE_WATER")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF19", "f19SeSinglePhase", "F19 se.dsmr.5.0 single-phase", SE_SOURCE, "SE", "se.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_HEAT")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF20", "f20SeBadCrc", "F20 se bad CRC", derived("Swedish three-phase fixture"), "SE", "se.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_HEAT"), (2, "DSMR_MBUS_DEVICE_WATER")], False, "DSMR_DIAG_CRC_MISMATCH", False),
  ("fixtureF21", "f21FiThreePhase", "F21 fi.dsmr.5.0 three-phase", FI_SOURCE, "FI", "fi.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF22", "f22FiSinglePhase", "F22 fi.dsmr.5.0 single-phase", FI_SOURCE, "FI", "fi.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF23", "f23FiMissingVersion", "F23 fi missing version", derived("Finnish single-phase fixture"), "FI", "fi.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_MISSING_VERSION", False),
  ("fixtureF24", "f24LtThreePhase", "F24 lt.dsmr.5.0 three-phase", LT_SOURCE, "LT", "lt.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS"), (2, "DSMR_MBUS_DEVICE_WATER")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF25", "f25LtSinglePhase", "F25 lt.dsmr.5.0 single-phase", LT_SOURCE, "LT", "lt.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF26", "f26LtMalformedTrailer", "F26 lt malformed trailer", derived("Lithuanian single-phase fixture"), "LT", "lt.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_MALFORMED_TRAILER", False),
  ("fixtureF27", "f27IeThreePhase", "F27 ie.dsmr.5.0 three-phase", IE_SOURCE, "IE", "ie.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF28", "f28IeSinglePhase", "F28 ie.dsmr.5.0 single-phase", IE_SOURCE, "IE", "ie.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF29", "f29IeDuplicate", "F29 ie duplicated data line", derived("Irish single-phase fixture"), "IE", "ie.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_DUPLICATE_FIELD", False),
  ("fixtureF30", "f30LuThreePhase", "F30 lu.dsmr.5.0 three-phase clear text", LU_EXAMPLE_SOURCE, "LU", "lu.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS"), (4, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF31", "f31LuSinglePhase", "F31 lu.dsmr.5.0 single-phase", LU_SOURCE, "LU", "lu.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], True, "DSMR_DIAG_NONE", False),
  ("fixtureF32", "f32LuPaddingTimestamps", "F32 lu padding timestamps rejected", LU_EXAMPLE_SOURCE, "LU", "lu.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS"), (2, "DSMR_MBUS_DEVICE_WATER")], False, "DSMR_DIAG_INVALID_TIMESTAMP", False),
  ("fixtureF33", "f33LuBadVersion", "F33 lu bad version 43", derived("Luxembourg single-phase fixture"), "LU", "lu.dsmr.5.0", [(1, "DSMR_MBUS_DEVICE_GAS")], False, "DSMR_DIAG_INVALID_VERSION", False),
  ("fixtureF34", "f34ChThreePhase", "F34 ch.dsmr.5.0 three-phase", CH_SOURCE, "CH", "ch.dsmr.5.0", None, True, "DSMR_DIAG_NONE", False),
  ("fixtureF35", "f35ChMinimumSet", "F35 ch.dsmr.5.0 single-phase minimum set", CH_SOURCE, "CH", "ch.dsmr.5.0", None, True, "DSMR_DIAG_NONE", False),
  ("fixtureF36", "f36ChBadCrc", "F36 ch bad CRC", derived("Swiss three-phase fixture"), "CH", "ch.dsmr.5.0", None, False, "DSMR_DIAG_CRC_MISMATCH", False),
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
  "fixtureF14": f14_nl4_auto,
  "fixtureF15": f15_hu_three,
  "fixtureF16": f16_hu_single,
  "fixtureF17": f17_hu_bad_version,
  "fixtureF18": f18_se_three,
  "fixtureF19": f19_se_single,
  "fixtureF20": f20_se_bad_crc,
  "fixtureF21": f21_fi_three,
  "fixtureF22": f22_fi_single,
  "fixtureF23": f23_fi_missing_version,
  "fixtureF24": f24_lt_three,
  "fixtureF25": f25_lt_single,
  "fixtureF26": f26_lt_malformed,
  "fixtureF27": f27_ie_three,
  "fixtureF28": f28_ie_single,
  "fixtureF29": f29_ie_duplicate,
  "fixtureF30": f30_lu_three,
  "fixtureF31": f31_lu_single,
  "fixtureF32": f32_lu_padding_rejected,
  "fixtureF33": f33_lu_bad_version,
  "fixtureF34": f34_ch_three,
  "fixtureF35": f35_ch_single,
  "fixtureF36": f36_ch_bad_crc,
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
