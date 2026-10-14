/* unit-тесты bitcpy: выравнивания, границы, перекрытия, guard-случаи */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "../space.h"

static int fails = 0;

#define CHECK(cond, msg)                                              \
  do {                                                              \
    if(!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); ++fails; } \
    else { printf("ok: %s\n", msg); } } while(0)

/* эталон: побитово через индексные геттеры/сеттеры */
static int getbit(const unsigned char *p, size_t i)
{
  return (p[i / CHAR_BIT] >> (CHAR_BIT - 1 - (i % CHAR_BIT))) & 1;
}

static void setbit(unsigned char *p, size_t i, int v)
{
  if(v)
    p[i / CHAR_BIT] |= (unsigned char)(0x80 >> (i % CHAR_BIT));
  else
    p[i / CHAR_BIT] &= (unsigned char)~(0x80 >> (i % CHAR_BIT));
}

static void refcpy(size_t n, const unsigned char *s, size_t so,
  unsigned char *d, size_t do_)
{
  size_t i;
  for(i = 0; i < n; ++i)
    setbit(d, do_ + i, getbit(s, so + i));
}

int main()
{
  unsigned char src[64], dst[64], ref[64];
  size_t n, so, do_;
  size_t t;

  /* нулевая длина и NULL — не падают */
  memset(src, 0, sizeof src);
  memset(dst, 0, sizeof dst);
  bitcpy(0, src, 0, dst, 0);
  bitcpy(10, NULL, 0, dst, 0);
  bitcpy(10, src, 0, NULL, 0);
  CHECK(1, "guards (n=0, NULL) no crash");

  /* выровненные и невыровненные копии против эталона */
  memset(src, 0, sizeof src);
  for(t = 0; t < sizeof src * CHAR_BIT; ++t)
    setbit(src, t, (t * 2654435761u >> 13) & 1);
  for(t = 0; t < 300; ++t)
  {
    n = (t * 37) % 200 + 1;
    so = (t * 53) % 180;
    do_ = (t * 29) % 180;
    memset(dst, 0xAA, sizeof dst);
    memset(ref, 0xAA, sizeof ref);
    bitcpy(n, src, so, dst, do_);
    refcpy(n, src, so, ref, do_);
    if(memcmp(dst, ref, sizeof dst) != 0)
    {
      printf("FAIL: mismatch n=%lu so=%lu do=%lu\n",
        (unsigned long)n, (unsigned long)so, (unsigned long)do_);
      ++fails;
      break;
    }
  }
  CHECK(fails == 0, "aligned/misaligned vs reference (300 cases)");

  /* в точности через границу байта, хвост < 8 бит */
  {
    unsigned char s2[4] = {0xFF, 0x00, 0xAA, 0x55};
    unsigned char d2[4] = {0, 0, 0, 0};
    unsigned char r2[4] = {0, 0, 0, 0};
    bitcpy(13, s2, 3, d2, 5);
    refcpy(13, s2, 3, r2, 5);
    CHECK(memcmp(d2, r2, 4) == 0, "13 bits so=3 do=5");
  }

  /* перекрытие: приёмник получает СТАРЫЕ биты (эталон — через снимок) */
  {
    unsigned char b1[16], b2[16], snap[16];
    for(t = 0; t < sizeof b1; ++t)
      b1[t] = (unsigned char)(t * 31 + 7);
    memcpy(b2, b1, sizeof b1);
    memcpy(snap, b1, sizeof snap);
    bitcpy(40, b1, 0, b1, 11);
    refcpy(40, snap, 0, b2, 11);
    CHECK(memcmp(b1, b2, sizeof b1) == 0, "overlap forward shift");
    for(t = 0; t < sizeof b1; ++t)
      b1[t] = (unsigned char)(t * 31 + 7);
    memcpy(b2, b1, sizeof b1);
    memcpy(snap, b1, sizeof snap);
    bitcpy(40, b1, 11, b1, 0);
    refcpy(40, snap, 11, b2, 0);
    CHECK(memcmp(b1, b2, sizeof b1) == 0, "overlap backward shift");
    for(t = 0; t < sizeof b1; ++t)
      b1[t] = (unsigned char)(t * 31 + 7);
    memcpy(b2, b1, sizeof b1);
    bitcpy(64, b1 + 2, 0, b1, 0);
    refcpy(64, b2 + 2, 0, b2, 0);
    CHECK(memcmp(b1, b2, sizeof b1) == 0, "overlap self aligned");
  }

  /* один бит */
  {
    unsigned char s3[2] = {0x80, 0x00};
    unsigned char d3[2] = {0x00, 0x00};
    bitcpy(1, s3, 0, d3, 15);
    CHECK(d3[1] == 0x01, "single bit 0->15");
  }

  if(fails == 0)
    printf("ALL TESTS PASSED\n");
  else
    printf("%d FAILURES\n", fails);
  if(fails)
    return 1;
  return 0;
}
