/*for database operation
*/
#include "app_db.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_froto.h"

#include "sys_def.h"
#include "util_dbg.h"

#include "ppGW.h"
#include "global.h"
#include "auxiliar.h"
#include "list.h"
#include "gwConfig.h"
#include "app_hci_evt_mgmt.h"
#include "app_pro_general.h"
#include "app_pro_general_new.h"
#include "app_cjson.h"
#include "app_common.h"
#include "app_gw_scheduler.h"

DBG_LOCAL_LOG_DEBUG


// Maximal item in one data selection request (only valid for short data)
// For example, if @p SZ_DTSELECTION_GROUP is 3, gateway would request at
// most three types of measurement in one request.
#define SZ_DTSELECTION_GROUP 3
// Whether collect the ACC waveform (0 indicates no need to collect)
#ifndef SENSOR_DATA_COL_RULE_REQUIRING_ACC_WAVE
#define SENSOR_DATA_COL_RULE_REQUIRING_ACC_WAVE (1)
#endif
// Whether collect the ENV3 waveform (0 indicates no need to collect)
#ifndef SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE
#define SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE (1)
#endif
// Whether collect the VEL waveform (0 indicates no need to collect)
#ifndef SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE
#define SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE (1)
#endif
// Whether collect waveforms based on the configuration (0 indicates force
// to upload all the waveforms if corresponding above MACROs are enabled)
#define SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF (1)

static struct database_operation_controller g_database_op_ctr = { 0 };
static uint64_t lastRegularSensingTs = 0;
static uint32_t lastRegularSensingMeasID = 0;
static bool dtitem_exist_fg = false;
static bool dtitem_acc_wave_exist_fgx = false;
static bool dtitem_acc_wave_exist_fgy = false;
static bool dtitem_acc_wave_exist_fgz = false;
#if (SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE == 1)
static bool dtitem_env_wave_exist_fg = false;
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE == 1 */
#if (SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE == 1)
static bool dtitem_vel_wave_exist_fg = false;
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE == 1 */
static uint32_t db_table_mod_report_cnt[4];  // To store the database table
                                             // modification report counter
                                             // (clear when handle these
                                             // modifications)
#if (ENABLE_MODBUS_FEATURE == 1)
// counterparts of those for mqtt, perfixed with 'ble' to distinguish them
static bool ble_dev_fetch_flag = false;
static uint8_t ble_dev_cnt_former = 0;
static uint8_t ble_dev_cnt_current = 0;
static uint32_t ble_dev_nameid_list_former[GW_MAX_CHILDREN_NUM] = {0};
static uint32_t ble_dev_nameid_list_current[GW_MAX_CHILDREN_NUM] = {0};
#endif
/*
 * a workaround for 'system hung' issue in 'msg-queue broken' corner case
 * note: 1). count 'EAGAIN'(No such file or directory) error only;
 *       2). skip the non-block send case as we only want to avoid hunging system here
 */
#define MSGSEND_ERR_CNT_MAX (2000)
#define MSGSEND_ERR_CMD "killall skf_gw_1"
uint8_t g_msgsnd_err_cnt = 0;
// A timer used to reduce the frequency of database operation report (i.e.,
// to avoid too frequent reaction to the database report)
static void db_mod_report_timer_handler(
  int sig,
  siginfo_t *pt_sig,
  void *uc);
static timer_t db_mod_report_timer_id = 0;
static bool db_modification_readout_enable_fg = true;  // False to disable
                                                       // configuration
                                                       // read-out from the
                                                       // database
static bool db_modification_timer_fg = true;  // True to indicate there is
                                              // a database operation
                                              // report need to react

static bool queue_match_command_id(
  const void *data,
  const void *user_data);
static void app_db_init_db_mod_report_timer_ctr(void);
static app_state_t to_handle_db_modification_report(
  gw_pro_command_report_new_modification_param_t *pt_modification_param);
static app_state_t to_handle_db_table_gateway_update_event(
  struct db_event_table_updated *pt_db_tbl_update_event);
static app_state_t to_handle_db_table_dev_update_event(
  struct db_event_table_updated *pt_db_tbl_update_event);
static app_state_t to_fill_sensor_conf_by_data_from_db(
  const gw_pro_command_sqlite_query_item_response_t *pt_query_resp,
  sensorConfig_t *pt_sensor_conf,
  struct fouta_fw_info *fw_info);
static app_state_t to_fill_gw_conf_by_data_from_db(
  const gw_pro_command_sqlite_query_item_response_t *pt_query_resp,
  gwConfig_t *pt_gwconf);
static app_state_t to_fill_dev_info_by_data_from_db(
  const gw_pro_command_sqlite_query_item_response_t *pt_query_resp,
  struct device_info *pt_dev_info);
static app_state_t to_fill_dtunit_array_for_sensor_conf_writing(
  struct msg_format_controller *pt_msg_format_ctr,
  const sensorConfig_t *pt_sensor_conf,
  struct fouta_fw_info *fw_info);
static app_state_t to_fill_dtunit_array_for_gateway_conf_writing(
  struct msg_format_controller *pt_msg_format_ctr,
  const gwConfig_t *pt_gwconf);
static app_state_t to_fill_dtunit_array_for_dev_writing(
  struct msg_format_controller *pt_msg_format_ctr,
  const struct device_info *pt_dev_info);
static app_state_t to_fill_dtunit_array_for_sensordata_writing_sit(
  struct msg_format_controller *pt_msg_format_ctr,
  const struct sit_dtpacket *pt_sit_dtpkt,
  enum sit_data_type dt_type);
static app_state_t app_db_deliver_partial_sensor_conf_to_db(
  struct dbop_controller *pt_dbop_ctr,
  const sensorConfig_t *pt_sensor_conf);
static app_state_t app_db_deliver_one_field_to_db(
  struct dbop_controller *pt_dbop_ctr,
  uint32_t table_idx,
  uint32_t item_idx,
  gw_pro_data_t kp_dtitem);
static app_state_t app_db_deliver_update_partial_gateway_conf_to_db(
  struct dbop_controller *pt_dbop_ctr,
  uint64_t kp_editime);
static app_state_t app_db_deliver_sensordata_to_db(
  struct dbop_controller *pt_dbop_ctr,
  struct data_item *pt_dtitem);
#if (SKF_GW_NEW != 1)
static bool is_dtitem_in_db(
    const struct data_item *pt_dtitem,
    const uint8_t *pt_macstr);
#endif
static void to_dump_gw_pro_msg_header(
  gw_pro_message_header_t *pt_gw_pro_msg_header);
static void to_cleanup_extra_files(void);
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
#if (ENABLE_MODBUS_FEATURE == 1)
static gwConfig_gwConfig_children_t *makeChildrenForGw(
  const char *type,
  const char *manufacturer,
  const char *name,
  const uint32_t nameId,
  const char *bleName,
  macAddr_t *macAddr,
  const char *description);
#else
static gwConfig_gwConfig_children_t *makeChildrenForGw(
  const char *type,
  const char *manufacturer,
  const char *name,
  const uint32_t nameId,
  const char *bleName,
  macAddr_t *macAddr);
#endif /* ENABLE_MODBUS_FEATURE == 1 */
#endif /* GWCONFIG_CHILDREN_USING_LINKED_LIST == 1 */

// The array of all the supported measurement type
static const SKFChina_Common_MeasurementType g_mtype_array[] = {
  SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE,
  SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE,
  SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE,
  SKFChina_Common_MeasurementType_REMAINING_VOLUME,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_RUN_CODE,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_CONDITON_CODE,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_MAG_PRE,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_VIB_PRE,
  SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT,
  SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MAX,
  SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_START,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_END,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR,
  SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT,
};

struct dbop_controller g_dbop_ctr = { 0 };
struct db_report_handler g_db_report_handler = { 0 };

/**
 * @brief Get the database operation handler
 */
struct dbop_controller *
app_db_get_dbop_controller(void)
{
  return &g_dbop_ctr;
}
/**
 * @brief To get a increment seqNo for IPC
 */
uint32_t
app_db_allocate_ipc_seqno(struct dbop_controller *pt_dbopctr)
{
  uint32_t tpret = 0;

  pthread_mutex_lock(&pt_dbopctr->mtx);
  if((pt_dbopctr->command_id_cnt < CMD_ID_CNT_INITIALIZER)
     || (pt_dbopctr->command_id_cnt >= CMD_ID_CNT_MAX))
  {
    pt_dbopctr->command_id_cnt = CMD_ID_CNT_INITIALIZER;
  }
  tpret = pt_dbopctr->command_id_cnt;
  pt_dbopctr->command_id_cnt = pt_dbopctr->command_id_cnt + 1;
  pthread_mutex_unlock(&pt_dbopctr->mtx);

  return tpret;
}
/**
 * @brief To initialize the mutex, the increment seqNo for IPC and the
 *        queue for waiting IPC response
 */
app_state_t
app_db_init_dbop_controller(struct dbop_controller *pt_dbop_ctr)
{
  app_state_t tpret = ST_OK;

  pthread_mutex_init(&pt_dbop_ctr->mtx, NULL);
  pt_dbop_ctr->pt_dbop_resp_waiting_queue = l_queue_new();
  pt_dbop_ctr->command_id_cnt = CMD_ID_CNT_INITIALIZER;

  return tpret;
}
/**
 * @brief To free the queue for waiting IPC response and the mutex
 */
void
app_db_deinit_dpop_controller(struct dbop_controller *pt_dbop_ctr)
{
  pthread_mutex_lock(&pt_dbop_ctr->mtx);
  l_queue_destroy(pt_dbop_ctr->pt_dbop_resp_waiting_queue, l_free);
  pthread_mutex_unlock(&pt_dbop_ctr->mtx);

  pthread_mutex_destroy(&pt_dbop_ctr->mtx);
}
/**
 * @brief To initialize an object for waiting response queue
 */
app_state_t
app_db_init_dbop_resp_waiting_obj(
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj,
  const gw_pro_command_t kp_cmd,
  const uint32_t kp_cmd_id)
{
  pthread_mutex_init(&pt_dbop_resp_waiting_obj->mtx, NULL);
  pthread_cond_init(&pt_dbop_resp_waiting_obj->cond_resp_received, NULL);
  pt_dbop_resp_waiting_obj->db_resp_event = EV_DB_OP_DEFAULT;
  pt_dbop_resp_waiting_obj->command_id = kp_cmd_id;
  pt_dbop_resp_waiting_obj->command = kp_cmd;
  pt_dbop_resp_waiting_obj->tim_val = time(NULL);

  return ST_OK;
}
/**
 * @brief De-initialize the object for the waiting response queue
 */
app_state_t
app_db_deinit_dbop_resp_waiting_obj(
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj)
{
  pthread_mutex_destroy(&pt_dbop_resp_waiting_obj->mtx);
  pthread_cond_destroy(&pt_dbop_resp_waiting_obj->cond_resp_received);
  return ST_OK;
}
/**
 * @brief Put the object onto the waiting response queue
 */
app_state_t
app_db_put_dbop_waiting_obj_on_queue(
  struct dbop_controller *pt_dbop_ctr,
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj)
{
  app_state_t tpret = ST_OK;

  if((NULL == pt_dbop_ctr) || (NULL == pt_dbop_resp_waiting_obj))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }
  
  pthread_mutex_lock(&pt_dbop_ctr->mtx);
  l_queue_push_tail(pt_dbop_ctr->pt_dbop_resp_waiting_queue,
                    pt_dbop_resp_waiting_obj);
  pthread_mutex_unlock(&pt_dbop_ctr->mtx);

  return tpret;
}
/**
 * @brief Remove the object from the waiting response queue
 */
app_state_t
app_db_remove_dbop_waiting_obj_from_queue(
  struct dbop_controller *pt_dbop_ctr,
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj)
{
  app_state_t tpret = ST_OK;

  if((NULL == pt_dbop_ctr) || (NULL == pt_dbop_resp_waiting_obj))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }
  
  pthread_mutex_lock(&pt_dbop_ctr->mtx);
  l_queue_remove(pt_dbop_ctr->pt_dbop_resp_waiting_queue,
                 pt_dbop_resp_waiting_obj);
  pthread_mutex_unlock(&pt_dbop_ctr->mtx);

  return tpret;
}
/**
 * @brief The callback for object comparison used in
 *        @p app_db_signal_and_pop_dbop_waiting_obj
 */
static bool
queue_match_command_id(
  const void *data,
  const void *user_data)
{
  bool tpret = false;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)data;
  uint32_t kp_cmd_id = (uint32_t)user_data;

  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return tpret;
  }

  DBG_LOG_DEBUG("Resp cmd_id %u, waiting_obj cmd_id %u",
                kp_cmd_id,
                pt_dbop_resp_waiting_obj->command_id);
  if(pt_dbop_resp_waiting_obj->command_id == kp_cmd_id)
  {
    tpret = true;
  }

  return tpret;
}
/**
 * @brief To pop the object from a queue (which an IPC message from process
 *        sql_op has been received). Specifically, this function would
 *        search the object based on commandId contained in
 *        @p pt_msg_from_db . The found object would be removed from the
 *        queue and trigger the corresponding condition signal
 *        @p cond_resp_received .
 */
app_state_t
app_db_signal_and_pop_dbop_waiting_obj(
  struct dbop_controller *pt_dbop_ctr,
  gw_pro_message_header_t *pt_msg_from_db)
{
  app_state_t tpret = ST_OK;
  uint32_t kp_cmd_id = 0;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_msg_from_db))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  kp_cmd_id = pt_msg_from_db->command_or_response.responseMsg.commandId;

  pthread_mutex_lock(&pt_dbop_ctr->mtx);
  // Remove the object if pt_dbop_resp_waiting_obj->command_id == kp_cmd_id
  pt_dbop_resp_waiting_obj = l_queue_remove_if(
    pt_dbop_ctr->pt_dbop_resp_waiting_queue,
    queue_match_command_id,
    (const void *)kp_cmd_id);
  if(pt_dbop_resp_waiting_obj)
  {
    // To signal the reception of response
    DBG_LOG_DEBUG("Recv-ed response from sql\n");

    pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
    // To copy the response info
    memcpy(&pt_dbop_resp_waiting_obj->resp_info,
           &pt_msg_from_db->command_or_response.responseMsg,
           sizeof(gw_pro_message_response_t));
    pt_dbop_resp_waiting_obj->db_resp_event = EV_DB_OP_RESP_RECV;
    pthread_cond_signal(&pt_dbop_resp_waiting_obj->cond_resp_received);
    pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  }
  else
  {
    DBG_LOG_WARN(
      "No matched waiting obj found:recd-cmd=%d, ID=%d\n",
      pt_msg_from_db->command_or_response.responseMsg.command, \
      pt_msg_from_db->command_or_response.responseMsg.commandId);
  }
  pthread_mutex_unlock(&pt_dbop_ctr->mtx);

  return tpret;
}
/**
 * @brief To handle commands from process sql_op
 */
app_state_t
app_db_handle_command_from_db(gw_pro_message_header_t *pt_msg_from_db)
{
  app_state_t tpret = ST_OK;

  if(NULL == pt_msg_from_db)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  if(GW_PRO_COMRESPFG_COMMAND != pt_msg_from_db->comRespFg)
  {
    DBG_LOG_ERR("Unexpected NULL comRespFg");
    tpret = ST_ERR;
    return tpret;
  }

  switch(pt_msg_from_db->command_or_response.commandMsg.command)
  {
    case gw_pro_command_report_new_modification:
    {
      tpret = app_db_put_db_report_on_queue(
        app_db_get_report_handler(),
        &pt_msg_from_db->command_or_response.commandMsg.param.report_new_modification_param);
      break;
    }
    default:
    {
      DBG_LOG_ERR("Unsupported command");
      tpret = ST_ERR;
      break;
    }
  }
  return tpret;
}
/**
 * @brief Get the report handler (to deal with the report from the process
 *        sql_op via IPC)
 */
struct db_report_handler *
app_db_get_report_handler(void)
{
  return &g_db_report_handler;
}
/**
 * @brief To initialize the mutex, condition viriable and the
 *        queue for handling database modification report
 */
app_state_t
app_db_init_report_handler(struct db_report_handler *pt_db_report_handler)
{
  app_state_t tpret = ST_OK;

  if(NULL == pt_db_report_handler)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  pthread_mutex_init(&pt_db_report_handler->mtx, NULL);
  pthread_cond_init(&pt_db_report_handler->cond_dt_available, NULL);
  pt_db_report_handler->pt_report_queue = l_queue_new();

  return tpret;
}
/**
 * @brief To free the queue, condition viriable, and the mutex
 */
void
app_db_deinit_report_handler(struct db_report_handler *pt_db_report_handler)
{
  if(NULL == pt_db_report_handler)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }

  pthread_mutex_lock(&pt_db_report_handler->mtx);
  l_queue_destroy(pt_db_report_handler->pt_report_queue, l_free);
  pthread_mutex_unlock(&pt_db_report_handler->mtx);

  pthread_cond_destroy(&pt_db_report_handler->cond_dt_available);
  pthread_mutex_destroy(&pt_db_report_handler->mtx);

  return;
}
/**
 * @brief Put the object onto the report queue (this function is generally
 *        called to apply the latest network/ntp configuration or BLE white
 *        list used in the process BLE)
 * @param @p pt_db_report_handler : the handler including the report queue
 * @param @p pt_event_tbl_update : table update event
 * @param @p kp_report_type : to indicate which table has been updated
 * @return @p ST_OK if no error happens
 */
app_state_t
app_db_put_db_table_update_event_on_queue(
  struct db_report_handler *pt_db_report_handler,
  struct db_event_table_updated *pt_event_tbl_update,
  enum db_report_type kp_report_type)
{
  app_state_t tpret = ST_OK;
  struct db_report_node *pt_report_node = NULL;

  if((NULL == pt_db_report_handler) || (NULL == pt_event_tbl_update))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  pt_report_node =
    (struct db_report_node *)l_malloc(sizeof(struct db_report_node));
  if(NULL == pt_report_node)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    return tpret;
  }
  memset(pt_report_node, 0, sizeof(struct db_report_node));

  pt_report_node->report_type = kp_report_type;
  memcpy(&pt_report_node->report_info.db_event_tbl_update,
         pt_event_tbl_update,
         sizeof(struct db_event_table_updated));

  // To put pt_report_node onto the report queue and trigger the signal
  // which is blocked at thread @p thandler_db_modification_report
  pthread_mutex_lock(&pt_db_report_handler->mtx);
  l_queue_push_tail(pt_db_report_handler->pt_report_queue,
                    (void *)pt_report_node);
  pthread_cond_signal(&pt_db_report_handler->cond_dt_available);
  pthread_mutex_unlock(&pt_db_report_handler->mtx);

  return tpret;
}
/**
 * @brief Put the object onto the report queue (this function is generally
 *        called when a database modification report has been received)
 * @param @p pt_db_report_handler : the handler including the report queue
 * @param @p pt_mod_report : The modification report information (e.g.,
 *                           which operation on which table)
 * @return @p ST_OK if no error happens
 */
app_state_t
app_db_put_db_report_on_queue(
  struct db_report_handler *pt_db_report_handler,
  gw_pro_command_report_new_modification_param_t *pt_mod_report)
{
  app_state_t tpret = ST_OK;
  struct db_report_node *pt_report_node = NULL;

  if((NULL == pt_db_report_handler) || (NULL == pt_mod_report))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  pt_report_node =
    (struct db_report_node *)l_malloc(sizeof(struct db_report_node));
  if(NULL == pt_report_node)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    return tpret;
  }
  memset(pt_report_node, 0, sizeof(struct db_report_node));

  pt_report_node->report_type = DB_REPORT_MODIFICATION;
  memcpy(&pt_report_node->report_info.db_new_modification,
         pt_mod_report,
         sizeof(gw_pro_command_report_new_modification_param_t));

  // To put pt_report_node onto the report queue and trigger the signal
  // which is blocked at thread @p thandler_db_modification_report
  pthread_mutex_lock(&pt_db_report_handler->mtx);
  l_queue_push_tail(pt_db_report_handler->pt_report_queue,
                    (void *)pt_report_node);
  pthread_cond_signal(&pt_db_report_handler->cond_dt_available);
  pthread_mutex_unlock(&pt_db_report_handler->mtx);

  return tpret;
}
/**
 * @brief Initialize the database controller (handle)
 */
app_state_t
app_db_init_database_op_ctr(
  struct database_operation_controller *pt_db_op_ctr)
{
  app_state_t tpret = ST_OK;

  // Get the message queue (create it if does not exist)
  // This queue is used to transfer messages from process sql_op
  pt_db_op_ctr->msgid_from_db = msgget(MSG_BLE_QUEUE_KEY,
                                       IPC_CREAT | MSG_QUEUE_FLAG);
  if(pt_db_op_ctr->msgid_from_db < 0)
  {
    DBG_LOG_ERR("Failed to create msg-queue, for %s", strerror(errno));
    tpret = ST_ERR;
    goto EXIT;
  }
  // Get the message queue (create it if does not exist)
  // This queue is used to transfer messages to process sql_op
  pt_db_op_ctr->msgid_to_db = msgget(MSG_SQL_QUEUE_KEY,
                                     IPC_CREAT | MSG_QUEUE_FLAG);
  if(pt_db_op_ctr->msgid_to_db < 0)
  {
    DBG_LOG_ERR("Failed to create msg_queue, for %s", strerror(errno));
    tpret = ST_ERR;
    goto EXIT;
  }

  pthread_mutex_init(&pt_db_op_ctr->mtx, NULL);
  pt_db_op_ctr->is_resp_received = false;
  pt_db_op_ctr->pt_dtitem_queue = l_queue_new();

EXIT:
  return tpret;
}
/**
 * @brief De-initialize the database controller (handle)
 */
void
app_db_deinit_database_op_ctr(
  struct database_operation_controller *pt_db_op_ctr)
{
  if(pt_db_op_ctr == NULL)
  {
    return;
  }

  msgctl(pt_db_op_ctr->msgid_from_db, IPC_RMID, 0);
  msgctl(pt_db_op_ctr->msgid_to_db, IPC_RMID, 0);
  pthread_mutex_destroy(&pt_db_op_ctr->mtx);
  l_queue_destroy(pt_db_op_ctr->pt_dtitem_queue, l_free);

  return;
}
/**
 * @brief To get the database controller (handle)
 */
struct database_operation_controller *
app_db_get_database_op_ctr(void)
{
  return &g_database_op_ctr;
}
/**
 * @brief Select items from a table (i.e., return indexes based on given
 *        filters).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_filter INPUT: Filter condition
 * @param bufsz INPUT: To indicate the maximal size of @p pt_idbuf
 * @param pt_idbuf The returned filtered indexes
 * @return The number of filtered items (or -1 indicating errors)
 */
int32_t
app_db_fetch_filtered_index(
  struct dbop_controller *pt_dbop_ctr,
  const gw_pro_command_sqlite_filter_param_t *pt_filter,
  uint32_t bufsz,
  uint32_t *pt_idbuf)
{
  int32_t tpret = 0;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;
  struct msg_format_controller *pt_msg_format_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_filter) ||
     (NULL == pt_idbuf))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = -1;
    goto EXIT;
  }

  switch(pt_filter->table)
  {
    case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:
    case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE:
    case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE:
    case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE:
      break;
    default:
      DBG_LOG_ERR("Unexpected table IDX %d", pt_filter->table);
      tpret = -1;
      goto EXIT;
  }

  // To fill the message to be sent to the process sql_op
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = -1;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_filter;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  memcpy(
    &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_filter_item_param, \
    pt_filter, sizeof(gw_pro_command_sqlite_filter_param_t));

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);

  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));

  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = -1;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX) {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n", MSGSEND_ERR_CNT_MAX);
          fflush(stdout);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = -1;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = -1;
    goto EXIT;
  }
  DBG_LOG_INFO(
    "Table %d filter response information: cmd = %d, cmdId = %d, success(0)/fail(1) = %d, dataNum = %d", \
    pt_filter->table, \
    pt_dbop_resp_waiting_obj->resp_info.command, pt_dbop_resp_waiting_obj->resp_info.commandId, \
    pt_dbop_resp_waiting_obj->resp_info.response, \
    pt_dbop_resp_waiting_obj->resp_info.responseInfo.sqlite_filter_item_reponse.dataNum);

  tpret =
    pt_dbop_resp_waiting_obj->resp_info.responseInfo.
    sqlite_filter_item_reponse
    .dataNum;
  if((tpret * sizeof(pt_idbuf[0])) > bufsz)
  {
    DBG_LOG_WARN("The returned buffer does not have enough space");
    tpret = bufsz / sizeof(pt_idbuf[0]);
  }
  memcpy(pt_idbuf,
         pt_dbop_resp_waiting_obj->resp_info.responseInfo.sqlite_filter_item_reponse.filteredItemIdx,
         tpret * sizeof(pt_idbuf[0]));

EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
/**
 * @brief To collect sensor data based on the rules (i.e., configuration)
 * @param sensorConfig: the input pointer to the configuration
 * @return Return @p ST_OK if everthing is OK.
 */
app_state_t
app_db_load_sensor_data(sensorConfig_t *sensorConfig)
{
  app_state_t tpret = ST_OK;
  uint32_t i = 0, j = 0, cnt = 0;
  time_t kp_tstamp;
  SKFChina_Common_MeasurementType tp_mtype_array[SZ_DTSELECTION_GROUP];
  const uint32_t kp_sz = sizeof(g_mtype_array) / sizeof(g_mtype_array[0]);
  bool retryFg = false;
  enum sensortype kp_stype = app_pro_gen_get_active_sensor_type_v2();

  // 1 "Short" data of VEL_OV to check whether new data exists
  // 1.1 To select VEL_OV
  dtitem_exist_fg = false;
  tp_mtype_array[0] = SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV;
  app_froto_shortDataReveivedRetryCnt = 0;
  do {
    app_froto_shortDataReveivedFg = 0;
    tpret = app_froto_send_data_selection_request(tp_mtype_array,
                                                  1,
                                                  app_pro_gen_get_gateway_idstr(
                                                    app_pro_gen_get_ctr()),
                                                  app_pro_gen_allocate_msg_seq_no());
    if(ST_OK == tpret)
    {
      while(1)
      {
        if(app_froto_shortDataReveivedFg & 0x01)
        {
          // Received
          usleep(200 * 1000);
          retryFg = false;
          break;
        }
        else
        {
          if((app_froto_shortDataReveivedFg & 0x02)
             && (app_froto_shortDataReveivedRetryCnt >= 3))
          {
            // Timeout & give it up
            retryFg = false;
            tpret = ST_ERR;
            DBG_LOG_ERR("Timeout & give it up\r\n");
            return tpret;
          }
          else
          {
            if(app_froto_shortDataReveivedFg & 0x02)
            {
              // Timeout & retry
              // Increase the sleep time can migitate packet lost
              DBG_LOG_WARN("Timeout & retry\r\n");
              sleep(app_froto_shortDataReveivedRetryCnt);
              retryFg = true;
              break;
            }
          }
        }
      }
      if(retryFg == true)
      {
        continue;
      }
      else
      {
        break;
      }
    }
    else if(ST_BUSSY == tpret)
    {
      DBG_LOG_ERR("Failed to retrieve data %d, for busy\r\n",
                  tp_mtype_array[cnt]);
      // Increase the sleep time can migitate packet lost
      sleep(1);
      continue;
    }
    else
    {
      DBG_LOG_ERR("Failed to retrieve data\r\n");
      break;
    }
  } while(1);

  // 1.2 To collect temperature
  // TODO: to optimize in the code structure
  // Set i to the index of
  // SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT in
  // g_mtype_array
  i = 9;
  j = 0;
  memset(tp_mtype_array, 0, sizeof(tp_mtype_array));
  // TODO: to optimize in the code structure
  // "9 + 3" indicates that the index of
  // SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN in
  // g_mtype_array
  while(i < 9 + 3)
  {
    DBG_LOG_DEBUG("Retrieve data %d (%u)\r\n",
                  g_mtype_array[i], i);
    tp_mtype_array[j % SZ_DTSELECTION_GROUP] = g_mtype_array[i];
    i++;
    j++;
    if((j >= SZ_DTSELECTION_GROUP) || (i >= kp_sz))
    {
      app_froto_shortDataReveivedRetryCnt = 0;
      while(1)
      {
        if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
             app_pro_gen_get_ctr()))
        {
          tpret = ST_ERR;
          return;
        }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
        if(false ==
          app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
        {
          tpret = ST_ERR;
          return;
        }
        app_froto_shortDataReveivedFg = 0;
        tpret = app_froto_send_data_selection_request(tp_mtype_array,
                                                      j,
                                                      app_pro_gen_get_gateway_idstr(
                                                        app_pro_gen_get_ctr()),
                                                      app_pro_gen_allocate_msg_seq_no());
        if(ST_OK == tpret)
        {
          while(1)
          {
            if(app_froto_shortDataReveivedFg & 0x01)
            {
              // Received
              usleep(200 * 1000);
              retryFg = false;
              break;
            }
            else
            {
              if((app_froto_shortDataReveivedFg & 0x02)
                 && (app_froto_shortDataReveivedRetryCnt >= 3))
              {
                // Timeout & give it up
                retryFg = false;
                tpret = ST_ERR;
                DBG_LOG_ERR("Timeout & give it up\r\n");
                return tpret;
              }
              else
              {
                if(app_froto_shortDataReveivedFg & 0x02)
                {
                  // Timeout & retry
                  // Increase the sleep time can migitate packet lost
                  DBG_LOG_WARN("Timeout & retry\r\n");
                  sleep(app_froto_shortDataReveivedRetryCnt);
                  retryFg = true;
                  break;
                }
              }
            }
          }
          if(retryFg == true)
          {
            continue;
          }
          else
          {
            break;
          }
        }
        else if(ST_BUSSY == tpret)
        {
          for(cnt = 0; cnt < j; cnt++)
          {
            DBG_LOG_ERR("Failed to retrieve data %d, for busy\r\n",
                        tp_mtype_array[cnt]);
          }
          // Increase the sleep time can migitate packet lost
          sleep(1);
          continue;
        }
        else
        {
          DBG_LOG_ERR("Failed to retrieve data\r\n");
          break;
        }
      }
      j = 0;
      memset(tp_mtype_array, 0, sizeof(tp_mtype_array));
    }
  }
  // 1.3 Existed or not
  // if(dtitem_exist_fg == true)
  // {
  //   return tpret;
  // }
  // 2 Collect data if dtitem_exist_fg is false
  // 2.1 "Short" data
  i = 3;
  j = 0;
  memset(tp_mtype_array, 0, sizeof(tp_mtype_array));
  while(i < kp_sz)
  {
    tp_mtype_array[j % SZ_DTSELECTION_GROUP] = g_mtype_array[i];
    i++;
    j++;
    if((j >= SZ_DTSELECTION_GROUP) || (i >= kp_sz))
    {
      app_froto_shortDataReveivedRetryCnt = 0;
      while(1)
      {
        if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
             app_pro_gen_get_ctr()))
        {
          tpret = ST_ERR;
          return;
        }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
        if(false ==
        app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
        {
          tpret = ST_ERR;
          return;
        }
        app_froto_shortDataReveivedFg = 0;
        tpret = app_froto_send_data_selection_request(tp_mtype_array,
                                                      j,
                                                      app_pro_gen_get_gateway_idstr(
                                                        app_pro_gen_get_ctr()),
                                                      app_pro_gen_allocate_msg_seq_no());
        if(ST_OK == tpret)
        {
          while(1)
          {
            if(app_froto_shortDataReveivedFg & 0x01)
            {
              // Received
              usleep(200 * 1000);
              retryFg = false;
              break;
            }
            else
            {
              if((app_froto_shortDataReveivedFg & 0x02)
                 && (app_froto_shortDataReveivedRetryCnt >= 3))
              {
                // Timeout & give it up
                retryFg = false;
                tpret = ST_ERR;
                DBG_LOG_ERR("Timeout & give it up\r\n");
                return tpret;
              }
              else
              {
                if(app_froto_shortDataReveivedFg & 0x02)
                {
                  // Timeout & retry
                  // Increase the sleep time can migitate packet lost
                  DBG_LOG_WARN("Timeout & retry\r\n");
                  sleep(app_froto_shortDataReveivedRetryCnt);
                  retryFg = true;
                  break;
                }
              }
            }
          }
          if(retryFg == true)
          {
            continue;
          }
          else
          {
            break;
          }
        }
        else if(ST_BUSSY == tpret)
        {
          for(cnt = 0; cnt < j; cnt++)
          {
            DBG_LOG_ERR("Failed to retrieve data %d, for busy\r\n",
                        tp_mtype_array[cnt]);
          }
          // Increase the sleep time can migitate packet lost
          sleep(1);
          continue;
        }
        else
        {
          DBG_LOG_ERR("Failed to retrieve data\r\n");
          break;
        }
      }
      j = 0;
      memset(tp_mtype_array, 0, sizeof(tp_mtype_array));
    }
  }

  // 3 Waveform
  dtitem_acc_wave_exist_fgx = false;
  dtitem_acc_wave_exist_fgy = false;
  dtitem_acc_wave_exist_fgz = false;
#if (SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE == 1)
  dtitem_env_wave_exist_fg = false;
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE == 1 */
#if (SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE == 1)
  dtitem_vel_wave_exist_fg = false;
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE == 1 */
  DBG_LOG_DEBUG("ACC_WAVE is required");
  // To collect SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE
  i = 0;
  j = 0;  
#if (SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1)
  kp_tstamp = time(NULL);
  DBG_LOG_DEBUG("period_regularSensing_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s);
  DBG_LOG_DEBUG("period_wave_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.period_wave_s);
  DBG_LOG_DEBUG("refTime_regularSensing_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.refTime_regularSensing_s);
  DBG_LOG_DEBUG("refTime_wave_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s);
  DBG_LOG_DEBUG("lastRegularSensingTs: %lu", lastRegularSensingTs);
  DBG_LOG_DEBUG("lastRegularSensingMeasID: %u", lastRegularSensingMeasID);
  DBG_LOG_DEBUG("current time: %d", kp_tstamp);
  if(((sensorConfig != NULL) && (lastRegularSensingTs != 0) &&
     (sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s !=
      0) &&
     (sensorConfig->bulletSensorConfig.schConfig.period_wave_s != 0) &&
     ((kp_tstamp >
       sensorConfig->bulletSensorConfig.schConfig.refTime_regularSensing_s)
      ||
      (kp_tstamp >
       sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s))) ||
      (lastRegularSensingMeasID >= 32768))
  {
    if(
      (
      ((lastRegularSensingTs) >=
       ((lastRegularSensingTs -
         sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s) /
        sensorConfig->bulletSensorConfig.schConfig.period_wave_s) *
       sensorConfig->bulletSensorConfig.schConfig.period_wave_s +
       sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s) &&
      ((lastRegularSensingTs -
        sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s)
       <
       ((lastRegularSensingTs -
         sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s) /
        sensorConfig->bulletSensorConfig.schConfig.period_wave_s) *
       sensorConfig->bulletSensorConfig.schConfig.period_wave_s +
       sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s)
      ) || 
      (lastRegularSensingMeasID >= 32768)
      )
    {
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1 */
      DBG_LOG_DEBUG("Sensor type: %d\r\n", kp_stype);
      if(kp_stype == SS_TYPE_PREDICTP)
      {
        uint32_t tpidx = 0;
		    SKFChina_SensingDataUpload_MeasurementTypeMsg kp_mtype_msg = {0};

        while(1)
        {
          if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
            app_pro_gen_get_ctr()))
          {
            tpret = ST_ERR;
            return;
          }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
          if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
          if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
          {
            tpret = ST_ERR;
            return;
          }

          memset( &kp_mtype_msg, 0, sizeof(SKFChina_SensingDataUpload_MeasurementTypeMsg));
			    kp_mtype_msg.measure_type = SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE;

          kp_mtype_msg.has_dimension = true;
          if(0 == tpidx)
          {
            // X
            kp_mtype_msg.dimension = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X;
            if(dtitem_acc_wave_exist_fgx == true)
            {
              tpidx = tpidx + 1;
              continue;
            }else{
              DBG_LOG_INFO("ACC_WAVE X is going to be uploaded\r\n");
            }
          }
          else if(1 == tpidx)
          {
            // Y
            kp_mtype_msg.dimension = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y;
            if(dtitem_acc_wave_exist_fgy == true)
            {
              tpidx = tpidx + 1;
              continue;
            }else{
              DBG_LOG_INFO("ACC_WAVE Y is going to be uploaded\r\n");
            }
          }
          else if(2 == tpidx)
          {
            // Z
            kp_mtype_msg.dimension = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Z;
            if(dtitem_acc_wave_exist_fgz == true)
            {
              tpidx = tpidx + 1;
              break;
              continue;
            }else{
              DBG_LOG_INFO("ACC_WAVE Z is going to be uploaded\r\n");
            }
          }
          else
          {
            DBG_LOG_WARN("SHOULD NOT BE HERE!");
            tpret = ST_ERR;
            return;
          }

          tpret = app_froto_send_data_selection_request_by_mtype_msg(
            kp_mtype_msg, 
            app_pro_gen_get_gateway_idstr(app_pro_gen_get_ctr()), 
            app_pro_gen_allocate_msg_seq_no());
          if(ST_OK == tpret)
          {				
            sleep(3);
            if(tpidx >= 2)
            {
              break;
            }
            tpidx = tpidx + 1;
          }
          else if(ST_BUSSY == tpret)
          {
            DBG_LOG_ERR("Failed to retrieve data ACC, for bussy\r\n");
            sleep(3);
            continue;
          }
          else
          {
            DBG_LOG_ERR("Failed to select data (tpidx %d)\r\n", tpidx);
            break;
          }
        }
      }else{
#if (SENSOR_DATA_COL_RULE_REQUIRING_ACC_WAVE == 1)
		    SKFChina_SensingDataUpload_MeasurementTypeMsg kp_mtype_msg = {0};
        
        if(dtitem_acc_wave_exist_fgx == false)
        {
          DBG_LOG_DEBUG("ACC_WAVE is going to be uploaded\r\n");
          while(1)
          {
            if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
              app_pro_gen_get_ctr()))
            {
              tpret = ST_ERR;
              return;
            }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
            if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))
#else
            if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
            {
              tpret = ST_ERR;
              return;
            }

            memset( &kp_mtype_msg, 0, sizeof(SKFChina_SensingDataUpload_MeasurementTypeMsg));
            kp_mtype_msg.measure_type = SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE;

            kp_mtype_msg.has_dimension = false;
            tpret = app_froto_send_data_selection_request_by_mtype_msg(
              kp_mtype_msg, 
              app_pro_gen_get_gateway_idstr(app_pro_gen_get_ctr()), 
              app_pro_gen_allocate_msg_seq_no());
            if(ST_OK == tpret)
            {				
              sleep(3);
              break;
            }
            else if(ST_BUSSY == tpret)
            {
              DBG_LOG_ERR("Failed to retrieve data ACC, for bussy\r\n");
              sleep(3);
              continue;
            }
            else
            {
              DBG_LOG_ERR("Failed to select data\r\n");
              break;
            }
          }
        }
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_ACC_WAVE == 1 */
      }
#if (SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1)
    }
  }
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1 */
#if (SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE == 1)
  DBG_LOG_DEBUG("ENV3_WAVE is required");
  // To collect SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE
  i = 1;
  j = 0;
#if (SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1)
  kp_tstamp = time(NULL);
  DBG_LOG_DEBUG("period_regularSensing_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s);
  DBG_LOG_DEBUG("period_wave_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.period_wave_s);
  DBG_LOG_DEBUG("refTime_regularSensing_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.refTime_regularSensing_s);
  DBG_LOG_DEBUG("refTime_wave_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s);
  DBG_LOG_DEBUG("lastRegularSensingTs: %lu", lastRegularSensingTs);
  DBG_LOG_DEBUG("lastRegularSensingMeasID: %u", lastRegularSensingMeasID);
  DBG_LOG_DEBUG("current time: %d", kp_tstamp);
  if(((sensorConfig != NULL) && (lastRegularSensingTs != 0) &&
     (sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s !=
      0) &&
     (sensorConfig->bulletSensorConfig.schConfig.period_wave_s != 0) &&
     ((kp_tstamp >
       sensorConfig->bulletSensorConfig.schConfig.refTime_regularSensing_s)
      ||
      (kp_tstamp >
       sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s))) ||
      (lastRegularSensingMeasID >= 32768))
  {
    if(
      (
      ((lastRegularSensingTs) >=
       ((lastRegularSensingTs -
         sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s) /
        sensorConfig->bulletSensorConfig.schConfig.period_wave_s) *
       sensorConfig->bulletSensorConfig.schConfig.period_wave_s +
       sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s) &&
      ((lastRegularSensingTs -
        sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s)
       <
       ((lastRegularSensingTs -
         sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s) /
        sensorConfig->bulletSensorConfig.schConfig.period_wave_s) *
       sensorConfig->bulletSensorConfig.schConfig.period_wave_s +
       sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s)
      ) ||
      (lastRegularSensingMeasID >= 32768)
      )
    {
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1 */
      if(dtitem_env_wave_exist_fg == false)
      {
        DBG_LOG_DEBUG("ENV3_WAVE is going to be uploaded\r\n");
        while(1)
        {
          if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
              app_pro_gen_get_ctr()))
          {
            tpret = ST_ERR;
            return;
          }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
          if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
          if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
          {
            tpret = ST_ERR;
            return;
          }
          tpret = app_froto_send_data_selection_request(
            &g_mtype_array[i],
            1,
            app_pro_gen_get_gateway_idstr(app_pro_gen_get_ctr()),
            app_pro_gen_allocate_msg_seq_no());
          if(ST_OK == tpret)
          {
            // Increase the sleep time can migitate packet lost
            sleep(3);
            break;
          }
          else if(ST_BUSSY == tpret)
          {
            DBG_LOG_ERR("Failed to retrieve data %d, for busy\r\n",
                        g_mtype_array[i]);
            // Increase the sleep time can migitate packet lost
            sleep(3);
            continue;
          }
          else
          {
            DBG_LOG_ERR("Failed to select data\r\n");
            break;
          }
        }
      }
#if (SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1)
    }
  }
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1 */
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE == 1 */
#if (SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE == 1)
  DBG_LOG_DEBUG("VEL_WAVE is required");
  // To collect SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE
  i = 2;
  j = 0;
#if (SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1)
  kp_tstamp = time(NULL);
  DBG_LOG_DEBUG("period_regularSensing_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s);
  DBG_LOG_DEBUG("period_wave_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.period_wave_s);
  DBG_LOG_DEBUG("refTime_regularSensing_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.refTime_regularSensing_s);
  DBG_LOG_DEBUG("refTime_wave_s: %u",
                sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s);
  DBG_LOG_DEBUG("lastRegularSensingTs: %lu", lastRegularSensingTs);
  DBG_LOG_DEBUG("lastRegularSensingMeasID: %u", lastRegularSensingMeasID);
  DBG_LOG_DEBUG("current time: %d", kp_tstamp);
  if(((sensorConfig != NULL) && (lastRegularSensingTs != 0) &&
     (sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s !=
      0) &&
     (sensorConfig->bulletSensorConfig.schConfig.period_wave_s != 0) &&
     ((kp_tstamp >
       sensorConfig->bulletSensorConfig.schConfig.refTime_regularSensing_s)
      ||
      (kp_tstamp >
       sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s)))
      || (lastRegularSensingMeasID >= 32768))
  {
    if(
      (
      ((lastRegularSensingTs) >=
       ((lastRegularSensingTs -
         sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s) /
        sensorConfig->bulletSensorConfig.schConfig.period_wave_s) *
       sensorConfig->bulletSensorConfig.schConfig.period_wave_s +
       sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s) &&
      ((lastRegularSensingTs -
        sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s)
       <
       ((lastRegularSensingTs -
         sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s) /
        sensorConfig->bulletSensorConfig.schConfig.period_wave_s) *
       sensorConfig->bulletSensorConfig.schConfig.period_wave_s +
       sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s)
      ) ||
      (lastRegularSensingMeasID >= 32768)
      )
    {
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1 */
      if(dtitem_vel_wave_exist_fg == false)
      {
        DBG_LOG_DEBUG("VEL_WAVE is going to be uploaded\r\n");
        while(1)
        {
          if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
              app_pro_gen_get_ctr()))
          {
            tpret = ST_ERR;
            return;
          }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
          if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
          if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
          {
            tpret = ST_ERR;
            return;
          }
          tpret = app_froto_send_data_selection_request(
            &g_mtype_array[i],
            1,
            app_pro_gen_get_gateway_idstr(app_pro_gen_get_ctr()),
            app_pro_gen_allocate_msg_seq_no());
          if(ST_OK == tpret)
          {
            // Increase the sleep time can migitate packet lost
            sleep(3);
            break;
          }
          else if(ST_BUSSY == tpret)
          {
            DBG_LOG_ERR("Failed to retrieve data %d, for busy\r\n",
                        g_mtype_array[i]);
            // Increase the sleep time can migitate packet lost
            sleep(3);
            continue;
          }
          else
          {
            DBG_LOG_ERR("Failed to select data\r\n");
            break;
          }
        }
      }
#if (SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1)
    }
  }
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_WAVE_BASED_ON_CONF == 1 */
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE == 1 */
  // 4 "Short" data (dummy tail)
  i = kp_sz - 1;
  j = 0;
  memset(tp_mtype_array, 0, sizeof(tp_mtype_array));
  while(i < kp_sz)
  {
    tp_mtype_array[j % SZ_DTSELECTION_GROUP] = g_mtype_array[i];
    i++;
    j++;
    if((j >= SZ_DTSELECTION_GROUP) || (i >= kp_sz))
    {
      while(1)
      {
        if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
             app_pro_gen_get_ctr()))
        {
          tpret = ST_ERR;
          return;
        }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
        if(false ==
        app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
        {
          tpret = ST_ERR;
          return;
        }
        app_froto_shortDataReveivedFg = 0;
        app_froto_shortDataReveivedRetryCnt = 0;
        tpret = app_froto_send_data_selection_request(tp_mtype_array,
                                                      j,
                                                      app_pro_gen_get_gateway_idstr(
                                                        app_pro_gen_get_ctr()),
                                                      app_pro_gen_allocate_msg_seq_no());
        if(ST_OK == tpret)
        {
          while(1)
          {
            if(app_froto_shortDataReveivedFg)
            {
              // Received or timeout
              usleep(200 * 1000);
              break;
            }
          }
          // Increase the sleep time can migitate packet lost
          // sleep(3);
          break;
        }
        else if(ST_BUSSY == tpret)
        {
          for(cnt = 0; cnt < j; cnt++)
          {
            DBG_LOG_ERR("Failed to retrieve data %d, for busy\r\n",
                        tp_mtype_array[cnt]);
          }
          // Increase the sleep time can migitate packet lost
          sleep(1);
          continue;
        }
        else
        {
          DBG_LOG_ERR("Failed to retrieve data\r\n");
          break;
        }
      }
      j = 0;
      memset(tp_mtype_array, 0, sizeof(tp_mtype_array));
    }
  }

  DBG_LOG_INFO("All data receivation is done");

  return tpret;
}

/**
 * @brief Fill the sensorConfig ( pointed by @p pt_sensor_conf ) based on
 *        the database ( pointed by @p pt_query_resp ).
 */
static app_state_t
to_fill_sensor_conf_by_data_from_db(
  const gw_pro_command_sqlite_query_item_response_t *pt_query_resp,
  sensorConfig_t *pt_sensor_conf,
  struct fouta_fw_info *fw_info)
{
  app_state_t tpret = ST_OK;

  if((NULL == pt_query_resp) || (NULL == pt_sensor_conf))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return ST_ERR;
  }

  for(uint32_t i = 0; i < pt_query_resp->dataNum; i++)
  {
    switch(pt_query_resp->
           inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field)
    {
      case 1:
      {
        // "INT PRIMARY KEY DEFAULT 0"
        // "nameId"
        pt_sensor_conf->nameId =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 2:
      {
        // "TEXT DEFAULT ' '"
        // "name"
        // TODO
        break;
      }
      case 3:
      {
        // "TEXT DEFAULT ' '",
        // "BLESensorName",
        // TODO
        break;
      }
      case 4:
      {
        // "TEXT DEFAULT ' '",
        // "macAddr",
        uint8_t tp_strbuf[GW_PRO_MAX_STRING_LEN_BYTE] = { 0 };
        for(uint32_t j = 0;
            j < strlen(pt_query_resp->
                       inquiredData[i %
                                    GW_PRO_MAX_DATA_FIELD_PER_OPERATION].
                       data.data_char);
            j++)
        {
          // Remove '-' in the MAC string
          if('-' != pt_query_resp
             ->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].
             data.data_char[j])
          {
            tp_strbuf[j % GW_PRO_MAX_STRING_LEN_BYTE] =
              pt_query_resp->
              inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
              data_char[j];
          }
        }
        DBG_LOG_DEBUG("The MAC-string original: %s -> %s",
                      pt_query_resp->
                      inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].
                      data.data_char,
                      tp_strbuf);
        idstr_to_bytes_array(pt_sensor_conf->macAddr.addr.addrArray,
                             sizeof(pt_sensor_conf->macAddr.addr.
                                    addrArray),
                             tp_strbuf);
        break;
      }
      case 5:
      {
        // "TEXT DEFAULT 'SKF'",
        // "manufacturer",
        snprintf(pt_sensor_conf->manufacturer,
          sizeof(pt_sensor_conf->manufacturer),
          "%s",
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_char);
        break;
      }
      case 6:
      {
        // "TEXT DEFAULT ' '",
        // "FirmwareVersion",
        uint8_t major, minor, bug;
        if(fw_info == NULL)
        {
          sscanf(pt_query_resp->
                inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
                "%d.%d.%d",
                &major, &minor, &bug);
          g_sys_sensor_fuota_info.currentVer = (major << 16) + (minor << 8) +
                                              (bug);
          DBG_LOG_DEBUG("g_sys_sensor_fuota_info.currentVer %u",
                        g_sys_sensor_fuota_info.currentVer);
        }else{
          sscanf(pt_query_resp->
                inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
                "%d.%d.%d",
                &major, &minor, &bug);
          fw_info->currentVer = (major << 16) + (minor << 8) + (bug);
          DBG_LOG_DEBUG("fw_info->currentVer %u", fw_info->currentVer);
        }
        break;
      }
      case 7:
      {
        // "INT DEFAULT 0",
        // "version",
        pt_sensor_conf->version =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 8:
      {
        // "INT DEFAULT 0",
        // "lastEditTime",
        pt_sensor_conf->lastEditTimeS =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint64;
        DBG_LOG_DEBUG("lastEditTimeS %lu", pt_sensor_conf->lastEditTimeS);
        break;
      }
      case 9:
      {
        // "BOOLEAN DEFAULT false",
        // "haveConfiguredToSensor",
        pt_sensor_conf->haveConfiguredToSensor =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 10:
      {
        // "INT DEFAULT 0",
        // "whenConfiguredToSensor",
        pt_sensor_conf->whenConfiguredToSensor =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint64;
        break;
      }
      case 11:
      {
        // "TEXT DEFAULT ' '",
        // "type",
        snprintf(pt_sensor_conf->type,
          sizeof(pt_sensor_conf->type),
          "%s",
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_char);
        break;
      }
      case 12:
      {
        // "INT DEFAULT 0",
        // "sensorMode",
        uint8_t sensorMode_tmp;
        sensorMode_tmp =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode =
          sensorMode_tmp;
        DBG_LOG_DEBUG("sensorMode %d",
                      pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode);
        break;
      }
      case 13:
      {
        // "REAL DEFAULT 0.0",
        // "batteryAlarmThreshold",
        pt_sensor_conf->bulletSensorConfig.sysConfig.
        batteryAlarmThrePercent =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 14:
      {
        // "INT DEFAULT 0",
        // "txPower_adv",
        pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAdv =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_int8;
        break;
      }
      case 15:
      {
        // "INT DEFAULT 0",
        // "dataRate_auxAdv",
        pt_sensor_conf->bulletSensorConfig.sysConfig.dataRateAuxAdv =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint16;
        break;
      }
      case 16:
      {
        // "INT DEFAULT 0",
        // "txPower_auxAdv",
        pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAuxAdv =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_int8;
        break;
      }
      case 17:
      {
        // "INT DEFAULT 0",
        // "period_quickPolling",
        pt_sensor_conf->bulletSensorConfig.schConfig.period_quickPolling_s
          = pt_query_resp->
            inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
            data_uint32;
        break;
      }
      case 18:
      {
        // "INT DEFAULT 0",
        // "referenceTime_quickPolling",
        pt_sensor_conf->bulletSensorConfig.schConfig.refTime_quickPolling_s
          = pt_query_resp->
            inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
            data_uint32;
        break;
      }
      case 19:
      {
        // "INT DEFAULT 0",
        // "period_regularSensing",
        pt_sensor_conf->bulletSensorConfig.schConfig.
        period_regularSensing_s =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 20:
      {
        // "INT DEFAULT 0",
        // "referenceTime_regularSensing",
        pt_sensor_conf->bulletSensorConfig.schConfig.
        refTime_regularSensing_s =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 21:
      {
        // "INT DEFAULT 0",
        // "period_comm",
        pt_sensor_conf->bulletSensorConfig.schConfig.period_comm_s =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 22:
      {
        // "INT DEFAULT 0",
        // "referenceTime_period_comm",
        pt_sensor_conf->bulletSensorConfig.schConfig.refTime_period_comm_s
          = pt_query_resp->
            inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
            data_uint32;
        break;
      }
      case 23:
      {
        // "INT DEFAULT 0",
        // "PRE_ACQ_VIB_FS_HZ",
        pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 24:
      {
        // "INT DEFAULT 0",
        // "PRE_ACQ_VIB_N",
        pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_n =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 25:
      {
        // "INT DEFAULT 0",
        // "PRE_ACQ_VIB_AXIS_ACQ_EVAL",
        pt_sensor_conf->bulletSensorConfig.senConfig.
        pre_acq_vib_axis_acq_eval =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 26:
      {
        // "INT DEFAULT 0",
        // "PRE_ACQ_VIB_RANGE",
        pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_range =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 27:
      {
        // "INT DEFAULT 0",
        // "PRE_ACQ_MAG_FS_HZ",
        pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 28:
      {
        // "INT DEFAULT 0",
        // "PRE_ACQ_MAG_N",
        pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_n =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 29:
      {
        // "INT DEFAULT 0",
        // "PRE_ACQ_MAG_AXIS_ACQ_EVAL",
        pt_sensor_conf->bulletSensorConfig.senConfig.
        pre_acq_mag_axis_acq_eval =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 30:
      {
        // "INT DEFAULT 0",
        // "Facc",
        pt_sensor_conf->bulletSensorConfig.senConfig.fAccHz =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 31:
      {
        // "INT DEFAULT 0",
        // "Nacc",
        pt_sensor_conf->bulletSensorConfig.senConfig.nAcc =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 32:
      {
        // "INT DEFAULT 0",
        // "ACQ_VIB_AXIS_ACQ_EVAL",
        pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval
          = pt_query_resp->
            inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
            data_uint8;
        break;
      }
      case 33:
      {
        // "INT DEFAULT 0",
        // "ACQ_VIB_RANGE",
        pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_range =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 34:
      {
        // "INT DEFAULT 0",
        // "ACQ_MAG_FS_HZ",
        pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_fs_hz =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 35:
      {
        // "INT DEFAULT 0",
        // "ACQ_MAG_N",
        pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_n =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 36:
      {
        // "INT DEFAULT 0",
        // "ACQ_MAG_AXIS_ACQ_EVAL",
        pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval
          = pt_query_resp->
            inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
            data_uint8;
        break;
      }
      case 37:
      {
        // "REAL DEFAULT 0.364",
        // "FS_COEF",
        pt_sensor_conf->bulletSensorConfig.senConfig.fs_coef =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 38:
      {
        // "REAL DEFAULT 0.0",
        // "GEE_COEF",
        pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 39:
      {
        // "REAL DEFAULT 0.0",
        // "V_COEF",
        pt_sensor_conf->bulletSensorConfig.algoConfig.V_COEF =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        DBG_LOG_DEBUG("V_COEF %f",
                      pt_sensor_conf->bulletSensorConfig.algoConfig.V_COEF);
        break;
      }
      case 40:
      {
        // "INT DEFAULT 0",
        // "MEAS_POSITION",
        pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_POSITION =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        DBG_LOG_DEBUG("MEAS_POSITION %d",
                      pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_POSITION);
        break;
      }
      case 41:
      {
        // "INT DEFAULT 0",
        // "MEAS_LOAD",
        pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_LOAD =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 42:
      {
        // "INT DEFAULT 0",
        // "MEAS_AXIS_VIB",
        pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 43:
      {
        // "INT DEFAULT 0",
        // "MEAS_AXIS_MAG",
        pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 44:
      {
        // "BOOLEAN DEFAULT false",
        // "VIB_START_FG",
        pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_START_FG =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 45:
      {
        // "REAL DEFAULT 0.0",
        // "VIB_RMS_START_TL",
        pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_RMS_START_TL =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 46:
      {
        // "BOOLEAN DEFAULT false",
        // "MAG_START_FG",
        pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_START_FG =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 47:
      {
        // "REAL DEFAULT 0.0",
        // "MAG_RMS_START_TL",
        pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_START_TL =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 48:
      {
        // "BOOLEAN DEFAULT false",
        // "MAG_STABLE_FG",
        pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_STABLE_FG =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 49:
      {
        // "REAL DEFAULT 0.0",
        // "MAG_RMS_VAR_TH",
        pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 50:
      {
        // "REAL DEFAULT 0.0",
        // "RPM_VAR_RANGE",
        pt_sensor_conf->bulletSensorConfig.algoConfig.RPM_VAR_RANGE =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 51:
      {
        // "INT DEFAULT 0",
        // "DEC_LOGIC_TEMP_M",
        pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint16;
        break;
      }
      case 52:
      {
        // "INT DEFAULT 0",
        // "DEC_LOGIC_TEMP_N",
        pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint16;
        break;
      }
      case 53:
      {
        // "INT DEFAULT 0",
        // "DEC_LOGIC_LEARN_NUM_TEMP",
        pt_sensor_conf->bulletSensorConfig.algoConfig.
        DEC_LOGIC_LEARN_NUM_TEMP =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint16;
        break;
      }
      case 54:
      {
        // "INT DEFAULT 0",
        // "DEC_LOGIC_VIB_M",
        pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint16;
        break;
      }
      case 55:
      {
        // "INT DEFAULT 0",
        // "DEC_LOGIC_VIB_N",
        pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint16;
        break;
      }
      case 56:
      {
        // "INT DEFAULT 0",
        // "DEC_LOGIC_LEARN_NUM_VIB",
        pt_sensor_conf->bulletSensorConfig.algoConfig.
        DEC_LOGIC_LEARN_NUM_VIB =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint16;
        break;
      }
      case 57:
      {
        // "INT DEFAULT 0",
        // "DEC_LOGIC_LEARN_NUM_MAG",
        pt_sensor_conf->bulletSensorConfig.algoConfig.
        DEC_LOGIC_LEARN_NUM_MAG =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint16;
        break;
      }
      case 58:
      {
        // "BOOLEAN DEFAULT false",
        // "FUNC_ANOM_TEMP",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 59:
      {
        // "BOOLEAN DEFAULT false",
        // "FUNC_ANOM_OV",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_OV =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 60:
      {
        // "BOOLEAN DEFAULT false",
        // "FUNC_ANOM_MECH",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 61:
      {
        // "BOOLEAN DEFAULT false",
        // "FUNC_ANOM_BRG",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 62:
      {
        // "BOOLEAN DEFAULT false",
        // "FUNC_ANOM_LUB",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 63:
      {
        // "BOOLEAN DEFAULT false",
        // "FUNC_ANOM_MTR",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 64:
      {
        // "BOOLEAN DEFAULT false",
        // "FUNC_ANOM_GEAR",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 65:
      {
        // "BOOLEAN DEFAULT false",
        // "FUNC_ANOM_FAN",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 66:
      {
        // "BOOLEAN DEFAULT false",
        // "FUNC_ANOM_PUMP",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 67:
      {
        // "INT DEFAULT 0",
        // "ASSET_LEVEL",
        pt_sensor_conf->bulletSensorConfig.algoConfig.ASSET_LEVEL =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 68:
      {
        // "INT DEFAULT 0",
        // "FLEX_TYPE",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FLEX_TYPE =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 69:
      {
        // "REAL DEFAULT 0.0",
        // "BORE_DIAMETER_MM",
        pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].
          data.data_float;
        DBG_LOG_DEBUG("BORE_DIAMETER_MM %f",
                      pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM);
        break;
      }
      case 70:
      {
        // "REAL DEFAULT 0.0",
        // "RUN_SPEED_RPM",
        pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].
          data.data_float;
        DBG_LOG_DEBUG("RUN_SPEED_RPM %f",
                      pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM);
        break;
      }
      case 71:
      {
        // "REAL DEFAULT 0.0",
        // "BRG_INFO_BPFO",
        pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFO =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 72:
      {
        // "REAL DEFAULT 0.0",
        // "BRG_INFO_BPFI",
        pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFI =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 73:
      {
        // "REAL DEFAULT 0.0",
        // "BRG_INFO_BSF",
        pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BSF =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 74:
      {
        // "REAL DEFAULT 0.0",
        // "BRG_INFO_FTF",
        pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_FTF =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 75:
      {
        // "INT DEFAULT 0",
        // "MTR_INFO_FL",
        pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_FL =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        DBG_LOG_DEBUG("MTR_INFO_FL %u",
                      pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_FL);
        break;
      }
      case 76:
      {
        // "INT DEFAULT 0",
        // "MTR_INFO_BAR",
        pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_BAR =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        DBG_LOG_DEBUG("MTR_INFO_BAR %u",
                      pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_BAR);
        break;
      }
      case 77:
      {
        // "INT DEFAULT 0",
        // "GEAR_INFO_TOOTH",
        pt_sensor_conf->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        DBG_LOG_DEBUG("GEAR_INFO_TOOTH %u",
                      pt_sensor_conf->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH);
        break;
      }
      case 78:
      {
        // "INT DEFAULT 0",
        // "FAN_INFO_BLADE",
        pt_sensor_conf->bulletSensorConfig.algoConfig.FAN_INFO_BLADE =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        DBG_LOG_DEBUG("FAN_INFO_BLADE %u",
                      pt_sensor_conf->bulletSensorConfig.algoConfig.FAN_INFO_BLADE);
        break;
      }
      case 79:
      {
        // "INT DEFAULT 0",
        // "PUMP_INFO_VANE",
        pt_sensor_conf->bulletSensorConfig.algoConfig.PUMP_INFO_VANE =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        DBG_LOG_DEBUG("PUMP_INFO_VANE %u",
                      pt_sensor_conf->bulletSensorConfig.algoConfig.PUMP_INFO_VANE);
        break;
      }
      case 80:
      {
        // "REAL DEFAULT 0.0",
        // "TEMP_OV_ALERT_CDEGREE",
        pt_sensor_conf->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE
          = pt_query_resp->
            inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
            data_float;
        DBG_LOG_DEBUG("TEMP_OV_ALERT_CDEGREE %f",
                      pt_sensor_conf->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE);
        break;
      }
      case 81:
      {
        // "INT DEFAULT 0",
        // "AlarmThrestemp",
        pt_sensor_conf->bulletSensorConfig.algoConfig.AlarmThrestemp =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_int32;
        break;
      }
      case 82:
      {
        // "REAL DEFAULT 0.0",
        // "ACC_OV_ALERT",
        pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALERT =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 83:
      {
        // "REAL DEFAULT 0.0",
        // "ACC_OV_ALARM",
        pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALARM =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 84:
      {
        // "REAL DEFAULT 0.0",
        // "ACC_HAL_ALERT",
        pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALERT =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 85:
      {
        // "REAL DEFAULT 0.0",
        // "ACC_HAL_ALARM",
        pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALARM =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 86:
      {
        // "REAL DEFAULT 0.0",
        // "VEL_OV_ALERT",
        pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALERT =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 87:
      {
        // "REAL DEFAULT 0.0",
        // "VEL_OV_ALARM",
        pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALARM =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 88:
      {
        // "REAL DEFAULT 0.0",
        // "VEL_HAL_ALERT",
        pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALERT =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 89:
      {
        // "REAL DEFAULT 0.0",
        // "VEL_HAL_ALARM",
        pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALARM =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 90:
      {
        // "REAL DEFAULT 0.0",
        // "ENV_OV_ALERT",
        pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALERT =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 91:
      {
        // "REAL DEFAULT 0.0",
        // "ENV_OV_ALARM",
        pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALARM =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 92:
      {
        // "REAL DEFAULT 0.0",
        // "ENV_HAL_ALERT",
        pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALERT =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 93:
      {
        // "REAL DEFAULT 0.0",
        // "ENV_HAL_ALARM",
        pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALARM =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_float;
        break;
      }
      case 94:
      {
        // "INT DEFAULT 0",
        // "waveDataAcqPeriod",
        pt_sensor_conf->bulletSensorConfig.schConfig.period_wave_s =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 95:
      {
        // "INT DEFAULT 0",
        // "waveDataAcqReference",
        pt_sensor_conf->bulletSensorConfig.schConfig.refTime_wave_s =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 96:
      {
        // "INT DEFAULT 0",
        // "FUOTA_Version",
        if(fw_info == NULL)
        {
          g_sys_sensor_fuota_info.ver =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        }else{
          fw_info->ver =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
          DBG_LOG_DEBUG("fw_info->currentVer %u", fw_info->ver);
        }
        break;
      }
      case 97:
      {
        // "TEXT DEFAULT ' '",
        // "FUOTA_Filename",
        if(fw_info == NULL)
        {
          snprintf(g_sys_sensor_fuota_info.fpath,
                 MAX_FPATH,
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        }else{
          snprintf(fw_info->fpath,
                 MAX_FPATH,
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        }
        break;
      }
#if (ENABLE_MODBUS_FEATURE == 1)
      // case 98: // skip it at the moment
      // {
      //   // "TEXT DEFAULT ' '",
      //   // "description",
      //   snprintf(pt_sensor_conf->description,
      //            MAX_FPATH,
      //            "%s",
      //            pt_query_resp->
      //            inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
      //            data_char);
      //   break;
      // }
#endif
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
      case 98:
      {
        // "INT DEFAULT 0",
        // "sensorModeParameter",
        pt_sensor_conf->bulletSensorConfig.sysConfig.sensorModeParameter =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
      default:
      {
        DBG_LOG_WARN("Unexpected colum index %d",
                     pt_query_resp->
                     inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
                     .field);
        break;
      }
    }
  }
}
/**
 * @brief Read bullet sensor configuration from table in a block manner.
 *        (i.e., this function would be blocked to wait for the response or
 *        a timeout happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_msg_format_ctr INPUT: Just a buffer (to store the IPC message
 *                          to the process sql_op)
 * @param pt_sensor_conf The returned sensor configuration
 * @param fw_info INPUT: The pointer to the firmware info. The global
 *                       variable @p g_sys_sensor_fuota_info would be used
 *                       if this parameter is NULL.
 * @param itemidx INPUT: The nameId of the sensor
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_read_sensor_conf_from_db(
  struct dbop_controller *pt_dbop_ctr,
  struct msg_format_controller *pt_msg_format_ctr,
  sensorConfig_t *pt_sensor_conf,
  struct fouta_fw_info *fw_info,
  uint32_t itemidx)
{
  app_state_t tpret = ST_OK;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_sensor_conf) ||
     (NULL == pt_msg_format_ctr))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // To fill the message to be sent to the process sql_op
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_query_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_query_item_param.table =
    GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_query_item_param.itemIdx = itemidx;

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);

  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);

  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.
                                    command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.
                                    command_or_response.commandMsg.
                                    commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX)
        {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n",
                      MSGSEND_ERR_CNT_MAX);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = ST_ERR;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = ST_ERR;
    goto EXIT;
  }

  DBG_LOG_INFO(
    "Sensor-conf reading response information: cmd = %d, cmdId = %d, success(0)/fail(1) = %d, dataNum = %d",
    pt_dbop_resp_waiting_obj->resp_info.command,
    pt_dbop_resp_waiting_obj->resp_info.commandId,
    pt_dbop_resp_waiting_obj->resp_info.response,
    pt_dbop_resp_waiting_obj->resp_info.responseInfo.
    sqlite_update_item_response.dataNum);
  if(pt_dbop_resp_waiting_obj->resp_info.responseInfo.
     sqlite_update_item_response.dataNum == 0)
  {
    DBG_LOG_ERR("Failed to find the item!");
    tpret = ST_ERR;
    goto EXIT;
  }

  // If everything is OK, then fill @p pt_sensor_conf based on the response
  to_fill_sensor_conf_by_data_from_db(
    &pt_dbop_resp_waiting_obj->resp_info.responseInfo.
    sqlite_update_item_response,
    pt_sensor_conf,
    fw_info);

EXIT:
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
/**
 * @brief Fetch bullet sensor configuration from table in a block manner.
 *        (i.e., this function would be blocked to wait for the response or
 *        a timeout happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_sensor_conf The returned sensor configuration
 * @param fw_info INPUT: The pointer to the firmware info. The global
 *                       variable @p g_sys_sensor_fuota_info would be used
 *                       if this parameter is NULL.
 * @param itemidx INPUT: The nameId of the sensor
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_fetch_sensor_conf_from_db(
  struct dbop_controller *pt_dbop_ctr,
  sensorConfig_t *pt_sensor_conf,
  struct fouta_fw_info *fw_info,
  uint32_t itemidx)
{
  app_state_t tpret = ST_OK;
  struct msg_format_controller *pt_msg_format_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_sensor_conf))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  // Read sensor configuration from database
  if(ST_OK !=
     app_db_read_sensor_conf_from_db(pt_dbop_ctr, pt_msg_format_ctr,
                                     pt_sensor_conf, fw_info, itemidx))
  {
    DBG_LOG_ERR("Failed to read from Sensor table");
    tpret = ST_ERR;
    goto EXIT;
  }

EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  return tpret;
}
/**
 * @brief Fill the gwConfig ( pointed by @p pt_gwconf ) based on
 *        the database ( pointed by @p pt_query_resp ).
 */
static app_state_t
to_fill_gw_conf_by_data_from_db(
  const gw_pro_command_sqlite_query_item_response_t *pt_query_resp,
  gwConfig_t *pt_gwconf)
{
  app_state_t tpret = ST_OK;

  if((NULL == pt_query_resp) || (NULL == pt_gwconf))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return ST_ERR;
  }
  for(uint32_t i = 0; i < pt_query_resp->dataNum; i++)
  {
    switch(pt_query_resp->
           inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field)
    {
      case 1:
      {
        // "INT PRIMARY KEY DEFAULT 0",
        // "nameId",
        pt_gwconf->gwConfig.nameId =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 2:
      {
        // "TEXT DEFAULT ' '",
        // "name",
        snprintf(pt_gwconf->gwConfig.name,
                 sizeof(pt_gwconf->gwConfig.name),
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
      case 3:
      {
        // "TEXT DEFAULT ' '",
        // "macAddr",
        uint8_t tp_strbuf[GW_PRO_MAX_STRING_LEN_BYTE] = { 0 };
        for(uint32_t j = 0;
            j < strlen(pt_query_resp->
                       inquiredData[i %
                                    GW_PRO_MAX_DATA_FIELD_PER_OPERATION].
                       data.data_char);
            j++)
        {
          // Remove '-' in the MAC string
          if('-' != pt_query_resp
             ->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].
             data.data_char[j])
          {
            tp_strbuf[j % GW_PRO_MAX_STRING_LEN_BYTE] =
              pt_query_resp->
              inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
              data_char[j];
          }
        }
        DBG_LOG_DEBUG("The MAC-string original: %s -> %s",
                      pt_query_resp->
                      inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].
                      data.data_char,
                      tp_strbuf);
        idstr_to_bytes_array(pt_gwconf->gwConfig.macAddr.addr.addrArray,
                             sizeof(pt_gwconf->gwConfig.macAddr.addr.
                                    addrArray),
                             tp_strbuf);
        break;
      }
      case 4:
      {
        // "TEXT DEFAULT 'SKF'",
        // "manufacturer",
        snprintf(pt_gwconf->gwConfig.manufacturer,
                 sizeof(pt_gwconf->gwConfig.manufacturer),
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
      case 5:
      {
        // "INT DEFAULT 0",
        // "ApplicationVersion",
        // TODO
        break;
      }
      case 6:
      {
        // "INT DEFAULT 0",
        // "LinuxKernelVersion",
        // TODO
        break;
      }
      case 7:
      {
        // "INT DEFAULT 0",
        // "version",
        pt_gwconf->version =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 8:
      {
        // "INT DEFAULT 0",
        // "lastEditTime",
        pt_gwconf->lastEditTimeS =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint64;
        break;
      }
      case 9:
      {
        // "TEXT DEFAULT ' '",
        // "type",
        snprintf(pt_gwconf->gwConfig.type,
                 sizeof(pt_gwconf->gwConfig.type),
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
      case 10:
      {
        // "INT DEFAULT 0",
        // "gatewayMode",
        pt_gwconf->gwConfig.gatewayMode =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 11:
      {
        // "INT DEFAULT 0",
        // "BLE1TxPower",
        pt_gwconf->gwConfig.bleConfig.ble1TxPower =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_int8;
        break;
      }
      case 12:
      {
        // "INT DEFAULT 0",
        // "BLE2TxPower",
        pt_gwconf->gwConfig.bleConfig.ble2TxPower =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_int8;
        break;
      }
      case 13:
      {
        // "INT DEFAULT 0",
        // "BLE1Antenna",
        pt_gwconf->gwConfig.bleConfig.ble1Antenna =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 14:
      {
        // "INT DEFAULT 0",
        // "BLE2Antenna",
        pt_gwconf->gwConfig.bleConfig.ble2Antenna =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 15:
      {
        // "INT DEFAULT 0",
        // "BLEFilterListNameNumber",
        pt_gwconf->gwConfig.childrenNumber =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 16:
      {
        // "INT DEFAULT 0",
        // "BLEDuplicatedDrop",
        pt_gwconf->gwConfig.bleConfig.bleDuplicatedDrop_ms =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 17:
      {
        // "BOOLEAN DEFAULT false",
        // "timeStamp",
        pt_gwconf->gwConfig.timeConfig.needTimestampForRecBeacon =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 18:
      {
        // "INT DEFAULT 0",
        // "timeSetting",
        pt_gwconf->gwConfig.timeConfig.timeSetting =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 19:
      {
        // "TEXT DEFAULT ' '",
        // "timeNTPUrl",
        snprintf(pt_gwconf->gwConfig.timeConfig.timeNtpUrl,
                 sizeof(pt_gwconf->gwConfig.timeConfig.timeNtpUrl),
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
      case 20:
      {
        // "INT DEFAULT 0",
        // "MQTTSecurityType",
        pt_gwconf->gwConfig.mqttConfig.mqttSecurityType =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 21:
      {
        // "TEXT DEFAULT ' '",
        // "MQTTHost",
        snprintf(pt_gwconf->gwConfig.mqttConfig.MQTTHost,
                 sizeof(pt_gwconf->gwConfig.mqttConfig.MQTTHost), "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
      case 22:
      {
        // "INT DEFAULT 0",
        // "MQTTPort",
        pt_gwconf->gwConfig.mqttConfig.MQTTPort =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 23:
      {
        // "BOOLEAN DEFAULT false",
        // "DHCPEnable",
        pt_gwconf->gwConfig.ipConfig.DHCPEnable =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 24:
      {
        // "TEXT DEFAULT ' '",
        // "staticIPAddr",
        DBG_LOG_DEBUG("staticIPAddr str %s",
                      pt_query_resp->
                      inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                      data_char);
        ipstr_to_bytes_array(
          pt_gwconf->gwConfig.ipConfig.staticIPAddr.addr.addrArray,
          sizeof(pt_gwconf->gwConfig.ipConfig.
                 staticIPAddr.addr.addrArray),
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_char);
        break;
      }
      case 25:
      {
        // "TEXT DEFAULT ' '",
        // "netMask",
        DBG_LOG_DEBUG("netMask %s",
                      pt_query_resp->
                      inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                      data_char);
        ipstr_to_bytes_array(
          pt_gwconf->gwConfig.ipConfig.netMask.addr.addrArray,
          sizeof(pt_gwconf->gwConfig.ipConfig.netMask.
                 addr.addrArray),
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_char);
        break;
      }
      case 26:
      {
        // "TEXT DEFAULT ' '",
        // "DNSServer",
        DBG_LOG_DEBUG("DNSServer %s",
                      pt_query_resp->
                      inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                      data_char);
        ipstr_to_bytes_array(
          pt_gwconf->gwConfig.ipConfig.dnsServer.addr.addrArray,
          sizeof(pt_gwconf->gwConfig.ipConfig.dnsServer.
                 addr.addrArray),
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_char);
        break;
      }
      case 27:
      {
        // "TEXT DEFAULT ' '",
        // "gatewayAddr",
        DBG_LOG_DEBUG("gatewayAddr %s",
                      pt_query_resp->
                      inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                      data_char);
        ipstr_to_bytes_array(
          pt_gwconf->gwConfig.ipConfig.gatewayAddr.addr.addrArray,
          sizeof(pt_gwconf->gwConfig.ipConfig.gatewayAddr.
                 addr.addrArray),
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_char);
        break;
      }
#if (ENABLE_MODBUS_FEATURE == 1)
      case 28:
      {
        // "INT DEFAULT 0",
        // "baudrateSlave",
        pt_gwconf->gwConfig.modbusConfig.baudrateSlave =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 29:
      {
        // "INT DEFAULT 0",
        // "slaveAddrSlave",
        pt_gwconf->gwConfig.modbusConfig.slaveAddrSlave =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 30:
      {
        // "INT DEFAULT 0",
        // "paritySlave",
        pt_gwconf->gwConfig.modbusConfig.paritySlave =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 31:
      {
        // "INT DEFAULT 0",
        // "stopSlave",
        pt_gwconf->gwConfig.modbusConfig.stopSlave =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 32:
      {
        // "INT DEFAULT 0",
        // "registerMode",
        pt_gwconf->gwConfig.modbusConfig.registerMode =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint8;
        break;
      }
      case 33:
      {
        // "TEXT DEFAULT ' '",
        // "description",
        DBG_LOG_DEBUG("description %s",
                      pt_query_resp->
                      inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                      data_char);
        snprintf(pt_gwconf->gwConfig.description,
                 sizeof(pt_gwconf->gwConfig.description),
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
#endif
      default:
      {
        DBG_LOG_WARN("Unknown field value");
        break;
      }
    }
  }
  return tpret;
}
/**
 * @brief Read bullet gateway configuration from table in a block manner.
 *        (i.e., this function would be blocked to wait for the response or
 *        a timeout happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_msg_format_ctr INPUT: Just a buffer (to store the IPC message
 *                          to the process sql_op)
 * @param pt_gwconf The returned gateway configuration
 * @param itemidx INPUT: The nameId of the sensor
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_read_gateway_conf_from_db(
  struct dbop_controller *pt_dbop_ctr,
  struct msg_format_controller *pt_msg_format_ctr,
  gwConfig_t *pt_gwconf,
  uint32_t itemidx)
{
  app_state_t tpret = ST_OK;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_gwconf) ||
     (NULL == pt_msg_format_ctr))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // To fill the message to be sent to the process sql_op
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_query_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_query_item_param.table =
    GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_query_item_param.itemIdx = itemidx;

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);

  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);

  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX) {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n", MSGSEND_ERR_CNT_MAX);
          fflush(stdout);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = ST_ERR;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = ST_ERR;
    goto EXIT;
  }

  DBG_LOG_INFO(
    "gwConf read response info: cmd = %d, cmdId = %d, success(0)/fail(1) = %d, dataNum = %d", \
    pt_dbop_resp_waiting_obj->resp_info.command, pt_dbop_resp_waiting_obj->resp_info.commandId, \
    pt_dbop_resp_waiting_obj->resp_info.response, \
    pt_dbop_resp_waiting_obj->resp_info.responseInfo.sqlite_update_item_response.dataNum);
  if(pt_dbop_resp_waiting_obj->resp_info.responseInfo.
     sqlite_update_item_response.dataNum == 0)
  {
    DBG_LOG_ERR("Failed to find the item!");
    tpret = ST_ERR;
    goto EXIT;
  }

  // If everything is OK, then fill @p pt_gwconf based on the response
  to_fill_gw_conf_by_data_from_db(
    &pt_dbop_resp_waiting_obj->resp_info.responseInfo.sqlite_update_item_response,
    pt_gwconf);

EXIT:
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
/**
 * @brief Fill the children list ( pointed by @p pt_dev_info ) based on
 *        the database ( pointed by @p pt_query_resp ).
 */
static app_state_t
to_fill_dev_info_by_data_from_db(
  const gw_pro_command_sqlite_query_item_response_t *pt_query_resp,
  struct device_info *pt_dev_info)
{
  app_state_t tpret = ST_OK;

  if((NULL == pt_query_resp) || (NULL == pt_dev_info))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return ST_ERR;
  }

  for(uint32_t i = 0; i < pt_query_resp->dataNum; i++)
  {
    switch(pt_query_resp->
           inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field)
    {
      case 1:
      {
        // "INTEGER PRIMARY KEY AUTOINCREMENT",
        // "nameId",
        pt_dev_info->nameid =
          pt_query_resp->
          inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
          data_uint32;
        break;
      }
      case 2:
      {
        // "TEXT DEFAULT ' '",
        // "name",
        snprintf(pt_dev_info->name,
                 GW_PRO_MAX_STRING_LEN_BYTE,
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
      case 3:
      {
        // "TEXT DEFAULT ' ' ",
        // "BLESensorName",
        snprintf(pt_dev_info->sensor_name,
                 GW_PRO_MAX_STRING_LEN_BYTE,
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
      case 4:
      {
        // "TEXT DEFAULT ' '",
        // "macAddr"
        snprintf(pt_dev_info->mac_addr,
                 GW_PRO_MAX_STRING_LEN_BYTE,
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
      case 5:
      {
        // "TEXT DEFAULT 'SKF'",
        // "manufacturer",
        snprintf(pt_dev_info->manufacturer,
                 GW_PRO_MAX_STRING_LEN_BYTE,
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
      case 6:
      {
        // "TEXT DEFAULT 'BulletSensor'",
        // "type",
        snprintf(pt_dev_info->sensor_type,
                 GW_PRO_MAX_STRING_LEN_BYTE,
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
#if (ENABLE_MODBUS_FEATURE == 1)
      case 7:
      {
        // "TEXT DEFAULT ''",
        // "description",
        snprintf(pt_dev_info->description,
                 GW_PRO_MAX_STRING_LEN_BYTE,
                 "%s",
                 pt_query_resp->
                 inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
                 data_char);
        break;
      }
#endif
      default:
      {
        DBG_LOG_WARN("Unknown table field");
        break;
      }
    }
  }
  return tpret;
}
/**
 * @brief Read device list (i.e., the children list) from table in a block
 *        manner. (i.e., this function would be blocked to wait for the
 *        response or a timeout happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_msg_format_ctr INPUT: Just a buffer (to store the IPC message
 *                          to the process sql_op)
 * @param pt_dev_info The returned children list
 * @param itemidx INPUT: The nameId of the sensor
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_read_dev_info_from_db(
  struct dbop_controller *pt_dbop_ctr,
  struct msg_format_controller *pt_msg_format_ctr,
  struct device_info *pt_dev_info,
  uint32_t itemidx)
{
  app_state_t tpret = ST_OK;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_dev_info) ||
     (NULL == pt_msg_format_ctr))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // To fill the message to be sent to the process sql_op
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_query_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_query_item_param.table =
    GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_query_item_param.itemIdx = itemidx;

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);

  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);

  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX) {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n", MSGSEND_ERR_CNT_MAX);
          fflush(stdout);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = ST_ERR;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = ST_ERR;
    goto EXIT;
  }

  DBG_LOG_INFO(
    "Dev read response info: cmd = %d, cmdId = %d, success(0)/fail(1) = %d, dataNum = %d\n", \
    pt_dbop_resp_waiting_obj->resp_info.command, pt_dbop_resp_waiting_obj->resp_info.commandId, \
    pt_dbop_resp_waiting_obj->resp_info.response, \
    pt_dbop_resp_waiting_obj->resp_info.responseInfo.sqlite_update_item_response.dataNum);
  if(pt_dbop_resp_waiting_obj->resp_info.responseInfo.
     sqlite_update_item_response.dataNum == 0)
  {
    DBG_LOG_ERR("Failed to find the item!");
    tpret = ST_ERR;
    goto EXIT;
  }

  // To fill @p pt_dev_info based on the response
  to_fill_dev_info_by_data_from_db(
    &pt_dbop_resp_waiting_obj->resp_info.responseInfo.sqlite_update_item_response,
    pt_dev_info);

EXIT:
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
// Deprecated
#if 0
/**
 * @brief Fetch children MAC address from table in a block manner.
 *        (i.e., this function would be blocked to wait for the response or
 *        a timeout happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_resbuf The returned children MAC address array
 * @param kp_bufsz The size of @p pt_resbuf
 * @return the number of children MAC address has been fetched, and -1
 *         would be returned when things go wrong.
 */
int32_t
app_db_fetch_children_mac_array_from_db(
  struct dbop_controller *pt_dbop_ctr,
  struct bt_addr *pt_resbuf,
  const uint32_t kp_bufsz)
{
  int32_t tpret = 0;
  gw_pro_command_sqlite_filter_param_t *pt_filter = NULL;
  uint32_t kp_idbuf[MAX_BT_WHITE_LIST] = { 0 };
  int32_t kp_i32 = 0;
  struct device_info dev_info = { 0 };
  struct msg_format_controller *pt_msg_format_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_resbuf) || (0 == kp_bufsz))
  {
    DBG_LOG_ERR("Invalid input parameters");
    tpret = -1;
    goto EXIT;
  }

  // Initialize memory for database filter
  pt_filter =
    (gw_pro_command_sqlite_filter_param_t *)l_malloc(
      sizeof(gw_pro_command_sqlite_filter_param_t));
  if(NULL == pt_filter)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = -1;
    goto EXIT;
  }
  memset(pt_filter, 0, sizeof(gw_pro_command_sqlite_filter_param_t));
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = -1;
    goto EXIT;
  }
  // To prepare the filter to select all from Dev table (in principle, all
  // the children should be selected)
  pt_filter->table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  pt_filter->dataNum = 1;
  pt_filter->filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 1;
  pt_filter->
  filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_all;
  // To get the number of the item in Dev table
  kp_i32 =
    app_db_fetch_filtered_index(pt_dbop_ctr,
                                pt_filter,
                                sizeof(kp_idbuf),
                                kp_idbuf);
  if(kp_i32 < 0)
  {
    DBG_LOG_ERR("Unexpected record number in table Dev: %d", kp_i32);
    // Just skip the following reading device from Dev table because the
    // invalid number
    tpret = -1;
    goto EXIT;
  }
  if(kp_i32 > MAX_BT_WHITE_LIST)
  {
    DBG_LOG_WARN(
      "Too many (%d) children MAC has been filtered (MAX system support %d items)",
      kp_i32,
      MAX_BT_WHITE_LIST);
    kp_i32 = MAX_BT_WHITE_LIST;
  }
  if((kp_i32 * sizeof(struct bt_addr) > kp_bufsz))
  {
    DBG_LOG_WARN(
      "There is no sufficient memory space to return all MAC addresses");
    kp_i32 = kp_bufsz / sizeof(struct bt_addr);
  }

  tpret = 0;
  for(uint32_t i = 0; i < kp_i32; i++)
  {
    // Read a device information to @p dev_info
    memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));
    memset(&dev_info, 0, sizeof(dev_info));
    if(ST_OK !=
       app_db_read_dev_info_from_db(pt_dbop_ctr,
                                    pt_msg_format_ctr,
                                    &dev_info,
                                    kp_idbuf[i % MAX_BT_WHITE_LIST]))
    {
      DBG_LOG_WARN(
        "Failed to read device info from Dev table (namdId %d).",
        kp_idbuf[i % MAX_BT_WHITE_LIST]);
      tpret = -1;
      goto EXIT;
    }

    macAddr_t tp_mac_addr = { 0 };
    uint8_t tp_strbuf[GW_PRO_MAX_STRING_LEN_BYTE] = { 0 };
    uint32_t tp_strlen = strlen(dev_info.mac_addr);
    uint32_t tp_idx = 0, tpval = 0;

    for(uint32_t j = 0; j < tp_strlen; j++)
    {
      // Remove '-' in the MAC string
      if('-' != dev_info.mac_addr[j])
      {
        tp_strbuf[tp_idx %
                  GW_PRO_MAX_STRING_LEN_BYTE] = dev_info.mac_addr[j];
        tp_idx++;
      }
    }
    idstr_to_bytes_array(tp_mac_addr.addr.addrArray,
                         sizeof(tp_mac_addr.addr.addrArray), tp_strbuf);

    pt_resbuf[i].nap = ((uint16_t)tp_mac_addr.addr.addrArray[0]) * 0x100 +
                       tp_mac_addr.addr.addrArray[1];
    pt_resbuf[i].uap = tp_mac_addr.addr.addrArray[2];
    pt_resbuf[i].lap = ((uint32_t)tp_mac_addr.addr.addrArray[3]) * 0x100 *
                       0x100 +
                       ((uint32_t)tp_mac_addr.addr.addrArray[4]) * 0x100 +
                       tp_mac_addr.addr.addrArray[5];
    tpret++;

    DBG_LOG_INFO("node idx = %d, MAC-info = %.4X : %.2X : %.6X", tpret,
                 pt_resbuf[i].nap, pt_resbuf[i].uap, pt_resbuf[i].lap);
  }

EXIT:
  if(pt_filter)
  {
    l_free(pt_filter);
    pt_filter = NULL;
  }
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  return tpret;
}
#endif
/**
 * @brief Fetch children MAC address from table in a block manner.
 *        (i.e., this function would be blocked to wait for the response or
 *        a timeout happens). The MAC list of predict sensor is returned by 
 *        @p pt_resbuf_p ; and the MAC list of Insight-T is returned by 
 *        @p pt_resbuf_t . Please make sure that the sizes of @p pt_resbuf_t 
 *        and @p pt_resbuf_p (i.e., @p kp_bufsz ) are identical.
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_resbuf_p The returned children MAC address array of predict sensor
 * @param pt_resbuf_t The returned children MAC address array of Insight-T
 * @param pt_name_id_p The returned children name ID array of predict sensor
 * @param pt_name_id_t The returned children name ID array of Insight-T
 * @param kp_bufsz The size of @p pt_resbuf_p .
 * @return the number of children MAC address has been fetched, and -1
 *         would be returned when things go wrong.
 */
int32_t
app_db_fetch_children_mac_array_from_db_v2(
  struct dbop_controller *pt_dbop_ctr,
  struct bt_addr *pt_resbuf_p,
  struct bt_addr *pt_resbuf_t,
  uint32_t *pt_name_id_p,
  uint32_t *pt_name_id_t,
  const uint32_t kp_bufsz)
{
  int32_t tpret = 0;
  gw_pro_command_sqlite_filter_param_t *pt_filter = NULL;
  uint32_t kp_idbuf[MAX_BT_WHITE_LIST] = { 0 };
  int32_t kp_i32 = 0;
  struct device_info dev_info = { 0 };
  struct msg_format_controller *pt_msg_format_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_resbuf_t) || (0 == kp_bufsz) ||
     (NULL == pt_resbuf_p))
  {
    DBG_LOG_ERR("Invalid input parameters");
    tpret = -1;
    goto EXIT;
  }

  // Initialize memory for database filter
  pt_filter =
    (gw_pro_command_sqlite_filter_param_t *)l_malloc(
      sizeof(gw_pro_command_sqlite_filter_param_t));
  if(NULL == pt_filter)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = -1;
    goto EXIT;
  }
  memset(pt_filter, 0, sizeof(gw_pro_command_sqlite_filter_param_t));
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = -1;
    goto EXIT;
  }
  // To prepare the filter to select all from Dev table (in principle, all
  // the children should be selected)
  pt_filter->table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  pt_filter->dataNum = 1;
  pt_filter->filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 1;
  pt_filter->
  filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_all;
  // To get the number of the item in Dev table
  kp_i32 =
    app_db_fetch_filtered_index(pt_dbop_ctr,
                                pt_filter,
                                sizeof(kp_idbuf),
                                kp_idbuf);
  if(kp_i32 < 0)
  {
    DBG_LOG_ERR("Unexpected record number in table Dev: %d", kp_i32);
    // Just skip the following reading device from Dev table because the
    // invalid number
    tpret = -1;
    goto EXIT;
  }
  if(kp_i32 > MAX_BT_WHITE_LIST)
  {
    DBG_LOG_WARN(
      "Too many (%d) children MAC has been filtered (MAX system support %d items)",
      kp_i32,
      MAX_BT_WHITE_LIST);
    kp_i32 = MAX_BT_WHITE_LIST;
  }
  if((kp_i32 * sizeof(struct bt_addr) > kp_bufsz))
  {
    DBG_LOG_WARN(
      "There is no sufficient memory space to return all MAC addresses");
    kp_i32 = kp_bufsz / sizeof(struct bt_addr);
  }

#if (ENABLE_MODBUS_FEATURE == 1)
  if(!ble_dev_fetch_flag) {
    ble_dev_cnt_former = kp_i32;
    memset(&ble_dev_nameid_list_former, 0, sizeof(ble_dev_nameid_list_former));
  }
#endif
  tpret = 0;
  for(uint32_t i = 0; i < kp_i32; i++)
  {
    // Read a device information to @p dev_info
    memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));
    memset(&dev_info, 0, sizeof(dev_info));
    if(ST_OK !=
       app_db_read_dev_info_from_db(pt_dbop_ctr,
                                    pt_msg_format_ctr,
                                    &dev_info,
                                    kp_idbuf[i % MAX_BT_WHITE_LIST]))
    {
      DBG_LOG_WARN(
        "Failed to read device info from Dev table (namdId %d).",
        kp_idbuf[i % MAX_BT_WHITE_LIST]);
      tpret = -1;
#if (ENABLE_MODBUS_FEATURE == 1)
      if(!ble_dev_fetch_flag) {
        ble_dev_cnt_former = i;
        memset(ble_dev_nameid_list_current, 0, sizeof(ble_dev_nameid_list_current));
        ble_dev_cnt_current = 0;
      }
#endif
      goto EXIT;
    }

#if (ENABLE_MODBUS_FEATURE == 1)
    if(!ble_dev_fetch_flag) {
      ble_dev_nameid_list_former[i] = dev_info.nameid;
    }
#endif
    macAddr_t tp_mac_addr = { 0 };
    uint8_t tp_strbuf[GW_PRO_MAX_STRING_LEN_BYTE] = { 0 };
    uint32_t tp_strlen = strlen(dev_info.mac_addr);
    uint32_t tp_idx = 0, tpval = 0;

    for(uint32_t j = 0; j < tp_strlen; j++)
    {
      // Remove '-' in the MAC string
      if('-' != dev_info.mac_addr[j])
      {
        tp_strbuf[tp_idx %
                  GW_PRO_MAX_STRING_LEN_BYTE] = dev_info.mac_addr[j];
        tp_idx++;
      }
    }
    idstr_to_bytes_array(tp_mac_addr.addr.addrArray,
                         sizeof(tp_mac_addr.addr.addrArray), tp_strbuf);
    if((strlen(TYPE_STR_INSIGHT_P) == strlen(dev_info.sensor_type)) &&
       (0 == memcmp(TYPE_STR_INSIGHT_P,
                    dev_info.sensor_type,
                    strlen(TYPE_STR_INSIGHT_P))))
    {
      // PredictSensor
      pt_resbuf_p->nap = ((uint16_t)tp_mac_addr.addr.addrArray[0]) *
                         0x100 + tp_mac_addr.addr.addrArray[1];
      pt_resbuf_p->uap = tp_mac_addr.addr.addrArray[2];
      pt_resbuf_p->lap = ((uint32_t)tp_mac_addr.addr.addrArray[3]) *
                         0x100 * 0x100 +
                         ((uint32_t)tp_mac_addr.addr.addrArray[4]) *
                         0x100 +
                         tp_mac_addr.addr.addrArray[5];
      *pt_name_id_p = dev_info.nameid;

      DBG_LOG_INFO("-p node idx = %d, MAC-info = %.4X : %.2X : %.6X", tpret,
                   pt_resbuf_p->nap, pt_resbuf_p->uap, pt_resbuf_p->lap);
      DBG_LOG_INFO("name id = %u", dev_info.nameid);

      pt_resbuf_p = pt_resbuf_p + 1; // Move the pointer
      pt_name_id_p = pt_name_id_p + 1;

      tpret++;
    }
    else if((strlen(TYPE_STR_INSIGHT_PP) == strlen(dev_info.sensor_type)) &&
       (0 == memcmp(TYPE_STR_INSIGHT_PP,
                    dev_info.sensor_type,
                    strlen(TYPE_STR_INSIGHT_PP))))
    {
      // PredictPro Sensor put into the same list. 
      pt_resbuf_p->nap = ((uint16_t)tp_mac_addr.addr.addrArray[0]) *
                         0x100 + tp_mac_addr.addr.addrArray[1];
      pt_resbuf_p->uap = tp_mac_addr.addr.addrArray[2];
      pt_resbuf_p->lap = ((uint32_t)tp_mac_addr.addr.addrArray[3]) *
                         0x100 * 0x100 +
                         ((uint32_t)tp_mac_addr.addr.addrArray[4]) *
                         0x100 +
                         tp_mac_addr.addr.addrArray[5];
      *pt_name_id_p = dev_info.nameid;

      DBG_LOG_INFO("pro node idx = %d, MAC-info = %.4X : %.2X : %.6X", tpret,
                   pt_resbuf_p->nap, pt_resbuf_p->uap, pt_resbuf_p->lap);
      DBG_LOG_INFO("name id = %u", dev_info.nameid);

      pt_resbuf_p = pt_resbuf_p + 1; // Move the pointer
      pt_name_id_p = pt_name_id_p + 1;

      tpret++;
    }
    else if((strlen(TYPE_STR_INSIGHT_T) == strlen(dev_info.sensor_type)) &&
            (0 == memcmp(TYPE_STR_INSIGHT_T,
                         dev_info.sensor_type,
                         strlen(TYPE_STR_INSIGHT_T))))
    {
      // Insight-T
      pt_resbuf_t->nap = ((uint16_t)tp_mac_addr.addr.addrArray[0]) *
                         0x100 + tp_mac_addr.addr.addrArray[1];
      pt_resbuf_t->uap = tp_mac_addr.addr.addrArray[2];
      pt_resbuf_t->lap = ((uint32_t)tp_mac_addr.addr.addrArray[3]) *
                         0x100 * 0x100 +
                         ((uint32_t)tp_mac_addr.addr.addrArray[4]) *
                         0x100 +
                         tp_mac_addr.addr.addrArray[5];
      *pt_name_id_t = dev_info.nameid;
      DBG_LOG_INFO("node idx = %d, MAC-info = %.4X : %.2X : %.6X", tpret,
                   pt_resbuf_t->nap, pt_resbuf_t->uap, pt_resbuf_t->lap);
      DBG_LOG_INFO("name id = %u", dev_info.nameid);

      pt_resbuf_t = pt_resbuf_t + 1; // Move the pointer
      pt_name_id_t = pt_name_id_t + 1;

      tpret++;
    }
    else
    {
      DBG_LOG_ERR("Sensor type %s is not supported",
                  dev_info.sensor_type);
    }
  }
#if (ENABLE_MODBUS_FEATURE == 1)
  DBG_LOG_INFO("[shm ble init dev] former_cnt:%d, nameId list:\r\n", ble_dev_cnt_former);
  for(uint8_t i=0;i<ble_dev_cnt_former;i++) {
    printf("%d ", ble_dev_nameid_list_former[i]);
  }
  printf("\r\n");
#endif

EXIT:
  if(pt_filter)
  {
    l_free(pt_filter);
    pt_filter = NULL;
  }
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  return tpret;
}  
/**
 * @brief To fetch the device list from database (to replace the function
 * @p app_db_fetch_children_mac_array_from_db_v2 )
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 * queue for timeout or receiving IPC messages)
 * @param pt_devarray The returned information with children MAC address,
 * name id, and product type (to indicate which type of sensor is)
 * @param kp_bufsz The size of @p pt_devarray (i.e., the number of array
 * item)
 * @return 0 when everything goes well.
 */
int32_t
app_db_fetch_devlist(
  struct dbop_controller *pt_dbop_ctr,
  struct devinfo_node *pt_devarray,
  const uint32_t array_sz)
{
  int32_t tpret = 0;
  gw_pro_command_sqlite_filter_param_t *pt_filter = NULL;
  uint32_t kp_idbuf[MAX_BT_WHITE_LIST] = { 0 };
  int32_t kp_i32 = 0;
  struct device_info dev_info = { 0 };
  struct msg_format_controller *pt_msg_format_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (0 == array_sz) || (NULL == pt_devarray))
  {
    DBG_LOG_ERR("Invalid input parameters");
    tpret = -1;
    goto EXIT;
  }

  // Initialize memory for database filter
  pt_filter =
    (gw_pro_command_sqlite_filter_param_t *)l_malloc(
      sizeof(gw_pro_command_sqlite_filter_param_t));
  if(NULL == pt_filter)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = -1;
    goto EXIT;
  }
  memset(pt_filter, 0, sizeof(gw_pro_command_sqlite_filter_param_t));
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = -1;
    goto EXIT;
  }
  // To prepare the filter to select all from Dev table (in principle, all
  // the children should be selected)
  pt_filter->table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  pt_filter->dataNum = 1;
  pt_filter->filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 1;
  pt_filter->
  filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_all;
  // To get the number of the item in Dev table
  kp_i32 =
    app_db_fetch_filtered_index(pt_dbop_ctr,
                                pt_filter,
                                sizeof(kp_idbuf),
                                kp_idbuf);
  if(kp_i32 < 0)
  {
    DBG_LOG_ERR("Unexpected record number in table Dev: %d", kp_i32);
    // Just skip the following reading device from Dev table because the
    // invalid number
    tpret = -1;
    goto EXIT;
  }
  if(kp_i32 > MAX_BT_WHITE_LIST)
  {
    DBG_LOG_WARN(
      "Too many (%d) children MAC has been filtered (MAX system support %d items)",
      kp_i32,
      MAX_BT_WHITE_LIST);
    kp_i32 = MAX_BT_WHITE_LIST;
  }
  if((kp_i32 > array_sz))
  {
    DBG_LOG_WARN(
      "There is no sufficient memory space to return all MAC addresses");
    kp_i32 = array_sz;
  }

#if (ENABLE_MODBUS_FEATURE == 1)
  if(!ble_dev_fetch_flag)
  {
    ble_dev_cnt_former = kp_i32;
    memset(&ble_dev_nameid_list_former, 0,
           sizeof(ble_dev_nameid_list_former));
  }
#endif
  tpret = 0;
  for(uint32_t i = 0; i < kp_i32; i++)
  {
    // Read a device information to @p dev_info
    memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));
    memset(&dev_info, 0, sizeof(dev_info));
    if(ST_OK !=
       app_db_read_dev_info_from_db(pt_dbop_ctr,
                                    pt_msg_format_ctr,
                                    &dev_info,
                                    kp_idbuf[i % MAX_BT_WHITE_LIST]))
    {
      DBG_LOG_ERR(
        "Failed to read device info from Dev table (namdId %d).",
        kp_idbuf[i % MAX_BT_WHITE_LIST]);
      tpret = -1;
#if (ENABLE_MODBUS_FEATURE == 1)
      if(!ble_dev_fetch_flag)
      {
        ble_dev_cnt_former = i;
        memset(ble_dev_nameid_list_current, 0,
               sizeof(ble_dev_nameid_list_current));
        ble_dev_cnt_current = 0;
      }
#endif
      goto EXIT;
    }

#if (ENABLE_MODBUS_FEATURE == 1)
    if(!ble_dev_fetch_flag)
    {
      ble_dev_nameid_list_former[i] = dev_info.nameid;
    }
#endif
    macAddr_t tp_mac_addr = { 0 };
    uint8_t tp_strbuf[GW_PRO_MAX_STRING_LEN_BYTE] = { 0 };
    uint32_t tp_strlen = strlen(dev_info.mac_addr);
    uint32_t tp_idx = 0, tpval = 0;

    for(uint32_t j = 0; j < tp_strlen; j++)
    {
      // Remove '-' in the MAC string
      if('-' != dev_info.mac_addr[j])
      {
        tp_strbuf[tp_idx %
                  GW_PRO_MAX_STRING_LEN_BYTE] = dev_info.mac_addr[j];
        tp_idx++;
      }
    }
    idstr_to_bytes_array(tp_mac_addr.addr.addrArray,
                         sizeof(tp_mac_addr.addr.addrArray), tp_strbuf);

    // to fill the device array
    pt_devarray[i].nameid = dev_info.nameid;
    snprintf(pt_devarray[i].stype, GW_PRO_MAX_STRING_LEN_BYTE, "%s",
             dev_info.sensor_type);
    memcpy(pt_devarray[i].mac_bytes, tp_mac_addr.addr.addrArray,
           sizeof(pt_devarray[i].mac_bytes));

    /*
       if((strlen(TYPE_STR_INSIGHT_P) == strlen(dev_info.sensor_type)) &&
         (0 == memcmp(TYPE_STR_INSIGHT_P,
                      dev_info.sensor_type,
                      strlen(TYPE_STR_INSIGHT_P))))
       {
        // PredictSensor
        pt_resbuf_p->nap = ((uint16_t)tp_mac_addr.addr.addrArray[0]) *
                           0x100 + tp_mac_addr.addr.addrArray[1];
        pt_resbuf_p->uap = tp_mac_addr.addr.addrArray[2];
        pt_resbuf_p->lap = ((uint32_t)tp_mac_addr.addr.addrArray[3]) *
                           0x100 * 0x100 +
                           ((uint32_t)tp_mac_addr.addr.addrArray[4]) *
                           0x100 +
                           tp_mac_addr.addr.addrArray[5];

        DBG_LOG_INFO("node idx = %d, MAC-info = %.4X : %.2X : %.6X", tpret,
                     pt_resbuf_p->nap, pt_resbuf_p->uap, pt_resbuf_p->lap);

        pt_resbuf_p = pt_resbuf_p + 1; // Move the pointer

        tpret++;
       }
       else if((strlen(TYPE_STR_INSIGHT_T) == strlen(dev_info.sensor_type))
          &&
              (0 == memcmp(TYPE_STR_INSIGHT_T,
                           dev_info.sensor_type,
                           strlen(TYPE_STR_INSIGHT_T))))
       {
        // Insight-T
        pt_resbuf_t->nap = ((uint16_t)tp_mac_addr.addr.addrArray[0]) *
                           0x100 + tp_mac_addr.addr.addrArray[1];
        pt_resbuf_t->uap = tp_mac_addr.addr.addrArray[2];
        pt_resbuf_t->lap = ((uint32_t)tp_mac_addr.addr.addrArray[3]) *
                           0x100 * 0x100 +
                           ((uint32_t)tp_mac_addr.addr.addrArray[4]) *
                           0x100 +
                           tp_mac_addr.addr.addrArray[5];
     * pt_name_id_t = dev_info.nameid;
        DBG_LOG_INFO("node idx = %d, MAC-info = %.4X : %.2X : %.6X", tpret,
                     pt_resbuf_t->nap, pt_resbuf_t->uap, pt_resbuf_t->lap);
        DBG_LOG_INFO("name id = %u", dev_info.nameid);

        pt_resbuf_t = pt_resbuf_t + 1; // Move the pointer
        pt_name_id_t = pt_name_id_t + 1;

        tpret++;
       }
       else
       {
        DBG_LOG_ERR("Sensor type %s is not supported",
                    dev_info.sensor_type);
       }
     */
  }
#if (ENABLE_MODBUS_FEATURE == 1)
  DBG_LOG_INFO("[shm ble init dev] former_cnt:%d, nameId list:\r\n",
               ble_dev_cnt_former);
  for(uint8_t i = 0; i < ble_dev_cnt_former; i++)
  {
    printf("%d ", ble_dev_nameid_list_former[i]);
  }
  printf("\r\n");
#endif

EXIT:
  if(pt_filter)
  {
    l_free(pt_filter);
    pt_filter = NULL;
  }
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  return tpret;
}
/**
 * @brief Fetch bullet gateway configuration and children list from table
 *		  (i.e., Gateway and Dev table) in a block manner. (i.e., this
 *		  function would be blocked to wait for the response or a timeout
 *		  happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_gwconf_and_dev The returned gateway configuration
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_fetch_gwconf_and_dev_from_db(
  struct dbop_controller *pt_dbop_ctr,
  gwconf_and_dev_t *pt_gwconf_and_dev)
{
  app_state_t tpret = ST_OK;
  gwConfig_t *pt_gwcfg = NULL;
  gw_pro_command_sqlite_filter_param_t *pt_filter = NULL;
  uint32_t kp_idbuf[MAX_BT_WHITE_LIST] = { 0 };
  int32_t kp_i32 = 0;
  struct device_info *pt_dev_info = NULL;

  struct msg_format_controller *pt_msg_format_ctr = NULL;
  gwConfig_gwConfig_children_t *pt_cfg_child = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_gwconf_and_dev))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // 1. Initialization
  // 1.1 Initialize memory for gwConfig
  pt_gwcfg = (gwConfig_t *)l_malloc(sizeof(gwConfig_t));
  if(NULL == pt_gwcfg)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_gwcfg, 0, sizeof(gwConfig_t));
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  INIT_LIST_HEAD(&pt_gwcfg->gwConfig.childrenList);
#endif /* GWCONFIG_CHILDREN_USING_LINKED_LIST == 1 */
  memset(kp_idbuf, 0, sizeof(kp_idbuf));
  // 1.2 Initialize memory for database filter
  pt_filter =
    (gw_pro_command_sqlite_filter_param_t *)l_malloc(
      sizeof(gw_pro_command_sqlite_filter_param_t));
  if(NULL == pt_filter)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_filter, 0, sizeof(gw_pro_command_sqlite_filter_param_t));
  // 1.3 Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));
  // 1.4 Initialize a temporary memory for device information
  pt_dev_info = (struct device_info *)l_malloc(sizeof(struct device_info));
  if(NULL == pt_dev_info)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dev_info, 0, sizeof(struct device_info));

  // 2. Read gateway configuration
  // To prepare the filter to select all from Gateway table, but only one
  // item in Gateway table is expected.
  pt_filter->table = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
  pt_filter->dataNum = 1;
  pt_filter->filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 1;
  pt_filter->
  filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_all;
  // To get the number of the item in Gateway table
  kp_i32 =
    app_db_fetch_filtered_index(pt_dbop_ctr, pt_filter, sizeof(kp_idbuf), 
                                kp_idbuf);
  if(1 != kp_i32)
  {
    DBG_LOG_WARN(
      "Unexpected record number in table Gateway: %d. Then, \
	  the first record would be read as the gateway configuration",
      kp_i32);
  }
  // To read the conf-info from Gateway table
  if((1 == kp_i32) &&
     (ST_OK !=
      app_db_read_gateway_conf_from_db(pt_dbop_ctr,
                                       pt_msg_format_ctr,
                                       pt_gwcfg,
                                       kp_idbuf[0])))
  {
    DBG_LOG_ERR("Failed to read from Gateway table");
    tpret = ST_ERR;
    goto EXIT;
  }

  // 2. Read Dev Table
  // To prepare the filter to select all from Dev table (in principle, all
  // the children should be selected)
  pt_filter->table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  pt_filter->dataNum = 1;
  pt_filter->filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 1;
  pt_filter->
  filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_all;
  // To get the number of the item in Dev table
  kp_i32 =
    app_db_fetch_filtered_index(pt_dbop_ctr,
                          pt_filter,
                          sizeof(kp_idbuf),
                          kp_idbuf);
  if((kp_i32 < 0) || (kp_i32 > MAX_BT_WHITE_LIST))
  {
    DBG_LOG_ERR("Unexpected record number in table Dev: %d", kp_i32);
    // Just skip the following reading device from Dev table because the
    // invalid number
    kp_i32 = 0;
  }
  for(uint32_t i = 0; i < kp_i32; i++)
  {
    // Read a device information to the memory pointed by @p pt_dev_info
    memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));
    memset(pt_dev_info, 0, sizeof(struct device_info));
    if(ST_OK !=
       app_db_read_dev_info_from_db(pt_dbop_ctr,
                                    pt_msg_format_ctr,
                                    pt_dev_info,
                                    kp_idbuf[i % MAX_BT_WHITE_LIST]))
    {
      DBG_LOG_WARN(
        "Failed to read device info from Dev table (namdId %d).",
        kp_idbuf[i % MAX_BT_WHITE_LIST]);
      continue;
    }
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
    macAddr_t tp_mac_addr = { 0 };
    uint8_t tp_strbuf[GW_PRO_MAX_STRING_LEN_BYTE] = { 0 };
    uint32_t tp_strlen = strlen(pt_dev_info->mac_addr);
    uint32_t tp_idx = 0;
    for(uint32_t j = 0; j < tp_strlen; j++)
    {
      // Remove '-' in the MAC string
      if('-' != pt_dev_info->mac_addr[j])
      {
        tp_strbuf[tp_idx %
                  GW_PRO_MAX_STRING_LEN_BYTE] = pt_dev_info->mac_addr[j];
        tp_idx++;
      }
    }
    idstr_to_bytes_array(tp_mac_addr.addr.addrArray,
                         sizeof(tp_mac_addr.addr.addrArray), tp_strbuf);

    // Generate a node in children list
#if (ENABLE_MODBUS_FEATURE == 1)
    pt_cfg_child = makeChildrenForGw(pt_dev_info->sensor_type,
                                     pt_dev_info->manufacturer, \
                                     pt_dev_info->name,
                                     pt_dev_info->nameid,
                                     pt_dev_info->sensor_name,
                                     &tp_mac_addr,
                                     pt_dev_info->description);
#else
    pt_cfg_child = makeChildrenForGw(pt_dev_info->sensor_type,
                                     pt_dev_info->manufacturer, \
                                     pt_dev_info->name,
                                     pt_dev_info->nameid,
                                     pt_dev_info->sensor_name,
                                     &tp_mac_addr);
#endif
    if(NULL == pt_cfg_child)
    {
      DBG_LOG_ERR("Failed to make dev-child");
      tpret = ST_ERR;
      goto EXIT;
    }
    // Add the node to  children list
    list_add_tail(&pt_cfg_child->childrenNode,
                  &pt_gwcfg->gwConfig.childrenList);
    pt_gwcfg->gwConfig.childrenNumber++;
#else /* GWCONFIG_CHILDREN_USING_LINKED_LIST == 1 */
#error \
    "So far, do not support that GWCONFIG_CHILDREN_USING_LINKED_LIST is not 1."
#endif /* GWCONFIG_CHILDREN_USING_LINKED_LIST != 1 */
  }

  if(ST_OK == tpret)
  {
    DBG_LOG_DEBUG("To write the children list to gw-conf");
    // Mutex lock is required before list operation
    pthread_mutex_lock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
    app_cjson_release_chilren_list(&pt_gwconf_and_dev->gw_conf_info);
    memcpy(&pt_gwconf_and_dev->gw_conf_info, pt_gwcfg, sizeof(gwConfig_t));
    // Copy the list to pt_gwconf_and_dev->gw_conf_info
    // The child-list is a double list, we need set each pointer
    // carefully!!!
    if(!list_empty(&pt_gwcfg->gwConfig.childrenList))
    {
      //next
      pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList.next =
        pt_gwcfg->gwConfig.childrenList.next;
      pt_gwcfg->gwConfig.childrenList.next->prev =
        &pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList;
      //previous
      pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList.prev =
        pt_gwcfg->gwConfig.childrenList.prev;
      pt_gwcfg->gwConfig.childrenList.prev->next =
        &pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList;
    }
    else
    {
      INIT_LIST_HEAD(&pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList);
    }
    pthread_mutex_unlock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
  }
EXIT:
  if(pt_gwcfg)
  {
    if(ST_OK != tpret)
    {
      // Release the list when error happens
      DBG_LOG_ERR(
        "To release the children list pointed by the temporary buffer.");
      app_cjson_release_chilren_list(pt_gwcfg);
    }
    l_free(pt_gwcfg);
    pt_gwcfg = NULL;
  }
  if(pt_filter)
  {
    l_free(pt_filter);
    pt_filter = NULL;
  }
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  if(pt_dev_info)
  {
    l_free(pt_dev_info);
    pt_dev_info = NULL;
  }
  return tpret;
}
/**
 * @brief Fetch bullet gateway configuration (without children list) from
 *        table (i.e., only Gateway table) in a block manner. (i.e., this
 *        function would be blocked to wait for the response or a timeout
 *        happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_gwconf_and_dev The returned gateway configuration
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_fetch_gwconf_without_dev_from_db(
  struct dbop_controller *pt_dbop_ctr,
  gwconf_and_dev_t *pt_gwconf_and_dev)
{
  app_state_t tpret = ST_OK;
  gwConfig_t *pt_gwcfg = NULL;
  gw_pro_command_sqlite_filter_param_t *pt_filter = NULL;
  uint32_t kp_idbuf[MAX_BT_WHITE_LIST] = { 0 };
  int32_t kp_i32 = 0;

  struct msg_format_controller *pt_msg_format_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_gwconf_and_dev))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // 1. Initialization
  // 1.1 Initialize memory for gwConfig
  pt_gwcfg = (gwConfig_t *)l_malloc(sizeof(gwConfig_t));
  if(NULL == pt_gwcfg)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_gwcfg, 0, sizeof(gwConfig_t));

  memset(kp_idbuf, 0, sizeof(kp_idbuf));
  // 1.2 Initialize memory for database filter
  pt_filter =
    (gw_pro_command_sqlite_filter_param_t *)l_malloc(
      sizeof(gw_pro_command_sqlite_filter_param_t));
  if(NULL == pt_filter)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_filter, 0, sizeof(gw_pro_command_sqlite_filter_param_t));
  // 1.3 Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  // 2. Read gateway configuration
  // To prepare the filter to select all from Gateway table, but only one
  // item in Gateway table is expected.
  pt_filter->table = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
  pt_filter->dataNum = 1;
  pt_filter->filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 1;
  pt_filter->
  filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_all;
  // To get the number of the item in Gateway table
  kp_i32 =
    app_db_fetch_filtered_index(pt_dbop_ctr, pt_filter, sizeof(kp_idbuf),
                                kp_idbuf);
  if(1 != kp_i32)
  {
    DBG_LOG_WARN(
      "Unexpected record number in table Gateway: %d. Then, \
	  the first record would be read as the gateway configuration",
      kp_i32);
  }
  // To read the conf-info from Gateway table
  if((1 == kp_i32) &&
     (ST_OK !=
      app_db_read_gateway_conf_from_db(pt_dbop_ctr,
                                       pt_msg_format_ctr,
                                       pt_gwcfg,
                                       kp_idbuf[0])))
  {
    DBG_LOG_ERR("Failed to read from Gateway table");
    tpret = ST_ERR;
    goto EXIT;
  }

  if(ST_OK == tpret)
  {
    // Mutex lock is required before list operation
    pthread_mutex_lock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
    pt_gwconf_and_dev->gw_conf_info.lastEditTimeS =
      pt_gwcfg->lastEditTimeS;
    pt_gwconf_and_dev->gw_conf_info.version = pt_gwcfg->version;
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.type),
           &(pt_gwcfg->gwConfig.type),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.type));
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.manufacturer),
           &(pt_gwcfg->gwConfig.manufacturer),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.manufacturer));
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.name),
           &(pt_gwcfg->gwConfig.name),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.name));
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.nameId),
           &(pt_gwcfg->gwConfig.nameId),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.nameId));
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.macAddr),
           &(pt_gwcfg->gwConfig.macAddr),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.macAddr));
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.gatewayMode),
           &(pt_gwcfg->gwConfig.gatewayMode),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.gatewayMode));
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.bleConfig),
           &(pt_gwcfg->gwConfig.bleConfig),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.bleConfig));
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.timeConfig),
           &(pt_gwcfg->gwConfig.timeConfig),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.timeConfig));
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.mqttConfig),
           &(pt_gwcfg->gwConfig.mqttConfig),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.mqttConfig));
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.ipConfig),
           &(pt_gwcfg->gwConfig.ipConfig),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.ipConfig));
#if (MODBUS_FEATURE_ENABLE == 1)
    memcpy(&(pt_gwconf_and_dev->gw_conf_info.gwConfig.modbusConfig),
           &(pt_gwcfg->gwConfig.modbusConfig),
           sizeof(pt_gwconf_and_dev->gw_conf_info.gwConfig.modbusConfig));
#endif
    pthread_mutex_unlock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
  }
EXIT:
  if(pt_gwcfg)
  {
    l_free(pt_gwcfg);
    pt_gwcfg = NULL;
  }
  if(pt_filter)
  {
    l_free(pt_filter);
    pt_filter = NULL;
  }
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  return tpret;
}
/**
 * @brief Fetch bullet gateway configuration and children list from table
 *		  (i.e., Gateway and Dev table) and then output to a json file in a
 *      block manner (i.e., this function would be blocked to wait for the
 *      response or a timeout happens). Note that the global viarable
 *      @p g_sys_gwConfig_and_dev also would be updated once the function
 *      called successfully.
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_fpath INPUT: The path of gwConfig.json
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_fetch_gwconf_and_dev_from_db_to_file(
  struct dbop_controller *pt_dbop_ctr,
  const uint8_t *pt_fpath)
{
  app_state_t tpret = ST_OK;

  if((NULL == pt_dbop_ctr) || (NULL == pt_fpath))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // First, get the gwConf and dev from database
  if(ST_OK !=
     app_db_fetch_gwconf_and_dev_from_db(app_db_get_dbop_controller(),
                                         sys_get_gwConfig_and_dev()))
  {
    DBG_LOG_ERR("Failed to read gwConf and Dev");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Then output to a json file (i.e., the gwConfig.json)
  pthread_mutex_lock(&(sys_get_gwConfig_and_dev()->mtx_for_gw_conf_info));
  if(true !=
     app_cjson_gwconfig_to_jsonstr(&(sys_get_gwConfig_and_dev()->
                                     gw_conf_info),
                                   pt_fpath))
  {
    DBG_LOG_ERR("Failed to write gwConfig and dev to file");
    tpret = ST_ERR;
    goto EXIT;
  }
  pthread_mutex_unlock(&(sys_get_gwConfig_and_dev()->mtx_for_gw_conf_info));

EXIT:
  return tpret;
}
/**
 * @brief Fetch bullet device information based on the given BLE MAC
 *      address from Dev table in a block manner. (i.e., this function
 *      would be blocked to wait for the response or a timeout happens).
 *      Note that when multiple item with the identical BLE MAC address, a
 *      name ID with the given BLE MAC address would be returned randomly.
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_dev_info The returned device information
 * @param pt_macstr INPUT: The given BLE MAC address
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_fetch_a_device_info_from_db_based_on_macaddr(
  struct dbop_controller *pt_dbop_ctr,
  struct device_info *pt_dev_info,
  const uint8_t *pt_macstr)
{
  app_state_t tpret = ST_OK;
  int32_t kp_int32 = 0;
  gw_pro_command_sqlite_filter_param_t kp_filter = { 0 };
  uint32_t kp_nameid = 0;
  struct msg_format_controller *pt_msg_format_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_dev_info) || (NULL == pt_macstr))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // To get the namdID based on the given BLE MAC address
  kp_filter.table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  kp_filter.dataNum = 1;
  kp_filter.filterData[0].field = 4;
  kp_filter.filterData[0].condition =
    gw_pro_sqlite_cond_str_equal;
  snprintf(
    kp_filter.filterData[0].condParam.string_value.s,
    GW_PRO_MAX_STRING_LEN_BYTE,
    "%s",
    pt_macstr);
  DBG_LOG_DEBUG("Filtered MAC address: %s", pt_macstr);

  kp_int32 = app_db_fetch_filtered_index(pt_dbop_ctr,
                                         &kp_filter,
                                         sizeof(kp_nameid),
                                         &kp_nameid);
  if(kp_int32 <= 0)
  {
    DBG_LOG_ERR("Failed to get nameId info from table %d, ret = %d",
                kp_filter.table, kp_int32);
    tpret = ST_ERR;
    goto EXIT;
  }

  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  // Read the device information based on the name ID
  if(ST_OK !=
     app_db_read_dev_info_from_db(pt_dbop_ctr,
                                  pt_msg_format_ctr,
                                  pt_dev_info,
                                  kp_nameid))
  {
    DBG_LOG_ERR("Failed to read from Dev table");
    tpret = ST_ERR;
    goto EXIT;
  }
EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  return tpret;
}
/**
 * @brief Fill the dtunit ( pointed by @p pt_msg_format_ctr ) with
 *        @p pt_sensor_conf to write to the database.
 */
static app_state_t
to_fill_dtunit_array_for_sensor_conf_writing(
  struct msg_format_controller *pt_msg_format_ctr,
  const sensorConfig_t *pt_sensor_conf,
  struct fouta_fw_info *fw_info)
{
  app_state_t tpret = ST_OK;

  if((NULL == pt_msg_format_ctr) || (NULL == pt_sensor_conf))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return ST_ERR;
  }

  // "INT PRIMARY KEY DEFAULT 0"
  // "nameId"
  pt_msg_format_ctr->dtunit[0 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 1;  // we
                                                                             // start
                                                                             // from
                                                                             // 1;
  pt_msg_format_ctr->dtunit[0 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_int32 =
    pt_sensor_conf->nameId;
#if 0 // Do not write the field because these field is not defined in sensorConfig_t 
  // "TEXT DEFAULT ' '"
  // "name"
  pt_msg_format_ctr->dtunit[1 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 2;
  snprintf(pt_msg_format_ctr->dtunit[1 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", "");
  // "TEXT DEFAULT ' '",
  // "BLESensorName",
  pt_msg_format_ctr->dtunit[2 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 3;
  snprintf(pt_msg_format_ctr->dtunit[2 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", "");
#endif
  // "INT DEFAULT 0",
  // "FUOTA_Version",
  pt_msg_format_ctr->dtunit[1 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    96;
  if(fw_info == NULL)
  {
    pt_msg_format_ctr->dtunit[1 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
    data_uint32 =
      g_sys_sensor_fuota_info.ver;
  }else{
    pt_msg_format_ctr->dtunit[1 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 = fw_info->ver;
  }
  
  // "TEXT DEFAULT ' '",
  // "FUOTA_Filename",
  pt_msg_format_ctr->dtunit[2 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    97;
  if(fw_info == NULL)
  {
    snprintf(pt_msg_format_ctr->dtunit[2 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s",
           g_sys_sensor_fuota_info.fpath);
  }else{
    snprintf(pt_msg_format_ctr->dtunit[2 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s",
           fw_info->fpath);
  }
  
  // "TEXT DEFAULT ' '",
  // "macAddr",
  pt_msg_format_ctr->dtunit[3 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 4;
  snprintf(pt_msg_format_ctr->dtunit[3 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%.2X%.2X%.2X%.2X%.2X%.2X", \
           pt_sensor_conf->macAddr.addr.addrArray[0], \
           pt_sensor_conf->macAddr.addr.addrArray[1], \
           pt_sensor_conf->macAddr.addr.addrArray[2], \
           pt_sensor_conf->macAddr.addr.addrArray[3], \
           pt_sensor_conf->macAddr.addr.addrArray[4], \
           pt_sensor_conf->macAddr.addr.addrArray[5]);
  // "TEXT DEFAULT 'SKF'",
  // "manufacturer",
  pt_msg_format_ctr->dtunit[4 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 5;
  snprintf(pt_msg_format_ctr->dtunit[4 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_sensor_conf->manufacturer);
  // "TEXT DEFAULT ' '",
  // "FirmwareVersion",
  pt_msg_format_ctr->dtunit[5 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 6;
  if(fw_info == NULL)
  {
    snprintf(pt_msg_format_ctr->dtunit[5 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%u.%u.%u",
           (g_sys_sensor_fuota_info.currentVer & 0xff0000) >> 16,
           (g_sys_sensor_fuota_info.currentVer & 0xff00) >> 8,
           (g_sys_sensor_fuota_info.currentVer & 0xff));
    DBG_LOG_DEBUG("g_sys_sensor_fuota_info.currentVer %u",
                g_sys_sensor_fuota_info.currentVer);
    DBG_LOG_DEBUG("g_sys_sensor_fuota_info.currentVer %s",
                pt_msg_format_ctr->dtunit[5 %
                                          GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char);
  }else{
    snprintf(pt_msg_format_ctr->dtunit[5 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%u.%u.%u",
           (fw_info->currentVer & 0xff0000) >> 16,
           (fw_info->currentVer & 0xff00) >> 8,
           (fw_info->currentVer & 0xff));
    DBG_LOG_DEBUG("fw_info->currentVer %u",
                fw_info->currentVer);
    DBG_LOG_DEBUG("fw_info->currentVer %s",
                pt_msg_format_ctr->dtunit[5 %
                                          GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char);
  }
  
  // "INT DEFAULT 0",
  // "version",
  pt_msg_format_ctr->dtunit[6 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 7;
  pt_msg_format_ctr->dtunit[6 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->version;
  // "INT DEFAULT 0",
  // "lastEditTime",
  pt_msg_format_ctr->dtunit[7 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 8;
  pt_msg_format_ctr->dtunit[7 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint64 =
    pt_sensor_conf->lastEditTimeS;
  // "BOOLEAN DEFAULT false",
  // "haveConfiguredToSensor",
  pt_msg_format_ctr->dtunit[8 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 9;
  pt_msg_format_ctr->dtunit[8 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->haveConfiguredToSensor;
  // "INT DEFAULT 0",
  // "whenConfiguredToSensor",
  pt_msg_format_ctr->dtunit[9 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    10;
  pt_msg_format_ctr->dtunit[9 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint64 =
    pt_sensor_conf->whenConfiguredToSensor;
  // "TEXT DEFAULT ' '",
  // "type",
  pt_msg_format_ctr->dtunit[10 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    11;
  snprintf(pt_msg_format_ctr->dtunit[10 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_sensor_conf->type);
  // "INT DEFAULT 0",
  // "sensorMode",
  pt_msg_format_ctr->dtunit[11 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    12;
  pt_msg_format_ctr->dtunit[11 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode;
  DBG_LOG_DEBUG("sensorMode %d",
                pt_msg_format_ctr->dtunit[11 %
                                          GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_uint8);
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
  // "INT DEFAULT 0",
  // "sensorModeParameter",
  pt_msg_format_ctr->dtunit[95 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    98;
  pt_msg_format_ctr->dtunit[95 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.sysConfig.sensorModeParameter;
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
  // "REAL DEFAULT 0.0",
  // "batteryAlarmThreshold",
  pt_msg_format_ctr->dtunit[12 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    13;
  pt_msg_format_ctr->dtunit[12 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.sysConfig.batteryAlarmThrePercent;
  DBG_LOG_DEBUG("batteryAlarmThreshold %d",
                pt_msg_format_ctr->dtunit[12 %
                                          GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_uint8);
  // "INT DEFAULT 0",
  // "txPower_adv",
  pt_msg_format_ctr->dtunit[13 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    14;
  pt_msg_format_ctr->dtunit[13 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_int8 =
    pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAdv;
  // "INT DEFAULT 0",
  // "dataRate_auxAdv",
  pt_msg_format_ctr->dtunit[14 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    15;
  pt_msg_format_ctr->dtunit[14 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint16 =
    pt_sensor_conf->bulletSensorConfig.sysConfig.dataRateAuxAdv;
  // "INT DEFAULT 0",
  // "txPower_auxAdv",
  pt_msg_format_ctr->dtunit[15 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    16;
  pt_msg_format_ctr->dtunit[15 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_int8 =
    pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAuxAdv;
  // "INT DEFAULT 0",
  // "period_quickPolling",
  pt_msg_format_ctr->dtunit[16 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    17;
  pt_msg_format_ctr->dtunit[16 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.schConfig.period_quickPolling_s;
  // "INT DEFAULT 0",
  // "referenceTime_quickPolling",
  pt_msg_format_ctr->dtunit[17 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    18;
  pt_msg_format_ctr->dtunit[17 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.schConfig.refTime_quickPolling_s;
  // "INT DEFAULT 0",
  // "period_regularSensing",
  pt_msg_format_ctr->dtunit[18 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    19;
  pt_msg_format_ctr->dtunit[18 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.schConfig.period_regularSensing_s;
  // "INT DEFAULT 0",
  // "referenceTime_regularSensing",
  pt_msg_format_ctr->dtunit[19 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    20;
  pt_msg_format_ctr->dtunit[19 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.schConfig.refTime_regularSensing_s;
  // "INT DEFAULT 0",
  // "period_comm",
  pt_msg_format_ctr->dtunit[20 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    21;
  pt_msg_format_ctr->dtunit[20 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.schConfig.period_comm_s;
  // "INT DEFAULT 0",
  // "referenceTime_period_comm",
  pt_msg_format_ctr->dtunit[21 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    22;
  pt_msg_format_ctr->dtunit[21 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.schConfig.refTime_period_comm_s;
  // "INT DEFAULT 0",
  // "PRE_ACQ_VIB_FS_HZ",
  pt_msg_format_ctr->dtunit[22 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    23;
  pt_msg_format_ctr->dtunit[22 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz;
  // "INT DEFAULT 0",
  // "PRE_ACQ_VIB_N",
  pt_msg_format_ctr->dtunit[23 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    24;
  pt_msg_format_ctr->dtunit[23 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_n;
  // "INT DEFAULT 0",
  // "PRE_ACQ_VIB_AXIS_ACQ_EVAL",
  pt_msg_format_ctr->dtunit[24 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    25;
  pt_msg_format_ctr->dtunit[24 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_axis_acq_eval;
  // "INT DEFAULT 0",
  // "PRE_ACQ_VIB_RANGE",
  pt_msg_format_ctr->dtunit[25 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    26;
  pt_msg_format_ctr->dtunit[25 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_range;
  // "INT DEFAULT 0",
  // "PRE_ACQ_MAG_FS_HZ",
  pt_msg_format_ctr->dtunit[26 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    27;
  pt_msg_format_ctr->dtunit[26 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz;
  // "INT DEFAULT 0",
  // "PRE_ACQ_MAG_N",
  pt_msg_format_ctr->dtunit[27 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    28;
  pt_msg_format_ctr->dtunit[27 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_n;
  // "INT DEFAULT 0",
  // "PRE_ACQ_MAG_AXIS_ACQ_EVAL",
  pt_msg_format_ctr->dtunit[28 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    29;
  pt_msg_format_ctr->dtunit[28 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_axis_acq_eval;
  // "INT DEFAULT 0",
  // "Facc",
  pt_msg_format_ctr->dtunit[29 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    30;
  pt_msg_format_ctr->dtunit[29 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.senConfig.fAccHz;
  // "INT DEFAULT 0",
  // "Nacc",
  pt_msg_format_ctr->dtunit[30 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    31;
  pt_msg_format_ctr->dtunit[30 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.senConfig.nAcc;
  // "INT DEFAULT 0",
  // "ACQ_VIB_AXIS_ACQ_EVAL",
  pt_msg_format_ctr->dtunit[31 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    32;
  pt_msg_format_ctr->dtunit[31 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval;
  // "INT DEFAULT 0",
  // "ACQ_VIB_RANGE",
  pt_msg_format_ctr->dtunit[32 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    33;
  pt_msg_format_ctr->dtunit[32 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_range;
  // "INT DEFAULT 0",
  // "ACQ_MAG_FS_HZ",
  pt_msg_format_ctr->dtunit[33 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    34;
  pt_msg_format_ctr->dtunit[33 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_fs_hz;
  // "INT DEFAULT 0",
  // "ACQ_MAG_N",
  pt_msg_format_ctr->dtunit[34 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    35;
  pt_msg_format_ctr->dtunit[34 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_n;
  // "INT DEFAULT 0",
  // "ACQ_MAG_AXIS_ACQ_EVAL",
  pt_msg_format_ctr->dtunit[35 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    36;
  pt_msg_format_ctr->dtunit[35 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval;
  // "REAL DEFAULT 0.364",
  // "FS_COEF",
  pt_msg_format_ctr->dtunit[36 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    37;
  pt_msg_format_ctr->dtunit[36 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.senConfig.fs_coef;
  // "REAL DEFAULT 0.0",
  // "GEE_COEF",
  pt_msg_format_ctr->dtunit[37 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    38;
  pt_msg_format_ctr->dtunit[37 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF;
  // "REAL DEFAULT 0.0",
  // "V_COEF",
  pt_msg_format_ctr->dtunit[38 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    39;
  pt_msg_format_ctr->dtunit[38 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.V_COEF;
  // "INT DEFAULT 0",
  // "MEAS_POSITION",
  pt_msg_format_ctr->dtunit[39 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    40;
  pt_msg_format_ctr->dtunit[39 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_POSITION;
  // "INT DEFAULT 0",
  // "MEAS_LOAD",
  pt_msg_format_ctr->dtunit[40 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    41;
  pt_msg_format_ctr->dtunit[40 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_LOAD;
  // "INT DEFAULT 0",
  // "MEAS_AXIS_VIB",
  pt_msg_format_ctr->dtunit[41 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    42;
  pt_msg_format_ctr->dtunit[41 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB;
  // "INT DEFAULT 0",
  // "MEAS_AXIS_MAG",
  pt_msg_format_ctr->dtunit[42 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    43;
  pt_msg_format_ctr->dtunit[42 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG;
  // "BOOLEAN DEFAULT false",
  // "VIB_START_FG",
  pt_msg_format_ctr->dtunit[43 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    44;
  pt_msg_format_ctr->dtunit[43 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_START_FG;
  // "REAL DEFAULT 0.0",
  // "VIB_RMS_START_TL",
  pt_msg_format_ctr->dtunit[44 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    45;
  pt_msg_format_ctr->dtunit[44 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_RMS_START_TL;
  // "BOOLEAN DEFAULT false",
  // "MAG_START_FG",
  pt_msg_format_ctr->dtunit[45 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    46;
  pt_msg_format_ctr->dtunit[45 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_START_FG;
  // "REAL DEFAULT 0.0",
  // "MAG_RMS_START_TL",
  pt_msg_format_ctr->dtunit[46 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    47;
  pt_msg_format_ctr->dtunit[46 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_START_TL;
  // "BOOLEAN DEFAULT false",
  // "MAG_STABLE_FG",
  pt_msg_format_ctr->dtunit[47 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    48;
  pt_msg_format_ctr->dtunit[47 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_STABLE_FG;
  // "REAL DEFAULT 0.0",
  // "MAG_RMS_VAR_TH",
  pt_msg_format_ctr->dtunit[48 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    49;
  pt_msg_format_ctr->dtunit[48 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH;
  // "REAL DEFAULT 0.0",
  // "RPM_VAR_RANGE",
  pt_msg_format_ctr->dtunit[49 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    50;
  pt_msg_format_ctr->dtunit[49 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.RPM_VAR_RANGE;
  // "INT DEFAULT 0",
  // "DEC_LOGIC_TEMP_M",
  pt_msg_format_ctr->dtunit[50 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    51;
  pt_msg_format_ctr->dtunit[50 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint16 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M;
  // "INT DEFAULT 0",
  // "DEC_LOGIC_TEMP_N",
  pt_msg_format_ctr->dtunit[51 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    52;
  pt_msg_format_ctr->dtunit[51 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint16 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N;
  // "INT DEFAULT 0",
  // "DEC_LOGIC_LEARN_NUM_TEMP",
  pt_msg_format_ctr->dtunit[52 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    53;
  pt_msg_format_ctr->dtunit[52 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint16 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_TEMP;
  // "INT DEFAULT 0",
  // "DEC_LOGIC_VIB_M",
  pt_msg_format_ctr->dtunit[53 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    54;
  pt_msg_format_ctr->dtunit[53 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint16 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M;
  // "INT DEFAULT 0",
  // "DEC_LOGIC_VIB_N",
  pt_msg_format_ctr->dtunit[54 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    55;
  pt_msg_format_ctr->dtunit[54 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint16 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N;
  // "INT DEFAULT 0",
  // "DEC_LOGIC_LEARN_NUM_VIB",
  pt_msg_format_ctr->dtunit[55 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    56;
  pt_msg_format_ctr->dtunit[55 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint16 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB;
  // "INT DEFAULT 0",
  // "DEC_LOGIC_LEARN_NUM_MAG",
  pt_msg_format_ctr->dtunit[56 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    57;
  pt_msg_format_ctr->dtunit[56 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint16 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG;
  // "BOOLEAN DEFAULT false",
  // "FUNC_ANOM_TEMP",
  pt_msg_format_ctr->dtunit[57 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    58;
  pt_msg_format_ctr->dtunit[57 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP;
  // "BOOLEAN DEFAULT false",
  // "FUNC_ANOM_OV",
  pt_msg_format_ctr->dtunit[58 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    59;
  pt_msg_format_ctr->dtunit[58 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_OV;
  // "BOOLEAN DEFAULT false",
  // "FUNC_ANOM_MECH",
  pt_msg_format_ctr->dtunit[59 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    60;
  pt_msg_format_ctr->dtunit[59 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH;
  // "BOOLEAN DEFAULT false",
  // "FUNC_ANOM_BRG",
  pt_msg_format_ctr->dtunit[60 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    61;
  pt_msg_format_ctr->dtunit[60 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG;
  // "BOOLEAN DEFAULT false",
  // "FUNC_ANOM_LUB",
  pt_msg_format_ctr->dtunit[61 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    62;
  pt_msg_format_ctr->dtunit[61 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB;
  // "BOOLEAN DEFAULT false",
  // "FUNC_ANOM_MTR",
  pt_msg_format_ctr->dtunit[62 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    63;
  pt_msg_format_ctr->dtunit[62 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR;
  // "BOOLEAN DEFAULT false",
  // "FUNC_ANOM_GEAR",
  pt_msg_format_ctr->dtunit[63 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    64;
  pt_msg_format_ctr->dtunit[63 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR;
  // "BOOLEAN DEFAULT false",
  // "FUNC_ANOM_FAN",
  pt_msg_format_ctr->dtunit[64 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    65;
  pt_msg_format_ctr->dtunit[64 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN;
  // "BOOLEAN DEFAULT false",
  // "FUNC_ANOM_PUMP",
  pt_msg_format_ctr->dtunit[65 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    66;
  pt_msg_format_ctr->dtunit[65 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP;
  // "INT DEFAULT 0",
  // "ASSET_LEVEL",
  pt_msg_format_ctr->dtunit[66 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    67;
  pt_msg_format_ctr->dtunit[66 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.ASSET_LEVEL;
  // "INT DEFAULT 0",
  // "FLEX_TYPE",
  pt_msg_format_ctr->dtunit[67 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    68;
  pt_msg_format_ctr->dtunit[67 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FLEX_TYPE;
  // "REAL DEFAULT 0.0",
  // "BORE_DIAMETER_MM",
  pt_msg_format_ctr->dtunit[68 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    69;
  pt_msg_format_ctr->dtunit[68 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM;
  // "REAL DEFAULT 0.0",
  // "RUN_SPEED_RPM",
  pt_msg_format_ctr->dtunit[69 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    70;
  pt_msg_format_ctr->dtunit[69 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM;
  // "REAL DEFAULT 0.0",
  // "BRG_INFO_BPFO",
  pt_msg_format_ctr->dtunit[70 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    71;
  pt_msg_format_ctr->dtunit[70 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFO;
  // "REAL DEFAULT 0.0",
  // "BRG_INFO_BPFI",
  pt_msg_format_ctr->dtunit[71 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    72;
  pt_msg_format_ctr->dtunit[71 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFI;
  // "REAL DEFAULT 0.0",
  // "BRG_INFO_BSF",
  pt_msg_format_ctr->dtunit[72 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    73;
  pt_msg_format_ctr->dtunit[72 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BSF;
  // "REAL DEFAULT 0.0",
  // "BRG_INFO_FTF",
  pt_msg_format_ctr->dtunit[73 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    74;
  pt_msg_format_ctr->dtunit[73 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_FTF;
  // "INT DEFAULT 0",
  // "MTR_INFO_FL",
  pt_msg_format_ctr->dtunit[74 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    75;
  pt_msg_format_ctr->dtunit[74 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_FL;
  // "INT DEFAULT 0",
  // "MTR_INFO_BAR",
  pt_msg_format_ctr->dtunit[75 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    76;
  pt_msg_format_ctr->dtunit[75 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_BAR;
  // "INT DEFAULT 0",
  // "GEAR_INFO_TOOTH",
  pt_msg_format_ctr->dtunit[76 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    77;
  pt_msg_format_ctr->dtunit[76 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH;
  // "INT DEFAULT 0",
  // "FAN_INFO_BLADE",
  pt_msg_format_ctr->dtunit[77 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    78;
  pt_msg_format_ctr->dtunit[77 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FAN_INFO_BLADE;
  // "INT DEFAULT 0",
  // "PUMP_INFO_VANE",
  pt_msg_format_ctr->dtunit[78 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    79;
  pt_msg_format_ctr->dtunit[78 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.PUMP_INFO_VANE;
  // "REAL DEFAULT 0.0",
  // "TEMP_OV_ALERT_CDEGREE",
  pt_msg_format_ctr->dtunit[79 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    80;
  pt_msg_format_ctr->dtunit[79 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE;
  // "INT DEFAULT 0",
  // "AlarmThrestemp",
  pt_msg_format_ctr->dtunit[80 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    81;
  pt_msg_format_ctr->dtunit[80 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_int32 =
    pt_sensor_conf->bulletSensorConfig.algoConfig.AlarmThrestemp;
  // "REAL DEFAULT 0.0",
  // "ACC_OV_ALERT",
  pt_msg_format_ctr->dtunit[81 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    82;
  pt_msg_format_ctr->dtunit[81 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALERT;
  // "REAL DEFAULT 0.0",
  // "ACC_OV_ALARM",
  pt_msg_format_ctr->dtunit[82 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    83;
  pt_msg_format_ctr->dtunit[82 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALARM;
  // "REAL DEFAULT 0.0",
  // "ACC_HAL_ALERT",
  pt_msg_format_ctr->dtunit[83 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    84;
  pt_msg_format_ctr->dtunit[83 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALERT;
  // "REAL DEFAULT 0.0",
  // "ACC_HAL_ALARM",
  pt_msg_format_ctr->dtunit[84 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    85;
  pt_msg_format_ctr->dtunit[84 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALARM;
  // "REAL DEFAULT 0.0",
  // "VEL_OV_ALERT",
  pt_msg_format_ctr->dtunit[85 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    86;
  pt_msg_format_ctr->dtunit[85 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALERT;
  // "REAL DEFAULT 0.0",
  // "VEL_OV_ALARM",
  pt_msg_format_ctr->dtunit[86 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    87;
  pt_msg_format_ctr->dtunit[86 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALARM;
  // "REAL DEFAULT 0.0",
  // "VEL_HAL_ALERT",
  pt_msg_format_ctr->dtunit[87 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    88;
  pt_msg_format_ctr->dtunit[87 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALERT;
  // "REAL DEFAULT 0.0",
  // "VEL_HAL_ALARM",
  pt_msg_format_ctr->dtunit[88 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    89;
  pt_msg_format_ctr->dtunit[88 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALARM;
  // "REAL DEFAULT 0.0",
  // "ENV_OV_ALERT",
  pt_msg_format_ctr->dtunit[89 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    90;
  pt_msg_format_ctr->dtunit[89 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALERT;
  // "REAL DEFAULT 0.0",
  // "ENV_OV_ALARM",
  pt_msg_format_ctr->dtunit[90 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    91;
  pt_msg_format_ctr->dtunit[90 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALARM;
  // "REAL DEFAULT 0.0",
  // "ENV_HAL_ALERT",
  pt_msg_format_ctr->dtunit[91 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    92;
  pt_msg_format_ctr->dtunit[91 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALERT;
  // "REAL DEFAULT 0.0",
  // "ENV_HAL_ALARM",
  pt_msg_format_ctr->dtunit[92 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    93;
  pt_msg_format_ctr->dtunit[92 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_float =
    pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALARM;
  // "INT DEFAULT 0",
  // "waveDataAcqPeriod",
  pt_msg_format_ctr->dtunit[93 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    94;
  pt_msg_format_ctr->dtunit[93 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.schConfig.period_wave_s;
  DBG_LOG_DEBUG("period_wave_s %d",
                pt_msg_format_ctr->dtunit[93 %
                                          GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_uint32);
  // "INT DEFAULT 0",
  // "waveDataAcqReference",
  pt_msg_format_ctr->dtunit[94 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    95;
  pt_msg_format_ctr->dtunit[94 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_sensor_conf->bulletSensorConfig.schConfig.refTime_wave_s;
  DBG_LOG_DEBUG("refTime_wave_s %d",
                pt_msg_format_ctr->dtunit[94 %
                                          GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_uint32);
  return tpret;
}

/**
 * @brief Deliver bullet sensor configuration to table in a block manner.
 *        (i.e., this function would be blocked to wait for the response or
 *        a timeout happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_sensor_conf The sensor configuration to be delivered
 * @param fw_info INPUT: The pointer to the firmware info. The global
 *                       variable @p g_sys_sensor_fuota_info would be used
 *                       if this parameter is NULL.
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_deliver_sensor_conf_to_db(
  struct dbop_controller *pt_dbop_ctr,
  const sensorConfig_t *pt_sensor_conf,
  struct fouta_fw_info *fw_info)
{
  app_state_t tpret = ST_OK;
  struct msg_format_controller *pt_msg_format_ctr = NULL;
  uint32_t kp_dtunit_sz = 0;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_sensor_conf))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  // Generate content
  to_fill_dtunit_array_for_sensor_conf_writing(pt_msg_format_ctr,
                                               pt_sensor_conf,
                                               fw_info);
  // Generate message
  // Generate the header of message
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.table =
    GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.itemIdx = pt_sensor_conf->nameId;
  // The number of column
  kp_dtunit_sz = sizeof(SensorConfig) / sizeof(SensorConfig[0]);
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.dataNum = kp_dtunit_sz;
  // Generate the payload of message
  for(uint32_t i = 0; i < kp_dtunit_sz; i++)
  {
    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
    sqlite_update_item_param.
    updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
      pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].field;
    memcpy(
      &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
      param.sqlite_update_item_param.
      updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data,
      &pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data,
      sizeof(pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data));
  }

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);

  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);

  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX) {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n", MSGSEND_ERR_CNT_MAX);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = ST_ERR;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = ST_ERR;
    goto EXIT;
  }
  DBG_LOG_INFO(
    "Sensor-conf write response info: cmd = %d, cmdId = %d, success(0)/fail(1) = %d", \
    pt_dbop_resp_waiting_obj->resp_info.command, pt_dbop_resp_waiting_obj->resp_info.commandId, \
    pt_dbop_resp_waiting_obj->resp_info.response);
EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
/**
 * @brief Deliver partial bullet sensor configuration (only including
 *        namdId, readable name, BLE name, BLE MAC address, manufacturer,
 *        and sensor type) to table in a block manner. (i.e., this function
 *        would be blocked to wait for the response or a timeout happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_sensor_conf The sensor configuration to be delivered
 * @return @p ST_OK should be returned if no error happens.
 */
static app_state_t
app_db_deliver_partial_sensor_conf_to_db(
  struct dbop_controller *pt_dbop_ctr,
  const sensorConfig_t *pt_sensor_conf)
{
  app_state_t tpret = ST_OK;
  struct msg_format_controller *pt_msg_format_ctr = NULL;
  uint32_t kp_dtunit_sz = 0;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_sensor_conf))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  // Generate content
  // "INT PRIMARY KEY DEFAULT 0"
  // "nameId"
  pt_msg_format_ctr->dtunit[0].field = 1;
  pt_msg_format_ctr->dtunit[0].data.data_int32 = pt_sensor_conf->nameId;
  // "TEXT DEFAULT ' '"
  // "name"
  pt_msg_format_ctr->dtunit[1].field = 2;
  snprintf(pt_msg_format_ctr->dtunit[1].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", "");
  // "TEXT DEFAULT ' '",
  // "BLESensorName",
  pt_msg_format_ctr->dtunit[2].field = 3;
  snprintf(pt_msg_format_ctr->dtunit[2].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", "");
  // "TEXT DEFAULT ' '",
  // "macAddr",
  pt_msg_format_ctr->dtunit[3].field = 4;
  snprintf(pt_msg_format_ctr->dtunit[3].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%.2X%.2X%.2X%.2X%.2X%.2X", \
           pt_sensor_conf->macAddr.addr.addrArray[0], \
           pt_sensor_conf->macAddr.addr.addrArray[1], \
           pt_sensor_conf->macAddr.addr.addrArray[2], \
           pt_sensor_conf->macAddr.addr.addrArray[3], \
           pt_sensor_conf->macAddr.addr.addrArray[4], \
           pt_sensor_conf->macAddr.addr.addrArray[5]);
  // "TEXT DEFAULT 'SKF'",
  // "manufacturer",
  pt_msg_format_ctr->dtunit[4].field = 5;
  snprintf(pt_msg_format_ctr->dtunit[4].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_sensor_conf->manufacturer);
  // "TEXT DEFAULT ' '",
  // "type",
  pt_msg_format_ctr->dtunit[5].field = 11;
  snprintf(pt_msg_format_ctr->dtunit[5].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_sensor_conf->type);

  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.table =
    GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.itemIdx = pt_sensor_conf->nameId;
  kp_dtunit_sz = 6;  // Only six fields to be updated
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.dataNum = kp_dtunit_sz;
  // Generate the payload of message
  for(uint32_t i = 0; i < kp_dtunit_sz; i++)
  {
    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
    sqlite_update_item_param.
    updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
      pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].field;
    memcpy(
      &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
      param.sqlite_update_item_param.
      updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data,
      &pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data,
      sizeof(pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data));
  }

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);

  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);

  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX) {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n", MSGSEND_ERR_CNT_MAX);
          fflush(stdout);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = ST_ERR;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = ST_ERR;
    goto EXIT;
  }
  DBG_LOG_INFO(
    "Sensor-conf partial write response info: cmd = %d, cmdId = %d, success(0)/fail(1) = %d", \
    pt_dbop_resp_waiting_obj->resp_info.command, pt_dbop_resp_waiting_obj->resp_info.commandId, \
    pt_dbop_resp_waiting_obj->resp_info.response);
EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
/**
 * @brief Fill the dtunit ( pointed by @p pt_msg_format_ctr ) with
 *        @p pt_gwconf to write to the database.
 */
static app_state_t
to_fill_dtunit_array_for_gateway_conf_writing(
  struct msg_format_controller *pt_msg_format_ctr,
  const gwConfig_t *pt_gwconf)
{
  app_state_t tpret = ST_OK;

  if((NULL == pt_msg_format_ctr) || (NULL == pt_gwconf))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return ST_ERR;
  }
  // "INT PRIMARY KEY DEFAULT 0",
  // "nameId",
  pt_msg_format_ctr->dtunit[0 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 1;
  pt_msg_format_ctr->dtunit[0 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_gwconf->gwConfig.nameId;
  // "TEXT DEFAULT ' '",
  // "name",
  pt_msg_format_ctr->dtunit[1 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 2;
  snprintf(pt_msg_format_ctr->dtunit[1 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_gwconf->gwConfig.name);
  // "TEXT DEFAULT ' '",
  // "macAddr",
  pt_msg_format_ctr->dtunit[2 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 3;
  snprintf(pt_msg_format_ctr->dtunit[2 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%.2X-%.2X-%.2X-%.2X-%.2X-%.2X",
           pt_gwconf->gwConfig.macAddr.addr.addrArray[0], \
           pt_gwconf->gwConfig.macAddr.addr.addrArray[1], \
           pt_gwconf->gwConfig.macAddr.addr.addrArray[2], \
           pt_gwconf->gwConfig.macAddr.addr.addrArray[3], \
           pt_gwconf->gwConfig.macAddr.addr.addrArray[4], \
           pt_gwconf->gwConfig.macAddr.addr.addrArray[5]);
  // "TEXT DEFAULT 'SKF'"
  // "manufacturer"
  pt_msg_format_ctr->dtunit[3 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 4;
  snprintf(pt_msg_format_ctr->dtunit[3 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s",
           pt_gwconf->gwConfig.manufacturer);
  // "INT DEFAULT 0",
  // "ApplicationVersion",
  // TODO
  pt_msg_format_ctr->dtunit[4 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 5;
  pt_msg_format_ctr->dtunit[4 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 = 0;
  // "INT DEFAULT 0",
  // "LinuxKernelVersion",
  // TODO
  pt_msg_format_ctr->dtunit[5 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 6;
  pt_msg_format_ctr->dtunit[5 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 = 0;
  // "INT DEFAULT 0",
  // "version",
  pt_msg_format_ctr->dtunit[6 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 7;
  pt_msg_format_ctr->dtunit[6 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_gwconf->version;
  // "INT DEFAULT 0",
  // "lastEditTime",
  pt_msg_format_ctr->dtunit[7 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 8;
  pt_msg_format_ctr->dtunit[7 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint64 =
    pt_gwconf->lastEditTimeS;
  // "TEXT DEFAULT ' '",
  // "type",
  pt_msg_format_ctr->dtunit[8 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 9;
  snprintf(pt_msg_format_ctr->dtunit[8 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, pt_gwconf->gwConfig.type);
  // "INT DEFAULT 0",
  // "gatewayMode",
  pt_msg_format_ctr->dtunit[9 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    10;
  pt_msg_format_ctr->dtunit[9 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_gwconf->gwConfig.gatewayMode;
  // "INT DEFAULT 0",
  // "BLE1TxPower",
  pt_msg_format_ctr->dtunit[10 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    11;
  pt_msg_format_ctr->dtunit[10 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_int8 =
    pt_gwconf->gwConfig.bleConfig.ble1TxPower;
  // "INT DEFAULT 0",
  // "BLE2TxPower",
  pt_msg_format_ctr->dtunit[11 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    12;
  pt_msg_format_ctr->dtunit[11 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_int8 =
    pt_gwconf->gwConfig.bleConfig.ble2TxPower;
  // "INT DEFAULT 0",
  // "BLE1Antenna",
  pt_msg_format_ctr->dtunit[12 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    13;
  pt_msg_format_ctr->dtunit[12 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_gwconf->gwConfig.bleConfig.ble1Antenna;
  // "INT DEFAULT 0",
  // "BLE2Antenna",
  pt_msg_format_ctr->dtunit[13 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    14;
  pt_msg_format_ctr->dtunit[13 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_gwconf->gwConfig.bleConfig.ble2Antenna;
  // "INT DEFAULT 0",
  // "BLEFilterListNameNumber",
  pt_msg_format_ctr->dtunit[14 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    15;
  pt_msg_format_ctr->dtunit[14 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_gwconf->gwConfig.childrenNumber;
  // "INT DEFAULT 0",
  // "BLEDuplicatedDrop",
  pt_msg_format_ctr->dtunit[15 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    16;
  pt_msg_format_ctr->dtunit[15 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_gwconf->gwConfig.bleConfig.bleDuplicatedDrop_ms;
  // "BOOLEAN DEFAULT false",
  // "timeStamp",
  pt_msg_format_ctr->dtunit[16 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    17;
  pt_msg_format_ctr->dtunit[16 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_gwconf->gwConfig.timeConfig.needTimestampForRecBeacon;
  // "INT DEFAULT 0",
  // "timeSetting",
  pt_msg_format_ctr->dtunit[17 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    18;
  pt_msg_format_ctr->dtunit[17 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_gwconf->gwConfig.timeConfig.timeSetting;
  // "TEXT DEFAULT ' '",
  // "timeNTPUrl",
  pt_msg_format_ctr->dtunit[18 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    19;
  snprintf(pt_msg_format_ctr->dtunit[18 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s",
           pt_gwconf->gwConfig.timeConfig.timeNtpUrl);
  // "INT DEFAULT 0",
  // "MQTTSecurityType",
  pt_msg_format_ctr->dtunit[19 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    20;
  pt_msg_format_ctr->dtunit[19 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_gwconf->gwConfig.mqttConfig.mqttSecurityType;
  // "TEXT DEFAULT ' '",
  // "MQTTHost",
  pt_msg_format_ctr->dtunit[20 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    21;
  snprintf(pt_msg_format_ctr->dtunit[20 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s",
           pt_gwconf->gwConfig.mqttConfig.MQTTHost);
  // "INT DEFAULT 0",
  // "MQTTPort",
  pt_msg_format_ctr->dtunit[21 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    22;
  pt_msg_format_ctr->dtunit[21 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint32 =
    pt_gwconf->gwConfig.mqttConfig.MQTTPort;
  // "BOOLEAN DEFAULT false",
  // "DHCPEnable",
  pt_msg_format_ctr->dtunit[22 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    23;
  pt_msg_format_ctr->dtunit[22 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.
  data_uint8 =
    pt_gwconf->gwConfig.ipConfig.DHCPEnable;
  // "TEXT DEFAULT ' '",
  // "staticIPAddr",
  pt_msg_format_ctr->dtunit[23 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    24;
  snprintf(pt_msg_format_ctr->dtunit[23 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%u.%u.%u.%u", \
           pt_gwconf->gwConfig.ipConfig.staticIPAddr.addr.addrArray[0], \
           pt_gwconf->gwConfig.ipConfig.staticIPAddr.addr.addrArray[1], \
           pt_gwconf->gwConfig.ipConfig.staticIPAddr.addr.addrArray[2], \
           pt_gwconf->gwConfig.ipConfig.staticIPAddr.addr.addrArray[3]);
  // "TEXT DEFAULT ' '",
  // "netMask",
  pt_msg_format_ctr->dtunit[24 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    25;
  snprintf(pt_msg_format_ctr->dtunit[24 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%u.%u.%u.%u", \
           pt_gwconf->gwConfig.ipConfig.netMask.addr.addrArray[0], \
           pt_gwconf->gwConfig.ipConfig.netMask.addr.addrArray[1], \
           pt_gwconf->gwConfig.ipConfig.netMask.addr.addrArray[2], \
           pt_gwconf->gwConfig.ipConfig.netMask.addr.addrArray[3]);
  // "TEXT DEFAULT ' '",
  // "DNSServer",
  pt_msg_format_ctr->dtunit[25 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    26;
  snprintf(pt_msg_format_ctr->dtunit[25 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%u.%u.%u.%u",
           pt_gwconf->gwConfig.ipConfig.dnsServer.addr.addrArray[0], \
           pt_gwconf->gwConfig.ipConfig.dnsServer.addr.addrArray[1], \
           pt_gwconf->gwConfig.ipConfig.dnsServer.addr.addrArray[2], \
           pt_gwconf->gwConfig.ipConfig.dnsServer.addr.addrArray[3]);
  // "TEXT DEFAULT ' '",
  // "gatewayAddr",
  pt_msg_format_ctr->dtunit[26 %
                            GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
    27;
  snprintf(pt_msg_format_ctr->dtunit[26 %
                                     GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%u.%u.%u.%u",
           pt_gwconf->gwConfig.ipConfig.gatewayAddr.addr.addrArray[0], \
           pt_gwconf->gwConfig.ipConfig.gatewayAddr.addr.addrArray[1], \
           pt_gwconf->gwConfig.ipConfig.gatewayAddr.addr.addrArray[2], \
           pt_gwconf->gwConfig.ipConfig.gatewayAddr.addr.addrArray[3]);
  return tpret;
}
/**
 * @brief Deliver bullet gateway configuration to table in a block manner.
 *        (i.e., this function would be blocked to wait for the response or
 *        a timeout happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_sensor_conf The gateway configuration to be delivered
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_deliver_gateway_conf_to_db(
  struct dbop_controller *pt_dbop_ctr,
  const gwConfig_t *pt_gwconf)
{
  app_state_t tpret = ST_OK;
  struct msg_format_controller *pt_msg_format_ctr = NULL;
  uint32_t kp_dtunit_sz = 0;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_gwconf))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  // Generate content
  to_fill_dtunit_array_for_gateway_conf_writing(pt_msg_format_ctr,
                                                pt_gwconf);
  // Generate message
  // Generate the header of message
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.table =
    GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.itemIdx = pt_gwconf->gwConfig.nameId;
  // The number of column
  kp_dtunit_sz = sizeof(GatewayConfig) / sizeof(GatewayConfig[0]);
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.dataNum = kp_dtunit_sz;
  // Generate the payload of message
  for(uint32_t i = 0; i < kp_dtunit_sz; i++)
  {
    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
    sqlite_update_item_param.
    updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
      pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].field;
    memcpy(
      &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
      param.sqlite_update_item_param.
      updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data,
      &pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data,
      sizeof(pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data));
  }

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);
  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);
  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX) {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n", MSGSEND_ERR_CNT_MAX);
          fflush(stdout);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = ST_ERR;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = ST_ERR;
    goto EXIT;
  }
  DBG_LOG_INFO(
    "GW-conf write response info: cmd = %d, cmdId = %d, success(0)/fail(1) = %d", \
    pt_dbop_resp_waiting_obj->resp_info.command, pt_dbop_resp_waiting_obj->resp_info.commandId, \
    pt_dbop_resp_waiting_obj->resp_info.response);
EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
/**
 * @brief Deliver only a field of an item in a given in a block manner.
 *        (i.e., this function would be blocked to wait for the response or
 *        a timeout happens). Note that this function would not check the
 *        validity of table_idx, item_idx, and kp_dtitem.
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param table_idx INPUT: The table
 * @param item_idx INPUT: The item
 * @param kp_dtitem INPUT: The field
 * @return @p ST_OK should be returned if no error happens.
 */
static app_state_t
app_db_deliver_one_field_to_db(
  struct dbop_controller *pt_dbop_ctr,
  uint32_t table_idx,
  uint32_t item_idx,
  gw_pro_data_t kp_dtitem)
{
  app_state_t tpret = ST_OK;
  struct msg_format_controller *pt_msg_format_ctr = NULL;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;

  if(NULL == pt_dbop_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  // Generate content
  memcpy(&pt_msg_format_ctr->dtunit[0],
         &kp_dtitem,
         sizeof(gw_pro_data_t));
  // Generate message
  // Generate the header of message
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.table =
    table_idx;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.itemIdx = item_idx;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.dataNum = 1;
  // Generate the payload of message
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.
  updateData[0].field = pt_msg_format_ctr->dtunit[0].field;
  memcpy(
    &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
    param.sqlite_update_item_param.
    updateData[0].data,
    &pt_msg_format_ctr->dtunit[0].data,
    sizeof(pt_msg_format_ctr->dtunit[0].data));

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);
  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);
  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX) {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n", MSGSEND_ERR_CNT_MAX);
          fflush(stdout);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = ST_ERR;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = ST_ERR;
    goto EXIT;
  }
  DBG_LOG_INFO(
    "One field write response info: cmd = %d, cmdId = %d, success(0)/fail(1) = %d", \
    pt_dbop_resp_waiting_obj->resp_info.command, pt_dbop_resp_waiting_obj->resp_info.commandId, \
    pt_dbop_resp_waiting_obj->resp_info.response);
EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
/**
 * @brief Deliver partial bullet gateway configuration (only including
 *        the last edit time) to table in a block manner. (i.e., this
 *        function would be blocked to wait for the response or a timeout
 *        happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param kp_editime INPUT: The last edit time
 * @return @p ST_OK should be returned if no error happens.
 */
static app_state_t
app_db_deliver_update_partial_gateway_conf_to_db(
  struct dbop_controller *pt_dbop_ctr,
  uint64_t kp_editime)
{
  app_state_t tpret = ST_OK;
  uint32_t *pt_idbuf = NULL;  // For index filter
  int32_t kp_int32 = 0;

  const uint32_t kp_tbidx = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
  gw_pro_data_t kp_dtitem = { 0 };

  gw_pro_command_sqlite_filter_param_t kp_filter = { 0 };

  if(NULL == pt_dbop_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  pt_idbuf = (uint32_t *)l_malloc(sizeof(uint32_t) *
                                  GW_PRO_MAX_DATA_ITEM_PER_OPERATION);
  if(NULL == pt_idbuf)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_idbuf, 0,
         sizeof(uint32_t) * GW_PRO_MAX_DATA_ITEM_PER_OPERATION);

  // To set up filter
  kp_filter.table = kp_tbidx;
  kp_filter.dataNum = 1;
  kp_filter.filterData[0].field = 1;
  kp_filter.filterData[0].condition = gw_pro_sqlite_cond_all;
  kp_int32 = app_db_fetch_filtered_index(pt_dbop_ctr,
                                         &kp_filter,
                                         sizeof(uint32_t) * GW_PRO_MAX_DATA_ITEM_PER_OPERATION,
                                         pt_idbuf);
  if(kp_int32 <= 0)
  {
    DBG_LOG_ERR("Failed to get nameId info from table %u, ret = %d",
                kp_tbidx, kp_int32);
    tpret = ST_ERR;
    goto EXIT;
  }

  // If found ...
  kp_dtitem.field = 8;
  kp_dtitem.data.data_uint64 = kp_editime;
  for(uint32_t i = 0; i < kp_int32; i++)
  {
    DBG_LOG_DEBUG("To update, field %u of table %u, to value %lu",
                 kp_dtitem.field,
                 kp_tbidx,
                 kp_dtitem.data.data_uint64);
    if(ST_OK !=
       app_db_deliver_one_field_to_db(pt_dbop_ctr, kp_tbidx,
                                      pt_idbuf[i %
                                               (sizeof(pt_idbuf) /
                                                sizeof(pt_idbuf[0]))],
                                      kp_dtitem))
    {
      DBG_LOG_ERR("Failed to deliver one field to DB");
      tpret = ST_ERR;
      goto EXIT;
    }
  }

EXIT:
  if(pt_idbuf)
  {
    l_free(pt_idbuf);
    pt_idbuf = NULL;
  }
  return tpret;
}
/**
 * @brief Fill the dtunit ( pointed by @p pt_msg_format_ctr ) with
 *        @p pt_dev_info to write to the database.
 */
static
app_state_t
to_fill_dtunit_array_for_dev_writing(
  struct msg_format_controller *pt_msg_format_ctr,
  const struct device_info *pt_dev_info)
{
  app_state_t tpret = ST_OK;

  if((NULL == pt_msg_format_ctr) || (NULL == pt_dev_info))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return ST_ERR;
  }
  // "INTEGER PRIMARY KEY AUTOINCREMENT",
  // "nameId",
  pt_msg_format_ctr->dtunit[0].field = 1;  // we start from 1
  pt_msg_format_ctr->dtunit[0].data.data_uint32 = pt_dev_info->nameid;
  // "TEXT DEFAULT ' '",
  // "name",
  pt_msg_format_ctr->dtunit[1].field = 2;
  snprintf(pt_msg_format_ctr->dtunit[1].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_dev_info->name);
  // "TEXT DEFAULT ' ' ",
  // "BLESensorName",
  pt_msg_format_ctr->dtunit[2].field = 3;
  snprintf(pt_msg_format_ctr->dtunit[2].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_dev_info->sensor_name);
  // "TEXT DEFAULT ' '",
  // "macAddr",
  pt_msg_format_ctr->dtunit[3].field = 4;
  snprintf(pt_msg_format_ctr->dtunit[3].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_dev_info->mac_addr);
  // "TEXT DEFAULT 'SKF'",
  // "manufacturer",
  pt_msg_format_ctr->dtunit[4].field = 5;
  snprintf(pt_msg_format_ctr->dtunit[4].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_dev_info->manufacturer);
  // "TEXT DEFAULT 'BulletSensor'",
  // "type",
  pt_msg_format_ctr->dtunit[5].field = 6;
  snprintf(pt_msg_format_ctr->dtunit[5].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_dev_info->sensor_type);
#if (ENABLE_MODBUS_FEATURE == 1)
  // "TEXT DEFAULT ''",
  // "description",
  pt_msg_format_ctr->dtunit[6].field = 7;
  snprintf(pt_msg_format_ctr->dtunit[6].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_dev_info->description);
#endif

  return tpret;
}
/**
 * @brief Deliver a child device to table in a block manner. (i.e., this
 *        function would be blocked to wait for the response or a timeout
 *        happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_dev_info The child device
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_deliver_dev_to_db(
  struct dbop_controller *pt_dbop_ctr,
  const struct device_info *pt_dev_info)
{
  app_state_t tpret = ST_OK;
  struct msg_format_controller *pt_msg_format_ctr = NULL;
  uint32_t kp_dtunit_sz = 0;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_dev_info))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  // Generate content
  to_fill_dtunit_array_for_dev_writing(pt_msg_format_ctr,
                                            pt_dev_info);
  // Generate message
  // Generate the header of message
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.table =
    GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.itemIdx = pt_dev_info->nameid;
  // The number of column
  kp_dtunit_sz = sizeof(DeviceList) / sizeof(DeviceList[0]);
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.dataNum = kp_dtunit_sz;
  // Generate the payload of message
  for(uint32_t i = 0; i < kp_dtunit_sz; i++)
  {
    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
    sqlite_update_item_param.
    updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
      pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].field;
    memcpy(
      &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
      param.sqlite_update_item_param.
      updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data,
      &pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data,
      sizeof(pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data));
  }

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);
  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);
  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt  = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX) {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n", MSGSEND_ERR_CNT_MAX);
          fflush(stdout);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = ST_ERR;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = ST_ERR;
    goto EXIT;
  }
  DBG_LOG_INFO(
    "Dev write response info: cmd = %d, cmdId = %d, success(0)/fail(1) = %d", \
    pt_dbop_resp_waiting_obj->resp_info.command, pt_dbop_resp_waiting_obj->resp_info.commandId, \
    pt_dbop_resp_waiting_obj->resp_info.response);
EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
/**
 * @brief Deliver bullet gateway configuration and Dev to table in a block
 *        manner. (i.e., this function would be blocked to wait for the
 *        response or a timeout happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_fpath The file gwConfig.json
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_deliver_gwconf_and_dev_to_db_from_json(
  struct dbop_controller *pt_dbop_ctr,
  const uint8_t *pt_fpath)
{
  app_state_t tpret = ST_OK;
  gwConfig_t *pt_gwcfg = NULL;
  sensorConfig_t tmpSensorConf;

  if((NULL == pt_dbop_ctr) || (NULL == pt_fpath))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Create gwConfig
  pt_gwcfg = (gwConfig_t *)l_malloc(sizeof(gwConfig_t));
  if(NULL == pt_gwcfg)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_gwcfg, 0, sizeof(gwConfig_t));
  // Assume GW_MAX_CHILDREN_NUM + 1 as unconfigured
  pt_gwcfg->gwConfig.childrenNumber = GW_MAX_CHILDREN_NUM + 1;
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  INIT_LIST_HEAD(&pt_gwcfg->gwConfig.childrenList);
#else /* GWCONFIG_CHILDREN_USING_LINKED_LIST == 1 */
#error \
  "So far, do not support that GWCONFIG_CHILDREN_USING_LINKED_LIST is not 1."
#endif /* GWCONFIG_CHILDREN_USING_LINKED_LIST != 1 */
  // To decode json string
  if(true != app_cjson_jsonstr_to_gwconfig(pt_gwcfg, pt_fpath))
  {
    DBG_LOG_ERR("Failed to generate gwConfig");
    tpret = ST_ERR;
    goto EXIT;
  }
  DBG_LOG_INFO("Generate gwConfig successfully!");

  // 1. If nameId of gateway existed, deliver gateway configuration to the
  // database; otherwise, just update the last edit time to the database
  DBG_LOG_DEBUG("The nameId of gateway: %u", pt_gwcfg->gwConfig.nameId);
  if(pt_gwcfg->gwConfig.nameId)
  {
    // 1.1 Update to the database
    // Clean all the items from the table before we write it
    app_db_clear_table(pt_dbop_ctr,
                       GW_PRO_TABLE_IDX_GW_CONFIG_TABLE);
    if(ST_OK != app_db_deliver_gateway_conf_to_db(pt_dbop_ctr, pt_gwcfg))
    {
      DBG_LOG_ERR("Failed to deliver gwConfig to table Gateway");
      tpret = ST_ERR;
      goto EXIT;
    }

    // 1.2 To update the global variable (@p g_sys_gwConfig_and_dev)
    // (except the children list)
    app_pro_gen_update_gwconf_except_childlist(
      sys_get_gwConfig_and_dev(), pt_gwcfg);

    // 1.3 To send to the report queue, where would apply the NTP and
    // ethernet configuration
    struct db_event_table_updated tp_db_event_tbl_update = { 0 };
    tp_db_event_tbl_update.table = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
    tp_db_event_tbl_update.ts = time(NULL);
    app_db_put_db_table_update_event_on_queue(
      app_db_get_report_handler(), &tp_db_event_tbl_update,
      DB_TBL_GATEWAY_UPDATED);
  }
  else
  {
    // Just update the last edit time to the database
    app_db_deliver_update_partial_gateway_conf_to_db(pt_dbop_ctr,
                                                     pt_gwcfg->lastEditTimeS);
  }

  // 2. Update the children list
  // 2.1 Update to the database
  if(pt_gwcfg->gwConfig.childrenNumber != GW_MAX_CHILDREN_NUM + 1)
  {
    DBG_LOG_DEBUG("To update table Dev");
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
    gwConfig_gwConfig_children_t *pt_child_ndtemp = NULL,
                                 *pt_cur_child_nd = NULL;
    struct device_info tp_devinfo = { 0 };

#if (ENABLE_MODBUS_FEATURE == 1)
    // TODO: collect the nameids before 'delete' dev table.
    //       do not delete the dev table directly here.
    if(ble_dev_fetch_flag) {
      // clear count, dev_list of 'former' case & copy 'current' into 'former' here
      ble_dev_cnt_former = ble_dev_cnt_current;
      memset(ble_dev_nameid_list_former, 0, sizeof(ble_dev_nameid_list_former));
      memcpy(ble_dev_nameid_list_former, ble_dev_nameid_list_current, sizeof(ble_dev_nameid_list_former));
      memset(ble_dev_nameid_list_current, 0, sizeof(ble_dev_nameid_list_current));
      ble_dev_cnt_current = 0;
      DBG_LOG_INFO("[shm ble update dev] former_cnt:%d, nameId list:\r\n", ble_dev_cnt_former);
      for(uint8_t i=0;i<ble_dev_cnt_former;i++) {
        printf("%d ", ble_dev_nameid_list_former[i]);
      }
      printf("\r\n");
    } else {  // Notes: update this flag here only!!! fetch the dev nameID list on first boot case
      ble_dev_fetch_flag = true;
    }
#else // do not delete dev table first for modbus case
    // Clean all the items from the table before we write it
    app_db_clear_table(pt_dbop_ctr,
                       GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE);
#endif
    // Write the children list to table Dev
    if(!list_empty(&pt_gwcfg->gwConfig.childrenList))
    {
      list_for_each_entry_safe(pt_cur_child_nd, pt_child_ndtemp,
                               &pt_gwcfg->gwConfig.childrenList,
                               childrenNode)
      {
        DBG_LOG_INFO("name=%s, nameId=%u",
                     pt_cur_child_nd->name,
                     pt_cur_child_nd->nameId);
        DBG_LOG_INFO("bleName=%s, mac=%.2X%.2X%.2x%.2X%.2X%.2X",
                     pt_cur_child_nd->bleName,
                     pt_cur_child_nd->macAddr.addr.addrArray[0],
                     pt_cur_child_nd->macAddr.addr.addrArray[1],
                     pt_cur_child_nd->macAddr.addr.addrArray[2],
                     pt_cur_child_nd->macAddr.addr.addrArray[3],
                     pt_cur_child_nd->macAddr.addr.addrArray[4],
                     pt_cur_child_nd->macAddr.addr.addrArray[5]);
#if (ENABLE_MODBUS_FEATURE == 1)
        DBG_LOG_INFO("type=%s, manufacture=%s, description=%s",
                     pt_cur_child_nd->type,
                     pt_cur_child_nd->manufacturer,
                     pt_cur_child_nd->description);
        ble_dev_nameid_list_current[ble_dev_cnt_current] = pt_cur_child_nd->nameId;
        ble_dev_cnt_current++;
#else
        DBG_LOG_INFO("type=%s, manufacture=%s",
                     pt_cur_child_nd->type,
                     pt_cur_child_nd->manufacturer);
#endif

        memset(&tp_devinfo, 0, sizeof(struct device_info));
        tp_devinfo.nameid = pt_cur_child_nd->nameId;
        snprintf(tp_devinfo.name, GW_PRO_MAX_STRING_LEN_BYTE, "%s",
                 pt_cur_child_nd->name);
        snprintf(tp_devinfo.sensor_name, GW_PRO_MAX_STRING_LEN_BYTE, "%s",
                 pt_cur_child_nd->bleName);
        snprintf(tp_devinfo.mac_addr, GW_PRO_MAX_STRING_LEN_BYTE,
                 "%.2X-%.2X-%.2X-%.2X-%.2X-%.2X", \
                 pt_cur_child_nd->macAddr.addr.addrArray[0], \
                 pt_cur_child_nd->macAddr.addr.addrArray[1], \
                 pt_cur_child_nd->macAddr.addr.addrArray[2], \
                 pt_cur_child_nd->macAddr.addr.addrArray[3], \
                 pt_cur_child_nd->macAddr.addr.addrArray[4], \
                 pt_cur_child_nd->macAddr.addr.addrArray[5]);
        snprintf(tp_devinfo.manufacturer, GW_PRO_MAX_STRING_LEN_BYTE, "%s",
                 pt_cur_child_nd->manufacturer);
        snprintf(tp_devinfo.sensor_type, GW_PRO_MAX_STRING_LEN_BYTE, "%s",
                 pt_cur_child_nd->type);
#if (ENABLE_MODBUS_FEATURE == 1)
        snprintf(tp_devinfo.description, GW_PRO_MAX_STRING_LEN_BYTE, "%s",
                 pt_cur_child_nd->description);
#endif

        if(app_db_deliver_dev_to_db(pt_dbop_ctr, &tp_devinfo))
        {
          DBG_LOG_ERR("Failed to deliver children list to table Dev");
          tpret = ST_ERR;
          goto EXIT;
        }

        tmpSensorConf.nameId = tp_devinfo.nameid;
        tmpSensorConf.macAddr.addr.addrArray[0] =
          pt_cur_child_nd->macAddr.addr.addrArray[0];
        tmpSensorConf.macAddr.addr.addrArray[1] =
          pt_cur_child_nd->macAddr.addr.addrArray[1];
        tmpSensorConf.macAddr.addr.addrArray[2] =
          pt_cur_child_nd->macAddr.addr.addrArray[2];
        tmpSensorConf.macAddr.addr.addrArray[3] =
          pt_cur_child_nd->macAddr.addr.addrArray[3];
        tmpSensorConf.macAddr.addr.addrArray[4] =
          pt_cur_child_nd->macAddr.addr.addrArray[4];
        tmpSensorConf.macAddr.addr.addrArray[5] =
          pt_cur_child_nd->macAddr.addr.addrArray[5];
        snprintf(tmpSensorConf.manufacturer, GW_PRO_MAX_STRING_LEN_BYTE,
                 "%s", pt_cur_child_nd->manufacturer);
        snprintf(tmpSensorConf.type, GW_PRO_MAX_STRING_LEN_BYTE, "%s",
                 pt_cur_child_nd->type);
        if(app_db_deliver_partial_sensor_conf_to_db(pt_dbop_ctr,
                                                    &tmpSensorConf))
        {
          DBG_LOG_ERR(
            "Failed to deliver children-list-related information to table Sensor");
          tpret = ST_ERR;
          goto EXIT;
        }
      }
#if (ENABLE_MODBUS_FEATURE == 1)
      DBG_LOG_INFO("[shm ble clean extra dev] start!\r\n");
      bool tmp_flag = false;
      gw_pro_command_sqlite_delete_item_param_t kp_del_param = { 0 };
      for(uint8_t i=0;i<ble_dev_cnt_former;i++) {
        tmp_flag = false;
        for(uint8_t j=0;j<ble_dev_cnt_current;j++) {
          if((0 != ble_dev_nameid_list_former[i]) && (ble_dev_nameid_list_former[i] == ble_dev_nameid_list_current[j])) {
            tmp_flag = true;
            break;
          }
        }
        if((!tmp_flag) && (0 != ble_dev_nameid_list_former[i])) { //nameID not in current list, delete it here
          kp_del_param.table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
          kp_del_param.itemIdx = ble_dev_nameid_list_former[i];
          kp_del_param.dataNum = 0;
          if (app_db_delete_item_in_db(pt_dbop_ctr, &kp_del_param)) {
            DBG_LOG_WARN("[shm ble] Failed to delete %d from Dev table\r\n", ble_dev_nameid_list_former[i]);
          }
          DBG_LOG_INFO("[shm ble] delete %d from Dev table!\r\n", ble_dev_nameid_list_former[i]);
          ble_dev_nameid_list_former[i] = 0;
        }
      }
      DBG_LOG_INFO("[shm ble clean extra dev] end!\r\n");
#endif

      // 2.2 To update the global variable (@p g_sys_gwConfig_and_dev)
      app_pro_gen_update_gwconf_childlist(
        sys_get_gwConfig_and_dev(), pt_gwcfg);
      // 2.3 To send to the report queue, where would update device list to
      // the ble white list which used by the ble process
      struct db_event_table_updated tp_db_event_tbl_update = { 0 };
      tp_db_event_tbl_update.table =
        GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
      tp_db_event_tbl_update.ts = time(NULL);
      app_db_put_db_table_update_event_on_queue(
        app_db_get_report_handler(), &tp_db_event_tbl_update,
        DB_TBL_DEV_UPDATED);
    }
#if (ENABLE_MODBUS_FEATURE == 1)
    else {  // children list is NULL
      ble_dev_cnt_current = 0;
      memset(ble_dev_nameid_list_current, 0, sizeof(ble_dev_nameid_list_current));
      // Clean all the items from the table before we write it
      app_db_clear_table(pt_dbop_ctr,
                        GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE);
    }
#endif
#else /* GWCONFIG_CHILDREN_USING_LINKED_LIST == 1 */
#error \
    "So far, do not support that GWCONFIG_CHILDREN_USING_LINKED_LIST is not 1."
#endif /* GWCONFIG_CHILDREN_USING_LINKED_LIST != 1 */
  }

EXIT:
  if(pt_gwcfg)
  {
    // Cannot release the children list because the children list has been
    // has been used by @p g_sys_gwConfig_and_dev
    // app_cjson_release_chilren_list(pt_gwcfg);

    l_free(pt_gwcfg);
    pt_gwcfg = NULL;
  }
  return tpret;
}
/**
 * @brief Fill the dtunit ( pointed by @p pt_msg_format_ctr ) with
 *        @p pt_sit_dtpkt (based on @p dt_type ) to write to the database.
 */
static app_state_t
to_fill_dtunit_array_for_sensordata_writing_sit(
  struct msg_format_controller *pt_msg_format_ctr,
  const struct sit_dtpacket *pt_sit_dtpkt,
  enum sit_data_type dt_type)
{
  app_state_t tpret = ST_OK;

  if((NULL == pt_msg_format_ctr) || (NULL == pt_sit_dtpkt))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return ST_ERR;
  }
  // "INTEGER PRIMARY KEY AUTOINCREMENT",
  // "SequenceNumber",
  // No needed
  // "INT DEFAULT 0",
  // "nameId",
  pt_msg_format_ctr->dtunit[0].field = 2;
  pt_msg_format_ctr->dtunit[0].data.data_uint32 =
    pt_sit_dtpkt->ad_data.nameId;
  // "TEXT DEFAULT ' '",
  // "BLESensorName",
  pt_msg_format_ctr->dtunit[1].field = 3;
  snprintf(pt_msg_format_ctr->dtunit[1].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE,
           "%s",
           pt_sit_dtpkt->ad_data.local_name);
  // "TEXT DEFAULT ' '",
  // "macAddr",
  pt_msg_format_ctr->dtunit[2].field = 4;
  snprintf(pt_msg_format_ctr->dtunit[2].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%.2X%.2X%.2X%.2X%.2X%.2X",
           pt_sit_dtpkt->addr[0], pt_sit_dtpkt->addr[1],
           pt_sit_dtpkt->addr[2], pt_sit_dtpkt->addr[3],
           pt_sit_dtpkt->addr[4], pt_sit_dtpkt->addr[5]);
  // "TEXT DEFAULT ' '",
  // "type",
  pt_msg_format_ctr->dtunit[3].field = 5;
  snprintf(pt_msg_format_ctr->dtunit[3].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE,
           "%s",
           "Insight-T");
  // "TEXT DEFAULT 'SKF'",
  // "manufacturer",
  pt_msg_format_ctr->dtunit[4].field = 6;
  snprintf(pt_msg_format_ctr->dtunit[4].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", "SKF");
  // "INT DEFAULT 0",
  // "measureseqno",
  pt_msg_format_ctr->dtunit[5].field = 7;
  pt_msg_format_ctr->dtunit[5].data.data_uint32 =
    pt_sit_dtpkt->ad_data.advcnt;
  // "INT DEFAULT 0",
  // "sampletime",
  pt_msg_format_ctr->dtunit[6].field = 8;
  pt_msg_format_ctr->dtunit[6].data.data_uint64 =
    pt_sit_dtpkt->tstamp;
  // "INT DEFAULT 0",
  // "ReceivedTimestamp",
  pt_msg_format_ctr->dtunit[7].field = 9;
  pt_msg_format_ctr->dtunit[7].data.data_uint64 =
    pt_sit_dtpkt->tstamp;
  // "INT DEFAULT 0",
  // "measurement",
  // No alarm
  pt_msg_format_ctr->dtunit[8].field = 10;
  pt_msg_format_ctr->dtunit[8].data.data_uint32 = 0;
  // "INT DEFAULT 0",
  // "DataType",
  pt_msg_format_ctr->dtunit[9].field = 11;
  if(SIT_DT_MTEMP == dt_type)
  {
    pt_msg_format_ctr->dtunit[9].data.data_uint32 =
      SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_CURRENT;
  }
  else if(SIT_DT_ETEMP == dt_type)
  {
    pt_msg_format_ctr->dtunit[9].data.data_uint32 =
      SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT;
  }
  else if(SIT_DT_RSSI == dt_type)
  {
    pt_msg_format_ctr->dtunit[9].data.data_uint32 =
      SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT;
  }
  else if(SIT_DT_VCC == dt_type)
  {
    // VCC
    pt_msg_format_ctr->dtunit[9].data.data_uint32 =
      SKFChina_Common_MeasurementType_VOLTAGE_CURRENT;
  }
  else
  {
    DBG_LOG_ERR("Unkown type!");
    return ST_ERR;
  }
  // "TEXT DEFAULT ' '",
  // "Format",
  pt_msg_format_ctr->dtunit[10].field = 12;
  snprintf(pt_msg_format_ctr->dtunit[10].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", DT_STORAGE_FORM_DIGIT);
  // "INT DEFAULT 0",
  // "range",
  pt_msg_format_ctr->dtunit[11].field = 13;
  pt_msg_format_ctr->dtunit[11].data.data_uint32 = 0;
  // "INT DEFAULT 0",
  // "unit",
  pt_msg_format_ctr->dtunit[12].field = 14;
  if(SIT_DT_MTEMP == dt_type)
  {
    pt_msg_format_ctr->dtunit[12].data.data_uint32 =
      SKFChina_Common_Unit_CDEGREE;
  }
  else if(SIT_DT_ETEMP == dt_type)
  {
    pt_msg_format_ctr->dtunit[12].data.data_uint32 =
      SKFChina_Common_Unit_CDEGREE;
  }
  else if(SIT_DT_RSSI == dt_type)
  {
    pt_msg_format_ctr->dtunit[12].data.data_uint32 =
      SKFChina_Common_Unit_DBM;
  }
  else if(SIT_DT_VCC == dt_type)
  {
    pt_msg_format_ctr->dtunit[12].data.data_uint32 =
      SKFChina_Common_Unit_VOLT;
  }
  else
  {
    DBG_LOG_ERR("Unkown type!");
    return ST_ERR;
  }
  // "INT DEFAULT 0",
  // "measurelengthsample",
  pt_msg_format_ctr->dtunit[13].field = 15;
  pt_msg_format_ctr->dtunit[13].data.data_uint32 = 1;
  // "INT DEFAULT 0",
  // "totaldatalengthsample",
  pt_msg_format_ctr->dtunit[14].field = 16;
  pt_msg_format_ctr->dtunit[14].data.data_uint32 = 1;
  // "INT DEFAULT 0",
  // "dimension",
  pt_msg_format_ctr->dtunit[15].field = 17;
  pt_msg_format_ctr->dtunit[15].data.data_uint32 = 0;
  // "INT DEFAULT 0",
  // "dataformat",
  pt_msg_format_ctr->dtunit[16].field = 18;
  if(SIT_DT_MTEMP == dt_type)
  {
    pt_msg_format_ctr->dtunit[16].data.data_uint32 =
      SKFChina_Common_Format_FORMAT_FLOAT32;
  }
  else if(SIT_DT_ETEMP == dt_type)
  {
    pt_msg_format_ctr->dtunit[16].data.data_uint32 =
      SKFChina_Common_Format_FORMAT_INT8;
  }
  else if(SIT_DT_RSSI == dt_type)
  {
    pt_msg_format_ctr->dtunit[16].data.data_uint32 =
      SKFChina_Common_Format_FORMAT_INT8;
  }
  else if(SIT_DT_VCC == dt_type)
  {
    pt_msg_format_ctr->dtunit[16].data.data_uint32 =
      SKFChina_Common_Format_FORMAT_FLOAT32;
  }
  else
  {
    DBG_LOG_ERR("Unknown type!");
    return ST_ERR;
  }
  // "INT DEFAULT 0",
  // "sampleperiods",
  pt_msg_format_ctr->dtunit[17].field = 19;
  pt_msg_format_ctr->dtunit[17].data.data_uint32 = 0;
  // "INT DEFAULT 0",
  // "encryp"
  pt_msg_format_ctr->dtunit[18].field = 20;
  pt_msg_format_ctr->dtunit[18].data.data_uint32 = 0;
  // "TEXT DEFAULT ' '",
  // "value",
  pt_msg_format_ctr->dtunit[19].field = 21;
  if(SIT_DT_MTEMP == dt_type)
  {
    pt_msg_format_ctr->dtunit[19].data.data_float =
      pt_sit_dtpkt->ad_data.m_tempval;
  }
  else if(SIT_DT_ETEMP == dt_type)
  {
    pt_msg_format_ctr->dtunit[19].data.data_int8 =
      pt_sit_dtpkt->ad_data.sensor_tpval;
  }
  else if(SIT_DT_RSSI == dt_type)
  {
    pt_msg_format_ctr->dtunit[19].data.data_int8 =
      pt_sit_dtpkt->rssi;
  }
  else if(SIT_DT_VCC == dt_type)
  {
    pt_msg_format_ctr->dtunit[19].data.data_float =
      pt_sit_dtpkt->ad_data.vcc_val;
  }
  else
  {
    DBG_LOG_ERR("Unkown type!");
    return ST_ERR;
  }
  // "REAL DEFAULT 0.0",
  // "sample_rate",
  pt_msg_format_ctr->dtunit[20].field = 22;
  pt_msg_format_ctr->dtunit[20].data.data_float = 0;
  // "INT DEFAULT 0",
  // "product",
  pt_msg_format_ctr->dtunit[21].field = 23;
  pt_msg_format_ctr->dtunit[21].data.data_uint32 =
    SKFChina_Common_ProductType_INSIGHT_T;
  // "INT DEFAULT 0",
  // "sensor",
  pt_msg_format_ctr->dtunit[22].field = 24;
  pt_msg_format_ctr->dtunit[22].data.data_uint32 = 0;

  return tpret;
}
/**
 * @brief Deliver sensor data (of Insight-T) to table in a non-block
 *        manner (i.e., this function would try to send IPC message to
 *        process sql_op, return immediately no matter successful or
 *        failure).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_sit_dtpkt INPUT: Data of SIT
 * @param dt_type INPUT: Data type of SIT
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_deliver_sensordata_sit_to_db_noblock(
  struct dbop_controller *pt_dbop_ctr,
  const struct sit_dtpacket *pt_sit_dtpkt,
  enum sit_data_type dt_type)
{
  app_state_t tpret = ST_OK;
  struct msg_format_controller *pt_msg_format_ctr = NULL;
  uint32_t kp_dtunit_sz = 0;
  struct database_operation_controller *pt_database_op_ctr = NULL;
  struct msqid_ds _ipc_ctrl_msg;

  if((NULL == pt_dbop_ctr) || (NULL == pt_sit_dtpkt))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  // Generate content
  if(ST_OK !=
     to_fill_dtunit_array_for_sensordata_writing_sit(pt_msg_format_ctr,
                                                     pt_sit_dtpkt,
                                                     dt_type))
  {
    tpret = ST_ERR;
    goto EXIT;
  }

  // Generate message
  // Generate the header of message
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.table =
    GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.itemIdx = 0;
  // the column number of sensordata (except "SequenceNumber" and "Sent")
  kp_dtunit_sz = sizeof(SensorData) / sizeof(SensorData[0]) - 2;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.dataNum = kp_dtunit_sz;
  // Generate the payload of message
  for(uint32_t i = 0; i < kp_dtunit_sz; i++)
  {
    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
    sqlite_update_item_param.
    updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
      pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].field;
    memcpy(
      &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
      param.sqlite_update_item_param.
      updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data,
      &pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data,
      sizeof(pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data));
  }

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);

  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);
  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
#if (0) // Just for debug: to provide more information to analysis why msgsnd failed ...
  // Update used volume of the IPC queue for debug
  if(msgctl(pt_database_op_ctr->msgid_to_db, IPC_STAT, &_ipc_ctrl_msg) == -1)
  {
    DBG_LOG_ERR("Failed to get stats of IPC queue");
    return -1;
  }
  DBG_LOG_DEBUG(
    "The used volume of the IPC queue (%d), type %d: %lu B\n",
    pt_database_op_ctr->msgid_to_db,
    dt_type,
    _ipc_ctrl_msg.__msg_cbytes);
#endif
  if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
            sizeof(gw_pro_message_header_t), IPC_NOWAIT) != 0)
  {
    DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
    tpret = ST_ERR;
    goto EXIT;
  }
EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  return tpret;
}
/**
 * @brief Deliver sensor data to table in a block manner. (i.e., this
 *        function would be blocked to wait for the response or a timeout
 *        happens). Note that no key field is required when insert data to
 *        table SensorData and the key field would increase and be filled
 *        automatically.
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_dtitem INPUT: The data item to be delivered
 * @return @p ST_OK should be returned if no error happens.
 */
static app_state_t
app_db_deliver_sensordata_to_db(
  struct dbop_controller *pt_dbop_ctr,
  struct data_item *pt_dtitem)
{
  app_state_t tpret = ST_OK;
  struct msg_format_controller *pt_msg_format_ctr = NULL;
  uint32_t kp_dtunit_sz = 0;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;
	enum sensortype kp_stype = SS_TYPE_UNKNOWN;
	uint32_t hld_dimen_info = 0;

  if((NULL == pt_dbop_ctr) || (NULL == pt_dtitem))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  kp_stype = app_pro_gen_get_active_sensor_type_v2();

  // Generate content
#if (1)
	hld_dimen_info = pt_dtitem->dimension; // to hold the dimension info, so we can recover it.
	
  if(pt_dtitem->data_type ==
     SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV)
  {
    // If check vel_ov, then check the three types of waveform first ...
    pt_dtitem->data_type =
      SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE;
    pt_dtitem->dimension = SKFChina_Common_Dimension_DIMENSION_X;
    if(is_dtitem_in_db(pt_dtitem,
                       pt_dtitem->sensor_mac))
    {
      DBG_LOG_INFO("Data item m_ts=%lu, dt-type=%d, mac-str =%s is in DB",
                   pt_dtitem->meas_ts,
                   pt_dtitem->data_type,
                   pt_dtitem->sensor_mac);
      dtitem_acc_wave_exist_fgx = true;
    }
    if (SS_TYPE_PREDICTP == kp_stype)
    {
      // For InsightPredictPro
      pt_dtitem->data_type =
      SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE;
      pt_dtitem->dimension = SKFChina_Common_Dimension_DIMENSION_Y;
      if(is_dtitem_in_db(pt_dtitem,
                        pt_dtitem->sensor_mac))
      {
        DBG_LOG_INFO("Data item m_ts=%lu, dt-type=%d, mac-str =%s is in DB",
                    pt_dtitem->meas_ts,
                    pt_dtitem->data_type,
                    pt_dtitem->sensor_mac);
        dtitem_acc_wave_exist_fgy = true;
      }
      pt_dtitem->data_type =
        SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE;
      pt_dtitem->dimension = SKFChina_Common_Dimension_DIMENSION_Z;
      if(is_dtitem_in_db(pt_dtitem,
                        pt_dtitem->sensor_mac))
      {
        DBG_LOG_INFO("Data item m_ts=%lu, dt-type=%d, mac-str =%s is in DB",
                    pt_dtitem->meas_ts,
                    pt_dtitem->data_type,
                    pt_dtitem->sensor_mac);
        dtitem_acc_wave_exist_fgz = true;
      }
    }
#if (SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE == 1)
    pt_dtitem->data_type =
      SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE;
    pt_dtitem->dimension = SKFChina_Common_Dimension_DIMENSION_X;
    if(is_dtitem_in_db(pt_dtitem,
                       pt_dtitem->sensor_mac))
    {
      DBG_LOG_INFO("Data item m_ts=%lu, dt-type=%d, mac-str =%s is in DB",
                   pt_dtitem->meas_ts,
                   pt_dtitem->data_type,
                   pt_dtitem->sensor_mac);
      dtitem_env_wave_exist_fg = true;
    }
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE == 1 */
#if (SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE == 1)
    pt_dtitem->data_type =
      SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE;
    pt_dtitem->dimension = SKFChina_Common_Dimension_DIMENSION_X;
    if(is_dtitem_in_db(pt_dtitem,
                       pt_dtitem->sensor_mac))
    {
      DBG_LOG_INFO("Data item m_ts=%lu, dt-type=%d, mac-str =%s is in DB",
                   pt_dtitem->meas_ts,
                   pt_dtitem->data_type,
                   pt_dtitem->sensor_mac);

      dtitem_vel_wave_exist_fg = true;
    }
#endif /* SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE == 1 */
    // Recover to vel_ov
    pt_dtitem->data_type =
      SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV;
	pt_dtitem->dimension = hld_dimen_info;
  }
	// To check if the data item is in DB or not, before we try to write
#if 0 // Deprecated
  if(is_dtitem_in_db(pt_dtitem,
    pt_dtitem->sensor_mac))
  {
    DBG_LOG_INFO("Data item m_ts=%lu, dt-type=%d, mac-str =%s is in DB",
                  pt_dtitem->meas_ts,
                  pt_dtitem->data_type,
                  pt_dtitem->sensor_mac);
    if(pt_dtitem->data_type ==
      SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV)
    {
      dtitem_exist_fg = true;
    }
    goto EXIT;
  }
#endif
#else
#warning \
  "To uncomment the branch above if the existence check before data insertion is not required!"
#endif

  // To get nameId, BLE MAC address, BLE name, sensor type, and
  // manufacturer
#if (1)
  struct device_info kp_dev_info = { 0 };
  if(ST_OK !=
     app_db_fetch_a_device_info_from_db_based_on_macaddr(pt_dbop_ctr,
                                                         &kp_dev_info,
                                                         pt_dtitem->
                                                         sensor_mac))
  {
    DBG_LOG_ERR("Failed to read from table Dev, MAC: %s",
                pt_dtitem->sensor_mac);
  }
  else
  {
    DBG_LOG_DEBUG("To update data item according to the query from Dev");
    pt_dtitem->name_id = kp_dev_info.nameid;
    snprintf(pt_dtitem->sensor_name,
             sizeof(pt_dtitem->sensor_name),
             "%s",
             kp_dev_info.sensor_name);
    snprintf(pt_dtitem->sensor_mac,
             sizeof(pt_dtitem->sensor_mac),
             "%s",
             kp_dev_info.mac_addr);
    snprintf(pt_dtitem->sensor_type,
             sizeof(pt_dtitem->sensor_type),
             "%s",
             kp_dev_info.sensor_type);
    snprintf(pt_dtitem->manufacturer,
             sizeof(pt_dtitem->manufacturer),
             "%s",
             kp_dev_info.manufacturer);
  }
#endif
  // "INTEGER PRIMARY KEY AUTOINCREMENT",
  // "SequenceNumber",
  // No needed
  // "INT DEFAULT 0",
  // "nameId",
  pt_msg_format_ctr->dtunit[0].field = 2;
  pt_msg_format_ctr->dtunit[0].data.data_uint32 = pt_dtitem->name_id;
  // "TEXT DEFAULT ' '",
  // "BLESensorName",
  pt_msg_format_ctr->dtunit[1].field = 3;
  snprintf(pt_msg_format_ctr->dtunit[1].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE,
           "%s",
           pt_dtitem->sensor_name);
  // "TEXT DEFAULT ' '",
  // "macAddr",
  pt_msg_format_ctr->dtunit[2].field = 4;
  snprintf(pt_msg_format_ctr->dtunit[2].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE,
           "%s",
           pt_dtitem->sensor_mac);
  // "TEXT DEFAULT ' '",
  // "type",
  pt_msg_format_ctr->dtunit[3].field = 5;
  snprintf(pt_msg_format_ctr->dtunit[3].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE,
           "%s",
           pt_dtitem->sensor_type);
  // "TEXT DEFAULT 'SKF'",
  // "manufacturer",
  pt_msg_format_ctr->dtunit[4].field = 6;
  snprintf(pt_msg_format_ctr->dtunit[4].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE,
           "%s",
           pt_dtitem->manufacturer);
  // "INT DEFAULT 0",
  // "measureseqno",
  pt_msg_format_ctr->dtunit[5].field = 7;
  pt_msg_format_ctr->dtunit[5].data.data_uint32 = pt_dtitem->meas_id;
  // "INT DEFAULT 0",
  // "sampletime",
  pt_msg_format_ctr->dtunit[6].field = 8;
  pt_msg_format_ctr->dtunit[6].data.data_uint64 = pt_dtitem->meas_ts;
  // "INT DEFAULT 0",
  // "ReceivedTimestamp",
  pt_msg_format_ctr->dtunit[7].field = 9;
  pt_msg_format_ctr->dtunit[7].data.data_uint64 = pt_dtitem->received_ts;
  // "INT DEFAULT 0",
  // "measurement",
  pt_msg_format_ctr->dtunit[8].field = 10;
  pt_msg_format_ctr->dtunit[8].data.data_uint32 = pt_dtitem->alarm;
  // "INT DEFAULT 0",
  // "DataType",
  pt_msg_format_ctr->dtunit[9].field = 11;
  pt_msg_format_ctr->dtunit[9].data.data_uint32 = pt_dtitem->data_type;
  // "TEXT DEFAULT ' '",
  // "Format",
  pt_msg_format_ctr->dtunit[10].field = 12;
  snprintf(pt_msg_format_ctr->dtunit[10].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_dtitem->format);
  // "INT DEFAULT 0",
  // "range",
  pt_msg_format_ctr->dtunit[11].field = 13;
  pt_msg_format_ctr->dtunit[11].data.data_uint32 = pt_dtitem->range;
  // "INT DEFAULT 0",
  // "unit",
  pt_msg_format_ctr->dtunit[12].field = 14;
  pt_msg_format_ctr->dtunit[12].data.data_uint32 = pt_dtitem->unit;  // g
  // "INT DEFAULT 0",
  // "measurelengthsample",
  pt_msg_format_ctr->dtunit[13].field = 15;
  pt_msg_format_ctr->dtunit[13].data.data_uint32 =
    pt_dtitem->measure_length_sample;
  // "INT DEFAULT 0",
  // "totaldatalengthsample",
  pt_msg_format_ctr->dtunit[14].field = 16;
  pt_msg_format_ctr->dtunit[14].data.data_uint32 =
    pt_dtitem->total_data_length_sample;
  // "INT DEFAULT 0",
  // "dimension",
  pt_msg_format_ctr->dtunit[15].field = 17;
  pt_msg_format_ctr->dtunit[15].data.data_uint32 = pt_dtitem->dimension;
  // "INT DEFAULT 0",
  // "dataformat",
  pt_msg_format_ctr->dtunit[16].field = 18;
  pt_msg_format_ctr->dtunit[16].data.data_uint32 = pt_dtitem->data_format;
  // "INT DEFAULT 0",
  // "sampleperiods",
  pt_msg_format_ctr->dtunit[17].field = 19;
  pt_msg_format_ctr->dtunit[17].data.data_uint32 =
    pt_dtitem->sample_period_s;
  // "INT DEFAULT 0",
  // "encryp"
  pt_msg_format_ctr->dtunit[18].field = 20;
  pt_msg_format_ctr->dtunit[18].data.data_uint32 = pt_dtitem->encryp;
  // "TEXT DEFAULT ' '",
  // "value",
  pt_msg_format_ctr->dtunit[19].field = 21;
  if(is_mtype_to_file(pt_dtitem->data_type))
  {
    snprintf(pt_msg_format_ctr->dtunit[19].data.data_char,
             GW_PRO_MAX_STRING_LEN_BYTE,
             "%s",
             &pt_dtitem->value.fpath.fpath[2]);  // To remove "./" before
                                                 // file name
  }
  else
  {
    memcpy(&pt_msg_format_ctr->dtunit[19].data.data_int64,
           &pt_dtitem->value.valu64,
           sizeof(uint64_t));
  }
  // "REAL DEFAULT 0.0",
  // "sample_rate",
  pt_msg_format_ctr->dtunit[20].field = 22;
  pt_msg_format_ctr->dtunit[20].data.data_float =
    pt_dtitem->sample_rate_hz;
  // "INT DEFAULT 0",
  // "product",
  pt_msg_format_ctr->dtunit[21].field = 23;
  pt_msg_format_ctr->dtunit[21].data.data_uint32 = pt_dtitem->product_type;
  // "INT DEFAULT 0",
  // "sensor",
  pt_msg_format_ctr->dtunit[22].field = 24;
  pt_msg_format_ctr->dtunit[22].data.data_uint32 =
    pt_dtitem->froto_sensor_type;

  // To record the last measurement timestamp
  // FIXME: move to a proper place
  if((pt_dtitem->data_type ==
      SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV) ||
     (pt_dtitem->data_type ==
      SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV) ||
     (pt_dtitem->data_type ==
      SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV))
  {
    lastRegularSensingTs = pt_dtitem->meas_ts;
    lastRegularSensingMeasID = pt_dtitem->meas_id;
  }

  // Generate message
  // Generate the header of message
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.table =
    GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.itemIdx = 0;
  // the column number of sensordata (except "SequenceNumber" and "Sent")
  kp_dtunit_sz = sizeof(SensorData) / sizeof(SensorData[0]) - 2;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.dataNum = kp_dtunit_sz;
  // Generate the payload of message
  for(uint32_t i = 0; i < kp_dtunit_sz; i++)
  {
    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
    sqlite_update_item_param.
    updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field =
      pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].field;
    memcpy(
      &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
      param.sqlite_update_item_param.
      updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data,
      &pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data,
      sizeof(pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data));
  }

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);

  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);

  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX) {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n", MSGSEND_ERR_CNT_MAX);
          fflush(stdout);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = ST_ERR;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = ST_ERR;
    goto EXIT;
  }
  DBG_LOG_INFO(
    "Sensor-data write response info: cmd = %d, cmdId = %d, success(0)/fail(1) = %d", \
    pt_dbop_resp_waiting_obj->resp_info.command, pt_dbop_resp_waiting_obj->resp_info.commandId, \
    pt_dbop_resp_waiting_obj->resp_info.response);
EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
/**
 * @brief To delete a record in database in a block manner.
 *        (i.e., this function would be blocked to wait for the response or
 *        a timeout happens).
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param pt_del_param The item info to be deleted
 * @return @p ST_OK should be returned if no error happens.
 */
app_state_t
app_db_delete_item_in_db(
  struct dbop_controller *pt_dbop_ctr,
  gw_pro_command_sqlite_delete_item_param_t *pt_del_param)
{
  app_state_t tpret = ST_OK;
  struct msg_format_controller *pt_msg_format_ctr = NULL;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
  struct database_operation_controller *pt_database_op_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_del_param))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Initialize memory for database IPC message
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  // Generate content (i.e., copy content from pt_del_param)
  memcpy(
    &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_delete_item_param,
    pt_del_param,
    sizeof(gw_pro_command_sqlite_delete_item_param_t));

  // Generate message
  // Generate the header of message
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_delete_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;

  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);
  // To fill the IPC message
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  // Output the header (just for debug)
  to_dump_gw_pro_msg_header(&pt_msg_format_ctr->msginfo.header);
  // To create a waiting object
  pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)l_malloc(sizeof(struct dbop_resp_waiting));
  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_dbop_resp_waiting_obj, 0, sizeof(struct dbop_resp_waiting));
  app_db_init_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command,
                                    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId);
  // To put the pt_dbop_resp_waiting_obj on waiting queue
  app_db_put_dbop_waiting_obj_on_queue(pt_dbop_ctr,
                                       pt_dbop_resp_waiting_obj);

  // To sent message to the process sql_op
  pt_database_op_ctr = app_db_get_database_op_ctr();
  g_msgsnd_err_cnt = 0;
  while(1)
  {
    if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
              sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
    {
      if(EAGAIN == errno)
      {
        if(g_msgsnd_err_cnt++ > MSGSEND_ERR_CNT_MAX) {
          DBG_LOG_ERR("msgsnd failed for %d times, restart process...\r\n", MSGSEND_ERR_CNT_MAX);
          fflush(stdout);
          system(MSGSEND_ERR_CMD);
        }
        // Failure since there's no space in the pipe
        DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
        // Retry until success ...
        usleep(5000);
      }
      else
      {
        tpret = ST_ERR;
        // Unknown error
        DBG_LOG_ERR("msgsnd failure, for %s\r\n", strerror(errno));
        app_db_remove_dbop_waiting_obj_from_queue(pt_dbop_ctr,
                                                  pt_dbop_resp_waiting_obj);
        goto EXIT;
      }
    }
    else
    {
      // Do nothing once msgsnd is successful ...
      break;
    }
  }

  // To wait for response (a signal sent to @p cond_resp_received)
  pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
  while(1)
  {
    if(EV_DB_OP_DEFAULT == pt_dbop_resp_waiting_obj->db_resp_event)
    {
      pthread_cond_wait(&pt_dbop_resp_waiting_obj->cond_resp_received,
                        &pt_dbop_resp_waiting_obj->mtx);
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
  // Received or timeout
  if(EV_DB_OP_RESP_RECV != pt_dbop_resp_waiting_obj->db_resp_event)
  {
    DBG_LOG_ERR("Received db_resp_event is %d",
                pt_dbop_resp_waiting_obj->db_resp_event);
    tpret = ST_ERR;
    goto EXIT;
  }
  DBG_LOG_INFO(
    "GW-conf write response info: cmd = %d, cmdId = %d, success(0)/fail(1) = %d", \
    pt_dbop_resp_waiting_obj->resp_info.command, pt_dbop_resp_waiting_obj->resp_info.commandId, \
    pt_dbop_resp_waiting_obj->resp_info.response);
EXIT:
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  if(pt_dbop_resp_waiting_obj)
  {
    app_db_deinit_dbop_resp_waiting_obj(pt_dbop_resp_waiting_obj);
    l_free(pt_dbop_resp_waiting_obj);
    pt_dbop_resp_waiting_obj = NULL;
  }
  return tpret;
}
/**
 * @brief To clear all the items in the table ( @p kp_tbidx ).
 * @return @p ST_OK is expected to be returned when things go well
 */
app_state_t
app_db_clear_table(
  struct dbop_controller *pt_dbop_ctr,
  const uint32_t kp_tbidx)
{
  app_state_t tpret = ST_OK;
  uint32_t *pt_idbuf = NULL;  // For index filter
  int32_t kp_int32 = 0;
  gw_pro_command_sqlite_filter_param_t kp_filter = { 0 };
  gw_pro_command_sqlite_delete_item_param_t kp_del_param = { 0 };

  if(NULL == pt_dbop_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  pt_idbuf = (uint32_t *)l_malloc(sizeof(uint32_t) *
                                  GW_PRO_MAX_DATA_ITEM_PER_OPERATION);
  if(NULL == pt_idbuf)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_idbuf, 0,
         sizeof(uint32_t) * GW_PRO_MAX_DATA_ITEM_PER_OPERATION);

  // To set up filter
  kp_filter.table = kp_tbidx;
  kp_filter.dataNum = 1;
  kp_filter.filterData[0].field = 1;
  kp_filter.filterData[0].condition = gw_pro_sqlite_cond_all;
  kp_int32 = app_db_fetch_filtered_index(pt_dbop_ctr,
                                         &kp_filter,
                                         sizeof(uint32_t) * GW_PRO_MAX_DATA_ITEM_PER_OPERATION,
                                         pt_idbuf);
  if(kp_int32 < 0)
  {
    DBG_LOG_ERR("Failed to get nameId info from DB");
    tpret = ST_ERR;
    goto EXIT;
  }

  // To delete data item
  for(uint32_t i = 0; i < kp_int32; i++)
  {
    memset(&kp_del_param, 0,
           sizeof(gw_pro_command_sqlite_delete_item_param_t));
    kp_del_param.table = kp_tbidx;
    kp_del_param.itemIdx = pt_idbuf[i];
    kp_del_param.dataNum = 0;
    DBG_LOG_INFO("To delete %d from table %d", kp_del_param.itemIdx,
                 kp_del_param.table);
    if(ST_OK != app_db_delete_item_in_db(pt_dbop_ctr, &kp_del_param))
    {
      DBG_LOG_ERR("Failed to delete data from DB");
      tpret = ST_ERR;
      goto EXIT;
    }
  }

EXIT:
  if(pt_idbuf)
  {
    l_free(pt_idbuf);
    pt_idbuf = NULL;
  }
  return tpret;
}
/**
 * @brief remove extra files in /home/root/down directory
 */
static void to_cleanup_extra_files()
{
  app_state_t tpret = ST_OK;
  char command[] = "echo -e \'#!/bin/bash\nCUR_DIR=/var/lib/skf-gateway/update\nif [ -d ${CUR_DIR} ]; then\n\tsub_dirs=$(find ${CUR_DIR} -type d)\n\tfor element in ${sub_dirs[*]}; do\n\t\tif [ $(find $element -maxdepth 1 -type f -exec basename {} \\; | wc -l) -gt 2 ]; then\n\t\t\tcd $element\n\t\t\techo $element\n\t\t\ttmp_files=$(ls -t | tail -n +3)\n\t\t\tfor each_file in ${tmp_files[*]}; do\n\t\t\t\tif [ ! $(test -d $each_file) ]; then\n\t\t\t\t\techo \"removing $each_file\"\n\t\t\t\t\trm -f $each_file\n\t\t\t\tfi\n\t\t\tdone\n\t\tfi\n\tdone\nfi\n\' > /home/root/tmp/cleanup.sh";
  pid_t status;
  if ((access("/var/lib/skf-gateway/update", 0)) == -1) {
    mkdir("/var/lib/skf-gateway/update", S_IRWXU);
  }
  if ((access("/home/root/tmp", 0)) == -1) {
    mkdir("/home/root/tmp", S_IRWXU);
  }

  status = system(command);
  if(WIFEXITED(status))
  {
    if(0 == WEXITSTATUS(status))
    {
      DBG_LOG_INFO("Create cleanup.sh successfully");
    }
    else
    {
      DBG_LOG_INFO(
        "Failed to create cleanup.sh, %d", WEXITSTATUS(
          status));
    }
  }
  else
  {
    DBG_LOG_INFO("Failed to do file cleanup, %d", WEXITSTATUS(status));
    tpret = ST_ERR;
    goto EXIT;
  }

  memset(command, 0, sizeof(command));
  snprintf(command,
           sizeof(command),"%s",
           "chmod 0700 /run/skf-gateway/cleanup.sh;/run/skf-gateway/cleanup.sh"
           );
  DBG_LOG_DEBUG("cmd: %s\r\n", command);
  status = system(command);
  if(WIFEXITED(status))
  {
    if(0 == WEXITSTATUS(status))
    {
      DBG_LOG_INFO("Extra files cleanup successfully");
    }
    else
    {
      DBG_LOG_INFO(
        "Failed to cleanup extra files, %d", WEXITSTATUS(
          status));
    }
  }
  else
  {
    DBG_LOG_INFO("Failed to clean extra files, %d", WEXITSTATUS(status));
    tpret = ST_ERR;
    goto EXIT;
  }
EXIT:
  return tpret;
}
/**
 * @brief Do the database cleanup: cleanup the database and remove the
 *        corresponding files
 * @param pt_dbop_ctr INPUT: Database operation controller (including a
 *                    queue for timeout or receiving IPC messages)
 * @param absTimeBefore_s INPUT: To cleanup the item whose
 *                        receivedtimestamp <= @p absTimeBefore_s
 * @param timeBefore_s INPUT: To cleanup corresponding files which was
 *                     modified @p timeBefore_s ago
 */
app_state_t
app_db_gabage_cleanup(
  struct dbop_controller *pt_dbop_ctr,
  time_t absTimeBefore_s,
  time_t timeBefore_s)
{
  app_state_t tpret = ST_OK;
  gw_pro_command_sqlite_delete_item_param_t kp_del_param = { 0 };

  if(NULL == pt_dbop_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // 1. Clean up the database
  // To set up the delete condition
  kp_del_param.table = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
  kp_del_param.dataNum = 1;

  // receivedtimestamp
  kp_del_param.deleteData[0 %
                          GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 9;
  kp_del_param.deleteData[0 %
                          GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_value_range;
  kp_del_param.deleteData[0 %
                          GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condParam.
  range_uint64.max =
    absTimeBefore_s;
  kp_del_param.deleteData[0 %
                          GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condParam.
  range_uint64.min =
    0;
  DBG_LOG_INFO("To clean items before %d from table %d", absTimeBefore_s,
               kp_del_param.table);
  if(ST_OK != app_db_delete_item_in_db(pt_dbop_ctr, &kp_del_param))
  {
    DBG_LOG_ERR("Failed to delete data from DB");
    tpret = ST_ERR;
    goto EXIT;
  }

  // 2. Clean up the corresponding files
  char command[100];
  pid_t status;

  memset(command, 0, sizeof(command));
  snprintf(command,
           sizeof(command),
           "find /var/lib/skf-gateway/data/ -mtime +%d -type f -name \"wave_*\" -exec rm -rf {} \\;",
           timeBefore_s / 3600 / 24 + 1);  // Never "-mtime +0"
  DBG_LOG_INFO("cmd: %s\r\n", command);
  status = system(command);
  if(WIFEXITED(status))
  {
    if(0 == WEXITSTATUS(status))
    {
      DBG_LOG_INFO("File cleanup successfully");
    }
    else
    {
      DBG_LOG_INFO(
        "Failed to do file cleanup (maybe there's nothing to clean), %d", WEXITSTATUS(
          status));
    }
  }
  else
  {
    DBG_LOG_INFO("Failed to do file cleanup, %d", WEXITSTATUS(status));
    tpret = ST_ERR;
    goto EXIT;
  }

  memset(command, 0, sizeof(command));
  snprintf(command,
           sizeof(command),
           "find /var/lib/skf-gateway/data/ -mtime +%d -type f -name \"alarm_code_*\" -exec rm -rf {} \\;",
           timeBefore_s / 3600 / 24 + 1);  // Never "-mtime +0"
  DBG_LOG_INFO("cmd: %s\r\n", command);
  status = system(command);
  if(WIFEXITED(status))
  {
    if(0 == WEXITSTATUS(status))
    {
      DBG_LOG_INFO("File cleanup successfully");
    }
    else
    {
      DBG_LOG_INFO(
        "Failed to do file cleanup (maybe there's nothing to clean), %d", WEXITSTATUS(
          status));
    }
  }
  else
  {
    DBG_LOG_INFO("Failed to do file cleanup, %d", WEXITSTATUS(status));
    tpret = ST_ERR;
    goto EXIT;
  }
  // 3. Clean up extra files in down dir
  to_cleanup_extra_files();
EXIT:
  return tpret;
}
/**
 * @brief Thread to deal with modification of database.
 */
void *
thandler_db_modification_report(void *pt_para)
{
  struct db_report_handler *pt_db_report_handler =
    app_db_get_report_handler();
  struct db_report_node *pt_report_node = NULL;

  app_db_init_db_mod_report_timer_ctr();
  while(1)
  {
    DBG_LOG_DEBUG("Waiting for a database modification message");

    // Wait until the message has been received
    pthread_mutex_lock(&pt_db_report_handler->mtx);
    while(1)
    {
      if(0 == l_queue_length(pt_db_report_handler->pt_report_queue))
      {
        pthread_cond_wait(&pt_db_report_handler->cond_dt_available,
                          &pt_db_report_handler->mtx);
      }
      else
      {
        pt_report_node = (struct db_report_node *)l_queue_pop_head(
          pt_db_report_handler->pt_report_queue);
        break;
      }
    }
    pthread_mutex_unlock(&pt_db_report_handler->mtx);

    DBG_LOG_INFO("The database modification type is %d",
                 pt_report_node->report_type);

    switch(pt_report_node->report_type)
    {
#if (0)
      case DB_REPORT_MODIFICATION:
      {
        // If the modification performed by the MQTT process
        to_handle_db_modification_report(
          &pt_report_node->report_info.db_new_modification);
        break;
      }
      case DB_TBL_DEV_UPDATED:
      {
        // If Dev Table has been modified over BLE
        to_handle_db_table_dev_update_event(
          &pt_report_node->report_info.db_event_tbl_update);
        break;
      }
#endif
      case DB_TBL_GATEWAY_UPDATED:
      {
        // If Gateway Table has been modified over BLE
        to_handle_db_table_gateway_update_event(
          &pt_report_node->report_info.db_event_tbl_update);
        break;
      }
      default:
      {
        DBG_LOG_ERR("Unknown database modification");
        break;
      }
    }

    // To release the node
    l_free(pt_report_node);
    pt_report_node = NULL;
  }
}
void
app_db_set_db_modification_readout_enable_fg(void)
{
  // DBG_LOG_DEBUG("db_modification_readout_enable_fg: true");
  db_modification_readout_enable_fg = true;
}
void
app_db_clear_db_modification_readout_enable_fg(void)
{
  DBG_LOG_DEBUG("db_modification_readout_enable_fg: false");
  db_modification_readout_enable_fg = false;
}
bool
app_db_get_db_modification_readout_enable_fg(void)
{
  return db_modification_readout_enable_fg;
}
/**
 * @brief Thread to read out the database modification
 */
void *
thandler_db_modification_readout(void *pt_para)
{
  DBG_LOG_DEBUG("Start thread: modification_readout");
  while(1)
  {
    if(db_modification_readout_enable_fg == true)
    {
      if(db_modification_timer_fg == true)
      {
        DBG_LOG_DEBUG("db_modification_readout_enable_fg: true");
        db_modification_timer_fg = false;
        if((db_table_mod_report_cnt[0] + db_table_mod_report_cnt[1]) != 0)
        {
          db_table_mod_report_cnt[0] = 0;
          db_table_mod_report_cnt[1] = 0;
          // Read gw config (from Gateway and Dev tables)
          if(ST_OK !=
             app_db_fetch_gwconf_and_dev_from_db(app_db_get_dbop_controller(),
                                                 sys_get_gwConfig_and_dev()))
          {
            DBG_LOG_ERR("Failed to read gw-conf");
            return;
          }

          // Set NTP
          app_com_setup_system_timesyncd(sys_get_gwConfig_and_dev());

          // Set network configuration
          app_com_set_network_configuration(sys_get_gwConfig_and_dev());

          // Update the BLE white list
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
          gwconf_and_dev_t *pt_gwconf_and_dev =
            sys_get_gwConfig_and_dev();
          int32_t tp_mac_num_p = 0;
          int32_t tp_mac_num_t = 0;
          struct bt_addr kp_white_list_p[MAX_BT_WHITE_LIST] = { 0 };
          struct bt_addr kp_white_list_t[MAX_BT_WHITE_LIST] = { 0 };
          uint32_t kp_white_list_nameid_t[MAX_BT_WHITE_LIST] = { 0 };

          gwConfig_gwConfig_children_t *pt_child_ndtemp = NULL,
                                       *pt_cur_child_nd = NULL;

          pthread_mutex_lock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
          if(!list_empty(&pt_gwconf_and_dev->gw_conf_info.gwConfig.
                         childrenList))
          {
            list_for_each_entry_safe(pt_cur_child_nd, pt_child_ndtemp,
                                     &pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList,
                                     childrenNode)
            {

              if((strlen(TYPE_STR_INSIGHT_P) ==
                  strlen(pt_cur_child_nd->type)) &&
                 (0 ==
                  memcmp(TYPE_STR_INSIGHT_P, pt_cur_child_nd->type,
                         strlen(TYPE_STR_INSIGHT_P))))
              {
                // Insight-P
                kp_white_list_p[tp_mac_num_p %
                                MAX_BT_WHITE_LIST].nap =
                  pt_cur_child_nd->macAddr.addr.addrArray[0] * 0x100 +
                  pt_cur_child_nd->macAddr.addr.addrArray[1];
                kp_white_list_p[tp_mac_num_p %
                                MAX_BT_WHITE_LIST].uap =
                  pt_cur_child_nd->macAddr.addr.addrArray[2];
                kp_white_list_p[tp_mac_num_p %
                                MAX_BT_WHITE_LIST].lap =
                  pt_cur_child_nd->macAddr.addr.addrArray[3] * 0x100 *
                  0x100 + pt_cur_child_nd->macAddr.addr.addrArray[4] *
                  0x100 + pt_cur_child_nd->macAddr.addr.addrArray[5];
                tp_mac_num_p++;
              }
              else if((strlen(TYPE_STR_INSIGHT_T) ==
                       strlen(TYPE_STR_INSIGHT_T)) &&
                      (0 ==
                       memcmp(TYPE_STR_INSIGHT_T, pt_cur_child_nd->type,
                              strlen(TYPE_STR_INSIGHT_T))))
              {
                // Insight-T
                kp_white_list_t[tp_mac_num_t %
                                MAX_BT_WHITE_LIST].nap =
                  pt_cur_child_nd->macAddr.addr.addrArray[0] * 0x100 +
                  pt_cur_child_nd->macAddr.addr.addrArray[1];
                kp_white_list_t[tp_mac_num_t %
                                MAX_BT_WHITE_LIST].uap =
                  pt_cur_child_nd->macAddr.addr.addrArray[2];
                kp_white_list_t[tp_mac_num_t %
                                MAX_BT_WHITE_LIST].lap =
                  pt_cur_child_nd->macAddr.addr.addrArray[3] * 0x100 *
                  0x100 + pt_cur_child_nd->macAddr.addr.addrArray[4] *
                  0x100 + pt_cur_child_nd->macAddr.addr.addrArray[5];
                kp_white_list_nameid_t[tp_mac_num_t %
                                       MAX_BT_WHITE_LIST] =
                  pt_cur_child_nd->nameId;
                tp_mac_num_t++;
              }
              else
              {
                DBG_LOG_ERR("%s and %s are the only valid sensor type STR",
                            TYPE_STR_INSIGHT_P, TYPE_STR_INSIGHT_T);
              }
            }

            tp_mac_num_p = tp_mac_num_p >
                           MAX_BT_WHITE_LIST ? MAX_BT_WHITE_LIST :
                           tp_mac_num_p;
            tp_mac_num_t = tp_mac_num_t >
                           MAX_BT_WHITE_LIST ? MAX_BT_WHITE_LIST :
                           tp_mac_num_t;
          }
          pthread_mutex_unlock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
          DBG_LOG_INFO(
            "Update white list with PredictSensor (%d), Insight-T (%d)",
            tp_mac_num_p,
            tp_mac_num_t);
          if(tp_mac_num_p || tp_mac_num_t)
          {
            app_sch_set_bt_white_list(kp_white_list_p, tp_mac_num_p);
            app_hci_update_bt_white_list(
              app_hci_get_dtpacket_saving_ctr(),
              kp_white_list_t,
              kp_white_list_nameid_t,
              tp_mac_num_t);
          }
#else /* GWCONFIG_CHILDREN_USING_LINKED_LIST == 1 */
#error \
          "So far, do not support that GWCONFIG_CHILDREN_USING_LINKED_LIST is not 1."
#endif /* GWCONFIG_CHILDREN_USING_LINKED_LIST != 1 */
        }
        if(db_table_mod_report_cnt[2])
        {
          db_table_mod_report_cnt[2] = 0;
        }
        if(db_table_mod_report_cnt[3])
        {
          db_table_mod_report_cnt[3] = 0;
        }
      }
    }
    else
    {
      DBG_LOG_DEBUG("db_modification_readout_enable_fg: false");
      if((db_table_mod_report_cnt[0] + db_table_mod_report_cnt[1]
          + db_table_mod_report_cnt[2] + db_table_mod_report_cnt[3]) != 0)
      {
        DBG_LOG_DEBUG(
          "The database modification woulld be act only when it is enabled.");
      }
      sleep(10);
    }
  }
}
/**
 * @brief The handler for database modification report timer
 */
static void
db_mod_report_timer_handler(
  int sig,
  siginfo_t *pt_sig,
  void *uc)
{
  DBG_LOG_DEBUG("Database modification report timer, sig %d", sig);
  db_modification_timer_fg = true;
}
/**
 * @brief Initialize the timeout timer for database modification report
 */
static void
app_db_init_db_mod_report_timer_ctr(void)
{
  struct sigaction kp_sa = { 0 };
  timer_t kp_timeid = 0;
  struct sigevent kp_sev = { 0 };
  sigset_t kp_msk = { 0 };

  // Initialize
  memset(db_table_mod_report_cnt, 0, sizeof(db_table_mod_report_cnt));

  // To setup the timer
  // 1. Set up the handler for timer signal
  kp_sa.sa_flags = SA_SIGINFO;
  kp_sa.sa_sigaction = db_mod_report_timer_handler;
  sigemptyset(&kp_sa.sa_mask);
  if(sigaction(SIG_DB_MODIFICATION_REPORT_TIMEOUT, &kp_sa, NULL) == -1)
  {
    DBG_LOG_ERR("Failed to set sigaction");
    return;
  }
  // 2. Block the timer signal temporarily
  sigemptyset(&kp_msk);
  sigaddset(&kp_msk, SIG_DB_MODIFICATION_REPORT_TIMEOUT);
  if(sigprocmask(SIG_SETMASK, &kp_msk, NULL) == -1)
  {
    DBG_LOG_ERR("Failed to set the signal mask");
    return;
  }
  // 3. Create the timer
  kp_sev.sigev_notify = SIGEV_SIGNAL;
  kp_sev.sigev_signo = SIG_DB_MODIFICATION_REPORT_TIMEOUT;
  kp_sev.sigev_value.sival_ptr = &kp_timeid;
  if(timer_create(CLOCKID, &kp_sev, &kp_timeid) == -1)
  {
    DBG_LOG_ERR("Failed to create timer, for %s", strerror(errno));
    return;
  }
  // 4. Unblock the signal
  if(sigprocmask(SIG_UNBLOCK, &kp_msk, NULL) == -1)
  {
    DBG_LOG_ERR("Failed to unblock signal\n");
    return;
  }

  // Restore the timer ID
  db_mod_report_timer_id = kp_timeid;

  return;
}
/**
 * @brief The handler of the database modification report: Start a timer
 * but not really handle modification. Really handling modification in
 * @p db_mod_report_timer_handler to avoid frequent database operation.
 */
static app_state_t
to_handle_db_modification_report(
  gw_pro_command_report_new_modification_param_t *pt_modification_param)
{
  app_state_t tpret = ST_OK;
  struct itimerspec kp_its = { 0 };

  if(NULL == pt_modification_param)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  DBG_LOG_DEBUG(
    "DB modfication report table=%d, itemidx=%d, opeartion=%d, ts=%d",
    pt_modification_param->table, \
    pt_modification_param->itemIdx,
    pt_modification_param->operation,
    pt_modification_param->timestamp);

  switch(pt_modification_param->table)
  {
    case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:
    {
      DBG_LOG_DEBUG("Gateway Table has been modified ...");
      db_table_mod_report_cnt[0]++;
      // Sync would be started after 20 s since the gateway table
      // modification report received
      kp_its.it_value.tv_sec = 20;
      kp_its.it_value.tv_nsec = 0;
      kp_its.it_interval.tv_sec = 0;
      kp_its.it_interval.tv_nsec = 0;
      if(timer_settime(db_mod_report_timer_id, 0, &kp_its, NULL) < 0)
      {
        DBG_LOG_ERR("Failed to set timer, for %s", strerror(errno));
        tpret = ST_ERR;
      }
      break;
    }
    case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE:
    {
      DBG_LOG_DEBUG("Dev Table has been modified ...");
      db_table_mod_report_cnt[1]++;
      // Sync would be started after 20 s since the dev table
      // modification report received
      kp_its.it_value.tv_sec = 20;
      kp_its.it_value.tv_nsec = 0;
      kp_its.it_interval.tv_sec = 0;
      kp_its.it_interval.tv_nsec = 0;
      if(timer_settime(db_mod_report_timer_id, 0, &kp_its, NULL) < 0)
      {
        DBG_LOG_ERR("Failed to set timer, for %s", strerror(errno));
        tpret = ST_ERR;
      }
      break;
    }
    case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE:
    {
      DBG_LOG_DEBUG("Sensor Table has been modified ...");
      db_table_mod_report_cnt[2]++;
      // Sync would be started after 20 s since the sensor table
      // modification report received
      kp_its.it_value.tv_sec = 20;
      kp_its.it_value.tv_nsec = 0;
      kp_its.it_interval.tv_sec = 0;
      kp_its.it_interval.tv_nsec = 0;
      if(timer_settime(db_mod_report_timer_id, 0, &kp_its, NULL) < 0)
      {
        DBG_LOG_ERR("Failed to set timer, for %s", strerror(errno));
        tpret = ST_ERR;
      }
      break;
    }
    case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE:
    {
      DBG_LOG_DEBUG("sensorData Table has been modified ...");
      db_table_mod_report_cnt[3]++;
      // Sync would be started after 20 s since the sensordata table
      // modification report received
      kp_its.it_value.tv_sec = 20;
      kp_its.it_value.tv_nsec = 0;
      kp_its.it_interval.tv_sec = 0;
      kp_its.it_interval.tv_nsec = 0;
      if(timer_settime(db_mod_report_timer_id, 0, &kp_its, NULL) < 0)
      {
        DBG_LOG_ERR("Failed to set timer, for %s", strerror(errno));
        tpret = ST_ERR;
      }
      break;
    }
    default:
    {
      DBG_LOG_ERR("Unknown table idx %d", pt_modification_param->table);
      tpret = ST_ERR;
      break;
    }
  }

  return tpret;
}
/**
 * @brief The handler of Gateway Table update (over BLE). When this
 * function has been called, everything has been read to @p gw_conf_info
 * pointed by @p g_pro_general_ctr .
 */
static app_state_t
to_handle_db_table_gateway_update_event(
  struct db_event_table_updated *pt_db_tbl_update_event)
{
  app_state_t tpret = ST_OK;
  gwconf_and_dev_t *pt_gwconf_and_dev = sys_get_gwConfig_and_dev();

  if(GW_PRO_TABLE_IDX_GW_CONFIG_TABLE !=
     pt_db_tbl_update_event->table)
  {
    DBG_LOG_ERR("Invalid table idx");
    tpret = ST_ERR;
    return tpret;
  }
  DBG_LOG_INFO("Gateway Table has been modified over BLE ...");

  // Set NTP
  app_com_setup_system_timesyncd(pt_gwconf_and_dev);

  // Set network configuration
  app_com_set_network_configuration(pt_gwconf_and_dev);

  return tpret;
}
/**
 * @brief The handler of Dev Table update (over BLE). When this function
 *        has been called, everything has been read to @p gw_conf_info 
 *        pointed by @p g_pro_general_ctr .
 */
static app_state_t
to_handle_db_table_dev_update_event(
  struct db_event_table_updated *pt_db_tbl_update_event)
{
  app_state_t tpret = ST_OK;
  gwconf_and_dev_t *pt_gwconf_and_dev = sys_get_gwConfig_and_dev();
  int32_t tp_mac_num_p = 0;
  struct bt_addr kp_white_list_p[MAX_BT_WHITE_LIST] = { 0 };
  int32_t tp_mac_num_t = 0;
  struct bt_addr kp_white_list_t[MAX_BT_WHITE_LIST] = { 0 };
  uint32_t kp_white_list_nameid_t[MAX_BT_WHITE_LIST] = { 0 };

  if(GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE !=
     pt_db_tbl_update_event->table)
  {
    DBG_LOG_ERR("Invalid table idx");
    tpret = ST_ERR;
    return tpret;
  }
  DBG_LOG_INFO("Dev Table has been modified over BLE ...");

#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  gwConfig_gwConfig_children_t *pt_child_ndtemp = NULL,
                               *pt_cur_child_nd = NULL;

  pthread_mutex_lock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
  if(!list_empty(&pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList))
  {
    tp_mac_num_p = 0;
    memset(kp_white_list_p, 0, sizeof(kp_white_list_p));
    tp_mac_num_t = 0;
    memset(kp_white_list_t, 0, sizeof(kp_white_list_t));

    list_for_each_entry_safe(pt_cur_child_nd, pt_child_ndtemp,
                             &pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList,
                             childrenNode)
    {
      if((strlen(TYPE_STR_INSIGHT_P) == strlen(pt_cur_child_nd->type)) &&
         (0 ==
          memcmp(TYPE_STR_INSIGHT_P, pt_cur_child_nd->type,
                 strlen(TYPE_STR_INSIGHT_P))))
      {
        kp_white_list_p[tp_mac_num_p %
                        MAX_BT_WHITE_LIST].nap =
          pt_cur_child_nd->macAddr.addr.addrArray[0] * 0x100 +
          pt_cur_child_nd->macAddr.addr.addrArray[1];  //2B
        kp_white_list_p[tp_mac_num_p %
                        MAX_BT_WHITE_LIST].uap =
          pt_cur_child_nd->macAddr.addr.addrArray[2];  //1B
        kp_white_list_p[tp_mac_num_p %
                        MAX_BT_WHITE_LIST].lap =
          pt_cur_child_nd->macAddr.addr.addrArray[3] * 0x100 * 0x100 +
          pt_cur_child_nd->macAddr.addr.addrArray[4] * 0x100 +
          pt_cur_child_nd->macAddr.addr.addrArray[5];  //3B
        tp_mac_num_p++;
      }
      else if((strlen(TYPE_STR_INSIGHT_T) ==
               strlen(pt_cur_child_nd->type)) &&
              (0 ==
               memcmp(TYPE_STR_INSIGHT_T, pt_cur_child_nd->type,
                      strlen(TYPE_STR_INSIGHT_T))))
      {
        kp_white_list_t[tp_mac_num_t %
                        MAX_BT_WHITE_LIST].nap =
          pt_cur_child_nd->macAddr.addr.addrArray[0] * 0x100 +
          pt_cur_child_nd->macAddr.addr.addrArray[1];  //2B
        kp_white_list_t[tp_mac_num_t %
                        MAX_BT_WHITE_LIST].uap =
          pt_cur_child_nd->macAddr.addr.addrArray[2];  //1B
        kp_white_list_t[tp_mac_num_t %
                        MAX_BT_WHITE_LIST].lap =
          pt_cur_child_nd->macAddr.addr.addrArray[3] * 0x100 * 0x100 +
          pt_cur_child_nd->macAddr.addr.addrArray[4] * 0x100 +
          pt_cur_child_nd->macAddr.addr.addrArray[5];  //3B
        kp_white_list_nameid_t[tp_mac_num_t %
                               MAX_BT_WHITE_LIST] =
          pt_cur_child_nd->nameId;
        tp_mac_num_t++;
      }
      else
      {
        DBG_LOG_ERR("%s and %s are the only valid sensor type STR",
                    TYPE_STR_INSIGHT_P, TYPE_STR_INSIGHT_T);
      }
    }

    tp_mac_num_p = tp_mac_num_p >
                   MAX_BT_WHITE_LIST ? MAX_BT_WHITE_LIST : tp_mac_num_p;
    tp_mac_num_t = tp_mac_num_t >
                   MAX_BT_WHITE_LIST ? MAX_BT_WHITE_LIST : tp_mac_num_t;
  }
  pthread_mutex_unlock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);

  // now we try to update the white list
  DBG_LOG_INFO("Try to update white list _P %d, _T %d", tp_mac_num_p,
                tp_mac_num_t);
  app_sch_set_bt_white_list(kp_white_list_p, tp_mac_num_t);
  app_hci_update_bt_white_list(
              app_hci_get_dtpacket_saving_ctr(),
              kp_white_list_t,
              kp_white_list_nameid_t,
              tp_mac_num_t);

#else /* GWCONFIG_CHILDREN_USING_LINKED_LIST == 1 */
#error \
  "So far, do not support that GWCONFIG_CHILDREN_USING_LINKED_LIST is not 1."
#endif /* GWCONFIG_CHILDREN_USING_LINKED_LIST != 1 */

  return tpret;
}
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
#if (ENABLE_MODBUS_FEATURE == 1)
static gwConfig_gwConfig_children_t *
makeChildrenForGw(
  const char *type,
  const char *manufacturer,
  const char *name,
  const uint32_t nameId,
  const char *bleName,
  macAddr_t *macAddr,
  const char *description)
{
  gwConfig_gwConfig_children_t *_childrenListNode = NULL;

  _childrenListNode = malloc(sizeof(gwConfig_gwConfig_children_t));
  if(_childrenListNode == NULL)
  {
    // Failed to make a node
    return _childrenListNode;
  }
  strncpy(_childrenListNode->type, type, sizeof(_childrenListNode->type));
  strncpy(_childrenListNode->manufacturer, manufacturer,
          sizeof(_childrenListNode->manufacturer));
  strncpy(_childrenListNode->name, name, sizeof(_childrenListNode->name));
  _childrenListNode->nameId = nameId;
  strncpy(_childrenListNode->bleName, bleName,
          sizeof(_childrenListNode->bleName));
  memcpy(_childrenListNode->macAddr.addr.addrArray,
         macAddr->addr.addrArray,
         sizeof(_childrenListNode->macAddr.addr.addrArray));
  strncpy(_childrenListNode->description, description,
          sizeof(_childrenListNode->description));
  return _childrenListNode;
}
#else
static gwConfig_gwConfig_children_t *
makeChildrenForGw(
  const char *type,
  const char *manufacturer,
  const char *name,
  const uint32_t nameId,
  const char *bleName,
  macAddr_t *macAddr)
{
  gwConfig_gwConfig_children_t *_childrenListNode = NULL;

  _childrenListNode = malloc(sizeof(gwConfig_gwConfig_children_t));
  if(_childrenListNode == NULL)
  {
    // Failed to make a node
    return _childrenListNode;
  }
  strncpy(_childrenListNode->type, type, sizeof(_childrenListNode->type));
  strncpy(_childrenListNode->manufacturer, manufacturer,
          sizeof(_childrenListNode->manufacturer));
  strncpy(_childrenListNode->name, name, sizeof(_childrenListNode->name));
  _childrenListNode->nameId = nameId;
  strncpy(_childrenListNode->bleName, bleName,
          sizeof(_childrenListNode->bleName));
  memcpy(_childrenListNode->macAddr.addr.addrArray,
         macAddr->addr.addrArray,
         sizeof(_childrenListNode->macAddr.addr.addrArray));
  return _childrenListNode;
}
#endif  /* end of ENABLE_MODBUS_FEATURE */
#endif /* GWCONFIG_CHILDREN_USING_LINKED_LIST == 1 */
/**
 * @brief To dump the IPC message header
 */
static void
to_dump_gw_pro_msg_header(gw_pro_message_header_t *pt_gw_pro_msg_header)
{
  DBG_LOG_DEBUG("==== Header of IPC message with process sql_op ====");
  DBG_LOG_DEBUG("from %d, msgTimestamp_s : %d",
                pt_gw_pro_msg_header->from,
                pt_gw_pro_msg_header->msgTimestamp_s);
  DBG_LOG_DEBUG("seqNo %d, current_package : %d , total_package : %d",
                pt_gw_pro_msg_header->seqNo,
                pt_gw_pro_msg_header->current_package,
                pt_gw_pro_msg_header->total_package);
  DBG_LOG_DEBUG("comRespFg %d", pt_gw_pro_msg_header->comRespFg);
  DBG_LOG_DEBUG("commandMsg.command %d, commandMsg.commandId %d",
                pt_gw_pro_msg_header->command_or_response.commandMsg.command,
                pt_gw_pro_msg_header->command_or_response.commandMsg.commandId);
  switch(pt_gw_pro_msg_header->command_or_response.commandMsg.command)
  {
    case gw_pro_command_sqlite_update_item:
    {
      DBG_LOG_DEBUG("gw_pro_command_sqlite_update_item");
      DBG_LOG_DEBUG(
        "sqlite_update_item_param.dataNum %d, sqlite_update_item_param.itemIdx %d, sqlite_update_item_param.table %d", \
        pt_gw_pro_msg_header->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum, \
        pt_gw_pro_msg_header->command_or_response.commandMsg.param.sqlite_update_item_param.itemIdx, \
        pt_gw_pro_msg_header->command_or_response.commandMsg.param.sqlite_update_item_param.table);
      break;
    }
    case gw_pro_command_sqlite_query_item:
    {
      DBG_LOG_DEBUG("gw_pro_command_sqlite_query_item");
      DBG_LOG_DEBUG(
        "query_item_param.itemIdx=%d, query_item_param.table=%d",
        pt_gw_pro_msg_header->command_or_response.commandMsg.param.sqlite_query_item_param.itemIdx, \
        pt_gw_pro_msg_header->command_or_response.commandMsg.param.sqlite_query_item_param.table);
      break;
    }
    case gw_pro_command_sqlite_delete_item:
    {
      DBG_LOG_DEBUG("gw_pro_command_sqlite_delete_item");
      break;
    }
    case gw_pro_command_sqlite_filter:
    {
      DBG_LOG_DEBUG("gw_pro_command_sqlite_filter");
      break;
    }
    default:
    {
      DBG_LOG_WARN("Unknown command");
      break;
    }
  }
  return;
}
/**
 * @brief To check whether the item has existed in the
 *        sensor data table based on @p meas_ts of @p pt_dtitem ,
 *        @p data_type of @p pt_dtitem , @p dimension of @p pt_dtitem
 *        and the mac address pointed by @p pt_macstr.
 * @return true when the data item has existed in database.
 */
#if (SKF_GW_NEW == 1)
bool
is_dtitem_in_db(
  const struct data_item *pt_dtitem,
  const uint8_t *pt_macstr)
#else
static bool
is_dtitem_in_db(
  const struct data_item *pt_dtitem,
  const uint8_t *pt_macstr)
#endif
{
  bool tpret = false;
  gw_pro_command_sqlite_filter_param_t kp_filter = { 0 };
  uint32_t kp_respbuf[2] = { 0 };
  int32_t kp_itemsz = 0;
  uint8_t _dataNum = 0;

  if(NULL == pt_dtitem)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = false;
    return tpret;
  }

  // to fill the filter
  kp_filter.table = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;  
  // measurement-time
  kp_filter.filterData[_dataNum % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 8;
  kp_filter.filterData[_dataNum %
                       GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_value_range;
  kp_filter.filterData[_dataNum %
                       GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condParam.
  range_uint64.max =
    pt_dtitem->meas_ts;
  kp_filter.filterData[_dataNum %
                       GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condParam.
  range_uint64.min =
    pt_dtitem->meas_ts;
  _dataNum++;
  // and
  kp_filter.filterData[_dataNum %
                       GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_and;
  _dataNum++;
  // data-type
  kp_filter.filterData[_dataNum % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 11;
  kp_filter.filterData[_dataNum %
                       GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
  gw_pro_sqlite_cond_value_range;
  kp_filter.filterData[_dataNum %
                       GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condParam.
  range_int32.max =
  pt_dtitem->data_type;
  kp_filter.filterData[_dataNum %
                       GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condParam.
  range_int32.min =
  pt_dtitem->data_type;
  _dataNum++;
  // and
  kp_filter.filterData[_dataNum %
                       GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_and;
  _dataNum++;
  // dimension
  if(pt_dtitem->dimension != SKFChina_Common_Dimension_UNKNOWN_DIMENSION)
  {
    kp_filter.filterData[_dataNum % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 17;
    kp_filter.filterData[_dataNum %
                        GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
      gw_pro_sqlite_cond_value_range;
    kp_filter.filterData[_dataNum %
                        GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condParam.
    range_int32.max = pt_dtitem->dimension;
    kp_filter.filterData[_dataNum %
                        GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condParam.
    range_int32.min = pt_dtitem->dimension;
    _dataNum++;
    // and
    kp_filter.filterData[_dataNum %
                       GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_and;
    _dataNum++;
  }
  //MAC string
  kp_filter.filterData[_dataNum % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 4;
  kp_filter.filterData[_dataNum %
                       GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_str_equal;
  snprintf(
    kp_filter.filterData[_dataNum % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condParam.string_value.s,
    GW_PRO_MAX_STRING_LEN_BYTE, "%s",
    pt_macstr);
  _dataNum++;

  kp_filter.dataNum = _dataNum;

  kp_itemsz = app_db_fetch_filtered_index(
    app_db_get_dbop_controller(),
    &kp_filter,
    sizeof(kp_respbuf),
    kp_respbuf);
  if(kp_itemsz)
  {
    tpret = true;
  }
  DBG_LOG_DEBUG(
    "Data has existed in sensorData table, the amount of items is %d",
    kp_itemsz);

  return tpret;
}
/**
 * @brief Deal with message from database
 */
app_state_t
app_db_handler_for_msg_from_database_v2(
  struct dbop_controller *pt_dbop_ctr,
  const struct msgbuf *pt_msgbuf)
{
  app_state_t tpret = ST_OK;

  DBG_LOG_INFO("db msg type recv-ed:%d\n", pt_msgbuf->mtype);

  switch(pt_msgbuf->mtype)
  {
    case gw_pro_command:
    {
      // To deal with command from db (e.g., modification report)
      app_db_handle_command_from_db((gw_pro_message_header_t *)pt_msgbuf->mtext);
      break;
    }
    case gw_pro_response:
    {
      // To search corresponding command from the queue
      app_db_signal_and_pop_dbop_waiting_obj(pt_dbop_ctr,
                                             (gw_pro_message_header_t *)pt_msgbuf->mtext);
      break;
    }
    default:
    {
      DBG_LOG_ERR("Unknown database message type %d", pt_msgbuf->mtype);
      break;
    }
  }
  return tpret;
}

/*to query the database
*/
static app_state_t to_query_database(struct database_operation_controller *pt_db_op_ctr, struct msg_format_controller*pt_msg_format_ctr,gw_pro_command_sqlite_query_item_param_t *pt_query_param)
{
	app_state_t tpret = ST_OK;
	if((NULL == pt_db_op_ctr)||(NULL == pt_msg_format_ctr)||(NULL == pt_query_param))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}
	// fill the message
	pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
	pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);
	pt_msg_format_ctr->msginfo.header.seqNo =  app_db_allocate_ipc_seqno( app_db_get_dbop_controller());//1;
	pt_msg_format_ctr->msginfo.header.current_package = 1;
	pt_msg_format_ctr->msginfo.header.total_package = 1;
	pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;

	pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command = gw_pro_command_sqlite_query_item;
	pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId = pt_msg_format_ctr->msginfo.header.seqNo;
	pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_query_item_param.itemIdx = pt_query_param->itemIdx; 
	pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_query_item_param.table = pt_query_param->table;

	// to fill the msg for queue
	pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
	memcpy( &pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo, sizeof(gw_pro_message_header_t));
	// to put the msg on the queue
	
	if(msgsnd( pt_db_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg, sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
	{
		tpret = ST_ERR;
		DBG_LOG_ERR("fail to put msg on queue, for %s", strerror(errno));
	}
	else
	{
		tpret = ST_OK;
		DBG_LOG_INFO("msg is sent to pro DB successfully");
	}
	return tpret;
}
/*to release the database operation message
*/
static void to_release_db_op_msg(struct db_op_msg *pt_op_msg)
{
	if(NULL == pt_op_msg)
	{
		DBG_LOG_ERR("unexpected NULL");
		return;
	}
	switch(pt_op_msg->cmd)
	{
		case gw_pro_command_sqlite_update_item:
		{
			switch(pt_op_msg->table_idx)
			{
				case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE:
				{// Sensor-data
					if(pt_op_msg->cmd_para.pt_dtitem)
					{
						l_free( pt_op_msg->cmd_para.pt_dtitem);
						pt_op_msg->cmd_para.pt_dtitem = NULL;
					}
					break;
				}
				case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:
				{// GW-configuration
					if(pt_op_msg->cmd_para.pt_gwconf)
					{
						l_free( pt_op_msg->cmd_para.pt_gwconf);
						pt_op_msg->cmd_para.pt_gwconf = NULL;
					}
					break;
				}
				case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE:
				{// DEV
					if(pt_op_msg->cmd_para.pt_sensorinfo)
					{
						l_free( pt_op_msg->cmd_para.pt_sensorinfo);
						pt_op_msg->cmd_para.pt_sensorinfo = NULL;
					}
					break;
				}
				case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE:
				{// sensor-configuration
					DBG_LOG_WARN("you probably have something to do here , when you see this string!?");
					break;
				}
				default:
				{
					DBG_LOG_ERR("unknown table idx");
					break;
				}
			}
			break;
		}
		case gw_pro_command_sqlite_delete_item:
		{
			break;
		}
		case gw_pro_command_sqlite_filter:
		{
			break;
		}
		case gw_pro_command_sqlite_query_item:
		{
			break;
		}
		default:
		{
			DBG_LOG_ERR("well , unknown sqlite cmd");
			break;
		}
	}
	l_free( pt_op_msg);
}

/*to deal with cmd-update
*/
static app_state_t cmd_update_handler(struct database_operation_controller *pt_db_op_ctr, struct msg_format_controller*pt_msg_format_ctr, const struct db_op_msg*pt_op_msg)
{
	app_state_t tpret = ST_OK;

	switch(pt_op_msg->table_idx)
	{
		case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:
		{
			// to_update_table_gw_conf( pt_db_op_ctr, pt_msg_format_ctr, pt_op_msg->cmd_para.pt_gwconf);
			break;
		}
		case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE:
		{
			// To send the data
      DBG_LOG_DEBUG("TO SEND DATA TO SENSORDATA");
#if (SKF_GW_NEW != 1)
      uint8_t kp_sensor_mac_str[MAX_BT_ADDRSTR] = {0};
	    app_pro_gen_get_active_client_id_str( kp_sensor_mac_str, MAX_BT_ADDRSTR);
      snprintf(pt_op_msg->cmd_para.pt_dtitem->sensor_mac, 
              sizeof(pt_op_msg->cmd_para.pt_dtitem->sensor_mac),
              "%s",
              kp_sensor_mac_str);
#endif /* SKF_GW_NEW != 1 */
      #if(0)// for debug only
				app_froto_to_dump_dtitem( pt_op_msg->cmd_para.pt_dtitem);
			#endif
      app_db_deliver_sensordata_to_db(app_db_get_dbop_controller(), pt_op_msg->cmd_para.pt_dtitem);
			break;
		}
		case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE:
		{
      DBG_LOG_DEBUG("TO SEND DATA TO DEV");
			// to_update_table_dev_info( pt_db_op_ctr, pt_msg_format_ctr, pt_op_msg->cmd_para.pt_sensorinfo);
			break;
		}
		case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE:
		{
			
			break;
		}
		default:
		{
			DBG_LOG_ERR("unknown table idx , nothing to do");
			tpret = ST_ERR;
			break;
		}
	}

	return tpret;
}

static struct msg_format_controller g_msg_format_ctr = {0};
/*threads to update the database
*/
void*thandler_db_data_update(void*pt_para)
{
	struct database_operation_controller *pt_db_op_ctr = app_db_get_database_op_ctr();
	//struct data_item *pt_dtitem;
	struct db_op_msg *pt_op_msg = NULL;
	struct msg_format_controller*pt_msg_format_ctr = &g_msg_format_ctr;
	while(1)
	{
		DBG_LOG_INFO("update database\n");
		pthread_mutex_lock( &pt_db_op_ctr->mtx);
		while(1)
		{
			if(0 == l_queue_length( pt_db_op_ctr->pt_dtitem_queue))
			{
				pthread_cond_wait( &pt_db_op_ctr->cond_dt_available, &pt_db_op_ctr->mtx);
			}
			else
			{
				pt_op_msg = (struct db_op_msg*)l_queue_pop_head( pt_db_op_ctr->pt_dtitem_queue);
				break;
			}
		}
		pthread_mutex_unlock( &pt_db_op_ctr->mtx);
		
		if(NULL == pt_op_msg)
		{
			DBG_LOG_ERR("some invalid msg is put on the queue");
			continue;
		}
		// to clean the buffer 		
		memset( pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

		switch(pt_op_msg->cmd)
		{
			case gw_pro_command_sqlite_update_item:
			{
				cmd_update_handler( pt_db_op_ctr, pt_msg_format_ctr, pt_op_msg);
				break;
			}
			case gw_pro_command_sqlite_delete_item:
			{
				/*...*/
				break;
			}
			case gw_pro_command_sqlite_filter:
			{
				/*...*/
				break;
			}
			case gw_pro_command_sqlite_query_item:
			{
				DBG_LOG_DEBUG("to send msg to query the database");
				to_query_database( pt_db_op_ctr, pt_msg_format_ctr, &pt_op_msg->cmd_para.query_item);
				break;
			}
			default:
			{
				DBG_LOG_WARN("unknown sqlite command %d", pt_op_msg->cmd);
				break;
			}
		}
		// to free the data-item
		//l_free( pt_dtitem);
		to_release_db_op_msg( pt_op_msg);

		#if(0)	
		#if(1)
			continue; // for debug only
		#endif
		//to wait for response
		pthread_mutex_lock( &pt_db_op_ctr->mtx_for_waiting);
		while(1)
		{
			if(false == pt_db_op_ctr->is_resp_received)
			{
				pthread_cond_wait( &pt_db_op_ctr->cond_for_waiting, &pt_db_op_ctr->mtx_for_waiting);
			}
			else
			{	
				DBG_LOG_INFO("the response is received successfully");

				pt_db_op_ctr->is_resp_received = false; // reset the flag
				break;
			}
		}
		pthread_mutex_unlock( &pt_db_op_ctr->mtx_for_waiting);	
		#endif
	}
}

/**
 * @brief Get the lastRegularSensingMeasID
 */
uint32_t 
app_db_fetch_lastRegularSensingMeasID(void)
{
  return lastRegularSensingMeasID;
}

// Deprecated
#if 0
/* To deal with messages from database */
app_state_t
app_db_handler_for_msg_from_database(
  struct database_operation_controller *pt_db_op_ctr,
  const struct msgbuf *pt_msgbuf)
{
  app_state_t tpret = ST_OK;

  DBG_LOG_INFO(" the msg type received is %d", pt_msgbuf->mtype);

  switch(pt_msgbuf->mtype)
  {
    case gw_pro_command:
    {

      break;
    }
    case gw_pro_response:
    {
      pthread_mutex_lock(&pt_db_op_ctr->mtx_for_waiting);
      if(true == pt_db_op_ctr->is_resp_received)
      {
        DBG_LOG_WARN(
          "Well! this is weird. DO NOT send any msg to process DB before the response is received");
        pthread_mutex_unlock(&pt_db_op_ctr->mtx_for_waiting);
        break;
      }
      pt_db_op_ctr->is_resp_received = true;
      memset(&pt_db_op_ctr->resp_msg, 0, sizeof(struct msgbuf));
      memcpy(&pt_db_op_ctr->resp_msg, pt_msgbuf, sizeof(struct msgbuf));
      pthread_cond_signal(&pt_db_op_ctr->cond_for_waiting);  // to signal
                                                             // the
                                                             // receivation
                                                             // of response
      pthread_mutex_unlock(&pt_db_op_ctr->mtx_for_waiting);
      //DBG_LOG_INFO("the DB response is passed on ....");
      break;
    }
    default:
    {
      DBG_LOG_ERR("unknown mtype %d", pt_msgbuf->mtype);
      break;
    }
  }
  return tpret;
}


/*to format gateway-configuration as json-string and write ti to file
   pt_dbop_ctr @ the database operation controller
   pt_fpath @ the file path where the gw_conf string is written to

   NOTE!!! the gw-conf is written as json-string

 */
app_state_t
app_db_write_gw_conf_to_file(
  struct dbop_controller *pt_dbop_ctr,
  const uint8_t *pt_fpath)
{
  app_state_t tpret = ST_OK;
  gwConfig_t *pt_gwcfg = NULL;
  gw_pro_command_sqlite_filter_param_t *pt_filter = NULL;
  uint32_t kp_idbuf[MAX_BT_WHITE_LIST] = { 0 };
  uint32_t tpu32 = 0;
  struct device_info *pt_dev_info = NULL;

  struct msg_format_controller *pt_msg_format_ctr = NULL;
  gwConfig_gwConfig_children_t *pt_cfg_child = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_fpath))
  {
    DBG_LOG_ERR("unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  pt_gwcfg = (gwConfig_t *)l_malloc(sizeof(gwConfig_t));
  if(NULL == pt_gwcfg)
  {
    DBG_LOG_ERR("malloc fail");
    tpret = ST_ERR;
    goto EXIT;
  }
  memset(pt_gwcfg, 0, sizeof(gwConfig_t));
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  INIT_LIST_HEAD(&pt_gwcfg->gwConfig.childrenList);
#else
#error "Todo===you need to do some modification here"
#endif

  // to allocate a filter
  pt_filter =
    (gw_pro_command_sqlite_filter_param_t *)l_malloc(
      sizeof(gw_pro_command_sqlite_filter_param_t));
  if(NULL == pt_filter)
  {
    DBG_LOG_ERR("malloc fail");
    tpret = ST_ERR;
    goto EXIT;
  }
  //to fill the filter
  memset(kp_idbuf, 0, sizeof(kp_idbuf));
  memset(pt_filter, 0, sizeof(gw_pro_command_sqlite_filter_param_t));
  pt_filter->table = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
  pt_filter->dataNum = 1;
  pt_filter->filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 1;
  pt_filter->filterData[0 %
                        GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_all;  //gw_pro_sqlite_condition_t
  // to read nameId of gw-conf from table Gateway
  if(1 !=
     app_db_fetch_filtered_index(pt_dbop_ctr, pt_filter, sizeof(kp_idbuf),
                                 kp_idbuf))
  {
    DBG_LOG_ERR("unexpected record number in table Gateway");
    tpret = ST_ERR;
    goto EXIT;
  }
  // to read the gw-conf from table Gateway
  pt_msg_format_ctr =
    (struct msg_format_controller *)l_malloc(sizeof(struct
                                                    msg_format_controller));
  if(NULL == pt_msg_format_ctr)
  {
    DBG_LOG_ERR("malloc fail");
    tpret = ST_ERR;
    goto EXIT;
  }
  if(ST_OK !=
     app_db_read_gateway_conf_from_db(pt_dbop_ctr, pt_msg_format_ctr,
                                      pt_gwcfg, kp_idbuf[0]))
  {
    DBG_LOG_ERR("fail to read record with ID %d from table Gateway",
                kp_idbuf[0]);
    tpret = ST_ERR;
    goto EXIT;
  }

  //to read table Dev
  memset(kp_idbuf, 0, sizeof(kp_idbuf));
  memset(pt_filter, 0, sizeof(gw_pro_command_sqlite_filter_param_t));
  pt_filter->table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  pt_filter->dataNum = 1;
  pt_filter->filterData[0 % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = 1;
  pt_filter->filterData[0 %
                        GW_PRO_MAX_DATA_FIELD_PER_OPERATION].condition =
    gw_pro_sqlite_cond_all;
  tpu32 =
    app_db_fetch_filtered_index(pt_dbop_ctr, pt_filter, sizeof(kp_idbuf),
                                kp_idbuf);
  if((tpu32 <= 0) || (tpu32 > MAX_BT_WHITE_LIST))
  {
    DBG_LOG_ERR("unexpected record number in table Dev, FYI %d", tpu32);
    tpret = ST_ERR;
    goto EXIT;
  }
  pt_dev_info = (struct device_info *)l_malloc(sizeof(struct device_info));
  if(NULL == pt_dev_info)
  {
    DBG_LOG_ERR("malloc fail");
    tpret = ST_ERR;
    goto EXIT;
  }
  for(uint32_t i = 0; i < tpu32; i++)
  {
    memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));
    memset(pt_dev_info, 0, sizeof(struct device_info));
    DBG_LOG_INFO("to read record with nameId %d from table Dev",
                 kp_idbuf[i % MAX_BT_WHITE_LIST]);
    if(app_db_read_dev_info_from_db(pt_dbop_ctr, pt_msg_format_ctr,
                                    pt_dev_info,
                                    kp_idbuf[i % MAX_BT_WHITE_LIST]))
    {
      DBG_LOG_ERR("fail to read table Dev");
      tpret = ST_ERR;
      goto EXIT;
    }
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
    macAddr_t tp_mac_addr = { 0 };  // TO-Fix
    uint8_t tp_strbuf[GW_PRO_MAX_STRING_LEN_BYTE] = { 0 };
    // to add the info to children List
    for(uint32_t i = 0; i < strlen(pt_dev_info->mac_addr); i++)
    {  // remove '-'
      if('-' != pt_dev_info->mac_addr[i])
      {
        tp_strbuf[i %
                  GW_PRO_MAX_STRING_LEN_BYTE] = pt_dev_info->mac_addr[i];
      }
    }
    idstr_to_bytes_array(tp_mac_addr.addr.addrArray,
                         sizeof(tp_mac_addr.addr.addrArray), tp_strbuf);

    pt_cfg_child = makeChildrenForGw(pt_dev_info->sensor_type,
                                     pt_dev_info->manufacturer, \
                                     pt_dev_info->name,
                                     pt_dev_info->nameid,
                                     pt_dev_info->sensor_name,
                                     &tp_mac_addr);
    if(NULL == pt_cfg_child)
    {
      DBG_LOG_ERR("fail to create conf-child");
      tpret = ST_ERR;
      goto EXIT;
    }
    list_add_tail(&pt_cfg_child->childrenNode,
                  &pt_gwcfg->gwConfig.childrenList);
    pt_gwcfg->gwConfig.childrenNumber++;
#else
#error "ToDo=== you need to do some modification here";
#endif
  }

#if (0)  // for debug only
  DBG_LOG_INFO("the folling is some MAC and IP dump");
  util_dbg_buf_dump(pt_gwcfg->gwConfig.macAddr.addr.addrArray,
                    sizeof(pt_gwcfg->gwConfig.macAddr.addr.addrArray));
  util_dbg_buf_dump(pt_gwcfg->gwConfig.ipConfig.dnsServer.addr.addrArray,
                    sizeof(pt_gwcfg->gwConfig.ipConfig.dnsServer.addr.
                           addrArray));
#endif

  // Now we write gw-conf to file as a json-string
  if(true != app_cjson_gwconfig_to_jsonstr(pt_gwcfg, pt_fpath))
  {
    DBG_LOG_ERR("fail to write gw-conf to file");
    tpret = ST_ERR;
    goto EXIT;
  }

EXIT:
  if(pt_gwcfg)
  {
    app_cjson_release_chilren_list(pt_gwcfg);
    l_free(pt_gwcfg);
    pt_gwcfg = NULL;
  }
  if(pt_filter)
  {
    l_free(pt_filter);
    pt_filter = NULL;
  }
  if(pt_msg_format_ctr)
  {
    l_free(pt_msg_format_ctr);
    pt_msg_format_ctr = NULL;
  }
  if(pt_dev_info)
  {
    l_free(pt_dev_info);
    pt_dev_info = NULL;
  }
  return tpret;
}
static uint8_t
msgbuf_tx_init(
  struct msg_format_controller *pt_msg_format_ctr,
  const char *tb,
  uint32_t *table) {
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_mqtt;
  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);
  pt_msg_format_ctr->msginfo.header.seqNo = 1;
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  if(0 == strcmp(tb, GATEWAY_CONFIGURE_TABLE))
  {
    *table = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
  }
  else if(0 == strcmp(tb, DEVICE_LIST_TABLE))
  {
    *table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  }
  else if(0 == strcmp(tb, SENSOR_CONFIGURE_TABLE))
  {
    *table = GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE;
  }
  else if(0 == strcmp(tb, SENSOR_DATA_TABLE))
  {
    *table = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
  }
  else
  {
    LOG_OUTPUT(OUTPOINT, "Invalid table!\r\n");
    return 1;
  }
  return 0;
}
/* to update table GW-conf
   ref@configure_content_t GatewayConfig[] =
 */
static app_state_t
to_update_table_gw_conf(
  struct database_operation_controller *pt_db_op_ctr,
  struct msg_format_controller *pt_msg_format_ctr,
  const gwConfig_gwConfig_t *pt_gwconf)
{
  app_state_t tpret = ST_OK;
  const uint32_t kp_col_num = 26;

  if((NULL == pt_db_op_ctr) || (NULL == pt_msg_format_ctr) ||
     (NULL == pt_gwconf))
  {
    DBG_LOG_ERR("unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  //        "INT PRIMARY KEY", "nameId",
  pt_msg_format_ctr->dtunit[0].field = 1;
  pt_msg_format_ctr->dtunit[0].data.data_uint32 = pt_gwconf->nameId;
  //  {"TEXT","name",},
  pt_msg_format_ctr->dtunit[1].field = 2;
  snprintf(pt_msg_format_ctr->dtunit[1].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_gwconf->name);

  //{"TEXT","macAddr",},
  pt_msg_format_ctr->dtunit[2].field = 3;
  snprintf(pt_msg_format_ctr->dtunit[2].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%02x:%02x:%02x:%02x:%02x:%02x", \
           pt_gwconf->macAddr.addr.addrArray[0], \
           pt_gwconf->macAddr.addr.addrArray[1], \
           pt_gwconf->macAddr.addr.addrArray[2], \
           pt_gwconf->macAddr.addr.addrArray[3], \
           pt_gwconf->macAddr.addr.addrArray[4], \
           pt_gwconf->macAddr.addr.addrArray[5]
           );
  //{"TEXT","manufacturer",},
  pt_msg_format_ctr->dtunit[3].field = 4;
  snprintf(pt_msg_format_ctr->dtunit[3].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_gwconf->manufacturer);

  //{"INT","ApplicationVersion",},
  pt_msg_format_ctr->dtunit[4].field = 5;
  pt_msg_format_ctr->dtunit[4].data.data_int32 = 666;

  //{ "INT","LinuxKernelVersion",},
  pt_msg_format_ctr->dtunit[5].field = 6;
  pt_msg_format_ctr->dtunit[5].data.data_int32 = 999;

  //{"INT","version",},
  pt_msg_format_ctr->dtunit[6].field = 7;
  pt_msg_format_ctr->dtunit[6].data.data_int32 = 222;
  //{"INT","lastEditTime",},
  pt_msg_format_ctr->dtunit[7].field = 8;
  pt_msg_format_ctr->dtunit[7].data.data_int32 = time(NULL);
  //{"TEXT","type",},
  pt_msg_format_ctr->dtunit[8].field = 9;
  snprintf(pt_msg_format_ctr->dtunit[8].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_gwconf->type);
  //{"INT", "gatewayMode", },
  pt_msg_format_ctr->dtunit[9].field = 10;
  pt_msg_format_ctr->dtunit[9].data.data_int32 = pt_gwconf->gatewayMode;
  //{"INT","BLE1TxPower",    },
  pt_msg_format_ctr->dtunit[10].field = 11;
  pt_msg_format_ctr->dtunit[10].data.data_int32 =
    pt_gwconf->bleConfig.ble1TxPower;
  //{"INT","BLE2TxPower",    },
  pt_msg_format_ctr->dtunit[11].field = 12;
  pt_msg_format_ctr->dtunit[11].data.data_int32 =
    pt_gwconf->bleConfig.ble2TxPower;
  //{"INT","BLE1Antenna",},
  pt_msg_format_ctr->dtunit[12].field = 13;
  pt_msg_format_ctr->dtunit[12].data.data_int32 =
    pt_gwconf->bleConfig.ble1Antenna;
  //{"INT","BLE2Antenna", },
  pt_msg_format_ctr->dtunit[13].field = 14;
  pt_msg_format_ctr->dtunit[13].data.data_int32 =
    pt_gwconf->bleConfig.ble2Antenna;
  //{"TEXT","BLEFilterListNameNumber",},
  pt_msg_format_ctr->dtunit[14].field = 15;
  snprintf(pt_msg_format_ctr->dtunit[14].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%d", pt_gwconf->childrenNumber);
  //{"INT","BLEDuplicatedDrop",},
  pt_msg_format_ctr->dtunit[15].field = 16;
  pt_msg_format_ctr->dtunit[15].data.data_int32 = 123;
  //{"BOOLEAN","timeStamp", },
  pt_msg_format_ctr->dtunit[16].field = 17;
  pt_msg_format_ctr->dtunit[16].data.data_int8 = 1;
  //{"INT","timeSetting",},
  pt_msg_format_ctr->dtunit[17].field = 18;
  pt_msg_format_ctr->dtunit[17].data.data_int32 =
    pt_gwconf->timeConfig.timeSetting;
  //{"TEXT","timeNTPUrl",   },
  pt_msg_format_ctr->dtunit[18].field = 19;
  snprintf(pt_msg_format_ctr->dtunit[18].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%d",
           pt_gwconf->timeConfig.timeNtpUrl);
  //{"INT","MQTTSecurityType",},
  pt_msg_format_ctr->dtunit[19].field = 20;
  pt_msg_format_ctr->dtunit[19].data.data_int32 =
    pt_gwconf->mqttConfig.mqttSecurityType;
  //{"TEXT","MQTTHost",},
  pt_msg_format_ctr->dtunit[20].field = 21;
  snprintf(pt_msg_format_ctr->dtunit[20].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s",
           pt_gwconf->mqttConfig.MQTTHost);
  //{"INT","MQTTPort",},
  pt_msg_format_ctr->dtunit[21].field = 22;
  pt_msg_format_ctr->dtunit[21].data.data_int32 =
    pt_gwconf->mqttConfig.MQTTPort;
  //{"BOOLEAN","DHCPEnable",},
  pt_msg_format_ctr->dtunit[22].field = 23;
  pt_msg_format_ctr->dtunit[22].data.data_int8 =
    pt_gwconf->ipConfig.DHCPEnable;
  //{"TEXT","staticIPAddr",},
  pt_msg_format_ctr->dtunit[23].field = 24;
  snprintf(pt_msg_format_ctr->dtunit[23].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%d.%d.%d.%d", \
           pt_gwconf->ipConfig.staticIPAddr.addr.addrArray[0], \
           pt_gwconf->ipConfig.staticIPAddr.addr.addrArray[1], \
           pt_gwconf->ipConfig.staticIPAddr.addr.addrArray[2], \
           pt_gwconf->ipConfig.staticIPAddr.addr.addrArray[3]);

  //{"TEXT","netMask", },
  pt_msg_format_ctr->dtunit[24].field = 25;
  snprintf(pt_msg_format_ctr->dtunit[24].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%d.%d.%d.%d", \
           pt_gwconf->ipConfig.netMask.addr.addrArray[0], \
           pt_gwconf->ipConfig.netMask.addr.addrArray[1], \
           pt_gwconf->ipConfig.netMask.addr.addrArray[2], \
           pt_gwconf->ipConfig.netMask.addr.addrArray[3]);
  //{"TEXT","DNSServer",    },
  pt_msg_format_ctr->dtunit[25].field = 26;
  snprintf(pt_msg_format_ctr->dtunit[25].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%d.%d.%d.%d", \
           pt_gwconf->ipConfig.dnsServer.addr.addrArray[0], \
           pt_gwconf->ipConfig.dnsServer.addr.addrArray[1], \
           pt_gwconf->ipConfig.dnsServer.addr.addrArray[2], \
           pt_gwconf->ipConfig.dnsServer.addr.addrArray[3]);
  //{"TEXT","Gateway",    },
  pt_msg_format_ctr->dtunit[26].field = 27;
  snprintf(pt_msg_format_ctr->dtunit[26].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%d.%d.%d.%d", \
           pt_gwconf->ipConfig.gatewayAddr.addr.addrArray[0], \
           pt_gwconf->ipConfig.gatewayAddr.addr.addrArray[1], \
           pt_gwconf->ipConfig.gatewayAddr.addr.addrArray[2], \
           pt_gwconf->ipConfig.gatewayAddr.addr.addrArray[3]);

  //
  if(msgbuf_tx_init(pt_msg_format_ctr, GATEWAY_CONFIGURE_TABLE,
                    &pt_msg_format_ctr->msginfo.header.command_or_response.
                    commandMsg.param.
                    sqlite_update_item_param.table))
  {
    DBG_LOG_ERR("fail to init msgbuf");
    tpret = ST_ERR;
    return tpret;
  }
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.itemIdx = 123;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.dataNum = kp_col_num;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;

  for(uint32_t i = 0; i < kp_col_num; i++)
  {
    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
    sqlite_update_item_param.updateData[i %
                                        GW_PRO_MAX_DATA_FIELD_PER_OPERATION
    ].field =
      pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].field;
    memcpy(
      &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_update_item_param.updateData[
        i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data, \
      &pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data,
      sizeof(pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data));
  }
  //  to
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));

  if(msgsnd(pt_db_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
            sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
  {
    tpret = ST_ERR;
    DBG_LOG_ERR("fail to send msg to pro DB, for %s", strerror(errno));
  }
  else
  {
    tpret = ST_OK;
    DBG_LOG_INFO("msg is sent to pro-DB successfully");
  }

  return tpret;
}
static app_state_t
to_update_table_dev_info(
  struct database_operation_controller *pt_db_op_ctr,
  struct msg_format_controller *pt_msg_format_ctr,
  const gwConfig_gwConfig_children_t *pt_sensorinfo)
{
  app_state_t tpret = ST_OK;
  uint32_t kp_col_num = 6;

  if((NULL == pt_db_op_ctr) || (NULL == pt_msg_format_ctr) ||
     (NULL == pt_sensorinfo))
  {
    DBG_LOG_ERR("unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  //{"INT PRIMARY KEY","nameId",},
  pt_msg_format_ctr->dtunit[0].field = 1;
  pt_msg_format_ctr->dtunit[0].data.data_int32 = pt_sensorinfo->nameId;
  //{ "TEXT","name",},
  pt_msg_format_ctr->dtunit[1].field = 2;
  snprintf(pt_msg_format_ctr->dtunit[1].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_sensorinfo->bleName);
  //{ "TEXT","BLESensorName",},
  pt_msg_format_ctr->dtunit[2].field = 3;
  snprintf(pt_msg_format_ctr->dtunit[2].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_sensorinfo->name);
  //{"TEXT","macAddr",},
  pt_msg_format_ctr->dtunit[3].field = 4;
  snprintf(pt_msg_format_ctr->dtunit[3].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%02x:%02x:%x02x:%02x:%02x:%02x", \
           pt_sensorinfo->macAddr.addr.addrArray[0], \
           pt_sensorinfo->macAddr.addr.addrArray[1], \
           pt_sensorinfo->macAddr.addr.addrArray[2], \
           pt_sensorinfo->macAddr.addr.addrArray[3], \
           pt_sensorinfo->macAddr.addr.addrArray[4], \
           pt_sensorinfo->macAddr.addr.addrArray[5]);
  //{ "TEXT","manufacturer",},
  pt_msg_format_ctr->dtunit[4].field = 5;
  snprintf(pt_msg_format_ctr->dtunit[4].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_sensorinfo->manufacturer);
  //{"TEXT","type",},
  pt_msg_format_ctr->dtunit[5].field = 6;
  snprintf(pt_msg_format_ctr->dtunit[5].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_sensorinfo->type);

  if(msgbuf_tx_init(pt_msg_format_ctr, DEVICE_LIST_TABLE,
                    &pt_msg_format_ctr->msginfo.header.command_or_response.
                    commandMsg.param.
                    sqlite_update_item_param.table))
  {
    DBG_LOG_ERR("fail to init msgbuf");
    tpret = ST_ERR;
    return tpret;
  }

  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.itemIdx = 123;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.dataNum = kp_col_num;
  for(uint32_t i = 0; i < kp_col_num; i++)
  {
    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
    sqlite_update_item_param.updateData[i %
                                        GW_PRO_MAX_DATA_FIELD_PER_OPERATION
    ].field =
      pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].field;
    memcpy(
      &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_update_item_param.updateData[
        i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data,
      &pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data, \
      sizeof(pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data));
  }

  //
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));
  if(msgsnd(pt_db_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
            sizeof(gw_pro_message_header_t), IPC_NOWAIT) < 0)
  {
    tpret = ST_ERR;
    DBG_LOG_ERR("fail to send msg ,for %s", strerror(errno));
  }
  else
  {
    tpret = ST_OK;
    DBG_LOG_INFO("msg is put on queue successfully");
  }

  return tpret;
}
/*
 */
app_state_t
app_db_operation_test(const gw_pro_command_t kp_cmd)
{
  app_state_t tpret = ST_OK;

  struct db_op_msg *pt_op_msg = NULL;
  struct database_operation_controller *pt_db_op_ctr =
    app_db_get_database_op_ctr();

  switch(kp_cmd)
  {
    case gw_pro_command_sqlite_query_item:
    {
      pt_op_msg = (struct db_op_msg *)l_malloc(sizeof(struct db_op_msg));
      if(NULL == pt_op_msg)
      {
        DBG_LOG_ERR("malloc fail");
        tpret = ST_ERR;
        break;
      }
      memset(pt_op_msg, 0, sizeof(struct db_op_msg));

      pt_op_msg->cmd = gw_pro_command_sqlite_query_item;
      pt_op_msg->cmd_para.query_item.itemIdx = 0;
      pt_op_msg->cmd_para.query_item.table =
        GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;

      // put the message on the queue
      pthread_mutex_lock(&pt_db_op_ctr->mtx);
      l_queue_push_tail(pt_db_op_ctr->pt_dtitem_queue, (void *)pt_op_msg);
      pthread_cond_signal(&pt_db_op_ctr->cond_dt_available);
      pthread_mutex_unlock(&pt_db_op_ctr->mtx);

      break;
    }
    case gw_pro_command_sqlite_filter:
    {
      break;
    }
    case gw_pro_command_sqlite_delete_item:
    {
      break;
    }
    default:
    {
      DBG_LOG_ERR("unexpected command");
      tpret = ST_ERR;
      break;
    }
  }
  return tpret;
}
/*to load all sensor data
 */
app_state_t
app_db_load_all_sensor_data(void)
{
  app_state_t tpret = ST_OK;
  uint32_t i = 0, j = 0;

  SKFChina_Common_MeasurementType tp_mtype_array[SZ_DTSELECTION_GROUP];

  const uint32_t kp_sz = sizeof(g_mtype_array) / sizeof(g_mtype_array[0]);

  for(i = 0; i < 3; i++)
  {  // the wave
     //break; //for debug
    while(1)
    {
      tpret = app_froto_send_data_selection_request(&g_mtype_array[i], 1, app_pro_gen_get_gateway_idstr(
                                                      app_pro_gen_get_ctr()),
                                                    app_pro_gen_allocate_msg_seq_no());
      if(ST_OK == tpret)
      {
        sleep(3);
        break;
      }
      else if(ST_BUSSY == tpret)
      {
        DBG_LOG_ERR("fail to retrieve data %d, for busy",
                    g_mtype_array[i]);
        sleep(3);
        continue;
      }
      else
      {
        DBG_LOG_ERR("fail to select data");
        break;
      }
    }
  }

#if (0)
  return tpret;  //for debug
#endif

  // the other
  i = 3;
  j = 0;
  memset(tp_mtype_array, 0, sizeof(tp_mtype_array));
  while(i < kp_sz)
  {
    tp_mtype_array[j % SZ_DTSELECTION_GROUP] = g_mtype_array[i];
    i++;
    j++;
    if((j >= SZ_DTSELECTION_GROUP) || (i >= kp_sz))
    {
      while(1)
      {
        tpret = app_froto_send_data_selection_request(tp_mtype_array, j, app_pro_gen_get_gateway_idstr(
                                                        app_pro_gen_get_ctr()),
                                                      app_pro_gen_allocate_msg_seq_no());
        if(ST_OK == tpret)
        {
          sleep(3);
          break;
        }
        else if(ST_BUSSY == tpret)
        {
          DBG_LOG_ERR("fail to retrieve data for busy");
          sleep(3);
          continue;
        }
        else
        {
          DBG_LOG_ERR("fail to retrieve data");
          break;
        }
      }

      j = 0;
      memset(tp_mtype_array, 0, sizeof(tp_mtype_array));
    }
  }

  DBG_LOG_INFO("All data receivation is done");

  return tpret;
}
/*
 */

static app_state_t
to_fill_dtunit_array_for_sit_data_writing(
  struct msg_format_controller *pt_msg_format_ctr,
  const struct sit_dtpacket *pt_sit_dtpkt)
{
  app_state_t tpret = ST_OK;

  /**/
#if (1)  // for debug only 0830
         //g_sensor_data_idx++;
         //kp_dtnum = 23; // the number of table column
         // to fill the data unit ref@configure_content_t SensorData[]

  //sequence number
  //pt_msg_format_ctr->dtunit[0].field = 1; // NOTE!!! the index start from
  // 1
  //pt_msg_format_ctr->dtunit[0].data.data_uint32 =
  // g_sensor_data_idx;//pt_dtitem->idx;

  //nameID
  pt_msg_format_ctr->dtunit[0].field = 2;  // NOTE!!! the index start from
                                           // 1
  pt_msg_format_ctr->dtunit[0].data.data_uint32 = 0;  //*((uint32_t*)&pt_sit_dtpkt->addr[2]);//pt_dtitem->name_id;
  //SensorName
  pt_msg_format_ctr->dtunit[1].field = 3;
  //snprintf( pt_msg_format_ctr->dtunit[1].data.data_char,
  // GW_PRO_MAX_STRING_LEN_BYTE, "%s",pt_dtitem->sensor_name);
  snprintf(pt_msg_format_ctr->dtunit[1].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s",
           pt_sit_dtpkt->ad_data.local_name);

  //macAddr
  pt_msg_format_ctr->dtunit[2].field = 4;
  snprintf(pt_msg_format_ctr->dtunit[2].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%.2X%.2X%.2X%.2X%.2X%.2X",
           pt_sit_dtpkt->addr[0], pt_sit_dtpkt->addr[1],
           pt_sit_dtpkt->addr[2], pt_sit_dtpkt->addr[3],
           pt_sit_dtpkt->addr[4], pt_sit_dtpkt->addr[5]);  //pt_dtitem->sensor_mac
  //DBG_LOG_ERR("MAC-add string %s",
  // pt_msg_format_ctr->dtunit[2].data.data_char);

  //type
  pt_msg_format_ctr->dtunit[3].field = 5;
  //pt_msg_format_ctr->dtunit[3].data.data_uint32 = pt_dtitem->sensor_type;
  snprintf(pt_msg_format_ctr->dtunit[3].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", "Insight-T");
  //snprintf( pt_msg_format_ctr->dtunit[3].data.data_char,
  // GW_PRO_MAX_STRING_LEN_BYTE, "%s", "BulletSensor");

  //manufacture
  pt_msg_format_ctr->dtunit[4].field = 6;
  snprintf(pt_msg_format_ctr->dtunit[4].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", "SKF");
  //measure-seq-no
  pt_msg_format_ctr->dtunit[5].field = 7;
  pt_msg_format_ctr->dtunit[5].data.data_uint32 = 0;
  //sample time
  pt_msg_format_ctr->dtunit[6].field = 8;
  pt_msg_format_ctr->dtunit[6].data.data_uint32 = pt_sit_dtpkt->tstamp;
  //Recicved TS
  pt_msg_format_ctr->dtunit[7].field = 9;
  pt_msg_format_ctr->dtunit[7].data.data_uint32 = pt_sit_dtpkt->tstamp;
  //measurement
  pt_msg_format_ctr->dtunit[8].field = 10;
  pt_msg_format_ctr->dtunit[8].data.data_uint32 = 0;
  //data-type
  pt_msg_format_ctr->dtunit[9].field = 11;
  pt_msg_format_ctr->dtunit[9].data.data_uint32 =
    SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT;
  //format
  pt_msg_format_ctr->dtunit[10].field = 12;
  snprintf(pt_msg_format_ctr->dtunit[10].data.data_char,
           GW_PRO_MAX_STRING_LEN_BYTE, "%s", DT_STORAGE_FORM_DIGIT);
  //range
  pt_msg_format_ctr->dtunit[11].field = 13;
  pt_msg_format_ctr->dtunit[11].data.data_uint32 = 0;
  //unit
  pt_msg_format_ctr->dtunit[12].field = 14;
  pt_msg_format_ctr->dtunit[12].data.data_uint32 =
    SKFChina_Common_Unit_CDEGREE;  // c
  //measurement length in Pts
  pt_msg_format_ctr->dtunit[13].field = 15;
  pt_msg_format_ctr->dtunit[13].data.data_uint32 = 0;
  //total data length
  pt_msg_format_ctr->dtunit[14].field = 16;
  pt_msg_format_ctr->dtunit[14].data.data_uint32 = 0;
  //dimension of data
  pt_msg_format_ctr->dtunit[15].field = 17;
  pt_msg_format_ctr->dtunit[15].data.data_uint32 = 0;
  //data format
  pt_msg_format_ctr->dtunit[16].field = 18;
  pt_msg_format_ctr->dtunit[16].data.data_uint32 =
    SKFChina_Common_Format_FORMAT_FLOAT32;  //SKFChina_Common_Format_FORMAT_INT8;//pt_dtitem->data_format;
  //sample period
  pt_msg_format_ctr->dtunit[17].field = 19;
  pt_msg_format_ctr->dtunit[17].data.data_uint32 = 0;
  //encryption
  pt_msg_format_ctr->dtunit[18].field = 20;
  pt_msg_format_ctr->dtunit[18].data.data_uint32 = 0;

  //value
  pt_msg_format_ctr->dtunit[19].field = 21;
  pt_msg_format_ctr->dtunit[19].data.data_float =
    pt_sit_dtpkt->ad_data.m_tempval;

  // sample-rate
  pt_msg_format_ctr->dtunit[20].field = 22;
  pt_msg_format_ctr->dtunit[20].data.data_float = 0;
  //Product type
  pt_msg_format_ctr->dtunit[21].field = 23;
  pt_msg_format_ctr->dtunit[21].data.data_uint32 =
    SKFChina_Common_ProductType_INSIGHT_T;
  // Sensor type
  pt_msg_format_ctr->dtunit[22].field = 24;
  pt_msg_format_ctr->dtunit[22].data.data_uint32 =
    SKFChina_Common_SensorType_NTC_TEMPERATURE;
#else
#endif

  return tpret;
}
/* to be called to write Insight-T data to database

   NOTE!!! we just try to write, return imediately when fail to write to
      DB. no waiting, no retry
 */
app_state_t
app_db_write_sit_data_to_db(
  struct dbop_controller *pt_dbop_ctr,
  struct msg_format_controller *pt_msg_format_ctr,
  const struct sit_dtpacket *pt_sit_dtpkt)
{
  app_state_t tpret = ST_OK;
  uint32_t kp_dtunit_sz = 0;
  struct database_operation_controller *pt_database_op_ctr = NULL;

  if((NULL == pt_dbop_ctr) || (NULL == pt_msg_format_ctr) ||
     (NULL == pt_sit_dtpkt))
  {
    DBG_LOG_ERR("unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // to clean the formating buffer
  memset(pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

  to_fill_dtunit_array_for_sit_data_writing(pt_msg_format_ctr,
                                            pt_sit_dtpkt);

  //
  pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
  pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);
  pt_msg_format_ctr->msginfo.header.seqNo = app_db_allocate_ipc_seqno(
    pt_dbop_ctr);
  pt_msg_format_ctr->msginfo.header.current_package = 1;
  pt_msg_format_ctr->msginfo.header.total_package = 1;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command
    = gw_pro_command_sqlite_update_item;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.
  commandId = pt_msg_format_ctr->msginfo.header.seqNo;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.itemIdx =
    pt_msg_format_ctr->dtunit[0].data.data_uint32;
  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.table = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;

  kp_dtunit_sz = sizeof(SensorData) / sizeof(SensorData[0]) - 1;  //

  pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
  sqlite_update_item_param.dataNum = kp_dtunit_sz;
  pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;

  for(uint32_t i = 0; i < kp_dtunit_sz; i++)
  {
    pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.
    sqlite_update_item_param.updateData[i %
                                        GW_PRO_MAX_DATA_FIELD_PER_OPERATION
    ].field = \
      pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].field;
    memcpy(
      &pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_update_item_param.updateData[
        i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data, \
      &pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data,
      sizeof(pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data));
  }

  // to fill the msg
  pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
  memcpy(&pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo,
         sizeof(gw_pro_message_header_t));

  // to send the msg
  pt_database_op_ctr = app_db_get_database_op_ctr();
  if(msgsnd(pt_database_op_ctr->msgid_to_db, &pt_msg_format_ctr->ipcmsg,
            sizeof(gw_pro_message_header_t), IPC_NOWAIT) != 0)
  {
    //DBG_LOG_ERR("msgsnd fail, for %s", strerror(errno));
    tpret = ST_ERR;
  }

EXIT:
  return tpret;
}
#endif // Deprecated
