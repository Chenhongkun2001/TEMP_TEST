/**
 * @file    huffman.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-11-21
 * @brief   The header of huffman.c.
 * @details
 */

#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

#include <stdint.h>

typedef struct
{
  uint8_t *buffer;
  uint32_t buf_size;
  uint32_t byte_index;
  uint32_t bit_offset;
}huffmanBitstreamBuf_t;

typedef struct huffmanNode
{
  int16_t symbol;
  struct huffmanNode *left;
  struct huffmanNode *right;
}huffmanNode_t;

uint32_t huffman_encode_static(
  const float *symbols,
  uint32_t n,
  huffmanBitstreamBuf_t *bs,
  int8_t check,
  int8_t *ret);
huffmanNode_t *build_decoding_tree();
void free_huffman_tree(huffmanNode_t *node);
uint32_t huffman_decode_static(
  huffmanBitstreamBuf_t *bs,
  huffmanNode_t *root,
  float *decoded_symbols,
  uint32_t max_symbols,
  int8_t *ret);

#endif /* __HUFFMAN_H__ */
