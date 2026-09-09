/**
 * @file    main.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-11-21
 * @brief   A demo of WaveSnap (compression and un-compression)
 * @details
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "dwt.h"
#include "huffman.h"
#include "testdata.h"

// 1: To generate data
// 0: To read recorded data
#define DATA_GENERATION 0

// Length of waveform (pts)
#define N (sizeof(original_data_i16) / sizeof(original_data_i16[0]))
#define MAX_SYMBOLS 100 // The max amount of Hoffman symbol (<= 256)

// The structure used by Huffman symbol
// Only used by the Huffman codec stats info computing
typedef struct
{
  int symbol;
  int frequency;
}SymbolEntry;

#if (DATA_GENERATION == 1)
static int16_t original_data_i16[64000];

#else
static int16_t original_data_i16[] = TESTDATA_FT0FQ10E2467;
// static int16_t original_data_i16[] = TESTDATA_FT0FQ25E2239;
// static int16_t original_data_i16[] = TESTDATA_FT0FQ45E0460;
// static int16_t original_data_i16[] = TESTDATA_FT1FQ03E1024;
// static int16_t original_data_i16[] = TESTDATA_FT1FQ15E0946;
// static int16_t original_data_i16[] = TESTDATA_FT1FQ20E4005;
// static int16_t original_data_i16[] = TESTDATA_FT1FQ50E3215;
// static int16_t original_data_i16[] = TESTDATA_FT1FQ50E3268;
#endif

#define WAVEFORM_OUT_START 0lu
#define WAVEFORM_OUT_END N

static int32_t find_symbol(
  SymbolEntry *table,
  uint32_t count,
  int32_t symbol);

#if (DATA_GENERATION == 1)
/**
 * @brief To generate a vibration data (with @p size pts in int16_t)
 * @param data: used to store the generated data
 * @param size: the number of samples.
 */
void
generate_vibration_data(
  int16_t *data,
  uint32_t size) {

  srand(time(NULL));

  const int16_t BASE_AMPLITUDE = 100;
  const int16_t FAULT_AMPLITUDE = 110;
  const int16_t IMPULSE_AMPLITUDE = 120;

  for(uint32_t _i = 0; _i < size; _i++)
  {
    double current_amplitude;

    // [0, 199]: BASE_AMPLITUDE
    // [200, 249]: IMPULSE_AMPLITUDE * exp(-0.2 * t), t starting from 200
    // [250, 999]: FAULT_AMPLITUDE
    // [1000, 1199]: BASE_AMPLITUDE
    // [1200, 1249]: IMPULSE_AMPLITUDE * exp(-0.2 * t), t starting from 200
    // [1250, 1999]: FAULT_AMPLITUDE
    // [2000, 2199]: BASE_AMPLITUDE
    // ...
    if((_i % 1000) < 200)
    {
      current_amplitude = BASE_AMPLITUDE;
    }
    else if((_i % 1000) >= 200 && (_i % 1000) < 250)
    {
      int32_t t = (_i % 1000) - 200;

      current_amplitude = IMPULSE_AMPLITUDE * exp(-0.2 * t);
      if(current_amplitude < FAULT_AMPLITUDE)
      {
        current_amplitude = FAULT_AMPLITUDE;
      }
    }
    else
    {
      current_amplitude = FAULT_AMPLITUDE;
    }

    // The sample value is "sin + noise"
    // The nois range is [-25, 24]
    double value =
      current_amplitude * sin(_i * 0.1) + (rand() % 50 - 25);

    if(value > 32767)
    {
      value = 32767;
    }
    if(value < -32768)
    {
      value = -32768;
    }

    data[_i] = (int16_t)round(value);
  }
}
#endif /* DATA_GENERATION == 1 */
/**
 * @brief To find whether the found symbol exists in the array. The index
 * would be returned if existence; -1 would be returned if no existence.
 * @param table: the symbol array.
 * @param count: size of the symbol array.
 * @param symbol: the symbol to be found.
 * @return a value >= 0 if existence; -1 otherwise.
 */
static int32_t
find_symbol(
  SymbolEntry *table,
  uint32_t count,
  int32_t symbol) {
  for(uint32_t _i = 0; _i < count; _i++)
  {
    if(table[_i].symbol == symbol)
    {
      return _i;
    }
  }
  return -1;
}
// Compare function (callback of sorting)
// typedef int (*__compar_fn_t) (const void *, const void *);
static int
compare_symbols(
  const void *a,
  const void *b) {
  return ((SymbolEntry *)b)->frequency - ((SymbolEntry *)a)->frequency;
}
/**
 * @brief To analyze the symbol amount of the array to be encoded.
 * @param symbols: the array to be encoded.
 * @param n: the length (in pts) of the array.
 * @param table: the symbol array.
 * @param max_symbols: size of the symbol array.
 * @return the amount of symbol.
 */
uint32_t
analyze_and_generate_huffman_info(
  float *symbols,
  uint32_t n,
  SymbolEntry *table,
  uint32_t max_symbols) {
  uint32_t _symbol_cnt = 0;

  for(uint32_t _i = 0; _i < n; _i++)
  {

    int32_t _s = (int32_t)symbols[_i];

    int32_t _idx = find_symbol(table, _symbol_cnt, _s);

    if(_idx != -1)
    {
      // If the symbol has been existed ...
      table[_idx].frequency++;
    }
    else
    {
      // Otherwise, add the symbol to the table ...
      if(_symbol_cnt < max_symbols)
      {
        table[_symbol_cnt].symbol = _s;
        table[_symbol_cnt].frequency = 1;
        _symbol_cnt++;
      }
      else
      {
        // Do nothing but output warning ...
        // If symbols are too much, then distortion would happen ...
        printf(
          "[WARN] The amount of Huffman symbols is too much and distortion would happen!\n");
      }
    }
  }

  // Sort (the most frequent symbol first)
  qsort(table, _symbol_cnt, sizeof(SymbolEntry), compare_symbols);

  return _symbol_cnt;
}
/**
 * @brief To calculate root mean square error.
 * @param original: the original array.
 * @param reconstructed: the data recovered from compression.
 * @param n: the length (in pts) of the array.
 * @return RMSE.
 */
float
calculate_rmse(
  const int16_t *original,
  const float *reconstructed,
  int n) {
  double mse = 0.0;

  for(int i = 0; i < n; i++)
  {
    double error = (double)original[i] - reconstructed[i];

    mse += error * error;
  }
  mse /= n;
  return (float)sqrt(mse);
}
int
main() {
  float *_data_float = malloc(N * sizeof(float));
  uint8_t *_encoded_symbols = malloc(N * sizeof(uint16_t));
  float *_decoded_symbols = malloc(N * sizeof(float));
  SymbolEntry _huffman_table[MAX_SYMBOLS];

  // 1. Generate data
#if (DATA_GENERATION == 1)
  generate_vibration_data(original_data_i16, N);
#endif
  for(uint32_t _i = 0; _i < N; _i++)
  {
    _data_float[_i] = (float)original_data_i16[_i];
  }
  printf("--- Original data length (N = %lu) has been generated ---\n", N);
  printf("Original data size: %lu Bytes (%lu bits)\n", N * 2, N * 16);
  printf("A part of the data (pts %lu - %lu): \n", WAVEFORM_OUT_START,
         WAVEFORM_OUT_END);
  for(uint32_t _i = WAVEFORM_OUT_START; _i < WAVEFORM_OUT_END; _i++)
  {
    printf("%d, \n", original_data_i16[_i]);
  }

  // 2. Compressing
  // 2.1 DWT (haar)
  haar_transform(_data_float, N, DWT_LEVELS);
  // 2.2 Quantization
  quantize_and_threshold(_data_float, N, DWT_QUANT_THRESHOLD,
                         DWT_QUANT_STEP);
#if 0 // Enable DWT (Haar) output to debug DWT
  printf("DWT (Haar) (pts %lu - %lu): \n", 0, WAVEFORM_OUT_END);

  for(uint32_t _i = 0; _i < WAVEFORM_OUT_END; _i++)
  {
    printf("%f, \n", _data_float[_i]);
  }
#endif

  // 2.3 Prepare for Huffman coding (symbol analysis) (can be removed in
  // the final implementation)
  // Just print some information here ...
  // Practically static huffman codec would be used.
  uint32_t _symbol_count =
    analyze_and_generate_huffman_info(_data_float, N, _huffman_table,
                                      MAX_SYMBOLS);

  printf("The amount of quantized symbol: %u\n", _symbol_count);
  for(uint32_t _i = 0; _i < _symbol_count; _i++)
  {
    printf("symbol %d (frequency: %d)\n",
           _huffman_table[_i].symbol,
           _huffman_table[_i].frequency);
  }

  // 2.4 Huffman coding
  huffmanBitstreamBuf_t _bs;
  uint8_t _tmp_buf[1024];
  uint32_t _total_bits = 0;
  int8_t _huffman_ret = 0;

  _bs.buffer = _tmp_buf;
  _bs.buf_size = sizeof(_tmp_buf) / sizeof(_tmp_buf[0]);
  _bs.bit_offset = 0;
  _bs.byte_index = 0;

  // 2.4.1 Checking memory (read 100 data to encode per loop)
  _total_bits = 0;
  for(uint32_t _i = 0, _bits = 0; _i < N;)
  {
    uint32_t _len;

    _len = (100 < (N - _i)) ? 100 : (N - _i);
    _bits =
      huffman_encode_static(&_data_float[_i], _len, &_bs, 1,
                            &_huffman_ret);
    if(_huffman_ret == 0)
    {
      _total_bits += _bits;
    }
    else
    {
      printf(
        "[ERROR] Overflow happened (the data to be Huffman encoded is not sparse enough) (%d, %d)!\n",
        _i, _len);
      return -1;
    }
    _i += _len;
  }

  // 2.4.2 Start encoding if everything is OK (read 100 data to encode per
  // loop)
  _total_bits = 0;
  _bs.bit_offset = 0;
  _bs.byte_index = 0;
  memset(&_bs.buffer[0], 0, _bs.buf_size);
  for(uint32_t _i = 0, _bits = 0; _i < N;)
  {
    uint32_t _len;

    _len = (100 < (N - _i)) ? 100 : (N - _i);
    _bits =
      huffman_encode_static(&_data_float[_i], _len, &_bs, 0,
                            &_huffman_ret);

    if(_huffman_ret == 0)
    {
      memcpy(&_encoded_symbols[_total_bits / 8], &_bs.buffer[0],
             _bits / 8 + 1 +
             (((_total_bits % 8 + _bits) % 8 != 0) ? 1 : 0));
      _bs.buffer[0] = _bs.buffer[_bs.byte_index];
      memset(&_bs.buffer[1], 0, _bs.buf_size - 1);
      _bs.byte_index = 0;
      _total_bits += _bits;
    }
    else
    {
      printf(
        "[ERROR] Overflow happened (the data to be Huffman encoded is not sparse enough) (%d, %d)!\n",
        _i, _len);
      return -1;
    }
    _i += _len;
  }
#if 0 // Enable Huffman encoding output to debug Huffman codec
  printf("Huffman encoding (pts %lu - %lu): \n", 0,
         _total_bits / 8 + ((_total_bits % 8 == 0) ? 0 : 1));
  for(uint32_t _i = 0;
      _i < (_total_bits / 8 + ((_total_bits % 8 == 0) ? 0 : 1));
      _i++)
  {
    printf("%d, \n", _encoded_symbols[_i]);
  }
#endif

  printf("\n--- Compressed waveform ---\n");
  printf("Compressed wave size: %d bits\n", _total_bits);
  printf("Compressed wave size: %.2f KB\n",
         (float)_total_bits / 8.0f / 1024.0f);
  printf("Original wave size: %.2f KB\n",
         (float)(N * 16) / 8.0f / 1024.0f);

  float _compression_ratio = (float)(N * 16 - _total_bits) / (N * 16) *
                             100.0f;

  printf("Compression rate: %.2f%%\n", _compression_ratio);

  // 3. De-compressing
  // 3.1 Huffman decoding
  // 3.1.1 Prepare for Huffman decoding
  huffmanNode_t *_root = build_decoding_tree();

  if(!_root)
  {
    printf("[ERR] Failed to build Huffman tree!\n");
    return 1;
  }

  // 3.1.2 Huffman decoding (decode 100 symbols per loop)
  _bs.buffer = _tmp_buf;
  _bs.buf_size = sizeof(_tmp_buf) / sizeof(_tmp_buf[0]);
  _bs.bit_offset = 0;
  _bs.byte_index = _bs.buf_size;

  uint32_t _stream_length =
    (_total_bits / 8 + ((_total_bits % 8 == 0) ? 0 : 1));
  uint32_t _stream_length_read = 0;
  uint32_t _decoded_count = 0;

  for(uint32_t _i = 0, _decoded_cnt_tmp = 0; _i < N;)
  {
    uint32_t _len;
    uint32_t _len_bs;

    // The length of symbol to decode in a loop
    _len = (100 < (N - _i)) ? 100 : (N - _i);
    // The length of bitstream to read in a loop
    _len_bs =
      (_bs.byte_index <
       (_stream_length -
        _stream_length_read)) ? _bs.byte_index : (_stream_length -
                                                  _stream_length_read);

    memmove(&_bs.buffer[0], &_bs.buffer[_bs.byte_index],
            _bs.buf_size - _bs.byte_index);
    memcpy(&_bs.buffer[_bs.buf_size - _bs.byte_index],
           &_encoded_symbols[_stream_length_read],
           _len_bs);
    _bs.byte_index = 0;

    _decoded_cnt_tmp = huffman_decode_static(&_bs, _root,
                                             &_decoded_symbols[
                                               _decoded_count], _len,
                                             &_huffman_ret);
    if(_huffman_ret == 0)
    {
      if(_decoded_cnt_tmp != _len)
      {
        printf(
          "[ERROR] Unknown error in Huffman decoding (%d, %d)!\n",
          _i, _len);
        return -2;
      }
      _stream_length_read += _len_bs;
      _decoded_count += _decoded_cnt_tmp;
    }
    else
    {
      printf(
        "[ERROR] Overflow happened in Huffman decoding (%d, %d)!\n",
        _i, _len);
      return -1;
    }
    _i += _len;
  }
  // 3.1.3 Free the tree for Huffman decoding
  free_huffman_tree(_root);
#if 0 // Enable Huffman decoding output to debug Huffman codec
  printf("Huffman decoding (pts %lu - %lu): \n", 0lu,
         _decoded_count);
  for(uint32_t _i = 0; _i < _decoded_count; _i++)
  {
    printf("%f, \n", _decoded_symbols[_i]);
  }
#endif

  // 3.2 Inverse quantization
  inverse_quantize(_decoded_symbols, N, DWT_QUANT_STEP);
  // 3.3 IDWT (haar)
  inverse_haar_transform(_decoded_symbols, N, DWT_LEVELS);

  printf("\n--- De-compressed waveform (%u decoded) ---\n",
         _decoded_count);
  printf("A part of the recovered data (pts %lu - %lu): \n",
         WAVEFORM_OUT_START, WAVEFORM_OUT_END);
  for(uint32_t _i = WAVEFORM_OUT_START; _i < WAVEFORM_OUT_END; _i++)
  {
    printf("%.1f, \n", _decoded_symbols[_i]);
  }

  float _rmse = calculate_rmse(original_data_i16, _decoded_symbols, N);

  printf("RMSE: %.2f\n", _rmse);

  return 0;
}
