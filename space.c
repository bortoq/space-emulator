/*
 * EMULATION OF PARALLEL ADDRESS SPACE (SYS + pre-bound data channels)
 *
 * copyright 2022-12-17, dmitri bortoq, mit license
 *
 * Entry point: flag parsing, seed of the core, engine ticks.
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

  /*
   * emulate computation (tick runs to end even with no live instructions;
   * stop: all NOP or tick limit)
   */
  while((tick_limit == 0 || time--))
    if(!computation())
      break;

  fflush(stdout);
#ifdef SPACE_DEV
  if(dump_path != NULL)
  {
    size_t nbytes = space_nbits / (size_t)CHAR_BIT
                  + (space_nbits % (size_t)CHAR_BIT != 0);
    FILE *f = fopen(dump_path, "wb");
    if(f == NULL || fwrite(space, 1, nbytes, f) != nbytes)
    {
      fprintf(stderr, "space: cannot write dump %s\n", dump_path);
      if(f != NULL)
        fclose(f);
      free(space);
      free(processor);
      return EXIT_USAGE;
    }
    fclose(f);
  }
#endif
  free(space);
  free(processor);
  return 0;
}
