#!/usr/bin/env python3
"""Generate proto_gen.h from proto.def (single source of truth).

Usage: proto_gen.py <proto.def> <out.h>

The generated header carries: operation codes, request/response field
tables (name, literal-or-symbolic width), and the device table. The
emulator (sys_ctrl.c) computes offsets and REQ_BITS/RSP_BITS from these
tables at run time, so widths may depend on the space size (ADDR, NF).
"""
import re
import sys

OP = re.compile(r'^XOP\(\s*([A-Za-z_]\w*)\s*,\s*(\d+)\s*\)$')
FIELD = re.compile(r'^XFIELD\(\s*(req|rsp)\s*,\s*([A-Za-z_]\w*)\s*,\s*(\d+|[A-Za-z_]\w*)\s*\)$')
DEV = re.compile(r'^XDEV\(\s*(\d+)\s*,\s*([A-Za-z_]\w*)\s*,\s*"([rw])"\s*\)$')

KNOWN_WIDTHS = {"ADDR": "PK_ADDR", "NF": "PK_NF"}


def fail(path, lineno, text, why):
    sys.stderr.write("%s:%d: %s: %s\n" % (path, lineno, text.strip(), why))
    raise SystemExit(1)


def parse(path):
    ops, req, rsp, devs = [], [], [], []
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    for lineno, raw in enumerate(text.splitlines(), 1):
            line = raw.split("//", 1)[0].strip()
            if not line:
                continue
            m = OP.match(line)
            if m:
                ops.append((m.group(1), int(m.group(2))))
                continue
            m = FIELD.match(line)
            if m:
                kind, name, width = m.groups()
                if width.isdigit():
                    entry = (name, int(width), "PK_LIT")
                elif width in KNOWN_WIDTHS:
                    entry = (name, 0, KNOWN_WIDTHS[width])
                else:
                    fail(path, lineno, line, "unknown width %r" % width)
                (req if kind == "req" else rsp).append(entry)
                continue
            m = DEV.match(line)
            if m:
                devs.append((int(m.group(1)), m.group(2), m.group(3)))
                continue
            fail(path, lineno, line, "unrecognized directive")
    if not ops:
        fail(path, 0, "", "no XOP")
    if not req or not rsp:
        fail(path, 0, "", "need both req and rsp fields")
    if not devs:
        fail(path, 0, "", "no XDEV")
    seen = set()
    for name, value in ops:
        if name in seen:
            fail(path, 0, name, "duplicate op")
        seen.add(name)
    return ops, req, rsp, devs


def emit(fields, name):
    out = ["static const proto_field %s[] = {" % name]
    for fname, bits, kind in fields:
        out.append('  {"%s", %d, %s},' % (fname, bits, kind))
    out.append("};")
    out.append("#define %s_NFIELDS %d" % (name.upper().replace("PROTO_", ""), len(fields)))
    return out


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: proto_gen.py <proto.def> <out.h>\n")
        return 2
    ops, req, rsp, devs = parse(sys.argv[1])
    L = []
    L.append("/* generated from %s by tools/proto_gen.py; do not edit */" % sys.argv[1])
    L.append("#ifndef PROTO_GEN_H_")
    L.append("#define PROTO_GEN_H_")
    L.append("")
    L.append("enum {")
    for name, value in ops:
        L.append("  OP_%s = %d," % (name, value))
    L.append("};")
    L.append("")
    L.append("typedef enum { PK_LIT, PK_ADDR, PK_NF } proto_width_kind;")
    L.append("typedef struct { const char *name; unsigned bits; proto_width_kind kind; } proto_field;")
    L.append("")
    L.extend(emit(req, "proto_req"))
    L.append("")
    L.extend(emit(rsp, "proto_rsp"))
    L.append("")
    L.append("typedef struct { unsigned id; const char *name; const char *mode; } proto_dev;")
    L.append("static const proto_dev proto_devs[] = {")
    for did, dname, mode in devs:
        L.append('  {%d, "%s", "%s"},' % (did, dname, mode))
    L.append("};")
    L.append("#define PROTO_NDEVS %d" % len(devs))
    L.append("")
    L.append("#endif /* PROTO_GEN_H_ */")
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
