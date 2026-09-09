/**
 * @file    dwt.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-11-21
 * @brief   header of dwt.c
 * @details
 */

#ifndef __DWT_H__
#define __DWT_H__

#include <stdint.h>

#define DWT_LEVELS 5 // Level of DWT (e.g., 5 means 1024 -> 32)
#define DWT_QUANT_THRESHOLD 200.0f // Quantized threshold of DWT
#define DWT_QUANT_STEP 5.0f // Quantized step (i.e., the scale factor)

void haar_transform(
  float *data,
  uint32_t n,
  uint16_t levels);
void inverse_haar_transform(
  float *coeffs,
  uint32_t n,
  uint16_t levels);
void quantize_and_threshold(
  float *coeffs,
  uint32_t n,
  float threshold,
  float quant_step);
void inverse_quantize(
  float *decoded_coeffs,
  uint32_t n,
  float quant_step);

#endif /* __DWT_H__ */
