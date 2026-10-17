# Channel design: from `DATA` to `SYS` plus an in-space driver

Status: **design specification** (source of truth for the channel
architecture). Scope: channels of the address space and the outer
boundary of the emulator. Invariant: **the address space definition and
the instruction format do not change** (`a = ceil(log2(space_nbits + 1))`,
`nf = a - 1`, `isz = 3a - 1`; stream bits MSB-first, numeric fields
LSB-first). Format compatibility with `copy-space` stays byte-for-byte.

---

## 1. Purpose and division

The task is to move decisions about the outside world **into the space
itself**. The emulator stays a **minimal universal substrate**, and all
application logic is code in the space. Hence a clear split.

**Our emulator (core).** A universal mechanism:

- positional address space and execution of copies (definition
  unchanged);
- **one** control channel `SYS` (position `space_nbits`): it takes a
  request record and returns an answer record;
- boot: the first tick reads the program from stdin;
- access to host devices through descriptors;
- stop when all instructions are NOP.

The emulator **knows nothing** about `copy-space`, data channels,
`--trace`, `--core`, `--dump-space`, `--restore`, `-t`: these are not its
subject.

**Application (in the space).** Everything else is ordinary code in the
space:

- **emulation of the old `copy-space` space** (its IN/OUT channels, slots,
  tick order) so that old application code works;
- drivers/protocols for devices on top of `SYS` (including GPU and the
  like);
- the log (trace), dump/restore, a tick counter instead of `-t`;
- anything at all that copycode can implement.

So there is no "opacity": any image can be traced/dumped if you run next
to it code that emulates the needed space and feeds it debug data on the
expected protocol. That is work of implementing code and the cost of
loading it — an **application**, not the core.

The universality of the core is enough for this: it runs arbitrary copies
(addresses are computed by the program, as in a Turing machine) and gives
access to the host through `SYS` (input/output/control).

### 1.1 What is settled about the emulator

- the space definition and the instruction format are unchanged (header);
- `SYS` is the service channel, initialised at once; the device channels
  of the `XDEV` table (`stdin`, `stdout`) are pre-bound at start **after**
  `SYS`, one position each; a further inherited descriptor becomes a channel
  when it is added to that table; there is no dynamic opening
  (§3, §9.1); `SYS` is an ordinary channel entry and may be closed like any
  other — after that the space is isolated from host control and state;
- exchange with files/devices goes through data channels, not through
  `SYS`; data goes directly between hostfd and a buffer in the space (§7);
- seed: the first instruction is an ordinary copy of the program image
  from the `stdin` channel into the processor — it has no special boot
  meaning (§5.3); stop by "all NOP"; the host tick limit `-t` **stays**;
- lazy instruction loading is allowed if it does not hinder parallel
  loading and parallel copies; the instruction order is not defined (§2);
- trust: range correctness is the program's guarantee (§9).

### 1.2 Settled about the channel model

Settled (2026-10-17):

- `dev` is removed; channels are bound to devices **by position** (§3.2);
- `READ`/`WRITE` are removed from `SYS`: reading/writing data **is** a copy
  through a data channel (§7); `CLOSE` stays (and also closes `SYS`);
- two pre-bound channels at start: `stdin` and `stdout` (one each) (§3);
- the seed is an ordinary copy (no separate boot meaning) (§5.3);
- `SYS` is closeable; closing it isolates the space from host control and
  state (§3.2, §9.1);
- the `SYS` answer carries the channel's last-operation state (bytes moved,
  `eof`, error) and size (§4);
- data copies move whole bytes; the copycode keeps them aligned (§7).

Still open: the exact widths of the control record fields (§4, §6) — to be
finalised in phase 2.

---

## 2. Terms and general invariants

- **`SYS`** — the service channel at position `space_nbits`, initialised
  at start. Through it the space issues control/state requests; `SYS` is
  itself an ordinary channel entry and may be closed (§3.2).
- **Data channels** — pre-bound at start **after** `SYS` (positions
  `space_nbits+1`, `space_nbits+2`, ...), one per device. Through them the
  exchange with devices (files and the like) goes; `SYS` does not take part
  in the data exchange. There is no dynamic opening.
- **Tick** — the loading of instructions from `PROCESSOR` is **not
  ordered** and may be parallel; copycode must not rely on the order of
  loading and execution. Channel service is a separate phase of the tick
  (§5).
- **Trust.** Range and address correctness is the program's guarantee; the
  emulator does not check them. The emulator is a minimal substrate (§9).

Invariants:

1. The instruction format does not change; addressing covers the channels
   (`a = ceil(log2(space_nbits + 1))`); image compatibility is kept.
2. Numeric fields are LSB-first; addresses and lengths are in bits/bytes
   (§4).
3. The result of a channel operation is available in the same tick and
   does not depend on the instruction order (§3.1).
4. The order of loading/execution of instructions inside a tick is not
   defined and must stay such that copycode does not depend on it.

---

## 3. `SYS` and data channels

Data take positions `[0, space_nbits)`. The service position is `SYS`:

```
position           role
space_nbits        SYS — service channel (initialised at once)
space_nbits + 1    stdin  — pre-bound at start
space_nbits + 2    stdout — pre-bound at start
...                further devices listed in proto.def, one position each
```

`SYS` is the service channel: through it the space issues control/state
requests and reads their answers. The exchange with devices (files and
the like) goes through data channels, **not through `SYS`**.

Address capacity: the address width `a = ceil(log2(space_nbits + 1))`
gives `P = 2^a` addressable positions. Channels get

```
P - space_nbits
```

positions, starting at `SYS`. So besides `SYS` there are
`P - space_nbits - 1` data channels. The set of devices is fixed at start
(§9.1): there is no dynamic opening, and the number of channels is
therefore fixed by the launching process.

Access to a channel is an ordinary copy instruction: `dst = channel
position` (write data out), `src = channel position` (read data in). The
seed is the mechanism of the first tick (§5.3).

`SYS` is a channel like the others and may be closed (a `CLOSE` request
through `SYS`). Closing `SYS` removes the last path from the space to the
host: the space can no longer control channels or read any state. Data may
still move through device channels that remain open.

### 3.1 Result of an operation and its time

A data channel carries data, and only data: its area holds `n` data bits.
There is no status inline. The state of an operation (how much was moved,
end of stream, error code) is read **through `SYS`**. The result is
available **in the same tick** and **does not depend on the instruction
order**: channel service is a separate phase of the tick (§5). The
emulator keeps the state of each operation until it is requested or until
a new exchange happens on the channel.

### 3.2 `SYS`, data channels and positions

Two kinds of channel, with different roles:

- **`SYS`** (position `space_nbits`) — the channel between copycode and
  the emulator: control, return codes, state requests, and in the future
  `SPAWN` (start another space) and other service operations. Data does
  **not** go through `SYS`.
- **Data channel** (position `space_nbits + k`, `k >= 1`) — the position
  of a device pre-bound at start (`stdin`, `stdout`, and further devices in
  `proto.def`). It carries **data** only.

Fields and terms:

- **position** — a channel is named by its **position** (`space_nbits + k`,
  `ADDR` bits), exactly like any other place in the space. There is no
  separate device number: the binding "position -> device" is fixed at
  start (§9.1).
- **Status** (bytes moved, `eof`, error) is read **through `SYS`**. The
  data channel carries no status fields.

`SYS` is an ordinary channel entry: a `CLOSE` naming its position closes
it. Closing `SYS` is the isolation switch — after it the space has no
control over the host and cannot read any state (§9.1).

The rule in one line: **data goes through the data channel, status goes
through `SYS`.**

---

## 4. Request and answer record format (width draft)

Widths are set in `proto.def` (§6) and may be refined without a change of
model. `ADDR = address_bits`, `NF = nfield_bits`.

The records below are the `SYS` protocol. The program builds a request
record in the space and copies it into `SYS`; the emulator puts the answer
record into `SYS` and the program reads it out. A data channel carries
data; it uses neither records nor status fields.

Request record:

| field | bits | meaning |
|---|---|---|
| `op`    | 8  | operation (0 = empty) |
| `chan`  | ADDR | position of the channel the request is about |
| `flags` | 16 | flags (reserved) |
| `arg`   | 32 | scalar argument (ioctl cmd, whence, ...) |
| `off`   | 64 | offset / second argument |

`REQ_BITS = 8 + ADDR + 16 + 32 + 64`.

Answer record:

| field | bits | meaning |
|---|---|---|
| `op`     | 8  | echo of the request |
| `status` | 8  | error code (0 = success), in the spirit of errno |
| `flags`  | 16 | reserved |
| `size`   | 64 | size (for FSTAT) |
| `moved`  | NF | bytes moved by the channel's last data operation |
| `eof`    | 1  | end of stream after that operation |

`RSP_BITS = 8 + 8 + 16 + 64 + NF + 1`.

`FSTAT` returns the channel's state: `size`, and the `moved`/`eof`/error of
its last data operation. So the program reads a data operation's result
through `SYS`, in the same tick (§3.1).

All fields are LSB-first (the numeric convention of the space). Alignment
and range correctness are the program's guarantee (§9).

---

## 5. Working with `SYS` in a tick

### 5.1 Order

```
tick():
    channel service (handling the requests recorded in PROCESSOR)
    loading and execution of instructions (order not defined; may be
    parallel)
```

Access to a channel is an ordinary copy instruction. The result of the
operations is available in the same tick through `SYS` and **does not
depend on the instruction order** (§3.1). The loading/execution of the
instructions themselves is not ordered and must stay so: copycode may not
rely on their order.

### 5.2 Request and answer

An instruction carries only `(n, dst, src)`. A control or state request
(`CLOSE`/`SEEK`/`FSTAT`/`IOCTL`, in the future `SPAWN`) is copied into
`SYS`, and the answer is read **through `SYS`**. Data goes through the data
channel as an ordinary copy (§7). A separate "block" with fixed positions
is not needed.

### 5.3 Seed (loading)

Loading is not a special meaning of the copy instruction. At start the
host seeds processor slot 0 with an **ordinary copy**: it reads the image
from the `stdin` channel (`space_nbits+1`) into the processor:

```
slot 0:  copy  (proc_n*isz bits)  ->  PROCESSOR_START,  from stdin-channel
```

In the first tick `sys_tick` serves that copy like any other data read,
and `run_instruction` skips it (it touches a channel). Tick 2 fetches and
runs the loaded program. The image is supplied by the run environment
(the `copy-space` project); the emulator ships only a stub
(`tests/stub/`). The image must overwrite the seeded slot: the seed is an
ordinary self-modifying copy, so a shorter stream leaves it running and
the run must be bounded with `-t`. There is no inline status record: the
loader reads stream state through `SYS` if it needs it.

The host seeds the slot (a chicken-and-egg necessity: before the first
tick there is no program), but the instruction itself is a plain copy and
carries no special meaning.

---

## 6. Source of truth: `proto.def` (X-macro)

A single specification file, included in the C emulator and read by the
copycode generator. No parsing at run time (except the dev mode
`--proto FILE`). The version/hash are compiled in and checked.

Sketch:

```c
/* proto.def — included with the macros XPROTO/XOP/XFIELD/XDEV */
XPROTO(version, 1)

XOP(NONE,  0)
XOP(CLOSE, 1)
XOP(SEEK,  2)
XOP(FSTAT, 3)
XOP(IOCTL, 4)
/* reserve: SPAWN, POLL, MMAP, ... */

XFIELD(req, op,    8)
XFIELD(req, chan,  ADDR)
XFIELD(req, flags, 16)
XFIELD(req, arg,   32)
XFIELD(req, off,   64)

XFIELD(rsp, op,     8)
XFIELD(rsp, status, 8)
XFIELD(rsp, flags,  16)
XFIELD(rsp, size,   64)
XFIELD(rsp, moved,  NF)
XFIELD(rsp, eof,    1)

XDEV(0, stdin,  "r")
XDEV(1, stdout, "w")
/* further devices (inherited descriptors) are listed here (§9.1) */
```

Three artefacts are generated from `proto.def` **at build time**:

1. **Emulator** (`sys.c`): the `op -> handler` table, field
   encoding/decoding, `REQ_BITS`/`RSP_BITS`, the `XDEV` table.
2. **In-space driver**: copycode layers that build the request record and
   parse the answer record (generated by the Python image builder), plus
   the compatibility layer for the old IN/OUT.
3. **Translator** (host, for debugging): a readable decode of the `SYS`
   exchange.

Version check: the generated driver writes `version` into
`flags`/`arg` or runs `PING`; the emulator checks it and rejects a
mismatch (`status = EPROTO`).

---

## 7. Data flow

Reading through a data channel `ch` (a pre-bound position):

```
driver: copies n bits from the channel ch into space[buf .. buf+n)  (read)
tick T:  emulator: reads from hostfd into the channel ch
         the result is available in the same tick (§3.1)
driver:  reads the status through SYS
```

Writing is symmetric: the driver copies n bits from `space[buf .. buf+n)`
into the channel ch, and the emulator writes them to hostfd. There is no
`OPEN`/`READ`/`WRITE`: reading and writing data **are** the copies. A data
copy moves whole bytes; the copycode keeps the byte alignment.

Each channel is bound to its own device at start; there is no device
selection per operation. **Data goes through the channel, the status goes
through `SYS`.** `SYS` does not take part in the data exchange.

---

## 8. Application: emulation of the old `copy-space` space

The emulator (core) knows nothing about `copy-space`. All compatibility
is an **application in the space**: code that emulates the old space and
feeds it the data it expects. Below is what exactly is reproduced and how
it is arranged.

### 8.1 Compatibility layer for the old IN/OUT in the space

The old contract: a channel is the number of an instruction in
`PROCESSOR` (1=IN, 2=OUT); `n` bits; one index is the buffer, the other is
the result `2*copied + reason` (`0=EOF`, `1=error`); byte alignment is
required; stream bits continue between operations; channel instructions do
no internal copying; the status is available to instructions of the same
tick.

The compatibility layer as a driver in the space:

- a service layer called every tick (the emulator gives an entry point — a
  fixed address of a "service" instruction, or `op=SERVICE` in SYS);
- it reads the fields of the channel instruction from `PROCESSOR` (slots
  1/2);
- IN -> the pre-bound `stdin` channel (`space_nbits+1`), OUT -> the
  pre-bound `stdout` channel (`space_nbits+2`); the exchange goes by
  copying through these channels;
- it keeps a bit cursor of the stream and byte packing;
- it forms `2*moved + reason`, checks alignment/range;
- service order: OUT, then IN.

Complexity estimate: **medium**, comparable to the existing in-space
loader of `copy-space`. Limits and risks — §8.3.

### 8.2 Outer boundary: `--core`, `--dump-space`, `--trace`, `--restore`

These are **not channels** but the outer boundary/observability. The
decision is to keep everything in the space (bootstrap/report/loader/
tracer layers); the host keeps only a minimal launch primitive:

| flag | host side | in the space |
|---|---|---|
| `--core FILE` | insert an image instead of boot (primitive) | bootstrap layer: reads a self-describing image through SYS and lays it out |
| `--dump-space FILE` | — | terminal layer: streams the whole space out |
| `--trace FILE` | — | tracer layer (§8.8); slow, for debugging |
| `--restore FILE` | insert an image + tick boundary (primitive) | loader layer + self-overwrite (§8.5) |

Conclusion: `--core`, `--dump-space`, `--trace` and `--restore` are
reachable in the space; on the host — only the minimal primitive (insert
an image, boot, tick boundary). The `copy-space` test infrastructure
(`tests/space_segments.py` — dumps, cursors, counting channel/copy events)
moves together with the tracer (§8.8).

### 8.3 Semantic risks of the compatibility layer

1. **Instruction order is not defined.** Loading and execution of
   instructions inside a tick are not ordered and may be parallel — and
   must stay so, so that copycode does not depend on the order. So
   "self-modification is visible in the same tick" from the old model
   **is not a contract**; the compatibility layer must not rely on it.
2. **Two channels against one `SYS`.** The old IN/OUT are triggered by an
   instruction number; the compatibility layer must remap them to plain
   copies through the data channels, reproducing the IN/OUT order and
   alignment.
3. **Stream bits and alignment** must be reproduced exactly.
4. **Performance.** Access to a channel is an ordinary instruction, the
   result in the same tick (§3.1); large operations move many bits at
   once.

---

### 8.4 Builds: core vs diagnostics

The emulator core is the channels and the execution of copies plus boot.
The log, `--core`/`--dump-space`, `--restore` and observability are not
part of the space definition and move into the space (§8.5, §8.8).

- **Release core:** `-s/-p/-t`, boot-from-stdin, channels. There are no
  diagnostic flags at all.
- **A separate diagnostic build is not required by the architecture:** the
  log is kept by the tracer in the space (§8.8), not by the emulator.

If internal debug points are needed for developing the emulator itself,
they are allowed only in a separate test build; the architecture does not
rely on them and they do not reach the release.

### 8.5 Dump/Restore in the space

There is no fundamental ban on restore in the space: the old `core` in
`copy-space` is already an in-space loader (4 instructions + self-copy for
re-loading + installation records through IN).

- **dump of the whole space from the space:** a terminal report layer
  streams the bits of the space out; deterministic; includes itself.
- **restore (install) in the space:** a carrier/loader layer reads an
  image and lays it out by positions, including its own area
  (self-overwrite), then passes control to the restored entry point. The
  design requirement is a trampoline/shadow, so as not to overwrite the
  code that performs the write.
- **file positions:** SEEK through SYS; preferably **position-explicit I/O
  (pread/pwrite)**, then offsets live in the space and SEEK is not needed.

An irreducible host boundary: the image bytes enter **from outside**, and
the first tick launches the emulator (this is the boot primitive).
Everything else — dump, install, cursors, reopen — can live in the space.

Real limits (not about in-space vs host):

1. **External state outside the space:** non-seekable devices
   (pipes/tty/socket), buffers/connections, the set of open fd. Solved by
   position-explicit + deterministic handles + reopen; for non-seekable —
   not solved on the host or in the space.
2. **Restore of a "dead" space:** if the code is destroyed, it cannot be
   restored in-space. A minimal host path is needed (TCB/last resort).
3. **Self-reference at mid-flight restore:** rewriting one's own code (see
   above) is a design requirement, not a ban.

What an artefact in the space costs: it reserves room/processor in
**every** run (important with a tight `-s`); a version link
snapshot <-> loader; the program may overwrite the loader; the trusted
base in the space grows.

Consequence: the host keeps a **minimal primitive** (insert an image +
launch + tick boundary + trace), the rest is in the space. Then
`--core`/`--dump-space`/`--restore` are mechanisms of programs, not flags
of the emulator; the release core may not contain them.

### 8.6 Save format and ease of installation

Check of the current `copy-space` images:

- initial loading: either a raw bit image (`--core` key), or a built-in
  loader that reads a stream through a channel and lays the image out in
  parts;
- checkpoint: raw space (`--dump-space`), supplied as `--core` on resume;
- GCC/RV32I image — a stream: installation records (a sequence of
  instructions that place the image) + processor + `RVS1` header +
  program;
- default setting: 16 Mibit (2 MiB), address 24 bits, 256 instructions of
  71 bits.

Conclusion: no separate complex copycode for save and restore is needed.
Installation already goes in parts, without a requirement of a single
pass. Enough to:

1. keep **all the state of channels and devices in the space** (explicit
   positions of operations, deterministic descriptors) — then the raw
   space already contains all the state;
2. if part of the state must be outside — a small block description (which
   channels are open, their positions) next to the image, which the
   launching process applies at start;
3. deterministically reopen descriptors at start;
4. an image header with a version and a size (like `RVS1`).

Such a format is transparent for already ported software and for host
scenarios: save is the raw space plus (if needed) a channel description;
restore is supplying the image and reopening descriptors.

### 8.7 Observability (log)

`copy-space` (`src/space_trace.c`) can do the **maximal** level:

- on every tick — **all executed copies**: `copy tick slot n dst src` (for
  every non-zero instruction, except the channel slots);
- **all channel operations**: `channel tick slot n dst copied error`;
- the terminal line `stop <reason>`.

But in practice the levels differ (`space_main.c`: `--trace` against
`--trace-channels`):

- **full** (`--trace`) — tests and limited runs (`--ticks` usually
  10000..100000);
- **channels only** (`--trace-channels`) — heavy production runs: GCC jobs
  (`run_job.py`: `channels_only=True`) and checkpoint/resume
  (`space_segments.py`, `segment_ticks=1e7`). Copies are counted, but
  `copy` events are not written; at the end — `counts <copies>
  <channels>`.

The observation is arranged as a callback (observer) that knows nothing
about the format: the core only calls it on every event, formatting is
outside the core. The observation point itself weighs almost nothing.

On "expensive" and moving into the space:

- the **channel level** is cheap: the exchange state already comes through
  `SYS` (§3.1), there are few events per tick; the program can write them;
- the **full level** is not "twice the time": the core does the same work
  (`bitcpy`), one formatting per every **non-zero** copy is added (NOPs are
  skipped, `space.c`) plus the output volume. For short copies the
  formatting per event may dominate (growth by factors), and the main
  weight is the log size (as many events as non-zero instructions; hence
  the level-1 compression and the "channels only" mode). That is why
  production chose the channel mode.

Consequence: the full log is more practical to keep on the emulator side
(the observation point is cheap, writing is outside the core); it is
reasonable to move the channel level into the space.

### 8.8 Tracing in the space (decision)

Decision: the program in the space keeps the log; there is no log code in
the emulator (no observation-point callback is added). Slow — acceptable;
use the mode carefully, on short sections.

Mechanism (sketch):

- the **tracer is a resident program or library** in the space. It reads
  `PROCESSOR` (visible in the space) and on every tick writes out
  non-zero instructions as events `copy tick slot n dst src`, channel
  events — from the `SYS` state (§3.1), plus `stop`.
- the **tick number** the tracer keeps itself: a resident instruction runs
  exactly once per tick.
- **its own events are excluded:** the tracer skips a fixed range of its
  own slots.
- **output is a separate debug channel** (a position past the space),
  bound to a host file at start; the tracer does not take `SYS`, so as not
  to compete with the program.
- **stop reason:** the tracer ends the section itself; the emulator, as
  before, stops by `--ticks` or when all instructions are NOP.

Consequences:

- the log becomes **opaque**: it exists only for images with the tracer
  built in. Ported images are traced only if built or wrapped with the
  tracer; the old emulator traced any image automatically.
- the resident tracer takes `PROCESSOR` slots and ticks; for debugging
  this is acceptable (the instruction count can be reduced for the
  section).
- bit precision under self-modification depends on the fetch model
  (§12.1).

### 8.9 Copycode modules compensating the difference between the old and the new emulator

The new core is bulk-synchronous: all instructions of a tick are read at
the start of the tick, all values are updated only after all copies of the
tick complete, and an instruction field patched in a tick is seen by the
next tick at the earliest. The old core executed the 256 slots of a tick in
order with immediate visibility and same-tick self-modification. Everything
that follows from this difference is compensated by modules in the space,
not in the core. The core keeps no compatibility code.

| difference | compensating module (copycode) | complexity |
|---|---|---|
| order and visibility of copies inside a tick, same-tick field patching | schedule serializer (§8.9.1) or generation-time schedule rewrite (§8.10) | large / large |
| IN/OUT are `PROCESSOR` slots 1/2 with `2*copied+reason` | channel adapter (§8.1) | medium |
| `--core` image insertion and `RVS1` stream loading | seed loader (§8.9.2) | medium |
| `--dump-space` | report/dump layer (§8.5, §8.9.2) | small |
| `--restore` | install/self-overwrite layer (§8.5, §8.9.2) | medium |
| `--trace`, `--trace-channels` | tracer layer (§8.8, §8.9.2) | medium |
| instruction field widths at a power-of-two size (old 24, new 25) | image regeneration, not a module | small |
| boot: the first tick is launched by the host | seed primitive (host) plus loader layer | small |

#### 8.9.1 Schedule serializer (order-of-copies compatibility)

Purpose: make an old schedule (a tick of 256 ordered copies with immediate
visibility and same-tick field patching) give the same result under the
bulk-synchronous tick.

Two routes:

1. **Generation-time rewrite (preferred for images we generate).** The copy
   schedule of a generated image is static: the guest cannot write into the
   code layers (the guest range is guarded). Rewrite the generator so that
   each dependent step is its own tick; see §8.10.
2. **In-space serializer (deferred fallback, for schedules built at run
   time).** A resident layer reads the 256 slots of `PROCESSOR`, performs the
   copy of one slot per tick, and advances the slot cursor, reproducing the
   old order. It is an interpreter of the old copy format; the slowdown is of
   the order of the number of slots per tick. It cannot be avoided in the
   core, because the core must keep parallel instruction loading and
   parallel copies.

Contract of the serializer: process slots in order; a copy's fields are
read after all earlier copies of the old tick have committed; a field
patched by slot `i` is seen by slot `j` with `j > i`; after the last slot,
publish the result of the old tick as the starting state of the next one.

Status: **deferred**. The in-space serializer itself, and the general
translator and optimizer of schedules, are not built now; they are needed
only when a run-time-built old schedule or a non-generated static image has
to be reproduced (§11, Deferred). The route to running the ported
components in the near term is the generation-time rewrite of §8.10.

#### 8.9.2 Boundary modules (loader, dump, restore, tracer)

Same contract shape as §8.1: a resident layer, driven once per tick,
exchanging with the host only through `SYS` and the pre-bound data
channels.

- **seed loader** — reads a self-describing image (`RVS1` header) from the
  `stdin` channel and lays it out by positions, including its own area
  (self-overwrite/trampoline, §8.5).
- **report/dump layer** — streams the whole space to the `stdout` channel;
  deterministic, includes itself.
- **install/restore layer** — reads an image, lays it out, passes control to
  the restored entry point; recovers cursors and reopens descriptors
  deterministically (§8.5, §8.6).
- **tracer layer** — writes `copy`/`channel`/`stop` events to a separate
  debug channel (§8.8).

Complexity: small to medium each. They replace the host flags
`--core`/`--dump-space`/`--restore`/`--trace`; the host keeps only the
minimal primitive (insert an image, launch, tick boundary) (§8.2).

### 8.10 Rewriting RV32I for the new space

Why: `modules/rv32i.py` builds a static schedule that relies on same-tick
visibility, which the new tick forbids (and which §8.3.1 rules out as a
contract). Concretely:

- the adder (`add`) writes the carry of bit `i` and reads it at bit `i+1`
  in the same tick; the whole 32-bit carry chain is one tick;
- `lookup`/`gate` patch the `src` field of a reading instruction and read
  it in the same layer;
- `dispatch`/`branch`/`jump` resolve the next layer from fields patched in
  the same layer.

What to change: make the generator primitives tick-explicit; the bodies of
the 40 guest instructions stay nearly unchanged.

- `add` — carry as its own tick per bit first, then a parallel-carry
  (carry-lookahead) scheme;
- `reduce_or`/`reduce_and` — a tree instead of a linear chain;
- `lookup`/`gate`/`dispatch` — a patch tick and a read tick;
- `guard`/`branch`/`jump` — resolve the next layer in the following tick;
- layer/link mechanism — unchanged: slot 255 copying the next layer into
  `PROCESSOR` is already next-tick in both cores.

Format and channels: regenerate the image with the new address width;
the old IN/OUT go through the channel adapter (§8.1).

Validation: differential against the old results — the `copy-space` units
(`ctype`, `sort`, `string`, `rbtree`, `atomic`, `init/main`) and the GCC
integrated runs; compare the result and, at the first step, the trace.

Complexity: **large**; this is the main cost of the migration. It is
preferred to a general translator and optimizer for the generated images
(§8.9.1), because the generator knows the semantic intent (an adder, a
disjunction) and can target the minimum dependence depth directly. The
general translator and optimizer are **deferred** until a run-time-built old
schedule or a non-generated static image must be reproduced (§11,
Deferred).

#### 8.10.1 Implementation plan (Variant A: DAG emitter + scheduler + log-depth primitives)

Status: **implemented, first milestone** (base profile). The scheduler,
the Kogge-Stone prefix adder and the reduction trees are in
`copy-space/modules/rv32i_bulk.py`; the host insert primitive `--core`
(§8.2, decision D1) is in this emulator. The base profile (no host
channels) reproduces `rv32i_reference.execute` on 94 programs on the new
emulator (`tests/test_rv32i_bulk.py`), and the schedule invariants
(disjointness, patch distance >= 2) are checked at generation. Deferred:
the io/report profiles (need the channel adapter §8.1), the state/trace
observability, and the in-space seed loader (§8.9.2).

**Decisions (binding).**
- **D1 (loader).** Implement the **host insert primitive (§8.2)** first. It
  unblocks end-to-end differential runs with the least work. The in-space seed
  loader (§8.9.2) stays deferred to phase 10 and is **not** on the critical
  path of §8.10. Primitive-level tests may use small images that fit the seed.
- **D2 (space size).** Keep the existing 2 MiB image
  (`SPACE_BYTES = 2097152`, `a = 24`, `isz = 71`) as the default, because it
  matches the old `INSN_BITS` exactly. Only if the measured `layers` exceeds
  the code-region budget do we widen the space (emulator `-s`, generator
  `SPACE_BYTES`/`ADDRESS_BITS`); this changes `a`, `isz` and `layer_bits`.
  Prefer reducing depth (primitive parallelism, then `proc_n`) before widening.

**Goal.** Regenerate the RV32I images so that every tick is a set of
pairwise-disjoint copies (the bulk-synchronous contract).

**Metrics.**
- `layers` = number of generated layers = number of ticks on a straight-line
  path;
- `image` = `layers * layer_bits`.
- Acceptance: (1) every layer's copies are pairwise disjoint (checked at
  generation); (2) `layers <= budget`; (3) the regenerated image reproduces
  the old results against `rv32i_reference.execute` (state frames and guest
  memory); (4) no reliance on same-tick visibility.

**Pipeline.** Split `modules/rv32i.py` into three parts. The primitive API
and the 40 instruction bodies stay unchanged.

1. **Frontend** (unchanged API): `emit`, `lookup`, `gate`, `add`,
   `reduce_or`, `dispatch`, `branch`, `jump`, `immediate`, `validate`, ...
   append typed **nodes** instead of raw `Copy` ops.
2. **Scheduler**: assigns every node a level and a slot; builds layers.
3. **Backend**: lays out layer homes, resolves symbolic positions, encodes
   the new format, writes the manifest.

**Node model.**
- Kinds: `ValueCopy(n, dst, src)`, `FieldPatch(instr, field, offset, width,
  src)`, `Link(next)`.
- Positions are symbolic: `Const(bit)` or `Field(instr, name, offset)`,
  resolved after packing.
- Every node keeps its read half-interval and its write descriptor.

**Level assignment (ASAP, emission order).**
- Process nodes in emission order. The old program is one
  sequentially-consistent order; preserving it preserves all read-after-write
  and write-after-write dependences (safe, not minimal).
- Maintain `avail[pos] = level` of the latest write to `pos`.
- `ValueCopy`: `level = 1 + max(avail[p] for p in reads)`, `0` when the read
  is static data; then set `avail[p] = level` for the written range.
- `FieldPatch`: same formula on its index reads; it additionally records a
  **minimum-distance-2 edge** to its target instruction.
- Instruction execution: `level = max(1 + max(avail[reads]),
  max(patch_level + 2))`.

**Why `+2` (invariant P1).** A patch at layer `L` writes the target's home
`H_M`; the link at layer `M-1` reads `H_M`; the write must precede the read,
so `L <= M-2`, i.e. `M >= L+2`. The patch then writes `H_{L+2}` while the
same layer's link reads `H_{L+1}` — disjoint (invariant **P2**). No
instruction is patched in the processor region: `resolve(Field)` always
returns the target's home, `H_target.level + target.slot*isz + offset`
(the old same-layer `base = 0` branch is removed).

**Layer packing.**
- Layer `k` is `layer_bits = proc_n * isz` bits at home
  `H_k = code_base + k * layer_bits`.
- Up to `proc_n - 1` instructions per layer; slot `proc_n - 1` holds the
  `Link`.
- Pack nodes by level. If a level exceeds `proc_n - 1`, split it into
  consecutive layers and raise the level of all dependents to a fixed point.
- Pad empty layers where `+2` headroom is missing at a block end.
- Layer `k`'s link targets layer `k+1`, except a block's last layer, whose
  link targets the successor block's entry layer (constant, or a patched
  `Field`).

**Primitives to rewrite.**

| primitive | from | to |
|---|---|---|
| `add`/`sub` | 32-stage ripple carry | parallel prefix (Kogge-Stone): `g_i=a_i&b_i`, `p_i=a_i^b_i`, prefix, `sum_i=p_i^c_i`; `ULT` = not carry-out |
| `reduce_or`/`reduce_and` | linear chain | binary tree, `ceil(log2 n)` levels |
| `validate`/`require_value`/`require_zero` | AND/OR chain | tree; replace "high bits are zero" with one magnitude check (`addr < limit` via a subtract borrow) where possible (21 sites) |
| `sll`/`srl`/`sra` | - | keep the 5-stage barrel shifter |
| `dispatch`/`branch`/`jump` | same-layer link patch | `FieldPatch` on the link source, with `+2` headroom |
| constant-index reads | patch | set `src` statically, no patch, no `+2` |

**Format.**
- Match the new engine exactly by choosing `-s` with `a = 24` (for example
  `-s 16777212`): `z = 23`, `isz = 71`, identical to the old `INSN_BITS`. Do
  **not** use exactly `2^24` (that yields `a = 25`).
- Loading: the seed loads only `proc_n * isz` bits. A full image needs the
  loader — the host insert primitive (§8.2) or the in-space seed loader
  (§8.9.2). Provide one before end-to-end runs; primitive-level tests can use
  small images that fit the seed.

**Budget and the `proc_n` lever.**
- `budget = (memory_bit - code_base) / layer_bits` = 447 at 2 MiB.
- If `layers > budget`: raise the space (up to 256 MiB is supported) or raise
  `proc_n` (wider layers, fewer links); choose by `layers * layer_bits`.

**Validation.**
1. Unit: for every generated image assert per-layer pairwise disjointness of
   all half-intervals; simulate the schedule in a Python model of the new
   tick and compare each primitive with the old result.
2. Differential: regenerate the `tests/test_rv32i.py` corpus, run on the new
   emulator (after the loader), compare state frames and guest memory with
   `rv32i_reference.execute`; keep the coverage and tick-budget checks.

**Risks.**
- `+2` headroom and level overflow force padding layers; watch `layers`.
- Preserving the emission order is safe but not minimal; a later
  list-scheduling pass can compact it.
- Self-modifying guest code and loops: patches live in static layer homes;
  verify with the `self-modifying-guest` case.

**Files.** Generator: `copy-space/modules/rv32i.py` (frontend + primitives)
plus a new scheduler/backend module; emulator: the image-insert primitive
(§8.2) for end-to-end runs.

## 9. Device access and debugging

Principle: **the emulator is a minimal substrate.** All logic that can be
moved into the space is moved there. The space is the environment on which
an architecture is launched (for example, a processor), on it a system, on
it user code. The emulator starts this chain and hands it to the
`copy-space` project; then trust, limits and protection are the concern of
the upper layers, which are already in the space.

Consequences:

- the emulator does not embed permanent fuses, registries or allow-lists:
  the device-access mechanism is minimal;
- binding a channel to a device is the simplest (in the limit — an
  inherited descriptor, §9.1); the emulator has no paths and no dynamic
  device opening;
- range and address correctness at the `SYS` boundary, as for internal
  copies, is the program's guarantee; the emulator does not check them;
- blocking synchronous exchange, `SIGPIPE` is ignored (as now);
- the protocol version and hash are checked at initialisation — a build
  correctness requirement, not a protection;
- scale: `SYS` accesses are per instruction, the result in the same tick
  (§3.1).

### 9.1 Binding channels to devices (settled)

There is no `dev` and no `--dev`: the set of devices is fixed at start and
the binding is done by the operating system. The launching process opens
the devices and passes them to the emulator as already open descriptors
(redirection). The emulator binds a channel position to a descriptor
number and does not see the paths.

The agreement is "channel `space_nbits+1+i` -> descriptor `i`" (`0`, `1`,
`2` are the standard streams; `3` and above the further inherited
descriptors, in order). At start the emulator pre-binds the devices listed
in the `XDEV` table of `proto.def`; the current table has the two standard
streams: the `stdin` channel at `space_nbits+1` and the `stdout` channel at
`space_nbits+2`. A further inherited descriptor becomes a channel only when
it is listed there.

Opening devices by name during a run is the concern of the upper layer;
the emulator does not implement it.

`SYS` is closeable like any channel. Closing `SYS` gives isolation: the
launching process can hand a space that, once it closes `SYS`, can no
longer control channels or read their state.

### 9.2 Debugging

Protecting the system during debugging is our temporary task, not a
property of the emulator. While development goes on, one may use stand-in
devices (for example, files in a temporary directory), a limited
environment and file-system snapshots. When everything is debugged, these
means are no longer needed and are removed; they do not stay in the
emulator.

## 10. Interfaces and contracts

### Emulator (`sys.c`, host)

```c
void sys_init(void);
/* pre:  parse_config passed
   post: the channel/descriptor table is ready, SYS plus the pre-bound
         device channels; on lack of room — exit(EXIT_USAGE) */

void sys_tick(void);
/* service the channel instructions of the tick.
   A copy touching a data channel moves data (dst -> device, device -> dst).
   A request record copied into SYS is executed; the answer is available
   in SYS in the same tick. CLOSE may close SYS itself (isolation). */
```

### In-space driver (copycode)

Contract: build the control record (`CLOSE`/`SEEK`/`FSTAT`/`IOCTL`) and copy it
into `SYS`; read the answer (with the status) **through `SYS`** in the
same tick. Data is moved by plain copies through the pre-bound data
channels. Invariants: the record is built entirely in the space; the data
buffer does not overlap the channel records.

### Compatibility layer IN/OUT

```
pre:  PROCESSOR holds a channel instruction (n, buf, res)
post: with valid n/buffer: [buf,buf+bytes*8) is filled (IN) or output
      (OUT); res = 2*moved_bits + reason; alignment/range violated ->
      res = 2*0 + 1, the stream state is cleared for the next operation
```

---

## 11. Implementation phases

1. **SYS frame**: `sys_ctrl.c` (instead of `DATA`), channel positions past
   the space, `proto.def` with the control ops
   (`CLOSE`/`SEEK`/`FSTAT`/`IOCTL`);
   pre-bound `stdin`/`stdout` channels; the seed as a plain copy.
   Tests: round-trip read/write through channels, state in the same tick.
2. **Generation**: extract encode/decode and the tables from `proto.def`.
3. **In-space driver**: generate the request/answer copycode layers.
4. **Compatibility layer IN/OUT**: reproduce the old contract on top of
   SYS.
5. **Dump/restore**: report layer (dump in the space), loader layer
   (install/self-overwrite), restoring cursors through SYS; host — only
   the primitive of inserting an image and the tick boundary (§8.5).
6. **Observability**: tracer in the space (§8.8), a separate debug
   channel; format `copy`/`channel`/`stop`, compatible with
   `tests/space_segments.py`.
7. **Translator** of the `SYS` exchange for debugging.
8. **(separately) device-model/agent** for the GPU class: buffer registry,
   poll/events; outside the channel core.

9. **RV32I for the new space** (§8.10): make the generator primitives
   tick-explicit, regenerate the image, validate differentially.
10. **Boundary modules** (§8.9.2): seed loader, dump, install and tracer in
    the space; the host keeps only the insert and launch primitive.

Every phase: `make check` green, the space format unchanged, bit
compatibility of `copy-space` images kept.

#### Deferred (later, only if needed)

- **Schedule serializer and general translator/optimizer** (§8.9.1): build
  only when a run-time-built old schedule or a non-generated static image
  must be reproduced. Until then the generation-time rewrite (§8.10) is the
  whole path to running the ported components, and no separate schedule
  translator or optimizer is written.

---

## 12. Open questions

1. **Fetch model.** Settled: lazy loading is allowed if it does not hinder
   parallel instruction loading and parallel copies. The order of
   loading/execution is not defined (§2, §8.3.1).
2. **Compatibility layer IN/OUT.** How to remap the old slots 1/2 to the
   data channels (two channels -> one block; keeping self-patch
   visibility).
3. **Fidelity of the test infrastructure.** The format of
   `--trace`/dump/cursors — compatible with `tests/space_segments.py` or
   new?
4. **Minimal `-s`.** With `a = ceil(log2(space_nbits + 1))` the channel
   area holds `2^a - space_nbits` positions, so `-s` must leave room for
   `SYS` plus the pre-bound devices (`space_nbits + 1 + NDEV <= 2^a`;
   `NDEV = 2` today). The minimum is `-s 16 -p 1` (`a = 5`). The practical
   minimum may grow when the in-space `SYS` driver must fit; recompute
   then.
5. **Run-time `--proto FILE`** — keep only as a dev mode.
6. **Explicit operation positions (pread/pwrite)** — make them mandatory
   for a self-contained in-space restore? Then SEEK is not needed, and
   restore = a pure image installation.
7. **Number of channels.** `SYS` is fixed (position `space_nbits`); the
   device channels are pre-bound after it by the launching process; the
   capacity is `P - space_nbits`, where `P = 2^a` (§3). The number is
   fixed at start, not grown at run time. Only the `XDEV` devices are bound
   today; further inherited descriptors (fd >= 3) are a planned extension.
8. **Tracing in the space.** Settled: the program in the space keeps the
   log, there is no log code in the emulator (§8.8). Open details: resident
   tracer or library; how its own slots are excluded; how the stop reason
   is determined; the separate debug channel and its binding; whether bit
   precision is needed (depends on §12.1).
9. **Debug output.** Through which descriptor the tracer writes the log
   (open through `SYS`) and how it is bound at start.
10. **Transparency.** Confirm that only images built with the tracer are
    traced (the old emulator traced any image automatically).

11. **Status query through `SYS`.** Settled: `FSTAT` returns the channel's
    size and the `moved`/`eof`/error of its last data operation (§4).
12. **Isolation.** `SYS` is closeable; closing it removes the last
    control/state path to the host (§3.2, §9.1). Confirm this is the
    intended meaning of "full isolation".
