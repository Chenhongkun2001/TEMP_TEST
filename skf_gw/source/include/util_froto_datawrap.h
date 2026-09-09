/*ref@InsightMetro
*/
#ifndef __UTIL_FROTO_DATAWRAP_H
#define __UTIL_FROTO_DATAWRAP_H

#include "stdint.h"
#include "pb.h"
#include "Froto.pb.h"


typedef struct {
    uint32_t size;
    uint8_t *buffer;
} protobuf_encode_general_bytes_t;


bool protobuf_encode_bytes(pb_callback_t *var, protobuf_encode_general_bytes_t *arg);
bool protobuf_decode_bytes(pb_callback_t *var, protobuf_encode_general_bytes_t *arg);
void decode_froto_header(pb_istream_t *stream, SKFChina_Froto_FrotoHeader *header, uint8_t *sensor_id, uint16_t sensor_id_size,
                         uint8_t *gateway_id, uint16_t gateway_id_size, uint8_t *acked_message_seq_no, uint16_t seq_no_size);
const pb_msgdesc_t *decode_unionmessage_type(pb_istream_t *stream, const pb_msgdesc_t *desc);
bool decode_submessage_contents(pb_istream_t *stream, const pb_msgdesc_t *messagetype, void *dest_struct);

bool encode_submessage(pb_ostream_t *stream, const pb_msgdesc_t *parent_filed, const pb_msgdesc_t *filed, void *message);
bool encode_submessage_v2(pb_ostream_t *stream, const pb_msgdesc_t *parent_filed, const pb_msgdesc_t *filed, void *message);

void encode_froto_header(pb_ostream_t *stream, const pb_msgdesc_t *parent_filed, const pb_msgdesc_t *filed, void *message);

void decode_one_filed(pb_istream_t *stream, uint8_t target_tag,
                      const pb_msgdesc_t *tar_fields, void *dest_struct);



bool protobuf_encode_bytes_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg);
bool protobuf_encode_svarint_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg);

bool protobuf_encode_repeated_varint_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg);
bool protobuf_encode_repeated_mtype_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg);
bool protobuf_encode_repeated_sensor_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg);
bool protobuf_encode_repeated_mtype_by_msg_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg);

#endif



