/**
 * @file    app_froto_config_decoder.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Implementation of config related message (of Froto) decoder.
 * @details
 */
#include "sys_def.h"
#include "froto/app_froto_new.h"
#include "froto/app_froto_common.h"
#include "froto/decoder/app_froto_config_decoder.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

/**
 * @brief Parse config hash upload message. Note that only one message
 * would be parsed if there are multiple messages in the input buffer
 * @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param config_hash_upload_processor A callback function to deal with the
 * data after parsing e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_config_hash_upload_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t config_hash_upload_processor,
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
  protobuf_codec_config_hash_t _hash;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint8_t _sensor_id_1_buf[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];

  *Froto_header = &(SKF_Froto_App->_messages.config_hash_upload.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_config_hash_upload_tag))
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
      SKF_Froto_App->_messages.config_hash_upload.sensor_id.arg =
        &_sensor_id_1;
      SKF_Froto_App->_messages.config_hash_upload.sensor_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_ConfigurationAndCommand_ConfigHashUpload_fields,
                           &SKF_Froto_App->_messages.config_hash_upload))
      {
        DBG_LOG_INFO("Froto APP config_hash_upload appVer: %d",
                      SKF_Froto_App->_messages.config_hash_upload.appVer);
        DBG_LOG_DEBUG("Froto APP config_hash_upload has header: %d",
                      SKF_Froto_App->_messages.config_hash_upload.
                      has_header);
        if(SKF_Froto_App->_messages.config_hash_upload.has_header == true)
        {
          DBG_LOG_INFO("Froto APP config_hash_upload header version: %d",
                        (*Froto_header)->version);
          DBG_LOG_DEBUG(
            "Froto APP config_hash_upload header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_INFO(
            "Froto APP config_hash_upload header which_peer_addr: %d",
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
              "Froto APP config_hash_upload header gateway id:");
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
          DBG_LOG_DEBUG("Froto APP config_hash_upload header is up: %d",
                        (*Froto_header)->is_up);
          DBG_LOG_DEBUG(
            "Froto APP config_hash_upload header msg seq no: %d",
            (*Froto_header)->message_seq_no);
          DBG_LOG_DEBUG("Froto APP config_hash_upload header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG(
            "Froto APP config_hash_upload header primitive type: %d",
            (*Froto_header)->primitive_type);
          DBG_LOG_DEBUG(
            "Froto APP config_hash_upload header message type: %d",
            (*Froto_header)->message_type);
          DBG_LOG_DEBUG(
            "Froto APP config_hash_upload header total block: %d",
            (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP config_hash_upload header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_DEBUG(
              "Froto APP config_hash_upload header current block: %d",
              (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG(
            "Froto APP config_hash_upload header long packet id: %d",
            (*Froto_header)->long_packet_id);
          DBG_LOG_DEBUG(
            "Froto APP config_hash_upload header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG(
              "Froto APP config_hash_upload header is big endian: %d",
              (*Froto_header)->is_big_endian);
          }
          DBG_LOG_DEBUG(
            "Froto APP config_hash_upload header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
        }
        DBG_LOG_DEBUG("Froto APP config_hash_upload has is big endian: %d",
                      SKF_Froto_App->_messages.config_hash_upload.
                      has_is_big_endian);
        if(SKF_Froto_App->_messages.config_hash_upload.has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG("Froto APP config_hash_upload is big endian: %d",
                        SKF_Froto_App->_messages.config_hash_upload.
                        is_big_endian);
        }
        DBG_LOG_DEBUG("Froto APP config_hash_upload product: %d",
                      SKF_Froto_App->_messages.config_hash_upload.product);
        DBG_LOG_DEBUG("Froto APP config_hash_upload hash: %u",
                      SKF_Froto_App->_messages.config_hash_upload.
                      config_hash_value);
        _hash.hash =
          SKF_Froto_App->_messages.config_hash_upload.config_hash_value;
        DBG_LOG_DEBUG(
          "Froto APP config_hash_upload has last edit time: %d",
          SKF_Froto_App->_messages.config_hash_upload.
          has_last_edit_time);
        if(SKF_Froto_App->_messages.config_hash_upload.has_last_edit_time)
        {
          _hash.has_last_edit_time = true;
          _hash.last_edit_time =
            SKF_Froto_App->_messages.config_hash_upload.last_edit_time.time;
          DBG_LOG_INFO(
            "Froto APP config_hash_upload has last edit time: %lu",
            SKF_Froto_App->_messages.config_hash_upload.
            last_edit_time.time);
        }
        else
        {
          _hash.has_last_edit_time = false;
        }
        DBG_LOG_DEBUG(
          "Froto APP config_hash_upload sensor id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              config_hash_upload.sensor_id.
                                              arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              config_hash_upload.sensor_id.
                                              arg))->length);
        _hash.sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              config_hash_upload.sensor_id.
                                              arg))->length;
        if(_hash.sensorIDLen)
        {
          _hash.sensorID = l_malloc(_hash.sensorIDLen);
          memcpy(_hash.sensorID,
                 ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->
                                                     _messages.
                                                     config_hash_upload.
                                                     sensor_id.arg))->
                 buffer,
                 _hash.sensorIDLen);
        }
        else
        {
          _hash.sensorID = NULL;
        }
        // TODO: Not support config_hash_bitmap and config_hash_len_byte
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(config_hash_upload_processor != NULL)
  {
    config_hash_upload_processor(&_hash, _errCode);
  }

  l_free(_hash.sensorID);
  _hash.sensorID = NULL;
  _hash.sensorIDLen = 0;

  return _rt;
}
/**
 * @brief Parse specific config upload message. Note that only one message
 * would be parsed if there are multiple messages in the input buffer
 * @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param specific_config_upload_processor A callback function to deal with
 * the data after parsing (e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_specific_config_upload_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t specific_config_upload_processor,
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
  protobuf_codec_specific_config_t _conf;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint8_t _sensor_id_1_buf[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];
  protobuf_codec_config_pair_ele_t _config_pair[FROTO_CONFIGPAIR_MAX_NUMBER];

  *Froto_header =
    &(SKF_Froto_App->_messages.specific_config_upload.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_specific_config_upload_tag))
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
      SKF_Froto_App->_messages.specific_config_upload.sensor_id.arg =
        &_sensor_id_1;
      SKF_Froto_App->_messages.specific_config_upload.sensor_id.funcs.
      decode = &protobuf_dec_bytes_callback;

      _conf.config_pair.buffer = _config_pair;
      _conf.config_pair.size = sizeof(_config_pair) /
                               sizeof(_config_pair[0]);
      _conf.config_pair.cnt = 0;
      memset(_config_pair, 0, sizeof(_config_pair));
      SKF_Froto_App->_messages.specific_config_upload.config_pair.arg =
        &_conf.config_pair;
      SKF_Froto_App->_messages.specific_config_upload.config_pair.funcs.
      decode =
        &protobuf_dec_config_pair_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_ConfigurationAndCommand_SpecificConfigUpload_fields,
                           &SKF_Froto_App->_messages.specific_config_upload))
      {
        DBG_LOG_INFO("Froto APP specific_config_upload appVer: %d",
                      SKF_Froto_App->_messages.specific_config_upload.
                      appVer);
        DBG_LOG_DEBUG("Froto APP specific_config_upload has header: %d",
                      SKF_Froto_App->_messages.specific_config_upload.
                      has_header);
        if(SKF_Froto_App->_messages.specific_config_upload.has_header ==
           true)
        {
          DBG_LOG_INFO(
            "Froto APP specific_config_upload header version: %d",
            (*Froto_header)->version);
          DBG_LOG_DEBUG(
            "Froto APP specific_config_upload header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_DEBUG(
            "Froto APP specific_config_upload header which_peer_addr: %d",
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
              "Froto APP specific_config_upload header gateway id:");
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
            "Froto APP specific_config_upload header is up: %d",
            (*Froto_header)->is_up);
          DBG_LOG_INFO(
            "Froto APP specific_config_upload header msg seq no: %d",
            (*Froto_header)->message_seq_no);
          DBG_LOG_DEBUG("Froto APP specific_config_upload header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG(
            "Froto APP specific_config_upload header primitive type: %d",
            (*Froto_header)->primitive_type);
          DBG_LOG_INFO(
            "Froto APP specific_config_upload header message type: %d",
            (*Froto_header)->message_type);
          DBG_LOG_DEBUG(
            "Froto APP specific_config_upload header total block: %d",
            (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP specific_config_upload header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_DEBUG(
              "Froto APP specific_config_upload header current block: %d",
              (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG(
            "Froto APP specific_config_upload header long packet id: %d",
            (*Froto_header)->long_packet_id);
          DBG_LOG_DEBUG(
            "Froto APP specific_config_upload header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG(
              "Froto APP specific_config_upload header is big endian: %d",
              (*Froto_header)->is_big_endian);
          }
          DBG_LOG_INFO(
            "Froto APP specific_config_upload header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
        }
        DBG_LOG_DEBUG(
          "Froto APP specific_config_upload has is big endian: %d",
          SKF_Froto_App->_messages.specific_config_upload.
          has_is_big_endian);
        if(SKF_Froto_App->_messages.specific_config_upload.
           has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG(
            "Froto APP specific_config_upload is big endian: %d",
            SKF_Froto_App->_messages.specific_config_upload.
            is_big_endian);
        }
        DBG_LOG_DEBUG("Froto APP specific_config_upload product: %d",
                      SKF_Froto_App->_messages.specific_config_upload.
                      product);

        DBG_LOG_DEBUG(
          "Froto APP specific_config_upload sensor id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              specific_config_upload.
                                              sensor_id.
                                              arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              specific_config_upload.
                                              sensor_id.
                                              arg))->length);
        _conf.sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              config_hash_upload.sensor_id.
                                              arg))->length;
        if(_conf.sensorIDLen)
        {
          _conf.sensorID = l_malloc(_conf.sensorIDLen);
          memcpy(_conf.sensorID,
                 ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->
                                                     _messages.
                                                     specific_config_upload
                                                     .sensor_id.arg))->
                 buffer,
                 _conf.sensorIDLen);
        }
        else
        {
          _conf.sensorID = NULL;
        }
        DBG_LOG_INFO("There are %d config pairs.", _conf.config_pair.cnt);
        for(uint32_t i = 0; i < _conf.config_pair.cnt; i++)
        {
          DBG_LOG_INFO("specific_config_upload which_config_item %d",
                        _conf.config_pair.buffer[i].which_config_item);
          if(_conf.config_pair.buffer[i].which_config_item ==
             SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag)
          {
            DBG_LOG_DEBUG("specific_config_upload config_item %d",
                          _conf.config_pair.buffer[i].config_item.
                          specific_config_item);
          }
          else if(_conf.config_pair.buffer[i].which_config_item ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag)
          {
            DBG_LOG_DEBUG("specific_config_upload fscheduler item %d",
                          _conf.config_pair.buffer[i].config_item.
                          fscheduler_config_item);
          }
          DBG_LOG_DEBUG("specific_config_upload has memory id %d",
                        _conf.config_pair.buffer[i].has_memory_id);
          if(_conf.config_pair.buffer[i].has_memory_id)
          {
            DBG_LOG_DEBUG("specific_config_upload memory id %d",
                          _conf.config_pair.buffer[i].memory_id);
          }
          DBG_LOG_DEBUG("specific_config_upload which_config_content %d",
                        _conf.config_pair.buffer[i].which_config_content);
          if(_conf.config_pair.buffer[i].which_config_content ==
             SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
          {
            DBG_LOG_DEBUG("specific_config_upload config content %u",
                          _conf.config_pair.buffer[i].content.
                          content_uint32);
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag)
          {
            DBG_LOG_DEBUG("specific_config_upload config content %d",
                          _conf.config_pair.buffer[i].content.content_int32);
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
          {
            DBG_LOG_DEBUG("specific_config_upload config content %f",
                          _conf.config_pair.buffer[i].content.content_float);
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
          {
            DBG_LOG_DEBUG("specific_config_upload config content %d",
                          _conf.config_pair.buffer[i].content.content_bool);
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
          {
            DBG_LOG_DEBUG("specific_config_upload config content");
            for(uint32_t m = 0;
                m < _conf.config_pair.buffer[i].content.time.cnt; m++)
            {
              DBG_LOG_DEBUG("%lu",
                            _conf.config_pair.buffer[i].content.time.time[m]);
            }
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
          {
            DBG_LOG_DEBUG("specific_config_upload config content");
            DBG_LOG_DEBUG("work mode %d",
                          _conf.config_pair.buffer[i].content.work_mode.
                          work_mode);
            DBG_LOG_DEBUG("params");
            util_dbg_buf_dump(
              _conf.config_pair.buffer[i].content.work_mode.parameters,
              _conf.config_pair.buffer[i].content.work_mode.parametersLen);
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
          {
            DBG_LOG_DEBUG("specific_config_upload config content");
            DBG_LOG_DEBUG("nameid %u",
                          _conf.config_pair.buffer[i].content.gateway_info.
                          name_id);
            DBG_LOG_DEBUG("macaddr");
            util_dbg_buf_dump(
              _conf.config_pair.buffer[i].content.gateway_info.mac_addr,
              _conf.config_pair.buffer[i].content.gateway_info.mac_addrLen);
          }
          else
          {
          }
        }
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(specific_config_upload_processor != NULL)
  {
    specific_config_upload_processor(&_conf, _errCode);
  }
  l_free(_conf.sensorID);
  _conf.sensorID = NULL;
  _conf.sensorIDLen = 0;
  for(uint32_t i = 0; i < _conf.config_pair.cnt; i++)
  {
    if(_conf.config_pair.buffer[i].which_config_content ==
       SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
    {
      l_free(_conf.config_pair.buffer[i].content.gateway_info.mac_addr);
      _conf.config_pair.buffer[i].content.gateway_info.mac_addr = NULL;
      _conf.config_pair.buffer[i].content.gateway_info.mac_addrLen = 0;
    }
    if(_conf.config_pair.buffer[i].which_config_content ==
       SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
    {
      l_free(_conf.config_pair.buffer[i].content.work_mode.parameters);
      _conf.config_pair.buffer[i].content.work_mode.parameters = NULL;
      _conf.config_pair.buffer[i].content.work_mode.parametersLen = 0;
    }
    if(_conf.config_pair.buffer[i].which_config_content ==
       SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
    {
      l_free(_conf.config_pair.buffer[i].content.time.time);
      _conf.config_pair.buffer[i].content.time.time = NULL;
      _conf.config_pair.buffer[i].content.time.size = 0;
      _conf.config_pair.buffer[i].content.time.cnt = 0;
    }
  }
  return _rt;
}
/**
 * @brief Parse specific config dissemination message. Note that only one
 * message would be parsed if there are multiple messages in the input
 * buffer @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param config_dissem_processor A callback function to deal with the data
 * after parsing (e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_config_dissem_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t config_dissem_processor,
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
  protobuf_codec_specific_config_t _conf;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint8_t _sensor_id_1_buf[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];
  protobuf_codec_config_pair_ele_t _config_pair[FROTO_CONFIGPAIR_MAX_NUMBER];

  *Froto_header =
    &(SKF_Froto_App->_messages.config_dissem.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_config_dissem_tag))
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
      SKF_Froto_App->_messages.config_dissem.sensor_id.arg =
        &_sensor_id_1;
      SKF_Froto_App->_messages.config_dissem.sensor_id.funcs.
      decode = &protobuf_dec_bytes_callback;

      _conf.config_pair.buffer = _config_pair;
      _conf.config_pair.size = sizeof(_config_pair) /
                               sizeof(_config_pair[0]);
      _conf.config_pair.cnt = 0;
      memset(_config_pair, 0, sizeof(_config_pair));
      SKF_Froto_App->_messages.config_dissem.config_pair.arg =
        &_conf.config_pair;
      SKF_Froto_App->_messages.config_dissem.config_pair.funcs.
      decode =
        &protobuf_dec_config_pair_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields,
                           &SKF_Froto_App->_messages.config_dissem))
      {
        DBG_LOG_INFO("Froto APP config_dissem appVer: %d",
                      SKF_Froto_App->_messages.config_dissem.
                      appVer);
        DBG_LOG_DEBUG("Froto APP config_dissem has header: %d",
                      SKF_Froto_App->_messages.config_dissem.
                      has_header);
        if(SKF_Froto_App->_messages.config_dissem.has_header ==
           true)
        {
          DBG_LOG_INFO(
            "Froto APP config_dissem header version: %d",
            (*Froto_header)->version);
          DBG_LOG_DEBUG(
            "Froto APP config_dissem header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_DEBUG(
            "Froto APP config_dissem header which_peer_addr: %d",
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
              "Froto APP config_dissem header gateway id:");
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
            "Froto APP config_dissem header is up: %d",
            (*Froto_header)->is_up);
          DBG_LOG_INFO(
            "Froto APP config_dissem header msg seq no: %d",
            (*Froto_header)->message_seq_no);
          DBG_LOG_DEBUG("Froto APP config_dissem header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG(
            "Froto APP config_dissem header primitive type: %d",
            (*Froto_header)->primitive_type);
          DBG_LOG_INFO(
            "Froto APP config_dissem header message type: %d",
            (*Froto_header)->message_type);
          DBG_LOG_INFO(
            "Froto APP config_dissem header total block: %d",
            (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP config_dissem header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_DEBUG(
              "Froto APP config_dissem header current block: %d",
              (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG(
            "Froto APP config_dissem header long packet id: %d",
            (*Froto_header)->long_packet_id);
          DBG_LOG_DEBUG(
            "Froto APP config_dissem header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG(
              "Froto APP config_dissem header is big endian: %d",
              (*Froto_header)->is_big_endian);
          }
          DBG_LOG_DEBUG(
            "Froto APP config_dissem header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
        }
        DBG_LOG_DEBUG(
          "Froto APP config_dissem has is big endian: %d",
          SKF_Froto_App->_messages.config_dissem.has_is_big_endian);
        if(SKF_Froto_App->_messages.config_dissem.has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG(
            "Froto APP config_dissem is big endian: %d",
            SKF_Froto_App->_messages.config_dissem.
            is_big_endian);
        }
        DBG_LOG_DEBUG("Froto APP config_dissem product: %d",
                      SKF_Froto_App->_messages.config_dissem.
                      product);

        DBG_LOG_DEBUG(
          "Froto APP config_dissem sensor id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              config_dissem.
                                              sensor_id.
                                              arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              config_dissem.
                                              sensor_id.
                                              arg))->length);
        _conf.sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              config_hash_upload.sensor_id.
                                              arg))->length;
        if(_conf.sensorIDLen)
        {
          _conf.sensorID = l_malloc(_conf.sensorIDLen);
          memcpy(_conf.sensorID,
                 ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->
                                                     _messages.
                                                     config_dissem
                                                     .sensor_id.arg))->
                 buffer,
                 _conf.sensorIDLen);
        }
        else
        {
          _conf.sensorID = NULL;
        }
        DBG_LOG_INFO("There are %d config pairs.", _conf.config_pair.cnt);
        for(uint32_t i = 0; i < _conf.config_pair.cnt; i++)
        {
          DBG_LOG_INFO("config_dissem which_config_item %d",
                        _conf.config_pair.buffer[i].which_config_item);
          if(_conf.config_pair.buffer[i].which_config_item ==
             SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag)
          {
            DBG_LOG_DEBUG("config_dissem config_item %d",
                          _conf.config_pair.buffer[i].config_item.
                          specific_config_item);
          }
          else if(_conf.config_pair.buffer[i].which_config_item ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag)
          {
            DBG_LOG_DEBUG("config_dissem fscheduler item %d",
                          _conf.config_pair.buffer[i].config_item.
                          fscheduler_config_item);
          }
          DBG_LOG_DEBUG("config_dissem has memory id %d",
                        _conf.config_pair.buffer[i].has_memory_id);
          if(_conf.config_pair.buffer[i].has_memory_id)
          {
            DBG_LOG_DEBUG("config_dissem memory id %d",
                          _conf.config_pair.buffer[i].memory_id);
          }
          DBG_LOG_DEBUG("config_dissem which_config_content %d",
                        _conf.config_pair.buffer[i].which_config_content);
          if(_conf.config_pair.buffer[i].which_config_content ==
             SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
          {
            DBG_LOG_DEBUG("config_dissem config content %u",
                          _conf.config_pair.buffer[i].content.
                          content_uint32);
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag)
          {
            DBG_LOG_DEBUG("config_dissem config content %d",
                          _conf.config_pair.buffer[i].content.content_int32);
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
          {
            DBG_LOG_DEBUG("config_dissem config content %f",
                          _conf.config_pair.buffer[i].content.content_float);
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
          {
            DBG_LOG_DEBUG("config_dissem config content %d",
                          _conf.config_pair.buffer[i].content.content_bool);
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
          {
            DBG_LOG_DEBUG("config_dissem config content");
            for(uint32_t m = 0;
                m < _conf.config_pair.buffer[i].content.time.cnt; m++)
            {
              DBG_LOG_DEBUG("%lu",
                            _conf.config_pair.buffer[i].content.time.time[m]);
            }
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
          {
            DBG_LOG_DEBUG("config_dissem config content");
            DBG_LOG_DEBUG("work mode %d",
                          _conf.config_pair.buffer[i].content.work_mode.
                          work_mode);
            DBG_LOG_DEBUG("params");
            util_dbg_buf_dump(
              _conf.config_pair.buffer[i].content.work_mode.parameters,
              _conf.config_pair.buffer[i].content.work_mode.parametersLen);
          }
          else if(_conf.config_pair.buffer[i].which_config_content ==
                  SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
          {
            DBG_LOG_DEBUG("config_dissem config content");
            DBG_LOG_DEBUG("nameid %u",
                          _conf.config_pair.buffer[i].content.gateway_info.
                          name_id);
            DBG_LOG_DEBUG("macaddr");
            util_dbg_buf_dump(
              _conf.config_pair.buffer[i].content.gateway_info.mac_addr,
              _conf.config_pair.buffer[i].content.gateway_info.mac_addrLen);
          }
          else
          {
          }
        }
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(config_dissem_processor != NULL)
  {
    config_dissem_processor(&_conf, _errCode);
  }
  l_free(_conf.sensorID);
  _conf.sensorID = NULL;
  _conf.sensorIDLen = 0;
  for(uint32_t i = 0; i < _conf.config_pair.cnt; i++)
  {
    if(_conf.config_pair.buffer[i].which_config_content ==
       SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
    {
      l_free(_conf.config_pair.buffer[i].content.gateway_info.mac_addr);
      _conf.config_pair.buffer[i].content.gateway_info.mac_addr = NULL;
      _conf.config_pair.buffer[i].content.gateway_info.mac_addrLen = 0;
    }
    if(_conf.config_pair.buffer[i].which_config_content ==
       SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
    {
      l_free(_conf.config_pair.buffer[i].content.work_mode.parameters);
      _conf.config_pair.buffer[i].content.work_mode.parameters = NULL;
      _conf.config_pair.buffer[i].content.work_mode.parametersLen = 0;
    }
    if(_conf.config_pair.buffer[i].which_config_content ==
       SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
    {
      l_free(_conf.config_pair.buffer[i].content.time.time);
      _conf.config_pair.buffer[i].content.time.time = NULL;
      _conf.config_pair.buffer[i].content.time.size = 0;
      _conf.config_pair.buffer[i].content.time.cnt = 0;
    }
  }
  return _rt;
}
/**
 * @brief Parse config retrieve message. Note that only one message
 * would be parsed if there are multiple messages in the input buffer
 * @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param config_retrieve_processor A callback function to deal with the
 * data after parsing e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_config_retrieve_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t config_retrieve_processor,
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
  protobuf_codec_config_retrieve_t _config_retrieve;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint8_t _sensor_id_1_buf[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];

  *Froto_header = &(SKF_Froto_App->_messages.config_hash_upload.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_config_retrieve_tag))
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
      SKF_Froto_App->_messages.config_retrieve.sensor_id.arg =
        &_sensor_id_1;
      SKF_Froto_App->_messages.config_retrieve.sensor_id.funcs.decode =
        &protobuf_dec_bytes_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_ConfigurationAndCommand_ConfigRetrieve_fields,
                           &SKF_Froto_App->_messages.config_retrieve))
      {
        DBG_LOG_INFO("Froto APP config_retrieve appVer: %d",
                      SKF_Froto_App->_messages.config_retrieve.appVer);
        DBG_LOG_DEBUG("Froto APP config_retrieve has header: %d",
                      SKF_Froto_App->_messages.config_retrieve.
                      has_header);
        if(SKF_Froto_App->_messages.config_retrieve.has_header == true)
        {
          DBG_LOG_INFO("Froto APP config_retrieve header version: %d",
                        (*Froto_header)->version);
          DBG_LOG_DEBUG(
            "Froto APP config_retrieve header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_DEBUG(
            "Froto APP config_retrieve header which_peer_addr: %d",
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
              "Froto APP config_retrieve header gateway id:");
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
          DBG_LOG_DEBUG("Froto APP config_retrieve header is up: %d",
                        (*Froto_header)->is_up);
          DBG_LOG_INFO(
            "Froto APP config_retrieve header msg seq no: %d",
            (*Froto_header)->message_seq_no);
          DBG_LOG_DEBUG("Froto APP config_retrieve header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG(
            "Froto APP config_retrieve header primitive type: %d",
            (*Froto_header)->primitive_type);
          DBG_LOG_INFO(
            "Froto APP config_retrieve header message type: %d",
            (*Froto_header)->message_type);
          DBG_LOG_INFO(
            "Froto APP config_retrieve header total block: %d",
            (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP config_retrieve header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_DEBUG(
              "Froto APP config_retrieve header current block: %d",
              (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG(
            "Froto APP config_retrieve header long packet id: %d",
            (*Froto_header)->long_packet_id);
          DBG_LOG_DEBUG(
            "Froto APP config_retrieve header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG(
              "Froto APP config_retrieve header is big endian: %d",
              (*Froto_header)->is_big_endian);
          }
          DBG_LOG_DEBUG(
            "Froto APP config_retrieve header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
        }
        DBG_LOG_DEBUG("Froto APP config_retrieve has is big endian: %d",
                      SKF_Froto_App->_messages.config_retrieve.
                      has_is_big_endian);
        if(SKF_Froto_App->_messages.config_retrieve.has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG("Froto APP config_retrieve is big endian: %d",
                        SKF_Froto_App->_messages.config_retrieve.
                        is_big_endian);
        }

        DBG_LOG_DEBUG("Froto APP config_retrieve payload: %d",
                      SKF_Froto_App->_messages.config_retrieve.payload);
        if(SKF_Froto_App->_messages.config_retrieve.payload ==
           SKFChina_ConfigurationAndCommand_RetrievePayload_GATEWAY_CONFIG_FILE_CONFIG)
        {
          _config_retrieve.isGWConfigFile = true;
        }
        else
        {
          _config_retrieve.isGWConfigFile = false;
        }
        DBG_LOG_DEBUG(
          "Froto APP config_retrieve sensor id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              config_retrieve.sensor_id.
                                              arg))->buffer,
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              config_retrieve.sensor_id.
                                              arg))->length);
        _config_retrieve.sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->_messages.
                                              config_retrieve.sensor_id.
                                              arg))->length;
        if(_config_retrieve.sensorIDLen)
        {
          _config_retrieve.sensorID =
            l_malloc(_config_retrieve.sensorIDLen);
          memcpy(_config_retrieve.sensorID,
                 ((protobuf_codec_general_bytes_t *)(SKF_Froto_App->
                                                     _messages.
                                                     config_retrieve.
                                                     sensor_id.arg))->
                 buffer,
                 _config_retrieve.sensorIDLen);
        }
        else
        {
          _config_retrieve.sensorID = NULL;
        }

        // TODO: Not support specific config and fscheduler
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(config_retrieve_processor != NULL)
  {
    config_retrieve_processor(&_config_retrieve, _errCode);
  }

  l_free(_config_retrieve.sensorID);
  _config_retrieve.sensorID = NULL;
  _config_retrieve.sensorIDLen = 0;

  return _rt;
}
