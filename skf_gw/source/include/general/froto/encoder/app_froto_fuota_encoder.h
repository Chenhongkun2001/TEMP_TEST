/**
 * @file    app_froto_fuota_encoder.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Header of app_froto_fuota_encoder.c
 * @details
 */
#ifndef __APP_FROTO_FUOTA_ENCODER_H__
#define __APP_FROTO_FUOTA_ENCODER_H__

#include "pb_decode.h"
#include "pb_encode.h"
#include "DeviceAppBulletGateway.pb.h"

bool app_froto_n_encode_msg_version_retrieve(
  SKFChina_Froto_FrotoPmtType frotoPrimitive,
  SKFChina_FirmwareUpdateOverTheAir_RetrievePayload retrievePayload,
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
bool app_froto_n_encode_msg_fuota_notify_dissem(
  SKFChina_Froto_FrotoPmtType frotoPrimitive,
  uint32_t fuotaTaskId,
  SKFChina_Common_HardwareType hardwareType,
  uint32_t hardwareVer,
  uint32_t firmwareVer,
  bool forceFuota,
  SKFChina_Common_CommunicationType updateMethod,
  SKFChina_Common_FUOTAType updateType,
  uint8_t whichFuotaMethod,
  uint8_t *url,
  uint32_t urlLen,
  uint8_t *topic,
  uint32_t topicLen,
  bool isBLEUart,
  uint8_t whichCheckValue,
  uint8_t *checkValue,
  uint32_t checkValueLen,
  SKFChina_Common_Encryption encryp,
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
bool app_froto_n_encode_msg_image_block_dissem(
  SKFChina_Froto_FrotoPmtType frotoPrimitive,
  uint32_t offset,
  bool is_fuota_task,
  uint32_t task_id,
  uint8_t *content,
  uint32_t contentLen,
  uint32_t hash,
  uint32_t totalBlock,
  uint32_t currentBlock,
  uint32_t longPktId,
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
bool app_froto_n_encode_msg_image_block_request(
  SKFChina_Froto_FrotoPmtType frotoPrimitive,
  bool isFuota,
  uint32_t taskId,
  uint32_t offset,
  uint32_t size,
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

#endif /* __APP_FROTO_FUOTA_ENCODER_H__ */
