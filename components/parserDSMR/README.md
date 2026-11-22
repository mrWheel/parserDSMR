<!--
    parserDSMR - reusable C library that parses European DSMR/P1 smart-meter telegrams
    Copyright (C) 2026  Willem Aandewiel (mrWheel)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
-->

<!--
 fileVersion: [2] 22-11-2026 - 07:45
-->

# parserDSMR — DSMR/P1 smart-meter telegram parser

parserDSMR reads and parses P1 telegrams from European smart electricity
meters. Point it at the P1 port — or at a stored buffer of bytes — and it
turns the raw serial stream into structured, decoded fields: energy
delivered per tariff, current power, voltage and current per phase, M-Bus
gas/water/heat readings, tariff indicator, text messages and more.

The portable core is plain ISO C11: no operating system, no dynamic
allocation, no locale dependence. The ESP-IDF adapter owns a UART receive
task and feeds it. Built and tested for the ESP32-S3.

## Installing the component

```sh
idf.py add-dependency "mrwheel/parserDSMR^0.9.2"
```

Include `dsmr_parser.h` for the portable core and `dsmr_uart_esp32.h` for
the ESP-IDF UART adapter.

## Quick start

The smallest complete application that reads a live P1 port and prints one
line per received field:

```c
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

The parser performs no dynamic allocation: the application owns the
instance and the workspace. Application-determined field names are set
through a label table in the configuration or with
`dsmrParserSetFieldLabel`; a field without a label stays reachable through
its OBIS code. Configuration, profile, M-Bus device table and label table
can be changed at run time with `dsmrParserApplyConfig`.

## Supported profiles

| Profile ID | Country | Reported version | UART | CRC |
|---|---|---|---|---|
| `nl.dsmr.4.0` | NL | `1-3:0.2.8(40)` | 115200 8N1 | required |
| `nl.dsmr.4.2` | NL | `1-3:0.2.8(42)` | 115200 8N1 | required |
| `nl.dsmr.5.0` | NL | `1-3:0.2.8(50)` | 115200 8N1 | required |
| `nl.dsmr.4` | NL | auto-selects 4.0 or 4.2 from the version field | 115200 8N1 | required |
| `be.emucs.1.x` | BE | `0-0:96.1.4(50213…50217)` | 115200 8N1 | required |
| `be.emucs.2.1` | BE | `0-0:96.1.4(50221)` | 115200 8N1 | required |
| `hu.dsmr.5.0` | HU | `1-3:0.2.8(50)` | 115200 8N1 | required |
| `se.dsmr.5.0` | SE | `1-3:0.2.8(50)` | 115200 8N1 | required |
| `fi.dsmr.5.0` | FI | `1-3:0.2.8(50)` | 115200 8N1 | required |
| `lt.dsmr.5.0` | LT | `1-3:0.2.8(50)` | 115200 8N1 | required |
| `ie.dsmr.5.0` | IE | `1-3:0.2.8(50)` | 115200 8N1 | required |
| `lu.dsmr.5.0` | LU | `1-3:0.2.8(42)` | 115200 8N1 | required |
| `ch.dsmr.5.0` | CH | `1-3:0.2.8(50)` | 115200 8N1 | required |

List the built-in identifiers at run time with `dsmrProfileListIds()`, or
pass a `customProfile` you own for a meter that fits none of them.

## Examples

Both example projects are standalone ESP-IDF applications that use this
component through its published dependency.

| Example | What it does |
|---|---|
| `examples/live_uart` | Waits for telegrams on the P1 port UART and prints one line per OBIS field plus a per-telegram summary. Adjust the defines at the top of `main/app_main.c` to your board and meter. |
| `examples/stored_telegram` | Runs from the USB console with no P1 wiring: it replays the embedded fixture telegrams, prints every accepted field on its own OBIS line, reports each rejected candidate with its reason and demonstrates dynamic reconfiguration through `dsmrParserApplyConfig`. |

Build either one as a normal ESP-IDF project:

```sh
cd examples/live_uart          # or examples/stored_telegram
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

## Hardware note

The smart meter P1 port is an idle-high open-collector signal. Most P1
interface boards condition and invert it; set `invertRx` accordingly. The
UART port, RX/TX pins, request GPIO and polarity are configurable through
`dsmrUartConfig`.

## License

GPL-3.0-or-later. See the `LICENSE` file shipped with this component.
