# parserDSMR - stored telegram test program (local development wrapper)
# Date: 2026-11-21 · Build: 3
#
# Builds the example source that ships inside the component, so the
# published example and this development project exercise the same code.
# The fixtures come from the copies bundled with the component example;
# refresh them after any fixture change with make fixtures in test/host.

idf_component_register(
  SRCS "../../../components/parserDSMR/examples/stored_telegram/main/app_main.c"
  PRIV_INCLUDE_DIRS "../../../components/parserDSMR/examples/stored_telegram/main/fixtures"
  REQUIRES parserDSMR
)
