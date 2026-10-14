/*
 * ADDRESS SPACE ENGINE (single channel version)
 *
 * Каждый такт делится на две фазы: сначала прочитываются все
 * PROCESSOR_N инструкций "copy n, dst, src" из специальных позиций,
 * затем исполняются все копирования над битовым пространством space.
 * Инструкция с первой позицией полуинтервала источника LAST —
 * ввод (stdin -> пространство), с первой позицией полуинтервала
 * приёмника LAST — вывод (пространство -> stdout); остальное —
 * обычные внутренние копирования. Записи в специальные позиции
 * (включая загрузку) действуют со следующего такта; порядок позиций
 * внутри фаз безразличен. Непересекаемость полуинтервалов
 * не проверяется: её обеспечивает программа, эмулятор на неё полагается.
 */
#include "space.h"

int halted = 0;
FILE *channel_in = NULL;
FILE *channel_out = NULL;

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

/* write parsed instruction bit string to special positions (init) */
static inline void store_instruction(size i, size n, size dst, size src)
{
  if(i >= PROCESSOR_N)
    return;
  bitcpy(s[0], &n, 0, space, INSTRUCTION_SIZE * i + o[0]);
  bitcpy(s[1], &dst, 0, space, INSTRUCTION_SIZE * i + o[1]);
  bitcpy(s[2], &src, 0, space, INSTRUCTION_SIZE * i + o[2]);
}

/* check instruction half-intervals before copy */
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

/* channel streams default to stdio (уже назначенные не трогаем — для тестов) */
static inline void init_channels(void)
{
  if(channel_in == NULL)
    channel_in = stdin;
  if(channel_out == NULL)
    channel_out = stdout;
}

/* copy n_bits from f to the half-interval [addr_bits, addr_bits + n_bits).
   return 0 if ok, -1 if skipped or short (short sets halted) */
int space_input(size n_bits, size addr_bits, FILE *f)
{
  size_t n_bytes;
  size_t done;
  if(f == NULL)
    return -1;
  if(n_bits == 0)
    return 0;
  if(n_bits % CHAR_BIT != 0)
    return -1;
  if(addr_bits % CHAR_BIT != 0)
    return -1;
  n_bytes = (size_t)n_bits / CHAR_BIT;
  if((size_t)addr_bits + (size_t)n_bits > SPACE_SIZE)
    return -1;
  done = fread(&space[addr_bits / CHAR_BIT], 1, n_bytes, f);
  if(done != n_bytes)
  {
    halted = 1;
    return -1;
  }
  return 0;
}

/* copy n_bits from the half-interval [addr_bits, addr_bits + n_bits) to f.
   return 0 if ok, -1 if skipped or short (short sets halted) */
int space_output(size n_bits, size addr_bits, FILE *f)
{
  size_t n_bytes;
  size_t done;
  if(f == NULL)
    return -1;
  if(n_bits == 0)
    return 0;
  if(n_bits % CHAR_BIT != 0)
    return -1;
  if(addr_bits % CHAR_BIT != 0)
    return -1;
  n_bytes = (size_t)n_bits / CHAR_BIT;
  if((size_t)addr_bits + (size_t)n_bits > SPACE_SIZE)
    return -1;
  done = fwrite(&space[addr_bits / CHAR_BIT], 1, n_bytes, f);
  if(done != n_bytes)
  {
    halted = 1;
    return -1;
  }
  return 0;
}

/* execute the already fetched i-th instruction:
   ordinary copy or single channel transfer */
void run_instruction(size i)
{
  size n, dst, src;
  if(i >= PROCESSOR_N)
    return;
  n = processor[i][0];
  dst = processor[i][1];
  src = processor[i][2];
  if(n == 0)
    return;
  if(src == LAST && dst == LAST)
    return;
  if(src == LAST)
  {
    space_input(n, dst, channel_in);
    return;
  }
  if(dst == LAST)
  {
    space_output(n, src, channel_out);
    return;
  }
  if(!instruction_ok(n, dst, src))
    return;
  bitcpy(n, space, src, space, dst);
}

/* one tick: first read all instructions, then execute all copies.
   такт исполняется до конца и при halted; канальные обращения идут
   в порядке инструкций, так как stdin/stdout последовательны */
void computation(void)
{
  size i;
  for(i = 0; i < PROCESSOR_N; ++i)
    fetch_instruction(i);
  for(i = 0; i < PROCESSOR_N; ++i)
    run_instruction(i);
}

/* write the single boot instruction bit string */
void init_space(void)
{
  init_channels();
  memset(space, 0, sizeof space);
  memset(processor, 0, sizeof processor);
  processor[0][0] = core[0];
  processor[0][1] = core[1];
  processor[0][2] = core[2];
  store_instruction(0, core[0], core[1], core[2]);
}
