/*
 * DISASSEMBLER (fixed variant)
 *
 * Читает битовый код инструкций из stdin, печатает тройки "n dst src".
 *
 * Исправления оригинала:
 *  - цикл по N_FIELDS вместо `sizeof s`;
 *  - аккуратный цикл чтения (без лишней итерации через do/feof);
 *  - читаемый формат 0xXXXX; проверка ошибок.
 */

#include "space.h"

/* Прочитать код i-й инструкции из образа в таблицу. */
static inline void read_code(size_t i)
{
  size_t a;
  if (i >= PROCESSOR_N) return;     /* guard */
  for (a = 0; a < N_FIELDS; ++a)    /* FIX: было sizeof s */
    bitcpy(s[a], space, INSTRUCTION_SIZE * i + o[a], &processor[i][a], 0);
}

/*
 * Прочитать очередной кусок кода из stdin в образ.
 * Возврат: число целых инструкций (неполный хвост отбрасывается
 * с предупреждением).
 */
static size_t read_codes(void)
{
  size_t got = fread(space, 1, sizeof space, stdin);
  size_t n = got * CHAR_BIT / INSTRUCTION_SIZE;
  size_t i;
  if (got * CHAR_BIT % INSTRUCTION_SIZE != 0)
    fprintf(stderr, "disassembler: warning: %lu trailing bits ignored\n",
            (unsigned long)(got * CHAR_BIT % INSTRUCTION_SIZE));
  for (i = 0; i < n; ++i)
    read_code(i);
  return n;
}

/* Напечатать одну инструкцию. */
static void write_instruction(size_t i)
{
  printf("0x%04X 0x%04X 0x%04X\n",
         processor[i][0], processor[i][1], processor[i][2]);
}

int main(void)
{
  size_t n;
  size_t i;

  memset(space, 0, sizeof space);
  memset(processor, 0, sizeof processor);

  while ((n = read_codes()) > 0) {
    for (i = 0; i < n; ++i)
      write_instruction(i);
    if (feof(stdin)) break;
    if (ferror(stdin)) {
      fprintf(stderr, "disassembler: read error\n");
      return EXIT_FAILURE;
    }
    memset(space, 0, sizeof space);
    memset(processor, 0, sizeof processor);
  }
  if (ferror(stdin)) {
    fprintf(stderr, "disassembler: read error\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
