/**
 * @file    app_froto_fuota_encoder.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Implementation of FUOTA related message (of Froto) encoder.
 * @details
 */
#include "sys_def.h"
#include "froto/app_froto_new.h"
#include "froto/app_froto_common.h"
#include "froto/encoder/app_froto_fuota_encoder.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

/**
 * @brief Encode version retrieve message
 * @param frotoPrimitive To assign the Froto primitive
 * @param retrievePayload To indicate which FUOTA information is to be
 * retrieved.
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
app_froto_n_encode_msg_version_retrieve(
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
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _msg_config = { 0 };
  uint32_t _seq_no;
  protobuf_codec_general_bytes_t _gw_id_info = { 0 };
  protobuf_codec_general_bytes_t _sensor_id_info = { 0 };
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
    SKFChina_App_AppMessage_version_retrieve_tag;

  // version_retrieve.header
  _msg_config._messages.version_retrieve.has_header = true;
  _msg_config._messages.version_retrieve.header.version = 1;
  _msg_config._messages.version_retrieve.header.is_up = false;
  _msg_config._messages.version_retrieve.header.message_seq_no = _seq_no;
  *used_seq_no =
    _msg_config._messages.version_retrieve.header.message_seq_no;
  _msg_config._messages.version_retrieve.header.time_to_live = 1; // Default
  _msg_config._messages.version_retrieve.header.primitive_type =
    frotoPrimitive;
  if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE)
  {
    _msg_config._messages.version_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive ==
          SKFChina_Froto_FrotoPmtType_RELIABLE_DISSEMINATE)
  {
    _msg_config._messages.version_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_RETRIVE)
  {
    _msg_config._messages.version_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_RETRIVE_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD)
  {
    _msg_config._messages.version_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_RELIABLE_UPLOAD)
  {
    _msg_config._messages.version_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else
  {
    _msg_config._messages.version_retrieve.header.message_type =
      SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;
  }
  *msgType = _msg_config._messages.version_retrieve.header.message_type;
  _msg_config._messages.version_retrieve.header.total_block = 1;
  _msg_config._messages.version_retrieve.header.has_ack_window_size_message
    = false;
  _msg_config._messages.version_retrieve.header.long_packet_id = _seq_no;
  _msg_config._messages.version_retrieve.header.has_current_block = false;
#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.version_retrieve.header.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.version_retrieve.header.has_is_big_endian = false;
  _msg_config._messages.version_retrieve.header.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  // Peer addr
  _msg_config._messages.version_retrieve.header.which_peer_addr =
    SKFChina_Froto_FrotoHeader_gateway_id_tag;
  _gw_id_info.buffer = l_malloc(pt_gwid_str_len);
  memcpy(_gw_id_info.buffer, pt_gwid_str, pt_gwid_str_len);
  _gw_id_info.size = pt_gwid_str_len;
  _gw_id_info.length = pt_gwid_str_len;
  _msg_config._messages.version_retrieve.header.peer_addr.gateway_id.arg =
    &_gw_id_info;
  _msg_config._messages.version_retrieve.header.peer_addr.gateway_id.funcs.
  encode = protobuf_enc_bytes_callback;
  // Sensor addr
  _sensor_id_info.buffer = l_malloc(pt_sensorid_str_len);
  memcpy(_sensor_id_info.buffer, pt_sensorid_str, pt_sensorid_str_len);
  _sensor_id_info.size = pt_sensorid_str_len;
  _sensor_id_info.length = pt_sensorid_str_len;
  _msg_config._messages.version_retrieve.header.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.version_retrieve.header.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // version_retrieve.appVer
  _msg_config._messages.version_retrieve.appVer = 1;

  // version_retrieve.sensor_id
  _msg_config._messages.version_retrieve.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.version_retrieve.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // version_retrieve.payload
  _msg_config._messages.version_retrieve.payload = retrievePayload;

#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.version_retrieve.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.version_retrieve.has_is_big_endian = false;
  _msg_config._messages.version_retrieve.is_big_endian = true;
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

  return rt;
}
/**
 * @brief Encode FUOTA notification dissemination message
 * @param frotoPrimitive To assign the Froto primitive
 * @param fuotaTaskId The FUOTA task ID
 * @param hardwareType The target hardware
 * @param hardwareVer The hardware version of to-be-updated target devices
 * @param firmwareVer The to-be-updated firmware version
 * @param forceFuota Force to perform the FUOTA no matter the version is
 * later than the current one if true
 * @param updateMethod The update communication method (e.g., BLE or LTE)
 * @param updateType The update type: diff or whole
 * @param whichFuotaMethod Detailed FUOTA method information (MQTT, HTTP,
 * or over BLE)
 * @param url The HTTP or MQTT URL. Cannot be NULL if @p whichFuotaMethod
 * is MQTT or HTTP
 * @param urlLen Length (in byte) of @p url . Cannot be 0 if
 * @p whichFuotaMethod is MQTT or HTTP
 * @param topic The MQTT topic. Cannot be NULL if @p whichFuotaMethod is
 * MQTT
 * @param topicLen Length (in byte) of @p topic . Cannot be 0 if
 * @p whichFuotaMethod is MQTT
 * @param isBLEUart Valid if @p whichFuotaMethod is BLE. To indicate
 * whether UART over BLE is used.
 * @param whichCheckValue To indicate which check value is used for the
 * image.
 * @param checkValue The check value.
 * @param checkValueLen The length (in byte) of check value.
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
app_froto_n_encode_msg_fuota_notify_dissem(
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
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _msg_config = { 0 };
  uint32_t _seq_no;
  protobuf_codec_general_bytes_t _gw_id_info = { 0 };
  protobuf_codec_general_bytes_t _sensor_id_info = { 0 };
  protobuf_codec_general_bytes_t _http_url = { 0 };
  protobuf_codec_general_bytes_t _mqtt_url = { 0 };
  protobuf_codec_general_bytes_t _mqtt_topic = { 0 };
  protobuf_codec_general_bytes_t _check_value = { 0 };

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
  if(whichFuotaMethod ==
     SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_http_tag)
  {
    if((urlLen == 0) || (url == NULL))
    {
      // url should be given
      return false;
    }
  }
  else if(whichFuotaMethod ==
          SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_froto_over_mqtt_tag)
  {
    if((urlLen == 0) || (url == NULL))
    {
      // url should be given
      return false;
    }
    if((topicLen == 0) || (topic == NULL))
    {
      // mqtt topic should be given
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
    SKFChina_App_AppMessage_fuota_notify_dissem_tag;

  // fuota_notify_dissem.header
  _msg_config._messages.fuota_notify_dissem.has_header = true;
  _msg_config._messages.fuota_notify_dissem.header.version = 1;
  _msg_config._messages.fuota_notify_dissem.header.is_up = false;
  _msg_config._messages.fuota_notify_dissem.header.message_seq_no =
    _seq_no;
  *used_seq_no =
    _msg_config._messages.fuota_notify_dissem.header.message_seq_no;
  _msg_config._messages.fuota_notify_dissem.header.time_to_live = 1; // Default
  _msg_config._messages.fuota_notify_dissem.header.primitive_type =
    frotoPrimitive;
  if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE)
  {
    _msg_config._messages.fuota_notify_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive ==
          SKFChina_Froto_FrotoPmtType_RELIABLE_DISSEMINATE)
  {
    _msg_config._messages.fuota_notify_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD)
  {
    _msg_config._messages.fuota_notify_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_RELIABLE_UPLOAD)
  {
    _msg_config._messages.fuota_notify_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else
  {
    _msg_config._messages.fuota_notify_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;
  }
  *msgType = _msg_config._messages.fuota_notify_dissem.header.message_type;
  _msg_config._messages.fuota_notify_dissem.header.total_block = 1;
  _msg_config._messages.fuota_notify_dissem.header.
  has_ack_window_size_message
    = false;
  _msg_config._messages.fuota_notify_dissem.header.long_packet_id =
    _seq_no;
  _msg_config._messages.fuota_notify_dissem.header.has_current_block =
    false;
#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.fuota_notify_dissem.header.has_is_big_endian =
    false;
#else
  // Big endian
  _msg_config._messages.fuota_notify_dissem.header.has_is_big_endian =
    false;
  _msg_config._messages.fuota_notify_dissem.header.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  // Peer addr
  _msg_config._messages.fuota_notify_dissem.header.which_peer_addr =
    SKFChina_Froto_FrotoHeader_gateway_id_tag;
  _gw_id_info.buffer = l_malloc(pt_gwid_str_len);
  memcpy(_gw_id_info.buffer, pt_gwid_str, pt_gwid_str_len);
  _gw_id_info.size = pt_gwid_str_len;
  _gw_id_info.length = pt_gwid_str_len;
  _msg_config._messages.fuota_notify_dissem.header.peer_addr.gateway_id.arg
    =
      &_gw_id_info;
  _msg_config._messages.fuota_notify_dissem.header.peer_addr.gateway_id.
  funcs.
  encode = protobuf_enc_bytes_callback;
  // Sensor addr
  _sensor_id_info.buffer = l_malloc(pt_sensorid_str_len);
  memcpy(_sensor_id_info.buffer, pt_sensorid_str, pt_sensorid_str_len);
  _sensor_id_info.size = pt_sensorid_str_len;
  _sensor_id_info.length = pt_sensorid_str_len;
  _msg_config._messages.fuota_notify_dissem.header.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.fuota_notify_dissem.header.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // fuota_notify_dissem.appVer
  _msg_config._messages.fuota_notify_dissem.appVer = 1;

  // fuota_notify_dissem.sensor_id
  _msg_config._messages.fuota_notify_dissem.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.fuota_notify_dissem.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // fuota_notify_dissem.fuota_task_id
  _msg_config._messages.fuota_notify_dissem.fuota_task_id = fuotaTaskId;

  // fuota_notify_dissem.target_hardware_type
  _msg_config._messages.fuota_notify_dissem.target_hardware_type =
    hardwareType;

  // fuota_notify_dissem.target_hardware_version
  _msg_config._messages.fuota_notify_dissem.target_hardware_version =
    hardwareVer;

  // fuota_notify_dissem.firmware_version
  _msg_config._messages.fuota_notify_dissem.firmware_version = firmwareVer;

  // fuota_notify_dissem.force_fuota
  _msg_config._messages.fuota_notify_dissem.force_fuota = forceFuota;

  // fuota_notify_dissem.update_method
  _msg_config._messages.fuota_notify_dissem.update_method = updateMethod;

  // fuota_notify_dissem.update_type
  _msg_config._messages.fuota_notify_dissem.update_type = updateType;

  // fuota_notify_dissem.update_type
  _msg_config._messages.fuota_notify_dissem.which_fuota_method =
    whichFuotaMethod;
  if(whichFuotaMethod ==
     SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_http_tag)
  {

    _http_url.buffer = l_malloc(urlLen);
    memcpy(_http_url.buffer, url, urlLen);
    _http_url.size = urlLen;
    _http_url.length = urlLen;
    _msg_config._messages.fuota_notify_dissem.fuota_method.fuota_with_http.
    url.arg = &_http_url;
    _msg_config._messages.fuota_notify_dissem.fuota_method.fuota_with_http.
    url.funcs.encode = protobuf_enc_bytes_callback;
  }
  else if(whichFuotaMethod ==
          SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_froto_over_mqtt_tag)
  {
    _mqtt_url.buffer = l_malloc(urlLen);
    memcpy(_mqtt_url.buffer, url, urlLen);
    _mqtt_url.size = urlLen;
    _mqtt_url.length = urlLen;
    _msg_config._messages.fuota_notify_dissem.fuota_method.
    fuota_with_froto_over_mqtt.url.arg = &_mqtt_url;
    _msg_config._messages.fuota_notify_dissem.fuota_method.
    fuota_with_froto_over_mqtt.url.funcs.encode =
      protobuf_enc_bytes_callback;
    _mqtt_topic.buffer = l_malloc(topicLen);
    memcpy(_mqtt_topic.buffer, topic, topicLen);
    _mqtt_topic.size = topicLen;
    _mqtt_topic.length = topicLen;
    _msg_config._messages.fuota_notify_dissem.fuota_method.
    fuota_with_froto_over_mqtt.mqtt_topic.arg = &_mqtt_topic;
    _msg_config._messages.fuota_notify_dissem.fuota_method.
    fuota_with_froto_over_mqtt.mqtt_topic.funcs.encode =
      protobuf_enc_bytes_callback;
  }
  else if(whichFuotaMethod ==
          SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_froto_over_ble_ots_tag)
  {
    _msg_config._messages.fuota_notify_dissem.fuota_method.
    fuota_with_froto_over_ble_ots.isBLEUart = isBLEUart;
  }

  // TODO: fuota_notify_dissem.supplementary_info
  // TODO: fuota_notify_dissem.build_id
  _msg_config._messages.fuota_notify_dissem.has_build_id = false;

  // fuota_notify_dissem.check_value
  _msg_config._messages.fuota_notify_dissem.which_check_value =
    whichCheckValue;
  if(whichCheckValue ==
     SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_elf_hash_value_tag)
  {
    _msg_config._messages.fuota_notify_dissem.check_value.elf_hash_value =
      *((uint32_t *)checkValue);
  }
  else if(whichCheckValue ==
          SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_sha1_value_tag)
  {
    _check_value.buffer = l_malloc(checkValueLen);
    memcpy(_check_value.buffer, checkValue, checkValueLen);
    _check_value.length = checkValueLen;
    _check_value.size = checkValueLen;
    _msg_config._messages.fuota_notify_dissem.check_value.sha1_value.arg =
      &_check_value;
    _msg_config._messages.fuota_notify_dissem.check_value.sha1_value.funcs.
    encode = protobuf_enc_bytes_callback;
  }
  else if(whichCheckValue ==
          SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_md5_value_tag)
  {
    _check_value.buffer = l_malloc(checkValueLen);
    memcpy(_check_value.buffer, checkValue, checkValueLen);
    _check_value.length = checkValueLen;
    _check_value.size = checkValueLen;
    _msg_config._messages.fuota_notify_dissem.check_value.md5_value.arg =
      &_check_value;
    _msg_config._messages.fuota_notify_dissem.check_value.md5_value.funcs.
    encode = protobuf_enc_bytes_callback;
  }

  // fuota_notify_dissem.encryp
  _msg_config._messages.fuota_notify_dissem.has_encryp = true;
  _msg_config._messages.fuota_notify_dissem.encryp = encryp;

#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.fuota_notify_dissem.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.fuota_notify_dissem.has_is_big_endian = false;
  _msg_config._messages.fuota_notify_dissem.is_big_endian = true;
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
  if(whichFuotaMethod ==
     SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_http_tag)
  {
    l_free(_http_url.buffer);
    _http_url.size = 0;
    _http_url.length = 0;
  }
  else if(whichFuotaMethod ==
          SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_froto_over_mqtt_tag)
  {
    l_free(_mqtt_url.buffer);
    _mqtt_url.size = 0;
    _mqtt_url.length = 0;
    l_free(_mqtt_topic.buffer);
    _mqtt_topic.size = 0;
    _mqtt_topic.length = 0;
  }

  if(whichCheckValue ==
     SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_sha1_value_tag)
  {
    l_free(_check_value.buffer);
    _check_value.size = 0;
    _check_value.length = 0;
  }
  else if(whichCheckValue ==
          SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_md5_value_tag)
  {
    l_free(_check_value.buffer);
    _check_value.size = 0;
    _check_value.length = 0;
  }

  return rt;
}
/**
 * @brief Encode FUOTA image dissemination message
 * @param frotoPrimitive To assign the Froto primitive (simple disseminate
 * or reliable disseminate)
 * @param offset Offset (in byte) of the start position
 * @param is_fuota_task To indicate the disseminated content is a FUOTA if
 * true.
 * @param task_id FUOTA task ID or file task ID.
 * @param content The content
 * @param contentLen Length (in byte) of the content
 * @param hash The hash value of the content
 * @param totalBlock Total block
 * @param currentBlock The current block (starting from 0)
 * @param longPktId The long packet ID (should be the seq number of packet
 * with the first block). Only valid when @p currentBlock is not zero.
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
app_froto_n_encode_msg_image_block_dissem(
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
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _msg_config = { 0 };
  uint32_t _seq_no;
  protobuf_codec_general_bytes_t _gw_id_info = { 0 };
  protobuf_codec_general_bytes_t _sensor_id_info = { 0 };
  protobuf_codec_general_bytes_t _content = { 0 };

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
  if((contentLen == 0) || (content == NULL))
  {
    // Content should be given
    return false;
  }
  if((totalBlock == 0) || (currentBlock >= totalBlock))
  {
    // Invalid
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
    SKFChina_App_AppMessage_image_block_dissem_tag;

  // image_block_dissem.header
  _msg_config._messages.image_block_dissem.has_header = true;
  _msg_config._messages.image_block_dissem.header.version = 1;
  _msg_config._messages.image_block_dissem.header.is_up = false;
  _msg_config._messages.image_block_dissem.header.message_seq_no =
    _seq_no;
  *used_seq_no =
    _msg_config._messages.image_block_dissem.header.message_seq_no;
  _msg_config._messages.image_block_dissem.header.time_to_live = 1; // Default
  _msg_config._messages.image_block_dissem.header.primitive_type =
    frotoPrimitive;
  if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE)
  {
    _msg_config._messages.image_block_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive ==
          SKFChina_Froto_FrotoPmtType_RELIABLE_DISSEMINATE)
  {
    _msg_config._messages.image_block_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD)
  {
    _msg_config._messages.image_block_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_RELIABLE_UPLOAD)
  {
    _msg_config._messages.image_block_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else
  {
    _msg_config._messages.image_block_dissem.header.message_type =
      SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;
  }
  *msgType = _msg_config._messages.image_block_dissem.header.message_type;
  _msg_config._messages.image_block_dissem.header.total_block = totalBlock;
  _msg_config._messages.image_block_dissem.header.
  has_ack_window_size_message
    = false;
  if(currentBlock == 0)
  {
    _msg_config._messages.image_block_dissem.header.long_packet_id =
      _seq_no;
  }
  else
  {
    _msg_config._messages.image_block_dissem.header.long_packet_id =
      longPktId;
  }
  if(totalBlock == 1)
  {
    _msg_config._messages.image_block_dissem.header.has_current_block =
      false;
  }
  else
  {
    _msg_config._messages.image_block_dissem.header.has_current_block =
      true;
    _msg_config._messages.image_block_dissem.header.current_block =
      currentBlock;
  }
#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.image_block_dissem.header.has_is_big_endian =
    false;
#else
  // Big endian
  _msg_config._messages.image_block_dissem.header.has_is_big_endian =
    false;
  _msg_config._messages.image_block_dissem.header.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  // Peer addr
  _msg_config._messages.image_block_dissem.header.which_peer_addr =
    SKFChina_Froto_FrotoHeader_gateway_id_tag;
  _gw_id_info.buffer = l_malloc(pt_gwid_str_len);
  memcpy(_gw_id_info.buffer, pt_gwid_str, pt_gwid_str_len);
  _gw_id_info.size = pt_gwid_str_len;
  _gw_id_info.length = pt_gwid_str_len;
  _msg_config._messages.image_block_dissem.header.peer_addr.gateway_id.arg
    =
      &_gw_id_info;
  _msg_config._messages.image_block_dissem.header.peer_addr.gateway_id.
  funcs.
  encode = protobuf_enc_bytes_callback;
  // Sensor addr
  _sensor_id_info.buffer = l_malloc(pt_sensorid_str_len);
  memcpy(_sensor_id_info.buffer, pt_sensorid_str, pt_sensorid_str_len);
  _sensor_id_info.size = pt_sensorid_str_len;
  _sensor_id_info.length = pt_sensorid_str_len;
  _msg_config._messages.image_block_dissem.header.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.image_block_dissem.header.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // image_block_dissem.appVer
  _msg_config._messages.image_block_dissem.appVer = 1;

  // image_block_dissem.sensor_id
  _msg_config._messages.image_block_dissem.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.image_block_dissem.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // image_block_dissem.offset
  _msg_config._messages.image_block_dissem.offset = offset;

  // image_block_dissem.task_id
  if(is_fuota_task)
  {
    _msg_config._messages.image_block_dissem.which_task_id =
      SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_fuota_task_id_tag;
    _msg_config._messages.image_block_dissem.task_id.fuota_task_id =
      task_id;
  }
  else
  {
    _msg_config._messages.image_block_dissem.which_task_id =
      SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_file_task_id_tag;
    _msg_config._messages.image_block_dissem.task_id.file_task_id =
      task_id;
  }

  // image_block_dissem.image_content
  _content.buffer = content;
  _content.length = contentLen;
  _content.size = contentLen;
  _msg_config._messages.image_block_dissem.image_content.arg = &_content;
  _msg_config._messages.image_block_dissem.image_content.funcs.encode =
    protobuf_enc_bytes_callback;

  // image_block_dissem.hash_value
  _msg_config._messages.image_block_dissem.hash_value = hash;

#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.fuota_notify_dissem.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.fuota_notify_dissem.has_is_big_endian = false;
  _msg_config._messages.fuota_notify_dissem.is_big_endian = true;
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

  return rt;
}
/**
 * @brief Encode FUOTA image upload message
 * @param frotoPrimitive To assign the Froto primitive (simple disseminate
 * or reliable disseminate)
 * @param offset Offset (in byte) of the start position
 * @param is_fuota_task To indicate the disseminated content is a FUOTA if
 * true.
 * @param task_id FUOTA task ID or file task ID.
 * @param content The content
 * @param contentLen Length (in byte) of the content
 * @param hash The hash value of the content
 * @param totalBlock Total block
 * @param currentBlock The current block (starting from 0)
 * @param longPktId The long packet ID (should be the seq number of packet
 * with the first block). Only valid when @p currentBlock is not zero.
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
app_froto_n_encode_msg_image_block_upload(
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
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _msg_config = { 0 };
  uint32_t _seq_no;
  protobuf_codec_general_bytes_t _gw_id_info = { 0 };
  protobuf_codec_general_bytes_t _sensor_id_info = { 0 };
  protobuf_codec_general_bytes_t _content = { 0 };

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
  if((contentLen == 0) || (content == NULL))
  {
    // Content should be given
    return false;
  }
  if((totalBlock == 0) || (currentBlock >= totalBlock))
  {
    // Invalid
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
    SKFChina_App_AppMessage_image_block_upload_tag;

  // image_block_upload.header
  _msg_config._messages.image_block_upload.has_header = true;
  _msg_config._messages.image_block_upload.header.version = 1;
  _msg_config._messages.image_block_upload.header.is_up = false;
  _msg_config._messages.image_block_upload.header.message_seq_no =
    _seq_no;
  *used_seq_no =
    _msg_config._messages.image_block_upload.header.message_seq_no;
  _msg_config._messages.image_block_upload.header.time_to_live = 1; // Default
  _msg_config._messages.image_block_upload.header.primitive_type =
    frotoPrimitive;
  if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE)
  {
    _msg_config._messages.image_block_upload.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive ==
          SKFChina_Froto_FrotoPmtType_RELIABLE_DISSEMINATE)
  {
    _msg_config._messages.image_block_upload.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD)
  {
    _msg_config._messages.image_block_upload.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_RELIABLE_UPLOAD)
  {
    _msg_config._messages.image_block_upload.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else
  {
    _msg_config._messages.image_block_upload.header.message_type =
      SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;
  }
  *msgType = _msg_config._messages.image_block_upload.header.message_type;
  _msg_config._messages.image_block_upload.header.total_block = totalBlock;
  _msg_config._messages.image_block_upload.header.
  has_ack_window_size_message
    = false;
  if(currentBlock == 0)
  {
    _msg_config._messages.image_block_upload.header.long_packet_id =
      _seq_no;
  }
  else
  {
    _msg_config._messages.image_block_upload.header.long_packet_id =
      longPktId;
  }
  if(totalBlock == 1)
  {
    _msg_config._messages.image_block_upload.header.has_current_block =
      false;
  }
  else
  {
    _msg_config._messages.image_block_upload.header.has_current_block =
      true;
    _msg_config._messages.image_block_upload.header.current_block =
      currentBlock;
  }
#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.image_block_upload.header.has_is_big_endian =
    false;
#else
  // Big endian
  _msg_config._messages.image_block_upload.header.has_is_big_endian =
    false;
  _msg_config._messages.image_block_upload.header.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  // Peer addr
  _msg_config._messages.image_block_upload.header.which_peer_addr =
    SKFChina_Froto_FrotoHeader_gateway_id_tag;
  _gw_id_info.buffer = l_malloc(pt_gwid_str_len);
  memcpy(_gw_id_info.buffer, pt_gwid_str, pt_gwid_str_len);
  _gw_id_info.size = pt_gwid_str_len;
  _gw_id_info.length = pt_gwid_str_len;
  _msg_config._messages.image_block_upload.header.peer_addr.gateway_id.arg
    =
      &_gw_id_info;
  _msg_config._messages.image_block_upload.header.peer_addr.gateway_id.
  funcs.
  encode = protobuf_enc_bytes_callback;
  // Sensor addr
  _sensor_id_info.buffer = l_malloc(pt_sensorid_str_len);
  memcpy(_sensor_id_info.buffer, pt_sensorid_str, pt_sensorid_str_len);
  _sensor_id_info.size = pt_sensorid_str_len;
  _sensor_id_info.length = pt_sensorid_str_len;
  _msg_config._messages.image_block_upload.header.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.image_block_upload.header.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // image_block_upload.appVer
  _msg_config._messages.image_block_upload.appVer = 1;

  // image_block_upload.sensor_id
  _msg_config._messages.image_block_upload.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.image_block_upload.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // image_block_upload.offset
  _msg_config._messages.image_block_upload.offset = offset;

  // image_block_upload.task_id
  if(is_fuota_task)
  {
    _msg_config._messages.image_block_upload.which_task_id =
      SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_fuota_task_id_tag;
    _msg_config._messages.image_block_upload.task_id.fuota_task_id =
      task_id;
  }
  else
  {
    _msg_config._messages.image_block_upload.which_task_id =
      SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_file_task_id_tag;
    _msg_config._messages.image_block_upload.task_id.file_task_id =
      task_id;
  }

  // image_block_upload.image_content
  _content.buffer = content;
  _content.length = contentLen;
  _content.size = contentLen;
  _msg_config._messages.image_block_upload.image_content.arg = &_content;
  _msg_config._messages.image_block_upload.image_content.funcs.encode =
    protobuf_enc_bytes_callback;

  // image_block_upload.hash_value
  _msg_config._messages.image_block_upload.hash_value = hash;

#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.image_block_upload.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.image_block_upload.has_is_big_endian = false;
  _msg_config._messages.image_block_upload.is_big_endian = true;
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

  return rt;
}
/**
 * @brief Encode image block request message
 * @param frotoPrimitive To assign the Froto primitive (simple disseminate,
 * reliable disseminate, or simple retrieve)
 * @param isFuota True if it is a FUOTA block request
 * @param taskId The task ID
 * @param offset The offset of the requested block
 * @param size The requested block size
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
app_froto_n_encode_msg_image_block_request(
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
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _msg_config = { 0 };
  uint32_t _seq_no;
  protobuf_codec_general_bytes_t _gw_id_info = { 0 };
  protobuf_codec_general_bytes_t _sensor_id_info = { 0 };
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
    SKFChina_App_AppMessage_image_block_request_tag;

  // image_block_request.header
  _msg_config._messages.image_block_request.has_header = true;
  _msg_config._messages.image_block_request.header.version = 1;
  _msg_config._messages.image_block_request.header.is_up = false;
  _msg_config._messages.image_block_request.header.message_seq_no =
    _seq_no;
  *used_seq_no =
    _msg_config._messages.image_block_request.header.message_seq_no;
  _msg_config._messages.image_block_request.header.time_to_live = 1; // Default
  _msg_config._messages.image_block_request.header.primitive_type =
    frotoPrimitive;
  if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE)
  {
    _msg_config._messages.image_block_request.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive ==
          SKFChina_Froto_FrotoPmtType_RELIABLE_DISSEMINATE)
  {
    _msg_config._messages.image_block_request.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_RETRIVE)
  {
    _msg_config._messages.image_block_request.header.message_type =
      SKFChina_Froto_FrotoMsgType_RETRIVE_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD)
  {
    _msg_config._messages.image_block_request.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_RELIABLE_UPLOAD)
  {
    _msg_config._messages.image_block_request.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else
  {
    _msg_config._messages.image_block_request.header.message_type =
      SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;
  }
  *msgType = _msg_config._messages.image_block_request.header.message_type;
  _msg_config._messages.image_block_request.header.total_block = 1;
  _msg_config._messages.image_block_request.header.
  has_ack_window_size_message
    = false;
  _msg_config._messages.image_block_request.header.long_packet_id =
    _seq_no;
  _msg_config._messages.image_block_request.header.has_current_block =
    false;
#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.image_block_request.header.has_is_big_endian =
    false;
#else
  // Big endian
  _msg_config._messages.image_block_request.header.has_is_big_endian =
    false;
  _msg_config._messages.image_block_request.header.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  // Peer addr
  _msg_config._messages.image_block_request.header.which_peer_addr =
    SKFChina_Froto_FrotoHeader_gateway_id_tag;
  _gw_id_info.buffer = l_malloc(pt_gwid_str_len);
  memcpy(_gw_id_info.buffer, pt_gwid_str, pt_gwid_str_len);
  _gw_id_info.size = pt_gwid_str_len;
  _gw_id_info.length = pt_gwid_str_len;
  _msg_config._messages.image_block_request.header.peer_addr.gateway_id.arg
    =
      &_gw_id_info;
  _msg_config._messages.image_block_request.header.peer_addr.gateway_id.
  funcs.
  encode = protobuf_enc_bytes_callback;
  // Sensor addr
  _sensor_id_info.buffer = l_malloc(pt_sensorid_str_len);
  memcpy(_sensor_id_info.buffer, pt_sensorid_str, pt_sensorid_str_len);
  _sensor_id_info.size = pt_sensorid_str_len;
  _sensor_id_info.length = pt_sensorid_str_len;
  _msg_config._messages.image_block_request.header.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.image_block_request.header.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // image_block_request.appVer
  _msg_config._messages.image_block_request.appVer = 1;

  // image_block_request.sensor_id
  _msg_config._messages.image_block_request.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.image_block_request.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // image_block_request.which_task_id
  if(isFuota == true)
  {
    _msg_config._messages.image_block_request.which_task_id =
      SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_fuota_task_id_tag;
    _msg_config._messages.image_block_request.task_id.fuota_task_id =
      taskId;
  }
  else
  {
    _msg_config._messages.image_block_request.which_task_id =
      SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_file_task_id_tag;
    _msg_config._messages.image_block_request.task_id.file_task_id =
      taskId;
  }
  _msg_config._messages.image_block_request.offset = offset;
  _msg_config._messages.image_block_request.size = size;

#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.image_block_request.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.image_block_request.has_is_big_endian = false;
  _msg_config._messages.image_block_request.is_big_endian = true;
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

  return rt;
}
/**
 * @brief Encode file notification dissemination message
 * @param frotoPrimitive To assign the Froto primitive (simple disseminate
 * or reliable disseminate)
 * @param taskId The task ID
 * @param updateMethod The update communication method (e.g., BLE or LTE)
 * @param fileType The file type (e.g., gwConfig or senorConfig, etc.)
 * @param whichFuotaMethod Detailed FUOTA method information (MQTT, HTTP,
 * or over BLE)
 * @param url The HTTP or MQTT URL. Cannot be NULL if @p whichFuotaMethod
 * is MQTT or HTTP
 * @param urlLen Length (in byte) of @p url . Cannot be 0 if
 * @p whichFuotaMethod is MQTT or HTTP
 * @param topic The MQTT topic. Cannot be NULL if @p whichFuotaMethod is
 * MQTT
 * @param topicLen Length (in byte) of @p topic . Cannot be 0 if
 * @p whichFuotaMethod is MQTT
 * @param isBLEUart Valid if @p whichFuotaMethod is BLE. To indicate
 * whether UART over BLE is used.
 * @param whichCheckValue To indicate which check value is used for the
 * image.
 * @param checkValue The check value.
 * @param checkValueLen The length (in byte) of check value.
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
app_froto_n_encode_msg_file_notify_upload(
  SKFChina_Froto_FrotoPmtType frotoPrimitive,
  uint32_t taskId,
  SKFChina_Common_CommunicationType updateMethod,
  SKFChina_Common_FileType fileType,
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
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _msg_config = { 0 };
  uint32_t _seq_no;
  protobuf_codec_general_bytes_t _gw_id_info = { 0 };
  protobuf_codec_general_bytes_t _sensor_id_info = { 0 };
  protobuf_codec_general_bytes_t _http_url = { 0 };
  protobuf_codec_general_bytes_t _mqtt_url = { 0 };
  protobuf_codec_general_bytes_t _mqtt_topic = { 0 };
  protobuf_codec_general_bytes_t _check_value = { 0 };

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
  if(whichFuotaMethod ==
     SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_http_tag)
  {
    if((urlLen == 0) || (url == NULL))
    {
      // url should be given
      return false;
    }
  }
  else if(whichFuotaMethod ==
          SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_froto_over_mqtt_tag)
  {
    if((urlLen == 0) || (url == NULL))
    {
      // url should be given
      return false;
    }
    if((topicLen == 0) || (topic == NULL))
    {
      // mqtt topic should be given
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
    SKFChina_App_AppMessage_file_notify_upload_tag;

  // file_notify_upload.header
  _msg_config._messages.file_notify_upload.has_header = true;
  _msg_config._messages.file_notify_upload.header.version = 1;
  _msg_config._messages.file_notify_upload.header.is_up = false;
  _msg_config._messages.file_notify_upload.header.message_seq_no =
    _seq_no;
  *used_seq_no =
    _msg_config._messages.file_notify_upload.header.message_seq_no;
  _msg_config._messages.file_notify_upload.header.time_to_live = 1; // Default
  _msg_config._messages.file_notify_upload.header.primitive_type =
    frotoPrimitive;
  if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE)
  {
    _msg_config._messages.file_notify_upload.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive ==
          SKFChina_Froto_FrotoPmtType_RELIABLE_DISSEMINATE)
  {
    _msg_config._messages.file_notify_upload.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD)
  {
    _msg_config._messages.file_notify_upload.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else if(frotoPrimitive == SKFChina_Froto_FrotoPmtType_RELIABLE_UPLOAD)
  {
    _msg_config._messages.file_notify_upload.header.message_type =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
  }
  else
  {
    _msg_config._messages.file_notify_upload.header.message_type =
      SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;
  }
  *msgType = _msg_config._messages.file_notify_upload.header.message_type;
  _msg_config._messages.file_notify_upload.header.total_block = 1;
  _msg_config._messages.file_notify_upload.header.
  has_ack_window_size_message
    = false;
  _msg_config._messages.file_notify_upload.header.long_packet_id =
    _seq_no;
  _msg_config._messages.file_notify_upload.header.has_current_block =
    false;
#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.file_notify_upload.header.has_is_big_endian =
    false;
#else
  // Big endian
  _msg_config._messages.file_notify_upload.header.has_is_big_endian =
    false;
  _msg_config._messages.file_notify_upload.header.is_big_endian = true;
#endif /* FROTO_USE_LITTLE_ENDIAN == 1 */
  // Peer addr
  _msg_config._messages.file_notify_upload.header.which_peer_addr =
    SKFChina_Froto_FrotoHeader_gateway_id_tag;
  _gw_id_info.buffer = l_malloc(pt_gwid_str_len);
  memcpy(_gw_id_info.buffer, pt_gwid_str, pt_gwid_str_len);
  _gw_id_info.size = pt_gwid_str_len;
  _gw_id_info.length = pt_gwid_str_len;
  _msg_config._messages.file_notify_upload.header.peer_addr.gateway_id.arg
    =
      &_gw_id_info;
  _msg_config._messages.file_notify_upload.header.peer_addr.gateway_id.
  funcs.
  encode = protobuf_enc_bytes_callback;
  // Sensor addr
  _sensor_id_info.buffer = l_malloc(pt_sensorid_str_len);
  memcpy(_sensor_id_info.buffer, pt_sensorid_str, pt_sensorid_str_len);
  _sensor_id_info.size = pt_sensorid_str_len;
  _sensor_id_info.length = pt_sensorid_str_len;
  _msg_config._messages.file_notify_upload.header.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.file_notify_upload.header.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // file_notify_upload.appVer
  _msg_config._messages.file_notify_upload.appVer = 1;

  // file_notify_upload.sensor_id
  _msg_config._messages.file_notify_upload.sensor_id.arg =
    &_sensor_id_info;
  _msg_config._messages.file_notify_upload.sensor_id.funcs.encode =
    protobuf_enc_bytes_callback;

  // file_notify_upload.task_id
  _msg_config._messages.file_notify_upload.task_id = taskId;

  // file_notify_upload.file_type
  _msg_config._messages.file_notify_upload.file_type = fileType;

  // file_notify_upload.update_method
  _msg_config._messages.file_notify_upload.update_method = updateMethod;

  // file_notify_upload.which_fuota_method
  _msg_config._messages.file_notify_upload.which_fuota_method =
    SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_froto_over_ble_ots_tag;
  _msg_config._messages.file_notify_upload.fuota_method.
  fuota_with_froto_over_ble_ots.isBLEUart = true;
  // file_notify_upload.update_type
  _msg_config._messages.file_notify_upload.which_fuota_method =
    whichFuotaMethod;
  if(whichFuotaMethod ==
     SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_http_tag)
  {
    _http_url.buffer = l_malloc(urlLen);
    memcpy(_http_url.buffer, url, urlLen);
    _http_url.size = urlLen;
    _http_url.length = urlLen;
    _msg_config._messages.file_notify_upload.fuota_method.fuota_with_http.
    url.arg = &_http_url;
    _msg_config._messages.file_notify_upload.fuota_method.fuota_with_http.
    url.funcs.encode = protobuf_enc_bytes_callback;
  }
  else if(whichFuotaMethod ==
          SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_froto_over_mqtt_tag)
  {
    _mqtt_url.buffer = l_malloc(urlLen);
    memcpy(_mqtt_url.buffer, url, urlLen);
    _mqtt_url.size = urlLen;
    _mqtt_url.length = urlLen;
    _msg_config._messages.file_notify_upload.fuota_method.
    fuota_with_froto_over_mqtt.url.arg = &_mqtt_url;
    _msg_config._messages.file_notify_upload.fuota_method.
    fuota_with_froto_over_mqtt.url.funcs.encode =
      protobuf_enc_bytes_callback;
    _mqtt_topic.buffer = l_malloc(topicLen);
    memcpy(_mqtt_topic.buffer, topic, topicLen);
    _mqtt_topic.size = topicLen;
    _mqtt_topic.length = topicLen;
    _msg_config._messages.file_notify_upload.fuota_method.
    fuota_with_froto_over_mqtt.mqtt_topic.arg = &_mqtt_topic;
    _msg_config._messages.file_notify_upload.fuota_method.
    fuota_with_froto_over_mqtt.mqtt_topic.funcs.encode =
      protobuf_enc_bytes_callback;
  }
  else if(whichFuotaMethod ==
          SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_froto_over_ble_ots_tag)
  {
    _msg_config._messages.file_notify_upload.fuota_method.
    fuota_with_froto_over_ble_ots.isBLEUart = isBLEUart;
  }

  // file_notify_upload.which_check_value
  _msg_config._messages.file_notify_upload.which_check_value =
    whichCheckValue;
  if(whichCheckValue ==
     SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_elf_hash_value_tag)
  {
    _msg_config._messages.file_notify_upload.check_value.elf_hash_value =
      *((uint32_t *)checkValue);
  }
  else if(whichCheckValue ==
          SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_sha1_value_tag)
  {
    _check_value.buffer = l_malloc(checkValueLen);
    memcpy(_check_value.buffer, checkValue, checkValueLen);
    _check_value.length = checkValueLen;
    _check_value.size = checkValueLen;
    _msg_config._messages.file_notify_upload.check_value.sha1_value.arg =
      &_check_value;
    _msg_config._messages.file_notify_upload.check_value.sha1_value.funcs.
    encode = protobuf_enc_bytes_callback;
  }
  else if(whichCheckValue ==
          SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_md5_value_tag)
  {
    _check_value.buffer = l_malloc(checkValueLen);
    memcpy(_check_value.buffer, checkValue, checkValueLen);
    _check_value.length = checkValueLen;
    _check_value.size = checkValueLen;
    _msg_config._messages.file_notify_upload.check_value.md5_value.arg =
      &_check_value;
    _msg_config._messages.file_notify_upload.check_value.md5_value.funcs.
    encode = protobuf_enc_bytes_callback;
  }

  // file_notify_upload.encryp
  _msg_config._messages.file_notify_upload.has_encryp = true;
  _msg_config._messages.file_notify_upload.encryp = encryp;

#if (FROTO_USE_LITTLE_ENDIAN == 1)
  // Little endian
  _msg_config._messages.file_notify_upload.has_is_big_endian = false;
#else
  // Big endian
  _msg_config._messages.file_notify_upload.has_is_big_endian = false;
  _msg_config._messages.file_notify_upload.is_big_endian = true;
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
  if(whichFuotaMethod ==
     SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_http_tag)
  {
    l_free(_http_url.buffer);
    _http_url.size = 0;
    _http_url.length = 0;
  }
  else if(whichFuotaMethod ==
          SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_froto_over_mqtt_tag)
  {
    l_free(_mqtt_url.buffer);
    _mqtt_url.size = 0;
    _mqtt_url.length = 0;
    l_free(_mqtt_topic.buffer);
    _mqtt_topic.size = 0;
    _mqtt_topic.length = 0;
  }

  if(whichCheckValue ==
     SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_sha1_value_tag)
  {
    l_free(_check_value.buffer);
    _check_value.size = 0;
    _check_value.length = 0;
  }
  else if(whichCheckValue ==
          SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_md5_value_tag)
  {
    l_free(_check_value.buffer);
    _check_value.size = 0;
    _check_value.length = 0;
  }

  return rt;
}
