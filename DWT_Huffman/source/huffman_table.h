/**
 * @file    huffman_table.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-11-21
 * @brief   The look-up table used by Huffman codec.
 * @details
 */

#ifndef __HUFFMAN_TABLE_H__
#define __HUFFMAN_TABLE_H__

#include <stdint.h>

#define INTERNAL_SYMBOL -32768
#define ESCAPE_SYMBOL 32767
#define RAW_SYMBOL_BITS 16 // The bit length of the original value
                           // (int16_t)

typedef struct
{
  int16_t symbol;
  uint16_t code_val;
  uint8_t len;
}huffmanCode_t;

const huffmanCode_t PdM_Static_Table[] = {
  { 0, 0 /* 0b0 */, 1 },
  { 5, 0x0004 /* 0b100 */, 3 },
  { -5, 0x0005 /* 0b101 */, 3 },
  { 6, 0x000c /* 0b1100 */, 4 },
  { -6, 0x000d /* 0b1101 */, 4 },
  { 7, 0x001c /* 0b11100 */, 5 },
  { -7, 0x001d /* 0b11101 */, 5 },
  { 8, 0x003c /* 0b111100 */, 6 },
  { -8, 0x003d /* 0b111101 */, 6 },
  { 9, 0x007c /* 0b1111100 */, 7 },
  { -9, 0x007d /* 0b1111101 */, 7 },
  { 10, 0x00fc /* 0b11111100 */, 8 },
  { -10, 0x00fd /* 0b11111101 */, 8 },
  // Rest part: for escape symbols
  { ESCAPE_SYMBOL, 0x007f /* 0b1111111 */, 7 },
};

#endif /* __HUFFMAN_TABLE_H__ */
