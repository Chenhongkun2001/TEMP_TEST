/**
 * @file    app_froto_data_selection_encoder.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Implementation of data selection (of Froto) encoder.
 * @details
 */
#include "sys_def.h"
#include "froto/app_froto_new.h"
#include "froto/app_froto_common.h"
#include "froto/encoder/app_froto_data_selection_encoder.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

/**
 * @brief Encode data selection message
 * @param frotoPrimitive To assign the Froto primitive
 * @param measurementType The measure types to collect
 * @param pt_dimension Pointer to the dimension of selected data (not required if NULL or set the pointed value to 0)
 * @param sensorType The sensors (corresponding to @p measurementType ).
 * Null is allowed. The corresponding sensor types would be set to unknown
 * when it is not given.
 * @param len The number of measure types
 * @param productType The product type (e.g., Insight Predict)
 * @param filter The filter to select. Null is allowed. The corresponding
 * fields (e.g., the start sample time and end sample time) would be set to
 * default values. For instance, start sample time would be set to 0, end
 * sample time would be set to current time.
 * @param pt_gwid_str The gateway ID string.
 * @param pt_gwid_str_len The length of @p pt_gwid_str
 * @param pt_sensorid_str The sensor (to be selected) ID string.
 * @param pt_sensorid_str_len The length of @p pt_sensorid_str
 * @param seq_no The given Froto message sequence number. Null is allowed.
 * The sequence number would be generated if @p seq_no is not given.
 * @param encodedBuf Pointer to the returned (encoded) message.
 * @param maxEncodedBufLen The max. length (in byte) of @p encodedBuf .
 * @param encodedBufLen The length (in byte) of the returned (encoded)
 * message.
 * @param used_seq_no Return the sequence number actually adopted
 * @param msgType Return the message type actually adopted
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_n_encode_msg_data_selection(
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
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _msg_config = { 0 };
  uint32_t _seq_no;
  protobuf_codec_general_bytes_t _gw_id_info = { 0 };
  protobuf_codec_general_bytes_t _sensor_id_info = { 0 };
  protobuf_codec_dataselect_meastype_t _meas_type = { 0 };
  protobuf_codec_general_bytes_t _s_type = { 0 };
  struct pb_ostream_s _ostream = { 0 };

  if((pt_gwid_str_len > MAX_ID_STR_LENGTH) || (pt_gwid_str_len == 0) ||
     (pt_gwid_str == NULL))
  {
    // The gateway ID string is with a invalid length
    return false;
  }
  if((pt_sensorid_str_len > MAX_ID_STR_LENGTH) ||
     (pt_sensorid_str_len == MAX_ID_STR_LENGTH) ||
     (pt_sensorid_str == NULL))
  {
    // The sensor ID string is with a invalid length
    return false;
  }
  if((len == 0) || (measurementType == NULL))
  {
    // Measurement type should be given
    return false;
  }
  if((encodedBuf == NULL) || (encodedBufLen == NULL))
  {
    // The pointers to the returned (encoded) buffer and its size cannot be
    // NULL
    return false;
  }
  if((used_seq_no == NULL) || (msgType == NULL))
  {
    // The pointers to the returned parameters cannot be NULL
    return false;
  }

  if(seq_no != NULL)
  {
    // The message sequence number has been given
    _seq_no = *seq_no;
  }
  else
  {
    // Otherwise, generate it
    _seq_no = app_froto_allocate_msg_seq_no();
  }

  _msg_config.appVer = 1;
  _msg_config.which__messages =
    SKFChina_App_AppMessage_data_selection_tag;

  // data_selection.header
  _msg_config._messages.data_selection.has_header = true;
  _msg_config._messages.data_selection.header.version = 1;
  _msg_config._messages.data_selection.header.is_up = false;
  _msg_config._messages.data_selection.header.message_seq_no = _seq_no;
  *used_seq_no =
    _msg_config._messages.data_selection.header.message_seq_no;
  _msg_config._messages.data_selection.header.time_to_live = 1; // Default
  _msg_config._messages.data_selection.header.primitive_type =
    frotoPrimitive;
  if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE)
  {
    _msg_config._messages.data_selection.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive ==
          SKFChina_Froto_FrotoPmtType_RELIABLE_DISSEMINATE)
  {
    _msg_config._messages.data_selection.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_RETRIVE)
  {
    _msg_config._messages.data_selection.header.message_type =
      SKFChina_Froto_FrotoMsgType_RETRIVE_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD)
  {
    _msg_config._messages.data_selection.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_RELIABLE_UPLOAD)
  {
    _msg_config._messages.data_selection.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else
  {
    _msg_config._messages.data_selection.header.message_type =
      SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;
  }
  *msgType = _msg_config._messages.data_selection.header.message_type;
  _msg_config._messages.data_selection.header.total_block = 1;
  _msg_config._messages.data_selection.header.has_ack_window_size_message
    = false;
  _msg_config._messages.data_selection.header.long_packet_id = _seq_no;
  _msg_config._messages.data_selection.header.has_current_block = false;
#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.data_selection.header.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.data_selection.header.has_is_big_endian = false;
  _msg_config._messages.data_selection.header.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  // Peer addr
  _msg_config._messages.data_selection.header.which_peer_addr =
    SKFChina_Froto_FrotoHeader_gateway_id_tag;
  _gw_id_info.buffer = l_malloc(pt_gwid_str_len);
  memcpy(_gw_id_info.buffer, pt_gwid_str, pt_gwid_str_len);
  _gw_id_info.size = pt_gwid_str_len;
  _gw_id_info.length = pt_gwid_str_len;
  _msg_config._messages.data_selection.header.peer_addr.gateway_id.arg =
    &_gw_id_info;
  _msg_config._messages.data_selection.header.peer_addr.gateway_id.funcs.
  encode = protobuf_enc_bytes_callback;
  // Sensor addr
  _sensor_id_info.buffer = l_malloc(pt_sensorid_str_len);
  memcpy(_sensor_id_info.buffer, pt_sensorid_str, pt_sensorid_str_len);
  _sensor_id_info.size = pt_sensorid_str_len;
  _sensor_id_info.length = pt_sensorid_str_len;
  _msg_config._messages.data_selection.header.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.data_selection.header.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // data_selection.appVer
  _msg_config._messages.data_selection.appVer = 1;

  // data_selection.sensor_id
  _msg_config._messages.data_selection.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.data_selection.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;
  // data_selection.measure_type
  _meas_type.size = len;
  _meas_type.cnt = len;
  _meas_type.buffer = l_malloc(len * sizeof(protobuf_codec_dataselect_meastype_ele_t));
  for(uint32_t i = 0; i < _meas_type.size; i++)
  {
    _meas_type.buffer[i].measureType = measurementType[i];
  }
  if(pt_dimension == NULL)
  {
    for(uint32_t _i = 0; _i < _meas_type.size; _i++)
    {
      _meas_type.buffer[_i].has_dimension = false;
    }
  }else{
    for(uint32_t _i = 0; _i < _meas_type.size; _i++)
    {
      if(_meas_type.buffer[_i].has_dimension == true)
      {
        _meas_type.buffer[_i].dimension = pt_dimension[_i];
      }
    }
  }
  _msg_config._messages.data_selection.measure_type.arg = &_meas_type;
  _msg_config._messages.data_selection.measure_type.funcs.encode =
    protobuf_enc_repeated_mtype_callback;

  // data_selection.sensor
  _s_type.size = len;
  _s_type.length = len;
  _s_type.buffer = l_malloc(len);
  if(sensorType != NULL)
  {
    for(uint32_t i = 0; i < _meas_type.size; i++)
    {
      _s_type.buffer[i] = sensorType[i];
    }
  }
  else
  {
    for(uint32_t i = 0; i < _meas_type.size; i++)
    {
      _s_type.buffer[i] = SKFChina_Common_SensorType_UNKNOWN_SENSOR;
    }
  }
  _msg_config._messages.data_selection.sensor.arg = &_s_type;
  _msg_config._messages.data_selection.sensor.funcs.encode =
    protobuf_enc_repeated_stype_callback;

  // data_selection.product
  _msg_config._messages.data_selection.product = productType;

  // data_selection.sample_time_start
  if(filter == NULL)
  {
    _msg_config._messages.data_selection.sample_time_start = 0;
    _msg_config._messages.data_selection.sample_time_end = time(NULL);
  }
  else
  {
    _msg_config._messages.data_selection.sample_time_start =
      filter->start_time_s;
    _msg_config._messages.data_selection.sample_time_end =
      filter->end_time_s;
  }

#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.data_selection.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.data_selection.has_is_big_endian = false;
  _msg_config._messages.data_selection.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  _ostream = pb_ostream_from_buffer((pb_byte_t *)encodedBuf,
                                    maxEncodedBufLen);
  if(false == pb_encode(&_ostream, SKFChina_App_AppMessage_fields,
                        (const void *)&_msg_config))
  {
    DBG_LOG_ERR("Failed to encode message, %s", _ostream.errmsg);
    rt = false;
    goto EXIT;
  }
  *encodedBufLen = _ostream.bytes_written;
  DBG_LOG_INFO("The encoded message is with %d bytes",
                _ostream.bytes_written);

EXIT:
  if(_sensor_id_info.buffer)
  {
    l_free(_sensor_id_info.buffer);
    _sensor_id_info.buffer = NULL;
    _sensor_id_info.length = 0;
    _sensor_id_info.size = 0;
  }
  if(_gw_id_info.buffer)
  {
    l_free(_gw_id_info.buffer);
    _gw_id_info.buffer = NULL;
    _gw_id_info.length = 0;
    _gw_id_info.size = 0;
  }
  if(_meas_type.buffer)
  {
    l_free(_meas_type.buffer);
    _meas_type.buffer = NULL;
    _meas_type.cnt = 0;
    _meas_type.size = 0;
  }
  if(_s_type.buffer)
  {
    l_free(_s_type.buffer);
    _s_type.buffer = NULL;
    _s_type.length = 0;
    _s_type.size = 0;
  }

  return rt;
}