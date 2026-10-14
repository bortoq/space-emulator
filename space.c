/*
 * EMULATION OF PARALLEL ADDRESS SPACE (fixed variant)
 *
 * copyright 2022-12-17, dmitri bortoq, mit license
 *
 * Каждый такт последовательно исполняет PROCESSOR_N инструкций
 * "copy n, dst, src" над битовым пространством space.
 * Согласованность (непересекающиеся интервалы в одном такте) —
 * ответственность программы.
 */
#include "space.h"

/* return bit string of instruction argument as a number */
static inline size get_argument(size src, size n)
{
  size v = 0;
  if(n > sizeof v * CHAR_BIT)
    n = sizeof v * CHAR_BIT;
  bitcpy(n, space, src, &v, 0);
  return v;
}

/* read "i-th instruction" bit string from bit space to the processor mechanism */
static inline void fetch_instruction(size i)
{
  size a;
  if(i >= PROCESSOR_N)
    return;
  for(a = 0; a < N_FIELDS; ++a)
    processor[i][a] = get_argument(PROCESSOR_START + i * INSTRUCTION_SIZE + o[a], s[a]);
}

/* save parsed instruction to the bit image (init) */
static inline void store_instruction(size i, size n, size dst, size src)
{
  if(i >= PROCESSOR_N)
    return;
  bitcpy(s[0], &n, 0, space, INSTRUCTION_SIZE * i + o[0]);
  bitcpy(s[1], &dst, 0, space, INSTRUCTION_SIZE * i + o[1]);
  bitcpy(s[2], &src, 0, space, INSTRUCTION_SIZE * i + o[2]);
}

/* check instruction bounds before copy */
static inline int instruction_ok(size n, size dst, size src)
{
  if((size_t)n > SPACE_SIZE)
    return 0;
  if((size_t)dst + (size_t)n > SPACE_SIZE)
    return 0;
  if((size_t)src + (size_t)n > SPACE_SIZE)
    return 0;
  return 1;
}

/* load and run copy instructions over bit space */
static void computation()
{
  size i;
  for(i = 0; i < PROCESSOR_N; ++i)
  {
    fetch_instruction(i);
    if(!instruction_ok(processor[i][0], processor[i][1], processor[i][2]))
      continue;
    if(processor[i][0] == 0)
      continue;
    bitcpy(processor[i][0], space, processor[i][2], space, processor[i][1]);
  }
}

/* access file f and copy byte aligned bit string. return 0 if ok */
static inline int channel(size_t (*copy)(), FILE *f, size n_bits, size addr_bits)
{
  size_t n_bytes;
  size_t done;
  if(copy == NULL || f == NULL)
    return -1;
  if(addr_bits % CHAR_BIT != 0)
    return -1;
  if(n_bits % CHAR_BIT != 0)
    return -1;
  n_bytes = (size_t)n_bits / CHAR_BIT;
  if(n_bytes == 0)
    return 0;
  if((size_t)addr_bits / CHAR_BIT + n_bytes > sizeof space)
    return -1;
  done = copy(&space[addr_bits / CHAR_BIT], 1, n_bytes, f);
  if(done == n_bytes)
    return 0;
  return -1;
}

/* copy stdin to stdout through src and dst channels. return 0 if ok */
static int channels()
{
  int code;
  fetch_instruction(CH_IN);
  fetch_instruction(CH_OUT);
  code = channel(fread, stdin, processor[CH_IN][0], processor[CH_IN][1]);
  if(code != 0 && feof(stdin))
    return -1;
  if(code != 0)
    return -1;
  code = channel(fwrite, stdout, processor[CH_OUT][0], processor[CH_OUT][1]);
  return code;
}

/* build bit image of the core and the first processor table */
static void init_space()
{
  size i;
  memset(space, 0, sizeof space);
  memset(processor, 0, sizeof processor);
  for(i = 0; i < 3; ++i)
  {
    processor[i][0] = core[i * 3];
    processor[i][1] = core[i * 3 + 1];
    processor[i][2] = core[i * 3 + 2];
    store_instruction(i, core[i * 3], core[i * 3 + 1], core[i * 3 + 2]);
  }
}

int main()
{
  /* prepare address space */
  uint64_t time = LIFE;

  init_space();

  /* emulate computation */
  while(time-- && channels() == 0)
    computation();

  fflush(stdout);
  return 0;
}
