/**
 * @file    app_froto_data_selection_encoder.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Header of app_froto_config_encoder.c
 * @details
 */
#ifndef __APP_FROTO_DATA_SELECTION_ENCODER_H__
#define __APP_FROTO_DATA_SELECTION_ENCODER_H__

#include "pb_decode.h"
#include "pb_encode.h"
#include "DeviceAppBulletGateway.pb.h"
#include "../app_froto_common.h"

bool app_froto_n_encode_msg_data_selection(
  SKFChina_Froto_FrotoPmtType frotoPrimitive,
  SKFChina_Common_MeasurementType *measurementType,
  uint8_t *pt_dimension,
  SKFChina_Common_SensorType *sensorType,
  uint32_t len,
  SKFChina_Common_ProductType productType,
  froto_data_selection_filter_t *filter,
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

#endif /* __APP_FROTO_DATA_SELECTION_ENCODER_H__ */