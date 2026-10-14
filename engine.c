/*
 * ADDRESS SPACE ENGINE (single channel version)
 *
 * Each tick has two phases: first read all
 * "copy n, dst, src" instructions from the special positions,
 * then execute all copies over the bit space.
 * An instruction whose source half-interval starts at the last
 * position of the space is input (stdin -> space),
 * one whose destination half-interval starts there is
 * output (space -> stdout); the rest are
 * ordinary internal copies. Writes to special positions
 * (including the load) take effect from the next tick; position order
 * inside phases does not matter. Half-interval non-overlap
 * is never checked: the program guarantees it, the emulator relies on it.
 */
#include "space.h"
#ifdef SIGPIPE
#include <signal.h>
#endif

FILE *channel_in = NULL;
FILE *channel_out = NULL;

/* return bit string of instruction argument as a number */
static inline size get_argument(size src, size n)
{
  size v = 0;
  if(n > sizeof v * CHAR_BIT)
    n = sizeof v * CHAR_BIT;
  bitcpy(n, space, src, &v, 0);
  return v;
}

/* read "i-th instruction" bit string from bit space to the processor mechanism */
static inline void fetch_instruction(size i)
{
  size a;
  if((size_t)i >= proc_n)
    return;
  for(a = 0; a < N_FIELDS; ++a)
    processor[i][a] = get_argument(PROCESSOR_START + i * INSTRUCTION_SIZE + o[a], s[a]);
}

/* write parsed instruction bit string to special positions (init) */
static inline void store_instruction(size i, size n, size dst, size src)
{
  if((size_t)i >= proc_n)
    return;
  bitcpy(s[0], &n, 0, space, INSTRUCTION_SIZE * i + o[0]);
  bitcpy(s[1], &dst, 0, space, INSTRUCTION_SIZE * i + o[1]);
  bitcpy(s[2], &src, 0, space, INSTRUCTION_SIZE * i + o[2]);
}

/* check instruction half-intervals before copy */
static inline int instruction_ok(size n, size dst, size src)
{
  if((size_t)n > space_nbits)
    return 0;
  if((size_t)dst + (size_t)n > space_nbits)
    return 0;
  if((size_t)src + (size_t)n > space_nbits)
    return 0;
  return 1;
}

/* channel streams default to stdio (already assigned ones stay, for tests) */
static inline void init_channels(void)
{
  if(channel_in == NULL)
    channel_in = stdin;
  if(channel_out == NULL)
    channel_out = stdout;
}

/* copy n_bits from f to the half-interval [addr_bits, addr_bits + n_bits).
   return 0 if full, -1 if skipped or short (end/failure seen via query) */
int space_input(size n_bits, size addr_bits, FILE *f)
{
  size_t n_bytes;
  size_t done;
  if(f == NULL)
    return -1;
  if(n_bits == 0)
    return 0;
  if(n_bits % CHAR_BIT != 0)
    return -1;
  if(addr_bits % CHAR_BIT != 0)
    return -1;
  n_bytes = (size_t)n_bits / CHAR_BIT;
  if((size_t)addr_bits + (size_t)n_bits > space_nbits)
    return -1;
  done = fread(&space[addr_bits / CHAR_BIT], 1, n_bytes, f);
  if(done != n_bytes)
    return -1;
  return 0;
}

/* copy n_bits from the half-interval [addr_bits, addr_bits + n_bits) to f.
   return 0 if full, -1 if skipped or short (end/failure seen via query) */
int space_output(size n_bits, size addr_bits, FILE *f)
{
  size_t n_bytes;
  size_t done;
  if(f == NULL)
    return -1;
  if(n_bits == 0)
    return 0;
  if(n_bits % CHAR_BIT != 0)
    return -1;
  if(addr_bits % CHAR_BIT != 0)
    return -1;
  n_bytes = (size_t)n_bits / CHAR_BIT;
  if((size_t)addr_bits + (size_t)n_bits > space_nbits)
    return -1;
  done = fwrite(&space[addr_bits / CHAR_BIT], 1, n_bytes, f);
  if(done != n_bytes)
    return -1;
  return 0;
}

/* state byte of the read side: bit 0 means end of input seen,
   bit 1 means input failure (sticky stdio flags, query never clears them) */
static unsigned char input_state(void)
{
  unsigned char st = 0;
  if(channel_in != NULL)
  {
    if(feof(channel_in))
      st |= 0x01;
    if(ferror(channel_in))
      st |= 0x02;
  }
  return st;
}

/* state byte of the write side: bit 0 means output failure */
static unsigned char output_state(void)
{
  unsigned char st = 0;
  if(channel_out != NULL && ferror(channel_out))
    st |= 0x01;
  return st;
}

/* query target (8 bits) must fit and lie outside the processor.
   alignment is not required: bitcpy writes it */
static inline int query_ok(size addr_bits)
{
  if((size_t)addr_bits + 8 > space_nbits)
    return 0;
  if((size_t)addr_bits < proc_n * INSTRUCTION_SIZE)
    return 0;
  return 1;
}

/* execute the already fetched i-th instruction:
   ordinary copy, channel transfer, or channel state query.
   NOP with LAST is a state query (the single exception). */
void run_instruction(size i)
{
  size n, dst, src, last;
  if((size_t)i >= proc_n)
    return;
  n = processor[i][0];
  dst = processor[i][1];
  src = processor[i][2];
  if(n == 0)
  {
    last = (size)(space_nbits - 1);
    if(src == last && dst != last && query_ok(dst))
    {
      unsigned char st = input_state();
      bitcpy(8, &st, 0, space, dst);
      return;
    }
    if(dst == last && src != last && query_ok(src))
    {
      unsigned char st = output_state();
      bitcpy(8, &st, 0, space, src);
      return;
    }
    return;
  }
  last = (size)(space_nbits - 1);
  if(src == last && dst == last)
    return;
  if(src == last)
  {
    space_input(n, dst, channel_in);
    return;
  }
  if(dst == last)
  {
    space_output(n, src, channel_out);
    return;
  }
  if(!instruction_ok(n, dst, src))
    return;
  bitcpy(n, space, src, space, dst);
}

/* one tick: first read all instructions, then execute all copies.
   return tick state: 1 if executed (a non-NOP was present), else 0.
   channel accesses follow instruction order,
   because stdin/stdout are sequential */
int computation(void)
{
  size i;
  int state = 0;
  for(i = 0; (size_t)i < proc_n; ++i)
  {
    fetch_instruction(i);
    if(processor[i][0])
      state = 1;
  }
  if(!state)
    return 0;
  for(i = 0; (size_t)i < proc_n; ++i)
    run_instruction(i);
  return 1;
}

/* allocate space, write the single boot instruction bit string */
void init_space(void)
{
  size boot_n, boot_last;
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
  init_channels();
  space = malloc((space_nbits + CHAR_BIT - 1) / CHAR_BIT);
  processor = malloc(proc_n * sizeof *processor);
  if(space == NULL || processor == NULL)
  {
    fprintf(stderr, "space: out of memory\n");
    exit(1);
  }
  memset(space, 0, (space_nbits + CHAR_BIT - 1) / CHAR_BIT);
  memset(processor, 0, proc_n * sizeof *processor);
  boot_n = (size)(proc_n * INSTRUCTION_SIZE);
  boot_last = (size)(space_nbits - 1);
  processor[0][0] = boot_n;
  processor[0][1] = PROCESSOR_START;
  processor[0][2] = boot_last;
  store_instruction(0, boot_n, PROCESSOR_START, boot_last);
}
