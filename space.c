/*
 * EMULATION OF PARALLEL ADDRESS SPACE (single channel version)
 *
 * copyright 2022-12-17, dmitri bortoq, mit license
 *
 * Точка входа: начальная инструкция ядра и LIFE тактов движка engine.c.
 * Непересекаемость полуинтервалов обеспечивает программа,
 * эмулятор на неё полагается (не проверяет).
 */
#include "space.h"

int main()
{
  /* prepare address space */
  uint64_t time = LIFE;

  init_space();

  /* emulate computation (доработать такт и при halted) */
  while(time-- && !halted)
    computation();

  fflush(stdout);
  return 0;
}
