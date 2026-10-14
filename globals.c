/*
 * ADDRESS SPACE TABLES
 *
 * Definitions for space.h (single definition file),
 * so there are no multiple definitions from the header.
 * Sizes come from flags; format widths are computed from -s;
 * memory is allocated in init_space.
 */
#include "space.h"

size_t space_nbits = SPACE_BITS_DFLT;
size_t proc_n = PROC_N_DFLT;
uint64_t tick_limit = 0;

size address_bits = 0;         /* bits in dst/src fields */
size nfield_bits = 0;          /* bits in the n field */
size instruction_size = 0;     /* bits in "copy n, dst, src" */

size (*processor)[3] = NULL;
unsigned char *space = NULL;

/* format of the instruction [n, dst, src] (widths from the space size) */
size o[N_FIELDS] = {0, 0, 0};
size s[N_FIELDS] = {0, 0, 0};
