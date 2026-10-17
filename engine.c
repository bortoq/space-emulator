/*
 * ADDRESS SPACE ENGINE
 *
 * Each tick has two phases: first read all
 * "copy n, dst, src" instructions from the processor positions,
 * then execute all copies over the bit space. A copy that touches a
 * channel position (>= space_nbits) is served by sys_tick, not by the
 * space buffer (see sys_ctrl.h). The seed (loading) is an ordinary copy
 * seeded into processor slot 0 by the host: it reads the image from the
 * stdin channel into the processor. Writes to processor positions
 * (including the seed) take effect from the next tick; position order
 * inside phases does not matter. Half-interval non-overlap and in-space
 * bounds are never checked: the program guarantees them, the emulator
 * relies on it.
 */
#include "space.h"
#include "sys_ctrl.h"
#include <errno.h>
#include <string.h>
#ifdef SIGPIPE
#include <signal.h>
#endif

FILE *channel_in = NULL;
FILE *channel_out = NULL;

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
size space_get(size at, size n)
{
  size v = 0;
  size k;
  if(n == 0)
    return 0;
  if(n > sizeof v * CHAR_BIT)
    n = (size)(sizeof v * CHAR_BIT);
  for(k = 0; k < n; ++k)
    if(space_bit(at + k))
      v = (size)(v | (size)1 << k);
  return v;
}

/* write instruction argument: value bit k at position at + k
 * fast path: one read-modify-write per touched byte */
void space_put(size n, size value, size at)
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
  processor[i][0] = space_get(base + o[0], s[0]);
  processor[i][1] = space_get(base + o[1], s[1]);
  processor[i][2] = space_get(base + o[2], s[2]);
}

/* write parsed instruction bit string to processor positions (init) */
static inline void store_instruction(size i, size n, size dst, size src)
{
  if((size_t)i >= proc_n)
    return;
  space_put(s[0], n, instruction_size * i + o[0]);
  space_put(s[1], dst, instruction_size * i + o[1]);
  space_put(s[2], src, instruction_size * i + o[2]);
}

/* channel streams default to stdio (already assigned ones stay, for tests) */
static inline void init_channels(void)
{
  if(channel_in == NULL)
    channel_in = stdin;
  if(channel_out == NULL)
    channel_out = stdout;
}

/*
 * execute the already fetched i-th instruction: an ordinary in-space
 * copy. NOP (n = 0) does nothing. A copy that touches the channel area
 * (position >= space_nbits) is served by sys_tick, not here; if that
 * area is not a live channel the copy is skipped (those positions do not
 * belong to the space buffer).
 */
void run_instruction(size i)
{
  size n, dst, src;
  if((size_t)i >= proc_n)
    return;
  n = processor[i][0];
  dst = processor[i][1];
  src = processor[i][2];
  if(n == 0)
    return;
  if(dst >= (size)space_nbits || src >= (size)space_nbits)
    return;
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
  sys_tick();
  for(i = 0; i < proc_n; ++i)
    if(processor[i][0])
      run_instruction((size)i);
  return 1;
}

/* allocate the space and seed slot 0 with an ordinary load copy */
void init_space(void)
{
  size boot_n, stdin_pos;
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
  sys_init();
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
  /*
   * --core FILE: the host insert primitive (roadmap 8.2). The file is a
   * raw space image (MSB-first bytes, the same layout the space buffer
   * uses); it replaces the seed, so the first tick fetches the entry
   * layer straight from position 0. A short file is zero padded by the
   * initial memset. Without --core the seed path below is used.
   */
  if(core_path != NULL)
  {
    FILE *f = fopen(core_path, "rb");
    if(f == NULL)
    {
      fprintf(stderr, "space: cannot open core %s\n", core_path);
      exit(EXIT_USAGE);
    }
    if(fread(space, 1, nbytes, f) < nbytes && ferror(f))
    {
      fprintf(stderr, "space: cannot read core %s\n", core_path);
      fclose(f);
      exit(EXIT_USAGE);
    }
    clearerr(f);
    if(ferror(f))
    {
      fprintf(stderr, "space: cannot read core %s\n", core_path);
      fclose(f);
      exit(EXIT_USAGE);
    }
    fclose(f);
    return;
  }
  /*
   * Seed slot 0 with an ordinary copy that reads the image from the
   * stdin channel into the processor. The host seeds the slot (before
   * the first tick there is no program); the instruction itself has no
   * special meaning. safe: parse_config guarantees
   * proc_n * instruction_size <= space_nbits and that it fits field n.
   */
  boot_n = (size)((size_t)proc_n * (size_t)instruction_size);
  stdin_pos = sys_device_pos(0);
  processor[0][0] = boot_n;
  processor[0][1] = PROCESSOR_START;
  processor[0][2] = stdin_pos;
  store_instruction(0, boot_n, PROCESSOR_START, stdin_pos);
}
