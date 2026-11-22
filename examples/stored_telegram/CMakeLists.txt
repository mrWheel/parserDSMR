# parserDSMR - stored telegram test program (local development project)
# Date: 2026-11-21 · Build: 3
#
# Development wrapper around components/parserDSMR/examples/stored_telegram.
# The published example resolves parserDSMR from the registry through the
# manifest in its main component; this project uses the local checkout.

cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stored_telegram)
