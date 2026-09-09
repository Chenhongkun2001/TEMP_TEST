#ifndef __APP_GW_SCHEDULER_H
#define __APP_GW_SCHEDULER_H

#include "sys_def.h"
#include "app_general_def.h"
#include "sensorConfig.h"
#include "util_froto.h"

enum ev_cmd_resp{
	EV_CMD_RESP_DEFAULT = 0,
	EV_CMD_RESP_RECEIVED = 1,
	EV_CMD_RESP_TIMEOUT = 2,
	/*...*/
};

/*data structure for response waiting, for each command(ref@struct msg_cmd cmd_info;)  we expect a response(ref@struct msg_feedback feedback_info)
*/
struct resp_waiting{
	pthread_mutex_t mtx;
	pthread_cond_t cond_resp_received;
	//bool is_fdback_received; //set to true 
	enum ev_cmd_resp cmd_resp_event;
	uint32_t msg_seq_no; // the msg_seq_no of the last CMD sent
	size_t tim_val; // for timeout control
	struct msg_feedback feedback_info;
};


struct resp_waiting_cotnroller{
	pthread_mutex_t mtx;
	struct l_queue *pt_resp_waiting_queue; // the item on the queue@struct resp_waiting
};

struct resp_waiting_cotnroller *app_sch_get_resp_waiting_ctr(void);


app_state_t app_sch_set_active_btdevice(uint8_t *pt_btaddr);

void app_sch_init_resp_waiting_obj(struct resp_waiting *pt_resp_waiting_obj, uint32_t kp_seq_no);
void app_sch_deinit_resp_waiting_obj(struct resp_waiting *pt_resp_waiting_obj);
//struct resp_waiting *app_sch_get_resp_waiting_obj(void);

app_state_t app_sch_create_resp_waiting_queue(void);
void app_sch_destroy_resp_waiting_queue(void);

void app_sch_init_resp_waiting_ctr(struct resp_waiting_cotnroller*pt_resp_waiting_ctr);
void app_sch_deinit_resp_waiting_ctr(struct resp_waiting_cotnroller*pt_resp_waiting_ctr);

void app_sch_put_waiting_obj_on_queue( struct resp_waiting*pt_resp_waiting_obj);
app_state_t app_sch_signal_and_pop_resp_waiting_obj(struct resp_waiting_cotnroller*pt_resp_waiting_ctr, struct msg_feedback *pt_fdback_info);
void app_sch_remove_waiting_obj_from_queue(struct resp_waiting*pt_resp_waiting_obj);
app_state_t app_sch_switch_bt_connection(uint8_t *pt_btaddr);






enum sensor_op_code{
	SENSOR_OP_UNDEFINED = 0,
	SENSOR_OP_DTSELECT = 1, // data selection
	SENSOR_OP_CONF_RETRIEVE = 2, // configuration retrieve
	SENSOR_OP_CONF_DISSEM = 3, // configuration dissemination
	SENSOR_OP_FW_VERSION_RETRIEVE = 4, // firmware version retrieve
	SENSOR_OP_IMG_DISSEM = 5, // OTA image dissemination
	/*....*/
};


enum sensor_op_resp_event{
	EV_SENSOR_OP_DEFAULT =0,
	EV_SENSOR_OP_RESP_RECV = 1,
	EV_SENSOR_OP_TIMEOUT = 2,
	/*...*/
};

struct conf_hash_and_last_edit_time{
	uint32_t hash_val;
	uint64_t last_edit_time;
};

#ifndef MAX_GENERAL_CONF_CONTENT
	#define  MAX_GENERAL_CONF_CONTENT (0x10)
#endif
struct sensor_conf_retrieve_resp{
	union {
	SKFChina_Common_SpecificConfigItem specific_item_id; //for SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG = 2,
	SKFChina_Common_fSchedulerConfigItem scheduler_item_id; // for SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG = 3,
	}conf_item_id; 
	union {
		uint8_t general_content[MAX_GENERAL_CONF_CONTENT];
		uint64_t val_u64;
		uint32_t val_u32;
		int32_t val_i32;
		uint16_t val_u16;
		uint8_t  val_u8;
		bool val_bool;
		float 	 val_float;
		struct conf_hash_and_last_edit_time hash_and_last_edit_time;
	}conf_val;
};

struct sensor_conf_retrieve{
	SKFChina_ConfigurationAndCommand_RetrievePayload payload_info;
	struct sensor_conf_retrieve_resp conf_retrieve_resp[MAX_RETR_CONF_ARRAY]; //
};

struct sensor_fw_version_info{
	uint32_t ver;
};

struct sensor_fw_dissem_resp{
	bool is_successful; 
};


union sensor_op_response{
		struct sensor_conf_retrieve sensor_conf_retrieve;// when op_code is  SENSOR_OP_CONF_RETRIEVE
		struct conf_hash_and_last_edit_time conf_hash; // when op_code is SENSOR_OP_CONF_DISSEM
		struct sensor_fw_version_info ver_info; // when op_code is SENSOR_OP_FW_VERSION_RETRIEVE 
		struct sensor_fw_dissem_resp fw_dissem_resp;// when op_code is SENSOR_OP_IMG_DISSEM
		/*...*/
	};


/*the operation info
*/
struct sensor_op_waiting{
	pthread_mutex_t mtx;
	pthread_cond_t cond_resp_received;
	//bool is_resp_received;
	enum sensor_op_resp_event resp_event;//
	bool is_obj_active; // set to true when the object is active
	uint32_t msg_seq_no; //
	time_t timval; // the time when the obejct is created

	
	enum sensor_op_code op_code; //to check the opeartion-code to known what response below is expected
	union sensor_op_response sensor_op_resp;
};


/*data struct definition for sensor operation(like OTA, configuration synchronization, data selection....) control
NTOE!!!
1. for each operation, we expect a response
*/
struct sensor_op_controller{
	pthread_mutex_t mtx;
	struct l_queue *pt_sensor_op_waiting_queue;	
};

struct sensor_op_controller *app_sch_get_sensor_op_controller(void);

app_state_t app_sch_read_conf_hash_and_last_edit_time(struct sensor_op_controller*pt_sensor_op_ctr, uint32_t *pt_hashvalue, uint32_t *pt_edit_time);
app_state_t app_sch_init_sensor_op_controller(struct sensor_op_controller*pt_sensor_op_ctr);
app_state_t app_sch_deinit_sensor_op_controller(struct sensor_op_controller*pt_sensor_op_ctr);
app_state_t app_sch_init_sensor_op_waiting_obj(struct sensor_op_waiting*pt_sensor_op_waiting_obj, uint32_t kp_msg_seq_no, enum sensor_op_code kp_op_code);
void app_sch_deinit_sensor_op_waiting_obj(struct sensor_op_waiting * pt_sensor_op_waiting_obj);

app_state_t app_sch_put_sensor_op_waiting_obj_on_queue( struct sensor_op_controller *pt_sensor_op_ctr, struct sensor_op_waiting*pt_sensor_op_waiting_obj);
void app_sch_remove_sensor_op_waiting_obj_from_queue(struct sensor_op_controller*pt_sensor_op_ctr, struct sensor_op_waiting *pt_sensor_op_waiting_obj);
app_state_t app_sch_signal_and_pop_sensor_op_waiting_obj( struct sensor_op_controller *pt_sensor_op_ctr, struct msg_tree_node*pt_msg_root, struct node_path kp_ndpath);

void app_sch_set_timval_to_sensor_op_waiting_obj(struct sensor_op_waiting *pt_sensor_op_waiting_obj, time_t kp_timval);
app_state_t app_sch_set_bt_white_list(struct bt_addr *pt_btaddr_array, uint8_t kp_arr_sz);

app_state_t app_sch_read_sensor_conf(struct sensor_op_controller*pt_sensor_op_ctr, sensorConfig_t*pt_sensor_conf);
app_state_t app_sch_read_conf_hash_and_last_edit_time(struct sensor_op_controller*pt_sensor_op_ctr, uint32_t *pt_hashvalue, uint32_t *pt_edit_time);
app_state_t app_sch_read_sensor_conf_v2(struct sensor_op_controller*pt_sensor_op_ctr, sensorConfig_t*pt_sensor_conf);



app_state_t app_sch_synchronize_time( const uint8_t*pt_gwid_str, const uint8_t*pt_sensor_id);



app_state_t app_sch_set_sensor_conf(struct sensor_op_controller*pt_sensor_op_ctr, const sensorConfig_t*pt_sensor_conf);

bool queue_match_for_sensor_waiting(const void *data, const void *user_data);

void app_sch_init_sensor_config_for_test(sensorConfig_t *pt_sensor_conf);



#endif

