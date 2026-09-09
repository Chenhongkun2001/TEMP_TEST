/**
 * @file    dwt.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-11-21
 * @brief   Implementation of discrete wavelet transform.
 * @details
 */
#include "dwt.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * @brief DWT (haar).
 * @param data: input data to be transformed and the transformed wave also
 * would be stored in the same place.
 * @param n: the number of samples.
 * @param levels: level of dwt.
 */
void
haar_transform(
  float *data,
  uint32_t n,
  uint16_t levels) {
  float _A, _D;
  uint32_t _step, _current_n;

  for(uint16_t _l = 0; _l < levels; _l++)
  {
    _step = 1 << _l;
    _current_n = n / _step;

    if(_current_n < 2)
    {
      break;
    }

    if((_current_n & 0x01) == 1)
    {
      // _current_n is odd
      for(uint32_t _i = 0; (_i + 1) < _current_n; _i += 2)
      {
        _A = round((data[_i] + data[_i + 1]) / 2.0f); // Approximation
        _D = round((data[_i] - data[_i + 1]) / 2.0f); // Detail

        memmove(&data[_i / 2 + 1], &data[_i / 2],
                _i / 2 * sizeof(data[0]));
        data[_i / 2] = _A;
        data[_i + 1] = _D;
      }
    }
    else
    {
      // _current_n is even
      for(uint32_t _i = 0; _i < _current_n; _i += 2)
      {
        _A = round((data[_i] + data[_i + 1]) / 2.0f); // Approximation
        _D = round((data[_i] - data[_i + 1]) / 2.0f); // Detail

        memmove(&data[_i / 2 + 1], &data[_i / 2],
                _i / 2 * sizeof(data[0]));
        data[_i / 2] = _A;
        data[_i + 1] = _D;
      }
    }
  }
}
/**
 * @brief Inverse DWT (haar).
 * @param data: input data to be IDWTed and the IDWTed wave also
 * would be stored in the same place.
 * @param n: the number of samples.
 * @param levels: level of IDWT (should be identical to the level of DWT).
 */
void
inverse_haar_transform(
  float *coeffs,
  uint32_t n,
  uint16_t levels) {

  for(int32_t _l = levels - 1; _l >= 0; _l--)
  {
    uint32_t _step = 1 << _l;
    uint32_t _current_n = n / _step;

    if(_current_n < 2)
    {
      break;
    }

    for(uint32_t _i = 0; _i < _current_n / 2; _i++)
    {
      float _A = coeffs[_i * 2]; // Approximation
      float _D = coeffs[_i + _current_n / 2]; // Detail

      memmove(&coeffs[_i * 2 + 2], &coeffs[_i * 2 + 1],
              sizeof(coeffs[0]) * (_current_n / 2 - _i - 1));
      coeffs[_i * 2] = _A + _D;
      coeffs[_i * 2 + 1] = _A - _D;
    }
  }
}
/**
 * @brief To quantize and sparsify the array (i.e. the outcome of DWT
 * (haar)).
 * @param coeffs: input data (i.e., outcome of DWT (haar) and the
 * quantized/sparsified wave also would be stored in the same place.
 * @param n: the number of samples.
 * @param threshold: set to 0 when the value is within the threshold.
 * @param quant_step: scale factor (i.e., value / @p quant_step ).
 */
void
quantize_and_threshold(
  float *coeffs,
  uint32_t n,
  float threshold,
  float quant_step) {

  for(uint32_t _i = n / (1 << DWT_LEVELS);
      _i < n;
      _i++)
  {
    if(fabs(coeffs[_i]) < threshold)
    {
      coeffs[_i] = 0.0f;
    }
  }
  for(uint32_t _i = 0; _i < n; _i++)
  {
    coeffs[_i] = round(coeffs[_i] / quant_step);
  }
}
/**
 * @brief To inverse the quantization the array (i.e. the quantized and
 * sparsified DWT (haar) array).
 * @param decoded_coeffs: input data (i.e., the quantized and sparsified
 * DWT (haar) array, and the recovered array also would be stored in the
 * same place.
 * @param n: the number of samples.
 * @param quant_step: scale factor (i.e., value * @p quant_step ).
 */
void
inverse_quantize(
  float *decoded_coeffs,
  uint32_t n,
  float quant_step) {
  for(uint32_t _i = 0; _i < n; _i++)
  {
    decoded_coeffs[_i] *= quant_step;
  }
}
