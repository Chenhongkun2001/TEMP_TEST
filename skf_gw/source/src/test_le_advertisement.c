/*
for Bluez device discovery test
*/

#include "stdio.h"

#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG


#if(1)

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>

#include <ell/ell.h>


#include "sys_def.h"
#include "bt_common.h"
#include "sys_timer.h"
#include "bt_advertising.h"

static void do_debug(const char *str, void *user_data)
{
	const char *prefix = user_data;

	l_info("%s%s", prefix, str);
}

static void signal_handler(uint32_t signo, void *user_data)
{
	DBG_LOG_INFO("signal %d is received %d", signo);
	switch (signo) {
	case SIGINT:
	case SIGTERM:
		l_info("Terminate");
		l_main_quit();
		break;
	}
}

static void ready_callback(void *user_data)
{
	l_info("ready");
	DBG_LOG_INFO("Ready");
}

static void disconnect_callback(void *user_data)
{
	DBG_LOG_INFO("Going to call l_main_quit");
	l_main_quit();
}

static void iwd_service_appeared(struct l_dbus *dbus, void *user_data)
{
	DBG_LOG_INFO("============================service appeared");
	l_info("Service appeared");
}

static void iwd_service_disappeared(struct l_dbus *dbus, void *user_data)
{
	DBG_LOG_INFO("=============================service disappeared");
	l_info("Service disappeared");
}

static void bluez_client_connected(struct l_dbus *dbus, void *user_data)
{
	DBG_LOG_INFO("client connected");
	l_info("client connected");
}

static void bluez_client_disconnected(struct l_dbus *dbus, void *user_data)
{
	DBG_LOG_INFO("client disconnected");
	l_info("client disconnected");
}

static void bluez_client_ready(struct l_dbus_client *client, void *user_data)
{
	DBG_LOG_INFO("client ready");
	l_info("client ready");
}

#if(1) // by Victor for TEST
	void app_dbus_message_func_sdmsg (struct l_dbus_message *message, void *user_data)
	{
		DBG_LOG_INFO("You probably need to send message here");
		l_dbus_message_set_arguments( message,"");
		DBG_LOG_INFO("TP96");
	}
	
	void app_dbus_client_proxy_result_func_recvmsg(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
	{
		DBG_LOG_INFO("The result is received");
	}
	void app_dbus_message_func_recvmsg (struct l_dbus_message *message,void *user_data)
	{
		struct l_dbus_message_iter kpiter = {0};
		const char *ptstr = NULL;
		unsigned int tpcnt = 0;
		
		const char *pterror = NULL, *ptxt = NULL;
		if(l_dbus_message_get_error( message, &pterror, &ptxt))
		{
			DBG_LOG_ERR("error = %s", pterror);
			DBG_LOG_ERR("message = %st", ptxt);
			goto ERR;
		}
		if(!l_dbus_message_get_arguments( message, "as", &kpiter))
		{
			DBG_LOG_ERR("=============SHIT");
			goto ERR;
		}
		DBG_LOG_INFO("Try to output the result later");
		//DBG_LOG_INFO("signature %s", result);
		while(l_dbus_message_iter_next_entry( &kpiter, &ptstr))
		{
			tpcnt++;
			DBG_LOG_INFO("idx %d the Name : %s", tpcnt, ptstr);		
		}
		ERR:
			return;
	}
	
	//set up argument to set BT power on
	void app_dbus_message_func_sdmsg_to_set_power_on (struct l_dbus_message *message, void *user_data)
	{
		struct l_dbus_message_builder *ptbuilder = NULL;
		bool tpbval = 1;

		DBG_LOG_INFO("set up message to power on BT");
		ptbuilder = l_dbus_message_builder_new( message);
		l_dbus_message_builder_append_basic( ptbuilder, 's', "org.buez.Adapter1");
		l_dbus_message_builder_append_basic( ptbuilder, 's', "Powered");
		l_dbus_message_builder_append_basic( ptbuilder, 's', &tpbval);
		l_dbus_message_builder_finalize(ptbuilder);
		l_dbus_message_builder_destroy( ptbuilder);		
		//l_dbus_message_set_arguments( message,"");
		DBG_LOG_INFO("TP96");
	}

	
	void app_dbus_message_func_recvmsg_from_set_power_on(struct l_dbus_message *message,void *user_data)
	{
		DBG_LOG_INFO("Hello, there!");
	}
#endif


#if(1)
// for LE-advertisement test
	void app_turn_on_le_advertisement(struct l_dbus *pt_dbus, struct l_dbus_proxy*pt_proxy)
	{
		if((pt_dbus == NULL)||(pt_proxy == NULL))
		{
			DBG_LOG_ERR("unexpected pointer null");
			return;
		}
		
		// to set up the advertising info
		bt_advertising_clear_ad_info();
		//bt_advertising_set_local_name("RPI333");
		//bt_advertising_set_includes_appearance( true);
		//bt_advertising_set_includes_tx_power( true);
		bt_advertising_set_includes_name( true);
		
		// to start the LE advertising
		//bt_advertising_register( pt_dbus, pt_proxy, "on");
		bt_advertising_register( pt_dbus, pt_proxy, bt_advertising_get_arguments_str(IDX_AD_BROADCAST));
	}

#endif


#if(0) 
#else

static struct l_dbus_proxy*g_kp_proxy_adapter = NULL;
static struct l_dbus_proxy*g_kp_proxy_leadvertisement_manager = NULL;

static void proxy_added(struct l_dbus_proxy *proxy, void *user_data)
{
	const char *interface = l_dbus_proxy_get_interface(proxy);
	const char *path = l_dbus_proxy_get_path(proxy);

	//DBG_LOG_INFO("proxy added: 0x%x, %s %s", proxy, path, interface);
	l_info("proxy added: %s %s", path, interface);
	#if(1)
		// turn on the BLE advertising
		if (!strcmp(interface, BT_IF_NM_LEADVERTISING_MANAGER))
		{ // 
			DBG_LOG_INFO("Going to return on LE-advertisement");
			if(g_kp_proxy_adapter)
			{
				DBG_LOG_INFO("Going to set power on BT");
				l_dbus_proxy_set_property( g_kp_proxy_adapter, NULL, NULL, NULL, "Powered", "b", 1);
			}
			else
			{
				DBG_LOG_ERR("Fail to set power on BT");
			}
			g_kp_proxy_leadvertisement_manager = proxy;
			app_turn_on_le_advertisement( sys_init_get_dbus(sys_get_mgt()), proxy);
		}
		else if(!strcmp(interface, BT_IF_NM_ADAPTER))
		{
			g_kp_proxy_adapter = proxy;
		}
	#endif
	
	//add proxy to the list
	sys_add_proxy( &g_app_management, proxy);

	
	return;
	ERR:
		//DBG_LOG_ERR("What the hell!");
		return;
				
}

#endif

static void proxy_removed(struct l_dbus_proxy *proxy, void *user_data)
{
	//DBG_LOG_INFO("proxy removed: %s %s", l_dbus_proxy_get_path(proxy),l_dbus_proxy_get_interface(proxy));	
	//l_info("proxy removed: %s %s", l_dbus_proxy_get_path(proxy),l_dbus_proxy_get_interface(proxy));
	sys_remove_proxy( sys_get_mgt(), proxy);
}

static void property_changed(struct l_dbus_proxy *proxy, const char *name,
				struct l_dbus_message *msg, void *user_data)
{
	#if(0)
		DBG_LOG_INFO("property changed: %s (%s %s)", name,l_dbus_proxy_get_path(proxy),l_dbus_proxy_get_interface(proxy));	
		l_info("property changed: %s (%s %s)", name,l_dbus_proxy_get_path(proxy),l_dbus_proxy_get_interface(proxy));
		DBG_LOG_INFO("proxy:0x%x, interface:0x%x", proxy, l_dbus_proxy_get_interface(proxy));
	#endif
	
	if (!strcmp(name, "Address")) {
		char *str;

		if (!l_dbus_message_get_arguments(msg, "s", &str)) {
			DBG_LOG_ERR("What is this?");
			return;
		}

		//DBG_LOG_INFO("Address:%s", str);
		l_info("   Address: %s", str);
	}
	else if(!strcmp(name, "RSSI"))
	{
		int16_t tp_rssi = 0;
		if(!l_dbus_message_get_arguments( msg,"n", &tp_rssi))
		{
			DBG_LOG_ERR("This is not suppose to happen!");
			return;
		}
		//DBG_LOG_INFO("object_path:%s, new RSSI val:%d", l_dbus_proxy_get_path(proxy), tp_rssi);
	}
	else if(!strcmp(name, "Connected"))
	{
		bool tp_connected = false;
		if(!l_dbus_message_get_arguments( msg, "b", &tp_connected))
		{
			DBG_LOG_ERR("this is not suppose to happen");
			return;
		}
		DBG_LOG_INFO("object:%s, new Connected:%d", l_dbus_proxy_get_path( proxy), tp_connected);
	}
	else
	{
		;
	}
}

#endif


#if(1)
	//l_timeout_create(unsigned int seconds, l_timeout_notify_cb_t callback, void * user_data, l_timeout_destroy_cb_t destroy)
	/*
	typedef void (*l_timeout_notify_cb_t) (struct l_timeout *timeout,void *user_data);*/
	static void tim_cbk_for_le_advertisement(struct l_timeout*timeout, void*user_data)	
	{
		static bool kp_flg_adv = false;
		DBG_LOG_INFO("for LE advertisement test");
		if(kp_flg_adv == false)
		{
			bt_advertising_unregister( sys_init_get_dbus(sys_get_mgt()), g_kp_proxy_leadvertisement_manager);
			kp_flg_adv = true;
		}
		else
		{
			kp_flg_adv = false;
			bt_advertising_register( sys_init_get_dbus(sys_get_mgt()), g_kp_proxy_leadvertisement_manager, "on");
		}
		l_timeout_modify( timeout, 60);
	}
	/*typedef void (*l_timeout_destroy_cb_t) (void *user_data);*/
	static void tim_cbk_destroy_for_le_advertisement(void*user_data)
	{
		DBG_LOG_INFO("for LE advertisement test");
	}
#endif

int main(void)
{
	struct l_dbus_client *ptclient = NULL;
	struct l_dbus *ptdbus = NULL;
	uint32_t kp_iwd_watch_id = 0;
	
	DBG_LOG_INFO("=======This is  the application for SKF_ GW=======\n");


	#if(1) // for debug only
		if(sys_init_app_management(&g_app_management) != ST_OK)
		{
			DBG_LOG_ERR("fail to init app-mamagement");
			goto ERR;
		}
		//sys_dump_bt_wlist((const sys_conf *)&g_app_management.conf);
		//sys_destroy_app_management(&g_app_management);
		//return 0;
	#endif



	if(!l_main_init())
	{
		DBG_LOG_ERR("Fail to init ");
		goto ERR;
	}
	// set log output
	l_log_set_syslog();

	ptdbus = l_dbus_new_default(L_DBUS_SYSTEM_BUS);

	//
	sys_init_set_dbus( sys_get_mgt(), ptdbus);
	
	l_dbus_set_debug( ptdbus, do_debug, "TEST666", NULL);
	l_dbus_set_ready_handler( ptdbus, ready_callback, ptdbus, NULL);
	l_dbus_set_disconnect_handler( ptdbus, disconnect_callback, NULL, NULL);
	
	//service basic watch
	//kp_iwd_watch_id = l_dbus_add_service_watch( ptdbus, "net.connman.iwd", iwd_service_appeared, iwd_service_disappeared, NULL, NULL);
	kp_iwd_watch_id = l_dbus_add_service_watch( ptdbus, "org.bluez", iwd_service_appeared, iwd_service_disappeared, NULL, NULL);


	//proxy example
	ptclient = l_dbus_client_new( ptdbus, "org.bluez", "/org/bluez");

	l_dbus_client_set_connect_handler( ptclient, bluez_client_connected, NULL, NULL);
	l_dbus_client_set_disconnect_handler( ptclient, bluez_client_disconnected, NULL, NULL);
	
	l_dbus_client_set_proxy_handlers( ptclient, proxy_added, proxy_removed, property_changed, NULL, NULL);
	
	l_dbus_client_set_ready_handler( ptclient, bluez_client_ready, NULL, NULL);

	#if(0)// TEST AREA
		l_dbus_method_call( ptdbus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames", app_dbus_message_func_sdmsg, app_dbus_message_func_recvmsg, NULL, NULL);
	#else
		// to power on BT
		DBG_LOG_INFO("Going to set BT power on");
		//l_dbus_method_call( ptdbus, "org.bluez", "/org/bluez/hci0", "org.freedesktop.DBus.Properties", "Set", app_dbus_message_func_sdmsg_to_set_power_on, app_dbus_message_func_recvmsg_from_set_power_on, NULL, NULL);
		struct l_timeout *pt_timer = NULL;
		pt_timer = l_timeout_create(120, tim_cbk_for_le_advertisement, NULL, tim_cbk_destroy_for_le_advertisement);
	#endif	
	
	l_main_run_with_signal( signal_handler, NULL);

	DBG_LOG_INFO("TP666");
	
	l_dbus_remove_watch( ptdbus, kp_iwd_watch_id);
	l_dbus_client_destroy( ptclient);
	l_dbus_destroy(ptdbus);
	l_main_exit();
	
	DBG_LOG_INFO("Going to exit the application");
	return 0;
	
	ERR:
		return -1;
}

















