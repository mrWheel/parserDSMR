//--    parserDSMR - public API of the ESP-IDF UART receive adapter
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
//-- File: include/dsmr_uart_esp32.h - ESP-IDF UART adapter
//-- Date: 2026-11-21 · Build: 3
//--
//-- This adapter is outside the Arduino compilation path; the portable core
//-- under src/ does not depend on it. It configures the UART from the
//-- resolved profile (baud rate, data bits, parity, stop bits) and owns the
//-- receive task. The parser event callback is called from the receive task,
//-- so one parser instance belongs to this task.
//--
//-- P1 interface hardware note: the smart meter P1 port is an idle-high
//-- serial interface. The chosen P1 interface board must provide signal
//-- conditioning and level shifting from the meter's logic level to the
//-- ESP32-S3 3.3 V I/O level. When the board presents the signal inverted,
//-- enable invertRx. The request signal, when wired to a GPIO, is driven by
//-- this adapter; when the board drives request externally, set requestGpio
//-- to -1 and drive the signal yourself.
//-- ==========================================================================

#ifndef DSMR_UART_ESP32_H
#define DSMR_UART_ESP32_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

#include "driver/uart.h"
#include "esp_err.h"

#include "dsmr_parser.h"

typedef struct
{
  uart_port_t uartPort;
  int rxPin;
  int txPin;
  int requestGpio;
  bool requestActiveHigh;
  bool invertRx;
  uint32_t requestLeadMs;
  uint32_t taskStackSize;
  uint8_t taskPriority;
} dsmrUartConfig;

typedef struct
{
  dsmrParser *parser;
  dsmrUartConfig config;
  dsmrEventCallback callback;
  void *userData;
  void *taskHandle;

  //-- Queue handle of the UART driver event queue, owned by this adapter.
  //-- The receive task drains it and reports overflow and framing errors to
  //-- the parser as data loss events.
  void *eventQueue;
  volatile bool running;
} dsmrUart;

//-- Installs and configures the UART from the parser profile, starts the
//-- receive task, and only then activates the request signal. The parser
//-- must already be initialized; its resolved profile governs the line
//-- settings. The application must not feed the same parser from another
//-- task while this adapter runs.
esp_err_t dsmrUartStart(dsmrUart *uart, dsmrParser *parser, const dsmrUartConfig *config, dsmrEventCallback callback, void *userData);

//-- Stops the receive task and releases the resources owned by this
//-- adapter: the UART driver and the request GPIO.
esp_err_t dsmrUartStop(dsmrUart *uart);

#ifdef __cplusplus
}
#endif

#endif //-- DSMR_UART_ESP32_H
