#!/bin/sh
# Флаги размеров и предела: -s (биты пространства), -p (инструкции
# процессора), -t (такты). Значения по умолчанию: 65520, 64, без предела.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "нет ./space, сначала make"
  exit 1
fi
# минимум для 8 инструкций: запись 384 бита + байт состояния вне её.
# поток 48 нулей: такт загрузки, такт NOP — останов, тишина.
{ head -c 48 /dev/zero; } | timeout 5 ./space -s 392 -p 8 > /tmp/cfg_small.out
test ! -s /tmp/cfg_small.out
echo "ok: -s 392 -p 8 (загрузка, останов NOPами)"
# абсолютный минимум: 48 бит записи + 8 бит состояния; поток 6 нулей.
{ head -c 6 /dev/zero; } | timeout 5 ./space -s 56 -p 1 > /tmp/cfg_min.out
test ! -s /tmp/cfg_min.out
echo "ok: -s 56 -p 1 (наименьшее пространство)"
# некратное 8 тоже строится (лишний хвост байта не адресуется).
{ head -c 6 /dev/zero; } | timeout 5 ./space -s 60 -p 1 > /tmp/cfg_odd.out
test ! -s /tmp/cfg_odd.out
echo "ok: -s 60 -p 1 (размер без кратности 8)"
# явные значения по умолчанию совпадают с неявными (ZZ как в boot.sh).
{ printf '\010\000\000\040\357\377\010\000\020\040\000\040\010\000\357\377\020\040'; head -c 366 /dev/zero; printf 'Z'; } | timeout 5 ./space -s 65520 -p 64 -t 3 > /tmp/cfg_dflt.out
printf '\132\132' > /tmp/cfg_expected.out
cmp /tmp/cfg_dflt.out /tmp/cfg_expected.out
echo "ok: -s/-p по умолчанию (ZZ)"
check_err() {
  set +e
  "$@" >/dev/null 2>&1
  code=$?
  set -e
  test "$code" -eq 2
}
check_err ./space -s 100 < /dev/null
check_err ./space -s 100000 < /dev/null
check_err ./space -p 0 < /dev/null
check_err ./space -p 64 -s 384 < /dev/null
check_err ./space -s 384 -p 8 < /dev/null
check_err ./space -s 48 -p 1 < /dev/null
check_err ./space -s < /dev/null
check_err ./space -x < /dev/null
check_err ./space foo < /dev/null
echo "ok: неверные флаги — код 2"
