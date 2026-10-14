CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
COMMON  = globals.c bitcopy.c
ENGINE  = engine.c $(COMMON)

all: space

space: space.c engine.c $(COMMON) space.h
	$(CC) $(CFLAGS) -o space space.c engine.c $(COMMON)

check: all
	./tests/run_tests.sh

clean:
	rm -f space

.PHONY: all check clean
