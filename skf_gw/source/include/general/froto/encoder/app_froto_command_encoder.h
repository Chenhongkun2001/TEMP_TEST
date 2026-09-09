/**
 * @file    app_froto_command_encoder.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-11-14
 * @brief   Header of app_froto_command_encoder.c
 * @details
 */
#ifndef __APP_FROTO_COMMAND_ENCODER_H__
#define __APP_FROTO_COMMAND_ENCODER_H__

#include "pb_decode.h"
#include "pb_encode.h"
#include "DeviceAppBulletGateway.pb.h"
#include "../app_froto_common.h"

bool app_froto_n_encode_msg_command_dissem(
  SKFChina_Froto_FrotoPmtType frotoPrimitive,
  protobuf_codec_command_pair_t *commandPair,
  SKFChina_Common_ProductType productType,
  const uint8_t *pt_gwid_str,
  uint8_t pt_gwid_str_len,
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  uint32_t *seq_no,
  uint8_t *encodedBuf,
  uint32_t maxEncodedBufLen,
  uint32_t *encodedBufLen,
  uint32_t *used_seq_no,
  SKFChina_Froto_FrotoMsgType *msgType,
  uint8_t *errCode);

#endif /* __APP_FROTO_COMMAND_ENCODER_H__ */
