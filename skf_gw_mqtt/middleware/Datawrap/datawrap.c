
#include <stdio.h>
#include "global.h"
#include "datawrap.h"
DBG_LOCAL_LOG_INFO

/*!
 * brief pb encode bytes callback.
 *
 * pb encode bytes callback.
 *
 * param stream_p
 * param field
 * param arg
 * return none
 */
static bool protobuf_encode_bytes_callback(pb_ostream_t *stream_p,
                                           const pb_field_t *field,
                                           void *const *arg) {
  if (arg != NULL) {
    if (!pb_encode_tag_for_field(stream_p, field)) {
      return false;
    }

    protobuf_encode_general_bytes_t *temp_protobuf_encode_bytes =
        (protobuf_encode_general_bytes_t *)(*arg);
    if (!pb_encode_string(stream_p, temp_protobuf_encode_bytes->buffer,
                          temp_protobuf_encode_bytes->size)) {
      return false;
    }
    return true;
  } else {
    return false;
  }
}
/*!
 * brief pb decode bytes callback.
 *
 * pb decode bytes callback.
 *
 * param stream_p
 * param field
 * param arg
 * return none
 */
static bool protobuf_decode_bytes_callback(pb_istream_t *stream_p,
                                           const pb_field_t *field,
                                           void **arg) {
  if (arg != NULL) {
    protobuf_encode_general_bytes_t *temp_protobuf_decode_bytes =
        (protobuf_encode_general_bytes_t *)(*arg);
    if (stream_p->bytes_left > temp_protobuf_decode_bytes->size)
      return false; /* overflow */
    if (pb_read(stream_p, temp_protobuf_decode_bytes->buffer,
                stream_p->bytes_left))
      return true;
  }

  return false;
}

bool protobuf_encode_bytes(pb_callback_t *var,
                           protobuf_encode_general_bytes_t *arg) {
  if(var == NULL)
    return false;

  var->funcs.encode = &protobuf_encode_bytes_callback;
  var->arg = arg;
  return true;
}

bool protobuf_decode_bytes(pb_callback_t *var,
                           protobuf_encode_general_bytes_t *arg) {
  if(var == NULL)
    return false;

  var->funcs.decode = &protobuf_decode_bytes_callback;
  var->arg = arg;
  return true;
}
const pb_msgdesc_t *decode_unionmessage_type(pb_istream_t *stream,
                                             const pb_msgdesc_t *desc) {
  pb_wire_type_t wire_type;
  uint32_t tag;
  bool eof;

  while (pb_decode_tag(stream, &wire_type, &tag, &eof)) {
    if (wire_type == PB_WT_STRING) {
      pb_field_iter_t iter;
      if (pb_field_iter_begin(&iter, desc, NULL) &&
          pb_field_iter_find(&iter, tag)) {
        /* Found our field. */
        return iter.submsg_desc;
      }
    }

    /* Wasn't our field.. */
    pb_skip_field(stream, wire_type);
  }

  return NULL;
}

void decode_one_filed(pb_istream_t *stream, uint8_t target_tag,
                      const pb_msgdesc_t *tar_fields, void *dest_struct) {
  pb_wire_type_t wire_type;
  uint32_t tag;
  bool eof;
  bool status = false;
  pb_istream_t sub_istream;
  do {
    if (true == (status = pb_decode_tag(stream, &wire_type, &tag, &eof))) {
      if (PB_WT_STRING == wire_type) {
        if (tag == target_tag) {
          if (true == pb_make_string_substream(stream, &sub_istream)) {
            if (true == pb_decode(&sub_istream, tar_fields, dest_struct)) {
              pb_close_string_substream(stream, &sub_istream);
              break;
            }else{
              LOG_WARN(OUTPOINT, "Failed to decode: %s\r\n", sub_istream.errmsg);
            }
            pb_close_string_substream(stream, &sub_istream);
          }
        }
      }
    }
  } while (true == status);
}

bool decode_submessage_contents(pb_istream_t *stream,
                                const pb_msgdesc_t *messagetype,
                                void *dest_struct) {
  pb_istream_t substream;
  bool status;
  if (!pb_make_string_substream(stream, &substream))
    return false;
  status = pb_decode(&substream, messagetype, dest_struct);
  pb_close_string_substream(stream, &substream);
  return status;
}

bool encode_submessage(pb_ostream_t *stream, const pb_msgdesc_t *parent_filed,
                       const pb_msgdesc_t *filed, void *message) {
  pb_field_iter_t iter;

  if (!pb_field_iter_begin(&iter, parent_filed, message))
    return false;

  do {
    if (iter.submsg_desc == filed) {
      /* This is our field, encode the message using it. */
      if (!pb_encode_tag_for_field(stream, &iter))
        return false;

      return pb_encode_submessage(stream, filed, message);
    }
  } while (pb_field_iter_next(&iter));

  /* Didn't find the field for messagetype */
  return false;
}

void decode_froto_header(pb_istream_t *stream,
                         SKFChina_Froto_FrotoHeader *header, uint8_t *sensor_id,
                         uint16_t sensor_id_size, uint8_t *gateway_id,
                         uint16_t gateway_id_size,
                         uint8_t *acked_message_seq_no, uint16_t seq_no_size) {
  // decode the sensor id from header
  protobuf_encode_general_bytes_t id_arg;
  id_arg.buffer = sensor_id;
  id_arg.size = sensor_id_size;
  protobuf_decode_bytes(&(header->sensor_id), &id_arg);
  const pb_msgdesc_t *type =
      decode_unionmessage_type(stream, SKFChina_App_AppMessage_fields);
  pb_istream_t substream;
  pb_make_string_substream(stream, &substream);
  type = decode_unionmessage_type(&substream, type);
  if (!decode_submessage_contents(&substream, type, header)) {
    LOG_WARN(OUTPOINT, "decode header failed\r\n");
  }
  pb_close_string_substream(stream, &substream);
}

void encode_froto_header(pb_ostream_t *stream, const pb_msgdesc_t *parent_filed,
                         const pb_msgdesc_t *filed, void *message) {
  pb_ostream_t substream;
  if (!pb_make_string_substream(stream, &substream))
    return;
  LOG_INFO(OUTPOINT, "encode firmware update notification ... \r\n");
  if (false == encode_submessage(&substream, parent_filed, filed, message)) {
    LOG_WARN(OUTPOINT, "failed\r\n");
    return;
  }
  pb_close_string_substream(stream, &substream);
}
