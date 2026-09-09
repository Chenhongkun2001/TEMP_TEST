#ifndef __BT_ADVERTISING_H
#define __BT_ADVERTISING_H

#include <ell/ell.h>
#include <stdint.h>
#include <stdbool.h>
#include "app_common.h"

// The advertising interval
// value * 0.625ms
#ifndef MIN_BT_ADV_INTERVAL	
#define MIN_BT_ADV_INTERVAL	(20U)
#endif
#ifndef MAX_BT_ADV_INTERVAL
#define MAX_BT_ADV_INTERVAL	(10485U)
#endif

//ref@g_ad_arguments[]
#define IDX_AD_ON			(0x0)
#define IDX_AD_PERIPHERAL	(0x02)
#define IDX_AD_BROADCAST	(0x03)


struct ad_data {
	uint8_t data[25];
	uint8_t len;
};

struct service_data {
	char *uuid;
	struct ad_data data;
};

struct manufacturer_data {
	uint16_t id;
	struct ad_data data;
};

struct data {
	uint8_t type;
	struct ad_data data;
};

/*data struct to keep info for BT advertising controlling*/
struct ad{
	bool registered;
	char *type; 
	char *local_name;
	char *secondary;
	uint32_t min_interval;
	uint32_t max_interval;
	uint16_t local_appearance;
	uint16_t duration;
	uint16_t timeout;
	uint16_t discoverable_to;
	char **uuids;
	size_t uuids_len;
	struct service_data service;
	struct manufacturer_data manufacturer;
	struct data data;
	bool discoverable;
	bool tx_power;
	bool name; // set to true to include the local-name, we can overwrite Local-Name by member "*local_name"
	bool appearance;
}; 



struct l_dbus_proxy*bt_ad_find_proxy_for_leadvertisement( struct l_queue*pt_queue);
bool bt_advertising_register_v2(
  struct l_dbus *pt_dbus,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_type,
  struct pthread_cond_var *pt_cond_var);
bool bt_advertising_register(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy, const char *pt_type);
//void bt_advertising_unregister(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy);
//void bt_advertising_unregister(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy, struct pthread_cond_var*pt_cond_var);
void bt_advertising_unregister(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy, struct pthread_cond_var*pt_cond_var);



bool bt_advertising_set_includes_appearance(bool kp_newval);
bool bt_advertising_set_includes_name(bool kp_newval);
bool bt_advertising_set_includes_tx_power(bool kp_newval);
bool bt_advertising_set_local_name(const char*pt_name);
bool bt_advertising_get_nmstr(uint8_t *pt_strbuf,const uint32_t bufsz);

void bt_advertising_clear_ad_info(void);

const char* bt_advertising_get_arguments_str(uint8_t kpidx);
bool bt_advertising_set_adv_interval(uint32_t minval, uint32_t maxval);


#endif

