#ifndef __BT_CONNECTION_H
#define __BT_CONNECTION_H

#include <ell/ell.h>

#include "sys_def.h"
#include "bt_common.h"

app_state_t bt_con_connect(struct l_dbus_proxy *pt_proxy, struct pthread_cond_var *pt_cond_var);
app_state_t bt_con_pair(struct l_dbus_proxy*pt_proxy);

bool bt_con_is_dev_paired(struct l_dbus_proxy*pt_proxy);
bool bt_con_is_dev_connected(struct l_dbus_proxy*pt_proxy);


app_state_t bt_con_disconnect(struct l_dbus_proxy*pt_proxy, struct pthread_cond_var*pt_cond_var);

#if (SKF_GW_NEW == 1)
app_state_t bt_con_search_for_sensor_and_connect_v2(
  app_management *pt_app_mgt,
  struct pthread_cond_var *kp_cond_var);
#endif /* SKF_GW_NEW == 1 */

#endif

