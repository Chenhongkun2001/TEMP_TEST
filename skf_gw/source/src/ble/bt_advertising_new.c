/**
 * @file    bt_advertising_new.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-18
 * @brief   Implementation of BLE advertising.
 * @details
 */
#include "bt_advertising_new.h"
// #if (0)
#include "bt_advertising.h"
// #endif
#include "bt_common.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

static struct bluetooth_adv_controller g_bt_adv_ctr;

struct bluetooth_adv_controller *
bt_adv_n_get_adv_ctr(void)
{
  return &g_bt_adv_ctr;
}
/**
 * @brief Initialize the controller for advertising
 */
void
bt_adv_n_init_adv_ctr(struct bluetooth_adv_controller *pt_adv_ctr)
{
  if(NULL == pt_adv_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }
  pt_adv_ctr->dbus = NULL;
  pt_adv_ctr->proxy_le_adv_manager = NULL;

  app_com_deinit_pthread_cond_var_ctr(&pt_adv_ctr->cond_var);
  app_com_init_pthread_cond_var_ctr(&pt_adv_ctr->cond_var);
}
/**
 * @brief Set the pointer to the Bluez DBUS
 */
void
bt_adv_n_set_dbus_handler(
  struct bluetooth_adv_controller *pt_adv_ctr,
  const struct l_dbus *pt_dbus)
{
  if((NULL == pt_adv_ctr) || (NULL == pt_dbus))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }
  pt_adv_ctr->dbus = pt_dbus;
  return;
}
/**
 * @brief Get the pointer to the Bluez DBUS
 * @return The pointer to the Bluez DBUS
 */
struct l_dbus *
bt_adv_n_get_dbus_handler(
  const struct bluetooth_adv_controller *pt_adv_ctr)
{
  struct l_dbus *_ret = NULL;

  if(NULL == pt_adv_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return _ret;
  }
  _ret = pt_adv_ctr->dbus;
  return _ret;
}
/**
 * @brief Set the pointer to le advertising manager proxy
 */
app_state_t
bt_adv_n_set_adv_manager_proxy(
  struct bluetooth_adv_controller *pt_adv_ctr,
  const struct l_dbus_proxy *adv_proxy)
{
  app_state_t _ret = ST_OK;
  const char *_pt_interface = NULL;

  if((pt_adv_ctr == NULL) || (adv_proxy == NULL))
  {
    DBG_LOG_ERR("Invalid parameter");
    _ret = ST_ERR;
    return _ret;
  }

  _pt_interface = l_dbus_proxy_get_interface(adv_proxy);
  if(_pt_interface == NULL)
  {
    DBG_LOG_ERR("Failed to get proxy interface");
    _ret = ST_ERR;
    return _ret;
  }
  if(strcmp(_pt_interface, BT_IF_NM_LEADVERTISING_MANAGER))
  {
    // Do nothing, just quit
    _ret = ST_ERR;
    return _ret;
  }

  if(pt_adv_ctr->proxy_le_adv_manager)
  {
    DBG_LOG_WARN("hld_proxy_le_adv_manager1 is not NULL");
  }
  pt_adv_ctr->proxy_le_adv_manager = adv_proxy;

  // to post the event 
  #if(1)
	app_management *pt_app_mgt = sys_get_mgt(); 
  	pthread_mutex_lock( &pt_app_mgt->mtx_adv_scanning_ctr);
	l_queue_push_tail( pt_app_mgt->pt_adv_scanning_event_queue, (void *)BT_EVENT_ADD_LEADV_MGR);
	pthread_cond_signal( &pt_app_mgt->cond_val_adv_scanning_ctr);
	pthread_mutex_unlock( &pt_app_mgt->mtx_adv_scanning_ctr);
  #endif
  
  DBG_LOG_INFO("proxy_le_adv_manager @ 0x%llx",
                pt_adv_ctr->proxy_le_adv_manager);				
  return _ret;
}
/**
 * @brief Get the pointer to le advertising manager proxy
 * @return The pointer to le advertising manager proxy
 */
struct l_dbus_proxy *
bt_adv_n_get_adv_manager_proxy(
  const struct bluetooth_adv_controller *pt_adv_ctr)
{
  struct l_dbus_proxy *_ret = NULL;

  if(NULL == pt_adv_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return _ret;
  }
  _ret = pt_adv_ctr->proxy_le_adv_manager;
  return _ret;
}
/**
 * @brief Set the gateway's BLE MAC address to @p gw_id_buf of the data
 * structure pointed by @p pt_adv_ctr
 */
app_state_t
bt_adv_n_set_adv_gwid_string(
  struct bluetooth_adv_controller *pt_adv_ctr,
  const struct l_dbus_proxy *pt_proxy_adapter)
{
  app_state_t _ret = ST_OK;
  const char *_pt_interface = NULL;
  const char *_pt_addr = NULL;
  char _strbuf[MAX_ID_STR_LENGTH] = { 0 };
  uint8_t i = 0, cnt = 0;

  if((NULL == pt_adv_ctr) || (NULL == pt_proxy_adapter))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }
  _pt_interface = l_dbus_proxy_get_interface(pt_proxy_adapter);
  if(NULL == _pt_interface)
  {
    DBG_LOG_ERR("Failed to get proxy interface");
    _ret = ST_ERR;
    return _ret;
  }
  if(strcmp(_pt_interface, BT_IF_NM_ADAPTER))
  {
    // Do nothing, just quit
    _ret = ST_ERR;
    return _ret;
  }
  if(false == l_dbus_proxy_get_property(pt_proxy_adapter, "Address", "s",
                                        &_pt_addr))
  {
    DBG_LOG_ERR("Failed to get proxy property");
    _ret = ST_ERR;
    return _ret;
  }

  i = 0;
  cnt = 0;
  memset(_strbuf, 0, sizeof(_strbuf));
  while(_pt_addr[i])
  {
    if(_pt_addr[i] != ':')
    {
      _strbuf[cnt % MAX_ID_STR_LENGTH] = _pt_addr[i];
      cnt++;
    }
    i++;
  }

  snprintf(pt_adv_ctr->gw_id_buf, sizeof(pt_adv_ctr->gw_id_buf), "%s",
           _strbuf);

  DBG_LOG_INFO("gw_id_buf %s", pt_adv_ctr->gw_id_buf);
  return _ret;
}
/**
 * @brief Get the gateway's BLE MAC address from @p gw_id_buf of the data
 * structure pointed by @p pt_adv_ctr
 * @return A pointer to the string @p gw_id_buf
 */
const char *
bt_adv_n_get_gwid_str(const struct bluetooth_adv_controller *pt_adv_ctr)
{
  if(NULL == pt_adv_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return NULL;
  }
  return pt_adv_ctr->gw_id_buf;
}
/**
 * @brief Turn on the LE advertisement
 */
void
bt_adv_n_turn_on_le_advertisement(
  struct bluetooth_adv_controller *pt_adv_ctr)
{
  uint8_t _strbuf[MAX_ID_STR_LENGTH] = { 0 };
  const uint8_t *_pt_idstr = NULL;
  size_t _strlen = 0;

  struct l_dbus *_pt_dbus;
  struct l_dbus_proxy *_pt_proxy;

  if(NULL == pt_adv_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }
  _pt_dbus = pt_adv_ctr->dbus;
  _pt_proxy = pt_adv_ctr->proxy_le_adv_manager;
  if((NULL == _pt_dbus) || (NULL == _pt_proxy))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }
  _pt_idstr = bt_adv_n_get_gwid_str(bt_adv_n_get_adv_ctr());

  if(NULL == _pt_idstr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }

  bt_advertising_clear_ad_info();

  // Set local name
  _strlen = strlen(_pt_idstr);
  if(_strlen >= 6)
  {
    snprintf(_strbuf, sizeof(_strbuf), "%s%s", NM_SKF_GW_BT_SERVER,
             &_pt_idstr[_strlen - 6]);
  }
  else
  {
    snprintf(_strbuf, sizeof(_strbuf), "%s", NM_SKF_GW_BT_SERVER);
  }
  bt_advertising_set_local_name(_strbuf);

  // Set advertising interval
  bt_advertising_set_adv_interval(MIN_BT_ADV_INTERVAL + 10U,
                                  MIN_BT_ADV_INTERVAL + 100U);

  // Setup the timeout timer
  pt_adv_ctr->cond_var.is_op_done = false;
  clock_gettime(CLOCK_MONOTONIC, &pt_adv_ctr->cond_var.tv);
  pt_adv_ctr->cond_var.tv.tv_sec = pt_adv_ctr->cond_var.tv.tv_sec +
                                   CONFIG_BT_TURN_ON_LE_ADVERTISEMENT_TIMEOUT_S;
  // Start LE advertisement
  bt_advertising_register_v2(_pt_dbus,
                          _pt_proxy,
                          bt_advertising_get_arguments_str(
                            IDX_AD_PERIPHERAL),
                          &pt_adv_ctr->cond_var);

  // Wait for the operation done
  pthread_mutex_lock(&pt_adv_ctr->cond_var.mtx);
  while(false == pt_adv_ctr->cond_var.is_op_done)
  {
    if(pthread_cond_timedwait(&pt_adv_ctr->cond_var.cond_var,
                              &pt_adv_ctr->cond_var.mtx,
                              &pt_adv_ctr->cond_var.tv) == ETIMEDOUT)
    {
      DBG_LOG_ERR("Turning on advertisement is timeout");
      break;
    }
  }
  pthread_mutex_unlock(&pt_adv_ctr->cond_var.mtx);
  return;
}
/**
 * @brief Turn off the LE advertisement
 */
void
bt_adv_n_turn_off_le_advertisement(
  struct bluetooth_adv_controller *pt_adv_ctr)
{
  struct l_dbus *_pt_dbus;
  struct l_dbus_proxy *_pt_proxy;

  if(NULL == pt_adv_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }
  _pt_dbus = pt_adv_ctr->dbus;
  _pt_proxy = pt_adv_ctr->proxy_le_adv_manager;
  if((NULL == _pt_dbus) || (NULL == _pt_proxy))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }

  // Setup the timeout timer
  pt_adv_ctr->cond_var.is_op_done = false;
  clock_gettime(CLOCK_MONOTONIC, &pt_adv_ctr->cond_var.tv);
  pt_adv_ctr->cond_var.tv.tv_sec = pt_adv_ctr->cond_var.tv.tv_sec +
                                   CONFIG_BT_TURN_OFF_LE_ADVERTISEMENT_TIMEOUT_S;

  bt_advertising_unregister(_pt_dbus,
                            _pt_proxy,
                            &pt_adv_ctr->cond_var);

  // Wait for the operation done
  pthread_mutex_lock(&pt_adv_ctr->cond_var.mtx);
  while(false == pt_adv_ctr->cond_var.is_op_done)
  {
    if(pthread_cond_timedwait(&pt_adv_ctr->cond_var.cond_var,
                              &pt_adv_ctr->cond_var.mtx,
                              &pt_adv_ctr->cond_var.tv) == ETIMEDOUT)
    {
      DBG_LOG_ERR("Turning off advertisement is timeout");
      break;
    }
  }
  pthread_mutex_unlock(&pt_adv_ctr->cond_var.mtx);
  return;
}
