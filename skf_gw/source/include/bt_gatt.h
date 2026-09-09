#ifndef __BT_GATT_H
#define __BT_GATT_H
#include <ell/ell.h>

#include "sys_def.h"

#ifndef MAX_ATTR_VAL_LEN 
	#define MAX_ATTR_VAL_LEN	512
#endif

#ifndef NUM_BTCHR_EXPECTED
	#define NUM_BTCHR_EXPECTED (10U) // the number of characteristic we expect
#endif



//data struct for characteristic writing operation
struct attr_value{
	uint8_t *ptbuf;
	uint16_t len; //MAX ref@MAX_ATTR_VAL_LEN
	struct pthread_cond_var *pt_cond_var; // for data writing sychronization
};

struct sock_io {
	struct l_dbus_proxy*proxy;
	struct l_io *io;
	uint16_t mtu;
};

struct sock_io * get_g_notify_io(void);
struct l_io *sock_io_new(int fd, void *user_data);


	struct l_dbus_proxy*bt_gatt_find_characteristic(struct l_queue*pt_queue, const char*pt_uuid);
	//app_state_t bt_gatt_charac_write_value(struct l_dbus_proxy*pt_proxy, const uint8_t *ptdata, const uint16_t kplen);		
	app_state_t bt_gatt_charac_write_value(struct l_dbus_proxy*pt_proxy, const uint8_t *ptdata, const uint16_t kplen, struct pthread_cond_var*pt_cond_var);

	app_state_t bt_gatt_acquire_notify(struct l_dbus_proxy*pt_proxy);
	app_state_t bt_gatt_release_notify(struct l_dbus_proxy*pt_proxy);

	bool bt_gatt_is_notification_acquired(struct l_dbus_proxy*pt_proxy);

	
	app_state_t bt_gatt_register_characteristic(struct l_dbus*pt_dbus, const char*pt_uuid, const char*pt_flagstr,const uint16_t kp_hdval);
	app_state_t bt_gatt_unregister_characteristic(struct l_dbus*pt_dbus, const char*pt_uuid);
	app_state_t bt_gatt_register_service(struct l_dbus *pt_dbus, const char*pt_uuid, const uint16_t kp_handle, bool is_primary);
	app_state_t bt_gatt_unregister_service(struct l_dbus*pt_dbus, const char*pt_uuid);

	app_state_t bt_gatt_register_application( struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy);
	app_state_t bt_gatt_unregister_application(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy);

	
	//void bt_gatt_local_service_send_notification(struct app_management*pt_app_mgt);
	app_state_t bt_gatt_charac_send_notification( app_management *pt_app_mgt,uint8_t *pt_dtbuf, uint32_t kplen, struct pthread_cond_var*pt_cond_var);

	bool bt_gatt_is_btchr_registration_done(struct l_queue *pt_queue);

	
	app_state_t bt_gatt_charac_write_value_v2( app_management *pt_app_mgt, const uint8_t *ptdata, const uint16_t kplen, struct pthread_cond_var*pt_cond_var);
	
#endif

