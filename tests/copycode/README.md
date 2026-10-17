# Copycode tests

Tests for the **in-space driver** (the code that runs inside the space)
are kept separate from the emulator tests in `tests/`.

- Emulator tests (`tests/*.sh`, `tests/test_sys.c`) exercise the host side:
  the space engine, bit copy, configuration, the `SYS` protocol and the
  channel data path.
- Copycode tests (here) exercise programs built for the space: the boot
  loader, the `SYS` request/answer builder, the IN/OUT compatibility
  layer, the tracer, and so on.

## Run

`sys_driver.sh` is run by `make check`: `tools/sys_gen.py` builds an
in-space program that issues an `FSTAT` request through `SYS`, reads the
answer back through `SYS`, and writes it to the `stdout` channel
(roadmap §11 phase 3). The other files here document the intended
copycode contracts and are not run by `make check` yet.

No copycode ships in the run environment: the loader (the `copy-space`
project) supplies it and this repository carries only a stub
(`tests/stub/`). `tools/sys_gen.py` and `tools/driver_gen.py` generate the
copy programs the tests here run.

`scatter.sh` is the bit-by-bit scatter/gather roundtrip; it targets the
old `DATA`-channel model and is kept as a specification until that
compatibility layer exists (§8.1, §11 phase 4).
