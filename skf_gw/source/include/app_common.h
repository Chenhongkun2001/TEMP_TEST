/**
 * @file    app_common.h
 * @author  Victor Yan
 * @date    2024-10-17
 * @brief   Header file of app_common.c
 * @details
 */
#ifndef __APP_COMMON_H__
#define __APP_COMMON_H__

#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include "sys_def.h"
#include "app_general_def.h"

#define TIMEOUT_BT_DISCONNECTION_S (30)
#define TIMEOUT_BT_TURNING_OFF_S (30)
#define TIMEOUT_BT_TURNING_ON_S (30)
#define TIMEOUT_BT_CONNECT_S (60)

// move this defination into 'app_general_def.h' file
// as pro_general_controller also refer to it
#if (0)
/**
 * Struct for thread sychronizaiotn 
 */
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
#define CONFIG_COND_VAR_MAGIC_INIT    (0x123456)
#define CONFIG_COND_VAR_MAGIC_DEINIT  (0x876543)
struct pthread_cond_var
{
  bool is_op_done;
  uint32_t para;
  pthread_mutex_t mtx;
  pthread_cond_t cond_var;
  pthread_condattr_t cattr;
  struct timespec tv;
  struct l_dbus_proxy *proxy;
  uint32_t magic;
};
#else
struct pthread_cond_var
{
  bool is_op_done;
  uint32_t para;
  pthread_mutex_t mtx;
  pthread_cond_t cond_var;
  pthread_condattr_t cattr;
  struct timespec tv;
};
#endif  // "SKF_GW_CONFIG_MULTI_CONNECTION == 1"
#endif

/* Data structure for BLE transfer */
struct bluetooth_trans_controller
{
  struct l_queue *pt_data_queue;  // The queue where data to send via BLE
  sem_t sem_data_num;  // The number of the data item on the queue above
  pthread_mutex_t mtx;  // Mutex for atomic operation
};

struct bluetooth_trans_controller *app_com_get_data_trans_ctr(void);
struct bluetooth_trans_controller *app_com_get_notification_trans_ctr(
  void);
app_state_t app_com_init_data_trans_ctr(
  struct bluetooth_trans_controller *pt_data_trans_ctr);
void app_com_destroy_data_trans_ctr(
  struct bluetooth_trans_controller *pt_trans_ctr);
app_state_t app_com_post_to_data_trans_queue(
  struct bluetooth_trans_controller *pt_trans_ctr,
  uint8_t *pt_dtbuf,
  const uint32_t kplen);
struct data_unit *app_com_wait_for_data_trans_queue(
  struct bluetooth_trans_controller *pt_trans_ctr);
void app_com_release_data_unit(struct data_unit *pt_dtunit);

app_state_t app_com_init_pthread_cond_var_ctr(
  struct pthread_cond_var *pt_cond_var_ctr);
app_state_t app_com_deinit_pthread_cond_var_ctr(
  struct pthread_cond_var *pt_cond_var_ctr);

// Disabled as we are using the IPC communication in a block manner
#if 0
struct ipc_msg_sender
{
  // message ID
  int msgid_for_sending;
  // message size
  uint32_t hld_sz_msgunit;
  // The pointer to the message which is going to be sent
  void *pt_msg_to_be_sent;

  // The mutex of the queue
  pthread_mutex_t mtx;
  // The queue of messages which are going to be sent
  struct l_queue *msg_waiting_queue;
};

struct ipc_msg_sender *app_com_get_ipc_msg_sender_for_ble_to_gen(void);
struct ipc_msg_sender *app_com_get_ipc_msg_sender_for_gen_to_db(void);
app_state_t app_com_init_ipc_msg_sender(
  struct ipc_msg_sender *pt_msg_sender,
  int msgid,
  uint32_t sz_msgunit);
app_state_t app_com_deinit_ipc_msg_sender(
  struct ipc_msg_sender *pt_msg_sender);
app_state_t app_com_send_ipc_msg(
  struct ipc_msg_sender *pt_msg_sender,
  void *pt_msg,
  uint32_t msg_size);
#endif
app_state_t app_com_set_network_configuration(
  const gwconf_and_dev_t *pt_gwconf_and_dev);
app_state_t app_com_setup_system_timesyncd(
  const gwconf_and_dev_t *pt_gwconf_and_dev);
app_state_t DH_GetDiskAvailSpaceMB(
  char *pDisk,
  uint64_t *availSpaceMB);
#if 0  // Do NOT support timezone configuration
enum tim_zone_idx
{
  IDX_TIM_ZONE_E8 = 20,
};
app_state_t app_com_set_time_zone(const enum tim_zone_idx zone_idx);
#endif

enum net_id{
	NET_UNKNOWN = 0,
	NET_ETH0 = 1,
	NET_ETH1 = 2,
	NET_LTE = 3,
	NET_WIFI = 4,
};


bool app_com_is_lte_module_attached(void);
app_state_t app_com_set_lte_con_state(const bool nstate);
app_state_t app_com_set_up_route_for_dhcp(const enum net_id net);
app_state_t app_com_set_up_default_route( const enum net_id net, const char*pt_gwip);
app_state_t app_com_set_lte_power_supply(const bool nstate);
uint64_t app_gen_n_get_time_ms(void);
app_state_t DH_GetDiskAvailSpaceMB(char *pDisk,uint64_t *availSpaceMB);

// Deprecated (currently just for test)
app_state_t app_com_set_wwan0_managed(void);



#endif /* __APP_COMMON_H__ */

