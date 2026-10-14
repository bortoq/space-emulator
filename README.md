# Positional address space emulator

Fixed variant of demo `demo1` from `/media/user/PROJECT/work/model/s1/demo1/`
(original: dmitri bortoq, 2022–2024, MIT). Device and idea come from the original
`demo1.readme.rus.txt` / `demo1.readme2.rus.txt` files.

## 1. Space device

Address space is a set of **simultaneously available positions**.
Each position holds a binary value (base 2; others are possible).
The single action of the space is **copying**:

```
copy n, dst, src   — copy n bits from src to dst
```

Copying bit strings from address half-intervals must be **consistent**:
one simultaneous copy involves only **non-overlapping**
half-intervals. A run of such instructions, executed by the space in one tick, is
a program. The emulator never checks half-interval non-overlap — it relies
on it; the program running in the space guarantees it.
The single outward channel is the last position of the space, `LAST`:

- first position of the source half-interval is `LAST` (`[n, dst, LAST]`: the half-interval
  `[LAST, LAST+n)` is outside the space) — input: `n` bits from `stdin` to `[dst, dst+n)`;
- first position of the destination half-interval is `LAST` (`[n, LAST, src]`: the half-interval
  `[LAST, LAST+n)` is outside the space) — output: `n` bits from `[src, src+n)` to `stdout`.

Channel copies are ordinary processor instructions and run in order,
in the same tick.
NOP (0 in field `n`) means no outward accesses. Only the half-interval inside
the space is checked; `[n, LAST, LAST]` is nop.
NOP with `LAST` is a channel state query (the single exception):
`[0, dst, LAST]` writes 8 state bits of the read side to `[dst, dst+8)`,
`[0, LAST, src]` writes 8 state bits of the write side to `[src, src+8)`.
Read-side byte: bit 0 means end of input seen, bit 1 means input failure;
write-side byte: bit 0 means output failure; other bits are zero, reserved.
The query target lies outside the processor. A query never touches the stream.
Short reads and writes (less than requested) place what is available
and never stop: a repeated access is also a transfer,
after every access the user checks the state with a channel NOP.
There are no channel stops at all: only NOP-filled processor and the flag.
One tick holds at most one input and one output with nonzero `n`
(NOPs neither read nor write): channels are sequential, and
no copy order can be set in space, so two inputs
(two outputs) would split one stream nondeterministically. The limit
is guaranteed by software; the emulator never checks it.

The space is emulated in software (hardware emulation is also possible):
the emulator lives in ticks matching host ticks. A tick has two
phases: first read all 64 instructions from the special positions, then
execute all copies (and syscalls on channel access). Therefore
writes to special positions — including the load — take effect from the next
tick; position order inside phases does not matter, the result is deterministic,
simultaneous execution by host cores is allowed.
The space stops when the processor is filled with NOPs (all `n`
are zero): such a tick is never executed. Program self-stop is
a copy zeroing the `n` fields of the instruction records (takes effect from
the next tick, like every write to special positions).

## 2. Use for computations

1. **Function = value table.** Every computation reduces to functions,
   and a function reduces to a table (look-up table).
2. **Computation = addressed copies.** To compute a function means
   to copy the needed strings from the table to the needed positions.
3. **Addressing = sorting.** A sorting mechanism performs addressed copies:
   wherever an index (a unique value) is, after sorting
   it takes its position — this gives absolute addressing.
   Before sorting, indexes are laid out so that their ordering
   brings the attached values to the required places; indexes carry
   a data bit and a copy-direction bit.
4. **Pipelined tick:** while addressing setup and index sorting run,
   computation ticks of parallel copies also run.

This demo (`space.c`) emulates the space itself directly —
executing copy instructions in sequence; the sort-and-index mechanism
lives "virtually" above it. The space is enough for computations
in the Turing thesis sense; the requirements making it a compute system
are covered in later demos (see original, "demo 5").

## 3. Repository contents

| File | Purpose |
|---|---|
| `space.h` / `globals.c` | `copy n,dst,src` format, `LAST` channel, tables; sizes come from `-s`/`-p` flags |
| `space.c` | entry point: boot instruction and ticks |
| `config.c` | parse flags `-s`/`-p`/`-t`, check limits |
| `engine.c` | engine: two-phase tick, channel copies through `LAST` |
| `bitcopy.c` | bitwise copy `bitcpy(n, src, s, dst, d)` |
| `tests/` | end-to-end checks: load (`boot.sh`), NOP stop (`stop.sh`), sizes and flags (`cfg.sh`), state queries (`state.sh`); entry point is `run_tests.sh` |

Instruction format: `n` (16 bits), `dst` (16 bits), `src` (16 bits),
48 bits total. NOP is an instruction with 0 in field `n`: it copies nothing
and accesses nothing. Field values follow host byte order
(little-endian on x86: field 8 means bytes `08 00`); `bitcpy` moves
field bytes as they are, so records are not portable across ends. The boot core is one load instruction
(`N·48, 0, LAST`): all processor instructions from `stdin` to processor
positions `[0, 48N)`. Usage: `space < user_space > result`;
the user prepares the stream (code and data); short reads never
stop (available bytes are placed, the rest is untouched).
First tick: core read and load; from the second tick the resident
program runs. User data, buffers and code live outside `[0, 48N)`.

## 4. Build and run

```sh
make                # space
make check          # + tests (tests/run_tests.sh)

# usage: space [-s bits] [-p num] [-t ticks] < user_space > result.
# defaults: 65520 bits, 64 instructions, no tick limit.
# version limits (in field widths w_n, w_a; currently 16, 16):
# bits: 1..2^w_a; instructions: at least 1;
# the instruction record plus the channel state byte outside it must fit:
# N <= (S-8)/(w_n+2w_a); violation means exit code 2.
# Minimum proof: with N=1 the record takes w_n+2w_a bits; the state
# byte (8 bits, by agreement) lives outside the processor: S >= w_n+2w_a+8.
# With current widths: 48+8 = 56, S_min = 56 (`-s 56 -p 1`, script-checked
# as buildable; everything smaller is rejected);
# with w_n=4, w_a=5: 14+8 = 22 (`-s 22 -p 1`: 4+5+5 of record plus 8 bits).
# No size multiple is required besides what the content implies: arrays
# are allocated rounding up. Hence with current widths N <= 1365.

# example: empty stdin, nothing to read (1 tick limit), no outward output.
printf '' | ./space -t 1 | xxd
# tick limit flag (no flag means no limit); bad flag means exit code 2.
{ printf '\010\000\000\040\357\377\010\000\020\040\000\040\010\000\357\377\020\040'; head -c 366 /dev/zero; printf 'Z'; } | ./space -t 2 | xxd   # outward: 5A
# example: program (input a byte to [0x2000], copy to [0x2010], output)
# plus byte 'Z'; half-intervals do not overlap.
# field values follow host byte order (field 8 means bytes 08 00).
# tick 1 executes previously read instructions (load, silence);
# tick 2: input 'Z', copy, output 'Z'; tick 3: end (3 tick limit).
# outward: 5A 5A.
{ printf '\010\000\000\040\357\377\010\000\020\040\000\040\010\000\357\377\020\040'; head -c 366 /dev/zero; printf 'Z'; } | ./space -t 3 | xxd
# example: state queries (7-triple program + 'Q', 4 tick limit):
# outward 00 00 00, 00 01 00, 01 01 00 (see tests/state.sh).
```

## 5. What was fixed relative to the original

1. `space.h`: explicit `bitcpy` prototype; tables through `extern`
   (definitions in `globals.c`), space is `unsigned char`.
2. `fetch_instruction`: loop over `N_FIELDS` instead of `sizeof s` (= 6 instead of 3)
   and write to field `[a]` instead of always `[0]`.
3. Channels reworked into the single `LAST` channel (last position):
   channel copies are ordinary instructions in order, with no
   separate phase and no descriptors (`channels()` removed); NOP means zero
   syscalls; only the half-interval inside the space is checked
   (the half-interval on the `LAST` side is outside by construction).
   Short reads and writes place what is available and never stop;
   end and failures are seen through a state query (see p. 13).
4. Half-interval non-overlap is never checked (the original gave no guarantees
   either): the program guarantees it, the emulator relies on it.
   Channel half-intervals are not excluded from the rule, but their overlaps
   are the software's concern (covered separately).
5. `init`: the core bit record is built with bitwise packing, not raw
   `memcpy` of parsed `uint16_t` (host byte order dependence);
   the core itself is one load instruction computed at startup,
   `(N·48, 0, LAST)`, instead of three triples: the engine never changes,
   the user loads code and data alone.
6. `get_argument`: never overwrites the length by copying into the same parameter.
7. `computation`/`run_instruction`: half-interval bound checks
   inside the space, skip of NOP and `[n, LAST, LAST]`.
8. `bitcopy.c`: written from scratch;
   three paths by half-interval position inside bytes — with equal positions
   (`memcpy` plus edges with masks), short shifted copies (64-bit word),
   long shifted copies (shift-and-carry loop, two bytes at a time
   with independent computations);
   half-interval overlaps are never checked — overlapping copies
   never occur by space device;
   `NULL`/`n = 0` checks.
9. User data and buffers live outside instruction records `[0, 48N)`:
   the old core held the input buffer inside instruction 0 record, and input
   wrote over its `src` field, disarming the channel from the second tick.
10. Two-phase tick (read all instructions, then execute all
    copies) instead of interleaving: writes to special positions take effect
    from the next tick; boot triple placement does not matter
    (zero position by agreement); order does not matter, simultaneous
    execution by host cores is allowed.
11. Sizes and limit come from flags (`-s` bits, `-p` instructions, `-t` ticks),
    removed from C code so the emulator needs no rebuild (`LIFE` removed,
    `processor`/`space` tables and the core triple are allocated dynamically;
    no flag means defaults (65520, 64, no limit);
    bad flag means exit code 2. The boot triple of the core is computed at startup.
    There are no channel stops: short reads never stop,
    end and failures are seen through a state query (see p. 13).
12. Stop on NOP-filled processor: `computation` returns state
    (1 if at least one non-NOP instruction was present, else 0 with no execution);
    the loop stops when all `n` are zero. Check —
    `tests/stop.sh`: the program zeroes its own instruction record and outputs
    zero — exactly one byte outward.
13. Channel NOPs: `[0, dst, LAST]` writes 8 read-side state bits
    to `[dst, dst+8)`, `[0, LAST, src]` writes 8 write-side state bits
    to `[src, src+8)` (bit 0 means end/failure, the rest is zero, reserved;
    target lies outside the processor). State is read from sticky `stdio`
    flags at query time, there is no separate storage. A query never touches the stream.
    `SIGPIPE` is ignored, so an output failure is seen in the bit instead of killing
    the process. Short reads and writes require repeated channel access;
    after every access the user checks the state with a channel NOP. Check — `tests/state.sh`.

License: MIT (as in the original).
