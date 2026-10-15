#!/bin/sh
# Emulator checks: build and end-to-end checks over stdin/stdout.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "== build =="
$CC $CFLAGS -o space space.c engine.c config.c globals.c bitcopy.c

echo "== load and run =="
./tests/boot.sh

echo "== NOP stop =="
./tests/stop.sh

echo "== transfer status =="
./tests/status.sh

echo "== channel bit transfers =="
./tests/channels.sh

echo "== guard clauses =="
./tests/guards.sh

echo "== sizes and flags =="
./tests/cfg.sh

echo "ALL CHECKS PASSED"
