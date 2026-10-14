# Positional address space emulator

This is a fixed version of demo `demo1` from `/media/user/PROJECT/work/model/s1/demo1/`
(original author: dmitri bortoq, 2022–2024, MIT). The idea comes from
`demo1.readme.rus.txt` and `demo1.readme2.rus.txt`.

## 1. Space device

The address space is a set of positions, all available at once.
Each position holds one bit. The space does one thing: it copies.

```
copy n, dst, src   — copy n bits from src to dst
```

One copy uses two half-intervals, source and destination. They do not
overlap; the program keeps this rule. A program is a sequence of ticks:
the space executes one run of instructions in each tick.

The channel is the last position of the space, `LAST`; it carries input and
output, and one tick may perform both:

- `[n, dst, LAST]` — input: `n` bits from `stdin` to `[dst, dst+n)`;
- `[n, LAST, src]` — output: `n` bits from `[src, src+n)` to `stdout`.

Channel copies are normal processor instructions. They run in order inside
the tick. Stream bits go on from one access to the next. The first bit of a
stream byte is its high bit. A short transfer takes the bits it got; the
next access takes the rest.

A NOP with `LAST` (`n = 0`) is a state query: `[0, dst, LAST]` writes 8 bits
of the read state to `[dst, dst+8)`, `[0, LAST, src]` writes 8 bits of the
write state to `[src, src+8)`. Read state: bit 0 — the input ended,
bit 1 — the input failed. Write state: bit 0 — the output failed.
The other bits are zero. The query target is outside the processor.

One tick does at most one input and one output with `n` not zero.
The program must keep to this rule.

Each tick has two phases. First it reads all `proc_n` instructions.
Then it runs all copies. A write to processor positions, the load
included, takes effect from the next tick. The order inside a phase
does not change the result.

The space stops when the processor holds only NOPs — all `n` are zero.
The program can stop itself: it copies zeros into the `n` fields of
its instructions.

## 2. Use for computations

1. **A function is a value table.** Every computation comes down to
   functions, and a function comes down to a table (look-up table).
2. **A computation is addressed copies.** To compute a function means
   to copy the right strings from the table to the right positions.
3. **Addressing is sorting.** A sort mechanism makes addressed copies.
   After sorting, an index (a unique value) sits at its own place —
   this gives absolute addressing. Before sorting, we lay out indexes
   so that their order moves the attached values to the right places.
   Indexes carry a data bit and a copy-direction bit.
4. **Pipelined tick:** addressing setup and index sorting run together
   with computation ticks of parallel copies.

This demo (`space.c`) emulates the space only. It runs copy instructions
one by one. The sort-and-index mechanism lives above it. The space covers
computations in the Turing thesis sense; a real compute system needs more
(see original, "demo 5").

## 3. Repository contents

| File | Purpose |
|---|---|
| `space.h` / `globals.c` | format `copy n,dst,src`, `LAST` channel, tables; sizes come from `-s`/`-p`, field widths from the space size |
| `space.c` | entry point: boot instruction, ticks |
| `config.c` | flags `-s`/`-p`/`-t`, field widths, limit checks |
| `engine.c` | two-phase tick, channel copies through `LAST` |
| `bitcopy.c` | bitwise copy `bitcpy(n, src, s, dst, d)` |
| `tests/` | `boot.sh` — load, `stop.sh` — NOP stop, `state.sh` — state queries, `channels.sh` — channel transfers, `cfg.sh` — sizes and flags; entry point `run_tests.sh` |

The instruction format depends on the space size. Take
`a = ceil(log2 space bits)`. Field `n` takes `a-1` bits. Fields `dst` and
`src` take `a` bits each. One instruction takes `3a-1` bits. Bit `k` of a
value sits at bit `k` of its field. With the default `-s 65520` one
instruction is `15+16+16 = 47` bits. NOP is an instruction with 0 in
field `n`.

The boot instruction is one load: `(N·instruction, 0, LAST)`. It reads
all processor instructions from `stdin` to positions `[0, N·instruction)`.
Run it as `space < user_space > result`. The stream has program bits first
and data after them. Tick 1 reads and loads; the program runs from tick 2.
User data and buffers live outside `[0, N·instruction)`.

## 4. Build and run

```sh
make                # space
make check          # + tests (tests/run_tests.sh)

# run: space [-s bits] [-p num] [-t ticks] < user_space > result.
# defaults: 65520 bits, 64 instructions, no tick limit.
# limits: -s >= 8; with a = ceil(log2 -s):
#   -p * (3a-1) <= -s - 8       (processor plus state byte)
#   -p * (3a-1) <= 2^(a-1) - 1  (boot count fits field n)
# a wrong flag or limit gives exit code 2.
# smallest space: -s 22 -p 1 (one instruction of 14 bits).
# with defaults: instruction 47 bits, -p up to 697.

# smallest example: 14 zero program bits; load tick, NOP stop, silence:
head -c 2 /dev/zero | ./space -s 22 -p 1 | xxd    # empty output

# example with defaults (stream bytes: first printf in tests/boot.sh):
# program (8,0x2000,LAST), (8,0x2010,0x2000), (8,LAST,0x2010),
# 61 NOPs — 3008 bits = 376 bytes — then 'Z'.
# tick 1: load; tick 2: input 'Z', copy, output 'Z';
# tick 3: input ends (3-tick limit). Output: 5A 5A.

# state queries (program in tests/state.sh), stream byte 'Q',
# 4 tick limit. Output: 00 00 00, 00 01 00, 01 01 00.
```

## 5. Changes relative to the original

1. `space.h`: clear `bitcpy` prototype; tables use `extern`
   (definitions in `globals.c`); the space is `unsigned char`.
2. `fetch_instruction`: loops over `N_FIELDS`; writes to field `[a]`.
3. Channels use one `LAST` channel: normal instructions in order,
   state queries, bit-level transfers. A short transfer keeps the bits
   it got.
4. `init`: the boot uses bit packing (bit `k` of a value sits at
   position `k`). The boot is one load `(N·instruction, 0, LAST)`,
   made at startup.
5. `get_argument`: builds field values bit by bit.
6. `computation`/`run_instruction`: check bounds inside the space;
   skip NOP and `[n, LAST, LAST]`.
7. `bitcopy.c`: written from scratch. Three paths: equal positions use
   `memcpy` with masked edges, short shifted copies use a 64-bit word,
   long shifted copies use a shift-and-carry loop. Also checks for
   `NULL` and `n = 0`.
8. Input writes only where the program's `dst` says. Data and buffers
   live outside the processor. The old `CH_IN` wrote input to position 4
   (`src` of instruction 0) and broke channel control from tick 2.
   Now channel control lives in channel instructions in any processor
   position.
9. Two-phase tick: writes take effect from the next tick. The boot
   place and the order inside a phase do not matter.
10. Sizes come from flags, field widths from `-s`, the boot is made
    at startup. Defaults: 65520, 64, no limit. A bad flag or a wrong
    limit gives exit code 2.
11. Stop on a NOP processor: `computation` returns the state; the loop
    stops when all `n` are zero (`tests/stop.sh`).
12. State queries read the `stdio` flags at query time; the flags keep
    their state. `SIGPIPE` is ignored, so an output failure shows in
    the state (`tests/state.sh`).

License: MIT (as in the original).
