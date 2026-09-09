/**
 * @file    app_froto_new.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Implementation of general callbacks used by Froto protocol.
 * @details
 */
#include "sys_def.h"
#include "froto/app_froto_new.h"
#include "froto/app_froto_common.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

static uint32_t seq_no_counter;
/**
 * @brief To allocate an unique message sequence number (increasing by 1
 * once the routine is called)
 * @return An allocated message sequence number
 */
uint32_t
app_froto_allocate_msg_seq_no(void)
{
  seq_no_counter++;
  return seq_no_counter;
}

/**************************************************************/
/**********************CRC32 Calculation***********************/
/**************************************************************/
// Borrow code from
// https://blog.csdn.net/gongmin856/article/details/77101397
static const uint32_t g_crc32tab[] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL,
  0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
  0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L,
  0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L,
  0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L,
  0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL,
  0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L,
  0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L,
  0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L,
  0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
  0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L,
  0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL,
  0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL,
  0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL,
  0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L,
  0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L,
  0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL,
  0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
  0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL,
  0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L,
  0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL,
  0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L,
  0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L,
  0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L,
  0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L,
  0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,
  0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL,
  0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L,
  0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L,
  0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL,
  0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L,
  0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL,
  0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L,
  0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
  0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L,
  0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L,
  0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL,
  0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L,
  0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL,
  0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL,
  0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L,
  0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
  0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL,
  0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L,
  0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L,
  0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L,
  0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL,
  0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L,
  0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L,
  0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
  0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L,
  0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL
};

/**
 * @brief Caculate CRC32 value
 * @param buf: The input
 * @param size: The size (in byte) of the input buffer
 */
uint32_t
froto_crc32(
  const uint8_t *buf,
  uint32_t size)
{

  uint32_t i, crc;

  crc = 0xFFFFFFFF;
  for(i = 0; i < size; i++)
  {
    crc = g_crc32tab[(crc ^ buf[i]) & 0xff] ^ (crc >> 8);
  }

  return crc ^ 0xFFFFFFFF;
}
/**
 * @brief Encode general bytes
 */
bool
protobuf_enc_bytes_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg)
{
  if(arg != NULL)
  {
    if(!pb_encode_tag_for_field(stream_p, field))
    {
      return false;
    }

    protobuf_codec_general_bytes_t *temp_protobuf_encode_bytes =
      (protobuf_codec_general_bytes_t *)(*arg);

    if(!pb_encode_string(stream_p,
                         temp_protobuf_encode_bytes->buffer,
                         temp_protobuf_encode_bytes->size))
    {
      return false;
    }
    return true;
  }
  else
  {
    return false;
  }
}
/**
 * @brief Encode repeated measurement type
 */
bool
protobuf_enc_repeated_mtype_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg)
{
  bool rt = true;
  SKFChina_SensingDataUpload_MeasurementTypeMsg mType;
  protobuf_codec_dataselect_meastype_t *pt_buf =
    (protobuf_codec_dataselect_meastype_t *)(*arg);

  if(arg != NULL)
  {
    for(uint16_t cnt = 0; cnt < pt_buf->size; cnt++)
    {
      mType.measure_type = pt_buf->buffer[cnt].measureType;
      mType.has_dimension = pt_buf->buffer[cnt].has_dimension;
      if(mType.has_dimension == true)
      {
        mType.dimension = pt_buf->buffer[cnt].dimension;
      }
      rt = pb_encode_tag_for_field(stream_p, field);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode tag\r\n");
        return false;
      }

      rt = pb_encode_submessage(stream_p,
                                SKFChina_SensingDataUpload_MeasurementTypeMsg_fields,
                                &mType);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode submessage\r\n");
        return false;
      }
    }
    return true;
  }
  else
  {
    DBG_LOG_WARN("Arg error\r\n");
    return false;
  }
}
/**
 * @brief Encode repeated sensor type
 */
bool
protobuf_enc_repeated_stype_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg)
{
  bool rt = true;
  SKFChina_SensingDataUpload_SensorTypeMsg sType;
  protobuf_codec_general_bytes_t *pt_buf =
    (protobuf_codec_general_bytes_t *)(*arg);

  if(arg != NULL)
  {
    for(uint16_t cnt = 0; cnt < pt_buf->size; cnt++)
    {
      sType.sensor = pt_buf->buffer[cnt];
      rt = pb_encode_tag_for_field(stream_p, field);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode tag\r\n");
        return false;
      }

      rt = pb_encode_submessage(stream_p,
                                SKFChina_SensingDataUpload_MeasurementTypeMsg_fields,
                                &sType);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode submessage\r\n");
        return false;
      }
    }
    return true;
  }
  else
  {
    DBG_LOG_WARN("Arg error\r\n");
    return false;
  }
}
/**
 * @brief Encode repeated specific config item
 */
bool
protobuf_enc_repeated_specific_conf_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg)
{
  bool rt = true;
  SKFChina_ConfigurationAndCommand_SpecificConfigItemMsg configItem;
  protobuf_codec_config_item_t *pt_buf =
    (protobuf_codec_config_item_t *)(*arg);

  if(arg != NULL)
  {
    for(uint16_t cnt = 0; cnt < pt_buf->size; cnt++)
    {
      configItem.specific_config_item = pt_buf->buffer[cnt];
      rt = pb_encode_tag_for_field(stream_p, field);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode tag\r\n");
        return false;
      }

      rt = pb_encode_submessage(stream_p,
                                SKFChina_ConfigurationAndCommand_SpecificConfigItemMsg_fields,
                                &configItem);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode submessage\r\n");
        return false;
      }
    }
    return true;
  }
  else
  {
    DBG_LOG_WARN("Arg error\r\n");
    return false;
  }
}
/**
 * @brief Encode repeated fScheduler config item
 */
bool
protobuf_enc_repeated_fscheduler_conf_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg)
{
  bool rt = true;
  SKFChina_ConfigurationAndCommand_fSchedulerConfigItemMsg configItem;
  protobuf_codec_general_bytes_t *pt_buf =
    (protobuf_codec_general_bytes_t *)(*arg);

  if(arg != NULL)
  {
    for(uint16_t cnt = 0; cnt < pt_buf->size; cnt++)
    {
      configItem.fscheduler_config_item = pt_buf->buffer[cnt];
      rt = pb_encode_tag_for_field(stream_p, field);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode tag\r\n");
        return false;
      }

      rt = pb_encode_submessage(stream_p,
                                SKFChina_ConfigurationAndCommand_fSchedulerConfigItemMsg_fields,
                                &configItem);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode submessage\r\n");
        return false;
      }
    }
    return true;
  }
  else
  {
    DBG_LOG_WARN("Arg error\r\n");
    return false;
  }
}
/**
 * @brief Encode repeated config pair
 */
bool
protobuf_enc_repeated_conf_pair_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg)
{
  bool rt = true;
  SKFChina_ConfigurationAndCommand_ConfigPair configItem;
  protobuf_codec_config_pair_t *pt_buf =
    (protobuf_codec_config_pair_t *)(*arg);

  if(arg != NULL)
  {
    for(uint16_t cnt = 0; cnt < pt_buf->size; cnt++)
    {
      configItem.which_config_item =
        pt_buf->buffer[cnt].which_config_item;

      if(configItem.which_config_item ==
         SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag)
      {
        configItem.config_item.specific_config_item =
          pt_buf->buffer[cnt].config_item.specific_config_item;
      }
      else if(configItem.which_config_item ==
              SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag)
      {
        configItem.config_item.fscheduler_config_item =
          pt_buf->buffer[cnt].config_item.fscheduler_config_item;
      }

      configItem.has_memory_id =
        pt_buf->buffer[cnt].has_memory_id;
      if(configItem.has_memory_id)
      {
        configItem.memory_id = pt_buf->buffer[cnt].memory_id;
      }

      configItem.which_config_content =
        pt_buf->buffer[cnt].which_config_content;

      if(configItem.which_config_content ==
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        configItem.config_content.general_config_content_uint32 =
          pt_buf->buffer[cnt].content.content_uint32;
      }
      else if(configItem.which_config_content ==
              SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag)
      {
        configItem.config_content.general_config_content_int32 =
          pt_buf->buffer[cnt].content.content_int32;
      }
      else if(configItem.which_config_content ==
              SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        configItem.config_content.general_config_content_float =
          pt_buf->buffer[cnt].content.content_float;
      }
      else if(configItem.which_config_content ==
              SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        configItem.config_content.general_config_content_bool =
          pt_buf->buffer[cnt].content.content_bool;
      }
      else if(configItem.which_config_content ==
              SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
      {
        pt_buf->buffer[cnt].content.time.cnt = 0;
        configItem.config_content.time_config_content.time.arg =
          &pt_buf->buffer[cnt].content.time;

        configItem.config_content.time_config_content.time.funcs.encode
          = protobuf_enc_timearray_callback;
      }
      else if(configItem.which_config_content ==
              SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
      {
        configItem.config_content.work_mode_content.work_mode =
          pt_buf->buffer[cnt].content.work_mode.work_mode;

        protobuf_codec_general_bytes_t _param;

        _param.buffer =
          pt_buf->buffer[cnt].content.work_mode.parameters;
        _param.size =
          pt_buf->buffer[cnt].content.work_mode.parametersLen;
        _param.length =
          pt_buf->buffer[cnt].content.work_mode.parametersLen;
        configItem.config_content.work_mode_content.parameter.arg =
          &_param;
        configItem.config_content.work_mode_content.parameter.funcs.encode
          = protobuf_enc_bytes_callback;
      }
      else if(configItem.which_config_content ==
              SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
      {
        configItem.config_content.gateway_info_content.name_id =
          pt_buf->buffer[cnt].content.gateway_info.name_id;

        protobuf_codec_general_bytes_t _mac_addr;

        _mac_addr.buffer =
          pt_buf->buffer[cnt].content.gateway_info.mac_addr;
        _mac_addr.size =
          pt_buf->buffer[cnt].content.gateway_info.mac_addrLen;
        _mac_addr.length =
          pt_buf->buffer[cnt].content.gateway_info.mac_addrLen;
        configItem.config_content.gateway_info_content.mac_address.arg =
          &_mac_addr;
        configItem.config_content.gateway_info_content.mac_address.funcs.
        encode
          = protobuf_enc_bytes_callback;
      }

      rt = pb_encode_tag_for_field(stream_p, field);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode tag\r\n");
        return false;
      }
      rt = pb_encode_submessage(stream_p,
                                SKFChina_ConfigurationAndCommand_ConfigPair_fields,
                                &configItem);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode submessage\r\n");
        return false;
      }
    }
    return true;
  }
  else
  {
    DBG_LOG_WARN("Arg error\r\n");
    return false;
  }
}
/**
 * @brief Encode time array
 */
bool
protobuf_enc_timearray_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg) {
  bool rt = true;
  SKFChina_ConfigurationAndCommand_TimeArrayElement time_ele;
  protobuf_codec_timearray_t *t =
    (protobuf_codec_timearray_t *)(*arg);

  if(arg != NULL)
  {
    for(uint16_t cnt = 0; cnt < t->size; cnt++)
    {
      time_ele.time = t->time[cnt];
      rt = pb_encode_tag_for_field(stream_p, field);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode tag\r\n");
        return false;
      }

      rt = pb_encode_submessage(stream_p,
                                SKFChina_ConfigurationAndCommand_TimeArrayElement_fields,
                                &time_ele);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to encode submessage\r\n");
        return false;
      }
    }
    return true;
  }
  else
  {
    DBG_LOG_WARN("Arg error\r\n");
    return false;
  }
}
/**
 * @brief Decode general bytes
 */
bool
protobuf_dec_bytes_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg) {
  if(arg != NULL)
  {
    protobuf_codec_general_bytes_t *temp_protobuf_decode_bytes =
      (protobuf_codec_general_bytes_t *)(*arg);

    if(stream_p->bytes_left > temp_protobuf_decode_bytes->size)
    {
      return false; /* overflow */
    }
    temp_protobuf_decode_bytes->length = stream_p->bytes_left;
    if(pb_read(stream_p, temp_protobuf_decode_bytes->buffer,
               stream_p->bytes_left))
    {
      return true;
    }
  }

  return false;
}
/**
 * @brief Decode acked msg seq number
 */
bool
protobuf_dec_acked_seq_number_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg) {
  bool rt;
  pb_istream_t _stream = pb_istream_from_buffer(stream_p->state,
                                                stream_p->bytes_left);
  SKFChina_Froto_SeqNoElement ack_seq;
  protobuf_codec_acked_seq_number_t *ack_seq_array =
    (protobuf_codec_acked_seq_number_t *)(*arg);

  if(arg != NULL)
  {
    rt = pb_decode(&_stream,
                   SKFChina_Froto_SeqNoElement_fields,
                   &ack_seq);
    if(rt == false)
    {
      DBG_LOG_WARN("Failed to decode field\r\n");
      return false;
    }

    if(ack_seq_array->cnt < ack_seq_array->size)
    {
      ack_seq_array->buffer[ack_seq_array->cnt] = ack_seq.seqNo;
      ack_seq_array->cnt++;
    }
    return true;
  }
  else
  {
    DBG_LOG_WARN("Arg error\r\n");
    return false;
  }

  return false;
}
/**
 * @brief Decode time array
 */
bool
protobuf_dec_timearray_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg) {
  bool rt;
  pb_istream_t _stream = pb_istream_from_buffer(stream_p->state,
                                                stream_p->bytes_left);
  SKFChina_ConfigurationAndCommand_TimeArrayElement time_ele;
  protobuf_codec_timearray_t *t =
    (protobuf_codec_timearray_t *)(*arg);

  if(arg != NULL)
  {
    rt = pb_decode(&_stream,
                   SKFChina_ConfigurationAndCommand_TimeArrayElement_fields,
                   &time_ele);
    if(rt == false)
    {
      DBG_LOG_WARN("Failed to decode field\r\n");
      return false;
    }

    if(t->cnt < t->size)
    {
      t->time[t->cnt] = time_ele.time;
      t->cnt++;
    }
    return true;
  }
  else
  {
    DBG_LOG_WARN("Arg error\r\n");
    return false;
  }

  return false;
}
/**
 * @brief Decode data pair
 */
bool
protobuf_dec_data_pair_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg) {
  bool rt;
  pb_istream_t _stream = pb_istream_from_buffer(stream_p->state,
                                                stream_p->bytes_left);
  SKFChina_SensingDataUpload_DataPair data_pair;
  protobuf_codec_data_pair_t *data_pair_array =
    (protobuf_codec_data_pair_t *)(*arg);
  protobuf_codec_general_bytes_t id;
  uint8_t id_buf[MAX_ID_STR_LENGTH];
  protobuf_codec_general_bytes_t mac_address;
  uint8_t mac_address_buf[MAX_ID_STR_LENGTH];
  protobuf_codec_general_bytes_t iccid;
  uint8_t iccid_buf[MAX_ID_STR_LENGTH];
  protobuf_codec_general_bytes_t imei;
  uint8_t imei_buf[MAX_ID_STR_LENGTH];
  protobuf_codec_general_bytes_t _data;
  uint8_t _data_buf[FROTO_MAX_DATA_LENGTH];

  if(arg != NULL)
  {
    id.buffer = id_buf;
    id.size = sizeof(id_buf);
    id.length = 0;
    data_pair.measurement.sensor_id.arg = (void *)&id;
    data_pair.measurement.sensor_id.funcs.decode =
      protobuf_dec_bytes_callback;

    mac_address.buffer = mac_address_buf;
    mac_address.size = sizeof(mac_address_buf);
    mac_address.length = 0;
    data_pair.measurement.mac_address.arg = (void *)&mac_address;
    data_pair.measurement.mac_address.funcs.decode =
      protobuf_dec_bytes_callback;

    iccid.buffer = iccid_buf;
    iccid.size = sizeof(iccid_buf);
    iccid.length = 0;
    data_pair.measurement.iccid.arg = (void *)&iccid;
    data_pair.measurement.iccid.funcs.decode =
      protobuf_dec_bytes_callback;

    imei.buffer = imei_buf;
    imei.size = sizeof(imei_buf);
    imei.length = 0;
    data_pair.measurement.imei.arg = (void *)&imei;
    data_pair.measurement.imei.funcs.decode =
      protobuf_dec_bytes_callback;

    _data.buffer = _data_buf;
    _data.size = sizeof(_data_buf);
    _data.length = 0;
    data_pair.measurement_data.data.arg = (void *)&_data;
    data_pair.measurement_data.data.funcs.decode =
      protobuf_dec_bytes_callback;

    rt = pb_decode(&_stream,
                   SKFChina_SensingDataUpload_DataPair_fields,
                   &data_pair);
    if(rt == false)
    {
      DBG_LOG_WARN("Failed to decode field\r\n");
      return false;
    }

    if(data_pair_array->cnt < data_pair_array->size)
    {
      data_pair_array->buffer[data_pair_array->cnt].has_measurement =
        data_pair.has_measurement;
      if(data_pair.has_measurement)
      {
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        measureType = data_pair.measurement.measure_type;
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        has_sensor_type = data_pair.measurement.has_sensor;
        if(data_pair.measurement.has_sensor)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          sensorType = data_pair.measurement.sensor;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        productType = data_pair.measurement.product;
        data_pair_array->buffer[data_pair_array->cnt].measurement.has_range
          = data_pair.measurement.has_range;
        if(data_pair.measurement.has_range)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.range =
            data_pair.measurement.range;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.has_unit
          = data_pair.measurement.has_unit;
        if(data_pair.measurement.has_unit)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.unit =
            data_pair.measurement.unit;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        has_sample_rate_hz = data_pair.measurement.has_sample_rate_hz;
        if(data_pair.measurement.has_sample_rate_hz)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          sample_rate_hz =
            data_pair.measurement.sample_rate_hz;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        has_sample_period_s = data_pair.measurement.has_sample_period_s;
        if(data_pair.measurement.has_sample_period_s)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          sample_period_s = data_pair.measurement.sample_period_s;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        has_sample_dimension = data_pair.measurement.has_dimension;
        if(data_pair.measurement.has_dimension)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          sample_dimension = (uint8_t)data_pair.measurement.dimension;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        has_measure_length_sample =
          data_pair.measurement.has_measure_length_sample;
        if(data_pair.measurement.has_measure_length_sample)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          measure_length_sample =
            data_pair.measurement.measure_length_sample;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        has_total_data_length_sample =
          data_pair.measurement.has_total_data_length_sample;
        if(data_pair.measurement.has_total_data_length_sample)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          total_data_length_sample =
            data_pair.measurement.total_data_length_sample;
        }

        data_pair_array->buffer[data_pair_array->cnt].measurement.
        measure_seq_no = data_pair.measurement.measure_seq_no;
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        data_format = data_pair.measurement.data_format;
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        has_crc32_value = data_pair.measurement.has_crc32_value;
        if(data_pair.measurement.has_crc32_value)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          crc32_value = data_pair.measurement.crc32_value;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        has_encryp = data_pair.measurement.has_encryp;
        if(data_pair.measurement.has_encryp)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.encryp
            = data_pair.measurement.encryp;
        }

        data_pair_array->buffer[data_pair_array->cnt].measurement.
        sensorIDLen =
          ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                              sensor_id.arg))->length;
        if(data_pair_array->buffer[data_pair_array->cnt].measurement.
           sensorIDLen)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          sensorID
            = l_malloc(
                ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                                    sensor_id.arg))->length);
          memcpy(
            data_pair_array->buffer[data_pair_array->cnt].measurement.
            sensorID,
            ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                                sensor_id.arg))->buffer,
            data_pair_array->buffer[data_pair_array->cnt].measurement.
            sensorIDLen);
        }
        else
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          sensorID = NULL;
        }

        data_pair_array->buffer[data_pair_array->cnt].measurement.
        sample_time = data_pair.measurement.sample_time;
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        has_memory_id = data_pair.measurement.has_memory_id;
        if(data_pair.measurement.has_memory_id)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          memory_id = data_pair.measurement.memory_id;
        }
        // TODO: Do not fully support alarm_or_notify ...
        if(data_pair.measurement.has_alarm_or_notify)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          alarm = data_pair.measurement.alarm_or_notify.measurement;
        }else{
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          alarm = SKFChina_Common_AlarmAndNotify_UNKNOWN_ALARM_EVENT;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        mac_addressLen =
          ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                              mac_address.arg))->length;
        if(data_pair_array->buffer[data_pair_array->cnt].measurement.
           mac_addressLen)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          mac_address
            = l_malloc(
                ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                                    mac_address.arg))->
                length);
          memcpy(
            data_pair_array->buffer[data_pair_array->cnt].measurement.
            mac_address,
            ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                                mac_address.arg))->buffer,
            data_pair_array->buffer[data_pair_array->cnt].measurement.
            mac_addressLen);
        }
        else
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          mac_address = NULL;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        iccidLen =
          ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                              iccid.arg))->length;
        if(data_pair_array->buffer[data_pair_array->cnt].measurement.
           iccidLen)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          iccid
            = l_malloc(
                ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                                    iccid.arg))->
                length);
          memcpy(
            data_pair_array->buffer[data_pair_array->cnt].measurement.
            iccid,
            ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                                iccid.arg))->buffer,
            data_pair_array->buffer[data_pair_array->cnt].measurement.
            iccidLen);
        }
        else
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          iccid = NULL;
        }
        data_pair_array->buffer[data_pair_array->cnt].measurement.
        imeiLen =
          ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                              imei.arg))->length;
        if(data_pair_array->buffer[data_pair_array->cnt].measurement.
           imeiLen)
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          imei
            = l_malloc(
                ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                                    imei.arg))->
                length);
          memcpy(
            data_pair_array->buffer[data_pair_array->cnt].measurement.
            imei,
            ((protobuf_codec_general_bytes_t *)(data_pair.measurement.
                                                imei.arg))->buffer,
            data_pair_array->buffer[data_pair_array->cnt].measurement.
            imeiLen);
        }
        else
        {
          data_pair_array->buffer[data_pair_array->cnt].measurement.
          imei = NULL;
        }
      }
      if(data_pair.has_measurement_data)
      {
        data_pair_array->buffer[data_pair_array->cnt].data.start_point =
          data_pair.measurement_data.start_point;
        data_pair_array->buffer[data_pair_array->cnt].data.crc32 =
          data_pair.measurement_data.crc32_value;
        data_pair_array->buffer[data_pair_array->cnt].data.dataLen =
          ((protobuf_codec_general_bytes_t *)(data_pair.measurement_data.
                                              data.arg))->length;
        data_pair_array->buffer[data_pair_array->cnt].data.crc32_valid =
          false;
        if(data_pair_array->buffer[data_pair_array->cnt].data.dataLen)
        {
          data_pair_array->buffer[data_pair_array->cnt].data.data
            = l_malloc(
                ((protobuf_codec_general_bytes_t *)(data_pair.
                                                    measurement_data.data.
                                                    arg))->length);
          memcpy(
            data_pair_array->buffer[data_pair_array->cnt].data.data,
            ((protobuf_codec_general_bytes_t *)(data_pair.measurement_data.
                                                data.arg))->buffer,
            data_pair_array->buffer[data_pair_array->cnt].data.dataLen);
          if(data_pair.measurement_data.crc32_value ==
             froto_crc32(
               data_pair_array->buffer[data_pair_array->cnt].data.data,
               data_pair_array->buffer[data_pair_array->cnt].data.dataLen))
          {
            data_pair_array->buffer[data_pair_array->cnt].data.crc32_valid
              = true;
          }
        }
        else
        {
          data_pair_array->buffer[data_pair_array->cnt].data.data = NULL;
        }
      }
      data_pair_array->cnt++;
    }
    return true;
  }
  else
  {
    DBG_LOG_WARN("Arg error\r\n");
    return false;
  }

  return false;
}
/**
 * @brief Decode config pair
 */
bool
protobuf_dec_config_pair_callback(
  pb_istream_t *stream_p,
  const pb_field_t *field,
  void **arg) {
  bool rt;
  pb_istream_t _stream_before_decode;
  pb_istream_t _stream = pb_istream_from_buffer(stream_p->state,
                                                stream_p->bytes_left);
  SKFChina_ConfigurationAndCommand_ConfigPair config_pair;
  protobuf_codec_config_pair_t *config_pair_array =
    (protobuf_codec_config_pair_t *)(*arg);

  if(arg != NULL)
  {
    if(config_pair_array->cnt < config_pair_array->size)
    {
      memcpy(&_stream_before_decode, &_stream, sizeof(pb_istream_t));
      rt = pb_decode(&_stream,
                     SKFChina_ConfigurationAndCommand_ConfigPair_fields,
                     &config_pair);
      if(rt == false)
      {
        DBG_LOG_WARN("Failed to decode field\r\n");
        return false;
      }
      config_pair_array->buffer[config_pair_array->cnt].which_config_item =
        config_pair.which_config_item;
      if(config_pair.which_config_item ==
         SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag)
      {
        config_pair_array->buffer[config_pair_array->cnt].config_item.
        specific_config_item =
          config_pair.config_item.specific_config_item;
      }
      else if(config_pair.which_config_item ==
              SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag)
      {
        config_pair_array->buffer[config_pair_array->cnt].config_item.
        fscheduler_config_item =
          config_pair.config_item.fscheduler_config_item;
      }
      else
      {
        DBG_LOG_WARN("which_config_item is invalid\r\n");
        return false;
      }

      config_pair_array->buffer[config_pair_array->cnt].has_memory_id =
        config_pair.has_memory_id;
      if(config_pair.has_memory_id)
      {
        config_pair_array->buffer[config_pair_array->cnt].memory_id =
          config_pair.memory_id;
      }
      else
      {
        config_pair_array->buffer[config_pair_array->cnt].memory_id = 0;
      }

      config_pair_array->buffer[config_pair_array->cnt].
      which_config_content
        = config_pair.which_config_content;
      if(config_pair.which_config_content ==
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        config_pair_array->buffer[config_pair_array->cnt].content.
        content_uint32 =
          config_pair.config_content.general_config_content_uint32;
      }
      else if(config_pair.which_config_content ==
              SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag)
      {
        config_pair_array->buffer[config_pair_array->cnt].content.
        content_int32 =
          config_pair.config_content.general_config_content_int32;
      }
      else if(config_pair.which_config_content ==
              SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        config_pair_array->buffer[config_pair_array->cnt].content.
        content_float =
          config_pair.config_content.general_config_content_float;
      }
      else if(config_pair.which_config_content ==
              SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        config_pair_array->buffer[config_pair_array->cnt].content.
        content_bool =
          config_pair.config_content.general_config_content_bool;
      }
      else
      {
        // Need re-decode as a message
        if(config_pair.which_config_content ==
           SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
        {
          pb_wire_type_t _wire_type;
          uint32_t _tag;
          bool _eof;
          pb_istream_t _substream;
          protobuf_codec_timearray_t t;
          uint64_t _time[FROTO_TIME_MAX_NUMBER];

          t.time = _time;
          t.size = sizeof(_time) / sizeof(_time[0]);
          t.cnt = 0;
          config_pair.config_content.time_config_content.time.arg = &t;
          config_pair.config_content.time_config_content.time.funcs.decode
            = protobuf_dec_timearray_callback;

          memcpy(&_stream, &_stream_before_decode, sizeof(pb_istream_t));
          while(pb_decode_tag(&_stream, &_wire_type, &_tag, &_eof))
          {
            if((_wire_type == PB_WT_STRING) &&
               (_tag ==
                SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag))
            {
              pb_make_string_substream(&_stream, &_substream);
              if(true == pb_decode(&_substream,
                                   SKFChina_ConfigurationAndCommand_TimeArray_fields,
                                   &config_pair.config_content.
                                   time_config_content.time))
              {
                if(t.cnt)
                {
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .time.time = l_malloc(t.cnt * sizeof(t.time[0]));
                  memcpy(
                    config_pair_array->buffer[config_pair_array->cnt].
                    content.time.time, t.time, t.cnt * sizeof(t.time[0]));
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .time.cnt = t.cnt;
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .time.size = t.cnt;
                }
                else
                {
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .time.time = NULL;
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .time.cnt = 0;
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .time.size = 0;
                }
              }
              pb_close_string_substream(&_stream, &_substream);
            }
          }
        }
        else if(config_pair.which_config_content ==
                SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
        {
          pb_wire_type_t _wire_type;
          uint32_t _tag;
          bool _eof;
          pb_istream_t _substream;
          protobuf_codec_general_bytes_t _parameters;
          uint8_t _parameter_buf[FROTO_WORKMODE_PARAM_MAX_NUMBER];

          _parameters.buffer = _parameter_buf;
          _parameters.size = sizeof(_parameter_buf);
          _parameters.length = 0;
          config_pair.config_content.work_mode_content.parameter.arg =
             &_parameters;
          config_pair.config_content.work_mode_content.parameter.funcs.
           decode = protobuf_dec_bytes_callback;

          memcpy(&_stream, &_stream_before_decode, sizeof(pb_istream_t));
          while(pb_decode_tag(&_stream, &_wire_type, &_tag, &_eof))
          {
            if((_wire_type == PB_WT_STRING) &&
               (_tag ==
                SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag))
            {
              pb_make_string_substream(&_stream, &_substream);
              if(true == pb_decode(&_substream,
                                   SKFChina_ConfigurationAndCommand_WorkModePair_fields,
                                   &config_pair.config_content.
                                   work_mode_content))
              {
                config_pair_array->buffer[config_pair_array->cnt].content.
                work_mode.work_mode =
                  config_pair.config_content.work_mode_content.work_mode;
                if(_parameters.length)
                {
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .work_mode.parameters =
                    l_malloc(_parameters.length);
                  memcpy(
                    config_pair_array->buffer[config_pair_array->cnt].
                    content.work_mode.parameters,
                    _parameters.buffer,
                    _parameters.length);
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .work_mode.parametersLen = _parameters.length;
                }
                else
                {
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .work_mode.parameters = NULL;
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .work_mode.parametersLen = 0;
                }
              }
              pb_close_string_substream(&_stream, &_substream);
            }
          }
        }
        else if(config_pair.which_config_content ==
                SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
        {
          pb_wire_type_t _wire_type;
          uint32_t _tag;
          bool _eof;
          pb_istream_t _substream;
          protobuf_codec_general_bytes_t _mac;
          uint8_t _mac_addr[MAX_ID_STR_LENGTH];

          _mac.buffer = _mac_addr;
          _mac.size = sizeof(_mac_addr);
          _mac.length = 0;
          config_pair.config_content.gateway_info_content.mac_address.arg =
            &_mac;
          config_pair.config_content.gateway_info_content.mac_address.funcs
          .decode = protobuf_dec_bytes_callback;

          memcpy(&_stream, &_stream_before_decode, sizeof(pb_istream_t));
          while(pb_decode_tag(&_stream, &_wire_type, &_tag, &_eof))
          {
            if((_wire_type == PB_WT_STRING) &&
               (_tag ==
                SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag))
            {
              pb_make_string_substream(&_stream, &_substream);
              if(true == pb_decode(&_substream,
                                   SKFChina_ConfigurationAndCommand_GatewayInfoPair_fields,
                                   &config_pair.config_content.
                                   gateway_info_content))
              {
                config_pair_array->buffer[config_pair_array->cnt].content.
                gateway_info.name_id =
                  config_pair.config_content.gateway_info_content.name_id;
                if(_mac.length)
                {
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .gateway_info.mac_addr =
                    l_malloc(_mac.length);
                  memcpy(
                    config_pair_array->buffer[config_pair_array->cnt].
                    content.gateway_info.mac_addr,
                    _mac.buffer,
                    _mac.length);
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .gateway_info.mac_addrLen =
                    _mac.length;
                }
                else
                {
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .gateway_info.mac_addr = NULL;
                  config_pair_array->buffer[config_pair_array->cnt].content
                  .gateway_info.mac_addrLen = 0;
                }
              }
              pb_close_string_substream(&_stream, &_substream);
            }
          }
        }
        else
        {
          // Do nothing ...
        }
      }
      config_pair_array->cnt++;
    }
    return true;
  }
  else
  {
    DBG_LOG_WARN("Arg error\r\n");
    return false;
  }

  return false;
}
