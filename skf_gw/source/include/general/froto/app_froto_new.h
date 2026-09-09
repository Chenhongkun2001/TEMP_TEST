/**
 * @file    app_froto_new.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Header of app_froto_new.c
 * @details
 */
#ifndef __APP_FROTO_NEW_H__
#define __APP_FROTO_NEW_H__

#include "pb_decode.h"
#include "pb_encode.h"
#include "DeviceAppBulletGateway.pb.h"
#include "froto/app_froto_common.h"
#include "app_common.h"

#ifndef FROTO_USE_LITTLE_ENDIAN
#define FROTO_USE_LITTLE_ENDIAN (1)
#else
#if (FROTO_USE_DEFAULT_ENDIAN != 1)
#error "Not support big endian"
#endif
#endif

#define FROTO_MAX_CONCURRENT_NUM_REPLY_ACK (40)
#define FROTO_MAX_SINGLE_CHANNEL_CONCURRENT_NUM_REPLY_ACK (10)
#define FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER (10)
#define FROTO_DATAPAIR_MAX_NUMBER (5)
#define FROTO_CONFIGPAIR_MAX_NUMBER (5)
#define FROTO_TIME_MAX_NUMBER (10)
#define FROTO_WORKMODE_PARAM_MAX_NUMBER (20)
#define FROTO_MAX_DATA_LENGTH (320U)
#define FROTO_MAX_IMAGE_FILE_LENGTH (320U)

typedef bool ( *froto_send_msg_t )(
  void *userData,
  void *data_to_be_sent,
  uint32_t dataLen,
  uint8_t *err_code);
typedef bool ( *froto_read_msg_t )(
  void *userData,
  void *read_data,
  uint32_t *dataLen,
  uint32_t maxDataLen,
  uint8_t *err_code);

// Receiving messages
bool app_froto_n_get_message_type(
  uint8_t *readBuf,
  uint32_t readBufLen,
  uint8_t *errCode);
bool app_froto_n_get_message_type_server(
  uint8_t *readBuf,
  uint32_t readBufLen,
  froto_after_parse_t data_upload_processor,
  froto_after_parse_t config_hash_upload_processor,
  froto_after_parse_t specific_config_upload_processor,
  froto_after_parse_t current_version_upload_processor,
  froto_after_parse_t fuota_status_processor,
  froto_after_parse_t image_block_request_processor,
  froto_after_parse_t config_dissem_processor,
  froto_after_parse_t file_notify_dissem_processor,
  froto_after_parse_t image_block_dissem_processor,
  froto_after_parse_t config_retrieve_processor,
  froto_after_parse_t image_block_retrieve_processor,
  uint8_t *errCode);

// Timing management routines used by Froto primitives
bool app_froto_n_init(uint8_t *errCode);
bool app_froto_gabage_clean(
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  uint8_t *errCode);
bool app_froto_n_simple_send(
  uint8_t *encodedBuf,
  uint32_t encodedBufLen,
  void *userData,
  froto_send_msg_t send,
  uint8_t *errCode);
bool app_froto_n_send_with_reply(
  uint8_t *encodedBuf,
  uint32_t encodedBufLen,
  void *userData,
  uint32_t timeout_s,
  froto_send_msg_t send,
  uint8_t *readBuf,
  uint32_t maxReadBufLen,
  uint32_t *readBufLen,
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  SKFChina_Froto_FrotoPmtType expPrimitive,
  SKFChina_Froto_FrotoMsgType expMsgType,
  bool checkSeqNo,
  uint32_t expSeqNo,
  uint32_t seqNo,
  uint8_t *errCode);
bool app_froto_n_send_with_multiple_replies_first_half(
  uint8_t *encodedBuf,
  uint32_t encodedBufLen,
  void *userData,
  uint32_t timeout_s,
  froto_send_msg_t send,
  uint8_t *readBuf,
  uint32_t maxReadBufLen,
  uint32_t *readBufLen,
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  SKFChina_Froto_FrotoPmtType expPrimitive,
  SKFChina_Froto_FrotoMsgType expMsgType,
  bool checkSeqNo,
  uint32_t expSeqNo,
  uint32_t seqNo,
  struct pthread_cond_var *cond_var,
  void **state,
  uint8_t *errCode);
bool app_froto_n_send_with_multiple_replies_second_half(
  uint32_t timeout_s,
  uint8_t *readBuf,
  uint32_t maxReadBufLen,
  uint32_t *readBufLen,
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  SKFChina_Froto_FrotoPmtType expPrimitive,
  SKFChina_Froto_FrotoMsgType expMsgType,
  bool checkSeqNo,
  uint32_t expSeqNo,
  uint32_t seqNo,
  struct pthread_cond_var *cond_var,
  void **state,
  bool lastOne,
  uint8_t *errCode);
bool app_froto_n_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  froto_after_parse_t data_upload_processor,
  froto_after_parse_t config_hash_upload_processor,
  froto_after_parse_t specific_config_upload_processor,
  froto_after_parse_t current_version_upload_processor,
  froto_after_parse_t fuota_status_processor,
  froto_after_parse_t image_block_request_processor,
  froto_after_parse_t config_dissem_processor,
  froto_after_parse_t file_notify_dissem_processor,
  froto_after_parse_t image_block_dissem_processor,
  froto_after_parse_t config_retrieve_processor,
  froto_after_parse_t image_block_retrieve_processor,
  uint8_t *errCode);

#endif /* __APP_FROTO_NEW_H__ */
