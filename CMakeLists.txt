# parserDSMR - ESP-IDF component build
# Date: 2026-11-16 · Build: 2

# This project demonstrates how the parserDSMR component works.
# For actual use, include this component in another ESP-IDF project.

cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(parserDSMR)
