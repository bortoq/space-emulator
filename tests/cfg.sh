#!/bin/sh
# Size and limit flags: -s (space bits), -p (processor
# instructions), -t (ticks). Defaults: 65520, 64, no limit.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
# minimum for 8 instructions: 384-bit record plus state byte outside it.
# stream of 48 zeros: load tick, NOP tick, stop, silence.
{ head -c 48 /dev/zero; } | timeout 5 ./space -s 392 -p 8 > /tmp/cfg_small.out
test ! -s /tmp/cfg_small.out
echo "ok: -s 392 -p 8 (load, NOP stop)"
# absolute minimum: 48-bit record plus 8-bit state; stream of 6 zeros.
{ head -c 6 /dev/zero; } | timeout 5 ./space -s 56 -p 1 > /tmp/cfg_min.out
test ! -s /tmp/cfg_min.out
echo "ok: -s 56 -p 1 (smallest space)"
# non-multiple-of-8 size also builds (unused tail of the byte is never addressed).
{ head -c 6 /dev/zero; } | timeout 5 ./space -s 60 -p 1 > /tmp/cfg_odd.out
test ! -s /tmp/cfg_odd.out
echo "ok: -s 60 -p 1 (size without multiple of 8)"
# explicit defaults match the implicit ones (ZZ as in boot.sh).
{ printf '\010\000\000\040\357\377\010\000\020\040\000\040\010\000\357\377\020\040'; head -c 366 /dev/zero; printf 'Z'; } | timeout 5 ./space -s 65520 -p 64 -t 3 > /tmp/cfg_dflt.out
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
check_err ./space -s 100 < /dev/null
check_err ./space -s 100000 < /dev/null
check_err ./space -p 0 < /dev/null
check_err ./space -p 64 -s 384 < /dev/null
check_err ./space -s 384 -p 8 < /dev/null
check_err ./space -s 48 -p 1 < /dev/null
check_err ./space -s < /dev/null
check_err ./space -x < /dev/null
check_err ./space foo < /dev/null
echo "ok: bad flags give exit code 2"
