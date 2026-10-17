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
 * The position after the space (space_nbits) begins the channel area:
 * SYS at space_nbits, then the device channels pre-bound at start (see
 * sys_ctrl.h). A copy that touches a channel is served by the emulator,
 * not by the space buffer.
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
 * proc_n * instruction_size <= space_nbits
 * (the processor fits in the space),
 * proc_n * instruction_size <= 2^nfield_bits - 1
 * (the seed count must fit the count field).
 * Field widths follow the space size, so smaller spaces use narrower
 * fields. With default sizes the format is 16+15 = 47 bits and
 * proc_n <= 697. The smallest space is -s 27 -p 1 (one 14-bit
 * instruction). Larger -s/-p are allowed
 * as long as the two fit relations hold (plus available memory);
 * there is no SPACE_BITS_MAX and no separate -p cap.
 */

/* configuration (-s, -p, -t; definitions in globals.c) */
extern size_t space_nbits;        /* bits in the space */
extern size_t proc_n;             /* instructions in the processor */
extern uint64_t tick_limit;       /* -t (0 means no limit) */
extern const char *core_path;    /* --core FILE: raw space image to insert */
extern const char *dump_path;    /* --dump-space FILE: space after run (SPACE_DEV) */

/* format derived from the space size (computed in parse_config) */
extern size address_bits;         /* bits in "dst"/"src" */
extern size nfield_bits;          /* bits in "n" */
extern size instruction_size;     /* bits in "copy n, dst, src" */

/* read/write a numeric field: value bit k at bit address at + k */
size space_get(size at, size n);
void space_put(size n, size value, size at);

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
void init_space(void);           /* allocation, seed, channels */

#endif /* SPACE_H_ */
