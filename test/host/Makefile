# parserDSMR - host tests for the portable C11 core
# Date: 2026-11-21 · Build: 3
#
# The sources under test live in the publishable component at
# ../../components/parserDSMR.

CC ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -g -fsanitize=address,undefined
CPPFLAGS := -I../../components/parserDSMR/include -I../../components/parserDSMR/src -I../fixtures

SRCS := \
  ../../components/parserDSMR/src/dsmr_crc.c \
  ../../components/parserDSMR/src/dsmr_value.c \
  ../../components/parserDSMR/src/dsmr_profile.c \
  ../../components/parserDSMR/src/dsmr_framing.c \
  ../../components/parserDSMR/src/dsmr_decode.c \
  ../../components/parserDSMR/src/dsmr_parser.c \
  test_dsmr.c

test_dsmr: $(SRCS) ../fixtures/dsmr_fixtures.h ../fixtures/dsmr_sample_telegram.h ../fixtures/dsmr_sample_labels.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRCS)

run: test_dsmr
	./test_dsmr

fixtures:
	python3 -I ../fixtures/tools/generate_fixtures.py

clean:
	rm -f test_dsmr

.PHONY: run fixtures clean
