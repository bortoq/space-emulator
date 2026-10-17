#!/usr/bin/env python3
"""In-space SYS driver copycode generator.

Builds a copy program (the in-space driver) that exercises the SYS
request/answer path from inside the space:

  * builds a FSTAT request record with ordinary copies (op and chan fields
    are copied from constant-carrying NOP slots);
  * issues the request: a copy with dst = SYS and src = the record address;
  * reads the answer: a copy with dst = the answer buffer and src = SYS;
  * outputs the answer record to the stdout data channel.

Tick rule: a write to a processor position takes effect from the next tick,
so the request record is built one tick before it is issued and the output
instruction is enabled one tick before it must fire.

Order rule: every tick performs only copies whose half-intervals are
disjoint, so the result does not depend on the order of copies inside a
tick.

Format matches the emulator: a = bit_length(space_bits), z = a - 1,
isz = 2*a + z. Record fields follow proto.def: REQ op(8) chan(ADDR)
flags(16) arg(32) off(64); RSP op(8) status(8) flags(16) size(64) moved(NF)
eof(1).
"""
import argparse
import sys


def format_of(space_bits):
    a = space_bits.bit_length()
    if a < 8:
        raise ValueError("space too small for the SYS driver (need address bits >= 8)")
    return a, a - 1, 2 * a + (a - 1)


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


def offsets(widths):
    off, at = [], 0
    for w in widths:
        off.append(at)
        at += w
    return off, at


def build(space_bits, proc_n):
    a, z, isz = format_of(space_bits)
    if proc_n < 16:
        raise ValueError("proc_n too small for the driver")
    sys_pos = space_bits
    out_pos = space_bits + 2
    if out_pos >= (1 << a):
        raise ValueError("no room for the stdout channel")

    # record layouts from proto.def, resolved against the space size
    req_w = [8, a, 16, 32, 64]
    req_off, req_bits = offsets(req_w)
    rsp_w = [8, 8, 16, 64, z, 1]
    rsp_off, rsp_bits = offsets(rsp_w)

    proc_bits = proc_n * isz
    record = ((proc_bits + 7) // 8) * 8                # request record (bit addr)
    answer = record + ((req_bits + 7) // 8) * 8        # answer buffer

    op_const, chan_const, out_n_const = 0, 1, 7
    build_op, build_chan, issue, read_answer = 2, 3, 4, 5
    output_answer, set_output_n = 6, 8

    prog = [(0, 0, 0)] * proc_n
    prog[op_const] = (0, 3, 0)                         # OP_FSTAT = 3
    prog[chan_const] = (0, out_pos, 0)
    prog[build_op] = (8, record + req_off[0], op_const * isz + z)
    prog[build_chan] = (a, record + req_off[1], chan_const * isz + z)
    prog[issue] = (1, sys_pos, record)
    prog[read_answer] = (rsp_bits, answer, sys_pos)
    prog[output_answer] = (0, out_pos, answer)         # gated: n starts at 0
    prog[out_n_const] = (0, rsp_bits, 0)
    prog[set_output_n] = (z, output_answer * isz, out_n_const * isz + z)

    image = encode(prog, a, z)
    meta = {"a": a, "z": z, "isz": isz, "req_bits": req_bits,
            "rsp_bits": rsp_bits, "record": record, "answer": answer,
            "sys_pos": sys_pos, "out_pos": out_pos, "ticks": 4}
    return image, meta


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
    args = ap.parse_args()

    image, meta = build(args.space, args.proc)
    emit(image, args.out)
    sys.stderr.write("sys_driver: " +
                     " ".join("%s=%s" % kv for kv in meta.items()) + "\n")


if __name__ == "__main__":
    main()
