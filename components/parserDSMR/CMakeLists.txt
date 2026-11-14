# parserDSMR - ESP-IDF component
# Date: 2026-11-14 · Build: 1

set(dsmr_sources
  "src/dsmr_crc.c"
  "src/dsmr_framing.c"
  "src/dsmr_value.c"
  "src/dsmr_profile.c"
  "src/dsmr_decode.c"
  "src/dsmr_parser.c"
  "adapter/dsmr_uart_esp32.c"
)

idf_component_register(
  SRCS ${dsmr_sources}
  INCLUDE_DIRS "include"
  PRIV_INCLUDE_DIRS "src"
  REQUIRES esp_driver_uart esp_driver_gpio esp_timer
)
