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
 * Размеры задаются флагами (-s биты пространства, -p инструкции процессора),
 * ниже — значения по умолчанию и формат (формат неизменен).
 * Единственный канал наружу — последняя позиция пространства.
 */
#ifndef SPACE_H_
#define SPACE_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* defaults and format */
enum {
  SPACE_BITS_DFLT = 0xFFF0,       /* -s по умолчанию (бит) */
  PROC_N_DFLT = 64,               /* -p по умолчанию (инструкций) */
  SPACE_BITS_MAX = 0x10000,       /* наибольший размер (позиции 16-битные) */
  N_SIZE = 16,                    /* бит в поле "n" инструкции */
  ADDRESS_SIZE = 16,              /* бит в поле "dst" (у "src" столько же) */
  INSTRUCTION_SIZE = 16 + 2 * 16, /* бит в инструкции "copy n, dst, src" (> 10) */
  PROCESSOR_START = 0,            /* битовый индекс первой инструкции */
  N_FIELDS = 3                    /* полей в инструкции: n, dst, src */
};

/* limits (проверяются при запуске):
   space_nbits % INSTRUCTION_SIZE == 0 (отсюда и кратность 8),
   space_nbits <= SPACE_BITS_MAX (позиции адресуются 16 битами),
   proc_n >= 1, proc_n * INSTRUCTION_SIZE <= space_nbits (образ влезает).
   Отсюда proc_n <= 1365 < 65535: индекса `size` достаточно везде. */

/* types */
typedef uint16_t  size;           /* large enough for instruction argument */

/* configuration (-s, -p, -t; определения — в globals.c) */
extern size_t space_nbits;        /* бит в пространстве */
extern size_t proc_n;             /* инструкций в процессоре */
extern uint64_t tick_limit;       /* -t (0 — без предела) */

/* copy bits from one array to another (n, src, s, dst, d).
   Полуинтервалы [s, s + n) и [d, d + n) не пересекаются
   (гарантия вызывающей стороны). */
void bitcpy(size_t n, const void *src, size_t s, void *dst, size_t d);

/* GLOBALS (определения — в globals.c, движок — в engine.c) */
extern size (*processor)[3];               /* parsed copy instructions */
extern unsigned char *space;               /* data bits of the bit space */

/* format of the instruction [n, dst, src] */
extern const size o[N_FIELDS];
extern const size s[N_FIELDS];

/* ENGINE */
extern int halted;               /* 1 после конца ввода/ошибки: такт до конца, затем останов */
extern FILE *channel_in;         /* поток ввода (по умолчанию stdin) */
extern FILE *channel_out;        /* поток вывода (по умолчанию stdout) */
void run_instruction(size i);    /* исполнить прочитанную i-ю инструкцию */
int computation(void);           /* такт: 1 если исполнен (был живой), 0 если все NOP */
void init_space(void);           /* выделение, начальная инструкция, каналы */
int space_input(size n_bits, size addr_bits, FILE *f);  /* f -> space, 0 ok */
int space_output(size n_bits, size addr_bits, FILE *f); /* space -> f, 0 ok */

#endif /* SPACE_H_ */
