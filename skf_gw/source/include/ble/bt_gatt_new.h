/**
 * @file    bt_gatt_new.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-04-17
 * @brief   Header of bt_gatt_new.c
 * @details
 */
#ifndef __BT_GATT_NEW_H__
#define __BT_GATT_NEW_H__

#include "sys_def.h"
#include "app_common.h"
#include "bt_connection_new.h"

#ifndef EXPECTED_BT_CHR_NUM
#define EXPECTED_BT_CHR_NUM (10U) // The number of BLE peer's
                                  // characteristics we expect
#endif

// BLE services
#ifndef NORDIC_UART_SERVICE_UART
#define NORDIC_UART_SERVICE_UART "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#endif
#ifndef NORDIC_UART_CHARAC_TX_UUID // Server -> Client
#define NORDIC_UART_CHARAC_TX_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#endif
#ifndef NORDIC_UART_CHARAC_RX_UUID // Client -> Server
#define NORDIC_UART_CHARAC_RX_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#endif
#ifndef NORDIC_UART_DESC_TX_UUID
//#define NORDIC_UART_DESC_TX_UUID "0x2902"
#define NORDIC_UART_DESC_TX_UUID "00002902-0000-1000-8000-00805f9b34fb"
#endif
#ifndef BLE_DEV_INFO_SERVICE_UUID
#define BLE_DEV_INFO_SERVICE_UUID "0000180A-0000-1000-8000-00805f9b34fb"
#endif
#ifndef BLE_MANUFACTURER_NM_CHR_UUID
#define BLE_MANUFACTURER_NM_CHR_UUID "00002a29-0000-1000-8000-00805f9b34fb"
#endif
#ifndef BLE_HARDWARE_VERSION_CHR_UUID
#define BLE_HARDWARE_VERSION_CHR_UUID \
        "00002a27-0000-1000-8000-00805f9b34fb"
#endif
#ifndef BLE_FIRMWARE_VERSION_CHR_UUID
#define BLE_FIRMWARE_VERSION_CHR_UUID \
        "00002a26-0000-1000-8000-00805f9b34fb"
#endif

// D-BUS related
#ifndef BT_OBJ_PATH_APP
#define BT_OBJ_PATH_APP "/org/bluez/app_skf_gw"
#endif
#ifndef DBUS_INTERFACE_PROPERTIES
#define DBUS_INTERFACE_PROPERTIES "org.freedesktop.DBus.Properties"
#endif

app_state_t bt_gatt_n_write_data_to_peer_rx(
  struct con_controller *pt_con_ctr,
  struct pthread_cond_var *pt_cond_var,
  const uint8_t *ptdata,
  const uint32_t kplen);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
app_state_t bt_gatt_n_aquire_peer_tx_notification(
  const struct con_controller *pt_con_ctr,
  uint8_t idx);
#else
app_state_t bt_gatt_n_aquire_peer_tx_notification(
  const struct con_controller *pt_con_ctr);
#endif
      
app_state_t bt_gatt_n_register_local_service(
  struct l_dbus *pt_dbus,
  struct l_dbus_proxy *pt_proxy);
app_state_t bt_gatt_n_unregister_local_service(
  struct l_dbus *pt_dbus,
  struct l_dbus_proxy *pt_proxy);
app_state_t bt_gatt_n_write_data_to_local_tx(
  app_management *pt_app_mgt,
  struct pthread_cond_var *pt_cond_var,
  const uint8_t *ptdata,
  const uint32_t kplen);
app_state_t bt_gatt_n_find_connected_dev_proxy(
  app_management *pt_app_mgt,
  bool needNotify);

#endif /* __BT_GATT_NEW_H__ */
