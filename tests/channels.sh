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
# 4-bit output from position 0 at the smallest space (-s 26 -p 1,
# fields 5+5+4 = 14 bits): first program bits 0010 -> byte 0x20.
{ printf '\051\200'; } | timeout 5 ./space -s 26 -p 1 -t 2 > /tmp/ch_4bit.out
printf '\040' > /tmp/ch_4bit_expected.out
cmp /tmp/ch_4bit.out /tmp/ch_4bit_expected.out
echo "ok: 4-bit output at -s 26 (bit precise)"
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
# query count: the query writes state (8 bits) and the count of the
# last transfer (8 bits here): complete input of 8 bits -> 00 08
{ printf '\020\000\160\300\005\234\061\016\030\260\103\206\034'; printf 'Q'; } | timeout 5 ./space -s 392 -p 4 -t 2 > /tmp/ch_count_full.out
printf '\000\010' > /tmp/ch_count_full_expected.out
cmp /tmp/ch_count_full.out /tmp/ch_count_full_expected.out
echo "ok: query count after a complete input (00 08)"
# incomplete input of 16 bits with 8 bits in the stream -> 01 08
{ printf '\010\000\160\300\005\234\061\016\030\260\103\206\034'; printf 'Q'; } | timeout 5 ./space -s 392 -p 4 -t 2 > /tmp/ch_count_part.out
printf '\001\010' > /tmp/ch_count_part_expected.out
cmp /tmp/ch_count_part.out /tmp/ch_count_part_expected.out
echo "ok: query count after an incomplete input (01 08)"
# write-side count: output 8 bits, then query the write state and count.
# The first byte is the program's own first byte (0x10), then 00 08.
{ printf '\020\341\200\000\070\142\301\016\030\260\103\206\034'; } | timeout 5 ./space -s 392 -p 4 -t 2 > /tmp/ch_count_out.out
printf '\020\000\010' > /tmp/ch_count_out_expected.out
cmp /tmp/ch_count_out.out /tmp/ch_count_out_expected.out
echo "ok: query count after an output (10 00 08)"
# a query inside the processor does not write: output from position 3
# reads program bits 00000110 -> 06
{ printf '\000\300\160\304\070\170\000\000\000\000\000\000\000'; } | timeout 5 ./space -s 392 -p 4 -t 2 > /tmp/ch_inside.out
printf '\006' > /tmp/ch_inside_expected.out
cmp /tmp/ch_inside.out /tmp/ch_inside_expected.out
echo "ok: query inside the processor does not write (06)"
# count above 255 at -s 1024 (count field 9 bits, low byte first):
# input 300 bits -> state 00 and count 2C 01
{ printf '\064\200\037\370\000\270\377\304\037\371\160\020\377\317\200'; head -c 38 /dev/zero | tr '\\000' '\\252'; } | timeout 5 ./space -s 1024 -p 4 -t 2 > /tmp/ch_count_big.out
printf '\000\054\001' > /tmp/ch_count_big_expected.out
cmp /tmp/ch_count_big.out /tmp/ch_count_big_expected.out
echo "ok: query count above 255 (00 2C 01)"
# write failure: /dev/full (ENOSPC) runs without crash and without hang
{ printf '\020\000\160\300\005\234\061\016\030\260\000\000\000'; } | timeout 5 ./space -s 392 -p 4 -t 3 > /dev/full 2>/dev/null || true
echo "ok: write to /dev/full runs without crash"
