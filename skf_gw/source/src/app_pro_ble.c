/*purpose of this process
1.BT management (power control, configuration, pairing...)

2. management local service;

3.scanning for LE devices;

4. Advertising ;

5.Manage BT connection/disconnection;

6.Data communication;

....
*/
#include "app_pro_ble.h"

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/ipc.h>

#include "pb.h"
#include <pthread.h>

#include "sys_def.h"
#include "app_general_def.h"
#include "util_dbg.h"
#include "app_common.h"

DBG_LOCAL_LOG_DEBUG


#define DELAY_AND_RETRY(func, time_us, retry_cnt, retry_msg, fail_msg, \
                        success_msg) do { \
    uint32_t cnt = retry_cnt; \
    do { \
      if(0 != func) \
      { \
        if(cnt != 0) { \
          cnt--; \
          retry_msg; \
          usleep(time_us); \
        } else { \
          fail_msg; \
          break; \
        } \
      } else { \
        success_msg; \
        break; \
      } \
    } while(cnt > 0); \
} while(0);


/*thread for BT test , only for debug
*/
//bool g_dbg_chrc_write_flg = false; // only for debug , chrc writting sychronization 
uint32_t g_dbg_block_idx = 1; // the block index;

static void*thandler_bt_test(void*pt_para)
{
	app_management *pt_app_mgt = NULL;
	struct l_dbus_proxy const *pt_proxy_for_writting = NULL;
	// uint8_t tpu8 = 'A';

	pt_app_mgt = sys_get_mgt();
	
	while(1)
	{

		#if(0) // the original
		if(NULL == sys_get_connected_proxy(sys_get_mgt()))
		{
			DBG_LOG_WARN("No connected devices yet");
			sleep(10);
			continue;
		}
		#else // wait for BT connection event
				pthread_mutex_lock( &pt_app_mgt->mtx_con_event);
				while(1)
				{
					if(pt_app_mgt->bt_con_event != BT_EVENT_CONNECTION)
					{					
						DBG_LOG_INFO("Going to wait for BT connection event");
						pthread_cond_wait( &pt_app_mgt->cond_val_con_event, &pt_app_mgt->mtx_con_event);
					}
					else
					{
						DBG_LOG_INFO("BT connection event happens");
						break;
					}
				}
				pthread_mutex_unlock( &pt_app_mgt->mtx_con_event);
		#endif
		//pthread_mutex_lock( &pt_app_mgt->mtx_q_characteristic);	
		DBG_LOG_INFO("Going to look for chrc %s", NORDIC_UART_CHARAC_RX_UUID);
		pt_proxy_for_writting = bt_gatt_find_characteristic( sys_get_gatt_characteristic_queue(sys_get_mgt()), (const char *)NORDIC_UART_CHARAC_RX_UUID);
		if(NULL == pt_proxy_for_writting)
		{
			//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	
			DBG_LOG_WARN("no chrc %s is found", NORDIC_UART_CHARAC_RX_UUID);
			sleep(10);
			continue;
		}
		//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	

		#if(0) // the img 
		for(uint32_t tpcnt = 0; tpcnt < 1000; tpcnt++ )
		{
			util_froto_fill_msg_img_block_dissem();
			if(g_froto_encoded_msg.flg && g_froto_encoded_msg.len)
			{				
				DBG_LOG_INFO("Going to write charac len %d, block idx %d", g_froto_encoded_msg.len, g_dbg_block_idx);
				g_dbg_chrc_write_flg = false;
				bt_gatt_charac_write_value( pt_proxy_for_writting, g_froto_encoded_msg.buf, g_froto_encoded_msg.len);

				//kpu32++;
				g_dbg_block_idx++;
				if(g_dbg_block_idx > 1000)
				{
					g_dbg_block_idx = 1;
				}
				
			}
			util_dbg_release_froto_msg(&g_froto_encoded_msg);			
			DBG_LOG_INFO("waiting for chrc-writting to be done");
			while(g_dbg_chrc_write_flg == false)
			{	
				;
			}
			DBG_LOG_INFO("exit waiting for chrc-writting");
			if(NULL == sys_get_connected_proxy( sys_get_mgt()))
			{
				g_dbg_block_idx = 1;
				break;
			}
		}
		#else
			#if(0)
			util_froto_fill_msg_config_dissem_set_time();
			if(g_froto_encoded_msg.flg && g_froto_encoded_msg.len)
			{				
				DBG_LOG_INFO("Going to write charac len %d", g_froto_encoded_msg.len);
				g_dbg_chrc_write_flg = false;
				bt_gatt_charac_write_value( pt_proxy_for_writting, g_froto_encoded_msg.buf, g_froto_encoded_msg.len);
			}
			util_dbg_release_froto_msg(&g_froto_encoded_msg);			
			DBG_LOG_INFO("waiting for chrc-writting to be done");
			while(g_dbg_chrc_write_flg == false)
			{	
				;
			}
			sleep(20);
			 util_froto_fill_msg_data_selection_retrieve_battery();
			 if(g_froto_encoded_msg.flg && g_froto_encoded_msg.len)
			{				
				DBG_LOG_INFO("Going to write charac len %d", g_froto_encoded_msg.len);
				g_dbg_chrc_write_flg = false;
				bt_gatt_charac_write_value( pt_proxy_for_writting, g_froto_encoded_msg.buf, g_froto_encoded_msg.len);
			}
			util_dbg_release_froto_msg(&g_froto_encoded_msg);			
			DBG_LOG_INFO("waiting for chrc-writting to be done");
			while(g_dbg_chrc_write_flg == false)
			{	
				;
			}
			sleep(20);
			util_froto_fill_msg_data_selection_retrieve_data();
			if(g_froto_encoded_msg.flg && g_froto_encoded_msg.len)
			{				
				DBG_LOG_INFO("Going to write charac len %d", g_froto_encoded_msg.len);
				g_dbg_chrc_write_flg = false;
				bt_gatt_charac_write_value( pt_proxy_for_writting, g_froto_encoded_msg.buf, g_froto_encoded_msg.len);
			}
			util_dbg_release_froto_msg(&g_froto_encoded_msg);			
			DBG_LOG_INFO("waiting for chrc-writting to be done");
			while(g_dbg_chrc_write_flg == false)
			{	
				;
			}
			sleep(20);
			util_froto_fill_msg_command_dissem();
			if(g_froto_encoded_msg.flg && g_froto_encoded_msg.len)
			{				
				DBG_LOG_INFO("Going to write charac len %d", g_froto_encoded_msg.len);
				g_dbg_chrc_write_flg = false;
				bt_gatt_charac_write_value( pt_proxy_for_writting, g_froto_encoded_msg.buf, g_froto_encoded_msg.len);
			}
			util_dbg_release_froto_msg(&g_froto_encoded_msg);			
			DBG_LOG_INFO("waiting for chrc-writting to be done");
			while(g_dbg_chrc_write_flg == false)
			{	
				;
			}
			sleep(20);
			util_froto_fill_msg_version_retrieve();
			if(g_froto_encoded_msg.flg && g_froto_encoded_msg.len)
			{				
				DBG_LOG_INFO("Going to write charac len %d", g_froto_encoded_msg.len);
				g_dbg_chrc_write_flg = false;
				bt_gatt_charac_write_value( pt_proxy_for_writting, g_froto_encoded_msg.buf, g_froto_encoded_msg.len);
			}
			util_dbg_release_froto_msg(&g_froto_encoded_msg);			
			DBG_LOG_INFO("waiting for chrc-writting to be done");
			while(g_dbg_chrc_write_flg == false)
			{	
				;
			}
			sleep(20);
			util_froto_fill_msg_fuota_notify_dissem();
			if(g_froto_encoded_msg.flg && g_froto_encoded_msg.len)
			{				
				DBG_LOG_INFO("Going to write charac len %d", g_froto_encoded_msg.len);
				g_dbg_chrc_write_flg = false;
				bt_gatt_charac_write_value( pt_proxy_for_writting, g_froto_encoded_msg.buf, g_froto_encoded_msg.len);
			}
			util_dbg_release_froto_msg(&g_froto_encoded_msg);			
			DBG_LOG_INFO("waiting for chrc-writting to be done");
			while(g_dbg_chrc_write_flg == false)
			{	
				;
			}
			sleep(20);
			#else
				#if(0)
				if(BT_ROLE_CLIENT == pt_app_mgt->bt_role)
				{
					DBG_LOG_INFO("Going to post data to the trans-queue for test");
					//tpu8 = 'C';
					app_com_post_to_data_trans_queue( app_com_get_data_trans_ctr(), &tpu8, sizeof(tpu8));
					// update the data
					tpu8 ++;
					if((tpu8 > '9') || (tpu8 < '0'))
					{
						tpu8 = '0';
					}
				}
				else if(BT_ROLE_SERVER == pt_app_mgt->bt_role)
				{
					#if(0)
						DBG_LOG_INFO("Going to send the chrc-notification");
						//send-notification test for local characteristic
						//bt_gatt_local_service_send_notification(sys_get_mgt());
						//sleep(10);
						bt_gatt_local_service_send_notification_v2( sys_get_mgt());
					#else
						DBG_LOG_INFO("Going to post data to the trans queue for test");
						//tpu8 = 'S';
						app_com_post_to_data_trans_queue( app_com_get_notification_trans_ctr(), &tpu8, sizeof(tpu8));
						// update the data
						tpu8 ++;
						if((tpu8 > 'Z')||(tpu8 < 'A'))
						{
							tpu8 = 'A';
						}
					#endif
				}
				else
				{
					DBG_LOG_ERR("this is not supposed to happen");
				}
				#else
					DBG_LOG_INFO("Not data is sent here");
				#endif
			#endif
			
		#endif
		
		sleep(60);
	}
}

/*thread for data sending
*/
static void*thandler_bt_data_sending(void*pt_para)
{
	app_management *pt_app_mgt = NULL;
	
	// struct l_dbus_proxy const *pt_proxy_for_writting = NULL;
	struct pthread_cond_var kp_cond_var = {0};
	struct data_unit *pt_dtunit = NULL;
	
	DBG_LOG_INFO("thread for BT data sending is running");

	if(ST_OK != app_com_init_pthread_cond_var_ctr(&kp_cond_var))
	{
		DBG_LOG_ERR("***fail to create condiction-variable for sychronization");
	}

	pt_app_mgt = sys_get_mgt();
	
	while(1)
	{// to complete	
		#if(0)
		util_froto_fill_msg_fuota_notify_dissem();
		if(g_froto_encoded_msg.flg && g_froto_encoded_msg.len)
		{				
			DBG_LOG_INFO("Going to write charac len %d", g_froto_encoded_msg.len);
			
			kp_cond_var.is_op_done = false;
			bt_gatt_charac_write_value( pt_proxy_for_writting, g_froto_encoded_msg.buf, g_froto_encoded_msg.len, &kp_cond_var);

			// to wait for the chrc-writting to be done
			pthread_mutex_lock( &kp_cond_var.mtx);
			while(1)
			{
				if(false == kp_cond_var.is_op_done)
				{					
					DBG_LOG_INFO("waiting for chrc-writting to be done");
					pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
				}
				else
				{
					DBG_LOG_INFO("the chrc-writting is done");
					break;
				}
			}
			pthread_mutex_unlock(&kp_cond_var.mtx);
		}
		util_dbg_release_froto_msg(&g_froto_encoded_msg);			
		#else
			DBG_LOG_INFO("Going to wait for data from the trans-queue");
			pt_dtunit = app_com_wait_for_data_trans_queue( app_com_get_data_trans_ctr());
			if(NULL == pt_dtunit)
			{
				DBG_LOG_WARN("no valid data unit get");
				continue;
			}

			#if(1) // to make data-writting thread-safe
				struct timespec tp_tspec;
				int tpint = 0;
				kp_cond_var.is_op_done = false;
				if(ST_OK != bt_gatt_charac_write_value_v2( pt_app_mgt, pt_dtunit->pt_buf, pt_dtunit->len, &kp_cond_var))
				{
					DBG_LOG_ERR("bt_gatt_charac_write_value_v2 fail");
					app_com_release_data_unit( pt_dtunit);
					continue;
				}
				// to wait for chrc-wirtting to be done
				pthread_mutex_lock( &kp_cond_var.mtx);
				clock_gettime(CLOCK_REALTIME, &tp_tspec);
				tp_tspec.tv_sec = tp_tspec.tv_sec + 3; // wait for 3s at most
				while(1)
				{
					if(false == kp_cond_var.is_op_done)
					{
						tpint = pthread_cond_timedwait( &kp_cond_var.cond_var, &kp_cond_var.mtx, &tp_tspec);
						if((0 != tpint)&&(ETIMEDOUT == tpint))
						{
							DBG_LOG_ERR("===chrc-writting timeout");
							break;						
						}
					}
					else
					{
						DBG_LOG_INFO("the chrc-writting is done");
						break;
					}
				}
				pthread_mutex_unlock( &kp_cond_var.mtx);
			#else // the original branch
			//pthread_mutex_lock( &pt_app_mgt->mtx_q_characteristic);	
			pt_proxy_for_writting = bt_gatt_find_characteristic( sys_get_gatt_characteristic_queue(sys_get_mgt()), (const char *)NORDIC_UART_CHARAC_RX_UUID);
			if(NULL == pt_proxy_for_writting)
			{
				//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	
				DBG_LOG_WARN("no chrc %s is found", NORDIC_UART_CHARAC_RX_UUID);
				continue;
			}
			//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	

			#if(0)// for debug
				usleep(100*1000);
			#endif
			
			kp_cond_var.is_op_done = false;
			
			if(ST_OK != bt_gatt_charac_write_value( pt_proxy_for_writting, pt_dtunit->pt_buf, pt_dtunit->len, &kp_cond_var))
			{
				DBG_LOG_WARN("chrc-writting fail");
				app_com_release_data_unit( pt_dtunit);
				continue;
			}
			// to wait for the chrc-writting to be done
			pthread_mutex_lock( &kp_cond_var.mtx);
			while(1)
			{
				if(false == kp_cond_var.is_op_done)
				{
					DBG_LOG_INFO("Going to wait for chrc-writting to be done");
					pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);									
				}
				else
				{
					DBG_LOG_INFO("the crhc-writting is done");
					break;
				}
			}
			pthread_mutex_unlock( &kp_cond_var.mtx);
			#endif
			
			// to release the data-unit
			app_com_release_data_unit( pt_dtunit);
			
		#endif		
		//sleep(5);
	}

	// to release the condiction-variable
	app_com_deinit_pthread_cond_var_ctr( &kp_cond_var);
}

#if(1)

//void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data)
static void msg_setup_for_disconnect_from_mobile_app(struct l_dbus_message *message,void *user_data)
{
	l_dbus_message_set_arguments( message, "");
}

//void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
static void msg_reply_for_disconnect_from_mobile_app(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	const char*pt_name = NULL, *pt_text = NULL;
	struct pthread_cond_var*pt_cond_var = NULL;

	//to signal the disconnection reply
	if(user_data)
	{
		pt_cond_var = (struct pthread_cond_var*)user_data;
		pthread_mutex_lock( &pt_cond_var->mtx);
		pt_cond_var->is_op_done = true;
		pthread_cond_signal( &pt_cond_var->cond_var);
		pthread_mutex_unlock( &pt_cond_var->mtx);
	}

	if(l_dbus_message_is_error(result))
	{
		l_dbus_message_get_error( result, &pt_name, &pt_text);
		DBG_LOG_ERR("fail to disconnect for %s %s", pt_name, pt_text);
	}
	DBG_LOG_INFO("method-Disconnect is done successfully");
	return;
}


//typedef void (*l_dbus_destroy_func_t) (void *user_data);
static void msg_destroy_for_disconnect_from_mobile_app(void *user_data)
{
	return;
}

/*to make sure no connection to mobile-APP when GW is switched from BT-server to BT-client
ret@ST_OK when all goes well

NOTE!! this function will be blocked to wait for the disconection reply
*/
static app_state_t to_disconnect_from_mobile_app( app_management*pt_app_mgt, struct pthread_cond_var*pt_cond_var)
{
	app_state_t tpret = ST_OK;
	uint8_t kp_active_macstr[MAX_ID_STR_LENGTH] = {0};
	struct l_dbus_proxy *pt_proxy = NULL, *pt_proxy_to_disconnect = NULL;
	struct l_queue_entry*pt_dev_queue_entry = NULL;
	const char*pt_proxy_addrstr = NULL;
	bool tp_bval = false;
	bool flg_is_disconnect_called = false;

	struct timespec tp_tspec;
	int tpint = 0;
		
	if((NULL == pt_app_mgt)||(NULL == pt_cond_var))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}
	snprintf( kp_active_macstr, sizeof(kp_active_macstr), "%s", sys_get_active_bt_device());

	DBG_LOG_INFO("to disconnect from mobile APP, active MAC-str %s", kp_active_macstr);
	while(1)
	{
		//MK417U_______________________________________________________
		pthread_mutex_lock( &pt_app_mgt->mtx_q_devices);
		pt_dev_queue_entry = l_queue_get_entries( pt_app_mgt->pt_q_devices);
		pt_proxy = NULL;
		pt_proxy_to_disconnect = NULL;
		flg_is_disconnect_called = false;
		while(1)
		{
			if(NULL == pt_dev_queue_entry)
			{
				break;
			}
			//DBG_LOG_WARN("BKP");
			pt_proxy = *((struct l_dbus_proxy**)pt_dev_queue_entry->data);
			if(!l_dbus_proxy_get_property( pt_proxy, "Connected", "b", &tp_bval))
			{
				DBG_LOG_ERR("fail to get property-Connected");
				break;
			}
			if(false == tp_bval)
			{ // the proxy is not connected
				pt_dev_queue_entry = pt_dev_queue_entry->next;
				continue;
			}			
			//DBG_LOG_WARN("BKP");
			// to check the MAC-string
			if(!l_dbus_proxy_get_property( pt_proxy, "Address", "s", &pt_proxy_addrstr))
			{
				DBG_LOG_ERR("fail to get property-Address");
				break;
			}			

			DBG_LOG_INFO("BKP %s vs %s", kp_active_macstr, pt_proxy_addrstr);
			
			if(strcmp( kp_active_macstr, pt_proxy_addrstr) == 0)
			{
				DBG_LOG_INFO("a connected sensor(%s) is found", pt_proxy_addrstr);
				pt_dev_queue_entry = pt_dev_queue_entry->next;
				continue;
			}
			
			// DBG_LOG_INFO("BKP_disconnect");
			pt_proxy_to_disconnect = pt_proxy;
			break;
		}
		
		//to disconnect
		if(pt_proxy_to_disconnect)
		{
			DBG_LOG_INFO("to disconnect from proxy(%s)", pt_proxy_addrstr);
			
			pt_cond_var->is_op_done = false;
			if(0 == l_dbus_proxy_method_call( pt_proxy_to_disconnect, "Disconnect", msg_setup_for_disconnect_from_mobile_app, msg_reply_for_disconnect_from_mobile_app, (void *)pt_cond_var, msg_destroy_for_disconnect_from_mobile_app))
			{
				flg_is_disconnect_called = false;			
				DBG_LOG_ERR("fail to call method-Disconnect");
			}
			else
			{
				//DBG_LOG_ERR("")
				flg_is_disconnect_called = true;
			}
		}
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_devices);
		//MK417D______________________________________________________



		// to wait for disconnection reply
		if(false == flg_is_disconnect_called)	
		{// fail to call method-Disconnect			
			break;
		}
		DBG_LOG_INFO("to wait for disconnection reply");
		// to wait for disconnection-reply
		pthread_mutex_lock( &pt_cond_var->mtx);
		clock_gettime(CLOCK_REALTIME, &tp_tspec);
		tp_tspec.tv_sec = tp_tspec.tv_sec + 3;
		while(1)
		{
			if(false == pt_cond_var->is_op_done)
			{
				tpint = pthread_cond_timedwait( &pt_cond_var->cond_var, &pt_cond_var->mtx, &tp_tspec);
				if(ETIMEDOUT == tpint)
				{
					DBG_LOG_ERR("waiting for disconnection reply timeout");
					break;
				}
			}
			else
			{
				break;
			}
		}
		pthread_mutex_unlock( &pt_cond_var->mtx);
		if(ETIMEDOUT == tpint)
		{
			break;
		}
	}
	EXIT:
		return tpret;
}

#endif
/*typedef void (*l_timeout_notify_cb_t) (struct l_timeout *timeout,void *user_data);
l_timeout_create(unsigned int seconds, l_timeout_notify_cb_t callback, void * user_data, l_timeout_destroy_cb_t destroy)
the timeout callback to send an dev-proxy adding event
the timer callback to trig the BT dev try to create a BT connection
*/
static tim_cbk_for_ble_connection(struct l_timeout *timeout,void *user_data)
{
	app_management *pt_app_mgt = sys_get_mgt();
	enum bluetooth_event kp_btevent = BT_EVENT_TO_CON;
	
	pthread_mutex_lock(&pt_app_mgt->mtx_adv_scanning_ctr);
	l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)kp_btevent);
	pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
	pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);
}

/*threads for BT connection management
*/
static void*thandler_bt_connection_manager(void*pt_para)
{
	app_management *pt_app_mgt = NULL;
	struct l_dbus_proxy*pt_proxy = NULL;
	uint8_t scanning_nothing_cnt = 0;

	enum bluetooth_event hld_btevent = BT_EVENT_UNDEFINED;

	struct l_timeout *pt_timer_for_con_ctr = NULL; // timer for connection control	

	struct pthread_cond_var kp_cond_var = {0}; // for BT OP sychronization
	
	DBG_LOG_INFO("thread for BT connection management is running");
	
	pt_app_mgt = sys_get_mgt();

	app_com_init_pthread_cond_var_ctr( &kp_cond_var);
	bt_common_scanning_init();
	
	while(1)
	{//
		#if(0) // the original
		#else
			// wait for BT event
			pthread_mutex_lock(&pt_app_mgt->mtx_adv_scanning_ctr);
			while(1)
			{
				
				if(0 == l_queue_length(pt_app_mgt->pt_adv_scanning_event_queue))
				{
					// pthread_cond_wait( &pt_app_mgt->cond_val_adv_scanning_ctr, &pt_app_mgt->mtx_adv_scanning_ctr);
					clock_gettime(CLOCK_MONOTONIC, &pt_app_mgt->cond_val_adv_scanning_ctr_tv);
					pt_app_mgt->cond_val_adv_scanning_ctr_tv.tv_sec += 30;
					if(pthread_cond_timedwait(&pt_app_mgt->cond_val_adv_scanning_ctr, 
					  						  &pt_app_mgt->mtx_adv_scanning_ctr,
											  &pt_app_mgt->cond_val_adv_scanning_ctr_tv) == ETIMEDOUT)
					{
						if(hld_btevent == BT_EVENT_UNDEFINED)
						{
							DBG_LOG_ERR("thandler_bt_connection_manager waiting timeout!");
							sync();
							sync();
							sync();
							system("reboot");
							// exit(1);
							// break;
						}else{
							DBG_LOG_DEBUG("hld_btevent = %d, scanning_nothing_cnt = %d", 
										  hld_btevent, 
										  scanning_nothing_cnt);
							if(BT_ROLE_CLIENT ==  pt_app_mgt->bt_role)
							{
								if(BT_EVENT_TO_SCANNING == hld_btevent)
								{
									scanning_nothing_cnt++;
									if(scanning_nothing_cnt > 3)
									{
										fflush(stdout);
										exit(1); // To avoid BLE nic failed to scan ...
									}
								}else{
									scanning_nothing_cnt = 0;
								}
							}
							continue;
						}
					}
				}
				else
				{
					hld_btevent = (enum bluetooth_event)l_queue_pop_head( pt_app_mgt->pt_adv_scanning_event_queue);
					break;
				}
			}
			pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);

			DBG_LOG_INFO("trigged by BT-event %d", hld_btevent);
			
			switch(hld_btevent)
			{
				case BT_EVENT_ADD_ADAPTER:
				{// set the power on
					struct ipc_msg_v2 tp_msg = {0};
					int tp_msgid = -1;
					
					DBG_LOG_INFO("Going to turn BT power on");
					pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
					if(NULL == pt_proxy)
					{
						DBG_LOG_ERR("this is not supposed to happen");
						break;
					}
					bt_common_set_powered( pt_proxy, true);
					// to set the GateWay ID
					bt_common_set_gateway_id( pt_app_mgt, pt_proxy);
					DBG_LOG_DEBUG("the Gateway ID is %s", sys_get_gw_id_string());

					// to notify the ADAPTER1 is added
					tp_msg.mtype = M_TYPE_FEEDBACK;
					tp_msg.mtext.feedback_info.cmd = CMD_NOTIFY;
					tp_msg.mtext.feedback_info.msgid = 0;
					tp_msg.mtext.feedback_info.info.event_notification_info.bt_event= BT_EVENT_ADD_ADAPTER;
					tp_msgid = sys_get_msgid_to_pro_general( pt_app_mgt);
					if(tp_msgid < 0)
					{
						DBG_LOG_ERR("fail to get valid msg-queue ID");
						break;
					}					
					
					#if(1) // the original branch
					DELAY_AND_RETRY(msgsnd(tp_msgid, &tp_msg, sizeof(union msg_load), IPC_NOWAIT), 
						500000, 
						100,
						if(EAGAIN == errno)
						{
							DBG_LOG_WARN("Failed to put msg on queue, for %s, and retry\r\n",
										strerror(errno));
						}
						else
						{
							DBG_LOG_ERR("Failed to put msg on queue, for %s\r\n",
										strerror(errno));
							break;
						},
						DBG_LOG_ERR("Failed to put msg on the queue, for %s, and give it up", strerror(errno)),
						);
					#else
						app_com_send_ipc_msg( app_com_get_ipc_msg_sender_for_ble_to_gen(), (void *)&tp_msg, sizeof(union msg_load));
					#endif
					
					break;
				}
				case BT_EVENT_TO_CON:
				{
					// uint8_t tp_btaddr_buf[MAX_BT_ADDRSTR] = {0};

					//pthread_mutex_lock(&(proxyListGroupCondVar.mtx));

					if(BT_ROLE_CLIENT != sys_get_bluetooth_role( pt_app_mgt))
					{
						DBG_LOG_WARN("not a BT client!");
						if(pt_timer_for_con_ctr)
						{
							l_timeout_remove( pt_timer_for_con_ctr);
							pt_timer_for_con_ctr = NULL;
						}
						//pthread_mutex_unlock(&(proxyListGroupCondVar.mtx));
						break;
					}
					// to reset the time
					if(pt_timer_for_con_ctr)
					{
						if(NULL == sys_get_connected_proxy( pt_app_mgt))
						{ // reset the timer and try agin
							l_timeout_modify( pt_timer_for_con_ctr, 3);
						}
						else
						{
							DBG_LOG_INFO("the timer for con is released");
							l_timeout_remove( pt_timer_for_con_ctr);
							pt_timer_for_con_ctr = NULL;
						}
					}
					else
					{
						DBG_LOG_ERR("This is not suppose to happen");
					}

#if(1)
					bt_con_search_for_sensor_and_connect( pt_app_mgt,
													      &kp_cond_var);
#else
					DBG_LOG_DEBUG("sys_get_connected_proxy(pt_app_mgt) = %d", sys_get_connected_proxy(pt_app_mgt));
					DBG_LOG_DEBUG("sys_get_con_in_progress_flg() = %d", sys_get_con_in_progress_flg());
					
					if((NULL == sys_get_connected_proxy(pt_app_mgt)) && (false == sys_get_con_in_progress_flg()))
					{
						// >>> @20240530 Added by Sean (to sort the list order effectively, so move this code in to the "if")
						DBG_LOG_INFO("to find the sensor and connect to it");
						
						//pt_proxy = bt_common_to_find_dev_proxy_by_name( sys_get_devices_queue( pt_app_mgt), "Victor996");						
						//pt_proxy = bt_common_to_find_dev_proxy_by_name( sys_get_devices_queue( pt_app_mgt), "OK62xxBT9");					
						//pt_proxy = bt_common_to_find_dev_proxy_by_name( sys_get_devices_queue( pt_app_mgt), "BUL100001");
						//pt_proxy = bt_common_to_find_dev_proxy_by_addrstr( sys_get_devices_queue( pt_app_mgt), sys_get_active_bt_device());
						pthread_mutex_lock( &pt_app_mgt->mtx_q_devices);
						pt_proxy = bt_common_to_find_dev_proxy_on_white_list( sys_get_devices_queue( pt_app_mgt), tp_btaddr_buf, MAX_BT_ADDRSTR);
						if(NULL == pt_proxy)
						{
							DBG_LOG_WARN("fail to find the sensor");
							pthread_mutex_unlock( &pt_app_mgt->mtx_q_devices);
							//pthread_mutex_unlock(&(proxyListGroupCondVar.mtx));

							if((false == sys_get_con_in_progress_flg()) && (l_queue_length(sys_get_mgt()->pt_q_devices) == 0))
							{
								// >>> @20240613 Added by Sean (to restart scanning once a refresh happens)
								// Just avoid re-turn on scanning, so turn off scanning first
								// to wait for scanning is off
								DBG_LOG_DEBUG("Turn off scanning as a refresh happened ...");
								pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
								kp_cond_var.is_op_done = false;
								clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
								kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_OFF_S;
								if(ST_OK == bt_common_turn_off_scanning( pt_proxy, &kp_cond_var))
								{
									pthread_mutex_lock( &kp_cond_var.mtx);
									while(false == kp_cond_var.is_op_done)
									{
										if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
																&kp_cond_var.mtx,
																&kp_cond_var.tv) == ETIMEDOUT)
										{
											DBG_LOG_ERR("Turnning off scanning timeout!");
											break;
										}
									}
									pthread_mutex_unlock( &kp_cond_var.mtx);
								}
								DBG_LOG_DEBUG("Turn off scanning successfully ...");

								while(1)
								{
									DBG_LOG_DEBUG("Turn on scanning as a refresh happened ...");
									pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
									kp_cond_var.is_op_done = false;
									clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
									kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_ON_S;
									if(ST_OK == bt_common_turn_on_scanning( pt_proxy, &kp_cond_var))
									{
										pthread_mutex_lock( &kp_cond_var.mtx);
										while(false == kp_cond_var.is_op_done)
										{
											if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
																	&kp_cond_var.mtx,
																	&kp_cond_var.tv) == ETIMEDOUT)
											{
												DBG_LOG_ERR("Turnning on scanning timeout!");
												kp_cond_var.para++;
												break;
											}
										}
										pthread_mutex_unlock( &kp_cond_var.mtx);
									}
									// pthread_mutex_lock( &kp_cond_var.mtx);
									// while(1)
									// {
									// 	if(false == kp_cond_var.is_op_done)
									// 	{
									// 		pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
									// 	}
									// 	else
									// 	{
									// 		break;
									// 	}
									// }
									// pthread_mutex_unlock( &kp_cond_var.mtx);
									if(kp_cond_var.para == 0)
									{
										DBG_LOG_DEBUG("Turn on scanning successfully ...");
										break;
									}
									else
									{
										DBG_LOG_DEBUG("Power off");
										bt_common_set_powered(pt_proxy, false);
										sleep(1);
										bt_common_scanning_init();
										bt_common_set_powered(pt_proxy, true);
										sleep(2);
										DBG_LOG_DEBUG("Power on");
										if(kp_cond_var.para > 2)
										{
											DBG_LOG_ERR("Turn on scanning fail!");
											exit(1);
										}else{
											DBG_LOG_ERR("retry turning on scanning...");
										}
									}
								}
								// <<<
							}




							break;
						}
						pthread_mutex_unlock( &pt_app_mgt->mtx_q_devices);
						DBG_LOG_INFO("BKP, connected_proxy = 0x%x, in_process_flg = %d", sys_get_connected_proxy( pt_app_mgt), sys_get_con_in_progress_flg());
						#warning "TODO===this is not a good way to check if the device is conneced, we should check property-Conneccted"
						// <<<
						DBG_LOG_INFO("to create a connect with the sensor");
						// to get the proxy address
						sys_set_active_bt_device( tp_btaddr_buf); // to update the ACTIVE BT-address
						
						// try to create a connection
						kp_cond_var.is_op_done = false;
						clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
						kp_cond_var.tv.tv_sec += TIMEOUT_BT_CONNECT_S;
						if(bt_con_connect( pt_proxy, &kp_cond_var) == ST_OK)
						{
							sys_set_con_in_progress_flg(true);	
							pthread_mutex_lock( &kp_cond_var.mtx);
							while(false == kp_cond_var.is_op_done)
							{
								if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
   														  &kp_cond_var.mtx,
														  &kp_cond_var.tv) == ETIMEDOUT)
								{
									DBG_LOG_ERR("Connection timeout!");
									sys_set_con_in_progress_flg(false);
									exit(1);
									bt_common_rm_devices( sys_get_proxy_adapter1(sys_get_mgt()), pt_proxy);
									break;
								}
							}
							pthread_mutex_unlock( &kp_cond_var.mtx);
						}else{
							sys_set_con_in_progress_flg(false);
							// >>> @20240604 Added by Sean (to restart scanning once connect failure happens)
							// Just avoid re-turn on scanning, so turn off scanning first
							// to wait for scanning is off
							DBG_LOG_DEBUG("Turn off scanning as a fail-to-connect happened ...");
							pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
							kp_cond_var.is_op_done = false;
							clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
							kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_OFF_S;
							if(ST_OK == bt_common_turn_off_scanning( pt_proxy, &kp_cond_var))
							{
								pthread_mutex_lock( &kp_cond_var.mtx);
								while(false == kp_cond_var.is_op_done)
								{
									if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
															&kp_cond_var.mtx,
															&kp_cond_var.tv) == ETIMEDOUT)
									{
										DBG_LOG_ERR("Turnning off scanning timeout!");
										break;
									}
								}
								pthread_mutex_unlock( &kp_cond_var.mtx);
							}
							DBG_LOG_DEBUG("Turn off scanning successfully ...");

							while(1)
							{
								DBG_LOG_DEBUG("Turn on scanning as a fail-to-connect happened ...");
								pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
								kp_cond_var.is_op_done = false;
								clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
								kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_ON_S;
								if(ST_OK == bt_common_turn_on_scanning( pt_proxy, &kp_cond_var))
								{
									pthread_mutex_lock( &kp_cond_var.mtx);
									while(false == kp_cond_var.is_op_done)
									{
										if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
																&kp_cond_var.mtx,
																&kp_cond_var.tv) == ETIMEDOUT)
										{
											DBG_LOG_ERR("Turnning on scanning timeout!");
											kp_cond_var.para++;
											break;
										}
									}
									pthread_mutex_unlock( &kp_cond_var.mtx);
								}
								// pthread_mutex_lock( &kp_cond_var.mtx);
								// while(1)
								// {
								// 	if(false == kp_cond_var.is_op_done)
								// 	{
								// 		pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
								// 	}
								// 	else
								// 	{
								// 		break;
								// 	}
								// }
								// pthread_mutex_unlock( &kp_cond_var.mtx);
								if(kp_cond_var.para == 0)
								{
									DBG_LOG_DEBUG("Turn on scanning successfully ...");
									break;
								}
								else
								{
									DBG_LOG_DEBUG("Power off");
									bt_common_set_powered(pt_proxy, false);
									sleep(1);
									bt_common_scanning_init();
									bt_common_set_powered(pt_proxy, true);
									sleep(2);
									DBG_LOG_DEBUG("Power on");
									if(kp_cond_var.para > 2)
									{
										DBG_LOG_ERR("Turn on scanning fail!");
										exit(1);
									}else{
										DBG_LOG_ERR("retry turning on scanning...");
									}
								}
							}
							// <<<
						}
					}
					else{
						DBG_LOG_DEBUG("TODO: To add a timeout!");
						if((false == sys_get_con_in_progress_flg()) && (l_queue_length(sys_get_mgt()->pt_q_devices) == 0))
						{
							// >>> @20240612 Added by Sean (to restart scanning once refresh happens)
							// Just avoid re-turn on scanning, so turn off scanning first
							// to wait for scanning is off
							DBG_LOG_DEBUG("Turn off scanning as a refresh happened ...");
							pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
							kp_cond_var.is_op_done = false;
							clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
							kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_OFF_S;
							if(ST_OK == bt_common_turn_off_scanning( pt_proxy, &kp_cond_var))
							{
								pthread_mutex_lock( &kp_cond_var.mtx);
								while(false == kp_cond_var.is_op_done)
								{
									if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
															&kp_cond_var.mtx,
															&kp_cond_var.tv) == ETIMEDOUT)
									{
										DBG_LOG_ERR("Turnning off scanning timeout!");
										break;
									}
								}
								pthread_mutex_unlock( &kp_cond_var.mtx);
							}
							DBG_LOG_DEBUG("Turn off scanning successfully ...");

							while(1)
							{
								DBG_LOG_DEBUG("Turn on scanning as a refresh happened ...");
								pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
								kp_cond_var.is_op_done = false;
								clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
								kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_ON_S;
								if(ST_OK == bt_common_turn_on_scanning( pt_proxy, &kp_cond_var))
								{
									pthread_mutex_lock( &kp_cond_var.mtx);
									while(false == kp_cond_var.is_op_done)
									{
										if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
																&kp_cond_var.mtx,
																&kp_cond_var.tv) == ETIMEDOUT)
										{
											DBG_LOG_ERR("Turnning on scanning timeout!");
											kp_cond_var.para++;
											break;
										}
									}
									pthread_mutex_unlock( &kp_cond_var.mtx);
								}
								// pthread_mutex_lock( &kp_cond_var.mtx);
								// while(1)
								// {
								// 	if(false == kp_cond_var.is_op_done)
								// 	{
								// 		pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
								// 	}
								// 	else
								// 	{
								// 		break;
								// 	}
								// }
								// pthread_mutex_unlock( &kp_cond_var.mtx);
								if(kp_cond_var.para == 0)
								{
									DBG_LOG_DEBUG("Turn on scanning successfully ...");
									break;
								}
								else
								{
									DBG_LOG_DEBUG("Power off");
									bt_common_set_powered(pt_proxy, false);
									sleep(1);
									bt_common_scanning_init();
									bt_common_set_powered(pt_proxy, true);
									sleep(2);
									DBG_LOG_DEBUG("Power on");
									if(kp_cond_var.para > 2)
									{
										DBG_LOG_ERR("Turn on scanning fail!");
										exit(1);
									}else{
										DBG_LOG_ERR("retry turning on scanning...");
									}
								}
							}
							// <<<
						}
						
					}
					//pthread_mutex_unlock(&(proxyListGroupCondVar.mtx));
					#endif
					break;
				}
				case BT_EVENT_ADD_DEVPROXY:
				{ // to check , to connect
					if(BT_ROLE_CLIENT == pt_app_mgt->bt_role)
					{

						#if(0) // the original
						#else
							if(NULL == pt_timer_for_con_ctr)
							{// to create a timer to check , create an connection when necessary
								DBG_LOG_INFO("a new timer is created");
								pt_timer_for_con_ctr = l_timeout_create( 1, tim_cbk_for_ble_connection, NULL, NULL);
							}
							// to disconnect from mobile APP if there is any connection exist when GW is switched to BT-client
							to_disconnect_from_mobile_app( pt_app_mgt, &kp_cond_var);
						#endif
					}
					else if(BT_ROLE_SERVER == pt_app_mgt->bt_role)
					{ // work as a BT-server
						#if(1) // just for debug
							// NOTE!! this is not a good way , fix it latter
							struct l_dbus_proxy**ppt_proxy = NULL;
							struct l_dbus_proxy*tp_p_proxy = NULL;
							struct l_queue_entry*pt_entries = NULL;
							uint32_t tp_cnt = 0;
							pthread_mutex_lock( &pt_app_mgt->mtx_q_devices);
							DBG_LOG_INFO("the device queue length %d", l_queue_length(sys_get_devices_queue( pt_app_mgt)));
							pt_entries = l_queue_get_entries( sys_get_devices_queue(pt_app_mgt));
							while(pt_entries)
							{
								ppt_proxy = (struct l_dbus_proxy**)pt_entries->data;
								if(ppt_proxy)
								{
									pt_proxy = *ppt_proxy;
									if(pt_proxy && (true == bt_con_is_dev_connected(  pt_proxy)))
									{
										tp_p_proxy = pt_proxy;
										tp_cnt++;
									}
								}
	
								pt_entries = pt_entries->next;
							}
							pthread_mutex_unlock( &pt_app_mgt->mtx_q_devices);
							if(NULL == tp_p_proxy)
							{
								break;
							}
							if(tp_cnt != 1)
							{ // NOTE!!! you need to do some modification to the application architecture when this happens
								DBG_LOG_WARN("Oops! we do not expect to connect two devices at the same time!");
							}
							sys_set_connected_proxy(  pt_app_mgt,  tp_p_proxy);
							// to send the signal
							pthread_mutex_lock( &pt_app_mgt->mtx_con_event);
							pt_app_mgt->bt_con_event = BT_EVENT_CONNECTION;
							pthread_cond_broadcast( &pt_app_mgt->cond_val_con_event);
							pthread_mutex_unlock( &pt_app_mgt->mtx_con_event);

							// to report the connection event
							pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
							l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)BT_EVENT_CONNECTION);
							pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
							pthread_mutex_unlock(&pt_app_mgt->mtx_adv_scanning_ctr);
						#endif	
					}
					else
					{
						DBG_LOG_ERR("this is not supposed to happen");
					}
					break;
				}
				case BT_EVENT_ADD_CHRC_PROXY:
				{			
					if(BT_ROLE_SERVER == sys_get_bluetooth_role( pt_app_mgt))
					{
						DBG_LOG_INFO("BT server, no need to enable notification");
						break;
					}
					//pthread_mutex_lock( &pt_app_mgt->mtx_q_characteristic);	
					if(true != bt_gatt_is_btchr_registration_done( sys_get_gatt_characteristic_queue(sys_get_mgt())))
					{
						//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	
						DBG_LOG_WARN(" to wait for all BT-chr registered");
						break;
					}

					#if(0) // the original
					pt_proxy = bt_gatt_find_characteristic( sys_get_gatt_characteristic_queue(sys_get_mgt()), NORDIC_UART_CHARAC_TX_UUID);
					if(NULL == pt_proxy)
					{
						//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	
						DBG_LOG_WARN("fail to find %s", NORDIC_UART_CHARAC_TX_UUID);
						break;
					}
					if(false == bt_gatt_is_notification_acquired( pt_proxy))
					{// to enable NORDIC TX notification
						bt_gatt_acquire_notify( pt_proxy);
					}
					//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	
					#else
						bt_gatt_acquire_notify_v2( pt_app_mgt);
					#endif
					break;
				}
				case BT_EVENT_DISCONNECTION:
				{ // to reconnect
					struct ipc_msg_v2 tp_msg = {0};
					int tp_msgid = -1;
					
					if((BT_ROLE_CLIENT == sys_get_bluetooth_role( pt_app_mgt))&& (NULL == pt_timer_for_con_ctr))
					{ // set up a timer
						DBG_LOG_INFO("a new timer is created");
						pt_timer_for_con_ctr = l_timeout_create( 1, tim_cbk_for_ble_connection, NULL, NULL);
					}

					//to notify the BT disconnection event
					tp_msgid = sys_get_msgid_to_pro_general( pt_app_mgt);
					if(tp_msgid < 0)
					{
						DBG_LOG_ERR("make sure the msg-queue is created for msg to pro-gen");
						break;
					}
					tp_msg.mtype = M_TYPE_FEEDBACK;
					tp_msg.mtext.feedback_info.cmd = CMD_NOTIFY;
					tp_msg.mtext.feedback_info.msgid = 0;//
					tp_msg.mtext.feedback_info.info.event_notification_info.bt_event = BT_EVENT_DISCONNECTION;
					#if(1) // the original branch
					DELAY_AND_RETRY(msgsnd(tp_msgid, &tp_msg, sizeof(union msg_load), IPC_NOWAIT), 
						500000, 
						100,
						if(EAGAIN == errno)
						{
							DBG_LOG_WARN("Failed to put msg on queue, for %s, and retry\r\n",
										strerror(errno));
						}
						else
						{
							DBG_LOG_ERR("Failed to put msg on queue, for %s\r\n",
										strerror(errno));
							exit(1);
							break;
						},
						DBG_LOG_ERR("Failed to put msg on the queue, for %s, and give it up", strerror(errno));exit(1);,
						);
					#else
						app_com_send_ipc_msg( app_com_get_ipc_msg_sender_for_ble_to_gen(), (void *)&tp_msg, sizeof(union msg_load));
					#endif
					
					// >>> @20240530 Added by Sean (to turn on scanning when disconnection happened)
					if(BT_ROLE_CLIENT ==  pt_app_mgt->bt_role)
					{
						sleep(1);

						// Just avoid re-turn on scanning, so turn off scanning first
						// to wait for scanning is off
						DBG_LOG_DEBUG("Turn off scanning as a dis-connection happened ...");
						pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
						kp_cond_var.is_op_done = false;
						clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
						kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_OFF_S;
						if(ST_OK == bt_common_turn_off_scanning( pt_proxy, &kp_cond_var))
						{
							pthread_mutex_lock( &kp_cond_var.mtx);
							while(false == kp_cond_var.is_op_done)
							{
								if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
														&kp_cond_var.mtx,
														&kp_cond_var.tv) == ETIMEDOUT)
								{
									DBG_LOG_ERR("Turnning off scanning timeout!");
									break;
								}
							}
							pthread_mutex_unlock( &kp_cond_var.mtx);
						}
						DBG_LOG_DEBUG("Turn off scanning successfully ...");

						while(1)
						{
							DBG_LOG_DEBUG("Turn on scanning as a dis-connection happened ...");
							pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
							kp_cond_var.is_op_done = false;
							clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
							kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_ON_S;
							if(ST_OK == bt_common_turn_on_scanning( pt_proxy, &kp_cond_var))
							{
								pthread_mutex_lock( &kp_cond_var.mtx);
								while(false == kp_cond_var.is_op_done)
								{
									if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
															&kp_cond_var.mtx,
															&kp_cond_var.tv) == ETIMEDOUT)
									{
										DBG_LOG_ERR("Turnning on scanning timeout!");
										kp_cond_var.para++;
										break;
									}
								}
								pthread_mutex_unlock( &kp_cond_var.mtx);
							}
							// pthread_mutex_lock( &kp_cond_var.mtx);
							// while(1)
							// {
							// 	if(false == kp_cond_var.is_op_done)
							// 	{
							// 		pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
							// 	}
							// 	else
							// 	{
							// 		break;
							// 	}
							// }
							// pthread_mutex_unlock( &kp_cond_var.mtx);
							if(kp_cond_var.para == 0)
							{
								DBG_LOG_DEBUG("Turn on scanning successfully ...");
								break;
							}
							else
							{
								DBG_LOG_DEBUG("Power off");
								bt_common_set_powered(pt_proxy, false);
								sleep(1);
								bt_common_scanning_init();
								bt_common_set_powered(pt_proxy, true);
								sleep(2);
								DBG_LOG_DEBUG("Power on");
								if(kp_cond_var.para > 2)
								{
									DBG_LOG_ERR("Turn on scanning fail!");
									exit(1);
								}else{
									DBG_LOG_ERR("retry turning on scanning...");
								}
							}
						}
					}
					else
					{ ;}// nothing to do
					// <<<

					break;
				}
				case BT_EVENT_CONNECTION:
				{ // we need to notify the connection-event to progress-general
					struct ipc_msg_v2 tp_msg = {0};
					int tp_msgid = sys_get_msgid_to_pro_general( pt_app_mgt);
					if(tp_msgid < 0)
					{
						DBG_LOG_ERR("make sure the message-queue is created for message to pro-gen");
						break;
					}
					DBG_LOG_INFO("the BT event notification is sent by %d", tp_msgid);
					
					tp_msg.mtype = M_TYPE_FEEDBACK;
					tp_msg.mtext.feedback_info.cmd = CMD_NOTIFY;
					tp_msg.mtext.feedback_info.msgid = 0; // NOTE!!!for CMD_NOTIFY, we do not check the msgid; so set to anything will be OK
					tp_msg.mtext.feedback_info.info.event_notification_info.bt_event = BT_EVENT_CONNECTION;
					//tp_msg.mtext.feedback_info.info.event_notification_info.info.connected_dev; // to fix
					memcpy( tp_msg.mtext.feedback_info.info.event_notification_info.info.active_bt_device, sys_get_active_bt_device(), MAX_BT_ADDRSTR);
					#if(1)
					DELAY_AND_RETRY(msgsnd(tp_msgid, &tp_msg, sizeof(union msg_load), IPC_NOWAIT), 
						500000, 
						100,
						if(EAGAIN == errno)
						{
							DBG_LOG_WARN("Failed to notify the BT connection event, for %s, and retry\r\n",
										strerror(errno));
						}
						else
						{
							DBG_LOG_ERR("Failed to notify the BT connection event, for %s\r\n",
										strerror(errno));
							break;
						},
						DBG_LOG_ERR("Failed to notify the BT connection event, for %s, and give it up", strerror(errno)),
						);					
					#else
						app_com_send_ipc_msg( app_com_get_ipc_msg_sender_for_ble_to_gen(), (void *)&tp_msg, sizeof(union msg_load));
					#endif
					
					// >>> @20240530 Added by Sean (to turn off scanning once connected)
					DBG_LOG_DEBUG("Turn off scanning as a connection happened ...");
					pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
					kp_cond_var.is_op_done = false;
					clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
					kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_OFF_S;
					if(ST_OK == bt_common_turn_off_scanning( pt_proxy, &kp_cond_var))
					{
						pthread_mutex_lock( &kp_cond_var.mtx);
						while(false == kp_cond_var.is_op_done)
						{
							if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
													&kp_cond_var.mtx,
													&kp_cond_var.tv) == ETIMEDOUT)
							{
								DBG_LOG_ERR("Turnning off scanning timeout!");
								break;
							}
						}
						pthread_mutex_unlock( &kp_cond_var.mtx);
					}
					DBG_LOG_DEBUG("Turn off scanning successfully ...");
					// <<<

#if (SCANNING_IN_A_CONNECTION == 1)
					// To keep scanning even a connection to sensor has been established
					if(BT_ROLE_CLIENT ==  pt_app_mgt->bt_role)
					{
						kp_cond_var.is_op_done = false;
						clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
						kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_ON_S;
						if(ST_OK == bt_common_turn_on_scanning( pt_proxy, &kp_cond_var))
						{
							pthread_mutex_lock( &kp_cond_var.mtx);
							while(false == kp_cond_var.is_op_done)
							{
								if(pthread_cond_timedwait(&kp_cond_var.cond_var, &kp_cond_var.mtx,&kp_cond_var.tv) == ETIMEDOUT)
								{
									DBG_LOG_ERR("Turnning on scanning timeout!");
									kp_cond_var.para++;
									break;
								}
							}
							pthread_mutex_unlock( &kp_cond_var.mtx);
						}
						// pthread_mutex_lock( &kp_cond_var.mtx);
						// while(1)
						// {
						// 	if(false == kp_cond_var.is_op_done)
						// 	{
						// 		pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
						// 	}
						// 	else
						// 	{
						// 		break;
						// 	}
						// }
						// pthread_mutex_unlock( &kp_cond_var.mtx);
						if(kp_cond_var.para == 0)
						{
							DBG_LOG_DEBUG("Turn on scanning successfully ...");
							break;
						}
					}
#endif /* SCANNING_IN_A_CONNECTION == 1 */
					//MK1375D____________________________________________________________________________________


                    // >>> @20240524 Added by Sean (to fix the "fail to reconnect bug")
					//pthread_mutex_lock( &pt_app_mgt->mtx_q_characteristic);	
                    pt_proxy = bt_gatt_find_characteristic( sys_get_gatt_characteristic_queue(sys_get_mgt()), NORDIC_UART_CHARAC_TX_UUID);
                    if(NULL == pt_proxy)
                    {
						//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	
                        DBG_LOG_WARN("fail to find %s", NORDIC_UART_CHARAC_TX_UUID);
                        break;
                    }
                    if(false == bt_gatt_is_notification_acquired( pt_proxy))
                    {// to enable NORDIC TX notification
                        bt_gatt_acquire_notify( pt_proxy);
                    }
					//pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);	
                    // <<<

					

					break;
				}
				case BT_EVENT_POWERON:
				{ // to make sure the power is on
					pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
					DBG_LOG_INFO("to advertise / scan, bt-role %d", pt_app_mgt->bt_role);
					if(BT_ROLE_CLIENT ==  pt_app_mgt->bt_role)
					{
						// >>> @20240530 Added by Sean (just avoid re-turn on scanning, so turn off scanning first)
						DBG_LOG_DEBUG("Turn off scanning as a power on happened ...");
						kp_cond_var.is_op_done = false;
						clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
						kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_OFF_S;
						if(ST_OK == bt_common_turn_off_scanning( pt_proxy, &kp_cond_var))
						{
							pthread_mutex_lock( &kp_cond_var.mtx);
							while(false == kp_cond_var.is_op_done)
							{
								if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
														&kp_cond_var.mtx,
														&kp_cond_var.tv) == ETIMEDOUT)
								{
									DBG_LOG_ERR("Turnning off scanning timeout!");
									break;
								}
							}
							pthread_mutex_unlock( &kp_cond_var.mtx);
						}
						DBG_LOG_DEBUG("Turn off scanning successfully ...");
						// <<<

						kp_cond_var.is_op_done = false;
						clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
						kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_ON_S;
						if(ST_OK == bt_common_turn_on_scanning( pt_proxy, &kp_cond_var))
						{
							pthread_mutex_lock( &kp_cond_var.mtx);
							while(false == kp_cond_var.is_op_done)
							{
								if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
														&kp_cond_var.mtx,
														&kp_cond_var.tv) == ETIMEDOUT)
								{
									DBG_LOG_ERR("Turnning on scanning timeout!");
									kp_cond_var.para++;
									break;
								}
							}
							pthread_mutex_unlock( &kp_cond_var.mtx);
						}
						if(kp_cond_var.para == 0)
						{
							DBG_LOG_DEBUG("Turn on scanning successfully ...");
							break;
						}
						else
						{
							DBG_LOG_DEBUG("Power off");
							bt_common_set_powered(pt_proxy, false);
							sleep(1);
							bt_common_scanning_init();
							bt_common_set_powered(pt_proxy, true);
							sleep(2);
							DBG_LOG_DEBUG("Power on");
							if(kp_cond_var.para > 2)
							{
								DBG_LOG_ERR("Turn on scanning fail!");
								exit(1);
							}else{
								DBG_LOG_ERR("retry turning on scanning...");
							}
						}
						// pthread_mutex_lock( &kp_cond_var.mtx);
						// while(1)
						// {
						// 	if(false == kp_cond_var.is_op_done)
						// 	{
						// 		pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
						// 	}
						// 	else
						// 	{
						// 		break;
						// 	}
						// }
						// pthread_mutex_unlock( &kp_cond_var.mtx);
					}
					else
					{ ;}// nothing to do
					break;
				}
				case BT_EVENT_ADD_LEADV_MGR:
				{
					if(BT_ROLE_SERVER == pt_app_mgt->bt_role)
					{						
						DBG_LOG_INFO("to turn on LE advertising");
						bt_com_turn_on_le_advertisement( sys_init_get_dbus( pt_app_mgt), sys_get_proxy_le_adv_manager1(pt_app_mgt));
					}
					else
					{ ;} // nothing to do
					break;
				}
				case BT_EVENT_TO_ADV:
				{// to turn on advertising , turn dev to connectable
					DBG_LOG_INFO("BT advertising is on");
					
					break;
				}
				case BT_EVENT_TO_SCANNING:
				{// to turn on scanning
					DBG_LOG_INFO("BT scanning is on");
					break;
				}
				case BT_EVENT_SW_TO_CLIENT:
				{
					int tp_msgid = -1;
					struct ipc_msg_v2 tp_msg = {0};
					
					DBG_LOG_INFO("=== to switch to BT client @ %d", time(NULL));
					if(BT_ROLE_CLIENT == sys_get_bluetooth_role( pt_app_mgt))
					{
						DBG_LOG_INFO("the current BT role is client already");
						break;
					}
					// to check and disconnect the current connection
					pt_proxy = sys_get_connected_proxy( pt_app_mgt);
					if(pt_proxy)
					{
						kp_cond_var.is_op_done = false;
						clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
						kp_cond_var.tv.tv_sec += TIMEOUT_BT_DISCONNECTION_S;
						if(ST_OK == bt_con_disconnect(pt_proxy, &kp_cond_var))
						{
							pthread_mutex_lock( &kp_cond_var.mtx);
							while(false == kp_cond_var.is_op_done)
							{
								if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
														  &kp_cond_var.mtx,
														  &kp_cond_var.tv) == ETIMEDOUT)
								{
									DBG_LOG_ERR("Disconnect timeout!");
									// Force to clear the hld_connected_proxy
  									sys_set_connected_proxy(sys_get_mgt(), NULL);
									
									// Force to power off and on
									DBG_LOG_DEBUG("Power off");
									bt_common_set_powered(pt_proxy, false);
									sleep(1);
									bt_common_scanning_init();
									bt_common_set_powered(pt_proxy, true);
									sleep(2);
									DBG_LOG_DEBUG("Power on");

									// Force reset status (this code is copied from "property_changed()")
									pthread_mutex_lock( &pt_app_mgt->mtx_con_event);
									sys_set_connected_proxy( pt_app_mgt, NULL);
									pt_app_mgt->bt_con_event = BT_EVENT_DISCONNECTION;
									pthread_mutex_unlock( &pt_app_mgt->mtx_con_event);
									sys_set_con_in_progress_flg(false); // reset the flag
									// >>> @20240530 Added by Sean (to fix the "gabage-in-the-list bug")
									bt_common_rm_devices( sys_get_proxy_adapter1(sys_get_mgt()), pt_proxy);
									// <<<
									// to signal the disconnection of BT
									pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
									l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)BT_EVENT_DISCONNECTION);
									pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
									pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);
									break;
								}
							}
							pthread_mutex_unlock( &kp_cond_var.mtx);
						}
						// // wait for disconnection OP to be done
						// pthread_mutex_lock( &kp_cond_var.mtx);
						// while(1)
						// {
						// 	if(false == kp_cond_var.is_op_done)
						// 	{
						// 		pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
						// 	}
						// 	else
						// 	{
						// 		break;
						// 	}
						// }
						// pthread_mutex_unlock( &kp_cond_var.mtx);
					}
					//to turn off advertising
					pt_proxy = sys_get_proxy_le_adv_manager1( pt_app_mgt);
					if(NULL == pt_proxy)
					{
						DBG_LOG_ERR("proxy for adv is invalid");
						break;
					}
					kp_cond_var.is_op_done = false;
					bt_com_turn_off_le_advertisement( sys_init_get_dbus( pt_app_mgt),  pt_proxy, &kp_cond_var);
#if (1)
					struct timespec tp_tspec = {0};
#endif
					pthread_mutex_lock( &kp_cond_var.mtx);
#if (1)
					clock_gettime(CLOCK_REALTIME, &tp_tspec);
					tp_tspec.tv_sec = tp_tspec.tv_sec + CONFIG_BT_TURN_OFF_LE_ADVERTISEMENT_TIMEOUT; // to be confirmed.
#endif
					while(1)
					{
						if(false == kp_cond_var.is_op_done)
						{
#if (1)
							int tpint = 0;
							tpint = pthread_cond_timedwait( &kp_cond_var.cond_var, &kp_cond_var.mtx, &tp_tspec);
							if((0 != tpint)&&(ETIMEDOUT == tpint))
							{
								DBG_LOG_ERR("===wait turn off le ad. timeout");
								break;						
							}
#else
							pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
#endif
						}
						else
						{
							break;
						}
					}
					pthread_mutex_unlock( &kp_cond_var.mtx);
					// to turn on scanning
					pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
					if(NULL == pt_proxy)
					{
						DBG_LOG_ERR("no valid proxy ADAPTER1");
						break;
					}

					// >>> @20240530 Added by Sean (just avoid re-turn on scanning, so turn off scanning first)
					DBG_LOG_DEBUG("Turn off scanning as a switch-to-client happened ...");
					pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
					kp_cond_var.is_op_done = false;
					clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
					kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_OFF_S;
					if(ST_OK == bt_common_turn_off_scanning( pt_proxy, &kp_cond_var))
					{
						pthread_mutex_lock( &kp_cond_var.mtx);
						while(false == kp_cond_var.is_op_done)
						{
							if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
													&kp_cond_var.mtx,
													&kp_cond_var.tv) == ETIMEDOUT)
							{
								DBG_LOG_ERR("Turnning off scanning timeout!");
								break;
							}
						}
						pthread_mutex_unlock( &kp_cond_var.mtx);
					}
					DBG_LOG_DEBUG("Turn off scanning successfully ...");
					// <<<
					
					while(1)
					{
						kp_cond_var.is_op_done = false;
						clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
						kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_ON_S;
						if(ST_OK == bt_common_turn_on_scanning( pt_proxy, &kp_cond_var))
						{
							pthread_mutex_lock( &kp_cond_var.mtx);
							while(false == kp_cond_var.is_op_done)
							{
								if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
														&kp_cond_var.mtx,
														&kp_cond_var.tv) == ETIMEDOUT)
								{
									DBG_LOG_ERR("Turnning on scanning timeout!");
									kp_cond_var.para++;
									break;
								}
							}
							pthread_mutex_unlock( &kp_cond_var.mtx);
						}
						// pthread_mutex_lock( &kp_cond_var.mtx);
						// while(1)
						// {
						// 	if(false == kp_cond_var.is_op_done)
						// 	{
						// 		pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
						// 	}
						// 	else
						// 	{
						// 		break;
						// 	}
						// }
						// pthread_mutex_unlock( &kp_cond_var.mtx);
						if(kp_cond_var.para == 0)
						{
							DBG_LOG_DEBUG("Turn on scanning successfully ...");
							break;
						}
						else
						{
							DBG_LOG_DEBUG("Power off");
							bt_common_set_powered(pt_proxy, false);
							sleep(1);
							bt_common_scanning_init();
							bt_common_set_powered(pt_proxy, true);
							sleep(2);
							DBG_LOG_DEBUG("Power on");
							if(kp_cond_var.para > 2)
							{
								DBG_LOG_ERR("Turn on scanning fail!");
								exit(1);
							}else{
								DBG_LOG_ERR("retry turning on scanning...");
							}
						}
					}
					DBG_LOG_INFO("to set the BT-role to %d", BT_ROLE_CLIENT);
					// to set the BT mode
					sys_set_bluetooth_role( pt_app_mgt, BT_ROLE_CLIENT);

					// to notify the BT switch is done
					tp_msgid = sys_get_msgid_to_pro_general( pt_app_mgt);
					if(tp_msgid < 0)
					{
						DBG_LOG_ERR("fail to get valid msg-ID");
						break;
					}
					tp_msg.mtype = M_TYPE_FEEDBACK;
					tp_msg.mtext.feedback_info.cmd = CMD_NOTIFY;
					tp_msg.mtext.feedback_info.msgid = 0;
					tp_msg.mtext.feedback_info.info.event_notification_info.bt_event = BT_EVENT_SW_TO_CLIENT;
					#if(1)
					DELAY_AND_RETRY(msgsnd(tp_msgid, &tp_msg, sizeof(union msg_load), IPC_NOWAIT), 
						500000, 
						100,
						if(EAGAIN == errno)
						{
							DBG_LOG_WARN("Failed to put msg on queue, for %s, and retry\r\n",
										strerror(errno));
						}
						else
						{
							DBG_LOG_ERR("Failed to put msg on queue, for %s\r\n",
										strerror(errno));
							break;
						},
						DBG_LOG_ERR("Failed to put msg on queue, for %s, and give it up", strerror(errno)),
						);
					#else
						app_com_send_ipc_msg( app_com_get_ipc_msg_sender_for_ble_to_gen(), (void *)&tp_msg, sizeof(union msg_load));
					#endif
					break;
				}
				case BT_EVENT_SW_TO_SERVER:
				{
					int tp_msgid = -1;
					struct ipc_msg_v2 tp_msg = {0};
					
					DBG_LOG_INFO("===to switch to BT server @ %d", time(NULL));
					if(BT_ROLE_SERVER == sys_get_bluetooth_role( pt_app_mgt))
					{
						DBG_LOG_INFO("the current BT role is server already");
						break;
					}

					#if(1)
					/*to set BT-role before anything to be done, there is a reason ! ref@msg_reply_for_bt_connect_v2*/ 
					//set the BT role to BT-server
					sys_set_bluetooth_role( pt_app_mgt, BT_ROLE_SERVER);
					#endif
					
					pt_proxy = sys_get_connected_proxy( pt_app_mgt);
					if(pt_proxy)
					{ 
						// to disconnect from the peer device first
						kp_cond_var.is_op_done = false;
						clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
						kp_cond_var.tv.tv_sec += TIMEOUT_BT_DISCONNECTION_S;
						if(ST_OK == bt_con_disconnect(pt_proxy, &kp_cond_var))
						{
							pthread_mutex_lock( &kp_cond_var.mtx);
							while(false == kp_cond_var.is_op_done)
							{
								if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
														  &kp_cond_var.mtx,
														  &kp_cond_var.tv) == ETIMEDOUT)
								{
									DBG_LOG_ERR("Disconnect timeout!");
									// Force to clear the hld_connected_proxy
  									sys_set_connected_proxy(sys_get_mgt(), NULL);

									// Force to power off and on
									DBG_LOG_DEBUG("Power off");
									bt_common_set_powered(pt_proxy, false);
									sleep(1);
									bt_common_scanning_init();
									bt_common_set_powered(pt_proxy, true);
									sleep(2);
									DBG_LOG_DEBUG("Power on");

									// Force reset status (this code is copied from "property_changed()")
									pthread_mutex_lock( &pt_app_mgt->mtx_con_event);
									sys_set_connected_proxy( pt_app_mgt, NULL);
									pt_app_mgt->bt_con_event = BT_EVENT_DISCONNECTION;
									pthread_mutex_unlock( &pt_app_mgt->mtx_con_event);
									sys_set_con_in_progress_flg(false); // reset the flag
									// >>> @20240530 Added by Sean (to fix the "gabage-in-the-list bug")
									bt_common_rm_devices( sys_get_proxy_adapter1(sys_get_mgt()), pt_proxy);
									// <<<
									// to signal the disconnection of BT
									pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
									l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)BT_EVENT_DISCONNECTION);
									pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
									pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);

									break;
								}
							}
							pthread_mutex_unlock( &kp_cond_var.mtx);
						}
						// kp_cond_var.is_op_done = false;
						// bt_con_disconnect( pt_proxy, &kp_cond_var);
						// // wait for disconnection OP to be done
						// pthread_mutex_lock( &kp_cond_var.mtx);
						// while(1)
						// {
						// 	if(false == kp_cond_var.is_op_done)
						// 	{
						// 		pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
						// 	}
						// 	else
						// 	{
						// 		break;
						// 	}
						// }
						// pthread_mutex_unlock( &kp_cond_var.mtx);
					}
					// to turn off BT scanning
					pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
					if(NULL == pt_proxy)
					{
						DBG_LOG_ERR("no valid adapter is added yet");
						break;
					}
					// to wait for scanning is off
					kp_cond_var.is_op_done = false;
					clock_gettime(CLOCK_MONOTONIC, &kp_cond_var.tv);
					kp_cond_var.tv.tv_sec += TIMEOUT_BT_TURNING_OFF_S;
					if(ST_OK == bt_common_turn_off_scanning( pt_proxy, &kp_cond_var))
					{
						pthread_mutex_lock( &kp_cond_var.mtx);
						while(false == kp_cond_var.is_op_done)
						{
							if(pthread_cond_timedwait(&kp_cond_var.cond_var, 
													&kp_cond_var.mtx,
													&kp_cond_var.tv) == ETIMEDOUT)
							{
								DBG_LOG_ERR("Turnning off scanning timeout!");
								break;
							}
						}
						pthread_mutex_unlock( &kp_cond_var.mtx);
					}
					// pthread_mutex_lock( &kp_cond_var.mtx);
					// while(1)
					// {
					// 	if(false == kp_cond_var.is_op_done)
					// 	{
					// 		pthread_cond_wait( &kp_cond_var.cond_var, &kp_cond_var.mtx);
					// 	}
					// 	else
					// 	{
					// 		break;
					// 	}
					// }
					// pthread_mutex_unlock( &kp_cond_var.mtx);

					// to turn on advertising					
					bt_com_turn_on_le_advertisement( sys_init_get_dbus( pt_app_mgt), sys_get_proxy_le_adv_manager1(pt_app_mgt));
					DBG_LOG_INFO("to set the BT-role to %d @ %d", BT_ROLE_SERVER, time(NULL));
					//set the BT role to BT-server
					sys_set_bluetooth_role( pt_app_mgt, BT_ROLE_SERVER);

					// to notify the SW is done
					tp_msgid = sys_get_msgid_to_pro_general( pt_app_mgt);
					if(tp_msgid < 0)
					{
						DBG_LOG_ERR("invalid msg-ID");
						break;
					}
					tp_msg.mtype = M_TYPE_FEEDBACK;
					tp_msg.mtext.feedback_info.cmd = CMD_NOTIFY;
					tp_msg.mtext.feedback_info.msgid = 0;
					tp_msg.mtext.feedback_info.info.event_notification_info.bt_event = BT_EVENT_SW_TO_SERVER;
					#if(1)
					DELAY_AND_RETRY(msgsnd(tp_msgid, &tp_msg, sizeof(union msg_load), IPC_NOWAIT), 
						500000, 
						100,
						if(EAGAIN == errno)
						{
							DBG_LOG_WARN("Failed to notify the BT SW is done, for %s, and retry\r\n",
										strerror(errno));
						}
						else
						{
							DBG_LOG_ERR("Failed to notify the BT SW is done, for %s\r\n",
										strerror(errno));
							break;
						},
						DBG_LOG_ERR("Failed to notify the BT SW is done, for %s, and give it up", strerror(errno)),
						);
					#else
						app_com_send_ipc_msg( app_com_get_ipc_msg_sender_for_ble_to_gen(), (void *)&tp_msg, sizeof(union msg_load));
					#endif
					break;
				}
				case BT_EVENT_SW_TO_DISCONNECT:
				{ // just disconnect from the peer device
					bt_common_swtich_bt_connection( pt_app_mgt, &kp_cond_var);	
					break;
				}
				default:
				{
					DBG_LOG_ERR("Oops! This is not expected to happen");
					break;
				}
			}
		#endif
		
	}
}

/*threads for BT notification
*/
static void*thandler_bt_notification_sending(void*pt_para)
{
	app_management *pt_app_mgt = NULL;
	struct data_unit *pt_dtunit = NULL;
	struct pthread_cond_var kp_cond_for_notification_synch = {0};

	if(ST_OK != app_com_init_pthread_cond_var_ctr(&kp_cond_for_notification_synch))
	{
		DBG_LOG_ERR("***fail to create cond-var for synchronization");
	}
	pt_app_mgt = sys_get_mgt();
	while(1)
	{
		DBG_LOG_INFO("Going to wait for data-unit from the trans queue");
		pt_dtunit = app_com_wait_for_data_trans_queue( app_com_get_notification_trans_ctr());
		if(NULL == pt_dtunit)
		{
			DBG_LOG_WARN("no valid data-unit gotten");
			continue;
		}

		kp_cond_for_notification_synch.is_op_done = false;
		if(ST_OK != bt_gatt_charac_send_notification( pt_app_mgt, pt_dtunit->pt_buf, pt_dtunit->len, &kp_cond_for_notification_synch))
		{
			DBG_LOG_ERR("fail to send notification");
			app_com_release_data_unit( pt_dtunit);
			continue;
		}

#if (1)
		struct timespec tp_tspec = {0};
#endif
		// to wait for the value-notification being done
		pthread_mutex_lock( &kp_cond_for_notification_synch.mtx);
#if (1)
		clock_gettime(CLOCK_REALTIME, &tp_tspec);
		tp_tspec.tv_sec = tp_tspec.tv_sec + CONFIG_BT_GATT_CHARAC_WAIT_FOR_NOTIFICATION_TIMEOUT; // to be confirmed.
#endif
		while(1)
		{
			if(false == kp_cond_for_notification_synch.is_op_done)
			{
				DBG_LOG_INFO("to wait for value-notification to be done");
#if (1)
				int tpint = 0;
				tpint = pthread_cond_timedwait( &kp_cond_for_notification_synch.cond_var, &kp_cond_for_notification_synch.mtx, &tp_tspec);
				if((0 != tpint)&&(ETIMEDOUT == tpint))
				{
				  DBG_LOG_ERR("===wait value-notification timeout");
				  break;						
				}
#else
					pthread_cond_wait( &kp_cond_for_notification_synch.cond_var, &kp_cond_for_notification_synch.mtx);
#endif
			}
			else
			{
				break;
			}
		}
		pthread_mutex_unlock( &kp_cond_for_notification_synch.mtx);
		// to releae the data unit
		app_com_release_data_unit( pt_dtunit);
	}
	// the release the cond-var
	app_com_deinit_pthread_cond_var_ctr( &kp_cond_for_notification_synch);
}

/*threads for BT scanning/advertising management
*/
static void*thandler_bt_scanning_advertising_manager(void*pt_para)
{
	int kp_status = 0;
	pid_t kp_pid;
	DBG_LOG_INFO("thread for scanning/advertising management");
	while(1)
	{// to complete
		kp_pid = wait(&kp_status);
		printf("child-process(%d) exit with status %d\n", kp_pid, kp_status);
		sleep(5);
		exit(1);
	}
}


/*threads to receive IPC-message from pro-general
*/
static void thandler_bt_recv_ipc_msg(void*pt_parg)
{
	ssize_t kp_sz;
	int hld_msgid_from_pro_gen = sys_get_msgid_from_pro_general(sys_get_mgt());
	struct ipc_msg_v2 kp_ipc_msg = {0};
	enum bluetooth_role kp_btrole = BT_ROLE_UNKNOWN;
	
	DBG_LOG_INFO(" is running");
	while(1)
	{
		memset( &kp_ipc_msg, 0, sizeof(kp_ipc_msg));
		kp_sz = msgrcv( hld_msgid_from_pro_gen, (void*)&kp_ipc_msg, sizeof(union msg_load), 0, 0);
		if(kp_sz < 0)
		{
			DBG_LOG_ERR("fail to receive msg from %d", hld_msgid_from_pro_gen);
			continue;
		}
		DBG_LOG_INFO("ipc_msg type %ld, load-len %ld", kp_ipc_msg.mtype, kp_sz);
		switch (kp_ipc_msg.mtype)
		{
			case M_TYPE_PROTO_DATA: 
			{ // pass the message on to the peer-device
				#warning "TODO====to fix the following"
				if(NULL == sys_get_connected_proxy(sys_get_mgt()))
				{// this is not a good solution, to fix it
					DBG_LOG_WARN("no BT connection yet");
					break;
				}
				DBG_LOG_INFO("the msg-queue ID is %d", hld_msgid_from_pro_gen);
				DBG_LOG_INFO("the proto-pkt len is %u", kp_ipc_msg.mtext.proto_pkt.len);
				kp_btrole = sys_get_bluetooth_role( sys_get_mgt());
				if(BT_ROLE_CLIENT == kp_btrole)
				{
					app_com_post_to_data_trans_queue( app_com_get_data_trans_ctr(), (uint8_t *)kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_ipc_msg.mtext.proto_pkt.len);
				}
				else if(BT_ROLE_SERVER == kp_btrole)
				{
					app_com_post_to_data_trans_queue( app_com_get_notification_trans_ctr(), (uint8_t *)kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_ipc_msg.mtext.proto_pkt.len);					
				}
				else
				{
					DBG_LOG_WARN("unknown BT role");
				}
				break;
			}
			case M_TYPE_COMMAND: //
			{ //
	
				app_pro_hanlder_for_cmd( &kp_ipc_msg);

				break;
			}
			case M_TYPE_FEEDBACK:
			{//
				DBG_LOG_WARN("***Well this is unexpected!");
				break;
			}
			default:
			{
				DBG_LOG_ERR("This is not supposed to happen");
				break;
			}
		}
	}
}


/*to create threads for BT operation
*/
static void pro_ble_create_threads(void)
{
	
	int tpint = -1;
	pthread_t kp_thread = -1;
	tpint = pthread_create( &kp_thread, NULL, thandler_bt_test, NULL);
	if(0 != tpint)
	{
		DBG_LOG_ERR("fail to create thanddler_bt_test");
		return;
	}

	tpint = pthread_create( &kp_thread, NULL, thandler_bt_connection_manager, NULL);
	if(0 != tpint)
	{
		DBG_LOG_ERR("fail to create thread for connection management");
		return;
	}

	tpint = pthread_create( &kp_thread, NULL, thandler_bt_scanning_advertising_manager, NULL);
	if(0 != tpint)
	{
		DBG_LOG_ERR("fail to create thandler_bt_scanning/advertising management");
		return;
	}
	tpint = pthread_create( &kp_thread, NULL, thandler_bt_data_sending, NULL);
	if(0 != tpint)
	{
		DBG_LOG_ERR("fail to create thandler_bt_data_sending");
		return;
	}
	tpint = pthread_create( &kp_thread, NULL, thandler_bt_notification_sending, NULL);
	if(0 != tpint)
	{
		DBG_LOG_ERR("fail to create thander_bt_notification_sending");
		return; 
	}
	// for IPC-msg receivation
	tpint = pthread_create( &kp_thread, NULL, thandler_bt_recv_ipc_msg, NULL);
	if(0 != tpint)
	{
		DBG_LOG_ERR("fail to create thread");
		return;
	}
}

static app_state_t to_create_time_routine(void);


//extern uint32_t g_tpval;
int pro_ble_handler(const int msgid_in, const int msgid_out);

void
app_pro_ble(void)
{
  key_t kp_key = -1;
  int hld_msgid_to_pro_general = -1, hld_msgid_from_pro_general = -1;

  // To create the message queue for IPC
  kp_key = ftok(FPATH_FOR_IPC0, PROJECT_ID_FOR_ICP);
  hld_msgid_to_pro_general = msgget(kp_key,
                                    PERMISSION_FLAG_FOR_IPC0 | IPC_CREAT);
  if(hld_msgid_to_pro_general == -1)
  {
    DBG_LOG_ERR("Fail to create msg-queue");
    exit(EXIT_FAILURE);
  }
  kp_key = ftok(FPATH_FOR_IPC1, PROJECT_ID_FOR_ICP);
  hld_msgid_from_pro_general = msgget(kp_key,
                                      PERMISSION_FLAG_FOR_IPC1 |
                                      IPC_CREAT);
  if(hld_msgid_from_pro_general == -1)
  {
    DBG_LOG_ERR("Fail to create msg-queue");
    exit(EXIT_FAILURE);
  }
  DBG_LOG_INFO("msg_to_pro_general %d, msg_from_pro_general %d\n",
               hld_msgid_to_pro_general, hld_msgid_from_pro_general);

	// to initialize the IPC-msg sender
#if 0 // Disabled as we are using the IPC communication in a block manner
	app_com_init_ipc_msg_sender(
		app_com_get_ipc_msg_sender_for_ble_to_gen(), 
		hld_msgid_to_pro_general, 
		sizeof(union msg_load));
#endif

	//initialization for BT
	#if(1) // for debug only
			if(sys_init_app_management(sys_get_mgt()) != ST_OK)
			{
				DBG_LOG_ERR("fail to init app-mamagement");
				return;
			}
			// to setup the message-queue for IPC with process-general
			sys_init_set_msgid( sys_get_mgt(), hld_msgid_from_pro_general, hld_msgid_to_pro_general);
			//sys_dump_bt_wlist((const sys_conf *)&g_app_management.conf);
			//sys_destroy_app_management(&g_app_management);
			//return 0;	
	#endif
	
	#if(1)
		// to do some initialization for sychronization between threads created below
		#warning "TODO===remember to destroy resource created here!"
		app_com_init_data_trans_ctr( app_com_get_data_trans_ctr());
		app_com_init_data_trans_ctr( app_com_get_notification_trans_ctr());

		// to create threads for BT operation
		pro_ble_create_threads();
		DBG_LOG_INFO("to start the BT");
		// start the BT service
		pro_ble_handler( hld_msgid_from_pro_general, hld_msgid_to_pro_general);
	#else // only for test
	struct ipc_msg kp_msg = {0};
	uint32_t *pt_u32 = NULL;
	ssize_t kp_sz = 0;
	while(1)
	{
		//sleep(3);
		printf("this is the process-ble global-tpval@0x%x = %d\n", &g_tpval, g_tpval);
		g_tpval ++;

		kp_sz = msgrcv( hld_msgid_from_pro_general,(void*)&kp_msg, sizeof(kp_msg.val), 1, 0);
		if(kp_sz == -1)
		{
			printf("fail to receive msg from queue\n");
			continue;
		}
		printf("the mseesage received(sz %d) is %d-%d\n", kp_sz,kp_msg.type, kp_msg.val);
		
		//break;
	}
	#endif
}


/*handler to deal with the command from process-general
ret@ST_OK
*/
app_state_t app_pro_hanlder_for_cmd(const struct ipc_msg_v2 *pt_msg)
{
	app_state_t tpret = ST_OK;
	app_management *pt_app_mgt = sys_get_mgt();

	
	struct ipc_msg_v2 tp_ipc_msg = {0};
	int tp_msgid_to_pro_general = sys_get_msgid_to_pro_general(sys_get_mgt());
	union msg_feedback_info tp_fdback_info = {0};
	
	if(NULL == pt_msg)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}
	if(M_TYPE_COMMAND != pt_msg->mtype)
	{
		DBG_LOG_ERR("unexected msg type");
		tpret = ST_ERR;
		return tpret;
	}
	switch (pt_msg->mtext.cmd_info.cmd)
	{
		case CMD_SW_TO_SERVER:
		{
			DBG_LOG_ERR("****to switch to BT server");
			// to trig the switch of BT mode
			pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
			l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)BT_EVENT_SW_TO_SERVER);
			pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
			pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);
			
			tp_fdback_info.tpu32 = 555;
			
			break;
		}
		case CMD_SW_TO_CLIENT:
		{
			DBG_LOG_ERR("***to switch to Client");
			// to trig the switch of BT mode
			pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
			l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)BT_EVENT_SW_TO_CLIENT);
			pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
			pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);

			tp_fdback_info.tpu32 = 555;
			
			break;
		}
		case CMD_NOTIFY:
		{
			DBG_LOG_ERR("CMD_NOTIFY is unexpected here");
			tpret = ST_ERR;
			break;
		}
		case CMD_ACQUIRE_GWID:
		{
			snprintf( tp_fdback_info.gw_id, MAX_ID_STR_LENGTH, "%s", sys_get_gw_id_string());
			break;
		}
		case CMD_SET_ACTIVE_BTCLIENT:
		{ // to set the active client

			/*MK1202 Now we seach the dev-queue ,to find any BT-device that is on white-list and connect to it. so command "CMD_SET_ACTIVE_BTCLIENT" is not necessary anymore.
			20240528
			*/
			
			//sys_set_active_bt_device( pt_msg->mtext.cmd_info.para.btdev_to_active);			
			//DBG_LOG_INFO("the active client is set to %s", sys_get_active_bt_device());
			tp_fdback_info.tpu32 = 666;
			
			break;
		}
		case CMD_DISCONNECTION:
		{ // 
			// put the event on the queue
			pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);

			//sys_set_active_bt_device( pt_msg->mtext.cmd_info.para.btdev_to_connect); // overwrite the active device, ref@MK1202
			
			l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)BT_EVENT_SW_TO_DISCONNECT);
			pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
			pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);
			break;
		}
		case CMD_SET_WHITE_LIST:
		{
			pthread_mutex_lock(&pt_app_mgt->mtx_for_white_list);
			sys_set_bt_white_list( pt_msg->mtext.cmd_info.para.bt_white_list, MAX_BT_WHITE_LIST);
			pthread_mutex_unlock(&pt_app_mgt->mtx_for_white_list);
			DBG_LOG_INFO("the whilte list is set!");
			break;
		}
		default:
		{
			DBG_LOG_WARN("unexpected CMD type %d", pt_msg->mtext.cmd_info.cmd);
			tpret = ST_ERR;
			break;
		}
	}

	// to give the feedback which means the message is received successfully
	if(tp_msgid_to_pro_general < 0)
	{
		DBG_LOG_ERR("invalid msg-ID");
		tpret = ST_ERR;
		return tpret;
	}
	tp_ipc_msg.mtype = M_TYPE_FEEDBACK;
	tp_ipc_msg.mtext.feedback_info.cmd = pt_msg->mtext.cmd_info.cmd;
	tp_ipc_msg.mtext.feedback_info.msgid = pt_msg->mtext.cmd_info.msgid;
	memcpy( &tp_ipc_msg.mtext.feedback_info.info, &tp_fdback_info, sizeof(union msg_feedback_info));
	
	#if(1) //the original branch
	DELAY_AND_RETRY(msgsnd(tp_msgid_to_pro_general, &tp_ipc_msg, sizeof(union msg_load), IPC_NOWAIT), 
					500000, 
					100,
					if(EAGAIN == errno)
					{
						DBG_LOG_WARN("Failed to send feedback to process general, for %s, and retry\r\n",
									strerror(errno));
					}
					else
					{
						DBG_LOG_ERR("Failed to send feedback to process general, for %s\r\n",
									strerror(errno));
						tpret = ST_ERR;
						return tpret;
					},
					DBG_LOG_ERR("Failed to send feedback to process general, for %s, and give it up", strerror(errno));
					tpret = ST_ERR;
					return tpret;,
					);
	#else
		app_com_send_ipc_msg( app_com_get_ipc_msg_sender_for_ble_to_gen(), (void *)&tp_ipc_msg, sizeof(union msg_load));
	#endif

	return tpret;
}







