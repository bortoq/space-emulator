/*
 * EMULATION OF PARALLEL ADDRESS SPACE (fixed variant)
 *
 * Оригинал: dmitri bortoq, 2022, MIT license.
 * Эмуляция: каждый такт последовательно исполняет PROCESSOR_N
 * инструкций "copy n, dst, src" над битовым пространством space.
 * Параллелизм хоста не используется; согласованность (непересекающиеся
 * интервалы в одном такте) — ответственность программы.
 */

#include "space.h"

/* Прочитать поле инструкции как число (без затирания длины). */
static inline size get_argument(size_t src, size_t n)
{
  size v = 0;                       /* guard: поле шире size обрезается */
  if (n > sizeof v * CHAR_BIT)
    n = sizeof v * CHAR_BIT;
  bitcpy(n, space, src, &v, 0);
  return v;
}

/* Прочитать битовый образ i-й инструкции в таблицу процессора. */
static inline void fetch_instruction(size_t i)
{
  size_t a;
  if (i >= PROCESSOR_N) return;     /* guard */
  for (a = 0; a < N_FIELDS; ++a)    /* FIX: было sizeof s (=6) и индекс [0] */
    processor[i][a] = get_argument(PROCESSOR_START + i * INSTRUCTION_SIZE + o[a], s[a]);
}

/* Записать разобранную инструкцию в битовый образ (для init). */
static inline void store_instruction(size_t i, size n, size dst, size src)
{
  if (i >= PROCESSOR_N) return;     /* guard */
  bitcpy(s[0], &n, 0, space, INSTRUCTION_SIZE * i + o[0]);
  bitcpy(s[1], &dst, 0, space, INSTRUCTION_SIZE * i + o[1]);
  bitcpy(s[2], &src, 0, space, INSTRUCTION_SIZE * i + o[2]);
}

/* Проверка границ инструкции перед копированием. */
static inline int instruction_ok(size n, size dst, size src)
{
  if ((size_t)n > SPACE_SIZE) return 0;
  if ((size_t)dst + (size_t)n > SPACE_SIZE) return 0;
  if ((size_t)src + (size_t)n > SPACE_SIZE) return 0;
  return 1;
}

/* Загрузить и исполнить инструкции копирования над пространством. */
static void computation(void)
{
  size_t i;
  for (i = 0; i < PROCESSOR_N; ++i) {
    fetch_instruction(i);
    if (!instruction_ok(processor[i][0], processor[i][1], processor[i][2]))
      continue;                     /* guard: выход за space — пропуск */
    if (processor[i][0] == 0)
      continue;                     /* nop */
    bitcpy(processor[i][0], space, processor[i][2], space, processor[i][1]);
  }
}

/*
 * Обмен с файлом f байтово-выровненной битовой строки.
 * s — бит, a — битовый адрес. Возврат 0 при успехе.
 * FIX: было `s /= CHAR_BIT > 0` (делило на 1) и сравнение
 * результата fwrite/fread с s вместо числа элементов.
 */
static inline int channel(size_t (*copy)(void *, size_t, size_t, FILE *),
                          FILE *f, size n_bits, size addr_bits)
{
  size_t n_bytes;
  size_t done;
  if (copy == NULL || f == NULL) return -1;      /* guard */
  if (addr_bits % CHAR_BIT != 0) return -1;      /* guard: только байтовые адреса */
  if ((n_bits % CHAR_BIT) != 0) return -1;       /* guard: только целые байты */
  n_bytes = (size_t)n_bits / CHAR_BIT;
  if (n_bytes == 0) return 0;                    /* nop */
  if ((size_t)addr_bits / CHAR_BIT + n_bytes > sizeof space) return -1;
  done = copy(&space[addr_bits / CHAR_BIT], 1, n_bytes, f);
  return (done == n_bytes) ? 0 : -1;
}

/*
 * Ввод из stdin в space, вывод из space в stdout.
 * Использует дескрипторы CH_IN / CH_OUT (см. core).
 * FIX: оригинал читал processor[1]/processor[2] до их инициализации
 * и путал индексы (ядро описывает каналы в [0] и [1]).
 */
static int channels(void)
{
  int code;
  fetch_instruction(CH_IN);         /* дескрипторы всегда свежие */
  fetch_instruction(CH_OUT);
  code = channel((size_t (*)(void *, size_t, size_t, FILE *))fread,
                 stdin, processor[CH_IN][0], processor[CH_IN][1]);
  if (code != 0 && feof(stdin)) return -1; /* EOF — останов */
  if (code != 0) return -1;
  code = channel((size_t (*)(void *, size_t, size_t, FILE *))fwrite,
                 stdout, processor[CH_OUT][0], processor[CH_OUT][1]);
  return code;
}

/* Построить битовый образ ядра и первичную таблицу процессора. */
static void init_space(void)
{
  size_t i;
  memset(space, 0, sizeof space);
  memset(processor, 0, sizeof processor);
  for (i = 0; i < 3; ++i) {
    processor[i][0] = core[i * 3];
    processor[i][1] = core[i * 3 + 1];
    processor[i][2] = core[i * 3 + 2];
    store_instruction(i, core[i * 3], core[i * 3 + 1], core[i * 3 + 2]);
  }
}

int main(void)
{
  uint64_t time = LIFE;             /* такты эмуляции */

  init_space();                     /* FIX: образ собирается побитово,
                                       а не сырым memcpy ядра */

  while (time-- && channels() == 0)
    computation();

  fflush(stdout);
  return 0;
}
