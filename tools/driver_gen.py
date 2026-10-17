#!/usr/bin/env python3
"""
In-space driver generator for the channel model.

The host seeds slot 0 with an ordinary load copy (the seed) that reads the
image from the stdin channel. The image built here is an ordinary copy
program:

  --write TEXT   materialise each byte in a NOP slot (its dst field) in the
                 bit order the space uses, then copy the bytes to the
                 stdout channel. Two slots per byte. Run with `-t 2`:
                 tick 1 seeds, tick 2 writes.
  --echo N       copy N bytes from the stdin channel into a buffer past the
                 processor, then copy them to the stdout channel. Run with
                 `-t 2`.
  --nop          an all-NOP image (the simplest stub loader). The seed
                 copies it over the processor, so the space stops on the
                 next tick. Placeholder for the real loader (copy-space).

Channels are pre-bound by position (no OPEN): stdin = space+1, stdout =
space+2. Data bytes in the space are MSB-first, so a byte is stored as
reverse8(byte) in a low-bit-first field. Layout constants match the
emulator: a = bit_length(space_bits), z = a - 1, isz = 2*a + z.
"""
import argparse
import sys


def format_of(space_bits):
    a = space_bits.bit_length()
    if a < 8:
        raise ValueError("space too small for byte data (need address bits >= 8)")
    return a, a - 1, 2 * a + (a - 1)


def reverse8(b):
    r = 0
    for _ in range(8):
        r = (r << 1) | (b & 1)
        b >>= 1
    return r


def encode(prog, a, z):
    bits = []
    for n, dst, src in prog:
        bits += [(n >> k) & 1 for k in range(z)]
        bits += [(dst >> k) & 1 for k in range(a)]
        bits += [(src >> k) & 1 for k in range(a)]
    while len(bits) % 8:
        bits.append(0)
    out = bytearray()
    for i in range(0, len(bits), 8):
        b = 0
        for j in range(8):
            b = (b << 1) | bits[i + j]
        out.append(b)
    return bytes(out)


def build_write(space_bits, proc_n, data):
    a, z, isz = format_of(space_bits)
    out_pos = space_bits + 2
    n = len(data)
    if 2 * n > proc_n:
        raise ValueError("text too long for proc_n (need 2 slots per byte)")
    prog = [(0, 0, 0)] * proc_n
    for i, byte in enumerate(data):          # NOP slots carrying the value
        prog[1 + i] = (0, reverse8(byte), 0)
    for i in range(n):                       # copy each value to stdout
        prog[1 + n + i] = (8, out_pos, (1 + i) * isz + z)
    return encode(prog, a, z), {"a": a, "z": z, "isz": isz, "slots": 2 * n}


def build_nop(space_bits, proc_n):
    a, z, isz = format_of(space_bits)
    nbits = proc_n * isz
    return bytes((nbits + 7) // 8), {"a": a, "z": z, "isz": isz, "bits": nbits}


def build_echo(space_bits, proc_n, nbytes):
    a, z, isz = format_of(space_bits)
    in_pos = space_bits + 1
    out_pos = space_bits + 2
    buf = ((proc_n * isz) + 7) // 8 * 8      # byte-aligned, past the processor
    prog = [(0, 0, 0)] * proc_n
    prog[1] = (nbytes * 8, buf, in_pos)      # read n bytes from stdin
    prog[2] = (nbytes * 8, out_pos, buf)     # write them to stdout
    return encode(prog, a, z), {"a": a, "z": z, "isz": isz, "buf": buf}


def emit(image, out):
    if out == "-":
        sys.stdout.buffer.write(image)
    else:
        with open(out, "wb") as f:
            f.write(image)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-s", "--space", type=int, required=True)
    ap.add_argument("-p", "--proc", type=int, required=True)
    ap.add_argument("-o", "--out", default="-")
    ap.add_argument("--write", dest="write_text", help="text to write to stdout")
    ap.add_argument("--echo", type=int, metavar="N",
                    help="read N bytes from stdin and write them to stdout")
    ap.add_argument("--nop", action="store_true",
                    help="emit an all-NOP stub loader image")
    args = ap.parse_args()

    if ((args.write_text is None) + (args.echo is None) + (not args.nop)) != 2:
        ap.error("give exactly one of --write, --echo or --nop")

    if args.nop:
        image, info = build_nop(args.space, args.proc)
        emit(image, args.out)
        sys.stderr.write("driver: a=%d z=%d isz=%d nop bits=%d\n"
                         % (info["a"], info["z"], info["isz"], info["bits"]))
    elif args.write_text is not None:
        data = args.write_text.encode()
        image, info = build_write(args.space, args.proc, data)
        emit(image, args.out)
        sys.stderr.write("driver: a=%d z=%d isz=%d write=%r\n"
                         % (info["a"], info["z"], info["isz"], data))
    else:
        image, info = build_echo(args.space, args.proc, args.echo)
        emit(image, args.out)
        sys.stderr.write("driver: a=%d z=%d isz=%d echo=%d\n"
                         % (info["a"], info["z"], info["isz"], args.echo))


if __name__ == "__main__":
    main()
