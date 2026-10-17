# Positional address space emulator

## 1. The space

### Address space

The address space is a set of positions. All positions are available at
the same time. Each position holds one bit. The space does one thing: it
copies.

```
copy n, dst, src   — copy n bits from src to dst
```

One copy uses two half-intervals: source and destination. A program is a
sequence of ticks. In each tick the space runs one round of instructions.

### Trust

The emulator trusts the program. The program guarantees that every copy
stays inside the space and that its two half-intervals do not overlap.
Part of the machine can live in the space itself.

### Processor and instructions

The processor holds `proc_n` instructions at positions
`[0, N·instruction)`. Each tick has two phases. First the emulator reads
all instructions. Then it runs all copies. A write to a processor
position, the load included, takes effect from the next tick. The order
inside a phase does not change the result.

The instruction format follows the space size. Let
`a = ceil(log2(space bits + 1))`, the bit length of the space size. Field
`n` takes `a-1` bits. Fields `dst` and `src` take `a` bits each. One
instruction takes `3a-1` bits. Bit `k` of a value sits at bit `k` of its
field. With the default `-s 65520`, one instruction is `15+16+16 = 47`
bits. A NOP is an instruction with `0` in field `n`.

Loading is a plain copy, not a special instruction. At start the host
seeds slot 0 with an ordinary copy that reads the image from the `stdin`
channel into positions `[0, N·instruction)`. Tick 1 serves that copy.
The user program runs from tick 2.

The emulator does not synthesise a loader: the run environment supplies
it (the `copy-space` project). This repository ships only a stub
(`tests/stub/`). The image must overwrite the seeded slot; a shorter
stream leaves the self-modifying seed copy running, so bound such a run
with `-t`.

The space stops when the processor holds only NOPs — all `n` are zero.
A program can stop itself: it copies zeros into the `n` fields of its
instructions.

### Channels

Channels live at positions `space_nbits` and above. They are positions
like the others, but a copy that touches a channel goes to the emulator
instead of to the space. There are two kinds of channels.

**`SYS`** is at position `space_nbits`. It is the control channel
between the program and the emulator. The program copies a request
record into `SYS`. The emulator puts an answer record into `SYS`. The
program copies the answer out. The answer holds the status. The
requests are the operations listed in `proto.def` (`CLOSE`, `SEEK`,
`FSTAT`, `IOCTL`; in the future `SPAWN`). `SYS` is a channel like the
others: it can be closed. After `SYS` is closed the program has no way
to control channels or read their status — it is isolated from the host.

**Data channels** are bound to devices **at start**, one position each:
`stdin` at `space_nbits + 1`, `stdout` at `space_nbits + 2`. The devices
are the `XDEV` table in `proto.def`. A data channel carries **data**. A
copy to a data channel sends data out; a copy from a data channel brings
data in. There is no dynamic opening.

```
position             role
space_nbits          SYS — control channel (always present)
space_nbits + 1      stdin
space_nbits + 2      stdout
...                  further devices listed in proto.def
```

**Data goes through the data channel. The status goes through `SYS`.**
The channel itself carries no status fields. A channel is named by its
**position** — there is no separate device number.

The address width is `a` bits, so the address space has `P = 2^a`
positions. Channels take the positions from `space_nbits` to `P-1`.
There is room for `P - space_nbits` channels in all, `SYS` included.
The set of devices is fixed by the launching process.

### The rest of the space

Positions outside the processor and the channels hold user data. The
program sets them with ordinary copies and through the channels.

## 2. Use for computations

1. **A function is a value table.** Every computation comes down to
   functions, and a function comes down to a table (a look-up table).
2. **A computation is addressed copies.** To compute a function means to
   copy the right strings from the table to the right positions.

## 3. Build and run

Needs a GCC/Clang-compatible compiler (`unsigned __int128` is used for
wide instruction formats).

```sh
make                # space
make check          # + tests (tests/run_tests.sh)

# run: space [-s bits] [-p num] [-t ticks] < user_space > result.
# defaults: 65520 bits, 64 instructions, no tick limit.
# limits (no artificial caps on -s/-p besides memory),
# with a = ceil(log2(space bits + 1)):
#   -p * (3a-1) <= -s                     (processor fits in the space)
#   -p * (3a-1) <= 2^(a-1) - 1            (seed count fits field n)
#   -s + 1 + NDEV <= 2^a                  (room for SYS + device channels)
# NDEV is the number of devices in proto.def (2: stdin, stdout).
# a wrong flag or a limit violation gives exit code 2.
# smallest space: -s 16 -p 1 (one 14-bit instruction, a=5).
# with defaults: instruction 47 bits, -p up to 697.

# smallest example: 14 zero program bits; seed tick, NOP stop, silence:
head -c 2 /dev/zero | ./space -s 16 -p 1 | xxd    # empty output

# stub loader (all-NOP image): the seed loads it, the space stops:
./space -s 65520 -p 64 < tests/stub/nop.bin       # silent, exits 0

# example: write "ABC" to the stdout channel (driver_gen builds the copy
# program; the run is two ticks: seed, data):
python3 tools/driver_gen.py -s 65520 -p 64 --write ABC -o /tmp/drv.bin
./space -s 65520 -p 64 -t 2 < /tmp/drv.bin         # prints ABC
```

License: MIT.
