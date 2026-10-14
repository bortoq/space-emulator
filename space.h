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
 * Sizes come from flags (-s space bits, -p processor instructions),
 * below are defaults and the format (the format never changes).
 * The single outward channel is the last position of the space.
 */
#ifndef SPACE_H_
#define SPACE_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* defaults and format */
enum {
  SPACE_BITS_DFLT = 0xFFF0,       /* -s default (bits) */
  PROC_N_DFLT = 64,               /* -p default (instructions) */
  N_SIZE = 16,                    /* bits in the "n" field of an instruction */
  ADDRESS_SIZE = 16,              /* bits in the "dst" field ("src" has the same) */
  INSTRUCTION_SIZE = 16 + 2 * 16, /* bits in "copy n, dst, src" (> 10) */
  PROCESSOR_START = 0,            /* bit index of the first instruction */
  N_FIELDS = 3                    /* fields in an instruction: n, dst, src */
};

/* limits (checked at startup):
   1 <= space_nbits <= 2^ADDRESS_SIZE (fields address positions),
   proc_n >= 1, space_nbits >= proc_n * INSTRUCTION_SIZE + 8
   (processor instructions plus the channel state byte outside them).
   Division without overflow: proc_n <= (space_nbits - 8) / INSTRUCTION_SIZE.
   No size multiple is required besides what the content implies: arrays
   are allocated rounding up, extra bits of the last byte
   are never addressed. With current widths proc_n <= 1365 < 65535:
   a `size` index is enough everywhere. */

/* types */
typedef uint16_t  size;           /* large enough for instruction argument */
_Static_assert(N_SIZE <= 16 && ADDRESS_SIZE <= 16, "fields fit size");

/* configuration (-s, -p, -t; definitions in globals.c) */
extern size_t space_nbits;        /* bits in the space */
extern size_t proc_n;             /* instructions in the processor */
extern uint64_t tick_limit;       /* -t (0 means no limit) */

/* copy bits from one array to another (n, src, s, dst, d).
   Half-intervals [s, s + n) and [d, d + n) do not overlap
   (caller guarantee). */
void bitcpy(size_t n, const void *src, size_t s, void *dst, size_t d);

/* GLOBALS (definitions in globals.c, engine in engine.c) */
extern size (*processor)[3];               /* parsed copy instructions */
extern unsigned char *space;               /* data bits of the bit space */

/* format of the instruction [n, dst, src] */
extern const size o[N_FIELDS];
extern const size s[N_FIELDS];

/* ENGINE */
extern FILE *channel_in;         /* input stream (stdin by default) */
extern FILE *channel_out;        /* output stream (stdout by default) */
void run_instruction(size i);    /* execute the already read i-th instruction */
int computation(void);           /* tick: state 1 if executed, 0 if all NOP */
void parse_config(int argc, char **argv); /* flags -s/-p/-t, limit checks */
void init_space(void);           /* allocation, boot instruction, channels */
int space_input(size n_bits, size addr_bits, FILE *f);  /* f -> space, 0 ok */
int space_output(size n_bits, size addr_bits, FILE *f); /* space -> f, 0 ok */

#endif /* SPACE_H_ */
