/**
 * @file    huffman.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-11-21
 * @brief   Implementation of huffman codec.
 * @details
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "huffman.h"
#include "huffman_table.h"

static const huffmanCode_t *find_code(int16_t symbol);
static int8_t write_code_value(
  huffmanBitstreamBuf_t *bs,
  uint16_t code_val,
  uint32_t len);
static int8_t read_bit(huffmanBitstreamBuf_t *bs);
static int16_t read_raw_symbol(
  huffmanBitstreamBuf_t *bs,
  int8_t *ret);

/**
 * @brief Huffman encoding.
 * @param symbols: input data to be encoded.
 * @param n: the number of symbols.
 * @param bs: the encoded stream. Nothing would be filled to the stream if
 * it is in check mode (i.e., @p check is non-zero); the encoded stream
 * would be filled here otherwise.
 * @param check: if check is not zero, then only calculate the bit length
 * after encoding (but no real encoding happens).
 * @param ret: a pointer to return error. -1 would be returned if bit
 * length after encoding is greater than the buffer size. 0 would be
 * returned if no error happened. NULL is allowed. If NULL, then nothing
 * can be returned.
 * @return: In check mode, the bit length after encoding is returned;
 * Otherwise, the returned bit length is only valid if no error happened.
 */
uint32_t
huffman_encode_static(
  const float *symbols,
  uint32_t n,
  huffmanBitstreamBuf_t *bs,
  int8_t check,
  int8_t *ret) {
  uint32_t _total_bits = 0;
  const huffmanCode_t *_escape_code = find_code(ESCAPE_SYMBOL);

  if(ret != NULL)
  {
    *ret = 0;
  }

  if(!_escape_code)
  {
    return 0;
  }

  if(check != 0)
  {
    // If check is not zero, then only calculate the bit length after
    // encoding (but no real encoding happens).
    for(uint32_t _i = 0; _i < n; _i++)
    {
      int16_t _symbol = (int16_t)symbols[_i];
      const huffmanCode_t *_code_entry = find_code(_symbol);

      if(_code_entry)
      {
        _total_bits += _code_entry->len;
      }
      else
      {
        _total_bits += _escape_code->len;
        _total_bits += RAW_SYMBOL_BITS;
      }
    }
    if(_total_bits > (bs->buf_size * 8))
    {
      if(ret != NULL)
      {
        *ret = -1;
      }
    }
  }
  else
  {
    // Otherwise, encoding starts ...
    for(uint32_t _i = 0; _i < n; _i++)
    {
      int16_t _symbol = (int16_t)symbols[_i];
      const huffmanCode_t *_code_entry = find_code(_symbol);

      if(_code_entry)
      {
        // Write the Huffman code
        if(-1 == write_code_value(bs, _code_entry->code_val,
                                  _code_entry->len))
        {
          if(ret != NULL)
          {
            *ret = -1;
            break;
          }
        }
        _total_bits += _code_entry->len;
      }
      else
      {
        // Escape: write escape symbol + original value

        // a. write escape symbol
        if(-1 == write_code_value(bs, _escape_code->code_val,
                                  _escape_code->len))
        {
          if(ret != NULL)
          {
            *ret = -1;
            break;
          }
        }
        _total_bits += _escape_code->len;

        // b. write original value
        if(-1 == write_code_value(bs, (uint16_t)_symbol, RAW_SYMBOL_BITS))
        {
          if(ret != NULL)
          {
            *ret = -1;
            break;
          }
        }
        _total_bits += RAW_SYMBOL_BITS;
      }
    }
  }
  return _total_bits;
}
/**
 * @brief Build a tree for Huffman decoding according to
 * @p PdM_Static_Table . "1" to right and "0" to left.
 * @return: root of the tree.
 */
huffmanNode_t *
build_decoding_tree(void) {
  huffmanNode_t *_root = (huffmanNode_t *)calloc(1, sizeof(huffmanNode_t));

  if(!_root)
  {
    return NULL;
  }
  _root->symbol = INTERNAL_SYMBOL;

  for(size_t _i = 0;
      _i < (sizeof(PdM_Static_Table) / sizeof(PdM_Static_Table[0]));
      _i++)
  {
    const huffmanCode_t *_entry = &PdM_Static_Table[_i];
    huffmanNode_t *_current = _root;

    for(uint32_t _j = 0; _j < _entry->len; _j++)
    {

      uint8_t _bit = (_entry->code_val >> (_entry->len - 1 - _j)) & 0x01;

      if(_bit == 0)
      {
        // bit == 0
        if(!_current->left)
        {
          _current->left =
            (huffmanNode_t *)calloc(1, sizeof(huffmanNode_t));
          _current->left->symbol = INTERNAL_SYMBOL;
        }
        _current = _current->left;
      }
      else
      {
        // bit == 1
        if(!_current->right)
        {
          _current->right =
            (huffmanNode_t *)calloc(1, sizeof(huffmanNode_t));
          _current->right->symbol = INTERNAL_SYMBOL;
        }
        _current = _current->right;
      }
    }
    _current->symbol = _entry->symbol;
  }
  return _root;
}
/**
 * @brief Free the tree in a recursive manner.
 * @node: root of the tree.
 */
void
free_huffman_tree(huffmanNode_t *node) {
  if(!node)
  {
    return;
  }
  free_huffman_tree(node->left);
  free_huffman_tree(node->right);
  free(node);
}
/**
 * @brief Huffman decoding.
 * @param bs: the input stream to be decoded.
 * @param root: decoding tree (should be initiatied before this function
 * being called).
 * @param decoded_symbols: the output decoded symbols.
 * @param max_symbols: the expected amount of decoded symbols.
 * @param ret: a pointer to return error. -1 would be returned if memory
 * overflow happened in @p bs . -2 if decoding error happened. 0 would be
 * returned if no error happened. NULL is allowed. If NULL, then nothing
 * can be returned.
 * @return: the amount of decoded symbol is returned. The returned value is
 * only valid if no error happened.
 */
uint32_t
huffman_decode_static(
  huffmanBitstreamBuf_t *bs,
  huffmanNode_t *root,
  float *decoded_symbols,
  uint32_t max_symbols,
  int8_t *ret) {

  uint32_t _symbol_idx = 0;
  huffmanNode_t *_current = root;

  if(ret != NULL)
  {
    *ret = 0;
  }

  while(_symbol_idx < max_symbols)
  {
    int8_t _bit = read_bit(bs);

    if(_bit == -1)
    {
      if(ret != NULL)
      {
        *ret = -1;
      }
      break;
    }

    _current = (_bit == 0) ? _current->left : _current->right;

    if(!_current)
    {
      printf("[Error] Invalid code path encountered!\n");
      if(ret != NULL)
      {
        *ret = -2;
      }
      break;
    }

    if(_current->symbol != INTERNAL_SYMBOL)
    {
      if(_current->symbol == ESCAPE_SYMBOL)
      {
        // Read the raw value if escape
        int8_t _ret;
        int16_t _tmp = read_raw_symbol(bs, &_ret);

        if(_ret != 0)
        {
          if(ret != NULL)
          {
            *ret = -1;
          }
          break;
        }
        decoded_symbols[_symbol_idx++] = (float)_tmp;
      }
      else
      {
        // For normal Huffman codes
        decoded_symbols[_symbol_idx++] = (float)_current->symbol;
      }

      _current = root;
    }
  }
  return _symbol_idx;
}
// --------------------------------------------------
// Static functions are implemented below ...
static const huffmanCode_t *
find_code(int16_t symbol) {
  for(size_t _i = 0;
      _i < (sizeof(PdM_Static_Table) / sizeof(PdM_Static_Table[0]));
      _i++)
  {
    if(PdM_Static_Table[_i].symbol == symbol)
    {
      return &PdM_Static_Table[_i];
    }
  }
  return NULL;
}
static int8_t
write_code_value(
  huffmanBitstreamBuf_t *bs,
  uint16_t code_val,
  uint32_t len) {
  for(uint32_t _i = 0; _i < len; _i++)
  {
    uint8_t _bit = (code_val >> (len - 1 - _i)) & 0x01;

    if(bs->byte_index >= bs->buf_size)
    {
      return -1;
    }

    if(_bit == 1)
    {
      bs->buffer[bs->byte_index] |= (1 << (7 - bs->bit_offset));
    }

    bs->bit_offset++;

    if(bs->bit_offset == 8)
    {
      bs->bit_offset = 0;
      bs->byte_index++;
    }
  }

  return 0;
}
static int8_t
read_bit(huffmanBitstreamBuf_t *bs) {
  if(bs->byte_index >= bs->buf_size)
  {
    return -1;
  }

  int8_t _bit = (bs->buffer[bs->byte_index] >>
                 (7 - bs->bit_offset)) & 0x01;

  bs->bit_offset++;
  if(bs->bit_offset == 8)
  {
    bs->bit_offset = 0;
    bs->byte_index++;
  }
  return _bit;
}
static int16_t
read_raw_symbol(
  huffmanBitstreamBuf_t *bs,
  int8_t *ret) {
  int16_t _symbol = 0;

  if(ret != NULL)
  {
    *ret = 0;
  }

  for(uint32_t _i = 0; _i < RAW_SYMBOL_BITS; _i++)
  {
    int8_t _bit = read_bit(bs);

    if(_bit == -1)
    {
      if(ret != NULL)
      {
        *ret = -1;
      }
      return 0;
    }

    _symbol |= (_bit << (RAW_SYMBOL_BITS - 1 - _i));
  }
  return _symbol;
}
