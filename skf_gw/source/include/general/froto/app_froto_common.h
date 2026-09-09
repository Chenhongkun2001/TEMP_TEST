/**
 * @file    app_froto_common.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-28
 * @brief   Header of app_froto_common.c
 * @details
 */
#ifndef __APP_FROTO_COMMON_H__
#define __APP_FROTO_COMMON_H__

#include "pb_decode.h"
#include "pb_encode.h"
#include "DeviceAppBulletGateway.pb.h"

typedef bool ( *froto_after_parse_t )(
  void *parsed_data,
  uint8_t *err_code);

typedef struct
{
  uint64_t start_time_s;
  uint64_t end_time_s;
}froto_data_selection_filter_t;
typedef struct
{
  uint32_t size;
  uint32_t length; // Actual length of data in buffer
  uint8_t *buffer;
}protobuf_codec_general_bytes_t;
typedef struct
{
  uint32_t size;
  uint32_t cnt;
  uint32_t *buffer;
}protobuf_codec_acked_seq_number_t;
#define protobuf_codec_config_item_t protobuf_codec_acked_seq_number_t
typedef struct
{
  SKFChina_Common_MeasurementType measureType;
  bool has_dimension;
  uint8_t dimension;
}protobuf_codec_dataselect_meastype_ele_t;
typedef struct
{
  uint32_t size;
  uint32_t cnt;
  protobuf_codec_dataselect_meastype_ele_t *buffer;
}protobuf_codec_dataselect_meastype_t;
typedef struct
{
  SKFChina_Common_MeasurementType measureType;
  bool has_sensor_type;
  SKFChina_Common_SensorType sensorType;
  SKFChina_Common_ProductType productType;
  bool has_range;
  SKFChina_Common_Range range;
  bool has_unit;
  SKFChina_Common_Unit unit;
  bool has_sample_rate_hz;
  float sample_rate_hz;
  bool has_sample_period_s;
  uint32_t sample_period_s;
  bool has_sample_dimension;
  uint8_t sample_dimension;
  bool has_measure_length_sample;
  uint32_t measure_length_sample;
  bool has_total_data_length_sample;
  uint32_t total_data_length_sample;
  uint32_t measure_seq_no;
  SKFChina_Common_Format data_format;
  bool has_crc32_value;
  uint32_t crc32_value;
  bool has_encryp;
  SKFChina_Common_Encryption encryp;
  uint8_t *sensorID;
  uint8_t sensorIDLen;
  uint64_t sample_time;
  bool has_memory_id;
  int32_t memory_id;
  SKFChina_Common_AlarmAndNotify alarm;
  uint8_t *mac_address;
  uint8_t mac_addressLen;
  uint8_t *iccid;
  uint8_t iccidLen;
  uint8_t *imei;
  uint8_t imeiLen;
}protobuf_codec_data_pair_ele_meas_t;
typedef struct
{
  uint32_t start_point;
  uint32_t crc32;
  bool crc32_valid;
  uint8_t *data;
  uint32_t dataLen;
}protobuf_codec_data_pair_ele_data_t;
typedef struct
{
  bool has_measurement;
  protobuf_codec_data_pair_ele_meas_t measurement;
  protobuf_codec_data_pair_ele_data_t data;
}protobuf_codec_data_pair_ele_t;
typedef struct
{
  uint32_t size;
  uint32_t cnt;
  protobuf_codec_data_pair_ele_t *buffer;
}protobuf_codec_data_pair_t;
typedef struct
{
  uint8_t *sensorID;
  uint8_t sensorIDLen;
  SKFChina_Common_HardwareType hardware_type;
  uint32_t hardware_version;
  uint32_t firmware_version;
}protobuf_codec_current_version_t;
typedef struct
{
  uint8_t *sensorID;
  uint8_t sensorIDLen;
  uint8_t fileType;
  uint32_t taskId;
  uint8_t update_method;
  bool overBleUART;
}protobuf_codec_file_notify_dissem_t;
typedef struct
{
  uint8_t *sensorID;
  uint8_t sensorIDLen;
  bool isFuota;
  uint32_t taskId;
  uint32_t offset;
  uint32_t seqNo;
  uint32_t longPktId;
  uint8_t *content;
  uint32_t contentLen;
  uint32_t hashValue;
  uint32_t currentBlock;
  uint32_t totalBlock;
}protobuf_codec_image_block_dissem_t;
typedef struct
{
  uint8_t *sensorID;
  uint8_t sensorIDLen;
  uint32_t task_id;
  SKFChina_FirmwareUpdateOverTheAir_UpdateStatus status;
  uint32_t receivedBytes;
}protobuf_codec_fuota_status_t;
typedef struct
{
  uint8_t *sensorID;
  uint8_t sensorIDLen;
  uint32_t task_id;
  bool is_fuota_task;
  uint32_t offset;
  uint32_t size;
}protobuf_codec_image_block_request_status_t;
typedef struct
{
  uint8_t *sensorID;
  uint8_t sensorIDLen;
  uint32_t task_id;
  bool is_fuota_task;
  uint32_t offset;
  uint32_t size;
}protobuf_codec_image_block_retrieve_status_t;
typedef struct
{
  uint8_t *sensorID;
  uint8_t sensorIDLen;
  bool has_last_edit_time;
  uint64_t last_edit_time;
  uint32_t hash;
}protobuf_codec_config_hash_t;
typedef struct
{
  uint8_t *sensorID;
  uint8_t sensorIDLen;
  bool isGWConfigFile;
}protobuf_codec_config_retrieve_t;
typedef struct
{
  uint8_t *mac_addr;
  uint8_t mac_addrLen;
  uint32_t name_id;
}protobuf_codec_gatewayinfo_t;
typedef struct
{
  SKFChina_Common_WorkMode work_mode;
  uint8_t *parameters;
  uint8_t parametersLen;
}protobuf_codec_workmode_t;
typedef struct
{
  uint32_t size;
  uint32_t cnt;
  uint64_t *time;
}protobuf_codec_timearray_t;
typedef struct
{
  uint8_t which_config_item;

  union
  {
    SKFChina_Common_SpecificConfigItem specific_config_item;
    SKFChina_Common_fSchedulerConfigItem fscheduler_config_item;
  }config_item;

  bool has_memory_id;
  uint32_t memory_id;
  uint8_t which_config_content;

  union
  {
    bool content_bool;
    uint32_t content_uint32;
    int32_t content_int32;
    float content_float;
    protobuf_codec_timearray_t time;
    protobuf_codec_workmode_t work_mode;
    protobuf_codec_gatewayinfo_t gateway_info;
  }content;
}protobuf_codec_config_pair_ele_t;
typedef struct
{
  uint32_t size;
  uint32_t cnt;
  protobuf_codec_config_pair_ele_t *buffer;
}protobuf_codec_config_pair_t;
typedef struct
{
  uint8_t *sensorID;
  uint8_t sensorIDLen;
  protobuf_codec_config_pair_t config_pair;
}protobuf_codec_specific_config_t;
typedef struct
{
  SKFChina_Common_Command command;
  uint8_t which_command_para;

  union
  {
    uint32_t para_uint32;
    int32_t para_int32;
    float para_float;
    protobuf_codec_timearray_t time;
  }para;
}protobuf_codec_command_pair_t;

uint32_t froto_crc32(
  const uint8_t *buf,
  uint32_t size);
uint32_t app_froto_allocate_msg_seq_no(void);

bool protobuf_enc_bytes_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg);
bool protobuf_enc_repeated_mtype_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg);
bool protobuf_enc_repeated_stype_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg);
bool protobuf_enc_repeated_specific_conf_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg);
bool protobuf_enc_repeated_fscheduler_conf_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg);
bool protobuf_enc_repeated_conf_pair_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg);
bool protobuf_enc_timearray_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg);
bool protobuf_dec_bytes_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg);
bool protobuf_dec_timearray_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg);
bool protobuf_dec_acked_seq_number_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg);
bool protobuf_dec_data_pair_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg);
bool protobuf_dec_config_pair_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg);

#endif /* __APP_FROTO_COMMON_H__ */
