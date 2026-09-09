#ifndef __APP_DB_H__
#define __APP_DB_H__

#include "pthread.h"
#include "sys_def.h"
#include "ell/ell.h"
#include "ell/queue.h"
#include "global.h"
#include "ppGW.h"
#include "gwConfig.h"
#include "sensorConfig.h"

#include "util_froto.h"

#include "app_hci_evt_mgmt.h"

/*data structure definition for database operation
*/
struct db_op_msg{
	gw_pro_command_t cmd;
	uint8_t table_idx; // the table name , going to operate ref@#GW_PRO_TABLE_IDX_xxx @ppGW.h
	union{
		// for CMD gw_pro_command_sqlite_update_item
		struct data_item *pt_dtitem; //for table sensor-data
		 gwConfig_gwConfig_t *pt_gwconf; //for table GW configuration
		 gwConfig_gwConfig_children_t *pt_sensorinfo; // for table DEV
		  sensorConfig_t *pt_sensor_conf;// for table Sensor_Conf
		
		 //for CMD gw_pro_command_sqlite_query_item
		gw_pro_command_sqlite_query_item_param_t query_item; // 
	}cmd_para;
};


/*data structure definition for database operation control
*/
struct database_operation_controller{
	int msgid_to_db;
	int msgid_from_db;

	pthread_mutex_t mtx;
	struct l_queue *pt_dtitem_queue; //ref@struct db_op_msg  we put all data to be inserted into database on the queue before it is inserted into database
	pthread_cond_t cond_dt_available;

	// for each msg sent to process Database ,we expect a response
	#if(1)// abandoned @ 20240526
	// pthread_mutex_t mtx_for_waiting;
	bool is_resp_received;
	struct msgbuf resp_msg; //
	// pthread_cond_t cond_for_waiting;	
	#endif
};


void*thandler_db_data_update(void*pt_para);



#if(1)
#ifndef MAX_MSG_DTUNIT
	#define MAX_MSG_DTUNIT	GW_PRO_MAX_DATA_FIELD_PER_OPERATION
#endif
struct msg_format_controller{
	gw_pro_data_t dtunit[MAX_MSG_DTUNIT];
	gw_pro_msgbuf_t msginfo;
	struct msgbuf ipcmsg;
};
#endif


enum db_op_resp_event
{
  EV_DB_OP_DEFAULT = 0,
  EV_DB_OP_RESP_RECV = 1,
  EV_DB_OP_TIMEOUT = 2,
};

// The object of @p pt_dbop_resp_waiting_queue
struct dbop_resp_waiting
{
  pthread_mutex_t mtx;
  // To signal the thread waiting on it
  pthread_cond_t cond_resp_received;
  // To indicate whether a response received or a timeout happens
  enum db_op_resp_event db_resp_event;
  // The command ID sent to the process sql_op
  uint32_t command_id;
  // The command
  gw_pro_command_t command;
  // In case of timeout, just put current time here and the thread @p
  // thandler_tim_service would check the time and signal timeout
  uint32_t tim_val;
  // The response message
  gw_pro_message_response_t resp_info;
};


// Number from 0 to 10 is reserved for operation that response can be 
// ignored
#ifndef CMD_ID_CNT_INITIALIZER
#define CMD_ID_CNT_INITIALIZER	(10U)
#endif
#ifndef CMD_ID_CNT_MAX
#define CMD_ID_CNT_MAX	(0xFFFFFFFF)
#endif
#ifndef CMD_ID_FOR_INSIGHT_T
#define CMD_ID_FOR_INSIGHT_T	(0U)
#endif


// For communication between the process (itself) and sql_op
struct dbop_controller
{
  pthread_mutex_t mtx;
  // A queue where an object should be put onto the queue once a
  // message sent to the process sql_op and a response is expected
  struct l_queue *pt_dbop_resp_waiting_queue;
  uint32_t command_id_cnt;  // counter for SeqNo of IPC
};

/**
 * @brief The Device information
 */
struct device_info
{
  // Same as @p nameId in @p gwConfig_gwConfig_children_t
  uint32_t nameid;
  // Same as @p name in @p gwConfig_gwConfig_children_t
  char name[50];
  // Same as @p bleName in @p gwConfig_gwConfig_children_t
  char sensor_name[50];
  // Max length of MAC address seperated by dash (like "C4-BD-6A-10-00-00")
  uint8_t mac_addr[19];
  // Same as @p manufacturer in @p gwConfig_gwConfig_children_t
  char manufacturer[10];
  // Same as @p type in @p gwConfig_gwConfig_children_t
  char sensor_type[50];
#if (ENABLE_MODBUS_FEATURE == 1)
  // The description field of device in "CH00-AD0000-L00-PRS" format
  char description[GW_PRO_MAX_STRING_LEN_BYTE];  
#endif
};

// To handle report once a modification report received from the process
// sql_op
struct db_report_handler
{
  pthread_mutex_t mtx;
  // A queue where an object should be put onto the queue once a
  // modification report received from the process sql_op
  struct l_queue *pt_report_queue;
  // To signal the thread waiting on it
  pthread_cond_t cond_dt_available;
};

enum db_report_type{
	DB_REPORT_UNDEFINED = 0,
  // The database has been modified by process mqtt_op
	DB_REPORT_MODIFICATION = 1,
	// The following event is generated when a configuration 
	DB_TBL_GATEWAY_UPDATED = 10,
	DB_TBL_DEV_UPDATED = 11,
};

struct db_event_table_updated{
	uint32_t table; // GW_PRO_TABLE_IDX_xxx
	uint64_t ts;
};

union db_report_info{
  // use it when report_type = DB_REPORT_MODIFICATION
	gw_pro_command_report_new_modification_param_t db_new_modification;
  // use it when report_type = DB_TBL_xxxx_UPDATED
	struct db_event_table_updated db_event_tbl_update; 
};

struct db_report_node{	
	enum db_report_type report_type;
	union db_report_info report_info;
};

#if (SKF_GW_NEW == 1)
bool is_dtitem_in_db(
  const struct data_item *pt_dtitem,
  const uint8_t *pt_macstr);
#endif
struct dbop_controller *app_db_get_dbop_controller(void);
uint32_t app_db_allocate_ipc_seqno(struct dbop_controller *pt_dbopctr);
app_state_t app_db_init_dbop_controller(
  struct dbop_controller *pt_dbop_ctr);
void app_db_deinit_dpop_controller(struct dbop_controller *pt_dbop_ctr);
app_state_t app_db_init_dbop_resp_waiting_obj(
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj,
  const gw_pro_command_t kp_cmd,
  const uint32_t kp_cmd_id);
app_state_t app_db_deinit_dbop_resp_waiting_obj(
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj);
app_state_t app_db_put_dbop_waiting_obj_on_queue(
  struct dbop_controller *pt_dbop_ctr,
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj);
app_state_t app_db_remove_dbop_waiting_obj_from_queue(
  struct dbop_controller *pt_dbop_ctr,
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj);
app_state_t app_db_signal_and_pop_dbop_waiting_obj(
  struct dbop_controller *pt_dbop_ctr,
  gw_pro_message_header_t *pt_msg_from_db);
app_state_t app_db_handle_command_from_db(
  gw_pro_message_header_t *pt_msg_from_db);
struct db_report_handler *app_db_get_report_handler(void);
app_state_t app_db_init_report_handler(
  struct db_report_handler *pt_db_report_handler);
void app_db_deinit_report_handler(
  struct db_report_handler *pt_db_report_handler);
app_state_t app_db_put_db_report_on_queue(
  struct db_report_handler *pt_db_report_handler,
  gw_pro_command_report_new_modification_param_t *pt_mod_report);
app_state_t app_db_put_db_table_update_event_on_queue(
  struct db_report_handler *pt_db_report_handler,
  struct db_event_table_updated *pt_event_tbl_update,
  enum db_report_type kp_report_type);
app_state_t app_db_init_database_op_ctr(
  struct database_operation_controller *pt_db_op_ctr);
void app_db_deinit_database_op_ctr(
  struct database_operation_controller *pt_db_op_ctr);
struct database_operation_controller *app_db_get_database_op_ctr(void);
int32_t app_db_fetch_filtered_index(
  struct dbop_controller *pt_dbop_ctr,
  const gw_pro_command_sqlite_filter_param_t *pt_filter,
  uint32_t bufsz,
  uint32_t *pt_idbuf);
app_state_t app_db_load_sensor_data(sensorConfig_t *sensorConfig);
app_state_t app_db_read_sensor_conf_from_db(
  struct dbop_controller *pt_dbop_ctr,
  struct msg_format_controller *pt_msg_format_ctr,
  sensorConfig_t *pt_sensor_conf,
  struct fouta_fw_info *fw_info,
  uint32_t itemidx);
app_state_t app_db_fetch_sensor_conf_from_db(
  struct dbop_controller *pt_dbop_ctr,
  sensorConfig_t *pt_sensor_conf,
  struct fouta_fw_info *fw_info,
  uint32_t itemidx);
app_state_t app_db_read_gateway_conf_from_db(
  struct dbop_controller *pt_dbop_ctr,
  struct msg_format_controller *pt_msg_format_ctr,
  gwConfig_t *pt_gwconf,
  uint32_t itemidx);
app_state_t app_db_read_dev_info_from_db(
  struct dbop_controller *pt_dbop_ctr,
  struct msg_format_controller *pt_msg_format_ctr,
  struct device_info *pt_dev_info,
  uint32_t itemidx);
#if 0 // Deprecated
int32_t app_db_fetch_children_mac_array_from_db(
  struct dbop_controller *pt_dbop_ctr,
  struct bt_addr *pt_resbuf,
  const uint32_t kp_bufsz);
#endif
int32_t app_db_fetch_children_mac_array_from_db_v2(
  struct dbop_controller *pt_dbop_ctr,
  struct bt_addr *pt_resbuf_p,
  struct bt_addr *pt_resbuf_t,
  uint32_t * pt_name_id_p,
  uint32_t * pt_name_id_t,
  const uint32_t kp_bufsz);
app_state_t app_db_fetch_gwconf_and_dev_from_db(
  struct dbop_controller *pt_dbop_ctr,
  gwconf_and_dev_t *pt_gwconf_and_dev);
app_state_t app_db_fetch_gwconf_without_dev_from_db(
  struct dbop_controller *pt_dbop_ctr,
  gwconf_and_dev_t *pt_gwconf_and_dev);
app_state_t app_db_fetch_gwconf_and_dev_from_db_to_file(
  struct dbop_controller *pt_dbop_ctr,
  const uint8_t *pt_fpath);
app_state_t app_db_fetch_a_device_info_from_db_based_on_macaddr(
  struct dbop_controller *pt_dbop_ctr,
  struct device_info *pt_dev_info,
  const uint8_t *pt_macstr);
app_state_t app_db_deliver_sensor_conf_to_db(
  struct dbop_controller *pt_dbop_ctr,
  const sensorConfig_t *pt_sensor_conf,
  struct fouta_fw_info *fw_info);
app_state_t app_db_deliver_dev_to_db(
  struct dbop_controller *pt_dbop_ctr,
  const struct device_info *pt_dev_info);
app_state_t app_db_deliver_gateway_conf_to_db(
  struct dbop_controller *pt_dbop_ctr,
  const gwConfig_t *pt_gwconf);
app_state_t app_db_deliver_gwconf_and_dev_to_db_from_json(
  struct dbop_controller *pt_dbop_ctr,
  const uint8_t *pt_fpath);
app_state_t app_db_deliver_sensordata_sit_to_db_noblock(
  struct dbop_controller *pt_dbop_ctr,
  const struct sit_dtpacket *pt_sit_dtpkt,
  enum sit_data_type dt_type);
app_state_t app_db_delete_item_in_db(
  struct dbop_controller *pt_dbop_ctr,
  gw_pro_command_sqlite_delete_item_param_t *pt_del_param);
app_state_t app_db_clear_table(
  struct dbop_controller *pt_dbop_ctr,
  const uint32_t kp_tbidx);
app_state_t app_db_gabage_cleanup(
  struct dbop_controller *pt_dbop_ctr,
  time_t absTimeBefore_s,
  time_t timeBefore_s);
void *thandler_db_modification_report(void *pt_para);
void *thandler_db_modification_readout(void *pt_para);
void app_db_set_db_modification_readout_enable_fg(void);
void app_db_clear_db_modification_readout_enable_fg(void);
bool app_db_get_db_modification_readout_enable_fg(void);
app_state_t app_db_handler_for_msg_from_database_v2(
  struct dbop_controller *pt_dbop_ctr,
  const struct msgbuf *pt_msgbuf);

uint32_t app_db_fetch_lastRegularSensingMeasID(void);

int32_t app_db_fetch_devlist(
  struct dbop_controller *pt_dbop_ctr,
  struct devinfo_node *pt_devarray,
  const uint32_t kp_bufsz);


// Deprecated
#if 0
app_state_t app_db_handler_for_msg_from_database(
  struct database_operation_controller *pt_db_op_ctr,
  const struct msgbuf *pt_msgbuf);
app_state_t app_db_write_gw_conf_to_file(
  struct dbop_controller *pt_dbop_ctr,
  const uint8_t *pt_fpath);
app_state_t app_db_handler_for_msg_from_database(
  struct database_operation_controller *pt_db_op_ctr,
  const struct msgbuf *pt_msgbuf);
app_state_t app_db_operation_test(const gw_pro_command_t kp_cmd);
app_state_t app_db_load_all_sensor_data(void);
app_state_t app_db_write_sit_data_to_db(
  struct dbop_controller *pt_dbop_ctr,
  struct msg_format_controller *pt_msg_format_ctr,
  const struct sit_dtpacket *pt_sit_dtpkt);
#endif

#endif /* __APP_DB_H__ */


