#!/bin/sh
# Smoke + unit тесты исправленного эмулятора.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "== build =="
$CC $CFLAGS -o tests/test_bitcpy tests/test_bitcpy.c globals.c bitcopy.c
$CC $CFLAGS -o space space.c globals.c bitcopy.c
$CC $CFLAGS -o assembler assembler.c globals.c bitcopy.c
$CC $CFLAGS -o disassembler disassembler.c globals.c bitcopy.c

echo "== unit: bitcpy =="
./tests/test_bitcpy

echo "== roundtrip: assembler -> disassembler =="
printf '8 32 3072\n8 3080 0\n3024 3024 3072\n' | ./assembler > /tmp/rt.bin
./disassembler < /tmp/rt.bin > /tmp/rt.txt
cat /tmp/rt.txt
grep -qi "0x0008 0x0020 0x0C00" /tmp/rt.txt
grep -qi "0x0008 0x0C08 0x0000" /tmp/rt.txt
grep -qi "0x0BD0 0x0BD0 0x0C00" /tmp/rt.txt
echo "roundtrip OK"

echo "== bad input: assembler rejects garbage =="
if printf 'oops\n' | ./assembler > /dev/null 2>/tmp/err.txt; then
  echo "FAIL: assembler accepted garbage"; exit 1
else
  echo "assembler rejected garbage (rc=$?): $(cat /tmp/err.txt)"
fi

echo "ALL CHECKS PASSED"
