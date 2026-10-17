#!/bin/sh
# Emulator configuration limits (-s/-p/-t).
# With a = ceil(log2(space bits + 1)): instruction 3a-1 bits, and
#   p*(3a-1) <= s,  p*(3a-1) <= 2^(a-1)-1,  s + 1 + NDEV <= 2^a.
# Smallest: -s 16 -p 1 (a=5). A wrong flag or violated limit -> exit 2.
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
run() { printf '' | timeout 5 ./space "$@" -t 1 >/dev/null 2>&1; echo $?; }
chk() {
  d=$1; exp=$2; shift 2
  code=$(run "$@")
  if [ "$code" -ne "$exp" ]; then
    echo "FAIL: $d (got $code, want $exp)"; exit 1
  fi
  echo "ok: $d"
}
chk "defaults"                       0
chk "smallest -s 16 -p 1"            0 -s 16 -p 1
chk "below smallest -s 15 -p 1"      2 -s 15 -p 1
chk "no channel room -s 31 -p 1"     2 -s 31 -p 1
chk "processor fits -s/-p"           0 -s 65520 -p 697
chk "processor overflow"             2 -s 65520 -p 698
chk "seed count overflow -s 27 -p 2" 2 -s 27 -p 2
chk "-s 0"                           2 -s 0 -p 1
chk "-p 0"                           2 -s 27 -p 0
chk "bad flag"                       2 foo
chk "-t overflow"                    2 -t 99999999999999999999999
