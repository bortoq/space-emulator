/*
 * ADDRESS SPACE TABLES
 *
 * Определения таблиц из space.h (один файл определений),
 * чтобы не было множественных определений из заголовка.
 */
#include "space.h"

size processor[PROCESSOR_N][3];
unsigned char space[SPACE_SIZE / CHAR_BIT];

/* format of the instruction "copy n, dst, src" */
const size o[N_FIELDS] = {0, N_SIZE, N_SIZE + ADDRESS_SIZE};
const size s[N_FIELDS] = {N_SIZE, ADDRESS_SIZE, ADDRESS_SIZE};

/* core: ввод через LAST, вывод через LAST, загрузчик.
   канальные буферы — вне образов инструкций [0, 3072)
   и вне полуинтервалов загрузчика, иначе ввод портит образ */
const size core[9] = {
  0x0008, 0x2000, LAST, /* in:  8 бит stdin -> [0x2000, 0x2008) */
  0x0008, LAST, 0x2010, /* out: 8 бит [0x2010, 0x2018) -> stdout */
  0x0BD0, 0x0BD0, 0x0C00, /* loader: 3024 бита [0x0C00, ...) -> [0x0BD0, ...) */
};
