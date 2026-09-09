/**
 * @file    app_froto_fuota_decoder.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Header of app_froto_fuota_decoder.c
 * @details
 */
#ifndef __APP_FROTO_FUOTA_DECODER_H__
#define __APP_FROTO_FUOTA_DECODER_H__

#include "pb_decode.h"
#include "pb_encode.h"
#include "DeviceAppBulletGateway.pb.h"

bool app_froto_current_version_upload_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t current_version_upload_processor,
  uint8_t *errCode);
bool app_froto_fuota_status_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t fuota_status_processor,
  uint8_t *errCode);
bool app_froto_image_block_request_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t image_block_request_processor,
  uint8_t *errCode);
bool app_froto_image_block_retrieve_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t image_block_retrieve_processor,
  uint8_t *errCode);
bool app_froto_file_notify_dissem_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t file_notify_dissem_processor,
  uint8_t *errCode);
bool app_froto_image_block_dissem_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t image_block_dissem_processor,
  uint8_t *errCode);

#endif /* __APP_FROTO_FUOTA_DECODER_H__ */
