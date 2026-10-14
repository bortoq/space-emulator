/*
 * COMMAND LINE CONFIGURATION
 *
 * Parse flags -s/-p/-t into globals.c; check version limits.
 * Usage: space [-s bits] [-p num] [-t ticks] < user_space > result.
 */
#include "space.h"
#include <errno.h>

/* usage and exit 2 */
static void usage(void)
{
  fprintf(stderr, "usage: space [-s bits] [-p num] [-t ticks]\n");
  exit(2);
}

/* decimal integer; error means usage */
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

/* flag value: rest of the same argument (-s384) or the next one (-s 384) */
static const char *flag_value(int *i, int argc, char **argv, const char *arg)
{
  if(arg[2])
    return arg + 2;
  ++*i;
  if(*i >= argc)
    usage();
  return argv[*i];
}

/* flags into globals.c; check version limits */
void parse_config(int argc, char **argv)
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
  if(space_nbits < 1 || space_nbits > (1ULL << ADDRESS_SIZE))
  {
    fprintf(stderr, "space: bits must be 1..%llu (-s)\n", 1ULL << ADDRESS_SIZE);
    usage();
  }
  if(space_nbits < 8 || proc_n > (space_nbits - 8) / INSTRUCTION_SIZE)
  {
    fprintf(stderr, "space: need processor image plus state byte outside it (-s/-p)\n");
    usage();
  }
}
