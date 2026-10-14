CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
COMMON  = globals.c bitcopy.c

all: space

space: space.c $(COMMON) space.h
	$(CC) $(CFLAGS) -o space space.c $(COMMON)

check: all
	./tests/run_tests.sh

clean:
	rm -f space tests/test_bitcpy

.PHONY: all check clean
