CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
COMMON  = globals.c bitcopy.c

all: space assembler disassembler

space: space.c $(COMMON) space.h
	$(CC) $(CFLAGS) -o space space.c $(COMMON)

assembler: assembler.c $(COMMON) space.h
	$(CC) $(CFLAGS) -o assembler assembler.c $(COMMON)

disassembler: disassembler.c $(COMMON) space.h
	$(CC) $(CFLAGS) -o disassembler disassembler.c $(COMMON)

check: all
	./tests/run_tests.sh

clean:
	rm -f space assembler disassembler tests/test_bitcpy

.PHONY: all check clean
