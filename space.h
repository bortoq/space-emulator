/*
 * ADDRESS SPACE DEFINITIONS (fixed variant)
 *
 * copyright 2024-10-31, dmitri bortoq, mit license
 *
 * Address space of positions: simultaneously available bit positions.
 * The single action: copying bit half-intervals
 *   copy n, dst, src  (n bits from [src, src+n) to [dst, dst+n)).
 * The emulator does not check half-interval non-overlap, it relies on it;
 * the program running in the space guarantees it.
 *
 * Sizes come from flags (-s space bits, -p processor instructions).
 * Instruction field widths follow the space size: address bits =
 * ceil(log2 space bits), count field one bit less, instruction =
 * 2*address bits + count bits (computed in parse_config).
 * The single channel is the last position of the space. Channel access
 * transfers any number of bits at any bit position; stream bits
 * continue across accesses.
 */
#ifndef SPACE_H_
#define SPACE_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* types */
typedef uint16_t  size;  /* large enough for a field value */

/* defaults and fixed constants (format widths follow the space size) */
enum {
  SPACE_BITS_DFLT = 0xFFF0, /* -s default (bits) */
  PROC_N_DFLT = 64,         /* -p default (instructions) */
  SPACE_BITS_MAX = 1 << (8 * (int)sizeof(size)), /* -s limit (addresses are fields) */
  PROCESSOR_START = 0,      /* bit index of the first instruction */
  N_FIELDS = 3,             /* fields in an instruction: n, dst, src */
  STATE_BITS = 8,           /* query: leading state bits, then the last
                               transfer count in nfield_bits (low byte first) */
  STATE_END = 0x01,         /* read side: end of input seen */
  STATE_IN_FAIL = 0x02,     /* read side: input failure */
  STATE_OUT_FAIL = 0x01,    /* write side: output failure */
  EXIT_USAGE = 2,           /* bad flags */
  EXIT_MEMORY = 1           /* allocation failure */
};

/* limits (checked at startup, derived in parse_config):
   8 <= space_nbits <= SPACE_BITS_MAX,
   proc_n >= 1,
   proc_n * instruction_size <= space_nbits - (STATE_BITS + nfield_bits)
   (processor plus the query record: state and count),
   proc_n * instruction_size <= 2^nfield_bits - 1
   (the boot instruction count must fit the count field).
   Field widths follow the space size, so smaller spaces use narrower
   fields: at -s 22 the format is 5+5+4 = 14 bits and the space holds
   exactly 1 instruction. With default sizes the format is 16+15 = 47
   bits and proc_n <= 697; a `size` index is enough everywhere. */

/* configuration (-s, -p, -t; definitions in globals.c) */
extern size_t space_nbits;        /* bits in the space */
extern size_t proc_n;             /* instructions in the processor */
extern uint64_t tick_limit;       /* -t (0 means no limit) */

/* format derived from the space size (computed in parse_config) */
extern size address_bits;         /* bits in "dst"/"src" */
extern size nfield_bits;          /* bits in "n" */
extern size instruction_size;     /* bits in "copy n, dst, src" */

/* copy bits from one array to another (n, src, s, dst, d).
   Half-intervals [s, s + n) and [d, d + n) do not overlap
   (caller guarantee). */
void bitcpy(size_t n, const void *src, size_t s, void *dst, size_t d);

/* GLOBALS (definitions in globals.c, engine in engine.c) */
extern size (*processor)[3];               /* parsed copy instructions */
extern unsigned char *space;               /* data bits of the bit space */

/* format of the instruction [n, dst, src] (widths from the space size) */
extern size o[N_FIELDS];
extern size s[N_FIELDS];

/* ENGINE */
extern FILE *channel_in;         /* input stream (stdin by default) */
extern FILE *channel_out;        /* output stream (stdout by default) */
void run_instruction(size i);    /* execute the already read i-th instruction */
int computation(void);           /* tick: state 1 if executed, 0 if all NOP */
void parse_config(int argc, char **argv); /* flags -s/-p/-t, limit checks */
void init_space(void);           /* allocation, boot instruction, channels */
int space_input(size n_bits, size addr_bits, FILE *f);  /* f -> space, 0 ok */
int space_output(size n_bits, size addr_bits, FILE *f); /* space -> f, 0 ok */
void channel_flush(void);        /* write collected output bits at program end */

#endif /* SPACE_H_ */
