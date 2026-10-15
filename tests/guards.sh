#!/bin/sh
# Guard clauses: bad instructions are skipped, the tick goes on.
# -s 392 -p 4: fields 9+8+8 = 26 bits, program 104 bits = 13 bytes,
# read status 17 bits, write status 16 bits. DATA = 392.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
# ordinary copy out of bounds (dst+n > space): skipped, then the next
# instruction outputs the first 8 program bits (n field of instr 0,
# value 200 = 1100 1000, first bit high) -> 13
{ printf '\023\064\200\006\004\140\000\000\000\000\000\000\000'; } | timeout 5 ./space -s 392 -p 4 -t 2 > /tmp/guard_oob.out
printf '\023' > /tmp/guard_oob_expected.out
cmp /tmp/guard_oob.out /tmp/guard_oob_expected.out
echo "ok: copy out of bounds is skipped (13)"
# input with n below the status width (1 < 17): skipped, then the next
# instruction outputs the first 8 program bits (n field of instr 0,
# value 1 = 0000 0001, first bit high) -> 80
{ printf '\200\300\010\306\004\140\000\000\000\000\000\000\000'; } | timeout 5 ./space -s 392 -p 4 -t 2 > /tmp/guard_small.out
printf '\200' > /tmp/guard_small_expected.out
cmp /tmp/guard_small.out /tmp/guard_small_expected.out
echo "ok: input below the status width is skipped (80)"
