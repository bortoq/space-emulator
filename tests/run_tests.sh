#!/bin/sh
# Unit-тесты исправленного эмулятора.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "== build =="
$CC $CFLAGS -o tests/test_bitcpy tests/test_bitcpy.c globals.c bitcopy.c
$CC $CFLAGS -o space space.c globals.c bitcopy.c

echo "== unit: bitcpy =="
./tests/test_bitcpy

echo "== smoke: space =="
printf '' | ./space | xxd > /tmp/space_empty.txt
test ! -s /tmp/space_empty.txt
echo "empty stdin OK (no output)"
printf 'Hi!' | timeout 5 ./space | xxd > /tmp/space_io.txt
cat /tmp/space_io.txt
echo "io smoke OK"

echo "ALL CHECKS PASSED"
