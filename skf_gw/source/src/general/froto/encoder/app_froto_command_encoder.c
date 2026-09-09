/**
 * @file    app_froto_command_encoder.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-11-14
 * @brief   Implementation of command related message (of Froto) encoder.
 * @details
 */
#include "sys_def.h"
#include "froto/app_froto_new.h"
#include "froto/app_froto_common.h"
#include "froto/encoder/app_froto_command_encoder.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

/**
 * @brief Encode configuration dissemination message
 * @param frotoPrimitive To assign the Froto primitive
 * @param commandPair Command pair. Only 1 command pair is supported.
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
app_froto_n_encode_msg_command_dissem(
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
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _msg_config = { 0 };
  uint32_t _seq_no;
  protobuf_codec_general_bytes_t _gw_id_info = { 0 };
  protobuf_codec_general_bytes_t _sensor_id_info = { 0 };
  protobuf_codec_command_pair_t _command_pair = { 0 };
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
  if(commandPair == NULL)
  {
    // Command pair should be given
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
    SKFChina_App_AppMessage_command_dissem_tag;

  // command_dissem.header
  _msg_config._messages.command_dissem.has_header = true;
  _msg_config._messages.command_dissem.header.version = 1;
  _msg_config._messages.command_dissem.header.is_up = false;
  _msg_config._messages.command_dissem.header.message_seq_no = _seq_no;
  *used_seq_no =
    _msg_config._messages.command_dissem.header.message_seq_no;
  _msg_config._messages.command_dissem.header.time_to_live = 1; // Default
  _msg_config._messages.command_dissem.header.primitive_type =
    frotoPrimitive;
  if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE)
  {
    _msg_config._messages.command_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive ==
          SKFChina_Froto_FrotoPmtType_RELIABLE_DISSEMINATE)
  {
    _msg_config._messages.command_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD)
  {
    _msg_config._messages.command_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_RELIABLE_UPLOAD)
  {
    _msg_config._messages.command_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else
  {
    _msg_config._messages.command_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;
  }
  *msgType = _msg_config._messages.command_dissem.header.message_type;
  _msg_config._messages.command_dissem.header.total_block = 1;
  _msg_config._messages.command_dissem.header.has_ack_window_size_message
    = false;
  _msg_config._messages.command_dissem.header.long_packet_id = _seq_no;
  _msg_config._messages.command_dissem.header.has_current_block = false;
#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.command_dissem.header.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.command_dissem.header.has_is_big_endian = false;
  _msg_config._messages.command_dissem.header.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  // Peer addr
  _msg_config._messages.command_dissem.header.which_peer_addr =
    SKFChina_Froto_FrotoHeader_gateway_id_tag;
  _gw_id_info.buffer = l_malloc(pt_gwid_str_len);
  memcpy(_gw_id_info.buffer, pt_gwid_str, pt_gwid_str_len);
  _gw_id_info.size = pt_gwid_str_len;
  _gw_id_info.length = pt_gwid_str_len;
  _msg_config._messages.command_dissem.header.peer_addr.gateway_id.arg =
    &_gw_id_info;
  _msg_config._messages.command_dissem.header.peer_addr.gateway_id.funcs.
  encode = protobuf_enc_bytes_callback;
  // Sensor addr
  _sensor_id_info.buffer = l_malloc(pt_sensorid_str_len);
  memcpy(_sensor_id_info.buffer, pt_sensorid_str, pt_sensorid_str_len);
  _sensor_id_info.size = pt_sensorid_str_len;
  _sensor_id_info.length = pt_sensorid_str_len;
  _msg_config._messages.command_dissem.header.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.command_dissem.header.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // command_dissem.appVer
  _msg_config._messages.command_dissem.appVer = 1;

  // command_dissem.sensor_id
  _msg_config._messages.command_dissem.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.command_dissem.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // command_dissem.product
  _msg_config._messages.command_dissem.product = productType;

  // command_dissem.command_pair
  _command_pair.command = commandPair->command;
  _msg_config._messages.command_dissem.has_command_pair = true;
  _msg_config._messages.command_dissem.command_pair.command =
    _command_pair.command;
  _command_pair.which_command_para =
    commandPair->which_command_para;
  if(_command_pair.which_command_para ==
     SKFChina_ConfigurationAndCommand_CommandPair_general_config_content_uint32_tag)
  {
    _command_pair.para.para_uint32 =
      commandPair->para.para_uint32;
    _msg_config._messages.command_dissem.command_pair.command_para.
    general_config_content_uint32 = _command_pair.para.para_uint32;
  }
  else if(_command_pair.which_command_para ==
          SKFChina_ConfigurationAndCommand_CommandPair_general_config_content_int32_tag)
  {
    _command_pair.para.para_int32 =
      commandPair->para.para_int32;
    _msg_config._messages.command_dissem.command_pair.command_para.
    general_config_content_int32 = _command_pair.para.para_int32;
  }
  else if(_command_pair.which_command_para ==
          SKFChina_ConfigurationAndCommand_CommandPair_general_config_content_float_tag)
  {
    _command_pair.para.para_float =
      commandPair->para.para_float;
    _msg_config._messages.command_dissem.command_pair.command_para.
    general_config_content_float = _command_pair.para.para_float;
  }
  else if(_command_pair.which_command_para ==
          SKFChina_ConfigurationAndCommand_CommandPair_time_config_content_tag)
  {
    _command_pair.para.time.cnt =
      commandPair->para.time.cnt;
    _command_pair.para.time.size =
      commandPair->para.time.size;
    _command_pair.para.time.time =
      l_malloc((_command_pair.para.time.cnt) *
               sizeof(_command_pair.para.time.time[0]));
    memcpy(_command_pair.para.time.time,
           commandPair->para.time.time,
           (_command_pair.para.time.cnt) *
           sizeof(_command_pair.para.time.time[0]));
    _command_pair.para.time.cnt = 0;
    _msg_config._messages.command_dissem.command_pair.command_para.
    time_config_content.time.arg = &_command_pair.para.time;
    _msg_config._messages.command_dissem.command_pair.command_para.
    time_config_content.time.funcs.encode = protobuf_enc_timearray_callback;
  }

#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.command_dissem.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.command_dissem.has_is_big_endian = false;
  _msg_config._messages.command_dissem.is_big_endian = true;
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
  if(commandPair->which_command_para ==
     SKFChina_ConfigurationAndCommand_CommandPair_time_config_content_tag)
  {
    l_free(_command_pair.para.time.time);
    _command_pair.para.time.size = 0;
    _command_pair.para.time.cnt = 0;
  }
  return rt;
}
