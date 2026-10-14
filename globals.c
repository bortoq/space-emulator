/*
 * Общие таблицы адресного пространства.
 * Один файл определений на все программы (space, assembler, disassembler),
 * чтобы не было множественных определений из заголовка.
 */
#include "space.h"

size processor[PROCESSOR_N][3];
unsigned char space[SPACE_SIZE / CHAR_BIT];

/* Формат инструкции "copy n, dst, src" */
const size o[N_FIELDS] = {0, N_SIZE, N_SIZE + ADDRESS_SIZE};
const size s[N_FIELDS] = {N_SIZE, ADDRESS_SIZE, ADDRESS_SIZE};

/*
 * Стартовое ядро: три разобранные инструкции (n, dst, src).
 *  [0] канал ввода  (stdin -> space):  8 бит в позицию 0x0020
 *  [1] канал вывода (space -> stdout): 8 бит из позиции 0x0C08
 *  [2] загрузчик: 3024 бита из 0x0C00 в 0x0BD0
 */
const size core[9] = {
  0x0008, 0x0020, 0x0C00, /* канал ввода */
  0x0008, 0x0C08, 0x0000, /* канал вывода */
  0x0BD0, 0x0BD0, 0x0C00, /* загрузчик */
};
