#ifndef DATAWRAP_H
#define DATAWRAP_H

#include "common.h"
#include "downmsg.h"
#include "upmsg.h"

bool protobuf_encode_bytes(pb_callback_t *var, protobuf_encode_general_bytes_t *arg);
bool protobuf_decode_bytes(pb_callback_t *var, protobuf_encode_general_bytes_t *arg);
void decode_froto_header(pb_istream_t *stream, SKFChina_Froto_FrotoHeader *header, uint8_t *sensor_id, uint16_t sensor_id_size,
                         uint8_t *gateway_id, uint16_t gateway_id_size, uint8_t *acked_message_seq_no, uint16_t seq_no_size);
const pb_msgdesc_t *decode_unionmessage_type(pb_istream_t *stream, const pb_msgdesc_t *desc);
bool decode_submessage_contents(pb_istream_t *stream, const pb_msgdesc_t *messagetype, void *dest_struct);

bool encode_submessage(pb_ostream_t *stream, const pb_msgdesc_t *parent_filed, const pb_msgdesc_t *filed, void *message);

void encode_froto_header(pb_ostream_t *stream, const pb_msgdesc_t *parent_filed, const pb_msgdesc_t *filed, void *message);

void decode_one_filed(pb_istream_t *stream, uint8_t target_tag,
                      const pb_msgdesc_t *tar_fields, void *dest_struct);
#endif
