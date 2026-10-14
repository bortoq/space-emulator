/*
 * BIT COPY (fixed variant)
 *
 * Основано на общеизвестном сниппете bitcpy (stackoverflow);
 * исходный файл demo1 помечен "TODO: find author ... and ask for license".
 *
 * Исправления:
 *  - явный прототип (совпадает с space.h), проверки указателей;
 *  - сравнение длин в size_t (без усечения к int);
 *  - перекрывающиеся интервалы копируются безопасно
 *    (снимок источника в боковой буфер, параллельная семантика);
 *  - убран неявный int-цикл while(--byte_len >= 0).
 *
 * Соглашение о битах: бит 0 байта = старший (0x80), далее подряд.
 */
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

/*
 * copy bits from one array to another (n, src, s, dst, d)
 *
 * n - number of bits to copy
 * src - source array
 * s - source bit index
 * dst - destination array
 * d - destination bit index
 */
void bitcpy(size_t n, const void *src_org, size_t src_offset, void *dst_org, size_t dst_offset)
{
#define PREPARE_FIRST_COPY()                                  \
    do {                                                      \
      if(n >= (size_t)(CHAR_BIT - dst_offset_modulo)) {       \
        *dst &= reverse_mask[dst_offset_modulo];              \
        n -= (size_t)(CHAR_BIT - dst_offset_modulo);          \
      }                                                       \
      else {                                                  \
        *dst &= (unsigned char)(reverse_mask[dst_offset_modulo] \
          | reverse_mask_xor[dst_offset_modulo + n]);         \
        c &= reverse_mask[dst_offset_modulo + n];             \
        n = 0;                                                \
      }                                                       \
    } while(0)

  static const unsigned char reverse_mask[] = {
    0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
  static const unsigned char reverse_mask_xor[] = {
    0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01, 0x00};

  const unsigned char *src;
  unsigned char *dst;
  size_t src_offset_modulo;
  size_t dst_offset_modulo;

  if(n == 0)
    return;
  if(src_org == NULL || dst_org == NULL)
    return;

  /* перекрытие в одном массиве: сначала снимок, потом приёмник */
  if(src_org == dst_org && src_offset != dst_offset &&
    src_offset < dst_offset + n && dst_offset < src_offset + n)
  {
    unsigned char* tmp;
    size_t bytes;
    bytes = (n + CHAR_BIT - 1) / CHAR_BIT + 1;
    tmp = (unsigned char*)malloc(bytes);
    if(tmp == NULL)
      return;
    memset(tmp, 0, bytes);
    bitcpy(n, src_org, src_offset, tmp, 0);
    bitcpy(n, tmp, 0, (void*)dst_org, dst_offset);
    free(tmp);
    return;
  }

  src = (const unsigned char*)src_org + (src_offset / CHAR_BIT);
  dst = (unsigned char*)dst_org + (dst_offset / CHAR_BIT);

  src_offset_modulo = src_offset % CHAR_BIT;
  dst_offset_modulo = dst_offset % CHAR_BIT;

  if(src_offset_modulo == dst_offset_modulo)
  {
    size_t byte_len;
    size_t tail;
    if(src_offset_modulo)
    {
      unsigned char c;

      c = reverse_mask_xor[dst_offset_modulo] & *src++;
      PREPARE_FIRST_COPY();
      *dst++ |= c;
    }

    byte_len = n / CHAR_BIT;
    tail = n % CHAR_BIT;

    if(byte_len)
    {
      memmove(dst, src, byte_len);
      src += byte_len;
      dst += byte_len;
    }
    if(tail)
    {
      *dst &= reverse_mask_xor[tail];
      *dst |= reverse_mask[tail] & *src;
    }
  }
  else
  {
    size_t bit_diff_ls;
    size_t bit_diff_rs;
    size_t byte_len;
    size_t tail;
    unsigned char c;

    /* Begin: Line things up on destination */
    if(src_offset_modulo > dst_offset_modulo)
    {
      bit_diff_ls = src_offset_modulo - dst_offset_modulo;
      bit_diff_rs = CHAR_BIT - bit_diff_ls;

      c = *src++ << bit_diff_ls;
      c |= *src >> bit_diff_rs;
      c &= reverse_mask_xor[dst_offset_modulo];
    }
    else
    {
      bit_diff_rs = dst_offset_modulo - src_offset_modulo;
      bit_diff_ls = CHAR_BIT - bit_diff_rs;

      c = *src >> bit_diff_rs & reverse_mask_xor[dst_offset_modulo];
    }

    PREPARE_FIRST_COPY();
    *dst++ |= c;

    /* Middle: copy with only shifting the source */
    byte_len = n / CHAR_BIT;

    for(tail = 0; tail < byte_len; ++tail)
    {
      c = *src++ << bit_diff_ls;
      c |= *src >> bit_diff_rs;
      *dst++ = c;
    }

    /* End: copy the remaining bits */
    tail = n % CHAR_BIT;
    if(tail)
    {
      c = *src++ << bit_diff_ls;
      c |= *src >> bit_diff_rs;
      c &= reverse_mask[tail];

      *dst &= reverse_mask_xor[tail];
      *dst |= c;
    }
  }
#undef PREPARE_FIRST_COPY
}
