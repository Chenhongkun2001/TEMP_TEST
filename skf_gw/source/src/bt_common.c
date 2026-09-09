#include "bt_common.h"
#include "sys_def.h"
#include "util_dbg.h"
#include "bt_advertising.h"
#include "app_common.h"
#include "bt_connection.h"

DBG_LOCAL_LOG_DEBUG


static pthread_mutex_t mtx_scanning;

// callback to set up scanning filter
static void bt_common_msg_setup_for_scan_filter(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder *ptbuilder = NULL;
	//pthread_mutex_lock( &mtx_scanning);
	DBG_LOG_INFO("To set up scanning filter");

	ptbuilder = l_dbus_message_builder_new( message);
	l_dbus_message_builder_enter_array( ptbuilder, "{sv}");

	l_dbus_message_builder_enter_dict( ptbuilder, "sv");
	l_dbus_message_builder_append_basic( ptbuilder, 's', "Transport");
	l_dbus_message_builder_enter_variant( ptbuilder, "s");
	l_dbus_message_builder_append_basic( ptbuilder, 's', "le");
	l_dbus_message_builder_leave_variant( ptbuilder);
	l_dbus_message_builder_leave_dict(ptbuilder);

	l_dbus_message_builder_enter_dict( ptbuilder, "sv");
	l_dbus_message_builder_append_basic( ptbuilder, 's', "Pattern");
	l_dbus_message_builder_enter_variant( ptbuilder, "s");
	l_dbus_message_builder_append_basic( ptbuilder, 's', "C4:BD:6A");
	l_dbus_message_builder_leave_variant( ptbuilder);
	l_dbus_message_builder_leave_dict(ptbuilder);

	l_dbus_message_builder_leave_array(ptbuilder);

	l_dbus_message_builder_finalize( ptbuilder);
	l_dbus_message_builder_destroy( ptbuilder);
}


static void bt_common_msg_for_scan_filter_reply(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	DBG_LOG_INFO("Hello there");
	if(l_dbus_message_is_error(result))
	{
		const char *ptname = NULL, *ptdesc = NULL;
		l_dbus_message_get_error( result, &ptname, &ptdesc);
		DBG_LOG_WARN("Fail to set discovery filter:%s, %s", ptname, ptdesc);
	}
	else
	{
		DBG_LOG_INFO("the filter for BLE scanning is set successfully");
	}
	//pthread_mutex_unlock( &mtx_scanning);
}

//l_dbus_message_func_t setup
static 	void bt_common_msg_setup_for_start_discovery(struct l_dbus_message *message,void *user_data)
{
	//pthread_mutex_lock( &mtx_scanning);
	DBG_LOG_INFO("set up message for method start_discovery");
	l_dbus_message_set_arguments( message, "");
}

//l_dbus_client_proxy_result_func_t reply,
static void bt_common_msg_for_start_discovery_reply(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	const char*pt_name = NULL, *pt_text = NULL;
	struct pthread_cond_var *pt_cond_var = NULL;

	if(l_dbus_message_is_error( result))
	{
		if(l_dbus_message_get_error( result, &pt_name, &pt_text))
		{
			DBG_LOG_ERR("fail to start discovery, with err-info :%s-%s", pt_name, pt_text);
		}
		else
		{
			DBG_LOG_ERR("fail to start discovery, fail to get err-info");
		}

		if(user_data)
		{
			pt_cond_var = (struct pthread_cond_var*)user_data;
			pthread_mutex_lock( &pt_cond_var->mtx);
			pt_cond_var->para++;
			pthread_mutex_unlock( &pt_cond_var->mtx);
		}
	}
	else
	{
		if(user_data)
		{
			pt_cond_var = (struct pthread_cond_var*)user_data;
			pthread_mutex_lock( &pt_cond_var->mtx);
			pt_cond_var->para = 0;
			pthread_mutex_unlock( &pt_cond_var->mtx);
		}
		DBG_LOG_DEBUG("Start discovery successfully");
	}
#if (SKF_GW_NEW != 1)
	#if(1) // to signal the thread when start discovery is done
		app_management *pt_app_mgt = sys_get_mgt();
		enum bluetooth_event kp_btevent = BT_EVENT_TO_SCANNING;
		pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
		l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)kp_btevent);
		pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
		pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);
	#endif
#endif
	// >>> @20240530 Added by Sean (to change the flag in pt_cond_var)
	#if(1)
		if(user_data)
		{
			pt_cond_var = (struct pthread_cond_var*)user_data;
			pthread_mutex_lock( &pt_cond_var->mtx);
			pt_cond_var->is_op_done = true;
			pthread_cond_signal( &pt_cond_var->cond_var);
			pthread_mutex_unlock( &pt_cond_var->mtx);
		}
	#endif
	// <<<
	DBG_LOG_DEBUG("bt_common_msg_for_start_discovery_reply done");
	//pthread_mutex_unlock( &mtx_scanning);
}

void bt_common_scanning_init(void)
{
	DBG_LOG_DEBUG("init mtx_scanning");
	if(pthread_mutex_init( &mtx_scanning, NULL))
	{
		DBG_LOG_ERR("fail to init the mutex for scanning");
		return;
	}
}
void bt_common_scanning_deinit(void)
{
	pthread_mutex_destroy(&mtx_scanning);
}

/*Function:to turn on BT scanning for BLE devices
ret@ST_OK when things go well
*/
app_state_t bt_common_turn_on_sanning(struct l_dbus_proxy*pt_proxy, void*user_data)
{
	app_state_t tpret = ST_OK;
	char *pt_str = NULL;
	const char *pt_interface = l_dbus_proxy_get_interface(pt_proxy);
	const char*pt_path = l_dbus_proxy_get_path(pt_proxy);
	if(strcmp(pt_interface, "org.bluez.Adapter1"))
	{
		DBG_LOG_WARN("invalid Adapter");
		tpret = ST_ERR;
		goto ERR;
	}

	// to get the local address
	if(!l_dbus_proxy_get_property( pt_proxy, "Address", "s", &pt_str))
	{
		DBG_LOG_ERR("Fail to get BT Address");
		tpret = ST_ERR;
		goto ERR;
	}
	DBG_LOG_INFO("the local address : %s", pt_str);

	//l_dbus_proxy_set_property( pt_proxy, NULL, NULL, NULL, "Powered", "b", 1);
	l_dbus_proxy_method_call( pt_proxy, "SetDiscoveryFilter", bt_common_msg_setup_for_scan_filter, bt_common_msg_for_scan_filter_reply, NULL, NULL);
	l_dbus_proxy_method_call( pt_proxy, "StartDiscovery", bt_common_msg_setup_for_start_discovery, bt_common_msg_for_start_discovery_reply, NULL, NULL);
	return tpret;
	ERR:
		return tpret;
}

// >>> @20240530 Added by Sean (to change the flag in pt_cond_var)
/**
 * @brief: To turn on BT scanning and return ST_OK when things go well.
 */
app_state_t
bt_common_turn_on_scanning(
  struct l_dbus_proxy *pt_proxy,
  struct pthread_cond_var *pt_cond_var)
{
  app_state_t tpret = ST_OK;
  char *pt_str = NULL;
  const char *pt_interface = l_dbus_proxy_get_interface(pt_proxy);
  const char *pt_path = l_dbus_proxy_get_path(pt_proxy);

  if(strcmp(pt_interface, "org.bluez.Adapter1"))
  {
    DBG_LOG_ERR("Invalid Adapter");
    tpret = ST_ERR;
    goto ERR;
  }

  // To get the local address
  if(!l_dbus_proxy_get_property(pt_proxy, "Address", "s", &pt_str))
  {
    DBG_LOG_ERR("Failed to get BT Address");
    tpret = ST_ERR;
    goto ERR;
  }
  DBG_LOG_INFO("The local address : %s", pt_str);

  //l_dbus_proxy_set_property( pt_proxy, NULL, NULL, NULL, "Powered", "b",
  // 1);
  if(0 == l_dbus_proxy_method_call(pt_proxy, "SetDiscoveryFilter",
          			               bt_common_msg_setup_for_scan_filter,
                    			   bt_common_msg_for_scan_filter_reply, 
								   NULL,
                           		   NULL))
  {
	DBG_LOG_ERR("Failed to call method SetDiscoveryFilter");
	tpret = ST_ERR;
	goto ERR;
  }
  if(0 == l_dbus_proxy_method_call(pt_proxy, "StartDiscovery",
								   bt_common_msg_setup_for_start_discovery,
								   bt_common_msg_for_start_discovery_reply,
								   pt_cond_var, 
								   NULL))
  {
	DBG_LOG_ERR("Failed to call method StartDiscovery");
	tpret = ST_ERR;
	goto ERR;
  }
ERR:
  return tpret;
}
// <<<


#if(0)
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

#else
	/*
	*/
	void bt_com_turn_on_le_advertisement(struct l_dbus *pt_dbus, struct l_dbus_proxy*pt_proxy)
	{
		uint8_t kp_strbuf[MAX_ID_STR_LENGTH] = {0};
		
		if((pt_dbus == NULL)||(pt_proxy == NULL))
		{
			DBG_LOG_ERR("unexpected pointer null");
			return;
		}
		
		// to set up the advertising info
		bt_advertising_clear_ad_info();
		//bt_advertising_set_includes_appearance( true);
		//bt_advertising_set_includes_tx_power( true);

		bt_advertising_get_nmstr( kp_strbuf, sizeof(kp_strbuf));
		bt_advertising_set_local_name( kp_strbuf);
		//bt_advertising_set_includes_name( true);		
		
		//set advertising interval
		bt_advertising_set_adv_interval(MIN_BT_ADV_INTERVAL + 10, MIN_BT_ADV_INTERVAL + 100);		
		//bt_advertising_set_adv_interval(MAX_BT_ADV_INTERVAL - 100, MAX_BT_ADV_INTERVAL);
		
		// to start the LE advertising
		//bt_advertising_register( pt_dbus, pt_proxy, "on");
		//bt_advertising_register( pt_dbus, pt_proxy, bt_advertising_get_arguments_str(IDX_AD_BROADCAST));
		bt_advertising_register( pt_dbus, pt_proxy, bt_advertising_get_arguments_str(IDX_AD_PERIPHERAL));
	}	
#endif

void bt_com_turn_off_le_advertisement(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy, struct pthread_cond_var*pt_cond_var)
{
	app_state_t tpret = ST_OK;
	if((pt_dbus == NULL)||(NULL == pt_proxy))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	bt_advertising_unregister(pt_dbus, pt_proxy, pt_cond_var);
	return;
}



/*
typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);

to deal with the set-powered result
*/
static void check_set_powered_result (struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{		
	const char*pt_name = NULL, *pt_text = NULL;
	if(l_dbus_message_is_error(result))
	{// fail to setup powered
		if(l_dbus_message_get_error( result, &pt_name, &pt_text))
		{
			DBG_LOG_ERR("fail to set property powered, err-info:%s , %s", pt_name, pt_text);
		}
		else
		{
			DBG_LOG_ERR("fail to get err-info");
		}
		;
	}
	else
	{
		;
	}

#if (SKF_GW_NEW != 1)
	#if(1) // to signal that BT powe-on is done
		#warning "TODO===DO we always power on BT  successfully! to fix it if not"
		app_management *pt_app_mgt = sys_get_mgt();
		enum bluetooth_event kp_btevent = BT_EVENT_UNDEFINED;
		pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
		kp_btevent = BT_EVENT_POWERON;
		l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)kp_btevent);
		pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
		pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);
	#endif
#endif /* SKF_GW_NEW != 1 */
	return;
}
/*typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
static void destroy_for_set_powered (void *user_data)
{
	return;
}

/*to turn on/off the BT
kp_newval : true/false
ret@ST_OK when things go well
*/
app_state_t bt_common_set_powered(struct l_dbus_proxy*pt_proxy, bool kp_newval)
{
	app_state_t tpret = ST_OK;
	const char*pt_interface = NULL;
	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = ST_ERR;
		return tpret;
	}
	pt_interface = l_dbus_proxy_get_interface(pt_proxy);
	if(pt_interface == NULL)
	{
		DBG_LOG_ERR("fail to get proxy interface");
		tpret = ST_ERR;
		return tpret;
	}

	if(strcmp(pt_interface, BT_IF_NM_ADAPTER))
	{
		DBG_LOG_ERR("a proxy with interface %s is expected", BT_IF_NM_ADAPTER);
		tpret = ST_ERR;
		return tpret;
	}
	DBG_LOG_INFO("Going to set Powered to %d", kp_newval);
	l_dbus_proxy_set_property( pt_proxy, check_set_powered_result, NULL, destroy_for_set_powered, "Powered", "b", kp_newval);

}


/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);
*/
static void bt_common_msg_setup_for_stop_discovery(struct l_dbus_message *message,void *user_data)
{
	//pthread_mutex_lock( &mtx_scanning);
	l_dbus_message_set_arguments( message, "");
	return;
}

/*
typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);

*/
static void bt_common_for_stop_discovery_reply(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	#if(1)
		struct pthread_cond_var *pt_cond_var = NULL;
		if(user_data)
		{
			pt_cond_var = (struct pthread_cond_var*)user_data;
			pthread_mutex_lock( &pt_cond_var->mtx);
			pt_cond_var->is_op_done = true;
			pthread_cond_signal( &pt_cond_var->cond_var);
			pthread_mutex_unlock( &pt_cond_var->mtx);
		}
	#endif
	
	if(l_dbus_message_is_error(result))
	{ // error happens
		const char*pt_name = NULL, *pt_text = NULL;
		l_dbus_message_get_error( result, &pt_name, &pt_text);
		DBG_LOG_ERR("fail to stop discovery with err:%s, %s", pt_name, pt_text);
#if (SKF_GW_NEW != 1)
		if(strcmp(pt_text, "Operation already in progress") == 0)
			exit(1);
#endif
		//pthread_mutex_unlock( &mtx_scanning);
		return;
	}
	DBG_LOG_INFO("the Discovery is stopped successfully");
	//pthread_mutex_unlock( &mtx_scanning);
	return;
}

/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
static void bt_common_for_stop_discovery_destroy(void*user_data)
{
	DBG_LOG_WARN("nothing to do!");
}

/*to turn off scanning
ret@ST_OK when things go well
NOTE!!! it is OK to set pt_cond_var to NULL when you do not want to known when the OP is done
*/
app_state_t bt_common_turn_off_scanning(struct l_dbus_proxy*pt_proxy, struct pthread_cond_var*pt_cond_var)
{
	app_state_t tpret = ST_OK;
	const char*pt_interface = NULL;
	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if(pt_interface == NULL)
	{
		DBG_LOG_ERR("fail to get proxy interface");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(strcmp( pt_interface, BT_IF_NM_ADAPTER))
	{
		DBG_LOG_ERR("proxy with interface : %s is expected", BT_IF_NM_ADAPTER);
		tpret = ST_ERR;
		goto EXIT;
	}
	if(0 == l_dbus_proxy_method_call(pt_proxy, "StopDiscovery", bt_common_msg_setup_for_stop_discovery, bt_common_for_stop_discovery_reply, pt_cond_var, bt_common_for_stop_discovery_destroy))
	{
		DBG_LOG_ERR("fail to call method StopDiscovery");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	EXIT:
		return tpret;
}


/*set up argument to set BT power on
typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message, void *user_data);

*/
static void bt_common_dbus_msg_func_sdmsg_to_get_dev_addr (struct l_dbus_message *message, void *user_data)
{
	struct l_dbus_message_builder *ptbuilder = NULL;

	ptbuilder = l_dbus_message_builder_new( message);
	l_dbus_message_builder_append_basic( ptbuilder, 's', "org.bluez.Device1");
	l_dbus_message_builder_append_basic( ptbuilder, 's', "Address");
	l_dbus_message_builder_finalize(ptbuilder);
	l_dbus_message_builder_destroy( ptbuilder); 	

}

/*
typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data);
*/
static void bt_common_dbus_msg_func_recvmsg_from_get_dev_addr(struct l_dbus_message *message,void *user_data)
{
	char*pt_dev_str = NULL;
	uint32_t tpu32 = 0;	
	struct l_dbus_message_iter kpiter = {0};
	
	const char*pt_name = NULL, *pt_text = NULL;
	if(l_dbus_message_is_error( message))
	{
		l_dbus_message_get_error( message, &pt_name, &pt_text);
		DBG_LOG_ERR("fail to get dev addr,err:%s,msg:%s", pt_name, pt_text);
		goto ERR;
	}
	else
	{
		;//DBG_LOG_INFO("Address Getting no error!");
	}
	if(!l_dbus_message_get_arguments( message, "v", &kpiter))
	{
		DBG_LOG_ERR("fail to get argument, tpu32:%u", tpu32);
		goto ERR;
	}
	
	if(!l_dbus_message_iter_get_variant( &kpiter, "s", &pt_dev_str))
	{
		DBG_LOG_ERR("fail to get property Address");
		goto ERR;
	}	
	
	// DBG_LOG_INFO("Dev Address:%s", pt_dev_str);
	if(user_data)
	{
		sprintf( (char*)user_data,"%s", pt_dev_str);
		//((char*)user_data)[0] = 'F';
		DBG_LOG_INFO("Dev-Addr: %s, data-in-buf:%s, user_data@0x%X",  pt_dev_str, user_data, user_data);
	}
	else
	{
		DBG_LOG_WARN("You need to give a buffer, so the addr info(%s) can be returned", pt_dev_str);
	}
	return;
	ERR:
		return;
}




/*Function: to get the address of Device proxy
ptbuf : the buffer where the BT address is returned, Note!!!make sure the buffer has enough space for the address string 
ret@ST_OK is returned when things go well
*/
app_state_t bt_common_get_proxy_addr(struct l_dbus_proxy*pt_proxy, char* ptbuf)
{
	app_state_t tpret = ST_OK;

	struct l_dbus *pt_dbus = NULL;
	const char*pt_path = NULL;
	const char*pt_interface = NULL;
	
	if((pt_proxy==NULL)||(ptbuf==NULL))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		goto ERR;
	}
	pt_dbus = sys_init_get_dbus(sys_get_mgt());
	if(pt_dbus==NULL)
	{
		DBG_LOG_ERR("make sure app management is initialized");	
		tpret = ST_ERR;
		goto ERR;
	}

	pt_path = l_dbus_proxy_get_path( pt_proxy);
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);

	if(!strcmp(pt_interface,"org.bluez.Adapter1")&&!strcmp(pt_interface, "org.bluez.Device1"))
	{
		DBG_LOG_INFO("proxy:%s, %s has not property Address", pt_path, pt_interface);
		tpret = ST_ERR;
		goto ERR;
	}
	//DBG_LOG_INFO(" pt_dbus:0x%x ,path:%s, interface:%s", pt_dbus, pt_path, pt_interface);
	//l_dbus_method_call( pt_dbus, "org.bluez", pt_path, "org.freedesktop.DBus.Properties", "Get", bt_common_dbus_msg_func_sdmsg_to_get_dev_addr, bt_common_dbus_msg_func_recvmsg_from_get_dev_addr, NULL, NULL);
	l_dbus_method_call( pt_dbus, "org.bluez", pt_path, "org.freedesktop.DBus.Properties", "Get", bt_common_dbus_msg_func_sdmsg_to_get_dev_addr, bt_common_dbus_msg_func_recvmsg_from_get_dev_addr, (void*)ptbuf, NULL);

	return tpret;
	ERR:
		return tpret;
}


/*typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
function: the queue-match callback to compare the proxy address
para-a: pointer point to the proxy
para-b: the buffer where the address-string is
ret@true when the address match
*/
static bool queue_match_compare_proxy_address(const void*a, const void*b)
{
	bool tpret = true;
	int tplen = 0;
	
	struct l_dbus_proxy*pt_proxy = *((struct l_dbus_proxy**)a);
	const char*pt_addrbuf = (const char*)b;
	const char*pt_proxy_addr = NULL;

	if(!l_dbus_proxy_get_property( pt_proxy, "Address", "s", &pt_proxy_addr))
	{
		DBG_LOG_ERR("fail to get proxy property Address");
		tpret = false;
		return tpret;
	}

	//DBG_LOG_INFO("proxy_addr_str:%s, addr-given:%s", pt_proxy_addr, pt_addrbuf);

	tplen = strlen(pt_proxy_addr);
	if(strcmp(pt_proxy_addr, pt_addrbuf) != 0)
	{ // the address DOES NOT match
		tpret = false;
	}
	else{
		if((((uint64_t)pt_proxy) > dbusRefAddrUpperBound) || (((uint64_t)pt_proxy) < dbusRefAddrLowerBound))
		{
			DBG_LOG_DEBUG("dbusRefAddrUpperBound: 0x%llx", dbusRefAddrUpperBound);
			DBG_LOG_DEBUG("dbusRefAddrLowerBound: 0x%llx", dbusRefAddrLowerBound);
			DBG_LOG_DEBUG("pt_proxy: 0x%llx", (uint64_t)pt_proxy);
			DBG_LOG_ERR("The proxy is with invalid address");
			//bt_common_rm_devices(sys_get_proxy_adapter1(sys_get_mgt()), pt_proxy);
			//tpret = false;
			//return tpret;
		}
	}
	return tpret;
	
}

/*function:to find the device-proxy by the given address
pt_queue: the device-proxy queue
kp_btaddr : the address of the BT device we are trying to find
ret@ the pointer of the proxy is returned when found, otherwise NULL is returned
*/
struct l_dbus_proxy* bt_common_to_find_dev_proxy(struct l_queue*pt_queue, const struct bt_addr kp_btaddr)
{
	struct l_dbus_proxy **tpret = NULL;
	
	char kp_addrbuf[0x20] = {0};
	snprintf(kp_addrbuf,sizeof(kp_addrbuf),"%02X:%02X:%02X:%02X:%02X:%02X", (kp_btaddr.nap&0xFF00)>>8,(kp_btaddr.nap&0xFF),kp_btaddr.uap,(kp_btaddr.lap&0xFF0000)>>16,(kp_btaddr.lap&0xFF00)>>8,kp_btaddr.lap&0xFF);
	DBG_LOG_INFO("try to find the proxy with the addr : %s", kp_addrbuf);

	tpret = l_queue_find( (struct l_queue*)pt_queue, queue_match_compare_proxy_address, kp_addrbuf);
	if(tpret == NULL)
	{
		return NULL;
	}
	return *tpret;
}

/*to find the dev proxy which match the given address string
*/
struct l_dbus_proxy* bt_common_to_find_dev_proxy_by_addrstr(struct l_queue*pt_queue, uint8_t *pt_addrstr)
{
	struct l_dbus_proxy **tpret = NULL;
	
	DBG_LOG_DEBUG("Try to find: %s", pt_addrstr);

	tpret = l_queue_find( (struct l_queue*)pt_queue, queue_match_compare_proxy_address, pt_addrstr);
	if(tpret == NULL)
	{
		return NULL;
	}
	return *tpret;
}


// >>> @20240530 Added by Sean (to put the connected address to the last)
/**
 * @brief: To put the currentIdx-th element in the list to the last (i.e.,
 * list_sz-th) position, and the rest elements move up.
 */
void
refresh_white_list(
  struct bt_addr *list,
  uint32_t list_sz,
  uint32_t currentIdx)
{
  struct bt_addr tmp;
  uint32_t cnt;

  uint8_t *pt_nap = NULL, *pt_lap = NULL;
  uint16_t to_check_endian = 0;

  static uint8_t refreshDelay = 0;

  refreshDelay++;

  if((list_sz - 1) == currentIdx)
  {
    DBG_LOG_DEBUG("Do nothing, list_sz = %d, currentIdx = %d", list_sz,
                  currentIdx);
    // Do nothing because the current element has been in the last position
  }
  else
  {
    DBG_LOG_DEBUG("Refresh, list_sz = %d, currentIdx = %d", list_sz,
                  currentIdx);

    // if(refreshDelay % 4 == 0)
    {
      memcpy(&tmp, &(list[currentIdx]), sizeof(tmp));
      for(cnt = currentIdx; cnt < (list_sz - 1); cnt++)
      {
        memcpy(&(list[cnt]), &(list[cnt + 1]), sizeof(list[cnt]));
      }
      memcpy(&(list[list_sz - 1]), &tmp, sizeof(list[currentIdx]));
    }

    // Just for debug
    DBG_LOG_DEBUG("Refreshed list:");
    for(cnt = 0; cnt < list_sz; cnt++)
    {
      pt_nap = (uint8_t *)(&list[cnt % MAX_BT_WHITE_LIST].nap);
      pt_lap = (uint8_t *)(&list[cnt % MAX_BT_WHITE_LIST].lap);

      to_check_endian = 0xAABB;
      if(*((uint8_t *)(&to_check_endian)) == 0xAA)
      {
        DBG_LOG_DEBUG("%d: %02X:%02X:%02X:%02X:%02X:%02X", cnt, pt_nap[0],
                      pt_nap[1], list[cnt % MAX_BT_WHITE_LIST].uap,
                      pt_lap[0], pt_lap[1], pt_lap[2]);
      }
      else
      {
        DBG_LOG_DEBUG("%d: %02X:%02X:%02X:%02X:%02X:%02X", cnt, pt_nap[1],
                      pt_nap[0], list[cnt % MAX_BT_WHITE_LIST].uap,
                      pt_lap[2], pt_lap[1], pt_lap[0]);
      }
    }
  }
}
// <<<


/*to find the dev-proxy on the white list
pt_dev_addrstr@ the buffer where the dev-proxy address is returned . NOTE!!make sure the buffer has enough space to keep the addr-str
*/
struct l_dbus_proxy*bt_common_to_find_dev_proxy_on_white_list(struct l_queue*pt_queue, uint8_t *pt_addrstr_buf, const uint8_t kp_bufsz )
{
	struct l_dbus_proxy *tpret = NULL;
	struct bt_addr *pt_white_list = NULL;
	uint8_t kp_addrstr_buf[MAX_BT_ADDRSTR] = {0};
	uint8_t *pt_nap = NULL, *pt_lap = NULL;
	uint16_t to_check_endian = 0;
	
	if(NULL == pt_queue)
	{
		DBG_LOG_ERR("unexpected NULL");
		goto EXIT;
	}
	pt_white_list = (struct bt_addr*)l_malloc(MAX_BT_WHITE_LIST*sizeof(struct bt_addr));
	if(NULL == pt_white_list)
	{
		DBG_LOG_ERR("malloc fail");
		goto EXIT;
	}
	memset( pt_white_list, 0, MAX_BT_WHITE_LIST*sizeof(struct bt_addr));

	sys_get_bt_white_list( pt_white_list, MAX_BT_WHITE_LIST);

	for(uint32_t i = 0; i < MAX_BT_WHITE_LIST; i++)
	{
		memset( kp_addrstr_buf, 0, MAX_BT_ADDRSTR);
		pt_nap = (uint8_t*)(&pt_white_list[i % MAX_BT_WHITE_LIST].nap);
		pt_lap = (uint8_t*)(&pt_white_list[i % MAX_BT_WHITE_LIST].lap);

		to_check_endian = 0xAABB;
		if( *((uint8_t*)(&to_check_endian)) == 0xAA )
		{
			snprintf( kp_addrstr_buf, MAX_BT_ADDRSTR,"%02X:%02X:%02X:%02X:%02X:%02X",pt_nap[0], pt_nap[1], pt_white_list[i % MAX_BT_WHITE_LIST].uap, pt_lap[0], pt_lap[1], pt_lap[2]);
		}
		else
		{
			snprintf( kp_addrstr_buf, MAX_BT_ADDRSTR,"%02X:%02X:%02X:%02X:%02X:%02X",pt_nap[1], pt_nap[0], pt_white_list[i % MAX_BT_WHITE_LIST].uap, pt_lap[2], pt_lap[1], pt_lap[0]);					
		}
		DBG_LOG_INFO("try to find BT-dev with addr:%s", kp_addrstr_buf);

		tpret = bt_common_to_find_dev_proxy_by_addrstr( pt_queue, kp_addrstr_buf);
		if(NULL != tpret)
		{// we find the dev-proxy
			DBG_LOG_WARN("dev-proxy with addr :%s is found, Going to connect to it. Race-condiction here, to fix it", kp_addrstr_buf);
			DBG_LOG_DEBUG("i = %d", i);
			snprintf( pt_addrstr_buf, kp_bufsz,"%s", kp_addrstr_buf);
			// >>> @20240530 Added by Sean (to put the connected address to the last)
			refresh_white_list(pt_white_list, MAX_BT_WHITE_LIST, i);
			// <<<
			break;
		}
	}

	// >>> @20240530 Added by Sean (to put the connected address to the last)
	pthread_mutex_lock(&(sys_get_mgt()->mtx_for_white_list));
	sys_set_bt_white_list(pt_white_list, MAX_BT_WHITE_LIST);
	pthread_mutex_unlock(&(sys_get_mgt()->mtx_for_white_list));
	// <<<

	EXIT:
		if(pt_white_list)
		{
			l_free( pt_white_list);
			pt_white_list = NULL;
		}
		if(tpret == NULL)
		{
			return NULL;
		}
		return tpret;
}



/*
typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
to compare the name of proxy with the given string
ret@true is returned when they match
*/
static bool queue_match_compare_proxy_name(const void*a, const void*b)
{
	bool tpret = false;
	const char*pt_devnm = NULL;
	
	struct l_dbus_proxy*pt_proxy = *((struct l_dbus_proxy**)a);
	if((pt_proxy == NULL)||(b == NULL))
	{
		tpret = false;
		return tpret;
	}
	
	if(false == l_dbus_proxy_get_property( pt_proxy, "Alias", "s", &pt_devnm))
	{
		tpret = false;
		return tpret;
	}
	//DBG_LOG_INFO("target %s, devnm %s", b, pt_devnm);
	if(0 == strcmp( pt_devnm,(const char*)b))
	{ //the match
		tpret = true;
	}
	else
	{
		tpret = false;
	}
	return tpret;
}

/*function: to find the device-proxy by the Name string given
pt_queue: the device-proxy queue
kp_nmstr : the name of device going to find
ret@the pointer of the proxy is returned when found, otherewise NULL is returned
*/
struct l_dbus_proxy*bt_common_to_find_dev_proxy_by_name(struct l_queue*pt_queue, const char*pt_nmstr)
{
	struct l_dbus_proxy**ptret = NULL;
	if((pt_queue == NULL)||(pt_nmstr == NULL))
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		return NULL;
	}

	if(0 == l_queue_length( pt_queue))
	{
		DBG_LOG_ERR("empty queue, nothing to find");
		return NULL;
	}
	
	//DBG_LOG_INFO("to find the proxy with name :%s", pt_nmstr);
	ptret = l_queue_find( pt_queue, queue_match_compare_proxy_name, pt_nmstr);
	if(ptret == NULL)
	{
		return NULL;
	}
	return *ptret;
}



/*
typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data);
function to set up dbus message
*/
static void msg_setup_for_charac_read_value(struct l_dbus_message *message,void *user_data)
{
	uint16_t kpoffset = 0;
	struct l_dbus_message_builder*ptbuilder = NULL;
	
	ptbuilder = l_dbus_message_builder_new( message);
	l_dbus_message_builder_enter_array( ptbuilder, "{sv}");
	l_dbus_message_builder_enter_dict( ptbuilder, "sv");

	l_dbus_message_builder_append_basic( ptbuilder, 's', "offset");

	l_dbus_message_builder_enter_variant( ptbuilder, "q");
	l_dbus_message_builder_append_basic( ptbuilder, 'q', (const void *)&kpoffset);
	l_dbus_message_builder_leave_variant( ptbuilder);

	l_dbus_message_builder_leave_dict( ptbuilder);
	
	l_dbus_message_builder_leave_array( ptbuilder);

	l_dbus_message_builder_finalize( ptbuilder);
	l_dbus_message_builder_destroy( ptbuilder);
}

/*
typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data);
*/
void client_charac_read_value_reply(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	const char*pt_path = NULL;
	const char*pt_name = NULL;
	const char*pt_text = NULL;

	struct l_dbus_message_iter kp_msg_iter = {0};
	uint8_t tp_iter = 0;
	
	
	pt_path = l_dbus_proxy_get_path( proxy);
	if(l_dbus_message_is_error(result))
	{
		l_dbus_message_get_error( result, &pt_name, &pt_text);
		DBG_LOG_ERR("%s fail to read value , err info: %s , %s", pt_path, pt_name, pt_text);
		return;
	}
	// continue to get the value read
	if(!l_dbus_message_get_arguments( result, "ay", &kp_msg_iter))
	{
		DBG_LOG_ERR("fail to get msg argument!");
		return;
	}
	while(l_dbus_message_iter_next_entry( &kp_msg_iter, &tp_iter))
	{
		DBG_LOG_INFO("%s bytes read : 0x%X", pt_path, tp_iter);		
	}
}

/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
void udata_destroy_for_charac_read_value(void*user_data)
{
	DBG_LOG_INFO("you probably need to release user_data here if it is allocated from stack");
}


/*to read the ble attr-value
pt_proxy : the characteristic proxy going to read value
kpoffset : the value offset
ret@true is returned when things go well
*/
bool bt_common_ble_charac_readvalue(struct l_dbus_proxy*pt_proxy,const uint16_t kpoffset)
{
	bool tpret = true;
	const char*pt_interface = NULL;
	uint32_t tp_callid = 0;


	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = false;
		goto ERR;
	}

	pt_interface = l_dbus_proxy_get_interface(pt_proxy);
	if(pt_interface == NULL)
	{
		DBG_LOG_ERR("fail to get the proxy interface");
		tpret = false;
		goto ERR;
	}

	// to check if the given proxy is a characteristic-proxy
	if(strcmp( BT_IF_NM_GATTCHARACTERISTIC, pt_interface) != 0)
	{
		DBG_LOG_ERR("invalid service interface:%s", pt_interface);
		tpret = false;
		goto ERR;
	}

 	tp_callid = l_dbus_proxy_method_call( pt_proxy, "ReadValue", msg_setup_for_charac_read_value, client_charac_read_value_reply, NULL, udata_destroy_for_charac_read_value);
	if(tp_callid == 0)
	{
		DBG_LOG_ERR("fail to call proxy method");
		tpret = false;
		goto ERR;
	}
	return tpret;
	ERR:
		return tpret;
}


/*
typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);
call back for dev-removeing

*/
static void msg_setup_for_dev_remove(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder*pt_builder = NULL;
	pt_builder = l_dbus_message_builder_new( message);
	if(pt_builder == NULL)
	{
		DBG_LOG_ERR("fail to create message builder");
		return;
	}
	l_dbus_message_builder_append_basic( pt_builder, 'o', (const void *)user_data);

	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);
	
	return;
}

/*
typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);

*/
static void msg_reply_for_dev_remove(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	const char*pt_name = NULL, *pt_text = NULL;
	if(l_dbus_message_is_error(result))
	{
		l_dbus_message_get_error( result, &pt_name, &pt_text);
		DBG_LOG_ERR("fail to remove device, err_info %s, %s", pt_name, pt_text);
	}
	else
	{
		DBG_LOG_INFO("the device is removed(unbonded) successfully");
	}
	return;
}

/*typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
static void msg_destroy_for_dev_remove(void *user_data)
{
	if(user_data)
	{
		l_free(user_data);
	}
	return;
}


/*to remove the pairing-info(unbond)
ret@ST_OK is returned when things go well
*/
app_state_t bt_common_rm_devices( struct l_dbus_proxy*pt_adapter, struct l_dbus_proxy*pt_proxy)
{
	app_state_t tpret = ST_OK;
	const char*pt_interface = NULL;

	const char*pt_path = NULL;
	char*pt_strbuf = NULL;
	size_t kp_len = 0;
	
	if((pt_proxy == NULL)||(pt_adapter == NULL))
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = ST_ERR;
		return tpret;
	}
	// to check if the Adapter1 is given
	pt_interface = l_dbus_proxy_get_interface( pt_adapter);
	if(pt_interface == NULL)
	{
		DBG_LOG_ERR("invalid interface");
		tpret = ST_ERR;
		return tpret;
	}
	if(strcmp( pt_interface, BT_IF_NM_ADAPTER))
	{
		DBG_LOG_ERR("a proxy with interface %s is expected here", BT_IF_NM_ADAPTER);
		tpret = ST_ERR;
		return tpret;
	}

	if(true)//(((uint64_t)pt_proxy) <= dbusRefAddrUpperBound) && (((uint64_t)pt_proxy) >= dbusRefAddrLowerBound))
	{
		// to check if going to remove a dev-proxy
		pt_interface = l_dbus_proxy_get_interface( pt_proxy);
		if(pt_interface == NULL)
		{
			DBG_LOG_ERR("invalid proxy interface");
			tpret = ST_ERR;
			return tpret;
		}
		if(strcmp( pt_interface, BT_IF_NM_DEVICE))
		{
			DBG_LOG_ERR("a proxy with interface %s is expected", BT_IF_NM_DEVICE); 
			tpret = ST_ERR;
			return tpret;
		}
		pt_path = l_dbus_proxy_get_path( pt_proxy);
		if(pt_path == NULL)
		{
			DBG_LOG_ERR("invali proxy path");
			tpret = ST_ERR;
			return tpret;
		}
		kp_len = strlen(pt_path) + 1; // for the terminal character
		pt_strbuf = (char*)l_malloc( kp_len);
		if(pt_strbuf == NULL)
		{
			DBG_LOG_ERR("fail to allocate memory");
			tpret = ST_ERR;
			return tpret;
		}
		memset( pt_strbuf, 0, kp_len);
		memcpy( pt_strbuf, pt_path, kp_len);
		if(0 == l_dbus_proxy_method_call( pt_adapter, "RemoveDevice", msg_setup_for_dev_remove, msg_reply_for_dev_remove, (void *)pt_strbuf, msg_destroy_for_dev_remove))
		{
			DBG_LOG_ERR("fail to call method RemoveDevice");
			tpret = ST_ERR;
			return tpret;
		}
		// TODO: delay 300ms to give bluez some time to sync status(pairing)
		usleep(300*1000);
		return tpret;
	}else{
		DBG_LOG_ERR("Unexpected proxy to bd removed");
		tpret = ST_ERR;
		return tpret;
	}
}


/*to set the gateway ID; NOTE! Now we take the local BT address as the ID
ret@ST_OK is returned when things go well
*/
app_state_t bt_common_set_gateway_id( app_management*pt_app_mgt, struct l_dbus_proxy*pt_adapter)
{	
	app_state_t tpret = ST_OK;
	const char*pt_interface = NULL;
	const char*pt_addr = NULL;
	char kp_strbuf[MAX_ID_STR_LENGTH] = {0};
	uint8_t i = 0, cnt = 0;
	
	if((NULL == pt_app_mgt)||(NULL == pt_adapter))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}
	pt_interface = l_dbus_proxy_get_interface( pt_adapter);
	if(NULL == pt_interface)
	{
		DBG_LOG_ERR("fail to get proxy interface");
		tpret = ST_ERR;
		return tpret;
	}
	if(strcmp( pt_interface, BT_IF_NM_ADAPTER))
	{
		DBG_LOG_ERR("a proxy with interface %s is expected", BT_IF_NM_ADAPTER);
		tpret = ST_ERR;
		return tpret;
	}
	if(false == l_dbus_proxy_get_property( pt_adapter, "Address", "s", &pt_addr))
	{
		DBG_LOG_ERR("fail to get property Address");
		tpret = ST_ERR;
		return tpret;
	}
	
	i = 0;
	cnt = 0;
	while(pt_addr[i])
	{
		if(pt_addr[i] != ':')
		{
			kp_strbuf[cnt % MAX_ID_STR_LENGTH] = pt_addr[i];
			cnt++;
		}
		i++;
	}

	kp_strbuf[MAX_ID_STR_LENGTH - 1] = 0; // make sure 0 is the string terminal
	sys_set_gw_id_string( kp_strbuf);
	
	return tpret;
}




/*to switch the BT connection
*/

app_state_t bt_common_swtich_bt_connection( app_management *pt_app_mgt, struct pthread_cond_var *pt_cond_var)
{
	app_state_t tpret = ST_OK;
	struct l_dbus_proxy *pt_proxy = NULL;
	#warning "TO-Fix=============================this function is not thread safe, fix it later"
	pt_proxy = sys_get_connected_proxy(pt_app_mgt);
	if(NULL == pt_proxy)
	{ // nothing to do , when no device is connected
		DBG_LOG_WARN("no dev is connected, nothing to do");
		return tpret;
	}

	if(BT_ROLE_CLIENT != sys_get_bluetooth_role( pt_app_mgt))
	{// invalid operation
		DBG_LOG_WARN("only for BT_CLIENT");
		return tpret;
	}
	
	// to disconnect from the peer device first
	pt_cond_var->is_op_done = false;
	struct timespec tmpTime; // Just for debug
	clock_gettime(CLOCK_MONOTONIC, &pt_cond_var->tv);
	DBG_LOG_DEBUG("Current time %d", pt_cond_var->tv.tv_sec);
	pt_cond_var->tv.tv_sec += TIMEOUT_BT_DISCONNECTION_S;
	DBG_LOG_DEBUG("Expired time %d", pt_cond_var->tv.tv_sec);
	if(ST_OK == bt_con_disconnect(pt_proxy, pt_cond_var))
	{
		pthread_mutex_lock( &pt_cond_var->mtx);
		while(false == pt_cond_var->is_op_done)
		{
			if(pthread_cond_timedwait(&pt_cond_var->cond_var, 
										&pt_cond_var->mtx,
										&pt_cond_var->tv) == ETIMEDOUT)
			{
				// Just for debug
				DBG_LOG_DEBUG("Expired time %d", pt_cond_var->tv.tv_sec);
				clock_gettime(CLOCK_MONOTONIC, &tmpTime);
				DBG_LOG_DEBUG("Current %d", tmpTime.tv_sec);

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
			else{
				clock_gettime(CLOCK_MONOTONIC, &tmpTime);
				DBG_LOG_DEBUG("Current %d", tmpTime.tv_sec);
				DBG_LOG_DEBUG("Current %d", tmpTime.tv_sec);
			}
		}
		pthread_mutex_unlock( &pt_cond_var->mtx);
	}

	// pt_cond_var->is_op_done = false;
	// bt_con_disconnect( pt_proxy, pt_cond_var);
	// pthread_mutex_lock( &pt_cond_var->mtx);
	// while(1)
	// {
	// 	if(false == pt_cond_var->is_op_done)
	// 	{
	// 		pthread_cond_wait( &pt_cond_var->cond_var, &pt_cond_var->mtx);
	// 	}
	// 	else
	// 	{
	// 		break;
	// 	}
	// }
	// pthread_mutex_unlock( &pt_cond_var->mtx);
	return tpret;
}


















