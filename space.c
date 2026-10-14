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
