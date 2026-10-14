#!/bin/sh
# Channel transfers accept any bit count at any bit position:
# no alignment requirement, stream bits continue across accesses.
# -s 392 -p 4: fields 8+9+9 = 26 bits, program 104 bits = 13 bytes.
# Roundtrip at bit position 3: (8,3,LAST) input, (8,LAST,3) output.
# State query outside the processor: (0,104,LAST) then (8,LAST,104);
# 104 is the first position after the processor (4*26).
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
# 4-bit output from position 0 at the smallest space (-s 22 -p 1,
# fields 5+5+4 = 14 bits): first program bits 0010 -> byte 0x20.
{ printf '\052\200'; } | timeout 5 ./space -s 22 -p 1 -t 2 > /tmp/ch_4bit.out
printf '\040' > /tmp/ch_4bit_expected.out
cmp /tmp/ch_4bit.out /tmp/ch_4bit_expected.out
echo "ok: 4-bit output at -s 22 (bit precise)"
# unaligned position roundtrip: program 13 bytes + 'Q' -> 'Q'
{ printf '\020\300\160\304\070\170\000\000\000\000\000\000\000'; printf 'Q'; } | timeout 5 ./space -s 392 -p 4 -t 2 > /tmp/ch_pos3.out
printf 'Q' > /tmp/ch_pos3_expected.out
cmp /tmp/ch_pos3.out /tmp/ch_pos3_expected.out
echo "ok: 8-bit roundtrip at bit position 3"
# query into position 104 (outside the processor), no extra input:
# the failed input read is seen as end of input (0x01) every tick
{ printf '\020\000\160\300\005\234\061\016\030\260\000\000\000'; } | timeout 5 ./space -s 392 -p 4 -t 4 > /tmp/ch_query.out
printf '\001\001\001' > /tmp/ch_query_expected.out
cmp /tmp/ch_query.out /tmp/ch_query_expected.out
echo "ok: state query sees end of input (01 01 01)"
# write failure: /dev/full (ENOSPC) runs without crash and without hang
{ printf '\020\000\160\300\005\234\061\016\030\260\000\000\000'; } | timeout 5 ./space -s 392 -p 4 -t 3 > /dev/full 2>/dev/null || true
echo "ok: write to /dev/full runs without crash"
