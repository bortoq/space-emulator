/*
 * ADDRESS SPACE DEFINITIONS (fixed variant)
 *
 * copyright 2024-10-31, dmitri bortoq, mit license
 *
 * Адресное пространство позиций: одновременно доступные битовые позиции.
 * Единственное действие: копирование битовых интервалов
 *   copy n, dst, src  (n бит из src в dst, интервалы не пересекаются).
 */
#ifndef SPACE_H_
#define SPACE_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* address space settings */
enum {
  SPACE_SIZE = 0xFFF0,             /* бит в пространстве; ограничение версии:
                                      кратно 8 и кратно INSTRUCTION_SIZE */
  LIFE = 0x10,                    /* тактов эмуляции (> 1) */
  PROCESSOR_N = 64,               /* инструкций в процессоре (> 2), кратно 8 */
  N_SIZE = 16,                    /* бит в поле "n" инструкции */
  ADDRESS_SIZE = 16,              /* бит в поле "dst" (у "src" столько же) */
  INSTRUCTION_SIZE = 16 + 2 * 16, /* бит в инструкции "copy n, dst, src" (> 10) */
  PROCESSOR_START = 0,            /* битовый индекс первой инструкции */
  N_FIELDS = 3,                   /* полей в инструкции: n, dst, src */
  CH_IN = 0,                      /* индекс инструкции-дескриптора ввода */
  CH_OUT = 1                      /* индекс инструкции-дескриптора вывода */
};

/* types */
typedef uint16_t  size;           /* large enough for instruction argument */

/* copy bits from one array to another (n, src, s, dst, d) */
void bitcpy(size_t n, const void *src, size_t s, void *dst, size_t d);

/* GLOBALS (определения — в globals.c) */
extern size processor[PROCESSOR_N][3];       /* parsed copy instructions */
extern unsigned char space[SPACE_SIZE / CHAR_BIT]; /* data bits of the bit space */

/* format of the instruction [n, dst, src] */
extern const size o[N_FIELDS];
extern const size s[N_FIELDS];

/* стартовое ядро: 3 разобранные инструкции (n, dst, src) */
extern const size core[9];

#endif /* SPACE_H_ */
