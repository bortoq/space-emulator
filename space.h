/*
 * ADDRESS SPACE DEFINITIONS (fixed variant)
 *
 * copyright 2024-10-31, dmitri bortoq, mit license
 *
 * Address space of positions: simultaneously available bit positions.
 * The single action: copying bit half-intervals
 * copy n, dst, src  (n bits from [src, src+n) to [dst, dst+n)).
 * The emulator does not check half-interval non-overlap, it relies on it;
 * the program running in the space guarantees it.
 *
 * Sizes come from flags (-s space bits, -p processor instructions).
 * Instruction field widths follow the space size: address bits =
 * ceil(log2(space bits + 1)), count field one bit less, instruction =
 * 2*address bits + count bits (computed in parse_config).
 * The single channel, DATA, is the position just after the space
 * (space_nbits). Channel access transfers any number of bits at any
 * bit position; stream bits continue across accesses. A transfer
 * carries its own status in the tail of its half-interval (see engine.c).
 */
#ifndef SPACE_H_
#define SPACE_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* types */
typedef uint64_t  size;  /* field value / position: no artificial cap */

/* defaults and fixed constants (format widths follow the space size) */
enum {
  SPACE_BITS_DFLT = 0xFFF0, /* -s default (bits) */
  PROC_N_DFLT = 64,         /* -p default (instructions) */
  PROCESSOR_START = 0,      /* bit index of the first instruction */
  N_FIELDS = 3,             /* fields in an instruction: n, dst, src */
  ERR_BITS = 8,             /* status: 8-bit error number (errno) */
  EOF_BITS = 1,             /* status: read end-of-input bit */
  ERR_IO = 1,               /* error number when the stream has no errno */
  EXIT_USAGE = 2,           /* bad flags */
  EXIT_MEMORY = 1           /* allocation failure */
};

/*
 * limits (checked at startup, derived in parse_config; no artificial
 * caps on -s/-p besides memory):
 * proc_n >= 1,
 * proc_n * instruction_size + (EOF_BITS + ERR_BITS + nfield_bits)
 * <= space_nbits
 * (processor plus the boot read status record),
 * proc_n * instruction_size <= 2^nfield_bits - 1
 * (the boot instruction count must fit the count field).
 * Field widths follow the space size, so smaller spaces use narrower
 * fields. With default sizes the format is 16+15 = 47 bits and
 * proc_n <= 697. The smallest space is -s 27 -p 1 (one 14-bit
 * instruction plus the boot status record). Larger -s/-p are allowed
 * as long as the two fit relations hold (plus available memory);
 * there is no SPACE_BITS_MAX and no separate -p cap.
 */

/* configuration (-s, -p, -t; definitions in globals.c) */
extern size_t space_nbits;        /* bits in the space */
extern size_t proc_n;             /* instructions in the processor */
extern uint64_t tick_limit;       /* -t (0 means no limit) */

/* format derived from the space size (computed in parse_config) */
extern size address_bits;         /* bits in "dst"/"src" */
extern size nfield_bits;          /* bits in "n" */
extern size instruction_size;     /* bits in "copy n, dst, src" */

/*
 * copy bits from one array to another (n, src, s, dst, d).
 * Half-intervals [s, s + n) and [d, d + n) do not overlap
 * (caller guarantee).
 */
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
