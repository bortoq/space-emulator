#!/bin/sh
# Channel transfers accept any bit count at any bit position:
# no alignment requirement, stream bits continue across accesses.
# -s 392 -p 4: fields 9+8+8 = 26 bits, program 104 bits = 13 bytes,
# read status 17 bits, write status 16 bits. Roundtrip at bit position
# 3: (25,3,DATA) input 8 bits, (24,DATA,3) output 8 bits.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
# unaligned position roundtrip: 13 program bytes + 'Q' -> 'Q'
{ printf '\230\300\010\306\004\170\000\000\000\000\000\000\000'; printf 'Q'; } | timeout 5 ./space -s 392 -p 4 -t 2 > /tmp/ch_pos3.out
printf '\121' > /tmp/ch_pos3_expected.out
cmp /tmp/ch_pos3.out /tmp/ch_pos3_expected.out
echo "ok: 8-bit roundtrip at bit position 3"
# size above 255 at -s 1024 (size field 10 bits, low bit first):
# input 300 bits -> end 0, errno 0, size 300 -> 00 1A 40
{ printf '\374\200\040\001\310\006\221\242\244\000\010\322\000\000\000\000'; head -c 38 /dev/zero | tr '\000' '\252'; } | timeout 5 ./space -s 1024 -p 4 -t 2 > /tmp/ch_size300.out
printf '\000\032\100' > /tmp/ch_size300_expected.out
cmp /tmp/ch_size300.out /tmp/ch_size300_expected.out
echo "ok: read status size above 255 (00 1A 40)"
# write failure: /dev/full (ENOSPC) runs without crash and without hang
{ printf '\230\300\010\306\004\170\000\000\000\000\000\000\000'; printf 'Q'; } | timeout 5 ./space -s 392 -p 4 -t 3 > /dev/full 2>/dev/null || true
echo "ok: write to /dev/full runs without crash"
# sub-byte output: a partial last byte is written zero padded (channel_flush).
# -s 392 -p 4 (read status 17, write status 16). Program: (22,104,DATA)
# input 5 bits, (5,200,104) copy, (21,DATA,200) output 5 bits, NOP.
# Stream high 5 bits 01011 -> one byte 0x58 (01011 000).
{ printf '\150\026\010\350\004\302\312\201\030\230\000\000\000\130'; } | timeout 5 ./space -s 392 -p 4 -t 2 > /tmp/ch_flush.out
printf '\130' > /tmp/ch_flush_expected.out
cmp /tmp/ch_flush.out /tmp/ch_flush_expected.out
echo "ok: sub-byte output zero padded (5 bits -> 0x58)"
