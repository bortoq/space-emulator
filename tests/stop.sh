#!/bin/sh
# Stop on NOP-filled processor: the program zeroes its own instruction record,
# outputs zero and stops. Program: (3072,0,0x2000), (8,LAST,0x3000),
# 62 zero triples. Half-intervals do not overlap.
# Field values follow host byte order.
# Tick 1 executes the previously read instructions (load, silence);
# tick 2 zeroes the record and outputs zero; tick 3: all n are zero, stop.
# Outward: a single zero byte (fifteen without quiescence).
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
{ printf '\000\014\000\000\000\040\010\000\357\377\000\060'; head -c 372 /dev/zero; } | timeout 5 ./space > /tmp/stop_run.out
head -c 1 /dev/zero > /tmp/stop_expected.out
cmp /tmp/stop_run.out /tmp/stop_expected.out
echo "ok: NOP stop (single zero byte)"
