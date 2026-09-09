/**
 * @file    sys_def.h
 * @author  Victor Yan
 * @date    2024-12-11
 * @brief   Declarations, MACROs of D-Bus application 
 *          functions.
 * @details
 */
#ifndef __SYS_DEF_H__
#define __SYS_DEF_H__

#include <stdint.h>
#include <ell/ell.h>

#include "app_config.h"
#include "gwConfig.h"
#include "unittest.h"

// Supported sensors
#ifndef TYPE_STR_INSIGHT_T
#define TYPE_STR_INSIGHT_T "Insight-T"
#endif
#ifndef TYPE_STR_INSIGHT_P
#define TYPE_STR_INSIGHT_P "PredictSensor"
#endif

#ifndef TYPE_STR_INSIGHT_PP
	#define TYPE_STR_INSIGHT_PP "PredictSensorPro"
								 //PredictSensorPro	
#endif



// The max number of sensor that the GW can manage
#ifndef MAX_BT_WHITE_LIST
#ifndef GW_MAX_CHILDREN_NUM
#define MAX_BT_WHITE_LIST (40U)
#else
#define MAX_BT_WHITE_LIST GW_MAX_CHILDREN_NUM
#endif
#endif

// The local name to be advertised when GW run as a BT server
#ifndef NM_SKF_GW_BT_SERVER
#define NM_SKF_GW_BT_SERVER "PGW"
#endif

// A timeout when a connection failed
#ifndef MAX_BT_CON_WAITING_PERIOD
#define MAX_BT_CON_WAITING_PERIOD (30U)
#endif

// The max length of file path
#ifndef MAX_FPATH
#define MAX_FPATH (128U)
#endif

// The data should be reserved time
#ifndef SYS_DATA_RESERVED_TIME_S
#define SYS_DATA_RESERVED_TIME_S (3600 * 24 * 60)
#endif

// Whether collect data sensed by insight-T
// 1: enable insight-T collection
// 0: disable insight-T collection
#ifndef COLLECT_INSIGHT_T
#define COLLECT_INSIGHT_T (0)
#endif

// Whether keep BLE scanning when a sensor has been connected
// to the gateway
// 1: keep scanning in a connection
// 0: no scanning in a connection
#ifndef SCANNING_IN_A_CONNECTION
#define SCANNING_IN_A_CONNECTION (0)
#endif

// The max. packet size (in byte) to send over BLE UART GATT Service
#ifndef FILE_PKT_SIZE
#define FILE_PKT_SIZE (180U)
#endif

// The max length of ID
#ifndef MAX_ID_STR_LENGTH
#define MAX_ID_STR_LENGTH (32U) // 32 bytes
#endif

// The max duration of a BLE advertisement
#ifndef MAX_BT_BROADCAST
#define MAX_BT_BROADCAST (60U)
#endif

// Returned status of rountines
typedef enum
{
  ST_OK = 0,
  ST_ERR = 1,
  ST_BUSSY = 2,
}app_state_t;

// Enum of bluetooth event
enum bluetooth_event
{
  BT_EVENT_UNDEFINED = 0,
  BT_EVENT_DO_NOTHING = 6, // For update the BLE status
  BT_EVENT_DISCONNECTION = 1,
  BT_EVENT_CONNECTION = 2,
  BT_EVENT_RSSI_CHANGED = 3,
  BT_EVENT_SERVICES_RESOLVED = 4,
  BT_EVENT_ADD_ADAPTER = 11,
  
  BT_DBUS_IS_READY = 5,
	BT_DBUS_IS_NOT_READY = 7,
  
  BT_EVENT_SW_TO_SERVER = 30, // Have switched to server role successfully
  BT_EVENT_SW_TO_CLIENT = 31, // Have switched to client role successfully

  // TODO: uncleaned ...
  BT_EVENT_POWERON = 10,
  BT_EVENT_ADD_DEVPROXY = 12,  // Proxy of device is added to dbus
  BT_EVENT_ADD_CHRC_PROXY = 13,  // Proxy of characteristic is added to
                                 // dbus
  BT_EVENT_ADD_LEADV_MGR = 14,  // The le-advertisement manager is added

  BT_EVENT_TO_SCANNING = 20,  // Swtich to BT scanning
  BT_EVENT_TO_ADV = 21,  // Swtich to BT advertising
  BT_EVENT_TO_CON = 22,  // Try to find if it is necessary to connect some
                         // device

  BT_EVENT_SW_TO_DISCONNECT = 32,  // To disconnect the BT connection (in
                                   // the client mode)
};

// Enum of BT role
enum bluetooth_role
{
  BT_ROLE_UNKNOWN = 0,
  BT_ROLE_CLIENT = 1, // The gateway connecting with sensors
  BT_ROLE_SERVER = 2, // The gateway connecting with a mobile phone
};

// The default Bluetooth role
#ifndef BT_ROLE_DEFAULT
#define BT_ROLE_DEFAULT BT_ROLE_CLIENT
#endif


// TODO: This is just a temporary solution
struct fouta_fw_info
{
  uint32_t currentVer;
  uint32_t ver;  // the version info
  uint8_t fpath[MAX_FPATH];  // the filepath to image file to be
                             // disseminated to sensor
};

typedef struct
{
  // Configuration info (gwConfig and dev table)
  pthread_mutex_t mtx_for_gw_conf_info;
  gwConfig_t gw_conf_info;
}gwconf_and_dev_t;



struct bt_addr
{
  uint16_t nap;
  uint8_t uap;
  uint32_t lap;
};



#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
// define some macroes for bit-ops
#define CON_BIT_MASK            (CONFIG_BLE_CONNECTION_MAX_NUM - 1)
#define CON_BIT_SET(a,b)        ((a) = ((a) | (1 << ((b) % (CON_BIT_MASK)))))
#define CON_BIT_CLEAR(a,b)      ((a) = ((a) & ~(1 << ((b) % (CON_BIT_MASK)))))
#define CON_BIT_STATUS_GET(a,b) (((a) & ((1 << ((b) % (CON_BIT_MASK))))) ? true : false)
#endif


typedef struct
{
  // D-BUS
  struct l_dbus *pt_dbus;  // The dbus
  struct l_dbus_proxy *hld_proxy_adapter1;  // proxy of adapter
  struct l_dbus_proxy *hld_proxy_le_adv_manager1;  // proxy of BLE
                                                   // advertising manager
  struct l_dbus_proxy *hld_proxy_adv_monitor;  // Deprecated
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  struct l_dbus_proxy *hld_connected_proxy[CONFIG_BLE_CONNECTION_MAX_NUM];  // The connected proxy (set to
                                            // NULL when disconnected)
  uint8_t hld_con_proxy_idx;
  u_int16_t hld_con_status_bit;
  struct l_dbus_proxy *hld_server_connected_proxy;  // The connected proxy in server role(set to
                                             // NULL when disconnected)
#else
  struct l_dbus_proxy *hld_connected_proxy;  // The connected proxy (set to
                                             // NULL when disconnected)
#endif
  pthread_mutex_t mtx_for_connected_proxy;  // The mutex of the connected
                                            // proxy

  // IPC communication
  int hld_msgid_to_pro_general;  //msg-queue for sending messages to
                                 // process "general"
  int hld_msgid_from_pro_general;  // msg-queue for receiving messages from
                                   // process "general"

  // The active bluetooth device's ID
  uint8_t active_bt_device[MAX_ID_STR_LENGTH];

  // BLE white list
  struct bt_addr white_list[MAX_BT_WHITE_LIST];  // BLE white list governed
                                                 // by the gateway
  pthread_mutex_t mtx_for_white_list;  // The mutex of the white list

  // Gateway's ID
  uint8_t gw_id_str[MAX_ID_STR_LENGTH];

  // Queues
  // A queue to store adv manager / adapters
  struct l_queue *pt_q_ctrl;
  pthread_mutex_t mtx_q_ctrl;
  // A queue to store devices
  struct l_queue *pt_q_devices;
  pthread_mutex_t mtx_q_devices;
  // A queue to store services
  struct l_queue *pt_q_services;
  pthread_mutex_t mtx_q_services;
  // A queue to store characteristics
  struct l_queue *pt_q_characteristics;
  pthread_mutex_t mtx_q_characteristic;
  // A queue to store descriptors
  struct l_queue *pt_q_descriptors;
  pthread_mutex_t mtx_q_descriptors;
  // A queue to store managers
  struct l_queue *pt_q_managers;
  pthread_mutex_t mtx_q_managers;

  // A queue to store local services
  struct l_queue *pt_q_local_services;
  // A queue to store uuids
  struct l_queue *pt_q_uuids;

  // Condition variables for connection
  enum bluetooth_event bt_con_event;
  pthread_cond_t cond_val_con_event;  // To trigger relevant waiting
                                      // threads once connection
                                      // established
  pthread_mutex_t mtx_con_event;  // The mutex for @p cond_val_con_event

  // Event
  struct l_queue *pt_adv_scanning_event_queue;  // Put the event (e.g.,
                                                // BT_EVENT_ADD_ADAPTER) to
                                                // the queue
  pthread_cond_t cond_val_adv_scanning_ctr;
  pthread_condattr_t cond_val_adv_scanning_ctr_attr;
  struct timespec cond_val_adv_scanning_ctr_tv;
  pthread_mutex_t mtx_adv_scanning_ctr;  // Mutex for @p
                                         // cond_val_adv_scanning_ctr and
                                         // @p pt_adv_scanning_event_queue

  // Role 
  // sync-ed with that for general process
  enum bluetooth_role bt_role;
  bool bt_sw_in_progress;  // True when the BT mode switch is in progress
  pthread_mutex_t mtx_for_bt_sw;

  // Others
  // TODO: maybe we can skip it later as it's for 'old' version
  bool is_con_in_progress_flg;  // Set to true when the BT-connection is in
                                // progress, reset when the action of
                                // connecting has finished no matter
                                // whether it is successful or not
}app_management;

extern struct fouta_fw_info g_sys_sensor_fuota_info ;
extern uint32_t g_sys_sensor_fw_version_info;
extern app_management g_app_management;
extern uint64_t dbusRefAddrLowerBound;
extern uint64_t dbusRefAddrUpperBound;

gwconf_and_dev_t *sys_get_gwConfig_and_dev(void);
app_state_t sys_init_set_dbus(
  app_management *pt_app_mgt,
  struct l_dbus *pt_dbus);
struct l_dbus *sys_init_get_dbus(app_management *pt_app_mgt);
app_management *sys_get_mgt(void);
app_state_t sys_add_proxy(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy);
app_state_t sys_remove_proxy(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy);
bool sys_is_platform_big_endian(void);
void sys_dump_dbus_proxy_queue(const app_management *pt_app_mgt);
app_state_t sys_init_set_msgid(
  app_management *pt_app_mgt,
  int msgidFromGeneral,
  int msgidToGeneral);
int sys_get_msgid_to_pro_general(app_management *pt_app_mgt);
int sys_get_msgid_from_pro_general(app_management *pt_app_mgt);
app_state_t sys_init_app_management(app_management *kp_app_mgt);
app_state_t sys_destroy_app_management(app_management *kp_app_mgt);
struct l_queue *sys_get_gatt_service_queue(app_management *pt_app_mgt);
struct l_queue *sys_get_gatt_descriptor_queue(app_management *pt_app_mgt);
struct l_queue *sys_get_gatt_characteristic_queue(
  app_management *pt_app_mgt);
pthread_mutex_t *sys_get_mtx_for_gatt_chrc_queue(
  app_management *pt_app_mgt);
struct l_queue *sys_get_devices_queue(app_management *pt_app_mgt);
struct l_dbus_proxy *sys_get_proxy_adapter1(app_management *pt_app_mgt);
struct l_dbus_proxy *sys_get_proxy_le_adv_manager1(
  app_management *pt_app_mgt);
bool sys_set_connected_proxy(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy);
struct l_dbus_proxy *sys_get_connected_proxy(app_management *pt_app_mgt);
void sys_set_con_in_progress_flg(bool nstate);
bool sys_get_con_in_progress_flg(void);
void sys_set_bluetooth_role(
  app_management *pt_app_mgt,
  enum bluetooth_role kp_btrole);
enum bluetooth_role sys_get_bluetooth_role(app_management *pt_app_mgt);
app_state_t sys_set_gw_id_string(uint8_t *pt_str);
char *sys_get_gw_id_string(void);
void sys_set_active_bt_device(uint8_t *pt_btaddr);
char *sys_get_active_bt_device(void);
void sys_set_bt_white_list(
  struct bt_addr *pt_btaddr_array,
  uint32_t kp_array_size);
void sys_get_bt_white_list(
  struct bt_addr *pt_bt_addr_array,
  uint32_t kp_array_sz);
app_state_t sys_property_changed(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_name,
  struct l_dbus_message *pt_msg,
  void *user_data);
app_state_t sys_init_set_msgid(
  app_management *pt_app_mgt,
  int msgid_in,
  int msgid_out);
app_state_t sys_create_file_for_ipc(void);

#endif /* __SYS_DEF_H__ */


