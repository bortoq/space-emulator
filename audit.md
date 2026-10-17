# Audit of the positional address space emulator

Date: 2026-10-15. Revision under test: working tree on top of `6258659`
(channel redesign + previous audit fixes, not committed).
Role: Auditor. Method: reading the sources, static review, end-to-end
tests (`make check`), ASan+UBSan (`detect_leaks=1`), differential fuzzing
against an independent reference model, brute force over configuration
bounds, reproduction of the previous audit findings.

> **Historical record.** Sections 1-7 below describe revision
> `6258659` (and its predecessor `5771925`); that text, its
> `tests/guards.sh` reference and its line numbers are kept for the
> record only (guard branches were removed in `5771925`). The current
> state of the tree is audited in section 8.

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

## 8. Full audit (code + docs + tests)

Date: 2026-10-17. Revision under test: working tree on top of `b7e5c30`
(caps removal, tick acceleration: SWAR rev-fetch 64/128-bit, NOP early
return, bulk channel I/O, build `-O3 -flto`). Role: Auditor.

### 8.1 Result

Verdict: **[APPROVED]** (no critical or blocking defects; two non-blocking
test/documentation gaps, see 8.4). Scope: all sources (engine.c, bitcopy.c,
config.c, globals.c, space.c, space.h), README.md, readme.ru.md, audit.md,
Makefile, tests/*.sh.

### 8.2 Evidence

- `make` — no warnings; strict `-Wpedantic -Wshadow -Wcast-qual
  -Wstrict-prototypes -Wmissing-prototypes` — only the expected notes on
  `unsigned __int128` (GCC/Clang extension, see 8.3). `make check` — 31
  checks green (boot/stop/status/channels/scatter 5x random seed/cfg).
- ASan+UBSan on the whole channel/status set and 300-program differential
  fuzz — clean; exit codes on boundaries: bad flags → 2, memory → 1,
  success → 0 (`-s 2^63`, `-s/-p 0`, `-t` overflow, `-t ULLONG_MAX`).
- Wide fetch branch (`fetch_instruction_rev128`, isz 58..120): 16-bit
  roundtick at `-s 550000/600000/1000000/2000000` — data integrity OK
  (ad hoc; not in the repository suite, see 8.4).
- Flush zero-padding: manual 5-bit transfer → output `0x58` (5 data bits
  + zero padding) — OK (ad hoc; not in the repository suite, see 8.4).
- Perf (this session cumulative, medians): A(fetch,NOP) 1.59x,
  B(exec) 1.19x, C(697 instr) 1.25x, E(channels) 11.8x, F(wide) 4.26x,
  G(bulk shift) 1.07x.

### 8.3 Findings

| Code block | Type | Description | How to fix |
|------------|------|-------------|------------|
| `audit.md` §2–§7 | Warning (doc) | The audit text describes revision `6258659` and references `tests/guards.sh` — the file was removed together with its guard branches (commit `5771925`, trust refactor), and line numbers/flags are outdated (`-O2` vs `-O3 -flto`). A reader mistakes the history for the current state. | Mark §1–§7 as historical (revision hashes), or refresh the numbers; keep §8 as current. |
| `tests/` | Opt (test gaps) | 1) The wide rev128 datapath has only a boot-only case in `cfg.sh` (`-s 1000000`), no data roundtrip. 2) `channel_flush` zero padding of a partial last byte is never exercised end-to-end. Both paths were verified manually in this audit (8.2). | Add: a wide roundtrip (`-s 1000000`, in→copy→out, expect `5A 5A`) and a sub-byte flush transfer (5 bits → `58`). |
| `README.md` §"Channels" | Opt (doc) | "The program keeps `n` above `Sr`" — the precise contract is `n >= Sr` (`n == Sr` is a valid zero-data status-only transfer). | Reword to "at least". |
| `Makefile`, engine.c | Opt (doc/portability) | `CC ?= gcc` is a no-op (make predefines `CC=cc`); the build needs a GCC/Clang-compatible compiler because of `unsigned __int128`. | Note in README "requires gcc/clang"; drop `?=` or use `CC := gcc`. |
| `engine.c` per-bit path | Note | `fetch_instruction_at` (isz > 120, s >= 2^41 bits = 256 GiB) is unreachable at real memory sizes; kept as fallback. Same for `get_argument` n > 64 truncation. | Leave; optionally note in comments. |
| `bitcopy.c` aligned memcpy | Note | Overlapping half-intervals remain UB per trust boundary (documented in README/space.h since `5771925`). Ours is a deliberate, documented boundary. | Keep. |

### 8.4 Scores

| Area | Score | Notes |
|------|-------|-------|
| Memory safety | 10/10 | malloc/free balanced, no leaks (LeakSanitizer), no UAF, overflow-safe allocations; buffers exactly bounded (4096 B chunks; SWAR guards `+8/+16 <= nbytes`). |
| Security / trust | 9/10 | Trust boundary uniform and documented in space.h/engine.c/README; error paths capture errno at the failing call; exit codes 0/1/2 correct on all boundaries tested. |
| Complexity / perf | 9/10 | All ops O(n) with byte-level fast paths; no per-bit loops in the hot paths; fetch latency-optimized (SWAR), channels bulk (11.8x). Remaining reserve: bitcpy shifted-middle loop only. |
| Readability | 9/10 | Uniform naming/style, explanatory comments, single-definition header, -Wall -Wextra clean. |
| Tests | 8/10 | 31 end-to-end checks + differential fuzz (300/40/120 programs) + ASan/UBSan; gaps: wide datapath roundtrip and sub-byte flush (both verified ad hoc in 8.2). |
| Docs consistency | 8/10 | README matches behavior spot-checked on all formulas/limits (a, 3a-1, fit relations, -p 697, -s 27, status layouts, ZZ examples); stale audit.md sections 1-7 and missing doc/roadmap.md lower the score. |

### 8.5 Conclusion

The implementation matches README; formulas, limits and the trust
boundary are consistent across space.h, engine.c, bitcopy.c, README and
the test expectations. No open critical defects. The two ad-hoc checks
(wide roundtrip, flush padding) are worth moving into the suite:
they close the only two paths without an in-repo end-to-end test.

**Verdict: [APPROVED].**
