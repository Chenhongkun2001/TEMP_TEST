/*ref@InsightMetro
*/
#include "util_froto_datawrap.h"
#include "Froto.pb.h"
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "pb_common.h"

#include "DeviceAppBulletGateway.pb.h"
#include "SensingDataUpload.pb.h"

#include "util_dbg.h"


DBG_LOCAL_LOG_DEBUG


/*callback to encode uint64
*/
bool protobuf_encode_svarint_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	uint64_t *pt_u64 = NULL;
	if((NULL == arg))
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	pt_u64 = (uint64_t*)*arg;

	DBG_LOG_DEBUG("to encode svarint");

	//if(false == pb_encode_tag_for_field(stream_p, field))
	if(false == pb_encode_tag( stream_p, PB_WT_VARINT, 1))
	{
		DBG_LOG_ERR("fail to encode tag, %s", stream_p->errmsg);
		return false;
	}
	DBG_LOG_DEBUG("the svarint %d", *pt_u64);
	if(false == pb_encode_svarint( stream_p, *pt_u64))
	{
		DBG_LOG_ERR("fail to encode svarint, %s", stream_p->errmsg);
		return false;
	}

	DBG_LOG_INFO("exit encoding svarint");
	
	return true;
	
}



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
bool protobuf_encode_bytes_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
    if (arg != NULL) {
        if (!pb_encode_tag_for_field(stream_p, field)) {
            return false;
        }

        protobuf_encode_general_bytes_t *temp_protobuf_encode_bytes = (protobuf_encode_general_bytes_t *)(*arg);
		DBG_LOG_DEBUG("bytes size %d", temp_protobuf_encode_bytes->size);
		if (!pb_encode_string(stream_p,
                              temp_protobuf_encode_bytes->buffer,
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
static bool protobuf_decode_bytes_callback(pb_istream_t *stream_p, const pb_field_t *field, void **arg)
{
    if (arg != NULL) {
        protobuf_encode_general_bytes_t *temp_protobuf_decode_bytes = (protobuf_encode_general_bytes_t *)(*arg);
        if (stream_p->bytes_left > temp_protobuf_decode_bytes->size)
            return false; /* overflow */
        if (pb_read(stream_p, temp_protobuf_decode_bytes->buffer, stream_p->bytes_left))
            return true;
    }

    return false;
}

bool protobuf_encode_bytes(pb_callback_t *var, protobuf_encode_general_bytes_t *arg)
{
    var->funcs.encode = &protobuf_encode_bytes_callback;
    var->arg          = arg;
}

bool protobuf_decode_bytes(pb_callback_t *var, protobuf_encode_general_bytes_t *arg)
{
    var->funcs.decode = &protobuf_decode_bytes_callback;
    var->arg          = arg;
}
const pb_msgdesc_t *decode_unionmessage_type(pb_istream_t *stream, const pb_msgdesc_t *desc)
{
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
                      const pb_msgdesc_t *tar_fields, void *dest_struct)
{
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
                        if (true == pb_decode(&sub_istream, tar_fields,
                                              dest_struct)) {
                            pb_close_string_substream(stream, &sub_istream);
                            break;
                        }
                        pb_close_string_substream(stream, &sub_istream);
                    }
                }
            }
        }
    } while (true == status);
}

bool decode_submessage_contents(pb_istream_t *stream, const pb_msgdesc_t *messagetype, void *dest_struct)
{
    pb_istream_t substream;
    bool status;
    if (!pb_make_string_substream(stream, &substream))
        return false;
    status = pb_decode(&substream, messagetype, dest_struct);
    pb_close_string_substream(stream, &substream);
    return status;
}

bool encode_submessage(pb_ostream_t *stream, const pb_msgdesc_t *parent_filed, const pb_msgdesc_t *filed, void *message)
{
    pb_field_iter_t iter;

    if (!pb_field_iter_begin(&iter, parent_filed, message))
        return false;

	//DBG_LOG_INFO("to encode the submsg");
	
    do {
		
		//DBG_LOG_INFO( "idx = %d , submsg_desc = 0x%x, field = 0x%x",iter.index, iter.submsg_desc, filed);
        if (iter.submsg_desc == filed) {
            /* This is our field, encode the message using it. */
            if (!pb_encode_tag_for_field(stream, &iter))
                return false;

            return pb_encode_submessage(stream, filed, message);
        }
    } while (pb_field_iter_next(&iter));
	
	DBG_LOG_WARN("fail to find the msg field");
	/* Didn't find the field for messagetype */
    return false;
}



#if(1)
bool encode_submessage_v2(pb_ostream_t *stream, const pb_msgdesc_t *parent_filed, const pb_msgdesc_t *filed, void *message)
{
    pb_field_iter_t iter;

    if (!pb_field_iter_begin(&iter, parent_filed, message))
    {
    	DBG_LOG_ERR("pb_field_iter_begin fail");
        return false;
    }
	//DBG_LOG_INFO("to encode the submsg");
	
    do {
		
		DBG_LOG_DEBUG( "idx = %d , submsg_desc = 0x%x, field = 0x%x",iter.index, iter.submsg_desc, filed);
        if (iter.submsg_desc == filed) {
            /* This is our field, encode the message using it. */
            if (!pb_encode_tag_for_field(stream, &iter))
            {
            	DBG_LOG_ERR("pb_encode_tag_for_field fail");
                return false;
            }
            return pb_encode_submessage(stream, filed, message);
        }
    } while (pb_field_iter_next(&iter));
	
	DBG_LOG_WARN("fail to find the msg field");
	/* Didn't find the field for messagetype */
    return false;
}

#endif


void decode_froto_header(pb_istream_t *stream, SKFChina_Froto_FrotoHeader *header, uint8_t *sensor_id, uint16_t sensor_id_size,
                         uint8_t *gateway_id, uint16_t gateway_id_size, uint8_t *acked_message_seq_no, uint16_t seq_no_size)
{
    // decode the sensor id from header
    protobuf_encode_general_bytes_t id_arg;
    id_arg.buffer = sensor_id;
    id_arg.size   = sensor_id_size;
    protobuf_decode_bytes(&(header->sensor_id), &id_arg);
    // decode the gateway from header
    // protobuf_encode_general_bytes_t peer_addr_arg;
    // peer_addr_arg.buffer = gateway_id;
    // peer_addr_arg.size   = gateway_id_size;
    // protobuf_decode_bytes(&(header->peer_addr.gateway_id), &peer_addr_arg);

    // decode the acked_message_seq_no from header
    // protobuf_encode_general_bytes_t ack_msg_arg;
    // ack_msg_arg.buffer = gateway_id;
    // ack_msg_arg.size   = gateway_id_size;
    // protobuf_decode_bytes(&(header->acked_message_seq_no), &ack_msg_arg);
    const pb_msgdesc_t *type = decode_unionmessage_type(stream, SKFChina_App_AppMessage_fields);
    pb_istream_t substream;
    pb_make_string_substream(stream, &substream);
    type = decode_unionmessage_type(&substream, type);
    if (!decode_submessage_contents(&substream, type, header)) {
        DBG_LOG_ERR("decode header failed\r\n");
    }
    pb_close_string_substream(stream, &substream);
}

void encode_froto_header(pb_ostream_t *stream, const pb_msgdesc_t *parent_filed, const pb_msgdesc_t *filed, void *message)
{
    pb_ostream_t substream;
    if (!pb_make_string_substream(stream, &substream))
        return;
    DBG_LOG_ERR("encode firmware update notification ... \r\n");
    if (false == encode_submessage(&substream, parent_filed, filed, message)) {
        DBG_LOG_WARN("failed\r\n");
        return;
    }
    pb_close_string_substream(stream, &substream);
}



/*
*/
bool protobuf_encode_repeated_varint_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	protobuf_encode_general_bytes_t *pt_buf = *arg;
	if(NULL == pt_buf)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	for(uint32_t i = 0; i < pt_buf->size; i++)
	{
		if(false == pb_encode_tag_for_field( stream_p, field))
		{
			DBG_LOG_ERR("fail to encode tag");
			return false;
		}
		if(false == pb_encode_varint( stream_p, pt_buf->buffer[i]))
		{
			DBG_LOG_ERR("fail to encode varint");
			return false;
		}
	}
	return true;
}

/*
*/

bool protobuf_encode_repeated_mtype_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	uint32_t kp_u32 = 0;
	protobuf_encode_general_bytes_t *pt_buf = *arg;
	if(NULL == pt_buf)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	for(uint32_t i = 0; i < pt_buf->size; i++)
	{
		kp_u32 = pt_buf->buffer[i];
		encode_submessage( stream_p, SKFChina_SensingDataUpload_DataSelectionDisseminate_fields, SKFChina_SensingDataUpload_MeasurementTypeMsg_fields, (void *)&kp_u32);
	}
	return true;
}

#if(0) // the original
bool protobuf_encode_repeated_mtype_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	uint32_t kp_u32 = 0;
	SKFChina_SensingDataUpload_MeasurementTypeMsg tp_mtype = {0};
	protobuf_encode_general_bytes_t *pt_buf = *arg;
	if(NULL == pt_buf)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	for(uint32_t i = 0; i < pt_buf->size; i++)
	{
		kp_u32 = pt_buf->buffer[i];
		//encode_submessage( stream_p, SKFChina_SensingDataUpload_DataSelectionDisseminate_fields, SKFChina_SensingDataUpload_MeasurementTypeMsg_fields, (void *)&kp_u32);	

		memset( &tp_mtype, 0, sizeof(SKFChina_SensingDataUpload_MeasurementTypeMsg));
		if(kp_u32 == SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE)
		{
			tp_mtype.has_dimension = true;
			tp_mtype.dimension = 1;//SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X;
		}
		tp_mtype.measure_type = kp_u32;
		
		encode_submessage( stream_p, SKFChina_SensingDataUpload_DataSelectionDisseminate_fields, SKFChina_SensingDataUpload_MeasurementTypeMsg_fields, (void *)&tp_mtype);
	}
	return true;
}
#else
//SKFChina_SensingDataUpload_MeasurementTypeMsg

bool protobuf_encode_repeated_mtype_by_msg_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	uint32_t kp_u32 = 0;
	SKFChina_SensingDataUpload_MeasurementTypeMsg *pt_mtype_msg = (SKFChina_SensingDataUpload_MeasurementTypeMsg*)*arg;
	//protobuf_encode_general_bytes_t *pt_buf = *arg;
	if(NULL == pt_mtype_msg)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	encode_submessage( stream_p, SKFChina_SensingDataUpload_DataSelectionDisseminate_fields, SKFChina_SensingDataUpload_MeasurementTypeMsg_fields, (void *)pt_mtype_msg);
	return true;
}


#endif


/*
*/
bool protobuf_encode_repeated_sensor_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	uint32_t kp_u32 = 0;
	protobuf_encode_general_bytes_t *pt_buf = *arg;
	if(NULL == pt_buf)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	for(uint32_t i = 0; i < pt_buf->size; i++)
	{
		kp_u32 = pt_buf->buffer[i];
		encode_submessage( stream_p, SKFChina_SensingDataUpload_DataSelectionDisseminate_fields, SKFChina_SensingDataUpload_SensorTypeMsg_fields, (void *)&kp_u32);
	}
	return true;
}



