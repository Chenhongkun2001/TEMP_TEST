/**
 * @file    app_froto_config_encoder.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Implementation of config related message (of Froto) encoder.
 * @details
 */
#include "sys_def.h"
#include "froto/app_froto_new.h"
#include "froto/app_froto_common.h"
#include "froto/encoder/app_froto_config_encoder.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

/**
 * @brief Encode configuration dissemination message
 * @param frotoPrimitive To assign the Froto primitive
 * @param configPair Config pairs
 * @param productType The product type (e.g., Insight Predict)
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
app_froto_n_encode_msg_config_dissem(
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
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _msg_config = { 0 };
  uint32_t _seq_no;
  protobuf_codec_general_bytes_t _gw_id_info = { 0 };
  protobuf_codec_general_bytes_t _sensor_id_info = { 0 };
  protobuf_codec_config_pair_t _config_pair = { 0 };
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
  if(configPair == NULL)
  {
    // Config pair should be given
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
    SKFChina_App_AppMessage_config_dissem_tag;

  // config_dissem.header
  _msg_config._messages.config_dissem.has_header = true;
  _msg_config._messages.config_dissem.header.version = 1;
  _msg_config._messages.config_dissem.header.is_up = false;
  _msg_config._messages.config_dissem.header.message_seq_no = _seq_no;
  *used_seq_no =
    _msg_config._messages.config_dissem.header.message_seq_no;
  _msg_config._messages.config_dissem.header.time_to_live = 1; // Default
  _msg_config._messages.config_dissem.header.primitive_type =
    frotoPrimitive;
  if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE)
  {
    _msg_config._messages.config_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive ==
          SKFChina_Froto_FrotoPmtType_RELIABLE_DISSEMINATE)
  {
    _msg_config._messages.config_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD)
  {
    _msg_config._messages.config_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_RELIABLE_UPLOAD)
  {
    _msg_config._messages.config_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else
  {
    _msg_config._messages.config_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;
  }
  *msgType = _msg_config._messages.config_dissem.header.message_type;
  _msg_config._messages.config_dissem.header.total_block = 1;
  _msg_config._messages.config_dissem.header.has_ack_window_size_message
    = false;
  _msg_config._messages.config_dissem.header.long_packet_id = _seq_no;
  _msg_config._messages.config_dissem.header.has_current_block = false;
#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.config_dissem.header.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.config_dissem.header.has_is_big_endian = false;
  _msg_config._messages.config_dissem.header.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  // Peer addr
  _msg_config._messages.config_dissem.header.which_peer_addr =
    SKFChina_Froto_FrotoHeader_gateway_id_tag;
  _gw_id_info.buffer = l_malloc(pt_gwid_str_len);
  memcpy(_gw_id_info.buffer, pt_gwid_str, pt_gwid_str_len);
  _gw_id_info.size = pt_gwid_str_len;
  _gw_id_info.length = pt_gwid_str_len;
  _msg_config._messages.config_dissem.header.peer_addr.gateway_id.arg =
    &_gw_id_info;
  _msg_config._messages.config_dissem.header.peer_addr.gateway_id.funcs.
  encode = protobuf_enc_bytes_callback;
  // Sensor addr
  _sensor_id_info.buffer = l_malloc(pt_sensorid_str_len);
  memcpy(_sensor_id_info.buffer, pt_sensorid_str, pt_sensorid_str_len);
  _sensor_id_info.size = pt_sensorid_str_len;
  _sensor_id_info.length = pt_sensorid_str_len;
  _msg_config._messages.config_dissem.header.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.config_dissem.header.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // config_dissem.appVer
  _msg_config._messages.config_dissem.appVer = 1;

  // config_dissem.sensor_id
  _msg_config._messages.config_dissem.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.config_dissem.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // config_dissem.product
  _msg_config._messages.config_dissem.product = productType;

  // config_dissem.config_pair
  _config_pair.cnt = configPair->cnt;
  _config_pair.size = configPair->size;
  _config_pair.buffer = l_malloc((_config_pair.size) *
                                 (sizeof(configPair->buffer[0])));
  for(uint32_t i = 0; i < _config_pair.size; i++)
  {
    _config_pair.buffer[i].which_config_item =
      configPair->buffer[i].which_config_item;

    if(_config_pair.buffer[i].which_config_item ==
       SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag)
    {
      _config_pair.buffer[i].config_item.specific_config_item =
        configPair->buffer[i].config_item.specific_config_item;
    }
    else if(_config_pair.buffer[i].which_config_item ==
            SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag)
    {
      _config_pair.buffer[i].config_item.fscheduler_config_item =
        configPair->buffer[i].config_item.fscheduler_config_item;
    }

    _config_pair.buffer[i].has_memory_id =
      configPair->buffer[i].has_memory_id;
    _config_pair.buffer[i].memory_id = configPair->buffer[i].memory_id;
    _config_pair.buffer[i].which_config_content =
      configPair->buffer[i].which_config_content;
    if(_config_pair.buffer[i].which_config_content ==
       SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
    {
      _config_pair.buffer[i].content.content_uint32 =
        configPair->buffer[i].content.content_uint32;
    }
    else if(_config_pair.buffer[i].which_config_content ==
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag)
    {
      _config_pair.buffer[i].content.content_int32 =
        configPair->buffer[i].content.content_int32;
    }
    else if(_config_pair.buffer[i].which_config_content ==
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
    {
      _config_pair.buffer[i].content.content_float =
        configPair->buffer[i].content.content_float;
    }
    else if(_config_pair.buffer[i].which_config_content ==
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
    {
      _config_pair.buffer[i].content.content_bool =
        configPair->buffer[i].content.content_bool;
    }
    else if(_config_pair.buffer[i].which_config_content ==
            SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
    {
      _config_pair.buffer[i].content.time.cnt =
        configPair->buffer[i].content.time.cnt;
      _config_pair.buffer[i].content.time.size =
        configPair->buffer[i].content.time.size;
      _config_pair.buffer[i].content.time.time =
        l_malloc((_config_pair.buffer[i].content.time.cnt) *
                 sizeof(_config_pair.buffer[i].content.time.time[0]));
      memcpy(_config_pair.buffer[i].content.time.time,
             configPair->buffer[i].content.time.time,
             (_config_pair.buffer[i].content.time.cnt) *
             sizeof(_config_pair.buffer[i].content.time.time[0]));
    }
    else if(_config_pair.buffer[i].which_config_content ==
            SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
    {
      _config_pair.buffer[i].content.work_mode.work_mode =
        configPair->buffer[i].content.work_mode.work_mode;
      _config_pair.buffer[i].content.work_mode.parametersLen =
        configPair->buffer[i].content.work_mode.parametersLen;
      _config_pair.buffer[i].content.work_mode.parameters =
        l_malloc(configPair->buffer[i].content.work_mode.parametersLen);
      memcpy(_config_pair.buffer[i].content.work_mode.parameters,
             configPair->buffer[i].content.work_mode.parameters,
             _config_pair.buffer[i].content.work_mode.parametersLen);
    }
    else if(_config_pair.buffer[i].which_config_content ==
            SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
    {
      _config_pair.buffer[i].content.gateway_info.name_id =
        configPair->buffer[i].content.gateway_info.name_id;
      _config_pair.buffer[i].content.gateway_info.mac_addrLen =
        configPair->buffer[i].content.gateway_info.mac_addrLen;
      _config_pair.buffer[i].content.gateway_info.mac_addr =
        l_malloc(configPair->buffer[i].content.gateway_info.mac_addrLen);
      memcpy(_config_pair.buffer[i].content.gateway_info.mac_addr,
             configPair->buffer[i].content.gateway_info.mac_addr,
             _config_pair.buffer[i].content.gateway_info.mac_addrLen);
    }
  }
  _config_pair.cnt = 0;
  _msg_config._messages.config_dissem.config_pair.arg =
    &_config_pair;
  _msg_config._messages.config_dissem.config_pair.funcs.encode =
    protobuf_enc_repeated_conf_pair_callback;

#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.config_dissem.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.config_dissem.has_is_big_endian = false;
  _msg_config._messages.config_dissem.is_big_endian = true;
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
  for(uint32_t i = 0; i < _config_pair.size; i++)
  {

    if(configPair->buffer[i].which_config_content ==
       SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
    {
      l_free(_config_pair.buffer[i].content.time.time);
      _config_pair.buffer[i].content.time.size = 0;
      _config_pair.buffer[i].content.time.cnt = 0;
    }
    else if(configPair->buffer[i].which_config_content ==
            SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
    {
      l_free(_config_pair.buffer[i].content.work_mode.parameters);
      _config_pair.buffer[i].content.work_mode.parametersLen = 0;
    }
    else if(configPair->buffer[i].which_config_content ==
            SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
    {
      l_free(_config_pair.buffer[i].content.gateway_info.mac_addr);
      _config_pair.buffer[i].content.gateway_info.mac_addrLen = 0;
    }
  }
  l_free(_config_pair.buffer);

  return rt;
}
/**
 * @brief Encode configuration retrieve message
 * @param frotoPrimitive To assign the Froto primitive
 * @param retrievePayload To indicate which configuration is to be
 * retrieved.
 * @param name_id The name id of the sensor with ID @p pt_sensorid_str .
 * @param has_name_id True if @p name_id is given.
 * @param specificConfigItem The specification configuration item (required
 * when @p retrievePayload is set to
 * SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG).
 * @param fSchedulerConfigItem The fScheduler configuration item (required
 * when @p retrievePayload is set to
 * SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG).
 * @param len The number of @p specificConfigItem or
 * @p fSchedulerConfigItem .
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
app_froto_n_encode_msg_config_retrieve(
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
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _msg_config = { 0 };
  uint32_t _seq_no;
  protobuf_codec_general_bytes_t _gw_id_info = { 0 };
  protobuf_codec_general_bytes_t _sensor_id_info = { 0 };
  protobuf_codec_config_item_t _config_item = { 0 };
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
  if(retrievePayload ==
     SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG)
  {
    if((len == 0) || (specificConfigItem == NULL))
    {
      // A valid specific config item should be given
      return false;
    }
  }
  else if(retrievePayload ==
          SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG)
  {
    if((len == 0) || (fSchedulerConfigItem == NULL))
    {
      // A valid fScheduler config item should be given
      return false;
    }
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
    SKFChina_App_AppMessage_config_retrieve_tag;

  // config_retrieve.header
  _msg_config._messages.config_retrieve.has_header = true;
  _msg_config._messages.config_retrieve.header.version = 1;
  _msg_config._messages.config_retrieve.header.is_up = false;
  _msg_config._messages.config_retrieve.header.message_seq_no = _seq_no;
  *used_seq_no =
    _msg_config._messages.config_retrieve.header.message_seq_no;
  _msg_config._messages.config_retrieve.header.time_to_live = 1; // Default
  _msg_config._messages.config_retrieve.header.primitive_type =
    frotoPrimitive;
  if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE)
  {
    _msg_config._messages.config_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive ==
          SKFChina_Froto_FrotoPmtType_RELIABLE_DISSEMINATE)
  {
    _msg_config._messages.config_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_RETRIVE)
  {
    _msg_config._messages.config_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_RETRIVE_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD)
  {
    _msg_config._messages.config_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_RELIABLE_UPLOAD)
  {
    _msg_config._messages.config_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else
  {
    _msg_config._messages.config_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;
  }
  *msgType = _msg_config._messages.config_retrieve.header.message_type;
  _msg_config._messages.config_retrieve.header.total_block = 1;
  _msg_config._messages.config_retrieve.header.has_ack_window_size_message
    = false;
  _msg_config._messages.config_retrieve.header.long_packet_id = _seq_no;
  _msg_config._messages.config_retrieve.header.has_current_block = false;
#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.config_retrieve.header.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.config_retrieve.header.has_is_big_endian = false;
  _msg_config._messages.config_retrieve.header.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  // Peer addr
  _msg_config._messages.config_retrieve.header.which_peer_addr =
    SKFChina_Froto_FrotoHeader_gateway_id_tag;
  _gw_id_info.buffer = l_malloc(pt_gwid_str_len);
  memcpy(_gw_id_info.buffer, pt_gwid_str, pt_gwid_str_len);
  _gw_id_info.size = pt_gwid_str_len;
  _gw_id_info.length = pt_gwid_str_len;
  _msg_config._messages.config_retrieve.header.peer_addr.gateway_id.arg =
    &_gw_id_info;
  _msg_config._messages.config_retrieve.header.peer_addr.gateway_id.funcs.
  encode = protobuf_enc_bytes_callback;
  // Sensor addr
  _sensor_id_info.buffer = l_malloc(pt_sensorid_str_len);
  memcpy(_sensor_id_info.buffer, pt_sensorid_str, pt_sensorid_str_len);
  _sensor_id_info.size = pt_sensorid_str_len;
  _sensor_id_info.length = pt_sensorid_str_len;
  _msg_config._messages.config_retrieve.header.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.config_retrieve.header.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // config_retrieve.appVer
  _msg_config._messages.config_retrieve.appVer = 1;

  // config_retrieve.sensor_id
  _msg_config._messages.config_retrieve.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.config_retrieve.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // config_retrieve.name_id
  if(_msg_config._messages.config_retrieve.has_name_id)
  {
    _msg_config._messages.config_retrieve.name_id = name_id;
  }

  // config_retrieve.payload
  _msg_config._messages.config_retrieve.payload = retrievePayload;

  // config_retrieve.specific_config_item or
  // config_retrieve.fscheduler_config_item
  if(retrievePayload ==
     SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG)
  {
    _config_item.size = len;
    _config_item.cnt = len;
    _config_item.buffer = l_malloc(len * sizeof(uint32_t));
    for(uint32_t i = 0; i < _config_item.size; i++)
    {
      _config_item.buffer[i] = specificConfigItem[i];
    }
    _msg_config._messages.config_retrieve.specific_config_item.arg =
      &_config_item;
    _msg_config._messages.config_retrieve.specific_config_item.funcs.encode
      =
        protobuf_enc_repeated_specific_conf_callback;
  }
  else if(retrievePayload ==
          SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG)
  {
    _config_item.size = len;
    _config_item.cnt = len;
    _config_item.buffer = l_malloc(len * sizeof(uint32_t));
    for(uint32_t i = 0; i < _config_item.size; i++)
    {
      _config_item.buffer[i] = fSchedulerConfigItem[i];
    }
    _msg_config._messages.config_retrieve.specific_config_item.arg =
      &_config_item;
    _msg_config._messages.config_retrieve.specific_config_item.funcs.encode
      =
        protobuf_enc_repeated_fscheduler_conf_callback;
  }

#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.config_retrieve.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.config_retrieve.has_is_big_endian = false;
  _msg_config._messages.config_retrieve.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */

  // TODO: Not support httpPostUrl

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
  if(_config_item.buffer)
  {
    l_free(_config_item.buffer);
    _config_item.buffer = NULL;
    _config_item.cnt = 0;
    _config_item.size = 0;
  }

  return rt;
}
