#!/bin/sh
# Size and limit flags: -s (space bits), -p (processor instructions),
# -t (ticks). Defaults: 65520, 64, no limit. Field widths follow the
# space size: address bits a = ceil(log2 space bits), n field a-1
# bits, instruction 3a-1 bits. Limits: 8 <= -s, processor plus the
# state byte outside it must fit (-p * instruction <= -s - 8), the
# boot count must fit the n field (-p * instruction <= 2^(a-1) - 1).
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
# smallest space: 1 instruction of 14 bits (5+5+4) plus the query
# record (8+4 bits), stream of 2 zero bytes: load tick, NOP stop,
# silence.
{ head -c 2 /dev/zero; } | timeout 5 ./space -s 26 -p 1 > /tmp/cfg_min.out
test ! -s /tmp/cfg_min.out
echo "ok: -s 26 -p 1 (smallest space)"
# -s 56: instruction 17 bits (6+5+6), 1 instruction, stream of 3 zeros.
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
# maximum processor at defaults: -p 697 (boot 32759 <= 32767).
{ head -c 4 /dev/zero; } | timeout 5 ./space -p 697 -t 1 > /tmp/cfg_max.out
test ! -s /tmp/cfg_max.out
echo "ok: -p 697 (maximum at defaults)"
# explicit defaults match the implicit ones (ZZ as in boot.sh).
{ printf '\020\000\000\011\357\376\040\000\040\020\000\020\100\007\277\370\100\040\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000'; printf 'Z'; } | timeout 5 ./space -s 65520 -p 64 -t 3 > /tmp/cfg_dflt.out
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
check_err ./space -s 4 -p 1 < /dev/null
check_err ./space -s 100000 < /dev/null
check_err ./space -p 0 < /dev/null
check_err ./space -p 698 < /dev/null
check_err ./space -s 56 -p 2 < /dev/null
check_err ./space -s 100 -p 64 < /dev/null
check_err ./space -s 384 -p 64 < /dev/null
check_err ./space -s < /dev/null
check_err ./space -x < /dev/null
check_err ./space foo < /dev/null
echo "ok: bad flags give exit code 2"
