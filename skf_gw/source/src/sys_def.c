/**
 * @file    sys_def.c
 * @author  Victor Yan
 * @date    2024-12-11
 * @brief   Implementation of D-Bus application functions.
 * @details
 */
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ell/queue.h>
#include <ell/util.h>
#include <ell/ell.h>
#include <ell/dbus-client.h>

#include "sys_def.h"
#include "util_dbg.h"
#include "bt_common.h"

DBG_LOCAL_LOG_DEBUG

app_management g_app_management = { 0 };

uint64_t dbusRefAddrLowerBound = 0;
uint64_t dbusRefAddrUpperBound = 0;

// The sensor FUOTA information
struct fouta_fw_info g_sys_sensor_fuota_info = { 0 };
// Current sensor version
uint32_t g_sys_sensor_fw_version_info = 0;
// Current gateway configuration and children list
gwconf_and_dev_t g_sys_gwConfig_and_dev;

// TODO: To combine these similar functions
static app_state_t sys_add_ble_proxy_devices(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_add_ble_proxy_adapter(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_add_ble_proxy_gattmanager(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_add_ble_proxy_le_advertisingmanager1(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_add_ble_proxy_gattservices(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_add_ble_proxy_gattcharacteristics(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_add_ble_proxy_gattdescriptors(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);

// TODO: To combine these similar functions
static app_state_t sys_remove_ble_proxy_devices(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_remove_ble_proxy_ctrl(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_remove_ble_proxy_gattmanager(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_remove_ble_proxy_gattservices(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_remove_ble_proxy_gattcharacteristics(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);
static app_state_t sys_remove_ble_proxy_gattdescriptors(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy);

static bool sys_set_proxy_adapter1(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy);
static bool sys_set_proxy_le_adv_manager1(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy);

// typedef void (*l_queue_foreach_func_t) (void *data, void *user_data);
// A callback called by l_queue_foreach to output proxy addresses in the queue
static inline void sys_print_proxy_addr(
  void *ptdata,
  void *pt_user_data);
// typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
// A callback called by l_queue_find to find proxy in the queue
static bool sys_proxy_compare(
  const void *ptdata0,
  const void *ptdata1);
static bool sys_data_compare(
  const void *ptdata0,
  const void *ptdata1,
  const uint32_t kpsize);


/**
 * @brief Set dbus address
 */
app_state_t
sys_init_set_dbus(
  app_management *pt_app_mgt,
  struct l_dbus *pt_dbus)
{
  app_state_t tpret = ST_OK;

  if((pt_app_mgt == NULL) || (pt_dbus == NULL))
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  DBG_LOG_INFO("Dbus address is 0x%llx",
               (uint64_t)pt_dbus);
  pt_app_mgt->pt_dbus = pt_dbus;
  dbusRefAddrLowerBound = (uint64_t)pt_dbus;
  dbusRefAddrUpperBound = (uint64_t)pt_dbus + 0x1000000;

  return tpret;
}
/**
 * @brief Get dbus address
 */
struct l_dbus *
sys_init_get_dbus(app_management *pt_app_mgt)
{
  return pt_app_mgt->pt_dbus;
}
/**
 * @brief A callback called by l_queue_foreach to output proxy addresses
 */
// typedef void (*l_queue_foreach_func_t) (void *data, void *user_data);
static void
sys_print_proxy_addr(
  void *ptdata,
  void *pt_user_data)
{
  struct l_dbus_proxy *pt_proxy = NULL;
  const char *pt_path = NULL;

  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    return;
  }

  pt_proxy = *((struct l_dbus_proxy **)ptdata);
  pt_path = l_dbus_proxy_get_path(pt_proxy);
  if(pt_path == NULL)
  {
    DBG_LOG_ERR("Failed to get path of proxy (@0x%llx)",
                (uint64_t)pt_proxy);
    return;
  }
  DBG_LOG_INFO("proxy addr: 0x%llx, path: %s",
               (uint64_t)(*((struct l_dbus_proxy **)ptdata)),
               pt_path);
}
/**
 * @brief Dump all the proxy in the queues
 */
void
sys_dump_dbus_proxy_queue(const app_management *pt_app_mgt)
{
  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    return;
  }

  // q_ctrl
  pthread_mutex_lock(&pt_app_mgt->mtx_q_ctrl);
  DBG_LOG_DEBUG("q_ctrl, len = %d",
               l_queue_length(pt_app_mgt->pt_q_ctrl));
  if(l_queue_length(pt_app_mgt->pt_q_ctrl) != 0)
  {
    l_queue_foreach(pt_app_mgt->pt_q_ctrl,
                    sys_print_proxy_addr,
                    NULL);
  }
  pthread_mutex_unlock(&pt_app_mgt->mtx_q_ctrl);

  // q_devices
  pthread_mutex_lock(&pt_app_mgt->mtx_q_devices);
  DBG_LOG_DEBUG("q_devices, len = %d",
               l_queue_length(pt_app_mgt->pt_q_devices));
  if(l_queue_length(pt_app_mgt->pt_q_devices) != 0)
  {
    l_queue_foreach(pt_app_mgt->pt_q_devices,
                    sys_print_proxy_addr,
                    NULL);
  }
  pthread_mutex_unlock(&pt_app_mgt->mtx_q_devices);

  //q_local_services
  DBG_LOG_DEBUG("q_local_services, len = %d",
               l_queue_length(pt_app_mgt->pt_q_local_services));
  if(l_queue_length(pt_app_mgt->pt_q_local_services) != 0)
  {
    l_queue_foreach(pt_app_mgt->pt_q_local_services,
                    sys_print_proxy_addr,
                    NULL);
  }

  //q_services
  pthread_mutex_lock(&pt_app_mgt->mtx_q_services);
  DBG_LOG_DEBUG("q_services, len = %d",
               l_queue_length(pt_app_mgt->pt_q_services));
  if(l_queue_length(pt_app_mgt->pt_q_services) != 0)
  {
    l_queue_foreach(pt_app_mgt->pt_q_services, sys_print_proxy_addr, NULL);
  }
  pthread_mutex_unlock(&pt_app_mgt->mtx_q_services);

  //q_characteristics
  pthread_mutex_lock(&pt_app_mgt->mtx_q_characteristic);
  DBG_LOG_DEBUG("q_characteristics, len = %d",
               l_queue_length(pt_app_mgt->pt_q_characteristics));
  if(l_queue_length(pt_app_mgt->pt_q_characteristics) != 0)
  {
    l_queue_foreach(pt_app_mgt->pt_q_characteristics, sys_print_proxy_addr,
                    NULL);
  }
  pthread_mutex_unlock(&pt_app_mgt->mtx_q_characteristic);

  //q_descriptors
  pthread_mutex_lock(&pt_app_mgt->mtx_q_descriptors);
  DBG_LOG_DEBUG("q_descriptors, len = %d",
               l_queue_length(pt_app_mgt->pt_q_descriptors));
  if(l_queue_length(pt_app_mgt->pt_q_descriptors) != 0)
  {
    l_queue_foreach(pt_app_mgt->pt_q_descriptors, sys_print_proxy_addr,
                    NULL);
  }
  pthread_mutex_unlock(&pt_app_mgt->mtx_q_descriptors);

  //q_managers
  pthread_mutex_lock(&pt_app_mgt->mtx_q_managers);
  DBG_LOG_DEBUG("q_managers, len = %d",
               l_queue_length(pt_app_mgt->pt_q_managers));
  if(l_queue_length(pt_app_mgt->pt_q_managers) != 0)
  {
    l_queue_foreach(pt_app_mgt->pt_q_managers,
                    sys_print_proxy_addr,
                    NULL);
  }
  pthread_mutex_unlock(&pt_app_mgt->mtx_q_managers);

  //q_uuids
  DBG_LOG_DEBUG("q_uuids, len = %d",
               l_queue_length(pt_app_mgt->pt_q_uuids));
  if(l_queue_length(pt_app_mgt->pt_q_uuids) != 0)
  {
    l_queue_foreach(pt_app_mgt->pt_q_uuids,
                    sys_print_proxy_addr,
                    NULL);
  }

  return;
}
/**
 * @brief To set message queue identifier
 * @param pt_app_mgt: The handler of application manager
 * @param msgidFromGeneral: The message queue ID from process "general"
 * @param msgidToGeneral: The message queue ID to process "general"
 * @return @p ST_OK if everthing is OK
 */
app_state_t
sys_init_set_msgid(
  app_management *pt_app_mgt,
  int msgidFromGeneral,
  int msgidToGeneral)
{
  app_state_t tpret = ST_OK;

  if((pt_app_mgt == NULL) || (msgidFromGeneral < 0) ||
     (msgidToGeneral < 0))
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  pt_app_mgt->hld_msgid_from_pro_general = msgidFromGeneral;
  pt_app_mgt->hld_msgid_to_pro_general = msgidToGeneral;
  return tpret;
}
/**
 * @brief To get message queue ID of the queue to process "general"
 * @param pt_app_mgt: The handler of application manager
 * @return The message queue ID (-1 means invalid ID)
 */
int
sys_get_msgid_to_pro_general(app_management *pt_app_mgt)
{
  int kp_msgid = -1;

  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    return kp_msgid;
  }

  return pt_app_mgt->hld_msgid_to_pro_general;
}
/**
 * @brief To get message queue ID of the queue from process "general"
 * @param pt_app_mgt: The handler of application manager
 * @return The message queue ID (-1 means invalid ID)
 */
int
sys_get_msgid_from_pro_general(app_management *pt_app_mgt)
{
  int kp_msgid = -1;

  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    return kp_msgid;
  }
  
  return pt_app_mgt->hld_msgid_from_pro_general;
}
/**
 * @brief Initialize the application manager
 */
app_state_t
sys_init_app_management(app_management *kp_app_mgt)
{
  app_state_t tpret = ST_OK;

  if(kp_app_mgt == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }
  memset(kp_app_mgt, 0, sizeof(app_management));

  // To initialize the default controller interfaces
  kp_app_mgt->hld_proxy_adapter1 = NULL;
  kp_app_mgt->hld_proxy_adv_monitor = NULL; // Deprecated
  kp_app_mgt->hld_proxy_le_adv_manager1 = NULL;

  // To initialize queues
  kp_app_mgt->pt_q_ctrl = l_queue_new();
  if(pthread_mutex_init(&kp_app_mgt->mtx_q_ctrl, NULL))
  {
    DBG_LOG_ERR("Failed to init mtx_q_ctrl");
    tpret = ST_ERR;
    return tpret;
  }
  kp_app_mgt->pt_q_characteristics = l_queue_new();
  if(pthread_mutex_init(&kp_app_mgt->mtx_q_characteristic, NULL))
  {
    DBG_LOG_ERR("Failed to init mtx_q_characteristic");
    tpret = ST_ERR;
    return tpret;
  }
  kp_app_mgt->pt_q_descriptors = l_queue_new();
  if(pthread_mutex_init(&kp_app_mgt->mtx_q_descriptors, NULL))
  {
    DBG_LOG_ERR("Failed to init mtx_q_descriptors");
    tpret = ST_ERR;
    return tpret;
  }
  kp_app_mgt->pt_q_managers = l_queue_new();
  if(pthread_mutex_init(&kp_app_mgt->mtx_q_managers, NULL))
  {
    DBG_LOG_ERR("Failed to init mtx_q_managers");
    tpret = ST_ERR;
    return tpret;
  }
  kp_app_mgt->pt_q_services = l_queue_new();
  if(pthread_mutex_init(&kp_app_mgt->mtx_q_services, NULL))
  {
    DBG_LOG_ERR("Failed to init mtx_q_services");
    tpret = ST_ERR;
    return tpret;
  }
  kp_app_mgt->pt_q_devices = l_queue_new();
  if(pthread_mutex_init(&kp_app_mgt->mtx_q_devices, NULL))
  {
    DBG_LOG_ERR("Failed to init mtx_q_devices");
    tpret = ST_ERR;
    return tpret;
  }
  kp_app_mgt->pt_adv_scanning_event_queue = l_queue_new();
  if(pthread_mutex_init(&kp_app_mgt->mtx_adv_scanning_ctr, NULL))
  {
    DBG_LOG_ERR("Failed to init mtx_adv_scanning_ctr");
    tpret = ST_ERR;
    return tpret;
  }
  if(pthread_condattr_init(&kp_app_mgt->cond_val_adv_scanning_ctr_attr))
  {
    DBG_LOG_ERR("Failed to initialize condition attribute for adv/scanning");
    tpret = ST_ERR;
    return tpret;
  }
  if(pthread_condattr_setclock(&kp_app_mgt->cond_val_adv_scanning_ctr_attr,
                               CLOCK_MONOTONIC))
  {
    DBG_LOG_ERR(
      "Failed to set clock of condition attribute for adv/scanning");
    tpret = ST_ERR;
    return tpret;
  }
  if(pthread_cond_init(&kp_app_mgt->cond_val_adv_scanning_ctr,
                       &kp_app_mgt->cond_val_adv_scanning_ctr_attr))
  {
    DBG_LOG_ERR("Failed to init condition variable for adv/scanning");
    tpret = ST_ERR;
    return tpret;
  }
  kp_app_mgt->pt_q_local_services = l_queue_new();
  kp_app_mgt->pt_q_uuids = l_queue_new();

  // To initialize connection interfaces
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
  {
    kp_app_mgt->hld_connected_proxy[i] = NULL;
  }
  kp_app_mgt->hld_con_proxy_idx = 0;
  kp_app_mgt->hld_con_status_bit = 0;
#else
  kp_app_mgt->hld_connected_proxy = NULL;
#endif
  if(pthread_mutex_init(&kp_app_mgt->mtx_for_connected_proxy, NULL))
  {
    DBG_LOG_ERR("Failed to init mtx_for_connected_proxy");
    tpret = ST_ERR;
    return tpret;
  }
  kp_app_mgt->bt_con_event = BT_EVENT_DISCONNECTION;
  if(pthread_cond_init(&kp_app_mgt->cond_val_con_event, NULL))
  {
    DBG_LOG_ERR("Failed to init condition variable for connect event");
    tpret = ST_ERR;
    return tpret;
  }
  DBG_LOG_DEBUG("Condition variable for connect event is @0x%llx",
                (uint64_t)&kp_app_mgt->cond_val_con_event);
  if(pthread_mutex_init(&kp_app_mgt->mtx_con_event, NULL))
  {
    DBG_LOG_ERR(
      "Failed to init mtx for the condition variable for connect event");
    tpret = ST_ERR;
    return tpret;
  }

  // To initialize the mutex of white list
  if(pthread_mutex_init(&kp_app_mgt->mtx_for_white_list, NULL))
  {
    DBG_LOG_ERR("Failed to init mtx_for_white_list");
    tpret = ST_ERR;
    return tpret;
  }

  // To initialize the mutex of switch role
  if(pthread_mutex_init(&kp_app_mgt->mtx_for_bt_sw, NULL))
  {
    DBG_LOG_ERR("Failed to init mtx_for_bt_sw\n");
    tpret = ST_ERR;
    return tpret;
  }

  // Role: advertising / scanning control
#if 0
  kp_app_mgt->bt_role = BT_ROLE_CLIENT;
#else
  kp_app_mgt->bt_role = BT_ROLE_DEFAULT;
  kp_app_mgt->bt_sw_in_progress = false;
#endif

  // To initialize the IPC message queue ID
  kp_app_mgt->hld_msgid_from_pro_general = -1;
  kp_app_mgt->hld_msgid_to_pro_general = -1;

  return tpret;
}
/**
 * @brief Free the application manager
 */
app_state_t
sys_destroy_app_management(app_management *kp_app_mgt)
{
  app_state_t tpret = ST_OK;

  if(kp_app_mgt == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  // To free queues
  pthread_mutex_lock(&kp_app_mgt->mtx_q_ctrl);
  if(NULL != kp_app_mgt->pt_q_ctrl)
  {
    l_queue_destroy(kp_app_mgt->pt_q_ctrl, l_free);
    kp_app_mgt->pt_q_ctrl = NULL;
  }
  pthread_mutex_unlock(&kp_app_mgt->mtx_q_ctrl);
  pthread_mutex_destroy(&kp_app_mgt->mtx_q_ctrl);

  pthread_mutex_lock(&kp_app_mgt->mtx_q_characteristic);
  if(NULL != kp_app_mgt->pt_q_characteristics)
  {
    l_queue_destroy(kp_app_mgt->pt_q_characteristics, l_free);
    kp_app_mgt->pt_q_characteristics = NULL;
  }
  pthread_mutex_unlock(&kp_app_mgt->mtx_q_characteristic);
  pthread_mutex_destroy(&kp_app_mgt->mtx_q_characteristic);

  pthread_mutex_lock(&kp_app_mgt->mtx_q_descriptors);
  if(NULL != kp_app_mgt->pt_q_descriptors)
  {
    l_queue_destroy(kp_app_mgt->pt_q_descriptors, l_free);
    kp_app_mgt->pt_q_descriptors = NULL;
  }
  pthread_mutex_unlock(&kp_app_mgt->mtx_q_descriptors);
  pthread_mutex_destroy(&kp_app_mgt->mtx_q_descriptors);

  pthread_mutex_lock(&kp_app_mgt->mtx_q_managers);
  if(NULL != kp_app_mgt->pt_q_managers)
  {
    l_queue_destroy(kp_app_mgt->pt_q_managers, l_free);
    kp_app_mgt->pt_q_managers = NULL;
  }
  pthread_mutex_unlock(&kp_app_mgt->mtx_q_managers);
  pthread_mutex_destroy(&kp_app_mgt->mtx_q_managers);

  pthread_mutex_lock(&kp_app_mgt->mtx_q_services);
  if(NULL != kp_app_mgt->pt_q_services)
  {
    l_queue_destroy(kp_app_mgt->pt_q_services, l_free);
    kp_app_mgt->pt_q_services = NULL;
  }
  pthread_mutex_unlock(&kp_app_mgt->mtx_q_services);
  pthread_mutex_destroy(&kp_app_mgt->mtx_q_services);
  
  pthread_mutex_lock(&kp_app_mgt->mtx_q_devices);
  if(NULL != kp_app_mgt->pt_q_devices)
  {
    l_queue_destroy(kp_app_mgt->pt_q_devices, l_free);
    kp_app_mgt->pt_q_devices = NULL;
  }
  pthread_mutex_unlock(&kp_app_mgt->mtx_q_devices);
  pthread_mutex_destroy(&kp_app_mgt->mtx_q_devices);
  
  pthread_mutex_lock(&kp_app_mgt->mtx_adv_scanning_ctr);
  if(NULL != kp_app_mgt->pt_adv_scanning_event_queue)
  {
#if (1)
    printf("++++destroy q_len %d+++++\n", l_queue_length(kp_app_mgt->pt_adv_scanning_event_queue));
#endif
    l_queue_destroy(kp_app_mgt->pt_adv_scanning_event_queue, NULL);
    kp_app_mgt->pt_adv_scanning_event_queue = NULL;
  }
  pthread_cond_destroy(&kp_app_mgt->cond_val_adv_scanning_ctr);
  pthread_mutex_unlock(&kp_app_mgt->mtx_adv_scanning_ctr);
  pthread_mutex_destroy(&kp_app_mgt->mtx_adv_scanning_ctr);
   
  if(NULL != kp_app_mgt->pt_q_local_services)
  {
    l_queue_destroy(kp_app_mgt->pt_q_local_services, l_free);
    kp_app_mgt->pt_q_local_services = NULL;
  }
  if(NULL != kp_app_mgt->pt_q_uuids)
  {
    l_queue_destroy(kp_app_mgt->pt_q_uuids, l_free);
    kp_app_mgt->pt_q_uuids = NULL;
  }

  // To free connection interfaces
  pthread_mutex_destroy(&kp_app_mgt->mtx_for_connected_proxy);
  pthread_mutex_destroy(&kp_app_mgt->mtx_con_event);
  pthread_cond_destroy(&kp_app_mgt->cond_val_con_event);

  // To free the mutex of white list
  pthread_mutex_lock(&kp_app_mgt->mtx_for_white_list);
  // TODO
  pthread_mutex_unlock(&kp_app_mgt->mtx_for_white_list);
  pthread_mutex_destroy(&kp_app_mgt->mtx_for_white_list);
  pthread_mutex_destroy(&kp_app_mgt->mtx_for_bt_sw);
  // make sure they would be reseted in case dbus restarted
  kp_app_mgt->bt_role = BT_ROLE_DEFAULT;
  kp_app_mgt->bt_sw_in_progress = false;

  return tpret;
}
/**
 * @brief To compare the content of memory pointed by @p ptdata0 and
 *        @p ptdata1 , return true if they are identical.
 */
static bool
sys_data_compare(
  const void *ptdata0,
  const void *ptdata1,
  const uint32_t kpsize)
{
  bool tpret = true;
  uint32_t i = 0;

  if((ptdata0 == NULL) || (ptdata1 == NULL))
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = false;
    return tpret;
  }

  for(i = 0; i < kpsize; i++)
  {
    if(((uint8_t *)ptdata0)[i] != ((uint8_t *)ptdata1)[i])
    {
      tpret = false;
      break;
    }
  }

  return tpret;
}
/**
 * @brief A callback called by l_queue_find to find proxy in the queue
 */
// typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
static bool
sys_proxy_compare(
  const void *ptdata0,
  const void *ptdata1)
{
  struct l_dbus_proxy *pt_proxy0 = *((struct l_dbus_proxy **)ptdata0);
  struct l_dbus_proxy *pt_proxy1 = (struct l_dbus_proxy *)ptdata1;

  return sys_data_compare((void *)&pt_proxy0, (void *)&pt_proxy1,
                          sizeof(struct l_dbus_proxy *));
}
/**
 * @brief Add the proxy to the queue. The data of an entry in the queue is
 *        a pointer to the pointer @p pt_proxy .
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_add_ble_proxy_devices(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) != NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has existed in the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = (struct l_dbus_proxy **)l_malloc(sizeof(struct l_dbus_proxy *));
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    return tpret;
  }
  *ptdata = pt_proxy;
  if(!l_queue_push_tail(pt_queue, ptdata))
  {
    DBG_LOG_ERR("Failed to add proxy (@0x%llx) to queue",
                (uint64_t)pt_proxy);
    l_free(ptdata);
    tpret = ST_ERR;
    return tpret;
  }
  return tpret;
}
/**
 * @brief Add the proxy to the queue. The data of an entry in the queue is
 *        a pointer to the pointer @p pt_proxy .
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_add_ble_proxy_adapter(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) != NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has existed in the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = (struct l_dbus_proxy **)l_malloc(sizeof(struct l_dbus_proxy *));
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    return tpret;
  }
  *ptdata = pt_proxy;
  if(!l_queue_push_tail(pt_queue, ptdata))
  {
    DBG_LOG_ERR("Failed to add proxy (@0x%llx) to queue",
                (uint64_t)pt_proxy);
    l_free(ptdata);
    tpret = ST_ERR;
    return tpret;
  }
  return tpret;
}
/**
 * @brief Add the proxy to the queue. The data of an entry in the queue is
 *        a pointer to the pointer @p pt_proxy .
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_add_ble_proxy_gattmanager(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) != NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has existed in the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = (struct l_dbus_proxy **)l_malloc(sizeof(struct l_dbus_proxy *));
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    return tpret;
  }
  *ptdata = pt_proxy;
  if(!l_queue_push_tail(pt_queue, ptdata))
  {
    DBG_LOG_ERR("Failed to add proxy (@0x%llx) to queue",
                (uint64_t)pt_proxy);
    l_free(ptdata);
    tpret = ST_ERR;
    return tpret;
  }
  return tpret;
}
/**
 * @brief Add the proxy to the queue. The data of an entry in the queue is
 *        a pointer to the pointer @p pt_proxy .
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_add_ble_proxy_le_advertisingmanager1(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) != NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has existed in the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = (struct l_dbus_proxy **)l_malloc(sizeof(struct l_dbus_proxy *));
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    return tpret;
  }
  *ptdata = pt_proxy;
  if(!l_queue_push_tail(pt_queue, ptdata))
  {
    DBG_LOG_ERR("Failed to add proxy (@0x%llx) to queue",
                (uint64_t)pt_proxy);
    l_free(ptdata);
    tpret = ST_ERR;
    return tpret;
  }
  return tpret;
}
/**
 * @brief Add the proxy to the queue. The data of an entry in the queue is
 *        a pointer to the pointer @p pt_proxy .
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_add_ble_proxy_gattservices(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) != NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has existed in the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = (struct l_dbus_proxy **)l_malloc(sizeof(struct l_dbus_proxy *));
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    return tpret;
  }
  *ptdata = pt_proxy;
  if(!l_queue_push_tail(pt_queue, ptdata))
  {
    DBG_LOG_ERR("Failed to add proxy (@0x%llx) to queue",
                (uint64_t)pt_proxy);
    l_free(ptdata);
    tpret = ST_ERR;
    return tpret;
  }
  return tpret;
}
/**
 * @brief Add the proxy to the queue. The data of an entry in the queue is
 *        a pointer to the pointer @p pt_proxy .
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_add_ble_proxy_gattcharacteristics(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) != NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has existed in the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = (struct l_dbus_proxy **)l_malloc(sizeof(struct l_dbus_proxy *));
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    return tpret;
  }
  *ptdata = pt_proxy;
  if(!l_queue_push_tail(pt_queue, ptdata))
  {
    DBG_LOG_ERR("Failed to add proxy (@0x%llx) to queue",
                (uint64_t)pt_proxy);
    l_free(ptdata);
    tpret = ST_ERR;
    return tpret;
  }
  return tpret;
}
/**
 * @brief Add the proxy to the queue. The data of an entry in the queue is
 *        a pointer to the pointer @p pt_proxy .
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_add_ble_proxy_gattdescriptors(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) != NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has existed in the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = (struct l_dbus_proxy **)l_malloc(sizeof(struct l_dbus_proxy *));
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    return tpret;
  }
  *ptdata = pt_proxy;
  if(!l_queue_push_tail(pt_queue, ptdata))
  {
    DBG_LOG_ERR("Failed to add proxy (@0x%llx) to queue",
                (uint64_t)pt_proxy);
    l_free(ptdata);
    tpret = ST_ERR;
    return tpret;
  }
  return tpret;
}
/**
 * @brief Set proxy of adapter
 */
static bool
sys_set_proxy_adapter1(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy)
{
  bool tpret = true;
  const char *pt_interface = NULL;

  if((pt_app_mgt == NULL) || (pt_proxy == NULL))
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = false;
    return tpret;
  }

  pt_interface = l_dbus_proxy_get_interface(pt_proxy);
  if(pt_interface == NULL)
  {
    DBG_LOG_ERR("Failed to get proxy interface");
    tpret = false;
    return tpret;
  }
  if(strcmp(pt_interface, BT_IF_NM_ADAPTER))
  {
    DBG_LOG_ERR("Unexpected proxy with interface (%s)", pt_interface);
    tpret = false;
    return tpret;
  }

  if(pt_app_mgt->hld_proxy_adapter1)
  {
    DBG_LOG_WARN("hld_proxy_adapter1 is not NULL");
  }
  pt_app_mgt->hld_proxy_adapter1 = pt_proxy;

  return tpret;
}
/**
 * @brief Get proxy of adapter
 */
struct l_dbus_proxy *
sys_get_proxy_adapter1(app_management *pt_app_mgt)
{
  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    return NULL;
  }
  return pt_app_mgt->hld_proxy_adapter1;
}
/**
 * @brief Set proxy of BLE advertising manager
 */
static bool
sys_set_proxy_le_adv_manager1(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy)
{
  bool tpret = true;
  const char *pt_interface = NULL;

  if((pt_app_mgt == NULL) || (pt_proxy == NULL))
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = false;
    return tpret;
  }

  pt_interface = l_dbus_proxy_get_interface(pt_proxy);
  if(pt_interface == NULL)
  {
    DBG_LOG_ERR("Failed to get proxy interface");
    tpret = false;
    return tpret;
  }
  if(strcmp(pt_interface, BT_IF_NM_LEADVERTISING_MANAGER))
  {
    DBG_LOG_ERR("Unexpected proxy with interface (%s)", pt_interface);
    tpret = false;
    return tpret;
  }

  if(pt_app_mgt->hld_proxy_le_adv_manager1)
  {
    DBG_LOG_WARN("hld_proxy_le_adv_manager1 is not NULL");
  }
  pt_app_mgt->hld_proxy_le_adv_manager1 = pt_proxy;

  return tpret;
}
/**
 * @brief Get proxy of BLE advertising manager
 */
struct l_dbus_proxy *
sys_get_proxy_le_adv_manager1(app_management *pt_app_mgt)
{
  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    return NULL;
  }
  return pt_app_mgt->hld_proxy_le_adv_manager1;
}
/**
 * @brief Set proxy of connected device (set the connected proxy to NULL if
 *        @p pt_proxy is NULL)
 */
bool
sys_set_connected_proxy(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy)
{
  bool tpret = true;

  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = false;
    return tpret;
  }

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pthread_mutex_lock(&pt_app_mgt->mtx_for_connected_proxy);
  bool free_flag = false;
  bool match_flag = false;
  u_int8_t tmp_idx = CONFIG_BLE_CONNECTION_MAX_NUM + 1; // init it to an 'illegal' one first
  if(NULL == pt_proxy)
  {
    // Set to NULL directly ??? to be confirmed
    // Notes: the idx may not be the right one here??? log it out for further debug here
    if(NULL != pt_app_mgt->hld_connected_proxy[pt_app_mgt->hld_con_proxy_idx])
    {
      DBG_LOG_INFO("set to null. idx:%d, proxy@0x%llx", pt_app_mgt->hld_con_proxy_idx,
                    pt_app_mgt->hld_connected_proxy[pt_app_mgt->hld_con_proxy_idx]);
      pt_app_mgt->hld_connected_proxy[pt_app_mgt->hld_con_proxy_idx] = NULL;
      CON_BIT_CLEAR(pt_app_mgt->hld_con_status_bit,pt_app_mgt->hld_con_proxy_idx);
    } // else {}
    // do nothing for 'else' here, avoid deleting proxy unexpectedly
  } else
  {
    for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
    {
      // target proxy was already connected.
      if(pt_proxy == pt_app_mgt->hld_connected_proxy[i])
      {
        match_flag = true;
        // set the idx & flag no matter whether they're set before
        pt_app_mgt->hld_con_proxy_idx = i;
        CON_BIT_SET(pt_app_mgt->hld_con_status_bit,i);
        break;
      }
      // record the idx of the first 'free' item
      if((false == free_flag) && (NULL == pt_app_mgt->hld_connected_proxy[i]))
      {
        tmp_idx = i;
        free_flag = true;
      }
    }
    if(match_flag)
    {
      DBG_LOG_INFO("match_flag:%d,proxy@0x%llx\n", (uint8_t)match_flag, pt_proxy);
    } else if(free_flag)
    {
      DBG_LOG_INFO("new conn proxy@0x%llx set!idx:%d\n", pt_proxy, tmp_idx);
      pt_app_mgt->hld_connected_proxy[tmp_idx] = pt_proxy;
      pt_app_mgt->hld_con_proxy_idx = tmp_idx;
      CON_BIT_SET(pt_app_mgt->hld_con_status_bit,tmp_idx);
    } else
    {
      DBG_LOG_WARN("no more conn for proxy@0x%llx any more\n", pt_proxy);
      tpret = false;
    }
  }
  pthread_mutex_unlock(&pt_app_mgt->mtx_for_connected_proxy);
#else
  pthread_mutex_lock(&pt_app_mgt->mtx_for_connected_proxy);
  if(pt_app_mgt->hld_connected_proxy)
  {
    DBG_LOG_WARN("hld_connected_proxy is not NULL");
  }
  pt_app_mgt->hld_connected_proxy = pt_proxy;
  pthread_mutex_unlock(&pt_app_mgt->mtx_for_connected_proxy);
#endif

  return tpret;
}
/**
 * @brief Get proxy of connected device
 */
struct l_dbus_proxy *
sys_get_connected_proxy(app_management *pt_app_mgt)
{
  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    return NULL;
  }
  if(pt_app_mgt->bt_role == BT_ROLE_CLIENT)
  {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    return pt_app_mgt->hld_connected_proxy[pt_app_mgt->hld_con_proxy_idx];
#else
    return pt_app_mgt->hld_connected_proxy;
#endif
  }
  else if(pt_app_mgt->bt_role == BT_ROLE_SERVER)
  {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    return pt_app_mgt->hld_server_connected_proxy;
#else
    return pt_app_mgt->hld_connected_proxy;
#endif
  }
}
/**
 * @brief Set the flag to indicate whether the gateway is in a connection
 * @param nstate: true means the gateway is in a connection
 */
void
sys_set_con_in_progress_flg(bool nstate)
{
  app_management *pt_app_mgt = sys_get_mgt();

  pt_app_mgt->is_con_in_progress_flg = nstate;
}
/**
 * @brief Get the flag to indicate whether the gateway is in a connection
 *        (true means the gateway is in a connection)
 */
bool
sys_get_con_in_progress_flg(void)
{
  app_management *pt_app_mgt = sys_get_mgt();

  return pt_app_mgt->is_con_in_progress_flg;
}
/**
 * @brief Set the bluetooth role: client/server
 */
void
sys_set_bluetooth_role(
  app_management *pt_app_mgt,
  enum bluetooth_role kp_btrole)
{
  pt_app_mgt->bt_role = kp_btrole;
}
/**
 * @brief Get the bluetooth role: client/server
 */
enum bluetooth_role
sys_get_bluetooth_role(app_management *pt_app_mgt)
{
  if(NULL == pt_app_mgt)
  {
    DBG_LOG_ERR("Invalid parameter");
    return BT_ROLE_UNKNOWN;
  }
  return pt_app_mgt->bt_role;
}
/**
 * @brief Set gateway's ID (i.e., the BLE MAC address)
 * @param pt_str The set ID. NB: The ID should be with format of string.
 */
app_state_t
sys_set_gw_id_string(uint8_t *pt_str)
{
  app_state_t tpret = ST_OK;
  app_management *pt_app_mgt = sys_get_mgt();

  if(NULL == pt_str)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }
  if(snprintf(pt_app_mgt->gw_id_str,
              MAX_ID_STR_LENGTH,
              "%s",
              pt_str) < 0)
  {
    DBG_LOG_ERR("Failed to set gateway's ID");
    tpret = ST_ERR;
    return tpret;
  }

  return tpret;
}
/**
 * @brief Get gateway's ID
 * @return gateway's ID with format of string.
 */
char *
sys_get_gw_id_string(void)
{
  app_management *pt_app_mgt = sys_get_mgt();

  return pt_app_mgt->gw_id_str;
}
/**
 * @brief Set the active BLE device
 * @param pt_btaddr BLE MAC address (with format of string) of the active
 *                  device.
 */
void
sys_set_active_bt_device(uint8_t *pt_btaddr)
{
  app_management *pt_app_mgt = sys_get_mgt();
  if(NULL == pt_btaddr)
  {
    DBG_LOG_ERR("Invalid parameter");
    return;
  }

  snprintf(pt_app_mgt->active_bt_device, MAX_BT_ADDRSTR, "%s", pt_btaddr);

  DBG_LOG_INFO("Set %s as active device",
               pt_app_mgt->active_bt_device);
}
/**
 * @brief Get active BLE device
 * @return BLE MAC address (with format of string) of the active device
 */
char *
sys_get_active_bt_device(void)
{
  app_management *pt_app_mgt = sys_get_mgt();

  return pt_app_mgt->active_bt_device;
}
/**
 * @brief Set BLE white list (where devices governed by the gateway are
 *        defined in the list). Copy the list pointed by @p pt_btaddr_array
 *        (with @p kp_array_size elements) to the global object (
 *        @p g_app_management )
 * @param pt_btaddr_array The BLE address array
 * @param kp_array_size The number of elements in the array pointed by
 *                      @p pt_btaddr_array . If @p kp_array_size >
 *                      @p MAX_BT_WHITE_LIST (40), only first
 *                      @p MAX_BT_WHITE_LIST BLE MAC addresses would be
 *                      copied.
 */
void
sys_set_bt_white_list(
  struct bt_addr *pt_btaddr_array,
  uint32_t kp_array_size)
{
  app_management *pt_app_mgt = sys_get_mgt();

  if((pt_btaddr_array == NULL) && (kp_array_size != 0))
  {
    DBG_LOG_ERR("Invalid parameter");
    return;
  }

  memset(pt_app_mgt->white_list, 0,
         MAX_BT_WHITE_LIST * sizeof(struct bt_addr));
  if(kp_array_size == 0)
  {
    return;
  }

  if(kp_array_size > MAX_BT_WHITE_LIST)
  {
    memcpy(pt_app_mgt->white_list, pt_btaddr_array,
           MAX_BT_WHITE_LIST * sizeof(struct bt_addr));
  }
  else
  {
    memcpy(pt_app_mgt->white_list, pt_btaddr_array,
           kp_array_size * sizeof(struct bt_addr));
  }
}
/**
 * @brief Get the first @p kp_array_sz elements in @p white_list of 
 *        @p g_app_management .
 * @param pt_bt_addr_array A pointer to memory to store the returned white
 *                         list
 * @param kp_array_sz The number of the element to get. If @p kp_array_sz >
 *                    @p MAX_BT_WHITE_LIST (40), only @p MAX_BT_WHITE_LIST
 *                    BLE MAC addresses would be returned.
 */
void
sys_get_bt_white_list(
  struct bt_addr *pt_bt_addr_array,
  uint32_t kp_array_sz)
{
  app_management *pt_app_mgt = sys_get_mgt();
  const uint32_t kp_sz = MAX_BT_WHITE_LIST >
                         kp_array_sz ? kp_array_sz : MAX_BT_WHITE_LIST;

  if(kp_array_sz == 0)
  {
    return;
  }

  if(NULL == pt_bt_addr_array)
  {
    DBG_LOG_ERR("Invalid parameter");
    return;
  }
  
  memcpy(pt_bt_addr_array, pt_app_mgt->white_list,
         kp_sz * sizeof(struct bt_addr));
}
/**
 * @brief Add the proxy in dbus to the queue
 * @return @p ST_OK when everything is OK.
 */
app_state_t
sys_add_proxy(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy)
{
  const char *pt_interface = NULL;
  const char *pt_path = NULL;

  enum bluetooth_event kp_btevent = BT_EVENT_UNDEFINED;
  app_state_t tp_bval = ST_OK;
  app_state_t tpret = ST_OK;

  if((pt_app_mgt == NULL) || (pt_proxy == NULL))
  {
    tpret = ST_ERR;
    DBG_LOG_ERR("Invalid parameter");
    goto EXIT;
  }

  pt_interface = l_dbus_proxy_get_interface(pt_proxy);
  pt_path = l_dbus_proxy_get_path(pt_proxy);

#if (1)
  DBG_LOG_INFO("Proxy added (@0x%llx), %s, %s",
               (uint64_t)pt_proxy,
               pt_path,
               pt_interface);
#endif

  if(!strcmp(pt_interface, "org.bluez.Device1"))
  {
#if (SKF_GW_NEW == 1)
    if(pt_app_mgt->bt_role == BT_ROLE_SERVER)
    {
#endif
      // If it's a proxy of device
      if(((((uint64_t)pt_proxy) > dbusRefAddrUpperBound) ||
          (((uint64_t)pt_proxy) < dbusRefAddrLowerBound)) &&
        (pt_app_mgt->bt_role == BT_ROLE_CLIENT))
      {
        DBG_LOG_WARN("dbusRefAddrUpperBound: 0x%llx",
                    dbusRefAddrUpperBound);
        DBG_LOG_WARN("dbusRefAddrLowerBound: 0x%llx",
                    dbusRefAddrLowerBound);
        DBG_LOG_WARN("pt_proxy: 0x%llx", (uint64_t)pt_proxy);
        DBG_LOG_WARN("Unexpected pt_proxy address");
#if 0
        tpret = ST_ERR;
        goto EXIT;
#endif
      }

      // Add to the queue (pt_q_devices)
      pthread_mutex_lock(&pt_app_mgt->mtx_q_devices);
      tp_bval = sys_add_ble_proxy_devices(pt_app_mgt->pt_q_devices,
                                          pt_proxy);
      pthread_mutex_unlock(&pt_app_mgt->mtx_q_devices);
      if(tp_bval != ST_OK)
      {
        DBG_LOG_ERR("Failed to add proxy (@0x%llx)", (uint64_t)pt_proxy);
        tpret = ST_ERR;
        goto EXIT;
      }

      // Add the event to the queue (pt_adv_scanning_event_queue)
      // and trigger @p cond_val_adv_scanning_ctr
      // NB: the value of kp_btevent would be passed to the queue as a
      // pointer value because "data" of an entry in
      // @p pt_adv_scanning_event_queue is actually a pointer
      kp_btevent = BT_EVENT_ADD_DEVPROXY; 
      pthread_mutex_lock(&pt_app_mgt->mtx_adv_scanning_ctr);
      l_queue_push_tail(pt_app_mgt->pt_adv_scanning_event_queue,
                        (void *)kp_btevent);
      pthread_cond_signal(&pt_app_mgt->cond_val_adv_scanning_ctr);
      pthread_mutex_unlock(&pt_app_mgt->mtx_adv_scanning_ctr);
#if (SKF_GW_NEW == 1)
    }
#endif
  }
  else if(!strcmp(pt_interface, "org.bluez.Adapter1"))
  {
    // If it's a proxy of adapter
    // Add to the queue (pt_q_ctrl)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_ctrl);
    tp_bval = sys_add_ble_proxy_adapter(pt_app_mgt->pt_q_ctrl, pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_ctrl);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to add proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }

    // Set the proxy to @p hld_proxy_adapter1 of @p pt_app_mgt
    sys_set_proxy_adapter1(pt_app_mgt, pt_proxy);

    // Add the event to the queue (pt_adv_scanning_event_queue)
    // and trigger @p cond_val_adv_scanning_ctr
    // NB: the value of kp_btevent would be passed to the queue as a
    // pointer value because "data" of an entry in
    // @p pt_adv_scanning_event_queue is actually a pointer
    kp_btevent = BT_EVENT_ADD_ADAPTER;
    pthread_mutex_lock(&pt_app_mgt->mtx_adv_scanning_ctr);
    l_queue_push_tail(pt_app_mgt->pt_adv_scanning_event_queue,
                      (void *)kp_btevent);
    pthread_cond_signal(&pt_app_mgt->cond_val_adv_scanning_ctr);
    pthread_mutex_unlock(&pt_app_mgt->mtx_adv_scanning_ctr);
  }
  else if(!strcmp(pt_interface, "org.bluez.AgentManager1"))
  {
    // If it's a proxy of agent manager
    DBG_LOG_WARN(
      "TODO: Add to the corresponding queue if method like RegisterAgent is required");
  }
  else if(!strcmp(pt_interface, "org.bluez.GattService1"))
  {
    // If it's a proxy of gatt service
    // Add to the queue (pt_q_services)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_services);
    tp_bval = sys_add_ble_proxy_gattservices(pt_app_mgt->pt_q_services,
                                             pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_services);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to add proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }
  }
#if (SKF_GW_NEW != 1)
  else if(!strcmp(pt_interface, "org.bluez.GattCharacteristic1"))
  {
    // If it's a proxy of gatt characteristic
    // Add to the queue (pt_q_characteristics)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_characteristic);
    tp_bval = sys_add_ble_proxy_gattcharacteristics(
      pt_app_mgt->pt_q_characteristics, pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_characteristic);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to add proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }

    // Add the event to the queue (pt_adv_scanning_event_queue)
    // and trigger @p cond_val_adv_scanning_ctr
    // NB: the value of kp_btevent would be passed to the queue as a
    // pointer value because "data" of an entry in
    // @p pt_adv_scanning_event_queue is actually a pointer
    kp_btevent = BT_EVENT_ADD_CHRC_PROXY;
    pthread_mutex_lock(&pt_app_mgt->mtx_adv_scanning_ctr);
    l_queue_push_tail(pt_app_mgt->pt_adv_scanning_event_queue,
                      (void *)kp_btevent);
    pthread_cond_signal(&pt_app_mgt->cond_val_adv_scanning_ctr);
    pthread_mutex_unlock(&pt_app_mgt->mtx_adv_scanning_ctr);
  }
#endif
  else if(!strcmp(pt_interface, "org.bluez.GattDescriptor1"))
  {
    // If it's a proxy of gatt descriptor
    // Add to the queue (mtx_q_descriptors)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_descriptors);
    tp_bval = sys_add_ble_proxy_gattdescriptors(
      pt_app_mgt->pt_q_descriptors,
      pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_descriptors);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to add proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }
  }
  else if(!strcmp(pt_interface, BT_IF_NM_GATTMANAGER))
  {
    // If it's a proxy of gatt manager
    // Add to the queue (pt_q_managers)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_managers);
    tp_bval = sys_add_ble_proxy_gattmanager(pt_app_mgt->pt_q_managers,
                                            pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_managers);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to add proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }
  }
  // skip it to avoid sending BT_EVENT_ADD_LEADV_MGR event twice
#if (APP_PRO_BLE_NEW_ADV_REQUIRED != 1)
  else if(!strcmp(pt_interface, BT_IF_NM_LEADVERTISING_MANAGER))
  {
    // If it's a proxy of BLE advertising manager
    // Add to the queue (pt_q_ctrl)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_ctrl);
    tp_bval = sys_add_ble_proxy_le_advertisingmanager1(
      pt_app_mgt->pt_q_ctrl,
      pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_ctrl);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to add proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }

    // Set the proxy to @p hld_proxy_le_adv_manager1 of @p pt_app_mgt
    sys_set_proxy_le_adv_manager1(pt_app_mgt, pt_proxy);

    // Add the event to the queue (pt_adv_scanning_event_queue)
    // and trigger @p cond_val_adv_scanning_ctr
    // NB: the value of kp_btevent would be passed to the queue as a
    // pointer value because "data" of an entry in
    // @p pt_adv_scanning_event_queue is actually a pointer
    pthread_mutex_lock(&pt_app_mgt->mtx_adv_scanning_ctr);
    kp_btevent = BT_EVENT_ADD_LEADV_MGR;
    l_queue_push_tail(pt_app_mgt->pt_adv_scanning_event_queue,
                      (void *)kp_btevent);
    pthread_cond_signal(&pt_app_mgt->cond_val_adv_scanning_ctr);
    pthread_mutex_unlock(&pt_app_mgt->mtx_adv_scanning_ctr);
  }
#endif  // end of APP_PRO_BLE_NEW_ADV_REQUIRED != 1
  else
  {
    DBG_LOG_WARN("Unknown path: %s interface: %s", pt_path,
                 pt_interface);
    tpret = ST_ERR;
    goto EXIT;
  }
EXIT:
  return tpret;
}
/**
 * @brief Get the BLE manager (@p g_app_management)
 * @return a pointer to the BLE manager (@p g_app_management)
 */
app_management *
sys_get_mgt(void)
{
  return &g_app_management;
}
/**
 * @brief Remove the proxy from the queue.
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_remove_ble_proxy_devices(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) == NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has been removed from the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = l_queue_remove_if(pt_queue,
                             sys_proxy_compare,
                             (const void *)pt_proxy);
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to remove proxy (@0x%llx) from queue",
                (uint64_t)pt_proxy);
    tpret = ST_ERR;
    return tpret;
  }
#if 0
  DBG_LOG_DEBUG("Going to free memory @(0x%llx)", (uint64_t)pt_data);
#endif
  l_free(ptdata);
  return tpret;
}
/**
 * @brief Remove the proxy from the queue.
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_remove_ble_proxy_ctrl(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) == NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has been removed from the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = l_queue_remove_if(pt_queue,
                             sys_proxy_compare,
                             (const void *)pt_proxy);
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to remove proxy (@0x%llx) from queue",
                (uint64_t)pt_proxy);
    tpret = ST_ERR;
    return tpret;
  }
#if 0
  DBG_LOG_DEBUG("Going to free memory @(0x%llx)", (uint64_t)pt_data);
#endif
  l_free(ptdata);
  return tpret;
}
/**
 * @brief Remove the proxy from the queue.
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_remove_ble_proxy_gattmanager(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) == NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has been removed from the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = l_queue_remove_if(pt_queue,
                             sys_proxy_compare,
                             (const void *)pt_proxy);
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to remove proxy (@0x%llx) from queue",
                (uint64_t)pt_proxy);
    tpret = ST_ERR;
    return tpret;
  }
#if 0
  DBG_LOG_DEBUG("Going to free memory @(0x%llx)", (uint64_t)pt_data);
#endif

  l_free(ptdata);
  return tpret;
}
/**
 * @brief Remove the proxy from the queue.
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_remove_ble_proxy_gattservices(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) == NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has been removed from the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = l_queue_remove_if(pt_queue,
                             sys_proxy_compare,
                             (const void *)pt_proxy);
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to remove proxy (@0x%llx) from queue",
                (uint64_t)pt_proxy);
    tpret = ST_ERR;
    return tpret;
  }
#if 0
  DBG_LOG_DEBUG("Going to free memory @(0x%llx)", (uint64_t)pt_data);
#endif
  l_free(ptdata);
  return tpret;
}
/**
 * @brief Remove the proxy from the queue.
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_remove_ble_proxy_gattcharacteristics(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) == NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has been removed from the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = l_queue_remove_if(pt_queue,
                             sys_proxy_compare,
                             (const void *)pt_proxy);
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to remove proxy (@0x%llx) from queue",
                (uint64_t)pt_proxy);
    tpret = ST_ERR;
    return tpret;
  }
#if 0
  DBG_LOG_DEBUG("Going to free memory @(0x%llx)", (uint64_t)pt_data);
#endif
  l_free(ptdata);
  return tpret;
}
/**
 * @brief Remove the proxy from the queue.
 * @return @p ST_OK when everything is OK
 */
static app_state_t
sys_remove_ble_proxy_gattdescriptors(
  struct l_queue *pt_queue,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t tpret = ST_OK;
  struct l_dbus_proxy **ptdata = NULL;

  if(pt_queue == NULL)
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  if(l_queue_find(pt_queue, sys_proxy_compare,
                  (const void *)pt_proxy) == NULL)
  {
    DBG_LOG_WARN("The proxy (@0x%llx) has been removed from the queue",
                 (uint64_t)pt_proxy);
    return tpret;
  }

  ptdata = l_queue_remove_if(pt_queue,
                             sys_proxy_compare,
                             (const void *)pt_proxy);
  if(ptdata == NULL)
  {
    DBG_LOG_ERR("Failed to remove proxy (@0x%llx) from queue",
                (uint64_t)pt_proxy);
    tpret = ST_ERR;
    return tpret;
  }
#if 0
  DBG_LOG_DEBUG("Going to free memory @(0x%llx)", (uint64_t)pt_data);
#endif
  l_free(ptdata);
  return tpret;
}
/**
 * @brief Remove the proxy in dbus from the queue
 * @return @p ST_OK when everything is OK.
 */
app_state_t
sys_remove_proxy(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy)
{
  const char *pt_interface = NULL;
  const char *pt_path = NULL;

  app_state_t tp_bval = ST_OK;
  app_state_t tpret = ST_OK;

  if((pt_app_mgt == NULL) || (pt_proxy == NULL))
  {
    tpret = ST_ERR;
    DBG_LOG_ERR("Invalid parameter");
    goto EXIT;
  }

  pt_interface = l_dbus_proxy_get_interface(pt_proxy);
  pt_path = l_dbus_proxy_get_path(pt_proxy);

#if (1)
  DBG_LOG_INFO("Proxy removed (@0x%llx), %s, %s",
               (uint64_t)pt_proxy,
               pt_path,
               pt_interface);
#endif

#if (SKF_GW_NEW == 1)
  // The bluetooth role check could be skipped here, because
  // in most cases the bluetooth role is no longer BT_ROLE_SERVER
  // when the device removal has been triggered. If the role check 
  // exists here, the proxy cannot be removed from the device
  // queue and segmentation fault happened.
  if((!strcmp(pt_interface, "org.bluez.Device1")) 
      // && (pt_app_mgt->bt_role == BT_ROLE_SERVER)
      )
  {
    pthread_mutex_lock(&pt_app_mgt->mtx_for_connected_proxy);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    if(pt_proxy == pt_app_mgt->hld_server_connected_proxy)
#else
    if(pt_proxy == pt_app_mgt->hld_connected_proxy)
#endif
    {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      pt_app_mgt->hld_server_connected_proxy = NULL;
#else
      pt_app_mgt->hld_connected_proxy = NULL;
#endif
      DBG_LOG_WARN("proxy (@%p) removed in server role\n", pt_proxy);
    }
    pthread_mutex_unlock(&pt_app_mgt->mtx_for_connected_proxy);
    // and remove proxy from the queue (pt_q_devices)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_devices);
    tp_bval = sys_remove_ble_proxy_devices(pt_app_mgt->pt_q_devices,
                                           pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_devices);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to remove proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
    }
    DBG_LOG_INFO("remove proxy (@0x%llx) in server role\n", (uint64_t)pt_proxy);
    goto EXIT;
  }
#endif  /*end of SKF_GW_NEW == 1*/

#if (SKF_GW_NEW != 1)
  if(!strcmp(pt_interface, "org.bluez.Device1"))
  {
    // If it's a proxy of device
    // Remove proxy from the queue (pt_q_devices)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_devices);
    tp_bval = sys_remove_ble_proxy_devices(pt_app_mgt->pt_q_devices,
                                           pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_devices);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to remove proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }
  }
  else if(!strcmp(pt_interface, "org.bluez.Adapter1"))
#else
  if(!strcmp(pt_interface, "org.bluez.Adapter1"))
#endif
  {
    // If it's a proxy of adapter
    // Reset the handler of adapter, when the interface is removed
    pt_app_mgt->hld_proxy_adapter1 = NULL;

    // Remove proxy from the queue (pt_q_ctrl)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_ctrl);
    tp_bval = sys_remove_ble_proxy_ctrl(pt_app_mgt->pt_q_ctrl, pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_ctrl);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to remove proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }
  }
  else if(!strcmp(pt_interface, "org.bluez.AgentManager1"))
  {
    // If it's a proxy of agent manager
    DBG_LOG_WARN(
      "TODO: Remove from the corresponding queue if it has been added to the queue");
  }
  else if(!strcmp(pt_interface, "org.bluez.GattService1"))
  {
    // If it's a proxy of gatt service
    // Remove proxy from the queue (pt_q_services)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_services);
    tp_bval = sys_remove_ble_proxy_gattservices(pt_app_mgt->pt_q_services,
                                                pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_services);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to remove proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }
  }
#if (SKF_GW_NEW != 1)
  else if(!strcmp(pt_interface, "org.bluez.GattCharacteristic1"))
  {
    // If it's a proxy of gatt characteristic
    // Remove proxy from the queue (pt_q_characteristics)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_characteristic);
    tp_bval = sys_remove_ble_proxy_gattcharacteristics(
      pt_app_mgt->pt_q_characteristics, pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_characteristic);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to remove proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }
  }
#endif
  else if(!strcmp(pt_interface, "org.bluez.GattDescriptor1"))
  {
    // If it's a proxy of gatt descriptor
    // Remove proxy from the queue (mtx_q_descriptors)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_descriptors);
    tp_bval = sys_remove_ble_proxy_gattdescriptors(
      pt_app_mgt->pt_q_descriptors,
      pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_descriptors);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to remove proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }
  }
  else if(!strcmp(pt_interface, BT_IF_NM_GATTMANAGER))
  {
    // If it's a proxy of gatt manager
    // Remove from the queue (pt_q_managers)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_managers);
    tp_bval = sys_remove_ble_proxy_gattmanager(pt_app_mgt->pt_q_managers,
                                               pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_managers);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to remove proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }
  }
  else if(!strcmp(pt_interface, BT_IF_NM_LEADVERTISING_MANAGER))
  {
    // If it's a proxy of BLE advertising manager
    // Reset the handler of advertising manager,
    // when the interface is removed
    pt_app_mgt->hld_proxy_le_adv_manager1 = NULL;

    // Remove from the queue (pt_q_ctrl)
    pthread_mutex_lock(&pt_app_mgt->mtx_q_ctrl);
    tp_bval = sys_remove_ble_proxy_ctrl(
      pt_app_mgt->pt_q_ctrl,
      pt_proxy);
    pthread_mutex_unlock(&pt_app_mgt->mtx_q_ctrl);
    if(tp_bval != ST_OK)
    {
      DBG_LOG_ERR("Failed to remove proxy (@0x%llx)", (uint64_t)pt_proxy);
      tpret = ST_ERR;
      goto EXIT;
    }
  }
  else
  {
    DBG_LOG_WARN("Unknown path: %s interface: %s", pt_path,
                 pt_interface);
    tpret = ST_ERR;
    goto EXIT;
  }
EXIT:
  return tpret;
}
/**
 * @brief Update application manager (@p pt_app_mgt) when proerty changed
 * @param pt_app_mgt: The input application manager
 * @param pt_proxy: Proxy of added device
 * @param pt_name: Pointer to the changed property
 * @param pt_msg: Pointer to specific message
 * @param user_data: User data (is not mandatory)
 * @return @ST_OK if everything is OK
 */
app_state_t
sys_property_changed(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_name,
  struct l_dbus_message *pt_msg,
  void *user_data)
{
  app_state_t tpret = ST_OK;
  const char *pt_path = NULL;

  if(!strcmp(pt_name, "Connected"))
  {
    // There is a device get connected
    bool tp_connected = false;
    if(!l_dbus_message_get_arguments(pt_msg, "b", &tp_connected))
    {
      DBG_LOG_ERR("Failed to get property Connected");
      tpret = ST_ERR;
      return tpret;
    }
    pt_path = l_dbus_proxy_get_path(pt_proxy);
    DBG_LOG_INFO("Proxy: %s, connection: %d", pt_path, tp_connected);
    if(tp_connected)
    {
      if(BT_ROLE_SERVER == sys_get_bluetooth_role(pt_app_mgt))
      {
        // when the role is server, we need to report
        // @p BT_EVENT_ADD_DEVPROXY
        // to @p pt_adv_scanning_event_queue when connection happens
        pthread_mutex_lock(&pt_app_mgt->mtx_adv_scanning_ctr);
        l_queue_push_tail(pt_app_mgt->pt_adv_scanning_event_queue,
                          (void *)BT_EVENT_ADD_DEVPROXY);
        pthread_cond_signal(&pt_app_mgt->cond_val_adv_scanning_ctr);
        pthread_mutex_unlock(&pt_app_mgt->mtx_adv_scanning_ctr);
      }
      else if(BT_ROLE_CLIENT == sys_get_bluetooth_role(pt_app_mgt))
      {
        // TODO: The status of being connected should be changed here

        // Currently, the status of being connected is updated to
        // application in a connection reply callback
        // @p msg_reply_for_bt_connect_v2
      }
    }
    else
    {
      // Update the connection progress flag
      // Reset the flag: to indicate no connecting is in progress
      sys_set_con_in_progress_flg(false);

      // Set the connected proxy to NULL
      sys_set_connected_proxy(pt_app_mgt, NULL);

      // Update the connection event
      pthread_mutex_lock(&pt_app_mgt->mtx_con_event);
      pt_app_mgt->bt_con_event = BT_EVENT_DISCONNECTION;
      pthread_mutex_unlock(&pt_app_mgt->mtx_con_event);
      // And to signal the disconnection of BT
      pthread_mutex_lock(&pt_app_mgt->mtx_adv_scanning_ctr);
      l_queue_push_tail(pt_app_mgt->pt_adv_scanning_event_queue,
                        (void *)BT_EVENT_DISCONNECTION);
      pthread_cond_signal(&pt_app_mgt->cond_val_adv_scanning_ctr);
      pthread_mutex_unlock(&pt_app_mgt->mtx_adv_scanning_ctr);

      // TODO: We need to double check whether we can call dbus method 
      // in a dbus method
      bt_common_rm_devices(sys_get_proxy_adapter1(sys_get_mgt()),
                            pt_proxy);

#if (0)
      // when the device is disconnected , we remove the proxy from the
      // device queue
      if(false ==
         sys_remove_ble_proxy_devices(sys_get_devices_queue(sys_get_mgt()),
                                      pt_proxy))
      {
        DBG_LOG_ERR("Failed to remove the proxy of device from the queue");
      }
      else
      {
        DBG_LOG_DEBUG(
          "Device has been removed from the queue as a disconnection happened");
      }
#endif
    }
  }
  else if(!strcmp(pt_name, "RSSI"))
  {  
    // There is a change of RSSI
  }
  else
  {  
    // More ...
  }
  
  return tpret;
}
/**
 * @brief To get the queue of service
 */
struct l_queue *
sys_get_gatt_service_queue(app_management *pt_app_mgt)
{
  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return NULL;
  }
  return pt_app_mgt->pt_q_services;
}
/**
 * @brief To get the queue of descriptor
 */
struct l_queue *
sys_get_gatt_descriptor_queue(app_management *pt_app_mgt)
{
  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return NULL;
  }
  return pt_app_mgt->pt_q_descriptors;
}
/**
 * @brief To get the queue of characteristic
 */
struct l_queue *
sys_get_gatt_characteristic_queue(app_management *pt_app_mgt)
{
  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return NULL;
  }

  return pt_app_mgt->pt_q_characteristics;
}
/**
 * @brief To get the mutex of characteristic
 */
pthread_mutex_t *
sys_get_mtx_for_gatt_chrc_queue(app_management *pt_app_mgt)
{
  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return NULL;
  }
  return &pt_app_mgt->mtx_q_characteristic;
}
/**
 * @brief To get the queue of device
 */
struct l_queue *
sys_get_devices_queue(app_management *pt_app_mgt)
{
  if(pt_app_mgt == NULL)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return NULL;
  }
  return pt_app_mgt->pt_q_devices;
}
/**
 * @brief Create files for IPC
 * @return @p ST_OK if everything is OK.
 */
app_state_t sys_create_file_for_ipc(void)
{
	app_state_t tpret = ST_OK;
	int tpfd = 0;
	tpfd = open( FPATH_FOR_IPC0,O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
	if(tpfd < 0)
	{
		DBG_LOG_ERR("fail to create file %s, for %s", FPATH_FOR_IPC0, strerror(errno));
		tpret = ST_ERR;
		return tpret;
	}
	close(tpfd);
	
	tpfd = open( FPATH_FOR_IPC1,O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
	if(tpfd < 0)
	{
		DBG_LOG_ERR("fail to create file %s, for %s", FPATH_FOR_IPC1, strerror(errno));
		tpret = ST_ERR;
		return tpret;
	}
	close(tpfd);

	return tpret;
}
/**
 * @brief Get the global object of gateway configuration and device list
 *        (@p g_sys_gwConfig_and_dev)
 */
gwconf_and_dev_t *
sys_get_gwConfig_and_dev(void)
{
  return &g_sys_gwConfig_and_dev;
}
/**
 * @brief Get the endianness
 * @return false if little endian
 */
bool
sys_is_platform_big_endian(void)
{
  bool tpret = true;
  uint16_t tpu16 = 1;
  uint8_t *pt_u8 = (uint8_t *)&tpu16;

  if(*pt_u8)
  {
    tpret = false;
  }

  return tpret;
}