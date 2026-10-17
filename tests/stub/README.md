# Space stub (placeholder loader)

The emulator does **not** synthesise a boot program: the run environment
supplies it. In the full system this is the `copy-space` project; here we
only need a stub to drive the emulator.

`nop.bin` is the simplest such stub: an all-NOP image for the default
configuration (`-s 65520 -p 64`). The seed copies it over the processor,
so on the next tick the processor holds only NOPs and the space stops.

```sh
./space -s 65520 -p 64 < tests/stub/nop.bin   # silent, exits 0
```

Regenerate it for other sizes (the image covers `proc_n` instructions):

```sh
python3 tools/driver_gen.py -s 65520 -p 64 --nop -o tests/stub/nop.bin
```

A seed stream must overwrite the whole image. If it is shorter, the
leftover bits of the self-modifying seed copy keep running; without `-t`
that does not terminate. Provide a full loader (or bound the run with
`-t`).
