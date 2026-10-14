#!/bin/sh
# Channel state queries with NOPs: (0,dst,LAST) is the read side,
# (0,LAST,src) is the write side; k = 8 bits (bit 0 means end/failure).
# Program: query, input, query, output, output, query, output + 'Q'.
# Tick 1: load (silence); tick 2: all zero except the input 'Q';
# tick 3: the second query sees the end; tick 4: both queries see the end.
# Outward: 00 00 00 | 00 01 00 | 01 01 00.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
{ printf '\000\000\000\040\357\377\010\000\020\040\357\377\000\000\040\040\357\377\010\000\357\377\000\040\010\000\357\377\040\040\000\000\357\377\060\040\010\000\357\377\060\040'; head -c 342 /dev/zero; printf 'Q'; } | timeout 5 ./space -t 4 > /tmp/state_run.out
printf '\000\000\000\000\001\000\001\001\000' > /tmp/state_expected.out
cmp /tmp/state_run.out /tmp/state_expected.out
echo "ok: state queries see the end (00 00 00, 00 01 00, 01 01 00)"
