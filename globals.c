/*
 * ADDRESS SPACE TABLES
 *
 * Definitions for space.h (single definition file),
 * so there are no multiple definitions from the header.
 * Sizes come from flags; memory is allocated in init_space.
 */
#include "space.h"

size_t space_nbits = SPACE_BITS_DFLT;
size_t proc_n = PROC_N_DFLT;
uint64_t tick_limit = 0;

size (*processor)[3] = NULL;
unsigned char *space = NULL;

/* format of the instruction "copy n, dst, src" (format never changes) */
const size o[N_FIELDS] = {0, N_SIZE, N_SIZE + ADDRESS_SIZE};
const size s[N_FIELDS] = {N_SIZE, ADDRESS_SIZE, ADDRESS_SIZE};
