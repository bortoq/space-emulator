/*
 * BIT COPY (clean implementation)
 *
 * Побитовое копирование между массивами. Написано заново,
 * искать автора не нужно.
 *
 * Пересечение копируемых полуинтервалов не контролируется:
 * гарантируется, что копирований "внахлест" не будет
 * (устройство пространства требует непересекающихся интервалов).
 *
 * Соглашение о битах: бит 0 байта = старший (0x80), далее подряд.
 */
#include <limits.h>
#include <stddef.h>

/*
 * copy bits from one array to another (n, src, s, dst, d)
 *
 * n - number of bits to copy
 * src - source array
 * s - source bit index
 * dst - destination array
 * d - destination bit index
 *
 * intervals [s, s + n) and [d, d + n) must not overlap
 */
void bitcpy(size_t n, const void *src_org, size_t src_offset, void *dst_org, size_t dst_offset)
{
  const unsigned char *src;
  unsigned char *dst;
  size_t i;

  if(n == 0)
    return;
  if(src_org == NULL || dst_org == NULL)
    return;

  src = (const unsigned char*)src_org;
  dst = (unsigned char*)dst_org;

  for(i = 0; i < n; ++i)
  {
    size_t s = src_offset + i;
    size_t d = dst_offset + i;
    if((src[s / CHAR_BIT] >> (CHAR_BIT - 1 - s % CHAR_BIT)) & 1)
      dst[d / CHAR_BIT] |= (unsigned char)(0x80 >> (d % CHAR_BIT));
    else
      dst[d / CHAR_BIT] &= (unsigned char)~(0x80 >> (d % CHAR_BIT));
  }
}
