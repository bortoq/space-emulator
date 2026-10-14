#!/bin/sh
# Проверки эмулятора: сборка и сквозные проверки через stdin/stdout.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "== сборка =="
$CC $CFLAGS -o space space.c engine.c config.c globals.c bitcopy.c

echo "== загрузка и исполнение =="
./tests/boot.sh

echo "== останов NOPами =="
./tests/stop.sh

echo "== запросы состояния =="
./tests/state.sh

echo "== размеры и флаги =="
./tests/cfg.sh

echo "ВСЕ ПРОВЕРКИ ПРОШЛИ"
