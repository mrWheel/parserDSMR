//--    parserDSMR - sample DSMR 5.0 telegram used by the replay demos
//--    Copyright (C) 2026  Willem Aandewiel (mrWheel)
//--
//--    This program is free software: you can redistribute it and/or modify
//--    it under the terms of the GNU General Public License as published by
//--    the Free Software Foundation, either version 3 of the License, or
//--    (at your option) any later version.
//--
//--    This program is distributed in the hope that it will be useful,
//--    but WITHOUT ANY WARRANTY; without even the implied warranty of
//--    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//--    GNU General Public License for more details.
//--
//--    You should have received a copy of the GNU General Public License
//--    along with this program.  If not, see <https://www.gnu.org/licenses/>.

//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: test/fixtures/dsmr_sample_telegram.h - sample DSMR 5.0 telegram used by the replay demos
//-- Date: 2026-11-21 · Build: 1
//-- ==========================================================================

//-- Sample telegram replayed by the root ESP-IDF test project and host tests.
//-- It has the required leading slash and a CRC computed for this exact frame.

#ifndef DSMR_SAMPLE_TELEGRAM_H
#define DSMR_SAMPLE_TELEGRAM_H

static const char dsmrSampleTelegram[] =
  "/SK5\\2M550T-1014\r\n"
  "\r\n"
  "1-3:0.2.8(50)\r\n"
  "0-0:1.0.0(261114170412W)\r\n"
  "0-0:96.1.1(4530303832303039333633373932343234)\r\n"
  "1-0:1.8.1(001189.755*kWh)\r\n"
  "1-0:1.8.2(001075.650*kWh)\r\n"
  "1-0:2.8.1(000213.728*kWh)\r\n"
  "1-0:2.8.2(000496.982*kWh)\r\n"
  "0-0:96.14.0(0001)\r\n"
  "1-0:1.7.0(02.906*kW)\r\n"
  "1-0:2.7.0(00.000*kW)\r\n"
  "0-0:96.7.21(00004)\r\n"
  "0-0:96.7.9(00003)\r\n"
  "1-0:99.97.0(1)(0-0:96.7.19)(241120143608W)(0000000284*s)\r\n"
  "1-0:32.32.0(00001)\r\n"
  "1-0:52.32.0(00002)\r\n"
  "1-0:72.32.0(00002)\r\n"
  "1-0:32.36.0(00001)\r\n"
  "1-0:52.36.0(00000)\r\n"
  "1-0:72.36.0(00000)\r\n"
  "0-0:96.13.0()\r\n"
  "1-0:32.7.0(233.5*V)\r\n"
  "1-0:52.7.0(236.5*V)\r\n"
  "1-0:72.7.0(231.7*V)\r\n"
  "1-0:31.7.0(000*A)\r\n"
  "1-0:51.7.0(000*A)\r\n"
  "1-0:71.7.0(011*A)\r\n"
  "1-0:21.7.0(00.094*kW)\r\n"
  "1-0:41.7.0(00.139*kW)\r\n"
  "1-0:61.7.0(02.679*kW)\r\n"
  "1-0:22.7.0(00.000*kW)\r\n"
  "1-0:42.7.0(00.000*kW)\r\n"
  "1-0:62.7.0(00.000*kW)\r\n"
  "0-1:24.1.0(003)\r\n"
  "0-1:96.1.0(4730303339303031363532303530323136)\r\n"
  "0-1:24.2.1(261114170006W)(07802.279*m3)\r\n"
  "!CB64\r\n";

#endif
