#!/bin/sh
# Emulator wide-format decode (58 <= instruction_size <= 120, the
# rev128 path). At -s 1048576: a = 21, instruction 2*21+(21-1) = 62 bits.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
python3 tools/driver_gen.py -s 1048576 -p 8 --write OK -o /tmp/wide.bin 2>/dev/null
timeout 5 ./space -s 1048576 -p 8 -t 2 < /tmp/wide.bin > /tmp/wide.out
printf 'OK' > /tmp/wide.exp
cmp /tmp/wide.out /tmp/wide.exp
echo "ok: wide format (isz 62) write -> OK"
python3 tools/driver_gen.py -s 1048576 -p 8 --echo 2 -o /tmp/wide_e.bin 2>/dev/null
{ cat /tmp/wide_e.bin; printf 'HI'; } | timeout 5 ./space -s 1048576 -p 8 -t 2 > /tmp/wide_e.out
printf 'HI' > /tmp/wide_e.exp
cmp /tmp/wide_e.out /tmp/wide_e.exp
echo "ok: wide format (isz 62) echo -> HI"
