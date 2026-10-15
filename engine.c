/*
 * ADDRESS SPACE ENGINE (single channel version)
 *
 * Each tick has two phases: first read all
 * "copy n, dst, src" instructions from the processor positions,
 * then execute all copies over the bit space.
 * The single channel DATA is the position just after the space
 * (space_nbits). An instruction whose source field is DATA is input
 * (stdin -> space), one whose destination field is DATA is output
 * (space -> stdout); the rest are ordinary internal copies.
 * A channel transfer carries its own status in the tail of its
 * half-interval, so the program reads it right after the transfer:
 * input  [dst, dst+n): data in [dst, dst+n-Sr), status in
 * [dst+n-Sr, dst+n): end bit (1), errno (8), size (nfield_bits).
 * output [src, src+n): data in [src, src+n-Sw), status in
 * [src+n-Sw, src+n): errno (8), size (nfield_bits).
 * Sr = EOF_BITS + ERR_BITS + nfield_bits, Sw = ERR_BITS + nfield_bits.
 * The status numeric fields are low bit first (bit k at offset k);
 * errno is 0 without failure. A short transfer fills the data bits it
 * got and never stops. The untouched data bits keep their old value.
 * Stream bits continue across accesses (one stream byte holds 8 bits,
 * the first bit is the high one). Writes to processor positions
 * (including the load) take effect from the next tick; position order
 * inside phases does not matter. Half-interval non-overlap and in-space
 * bounds are never checked: the program guarantees them, the emulator
 * relies on it.
 */
#include "space.h"
#include <errno.h>
#ifdef SIGPIPE
#include <signal.h>
#endif

FILE *channel_in = NULL;
FILE *channel_out = NULL;

/*
 * stream bit cursors: unread bits of the current input byte and
 * bits already collected into the current output byte
 */
static unsigned char in_byte;
static int in_left;
static unsigned char out_byte;
static int out_used;

/* stream error numbers captured when the failing operation runs */
static unsigned char in_error;
static unsigned char out_error;

/* data bits moved by the last input: the boot load uses it */
static size last_input_bits;

/* the boot load runs in the first tick; it adds the status record */
static int boot_pending = 0;

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
    n = (size)(sizeof v * CHAR_BIT);
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

/* bits in the status tail of an input (eof, errno, size) */
static inline size read_status_bits(void)
{
  return (size)(EOF_BITS + ERR_BITS + nfield_bits);
}

/* bits in the status tail of an output (errno, size) */
static inline size write_status_bits(void)
{
  return (size)(ERR_BITS + nfield_bits);
}

/*
 * 8-bit error number of a stream: 0 when clean, else the code captured
 * at the failed operation (or ERR_IO when there is no code)
 */
static unsigned char op_errno(FILE *f, unsigned char stored)
{
  if(f == NULL)
    return (unsigned char)ERR_IO;
  if(!ferror(f))
    return 0;
  if(stored)
    return stored;
  return (unsigned char)ERR_IO;
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
}

/*
 * input: half-interval [addr_bits, addr_bits + n_bits); data first,
 * then the status tail (eof, errno, size). return 0 on a channel
 * transfer, -1 when there is no stream.
 */
int space_input(size n_bits, size addr_bits, FILE *f)
{
  size data, moved, base, take;
  last_input_bits = 0;
  if(f == NULL)
    return -1;
  /*
   * No in-space bounds check and no check that n_bits holds the status
   * tail: part of the space mechanism lives in the space itself, so the
   * program guarantees both and the emulator trusts the program. A too
   * small n_bits is a program error, not an emulator case.
   */
  data = (size)(n_bits - read_status_bits());
  moved = 0;
  while(moved < data)
  {
    if(in_left == 0)
    {
      if(fread(&in_byte, 1, 1, f) != 1)
      {
        if(ferror(f) && in_error == 0)
        {
          if(errno)
            in_error = (unsigned char)errno;
          else
            in_error = (unsigned char)ERR_IO;
        }
        break;
      }
      in_left = CHAR_BIT;
    }
    if((size)in_left < (size)(data - moved))
      take = (size)in_left;
    else
      take = (size)(data - moved);
    bitcpy(take, &in_byte, (size_t)(CHAR_BIT - in_left), space, (size_t)(addr_bits + moved));
    in_left -= (int)take;
    moved = (size)(moved + take);
  }
  base = (size)(addr_bits + data);
  if(feof(f))
    space_set_bit((size_t)base, 1);
  else
    space_set_bit((size_t)base, 0);
  store_argument(ERR_BITS, op_errno(f, in_error), (size)(base + EOF_BITS));
  store_argument(nfield_bits, moved, (size)(base + EOF_BITS + ERR_BITS));
  last_input_bits = moved;
  return 0;
}

/*
 * output: half-interval [addr_bits, addr_bits + n_bits); data first,
 * then the status tail (errno, size). bits stay collected until a
 * byte is full. return 0 on a channel transfer, -1 when there is no
 * stream.
 */
int space_output(size n_bits, size addr_bits, FILE *f)
{
  size data, moved, base, take;
  if(f == NULL)
    return -1;
  /*
   * No in-space bounds check and no check that n_bits holds the status
   * tail: part of the space mechanism lives in the space itself, so the
   * program guarantees both and the emulator trusts the program. A too
   * small n_bits is a program error, not an emulator case.
   */
  data = (size)(n_bits - write_status_bits());
  moved = 0;
  while(moved < data)
  {
    if((size)(CHAR_BIT - out_used) < (size)(data - moved))
      take = (size)(CHAR_BIT - out_used);
    else
      take = (size)(data - moved);
    bitcpy(take, space, (size_t)(addr_bits + moved), &out_byte, (size_t)out_used);
    out_used += (int)take;
    moved = (size)(moved + take);
    if(out_used == CHAR_BIT)
    {
      unsigned char b = out_byte;
      out_used = 0;
      out_byte = 0;
      if(fwrite(&b, 1, 1, f) != 1)
      {
        if(ferror(f) && out_error == 0)
        {
          if(errno)
            out_error = (unsigned char)errno;
          else
            out_error = (unsigned char)ERR_IO;
        }
        break;
      }
    }
  }
  base = (size)(addr_bits + data);
  store_argument(ERR_BITS, op_errno(f, out_error), base);
  store_argument(nfield_bits, moved, (size)(base + ERR_BITS));
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

/* zero the count field of every instruction: the processor holds only NOPs */
static void stop_processor(void)
{
  size i;
  for(i = 0; (size_t)i < proc_n; ++i)
    store_argument(nfield_bits, 0, i * instruction_size + o[0]);
}

/*
 * execute the already fetched i-th instruction:
 * ordinary copy, input, or output. NOP (n = 0) does nothing.
 * The boot load is the first instruction of the first tick:
 * it reads N*instruction bits and adds the status record.
 */
void run_instruction(size i)
{
  size n, dst, src;
  if((size_t)i >= proc_n)
    return;
  n = processor[i][0];
  dst = processor[i][1];
  src = processor[i][2];
  if(boot_pending && i == 0)
  {
    boot_pending = 0;
    space_input((size)(n + read_status_bits()), dst, channel_in);
    if(last_input_bits < n)
      stop_processor();
    return;
  }
  if(n == 0)
    return;
  if((size_t)src == space_nbits && (size_t)dst == space_nbits)
    return;
  if((size_t)src == space_nbits)
  {
    space_input(n, dst, channel_in);
    return;
  }
  if((size_t)dst == space_nbits)
  {
    space_output(n, src, channel_out);
    return;
  }
  /*
   * No in-space bounds check: part of the space mechanism lives in the
   * space itself, so the program guarantees both half-intervals lie
   * inside the space. The emulator trusts the program.
   */
  bitcpy(n, space, src, space, dst);
}

/*
 * one tick: first read all instructions, then execute all copies.
 * return tick state: 1 if executed (a non-NOP was present), else 0.
 */
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
  size boot_n;
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
  processor[0][0] = boot_n;
  processor[0][1] = PROCESSOR_START;
  processor[0][2] = (size)space_nbits;
  store_instruction(0, boot_n, PROCESSOR_START, (size)space_nbits);
  boot_pending = 1;
}
