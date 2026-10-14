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

/* core: канал ввода, канал вывода, загрузчик */
const size core[9] = {
  0x0008, 0x0020, 0x0C00, /* channel in:  8 бит stdin -> space[0x0020] */
  0x0008, 0x0C08, 0x0000, /* channel out: 8 бит space[0x0C08] -> stdout */
  0x0BD0, 0x0BD0, 0x0C00, /* loader: 3024 бита 0x0C00 -> 0x0BD0 */
};
