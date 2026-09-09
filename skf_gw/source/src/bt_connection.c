#include "bt_connection.h"
#include "util_dbg.h"
#include "sys_def.h"


DBG_LOCAL_LOG_DEBUG


// typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
// void *user_data);
// A setup callback called by l_dbus_proxy_method_call ("pair")
static void msg_setup_for_pair(
  struct l_dbus_message *message,
  void *user_data);
// typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy
// *proxy, struct l_dbus_message *result, void *user_data);
// A reply callback called by l_dbus_proxy_method_call ("pair")
static void msg_reply_for_pair(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data);
// typedef void (*l_dbus_destroy_func_t) (void *user_data);
// A destroy callback called by l_dbus_proxy_method_call ("pair")
static void destroy_for_pair(void *user_data);

/**
 * @brief A setup callback called by l_dbus_proxy_method_call ("pair")
 */
// typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
// void *user_data);
static void
msg_setup_for_pair(
  struct l_dbus_message *message,
  void *user_data)
{
  // TODO
}
/**
 * @brief A reply callback called by l_dbus_proxy_method_call ("pair")
 */
// typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy
// *proxy, struct l_dbus_message *result, void *user_data);
static void
msg_reply_for_pair(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data)
{
  const char *pt_name = NULL, *pt_text = NULL;

  if(l_dbus_message_is_error(result))
  {
    if(l_dbus_message_get_error(result, &pt_name, &pt_text))
    {
      DBG_LOG_ERR("Failed to pair with error (%s), detailed info (%s)",
                  pt_name, pt_text);
    }
    else
    {
      DBG_LOG_ERR("Failed to pair but no error information");
    }
  }
  else
  {
    DBG_LOG_INFO("Pair goes well");
  }
  return;
}
/**
 * @brief A destroy callback called by l_dbus_proxy_method_call ("pair")
 */
// typedef void (*l_dbus_destroy_func_t) (void *user_data);
static void
destroy_for_pair(void *user_data)
{
  // TODO
}


/*to check if the device paired or not
ret@true when the device is paired
*/
bool bt_con_is_dev_paired(struct l_dbus_proxy*pt_proxy)
{
	bool tpret = false;
	const char*pt_interface = NULL;
	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		return false;
	}
	
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if(pt_interface == NULL)
	{
		DBG_LOG_ERR("invalid pointer");
		return false;
	}

	if(strcmp( pt_interface, BT_IF_NM_DEVICE))
	{// not a dev-proxy
		DBG_LOG_ERR("a proxy with interface %s is expected", pt_interface);
		return false;
	}

	if(false == l_dbus_proxy_get_property( pt_proxy, "Paired", "b", &tpret))
	{
		DBG_LOG_ERR("fail to get property Paired");
		return false;
	}
	return tpret;
}

/*to check if the device is connected
ret@true when the device is connected
*/
bool bt_con_is_dev_connected(struct l_dbus_proxy*pt_proxy)
{
	bool tpret = false;
	const char*pt_interface = NULL;
	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if(NULL == pt_interface)
	{
		DBG_LOG_ERR("invalid pointer");
		return false;
	}
	if(strcmp(pt_interface, BT_IF_NM_DEVICE))
	{// not a dev-proxy
		DBG_LOG_ERR("a proxy with interface %s is expected", pt_interface);
		return false;
	}
	if(false == l_dbus_proxy_get_property( pt_proxy, "Connected", "b", &tpret))
	{
		DBG_LOG_INFO("fail to get property connected");
		return false;
	}
	return tpret;
}


/*to pair a BT device
ret#ST_OK is returned when things go well
*/
app_state_t bt_con_pair(struct l_dbus_proxy*pt_proxy)
{
	app_state_t tpret = ST_OK;
	const char*pt_interface = NULL;
	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = ST_ERR;
		return tpret;
	}
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if(pt_interface == NULL)
	{
		DBG_LOG_ERR("fail to get proxy interface");
		tpret = ST_ERR;
		return tpret;
	}
	if(strcmp(pt_interface, BT_IF_NM_DEVICE))
	{
		DBG_LOG_ERR("a proxy with interface %s is expected", BT_IF_NM_DEVICE);
		tpret = ST_ERR;
		return tpret;
	}
	//if(0 == l_dbus_proxy_method_call( pt_proxy, "Pair", msg_setup_for_pair, msg_reply_for_pair, NULL, destroy_for_pair))	
	if(0 == l_dbus_proxy_method_call( pt_proxy, "Pair", NULL, msg_reply_for_pair, NULL, destroy_for_pair))
	{
		DBG_LOG_ERR("fail to call method Pair");
		tpret = ST_ERR;
		return tpret;
	}
	return tpret;
}



#if(1)
	/*
	typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data);
	*/
	static void msg_setup_for_bt_connect(struct l_dbus_message *message, void *user_data)
	{	
		DBG_LOG_INFO("no paramter for method-Connect");
		l_dbus_message_set_arguments( message,"");
	}
	/*
	typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data);
	*/
	static void msg_reply_for_bt_connect(struct l_dbus_proxy*proxy, struct l_dbus_message*result, void *user_data)
	{
		// To set the condition when BT disconnection operation has been done, no
		// matter failure or success
		if(user_data)
		{
			struct pthread_cond_var *pt_cond_var = NULL;
			pt_cond_var = (struct pthread_cond_var *)user_data;
			pthread_mutex_lock(&pt_cond_var->mtx);
			pt_cond_var->is_op_done = true;
			pthread_cond_signal(&pt_cond_var->cond_var);
			pthread_mutex_unlock(&pt_cond_var->mtx);
		}

		if(l_dbus_message_is_error(result))
		{
			const char*ptname = NULL, *ptext = NULL;
			l_dbus_message_get_error( result, &ptname, &ptext);
			DBG_LOG_ERR("the error info: %s , %s", ptname, ptext);
			// >>> @20240613 Added by Sean (to fix the "gabage-in-the-list bug")
			bt_common_rm_devices( sys_get_proxy_adapter1(sys_get_mgt()), proxy);
			sys_set_con_in_progress_flg(false); // reset the flag even fail to connect
			// DBG_LOG_DEBUG("Power off");
			// bt_common_set_powered(proxy, false);
			// sleep(1);
			// bt_common_set_powered(proxy, true);
			// sleep(2);
			// DBG_LOG_DEBUG("Power on");
			// <<<
			goto ERR;
		}
		DBG_LOG_INFO("msg to create a BT connection is sent successfully, proxy-path %s", l_dbus_proxy_get_path( proxy));
		
		#if(1) //ref@connection_reply
			app_management *pt_app_mgt = sys_get_mgt();			
			sys_set_connected_proxy( pt_app_mgt, proxy);
			pthread_mutex_lock( &pt_app_mgt->mtx_con_event);
			//sys_set_connected_proxy( pt_app_mgt, proxy);
			pt_app_mgt->bt_con_event = BT_EVENT_CONNECTION;
			if(pthread_cond_broadcast( &pt_app_mgt->cond_val_con_event))
			{
				DBG_LOG_ERR("fail to siginal the cond-val");
			}
			else
			{				
				DBG_LOG_INFO("the BT connection event is signaled, cond-val@0x%x", &pt_app_mgt->cond_val_con_event);
			}
			pthread_mutex_unlock(&pt_app_mgt->mtx_con_event);

			// to report the event
			pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
			l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)BT_EVENT_CONNECTION);
			pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
			pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);	
		#endif
		
		ERR:
			
			sys_set_con_in_progress_flg(false); // reset the flag even fail to connect
			
			return;
	}
#endif
/*to connect
*/
app_state_t
bt_con_connect(
  struct l_dbus_proxy *pt_proxy,
  struct pthread_cond_var *pt_cond_var)
{
	app_state_t tpret = ST_OK;
	const char*pt_interface = NULL;
	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = ST_ERR;
		return tpret;
	}

	// >>> @20240604 Added by Sean (to filter the unexpected proxy address)
	if((((uint64_t)pt_proxy) > dbusRefAddrUpperBound) || (((uint64_t)pt_proxy) < dbusRefAddrLowerBound))
	{
		DBG_LOG_ERR("unexpected pointer, 0x%llx", pt_proxy);
		// tpret = ST_ERR;
		// // >>> @20240613 Added by Sean (to fix the "gabage-in-the-list bug")
		// bt_common_rm_devices( sys_get_proxy_adapter1(sys_get_mgt()), pt_proxy);
		// // <<<
		// return tpret;
	}
	// <<< 

	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if(pt_interface == NULL)
	{
		DBG_LOG_ERR("fail to get proxy interface");
		tpret = ST_ERR;
		return tpret;
	}
	if(strcmp( pt_interface, BT_IF_NM_DEVICE))
	{
		DBG_LOG_ERR("a proxy with interface %s is expected", BT_IF_NM_DEVICE);
		tpret = ST_ERR;
		return tpret;
	}

	//if(0 == l_dbus_proxy_method_call( pt_proxy, "Connect", msg_setup_for_bt_connect, msg_reply_for_bt_connect, NULL, NULL))
	if(0 == l_dbus_proxy_method_call( pt_proxy, "Connect", NULL, msg_reply_for_bt_connect, pt_cond_var, NULL))
	{
		DBG_LOG_ERR("fail to call method Connect");
		tpret = ST_ERR;
		return tpret;
	}
	return tpret;
}


static struct l_dbus_proxy *pt_proxy_connect_timeout = NULL;

//MK283U__________________________________________________________
#if(1) // try to use a ell-timer to reset the in-process flag , when no BT-connection reply
		struct l_timeout *g_pt_timer_for_connection_reply = NULL;
		//l_timeout_create(unsigned int seconds, l_timeout_notify_cb_t callback, void * user_data, l_timeout_destroy_cb_t destroy)		
		//typedef void (*l_timeout_notify_cb_t) (struct l_timeout *timeout,void *user_data);		
		static void timeout_notify_cb_for_bt_connection_reply(struct l_timeout *timeout,void *user_data)
		{
			DBG_LOG_ERR("waiting for BT-connection reply timeout, to check and reset the in-process-flg");
			if(sys_get_con_in_progress_flg())
			{
				sys_set_con_in_progress_flg(false);
			}

			//l_timeout_remove( timeout);// to release the timer
			if(g_pt_timer_for_connection_reply)
			{
				l_timeout_remove( g_pt_timer_for_connection_reply);
				g_pt_timer_for_connection_reply = NULL;
			}

			// TODO: a workaround
			// exit(1);
			if(pt_proxy_connect_timeout != NULL)
			{
				DBG_LOG_DEBUG("Remove the device due to a connection timeout");
				bt_common_rm_devices( sys_get_proxy_adapter1(sys_get_mgt()), pt_proxy_connect_timeout);
			}
		}

		
		//typedef void (*l_timeout_destroy_cb_t) (void *user_data);		
		static void timeout_destroy_cb_for_bt_connection_reply(void *user_data)
		{
			return;
		}
#if(1)
		//MK277U___________________________________________
		/**/
		static void msg_reply_for_disconnect_from_sensor (struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
		{
			if(l_dbus_message_is_error(result))
			{
				const char*ptname = NULL, *ptext = NULL;
				l_dbus_message_get_error( result, &ptname, &ptext);
				DBG_LOG_ERR("Disconnect err-info %s %s", ptname, ptext);
				goto EXIT;
			}
			DBG_LOG_INFO("method-Disconnect is called successfully");
			EXIT:
				return;
		}
		
		/*a connection might be created after switching to BT-server
		ret@true is returned when method-Disconnec is called 
		*/
		static bool disconnect_from_sensor_in_server_mode(app_management*pt_app_mgt, struct l_dbus_proxy*pt_proxy)
		{
			bool tpret = true;
			char* pt_addrstr = NULL;
			uint8_t kp_sensor_addrstr[0x40] = {0};
			uint8_t kp_proxy_addrstr[0x40] = {0};
			if((NULL == pt_proxy)||(NULL == pt_app_mgt))
			{
				DBG_LOG_ERR("unexpected NULL");
				tpret = false;
				return tpret;
			}

			if(BT_ROLE_SERVER != sys_get_bluetooth_role(pt_app_mgt))
			{
				DBG_LOG_WARN("nothing to do ,when the role is not BT-server");
				tpret = false;
				return tpret;
			}

			// to get the property-Address
			if(!l_dbus_proxy_get_property(pt_proxy, "Address", "s", &pt_addrstr))
			{
				DBG_LOG_ERR("fail to get property-Address");
				tpret = false;
				return tpret;
			}

			snprintf( kp_sensor_addrstr, sizeof(kp_sensor_addrstr), "%s", sys_get_active_bt_device());
			snprintf( kp_proxy_addrstr, sizeof(kp_proxy_addrstr), "%s", pt_addrstr);
			DBG_LOG_WARN("to compare %s vs %s", kp_sensor_addrstr, kp_proxy_addrstr);
			if(strcmp( kp_sensor_addrstr, kp_proxy_addrstr))
			{
				DBG_LOG_ERR("the Address-string does not match");
				tpret = false;	
				return tpret;
			}
			
			if(0 == l_dbus_proxy_method_call( pt_proxy, "Disconnect", NULL, msg_reply_for_disconnect_from_sensor, NULL, NULL))
			{
				DBG_LOG_ERR("fail to call method-Disconnect");
				tpret = false;
				return tpret;
			}
			DBG_LOG_INFO("method-Disconnect is called");
			
			return tpret;
		}


		//#error "FYI ~ things will go wrong when we try to swtich to BT-server in the process of connecting to a sensor. "
		/* A connection to sensor might be created when the application is in BT-server mode. continue to fix this problem
		 if you want to develop under this branch. start from function disconnect_from_sensor_in_server_mode*/
		

		
		//MK277D___________________________________________



		/*
		typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data);
		*/
		static void msg_setup_for_bt_connect_v2(struct l_dbus_message *message, void *user_data)
		{	
			DBG_LOG_INFO("no paramter for method-Connect");
			l_dbus_message_set_arguments( message,"");
		}
		/*
		typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data);
		*/
		static void msg_reply_for_bt_connect_v2(struct l_dbus_proxy*proxy, struct l_dbus_message*result, void *user_data)
		{
			if(l_dbus_message_is_error(result))
			{
				const char*ptname = NULL, *ptext = NULL;
				l_dbus_message_get_error( result, &ptname, &ptext);
				DBG_LOG_ERR("the error info: %s , %s", ptname, ptext);
				// >>> @20240613 Added by Sean (to fix the "gabage-in-the-list bug")
				bt_common_rm_devices( sys_get_proxy_adapter1(sys_get_mgt()), proxy);
				goto ERR;
			}

			DBG_LOG_INFO("msg to create a BT connection is sent successfully, proxy-path %s", l_dbus_proxy_get_path( proxy));
			
		#if(1) //ref@connection_reply
				app_management *pt_app_mgt = sys_get_mgt();

				#if(1) 
				/*
				*/ 
				if(disconnect_from_sensor_in_server_mode( pt_app_mgt, proxy))
				{
					DBG_LOG_WARN("method-Disconnect to disconnect from sensor in server-mode");
					goto ERR;
				}
				#endif

				DBG_LOG_INFO("to set the connected-proxy@0x%p", proxy);
				//! DO NOT try to take another mutex when one is token.this will save you much trouble				
				sys_set_connected_proxy( pt_app_mgt, proxy);

				pthread_mutex_lock( &pt_app_mgt->mtx_con_event);
				//sys_set_connected_proxy( pt_app_mgt, proxy);
				pt_app_mgt->bt_con_event = BT_EVENT_CONNECTION;
				if(pthread_cond_broadcast( &pt_app_mgt->cond_val_con_event))
				{
					DBG_LOG_ERR("fail to siginal the cond-val");
				}
				else
				{				
					DBG_LOG_INFO("the BT connection event is signaled, cond-val@0x%x", &pt_app_mgt->cond_val_con_event);
				}
				pthread_mutex_unlock(&pt_app_mgt->mtx_con_event);
	
				// to report the event
				pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
				l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)BT_EVENT_CONNECTION);
				pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
				pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);	
		#endif
			
			ERR:
			#if(1)// to remove the timer which is waiting for BT-connection reply
				if(g_pt_timer_for_connection_reply)
				{
					DBG_LOG_INFO("to remove the timer which is waiting for connection-reply");
					l_timeout_remove( g_pt_timer_for_connection_reply);
					g_pt_timer_for_connection_reply = NULL;
				}
				else
				{
					DBG_LOG_WARN("you are not suppose to see this output!?");
				}
			#endif
				sys_set_con_in_progress_flg(false); // reset the flag even fail to connect
				
				return;
		}
#endif

struct hashCalculationState
{
  uint32_t bleDevicesListHashValue; // The hash value
  uint32_t bleDevicesListHashValueTmp; // The temporary value
};

static void bleDevicesListHashGenerator(
  void *data,
  void *user_data);
static bool scanningAnomalyDetector(struct l_queue *pt_queue);

/**
 * @brief To generate a hash value of the BLE device list (A callback
 *        called by @p scanningAnomalyDetector )
 * @param data: The element in the list.
 * @param user_data: A place to store hash value and temporary variables
 *                   (@p struct hashCalculationState).
 */
static void
bleDevicesListHashGenerator(
  void *data,
  void *user_data)
{
  struct l_dbus_proxy *pt_proxy = *((struct l_dbus_proxy **)data);
  struct hashCalculationState *state = (struct ashCalculationState *)user_data;
  const char *pt_proxy_addr = NULL;
  uint8_t tplen;

  if((data == NULL) || (user_data == NULL))
  {
    return;
  }

  if(!l_dbus_proxy_get_property(pt_proxy, "Address", "s", &pt_proxy_addr))
  {
    DBG_LOG_ERR("Failed to get proxy property (Address)!");
    return;
  }

  tplen = strlen(pt_proxy_addr);

  for(uint16_t i = 0; i < tplen; ++pt_proxy_addr, ++i)
  {
    state->bleDevicesListHashValue =
      (state->bleDevicesListHashValue << 4) + ((*pt_proxy_addr) & 0xff);
    if((state->bleDevicesListHashValueTmp =
          state->bleDevicesListHashValue &
          0xF0000000L) != 0)
    {
      state->bleDevicesListHashValue ^=
        (state->bleDevicesListHashValueTmp >> 24);
    }
    state->bleDevicesListHashValue &= ~state->bleDevicesListHashValueTmp;
  }
}
/**
 * @brief To detect the whether "BLE scanning" is working properly
 */
static bool
scanningAnomalyDetector(struct l_queue *pt_queue)
{
  struct hashCalculationState state;
  static uint32_t hashPrevious = 0;
  static uint32_t hashCurrent = 0;
  static uint8_t filterCnt = 0;

  if(l_queue_length(pt_queue) == 0)
  {
    // The queue has nothing
    // An empty queue is regarded as a kind of abnormal
    hashPrevious = 0;
    hashCurrent = 0;
    filterCnt++;
    if(filterCnt >= 20)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  else
  {
    state.bleDevicesListHashValue = 0;
    state.bleDevicesListHashValueTmp = 0;

    l_queue_foreach(pt_queue,
                    bleDevicesListHashGenerator,
                    &state);

    hashPrevious = hashCurrent;
    hashCurrent = state.bleDevicesListHashValue;

    if(hashPrevious != hashCurrent)
    {
      // The list is varying ...
      // Scanning looks good
      filterCnt = 0;
      return false;
    }
    else
    {
      // The list is static ...
      // Scanning is abnormal
      filterCnt++;
      if(filterCnt >= 20)
      {
        return true;
      }
      else
      {
        return false;
      }
    }
  }
}



#if (SKF_GW_NEW == 1)
/**
 * @breif Search whether the scanned device is in the list
 */
app_state_t
bt_con_search_for_sensor_and_connect_v2(
  app_management *pt_app_mgt,
  struct pthread_cond_var *kp_cond_var)
{
  app_state_t _ret = ST_OK;
  // MAC address related ...
  struct bt_addr _whitelist_array[MAX_BT_WHITE_LIST] = { 0 };
  uint8_t _btmac_strbuf[MAX_BT_WHITE_LIST][MAX_BT_ADDRSTR] = { 0 };
  uint8_t *_pt_nap = NULL, *_pt_lap = NULL;
  uint16_t _to_check_endian = 0;
  // dbus related ...
  struct l_dbus_proxy *_pt_proxy = NULL, *_pt_proxy_to_connect = NULL;
  struct l_queue_entry *_pt_dev_q_entry = NULL;
  const char *pt_proxy_addrstr = NULL;

  if(NULL == pt_app_mgt)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    goto EXIT;
  }

  memset(_whitelist_array, 0,
         MAX_BT_WHITE_LIST * sizeof(struct bt_addr));
  sys_get_bt_white_list(_whitelist_array, MAX_BT_WHITE_LIST);

  // To get the BLE MAC address string
  for(uint32_t i = 0; i < MAX_BT_WHITE_LIST; i++)
  {
    memset(_btmac_strbuf[i], 0, MAX_BT_ADDRSTR);
    _pt_nap = (uint8_t *)(&_whitelist_array[i].nap);
    _pt_lap = (uint8_t *)(&_whitelist_array[i].lap);

    _to_check_endian = 0xAABB;
    if(*((uint8_t *)&_to_check_endian) == 0xAA)
    {
      snprintf(_btmac_strbuf[i], MAX_BT_ADDRSTR,
               "%02X:%02X:%02X:%02X:%02X:%02X", _pt_nap[0], _pt_nap[1],
               _whitelist_array[i % MAX_BT_WHITE_LIST].uap, _pt_lap[0],
               _pt_lap[1], _pt_lap[2]);
    }
    else
    {
      snprintf(_btmac_strbuf[i], MAX_BT_ADDRSTR,
               "%02X:%02X:%02X:%02X:%02X:%02X", _pt_nap[1], _pt_nap[0],
               _whitelist_array[i % MAX_BT_WHITE_LIST].uap, _pt_lap[2],
               _pt_lap[1], _pt_lap[0]);
    }
  }

  pthread_mutex_lock(&pt_app_mgt->mtx_q_devices);
  _pt_dev_q_entry = l_queue_get_entries(pt_app_mgt->pt_q_devices);
  _pt_proxy_to_connect = NULL;

  for(uint32_t i = 0; i < MAX_BT_WHITE_LIST; i++)
  {
    _pt_proxy = bt_common_to_find_dev_proxy_by_addrstr(
      pt_app_mgt->pt_q_devices, _btmac_strbuf[i]);
    if(NULL != _pt_proxy)
    {
      // The device in the list is found
      DBG_LOG_INFO(
        "Device with addr (%s) is found",
        _btmac_strbuf[i]);
      DBG_LOG_DEBUG("Device order in the list is %d", i);
      _pt_proxy_to_connect = _pt_proxy;
      // Refresh the order ...
      refresh_white_list(_whitelist_array, MAX_BT_WHITE_LIST, i);
      pthread_mutex_lock(&pt_app_mgt->mtx_for_white_list);
      sys_set_bt_white_list(_whitelist_array, MAX_BT_WHITE_LIST);
      pthread_mutex_unlock(&pt_app_mgt->mtx_for_white_list);
      break;
    }
  }

  pthread_mutex_unlock(&pt_app_mgt->mtx_q_devices);

EXIT:
  return _ret;
}
#endif /* SKF_GW_NEW == 1 */

	
	/*to find a dev-proxy from device queue when try to connect to it
	ret@
	
	NOTE!! this function will be blocked , wait for the reply from BT-connection operation or timeout
	*/
	app_state_t bt_con_search_for_sensor_and_connect( app_management *pt_app_mgt,
													  struct pthread_cond_var * kp_cond_var)
	{
		app_state_t tpret = ST_OK;
		struct bt_addr kp_whitelist_array[MAX_BT_WHITE_LIST] = {0};
		uint8_t kp_btmac_strbuf[MAX_BT_WHITE_LIST][MAX_BT_ADDRSTR] = {0};
		uint8_t *pt_nap = NULL, *pt_lap = NULL;
		uint16_t to_check_endian = 0;
	
		static uint32_t hld_idx = 0;
		
		struct l_dbus_proxy *pt_proxy = NULL, *pt_proxy_to_connect = NULL;
		uint32_t kp_position = 0;
		struct l_queue_entry *pt_dev_q_entry = NULL;
		const char*pt_proxy_addrstr = NULL;
	
		
	
		if(NULL == pt_app_mgt)
		{
			DBG_LOG_ERR("unexpected NULL");
			tpret = ST_ERR;
			goto EXIT;
		}
	
		if((NULL != sys_get_connected_proxy( pt_app_mgt)) || (true == sys_get_con_in_progress_flg()))
		{
			DBG_LOG_WARN("there is a connection exist(0x%x) or connection-op in process(%d)", sys_get_connected_proxy( pt_app_mgt), sys_get_con_in_progress_flg());
			tpret = ST_ERR;
			goto EXIT;
		}
		
		memset( kp_whitelist_array, 0, MAX_BT_WHITE_LIST*sizeof(struct bt_addr));
		sys_get_bt_white_list( kp_whitelist_array, MAX_BT_WHITE_LIST);
	
		//to get the BT-mac string
		for(uint32_t i = 0; i < MAX_BT_WHITE_LIST; i++)
		{
			memset( kp_btmac_strbuf[i], 0, MAX_BT_ADDRSTR);
			pt_nap = (uint8_t*)(&kp_whitelist_array[i].nap);
			pt_lap = (uint8_t*)(&kp_whitelist_array[i].lap);
	
			to_check_endian = 0xAABB;
			if(*((uint8_t*)&to_check_endian) == 0xAA)
			{		
				snprintf( kp_btmac_strbuf[i], MAX_BT_ADDRSTR,"%02X:%02X:%02X:%02X:%02X:%02X",pt_nap[0], pt_nap[1], kp_whitelist_array[i % MAX_BT_WHITE_LIST].uap, pt_lap[0], pt_lap[1], pt_lap[2]);
			}
			else
			{		
				snprintf( kp_btmac_strbuf[i], MAX_BT_ADDRSTR,"%02X:%02X:%02X:%02X:%02X:%02X",pt_nap[1], pt_nap[0], kp_whitelist_array[i % MAX_BT_WHITE_LIST].uap, pt_lap[2], pt_lap[1], pt_lap[0]); 				
			}
		}
	
		g_pt_timer_for_connection_reply = l_timeout_create( MAX_BT_CON_WAITING_PERIOD, timeout_notify_cb_for_bt_connection_reply, NULL, timeout_destroy_cb_for_bt_connection_reply);
		if(NULL == g_pt_timer_for_connection_reply)
		{
			DBG_LOG_ERR("fail to create timer for BT-connection");
			tpret = ST_ERR;
			goto EXIT;
		}
		
	#warning "NOTE============to make sure dead-lock not happen here"
		DBG_LOG_WARN("====double check when weired things happen===");
		// now we try to find the dev-proxy and connect to it
		//MK287U_______________________________________________________________
		pthread_mutex_lock( &pt_app_mgt->mtx_q_devices);
		pt_dev_q_entry = l_queue_get_entries( pt_app_mgt->pt_q_devices);
		pt_proxy_to_connect = NULL;
		kp_position = 0;
		
		#if (1)
		uint8_t tp_btaddr_buf[MAX_BT_ADDRSTR] = {0};
		for(uint32_t i = 0;i < MAX_BT_WHITE_LIST; i++)
		{
			pt_proxy = bt_common_to_find_dev_proxy_by_addrstr( pt_app_mgt->pt_q_devices, kp_btmac_strbuf[i]);
			if(NULL != pt_proxy)
			{// we find the dev-proxy
				DBG_LOG_WARN("dev-proxy with addr :%s is found, Going to connect to it. Race-condiction here, to fix it", kp_btmac_strbuf[i]);
				DBG_LOG_DEBUG("i = %d", i);
				pt_proxy_to_connect = pt_proxy;
				snprintf( tp_btaddr_buf, MAX_BT_ADDRSTR,"%s", kp_btmac_strbuf[i]);
				// >>> @20240530 Added by Sean (to put the connected address to the last)
				refresh_white_list(kp_whitelist_array, MAX_BT_WHITE_LIST, i);
				// <<<
				break;
			}
			pthread_mutex_lock(&pt_app_mgt->mtx_for_white_list);
			sys_set_bt_white_list(kp_whitelist_array, MAX_BT_WHITE_LIST);
			pthread_mutex_unlock(&pt_app_mgt->mtx_for_white_list);
		}
		#else
		while(1)
		{
			if(NULL == pt_dev_q_entry)
			{
				DBG_LOG_WARN("end of dev-proxy queue, len = %d", l_queue_length( pt_app_mgt->pt_q_devices));
				break;
			}
			pt_proxy = *((struct l_dbus_proxy**)pt_dev_q_entry->data);
			if(!l_dbus_proxy_get_property( pt_proxy, "Address", "s", &pt_proxy_addrstr))
			{
				DBG_LOG_ERR("fail to get ")
				pt_proxy = NULL;
				tpret = ST_ERR;
				break;
			}
			for(uint32_t i = 0;i < MAX_BT_WHITE_LIST; i++)
			{
				DBG_LOG_INFO("dev-proxy %s compare to %s", pt_proxy_addrstr, kp_btmac_strbuf[(i + hld_idx) % MAX_BT_WHITE_LIST]);
				if(strcmp( pt_proxy_addrstr, kp_btmac_strbuf[(i + hld_idx) % MAX_BT_WHITE_LIST]) == 0)
				{//they match
					if((NULL == pt_proxy_to_connect) || (kp_position > i))
					{
						pt_proxy_to_connect = pt_proxy;
						kp_position = i;
						DBG_LOG_WARN("the proxy to connect is set to %s, i = %d, hld_idx = %d", pt_proxy_addrstr, i, hld_idx);
					}
					break;
				}
			}
			//__continue_to_have_the_damn_job_done
			// switch to next queue node
			pt_dev_q_entry = pt_dev_q_entry->next;
		}
		#endif
		DBG_LOG_INFO("proxy_to_connect@0x%x in_process_flg=%d, connected_proxy@0x%p", pt_proxy_to_connect, sys_get_con_in_progress_flg(),sys_get_connected_proxy(pt_app_mgt));
		if(pt_proxy_to_connect && (false == sys_get_con_in_progress_flg())&&(NULL == sys_get_connected_proxy( pt_app_mgt)))
		{  
			// Try to connect 
			sys_set_con_in_progress_flg( true);
			sys_set_active_bt_device(tp_btaddr_buf);// kp_btmac_strbuf[(kp_position + hld_idx) % MAX_BT_WHITE_LIST]);
	
			if(0 == l_dbus_proxy_method_call( pt_proxy_to_connect, "Connect", NULL,  msg_reply_for_bt_connect_v2, NULL, NULL))
			{//fail to call method Connect
				DBG_LOG_ERR(" method-Connect call fail");
				sys_set_con_in_progress_flg( false);// to reset flag, so we can try again latter

				// remove the timer when fail to call method-Connect
				l_timeout_remove( g_pt_timer_for_connection_reply);
				g_pt_timer_for_connection_reply = NULL;

				tpret = ST_ERR;
			}
			else
			{   // method-Connect is called successfully
				DBG_LOG_WARN("method-Connect is called, we need to wait for reply later");
				//is_connect_called_ok = true;
				hld_idx ++;
			}
			pt_proxy_connect_timeout = pt_proxy_to_connect;
		}
		else
		{
			DBG_LOG_INFO("well nothing to do");
			//is_connect_called_ok = false;

			// remove the timer when fail to call method-Connect
			l_timeout_remove( g_pt_timer_for_connection_reply);
			g_pt_timer_for_connection_reply = NULL;

			if((NULL == sys_get_connected_proxy(pt_app_mgt)) && 
			   (false == sys_get_con_in_progress_flg()) && 
			   (scanningAnomalyDetector(pt_app_mgt->pt_q_devices) == true))
			{
				// >>> @20240613 Added by Sean (to restart scanning once a refresh happens)
				// Just avoid re-turn on scanning, so turn off scanning first
				// to wait for scanning is off
				DBG_LOG_DEBUG("Turn off scanning as a refresh happened ...");
				pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
				kp_cond_var->is_op_done = false;
				clock_gettime(CLOCK_MONOTONIC, &(kp_cond_var->tv));
				kp_cond_var->tv.tv_sec += TIMEOUT_BT_TURNING_OFF_S;
				if(ST_OK == bt_common_turn_off_scanning( pt_proxy, kp_cond_var))
				{
					pthread_mutex_lock( &(kp_cond_var->mtx));
					while(false == (kp_cond_var->is_op_done))
					{
						if(pthread_cond_timedwait(&(kp_cond_var->cond_var), 
												&(kp_cond_var->mtx),
												&(kp_cond_var->tv)) == ETIMEDOUT)
						{
							DBG_LOG_ERR("Turnning off scanning timeout!");
							break;
						}
					}
					pthread_mutex_unlock( &(kp_cond_var->mtx));
				}
				DBG_LOG_DEBUG("Turn off scanning successfully ...");

				while(1)
				{
					DBG_LOG_DEBUG("Turn on scanning as a refresh happened ...");
					pt_proxy = sys_get_proxy_adapter1( pt_app_mgt);
					kp_cond_var->is_op_done = false;
					clock_gettime(CLOCK_MONOTONIC, &(kp_cond_var->tv));
					kp_cond_var->tv.tv_sec += TIMEOUT_BT_TURNING_ON_S;
					if(ST_OK == bt_common_turn_on_scanning( pt_proxy, kp_cond_var))
					{
						pthread_mutex_lock( &kp_cond_var->mtx);
						while(false == kp_cond_var->is_op_done)
						{
							if(pthread_cond_timedwait(&(kp_cond_var->cond_var), 
													&(kp_cond_var->mtx),
													&(kp_cond_var->tv)) == ETIMEDOUT)
							{
								DBG_LOG_ERR("Turnning on scanning timeout!");
								kp_cond_var->para++;
								break;
							}
						}
						pthread_mutex_unlock( &(kp_cond_var->mtx));
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
					if(kp_cond_var->para == 0)
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
						if(kp_cond_var->para > 2)
						{
							DBG_LOG_ERR("Turn on scanning fail!");
							exit(1);
						}else{
							DBG_LOG_ERR("retry turning on scanning...");
						}
					}
				}
			}
		}

#if 0
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
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_devices);
		//MK287D___________________________________________________________________
	
		
		EXIT:
			return tpret;
	}
	
#else // the following we use pthread-cond to wait for BT-connection reply. It turns out this is not a good way!!!

#if(1)
	/*
	typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data);
	*/
	static void msg_setup_for_bt_connect_v2(struct l_dbus_message *message, void *user_data)
	{	
		DBG_LOG_INFO("no paramter for method-Connect");
		l_dbus_message_set_arguments( message,"");
	}
	/*
	typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data);
	*/
	static void msg_reply_for_bt_connect_v2(struct l_dbus_proxy*proxy, struct l_dbus_message*result, void *user_data)
	{
		if(l_dbus_message_is_error(result))
		{
			const char*ptname = NULL, *ptext = NULL;
			l_dbus_message_get_error( result, &ptname, &ptext);
			DBG_LOG_ERR("the error info: %s , %s", ptname, ptext);
			goto ERR;
		}
		DBG_LOG_INFO("msg to create a BT connection is sent successfully, proxy-path %s", l_dbus_proxy_get_path( proxy));
		
		#if(1) //ref@connection_reply
			app_management *pt_app_mgt = sys_get_mgt();
			pthread_mutex_lock( &pt_app_mgt->mtx_con_event);
			sys_set_connected_proxy( pt_app_mgt, proxy);
			pt_app_mgt->bt_con_event = BT_EVENT_CONNECTION;
			if(pthread_cond_broadcast( &pt_app_mgt->cond_val_con_event))
			{
				DBG_LOG_ERR("fail to siginal the cond-val");
			}
			else
			{				
				DBG_LOG_INFO("the BT connection event is signaled, cond-val@0x%x", &pt_app_mgt->cond_val_con_event);
			}
			pthread_mutex_unlock(&pt_app_mgt->mtx_con_event);

			// to report the event
			pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
			l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)BT_EVENT_CONNECTION);
			pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
			pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);	
		#endif
		
		ERR:
			#if(1)// to siginal the receivation of reply
				struct pthread_cond_var *pt_cond_var = (struct pthread_cond_var*)user_data;
				if(pt_cond_var)
				{
					pthread_mutex_lock(&pt_cond_var->mtx);
					pt_cond_var->is_op_done = true;
					pthread_cond_signal( &pt_cond_var->cond_var);
					pthread_mutex_unlock( &pt_cond_var->mtx);
				}
			#endif
			sys_set_con_in_progress_flg(false); // reset the flag even fail to connect
			
			return;
	}
#endif



/*to find a dev-proxy from device queue when try to connect to it
ret@

NOTE!! this function will be blocked , wait for the reply from BT-connection operation or timeout
*/
app_state_t bt_con_search_for_sensor_and_connect( app_management *pt_app_mgt)
{
	app_state_t tpret = ST_OK;
	struct bt_addr kp_whitelist_array[MAX_BT_WHITE_LIST] = {0};
	uint8_t kp_btmac_strbuf[MAX_BT_WHITE_LIST][MAX_BT_ADDRSTR] = {0};
	uint8_t *pt_nap = NULL, *pt_lap = NULL;
	uint16_t to_check_endian = 0;

	static uint32_t hld_idx = 0;
	
	struct l_dbus_proxy *pt_proxy = NULL, *pt_proxy_to_connect = NULL;
	uint32_t kp_position = 0;
	struct l_queue_entry *pt_dev_q_entry = NULL;
	const char*pt_proxy_addrstr = NULL;

	struct pthread_cond_var kp_cond_var = {0};
	bool is_connect_called_ok = false;
	struct timespec kp_ts = {0};
	int32_t kp_int32 = 0;

	if(NULL == pt_app_mgt)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}

	if(NULL != sys_get_connected_proxy( pt_app_mgt))
	{
		DBG_LOG_WARN("nothing to do, there is a connection exist");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	sys_get_bt_white_list( kp_whitelist_array, MAX_BT_WHITE_LIST);

	//to get the BT-mac string
	for(uint32_t i = 0; i < MAX_BT_WHITE_LIST; i++)
	{
		pt_nap = (uint8_t*)(&kp_whitelist_array[i].nap);
		pt_lap = (uint8_t*)(&kp_whitelist_array[i].lap);

		to_check_endian = 0xAABB;
		if(*((uint8_t*)&to_check_endian) == 0xAA)
		{		
			snprintf( kp_btmac_strbuf[i], MAX_BT_ADDRSTR,"%02X:%02X:%02X:%02X:%02X:%02X",pt_nap[0], pt_nap[1], kp_whitelist_array[i % MAX_BT_WHITE_LIST].uap, pt_lap[0], pt_lap[1], pt_lap[2]);
		}
		else
		{		
			snprintf( kp_btmac_strbuf[i], MAX_BT_ADDRSTR,"%02X:%02X:%02X:%02X:%02X:%02X",pt_nap[1], pt_nap[0], kp_whitelist_array[i % MAX_BT_WHITE_LIST].uap, pt_lap[2], pt_lap[1], pt_lap[0]);					
		}
	}

	// to initialize cond-var
	if(ST_OK != app_com_init_pthread_cond_var_ctr(&kp_cond_var))
	{
		DBG_LOG_ERR("fail to init cond-var");
		tpret = ST_ERR;
		goto EXIT;
	}

	#warning "NOTE============to make sure dead-lock not happen here"
	DBG_LOG_WARN("====double check when weired things happen===");
	// now we try to find the dev-proxy and connect to it
	//MK287U_______________________________________________________________
	pthread_mutex_lock( &pt_app_mgt->mtx_q_devices);
	pt_dev_q_entry = l_queue_get_entries( pt_app_mgt->pt_q_devices);
	pt_proxy_to_connect = NULL;
	kp_position = 0;
	while(1)
	{
		if(NULL == pt_dev_q_entry)
		{
			break;
		}
		pt_proxy = *((struct l_dbus_proxy**)pt_dev_q_entry->data);
		if(!l_dbus_proxy_get_property( pt_proxy, "Address", "s", &pt_proxy_addrstr))
		{
			DBG_LOG_ERR("fail to get ")
			pt_proxy = NULL;
			tpret = ST_ERR;
			break;
		}
		for(uint32_t i = 0;i < MAX_BT_WHITE_LIST; i++)
		{
			DBG_LOG_INFO("dev-proxy %s compare to %s", pt_proxy_addrstr, kp_btmac_strbuf[(i + hld_idx) % MAX_BT_WHITE_LIST]);
			if(strcmp( pt_proxy_addrstr, kp_btmac_strbuf[(i + hld_idx) % MAX_BT_WHITE_LIST]) == 0)
			{//they match
				if((NULL == pt_proxy_to_connect) || (kp_position > i))
				{
					pt_proxy_to_connect = pt_proxy;
					kp_position = i;
					DBG_LOG_WARN("the proxy to connect is set to %s, i = %d", pt_proxy_addrstr, i);
				}
				break;
			}
		}
		//__continue_to_have_the_damn_job_done
		// switch to next queue node
		pt_dev_q_entry = pt_dev_q_entry->next;
	}
	DBG_LOG_INFO("proxy_to_connect@0x%x in_process_flg=%d, connected_proxy@0x%x", pt_proxy_to_connect, sys_get_con_in_progress_flg(),sys_get_connected_proxy(pt_app_mgt));
	if(pt_proxy_to_connect && (false == sys_get_con_in_progress_flg())&&(NULL == sys_get_connected_proxy( pt_app_mgt)))
	{// try to connect 
		sys_set_con_in_progress_flg( true);
		sys_set_active_bt_device( kp_btmac_strbuf[(kp_position + hld_idx) % MAX_BT_WHITE_LIST]);

		kp_cond_var.is_op_done = false;
		if(0 == l_dbus_proxy_method_call( pt_proxy_to_connect, "Connect", NULL,  msg_reply_for_bt_connect_v2, &kp_cond_var, NULL))
		{//fail to call method Connect
			DBG_LOG_ERR(" method-Connect call fail");
			sys_set_con_in_progress_flg( false);// to reset flag, so we can try again latter
			is_connect_called_ok = false;			
			tpret = ST_ERR;
		}
		else
		{ // method-Connect is called successfully
			DBG_LOG_WARN("method-Connect is called, we need to wait for reply later");
			is_connect_called_ok = true;
		}
	}
	else
	{
		DBG_LOG_INFO("well nothing to do");
		is_connect_called_ok = false;
	}
	pthread_mutex_unlock( &pt_app_mgt->mtx_q_devices);
	//MK287D___________________________________________________________________

	// to wait for reply
	if(is_connect_called_ok)
	{	
		DBG_LOG_INFO("to wait for reply from method-Connect call");
		pthread_mutex_lock(&kp_cond_var.mtx);
		clock_gettime(CLOCK_REALTIME, &kp_ts);
		kp_ts.tv_sec = kp_ts.tv_sec + 45;
		while(1)
		{
			if(false == kp_cond_var.is_op_done)
			{
				kp_int32 = pthread_cond_timedwait( &kp_cond_var.cond_var, &kp_cond_var.mtx, &kp_ts);
				if(ETIMEDOUT == kp_int32)
				{// waiting timeout
					DBG_LOG_ERR("waiting for BT-connection reply timeout");
					sys_set_con_in_progress_flg(false); // to reset the flag, so we can try again later
					break;
				}
			}
			else
			{				
				DBG_LOG_WARN("BT-connection reply is received");
				break;
			}
		}
		pthread_mutex_unlock( &kp_cond_var.mtx);
		
		//switch to next device when it connected successfully
		hld_idx++;
	}
	app_com_deinit_pthread_cond_var_ctr( &kp_cond_var);
	EXIT:
		return tpret;
}

#endif

//MK283D__________________________________________________________





static void msg_setup_for_bt_disconnect(
  struct l_dbus_message *message,
  void *user_data);
static void msg_reply_for_bt_disconnect(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data);
static void destroy_for_bt_disconnect(void *user_data);

/**
 * @brief: The callback called by Disconnect DBUS method: setup
 * @return: NULL
 */
static void
msg_setup_for_bt_disconnect(
  struct l_dbus_message *message,
  void *user_data)
{
  l_dbus_message_set_arguments(message, "");
}
/**
 * @brief: The callback called by Disconnect DBUS method: reply
 * @return: NULL
 */
static void
msg_reply_for_bt_disconnect(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data)
{
  const char *pt_name = NULL, *pt_text = NULL;

  // To set the condition when BT disconnection operation has been done, no
  // matter failure or success
  if(user_data)
  {
    struct pthread_cond_var *pt_cond_var = NULL;
    pt_cond_var = (struct pthread_cond_var *)user_data;
    pthread_mutex_lock(&pt_cond_var->mtx);
    pt_cond_var->is_op_done = true;
    pthread_cond_signal(&pt_cond_var->cond_var);
    pthread_mutex_unlock(&pt_cond_var->mtx);
  }

  if(l_dbus_message_is_error(result))
  {
    // DBUS error happens
    l_dbus_message_get_error(result, &pt_name, &pt_text);
    DBG_LOG_ERR("Failed to disconnect, err_info: %s-%s", pt_name, pt_text);
    return;
  }

  // If disconnection is successful, then clear the hld_connected_proxy
  sys_set_connected_proxy(sys_get_mgt(), NULL);

  DBG_LOG_INFO("BT disconnection is successful!");
}
/**
 * @brief: The callback called by Disconnect DBUS method: destroy
 * @return: NULL
 */
static void
destroy_for_bt_disconnect(void *user_data)
{
  return;
}
/**
 * @brief: To disconnect from the peer. Notice that @p pt_cond_var can be
 * NULL when you do not care about when the disconnection operation has
 * been done.
 * @return: ST_OK when things go well
 */
app_state_t
bt_con_disconnect(
  struct l_dbus_proxy *pt_proxy,
  struct pthread_cond_var *pt_cond_var)
{
  app_state_t tpret = ST_OK;
  const char *pt_interface = NULL;
  bool kp_con_status = false;

  if(pt_proxy == NULL)
  {
    DBG_LOG_ERR("Unexpected pointer of pt_proxy: NULL");
    tpret = ST_ERR;
    return tpret;
  }

  pt_interface = l_dbus_proxy_get_interface(pt_proxy);
  if(pt_interface == NULL)
  {
    DBG_LOG_ERR("Failed to get proxy interface");
    tpret = ST_ERR;
    return tpret;
  }
  if(strcmp(pt_interface, BT_IF_NM_DEVICE))
  {
    DBG_LOG_ERR("Expected interface: %s", BT_IF_NM_DEVICE);
    tpret = ST_ERR;
    return tpret;
  }

  // To check if the proxy is connected or not
  if(false ==
     l_dbus_proxy_get_property(pt_proxy, "Connected", "b", &kp_con_status))
  {
    DBG_LOG_ERR("Failed to check the connection status");
    tpret = ST_ERR;
    return tpret;
  }

  if(false == kp_con_status)
  {
    DBG_LOG_WARN("The device is not being connected");
    tpret = ST_OK;
    // To set condition when BT disconnection operation has been done, no
    // matter failure or success
    if(pt_cond_var != NULL)
    {
      pthread_mutex_lock(&pt_cond_var->mtx);
      pt_cond_var->is_op_done = true;
      pthread_cond_signal(&pt_cond_var->cond_var);
      pthread_mutex_unlock(&pt_cond_var->mtx);
    }
    return tpret;
  }

  if(0 ==
     l_dbus_proxy_method_call(pt_proxy,
                              "Disconnect",
                              msg_setup_for_bt_disconnect,
                              msg_reply_for_bt_disconnect,
                              pt_cond_var,
                              destroy_for_bt_disconnect))
  {
    DBG_LOG_ERR("Failed to call proxy method Disconnect");
    tpret = ST_ERR;
    return tpret;
  }

  return tpret;
}
