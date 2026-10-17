ifeq ($(origin CC),default)
CC      := gcc
endif
CFLAGS  ?= -std=c11 -Wall -Wextra -O3 -flto
COMMON  = globals.c bitcopy.c
ENGINE  = engine.c config.c sys_ctrl.c $(COMMON)
GEN     = tools/proto_gen.py

all: space

space: space.c $(ENGINE) space.h proto_gen.h
	$(CC) $(CFLAGS) -o space space.c $(ENGINE)

# development build: adds the --dump-space observability boundary
# (roadmap 8.4 allows debug points in a separate test build)
space-dev: space.c $(ENGINE) space.h proto_gen.h
	$(CC) $(CFLAGS) -DSPACE_DEV -o space-dev space.c $(ENGINE)

proto_gen.h: proto.def $(GEN)
	python3 $(GEN) proto.def proto_gen.h

check: all
	./tests/run_tests.sh

clean:
	rm -f space space-dev

.PHONY: all check clean
