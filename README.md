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

### Trust

The emulator trusts the program in every case, even against common
practice. The program guarantees that each copy stays inside the space
and that its two half-intervals do not overlap. Part of the machine
mechanism can live in the space itself.

### Processor and instructions

The processor holds `proc_n` instructions at positions
`[0, N·instruction)`. Each tick has two phases. First it reads all
instructions. Then it runs all copies. A write to processor positions,
the load included, takes effect from the next tick. The order inside
a phase does not change the result.

The instruction format depends on the space size. Take
`a = ceil(log2(space bits + 1))`. Field `n` takes `a-1` bits. Fields `dst`
and `src` take `a` bits each. One instruction takes `3a-1` bits. Bit `k`
of a value sits at bit `k` of its field. With the default `-s 65520`, one
instruction is `15+16+16 = 47` bits. NOP is an instruction with 0 in
field `n`.

The boot instruction is one load: `(N·instruction, 0, DATA)`. It reads
all processor instructions from `stdin` to positions `[0, N·instruction)`.
It then writes its read status record at
`[N·instruction, N·instruction+a+8)`. The stream has program bits first
and data after them. Tick 1 reads and loads. The user program runs from
tick 2. If the stream has fewer program bits, the load leaves the
processor full of NOPs and the space stops.

The space stops when the processor holds only NOPs — all `n` are zero.
The program can stop itself: it copies zeros into the `n` fields of
its instructions.

### Channels

The single channel is `DATA`, the position just after the space
(`space_nbits`). It carries input and output, and one tick may perform
both.

- `[n, dst, DATA]` — input: data and status from `stdin` to `[dst, dst+n)`
- `[n, DATA, src]` — output: data and status from `[src, src+n)` to `stdout`

A transfer carries its own status in the tail of its half-interval.
The program reads the status right after the transfer.

- Input `[dst, dst+n)`: data in `[dst, dst+n-Sr)`. Status in
  `[dst+n-Sr, dst+n)`: end bit (1), errno (8), size (`a-1`).
- Output `[src, src+n)`: data in `[src, src+n-Sw)`. Status in
  `[src+n-Sw, src+n)`: errno (8), size (`a-1`).

`Sr` is `1+8+(a-1)` and `Sw` is `8+(a-1)`. The size is the number of
data bits the transfer moved. The end bit is one when the input has
ended. errno is zero without a failure and the operation error number
otherwise. The status number fields take their low bit first. An
incomplete transfer fills the data bits it got and leaves the rest.
The program keeps `n` at least `Sr` for input and at least `Sw` for
output.

Channel copies are normal processor instructions. They run in order
inside the tick. Stream bits go on from one access to the next. The
first bit of a stream byte is its high bit. Bits stay in the emulator
until a byte is full. At program end the last output byte is written
with zero padding.

One tick does at most one input and one output. The program must keep
to this rule.

### The rest of the space

Positions outside the processor hold user data. The program
puts data there with ordinary copies and through the channel.

## 2. Use for computations

1. **A function is a value table.** Every computation comes down to
   functions, and a function comes down to a table (look-up table).
2. **A computation is addressed copies.** To compute a function means
   to copy the right strings from the table to the right positions.

## 3. Build and run

Needs a GCC/Clang-compatible compiler (`unsigned __int128` is used for
wide instruction formats).

```sh
make                # space
make check          # + tests (tests/run_tests.sh)

# run: space [-s bits] [-p num] [-t ticks] < user_space > result.
# defaults: 65520 bits, 64 instructions, no tick limit.
# limits (no artificial caps on -s/-p besides memory),
# with a = ceil(log2 (space bits + 1)):
#   -p * (3a-1) + (1 + 8 + a - 1) <= -s   (processor plus boot status record)
#   -p * (3a-1) <= 2^(a-1) - 1            (boot count fits field n)
# a wrong flag or fit violation gives exit code 2.
# smallest space: -s 27 -p 1 (one 14-bit instruction plus the boot record).
# with defaults: instruction 47 bits, -p up to 697.

# smallest example: 14 zero program bits; load tick, NOP stop, silence:
head -c 2 /dev/zero | ./space -s 27 -p 1 | xxd    # empty output

# example with defaults (stream bytes: first printf in tests/boot.sh):
# program (32,0x2000,DATA), (8,0x2100,0x2000), (31,DATA,0x2100),
# 61 NOPs — 3008 bits = 376 bytes — then 'Z'.
# tick 1: load; tick 2: input 'Z', copy, output 'Z';
# tick 3: input ends (3-tick limit). Output: 5A 5A.

# transfer status (program in tests/status.sh), stream byte 'Q',
# 2 tick limit. Output: 00 08 00 (end 0, errno 0, size 8).
```

License: MIT.
