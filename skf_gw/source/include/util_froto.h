#ifndef __UTIL_FROTO_H
#define __UTIL_FROTO_H

#include "sys_def.h"

#include <stdint.h>


#include "pb.h"
#include "Common.pb.h"
#include "ConfigurationAndCommand.pb.h"
#include "SensingDataUpload.pb.h"

#include "app_general_def.h"
#include "sensorConfig.h"

#define MAX_ID_STR_LENGTH	(0x10)



#if(0) // the original
struct msg_tree_node{
	struct msg_tree_node* next;
	uint32_t tag;
	enum node_role role;
	// the following is  valid only when the role is I_NODE
	uint32_t size ;
	pb_wire_type_t type;
	void *buf; // the data-info
};
#else
/*define a node to keep the Froto message info
Note!! we use a tree to keep the info from a Froto message
*/

enum node_role{
	ND_INFO = 0, // the info node
	ND_ROOT = 1, // the root node
};


struct data_info{
	uint32_t size; // the size of data buf in bytes
	pb_wire_type_t type; // the wire type
	void*dtbuf; // the data buffer
};

//definition for node payload
union node_payload{
	struct data_info dtinfo; // for ND_INFO@enum node_role
	struct msg_tree_node *branch;
};

struct msg_tree_node{
	struct msg_tree_node* next;
	uint32_t tag;
	enum node_role role;
	// check the info@role before you read nd_data
	union node_payload nd_data;
};

#ifndef MAX_TREE_NODE_DEPTH
#define MAX_TREE_NODE_DEPTH	0x10 // the max node depth
#endif
/*the data-structure to keep the path to any message-node on the tree
*/
struct node_path{
	uint32_t kpdepth; // the number of valid node-path info in the path[] below 
	uint32_t path[MAX_TREE_NODE_DEPTH]; // root->node1->node2->nod3 // start from node1
};


#endif




/*data structure for encoded Froto message
*/
struct encoded_froto_msg_pkt{
	uint32_t len; // the encodded-msg length
	bool is_valid; // set to true
	uint8_t *ptbuf; // the buffer where the encoded message is
};


#if(1)

#ifndef MAX_RETR_CONF_ARRAY
#define MAX_RETR_CONF_ARRAY	(5U)
#endif
struct config_to_retrieve{
	SKFChina_ConfigurationAndCommand_RetrievePayload payload_info;
	uint32_t msg_seq_no;
	uint32_t unit_num; // the effective unit in the array below
	union{
		SKFChina_Common_SpecificConfigItem specific_conf_array[MAX_RETR_CONF_ARRAY];//for SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG
		SKFChina_Common_fSchedulerConfigItem fsched_conf_array[MAX_RETR_CONF_ARRAY]; // for SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG
	}conf;
};
#endif


app_state_t util_froto_test(void);
app_state_t util_froto_fill_msg_config_dissem(void);

int32_t util_froto_pick_appmsg_tag( const pb_byte_t*pt_msgbuf, size_t kp_len);
app_state_t util_froto_decoder_for_msg_config_hash_upload(const pb_byte_t*pt_msgbuf, size_t kp_len);

struct encoded_froto_msg_pkt util_froto_fill_msg_config_dissem_set_time( const uint8_t*pt_gwid_str, const uint8_t*pt_sensor_id, uint32_t kp_seq_no);
struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection_retrieve_battery(void);

app_state_t util_froto_fill_msg_data_selection_retrieve_data(void);
app_state_t util_froto_fill_msg_command_dissem(void);

struct encoded_froto_msg_pkt util_froto_fill_msg_version_retrieve(const uint8_t *pt_gwid_str, const uint32_t kp_seq_no);
struct encoded_froto_msg_pkt util_froto_fill_msg_fuota_notify_dissem(const uint8_t*pt_gwid_str, const uint32_t kp_seq_no, struct longdata_xfer_ctr*pt_img_dissem_ctr);
//struct encoded_froto_msg_pkt util_froto_fill_msg_img_block_dissem(struct longdata_xfer_ctr*pt_img_dissem_ctr, const uint8_t*pt_gwid_str, uint32_t kp_seq_no);
struct encoded_froto_msg_pkt util_froto_fill_msg_img_block_dissem(struct longdata_xfer_ctr*pt_img_dissem_ctr, const uint8_t*pt_gwid_str, uint32_t kp_seq_no);

//struct encoded_froto_msg_pkt util_froto_fill_msg_config_retrieve(void);
struct encoded_froto_msg_pkt util_froto_fill_msg_config_retrieve(const struct config_to_retrieve *pt_conf, uint8_t*pt_sensorid, uint8_t*pt_gwid);


struct encoded_froto_msg_pkt util_froto_fill_msg_up_data_upload(void);
struct encoded_froto_msg_pkt  util_froto_fill_msg_up_image_block_request(const uint32_t kp_tskid, const uint32_t kp_msg_seq_no);
struct encoded_froto_msg_pkt util_froto_fill_msg_up_file_notify_upload(const uint32_t kp_tskid, const uint32_t kp_total_blk);


struct encoded_froto_msg_pkt util_froto_fill_msg_up_current_version_upload(void);

struct encoded_froto_msg_pkt util_froto_fill_msg_up_image_block_upload(const uint32_t kp_tskid, const uint32_t kp_total_blk, const uint32_t kp_cur_blk);


struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection(SKFChina_Common_MeasurementType kp_mtype,const uint8_t*pt_gwid_str, uint32_t kp_seq_no);



void util_froto_release_encoded_msg(pb_byte_t*pt_buf);
void util_froto_release_encoded_msg_v2(struct encoded_froto_msg_pkt *pt_encoded_pkt);


bool util_froto_release_msg_tree(struct msg_tree_node*pt_root);


void util_froto_msg_decoding_test(const pb_byte_t*pt_dtbuf, const uint32_t kplen, const pb_msgdesc_t *hld_msgdesc);
struct msg_tree_node *util_froto_locate_msg_node(const struct msg_tree_node*hld_root, const struct node_path kp_nd_path);
bool util_froto_locate_msg_node_v2(const struct msg_tree_node*hld_root, const struct node_path kp_nd_path, struct l_queue *pt_node_queue);


struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection_v2(SKFChina_Common_MeasurementType kp_mtype,const uint8_t*pt_gwid_str, uint32_t kp_seq_no);
struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection_v3(SKFChina_Common_MeasurementType *pt_mtype_array, const uint32_t kp_arr_len,const uint8_t*pt_gwid_str, uint32_t kp_seq_no);

uint8_t idstr_to_bytes_array( uint8_t *pt_dtbuf, uint8_t kp_bufsz, uint8_t *pt_str);

uint32_t ipstr_to_bytes_array( uint8_t *pt_dtbuf, uint8_t kp_bufsz, uint8_t *pt_str);


struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection_with_mtype_msg(SKFChina_SensingDataUpload_MeasurementTypeMsg kp_mtype_msg,const uint8_t*pt_gwid_str, uint32_t kp_seq_no);




#if(1)
// for conf-dissemination

// to create the froto msg to disseminate sensor configuration
#ifndef MAX_CONF_ITEM_DISSEM
	#define MAX_CONF_ITEM_DISSEM  (5) // the max number of conf-item we can disseminate by each Froto-message
#endif
struct conf_dissem_info{
	union{
		SKFChina_Common_SpecificConfigItem specific[MAX_CONF_ITEM_DISSEM];
		//SKFChina_Common_fSchedulerConfigItem  fscheduler[MAX_CONF_ITEM_DISSEM]; // not available for Bullet
	}conf_item_array;
	uint32_t item_num; // the number of valid item in the array above, max@MAX_CONF_ITEM_DISSEM
	uint8_t gw_macstr[MAX_ID_STR_LENGTH]; // the gateway mac-string
	uint8_t sensor_macstr[MAX_ID_STR_LENGTH]; // the sensor mac-string
	uint32_t seq_no; // the sequence-number of the Froto-msg
	sensorConfig_t *pt_sensor_conf; // the configuration to be disseminated to sensor
};


struct encoded_froto_msg_pkt  util_froto_fill_msg_specific_config_dissem(const struct conf_dissem_info*pt_conf_dissem);


uint32_t util_froto_bullet_crc32(
  const uint8_t *buf,
  uint32_t size);
uint32_t util_froto_bullet_elfhash(
  const uint8_t *inputData,
  uint32_t length);
uint32_t util_froto_cal_config_hash_value(
  sensorConfig_t *pt_sensor_conf,
  char *clientID);
bool util_froto_format_msg_info(
  struct msg_tree_node *pt_head,
  pb_istream_t *pt_istream,
  const pb_msgdesc_t *hld_msgdesc);


#endif




#endif

