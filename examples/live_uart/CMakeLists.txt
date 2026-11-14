# parserDSMR - live UART test program
# Date: 2026-11-14 · Build: 1

cmake_minimum_required(VERSION 3.16)

get_filename_component(DSMR_COMPONENT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
list(APPEND EXTRA_COMPONENT_DIRS "${DSMR_COMPONENT_ROOT}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(live_uart)
