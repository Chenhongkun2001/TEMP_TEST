/**
 * @file    app_froto_config_encoder.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Header of app_froto_config_encoder.c
 * @details
 */
#ifndef __APP_FROTO_CONFIG_ENCODER_H__
#define __APP_FROTO_CONFIG_ENCODER_H__

#include "pb_decode.h"
#include "pb_encode.h"
#include "DeviceAppBulletGateway.pb.h"
#include "../app_froto_common.h"

bool app_froto_n_encode_msg_config_dissem(
  SKFChina_Froto_FrotoPmtType frotoPrimitive,
  protobuf_codec_config_pair_t *configPair,
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
bool app_froto_n_encode_msg_config_retrieve(
  SKFChina_Froto_FrotoPmtType frotoPrimitive,
  SKFChina_ConfigurationAndCommand_RetrievePayload retrievePayload,
  uint32_t name_id,
  bool has_name_id,
  SKFChina_Common_SpecificConfigItem *specificConfigItem,
  SKFChina_Common_fSchedulerConfigItem *fSchedulerConfigItem,
  uint32_t len,
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

#endif /* __APP_FROTO_CONFIG_ENCODER_H__ */
