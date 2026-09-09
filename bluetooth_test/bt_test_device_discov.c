/*
for Bluez device discovery test
*/

#include "stdio.h"

#include "util_dbg.h"


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
	DBG_LOG_INFO("service appeared");
	l_info("Service appeared");
}

static void iwd_service_disappeared(struct l_dbus *dbus, void *user_data)
{
	DBG_LOG_INFO("service disappeared");
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

#if(1) // functions for start discovery
//l_dbus_message_func_t setup
	void app_dbus_msg_setup_for_start_discovery(struct l_dbus_message *message,void *user_data)
	{
		DBG_LOG_INFO("set up message for method start_discovery");
		l_dbus_message_set_arguments( message, "");
	}
	//l_dbus_client_proxy_result_func_t reply,
	void app_dbus_msg_for_start_discovery_reply(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
	{
		DBG_LOG_INFO("hello, there");
	}
	
#endif

#if(1)
// callback to set up scanning filter
void app_dbus_msg_setup_for_scan_filter(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder *ptbuilder = NULL;
	DBG_LOG_INFO("To set up scanning filter");

	ptbuilder = l_dbus_message_builder_new( message);
	l_dbus_message_builder_enter_array( ptbuilder, "{sv}");
	l_dbus_message_builder_enter_dict( ptbuilder, "sv");

	l_dbus_message_builder_append_basic( ptbuilder, 's', "Transport");

	l_dbus_message_builder_enter_variant( ptbuilder, "s");
	l_dbus_message_builder_append_basic( ptbuilder, 's', "le");
	l_dbus_message_builder_leave_variant( ptbuilder);

	l_dbus_message_builder_leave_dict(ptbuilder);
	l_dbus_message_builder_leave_array(ptbuilder);

	l_dbus_message_builder_finalize( ptbuilder);
	l_dbus_message_builder_destroy( ptbuilder);
}
void app_dbus_msg_for_scan_filter_reply(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	DBG_LOG_INFO("Hello there");
	if(l_dbus_message_is_error(result))
	{
		const char *ptname = NULL, *ptdesc = NULL;
		l_dbus_message_get_error( result, &ptname, &ptdesc);
		DBG_LOG_INFO("Fail to set discovery filter:%s, %s", ptname, ptdesc);
	}
	else
	{
		DBG_LOG_INFO("the filter for BLE scanning is set successfully");
	}
}
#endif

static void proxy_added(struct l_dbus_proxy *proxy, void *user_data)
{
	const char *interface = l_dbus_proxy_get_interface(proxy);
	const char *path = l_dbus_proxy_get_path(proxy);

	DBG_LOG_INFO("proxy added:%s %s", path, interface);
	l_info("proxy added: %s %s", path, interface);

	DBG_LOG_WARN("proxy added : %s  , interface :%s", path, interface);

	//if (!strcmp(interface, "org.bluez.Adapter1") ||!strcmp(interface, "org.bluez.Device1"))
	if (!strcmp(interface, "org.bluez.Adapter1"))
	{
		char *str;

		if (!l_dbus_proxy_get_property(proxy, "Address", "s", &str))
		{
			goto ERR;
		}
		DBG_LOG_INFO("Address : %s", str);
		l_info("   Address: %s", str);

		#if(0)
			int flg_disc = 0;
			if(!l_dbus_proxy_get_property(proxy, "Discoverable", "b", &flg_disc))
			{
				DBG_LOG_ERR("fail to get Discoverable property");
				goto ERR;
			}	
			DBG_LOG_INFO("Discoveralbe state 666 : %d", flg_disc);
		#endif

		#if(1)
		struct l_dbus_message_iter kpiter = {0};
		char *ptstr = NULL;
		if(!l_dbus_proxy_get_property( proxy, "UUIDs", "as", &kpiter))
		{
			DBG_LOG_ERR("fail to get UUIDS");
			goto ERR;
		}
		DBG_LOG_INFO("the UUIDS is :");
		while(l_dbus_message_iter_next_entry( &kpiter, &ptstr))
		{
			DBG_LOG_INFO("%s", ptstr);
		}
		//DBG_LOG_INFO("the fist UUID : s", ptastr);
		#endif
		DBG_LOG_INFO("Going to set Power on BT");
		l_dbus_proxy_set_property( proxy, NULL, NULL, NULL, "Powered", "b", 1);
		l_dbus_proxy_method_call( proxy, "SetDiscoveryFilter", app_dbus_msg_setup_for_scan_filter, app_dbus_msg_for_scan_filter_reply,NULL, NULL);
		l_dbus_proxy_method_call( proxy, "StartDiscovery", app_dbus_msg_setup_for_start_discovery, app_dbus_msg_for_start_discovery_reply, NULL, NULL);
	}
	return;
	ERR:
		//DBG_LOG_ERR("What the hell!");
		return;
				
}

static void proxy_removed(struct l_dbus_proxy *proxy, void *user_data)
{
	DBG_LOG_INFO("proxy removed: %s %s", l_dbus_proxy_get_path(proxy),l_dbus_proxy_get_interface(proxy));
	
	l_info("proxy removed: %s %s", l_dbus_proxy_get_path(proxy),
					l_dbus_proxy_get_interface(proxy));
}

static void property_changed(struct l_dbus_proxy *proxy, const char *name,
				struct l_dbus_message *msg, void *user_data)
{
	DBG_LOG_INFO("property changed: %s (%s %s)", name,l_dbus_proxy_get_path(proxy),l_dbus_proxy_get_interface(proxy));
	
	l_info("property changed: %s (%s %s)", name,
					l_dbus_proxy_get_path(proxy),
					l_dbus_proxy_get_interface(proxy));
	
	DBG_LOG_INFO("proxy:0x%x, interface:0x%x", proxy, l_dbus_proxy_get_interface(proxy));

	if (!strcmp(name, "Address")) {
		char *str;

		if (!l_dbus_message_get_arguments(msg, "s", &str)) {
			DBG_LOG_ERR("What is this?");
			return;
		}

		DBG_LOG_INFO("Address:%s", str);
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
		DBG_LOG_INFO("object_path:%s, new RSSI val:%d", l_dbus_proxy_get_path(proxy), tp_rssi);
	}
}

#endif


#if(1) // timer callback
	//l_timeout_notify_cb_t callback
	void fun_timer_callback(struct l_timeout *timeout,void *user_data)
	{
		DBG_LOG_INFO("Output from timer callback, timer_pointer:0x%x", timeout);
		l_timeout_modify( timeout, 100);
	}
	
	//typedef void (*l_timeout_destroy_cb_t) (void *user_data);
	void fun_timer_destroy_callback(void *user_data)
	{
		DBG_LOG_INFO("Output from timer destroy callback");
	}
#endif

int main(void)
{
	struct l_dbus_client *ptclient = NULL;
	struct l_dbus *ptdbus = NULL;
	uint32_t kp_iwd_watch_id = 0;
	
	DBG_LOG_INFO("=======This is  the application for SKF_ GW=======\n");


	if(!l_main_init())
	{
		DBG_LOG_ERR("Fail to init ");
		goto ERR;
	}
	// set log output
	l_log_set_syslog();

	ptdbus = l_dbus_new_default(L_DBUS_SYSTEM_BUS);
	l_dbus_set_debug( ptdbus, do_debug, "TEST666", NULL);
	l_dbus_set_ready_handler( ptdbus, ready_callback, ptdbus, NULL);
	l_dbus_set_disconnect_handler( ptdbus, disconnect_callback, NULL, NULL);
	
	//service basic watch
	kp_iwd_watch_id = l_dbus_add_service_watch( ptdbus, "net.connman.iwd", iwd_service_appeared, iwd_service_disappeared, NULL, NULL);

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
		pt_timer = l_timeout_create(10, fun_timer_callback, NULL, fun_timer_destroy_callback);
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

















