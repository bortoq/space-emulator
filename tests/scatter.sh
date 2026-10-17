#!/bin/sh
# Побитовый scatter/gather roundtrip на копикоде.
# Копипрограмма (генерируется из сида): читает из канала X=64 бит,
# копирует каждый бит в свою произвольную позицию памяти (кроме
# процессора, по 1 биту на позицию), затем собирает биты из этих мест
# в одно место в том же порядке, в котором читала, и пишет в канал.
# Скрипт генерирует из сида и значения, и позиции, запускает
# пространство и сверяет записанное в канал с выданным из канала.
# X=(proc_n-6)/2: каждый бит занимает 2 инструкции (разброс+сбор),
# 6 инструкций — обслуживание (вход, выход, 2 CTRL, 2 константы),
# итого 2*64+6=134 из -p 136 (2 NOP запас). Больше при 1 бит/позиция
# не влезет. Параметры: -s 8192 -p 136: a=14, nfield=13, инструкция
# 41 бит, программа 5576 бит = 697 байт, DATA=8192. Sr=22, Sw=21,
# вход n=86, выход n=85. Позиции (IN, OUT, 64 шт. S) выбираются
# генератором копипрограммы из сида (random.sample вне процессора и
# вне буферов), значения — из того же сида. Конвейер по тикам (записи
# в процессор действуют со следующего тика): тик2: вход+разброс,
# CTRL1 взводит CTRL2; тик3: сбор, CTRL2 взводит выход; тик4: выход
# один раз. -t 4 даёт ровно 8 байт выхода.
set -e
cd "$(dirname "$0")/.."
if [ ! -x ./space ]; then
  echo "no ./space, run make first"
  exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "need python3 for program generation"
  exit 1
fi
# Сид: SEED=число для повтора, иначе каждый запуск новый произвольный.
if [ -n "$SEED" ]; then
  BASE_SEED="$SEED"
  NTRIALS=1
else
  BASE_SEED="$(head -c 4 /dev/urandom | od -An -tu4 | tr -d ' ')"
  NTRIALS=5
fi
t=0
while [ "$t" -lt "$NTRIALS" ]; do
  if [ "$NTRIALS" -eq 1 ] && [ -n "$SEED" ]; then
    TRIAL_SEED="$BASE_SEED"
  else
    TRIAL_SEED="$(python3 -c "import random; print((($BASE_SEED+$t*7919) & 0xFFFFFFFF))")"
  fi
  SEED="$TRIAL_SEED" python3 - << 'PY' > /tmp/scatter_prog.bin
import random, os, sys
seed=int(os.environ["SEED"])
rng=random.Random(seed)
S_SPACE=8192; P_NUM=136; A=14; NF=13; INSTR=41; DATA=8192
X=64
N_IN=X+(1+8+NF); N_OUT=X+(8+NF)
P_BITS=P_NUM*INSTR; REC=1+8+NF; USTART=P_BITS+REC
# Произвольные IN/OUT из сида (вне процессора, интервалы не пересекаются).
for _ in range(1000):
    in_b=rng.randrange(USTART, S_SPACE-N_IN+1)
    out_b=rng.randrange(USTART, S_SPACE-N_OUT+1)
    if not (in_b < out_b+N_OUT and out_b < in_b+N_IN):
        break
else:
    raise SystemExit("no place")
IN_BUF=in_b; OUT_BUF=out_b
ex=set(range(IN_BUF,IN_BUF+N_IN))|set(range(OUT_BUF,OUT_BUF+N_OUT))
pool=[p for p in range(USTART,S_SPACE) if p not in ex]
S=rng.sample(pool,X)  # по одному биту на произвольную позицию
data_bytes=bytes(rng.getrandbits(8) for _ in range(X//8))
open('/tmp/scatter_data.bin','wb').write(data_bytes)
OUT_IDX=1+2*X; C1=OUT_IDX+1; C2=OUT_IDX+2; H13=OUT_IDX+3; HOUT=OUT_IDX+4
assert HOUT < P_NUM
OUT_N_POS=OUT_IDX*INSTR; C2_N_POS=C2*INSTR
H13_DST=H13*INSTR+NF; HOUT_DST=HOUT*INSTR+NF
prog=[(N_IN,IN_BUF,DATA)]
for i in range(X): prog.append((1,S[i],IN_BUF+i))    # разброс побитово
for i in range(X): prog.append((1,OUT_BUF+i,S[i]))   # сбор в том же порядке
prog.append((0,DATA,OUT_BUF))                          # выход (взводится в тик4)
prog.append((13,C2_N_POS,H13_DST))                     # CTRL1
prog.append((0,OUT_N_POS,HOUT_DST))                    # CTRL2
prog.append((0,13,0)); prog.append((0,N_OUT,0))        # константы 13, N_OUT
while len(prog)<P_NUM: prog.append((0,0,0))
assert len(prog)==P_NUM
bits=[]
for (n,d,s) in prog:
    for k in range(NF): bits.append((n>>k)&1)
    for k in range(A): bits.append((d>>k)&1)
    for k in range(A): bits.append((s>>k)&1)
assert len(bits)%8==0
ob=bytearray()
for i in range(0,len(bits),8):
    b=0
    for j in range(8): b=(b<<1)|bits[i+j]
    ob.append(b)
sys.stdout.buffer.write(ob)
# Мета для лога (позиции — выбор копипрограммы из сида).
sys.stderr.write(f"seed={seed} IN={IN_BUF} OUT={OUT_BUF} S_mod={[s%8 for s in S[:8]]}... data={data_bytes.hex()}\n")
PY
  test "$(wc -c < /tmp/scatter_prog.bin)" -eq 697
  test "$(wc -c < /tmp/scatter_data.bin)" -eq 8
  cat /tmp/scatter_prog.bin /tmp/scatter_data.bin > /tmp/scatter_stream.bin
  timeout 5 ./space -s 8192 -p 136 -t 4 < /tmp/scatter_stream.bin > /tmp/scatter_got.bin
  cmp /tmp/scatter_data.bin /tmp/scatter_got.bin
  echo "ok: bit roundtrip X=64 seed=$TRIAL_SEED"
  t=$((t+1))
done
echo "ok: scatter/gather 1 bit/position, X=(136-6)/2=64"
