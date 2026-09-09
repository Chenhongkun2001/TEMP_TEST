#ifndef __APP_PRO_BLE_H
#define __APP_PRO_BLE_H

#include "sys_def.h"
#include "app_general_def.h"

#ifndef CONFIG_BT_TURN_OFF_LE_ADVERTISEMENT_TIMEOUT
  // #define CONFIG_BT_TURN_OFF_LE_ADVERTISEMENT_TIMEOUT  (3)
  #define CONFIG_BT_TURN_OFF_LE_ADVERTISEMENT_TIMEOUT  (9)
#endif

#ifndef CONFIG_BT_GATT_CHARAC_WAIT_FOR_NOTIFICATION_TIMEOUT
  // #define CONFIG_BT_GATT_CHARAC_WAIT_FOR_NOTIFICATION_TIMEOUT  (5)
  #define CONFIG_BT_GATT_CHARAC_WAIT_FOR_NOTIFICATION_TIMEOUT  (15)
#endif

void app_pro_ble(void);


app_state_t app_pro_hanlder_for_cmd(const struct ipc_msg_v2 *pt_msg);


#endif

