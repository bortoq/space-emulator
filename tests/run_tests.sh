#!/bin/sh
# Проверки эмулятора: сборка, прямые вызовы движка, собранный эмулятор.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "== сборка =="
$CC $CFLAGS -o tests/test_bitcpy tests/test_bitcpy.c globals.c bitcopy.c
$CC $CFLAGS -o tests/test_channels tests/test_channels.c engine.c globals.c bitcopy.c
$CC $CFLAGS -o space space.c engine.c globals.c bitcopy.c

echo "== побитовые копирования =="
./tests/test_bitcpy

echo "== канал LAST =="
./tests/test_channels

echo "== загрузка и исполнение =="
./tests/boot.sh

echo "ВСЕ ПРОВЕРКИ ПРОШЛИ"
