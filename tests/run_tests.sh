#!/bin/sh
# Emulator checks (host side: engine, bit copy, config, SYS protocol,
# channel data path). Copycode tests live separately in tests/copycode/
# and are not run here; tests/legacy/ keeps old DATA-channel sequences.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -O3 -flto"

echo "== generate protocol =="
python3 tools/proto_gen.py proto.def proto_gen.h

echo "== build =="
$CC $CFLAGS -o space space.c engine.c config.c sys_ctrl.c globals.c bitcopy.c

echo "== SYS unit tests =="
$CC $CFLAGS -I. -o /tmp/test_sys tests/test_sys.c engine.c config.c sys_ctrl.c globals.c bitcopy.c
/tmp/test_sys

echo "== SYS end-to-end driver =="
./tests/sys_e2e.sh

echo "== config limits =="
./tests/cfg.sh

echo "== wide format =="
./tests/wide.sh

echo "== boot =="
./tests/boot.sh

echo "ALL CHECKS PASSED"
