#include "sys_timer.h"
#include "util_dbg.h"
#include "sys_def.h"
#include "bt_common.h"

DBG_LOCAL_LOG_DEBUG


#if(1)
	/*
	typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data);
	*/
	static void msg_setup_for_bt_connect(struct l_dbus_message *message, void *user_data)
	{	
		DBG_LOG_DEBUG("no paramter for method-Connect");
		l_dbus_message_set_arguments( message,"");
	}
	/*
	typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data);
	*/
	static void msg_reply_for_bt_connect(struct l_dbus_proxy*proxy, struct l_dbus_message*result, void *user_data)
	{
		// DBG_LOG_INFO("Oops!");
		if(l_dbus_message_is_error(result))
		{
			const char*ptname = NULL, *ptext = NULL;
			l_dbus_message_get_error( result, &ptname, &ptext);
			DBG_LOG_ERR("the error info: %s , %s", ptname, ptext);
			goto ERR;
		}
		DBG_LOG_INFO("msg to create a BT connection is sent successfully");
		
		ERR:
			return;
	}
#endif


//typedef void (*l_queue_foreach_func_t) (void *data, void *user_data);
static void cbk_for_ble_characteristic_reading(void *data, void*user_data)
{
	bool tpstate = false;
	struct l_dbus_proxy*pt_proxy = NULL;
	if(data == NULL)
	{
		DBG_LOG_ERR("invalid parameter");
		return;
	}
	pt_proxy = *((struct l_dbus_proxy**)data);
	tpstate = bt_common_ble_charac_readvalue( pt_proxy, 0);
	if(tpstate == false)
	{
		DBG_LOG_ERR("fail to read BLE characteristic");
		return;
	}
}


#if(1) // timer callback
	//l_timeout_notify_cb_t callback
	void sys_timer_callback(struct l_timeout *timeout,void *user_data)
	{
		struct bt_addr kp_btaddr = {0};
		app_management *pt_app_mgt = NULL;
		struct l_dbus_proxy *pt_proxy = NULL;
		
		//DBG_LOG_INFO("Output from timer callback, timer_pointer:0x%x", timeout);

		#if(0) // dump the proxy-queue for debug
			sys_dump_dbus_proxy_queue(sys_get_mgt());
		#endif
		
		pt_app_mgt = sys_get_mgt();
		// memcpy((char*)&kp_btaddr, (char*)l_queue_peek_head(pt_app_mgt->conf.pt_ble_wlist),sizeof(struct bt_addr));
		//pthread_mutex_lock( &pt_app_mgt->mtx_q_devices);
		pt_proxy = bt_common_to_find_dev_proxy( pt_app_mgt->pt_q_devices, kp_btaddr);
		if(pt_proxy)
		{//the proxy with the specific address is found
			const char *pt_path = NULL;
			pt_path = l_dbus_proxy_get_path(pt_proxy);
			DBG_LOG_INFO("Device:%s is on the BT-WList, you need to try to connect to it here", pt_path);
			#if (DBG_CT)
				if(sys_get_connected_proxy( sys_get_mgt()) == NULL)
				{
					//l_dbus_proxy_method_call(struct l_dbus_proxy *proxy,const char *method,l_dbus_message_func_t setup,l_dbus_client_proxy_result_func_t reply,void *user_data,l_dbus_destroy_func_t destroy)
					l_dbus_proxy_method_call( pt_proxy, "Connect", msg_setup_for_bt_connect, msg_reply_for_bt_connect, NULL, NULL);
				}
				else{
					//pthread_mutex_lock( &pt_app_mgt->mtx_q_characteristic);	
					if(l_queue_length(pt_app_mgt->pt_q_characteristics))
				 	{
						DBG_LOG_INFO("the charac-queue is not empty! Try to read from here");
						l_queue_foreach( pt_app_mgt->pt_q_characteristics, cbk_for_ble_characteristic_reading, NULL);
					}
					//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	
				}
			#endif
		}
		//pthread_mutex_unlock( &pt_app_mgt->mtx_q_devices);
		
		l_timeout_modify( timeout, 60);
	}
	
	//typedef void (*l_timeout_destroy_cb_t) (void *user_data);
	void sys_timer_destroy_callback(void *user_data)
	{
		DBG_LOG_INFO("Output from timer destroy callback");
	}
#endif





