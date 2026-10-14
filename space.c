/*
 * EMULATION OF PARALLEL ADDRESS SPACE (single channel version)
 *
 * copyright 2022-12-17, dmitri bortoq, mit license
 *
 * Точка входа: разбор флагов, начальная инструкция ядра и такты engine.c.
 * Использование: space [-s bits] [-p num] [-t ticks] < user_space > result.
 * Непересекаемость полуинтервалов обеспечивает программа,
 * эмулятор на неё полагается (не проверяет).
 */
#include "space.h"
#include <errno.h>

/* usage и выход 2 */
static void usage(void)
{
  fprintf(stderr, "usage: space [-s bits] [-p num] [-t ticks]\n");
  exit(2);
}

/* целое десятичное число; ошибка — usage */
static uint64_t parse_ull(const char *s)
{
  char *end;
  uint64_t v;
  if(*s < '0' || *s > '9')
    usage();
  errno = 0;
  v = strtoull(s, &end, 10);
  if(errno || *end)
    usage();
  return v;
}

/* значение флага: остаток того же аргумента (-s384) или следующий (-s 384) */
static const char *flag_value(int *i, int argc, char **argv, const char *arg)
{
  if(arg[2])
    return arg + 2;
  ++*i;
  if(*i >= argc)
    usage();
  return argv[*i];
}

/* флаги -s/-p/-t в globals.c; проверка пределов версии */
static void parse_config(int argc, char **argv)
{
  int i;
  for(i = 1; i < argc; ++i)
  {
    const char *arg = argv[i];
    if(arg[0] != '-' || arg[1] == 0)
      usage();
    switch(arg[1])
    {
      case 's': space_nbits = (size_t)parse_ull(flag_value(&i, argc, argv, arg)); break;
      case 'p': proc_n = (size_t)parse_ull(flag_value(&i, argc, argv, arg)); break;
      case 't': tick_limit = parse_ull(flag_value(&i, argc, argv, arg)); break;
      default: usage();
    }
  }
  if(proc_n < 1)
  {
    fprintf(stderr, "space: need at least 1 instruction (-p)\n");
    usage();
  }
  if(space_nbits % INSTRUCTION_SIZE || space_nbits > SPACE_BITS_MAX)
  {
    fprintf(stderr, "space: bits must be a nonzero multiple of 48 up to 65536 (-s)\n");
    usage();
  }
  if(proc_n > space_nbits / INSTRUCTION_SIZE)
  {
    fprintf(stderr, "space: processor image does not fit (-p/-s)\n");
    usage();
  }
}

int main(int argc, char **argv)
{
  /* configuration and address space */
  uint64_t time;

  parse_config(argc, argv);
  time = tick_limit;

  init_space();

  /* emulate computation (такт до конца и при halted) */
  while(!halted && (tick_limit == 0 || time--))
    if(!computation())
      break;

  fflush(stdout);
  free(space);
  free(processor);
  return 0;
}
