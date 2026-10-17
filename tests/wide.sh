#!/bin/sh
# Wide-format decode path (58 <= instruction_size <= 120).
# At -s 1000000: a = 20, n field 19 bits, instruction 20+20+19 = 59 bits,
# DATA = 1000000, read status 1+8+19 = 28 bits, write status 8+19 = 27.
# Program (p=3): input 16 bits to 200, copy 16 bits 200->400, output
# 16 bits from 400. Stream data 'ZZ' -> output 'ZZ'. Exercises the
# 128-bit SWAR rev decoder and the wide store_argument.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
{ printf '\064\000\002\140\000\004\205\341\000\000\046\000\004\300\003\120\000\001\041\170\114\000\055\055\000'; } | timeout 5 ./space -s 1000000 -p 3 -t 2 > /tmp/wide.out
printf '\132\132' > /tmp/wide_expected.out
cmp /tmp/wide.out /tmp/wide_expected.out
echo "ok: wide format roundtrip (-s 1000000, 59-bit instruction) -> ZZ"
