/**
 * @file    bt_advertising_new.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-18
 * @brief   Header of bt_advertising_new.c
 * @details
 */
#ifndef __BT_ADVERTISING_NEW_H__
#define __BT_ADVERTISING_NEW_H__

#include "sys_def.h"
#include "app_common.h"

#ifndef CONFIG_BT_TURN_OFF_LE_ADVERTISEMENT_TIMEOUT_S
#define CONFIG_BT_TURN_OFF_LE_ADVERTISEMENT_TIMEOUT_S (15) // original : 9. for verify only 
#endif

#ifndef CONFIG_BT_TURN_ON_LE_ADVERTISEMENT_TIMEOUT_S
#define CONFIG_BT_TURN_ON_LE_ADVERTISEMENT_TIMEOUT_S (15) // original : 9
#endif

struct bluetooth_adv_controller
{
  struct l_dbus *dbus;
  struct l_dbus_proxy *proxy_le_adv_manager;
  struct pthread_cond_var cond_var;
  char gw_id_buf[MAX_ID_STR_LENGTH];
};

struct bluetooth_adv_controller *bt_adv_n_get_adv_ctr(void);

void bt_adv_n_init_adv_ctr(struct bluetooth_adv_controller *pt_adv_ctr);
void bt_adv_n_set_dbus_handler(
  struct bluetooth_adv_controller *pt_adv_ctr,
  const struct l_dbus *pt_dbus);

struct l_dbus *bt_adv_n_get_dbus_handler(
  const struct bluetooth_adv_controller *pt_adv_ctr);

app_state_t bt_adv_n_set_adv_manager_proxy(
  struct bluetooth_adv_controller *pt_adv_ctr,
  const struct l_dbus_proxy *adv_proxy);

struct l_dbus_proxy *bt_adv_n_get_adv_manager_proxy(
  const struct bluetooth_adv_controller *pt_adv_ctr);

app_state_t bt_adv_n_set_adv_gwid_string(
  struct bluetooth_adv_controller *pt_adv_ctr,
  const struct l_dbus_proxy *pt_proxy_adapter);
const char *bt_adv_n_get_gwid_str(
  const struct bluetooth_adv_controller *pt_adv_ctr);
void bt_adv_n_turn_on_le_advertisement(
  struct bluetooth_adv_controller *pt_adv_ctr);
void bt_adv_n_turn_off_le_advertisement(
  struct bluetooth_adv_controller *pt_adv_ctr);

bool bt_ad_register_object(struct bluetooth_adv_controller *pt_adv_ctr);
bool bt_ad_register_advertisement(
  struct bluetooth_adv_controller *pt_adv_ctr);
bool bt_ad_unregister_advertisement(
  struct bluetooth_adv_controller *pt_adv_ctr);

#endif /* __BT_ADVERTISING_NEW_H__ */
