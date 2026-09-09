#ifndef __BT_COMMON_H
#define __BT_COMMON_H

#include <ell/ell.h>

#include "sys_def.h"
#include "app_common.h"

/**/
#ifndef BT_DBUS_DES_BLUEZ	
	#define BT_DBUS_DES_BLUEZ	"org.bluez"
#endif

#ifndef BT_DBUS_DES_BLUEZ_LEADV
 	#define BT_DBUS_DES_BLUEZ_LEADV "org.bluez.LEAdvertisement1"
#endif


#ifndef BT_OBJ_PATH_HCI
	#define BT_OBJ_PATH_HCI	"/org/bluez/hci0"	
#endif

#ifndef BT_OBJ_PATH_ADVERTISING
	#define BT_OBJ_PATH_ADVERTISING	"/org/bluez/advertising"
#endif

#ifndef BT_IF_NM_AGENTMANAGER
	#define BT_IF_NM_AGENTMANAGER	"org.bluez.AgentManager1"
#endif

#ifndef BT_IF_NM_AGENT
	#define BT_IF_NM_AGENT	"org.bluez.Agent1"
#endif

#ifndef BT_IF_NM_HEALTHMANAGER
	#define BT_IF_NM_HEALTHMANAGER	"org.bluez.HealthManager1"
#endif
#ifndef BT_IF_NM_ADAPTER
	#define BT_IF_NM_ADAPTER	"org.bluez.Adapter1"
#endif
#ifndef BT_IF_NM_GATTMANAGER	
	#define BT_IF_NM_GATTMANAGER "org.bluez.GattManager1"
#endif
#ifndef BT_IF_NM_LEADVERTISING_MANAGER
	#define BT_IF_NM_LEADVERTISING_MANAGER "org.bluez.LEAdvertisingManager1"
#endif

#ifndef BT_IF_NM_LEADVERTISEMENT
	#define BT_IF_NM_LEADVERTISEMENT	"org.bluez.LEAdvertisement1" //"org.bluez.LEAdvertisement1"
#endif

#ifndef BT_IF_NM_DEVICE	
	#define BT_IF_NM_DEVICE "org.bluez.Device1"
#endif
#ifndef BT_IF_NM_GATTCHARACTERISTIC // bluetooth interface name - GattCharacteristic
	#define BT_IF_NM_GATTCHARACTERISTIC	"org.bluez.GattCharacteristic1" 
#endif
#ifndef BT_IF_NM_GATTDESCRIPTOR
	#define BT_IF_NM_GATTDESCRIPTOR "org.bluez.GattDescriptor1"
#endif
#ifndef BT_IF_NM_GATTSERVICE
	#define BT_IF_NM_GATTSERVICE	"org.bluez.GattService1"
#endif

#ifndef BT_IF_NM_GATTPROFILE
	#define BT_IF_NM_GATTPROFILE "org.bluez.GattProfile1"
#endif

#ifndef BT_OBJ_PATH_APP
	#define BT_OBJ_PATH_APP "/org/bluez/app_skf_gw"
#endif


#ifndef DBUS_INTERFACE_PROPERTIES
	#define DBUS_INTERFACE_PROPERTIES	"org.freedesktop.DBus.Properties"
#endif

#ifndef DBUS_INTERFACE_INTROSPECTABLE
	#define DBUS_INTERFACE_INTROSPECTABLE "org.freedesktop.DBus.Introspectable"
#endif


app_state_t bt_common_set_powered(struct l_dbus_proxy*pt_proxy, bool kp_newval);

void bt_common_scanning_init(void);
void bt_common_scanning_deinit(void);
app_state_t bt_common_turn_on_sanning(struct l_dbus_proxy*pt_proxy, void*user_data);
app_state_t bt_common_turn_on_scanning(struct l_dbus_proxy *pt_proxy, struct pthread_cond_var *pt_cond_var);
app_state_t bt_common_turn_off_scanning(struct l_dbus_proxy*pt_proxy, struct pthread_cond_var *pt_cond_var);


app_state_t bt_common_get_proxy_addr(struct l_dbus_proxy*pt_proxy, char* ptbuf);

struct l_dbus_proxy* bt_common_to_find_dev_proxy( struct l_queue*pt_queue, const struct bt_addr kp_btaddr);
struct l_dbus_proxy*bt_common_to_find_dev_proxy_by_name(struct l_queue*pt_queue, const char*pt_nmstr);
struct l_dbus_proxy* bt_common_to_find_dev_proxy_by_addrstr(struct l_queue*pt_queue, uint8_t *pt_addrstr);



bool bt_common_ble_charac_readvalue(struct l_dbus_proxy*pt_proxy,const uint16_t kpoffset);


app_state_t bt_common_rm_devices( struct l_dbus_proxy*pt_adapter, struct l_dbus_proxy*pt_proxy);
void refresh_white_list(
  struct bt_addr *list,
  uint32_t list_sz,
  uint32_t currentIdx);

void bt_com_turn_on_le_advertisement(struct l_dbus *pt_dbus, struct l_dbus_proxy*pt_proxy);
void bt_com_turn_off_le_advertisement(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy, struct pthread_cond_var*pt_cond_var);


app_state_t bt_common_set_gateway_id( app_management*pt_app_mgt, struct l_dbus_proxy*pt_adapter);

app_state_t bt_common_swtich_bt_connection( app_management *pt_app_mgt, struct pthread_cond_var *pt_cond_var);


struct l_dbus_proxy*bt_common_to_find_dev_proxy_on_white_list(struct l_queue*pt_queue, uint8_t *pt_addrstr_buf, const uint8_t kp_bufsz );

#endif

