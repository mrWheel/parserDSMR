# parserDSMR - host tests for the portable C11 core
# Date: 2026-11-14 · Build: 1

CC ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -g -fsanitize=address,undefined
CPPFLAGS := -I../../include -I../../src -I../fixtures

SRCS := \
  ../../src/dsmr_crc.c \
  ../../src/dsmr_value.c \
  ../../src/dsmr_profile.c \
  ../../src/dsmr_framing.c \
  ../../src/dsmr_decode.c \
  ../../src/dsmr_parser.c \
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
