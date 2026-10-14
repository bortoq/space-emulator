# Positional address space emulator


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
in the same tick. Channel transfers accept any number of bits at any
bit position: no alignment is required, and stream bits continue across
accesses (the first bit of a stream byte is its high bit).
NOP (0 in field `n`) means no outward accesses. Only the half-interval inside
the space is checked; `[n, LAST, LAST]` does nothing.
NOP with `LAST` is a channel state query (the single exception):
`[0, dst, LAST]` writes 8 bits of the read state to `[dst, dst+8)`,
`[0, LAST, src]` writes 8 bits of the write state to `[src, src+8)`.
Read state byte: bit 0 means end of input seen, bit 1 means input failure;
write state byte: bit 0 means output failure; other bits are zero, reserved.
The query target lies outside the processor. A query never touches the stream.
Short reads and writes (less than requested) place what is available
and never stop: a repeated access is also a transfer,
after every access the user checks the state with a state query.
There are no channel stops at all: only NOP-filled processor and the flag.
One tick holds at most one input and one output with nonzero `n`
(NOPs neither read nor write): channels are sequential, and
no copy order can be set in space, so two inputs
(two outputs) would split one stream nondeterministically. The limit
is guaranteed by software; the emulator never checks it.

The space is emulated in software (hardware emulation is also possible):
the emulator lives in ticks matching host ticks. A tick has two
phases: first read all `proc_n` processor instructions from the special
positions, then execute all copies (and syscalls on channel access). Therefore
writes to special positions — including the load — take effect from the next
tick; position order inside phases does not matter, the result is deterministic,
simultaneous execution by host cores is allowed.
The space stops when the processor is filled with NOPs (all `n`
are zero): such a tick is never executed. Program self-stop is
a copy zeroing the `n` fields of the instructions (takes effect from
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
| `space.h` / `globals.c` | `copy n,dst,src` format, `LAST` channel, tables; sizes come from `-s`/`-p` flags, field widths are derived from the space size |
| `space.c` | entry point: boot instruction and ticks |
| `config.c` | parse flags `-s`/`-p`/`-t`, derive field widths, check limits |
| `engine.c` | engine: two-phase tick, bit-precise channel copies through `LAST` |
| `bitcopy.c` | bitwise copy `bitcpy(n, src, s, dst, d)` |
| `tests/` | end-to-end checks: load (`boot.sh`), NOP stop (`stop.sh`), state queries (`state.sh`), channel bit transfers (`channels.sh`), sizes and flags (`cfg.sh`); entry point is `run_tests.sh` |

Instruction format follows the space size: `a = ceil(log2 space bits)`;
field `n` is `a-1` bits, `dst` and `src` are `a` bits each, the instruction
is `3a-1` bits; value bit `k` of a field sits at the `k`-th bit of its field
(portable, no host byte order). At the default `-s 65520` the format is
`15+16+16 = 47` bits. NOP is an instruction with 0 in field `n`: it copies
nothing and accesses nothing. The boot instruction is one load
(`N·instruction, 0, LAST`): all processor instructions from `stdin` to
processor positions `[0, N·instruction)`. Usage: `space < user_space > result`;
the user prepares the stream (program bits, then data — data for input
must follow the program bits); short reads never stop (available bits are
placed, the rest is untouched). First tick: read and load; from the second
tick the loaded program runs. User data, buffers and code live outside
`[0, N·instruction)`.

## 4. Build and run

```sh
make                # space
make check          # + tests (tests/run_tests.sh)

# usage: space [-s bits] [-p num] [-t ticks] < user_space > result.
# defaults: 65520 bits, 64 instructions, no tick limit.
# the instruction format follows the space size: a = ceil(log2 -s),
# fields n: a-1 bits, dst: a bits, src: a bits, instruction 3a-1 bits.
# limits: -s >= 8; the processor plus the channel state byte outside it
# must fit: -p * (3a-1) <= -s - 8; the boot count must fit the n field:
# -p * (3a-1) <= 2^(a-1) - 1; violation means exit code 2.
# smallest space: a = 5, a 14-bit instruction (5+5+4) plus the 8-bit
# state byte outside the processor: -s 22 -p 1 (everything smaller is
# rejected). At defaults: instruction 47 bits, -p <= 697.
# No size multiple is required besides what the content implies: arrays
# are allocated rounding up.

# minimal example: one instruction of 14 bits; program = 14 zero bits
# (2 stream bytes); load tick, NOP stop, silence, exit 0:
head -c 2 /dev/zero | ./space -s 22 -p 1 | xxd    # empty output

# example at defaults (stream bytes: first printf in tests/boot.sh):
# program (instruction tuples n/dst/src): (8,0x2000,LAST),
# (8,0x2010,0x2000), (8,LAST,0x2010), 61 NOPs = 3008 bits = 376 bytes,
# then stream byte 'Z'; half-intervals do not overlap.
# tick 1: read and load; tick 2: input 'Z' to [0x2000,0x2008), copy to
# [0x2010,0x2018), output 'Z'; tick 3: end of input (3 tick limit).
# outward: 5A 5A.

# state queries (program: tests/state.sh): query, input, query,
# output, output, query, output + 'Q', 4 tick limit:
# outward 00 00 00, 00 01 00, 01 01 00.
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
   (the half-interval that starts at `LAST` is outside by construction).
   Channel transfers are bit-precise: any bit count at any bit position,
   no alignment requirement, stream bits continue across accesses.
   Short reads and writes place what is available and never stop;
   end and failures are seen through a state query (see p. 13).
4. Half-interval non-overlap is never checked (the original gave no guarantees
   either): the program guarantees it, the emulator relies on it.
   Channel half-intervals are not excluded from the rule, but their overlaps
   are the software's concern (covered separately).
5. `init`: the boot instruction is built with bitwise field packing
   (value bit `k` at position `k`, portable across hosts), not raw
   `memcpy` of parsed `uint16_t` in host byte order;
   the boot is one load instruction computed at startup,
   `(N·instruction, 0, LAST)`, instead of three instructions: the engine never
   changes, the user loads code and data alone.
6. `get_argument`: never overwrites the length by copying into the same parameter;
   field values are assembled bit by bit (value bit `k` at position `k`).
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
9. User data and buffers live outside the processor positions
   `[0, N·instruction)`: input writes only where the program's `dst`
   field points. The old `CH_IN` design wrote input into position 4
   (the `src` field of instruction 0), which could disarm the channel
   control from the second tick. `CH_IN` does not exist anymore: channel
   control is channel instructions in any processor position.
10. Two-phase tick (read all instructions, then execute all
    copies) instead of interleaving: writes to special positions take effect
    from the next tick; boot instruction placement does not matter
    (zero position by agreement); order does not matter, simultaneous
    execution by host cores is allowed.
11. Sizes and limits come from flags (`-s` bits, `-p` instructions, `-t` ticks)
    and field widths are derived from the space size, so the emulator needs
    no rebuild for other formats; no flag means defaults (65520, 64, no
    limit); bad flag or limit violation means exit code 2. The boot
    instruction is computed at startup. There are no channel stops:
    short reads never stop, end and failures are seen through a state
    query (see p. 13).
12. Stop on NOP-filled processor: `computation` returns state
    (1 if at least one non-NOP instruction was present, else 0 with no execution);
    the loop stops when all `n` are zero. Check —
    `tests/stop.sh`: the program zeroes its own instructions and outputs
    zero — exactly one byte outward.
13. Channel state queries: `[0, dst, LAST]` writes 8 bits of the read
    state to `[dst, dst+8)`, `[0, LAST, src]` writes 8 bits of the write
    state to `[src, src+8)` (bit 0 means end/failure, the rest is zero, reserved;
    target lies outside the processor). State is read from sticky `stdio`
    flags at query time, there is no separate storage. A query never touches
    the stream. `SIGPIPE` is ignored, so an output failure is seen in the
    bit instead of killing the process. Short reads and writes require
    repeated channel access; after every access the user checks the state
    with a state query. Check — `tests/state.sh`.

License: MIT (as in the original).
