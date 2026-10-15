# Audit of the positional address space emulator

Date: 2026-10-15. Revision under test: working tree on top of `6258659`
(channel redesign + previous audit fixes, not committed).
Role: Auditor. Method: reading the sources, static review, end-to-end
tests (`make check`), ASan+UBSan (`detect_leaks=1`), differential fuzzing
against an independent reference model, brute force over configuration
bounds, reproduction of the previous audit findings.

## 1. Result

Verdict: **[APPROVED]** (no critical or blocking issues).

All 5 findings of the previous audit are closed and verified. Two
non-blocking notes were found (one outside the contract, one optimization)
and one coverage gap.

## 2. Verification of the previous audit findings

| # | Finding | Status | Evidence |
|---|---------|--------|----------|
| §1 | Critical: overflow of `proc_n * instruction_size` in `parse_config` | Fixed | Division checks. `./space -p 5887258746928580303 -s 65520` → exit 2. Brute force over 566 bounds (S=27..65536): all matched. |
| §2 | Warning: no termination on an incomplete boot stream | Fixed | `stop_processor()` + `last_input_bits`. Empty stream and 1 byte at `-s 27 -p 1` → exit 0 without `-t` (was 124). |
| §3 | Warning: no tests for the guard branches | Fixed | `tests/guards.sh` (skip `13`, skip `80`), regressions in `cfg.sh`/`boot.sh`. Before, only a helper script checked them. |
| §4 | Opt: stale `errno` | Fixed | The code is captured when the failure happens (`in_error`/`out_error`); `op_errno()` returns it. |
| §5 | Opt: silent skip of `n < status` | Fixed | README: "A transfer below that is skipped." plus a description of the incomplete load. |

## 3. New notes

| Code block | Type | Description | How to fix |
|------------|------|-------------|------------|
| `bitcopy.c:80` (`memcpy`) via `engine.c:313` | Warning (outside the contract) | A copy with overlapping half-intervals inside one call makes an overlapping `memcpy` — UB; ASan reports it (`memcpy-param-overlap`). README (§"Space device") and `space.h` explicitly leave non-overlap to the program, but a malicious or buggy program causes UB in the emulator. It is also reachable by self-modification (writes to processor positions are allowed). | By decision: keep it as a trusted boundary (documented), or add a cheap overlap check for `[src,src+n)`/`[dst,dst+n)` before `memcpy` (fall back to a per-element loop). Not a blocker. |
| `engine.c:180-188, 242-248` | Opt | `errno` is read without a preceding `errno = 0;`. If `fread`/`fwrite` return short with `ferror` set but without a fresh `errno`, a stale value can end up in the status. | Set `errno = 0;` before the I/O operation. |
| `space.h:37` | Opt (doc) | The comment on `SPACE_BITS_MAX` ("the channel address needs a bits") is wrong: 17 address bits cover position 65536; the limit is set by hand (an upper bound of memory/format). | Reword: an upper bound of the chosen range. |

## 4. Verification

- **Build:** `-std=c11 -Wall -Wextra` — no warnings.
- **`make check`:** all checks green (boot/stop/status/channels/guards/cfg).
- **ASan+UBSan** (`detect_leaks=1`) on the whole set — no errors.
- **Differential fuzzing** of C against the reference model: 2500 valid
  programs (random `-s` 80..500, `-p`, up to 7 ticks, random streams,
  channels/copies/guard branches) — 0 mismatches. The first two mismatches
  were harness artifacts (padding bits of the last byte; a missing bound
  check in the model). After fixing them the emulator matched the model.
- **Brute force over configuration bounds:** 566 checks (`-p` max valid,
  `-p` max+1 → exit 2; `-s` 8/9/26/65537/100000 → exit 2; `27..65536` →
  exit 0) — 0 errors.
- The latent `out_byte` defect (tail bits after resetting a full byte) is
  confirmed fixed: the regression "size above 255 (00 1A 40)" passes.

## 5. Remaining risk and gaps

- The `errno` value in the status is not checked by an end-to-end test:
  boot reads stdin first and there is one output channel (`stdout`), so the
  error code cannot be observed outside. The write-failure path is covered
  with `/dev/full` only for absence of a crash/hang. I consider this
  acceptable under the current contract.
- "One input transfer and one output transfer per tick" and non-overlap of
  half-intervals are the program's duties; the emulator does not check them
  (by design).
- `doc/roadmap.md` is missing. The comparison used README as the spec; no
  functional deviations were found.

## 6. Conclusion

The implementation matches README. The fixes from the previous audit are
correct and confirmed. There are no open critical defects.

**Verdict: [APPROVED].**

## 7. Appendix (Coder): attempt to replace `bitcopy.c` with an external library

Date: 2026-10-15. Author: Coder. Sections §1–§6 and the Auditor's verdict
were not changed.

Goal: replace the hand-written `bitcopy.c` with maintained external C code
(the task "find quality maintained bitcopy code in C to replace ours").

Candidates considered: `openxc/bitfield-c` (`copy_bits`, BSD-3, MSB),
`hujiyo/bitcpy` (MIT, LSB), `noporpoise/BitArray` (CC0, LSB),
`textbrowser/bitcopy` (no license), GDB `copy_bitwise` (GPLv3+),
FFmpeg `ff_copy_bits` (LGPL). Only `openxc/bitfield-c` is compatible in
both license (MIT) and bit order (MSB); GPL/LGPL are incompatible with MIT,
the rest use LSB or have no license.

`openxc/bitfield-c` (rev `e5692e7f`, BSD-3) was integrated behind an
adapter over `bitcpy`. The upstream reads one byte past the source
(`bitarray.c:73`, reproduced under ASan as a heap-buffer-overflow), so a
guard byte was added for `space` and `in_byte`. `make check` and
ASan+UBSan passed.

A direct comparison with the previous implementation showed that ours is
better:

- **Speed** (gcc -O2, ns/call; ratio new/old): aligned n=65528 — 1.05x;
  aligned n=65000 — 1.03x; shifted n=65000 — 1.22–1.26x; small n=8 —
  1.37–2.09x; medium n=32..64 — 1.13–1.52x. The new one is slower on every
  workload, worst on small copies (the I/O hot path).
- **Memory:** `.text` of `space` is 8266 B (ours) against 8730 B (new),
  plus the guard byte; 202 source lines against 230 (adapter + vendor).
- **Other:** the upstream is frozen at 2021-07; our implementation needs no
  guard byte and has no OOB read.

Conclusion: the replacement attempt failed — the hand-written `bitcopy.c`
is faster, smaller, and safer. The replacement was rolled back and the tree
is back at `5771925`. Note on §3: the overlap/`memcpy` note still applies to
both implementations (both call `memcpy` in the aligned middle); the
replacement neither removed nor worsened it.
