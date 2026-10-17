#!/bin/sh
# Size and limit flags: -s (space bits), -p (processor instructions),
# -t (ticks). Defaults: 65520, 64, no limit. Field widths follow the
# space size: address bits a = ceil(log2 space bits + 1) (positions
# 0..space bits; the last position is the DATA channel), n field a-1
# bits, instruction 3a-1 bits. No artificial caps on -s/-p (memory
# only); processor plus the boot status record must fit
# (-p * instruction + (1+8+n field) <= -s), the boot count must fit
# the n field (-p * instruction <= 2^(a-1) - 1).
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
# smallest space: 1 instruction of 14 bits (5+5+4) plus the boot
# status record (1+8+4 bits), stream of 2 zero bytes: load tick, NOP
# stop, silence.
{ head -c 2 /dev/zero; } | timeout 5 ./space -s 27 -p 1 > /tmp/cfg_min.out
test ! -s /tmp/cfg_min.out
echo "ok: -s 27 -p 1 (smallest space)"
# -s 56: instruction 17 bits (6+6+5), 1 instruction, stream of 3 zeros.
{ head -c 3 /dev/zero; } | timeout 5 ./space -s 56 -p 1 > /tmp/cfg_56.out
test ! -s /tmp/cfg_56.out
echo "ok: -s 56 -p 1"
# -s 60 (not a power of two): 6 address bits, same 17-bit format.
{ head -c 3 /dev/zero; } | timeout 5 ./space -s 60 -p 1 > /tmp/cfg_odd.out
test ! -s /tmp/cfg_odd.out
echo "ok: -s 60 -p 1 (size without multiple of 8)"
# -s 392 -p 8: 26-bit instructions, 208-bit program = 26 zero bytes.
{ head -c 26 /dev/zero; } | timeout 5 ./space -s 392 -p 8 > /tmp/cfg_small.out
test ! -s /tmp/cfg_small.out
echo "ok: -s 392 -p 8 (load, NOP stop)"
# -s 384 -p 8 also fits now (26-bit instructions, boot 208 <= 255).
{ head -c 26 /dev/zero; } | timeout 5 ./space -s 384 -p 8 > /tmp/cfg_384.out
test ! -s /tmp/cfg_384.out
echo "ok: -s 384 -p 8"
# large space: -s 65536 -p 1, instruction 50 bits (17+17+16).
{ head -c 7 /dev/zero; } | timeout 5 ./space -s 65536 -p 1 > /tmp/cfg_maxs.out
test ! -s /tmp/cfg_maxs.out
echo "ok: -s 65536 -p 1"
# no artificial -s cap: above the old 65536 limit.
{ head -c 7 /dev/zero; } | timeout 5 ./space -s 65537 -p 1 > /tmp/cfg_big1.out
test ! -s /tmp/cfg_big1.out
echo "ok: -s 65537 -p 1 (above old cap)"
{ head -c 7 /dev/zero; } | timeout 5 ./space -s 100000 -p 1 > /tmp/cfg_big2.out
test ! -s /tmp/cfg_big2.out
echo "ok: -s 100000 -p 1 (above old cap)"
{ head -c 7 /dev/zero; } | timeout 5 ./space -s 1000000 -p 1 > /tmp/cfg_big3.out
test ! -s /tmp/cfg_big3.out
echo "ok: -s 1000000 -p 1 (above old cap)"
# maximum processor at defaults: -p 697 (boot 32759 <= 32767).
{ head -c 4 /dev/zero; } | timeout 5 ./space -p 697 -t 1 > /tmp/cfg_max.out
test ! -s /tmp/cfg_max.out
echo "ok: -p 697 (maximum at defaults)"
# explicit defaults match the implicit ones (ZZ as in boot.sh).
{ printf '\004\000\000\010\037\376\040\000\002\020\000\023\340\000\177\370\004\040\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000'; printf 'Z'; } | timeout 5 ./space -s 65520 -p 64 -t 3 > /tmp/cfg_dflt.out
printf '\132\132' > /tmp/cfg_expected.out
cmp /tmp/cfg_dflt.out /tmp/cfg_expected.out
echo "ok: default -s/-p (ZZ)"
check_err() {
  set +e
  "$@" >/dev/null 2>&1
  code=$?
  set -e
  test "$code" -eq 2
}
check_err ./space -s 21 -p 1 < /dev/null
check_err ./space -s 22 -p 1 < /dev/null
check_err ./space -s 25 -p 1 < /dev/null
check_err ./space -s 26 -p 1 < /dev/null
check_err ./space -s 4 -p 1 < /dev/null
check_err ./space -p 0 < /dev/null
check_err ./space -p 698 < /dev/null
check_err ./space -p 5887258746928580303 -s 65520 < /dev/null
check_err ./space -s 65536 -p 1311 < /dev/null
check_err ./space -s 56 -p 2 < /dev/null
check_err ./space -s 100 -p 64 < /dev/null
check_err ./space -s 384 -p 64 < /dev/null
check_err ./space -s < /dev/null
check_err ./space -x < /dev/null
check_err ./space foo < /dev/null
echo "ok: bad flags give exit code 2"
