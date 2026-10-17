#!/bin/sh
# Seed over stdin: no output on empty, short or all-NOP streams, a bad
# flag errors out, and -t bounds the run. The program itself is loaded
# from the stdin channel (proc_n * instruction_size bits), so an all-zero
# stream is an all-NOP program that stops immediately.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
printf '' | ./space -t 1 > /tmp/boot_empty.out
test ! -s /tmp/boot_empty.out
echo "ok: empty stdin, silence"
head -c 100 /dev/zero | timeout 5 ./space -t 1 > /tmp/boot_short.out
test ! -s /tmp/boot_short.out
echo "ok: short stream, silence"
head -c 376 /dev/zero | timeout 5 ./space > /tmp/boot_nop.out
test ! -s /tmp/boot_nop.out
echo "ok: all-NOP program, silence and stop"
printf '\000' | timeout 5 ./space -s 27 -p 1 > /tmp/boot_min.out
test ! -s /tmp/boot_min.out
echo "ok: stream below the seed count, silence (no -t)"
# stub loader: an all-NOP image overwrites the seed, so the space stops
if [ -f tests/stub/nop.bin ]; then
  timeout 5 ./space -s 65520 -p 64 < tests/stub/nop.bin > /tmp/boot_stub.out
  test ! -s /tmp/boot_stub.out
  echo "ok: stub loader, silence and stop (no -t)"
fi
set +e
./space foo < /dev/null >/dev/null 2>&1
code=$?
set -e
test "$code" -eq 2
echo "ok: bad flag gives exit code 2"
