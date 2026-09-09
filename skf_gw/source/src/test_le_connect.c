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
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <ell/ell.h>


#include "sys_def.h"
#include "bt_common.h"
#include "sys_timer.h"
#include "bt_connection.h"
#include "bt_agent.h"
#include "bt_gatt.h"
#include "app_local_service.h"
#include "app_general_def.h"

DBG_LOCAL_LOG_DEBUG


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
	exit(EXIT_FAILURE);
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


#if(0) 
// the original definiiton
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

#else

static void proxy_added(struct l_dbus_proxy *proxy, void *user_data)
{
	const char *interface = l_dbus_proxy_get_interface(proxy);
	const char *path = l_dbus_proxy_get_path(proxy);

	// DBG_LOG_DEBUG("proxy added: 0x%llx, %s %s", proxy, path, interface);

	#if(0) // the original
	// turn on the BLE scanning
	if (!strcmp(interface, "org.bluez.Adapter1"))
	{ 
		// to set BT power on
		bt_common_set_powered( proxy, true);
		#if(1)
			// trigger the BLE scanning operation
			bt_common_turn_on_sanning( proxy, NULL);
		#else
			DBG_LOG_WARN("NOTE!!!====================the BT scanning is off");
		#endif
	}
	else if(!strcmp(interface, BT_IF_NM_AGENTMANAGER))
	{ // to register the default agent
		#if(0)
		bt_agent_register( sys_init_get_dbus( sys_get_mgt()), proxy, bt_agent_get_io_capability_string(IDX_IO_NOINPUTNOOUTPUT));
		#else
			#warning "NOTE!!!====the default agent is applied";
			DBG_LOG_WARN("Note!===the default agent is applied!");
		#endif
	}
	else if(!strcmp(interface, BT_IF_NM_GATTMANAGER))
	{
		#if(1)	// for debug
			#warning "Note! to register the local service"
			app_register_local_service(sys_init_get_dbus(sys_get_mgt()), proxy);
		#endif
	}
	//add proxy to the list
	sys_add_proxy( sys_get_mgt(), proxy);
	#else
	// turn on the BLE scanning
	if (!strcmp(interface, BT_IF_NM_ADAPTER))
	{ 
		// to set BT power on
		//bt_common_set_powered( proxy, true);
		#if(1)
			// trigger the BLE scanning operation
			//bt_common_turn_on_sanning( proxy, NULL);
		#else
			DBG_LOG_WARN("NOTE!!!====================the BT scanning is off");
		#endif
		DBG_LOG_INFO("to have the job done @thandler_bt_connection_manager");
	}
	else if(!strcmp(interface, BT_IF_NM_AGENTMANAGER))
	{ // to register the default agent
		#if(0)
		bt_agent_register( sys_init_get_dbus( sys_get_mgt()), proxy, bt_agent_get_io_capability_string(IDX_IO_NOINPUTNOOUTPUT));
		#else
			#warning "NOTE!!!====the default agent is applied";
			DBG_LOG_WARN("Note!===the default agent is applied!");
		#endif
	}
	else if(!strcmp(interface, BT_IF_NM_GATTMANAGER))
	{
		#if(1)	// for debug
			#warning "Note! to register the local service"
			app_register_local_service(sys_init_get_dbus(sys_get_mgt()), proxy);
		#endif
	}
	//add proxy to the list
	sys_add_proxy( sys_get_mgt(), proxy);
	// >>> @20240604 Added by Sean (to filter the unexpected proxy address)
	// if(sys_add_proxy( sys_get_mgt(), proxy) != ST_OK)
	// {
	// 	DBG_LOG_ERR("unexpected pointer, 0x%llx", proxy);
	// 	//tpret = ST_ERR;
	// 	// >>> @20240613 Added by Sean (to fix the "gabage-in-the-list bug")
	// 	bt_common_rm_devices( sys_get_proxy_adapter1(sys_get_mgt()), proxy);
	// 	// <<<
	// 	//return tpret;
	// }
	// <<<
	#endif
	
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
	//pthread_mutex_lock(&(proxyListGroupCondVar.mtx));
	sys_remove_proxy( sys_get_mgt(), proxy);
	//pthread_mutex_unlock(&(proxyListGroupCondVar.mtx));
}

static void property_changed(struct l_dbus_proxy *proxy, const char *name,
				struct l_dbus_message *msg, void *user_data)
{
	#if(1)
		sys_property_changed( sys_get_mgt(), proxy, name, msg,  user_data);
		#if(1)
		if(!strcmp( name, "Connected"))
		{ // to stop BT scanning when some device is connected
			app_management *pt_app_mgt = sys_get_mgt();
			bool tp_bval = false;
			l_dbus_message_get_arguments( msg, "b", &tp_bval);
			if(tp_bval)
			{ // to turn off BT scanning
				//bt_common_set_powered( pt_app_mgt->hld_proxy_adapter1, false);
				//bt_common_turn_off_scanning(pt_app_mgt->hld_proxy_adapter1);
			}
			else
			{
				//bt_common_set_powered( pt_app_mgt->hld_proxy_adapter1, true);
				//bt_common_turn_on_sanning( pt_app_mgt->hld_proxy_adapter1, NULL);
				// to remove the device
				//DBG_LOG_WARN("Going to remove(unbond) %s", l_dbus_proxy_get_path( proxy));
				//bt_common_rm_devices( sys_get_proxy_adapter1(sys_get_mgt()), proxy);
			}
		}
		#endif
	#else // the original
	#endif
}

#endif


#if(1) // timer callback
	//l_timeout_notify_cb_t callback
	static void test_con_timer_callback(struct l_timeout *timeout,void *user_data)
	{
		struct bt_addr kp_btaddr = {0};
		app_management *pt_app_mgt = NULL;
		struct l_dbus_proxy *pt_proxy = NULL;
		
		//DBG_LOG_INFO("Output from timer callback, timer_pointer:0x%x", timeout);

		#if(0) // dump the proxy-queue for debug
			sys_dump_dbus_proxy_queue(sys_get_mgt());
		#endif
		
		pt_app_mgt = sys_get_mgt();
		#if(0) // to find device by BT-Address	
			memcpy((char*)&kp_btaddr, (char*)l_queue_peek_tail(pt_app_mgt->conf.pt_ble_wlist),sizeof(struct bt_addr));
			DBG_LOG_INFO("trying to find proxy with addr %x-%x-%x", kp_btaddr.nap, kp_btaddr.uap, kp_btaddr.lap);
			pt_proxy = bt_common_to_find_dev_proxy( pt_app_mgt->pt_q_devices, kp_btaddr);
		#else // to find device by Device Name
			pt_proxy = bt_common_to_find_dev_proxy_by_name( pt_app_mgt->pt_q_devices, "Victor996"); 
			//pt_proxy = bt_common_to_find_dev_proxy_by_name( pt_app_mgt->pt_q_devices, "BUL100001"); FKKK
		#endif
		if(pt_proxy)
		{//the proxy with the specific address is found
			const char *pt_path = NULL;
			pt_path = l_dbus_proxy_get_path(pt_proxy);
			DBG_LOG_INFO("Device:%s is on the BT-WList, you need to try to connect to it here", pt_path);
			#if (DBG_CT)
				if(sys_get_connected_proxy( sys_get_mgt()) == NULL)
				{
					//l_dbus_proxy_method_call(struct l_dbus_proxy *proxy,const char *method,l_dbus_message_func_t setup,l_dbus_client_proxy_result_func_t reply,void *user_data,l_dbus_destroy_func_t destroy)
					//l_dbus_proxy_method_call( pt_proxy, "Connect", msg_setup_for_bt_connect, msg_reply_for_bt_connect, NULL, NULL);
					if(bt_con_is_dev_paired(pt_proxy))
					{
						bt_con_connect( pt_proxy, NULL);
					}
					else
					{
						//bt_con_pair( pt_proxy);
						bt_con_connect( pt_proxy, NULL);
					}
					//bt_con_connect( pt_proxy);
					l_timeout_modify( timeout, 10);
				}
				else {
					//pthread_mutex_lock( &pt_app_mgt->mtx_q_characteristic);	
					if(l_queue_length(pt_app_mgt->pt_q_characteristics))
					{
						struct l_dbus_proxy*kp_proxy_to_write = NULL;

						uint8_t pt_data[4] = {0};
						static uint16_t tpu16 = 0, tpval = 0;
						
						DBG_LOG_INFO("the charac-queue is not empty! Try to read from here");

						kp_proxy_to_write = bt_gatt_find_characteristic( sys_get_gatt_characteristic_queue(sys_get_mgt()), (const char *)NORDIC_UART_CHARAC_RX_UUID);
						if(kp_proxy_to_write)
						{						
							DBG_LOG_ERR("TO write characteristic %s is found@0x%x", NORDIC_UART_CHARAC_RX_UUID, kp_proxy_to_write);
							#if(0)// the original							
							//#else // send Froto msg
								#if(1)
									// APIs for encoding test
									//util_froto_fill_msg_config_dissem_set_time();
									//util_froto_fill_msg_data_selection_retrieve_battery();			
									//util_froto_fill_msg_data_selection_retrieve_data();
									//util_froto_fill_msg_command_dissem();
									//util_froto_fill_msg_version_retrieve();
									//util_froto_fill_msg_fuota_notify_dissem();
									//util_froto_fill_msg_img_block_dissem();
								#endif
								if(g_froto_encoded_msg.flg)
								{		
									bt_gatt_charac_write_value( kp_proxy_to_write, g_froto_encoded_msg.buf, g_froto_encoded_msg.len);
								}
								util_dbg_release_froto_msg(&g_froto_encoded_msg);


								// for debug
								bt_gatt_local_service_send_notification(sys_get_mgt());
							#endif				
						}
						else
						{
							DBG_LOG_ERR("fail to find characteristic %s", NORDIC_UART_CHARAC_RX_UUID);
						}
						
						#if(1)
						// to AcquireNotify
						struct l_dbus_proxy*kp_proxy_to_aqnotify = NULL;
						kp_proxy_to_aqnotify = bt_gatt_find_characteristic( sys_get_gatt_characteristic_queue(sys_get_mgt()), (const char *)NORDIC_UART_CHARAC_TX_UUID);
						if(kp_proxy_to_aqnotify)
						{
							DBG_LOG_INFO("To notify characteristic %s is found@0x%x", NORDIC_UART_CHARAC_TX_UUID);
							if(bt_gatt_is_notification_acquired( kp_proxy_to_aqnotify))
							{
								//bt_gatt_release_notify( kp_proxy_to_aqnotify);
							}
							else
							{
								bt_gatt_acquire_notify( kp_proxy_to_aqnotify);
								DBG_LOG_WARN(" the NORDIX TX notification is not enabled yet");
							}
						}
						#endif

						// try to disconnect from the peer device
						if((tpu16 > 0)&&( tpu16 % 6 == 0))
						{
							#if(1) // keep connection
								DBG_LOG_INFO("keep connecting");
							#else
								bt_con_disconnect( sys_get_connected_proxy(sys_get_mgt()));
							#endif
						}
						l_timeout_modify( timeout, 60);
					}
					else
					{
						DBG_LOG_WARN("Oops!!!");
						l_timeout_modify( timeout, 60);
					}
					//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	
				}
			#endif
		}
		else
		{
			DBG_LOG_WARN("fail to find the proxy");
			l_timeout_modify( timeout, 20);				
		}
	}
	
	//typedef void (*l_timeout_destroy_cb_t) (void *user_data);
	static void test_con_timer_destroy_callback(void *user_data)
	{
		DBG_LOG_INFO("Output from timer destroy callback");
	}
#endif


/*call back for Dbus name acquire
typedef void (*l_dbus_name_acquire_func_t) (struct l_dbus *dbus, bool success,bool queued, void *user_data);
*/
static void request_name_callback(struct l_dbus *dbus, bool success,bool queued, void *user_data)
{
	DBG_LOG_INFO(" request name result = %s", success ? (queued ? "queued" : "success" ) :"Failed" );
}



struct l_timeout *g_timer_for_pro_ble = NULL;
static void timeout_notify_cb_for_pro_ble(
  struct l_timeout *timeout,
  void *user_data);
static app_state_t to_create_time_routine(void);

/**
 * @brief a callback used by timer @p g_timer_for_pro_ble
 *        typedef void (*l_timeout_notify_cb_t) (
 *          struct l_timeout *timeout,
 *          void *user_data);
 */
static void
timeout_notify_cb_for_pro_ble(
  struct l_timeout *timeout,
  void *user_data)
{
  DBG_LOG_DEBUG("BLE process is running");

// Disabled as we are using the IPC communication in a block manner
#if 0
  // Refresh the IPC message queue periodically
  app_com_send_ipc_msg(
    app_com_get_ipc_msg_sender_for_ble_to_gen(),
    NULL,
    0);
#endif

  // Reload the timer (called at next 1 s)
  l_timeout_modify(timeout, 1);
}
/**
 * @brief To create a timer (to call the callback every 1 s)
 */
static app_state_t
to_create_time_routine(void)
{
  app_state_t tpret = ST_OK;

  g_timer_for_pro_ble = l_timeout_create(1, timeout_notify_cb_for_pro_ble,
                                         NULL, NULL);
  if(NULL == g_timer_for_pro_ble)
  {
    DBG_LOG_ERR("Failed to create timer");
    tpret = ST_ERR;
    return tpret;
  }
  return tpret;
}





//int main(void)
/*the process handler for BLE
*/
int pro_ble_handler(const int msgid_in, const int msgid_out)
{
	struct l_dbus_client *ptclient = NULL;
	struct l_dbus *ptdbus = NULL;
	uint32_t kp_iwd_watch_id = 0;
	
	DBG_LOG_INFO("=======This is  the application for SKF_ GW=======\n");


	#if(1) // for msg-queue receivation test
		ssize_t kp_sz =0;
		struct ipc_msg kp_msg = {0};
		while(1)
		{
			kp_sz = msgrcv( msgid_in,(void*)&kp_msg, MAX_IPC_MSG_BUF, 1, 0);
			if(kp_sz > 0)
			{
				DBG_LOG_INFO("msgid_in %d, msgid_out %d ,the message recived is type : %d , val %d", msgid_in, msgid_out, kp_msg.type, kp_msg.dtbuf[0]);
				//sleep(5);
				break;
			}
			else
			{
				DBG_LOG_ERR("fail to receive message from msg-queue");		
			}
		}
	#endif


	#if(0) // for debug only
		if(sys_init_app_management(sys_get_mgt()) != ST_OK)
		{
			DBG_LOG_ERR("fail to init app-mamagement");
			goto ERR;
		}
		// to setup the message-queue for IPC with process-general
		sys_init_set_msgid( sys_get_mgt(), msgid_in, msgid_out);
		//sys_dump_bt_wlist((const sys_conf *)&g_app_management.conf);
		//sys_destroy_app_management(&g_app_management);
		//return 0;
	#endif



	if(!l_main_init())
	{
		DBG_LOG_ERR("Fail to init ");
		goto ERR;
	}

	#if(BLUEZ_DBUS_LOG_ENABLE)
	// set log output
	l_log_set_syslog();
	#else
		l_log_set_null();
		DBG_LOG_WARN("the bluez-dbus log output is disable");
	#endif
	
	ptdbus = l_dbus_new_default(L_DBUS_SYSTEM_BUS);

	//
	sys_init_set_dbus( sys_get_mgt(), ptdbus);

	#if(BLUEZ_DBUS_LOG_ENABLE)
		l_dbus_set_debug( ptdbus, do_debug, "TEST666", NULL);
	#endif
	
	l_dbus_set_ready_handler( ptdbus, ready_callback, ptdbus, NULL);
	l_dbus_set_disconnect_handler( ptdbus, disconnect_callback, NULL, NULL);
	
	//service basic watch
	//kp_iwd_watch_id = l_dbus_add_service_watch( ptdbus, "net.connman.iwd", iwd_service_appeared, iwd_service_disappeared, NULL, NULL);
	kp_iwd_watch_id = l_dbus_add_service_watch( ptdbus, "org.bluez", iwd_service_appeared, iwd_service_disappeared, NULL, NULL);


	//proxy example
	ptclient = l_dbus_client_new( ptdbus, "org.bluez", "/org/bluez");

	l_dbus_client_set_connect_handler( ptclient, bluez_client_connected, NULL, NULL);
	l_dbus_client_set_disconnect_handler( ptclient, bluez_client_disconnected, NULL, NULL);

	// Initialize the mutex (which is used to lock the proxy list group)
	// DBG_LOG_DEBUG(
	// 	"To initialize/create the mutex for the BLE device proxy list group");
	// if(ST_OK != app_com_init_pthread_cond_var_ctr(&proxyListGroupCondVar))
	// {
	// 	DBG_LOG_ERR("Fail to the mutex for the BLE device proxy list group");
	// }
	// else
	// {
	// 	DBG_LOG_DEBUG(
	// 	"The mutex for the BLE device proxy list group has been initialized successfully");
	// }

	l_dbus_client_set_proxy_handlers( ptclient, proxy_added, proxy_removed, property_changed, NULL, NULL);
	
	l_dbus_client_set_ready_handler( ptclient, bluez_client_ready, NULL, NULL);

	// to request a DBUS-connection name
	l_dbus_name_acquire( ptdbus, "test.by.victor", false, false, false, request_name_callback, NULL);
	
	#if(0)// TEST AREA
		l_dbus_method_call( ptdbus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames", app_dbus_message_func_sdmsg, app_dbus_message_func_recvmsg, NULL, NULL);
	#else
		#if(0)	
		// to power on BT
		DBG_LOG_INFO("Going to set BT power on");
		//l_dbus_method_call( ptdbus, "org.bluez", "/org/bluez/hci0", "org.freedesktop.DBus.Properties", "Set", app_dbus_message_func_sdmsg_to_set_power_on, app_dbus_message_func_recvmsg_from_set_power_on, NULL, NULL);
		struct l_timeout *pt_timer = NULL;
		pt_timer = l_timeout_create(10, test_con_timer_callback, NULL, test_con_timer_destroy_callback);
		#endif
		// to create a timer to do some periodic task
		to_create_time_routine();
	#endif	
	
	l_main_run_with_signal( signal_handler, NULL);

	DBG_LOG_INFO("TP666");
	
	l_dbus_remove_watch( ptdbus, kp_iwd_watch_id);
	l_dbus_client_destroy( ptclient);
	l_dbus_destroy(ptdbus);
	// to release the condiction-variable
	// app_com_deinit_pthread_cond_var_ctr( &proxyListGroupCondVar);
	l_main_exit();
	
	DBG_LOG_INFO("Going to exit the application");
	return 0;
	
	ERR:
		return -1;
}

















