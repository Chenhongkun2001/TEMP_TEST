/**
 * @file    app_froto_config_decoder.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Header of app_froto_config_decoder.c
 * @details
 */
#ifndef __APP_FROTO_CONFIG_DECODER_H__
#define __APP_FROTO_CONFIG_DECODER_H__

#include "pb_decode.h"
#include "pb_encode.h"
#include "DeviceAppBulletGateway.pb.h"

bool app_froto_config_hash_upload_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t config_hash_upload_processor,
  uint8_t *errCode);
bool app_froto_specific_config_upload_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t specific_config_upload_processor,
  uint8_t *errCode);
bool app_froto_config_dissem_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t config_dissem_processor,
  uint8_t *errCode);
bool app_froto_config_retrieve_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t config_retrieve_processor,
  uint8_t *errCode);

#endif /* __APP_FROTO_CONFIG_DECODER_H__ */
