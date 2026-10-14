/*
 * Побитовое копирование между массивами (исправленный вариант).
 * Основано на общеизвестном сниппете bitcpy (stackoverflow);
 * исходный файл demo1 помечен "TODO: find author ... and ask for license".
 *
 * Исправления:
 *  - явный прототип (совпадает с space.h), проверки указателей;
 *  - сравнение длин в size_t (без усечения к int);
 *  - перекрывающиеся интервалы копируются безопасно (через memmove
 *    в выровненной ветке и через временный буфер в сдвиговой ветке);
 *  - убран неявный int-цикл while(--byte_len >= 0).
 *
 * Соглашение о битах: бит 0 байта = старший (0x80), далее подряд.
 */

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

void bitcpy(size_t n, const void *src_org, size_t src_offset,
            void *dst_org, size_t dst_offset)
{
#define PREPARE_FIRST_COPY()                                          \
  do {                                                                \
    if (n >= (size_t)(CHAR_BIT - dst_offset_modulo)) {                \
      *dst &= reverse_mask[dst_offset_modulo];                        \
      n -= (size_t)(CHAR_BIT - dst_offset_modulo);                    \
    } else {                                                          \
      *dst &= (unsigned char)(reverse_mask[dst_offset_modulo]         \
        | reverse_mask_xor[dst_offset_modulo + n]);                   \
      c &= reverse_mask[dst_offset_modulo + n];                       \
      n = 0;                                                          \
    }                                                                 \
  } while (0)

  static const unsigned char reverse_mask[] = {
    0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
  static const unsigned char reverse_mask_xor[] = {
    0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01, 0x00};

  const unsigned char *src;
  unsigned char *dst;
  size_t src_offset_modulo;
  size_t dst_offset_modulo;

  if (n == 0) return;                    /* guard: нечего копировать */
  if (src_org == NULL || dst_org == NULL) return; /* guard: битые указатели */

  /*
   * Перекрытие в одном массиве: сначала целиком в боковой буфер,
   * потом из него в приёмник (параллельная семантика копирования).
   * Проверка ДО любых записей — иначе первый байт затёр бы источник.
   */
  if (src_org == dst_org && src_offset != dst_offset &&
      src_offset < dst_offset + n && dst_offset < src_offset + n) {
    unsigned char *tmp = (unsigned char *)malloc((n + CHAR_BIT - 1) / CHAR_BIT + 1);
    if (tmp == NULL) return;            /* guard: нет памяти */
    memset(tmp, 0, (n + CHAR_BIT - 1) / CHAR_BIT + 1);
    bitcpy(n, src_org, src_offset, tmp, 0);
    bitcpy(n, tmp, 0, (void *)dst_org, dst_offset);
    free(tmp);
    return;
  }

  src = (const unsigned char *)src_org + (src_offset / CHAR_BIT);
  dst = (unsigned char *)dst_org + (dst_offset / CHAR_BIT);

  src_offset_modulo = src_offset % CHAR_BIT;
  dst_offset_modulo = dst_offset % CHAR_BIT;

  if (src_offset_modulo == dst_offset_modulo) {
    /* Выровненный случай: начало совпадает — можно копировать целыми байтами. */
    size_t byte_len, tail;
    if (src_offset_modulo) {
      unsigned char c;
      c = (unsigned char)(reverse_mask_xor[dst_offset_modulo] & *src++);
      PREPARE_FIRST_COPY();
      *dst++ |= c;
    }

    byte_len = n / CHAR_BIT;
    tail = n % CHAR_BIT;

    if (byte_len) {
      /* memmove вместо memcpy: интервалы space могут перекрываться. */
      memmove(dst, src, byte_len);
      src += byte_len;
      dst += byte_len;
    }
    if (tail) {
      *dst &= reverse_mask_xor[tail];
      *dst |= (unsigned char)(reverse_mask[tail] & *src);
    }
  } else {
    /*
     * Невыровненный случай: битовый сдвиг источника.
     * При перекрытии src/dst побайтовое чтение-запись вперёд может
     * затереть ещё не прочитанные байты — работаем через временный буфер.
     */
    size_t bit_diff_ls, bit_diff_rs;
    size_t byte_len, tail;
    unsigned char c;

    if (src_offset_modulo > dst_offset_modulo) {
      bit_diff_ls = src_offset_modulo - dst_offset_modulo;
      bit_diff_rs = (size_t)CHAR_BIT - bit_diff_ls;
      c = (unsigned char)((*src++ << bit_diff_ls) & 0xFF);
      c |= (unsigned char)(*src >> bit_diff_rs);
      c &= reverse_mask_xor[dst_offset_modulo];
    } else {
      bit_diff_rs = dst_offset_modulo - src_offset_modulo;
      bit_diff_ls = (size_t)CHAR_BIT - bit_diff_rs;
      c = (unsigned char)((*src >> bit_diff_rs) & reverse_mask_xor[dst_offset_modulo]);
    }

    PREPARE_FIRST_COPY();
    *dst++ |= c;

    byte_len = n / CHAR_BIT;

    /* Перекрытия здесь нет (обработано в начале) — прямой проход. */
    for (tail = 0; tail < byte_len; ++tail) {
      c = (unsigned char)((*src++ << bit_diff_ls) & 0xFF);
      c |= (unsigned char)(*src >> bit_diff_rs);
      *dst++ = c;
    }

    tail = n % CHAR_BIT;
    if (tail) {
      c = (unsigned char)((*src++ << bit_diff_ls) & 0xFF);
      c |= (unsigned char)(*src >> bit_diff_rs);
      c &= reverse_mask[tail];
      *dst &= reverse_mask_xor[tail];
      *dst |= c;
    }
  }
#undef PREPARE_FIRST_COPY
}
