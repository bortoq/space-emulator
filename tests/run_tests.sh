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
# пустой stdin: загрузке нечего читать — halt, в ROM вывода нет (тишина)
printf '' | ./space > /tmp/space_empty.out
test ! -s /tmp/space_empty.out
echo "empty stdin OK (no output, halt)"
# программа: (8,0x2000,LAST), (8,LAST,0x2000), нули — потом 'Z'.
# такт 1: двухфазно — исполняется старый образ (загрузка, тишина);
# такт 2: ввод 'Z' + вывод 'Z'; такт 3: EOF — доработать (вывод 'Z') и halt.
# наружу: 5A 5A.
{ printf '\010\000\000\040\357\377\010\000\357\377\000\040'; head -c 372 /dev/zero; printf 'Z'; } | timeout 5 ./space > /tmp/space_io.out
printf '\132\132' > /tmp/space_expected.out
cmp /tmp/space_io.out /tmp/space_expected.out
echo "boot+run smoke OK (5A 5A)"

echo "ALL CHECKS PASSED"
