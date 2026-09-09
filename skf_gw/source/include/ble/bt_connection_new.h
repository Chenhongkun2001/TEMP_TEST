/**
 * @file    app_pro_ble_new.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-02-11
 * @brief   Header of app_pro_ble_new.c
 * @details
 */
#ifndef __BT_CONNECTION_NEW_H__
#define __BT_CONNECTION_NEW_H__

#include <ell/queue.h> // For queue
#include "sys_def.h"
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
#include "app_common.h"
#include "bt_gatt.h"  // for sock_io
#endif

// looks like origianl 30S is not enough for insight-T + multi-con case.
#define BT_CONNECTION_NEW_CONNECT_TIMEOUT_S (60)
#define BT_CONNECTION_NEW_DISCONNECT_TIMEOUT_S (30)

struct con_controller
{
  struct l_queue *dev_queue;
  pthread_mutex_t mtx_for_dev_queue;

  bool is_con_in_process;

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  struct l_dbus_proxy *connected_proxy[CONFIG_BLE_CONNECTION_MAX_NUM];
  struct pthread_cond_var pt_cond_var[CONFIG_BLE_CONNECTION_MAX_NUM];
  uint8_t con_proxy_idx;
  uint16_t con_status_bit;
  uint8_t con_mac_addr[CONFIG_BLE_CONNECTION_MAX_NUM][MAX_ID_STR_LENGTH];
  bool fg_is_services_resolved[CONFIG_BLE_CONNECTION_MAX_NUM]; // True when
                                                               // connection
                                                               // has been
                                                               // established
                                                               // and all
                                                               // the
                                                               // peer's
                                                               // services
                                                               // have been
                                                               // discovered
  uint8_t fg_service_idx;
  pthread_mutex_t mtx_for_service_flg;
  pthread_mutex_t mtx_for_sock_io;
#else
  struct l_dbus_proxy *connected_proxy;
#endif
  pthread_mutex_t mtx_for_connected_proxy;

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  struct l_queue *chr_queue[CONFIG_BLE_CONNECTION_MAX_NUM];
  struct l_dbus_proxy *proxy_uart_chr_rx[CONFIG_BLE_CONNECTION_MAX_NUM];
  struct l_dbus_proxy *proxy_uart_chr_tx[CONFIG_BLE_CONNECTION_MAX_NUM];
  struct l_queue *svr_chr_queue;
  
  // For server role
  struct l_dbus_proxy *svr_proxy_uart_chr_rx;
  struct l_dbus_proxy *svr_proxy_uart_chr_tx;
#else
  struct l_queue *chr_queue;
  struct l_dbus_proxy *proxy_uart_chr_rx;
  struct l_dbus_proxy *proxy_uart_chr_tx;
#endif
  pthread_mutex_t mtx_for_chr_queue;

  // White list and the mutex would be pointed to the white list and the
  // mutex of @p g_app_management
  pthread_mutex_t *mtx_for_white_list;
  struct bt_addr *white_list;
};

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
struct con_method_parameter
{
  struct pthread_cond_var *_pt_cond_var;
  uint8_t idx;
  uint8_t con_mac_addr[MAX_ID_STR_LENGTH];
};

#endif

// connect_stat_data is just for stats.
struct connect_stat_data
{
  struct l_dbus_proxy *_con_proxy;  // Pointer to the proxy of current
                                    // connection
  uint64_t start_con_time_ms; // Start time of current connection in ms.
};
extern struct connect_stat_data g_con_stat_data;

struct dev_queue_ele
{
  time_t timestamp;
  bool in_use;
  struct l_dbus_proxy *proxy;
};

struct con_controller *bt_con_n_get_con_ctr(void);

app_state_t bt_con_n_init_con_controller(
  struct con_controller *pt_con_ctr);
void bt_con_n_deinit_con_controller(struct con_controller *pt_con_ctr);
app_state_t bt_con_n_connect_to_dev(struct con_controller *pt_con_ctr);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
app_state_t bt_con_n_disconnect_from_dev(
  struct con_controller *pt_con_ctr,
  uint8_t idx,
  uint8_t *pt_con_id);
#else
app_state_t bt_con_n_disconnect_from_dev(
  struct con_controller *pt_con_ctr);
#endif
#if 0 // Deprecated (originally called by proxy_added)
app_state_t bt_con_n_append_dev_proxy(
  struct con_controller *pt_con_ctr,
  const struct l_dbus_proxy *pt_dev_proxy);
#else // Deprecated (originally called by proxy_added)
app_state_t bt_con_n_append_dev_proxy(
  struct con_controller *pt_con_ctr,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_name,
  struct l_dbus_message *pt_msg,
  void *user_data);
#endif // Called by property_changed
app_state_t bt_con_n_append_chr_proxy_to_queue(
  struct con_controller *pt_con_ctr,
  const struct l_dbus_proxy *pt_chr_proxy);
app_state_t bt_con_n_remove_dev_proxy(
  struct con_controller *pt_con_ctr,
  const struct l_dbus_proxy *pt_dev_proxy);
app_state_t bt_con_n_remove_chr_proxy_from_queue(
  struct con_controller *pt_con_ctr,
  const struct l_dbus_proxy *pt_chr_proxy);
app_state_t bt_con_n_proxy_property_changed(
  struct con_controller *pt_con_ctr,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_name,
  struct l_dbus_message *pt_msg,
  void *user_data);
app_state_t bt_con_n_service_property_changed(
  struct con_controller *pt_con_ctr,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_name,
  struct l_dbus_message *pt_msg,
  void *user_data);
app_state_t bt_con_n_dev_queue_periodic_refresh(
  struct con_controller *pt_con_ctr);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
app_state_t bt_con_n_connect_num_fetch(
  struct con_controller *pt_con_ctr,
  uint8_t *pt_con_num);
app_state_t bt_con_n_connect_idx_fetch(
  struct con_controller *pt_con_ctr,
  struct l_dbus_proxy *pt_proxy,
  uint8_t *pt_con_idx);
u_int8_t bt_con_n_get_fail_count(void);
void bt_con_n_set_fail_count(uint8_t cnt);
#endif

#endif /* __BT_CONNECTION_NEW_H__ */
