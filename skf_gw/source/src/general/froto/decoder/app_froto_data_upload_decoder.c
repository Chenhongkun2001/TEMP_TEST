/**
 * @file    app_froto_data_upload_decoder.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Implementation of data upload message (of Froto) decoder.
 * @details
 */
#include "sys_def.h"
#include "froto/app_froto_new.h"
#include "froto/app_froto_common.h"
#include "froto/decoder/app_froto_data_upload_decoder.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

/**
 * @brief Parse data upload message. Note that only one message would be
 * parsed if there are multiple messages in the input buffer
 * @p readBuf .
 * @param readBuf Pointed to the read (undecoded) message
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * @param istream Pointer to the the input stream of nano-pb
 * @param SKF_Froto_App Pointer to the root message structure.
 * @param Froto_header Pointer to the Froto header, which would be pointed
 * to the required header in this routine.
 * @param data_upload_processor A callback function to deal with the parsed
 * data parsing (e.g., saving to database).
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_data_upload_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  pb_istream_t *istream,
  SKFChina_App_AppMessage *SKF_Froto_App,
  SKFChina_Froto_FrotoHeader **Froto_header,
  froto_after_parse_t data_upload_processor,
  uint8_t *errCode)
{
  bool _rt = true;
  pb_istream_t _substream;
  pb_wire_type_t _wire_type;
  uint32_t _tag;
  bool _eof;
  uint8_t _errCode;
  protobuf_codec_general_bytes_t _sensor_id;
  protobuf_codec_general_bytes_t _gateway_id;
  protobuf_codec_acked_seq_number_t _acked_seq_no_array;
  protobuf_codec_data_pair_t _data_pair_array;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint8_t _self_id[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];
  protobuf_codec_data_pair_ele_t _data_pair[FROTO_DATAPAIR_MAX_NUMBER];

  *Froto_header = &(SKF_Froto_App->_messages.data_upload.header);
  *istream = pb_istream_from_buffer(readBuf, readBufLen);
  while(pb_decode_tag(istream, &_wire_type, &_tag, &_eof))
  {
    if((_wire_type == PB_WT_STRING) &&
       (_tag == SKFChina_App_AppMessage_data_upload_tag))
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

      _data_pair_array.buffer = _data_pair;
      _data_pair_array.size = sizeof(_data_pair) /
                              sizeof(_data_pair[0]);
      _data_pair_array.cnt = 0;
      memset(_data_pair, 0, sizeof(_data_pair));
      SKF_Froto_App->_messages.data_upload.data_pair.arg =
        &_data_pair_array;
      SKF_Froto_App->_messages.data_upload.data_pair.funcs.decode =
        &protobuf_dec_data_pair_callback;

      if(true == pb_decode(&_substream,
                           SKFChina_SensingDataUpload_DataUpload_fields,
                           &SKF_Froto_App->_messages.data_upload))
      {
#if (0)
        DBG_LOG_DEBUG("Froto APP data_upload appVer: %d",
                      SKF_Froto_App->_messages.data_upload.appVer);
#endif
        DBG_LOG_DEBUG("Froto APP data_upload has header: %d",
                      SKF_Froto_App->_messages.data_upload.has_header);
        if(SKF_Froto_App->_messages.data_upload.has_header == true)
        {
#if (0)
          DBG_LOG_DEBUG("Froto APP data_upload header version: %d",
                        (*Froto_header)->version);
#endif
          DBG_LOG_DEBUG(
            "Froto APP data_upload header sensor id: ");
          util_dbg_buf_dump(
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            buffer,
            ((protobuf_codec_general_bytes_t *)((*Froto_header)->
                                                sensor_id.arg))->
            length);
          DBG_LOG_DEBUG(
            "Froto APP data_upload header which_peer_addr: %d",
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
              "Froto APP data_upload header gateway id:");
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
          DBG_LOG_DEBUG("Froto APP data_upload header is up: %d",
                        (*Froto_header)->is_up);
          DBG_LOG_INFO(
            "Froto APP data_upload header msg seq no: %d",
            (*Froto_header)->message_seq_no);
#if (0)
          DBG_LOG_DEBUG("Froto APP data_upload header ttl: %d",
                        (*Froto_header)->time_to_live);
          DBG_LOG_DEBUG("Froto APP data_upload header primitive type: %d",
                        (*Froto_header)->primitive_type);
#endif
          DBG_LOG_INFO("Froto APP data_upload header message type: %d",
                        (*Froto_header)->message_type);
          DBG_LOG_INFO("Froto APP data_upload header total block: %d",
                        (*Froto_header)->total_block);
          DBG_LOG_DEBUG(
            "Froto APP data_upload header has current block: %d",
            (*Froto_header)->has_current_block);
          if((*Froto_header)->has_current_block)
          {
            DBG_LOG_DEBUG("Froto APP data_upload header current block: %d",
                          (*Froto_header)->current_block);
          }
          DBG_LOG_DEBUG("Froto APP data_upload header long packet id: %d",
                        (*Froto_header)->long_packet_id);
#if (0)
          DBG_LOG_DEBUG(
            "Froto APP data_upload header has is big endian: %d",
            (*Froto_header)->has_is_big_endian);
          if((*Froto_header)->has_is_big_endian == true)
          {
            DBG_LOG_DEBUG("Froto APP data_upload header is big endian: %d",
                          (*Froto_header)->is_big_endian);
          }
          DBG_LOG_DEBUG(
            "Froto APP data_upload header acked msg seq number: ");
          util_dbg_buf_dump(
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->buffer,
            ((protobuf_codec_acked_seq_number_t *)((*Froto_header)->
                                                   acked_message_seq_number
                                                   .arg))->cnt);
#endif
        }
#if (0)
        DBG_LOG_DEBUG("Froto APP data_upload has is big endian: %d",
                      SKF_Froto_App->_messages.data_upload.
                      has_is_big_endian);
        if(SKF_Froto_App->_messages.data_upload.has_is_big_endian ==
           true)
        {
          DBG_LOG_DEBUG("Froto APP data_upload is big endian: %d",
                        SKF_Froto_App->_messages.data_upload.
                        is_big_endian);
        }
#endif
        DBG_LOG_INFO("There are %d data pairs.", _data_pair_array.cnt);
        for(uint32_t i = 0; i < _data_pair_array.cnt; i++)
        {
          DBG_LOG_DEBUG("Data pair has_measurement %d",
                        _data_pair_array.buffer[i]
                        .has_measurement);
          if(_data_pair_array.buffer[i].has_measurement)
          {
            DBG_LOG_INFO("Data pair meas meas type %d",
                          _data_pair_array.buffer[i].measurement.
                          measureType);
#if (0)
            DBG_LOG_DEBUG("Data pair meas has sensor type %d",
                          _data_pair_array.buffer[i].measurement.
                          has_sensor_type);

            if(_data_pair_array.buffer[i].measurement.has_sensor_type)
            {
              DBG_LOG_DEBUG("Data pair meas sensor type %d",
                            _data_pair_array.buffer[i].measurement.
                            sensorType);
            }
            DBG_LOG_DEBUG("Data pair meas meas product type %d",
                          _data_pair_array.buffer[i].measurement.
                          productType);
            DBG_LOG_DEBUG("Data pair meas has range %d",
                          _data_pair_array.buffer[i].measurement.
                          has_range);

            if(_data_pair_array.buffer[i].measurement.has_range)
            {
              DBG_LOG_DEBUG("Data pair meas range %d",
                            _data_pair_array.buffer[i].measurement.
                            range);
            }
            DBG_LOG_DEBUG("Data pair meas has unit %d",
                          _data_pair_array.buffer[i].measurement.
                          has_unit);

            if(_data_pair_array.buffer[i].measurement.has_unit)
            {
              DBG_LOG_DEBUG("Data pair meas unit %d",
                            _data_pair_array.buffer[i].measurement.
                            unit);
            }
            DBG_LOG_DEBUG("Data pair meas has sample rate hz %d",
                          _data_pair_array.buffer[i].measurement.
                          has_sample_rate_hz);

            if(_data_pair_array.buffer[i].measurement.
               has_sample_rate_hz)
            {
              DBG_LOG_DEBUG("Data pair meas sample rate hz %f",
                            _data_pair_array.buffer[i].measurement.
                            sample_rate_hz);
            }
            DBG_LOG_DEBUG("Data pair meas has sample period s %d",
                          _data_pair_array.buffer[i].measurement.
                          has_sample_period_s);

            if(_data_pair_array.buffer[i].measurement.
               has_sample_period_s)
            {
              DBG_LOG_DEBUG("Data pair meas sample period s %d",
                            _data_pair_array.buffer[i].measurement.
                            sample_period_s);
            }
            DBG_LOG_DEBUG("Data pair meas has sample dimension %d",
                          _data_pair_array.buffer[i].measurement.
                          has_sample_dimension);

            if(_data_pair_array.buffer[i].measurement.
               has_sample_dimension)
            {
              DBG_LOG_DEBUG("Data pair meas sample dimension 0x%x",
                            _data_pair_array.buffer[i].measurement.
                            sample_dimension);
            }
            DBG_LOG_DEBUG("Data pair meas has meas length %d",
                          _data_pair_array.buffer[i].measurement.
                          has_measure_length_sample);

            if(_data_pair_array.buffer[i].measurement.
               has_measure_length_sample)
            {
              DBG_LOG_DEBUG("Data pair meas meas length %d",
                            _data_pair_array.buffer[i].measurement.
                            measure_length_sample);
            }
            DBG_LOG_DEBUG(
              "Data pair meas has total data len sample %d",
              _data_pair_array.buffer[i].measurement.
              has_total_data_length_sample);

            if(_data_pair_array.buffer[i].measurement.
               has_total_data_length_sample)
            {
              DBG_LOG_DEBUG(
                "Data pair meas total data len sample %d",
                _data_pair_array.buffer[i].measurement.
                total_data_length_sample);
            }
            DBG_LOG_DEBUG("Data pair meas meas seq no %d",
                          _data_pair_array.buffer[i].measurement.
                          measure_seq_no);
            DBG_LOG_DEBUG("Data pair meas data format %d",
                          _data_pair_array.buffer[i].measurement.
                          data_format);
            DBG_LOG_DEBUG(
              "Data pair meas has crc32 %d",
              _data_pair_array.buffer[i].measurement.
              has_crc32_value);

            if(_data_pair_array.buffer[i].measurement.has_crc32_value)
            {
              DBG_LOG_DEBUG(
                "Data pair meas crc32 %d",
                _data_pair_array.buffer[i].measurement.
                crc32_value);
            }
            DBG_LOG_DEBUG(
              "Data pair meas has encryp %d",
              _data_pair_array.buffer[i].measurement.
              has_encryp);

            if(_data_pair_array.buffer[i].measurement.has_encryp)
            {
              DBG_LOG_DEBUG(
                "Data pair meas encryp %d",
                _data_pair_array.buffer[i].measurement.
                encryp);
            }
            DBG_LOG_DEBUG("Data pair meas sensor id");
            util_dbg_buf_dump(
              _data_pair_array.buffer[i].measurement.sensorID,
              _data_pair_array.buffer[i].measurement.sensorIDLen);
            DBG_LOG_DEBUG("Data pair meas sample time %lu",
                          _data_pair_array.buffer[i].measurement.
                          sample_time);
            DBG_LOG_DEBUG("Data pair meas memory id %d",
                          _data_pair_array.buffer[i].measurement.
                          memory_id);
            // TODO: not fully support AlarmAndNotifyInfo
            DBG_LOG_DEBUG("Data pair meas alarm %d",
                          _data_pair_array.buffer[i].measurement.
                          alarm);
            DBG_LOG_DEBUG("Data pair meas mac addr");
            util_dbg_buf_dump(
              _data_pair_array.buffer[i].measurement.mac_address,
              _data_pair_array.buffer[i].measurement.mac_addressLen);
            DBG_LOG_DEBUG("Data pair meas iccid");
            util_dbg_buf_dump(
              _data_pair_array.buffer[i].measurement.iccid,
              _data_pair_array.buffer[i].measurement.iccidLen);
#endif
            DBG_LOG_DEBUG("Data pair meas imei");
            util_dbg_buf_dump(
              _data_pair_array.buffer[i].measurement.imei,
              _data_pair_array.buffer[i].measurement.imeiLen);
          }
#if (0)
          DBG_LOG_DEBUG("Data pair data start point %d",
                        _data_pair_array.buffer[i].data.start_point);
          DBG_LOG_DEBUG("Data pair data crc32 %u",
                        _data_pair_array.buffer[i].data.crc32);
          DBG_LOG_DEBUG("Data pair data crc32_valid %d",
                        _data_pair_array.buffer[i].data.crc32_valid);
#endif
          DBG_LOG_DEBUG("Data pair data data");
          if(_data_pair_array.buffer[i].measurement.data_format ==
             SKFChina_Common_Format_FORMAT_UINT16)
          {
            DBG_LOG_DEBUG("%d\n",
                          *(uint16_t *)_data_pair_array.buffer[i].
                          data.data);
          }
          else if(_data_pair_array.buffer[i].measurement.data_format
                  == SKFChina_Common_Format_FORMAT_INT16)
          {
            DBG_LOG_DEBUG("%d\n",
                          *(int16_t *)_data_pair_array.buffer[i].data
                          .data);
          }
          else if(_data_pair_array.buffer[i].measurement.data_format
                  == SKFChina_Common_Format_FORMAT_UINT32)
          {
            DBG_LOG_DEBUG("%d\n",
                          *(uint32_t *)_data_pair_array.buffer[i].
                          data.data);
          }
          else if(_data_pair_array.buffer[i].measurement.data_format
                  == SKFChina_Common_Format_FORMAT_INT32)
          {
            DBG_LOG_DEBUG("%d\n",
                          *(int32_t *)_data_pair_array.buffer[i].data
                          .data);
          }
          else if(_data_pair_array.buffer[i].measurement.data_format
                  == SKFChina_Common_Format_FORMAT_UINT8)
          {
            DBG_LOG_DEBUG("%d\n",
                          *(uint8_t *)_data_pair_array.buffer[i].data
                          .data);
          }
          else if(_data_pair_array.buffer[i].measurement.data_format
                  == SKFChina_Common_Format_FORMAT_INT8)
          {
            DBG_LOG_DEBUG("%d\n",
                          *(int8_t *)_data_pair_array.buffer[i].data.
                          data);
          }
          else if(_data_pair_array.buffer[i].measurement.data_format
                  == SKFChina_Common_Format_FORMAT_FLOAT32)
          {
            DBG_LOG_DEBUG("%f\n",
                          *(float *)_data_pair_array.buffer[i].data.
                          data);
          }
          else if(_data_pair_array.buffer[i].measurement.data_format
                  == SKFChina_Common_Format_FORMAT_DOUBLE64)
          {
            DBG_LOG_DEBUG("%f\n",
                          *(double *)_data_pair_array.buffer[i].data.
                          data);
          }
          else
          {
            util_dbg_buf_dump(
              _data_pair_array.buffer[i].data.data,
              _data_pair_array.buffer[i].data.dataLen);
          }
        }
      }
      pb_close_string_substream(istream, &_substream);
    }
  }
  if(data_upload_processor != NULL)
  {
    data_upload_processor(&_data_pair_array, _errCode);
  }
  for(uint32_t i = 0; i < _data_pair_array.cnt; i++)
  {
    l_free(_data_pair_array.buffer[i].measurement.sensorID);
    _data_pair_array.buffer[i].measurement.sensorID = NULL;
    _data_pair_array.buffer[i].measurement.sensorIDLen = 0;
    l_free(_data_pair_array.buffer[i].measurement.mac_address);
    _data_pair_array.buffer[i].measurement.mac_address = NULL;
    _data_pair_array.buffer[i].measurement.mac_addressLen = 0;
    l_free(_data_pair_array.buffer[i].measurement.iccid);
    _data_pair_array.buffer[i].measurement.iccid = NULL;
    _data_pair_array.buffer[i].measurement.iccidLen = 0;
    l_free(_data_pair_array.buffer[i].measurement.imei);
    _data_pair_array.buffer[i].measurement.imei = NULL;
    _data_pair_array.buffer[i].measurement.imeiLen = 0;
    l_free(_data_pair_array.buffer[i].data.data);
    _data_pair_array.buffer[i].data.data = NULL;
    _data_pair_array.buffer[i].data.dataLen = 0;
  }
  return _rt;
}
