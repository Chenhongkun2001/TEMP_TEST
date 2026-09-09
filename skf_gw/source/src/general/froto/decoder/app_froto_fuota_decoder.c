/**
 * @file    app_froto_fuota_decoder.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Implementation of fuota related message (of Froto) decoder.
 * @details
 */
#include "sys_def.h"
#include "froto/app_froto_new.h"
#include "froto/app_froto_common.h"
#include "froto/decoder/app_froto_fuota_decoder.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

/**
 * @brief Parse current version upload message. Note that only one message
 * would be parsed if there are multiple messages in the input buffer
 * @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param current_version_upload_processor A callback function to deal with
 * the parsed data parsing (e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_current_version_upload_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t current_version_upload_processor,
  uint8_t *errCode)
{
  bool _rt = true;
  pb_istream_t _substream;
  pb_wire_type_t _wire_type;
  uint32_t _tag;
  bool _eof;
  uint8_t _errCode;
  protobuf_codec_general_bytes_t _sensor_id;
  protobuf_codec_general_bytes_t _sensor_id_1;
  protobuf_codec_general_bytes_t _gateway_id;
  protobuf_codec_acked_seq_number_t _acked_seq_no_array;
  protobuf_codec_current_version_t _version;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint8_t _sensor_id_1_buf[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];

  *Froto_header =
    &(SKF_Froto_App->_messages.current_version_upload.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_current_version_upload_tag))
    {
      pb_make_string_substream(istream, &_substream);
      _sensor_id.buffer = _peer_id;
      _sensor_id.size = sizeof(_peer_id);
      _sensor_id.length = 0;
      (*Froto_header)->sensor_id.arg = &_sensor_id;
      (*Froto_header)->sensor_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _gateway_id.buffer = _self_id;
      _gateway_id.size = sizeof(_self_id);
      _gateway_id.length = 0;
      (*Froto_header)->peer_addr.gateway_id.arg = &_gateway_id;
      (*Froto_header)->peer_addr.gateway_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _acked_seq_no_array.buffer = _acked_seq_no;
      _acked_seq_no_array.size = sizeof(_acked_seq_no) /
                                 sizeof(_acked_seq_no[0]);
      _acked_seq_no_array.cnt = 0;
      (*Froto_header)->acked_message_seq_number.arg =
        &_acked_seq_no_array;
      (*Froto_header)->acked_message_seq_number.funcs.decode =
        &protobuf_dec_acked_seq_number_callback;

      _sensor_id_1.buffer = _sensor_id_1_buf;
      _sensor_id_1.size = sizeof(_sensor_id_1_buf);
      _sensor_id_1.length = 0;
      SKF_Froto_App->_messages.current_version_upload.sensor_id.arg =
        &_sensor_id_1;
      SKF_Froto_App->_messages.current_version_upload.sensor_id.funcs.
      decode =
        &protobuf_dec_bytes_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_FirmwareUpdateOverTheAir_CurrentVersionUpload_fields,
                           &SKF_Froto_App->_messages.current_version_upload))
      {
        DBG_LOG_INFO("Froto APP current_version_upload appVer: %d",
                      SKF_Froto_App->_messages.current_version_upload.
                      appVer);
        DBG_LOG_DEBUG("Froto APP current_version_upload has header: %d",
                      SKF_Froto_App->_messages.current_version_upload.
                      has_header);
        if(SKF_Froto_App->_messages.current_version_upload.has_header ==
           true)
        {
          DBG_LOG_INFO(
            "Froto APP current_version_upload header version: %d",
            (*Froto_header)->version);
          DBG_LOG_DEBUG(
            "Froto APP current_version_upload header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_INFO(
            "Froto APP current_version_upload header which_peer_addr: %d",
            (*Froto_header)->which_peer_addr);
          // This is a bug to deal with "bytes" in "oneof"
          // which_config_content would be set to zero when decoding
          // "bytes" in "oneof"
          // So we here, cannot determin the peer addr based on
          // which_peer_addr
          // if((*Froto_header)->which_peer_addr ==
          //    SKFChina_Froto_FrotoHeader_gateway_id_tag)
          {
            DBG_LOG_DEBUG(
              "Froto APP current_version_upload header gateway id:");
            util_dbg_buf_dump(
              ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                  peer_addr.
                                                  gateway_id.
                                                  arg))->buffer,
              ((protobuf_codec_general_bytes_t *)(
                 (*Froto_header)->peer_addr.gateway_id.arg))->length);
          }
          // else
          // {
          //   DBG_LOG_WARN("The peer addr type is not supported");
          // }
          DBG_LOG_DEBUG(
            "Froto APP current_version_upload header is up: %d",
            (*Froto_header)->is_up);
          DBG_LOG_INFO(
            "Froto APP current_version_upload header msg seq no: %d",
            (*Froto_header)->message_seq_no);
          DBG_LOG_DEBUG("Froto APP current_version_upload header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG(
            "Froto APP current_version_upload header primitive type: %d",
            (*Froto_header)->primitive_type);
          DBG_LOG_INFO(
            "Froto APP current_version_upload header message type: %d",
            (*Froto_header)->message_type);
          DBG_LOG_INFO(
            "Froto APP current_version_upload header total block: %d",
            (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP current_version_upload header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_INFO(
              "Froto APP current_version_upload header current block: %d",
              (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG(
            "Froto APP current_version_upload header long packet id: %d",
            (*Froto_header)->long_packet_id);
          DBG_LOG_DEBUG(
            "Froto APP current_version_upload header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG(
              "Froto APP current_version_upload header is big endian: %d",
              (*Froto_header)->is_big_endian);
          }
          DBG_LOG_DEBUG(
            "Froto APP current_version_upload header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
        }
        DBG_LOG_DEBUG(
          "Froto APP current_version_upload has is big endian: %d",
          SKF_Froto_App->_messages.current_version_upload.
          has_is_big_endian);
        if(SKF_Froto_App->_messages.current_version_upload.
           has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG(
            "Froto APP current_version_upload is big endian: %d",
            SKF_Froto_App->_messages.current_version_upload.
            is_big_endian);
        }
        DBG_LOG_DEBUG(
          "Froto APP current_version_upload sensor id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              current_version_upload.
                                              sensor_id.
                                              arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              current_version_upload.
                                              sensor_id.
                                              arg))->length);
        _version.sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              current_version_upload.
                                              sensor_id.
                                              arg))->length;
        if(_version.sensorIDLen)
        {
          _version.sensorID = l_malloc(_version.sensorIDLen);
          memcpy(_version.sensorID,
                 ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->
                                                     _messages.
                                                     current_version_upload
                                                     .
                                                     sensor_id.arg))->
                 buffer,
                 _version.sensorIDLen);
        }
        else
        {
          _version.sensorID = NULL;
        }
        _version.hardware_type =
          SKF_Froto_App->_messages.current_version_upload.hardware_type;
        DBG_LOG_INFO(
          "Froto APP current_version_upload hardware type: %d",
          _version.hardware_type);
        _version.hardware_version =
          SKF_Froto_App->_messages.current_version_upload.hardware_version;
        DBG_LOG_INFO(
          "Froto APP current_version_upload hardware version: %d",
          _version.hardware_version);
        _version.firmware_version =
          SKF_Froto_App->_messages.current_version_upload.firmware_version;
        DBG_LOG_INFO(
          "Froto APP current_version_upload firmware version: %d",
          _version.firmware_version);
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(current_version_upload_processor != NULL)
  {
    current_version_upload_processor(&_version, _errCode);
  }
  l_free(_version.sensorID);
  _version.sensorID = NULL;
  _version.sensorIDLen = 0;

  return _rt;
}
/**
 * @brief Parse current version upload message. Note that only one message
 * would be parsed if there are multiple messages in the input buffer
 * @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param fuota_status_processor A callback function to deal with
 * the parsed data parsing (e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_fuota_status_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t fuota_status_processor,
  uint8_t *errCode)
{
  bool _rt = true;
  pb_istream_t _substream;
  pb_wire_type_t _wire_type;
  uint32_t _tag;
  bool _eof;
  uint8_t _errCode;
  protobuf_codec_general_bytes_t _sensor_id;
  protobuf_codec_general_bytes_t _sensor_id_1;
  protobuf_codec_general_bytes_t _gateway_id;
  protobuf_codec_acked_seq_number_t _acked_seq_no_array;
  protobuf_codec_fuota_status_t _status;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint8_t _sensor_id_1_buf[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];

  *Froto_header = &(SKF_Froto_App->_messages.fuota_status.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_fuota_status_tag))
    {
      pb_make_string_substream(istream, &_substream);
      _sensor_id.buffer = _peer_id;
      _sensor_id.size = sizeof(_peer_id);
      _sensor_id.length = 0;
      (*Froto_header)->sensor_id.arg = &_sensor_id;
      (*Froto_header)->sensor_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _gateway_id.buffer = _self_id;
      _gateway_id.size = sizeof(_self_id);
      _gateway_id.length = 0;
      (*Froto_header)->peer_addr.gateway_id.arg = &_gateway_id;
      (*Froto_header)->peer_addr.gateway_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _acked_seq_no_array.buffer = _acked_seq_no;
      _acked_seq_no_array.size = sizeof(_acked_seq_no) /
                                 sizeof(_acked_seq_no[0]);
      _acked_seq_no_array.cnt = 0;
      (*Froto_header)->acked_message_seq_number.arg =
        &_acked_seq_no_array;
      (*Froto_header)->acked_message_seq_number.funcs.decode =
        &protobuf_dec_acked_seq_number_callback;

      _sensor_id_1.buffer = _sensor_id_1_buf;
      _sensor_id_1.size = sizeof(_sensor_id_1_buf);
      _sensor_id_1.length = 0;
      SKF_Froto_App->_messages.fuota_status.sensor_id.arg =
        &_sensor_id_1;
      SKF_Froto_App->_messages.fuota_status.sensor_id.funcs.
      decode =
        &protobuf_dec_bytes_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_FirmwareUpdateOverTheAir_UpdateStatusUpload_fields,
                           &SKF_Froto_App->_messages.fuota_status))
      {
        DBG_LOG_INFO("Froto APP fuota_status appVer: %d",
                      SKF_Froto_App->_messages.fuota_status.
                      appVer);
        DBG_LOG_DEBUG("Froto APP fuota_status has header: %d",
                      SKF_Froto_App->_messages.fuota_status.
                      has_header);
        if(SKF_Froto_App->_messages.fuota_status.has_header ==
           true)
        {
          DBG_LOG_DEBUG(
            "Froto APP fuota_status header version: %d",
            (*Froto_header)->version);
          DBG_LOG_DEBUG(
            "Froto APP fuota_status header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_INFO(
            "Froto APP fuota_status header which_peer_addr: %d",
            (*Froto_header)->which_peer_addr);
          // This is a bug to deal with "bytes" in "oneof"
          // which_config_content would be set to zero when decoding
          // "bytes" in "oneof"
          // So we here, cannot determin the peer addr based on
          // which_peer_addr
          // if((*Froto_header)->which_peer_addr ==
          //    SKFChina_Froto_FrotoHeader_gateway_id_tag)
          {
            DBG_LOG_DEBUG(
              "Froto APP fuota_status header gateway id:");
            util_dbg_buf_dump(
              ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                  peer_addr.
                                                  gateway_id.
                                                  arg))->buffer,
              ((protobuf_codec_general_bytes_t *)(
                 (*Froto_header)->peer_addr.gateway_id.arg))->length);
          }
          // else
          // {
          //   DBG_LOG_WARN("The peer addr type is not supported");
          // }
          DBG_LOG_DEBUG(
            "Froto APP fuota_status header is up: %d",
            (*Froto_header)->is_up);
          DBG_LOG_INFO(
            "Froto APP fuota_status header msg seq no: %d",
            (*Froto_header)->message_seq_no);
          DBG_LOG_DEBUG("Froto APP fuota_status header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG(
            "Froto APP fuota_status header primitive type: %d",
            (*Froto_header)->primitive_type);
          DBG_LOG_INFO(
            "Froto APP fuota_status header message type: %d",
            (*Froto_header)->message_type);
          DBG_LOG_INFO(
            "Froto APP fuota_status header total block: %d",
            (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP fuota_status header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_INFO(
              "Froto APP fuota_status header current block: %d",
              (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG(
            "Froto APP fuota_status header long packet id: %d",
            (*Froto_header)->long_packet_id);
          DBG_LOG_DEBUG(
            "Froto APP fuota_status header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG(
              "Froto APP fuota_status header is big endian: %d",
              (*Froto_header)->is_big_endian);
          }
          DBG_LOG_DEBUG(
            "Froto APP fuota_status header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
        }
        DBG_LOG_DEBUG(
          "Froto APP fuota_status has is big endian: %d",
          SKF_Froto_App->_messages.fuota_status.
          has_is_big_endian);
        if(SKF_Froto_App->_messages.fuota_status.
           has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG(
            "Froto APP fuota_status is big endian: %d",
            SKF_Froto_App->_messages.fuota_status.
            is_big_endian);
        }
        DBG_LOG_DEBUG(
          "Froto APP fuota_status sensor id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              fuota_status.
                                              sensor_id.
                                              arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              fuota_status.
                                              sensor_id.
                                              arg))->length);
        _status.sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              fuota_status.
                                              sensor_id.
                                              arg))->length;
        if(_status.sensorIDLen)
        {
          _status.sensorID = l_malloc(_status.sensorIDLen);
          memcpy(_status.sensorID,
                 ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->
                                                     _messages.
                                                     fuota_status
                                                     .
                                                     sensor_id.arg))->
                 buffer,
                 _status.sensorIDLen);
        }
        else
        {
          _status.sensorID = NULL;
        }

        _status.task_id =
          SKF_Froto_App->_messages.fuota_status.fuota_task_id;
        DBG_LOG_DEBUG(
          "Froto APP fuota_status fuota_task_id: %d", _status.task_id);
        _status.status =
          SKF_Froto_App->_messages.fuota_status.update_status;
        DBG_LOG_INFO(
          "Froto APP fuota_status update_status: %d", _status.status);
        _status.receivedBytes =
          SKF_Froto_App->_messages.fuota_status.received_bytes;
        DBG_LOG_INFO(
          "Froto APP fuota_status received_bytes: %d",
          _status.receivedBytes);
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(fuota_status_processor != NULL)
  {
    fuota_status_processor(&_status, _errCode);
  }
  l_free(_status.sensorID);
  _status.sensorID = NULL;
  _status.sensorIDLen = 0;

  return _rt;
}
/**
 * @brief Parse image block request message. Note that only one message
 * would be parsed if there are multiple messages in the input buffer
 * @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param image_block_request_processor A callback function to deal with
 * the parsed data parsing (e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_image_block_request_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t image_block_request_processor,
  uint8_t *errCode)
{
  bool _rt = true;
  pb_istream_t _substream;
  pb_wire_type_t _wire_type;
  uint32_t _tag;
  bool _eof;
  uint8_t _errCode;
  protobuf_codec_general_bytes_t _sensor_id;
  protobuf_codec_general_bytes_t _sensor_id_1;
  protobuf_codec_general_bytes_t _gateway_id;
  protobuf_codec_acked_seq_number_t _acked_seq_no_array;
  protobuf_codec_image_block_request_status_t _request;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint8_t _sensor_id_1_buf[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];

  *Froto_header = &(SKF_Froto_App->_messages.image_block_request.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_image_block_request_tag))
    {
      pb_make_string_substream(istream, &_substream);
      _sensor_id.buffer = _peer_id;
      _sensor_id.size = sizeof(_peer_id);
      _sensor_id.length = 0;
      (*Froto_header)->sensor_id.arg = &_sensor_id;
      (*Froto_header)->sensor_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _gateway_id.buffer = _self_id;
      _gateway_id.size = sizeof(_self_id);
      _gateway_id.length = 0;
      (*Froto_header)->peer_addr.gateway_id.arg = &_gateway_id;
      (*Froto_header)->peer_addr.gateway_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _acked_seq_no_array.buffer = _acked_seq_no;
      _acked_seq_no_array.size = sizeof(_acked_seq_no) /
                                 sizeof(_acked_seq_no[0]);
      _acked_seq_no_array.cnt = 0;
      (*Froto_header)->acked_message_seq_number.arg =
        &_acked_seq_no_array;
      (*Froto_header)->acked_message_seq_number.funcs.decode =
        &protobuf_dec_acked_seq_number_callback;

      _sensor_id_1.buffer = _sensor_id_1_buf;
      _sensor_id_1.size = sizeof(_sensor_id_1_buf);
      _sensor_id_1.length = 0;
      SKF_Froto_App->_messages.image_block_request.sensor_id.arg =
        &_sensor_id_1;
      SKF_Froto_App->_messages.image_block_request.sensor_id.funcs.
      decode =
        &protobuf_dec_bytes_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_fields,
                           &SKF_Froto_App->_messages.image_block_request))
      {
        DBG_LOG_INFO("Froto APP image_block_request appVer: %d",
                      SKF_Froto_App->_messages.image_block_request.
                      appVer);
        DBG_LOG_DEBUG("Froto APP image_block_request has header: %d",
                      SKF_Froto_App->_messages.image_block_request.
                      has_header);
        if(SKF_Froto_App->_messages.image_block_request.has_header ==
           true)
        {
          DBG_LOG_INFO(
            "Froto APP image_block_request header version: %d",
            (*Froto_header)->version);
          DBG_LOG_DEBUG(
            "Froto APP image_block_request header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_INFO(
            "Froto APP image_block_request header which_peer_addr: %d",
            (*Froto_header)->which_peer_addr);
          // This is a bug to deal with "bytes" in "oneof"
          // which_config_content would be set to zero when decoding
          // "bytes" in "oneof"
          // So we here, cannot determin the peer addr based on
          // which_peer_addr
          // if((*Froto_header)->which_peer_addr ==
          //    SKFChina_Froto_FrotoHeader_gateway_id_tag)
          {
            DBG_LOG_DEBUG(
              "Froto APP image_block_request header gateway id:");
            util_dbg_buf_dump(
              ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                  peer_addr.
                                                  gateway_id.
                                                  arg))->buffer,
              ((protobuf_codec_general_bytes_t *)(
                 (*Froto_header)->peer_addr.gateway_id.arg))->length);
          }
          // else
          // {
          //   DBG_LOG_WARN("The peer addr type is not supported");
          // }
          DBG_LOG_DEBUG(
            "Froto APP image_block_request header is up: %d",
            (*Froto_header)->is_up);
          DBG_LOG_INFO(
            "Froto APP image_block_request header msg seq no: %d",
            (*Froto_header)->message_seq_no);
          DBG_LOG_DEBUG("Froto APP image_block_request header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG(
            "Froto APP image_block_request header primitive type: %d",
            (*Froto_header)->primitive_type);
          DBG_LOG_INFO(
            "Froto APP image_block_request header message type: %d",
            (*Froto_header)->message_type);
          DBG_LOG_INFO(
            "Froto APP image_block_request header total block: %d",
            (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP image_block_request header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_INFO(
              "Froto APP image_block_request header current block: %d",
              (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG(
            "Froto APP image_block_request header long packet id: %d",
            (*Froto_header)->long_packet_id);
          DBG_LOG_DEBUG(
            "Froto APP image_block_request header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG(
              "Froto APP image_block_request header is big endian: %d",
              (*Froto_header)->is_big_endian);
          }
          DBG_LOG_DEBUG(
            "Froto APP image_block_request header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
        }
        DBG_LOG_DEBUG(
          "Froto APP image_block_request has is big endian: %d",
          SKF_Froto_App->_messages.image_block_request.
          has_is_big_endian);
        if(SKF_Froto_App->_messages.image_block_request.
           has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG(
            "Froto APP image_block_request is big endian: %d",
            SKF_Froto_App->_messages.image_block_request.
            is_big_endian);
        }
        DBG_LOG_DEBUG(
          "Froto APP image_block_request sensor id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_request.
                                              sensor_id.
                                              arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_request.
                                              sensor_id.
                                              arg))->length);
        _request.sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_request.
                                              sensor_id.
                                              arg))->length;
        if(_request.sensorIDLen)
        {
          _request.sensorID = l_malloc(_request.sensorIDLen);
          memcpy(_request.sensorID,
                 ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->
                                                     _messages.
                                                     image_block_request
                                                     .
                                                     sensor_id.arg))->
                 buffer,
                 _request.sensorIDLen);
        }
        else
        {
          _request.sensorID = NULL;
        }

        if(SKF_Froto_App->_messages.image_block_request.which_task_id ==
           SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_fuota_task_id_tag)
        {
          _request.is_fuota_task = true;
          _request.task_id =
            SKF_Froto_App->_messages.image_block_request.task_id.
            fuota_task_id;
        }
        else
        {
          _request.is_fuota_task = false;
          _request.task_id =
            SKF_Froto_App->_messages.image_block_request.task_id.
            file_task_id;
        }
        DBG_LOG_DEBUG(
          "Froto APP image_block_request is_fuota_task: %d",
          _request.is_fuota_task);
        DBG_LOG_DEBUG(
          "Froto APP image_block_request task id: %d",
          _request.task_id);

        _request.offset =
          SKF_Froto_App->_messages.image_block_request.offset;
        DBG_LOG_INFO(
          "Froto APP image_block_request offset: %d", _request.offset);
        _request.size =
          SKF_Froto_App->_messages.image_block_request.size;
        DBG_LOG_INFO(
          "Froto APP image_block_request size: %d", _request.size);
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(image_block_request_processor != NULL)
  {
    image_block_request_processor(&_request, _errCode);
  }
  l_free(_request.sensorID);
  _request.sensorID = NULL;
  _request.sensorIDLen = 0;

  return _rt;
}
/**
 * @brief Parse image block retrieve message. Note that only one message
 * would be parsed if there are multiple messages in the input buffer
 * @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param image_block_retrieve_processor A callback function to deal with
 * the parsed data parsing (e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_image_block_retrieve_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t image_block_retrieve_processor,
  uint8_t *errCode)
{
  bool _rt = true;
  pb_istream_t _substream;
  pb_wire_type_t _wire_type;
  uint32_t _tag;
  bool _eof;
  uint8_t _errCode;
  protobuf_codec_general_bytes_t _sensor_id;
  protobuf_codec_general_bytes_t _sensor_id_1;
  protobuf_codec_general_bytes_t _gateway_id;
  protobuf_codec_acked_seq_number_t _acked_seq_no_array;
  protobuf_codec_image_block_retrieve_status_t _retrieve;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint8_t _sensor_id_1_buf[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];

  *Froto_header = &(SKF_Froto_App->_messages.image_block_retrieve.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_image_block_retrieve_tag))
    {
      pb_make_string_substream(istream, &_substream);
      _sensor_id.buffer = _peer_id;
      _sensor_id.size = sizeof(_peer_id);
      _sensor_id.length = 0;
      (*Froto_header)->sensor_id.arg = &_sensor_id;
      (*Froto_header)->sensor_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _gateway_id.buffer = _self_id;
      _gateway_id.size = sizeof(_self_id);
      _gateway_id.length = 0;
      (*Froto_header)->peer_addr.gateway_id.arg = &_gateway_id;
      (*Froto_header)->peer_addr.gateway_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _acked_seq_no_array.buffer = _acked_seq_no;
      _acked_seq_no_array.size = sizeof(_acked_seq_no) /
                                 sizeof(_acked_seq_no[0]);
      _acked_seq_no_array.cnt = 0;
      (*Froto_header)->acked_message_seq_number.arg =
        &_acked_seq_no_array;
      (*Froto_header)->acked_message_seq_number.funcs.decode =
        &protobuf_dec_acked_seq_number_callback;

      _sensor_id_1.buffer = _sensor_id_1_buf;
      _sensor_id_1.size = sizeof(_sensor_id_1_buf);
      _sensor_id_1.length = 0;
      SKF_Froto_App->_messages.image_block_retrieve.sensor_id.arg =
        &_sensor_id_1;
      SKF_Froto_App->_messages.image_block_retrieve.sensor_id.funcs.
      decode =
        &protobuf_dec_bytes_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_fields,
                           &SKF_Froto_App->_messages.image_block_retrieve))
      {
        DBG_LOG_INFO("Froto APP image_block_retrieve appVer: %d",
                      SKF_Froto_App->_messages.image_block_retrieve.
                      appVer);
        DBG_LOG_DEBUG("Froto APP image_block_retrieve has header: %d",
                      SKF_Froto_App->_messages.image_block_retrieve.
                      has_header);
        if(SKF_Froto_App->_messages.image_block_retrieve.has_header ==
           true)
        {
          DBG_LOG_INFO(
            "Froto APP image_block_retrieve header version: %d",
            (*Froto_header)->version);
          DBG_LOG_DEBUG(
            "Froto APP image_block_retrieve header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_INFO(
            "Froto APP image_block_retrieve header which_peer_addr: %d",
            (*Froto_header)->which_peer_addr);
          // This is a bug to deal with "bytes" in "oneof"
          // which_config_content would be set to zero when decoding
          // "bytes" in "oneof"
          // So we here, cannot determin the peer addr based on
          // which_peer_addr
          // if((*Froto_header)->which_peer_addr ==
          //    SKFChina_Froto_FrotoHeader_gateway_id_tag)
          {
            DBG_LOG_DEBUG(
              "Froto APP image_block_retrieve header gateway id:");
            util_dbg_buf_dump(
              ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                  peer_addr.
                                                  gateway_id.
                                                  arg))->buffer,
              ((protobuf_codec_general_bytes_t *)(
                 (*Froto_header)->peer_addr.gateway_id.arg))->length);
          }
          // else
          // {
          //   DBG_LOG_WARN("The peer addr type is not supported");
          // }
          DBG_LOG_DEBUG(
            "Froto APP image_block_retrieve header is up: %d",
            (*Froto_header)->is_up);
          DBG_LOG_INFO(
            "Froto APP image_block_retrieve header msg seq no: %d",
            (*Froto_header)->message_seq_no);
          DBG_LOG_DEBUG("Froto APP image_block_retrieve header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG(
            "Froto APP image_block_retrieve header primitive type: %d",
            (*Froto_header)->primitive_type);
          DBG_LOG_INFO(
            "Froto APP image_block_retrieve header message type: %d",
            (*Froto_header)->message_type);
          DBG_LOG_INFO(
            "Froto APP image_block_retrieve header total block: %d",
            (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP image_block_retrieve header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_INFO(
              "Froto APP image_block_retrieve header current block: %d",
              (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG(
            "Froto APP image_block_retrieve header long packet id: %d",
            (*Froto_header)->long_packet_id);
          DBG_LOG_DEBUG(
            "Froto APP image_block_retrieve header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG(
              "Froto APP image_block_retrieve header is big endian: %d",
              (*Froto_header)->is_big_endian);
          }
          DBG_LOG_DEBUG(
            "Froto APP image_block_retrieve header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
        }
        DBG_LOG_DEBUG(
          "Froto APP image_block_retrieve has is big endian: %d",
          SKF_Froto_App->_messages.image_block_retrieve.
          has_is_big_endian);
        if(SKF_Froto_App->_messages.image_block_retrieve.
           has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG(
            "Froto APP image_block_retrieve is big endian: %d",
            SKF_Froto_App->_messages.image_block_retrieve.
            is_big_endian);
        }
        DBG_LOG_DEBUG(
          "Froto APP image_block_retrieve sensor id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_retrieve.
                                              sensor_id.
                                              arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_retrieve.
                                              sensor_id.
                                              arg))->length);
        _retrieve.sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_retrieve.
                                              sensor_id.
                                              arg))->length;
        if(_retrieve.sensorIDLen)
        {
          _retrieve.sensorID = l_malloc(_retrieve.sensorIDLen);
          memcpy(_retrieve.sensorID,
                 ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->
                                                     _messages.
                                                     image_block_retrieve
                                                     .
                                                     sensor_id.arg))->
                 buffer,
                 _retrieve.sensorIDLen);
        }
        else
        {
          _retrieve.sensorID = NULL;
        }

        if(SKF_Froto_App->_messages.image_block_retrieve.which_task_id ==
           SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_fuota_task_id_tag)
        {
          _retrieve.is_fuota_task = true;
          _retrieve.task_id =
            SKF_Froto_App->_messages.image_block_retrieve.task_id.
            fuota_task_id;
        }
        else
        {
          _retrieve.is_fuota_task = false;
          _retrieve.task_id =
            SKF_Froto_App->_messages.image_block_retrieve.task_id.
            file_task_id;
        }
        DBG_LOG_DEBUG(
          "Froto APP image_block_retrieve is_fuota_task: %d",
          _retrieve.is_fuota_task);
        DBG_LOG_DEBUG(
          "Froto APP image_block_retrieve task id: %d",
          _retrieve.task_id);

        _retrieve.offset =
          SKF_Froto_App->_messages.image_block_retrieve.offset;
        DBG_LOG_INFO(
          "Froto APP image_block_retrieve offset: %d", _retrieve.offset);
        _retrieve.size =
          SKF_Froto_App->_messages.image_block_retrieve.size;
        DBG_LOG_INFO(
          "Froto APP image_block_retrieve size: %d", _retrieve.size);
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(image_block_retrieve_processor != NULL)
  {
    image_block_retrieve_processor(&_retrieve, _errCode);
  }
  l_free(_retrieve.sensorID);
  _retrieve.sensorID = NULL;
  _retrieve.sensorIDLen = 0;

  return _rt;
}
/**
 * @brief Parse file notification dissemination message. Note that only one
 * message would be parsed if there are multiple messages in the input
 * buffer @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param file_notify_dissem_processor A callback function to deal with
 * the parsed data parsing (e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_file_notify_dissem_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t file_notify_dissem_processor,
  uint8_t *errCode)
{
  bool _rt = true;
  pb_istream_t _substream;
  pb_wire_type_t _wire_type;
  uint32_t _tag;
  bool _eof;
  uint8_t _errCode;
  protobuf_codec_general_bytes_t _sensor_id;
  protobuf_codec_general_bytes_t _sensor_id_1;
  protobuf_codec_general_bytes_t _gateway_id;
  protobuf_codec_acked_seq_number_t _acked_seq_no_array;
  protobuf_codec_file_notify_dissem_t _file_dissem_notify;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint8_t _sensor_id_1_buf[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];

  *Froto_header = &(SKF_Froto_App->_messages.file_notify_dissem.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_file_notify_dissem_tag))
    {
      pb_make_string_substream(istream, &_substream);
      _sensor_id.buffer = _peer_id;
      _sensor_id.size = sizeof(_peer_id);
      _sensor_id.length = 0;
      (*Froto_header)->sensor_id.arg = &_sensor_id;
      (*Froto_header)->sensor_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _gateway_id.buffer = _self_id;
      _gateway_id.size = sizeof(_self_id);
      _gateway_id.length = 0;
      (*Froto_header)->peer_addr.gateway_id.arg = &_gateway_id;
      (*Froto_header)->peer_addr.gateway_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _acked_seq_no_array.buffer = _acked_seq_no;
      _acked_seq_no_array.size = sizeof(_acked_seq_no) /
                                 sizeof(_acked_seq_no[0]);
      _acked_seq_no_array.cnt = 0;
      (*Froto_header)->acked_message_seq_number.arg =
        &_acked_seq_no_array;
      (*Froto_header)->acked_message_seq_number.funcs.decode =
        &protobuf_dec_acked_seq_number_callback;

      _sensor_id_1.buffer = _sensor_id_1_buf;
      _sensor_id_1.size = sizeof(_sensor_id_1_buf);
      _sensor_id_1.length = 0;
      SKF_Froto_App->_messages.file_notify_dissem.sensor_id.arg =
        &_sensor_id_1;
      SKF_Froto_App->_messages.file_notify_dissem.sensor_id.funcs.
      decode =
        &protobuf_dec_bytes_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fields,
                           &SKF_Froto_App->_messages.file_notify_dissem))
      {
        DBG_LOG_INFO("Froto APP file_notify_dissem appVer: %d",
                      SKF_Froto_App->_messages.file_notify_dissem.
                      appVer);
        DBG_LOG_DEBUG("Froto APP file_notify_dissem has header: %d",
                      SKF_Froto_App->_messages.file_notify_dissem.
                      has_header);
        if(SKF_Froto_App->_messages.file_notify_dissem.has_header ==
           true)
        {
          DBG_LOG_INFO(
            "Froto APP file_notify_dissem header version: %d",
            (*Froto_header)->version);
          DBG_LOG_DEBUG(
            "Froto APP file_notify_dissem header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_INFO(
            "Froto APP file_notify_dissem header which_peer_addr: %d",
            (*Froto_header)->which_peer_addr);
          // This is a bug to deal with "bytes" in "oneof"
          // which_config_content would be set to zero when decoding
          // "bytes" in "oneof"
          // So we here, cannot determin the peer addr based on
          // which_peer_addr
          // if((*Froto_header)->which_peer_addr ==
          //    SKFChina_Froto_FrotoHeader_gateway_id_tag)
          {
            DBG_LOG_DEBUG(
              "Froto APP file_notify_dissem header gateway id:");
            util_dbg_buf_dump(
              ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                  peer_addr.
                                                  gateway_id.
                                                  arg))->buffer,
              ((protobuf_codec_general_bytes_t *)(
                 (*Froto_header)->peer_addr.gateway_id.arg))->length);
          }
          // else
          // {
          //   DBG_LOG_WARN("The peer addr type is not supported");
          // }
          DBG_LOG_DEBUG(
            "Froto APP file_notify_dissem header is up: %d",
            (*Froto_header)->is_up);
          DBG_LOG_INFO(
            "Froto APP file_notify_dissem header msg seq no: %d",
            (*Froto_header)->message_seq_no);
          DBG_LOG_DEBUG("Froto APP file_notify_dissem header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG(
            "Froto APP file_notify_dissem header primitive type: %d",
            (*Froto_header)->primitive_type);
          DBG_LOG_INFO(
            "Froto APP file_notify_dissem header message type: %d",
            (*Froto_header)->message_type);
          DBG_LOG_INFO(
            "Froto APP file_notify_dissem header total block: %d",
            (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP file_notify_dissem header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_INFO(
              "Froto APP file_notify_dissem header current block: %d",
              (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG(
            "Froto APP file_notify_dissem header long packet id: %d",
            (*Froto_header)->long_packet_id);
          DBG_LOG_DEBUG(
            "Froto APP file_notify_dissem header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG(
              "Froto APP file_notify_dissem header is big endian: %d",
              (*Froto_header)->is_big_endian);
          }
          DBG_LOG_DEBUG(
            "Froto APP file_notify_dissem header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
        }
        DBG_LOG_DEBUG(
          "Froto APP file_notify_dissem has is big endian: %d",
          SKF_Froto_App->_messages.file_notify_dissem.
          has_is_big_endian);
        if(SKF_Froto_App->_messages.file_notify_dissem.
           has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG(
            "Froto APP file_notify_dissem is big endian: %d",
            SKF_Froto_App->_messages.file_notify_dissem.
            is_big_endian);
        }
        DBG_LOG_DEBUG(
          "Froto APP file_notify_dissem sensor id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              file_notify_dissem.
                                              sensor_id.
                                              arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              file_notify_dissem.
                                              sensor_id.
                                              arg))->length);
        _file_dissem_notify.sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              file_notify_dissem.
                                              sensor_id.
                                              arg))->length;
        if(_file_dissem_notify.sensorIDLen)
        {
          _file_dissem_notify.sensorID =
            l_malloc(_file_dissem_notify.sensorIDLen);
          memcpy(_file_dissem_notify.sensorID,
                 ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->
                                                     _messages.
                                                     file_notify_dissem
                                                     .
                                                     sensor_id.arg))->
                 buffer,
                 _file_dissem_notify.sensorIDLen);
        }
        else
        {
          _file_dissem_notify.sensorID = NULL;
        }
        _file_dissem_notify.fileType =
          SKF_Froto_App->_messages.file_notify_dissem.file_type;
        DBG_LOG_INFO(
          "Froto APP file_notify_dissem file type: %d",
          _file_dissem_notify.fileType);
        _file_dissem_notify.taskId =
          SKF_Froto_App->_messages.file_notify_dissem.task_id;
        DBG_LOG_DEBUG(
          "Froto APP file_notify_dissem task ID: %d",
          _file_dissem_notify.taskId);
        _file_dissem_notify.update_method =
          SKF_Froto_App->_messages.file_notify_dissem.update_method;
        DBG_LOG_DEBUG(
          "Froto APP file_notify_dissem update method: %d",
          _file_dissem_notify.update_method);
        if(SKF_Froto_App->_messages.file_notify_dissem.which_fuota_method
           ==
           SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_froto_over_ble_ots_tag)
        {
          _file_dissem_notify.overBleUART =
            SKF_Froto_App->_messages.file_notify_dissem.fuota_method.
            fuota_with_froto_over_ble_ots.isBLEUart;
        }
        else
        {
          _file_dissem_notify.overBleUART = false;
        }
        DBG_LOG_DEBUG(
          "Froto APP file_notify_dissem update fuota_with_froto_over_ble_ots isBLEUart: %d",
          _file_dissem_notify.overBleUART);
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(file_notify_dissem_processor != NULL)
  {
    file_notify_dissem_processor(&_file_dissem_notify, _errCode);
  }
  l_free(_file_dissem_notify.sensorID);
  _file_dissem_notify.sensorID = NULL;
  _file_dissem_notify.sensorIDLen = 0;

  return _rt;
}
/**
 * @brief Parse image/file block dissemination message. Note that only one
 * message would be parsed if there are multiple messages in the input
 * buffer @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param image_block_dissem_processor A callback function to deal with
 * the parsed data parsing (e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_image_block_dissem_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t image_block_dissem_processor,
  uint8_t *errCode)
{
  bool _rt = true;
  pb_istream_t _substream;
  pb_wire_type_t _wire_type;
  uint32_t _tag;
  bool _eof;
  uint8_t _errCode;
  protobuf_codec_general_bytes_t _sensor_id;
  protobuf_codec_general_bytes_t _sensor_id_1;
  protobuf_codec_general_bytes_t _gateway_id;
  protobuf_codec_general_bytes_t _content;
  protobuf_codec_acked_seq_number_t _acked_seq_no_array;
  protobuf_codec_image_block_dissem_t _image_block_dissem;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint8_t _sensor_id_1_buf[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];
  uint8_t _content_buf[FROTO_MAX_IMAGE_FILE_LENGTH];

  *Froto_header = &(SKF_Froto_App->_messages.image_block_dissem.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_image_block_dissem_tag))
    {
      pb_make_string_substream(istream, &_substream);
      _sensor_id.buffer = _peer_id;
      _sensor_id.size = sizeof(_peer_id);
      _sensor_id.length = 0;
      (*Froto_header)->sensor_id.arg = &_sensor_id;
      (*Froto_header)->sensor_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _gateway_id.buffer = _self_id;
      _gateway_id.size = sizeof(_self_id);
      _gateway_id.length = 0;
      (*Froto_header)->peer_addr.gateway_id.arg = &_gateway_id;
      (*Froto_header)->peer_addr.gateway_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      _acked_seq_no_array.buffer = _acked_seq_no;
      _acked_seq_no_array.size = sizeof(_acked_seq_no) /
                                 sizeof(_acked_seq_no[0]);
      _acked_seq_no_array.cnt = 0;
      (*Froto_header)->acked_message_seq_number.arg =
        &_acked_seq_no_array;
      (*Froto_header)->acked_message_seq_number.funcs.decode =
        &protobuf_dec_acked_seq_number_callback;

      _sensor_id_1.buffer = _sensor_id_1_buf;
      _sensor_id_1.size = sizeof(_sensor_id_1_buf);
      _sensor_id_1.length = 0;
      SKF_Froto_App->_messages.image_block_dissem.sensor_id.arg =
        &_sensor_id_1;
      SKF_Froto_App->_messages.image_block_dissem.sensor_id.funcs.
      decode =
        &protobuf_dec_bytes_callback;

      _content.buffer = _content_buf;
      _content.size = sizeof(_content_buf);
      _content.length = 0;
      SKF_Froto_App->_messages.image_block_dissem.image_content.arg =
        &_content;
      SKF_Froto_App->_messages.image_block_dissem.image_content.funcs.
      decode =
        &protobuf_dec_bytes_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_fields,
                           &SKF_Froto_App->_messages.image_block_dissem))
      {
        DBG_LOG_INFO("Froto APP image_block_dissem appVer: %d",
                      SKF_Froto_App->_messages.image_block_dissem.
                      appVer);
        DBG_LOG_DEBUG("Froto APP image_block_dissem has header: %d",
                      SKF_Froto_App->_messages.image_block_dissem.
                      has_header);
        if(SKF_Froto_App->_messages.image_block_dissem.has_header ==
           true)
        {
          DBG_LOG_INFO(
            "Froto APP image_block_dissem header version: %d",
            (*Froto_header)->version);
          DBG_LOG_DEBUG(
            "Froto APP image_block_dissem header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_INFO(
            "Froto APP image_block_dissem header which_peer_addr: %d",
            (*Froto_header)->which_peer_addr);
          // This is a bug to deal with "bytes" in "oneof"
          // which_config_content would be set to zero when decoding
          // "bytes" in "oneof"
          // So we here, cannot determin the peer addr based on
          // which_peer_addr
          // if((*Froto_header)->which_peer_addr ==
          //    SKFChina_Froto_FrotoHeader_gateway_id_tag)
          {
            DBG_LOG_DEBUG(
              "Froto APP image_block_dissem header gateway id:");
            util_dbg_buf_dump(
              ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                  peer_addr.
                                                  gateway_id.
                                                  arg))->buffer,
              ((protobuf_codec_general_bytes_t *)(
                 (*Froto_header)->peer_addr.gateway_id.arg))->length);
          }
          // else
          // {
          //   DBG_LOG_WARN("The peer addr type is not supported");
          // }
          DBG_LOG_DEBUG(
            "Froto APP image_block_dissem header is up: %d",
            (*Froto_header)->is_up);
          DBG_LOG_INFO(
            "Froto APP image_block_dissem header msg seq no: %d",
            (*Froto_header)->message_seq_no);
          DBG_LOG_DEBUG("Froto APP image_block_dissem header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG(
            "Froto APP image_block_dissem header primitive type: %d",
            (*Froto_header)->primitive_type);
          DBG_LOG_INFO(
            "Froto APP image_block_dissem header message type: %d",
            (*Froto_header)->message_type);
          DBG_LOG_INFO(
            "Froto APP image_block_dissem header total block: %d",
            (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP image_block_dissem header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_INFO(
              "Froto APP image_block_dissem header current block: %d",
              (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG(
            "Froto APP image_block_dissem header long packet id: %d",
            (*Froto_header)->long_packet_id);
          DBG_LOG_DEBUG(
            "Froto APP image_block_dissem header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG(
              "Froto APP image_block_dissem header is big endian: %d",
              (*Froto_header)->is_big_endian);
          }
          DBG_LOG_DEBUG(
            "Froto APP image_block_dissem header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
        }
        DBG_LOG_DEBUG(
          "Froto APP image_block_dissem has is big endian: %d",
          SKF_Froto_App->_messages.image_block_dissem.
          has_is_big_endian);
        if(SKF_Froto_App->_messages.image_block_dissem.
           has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG(
            "Froto APP image_block_dissem is big endian: %d",
            SKF_Froto_App->_messages.image_block_dissem.
            is_big_endian);
        }
        DBG_LOG_DEBUG(
          "Froto APP image_block_dissem sensor id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_dissem.
                                              sensor_id.
                                              arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_dissem.
                                              sensor_id.
                                              arg))->length);
        _image_block_dissem.sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_dissem.
                                              sensor_id.
                                              arg))->length;
        if(_image_block_dissem.sensorIDLen)
        {
          _image_block_dissem.sensorID =
            l_malloc(_image_block_dissem.sensorIDLen);
          memcpy(_image_block_dissem.sensorID,
                 ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->
                                                     _messages.
                                                     image_block_dissem
                                                     .
                                                     sensor_id.arg))->
                 buffer,
                 _image_block_dissem.sensorIDLen);
        }
        else
        {
          _image_block_dissem.sensorID = NULL;
        }
        if(SKF_Froto_App->_messages.image_block_dissem.which_task_id ==
           SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_fuota_task_id_tag)
        {
          _image_block_dissem.isFuota = true;
          _image_block_dissem.taskId =
            SKF_Froto_App->_messages.image_block_dissem.task_id.
            fuota_task_id;
          DBG_LOG_INFO("is FUOTA task, task ID: %lu",
                        _image_block_dissem.taskId);
        }
        else
        {
          _image_block_dissem.isFuota = false;
          _image_block_dissem.taskId =
            SKF_Froto_App->_messages.image_block_dissem.task_id.
            file_task_id;
          DBG_LOG_INFO("is file task, task ID: %lu",
                        _image_block_dissem.taskId);
        }
        _image_block_dissem.offset =
          SKF_Froto_App->_messages.image_block_dissem.offset;
        _image_block_dissem.contentLen =
          ((protobuf_codec_general_bytes_t *)SKF_Froto_App->_messages.
           image_block_dissem.image_content.arg)->length;
        _image_block_dissem.content =
          l_malloc(_image_block_dissem.contentLen);
        memcpy(_image_block_dissem.content,
               ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages
                                                   .image_block_dissem.
                                                   image_content.arg))->
               buffer,
               _image_block_dissem.contentLen);
        DBG_LOG_DEBUG(
          "Froto APP image_block_dissem content: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_dissem.
                                              image_content.arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              image_block_dissem.
                                              image_content.arg))->length);
        if((*Froto_header)->has_current_block)
        {
          _image_block_dissem.currentBlock =
            (*Froto_header)->current_block;
        }
        else
        {
          _image_block_dissem.currentBlock = 0;
        }
        _image_block_dissem.totalBlock = (*Froto_header)->total_block;
        _image_block_dissem.longPktId = (*Froto_header)->long_packet_id;
        _image_block_dissem.seqNo = (*Froto_header)->message_seq_no;
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(image_block_dissem_processor != NULL)
  {
    image_block_dissem_processor(&_image_block_dissem, _errCode);
  }
  l_free(_image_block_dissem.sensorID);
  _image_block_dissem.sensorID = NULL;
  _image_block_dissem.sensorIDLen = 0;
  l_free(_image_block_dissem.content);
  _image_block_dissem.content = NULL;
  _image_block_dissem.contentLen = 0;

  return _rt;
}
