#!/bin/sh
# In-space SYS driver copycode (roadmap §11 phase 3).
#
# tools/sys_gen.py builds a copy program that runs inside the space and
# exercises the whole SYS path from there: it builds an FSTAT request
# record with ordinary copies, copies the record into SYS, reads the
# answer back through SYS, and writes the answer record to the stdout
# data channel. A write to a processor position takes effect from the
# next tick, so the record is built one tick before it is issued and the
# first (warm-up) answer is empty. With -t 6 the four answer records are
#   op=0 (warm-up), op=3, op=3 moved=14, op=3 moved=14;
# moved=14 is the 14-byte answer the driver wrote to the stdout channel
# on the preceding tick, so the answer proves both directions of SYS.
#
# Two formats are checked, a=16 (-s 65520) and a=15 (-s 30000), so the
# field widths resolved from the space size are covered.
set -e
cd "$(dirname "$0")/../.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "need python3 for the driver generator"
  exit 1
fi

check()
{
  s=$1; p=$2; a=$3; z=$4
  python3 tools/sys_gen.py -s "$s" -p "$p" -o /tmp/sys_drv.bin 2>/dev/null
  timeout 5 ./space -s "$s" -p "$p" -t 6 < /tmp/sys_drv.bin > /tmp/sys_drv.out
  python3 - /tmp/sys_drv.out "$a" "$z" << 'PY'
import sys
data = open(sys.argv[1], "rb").read()
a = int(sys.argv[2]); z = int(sys.argv[3])
rsp_bits = 8 + 8 + 16 + 64 + z + 1
rsp_bytes = (rsp_bits + 7) // 8
stride = rsp_bytes * 8
if len(data) % rsp_bytes:
    raise SystemExit("output is not a whole number of answer records: %d bytes" % len(data))
bits = []
for b in data:
    for k in range(7, -1, -1):
        bits.append((b >> k) & 1)
off = [0, 8, 16, 32, 96, 96 + z]
w   = [8, 8, 16, 64, z, 1]
name = ["op", "status", "flags", "size", "moved", "eof"]
recs = []
for r in range(len(data) // rsp_bytes):
    chunk = bits[r * stride:(r + 1) * stride]
    v = []
    for o, ww in zip(off, w):
        x = 0
        for k in range(ww):
            x |= chunk[o + k] << k
        v.append(x)
    recs.append(dict(zip(name, v)))
for i, rec in enumerate(recs):
    print("rec%d:" % i, " ".join("%s=%d" % kv for kv in rec.items()))
if len(recs) != 4:
    raise SystemExit("want 4 answer records for -t 6, got %d" % len(recs))
last = recs[-1]
if not (last["op"] == 3 and last["status"] == 0 and last["moved"] == 14
        and last["eof"] == 0):
    raise SystemExit("last answer is not op=3/moved=14: %r" % last)
if len([r for r in recs if r["op"] == 3]) != 3:
    raise SystemExit("want 3 FSTAT answers, got %r" % recs)
if recs[0]["op"] != 0:
    raise SystemExit("warm-up answer should be op=0: %r" % recs[0])
PY
  echo "ok: in-space SYS driver a=$a (-s $s -p $p): FSTAT request/answer + 14-byte write"
}

check 65520 64 16 15
check 30000 64 15 14
echo "ok: copycode in-space SYS driver"
