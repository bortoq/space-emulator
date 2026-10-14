/*
 * EMULATION OF PARALLEL ADDRESS SPACE (single channel version)
 *
 * copyright 2022-12-17, dmitri bortoq, mit license
 *
 * Точка входа: начальная инструкция ядра и такты движка engine.c.
 * Необязательный аргумент — предел тактов (нет аргумента — без предела).
 * Непересекаемость полуинтервалов обеспечивает программа,
 * эмулятор на неё полагается (не проверяет).
 */
#include "space.h"
#include <errno.h>

/* предел тактов из argv[1] (нет аргумента — без предела); ошибка — выход 2 */
static uint64_t parse_limit(int argc, char **argv)
{
  const char *s;
  char *end;
  uint64_t v;
  if(argc < 2)
    return 0;
  if(argc > 2)
  {
    fprintf(stderr, "usage: space [max_ticks]\n");
    exit(2);
  }
  s = argv[1];
  if(*s < '0' || *s > '9')
  {
    fprintf(stderr, "usage: space [max_ticks]\n");
    exit(2);
  }
  errno = 0;
  v = strtoull(s, &end, 10);
  if(errno || *end)
  {
    fprintf(stderr, "usage: space [max_ticks]\n");
    exit(2);
  }
  return v;
}

int main(int argc, char **argv)
{
  /* prepare address space */
  uint64_t limit = parse_limit(argc, argv);
  uint64_t time = limit;

  init_space();

  /* emulate computation (такт до конца и при halted) */
  while(!halted && (limit == 0 || time--))
    computation();

  fflush(stdout);
  return 0;
}
