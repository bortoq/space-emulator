#!/bin/sh
# Check load and run of the built emulator over stdin/stdout.
# Program: (8,0x2000,LAST), (8,0x2010,0x2000), (8,LAST,0x2010),
# 61 zero triples, then 'Z'. Half-intervals do not overlap.
# Field values follow host byte order (field 8 means bytes 08 00).
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
printf '' | ./space -t 1 > /tmp/boot_empty.out
test ! -s /tmp/boot_empty.out
echo "ok: empty stdin, silence"
{ printf '\010\000\000\040\357\377\010\000\020\040\000\040\010\000\357\377\020\040'; head -c 366 /dev/zero; printf 'Z'; } | timeout 5 ./space -t 3 > /tmp/boot_run.out
printf '\132\132' > /tmp/boot_expected.out
cmp /tmp/boot_run.out /tmp/boot_expected.out
echo "ok: load tick, run tick (ZZ), EOF tick"
{ head -c 100 /dev/zero; } | timeout 5 ./space -t 1 > /tmp/boot_short.out
test ! -s /tmp/boot_short.out
echo "ok: short stream, silence"
{ printf '\010\000\000\040\357\377\010\000\020\040\000\040\010\000\357\377\020\040'; head -c 366 /dev/zero; printf 'Z'; } | timeout 5 ./space -t 2 > /tmp/boot_limit.out
printf 'Z' > /tmp/boot_limit_expected.out
cmp /tmp/boot_limit.out /tmp/boot_limit_expected.out
echo "ok: tick limit flag (2 ticks -> Z)"
set +e
./space foo < /dev/null >/dev/null 2>&1
code=$?
set -e
test "$code" -eq 2
echo "ok: bad flag gives exit code 2"
