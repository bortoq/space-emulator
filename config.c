/*
 * COMMAND LINE CONFIGURATION
 *
 * Parse flags -s/-p/-t into globals.c; derive instruction field widths
 * from the space size; check version limits.
 * Usage: space [-s bits] [-p num] [-t ticks] < user_space > result.
 */
#include "space.h"
#include <errno.h>

/* usage and exit */
static void usage(void)
{
  fprintf(stderr, "usage: space [-s bits] [-p num] [-t ticks]\n");
  exit(EXIT_USAGE);
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

/* field widths from the space size: address bits a = ceil(log2 space bits + 1)
   (positions 0..space_nbits, the last one is the DATA channel),
   count field one bit less, instruction = 2*address + count */
static void compute_format(void)
{
  size_t v;
  size a = 0;
  for(v = space_nbits; v; v >>= 1)
    ++a;
  address_bits = a;
  nfield_bits = (size)(a - 1);
  instruction_size = (size)(2 * a + (a - 1));
  o[0] = 0;
  s[0] = nfield_bits;
  o[1] = nfield_bits;
  s[1] = address_bits;
  o[2] = (size)(nfield_bits + address_bits);
  s[2] = address_bits;
}

/* flags into globals.c; derive format; check version limits */
void parse_config(int argc, char **argv)
{
  int i;
  size_t record;
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
  if(space_nbits < 8 || space_nbits > SPACE_BITS_MAX)
  {
    fprintf(stderr, "space: bits must be 8..%u (-s)\n", (unsigned)SPACE_BITS_MAX);
    usage();
  }
  compute_format();
  record = (size_t)EOF_BITS + (size_t)ERR_BITS + (size_t)nfield_bits;
  if(space_nbits <= record
     || proc_n > (space_nbits - record) / (size_t)instruction_size)
  {
    fprintf(stderr, "space: need processor instructions plus the boot status record (-s/-p)\n");
    usage();
  }
  if(proc_n > (((size_t)1 << nfield_bits) - 1) / (size_t)instruction_size)
  {
    fprintf(stderr, "space: boot count does not fit the n field (-s/-p)\n");
    usage();
  }
}
