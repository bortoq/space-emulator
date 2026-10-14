#!/bin/sh
# Unit-тесты и smoke-проверки эмулятора (единственный канал LAST).
set -e
cd "$(dirname "$0")/.."
CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "== build =="
$CC $CFLAGS -o tests/test_bitcpy tests/test_bitcpy.c globals.c bitcopy.c
$CC $CFLAGS -o tests/test_channels tests/test_channels.c engine.c globals.c bitcopy.c
$CC $CFLAGS -o space space.c engine.c globals.c bitcopy.c

echo "== unit: bitcpy =="
./tests/test_bitcpy

echo "== unit: channels =="
./tests/test_channels

echo "== smoke: space =="
# пустой stdin: ввод сразу в EOF — такт дорабатывается (1 нулевой байт наружу), halt
printf '' | ./space > /tmp/space_empty.out
head -c 1 /dev/zero > /tmp/space_one_zero
cmp /tmp/space_empty.out /tmp/space_one_zero
echo "empty stdin OK (tick finished, 1 zero byte, halt)"
# 'Hi!': 3 такта по байту + EOF-такт — наружу 4 нулевых байта
printf 'Hi!' | timeout 5 ./space > /tmp/space_io.out
head -c 4 /dev/zero > /tmp/space_four_zero
cmp /tmp/space_io.out /tmp/space_four_zero
echo "io smoke OK (4 zero bytes)"

echo "ALL CHECKS PASSED"
