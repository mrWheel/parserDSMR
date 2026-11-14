# parserDSMR - stored telegram test program
# Date: 2026-11-14 · Build: 1

idf_component_register(
  SRCS "app_main.c"
  INCLUDE_DIRS "."
  PRIV_INCLUDE_DIRS "../../../test/fixtures"
  REQUIRES parserDSMR
)
