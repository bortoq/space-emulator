/* unit-тесты единственного канала LAST: ввод, вывод, nop, внахлёст, halt.
   Образ инструкций пишем через write_image (публичный bitcpy) —
   заодно проверяется fetch в run_instruction. */
#include <stdio.h>
#include <string.h>
#include "../space.h"

static int fails = 0;

#define CHECK(cond, msg)                                              \
  do {                                                              \
    if(!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); ++fails; } \
    else { printf("ok: %s\n", msg); } } while(0)

/* tmpfile с содержимым s[0..n), позиция в начале (n == 0: пустой) */
static FILE *make_input(const char *s, size_t n)
{
  FILE *f = tmpfile();
  if(f == NULL)
    return NULL;
  if(n && fwrite(s, 1, n, f) != n)
    return NULL;
  rewind(f);
  return f;
}

/* всё содержимое f в buf (до cap), возврат числа байт */
static size_t read_all(FILE *f, char *buf, size_t cap)
{
  size_t n = 0;
  size_t k;
  rewind(f);
  while(n < cap && (k = fread(buf + n, 1, cap - n, f)) > 0)
    n += k;
  return n;
}

/* образ i-й инструкции (n, dst, src) в space */
static void write_image(size_t i, size n, size dst, size src)
{
  bitcpy(s[0], &n, 0, space, INSTRUCTION_SIZE * i + o[0]);
  bitcpy(s[1], &dst, 0, space, INSTRUCTION_SIZE * i + o[1]);
  bitcpy(s[2], &src, 0, space, INSTRUCTION_SIZE * i + o[2]);
}

int main()
{
  FILE *f;
  char buf[64];
  size_t n;
  long pos, pos2;

  /* ядро ставит каналы на stdio */
  channel_in = NULL;
  channel_out = NULL;
  init_space();
  CHECK(channel_in == stdin && channel_out == stdout, "core wires stdio");
  CHECK(processor[0][0] == 8 && processor[0][1] == 0x2000 && processor[0][2] == LAST,
    "core[0] is channel in");
  CHECK(processor[1][0] == 8 && processor[1][1] == LAST && processor[1][2] == 0x2010,
    "core[1] is channel out");

  /* назначенные потоки init_space не трогает */
  f = make_input("keeper", 6);
  CHECK(f != NULL, "tmpfile keeper");
  channel_in = f;
  channel_out = f;
  init_space();
  CHECK(channel_in == f && channel_out == f, "init keeps streams");

  /* ввод: поток -> полуинтервал пространства */
  f = make_input("ABC", 3);
  CHECK(f != NULL, "tmpfile in");
  channel_in = f;
  memset(space, 0, sizeof space);
  halted = 0;
  CHECK(space_input(24, 0x2000, channel_in) == 0, "input rc");
  CHECK(!halted, "no halt on full input");
  CHECK(memcmp(&space[0x2000 / 8], "ABC", 3) == 0, "input bytes landed");

  /* вывод: полуинтервал пространства -> поток */
  f = tmpfile();
  CHECK(f != NULL, "tmpfile out");
  channel_out = f;
  memset(space, 0, sizeof space);
  memcpy(&space[0x2010 / 8], "XYZ", 3);
  halted = 0;
  CHECK(space_output(24, 0x2010, channel_out) == 0, "output rc");
  CHECK(!halted, "no halt on full output");
  n = read_all(channel_out, buf, sizeof buf);
  CHECK(n == 3 && memcmp(buf, "XYZ", 3) == 0, "output bytes escaped");

  /* n == 0: потоки не трогаем */
  pos = ftell(channel_in);
  pos2 = ftell(channel_out);
  CHECK(space_input(0, 0x2000, channel_in) == 0, "input n=0 rc");
  CHECK(space_output(0, 0x2010, channel_out) == 0, "output n=0 rc");
  CHECK(ftell(channel_in) == pos && ftell(channel_out) == pos2, "n=0 untouched");
  CHECK(!halted, "n=0 no halt");

  /* кривой адрес/выравнивание: пропуск без halt и без касания */
  pos = ftell(channel_in);
  CHECK(space_input(8, SPACE_SIZE, channel_in) == -1, "input past end rc");
  CHECK(ftell(channel_in) == pos && !halted, "input past end untouched");
  CHECK(space_input(7, 0x2000, channel_in) == -1, "input unaligned n rc");
  CHECK(space_input(8, 1, channel_in) == -1, "input unaligned addr rc");
  CHECK(space_output(8, SPACE_SIZE, channel_out) == -1, "output past end rc");
  CHECK(space_input(8, 0x2000, NULL) == -1, "input NULL rc");
  CHECK(!halted, "no halt on bad addresses");

  /* конец ввода: короткий трансфер ставит halt */
  f = make_input("", 0);
  CHECK(f != NULL, "tmpfile empty");
  channel_in = f;
  halted = 0;
  CHECK(space_input(8, 0x2000, channel_in) == -1, "input eof rc");
  CHECK(halted, "halt on eof");
  halted = 0;

  /* диспетчер: [n, dst, LAST] — ввод */
  f = make_input("Z", 1);
  CHECK(f != NULL, "tmpfile Z");
  channel_in = f;
  memset(space, 0, sizeof space);
  memset(processor, 0, sizeof processor);
  write_image(5, 8, 0x2100, LAST);
  run_instruction(5);
  CHECK(space[0x2100 / 8] == 'Z' && !halted, "dispatch input");

  /* диспетчер: [n, LAST, src] — вывод того же байта */
  f = tmpfile();
  CHECK(f != NULL, "tmpfile Q");
  channel_out = f;
  write_image(6, 8, LAST, 0x2100);
  run_instruction(6);
  n = read_all(channel_out, buf, sizeof buf);
  CHECK(n == 1 && buf[0] == 'Z' && !halted, "dispatch output");

  /* [n, LAST, LAST] и n == 0 с LAST — nop без касаний */
  pos = ftell(channel_in);
  pos2 = ftell(channel_out);
  write_image(7, 8, LAST, LAST);
  run_instruction(7);
  write_image(7, 0, LAST, 0x2140);
  run_instruction(7);
  CHECK(ftell(channel_in) == pos && ftell(channel_out) == pos2, "degenerate untouched");
  CHECK(!halted, "degenerate no halt");

  /* внахлёст: ввод и вывод одного полуинтервала, ввод раньше */
  f = make_input("HI", 2);
  CHECK(f != NULL, "tmpfile HI");
  channel_in = f;
  f = tmpfile();
  CHECK(f != NULL, "tmpfile HI out");
  channel_out = f;
  write_image(7, 16, 0x2140, LAST);
  write_image(8, 16, LAST, 0x2140);
  run_instruction(7);
  run_instruction(8);
  n = read_all(channel_out, buf, sizeof buf);
  CHECK(n == 2 && memcmp(buf, "HI", 2) == 0 && !halted, "overlap pipe-through");

  /* ядро end-to-end: тройки core работают как обычные инструкции */
  init_space();
  f = make_input("Q", 1);
  CHECK(f != NULL, "tmpfile Q2");
  channel_in = f;
  f = tmpfile();
  CHECK(f != NULL, "tmpfile zero");
  channel_out = f;
  halted = 0;
  run_instruction(0);
  CHECK(space[0x2000 / 8] == 'Q', "core in landed");
  /* образ не испорчен вводом: второй такт — снова ввод */
  f = make_input("W", 1);
  CHECK(f != NULL, "tmpfile W");
  channel_in = f;
  run_instruction(0);
  CHECK(space[0x2000 / 8] == 'W', "channel still armed");
  run_instruction(1);
  n = read_all(channel_out, buf, sizeof buf);
  CHECK(n == 1 && buf[0] == 0 && !halted, "core out zero");

  /* кривой канальный адрес через диспетчер: пропуск */
  memset(space, 0, sizeof space);
  memset(processor, 0, sizeof processor);
  write_image(10, 8, SPACE_SIZE, LAST);
  f = make_input("Q", 1);
  CHECK(f != NULL, "tmpfile Q3");
  channel_in = f;
  pos = ftell(channel_in);
  halted = 0;
  run_instruction(10);
  CHECK(ftell(channel_in) == pos && !halted, "dispatch bad addr skipped");

  /* такт дорабатывается и при halted */
  memset(space, 0, sizeof space);
  memset(processor, 0, sizeof processor);
  write_image(9, 8, 0x2200, 0x2240);
  space[0x2240 / 8] = 0xA5;
  halted = 1;
  computation();
  CHECK(space[0x2200 / 8] == 0xA5 && halted, "tick finishes on halt");

  if(fails == 0)
    printf("ALL CHANNEL TESTS PASSED\n");
  else
    printf("%d FAILURES\n", fails);
  return fails ? 1 : 0;
}
