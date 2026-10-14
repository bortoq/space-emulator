/*
 * ASSEMBLER (fixed variant)
 *
 * Читает десятичные тройки "n dst src" из stdin, пишет битовый код
 * инструкций в stdout. Пакетирует по CHAR_BIT инструкций.
 *
 * Исправления оригинала:
 *  - цикл по N_FIELDS вместо `sizeof s`;
 *  - запись в поле [a], а не всегда в [0];
 *  - корректная обработка EOF/ошибки разбора (без бесконечного цикла);
 *  - проверка fwrite; коды возврата EXIT_SUCCESS/FAILURE.
 */

#include "space.h"

/* Прочитать одну тройку из stdin в слот i. Возврат как у scanf. */
static int read_instruction(size_t i)
{
  if (i >= PROCESSOR_N) return 0;   /* guard */
  return scanf("%hu %hu %hu%*[\t -~]",
               &processor[i][0], &processor[i][1], &processor[i][2]);
}

/*
 * Скомпилировать следующую тройку во временный слот.
 * Возврат: 1 — инструкция готова, 0 — EOF, -1 — ошибка разбора.
 */
static int compile_instruction(size_t n)
{
  int rc = read_instruction(n % CHAR_BIT);
  if (rc == EOF || rc == 0)
    return (feof(stdin)) ? 0 : (rc == 0 ? -1 : 0);
  if (rc == 3) return 1;
  return -1;
}

/* Сбросить i инструкций из битового образа в stdout. Возврат 0 при успехе. */
static int flush_space(size_t i)
{
  size_t bytes = (INSTRUCTION_SIZE * i + CHAR_BIT - 1) / CHAR_BIT;
  if (i == 0) return 0;
  if (fwrite(space, bytes, 1, stdout) != 1) {
    fprintf(stderr, "assembler: write error\n");
    return -1;
  }
  return 0;
}

/* Сохранить код n-й инструкции из временного слота в образ space. */
static void write_code(size_t n)
{
  size_t i = n % CHAR_BIT;
  size_t a;
  for (a = 0; a < N_FIELDS; ++a)    /* FIX: было sizeof s и processor[i][a] */
    bitcpy(s[a], &processor[i][a], 0, space, INSTRUCTION_SIZE * i + o[a]);
}

int main(void)
{
  size_t n = 0;
  int rc;

  memset(space, 0, sizeof space);
  memset(processor, 0, sizeof processor);

  for (;;) {
    rc = compile_instruction(n);
    if (rc == 0) break;             /* EOF — конец */
    if (rc < 0) {
      fprintf(stderr, "assembler: compilation error at instruction %lu\n",
              (unsigned long)n);
      return EXIT_FAILURE;
    }
    write_code(n);
    if (++n % CHAR_BIT == 0) {
      if (flush_space(CHAR_BIT) != 0)
        return EXIT_FAILURE;
      memset(space, 0, sizeof space); /* чистый следующий пакет */
    }
    if (n >= SPACE_SIZE / INSTRUCTION_SIZE) {
      fprintf(stderr, "assembler: space full at instruction %lu\n",
              (unsigned long)n);
      return EXIT_FAILURE;
    }
  }
  if (flush_space(n % CHAR_BIT) != 0)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
