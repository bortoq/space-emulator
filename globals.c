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

/* core: одна инструкция загрузки процессора из канала.
   пользователь грузит код и данные сам, данным место вне [0, 3072) */
const size core[3] = {
  PROC_IMAGE, PROCESSOR_START, LAST, /* boot: 3072 бита stdin -> [0, 3072) */
};
