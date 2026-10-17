CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O3 -flto
COMMON  = globals.c bitcopy.c
ENGINE  = engine.c config.c $(COMMON)

all: space

space: space.c $(ENGINE) space.h
	$(CC) $(CFLAGS) -o space space.c $(ENGINE)

check: all
	./tests/run_tests.sh

clean:
	rm -f space

.PHONY: all check clean
