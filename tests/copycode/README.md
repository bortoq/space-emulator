# Copycode tests

Tests for the **in-space driver** (the code that runs inside the space)
are kept separate from the emulator tests in `tests/`.

- Emulator tests (`tests/*.sh`, `tests/test_sys.c`) exercise the host side:
  the space engine, bit copy, configuration, the `SYS` protocol and the
  channel data path.
- Copycode tests (here) exercise programs built for the space: the boot
  loader, the `SYS` request/answer builder, the IN/OUT compatibility
  layer, the tracer, and so on.

No copycode ships yet: the run environment supplies the loader (the
`copy-space` project) and this repository carries only a stub
(`tests/stub/`). The files here document the intended copycode contracts
and are not run by `make check`.

`scatter.sh` is the bit-by-bit scatter/gather roundtrip; it targets the
copycode layer and is kept as a specification until that layer exists.
