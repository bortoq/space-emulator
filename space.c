/*
 * EMULATION OF PARALLEL ADDRESS SPACE (single channel version)
 *
 * copyright 2022-12-17, dmitri bortoq, mit license
 *
 * Entry point: flag parsing, boot instruction of the core, engine ticks.
 * Usage: space [-s bits] [-p num] [-t ticks] < user_space > result.
 * The program guarantees half-interval non-overlap,
 * the emulator relies on it (never checks).
 */
#include "space.h"

int main(int argc, char **argv)
{
  /* configuration and address space */
  uint64_t time;

  parse_config(argc, argv);
  time = tick_limit;

  init_space();

  /* emulate computation (tick runs to end even with no live instructions;
     stop: all NOP or tick limit) */
  while((tick_limit == 0 || time--))
    if(!computation())
      break;

  channel_flush();
  fflush(stdout);
  free(space);
  free(processor);
  return 0;
}
