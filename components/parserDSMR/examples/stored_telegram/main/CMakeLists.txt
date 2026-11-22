# parserDSMR - stored telegram test program
# Date: 2026-11-21 · Build: 3

idf_component_register(
  SRCS "app_main.c"
  INCLUDE_DIRS "."
  PRIV_INCLUDE_DIRS "fixtures"
  REQUIRES parserDSMR
)
