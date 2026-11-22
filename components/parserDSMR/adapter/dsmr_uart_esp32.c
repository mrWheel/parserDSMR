//--    parserDSMR - ESP-IDF UART receive adapter that feeds the parser from the P1 port
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
//-- File: adapter/dsmr_uart_esp32.c - ESP-IDF UART receive adapter
//-- Date: 2026-11-21 · Build: 3
//--
//-- The line settings come from the resolved profile of the parser. The
//-- receive task owns the parser instance while it runs: bytes arrive
//-- through dsmrParserFeed and idle periods through dsmrParserTick. A ring
//-- buffer overflow in the UART driver is reported to the parser as a data
//-- loss event so the partial frame is discarded and reception recovers.
//-- ==========================================================================

#include "dsmr_uart_esp32.h"

#include <inttypes.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *tag = "dsmrUart";

#define DSMR_UART_RX_BUFFER_BYTES 4096
#define DSMR_UART_READ_CHUNK_BYTES 512
#define DSMR_UART_POLL_PERIOD_MS 100
#define DSMR_UART_TASK_STACK_DEFAULT 4096
#define DSMR_UART_TASK_PRIORITY_DEFAULT 5
#define DSMR_UART_EVENT_QUEUE_LENGTH 16

static uint32_t dsmrUartNowMs(void)
{
  return (uint32_t)(esp_timer_get_time() / 1000LL);
}

//-- Drains the UART driver event queue without blocking. Overflow and
//-- framing errors are reported to the parser as data loss so the partial
//-- frame is discarded and reception recovers on the next complete frame.
//-- UART_DATA notifications are ignored: bytes are consumed by polling.
static void dsmrUartDrainEvents(dsmrUart *uart)
{
  QueueHandle_t queue = (QueueHandle_t)uart->eventQueue;
  uart_event_t event;

  if (queue == NULL)
  {
    return;
  }

  while (xQueueReceive(queue, &event, 0) == pdTRUE)
  {
    switch (event.type)
    {
      case UART_BUFFER_FULL:
      case UART_FIFO_OVF:
      case UART_FRAME_ERR:
      case UART_PARITY_ERR:
      {
        ESP_LOGW(tag, "uart event %d, notifying parser of data loss", (int)event.type);
        dsmrParserNotifyDataLoss(uart->parser, uart->callback, uart->userData);
        break;
      }

      default:
      {
        break;
      }
    }
  }
}

static void dsmrUartTask(void *argument)
{
  dsmrUart *uart = (dsmrUart *)argument;
  uint8_t chunk[DSMR_UART_READ_CHUNK_BYTES];

  while (uart->running)
  {
    int received = uart_read_bytes(uart->config.uartPort, chunk, sizeof chunk, pdMS_TO_TICKS(DSMR_UART_POLL_PERIOD_MS));

    if (received > 0)
    {
      dsmrParserFeed(uart->parser, chunk, (size_t)received, dsmrUartNowMs(), uart->callback, uart->userData);
    }
    else if (received < 0)
    {
      ESP_LOGW(tag, "uart read error, notifying parser of data loss");
      dsmrParserNotifyDataLoss(uart->parser, uart->callback, uart->userData);
    }
    else
    {
      dsmrParserTick(uart->parser, dsmrUartNowMs(), uart->callback, uart->userData);
    }

    dsmrUartDrainEvents(uart);
  }

  uart->taskHandle = NULL;
  vTaskDelete(NULL);
}

static esp_err_t dsmrUartConfigureUart(dsmrUart *uart, const dsmrProfile *profile)
{
  uart_config_t uartConfig;
  memset(&uartConfig, 0, sizeof uartConfig);
  uartConfig.baud_rate = (int)profile->uartBaudRate;
  uartConfig.data_bits = (profile->uartDataBits == 7) ? UART_DATA_7_BITS : UART_DATA_8_BITS;
  uartConfig.parity = (profile->uartParity == 1) ? UART_PARITY_ODD : ((profile->uartParity == 2) ? UART_PARITY_EVEN : UART_PARITY_DISABLE);
  uartConfig.stop_bits = (profile->uartStopBits == 2) ? UART_STOP_BITS_2 : UART_STOP_BITS_1;
  uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uartConfig.source_clk = UART_SCLK_DEFAULT;

  //-- The driver creates the event queue so overflow and framing errors can
  //-- be reported to the parser instead of dropping silently.
  esp_err_t result = uart_driver_install(uart->config.uartPort, DSMR_UART_RX_BUFFER_BYTES, 0, DSMR_UART_EVENT_QUEUE_LENGTH, (QueueHandle_t *)&uart->eventQueue, 0);

  if (result != ESP_OK)
  {
    ESP_LOGE(tag, "uart driver install failed: %s", esp_err_to_name(result));
    uart->eventQueue = NULL;
    return result;
  }

  result = uart_param_config(uart->config.uartPort, &uartConfig);

  if (result != ESP_OK)
  {
    ESP_LOGE(tag, "uart param config failed: %s", esp_err_to_name(result));
    uart_driver_delete(uart->config.uartPort);
    uart->eventQueue = NULL;
    return result;
  }

  result = uart_set_pin(uart->config.uartPort, uart->config.txPin, uart->config.rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  if (result != ESP_OK)
  {
    ESP_LOGE(tag, "uart pin config failed: %s", esp_err_to_name(result));
    uart_driver_delete(uart->config.uartPort);
    uart->eventQueue = NULL;
    return result;
  }

  if (uart->config.invertRx)
  {
    result = uart_set_line_inverse(uart->config.uartPort, UART_SIGNAL_RXD_INV);

    if (result != ESP_OK)
    {
      ESP_LOGE(tag, "rx inversion failed: %s", esp_err_to_name(result));
      uart_driver_delete(uart->config.uartPort);
      uart->eventQueue = NULL;
      return result;
    }
  }

  return ESP_OK;
}

static void dsmrUartActivateRequest(const dsmrUart *uart)
{
  if (uart->config.requestGpio < 0)
  {
    //-- The request signal is driven externally or not used at all.
    return;
  }

  gpio_reset_pin((gpio_num_t)uart->config.requestGpio);

  gpio_config_t gpioConfig;
  memset(&gpioConfig, 0, sizeof gpioConfig);
  gpioConfig.pin_bit_mask = 1ULL << (uint32_t)uart->config.requestGpio;
  gpioConfig.mode = GPIO_MODE_OUTPUT;
  gpio_pullup_dis((gpio_num_t)uart->config.requestGpio);
  gpio_config(&gpioConfig);

  gpio_set_level((gpio_num_t)uart->config.requestGpio, uart->config.requestActiveHigh ? 1 : 0);

  if (uart->config.requestLeadMs > 0u)
  {
    vTaskDelay(pdMS_TO_TICKS(uart->config.requestLeadMs));
  }
}

static void dsmrUartReleaseRequest(const dsmrUart *uart)
{
  if (uart->config.requestGpio < 0)
  {
    return;
  }

  gpio_set_level((gpio_num_t)uart->config.requestGpio, uart->config.requestActiveHigh ? 0 : 1);
  gpio_reset_pin((gpio_num_t)uart->config.requestGpio);
}

esp_err_t dsmrUartStart(dsmrUart *uart, dsmrParser *parser, const dsmrUartConfig *config, dsmrEventCallback callback, void *userData)
{
  if ((uart == NULL) || (parser == NULL) || (config == NULL) || (callback == NULL))
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (!parser->initialized || (parser->profile == NULL))
  {
    ESP_LOGE(tag, "parser is not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (uart->running)
  {
    ESP_LOGE(tag, "adapter is already running");
    return ESP_ERR_INVALID_STATE;
  }

  memset(uart, 0, sizeof *uart);
  uart->parser = parser;
  uart->config = *config;
  uart->callback = callback;
  uart->userData = userData;

  if (uart->config.taskStackSize == 0u)
  {
    uart->config.taskStackSize = DSMR_UART_TASK_STACK_DEFAULT;
  }

  if (uart->config.taskPriority == 0u)
  {
    uart->config.taskPriority = DSMR_UART_TASK_PRIORITY_DEFAULT;
  }

  esp_err_t result = dsmrUartConfigureUart(uart, parser->profile);

  if (result != ESP_OK)
  {
    return result;
  }

  uart->running = true;

  BaseType_t created = xTaskCreate(dsmrUartTask, "dsmrUart", uart->config.taskStackSize, uart, uart->config.taskPriority, (TaskHandle_t *)&uart->taskHandle);

  if (created != pdPASS)
  {
    ESP_LOGE(tag, "receive task creation failed");
    uart->running = false;
    uart_driver_delete(uart->config.uartPort);
    return ESP_ERR_NO_MEM;
  }

  //-- Reception is initialized before the request signal is activated.
  dsmrUartActivateRequest(uart);

  ESP_LOGI(tag, "receiving on UART %d at %" PRIu32 " baud, profile %s", (int)uart->config.uartPort, parser->profile->uartBaudRate, parser->profile->profileId);
  return ESP_OK;
}

esp_err_t dsmrUartStop(dsmrUart *uart)
{
  if (uart == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (!uart->running)
  {
    return ESP_OK;
  }

  uart->running = false;

  //-- Wait for the receive task to leave its read loop and release itself.
  while (uart->taskHandle != NULL)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  dsmrUartReleaseRequest(uart);
  uart_driver_delete(uart->config.uartPort);

  ESP_LOGI(tag, "receive task stopped, resources released");
  return ESP_OK;
}
