#!/bin/sh
# Проверка загрузки и исполнения собранного эмулятора через stdin/stdout.
# Программа: (8,0x2000,LAST), (8,LAST,0x2000), 62 нулевые тройки, затем 'Z'.
# Значения полей — в порядке байтов хоста (поле 8 — байты 08 00).
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "нет ./space, сначала make"
  exit 1
fi
printf '' | ./space > /tmp/boot_empty.out
test ! -s /tmp/boot_empty.out
echo "ok: пустой stdin — тишина и halt"
{ printf '\010\000\000\040\357\377\010\000\357\377\000\040'; head -c 372 /dev/zero; printf 'Z'; } | timeout 5 ./space > /tmp/boot_run.out
printf '\132\132' > /tmp/boot_expected.out
cmp /tmp/boot_run.out /tmp/boot_expected.out
echo "ok: такт загрузки, такт исполнения (ZZ), такт EOF и halt"
{ head -c 100 /dev/zero; } | timeout 5 ./space > /tmp/boot_short.out
test ! -s /tmp/boot_short.out
echo "ok: короткий поток — halt после такта, наружу ничего"
