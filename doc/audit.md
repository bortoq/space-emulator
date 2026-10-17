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
- `doc/roadmap.md` is present (added after this section was written). The
  earlier comparison used README as the spec; no
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
| Docs consistency | 8/10 | README matches behavior spot-checked on all formulas/limits (a, 3a-1, fit relations, -p 697, status layouts, ZZ examples); stale audit.md sections 1-7 and (at the time) a missing doc/roadmap.md lowered the score. |

### 8.5 Conclusion

The implementation matches README; formulas, limits and the trust
boundary are consistent across space.h, engine.c, bitcopy.c, README and
the test expectations. No open critical defects. The two ad-hoc checks
(wide roundtrip, flush padding) are worth moving into the suite:
they close the only two paths without an in-repo end-to-end test.

**Verdict: [APPROVED].**

---

## 9. Аудит реформы SYS и фиксации модели (код + доки + взаимосоответствие)

Дата: 2026-10-17. Ревизия: рабочее дерево поверх `b7e5c30`
(реформа SYS + in-space драйвер + фиксация модели «данные — в канале,
статус — через SYS»). Роль: Auditor. Охват: `engine.c`, `sys_ctrl.c/.h`,
`space.c`, `config.c`, `bitcopy.c`, `globals.c`, `proto.def`,
`tools/proto_gen.py`, `tools/driver_gen.py`, `README.md`, `doc/roadmap.md`,
`tests/*`.

### 9.1 Результат

Вердикт: **[REJECTED]** (один блокирующий дефект взаимосоответствия
документов и кода; несколько неблокирующих замечаний и пробелов в тестах).

### 9.2 Замечания

| Блок | Тип | Описание | Как исправить |
|---|---|---|---|
| `sys_ctrl.c:109` (`resolve_target`), `:161` (`encode_rsp`), `:295` (`service`), `:378` (`sys_tick`) vs `README.md:56-88`, `doc/roadmap.md:142-171,310-327` | **Крит (соответствие)** | Зафиксированная модель: **данные — в канале данных, статус — через `SYS`**; операции данных `dev` не используют. Код делает иначе: ответ-запись (со `status`/`moved`/`eof`) кодируется в **тот же** канал, куда пришёл запрос (`encode_rsp` по `src`-каналу), а `resolve_target` трактует `dev` и как позицию канала, и как номер устройства. README/roadmap с этой моделью расходятся с кодом. | В Coder-е: ответ/статус формировать в `SYS`; целевую структуру для данных выбирать по позиции канала, `dev` в операциях данных не читать. До этого документацию и код считать несогласованными. |
| `engine.c:385-473` (`space_output`), `:265` (`write_status_bits`), `:43-46,292-295` (`in_byte`/`out_byte`/`out_used`), `:476-483` (`channel_flush`) | Warning (мёртвый код) | `space_output` **нигде не вызывается** (проверено grep). `out_byte`/`out_used` заполняются только в нём, поэтому `channel_flush()` (`space.c:31`) фактически ничего не пишет. Остаток старого канала `DATA`. | Удалить `space_output`, `write_status_bits`, `out_byte`, `out_used`, `out_error`, `channel_flush` и вызов из `space.c`; либо, если путь нужен, вернуть вызов и тест. |
| `engine.c:1-27`, `space.h:16-19`, `space.c:2`, `config.c:54`, `sys_ctrl.h:1-13` | Warning (доки) | Комментарии описывают «single channel DATA» / «the last one is the DATA channel» / ответ-запись в канале. Расходится и с кодом (SYS-позиция), и с зафиксированной моделью. | Обновить шапки под `SYS`+каналы данных и модель «статус через `SYS`». |
| `README.md:84-88` vs `sys_ctrl.c:33-40` (`device_stream`) | Warning (соответствие) | README: «Other numbers are inherited host descriptors». Код для `dev >= 2` возвращает `ENODEV` (`device_stream` знает только 0/1). Проверено: `--open 5` даёт пустой вывод. | Либо реализовать унаследованные дескрипторы, либо убрать/смягчить фразу в README. |
| `doc/roadmap.md:645-655` (§11 фаза 1) | Warning (доки) | «`proto.def` with OPEN/CLOSE/READ/WRITE for dev 0/1» противоречит §3.2 (данные адресуются каналу, `dev` — только для `OPEN`). | Переформулировать фазу 1 под зафиксированную модель. |
| `tests/sys_e2e.sh:42-52`, `tests/test_sys.c:207-230` | Warning (тест/соответствие) | Тесты закрепляют **неканонический** путь `READ/WRITE dev=0/1 без OPEN` (ответ со статусом в канале). При переходе к модели «статус через SYS» эти тесты подтверждают неверное поведение. | Заменить на «`OPEN` → обмен через канал данных»; проверять статус через `SYS`. |
| `doc/audit.md:168` (§8.4) | Warning (доки) | Утверждает, что `doc/roadmap.md` отсутствует — файл есть. | Актуализировать §8.4. |
| `tests/run_tests.sh:4-6` | Warning (доки) | Комментарий «legacy … not run yet: phase 3» устарел (фаза 3 подключена). | Обновить комментарий. |
| `tests/test_sys.c:94` (`test_boot` → `init_space`), `:124` | Opt (утечка в тесте) | `init_space()` вызывается дважды; повторно `malloc` перезаписывает `space`/`processor`, `free` освобождает только второй. LeakSanitizer: `8382 byte(s) leaked in 2 allocation(s)`. В проде `init_space` вызывается один раз, утечки нет. | Освобождать перед повторным `init_space` или не звать его дважды в одном процессе. |
| `sys_ctrl.c:348-359` (`OP_SEEK`) | Opt (читаемость) | Ветки `if/else` присваивают один и тот же `status`. | Упростить до одного присваивания. |

### 9.3 Покрытие тестами

Есть (зелёные): round-trip `WRITE`/`READ`, ответ в том же такте, `EOF`,
`EINVAL`, `OPEN`/`CLOSE`, `dev=0/1` без `OPEN`, `handle == space_nbits+1`,
boot через `computation()`, 6 сквозных сценариев `sys_e2e.sh`, 5 boot-проверок.

Нет:
1. `SEEK`, `FSTAT`, `IOCTL` — не тестируются вовсе.
2. `ENOSPC` — исчерпание ёмкости каналов (`P - space_nbits`).
3. `ENODEV` при `dev >= 2`.
4. Модель «статус через `SYS`» — тестов нет (в коде её пока нет).
5. Широкий формат (`isz` 58..120, `fetch_instruction_rev128`) — только boot-кейс, без round-trip данных (повтор §8.4).
6. Частичный последний байт вывода (sub-byte flush) — только ad hoc (повтор §8.4).
7. Наследованные дескрипторы `dev >= 2` — не реализованы и не тестируются.

### 9.4 Проверка исполнением

- `make`, `./tests/run_tests.sh` — зелёные, без предупреждений.
- Прод-бинарь под ASan+UBSan (`detect_leaks=1`) на `driver_gen --open 1 --write ABC`
  — чисто, вывод `ABC` (утечек нет).
- `--open 5` → пустой вывод, код 0 (подтверждает `ENODEV`).
- `-s 27 -p 1`, `--open 1 --write ABC` — воспроизведены значения README.

### 9.5 Вывод

Блокирующий пункт — расхождение зафиксированной модели («статус через
`SYS`») с кодом, который по-прежнему размещает ответ в канале запроса.
Дополнительно: мёртвый код старого `DATA`-канала, устаревшие комментарии,
непокрытые операции и повторно открытые пробелы §8.4. После исправления
Coder-ом — перепроверить (модель, мёртвый код, тесты, соответствие README)
и только затем снять REJECTED.

**Вердикт: [REJECTED].**

---

## 10. Повторный аудит после реформы SYS (код + доки + тесты + жизнеспособность)

Дата: 2026-10-17. Ревизия: рабочее дерево (реформа SYS завершена: dead-code
удалён, ответ формируется только в `SYS`, `dev`/`READ`/`WRITE` убраны, доки
синхронизированы, добавлены unit-тесты `tests/test_sys.c`). Роль: Auditor.
Охват: `engine.c`, `sys_ctrl.c/.h`, `space.c/.h`, `config.c`, `globals.c`,
`bitcopy.c`, `proto.def`, `proto_gen.h`, `tools/*.py`, `Makefile`,
`README.md`, `doc/roadmap.md`, `tests/*`.

### 10.1 Результат

Вердикт: **[REJECTED]** — один блокирующий дефект **жизнеспособности**: на
коротком (в частности, пустом) seed-потоке эмулятор **зацикливается** без
`-t`. Ранее блокировавший пункт §9 (расхождение зафиксированной модели и
кода) **закрыт** и подтверждён.

### 10.2 Замечания

| Блок | Тип | Описание | Как исправить |
|---|---|---|---|
| `engine.c:362` (`store_instruction` в `init_space`), `engine.c:308-314` (`computation`), `sys_ctrl.c:200-236` (`do_read`) | **Крит (жизнеспособность)** | Seed-инструкция записывается **в саму загружаемую область** (`space_put(...)` для слота 0), поэтому `computation()` каждый такт видит `n[0]=boot_n≠0`. Если поток короче образа, `do_read` переносит только прочитанные байты и **не затирает** старшие биты поля `n` seed-инструкции → seed обслуживается каждый такт → бесконечный цикл без `-t`. Воспроизведение: `printf '' \| ./space -s 65520 -p 64` → не завершается (124); `head -c 1 /dev/zero \| ./space -s 65520 -p 64` → 124; `head -c 2 /dev/zero \| ...` → 0 (нужно ≥2 нулевых байта, т.к. `nfield_bits=15`); `./space < /dev/null` → 124. При `-t 1` → 0. Противоречит README («If the stream has fewer program bits, the processor stays full of NOPs and the space stops») и roadmap §5.3. Старые `stop_processor()`/`last_input_bits` (закрывавшие §2 истор.) были удалены в реформе, регрессия вернулась в новой форме. | В `do_read` при коротком чтении **дописывать нули** до `len` байт по `ptr + moved*CHAR_BIT` (`len-moved` байт): чтение в EOF = данные + нули. Это уже подразумевает e2e-тест «read at EOF → zero bytes», а неполный образ становится «образ + NOP», seed затирается, пространство останавливается. Альтернатива: детектировать незавершённый seed-load в `computation()` и останавливаться. Обязательно добавить регрессионный тест: `printf '' \| ./space` и `head -c 1 /dev/zero \| ./space` без `-t` → код 0. |
| `README.md:115,118`, `doc/roadmap.md` (§12 п.4) | Warning (доки) | Утверждается «smallest space: `-s 27 -p 1`». Фактически минимум `-s 16 -p 1`: `-s 16..29` → код 0, `-s 15` → «seed count does not fit the n field», `-s 30/31` → «no room for a device channel (-s)». Ограничение «место под `SYS`+2 канала» (`space_nbits <= 2^a - 3`) не задокументировано. | Указать реальную границу и добавить условие `space_nbits <= 2^a - 3` к двум fit-relations. |
| `tests/` (удалены `cfg.sh`, `scatter.sh`, `status.sh`, `stop.sh`, `wide.sh`, `channels.sh`; лежат в `tests/legacy/`, не запускаются) | Warning (тест) | Из набора ушли границы/фит-relations конфигурации, широкий формат (`isz` 58..120) и «останов» неполного потока — ровно класс дефекта 10.2-1. Регресс не пойман, т.к. `boot.sh` для пустого потока использует `-t 1`. | Вернуть в `run_tests.sh` проверки границ конфигурации, широкий round-trip и останов на коротком/пустом потоке. |
| `sys_ctrl.c:207,240` (`do_read`/`do_write`) | Opt | `errno` читается после `fread`/`fwrite` без предварительного `errno = 0`; при коротком чтении с `ferror` без свежего `errno` в статус может попасть устаревшее значение. | `errno = 0;` перед I/O. |
| `tests/test_sys.c`, `tests/sys_e2e.sh` | Opt (покрытие) | Не покрыты: путь ошибки ввода-вывода (`ERR_IO`/`EBADF`-ветки `do_read`/`do_write`), `sys_device_pos()` напрямую, `CLOSE` несуществующего канала (`EBADF`). | Добавить точечные юнит-кейсы. |
| `bitcopy.c:39`, `config.c:19,33` | Opt | `-Wshadow` (параметр `s` в `parse_ull`/`parse_size`), нет prototype у `bitcpy` (не включает `space.h`), `-Wconversion` в `bitcopy.c`; `parse_size` сравнивает `uint64_t > SIZE_MAX` (на 64-бит всегда ложно). | Мелочи прошлых ревизий; при желании — включить `space.h` в `bitcopy.c`, переименовать параметры. |

### 10.3 Проверка закрытия §9

| §9.2 | Статус | Доказательство |
|---|---|---|
| Крит (модель↔код) | **Закрыто** | `resolve_target` отсутствует (`grep` → none); ответ кодируется только в `SYS` (`sys_tick`, второй проход `src==SYS`); `dev` в операциях данных не читается. |
| Мёртвый код | **Закрыто** | `space_output`, `write_status_bits`, `in_byte`/`out_byte`/`out_used`, `channel_flush`, `stop_processor`, `op_errno` → `grep` none. |
| Шапки-доки | **Закрыто** | `engine.c`, `space.c`, `space.h`, `config.c`, `sys_ctrl.h` переписаны под `SYS`+каналы. |
| README vs `device_stream` | **Закрыто** | README/roadmap: устройства = таблица `XDEV` (`stdin`/`stdout`); унаследованные fd ≥ 3 отмечены как «planned» (§12 п.7). |
| roadmap §11 фаза 1 | **Закрыто** | Теперь `CLOSE`/`SEEK`/`FSTAT`/`IOCTL`. |
| тесты старой модели | **Закрыто** | `test_sys.c`/`sys_e2e.sh` переписаны под канон «данные — каналом, статус — через `SYS`». |
| `audit.md` §8.4 «roadmap отсутствует» | **Открыто** | Файл `doc/roadmap.md` существует — поправить §8.4 (файл аудитора). |
| `run_tests.sh` комментарий | **Закрыто** | Обновлён. |
| Утечка в `test_boot` | **Закрыто** | `free(space)/free(processor)`; LSan чист. |
| `OP_SEEK` if/else | **Закрыто** | Упрощено. |

### 10.4 Проверка исполнением (доказательства)

- Сборка `-Wall -Wextra` — без предупреждений. Строгий набор
  (`-Wpedantic -Wshadow -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes
  -Wconversion -Wsign-conversion`) — только ожидаемые `__int128` (ISO C) и
  `-Wshadow`/prototype nits из 10.2.
- `make check` → `ALL CHECKS PASSED` (unit + 4 e2e + 5 boot).
- ASan+UBSan+LSan (`detect_leaks=1`): unit, write/echo/boot, а также
  empty-stream (только зависание, без ошибок памяти) — чисто.
- Дифференциальный фуззинг `bitcpy` против поразрядной эталонной копии:
  **0 расхождений / 300000** (включая in-place без перекрытия), под
  ASan+UBSan.
- Дифференциальный фуззинг `space_put`/`space_get`: **0 / 300000** (контроль
  «чужих» бит и соответствия бит значению), под ASan+UBSan.
- Широкий формат (`isz` 58..120, ветка `fetch_instruction_rev128`):
  round-trip `-s 1048576/33554432/67108864 -p 8` → `OK`/`HI`. В
  репозиторном наборе не покрыт (см. 10.2-3).
- Границы конфигурации: `-s 16..29 -p 1` → 0; `-s 15` → 2 (seed count);
  `-s 30/31` → 2 (no room); `-s 0`/`-p 0` → 2; `-t ULLONG_MAX` → 0;
  переполнение `-t` → 2; `-p 698` (default `-s`) → 2.

### 10.5 Оценки

| Область | Оценка | Обоснование |
|---|---|---|
| Память/ресурсы | 10/10 | malloc/free сбалансированы, LSan чист; std-дескрипторы намеренно не закрываются; realloc каналов ограничен 3 записями; стек-буферы 32 КБ блочно. |
| Безопасность/trust | 6/10 | trust-граница задокументирована, но: (а) зависание на коротком/пустом потоке — DoS/ломает CI; (б) короткий **ненулевой** поток оставляет остаточные биты seed (возможна копия с перекрытием интервалов — UB-путь); (в) устаревший `errno` в статусе. |
| Сложность/перф | 9/10 | Все операции O(n); SWAR-fetch, блочный ввод-вывод; `sys_tick` — два прохода O(proc_n) на такт (приемлемо). |
| Читаемость | 9/10 | Единый стиль, содержательные комментарии; nit `-Wshadow`. |
| Тесты | 5/10 | Набор зелёный, но удалены cfg/wide/stop-тесты; регресс на коротком/пустом потоке не поймали; путь ошибок I/O не покрыт. |
| Соответствие докам | 6/10 | Модель `SYS`, поля, `CLOSE`/`SEEK`/`FSTAT`/`IOCTL`, байтовая гранулярность — совпадают; неверны «минимальная `-s`» и «неполный поток → NOP-стоп». |

### 10.6 Вывод

Модель `SYS`, код, доки и тесты согласованы; блокирующий пункт §9 закрыт.
Однако остаётся блокирующий дефект жизнеспособности: **короткий/пустой
seed-поток зацикливает эмулятор без `-t`**, что прямо противоречит README и
roadmap §5.3 и ломает неинтерактивный запуск (`./space < /dev/null`).
Дополнительно: неверная «минимальная `-s`», потерянные тесты границ/широкого
формата и мелкие opt-замечания.

После исправления 10.2-1 (рекомендация: zero-fill остатка в `do_read` +
регрессионный тест) — **перепроверить** (жизнеспособность на коротком/пустом
потоке, границы конфигурации, соответствие README/roadmap) и только затем
снять REJECTED.

**Вердикт: [REJECTED].**

---

## 11. Аудит после правок по §10 (закрытие, тесты, соответствие)

Дата: 2026-10-17. Ревизия: рабочее дерево после правок Coder по указаниям
заказчика (само-модификация seed — норма дизайна, образ грузит run-окружение;
минимум `-s`; разведение тестов эмулятора и копикода; `errno`; стаб-загрузчик).
Роль: Auditor. Охват: `config.c`, `sys_ctrl.c`, `tools/driver_gen.py`,
`README.md`, `doc/roadmap.md`, `tests/*`.

### 11.1 Результат

Вердикт: **[APPROVED].** Блокирующих дефектов нет. Крит §10.2-1 снят
**прямым решением заказчика**: запись seed-инструкции в загружаемую область —
намеренный самомодифицирующийся код, а образ обязан поставлять run-окружение
(ср. `copy-space`); ложное утверждение README/roadmap («неполный поток →
NOP-стоп») исправлено, добавлен стаб-загрузчик. Прочие пункты §10 закрыты.
Остаются неблокирующие замечания (см. 11.3).

### 11.2 Закрытие §10

| §10.2 | Статус | Доказательство |
|---|---|---|
| **Крит (жизнеспособность)** | **Снят решением заказчика** | README §3 и roadmap §5.3 больше не обещают «NOP-стоп» на неполном потоке: явно сказано, что образ грузит run-окружение и запуск ограничивают `-t`. Добавлен `tests/stub/nop.bin` (all-NOP, 376 байт) — `./space -s 65520 -p 64 < tests/stub/nop.bin` → тишина, код 0 без `-t`. Остаточное поведение (пустой/короткий загрузчик без `-t` не завершается) — **принятый риск**, документирован (см. 11.3). Правка `do_read` (zero-fill) **не применялась** по решению заказчика. |
| Минимум `-s` (доки) | **Закрыто** | Минимум `-s 16 -p 1`; третье отношение `-s + 1 + NDEV <= 2^a` внесено в README и реализовано в `config.c` (сообщение `not enough room for the SYS and device channels (-s)`). `tests/cfg.sh`: `-s 16` → 0, `-s 15` → 2, `-s 30/31` → 2. |
| Потерянные тесты | **Закрыто** | `tests/cfg.sh` (11 проверок границ/фит-relations), `tests/wide.sh` (широкий формат, isz 62: write+echo), стаб-тест и тест «поток ниже seed count» в `tests/boot.sh`; копикод-тесты вынесены в `tests/copycode/`. |
| `errno` без сброса | **Закрыто** | `errno = 0;` перед `fread`/`fwrite` (`sys_ctrl.c:210,247`) и в `do_fstat`; тест `ENOSPC` через `/dev/full`. |
| Покрытие errors / `sys_device_pos` / `EBADF` | **Закрыто** | `test_errors()`: `sys_device_pos(0/1/2)`, `ENOSPC` (`/dev/full`), `EISDIR` (чтение каталога), `EBADF` (`CLOSE` не-канала). |
| `-Wshadow`/prototype | **Открыто** | Opt, прежние ревизии; приемлемо. |
| `audit.md` §8.4 «roadmap отсутствует» | **Закрыто** | §8.4 поправлен в этом проходе. |

### 11.3 Замечания (неблокирующие)

| Блок | Тип | Описание | Как исправить |
|---|---|---|---|
| `engine.c` (seed), `sys_ctrl.c` (`do_read`) | **Note (принятый риск)** | Пустой/короткий **загрузчик** без `-t` оставляет остаточные биты seed → бесконечный цикл (`./space < /dev/null` → не завершается). По решению заказчика это ответственность run-окружения; поведение задокументировано, стаб прилагается. | Оставить (решение заказчика). Опционально: если первый seed-рид вернул 0 байт — трактовать как «нет образа» и завершаться; это лишь UX-страховка, не требование. |
| `tests/copycode/scatter.sh` | Opt (тест) | Спецификация будущего копикода; `cd` исправлен на `../..`, но скрипт не запускается (копикод ещё не реализован в пространстве). | Оставить как спеку; при появлении рантайма копикода — подключить отдельным таргетом. |
| `tools/driver_gen.py` `--nop` | Opt | Требует `a>=8` (byte-format, как `--write`/`--echo`), поэтому стаб нельзя сгенерировать для `-s < 256`; для дефолта работает. | Не критично; при желании — отдельный «битовый» путь для мелочи `a<8`. |
| `bitcopy.c` aligned memcpy | Note | Перекрытие полуинтервалов остаётся UB в рамках trust-границы (задокументировано). | Оставить (граница принята). |
| `tests/legacy/*` | Note (доки) | Ссылаются на старую модель `DATA`; не запускаются. | Оставить как историю; при желании — пометить шапкой «historical». |
| `bitcopy.c:39`, `config.c` | Opt | Прежние `-Wshadow`/prototype/`-Wconversion` nits; `parse_size` `v > SIZE_MAX` на 64-бит всегда ложно. | Мелочи; по желанию. |

### 11.4 Проверка исполнением (доказательства)

- `make check` → `ALL CHECKS PASSED`: unit + **23** shell-проверки
  (4 e2e + 11 cfg + 2 wide + 6 boot), сборка `-Wall -Wextra` без предупреждений.
- ASan+UBSan+LSan (`detect_leaks=1`): unit-набор, прод-бинарь на
  `tests/stub/nop.bin`, `--write ABC` (wide `--s 1048576 -p 8` → `OK`) — чисто.
- Границы конфигурации: `-s 15`→2; `16..29`→0; `30/31`→2; `63`→2; `64/65`→0;
  `254/255`→2; `256`→0; `65520/65533`→0; `65534/65535`→2; `65536`→0.
- Путь памяти: `ulimit -v 200000` + `-s 2^40` → `exit 1` `out of memory`
  (чистый отказ, не падение). Все границы под ASan — без отчётов санитайзера.
- Дифференциальный фуззинг `bitcpy`: **0 / 300000**.
- Обе ветки широкого формата: `driver_gen --write OK` → вывод `4f4b`,
  `--echo` → `HI`; `fetch_instruction_rev128` покрыт.

### 11.5 Оценки

| Область | §10 | §11 | Обоснование |
|---|---|---|---|
| Память/ресурсы | 10 | **10/10** | malloc/free сбалансированы, LSan чист; путь OOM даёт чистый `exit 1`. |
| Безопасность/trust | 6 | **8/10** | `errno` сбрасывается, `FSTAT` не затирает статус последней операции, добавлены тесты I/O-ошибок; запись вне границ отбивается `-s/-p`-проверками. Остаток: принятый риск пустого загрузчика и UB-путь перекрытия в trust-границе. |
| Сложность/перф | 9 | **9/10** | Все операции O(n); SWAR-fetch, блочный I/O; `sys_tick` — два прохода O(proc_n)/такт. |
| Читаемость | 9 | **9/10** | Единый стиль, содержательные комментарии; nit `-Wshadow`. |
| Тесты | 5 | **8/10** | Возвращены границы конфигурации, широкий round-trip, останов all-NOP и поток ниже seed count; покрыты `errno`/`EBADF`/`ENOSPC`/`EISDIR`/`sys_device_pos`; копикод разведён. Остаток: путь OOM и `--nop` при `a<8` вне набора. |
| Соответствие докам | 6 | **9/10** | Минимум `-s` верен, загрузчик и третье отношение описаны, README/roadmap/config/тесты согласованы; §8.4 актуализирован. Остаток: legacy-тесты «historical». |

### 11.6 Вывод

Все реальные дефекты §10 закрыты: минимум `-s` и третье отношение исправлены
в коде и доках, `errno`/`FSTAT` приведены в порядок, возвращены и расширены
тесты (границы, широкий формат, останов, I/O-ошибки), тесты копикода
разведены. Крит §10 снят по прямому решению заказчика и корректно отражён в
документации; остаточное поведение (пустой загрузчик без `-t`) — принятый,
задокументированный риск, не дефект реализации. Новых блокирующих проблем не
обнаружено.

**Вердикт: [APPROVED].**
