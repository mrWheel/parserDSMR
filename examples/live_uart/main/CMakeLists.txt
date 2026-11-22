# parserDSMR - live UART test program (local development wrapper)
# Date: 2026-11-21 · Build: 3
#
# Builds the example source that ships inside the component, so the
# published example and this development project exercise the same code.
# The component itself comes from the local checkout through the
# EXTRA_COMPONENT_DIRS entry of the project CMakeLists.txt.

idf_component_register(
  SRCS "../../../components/parserDSMR/examples/live_uart/main/app_main.c"
  REQUIRES parserDSMR
)
