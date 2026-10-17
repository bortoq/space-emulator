/*
 * COMMAND LINE CONFIGURATION
 *
 * Parse flags -s/-p/-t into globals.c; derive instruction field widths
 * from the space size; check fit relations (no artificial caps).
 * Usage: space [-s bits] [-p num] [-t ticks] < user_space > result.
 */
#include "space.h"
#include "proto_gen.h"
#include <errno.h>

/* usage and exit */
static void usage(void)
{
  fprintf(stderr, "usage: space [-s bits] [-p num] [-t ticks] [--core FILE]\n");
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

/* decimal integer fitting size_t; overflow means usage */
static size_t parse_size(const char *s)
{
  uint64_t v = parse_ull(s);
  if(v > (uint64_t)SIZE_MAX)
    usage();
  return (size_t)v;
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

/*
 * field widths from the space size: address bits a = ceil(log2 space bits + 1)
 * (positions 0..2^a-1; positions space_nbits..2^a-1 are channels),
 * count field one bit less, instruction = 2*address + count
 */
static void compute_format(void)
{
  size_t v;
  size a = 0;
  if(space_nbits == 0)
    usage();
  for(v = space_nbits; v; v >>= 1)
    ++a;
  if(a < 1)
    usage();
  address_bits = a;
  nfield_bits = (size)(a - 1);
  instruction_size = (size)(2 * a + (a - 1));
  if(instruction_size == 0)
    usage();
  o[0] = 0;
  s[0] = nfield_bits;
  o[1] = nfield_bits;
  s[1] = address_bits;
  o[2] = (size)(nfield_bits + address_bits);
  s[2] = address_bits;
}

/* flags into globals.c; derive format; check fit relations only */
void parse_config(int argc, char **argv)
{
  int i;
  size_t maxboot;
  for(i = 1; i < argc; ++i)
  {
    const char *arg = argv[i];
    if(arg[0] == '-' && arg[1] == '-')
    {
      if(strcmp(arg, "--core") == 0)
      {
        if(++i >= argc)
          usage();
        core_path = argv[i];
        continue;
      }
#ifdef SPACE_DEV
      if(strcmp(arg, "--dump-space") == 0)
      {
        if(++i >= argc)
          usage();
        dump_path = argv[i];
        continue;
      }
#endif
      usage();
    }
    if(arg[0] != '-' || arg[1] == 0)
      usage();
    switch(arg[1])
    {
      case 's': space_nbits = parse_size(flag_value(&i, argc, argv, arg)); break;
      case 'p': proc_n = parse_size(flag_value(&i, argc, argv, arg)); break;
      case 't': tick_limit = parse_ull(flag_value(&i, argc, argv, arg)); break;
      default: usage();
    }
  }
  if(proc_n < 1)
  {
    fprintf(stderr, "space: need at least 1 instruction (-p)\n");
    usage();
  }
  if(space_nbits < 1)
  {
    fprintf(stderr, "space: need at least 1 bit (-s)\n");
    usage();
  }
  compute_format();
  if(proc_n > space_nbits / (size_t)instruction_size)
  {
    fprintf(stderr, "space: the processor must fit the space (-s/-p)\n");
    usage();
  }
  /*
   * the channel area is the positions space_nbits .. 2^a-1: it must hold
   * SYS plus every device pre-bound at start (see sys_ctrl.c). Otherwise
   * the emulator cannot bind the channels the program needs.
   */
  if(address_bits < (size)(sizeof(size_t) * CHAR_BIT))
  {
    size_t positions = (size_t)1 << address_bits;
    if(space_nbits + 1 + PROTO_NDEVS > positions)
    {
      fprintf(stderr,
              "space: not enough room for the SYS and device channels (-s)\n");
      usage();
    }
  }
  if(nfield_bits >= (size)(sizeof(size_t) * CHAR_BIT))
  {
    /* count field wider than size_t: any fitting proc_n fits it */
    return;
  }
  maxboot = (((size_t)1 << nfield_bits) - 1) / (size_t)instruction_size;
  if(proc_n > maxboot)
  {
    fprintf(stderr, "space: seed count does not fit the n field (-s/-p)\n");
    usage();
  }
}
