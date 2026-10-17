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
#include <string.h>
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
  if(n == 0)
    return 0;
  if(n > sizeof v * CHAR_BIT)
    n = (size)(sizeof v * CHAR_BIT);
  for(k = 0; k < n; ++k)
    if(space_bit(src + k))
      v = (size)(v | (size)1 << k);
  return v;
}

/* write instruction argument: value bit k at position at + k
 * fast path: one read-modify-write per touched byte */
static inline void store_argument(size n, size value, size at)
{
  unsigned char *q;
  unsigned int off;
  size k = 0;
  if(n == 0)
    return;
  q = space + at / CHAR_BIT;
  off = (unsigned int)(at % CHAR_BIT);
  while(n)
  {
    unsigned int take = (unsigned int)CHAR_BIT - off;
    unsigned int j;
    unsigned int mask = 0;
    unsigned int bits = 0;
    if(take > n)
      take = (unsigned int)n;
    for(j = 0; j < take; ++j)
    {
      unsigned int m = 0x80u >> (off + j);
      mask |= m;
      if((value >> k) & 1)
        bits |= m;
      ++k;
    }
    *q = (unsigned char)((*q & ~mask) | bits);
    ++q;
    off = 0;
    n -= take;
  }
}

/* bit reversal of one byte: space bytes are MSB-first, fields LSB-first */
static const unsigned char rev8[256] =
{
  0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
  0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
  0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
  0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
  0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
  0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
  0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
  0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
  0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
  0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
  0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
  0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
  0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
  0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
  0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
  0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF
};

/* decode the whole instruction at bit offset base at once.
 * Only when the shifted window fits 64 bits (instruction_size <= 57):
 * reverse each covered byte, assemble the LSB-first word, split fields.
 * Same values as three get_argument calls; ~7 table lookups per
 * instruction instead of ~47 per-bit space_bit calls.
 */
static inline void fetch_instruction_rev(size i, size base)
{
  const unsigned char *q;
  unsigned int r;
  size w = 0;
  size j, nb;
  size mn, ma;
  if((size_t)i >= proc_n)
    return;
  q = space + base / CHAR_BIT;
  r = (unsigned int)(base % CHAR_BIT);
  nb = (size)((r + (unsigned int)instruction_size + CHAR_BIT - 1) / CHAR_BIT);
  /* fast path: an unaligned 64-bit load covers the whole instruction;
   * per-byte bit reverse done with a SWAR permutation. Bits above
   * instruction_size are ignored by the field extraction below.
   * Falls back to the byte loop when 8 bytes would cross the end. */
  if((size_t)(q - space) + 8 <= space_nbits / CHAR_BIT + (space_nbits % CHAR_BIT != 0))
  {
    uint64_t x;
    memcpy(&x, q, 8);
    x = ((x & 0xAAAAAAAAAAAAAAAAull) >> 1) | ((x & 0x5555555555555555ull) << 1);
    x = ((x & 0xCCCCCCCCCCCCCCCCull) >> 2) | ((x & 0x3333333333333333ull) << 2);
    x = ((x & 0xF0F0F0F0F0F0F0F0ull) >> 4) | ((x & 0x0F0F0F0F0F0F0F0Full) << 4);
    w = (size)((size)x >> r);
  }
  else
  {
    for(j = 0; j < nb; ++j)
      w = (size)(w | (size)rev8[q[j]] << (8 * j));
    w = (size)(w >> r);
  }
  mn = (size)(((size)1 << nfield_bits) - 1);
  ma = (size)(((size)1 << address_bits) - 1);
  processor[i][0] = (size)(w & mn);
  if(processor[i][0] == 0)
    return;                         /* NOP: dst/src are never used */
  w = (size)(w >> nfield_bits);
  processor[i][1] = (size)(w & ma);
  w = (size)(w >> address_bits);
  processor[i][2] = (size)(w & ma);
}

/* wide-format rev-decode (58 <= instruction_size <= 120) in a 128-bit
 * window; the 64-bit rev handles instruction_size <= 57, per-bit above.
 * Same values as three get_argument calls. */
static inline void fetch_instruction_rev128(size i, size base)
{
  const unsigned char *q;
  unsigned int r;
  unsigned __int128 w = 0;
  size j, nb;
  size mn, ma;
  if((size_t)i >= proc_n)
    return;
  q = space + base / CHAR_BIT;
  r = (unsigned int)(base % CHAR_BIT);
  nb = (size)((r + (unsigned int)instruction_size + CHAR_BIT - 1) / CHAR_BIT);
  /* same SWAR byte-reverse as the narrow decoder, in a 128-bit window
   * (instruction_size <= 120 fits 16 bytes); byte loop fallback at the end */
  if((size_t)(q - space) + 16 <= space_nbits / CHAR_BIT + (space_nbits % CHAR_BIT != 0))
  {
    unsigned __int128 x = 0;
    unsigned __int128 d = ((unsigned __int128)1 << 64) | 1;
    memcpy(&x, q, 16);
    x = ((x & ((unsigned __int128)0xAAAAAAAAAAAAAAAAull * d)) >> 1)
      | ((x & ((unsigned __int128)0x5555555555555555ull * d)) << 1);
    x = ((x & ((unsigned __int128)0xCCCCCCCCCCCCCCCCull * d)) >> 2)
      | ((x & ((unsigned __int128)0x3333333333333333ull * d)) << 2);
    x = ((x & ((unsigned __int128)0xF0F0F0F0F0F0F0F0ull * d)) >> 4)
      | ((x & ((unsigned __int128)0x0F0F0F0F0F0F0F0Full * d)) << 4);
    w = x >> r;
  }
  else
  {
    for(j = 0; j < nb; ++j)
      w |= (unsigned __int128)rev8[q[j]] << (8 * j);
    w >>= r;
  }
  mn = (size)(((size)1 << nfield_bits) - 1);
  ma = (size)(((size)1 << address_bits) - 1);
  processor[i][0] = (size)((size)w & mn);
  w >>= nfield_bits;
  processor[i][1] = (size)((size)w & ma);
  w >>= address_bits;
  processor[i][2] = (size)((size)w & ma);
}

/* read the instruction at bit offset base into processor[i] (base hoisted) */
static inline void fetch_instruction_at(size i, size base)
{
  if((size_t)i >= proc_n)
    return;
  processor[i][0] = get_argument(base + o[0], s[0]);
  processor[i][1] = get_argument(base + o[1], s[1]);
  processor[i][2] = get_argument(base + o[2], s[2]);
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
/* bulk input staging: one fread per chunk instead of one per byte */
#define INPUT_CHUNK_BITS ((size)32768)
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
  /* drain the few leftover stream bits first, if any */
  if(in_left != 0 && moved < data)
  {
    if((size)in_left < (size)(data - moved))
      take = (size)in_left;
    else
      take = (size)(data - moved);
    bitcpy(take, &in_byte, (size_t)(CHAR_BIT - in_left), space, (size_t)(addr_bits + moved));
    in_left -= (int)take;
    moved = (size)(moved + take);
  }
  while(moved < data)
  {
    /* in_left == 0 on entry: pull a whole chunk, consume it at once */
    unsigned char buf[(size_t)INPUT_CHUNK_BITS / CHAR_BIT];
    size rest = (size)(data - moved);
    size want = (size)((rest + CHAR_BIT - 1) / CHAR_BIT);
    size bits, excess;
    size_t got;
    if(want > INPUT_CHUNK_BITS / CHAR_BIT)
      want = INPUT_CHUNK_BITS / CHAR_BIT;
    got = fread(buf, 1, (size_t)want, f);
    if(got == 0)
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
    bits = (size)got * CHAR_BIT;
    if(bits > rest)
      bits = rest;
    /* one wide transfer per chunk: bitcpy picks its own fast path */
    bitcpy(bits, buf, 0, space, (size_t)(addr_bits + moved));
    moved = (size)(moved + bits);
    /* excess (< 8) stream bits stay buffered for the next transfer */
    excess = (size)got * CHAR_BIT - bits;
    if(excess)
    {
      in_byte = buf[bits / CHAR_BIT];
      in_left = (int)excess;
    }
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
/* bulk output staging: gather whole bytes with one bitcpy, one fwrite */
#define OUTPUT_CHUNK_BITS ((size)32768)
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
  /* complete a partially filled output byte first, if any */
  {
  int io_failed = 0;
  if(out_used != 0 && moved < data)
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
        io_failed = 1;
      }
    }
  }
  /* bulk: whole bytes gathered by one bitcpy and written by one fwrite */
  while(!io_failed && moved < data)
  {
    unsigned char buf[(size_t)OUTPUT_CHUNK_BITS / CHAR_BIT];
    size rest = (size)(data - moved);
    size chunk = (size)(rest - rest % CHAR_BIT);
    size_t nbytes, got;
    if(chunk == 0)
      break;
    if(chunk > OUTPUT_CHUNK_BITS)
      chunk = OUTPUT_CHUNK_BITS;
    nbytes = (size_t)(chunk / CHAR_BIT);
    bitcpy(chunk, space, (size_t)(addr_bits + moved), buf, 0);
    got = fwrite(buf, 1, nbytes, f);
    if(got < nbytes)
    {
      /* count the failed byte like the byte path (gathered bits count) */
      moved = (size)(moved + ((size)got + 1) * CHAR_BIT);
      if(moved > data)
        moved = data;
      if(ferror(f) && out_error == 0)
      {
        if(errno)
          out_error = (unsigned char)errno;
        else
          out_error = (unsigned char)ERR_IO;
      }
      io_failed = 1;
      break;
    }
    moved = (size)(moved + chunk);
  }
  /* tail (< 8 bits) stays buffered exactly like the byte path leaves it */
  if(!io_failed && moved < data)
  {
    take = (size)(data - moved);
    bitcpy(take, space, (size_t)(addr_bits + moved), &out_byte, (size_t)out_used);
    out_used += (int)take;
    moved = (size)(moved + take);
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
  size base;
  size isz = instruction_size;
  size w = nfield_bits;
  size_t i;
  if(proc_n == 0 || w == 0 || isz == 0)
    return;
  base = (size)(PROCESSOR_START + o[0]);
  for(i = 0; i < proc_n; ++i)
  {
    store_argument(w, 0, base);
    base = (size)(base + isz);
  }
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
  size base;
  size isz = instruction_size;
  size_t i;
  int state = 0;
  if(proc_n == 0 || isz == 0)
    return 0;
  /* fetch: one running offset, no multiply per field; the rev decoder
   * handles the common narrow formats, the per-bit path the wide ones */
  base = PROCESSOR_START;
  if(isz <= 120 && isz > 57)
  {
    for(i = 0; i < proc_n; ++i)
    {
      fetch_instruction_rev128((size)i, base);
      if(processor[i][0])
        state = 1;
      base = (size)(base + isz);
    }
  }
  else if(isz <= 57)
  {
    for(i = 0; i < proc_n; ++i)
    {
      fetch_instruction_rev((size)i, base);
      if(processor[i][0])
        state = 1;
      base = (size)(base + isz);
    }
  }
  else
  {
    for(i = 0; i < proc_n; ++i)
    {
      fetch_instruction_at((size)i, base);
      if(processor[i][0])
        state = 1;
      base = (size)(base + isz);
    }
  }
  if(!state)
    return 0;
  for(i = 0; i < proc_n; ++i)
    if(processor[i][0])
      run_instruction((size)i);
  return 1;
}

/* allocate space, write the single boot instruction bit string */
void init_space(void)
{
  size boot_n;
  size_t nbytes;
  size_t nproc;
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
  if(space_nbits == 0 || proc_n == 0 || instruction_size == 0)
  {
    fprintf(stderr, "space: bad configuration\n");
    exit(EXIT_USAGE);
  }
  init_channels();
  /* overflow-safe: (space_nbits + CHAR_BIT - 1) / CHAR_BIT without addition */
  nbytes = space_nbits / (size_t)CHAR_BIT + (space_nbits % (size_t)CHAR_BIT != 0);
  if(proc_n > SIZE_MAX / sizeof *processor)
  {
    fprintf(stderr, "space: out of memory\n");
    exit(EXIT_MEMORY);
  }
  nproc = proc_n * sizeof *processor;
  space = malloc(nbytes ? nbytes : 1);
  processor = malloc(nproc ? nproc : 1);
  if(space == NULL || processor == NULL)
  {
    fprintf(stderr, "space: out of memory\n");
    exit(EXIT_MEMORY);
  }
  memset(space, 0, nbytes);
  memset(processor, 0, nproc);
  /* safe: parse_config guarantees proc_n * instruction_size <= space_nbits */
  boot_n = (size)((size_t)proc_n * (size_t)instruction_size);
  processor[0][0] = boot_n;
  processor[0][1] = PROCESSOR_START;
  processor[0][2] = (size)space_nbits;
  store_instruction(0, boot_n, PROCESSOR_START, (size)space_nbits);
  boot_pending = 1;
}
