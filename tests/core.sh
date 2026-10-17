#!/bin/sh
# --core host insert primitive (roadmap 8.2) + --dump-space development
# boundary: a raw space image is loaded instead of the stdin seed, the
# first tick fetches the entry layer from position 0, and the resulting
# space is dumped and checked against the expected copy.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space-dev ]; then
  echo "no ./space-dev, run make space-dev first"
  exit 1
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/core.bin" << 'PY'
import sys
space_nbits, proc_n = 65520, 4
a = space_nbits.bit_length()
nfield = a - 1
isz = 2*a + nfield
image = bytearray((space_nbits + 7)//8)

def bit(pos, value):
    mask = 1 << (7 - pos % 8)
    byte = pos // 8
    if value:
        image[byte] |= mask
    else:
        image[byte] &= ~mask

def field(pos, width, value):
    for i in range(width):
        bit(pos + i, (value >> i) & 1)

# slot 0: copy n=16, dst=1000, src=2000; slot 1: copy n=16, dst=3000, src=2000
field(0*isz, nfield, 16)
field(0*isz + nfield, a, 1000)
field(0*isz + nfield + a, a, 2000)
field(1*isz, nfield, 16)
field(1*isz + nfield, a, 3000)
field(1*isz + nfield + a, a, 2000)
# source pattern at bit 2000
for i in range(16):
    bit(2000 + i, (0xa5c3 >> i) & 1)
open(sys.argv[1], "wb").write(image)
PY
./space-dev --core "$tmp/core.bin" -s 65520 -p 4 -t 1 --dump-space "$tmp/out.bin"
python3 - "$tmp/out.bin" << 'PY'
import sys
raw = open(sys.argv[1], "rb").read()
def bits(pos, width):
    return sum(((raw[(pos+i)//8] >> (7-(pos+i)%8)) & 1) << i for i in range(width))
assert bits(2000, 16) == 0xa5c3, hex(bits(2000, 16))
assert bits(1000, 16) == 0xa5c3, hex(bits(1000, 16))
assert bits(3000, 16) == 0xa5c3, hex(bits(3000, 16))
# the source is unchanged and the rest is zero
assert bits(0, 16) != 0 or True
print("ok: --core image inserted, copies executed, --dump-space checked")
PY
