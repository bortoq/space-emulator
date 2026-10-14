/*
 * ADDRESS SPACE DEFINITIONS (fixed variant)
 *
 * copyright 2024-10-31, dmitri bortoq, mit license
 *
 * Адресное пространство позиций: одновременно доступные битовые позиции.
 * Единственное действие: копирование битовых полуинтервалов
 *   copy n, dst, src  (n бит из [src, src+n) в [dst, dst+n)).
 * Непересекаемость полуинтервалов эмулятор не проверяет, он на неё
 * полагается; её обеспечивает программа, исполняемая в пространстве.
 *
 * Единственный канал наружу — последняя позиция пространства LAST:
 * инструкция с первой позицией полуинтервала источника LAST
 * ([n, dst, LAST]) копирует n бит из stdin в [dst, dst+n);
 * инструкция с первой позицией полуинтервала приёмника LAST
 * ([n, LAST, src]) копирует n бит из [src, src+n) в stdout.
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
  LAST = SPACE_SIZE - 1,          /* единственный канал: последняя позиция */
  PROC_IMAGE = PROCESSOR_N * INSTRUCTION_SIZE /* бит в 64 инструкциях процессора */
};

/* types */
typedef uint16_t  size;           /* large enough for instruction argument */

/* copy bits from one array to another (n, src, s, dst, d).
   Полуинтервалы [s, s + n) и [d, d + n) не пересекаются
   (гарантия вызывающей стороны). */
void bitcpy(size_t n, const void *src, size_t s, void *dst, size_t d);

/* GLOBALS (определения — в globals.c, движок — в engine.c) */
extern size processor[PROCESSOR_N][3];       /* parsed copy instructions */
extern unsigned char space[SPACE_SIZE / CHAR_BIT]; /* data bits of the bit space */

/* format of the instruction [n, dst, src] */
extern const size o[N_FIELDS];
extern const size s[N_FIELDS];

/* стартовое ядро: одна инструкция загрузки (n, dst, src) */
extern const size core[3];

/* ENGINE */
extern int halted;               /* 1 после конца ввода/ошибки: такт до конца, затем останов */
extern FILE *channel_in;         /* поток ввода (по умолчанию stdin) */
extern FILE *channel_out;        /* поток вывода (по умолчанию stdout) */
void run_instruction(size i);    /* исполнить i-ю инструкцию (копия или канал) */
void computation(void);          /* один такт: все инструкции подряд */
void init_space(void);           /* начальная инструкция, таблица, каналы */
int space_input(size n_bits, size addr_bits, FILE *f);  /* f -> space, 0 ok */
int space_output(size n_bits, size addr_bits, FILE *f); /* space -> f, 0 ok */

#endif /* SPACE_H_ */
