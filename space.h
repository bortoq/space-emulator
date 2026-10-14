/*
 * ADDRESS SPACE DEFINITIONS (fixed variant)
 *
 * Эмулятор параллельного адресного пространства позиций.
 * Оригинал: dmitri bortoq, demo1 (2022-2024), MIT license.
 * Исправленный вариант: устранены ошибки оригинала
 * (см. README.md, раздел "Исправления").
 *
 * Устройство: пространство = одновременно доступные битовые позиции.
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

/* Настройки адресного пространства */
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

/* Тип аргумента инструкции (достаточно широк для любого поля) */
typedef uint16_t size;

/*
 * Копирование битов (n, src, s, dst, d):
 *   n - число копируемых бит;
 *   src - исходный массив, s - исходный битовый индекс;
 *   dst - массив-приёмник, d - битовый индекс приёмника.
 * Бит 0 байта = старший бит (маска 0x80), нумерация подряд.
 * Перекрывающиеся интервалы внутри одного массива безопасны.
 */
void bitcpy(size_t n, const void *src, size_t s, void *dst, size_t d);

/* Таблицы (определения — в globals.c) */
extern size processor[PROCESSOR_N][3]; /* разобранные инструкции копирования */
extern unsigned char space[SPACE_SIZE / CHAR_BIT]; /* биты пространства */

/* Формат инструкции: смещения и длины полей [n, dst, src] */
extern const size o[N_FIELDS];
extern const size s[N_FIELDS];

/* Стартовое содержимое: 3 разобранные инструкции (n, dst, src) */
extern const size core[9];

#endif /* SPACE_H_ */
