#!/bin/sh
# End-to-end channel checks. tools/driver_gen.py builds a copy program;
# the host seeds it from the stdin channel, then the program moves data
# through the pre-bound device channels. Two ticks: seed, then data.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "need python3 for the driver generator"
  exit 1
fi

# write "ABC" to the stdout channel (space+2)
python3 tools/driver_gen.py -s 65520 -p 64 --write ABC -o /tmp/sys_drv1.bin 2>/dev/null
timeout 5 ./space -s 65520 -p 64 -t 2 < /tmp/sys_drv1.bin > /tmp/sys_drv1.out
printf 'ABC' > /tmp/sys_drv1.exp
cmp /tmp/sys_drv1.out /tmp/sys_drv1.exp
echo "ok: write to stdout channel -> ABC"

# same over a different format (address bits 15)
python3 tools/driver_gen.py -s 30000 -p 96 --write ABC -o /tmp/sys_drv2.bin 2>/dev/null
timeout 5 ./space -s 30000 -p 96 -t 2 < /tmp/sys_drv2.bin > /tmp/sys_drv2.out
cmp /tmp/sys_drv2.out /tmp/sys_drv1.exp
echo "ok: write to stdout channel over a=15 -> ABC"

# read from the stdin channel, write back to the stdout channel
python3 tools/driver_gen.py -s 65520 -p 64 --echo 4 -o /tmp/sys_drv3.bin 2>/dev/null
{ cat /tmp/sys_drv3.bin; printf 'WXYZ'; } | timeout 5 ./space -s 65520 -p 64 -t 2 > /tmp/sys_drv3.out
printf 'WXYZ' > /tmp/sys_drv3.exp
cmp /tmp/sys_drv3.out /tmp/sys_drv3.exp
echo "ok: stdin -> stdout channel roundtrip -> WXYZ"

# read at end of stream: the write still emits the requested bytes (zeros)
timeout 5 ./space -s 65520 -p 64 -t 2 < /tmp/sys_drv3.bin > /tmp/sys_drv4.out
printf '\000\000\000\000' > /tmp/sys_drv4.exp
cmp /tmp/sys_drv4.out /tmp/sys_drv4.exp
echo "ok: read at EOF -> zero bytes, no crash"
