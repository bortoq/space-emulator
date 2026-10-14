/*
 * ADDRESS SPACE ENGINE (single channel version)
 *
 * Each tick has two phases: first read all
 * "copy n, dst, src" instructions from the processor positions,
 * then execute all copies over the bit space.
 * An instruction whose source half-interval starts at the last
 * position of the space is input (stdin -> space),
 * one whose destination half-interval starts there is
 * output (space -> stdout); the rest are
 * ordinary internal copies. Channel access transfers any number
 * of bits at any bit position; stream bits continue across
 * accesses (one stream byte holds 8 bits, first bit is the high
 * one). A short transfer places what is available and never stops.
 * A query writes the state and the count of the last transfer
 * of that direction.
 * Writes to processor positions (including the load) take effect
 * from the next tick; position order inside phases does not matter.
 * Half-interval non-overlap is never checked: the program
 * guarantees it, the emulator relies on it.
 */
#include "space.h"
#ifdef SIGPIPE
#include <signal.h>
#endif

FILE *channel_in = NULL;
FILE *channel_out = NULL;

/* stream bit cursors: unread bits of the current input byte and
   bits already collected into the current output byte */
static unsigned char in_byte;
static int in_left;
static unsigned char out_byte;
static int out_used;
static size in_copied;   /* bits moved by the last input */
static size out_copied;  /* bits moved by the last output */

/* test one space bit (bit 0 of a byte is the high bit) */
static inline int space_bit(size_t p)
{
  return (space[p / CHAR_BIT] >> (CHAR_BIT - 1 - p % CHAR_BIT)) & 1;
}

/* set one space bit (bit 0 of a byte is the high bit) */
static inline void space_set_bit(size_t p, int v)
{
  unsigned char *b = space + p / CHAR_BIT;
  unsigned char m = (unsigned char)(1 << (CHAR_BIT - 1 - p % CHAR_BIT));
  if(v)
    *b = (unsigned char)(*b | m);
  else
    *b = (unsigned char)(*b & ~m);
}

/* return instruction argument: value bit k at position src + k */
static inline size get_argument(size src, size n)
{
  size v = 0;
  size k;
  if(n > sizeof v * CHAR_BIT)
    n = sizeof v * CHAR_BIT;
  for(k = 0; k < n; ++k)
    if(space_bit(src + k))
      v = (size)(v | (size)1 << k);
  return v;
}

/* write instruction argument: value bit k at position at + k */
static inline void store_argument(size n, size value, size at)
{
  size k;
  for(k = 0; k < n; ++k)
    space_set_bit(at + k, (value >> k) & 1);
}

/* write the transfer count as bytes after the state
   (low byte of the value first, bit 0 of a byte is the high bit) */
static inline void store_count(size at, size value)
{
  size b = 0;
  size total = (size)(nfield_bits + CHAR_BIT - 1) / CHAR_BIT;
  while(b < total)
  {
    unsigned char byte = (unsigned char)((value >> (b * CHAR_BIT)) & 0xFF);
    size left = (size)(nfield_bits - b * CHAR_BIT);
    size w = left < CHAR_BIT ? left : (size)CHAR_BIT;
    bitcpy(w, &byte, 0, space, (size)(at + b * CHAR_BIT));
    ++b;
  }
}

/* read "i-th instruction" bit string from bit space to the processor mechanism */
static inline void fetch_instruction(size i)
{
  size a;
  if((size_t)i >= proc_n)
    return;
  for(a = 0; a < N_FIELDS; ++a)
    processor[i][a] = get_argument(PROCESSOR_START + i * instruction_size + o[a], s[a]);
}

/* write parsed instruction bit string to processor positions (init) */
static inline void store_instruction(size i, size n, size dst, size src)
{
  if((size_t)i >= proc_n)
    return;
  store_argument(s[0], n, instruction_size * i + o[0]);
  store_argument(s[1], dst, instruction_size * i + o[1]);
  store_argument(s[2], src, instruction_size * i + o[2]);
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
  in_byte = 0;
  in_left = 0;
  out_byte = 0;
  out_used = 0;
  in_copied = 0;
  out_copied = 0;
}

/* copy n_bits from f to the half-interval [addr_bits, addr_bits + n_bits).
   any n_bits and addr_bits; stream bits continue across accesses.
   return 0 if full, -1 if skipped or short (end/failure seen via query) */
int space_input(size n_bits, size addr_bits, FILE *f)
{
  in_copied = 0;
  if(f == NULL)
    return -1;
  if(n_bits == 0)
    return 0;
  if((size_t)addr_bits + (size_t)n_bits > space_nbits)
    return -1;
  while(n_bits)
  {
    size take;
    if(in_left == 0)
    {
      if(fread(&in_byte, 1, 1, f) != 1)
        return -1;
      in_left = CHAR_BIT;
    }
    take = (size)in_left < n_bits ? (size)in_left : n_bits;
    bitcpy(take, &in_byte, CHAR_BIT - in_left, space, addr_bits);
    in_left -= (int)take;
    n_bits -= take;
    addr_bits = (size)(addr_bits + take);
    in_copied = (size)(in_copied + take);
  }
  return 0;
}

/* copy n_bits from the half-interval [addr_bits, addr_bits + n_bits) to f.
   any n_bits and addr_bits; bits stay collected until a byte is full.
   return 0 if full, -1 if skipped or failed */
int space_output(size n_bits, size addr_bits, FILE *f)
{
  out_copied = 0;
  if(f == NULL)
    return -1;
  if(n_bits == 0)
    return 0;
  if((size_t)addr_bits + (size_t)n_bits > space_nbits)
    return -1;
  while(n_bits)
  {
    size take;
    take = (size)(CHAR_BIT - out_used) < n_bits ? (size)(CHAR_BIT - out_used) : n_bits;
    bitcpy(take, space, addr_bits, &out_byte, out_used);
    out_used += (int)take;
    n_bits -= take;
    addr_bits = (size)(addr_bits + take);
    out_copied = (size)(out_copied + take);
    if(out_used == CHAR_BIT)
    {
      int ok = fwrite(&out_byte, 1, 1, f) == 1;
      out_used = 0;
      if(!ok)
        return -1;
    }
  }
  return 0;
}

/* write collected output bits (zero padded) at program end */
void channel_flush(void)
{
  if(out_used && channel_out != NULL)
  {
    fwrite(&out_byte, 1, 1, channel_out);
    out_used = 0;
  }
}

/* state byte of the read side: end of input seen, input failure
   (sticky stdio flags, query never clears them) */
static unsigned char input_state(void)
{
  unsigned char st = 0;
  if(channel_in != NULL)
  {
    if(feof(channel_in))
      st |= STATE_END;
    if(ferror(channel_in))
      st |= STATE_IN_FAIL;
  }
  return st;
}

/* state byte of the write side: output failure */
static unsigned char output_state(void)
{
  unsigned char st = 0;
  if(channel_out != NULL && ferror(channel_out))
    st |= STATE_OUT_FAIL;
  return st;
}

/* query target (STATE_BITS) must fit and lie outside the processor.
   alignment is not required: bitcpy writes it */
static inline int query_ok(size addr_bits)
{
  if((size_t)addr_bits + STATE_BITS + nfield_bits > space_nbits)
    return 0;
  if((size_t)addr_bits < proc_n * instruction_size)
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
      bitcpy(STATE_BITS, &st, 0, space, dst);
      store_count((size)(dst + STATE_BITS), in_copied);
      return;
    }
    if(dst == last && src != last && query_ok(src))
    {
      unsigned char st = output_state();
      bitcpy(STATE_BITS, &st, 0, space, src);
      store_count((size)(src + STATE_BITS), out_copied);
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
    exit(EXIT_MEMORY);
  }
  memset(space, 0, (space_nbits + CHAR_BIT - 1) / CHAR_BIT);
  memset(processor, 0, proc_n * sizeof *processor);
  boot_n = (size)(proc_n * instruction_size);
  boot_last = (size)(space_nbits - 1);
  processor[0][0] = boot_n;
  processor[0][1] = PROCESSOR_START;
  processor[0][2] = boot_last;
  store_instruction(0, boot_n, PROCESSOR_START, boot_last);
}
