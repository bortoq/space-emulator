# Positional address space emulator

## 1. Space device

### Address space

The address space is a set of positions, all available at once.
Each position holds one bit. The space does one thing: it copies.

```
copy n, dst, src   — copy n bits from src to dst
```

One copy uses two half-intervals, source and destination. A program is a
sequence of ticks. The space executes one run of instructions in each tick.

### Processor and instructions

The processor holds `proc_n` instructions at positions
`[0, N·instruction)`. Each tick has two phases. First it reads all
instructions. Then it runs all copies. A write to processor positions,
the load included, takes effect from the next tick. The order inside
a phase does not change the result.

The instruction format depends on the space size. Take
`a = ceil(log2(space bits))`. Field `n` takes `a-1` bits. Fields `dst` and
`src` take `a` bits each. One instruction takes `3a-1` bits. Bit `k` of a
value sits at bit `k` of its field. With the default `-s 65520`, one
instruction is `15+16+16 = 47` bits. NOP is an instruction with 0 in
field `n`.

The boot instruction is one load: `(N·instruction, 0, LAST)`. It reads
all processor instructions from `stdin` to positions `[0, N·instruction)`.
The stream has program bits first and data after them. Tick 1 reads and
loads. The user program runs from tick 2.

The space stops when the processor holds only NOPs — all `n` are zero.
The program can stop itself: it copies zeros into the `n` fields of
its instructions.

### Channels

The channel is the last position of the space, `LAST`. It carries input and
output, and one tick may perform both.

- `[n, dst, LAST]` — input: `n` bits from `stdin` to `[dst, dst+n)`
- `[n, LAST, src]` — output: `n` bits from `[src, src+n)` to `stdout`

Channel copies are normal processor instructions. They run in order inside
the tick. Stream bits go on from one access to the next. The first bit of a
stream byte is its high bit. An incomplete transfer takes the bits it got. The
next access takes the rest.

A NOP with `LAST` (`n = 0`) is a state query: `[0, dst, LAST]` writes 8 bits
of the read state to `[dst, dst+8)`, `[0, LAST, src]` writes 8 bits of the
write state to `[src, src+8)`. Read state: bit 0 — the input ended,
bit 1 — the input failed. Write state: bit 0 — the output failed.
The other bits are zero. The query target is outside the processor.
The state shows the stream conditions at query time.

One tick does at most one input and one output with `n` not zero.
The program must keep to this rule.

### The rest of the space

Positions outside the processor hold user data. The program
puts data there with ordinary copies and through the channel.

## 2. Use for computations

1. **A function is a value table.** Every computation comes down to
   functions, and a function comes down to a table (look-up table).
2. **A computation is addressed copies.** To compute a function means
   to copy the right strings from the table to the right positions.

## 3. Build and run

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

License: MIT.
