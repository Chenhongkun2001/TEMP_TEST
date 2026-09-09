/**
 * @file    bt_connection_new.c
 * @author  Xiaoyuan (Sean) Ma, Tongde (Victor) Yan
 * @date    2025-03-06
 * @brief   Implementation of BLE connection.
 * @details
 */
#include "app_common.h"
#include "bt_common.h"
#include "bt_connection_new.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

#ifdef UNITTEST
#if (UNITTEST == UNITTEST_CONNECTION_DISCONNECTION)
#define BT_CONNECTION_NEW_CONNECTION_REQUIRED (1)
#endif /* UNITTEST == UNITTEST_CONNECTION_DISCONNECTION */
#if (UNITTEST == UNITTEST_BLE_UART)
#define BT_CONNECTION_NEW_CONNECTION_REQUIRED (1)
#endif /* UNITTEST == UNITTEST_BLE_UART */
#if ((UNITTEST == UNITTEST_MULTI_CONNECTION) || (UNITTEST == UNITTEST_MULTI_CON_SCAN_AND_PARSE_INSIGHT_T))
#define BT_CONNECTION_NEW_CONNECTION_REQUIRED (1)
#define BT_CONNECTION_NEW_MULTI_CONNECTION_ENABLED (1)
#endif /* UNITTEST == UNITTEST_BLE_UART */
#if (UNITTEST == UNITTEST_UNIVERSE)
#define BT_CONNECTION_NEW_CONNECTION_REQUIRED (1)
#endif /* UNITTEST == UNITTEST_UNIVERSE */
#else /* UNITTEST */
#define BT_CONNECTION_NEW_CONNECTION_REQUIRED (1)
#endif /* UNITTEST */

#define BT_CONNECTION_NEW_SCANNED_ELE_TTL_S (5)

#if (BT_CONNECTION_NEW_MULTI_CONNECTION_ENABLED == 1)
struct connect_stat_data g_con_stat_data = {0};
#endif

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
static uint8_t g_fail_con_count = 0;
static void
get_mac_addr_from_path(
  char *path,
  char *mac_addr);
uint8_t bt_con_n_get_fail_count(void)
{
  return g_fail_con_count;
}
void bt_con_n_set_fail_count(uint8_t cnt)
{
  g_fail_con_count = cnt;
}
#endif

static struct con_controller g_con_ctr; // Connection controllers
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
static struct con_method_parameter g_con_method_param = {0};
// index counter for calculating the idx of pt_con_ctr->connected_proxy[] when connecting
uint32_t g_con_idx_coutnt = 0;
#endif

static void bt_connection_refresh_white_list(
  struct bt_addr *list,
  uint32_t list_sz,
  uint32_t currentIdx);
static app_state_t bt_con_n_set_inuse(
  struct con_controller *pt_con_ctr,
  struct l_dbus_proxy *pt_proxy,
  bool inuse);

/**
 * @brief Get the connection controller @p g_con_ctr
 */
struct con_controller *
bt_con_n_get_con_ctr(void)
{
  return &g_con_ctr;
}
/**
 * @brief Initialize the connection controller @p g_con_ctr
 */
app_state_t
bt_con_n_init_con_controller(struct con_controller *pt_con_ctr)
{
  app_state_t _ret = ST_OK;

  if(NULL == pt_con_ctr)
  {
    _ret = ST_ERR;
    DBG_LOG_ERR("Unexpected NULL");
    goto ERR;
  }
  pt_con_ctr->dev_queue = l_queue_new();
  if(NULL == pt_con_ctr->dev_queue)
  {
    DBG_LOG_ERR("Failed to create a queue");
    _ret = ST_ERR;
    goto ERR;
  }
  if(pthread_mutex_init(&pt_con_ctr->mtx_for_dev_queue, NULL))
  {
    DBG_LOG_ERR("Failed to initialize mutex");
    _ret = ST_ERR;
    goto ERR;
  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
  {
    pt_con_ctr->connected_proxy[i] = NULL;
    if(ST_ERR == app_com_init_pthread_cond_var_ctr(&pt_con_ctr->pt_cond_var[i]))
    {
      DBG_LOG_ERR("fail to init cond_var %d\n", i);
      _ret = ST_ERR;
      goto ERR;
    }
    // pt_con_ctr->thread_connect[i] = 0;
    memset(&pt_con_ctr->con_mac_addr[i], 0, MAX_ID_STR_LENGTH);
  }
  pt_con_ctr->con_proxy_idx = 0;
  pt_con_ctr->con_status_bit = 0;
  if(pthread_mutex_init(&pt_con_ctr->mtx_for_service_flg, NULL))
  {
    DBG_LOG_ERR("Failed to initialize mutex");
    _ret = ST_ERR;
    goto ERR;
  }
  if(pthread_mutex_init(&pt_con_ctr->mtx_for_sock_io, NULL))
  {
    DBG_LOG_ERR("Failed to initialize mutex");
    _ret = ST_ERR;
    goto ERR;
  }
#else
  pt_con_ctr->connected_proxy = NULL;
#endif
  pt_con_ctr->is_con_in_process = false;
  if(pthread_mutex_init(&pt_con_ctr->mtx_for_connected_proxy, NULL))
  {
    DBG_LOG_ERR("Failed to initialize mutex");
    _ret = ST_ERR;
    goto ERR;
  }

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
  {
    pt_con_ctr->chr_queue[i] = l_queue_new();
    if(NULL == pt_con_ctr->chr_queue[i])
    {
      DBG_LOG_ERR("Failed to create a queue");
      _ret = ST_ERR;
      goto ERR;
    }
    pt_con_ctr->proxy_uart_chr_rx[i] = NULL;
    pt_con_ctr->proxy_uart_chr_tx[i] = NULL;
  }
  pt_con_ctr->svr_chr_queue = l_queue_new();
  if(NULL == pt_con_ctr->svr_chr_queue)
  {
    DBG_LOG_ERR("Failed to create a queue for server\n");
    _ret = ST_ERR;
    goto ERR;
  }
  pt_con_ctr->svr_proxy_uart_chr_rx = NULL;
  pt_con_ctr->svr_proxy_uart_chr_tx = NULL;
#else
  pt_con_ctr->chr_queue = l_queue_new();
  if(NULL == pt_con_ctr->chr_queue)
  {
    DBG_LOG_ERR("Failed to create a queue");
    _ret = ST_ERR;
    goto ERR;
  }
  pt_con_ctr->proxy_uart_chr_rx = NULL;
  pt_con_ctr->proxy_uart_chr_tx = NULL;
#endif
  if(0 != pthread_mutex_init(&pt_con_ctr->mtx_for_chr_queue, NULL))
  {
    DBG_LOG_ERR("Failed to initialize mutex");
    _ret = ST_ERR;
    goto ERR;
  }

  pt_con_ctr->mtx_for_white_list = &(sys_get_mgt()->mtx_for_white_list);
  pt_con_ctr->white_list = sys_get_mgt()->white_list;

  return _ret;

ERR:
  if(pt_con_ctr->dev_queue)
  {
    l_queue_destroy(pt_con_ctr->dev_queue, l_free);
    pt_con_ctr->dev_queue = NULL;
  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
  {
    if(pt_con_ctr->chr_queue[i])
    {
      l_queue_destroy(pt_con_ctr->chr_queue[i], l_free);
      pt_con_ctr->chr_queue[i] = NULL;
    }
  }
  if(pt_con_ctr->svr_chr_queue)
  {
    l_queue_destroy(pt_con_ctr->svr_chr_queue, l_free);
    pt_con_ctr->svr_chr_queue = NULL;
  }
#else
  if(pt_con_ctr->chr_queue)
  {
    l_queue_destroy(pt_con_ctr->chr_queue, l_free);
    pt_con_ctr->chr_queue = NULL;
  }
#endif
  return _ret;
}
/**
 * @brief De-initialize the connection controller @p g_con_ctr
 */
void
bt_con_n_deinit_con_controller(struct con_controller *pt_con_ctr)
{
  if(NULL == pt_con_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }

  // Destroy the queue first ...
  int _ret = 0;
  // deinit dev_queue
  for(uint8_t i=0;i<20;i++)
  {
    _ret = pthread_mutex_trylock(&pt_con_ctr->mtx_for_dev_queue);
    if(0 == _ret)
    {
      if(NULL != pt_con_ctr->dev_queue)
      {
        l_queue_destroy(pt_con_ctr->dev_queue, l_free);
        pt_con_ctr->dev_queue = NULL;
      }
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
      DBG_LOG_DEBUG("dev_queue deinited!\n");  // debug only
      // Destroy the mutex then ...
      pthread_mutex_destroy(&pt_con_ctr->mtx_for_dev_queue);
      break;
    }
    sleep(1);
  }
  if (EBUSY == _ret)
  {
    DBG_LOG_WARN("fail to lock mtx_for_dev_queue! exit!\n");
    exit(EXIT_FAILURE);
  }
  else  // EINVAL
  {
    DBG_LOG_WARN("mtx_for_dev_queue invalid! do nothing!\n");
  }

  // deinit connected_proxy
  for(uint8_t i=0;i<20;i++)
  {
    _ret = pthread_mutex_trylock(&pt_con_ctr->mtx_for_connected_proxy);
    if(0 == _ret)
    {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
      {
        DBG_LOG_DEBUG("deinit cond_var %d!\n", i);  // debug only
        // only de-init cond_var in case following 2 variables are not null
        // to avoid being dead-locked...
        if((NULL != pt_con_ctr->connected_proxy[i]) ||
          (0 != pt_con_ctr->con_mac_addr[i][0]))
        {
          pt_con_ctr->connected_proxy[i] = NULL;
          memset(&pt_con_ctr->con_mac_addr[i], 0, MAX_ID_STR_LENGTH);
          app_com_deinit_pthread_cond_var_ctr(&pt_con_ctr->pt_cond_var[i]);
        }
      }
      pt_con_ctr->con_proxy_idx = 0;
      pt_con_ctr->con_status_bit = 0;
      DBG_LOG_DEBUG("deinit cond_var is done ...\n");
#else
      pt_con_ctr->connected_proxy = NULL;
#endif
      pt_con_ctr->is_con_in_process = false;
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
      pthread_mutex_destroy(&pt_con_ctr->mtx_for_connected_proxy);
      break;
    }
    sleep(1);
  }
  if (EBUSY == _ret)
  {
    DBG_LOG_WARN("fail to lock mtx_for_connected_proxy! exit!\n");
    exit(EXIT_FAILURE);
  }
  else  // EINVAL
  {
    DBG_LOG_WARN("mtx_for_connected_proxy invalid! do nothing!\n");
  }

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  // deinit chr_queue
  for(uint8_t i=0;i<20;i++)
  {
    // Destroy the queue first ...
    _ret = pthread_mutex_trylock(&pt_con_ctr->mtx_for_chr_queue);
    if(0 == _ret)
    {
      for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
      {
        if(NULL != pt_con_ctr->chr_queue[i])
        {
          l_queue_destroy(pt_con_ctr->chr_queue[i], NULL);
          pt_con_ctr->chr_queue[i] = NULL;
        }
        pt_con_ctr->proxy_uart_chr_rx[i] = NULL;
        pt_con_ctr->proxy_uart_chr_tx[i] = NULL;
      }
      if(NULL != pt_con_ctr->svr_chr_queue)
      {
        l_queue_destroy(pt_con_ctr->svr_chr_queue, NULL);
        pt_con_ctr->svr_chr_queue = NULL;
      }
      pt_con_ctr->svr_proxy_uart_chr_rx = NULL;
      pt_con_ctr->svr_proxy_uart_chr_tx = NULL;
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
      // Destroy the mutex then ...
      pthread_mutex_destroy(&pt_con_ctr->mtx_for_chr_queue);
      break;
    }
    sleep(1);
  }
  if (EBUSY == _ret)
  {
    DBG_LOG_WARN("fail to lock mtx_for_chr_queue! exit!\n");
    exit(EXIT_FAILURE);
  }
  else  // EINVAL
  {
    DBG_LOG_WARN("mtx_for_chr_queue invalid! do nothing!\n");
  }
#else
  // Destroy the queue first ...
  pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
  l_queue_destroy(pt_con_ctr->chr_queue, NULL);
  pt_con_ctr->chr_queue = NULL;
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
  // Destroy the mutex then ...
  pthread_mutex_destroy(&pt_con_ctr->mtx_for_chr_queue);
#endif
  DBG_LOG_DEBUG("deinit chr_queue is done ...\n");

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  // deinit service_flg
  for(uint8_t i=0;i<20;i++)
  {
    // Destroy the queue first ...
    _ret = pthread_mutex_trylock(&pt_con_ctr->mtx_for_service_flg);
    if(0 == _ret)
    {
      for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
      {
        pt_con_ctr->fg_is_services_resolved[i] = false;
      }
      pt_con_ctr->fg_service_idx = 0;
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_service_flg);
      // Destroy the mutex then ...
      pthread_mutex_destroy(&pt_con_ctr->mtx_for_service_flg);
      // do not destroy sock_io|io here as they may not available any more
      pthread_mutex_destroy(&pt_con_ctr->mtx_for_sock_io);
      DBG_LOG_DEBUG("deinit service_flg is done ...\n");
      break;
    }
    sleep(1);
  }
  if (EBUSY == _ret)
  {
    DBG_LOG_WARN("fail to lock mtx_for_service_flg! exit!\n");
    exit(EXIT_FAILURE);
    // pthread_mutex_unlock(&pt_con_ctr->mtx_for_service_flg);
    // usleep(5000);
  }
  else  // EINVAL
  {
    DBG_LOG_WARN("mtx_for_service_flg invalid! do nothing!\n");
  }
  DBG_LOG_DEBUG("deinit service_flag is done ...\n");
#endif

  // Detach the white list pointer
  pt_con_ctr->white_list = NULL;
  pt_con_ctr->mtx_for_white_list = NULL;
}
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
/**
 * @brief: The callback called by Disconnect DBUS method: destroy
 * @return: NULL
 */
static void
destroy_for_bt_disconnect(void *user_data)
{
  DBG_LOG_DEBUG("BT disconnect destroy is called!");

  return;
}
// typedef
// void (*l_dbus_message_func_t) (struct l_dbus_message *message, void
// *user_data);
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
// typedef
// void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
// struct l_dbus_message *result, void *user_data);
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
  // To set the condition when BT disconnection operation has been done, no
  // matter failure or success
  if(user_data)
  {
    struct pthread_cond_var *_pt_cond_var = NULL;

    _pt_cond_var = (struct pthread_cond_var *)user_data;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    if(CONFIG_COND_VAR_MAGIC_INIT != _pt_cond_var->magic)
    {
      DBG_LOG_ERR("signal for former con, skip it!\n");
      return;
    }
#endif
    if(pthread_mutex_trylock(&_pt_cond_var->mtx))
    {
      DBG_LOG_ERR(
        "Failed to acquire the lock (perhaps timeout has happened)");
      return;
    }
    else
    {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      // check whether the signal is for current connection
      if((NULL != _pt_cond_var->proxy) && (proxy != _pt_cond_var->proxy))
      {
        DBG_LOG_ERR("signal for former con, skip it!\n");
        pthread_mutex_unlock(&_pt_cond_var->mtx);
        return;
      }
#endif
      _pt_cond_var->is_op_done = true;
      pthread_cond_signal(&_pt_cond_var->cond_var);
      pthread_mutex_unlock(&_pt_cond_var->mtx);
    }
  }

  if(l_dbus_message_is_error(result))
  {
    const char *_pt_name = NULL, *_pt_text = NULL;

    // DBUS error happens
    l_dbus_message_get_error(result, &_pt_name, &_pt_text);
    DBG_LOG_ERR("Failed to disconnect, err_info: %s-%s", _pt_name,
                _pt_text);
    return;
  }

  // If disconnection is successful, then detach the pointer
  // connected_proxy
  pthread_mutex_lock(&bt_con_n_get_con_ctr()->mtx_for_connected_proxy);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  uint8_t tmp_idx = CONFIG_BLE_CONNECTION_MAX_NUM;
  struct con_controller *_pt_con_ctrl = bt_con_n_get_con_ctr();
  for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
  {
    if(proxy == _pt_con_ctrl->connected_proxy[i])
    {
      tmp_idx = i;
      break;
    }
  }
  if(tmp_idx >= CONFIG_BLE_CONNECTION_MAX_NUM)
  {
    for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
    {
      if(proxy == _pt_con_ctrl->pt_cond_var[i].proxy)
      {
        tmp_idx = i;
        break;
      }
    }
  }
  if(tmp_idx >= CONFIG_BLE_CONNECTION_MAX_NUM)
  {
    pthread_mutex_unlock(&bt_con_n_get_con_ctr()->mtx_for_connected_proxy);
    DBG_LOG_ERR("Fail to get idx of proxy: @0x%llx\n", proxy);
    return;
  }
  if(proxy != _pt_con_ctrl->connected_proxy[tmp_idx])
  {
    DBG_LOG_WARN("mis-match: proxy@0x%llx, con-pro:@0x%llx!\n",
                  proxy, _pt_con_ctrl->connected_proxy[tmp_idx]);
  }
  DBG_LOG_INFO("dis-con cb: proxy@0x%llx!\n", proxy);
  _pt_con_ctrl->connected_proxy[tmp_idx] = NULL;
  CON_BIT_CLEAR(_pt_con_ctrl->con_status_bit, tmp_idx);
  memset(&_pt_con_ctrl->con_mac_addr[tmp_idx], 0, MAX_ID_STR_LENGTH);
  _pt_con_ctrl->pt_cond_var[tmp_idx].proxy = NULL;
#else
  bt_con_n_get_con_ctr()->connected_proxy = NULL;
#endif
  pthread_mutex_unlock(&bt_con_n_get_con_ctr()->mtx_for_connected_proxy);
  pthread_mutex_lock(&bt_con_n_get_con_ctr()->mtx_for_dev_queue);
  bt_con_n_set_inuse(bt_con_n_get_con_ctr(), proxy, false);
  pthread_mutex_unlock(&bt_con_n_get_con_ctr()->mtx_for_dev_queue);

  DBG_LOG_INFO("BT disconnection is successful!");
  return;
}
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
/**
 * @brief Disconnect from the device which is being connected and assume
 * that there is only ONE device can be connected at ONE moment. Note that
 * there is a timeout timer would be triggered in the routine. Return
 * @p ST_ERR if a disconnect timeout happens.16:43:10
 * @param pt_con_ctr: The pointer to the connect controller (a data
 * @param idx: index of connection
 * @param pt_con_id: mac addr of connected sensor.eg: 'C4BD6A110186'
 * structure maintained by application)
 * @return ST_OK if everthing is OK.
 */
app_state_t
bt_con_n_disconnect_from_dev(struct con_controller *pt_con_ctr, 
                              uint8_t idx,
                              uint8_t *pt_con_id)
#else
/**
 * @brief Disconnect from the device which is being connected and assume
 * that there is only ONE device can be connected at ONE moment. Note that
 * there is a timeout timer would be triggered in the routine. Return
 * @p ST_ERR if a disconnect timeout happens.
 * @param pt_con_ctr: The pointer to the connect controller (a data
 * structure maintained by application)
 * @return ST_OK if everthing is OK.
 */
app_state_t
bt_con_n_disconnect_from_dev(struct con_controller *pt_con_ctr)
#endif
{
  app_state_t _ret = ST_OK;
#if (SKF_GW_CONFIG_MULTI_CONNECTION != 1)
  struct pthread_cond_var _cond_var;
#endif
  const char *_pt_interface = NULL;
  bool _con_status = false;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  DBG_LOG_INFO("bt_con_n_disconnect called! idx:%d\n", idx);
  if(idx >= CONFIG_BLE_CONNECTION_MAX_NUM)
  {
    DBG_LOG_ERR("Unexpected idx %d to dis-con!\n", idx);
    _ret = ST_ERR;
    return _ret;
  }
#endif

  if(NULL == pt_con_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION != 1)
  _ret = app_com_init_pthread_cond_var_ctr(&_cond_var);
  if(ST_OK != _ret)
  {
    DBG_LOG_ERR(
      "Failed to initialize the condition variable with attributes");
    return _ret;
  }
#else
  // check is_con_in_process to make sure only 1 con/dis-con is ongoing at the same time.
_WAIT_FOR_CON_FLAG:
  pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
  if(pt_con_ctr->is_con_in_process)
  {
    DBG_LOG_WARN("con_in_process is ongoing!%d\n", idx);
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    sleep(1);
    goto _WAIT_FOR_CON_FLAG;
  }
  else
  {
    DBG_LOG_INFO("start to disconnect %d!\n", idx);
    pt_con_ctr->is_con_in_process = true;
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
  }
#endif
  // Lock the mutex and prepare for disconnection
  pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  struct l_dbus_proxy *pt_connected_proxy = NULL;
  pt_connected_proxy = pt_con_ctr->connected_proxy[idx];
  if(NULL == pt_connected_proxy)
  {
    pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_WARN("No device(%d) is being connected", idx);
    _ret = ST_ERR;
    goto EXIT;
  }
  _pt_interface = l_dbus_proxy_get_interface(pt_connected_proxy);
  if(NULL == _pt_interface)
  {
    pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_ERR("Failed to get the interface");
    _ret = ST_ERR;
    goto EXIT;
  }
  if(strcmp(_pt_interface, BT_IF_NM_DEVICE))
  {
    pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_ERR("This should NOT happen ...");
    _ret = ST_ERR;
    goto EXIT;
  }
  // double check whether it's the connection we want to be dis-connected or not
  // in case parameter 'pt_con_id' is not NULL
  if(NULL != pt_con_id)
  {
    const char *path = l_dbus_proxy_get_path(pt_connected_proxy);
    char tmp_mac_addr[MAX_ID_STR_LENGTH] = {0};
    get_mac_addr_from_path(path, tmp_mac_addr);
    DBG_LOG_INFO("proxy mac str:%s idx:%d\n", tmp_mac_addr, idx);
    DBG_LOG_DEBUG("con+id:\n");
    for(uint8_t i=0;i<6;i++)
      printf("%02x",pt_con_id[i]);
    printf("\n");
    if(('C' != tmp_mac_addr[0]) || ('4' != tmp_mac_addr[1]) || (':' != tmp_mac_addr[2]))
    {
      pt_con_ctr->is_con_in_process = false;
      DBG_LOG_ERR("Fail to get mac addr:%s idx:%d\n", tmp_mac_addr, idx);
      // skip this error sliently here. to be confirmed!
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
      _ret = ST_ERR;
      goto EXIT;
    }
    else
    {
      uint8_t _btmac_strbuf[MAX_BT_ADDRSTR] = { 0 };
#if (1)
      snprintf(_btmac_strbuf, MAX_BT_ADDRSTR,
              "%02X:%02X:%02X:%02X:%02X:%02X", pt_con_id[0], pt_con_id[1],
              pt_con_id[2], pt_con_id[3], pt_con_id[4], pt_con_id[5]);
 #else
      uint8_t *_pt_nap = NULL, *_pt_lap = NULL;
      struct bt_addr *_bt_addr = (struct bt_addr *)pt_con_id;

      _pt_nap = (uint8_t *)(&_bt_addr->nap);
      _pt_lap = (uint8_t *)(&_bt_addr->lap);
      if(!sys_is_platform_big_endian())
      {
        snprintf(_btmac_strbuf, MAX_BT_ADDRSTR,
                "%02X:%02X:%02X:%02X:%02X:%02X", _pt_nap[0], _pt_nap[1],
                _bt_addr->uap,
                _pt_lap[0], _pt_lap[1], _pt_lap[2]);
      }
      else
      {
        snprintf(_btmac_strbuf, MAX_BT_ADDRSTR,
                "%02X:%02X:%02X:%02X:%02X:%02X", _pt_nap[1], _pt_nap[0],
                _bt_addr->uap,
                _pt_lap[2], _pt_lap[1], _pt_lap[0]);
      }
#endif
      DBG_LOG_DEBUG("con_id mac str:%s\n",_btmac_strbuf);
      if(0 != strcmp(_btmac_strbuf, tmp_mac_addr))
      {
        pt_con_ctr->is_con_in_process = false;
        // mac addrs are not same. skip it sliently.
        // it means this 'index' is used by other sensor for now
        DBG_LOG_WARN("mac not the same: skip dis-con! idx:%d\n", idx);
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
        _ret = ST_ERR;
        goto EXIT;
      }
    }
  }

  // Just double check the proxy is being connected
  if(false == l_dbus_proxy_get_property(pt_connected_proxy,
                                        "Connected", "b", &_con_status))
  {
    pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_ERR("Failed to get property \"Connected\"idx:%d \n",idx);
    _ret = ST_ERR;
    goto EXIT;
  }
  if(false == _con_status)
  {
    pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_WARN("device is not connected any more, idx:%d!\n", idx);
    _ret = ST_ERR;
    goto EXIT;
  }

  // Setup the timeout timer
  pt_con_ctr->pt_cond_var[idx].is_op_done = false;
  pt_con_ctr->pt_cond_var[idx].proxy = pt_connected_proxy;
  clock_gettime(CLOCK_MONOTONIC, &pt_con_ctr->pt_cond_var[idx].tv);
  pt_con_ctr->pt_cond_var[idx].tv.tv_sec = pt_con_ctr->pt_cond_var[idx].tv.tv_sec +
                        BT_CONNECTION_NEW_DISCONNECT_TIMEOUT_S;

  // Disconnect
  if(0 == l_dbus_proxy_method_call(pt_connected_proxy,
                                  "Disconnect",
                                  msg_setup_for_bt_disconnect,
                                  msg_reply_for_bt_disconnect,
                                  &pt_con_ctr->pt_cond_var[idx],
                                  destroy_for_bt_disconnect))
  {
    pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_ERR("Failed to call dbus method \"Disconnect\" idx:%d\n", idx);
    _ret = ST_ERR;
    goto EXIT;
  }
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);

  // Wait for "disconnection" done
  pthread_mutex_lock(&pt_con_ctr->pt_cond_var[idx].mtx);
  while(false == pt_con_ctr->pt_cond_var[idx].is_op_done)
  {
    if(pthread_cond_timedwait(&pt_con_ctr->pt_cond_var[idx].cond_var, &pt_con_ctr->pt_cond_var[idx].mtx,
                              &pt_con_ctr->pt_cond_var[idx].tv) == ETIMEDOUT)
    {
      DBG_LOG_ERR("Wait for disconnect operation reply timeout,idx:%d\n", idx);

      // Once timeout happened, force to clear connected_proxy and the
      // in-use flag
      pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
      bt_con_n_set_inuse(pt_con_ctr, pt_connected_proxy, false);
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
      pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
      // Make sure operate on the same connected_proxy we tried to disconnect
      // Do not udpate/use the con_proxy_idx here as it might be updated elsewhere
      if(pt_connected_proxy == pt_con_ctr->connected_proxy[idx])
      {
        pt_con_ctr->connected_proxy[idx] = NULL;
        CON_BIT_CLEAR(pt_con_ctr->con_status_bit,idx);
        // TODO. may be it's better to clear con_mac_addr[i] here. to be confirmed.
        memset(&pt_con_ctr->con_mac_addr[idx], 0, MAX_ID_STR_LENGTH);
      }
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
      // clear flag directly in case dis-connect timeout here
      pthread_mutex_lock(&pt_con_ctr->mtx_for_service_flg);
      pt_con_ctr->fg_is_services_resolved[idx] = false;
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_service_flg);
      DBG_LOG_WARN("service flag %d is cleared as dis-con timeout!\n", idx);
      _ret = ST_ERR;
      break;
    }
  }
  pt_con_ctr->pt_cond_var[idx].is_op_done = false;
  pthread_mutex_unlock(&pt_con_ctr->pt_cond_var[idx].mtx);
  pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
  pt_con_ctr->is_con_in_process = false;
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);

EXIT:
  DBG_LOG_INFO("disconnect idx%d exit, ret:%d!\n", idx, _ret);
  // delay 300ms if disconnect successfully to 'remove' the device at 'disconnect' event
  if(ST_OK == _ret)
    usleep(300000);
  bt_common_rm_devices(sys_get_proxy_adapter1(sys_get_mgt()), pt_connected_proxy);
  app_com_deinit_pthread_cond_var_ctr(&pt_con_ctr->pt_cond_var[idx]);
  // FIXME: give dbus some time(100ms) to release 'resource' to avoid being coredump-ed
  // to be verified.
  usleep(100000);
  return _ret;
#else
  if(NULL == pt_con_ctr->connected_proxy)
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_WARN("No device is being connected");
    _ret = ST_ERR;
    goto EXIT;
  }

  _pt_interface = l_dbus_proxy_get_interface(pt_con_ctr->connected_proxy);
  if(NULL == _pt_interface)
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_ERR("Failed to get the interface");
    _ret = ST_ERR;
    goto EXIT;
  }
  if(strcmp(_pt_interface, BT_IF_NM_DEVICE))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_ERR("This should NOT happen ...");
    _ret = ST_ERR;
    goto EXIT;
  }
  // Just double check the proxy is being connected
  if(false == l_dbus_proxy_get_property(pt_con_ctr->connected_proxy,
                                        "Connected", "b", &_con_status))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_ERR("Failed to get property \"Connected\"");
    _ret = ST_ERR;
    goto EXIT;
  }
  if(false == _con_status)
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_WARN("No device is being connected");
    _ret = ST_ERR;
    goto EXIT;
  }

  // Setup the timeout timer
  _cond_var.is_op_done = false;
  clock_gettime(CLOCK_MONOTONIC, &_cond_var.tv);
  _cond_var.tv.tv_sec = _cond_var.tv.tv_sec +
                        BT_CONNECTION_NEW_DISCONNECT_TIMEOUT_S;

  // Disconnect
  if(0 == l_dbus_proxy_method_call(pt_con_ctr->connected_proxy,
                                   "Disconnect",
                                   msg_setup_for_bt_disconnect,
                                   msg_reply_for_bt_disconnect,
                                   &_cond_var,
                                   destroy_for_bt_disconnect))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_ERR("Failed to call dbus method \"Disconnect\"");
    _ret = ST_ERR;
    goto EXIT;
  }
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);

  // Wait for "disconnection" done
  pthread_mutex_lock(&_cond_var.mtx);
  while(false == _cond_var.is_op_done)
  {
    if(pthread_cond_timedwait(&_cond_var.cond_var, &_cond_var.mtx,
                              &_cond_var.tv) == ETIMEDOUT)
    {
      DBG_LOG_ERR("Wait for disconnect operation reply timeout");

      // Once timeout happened, force to clear connected_proxy and the
      // in-use flag
      pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
      bt_con_n_set_inuse(pt_con_ctr, pt_con_ctr->connected_proxy, false);
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
      pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
      pt_con_ctr->connected_proxy = NULL;
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
      _ret = ST_ERR;
      break;
    }
  }
  pthread_mutex_unlock(&_cond_var.mtx);

EXIT:
  app_com_deinit_pthread_cond_var_ctr(&_cond_var);
  return _ret;
#endif
}
/**
 * @brief: To put the currentIdx-th element in the list to the last (i.e.,
 * list_sz-th) position, and the rest elements move up.
 */
static void
bt_connection_refresh_white_list(
  struct bt_addr *list,
  uint32_t list_sz,
  uint32_t currentIdx)
{
  struct bt_addr _tmp;
  uint32_t _cnt;

  uint8_t *_pt_nap = NULL, *_pt_lap = NULL;

  static uint8_t _refreshDelay = 0;

  _refreshDelay++;

  if((list_sz - 1) == currentIdx)
  {
    DBG_LOG_INFO("Do nothing, list_sz = %d, currentIdx = %d", list_sz,
                  currentIdx);
    // Do nothing because the current element has been in the last position
  }
  else
  {
    DBG_LOG_INFO("Refresh, list_sz = %d, currentIdx = %d", list_sz,
                  currentIdx);

    // if(_refreshDelay % 4 == 0)
    {
      memcpy(&_tmp, &(list[currentIdx]), sizeof(_tmp));
      for(_cnt = currentIdx; _cnt < (list_sz - 1); _cnt++)
      {
        memcpy(&(list[_cnt]), &(list[_cnt + 1]), sizeof(list[_cnt]));
      }
      memcpy(&(list[list_sz - 1]), &_tmp, sizeof(list[currentIdx]));
    }

    DBG_LOG_DEBUG("Refreshed list:");
    for(_cnt = 0; _cnt < list_sz; _cnt++)
    {
      _pt_nap = (uint8_t *)(&list[_cnt % MAX_BT_WHITE_LIST].nap);
      _pt_lap = (uint8_t *)(&list[_cnt % MAX_BT_WHITE_LIST].lap);
      if(sys_is_platform_big_endian())
      {
        DBG_LOG_DEBUG("%d: %02X:%02X:%02X:%02X:%02X:%02X", _cnt,
                      _pt_nap[0],
                      _pt_nap[1], list[_cnt % MAX_BT_WHITE_LIST].uap,
                      _pt_lap[0], _pt_lap[1], _pt_lap[2]);
      }
      else
      {
        DBG_LOG_DEBUG("%d: %02X:%02X:%02X:%02X:%02X:%02X", _cnt,
                      _pt_nap[1],
                      _pt_nap[0], list[_cnt % MAX_BT_WHITE_LIST].uap,
                      _pt_lap[2], _pt_lap[1], _pt_lap[0]);
      }
    }
  }
}
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
/**
 * @brief: The callback called by Disconnect DBUS method: destroy
 * @return: NULL
 */
static void
setup_for_bt_disconnect_timeout(void *user_data)
{
  DBG_LOG_INFO("setup_for_bt_disconnect_timeout is called!");
}
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
/**
 * @brief: The callback called by Disconnect DBUS method: destroy
 * @return: NULL
 */
static void
msg_reply_for_bt_disconnect_timeout(void *user_data)
{
  DBG_LOG_INFO("msg_reply_for_bt_disconnect_timeout is called!");
}
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
/**
 * @brief: The callback called by Disconnect DBUS method: destroy
 * @return: NULL
 */
static void
destroy_for_bt_disconnect_timeout(void *user_data)
{
  DBG_LOG_DEBUG("destroy_for_bt_disconnect_timeout is called!");
}
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
/**
 * @brief: The callback called by Disconnect DBUS method: destroy
 * @return: NULL
 */
static void
destroy_for_bt_connect(void *user_data)
{
  DBG_LOG_INFO("BT connect destroy is called!");
}
// typedef
// void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
// struct l_dbus_message *result, void *user_data);
/**
 * @brief: The callback called by Connect DBUS method: reply
 * @return: NULL
 */
static void
msg_reply_for_bt_connect(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data)
{
  struct con_controller *_pt_con_ctr = bt_con_n_get_con_ctr();
  app_management *_pt_app_mgt = sys_get_mgt();
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  uint8_t _idx = CONFIG_BLE_CONNECTION_MAX_NUM;
  uint8_t _con_mac_str[MAX_ID_STR_LENGTH] = {0};
#endif
  // To set the condition when BT connection operation has been done, no
  // matter failure or success
  if(user_data)
  {
    struct pthread_cond_var *_pt_cond_var = NULL;

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    struct con_method_parameter *_pt_con_method_param = 
      (struct con_method_parameter *)user_data;
    _pt_cond_var = _pt_con_method_param->_pt_cond_var;
    _idx = _pt_con_method_param->idx;
    memcpy((void *)_con_mac_str, 
           (void *)&_pt_con_method_param->con_mac_addr, 
           MAX_ID_STR_LENGTH);
    DBG_LOG_DEBUG("msg_reply_for_bt_connect called! idx:%d\n", _idx);
    if(_idx >= CONFIG_BLE_CONNECTION_MAX_NUM)
    {
      DBG_LOG_ERR("Unexpected idx %d to reply!\n", _idx);
      return;
    }
    
    if(NULL != _pt_cond_var)
    {
      if(CONFIG_COND_VAR_MAGIC_INIT != _pt_cond_var->magic)
      {
        DBG_LOG_ERR("signal for former con, skip it!\n");
#if (1)
        // handle a corner case here:
        // try to disconnect the 'timeout-ed' connection in case it's 'connect' method was timeout before
        // manually call 'disconnect' method here to disconnect current connection here;
        // otherwise, this connection would block the 'connect' method later.
        if(NULL == _pt_cond_var->proxy)
        { // proxy is not valid. have a basic check only
          return;
        }
        DBG_LOG_DEBUG("disconnect proxy@0x%llx in case con_var invalid!\n", _pt_cond_var->proxy);
        if(0 == l_dbus_proxy_method_call(_pt_cond_var->proxy,
                      "Disconnect",
                      setup_for_bt_disconnect_timeout,
                      msg_reply_for_bt_disconnect_timeout,
                      NULL,
                      destroy_for_bt_disconnect_timeout))
        {
          DBG_LOG_ERR("Failed to call dbus method \"Disonnect\" idx:%d\n", _idx);
        }
        else
        {
          DBG_LOG_INFO("call dbus method \"Disonnect\" idx:%d successful!\n", _idx);
        }
#endif
        return;
      }
#else
      _pt_cond_var = (struct pthread_cond_var *)user_data;
#endif
      if(pthread_mutex_trylock(&_pt_cond_var->mtx))
      {
        DBG_LOG_ERR(
          "Failed to acquire the lock (perhaps timeout has happened)");
        goto EXIT;
      }
      else
      {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        // check whether the signal is for current connection
        if((NULL != _pt_cond_var->proxy) && (proxy != _pt_cond_var->proxy))
        {
          DBG_LOG_ERR("signal for former con, skip it!\n");
          pthread_mutex_unlock(&_pt_cond_var->mtx);
          return;
        }
#endif
        _pt_cond_var->is_op_done = true;
        pthread_cond_signal(&_pt_cond_var->cond_var);
        pthread_mutex_unlock(&_pt_cond_var->mtx);
      }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    }
#endif
  }

  if(l_dbus_message_is_error(result))
  {
    const char *_pt_name = NULL, *_pt_text = NULL;

    // DBUS error happens
    l_dbus_message_get_error(result, &_pt_name, &_pt_text);
    DBG_LOG_ERR("Failed to connect, err_info: %s-%s", _pt_name,
                _pt_text);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    pthread_mutex_lock(&_pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_ERR("con err:proxy:@0x%llx idx:%d\n",
        _pt_con_ctr->connected_proxy[_idx],
        _idx);
    if(proxy == _pt_con_ctr->connected_proxy[_idx])
    {
      _pt_con_ctr->connected_proxy[_idx] = NULL;
      CON_BIT_CLEAR(_pt_con_ctr->con_status_bit,_idx);
      _pt_con_ctr->pt_cond_var[_idx].proxy = NULL; // sync with connected_proxy
      memset(&_pt_con_ctr->con_mac_addr[_idx], 0, MAX_ID_STR_LENGTH);
      DBG_LOG_DEBUG("proxy & idx match:%d\n",_idx);
    }
    else
    {
      // to be confirmed:
      DBG_LOG_ERR("proxy & idx mis-match:exp:%p,act:%p,idx:%d. do nothing!\n", 
                  _pt_con_ctr->connected_proxy[_idx], 
                  proxy, 
                  _idx);
    }
    _pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&_pt_con_ctr->mtx_for_connected_proxy);

#else
    pthread_mutex_lock(&_pt_con_ctr->mtx_for_connected_proxy);
    _pt_con_ctr->connected_proxy = NULL;
    _pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&_pt_con_ctr->mtx_for_connected_proxy);
#endif
    pthread_mutex_lock(&_pt_con_ctr->mtx_for_dev_queue);
    bt_con_n_set_inuse(_pt_con_ctr, proxy, false);
    pthread_mutex_unlock(&_pt_con_ctr->mtx_for_dev_queue);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    DBG_LOG_WARN("remove device:%d\n",_idx);
    bt_common_rm_devices(sys_get_proxy_adapter1(sys_get_mgt()), proxy);
#endif
    return;
  }

  // Connection is successful
  // Set "connection in progress" to false and attach the connected_proxy
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pthread_mutex_lock(&_pt_con_ctr->mtx_for_connected_proxy);
  // do nothing else so far as the update should be done by property_change() callback before
  DBG_LOG_INFO("con :exp proxy:@%p, act proxy:@%p idx:%d\n",
      _pt_con_ctr->connected_proxy[_idx], 
      proxy,
      _idx);
  bool _match_fg = false;
  if(NULL == _pt_con_ctr->connected_proxy[_idx])
  {
    _pt_con_ctr->connected_proxy[_idx] = proxy;
    CON_BIT_SET(_pt_con_ctr->con_status_bit,_idx);
    _pt_con_ctr->pt_cond_var[_idx].proxy = proxy; // sync with connected_proxy
    memcpy((void *)&_pt_con_ctr->con_mac_addr[_idx], 
    (void *)_con_mac_str, MAX_ID_STR_LENGTH);
    DBG_LOG_INFO("proxy is NULL. idx:%d, mac:%s\n",
                _idx, 
                _pt_con_ctr->con_mac_addr[_idx]);
    _match_fg = true;
  }
  else
  {
    if((proxy == _pt_con_ctr->connected_proxy[_idx]) && (proxy != NULL))
    {
      memcpy((void *)&_pt_con_ctr->con_mac_addr[_idx], 
        (void *)_con_mac_str, MAX_ID_STR_LENGTH);
      DBG_LOG_INFO("proxy matched. do nothing! idx:%d, mac:%s\n",
                  _idx, 
                  _pt_con_ctr->con_mac_addr[_idx]);
      _match_fg = true;
    }
    else
    { // proxy not match.
      // to be confirmed:
      DBG_LOG_ERR("proxy & idx mis-match:exp:%p,act:%p,idx:%d. %sdo nothing!\n", 
                  _pt_con_ctr->connected_proxy[_idx], 
                  proxy, 
                  _idx, 
                  _con_mac_str);
      _match_fg = false;
    }
  }

  // make sure clean stale MAC here
  if(_match_fg)
  {
    for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
    {
      if(i == _idx)
      {
        continue;
      }
      if(0 != strncmp((const char *)&_pt_con_ctr->con_mac_addr[i],
                      (const char *)_con_mac_str,
                      MAX_ID_STR_LENGTH))
      {
        continue;
      }
      if((NULL == _pt_con_ctr->connected_proxy[i]) &&
         (NULL == _pt_con_ctr->pt_cond_var[i].proxy))
      {
        memset(&_pt_con_ctr->con_mac_addr[i], 0, MAX_ID_STR_LENGTH);
        DBG_LOG_WARN("clear stale mac slot:%d, mac:%s\n", i, _con_mac_str);
      }
      else
      {
        DBG_LOG_ERR("dup active mac found: idx:%d and idx:%d, mac:%s\n",
                    i,
                    _idx,
                    _con_mac_str);
      }
    }
  }
  _pt_con_ctr->is_con_in_process = false;
  pthread_mutex_unlock(&_pt_con_ctr->mtx_for_connected_proxy);
  if(_match_fg)
  {
    pthread_mutex_lock(&_pt_con_ctr->mtx_for_dev_queue);
    bt_con_n_set_inuse(_pt_con_ctr, proxy, true);
    pthread_mutex_unlock(&_pt_con_ctr->mtx_for_dev_queue);
    DBG_LOG_INFO("BT connection is successful!");
  }else{
    pthread_mutex_lock(&_pt_con_ctr->mtx_for_dev_queue);
    bt_con_n_set_inuse(_pt_con_ctr, proxy, false);
    pthread_mutex_unlock(&_pt_con_ctr->mtx_for_dev_queue);
  }
#else
  pthread_mutex_lock(&_pt_con_ctr->mtx_for_connected_proxy);
  _pt_con_ctr->connected_proxy = proxy;
  _pt_con_ctr->is_con_in_process = false;
  pthread_mutex_unlock(&_pt_con_ctr->mtx_for_connected_proxy);
  pthread_mutex_lock(&_pt_con_ctr->mtx_for_dev_queue);
  bt_con_n_set_inuse(_pt_con_ctr, proxy, true);
  pthread_mutex_unlock(&_pt_con_ctr->mtx_for_dev_queue);

  DBG_LOG_INFO("BT connection is successful!");
#endif

EXIT:
  return;
}
// typedef
// bool (*l_queue_match_func_t) (const void *a, const void *b);
/**
 * @brief: The callback called by l_queue_find
 * @return: true if a's proxy property ("Address") is identical to string
 * pointed by b.
 */
static bool
q_match_dev_proxy_addrstr(
  const void *a,
  const void *b)
{
  bool _ret = false;
  struct l_dbus_proxy *_pt_proxy = ((struct dev_queue_ele *)a)->proxy;
  const char *_pt_addrbuf = (const char *)b;
  const char *_pt_proxy_addr = NULL;

  if(!l_dbus_proxy_get_property(_pt_proxy, "Address", "s",
                                &_pt_proxy_addr))
  {
    DBG_LOG_ERR("Failed to get proxy property");
    _ret = false;
    goto EXIT;
  }

  if(strcmp(_pt_proxy_addr, _pt_addrbuf) != 0)
  {
    // Do NOT match
    _ret = false;
    goto EXIT;
  }

  DBG_LOG_DEBUG("Matched Address match %s, %s", _pt_proxy_addr,
                _pt_addrbuf);
  _ret = true;
EXIT:
  return _ret;
}
/**
 * @brief Find whether there is an advertised device in the list. If found
 * and there is no other device is being connected, then connect the found
 * one; otherwise, do nothing. In other words, this routine assumes that
 * there is only ONE device can be connected at ONE moment.
 * @param pt_con_ctr: The pointer to the connect controller (a data
 * structure maintained by application)
 * @return ST_OK if everthing is OK.
 */
app_state_t
bt_con_n_connect_to_dev(struct con_controller *pt_con_ctr)
{
  app_state_t _ret = ST_OK;
#if (SKF_GW_CONFIG_MULTI_CONNECTION != 1)
  struct pthread_cond_var _cond_var;
#endif
  struct bt_addr _whitelist_array[MAX_BT_WHITE_LIST] = { 0 };
  uint8_t _btmac_strbuf[MAX_BT_WHITE_LIST][MAX_BT_ADDRSTR] = { 0 };
  uint8_t *_pt_nap = NULL, *_pt_lap = NULL;
  struct dev_queue_ele *_pt_proxy_ele = NULL;
  struct l_dbus_proxy *_pt_proxy_to_connect = NULL;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  u_int8_t tmp_idx = CONFIG_BLE_CONNECTION_MAX_NUM + 1; // init it to an 'illegal' one first
  struct con_method_parameter _con_method_param = {0};
#endif

  if(NULL == pt_con_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION != 1)
  _ret = app_com_init_pthread_cond_var_ctr(&_cond_var);
  if(ST_OK != _ret)
  {
    DBG_LOG_ERR(
      "Failed to initialize the condition variable with attributes");
    return _ret;
  }
#endif

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  // check whether: 1.connections are up to limit;
  //                2.there is connection setting up.
  bool free_flag = false;
  DBG_LOG_INFO("find proxy to connect\n");
  pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
  for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
  {
    // target proxy was already connected. skip further steps
#if (1)
    if(NULL == pt_con_ctr->connected_proxy[g_con_idx_coutnt % CONFIG_BLE_CONNECTION_MAX_NUM])
    {
      free_flag = true;
      tmp_idx = g_con_idx_coutnt % CONFIG_BLE_CONNECTION_MAX_NUM;
      DBG_LOG_INFO("free con proxy item found, idx:%d!\n", tmp_idx);
      g_con_idx_coutnt++;
      if(g_con_idx_coutnt > 4294967290)
        g_con_idx_coutnt = 0; // reset in case overflow (uint32)
      break;
    }
    g_con_idx_coutnt++;
#else
    if(NULL == pt_con_ctr->connected_proxy[i])
    {
      free_flag = true;
      tmp_idx = i;
      DBG_LOG_INFO("free con proxy item found, i:%d!\n", i);
      break;
    }
#endif
  }
  if((pt_con_ctr->is_con_in_process) || (!free_flag))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_WARN("is_con_in_process %d, free_flag:%d",
                pt_con_ctr->is_con_in_process,
                (uint8_t)free_flag);          
    _ret = ST_ERR;
    return _ret;
  }
  // set this flag here to make sure only 1 op is ongoing.
  pt_con_ctr->is_con_in_process = true;
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
#else
  // Check whether there is a device being connected
  if(pthread_mutex_trylock(&pt_con_ctr->mtx_for_connected_proxy))
  {
    DBG_LOG_ERR("Failed to acquire the lock");
    _ret = ST_ERR;
    goto EXIT;
  }
  if((pt_con_ctr->is_con_in_process) ||
     (NULL != pt_con_ctr->connected_proxy))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    DBG_LOG_WARN("is_con_in_process %d, connected_proxy@0x%llx",
                 pt_con_ctr->is_con_in_process,
                 pt_con_ctr->connected_proxy);
    _ret = ST_ERR;
    goto EXIT;
  }
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
#endif

  memset(_whitelist_array, 0,
         MAX_BT_WHITE_LIST * sizeof(struct bt_addr));
  pthread_mutex_lock(pt_con_ctr->mtx_for_white_list);
  sys_get_bt_white_list(_whitelist_array, MAX_BT_WHITE_LIST);

  // Prepare for finding (format MAC addresses)
  for(uint32_t i = 0; i < MAX_BT_WHITE_LIST; i++)
  {
    _pt_nap = (uint8_t *)(&pt_con_ctr->white_list[i].nap);
    _pt_lap = (uint8_t *)(&pt_con_ctr->white_list[i].lap);
    if(sys_is_platform_big_endian())
    {
      snprintf(_btmac_strbuf[i], MAX_BT_ADDRSTR,
               "%02X:%02X:%02X:%02X:%02X:%02X", _pt_nap[0], _pt_nap[1],
               pt_con_ctr->white_list[i % MAX_BT_WHITE_LIST].uap,
               _pt_lap[0], _pt_lap[1], _pt_lap[2]);
    }
    else
    {
      snprintf(_btmac_strbuf[i], MAX_BT_ADDRSTR,
               "%02X:%02X:%02X:%02X:%02X:%02X", _pt_nap[1], _pt_nap[0],
               pt_con_ctr->white_list[i % MAX_BT_WHITE_LIST].uap,
               _pt_lap[2], _pt_lap[1], _pt_lap[0]);
    }
    DBG_LOG_INFO("%s, vs: %02X\n",_btmac_strbuf[i], _whitelist_array[i].uap);
  }
  pthread_mutex_unlock(pt_con_ctr->mtx_for_white_list);

  // To find the deivce on white list and connect to it (if found)
  _pt_proxy_to_connect = NULL;
  _pt_proxy_ele = NULL;
  pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
  bt_con_n_dev_queue_periodic_refresh(pt_con_ctr);
  for(uint32_t i = 0; i < MAX_BT_WHITE_LIST; i++)
  {
    _pt_proxy_ele =
      (struct dev_queue_ele *)l_queue_find(
        pt_con_ctr->dev_queue,
        q_match_dev_proxy_addrstr,
        (const void *)_btmac_strbuf[i % MAX_BT_WHITE_LIST]);
    if(_pt_proxy_ele)
    {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      DBG_LOG_DEBUG("idx:%d!\n", i);
      // Skip candidates that already belong to an active slot even if BlueZ
      // has recreated the Device1 proxy for the same MAC address.
      bool _flg = false;
      pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
      for(uint8_t j=0;j<CONFIG_BLE_CONNECTION_MAX_NUM;j++)
      {
        if((NULL != pt_con_ctr->connected_proxy[j]) &&
          (0 == strncmp((const char *)_btmac_strbuf[i % MAX_BT_WHITE_LIST],
                        (const char *)&pt_con_ctr->con_mac_addr[j],
                        MAX_ID_STR_LENGTH)))
        {
          _flg = true;
          DBG_LOG_DEBUG("Dup mac is found! idx:%d, mac:%s\n",
                        j,
                        pt_con_ctr->con_mac_addr[j]);
          break;
        }
        if(_pt_proxy_ele->proxy == pt_con_ctr->connected_proxy[j])
        {
          _flg = true;
          break;
        }
      }
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
      if(_flg)
      {
        // skip the proxy already being connected.
        DBG_LOG_DEBUG("Dup proxy is found!\n");
        _pt_proxy_to_connect = NULL;
        _pt_proxy_ele = NULL;
      }
      else
      {
#endif
#if (BT_CONNECTION_NEW_MULTI_CONNECTION_ENABLED == 1)
      g_con_stat_data._con_proxy = _pt_proxy_ele->proxy;
      g_con_stat_data.start_con_time_ms = app_gen_n_get_time_ms();
#endif
      DBG_LOG_INFO("Device proxy %s (0x%llx) is found",
                    _btmac_strbuf[i % MAX_BT_WHITE_LIST],
                    _pt_proxy_ele->proxy);
      pthread_mutex_lock(pt_con_ctr->mtx_for_white_list);
      sys_get_bt_white_list(_whitelist_array, MAX_BT_WHITE_LIST);
      bt_connection_refresh_white_list(_whitelist_array,
                                       MAX_BT_WHITE_LIST,
                                       i);
      sys_set_bt_white_list(_whitelist_array, MAX_BT_WHITE_LIST);
      pthread_mutex_unlock(pt_con_ctr->mtx_for_white_list);
      _pt_proxy_to_connect = _pt_proxy_ele->proxy;
      break;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      }
#endif
    }
  }

  if((NULL == _pt_proxy_ele) || (NULL == _pt_proxy_to_connect))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
    DBG_LOG_WARN("Failed to find dev proxy to connect!\n");
    _ret = ST_ERR;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
    pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    return _ret;
#else
    goto EXIT;
#endif
  }
  // make sure each proxy has chance to be connected by putting unlock() here,
  // otherwise it might be flushed by proxies coming later
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
#if (BT_CONNECTION_NEW_CONNECTION_REQUIRED == 1)
  // The connection is in progress
  _ret = app_com_init_pthread_cond_var_ctr(&pt_con_ctr->pt_cond_var[tmp_idx]);
  if(ST_OK != _ret)
  {
    DBG_LOG_ERR(
      "Failed to init cond_var with attr\n");
    pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
    pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    return _ret;
  }
  // prepare for con_method_parameter
  g_con_method_param.idx = tmp_idx;
  g_con_method_param._pt_cond_var = &pt_con_ctr->pt_cond_var[tmp_idx];
  // TODO: fetch&store the mac addr of proxy.
  const char *_pt_addr = NULL;
  const char *_pt_str_pattern = "C4:BD:6A:"; // Prefix of SKF's BLE MAC
  memset(g_con_method_param.con_mac_addr, 0, MAX_ID_STR_LENGTH);
  if(false == l_dbus_proxy_get_property(_pt_proxy_to_connect,
                                        "Address", "s",
                                        &_pt_addr))
  {
    DBG_LOG_ERR("con:Failed to get addr property\n");
    _ret = ST_ERR;
    goto EXIT;
  }
  if(0 == strncmp(_pt_addr, _pt_str_pattern, strlen(_pt_str_pattern)))
  {
    memcpy((void *)g_con_method_param.con_mac_addr, 
           _pt_addr, 
           MAX_ID_STR_LENGTH);
    DBG_LOG_INFO("proxy to connect @0x%llx!idx:%d mac:%s\n", 
                 _pt_proxy_to_connect, 
                 tmp_idx, 
                 g_con_method_param.con_mac_addr);
  }
  else
  {
    DBG_LOG_INFO("Unknown proxy with MAC addr %s", _pt_addr);
    _ret = ST_ERR;
    goto EXIT;
  }
  // Setup the timeout timer
  pt_con_ctr->pt_cond_var[tmp_idx].is_op_done = false;
  pt_con_ctr->pt_cond_var[tmp_idx].proxy = _pt_proxy_to_connect;
  clock_gettime(CLOCK_MONOTONIC, &pt_con_ctr->pt_cond_var[tmp_idx].tv);
  pt_con_ctr->pt_cond_var[tmp_idx].tv.tv_sec = pt_con_ctr->pt_cond_var[tmp_idx].tv.tv_sec +
                        BT_CONNECTION_NEW_CONNECT_TIMEOUT_S;

  DBG_LOG_DEBUG("start to connect proxy@0x%llx,idx:%d!\n", _pt_proxy_to_connect, tmp_idx);
  if(0 == l_dbus_proxy_method_call(_pt_proxy_to_connect,
                "Connect",
                NULL,
                msg_reply_for_bt_connect,
                // &pt_con_ctr->pt_cond_var[tmp_idx],
                &g_con_method_param,
                destroy_for_bt_connect))
  {
    DBG_LOG_ERR("Failed to call dbus method \"Connect\" idx:%d\n", tmp_idx);
    _ret = ST_ERR;
    goto EXIT;
  }
  // Wait for "connection" done
  pthread_mutex_lock(&pt_con_ctr->pt_cond_var[tmp_idx].mtx);
  while(false == pt_con_ctr->pt_cond_var[tmp_idx].is_op_done)
  {
    if(pthread_cond_timedwait(&pt_con_ctr->pt_cond_var[tmp_idx].cond_var, &pt_con_ctr->pt_cond_var[tmp_idx].mtx,
                              &pt_con_ctr->pt_cond_var[tmp_idx].tv) == ETIMEDOUT)
    {
      pthread_mutex_unlock(&pt_con_ctr->pt_cond_var[tmp_idx].mtx);
      DBG_LOG_ERR("Wait for connect reply timeout, idx:%d\n", tmp_idx);
      g_fail_con_count++;
      DBG_LOG_ERR("connect goto exit! idx:%d\n", tmp_idx);
      _ret = ST_ERR;
      goto EXIT;
    }
  }
  pt_con_ctr->pt_cond_var[tmp_idx].is_op_done = false;
  pthread_mutex_unlock(&pt_con_ctr->pt_cond_var[tmp_idx].mtx);
  // don't de-init cond_var in case wait for signal successfull.
  _ret = ST_OK;
  bool tmp_flg = false;
  // de-init cond_var in case wait for signal failure.
  pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
  pt_con_ctr->is_con_in_process = false;
  if(pt_con_ctr->pt_cond_var[tmp_idx].proxy != _pt_proxy_to_connect)
  {
    DBG_LOG_DEBUG("pt_con_ctr->pt_cond_var[%d].proxy@0x%p,_pt_proxy_to_connect@0x%p!\n", 
    tmp_idx, pt_con_ctr->pt_cond_var[tmp_idx].proxy, _pt_proxy_to_connect);
    tmp_flg = true;
  }
  else
  {
    g_fail_con_count = 0;
  }
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
  if(tmp_flg)
  {
    DBG_LOG_ERR("status error fail to connect @%p,idx:%d\n", _pt_proxy_to_connect, tmp_idx);
    app_com_deinit_pthread_cond_var_ctr(&pt_con_ctr->pt_cond_var[tmp_idx]);  
  }
  return _ret;
#endif /* BT_CONNECTION_NEW_CONNECTION_REQUIRED == 1 */

EXIT:
  DBG_LOG_ERR("fail to connect @%p,idx:%d\n", _pt_proxy_to_connect, tmp_idx);
  pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
  pt_con_ctr->is_con_in_process = false;
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
  app_com_deinit_pthread_cond_var_ctr(&pt_con_ctr->pt_cond_var[tmp_idx]);
  return _ret;
#else
#if (BT_CONNECTION_NEW_CONNECTION_REQUIRED == 1)
  // The connection is in progress
  // Setup the timeout timer
  _cond_var.is_op_done = false;
  clock_gettime(CLOCK_MONOTONIC, &_cond_var.tv);
  _cond_var.tv.tv_sec = _cond_var.tv.tv_sec +
                        BT_CONNECTION_NEW_CONNECT_TIMEOUT_S;

  if(0 == l_dbus_proxy_method_call(_pt_proxy_to_connect,
                "Connect",
                NULL,
                msg_reply_for_bt_connect,
                &_cond_var,
                destroy_for_bt_connect))
  {
    // Failed to call method "Connect"
    pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
    pt_con_ctr->connected_proxy = NULL;
    pt_con_ctr->is_con_in_process = false;
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
    pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
    bt_con_n_set_inuse(pt_con_ctr, _pt_proxy_ele->proxy, false);
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);

    DBG_LOG_ERR("Failed to call dbus method \"Connect\"");
    _ret = ST_ERR;
    goto EXIT;
  }
  // Wait for "connection" done
  pthread_mutex_lock(&_cond_var.mtx);
  while(false == _cond_var.is_op_done)
  {
    if(pthread_cond_timedwait(&_cond_var.cond_var, &_cond_var.mtx,
                              &_cond_var.tv) == ETIMEDOUT)
    {
      DBG_LOG_ERR("Wait for connect operation reply timeout");

      // Once timeout happened, force to clear connected_proxy and the
      // in-use flag
      pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
      pt_con_ctr->connected_proxy = NULL;
      pt_con_ctr->is_con_in_process = false;
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
      pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
      bt_con_n_set_inuse(pt_con_ctr, _pt_proxy_ele->proxy, false);
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
      _ret = ST_ERR;
      break;
    }
  }
  pthread_mutex_unlock(&_cond_var.mtx);
#endif /* BT_CONNECTION_NEW_CONNECTION_REQUIRED == 1 */

EXIT:
  app_com_deinit_pthread_cond_var_ctr(&_cond_var);
  return _ret;
#endif
}
#if 0 // Deprecated (originally called by proxy_added)
/**
 * @brief To append the scanned device proxy to the queue managed by
 * connection controller
 * @param pt_con_ctr: Pointer to the connection controller
 * @param pt_dev_proxy: Pointer to the scanned device proxy
 * @return ST_OK if everything is OK
 */
app_state_t
bt_con_n_append_dev_proxy(
  struct con_controller *pt_con_ctr,
  const struct l_dbus_proxy *pt_dev_proxy)
{
  app_state_t _ret = ST_OK;
  const char *_pt_interface = NULL;
  const char *_pt_proxy_addr = NULL;
  struct dev_queue_ele _ele;
  const char *_pt_str_pattern = "C4:BD:6A:"; // Prefix of SKF's BLE MAC

  // Addr

  if((NULL == pt_con_ctr) || (NULL == pt_dev_proxy))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  _pt_interface = l_dbus_proxy_get_interface(pt_dev_proxy);
  if(NULL == _pt_interface)
  {
    DBG_LOG_ERR("Failed to get proxy interface");
    _ret = ST_ERR;
    return _ret;
  }
  if(strcmp(_pt_interface, "org.bluez.Device1"))
  {
    DBG_LOG_WARN("Dev proxy is expected");
    _ret = ST_ERR;
    return _ret;
  }

  // Double check the pattern
  if(!l_dbus_proxy_get_property(pt_dev_proxy, "Address", "s",
                                &_pt_proxy_addr))
  {
    DBG_LOG_ERR("Failed to get proxy property");
    _ret = ST_ERR;
    return _ret;
  }
  if(strncmp(_pt_proxy_addr, _pt_str_pattern, strlen(_pt_str_pattern)))
  {
    DBG_LOG_INFO("Unknown proxy with MAC addr %s", _pt_proxy_addr);
    _ret = ST_ERR;
    return _ret;
  }

  pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
  _ele.timestamp = time(NULL);
  _ele.in_use = false;
  _ele.proxy = pt_proxy;
  if(!l_queue_push_tail(pt_con_ctr->dev_queue, (void *)&_ele))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
    DBG_LOG_ERR("Failed to put scanned device proxy on queue");
    _ret = ST_ERR;
    return _ret;
  }
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);

  DBG_LOG_INFO("Dev %s has been added to the queue", _pt_proxy_addr);

  return _ret;
}
#else // Deprecated (originally called by proxy_added)
/**
 * @brief Handle proxy "RSSI" changed
 * @param pt_con_ctr: Pointer to the connection controller
 * @param pt_proxy: Pointer to changed proxy
 * @param pt_name: The changed property
 * @param pt_msg: dbus message
 * @param user_data: user data (not used)
 * @return ST_OK if everything is OK
 */
app_state_t
bt_con_n_append_dev_proxy(
  struct con_controller *pt_con_ctr,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_name,
  struct l_dbus_message *pt_msg,
  void *user_data)
{
  app_state_t _ret = ST_OK;
  const char *_pt_interface = NULL;
  const char *_pt_proxy_addr = NULL;
  const char *_pt_str_pattern = "C4:BD:6A:"; // Prefix of SKF's BLE MAC
  struct dev_queue_ele *_pt_tmp_ele = NULL;
  struct dev_queue_ele *_pt_ele = NULL;
  app_management *_pt_app_mgt = sys_get_mgt();

  if(!strcmp(pt_name, "RSSI"))
  {
    // Add the event to the queue (pt_adv_scanning_event_queue)
    // and trigger @p cond_val_adv_scanning_ctr
    pthread_mutex_lock(&_pt_app_mgt->mtx_adv_scanning_ctr);
    l_queue_push_tail(_pt_app_mgt->pt_adv_scanning_event_queue,
                      (void *)BT_EVENT_RSSI_CHANGED);
    pthread_cond_signal(&_pt_app_mgt->cond_val_adv_scanning_ctr);
    pthread_mutex_unlock(&_pt_app_mgt->mtx_adv_scanning_ctr);

    _pt_interface = l_dbus_proxy_get_interface(pt_proxy);
    if(NULL == _pt_interface)
    {
      DBG_LOG_ERR("Failed to get proxy interface");
      _ret = ST_ERR;
      return _ret;
    }
    if(strcmp(_pt_interface, "org.bluez.Device1"))
    {
      DBG_LOG_WARN("Dev proxy is expected");
      _ret = ST_ERR;
      return _ret;
    }

    // Double check the pattern
    if(!l_dbus_proxy_get_property(pt_proxy, "Address", "s",
                                  &_pt_proxy_addr))
    {
      DBG_LOG_ERR("Failed to get proxy property");
      _ret = ST_ERR;
      return _ret;
    }
    if(strncmp(_pt_proxy_addr, _pt_str_pattern, strlen(_pt_str_pattern)))
    {
      DBG_LOG_INFO("Unknown proxy with MAC addr %s", _pt_proxy_addr);
      _ret = ST_ERR;
      return _ret;
    }

    _pt_tmp_ele = NULL;
    pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
    _pt_tmp_ele = (struct dev_queue_ele *)l_queue_find(
      pt_con_ctr->dev_queue,
      q_match_dev_proxy_addrstr,
      (const void *)_pt_proxy_addr);
      if(_pt_tmp_ele)
    {
      _pt_tmp_ele->timestamp = time(NULL);
      _pt_tmp_ele->in_use = false;
      _pt_tmp_ele->proxy = pt_proxy;
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);

      DBG_LOG_INFO("Dev %s (0x%llx) has been in the queue", _pt_proxy_addr,
                   pt_proxy);
    }
    else
    {
      _pt_ele = l_malloc(sizeof(struct dev_queue_ele));
      if(_pt_ele == NULL)
      {
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
        DBG_LOG_ERR("Failed to malloc");
        _ret = ST_ERR;
        return _ret;
      }
      _pt_ele->timestamp = time(NULL);
      _pt_ele->in_use = false;
      _pt_ele->proxy = pt_proxy;
      if(!l_queue_push_tail(pt_con_ctr->dev_queue, (void *)_pt_ele))
      {
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
        DBG_LOG_ERR("Failed to put scanned device proxy on queue");
        _ret = ST_ERR;
        return _ret;
      }
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);

      DBG_LOG_INFO("Dev %s (0x%llx) has been added to the queue",
                   _pt_proxy_addr, pt_proxy);
    }
  }

  return _ret;
}
#endif
/**
 * @brief To append the discovered characteristics proxy to the queue
 * managed by connection controller
 * @param pt_con_ctr: Pointer to the connection controller
 * @param pt_dev_proxy: Pointer to the discovered characteristics proxy
 * @return ST_OK if everything is OK
 */
app_state_t
bt_con_n_append_chr_proxy_to_queue(
  struct con_controller *pt_con_ctr,
  const struct l_dbus_proxy *pt_chr_proxy)
{
  app_state_t _ret = ST_OK;
  const char *_pt_interface = NULL;
  char *_pt_proxy_uuid = NULL;

  if((NULL == pt_con_ctr) || (NULL == pt_chr_proxy))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  _pt_interface = l_dbus_proxy_get_interface(pt_chr_proxy);
  if(NULL == _pt_interface)
  {
    DBG_LOG_ERR("Failed to get proxy interface");
    _ret = ST_ERR;
    return _ret;
  }
  if(strcmp(_pt_interface, "org.bluez.GattCharacteristic1"))
  {
    DBG_LOG_WARN("Chr proxy is expected");
    _ret = ST_ERR;
    return _ret;
  }

  if(!l_dbus_proxy_get_property(pt_chr_proxy, "UUID", "s",
                                &_pt_proxy_uuid))
  {
    DBG_LOG_ERR("Failed to get proxy property");
    _ret = ST_ERR;
    return _ret;
  }

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  if(BT_ROLE_CLIENT == sys_get_mgt()->bt_role)  //client role
  {
    struct l_dbus_proxy *tmp_rx_proxy = NULL;
    struct l_dbus_proxy *tmp_tx_proxy = NULL;
    uint8_t tmp_idx = CONFIG_BLE_CONNECTION_MAX_NUM;
    bool free_flag = false;
    bool match_flag = false;
    bool uart_chr_flag = false;
    if(0 == strcmp(_pt_proxy_uuid, (const char *)NORDIC_UART_CHARAC_RX_UUID))
    {
      DBG_LOG_INFO("Chr UART RX: %s is found", _pt_proxy_uuid);
      tmp_rx_proxy = pt_chr_proxy;
      uart_chr_flag = true;
    }
    else if(0 == strcmp(_pt_proxy_uuid, (const char *)NORDIC_UART_CHARAC_TX_UUID))
    {
      DBG_LOG_INFO("Chr UART TX: %s is found", _pt_proxy_uuid);
      tmp_tx_proxy = pt_chr_proxy;
      uart_chr_flag = true;
    }
    if(ST_OK != bt_con_n_connect_idx_fetch(pt_con_ctr,
                                            pt_chr_proxy,
                                            &tmp_idx))
    {
      DBG_LOG_ERR("Failed to get index\n");
      return ST_ERR;
    }
    pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
    if(NULL != tmp_rx_proxy)  // uart_rx_proxy
    {
      pt_con_ctr->proxy_uart_chr_rx[tmp_idx] = tmp_rx_proxy;
      DBG_LOG_INFO("rx_pro:@0x%llx, idx:%d\n", pt_con_ctr->proxy_uart_chr_rx[tmp_idx], tmp_idx);
    }
    if (NULL != tmp_tx_proxy)    // uart_tx_proxy
    {
      pt_con_ctr->proxy_uart_chr_tx[tmp_idx] = tmp_tx_proxy;
      DBG_LOG_INFO("tx_pro:@0x%llx, idx:%d\n", pt_con_ctr->proxy_uart_chr_tx[tmp_idx], tmp_idx);
    }
    
    if((tmp_idx >= CONFIG_BLE_CONNECTION_MAX_NUM) ||
      (!l_queue_push_tail(pt_con_ctr->chr_queue[tmp_idx], (void *)pt_chr_proxy)))
    {
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
      DBG_LOG_ERR("Failed to put characteristics proxy on queue");
      _ret = ST_ERR;
      return _ret;
    }
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
  }
  else if (BT_ROLE_SERVER == sys_get_mgt()->bt_role) // server role
  {
    if(0 == strcmp(_pt_proxy_uuid, (const char *)NORDIC_UART_CHARAC_RX_UUID))
    {
      DBG_LOG_INFO("Chr UART RX: %s is found svr!\n", _pt_proxy_uuid);
      pt_con_ctr->svr_proxy_uart_chr_rx = pt_chr_proxy;
    }
    else if(0 == strcmp(_pt_proxy_uuid, (const char *)NORDIC_UART_CHARAC_TX_UUID))
    {
      DBG_LOG_INFO("Chr UART TX: %s is found svr!\n", _pt_proxy_uuid);
      pt_con_ctr->svr_proxy_uart_chr_tx = pt_chr_proxy;
    }

    pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
    if(!l_queue_push_tail(pt_con_ctr->svr_chr_queue, (void *)pt_chr_proxy))
    {
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
      DBG_LOG_ERR("Failed to put characteristics proxy on queue in server role\n");
      _ret = ST_ERR;
      return _ret;
    }
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
  }
#else
  if(0 == strcmp(_pt_proxy_uuid, (const char *)NORDIC_UART_CHARAC_RX_UUID))
  {
    DBG_LOG_INFO("Chr UART RX: %s is found", _pt_proxy_uuid);
    pt_con_ctr->proxy_uart_chr_rx = pt_chr_proxy;
  }
  else if(0 == strcmp(_pt_proxy_uuid, (const char *)NORDIC_UART_CHARAC_TX_UUID))
  {
    DBG_LOG_INFO("Chr UART TX: %s is found", _pt_proxy_uuid);
    pt_con_ctr->proxy_uart_chr_tx = pt_chr_proxy;
  }

  pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
  if(!l_queue_push_tail(pt_con_ctr->chr_queue, (void *)pt_chr_proxy))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
    DBG_LOG_ERR("Failed to put characteristics proxy on queue");
    _ret = ST_ERR;
    return _ret;
  }
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
#endif
  
  DBG_LOG_INFO("Chr %s has been added to the queue", _pt_proxy_uuid);

  return _ret;
}
// Called by property_changed
// typedef
// bool (*l_queue_match_func_t) (const void *a, const void *b);
/**
 * @brief: The callback called by l_queue_remove_if or l_queue_find
 * @return: true if pointers a and b are identical
 */
static bool
q_match_dev_proxy_compare(
  const void *a,
  const void *b)
{
  bool tpret = false;
  struct l_dbus_proxy *pta = ((struct dev_queue_ele *)a)->proxy;
  struct l_dbus_proxy *ptb = (struct l_dbus_proxy *)b;

  if(pta == ptb)
  {
    tpret = true;
  }
  return tpret;
}
/**
 * @brief To remove the device which does not exist from the queue managed
 * by connection controller
 * @param pt_con_ctr: Pointer to the connection controller
 * @param pt_dev_proxy: Pointer to the device proxy to be removed
 * @return ST_OK if everything is OK
 */
app_state_t
bt_con_n_remove_dev_proxy(
  struct con_controller *pt_con_ctr,
  const struct l_dbus_proxy *pt_dev_proxy)
{
  app_state_t _ret = ST_OK;
  const char *_pt_interface = NULL;
  const char *_pt_proxy_addr = NULL;
  struct dev_queue_ele *_pt_proxy_ele = NULL;

  if((NULL == pt_con_ctr) || (NULL == pt_dev_proxy))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  _pt_interface = l_dbus_proxy_get_interface(pt_dev_proxy);
  if(NULL == _pt_interface)
  {
    DBG_LOG_ERR("Failed to get proxy interface");
    _ret = ST_ERR;
    return _ret;
  }
  if(strcmp(_pt_interface, "org.bluez.Device1"))
  {
    DBG_LOG_WARN("Dev proxy is expected");
    _ret = ST_ERR;
    return _ret;
  }

  pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
  _pt_proxy_ele = l_queue_remove_if(pt_con_ctr->dev_queue,
                                    q_match_dev_proxy_compare,
                                    (const void *)pt_dev_proxy);
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);

  if(_pt_proxy_ele != NULL)
  {
    if(_pt_proxy_ele->proxy != NULL)
    {
      if(!l_dbus_proxy_get_property(_pt_proxy_ele->proxy, "Address", "s",
                                    &_pt_proxy_addr))
      {
        l_free(_pt_proxy_ele);
        DBG_LOG_ERR("Failed to get proxy property");
        _ret = ST_ERR;
        return _ret;
      }
    }
    DBG_LOG_INFO("Dev %s has been removed from the queue", _pt_proxy_addr);
    l_free(_pt_proxy_ele);
  }

  return _ret;
}
// Called by property_removed
// typedef
// bool (*l_queue_match_func_t) (const void *a, const void *b);
/**
 * @brief: The callback called by l_queue_remove_if or l_queue_find
 * @return: true if pointers a and b are identical
 */
static bool
q_match_proxy_addr_compare(
  const void *a,
  const void *b)
{
  bool tpret = false;
  const struct l_dbus_proxy *pta = (const struct l_dbus_proxy *)a;
  const struct l_dbus_proxy *ptb = (const struct l_dbus_proxy *)b;

  if((NULL == pta) || (NULL == ptb))
  {
    DBG_LOG_ERR("unexpected NULL");
    tpret = false;
    return tpret;
  }

  if(pta == ptb)
  {
    tpret = true;
  }
  return tpret;
}
/**    pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
    _pt_proxy_ele = l_queue_remove_if(pt_con_ctr->chr_queue,
                                      q_match_proxy_addr_compare,
                                      (const void *)pt_chr_proxy);
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);

    if(_pt_proxy_ele != NULL)
    {
      if(_pt_proxy_ele == pt_con_ctr->proxy_uart_chr_rx)
      {
        DBG_LOG_INFO("Chr UART RX would be detached");
        pt_con_ctr->proxy_uart_chr_rx = NULL;
      }
      else if(_pt_proxy_ele == pt_con_ctr->proxy_uart_chr_tx)
      {
        DBG_LOG_INFO("Chr UART TX would be detached");
        pt_con_ctr->proxy_uart_chr_tx = NULL;
      }
      DBG_LOG_INFO("Chr has been removed from the queue");
    }

 * @brief To remove the characteristics which does not exist from the queue
 * managed by connection controller
 * @param pt_con_ctr: Pointer to the connection controller
 * @param pt_chr_proxy: Pointer to the characteristics proxy to be removed
 * @return ST_OK if everything is OK
 */
app_state_t
bt_con_n_remove_chr_proxy_from_queue(
  struct con_controller *pt_con_ctr,
  const struct l_dbus_proxy *pt_chr_proxy)
{
  app_state_t _ret = ST_OK;
  const char *_pt_interface = NULL;
  struct l_dbus_proxy *_pt_proxy_ele = NULL;

  if((NULL == pt_con_ctr) || (NULL == pt_chr_proxy))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  _pt_interface = l_dbus_proxy_get_interface(pt_chr_proxy);
  if(NULL == _pt_interface)
  {
    DBG_LOG_ERR("Failed to get proxy interface");
    _ret = ST_ERR;
    return _ret;
  }
  if(strcmp(_pt_interface, "org.bluez.GattCharacteristic1"))
  {
    DBG_LOG_WARN("Chr proxy is expected");
    _ret = ST_ERR;
    return _ret;
  }

  if(BT_ROLE_CLIENT == sys_get_mgt()->bt_role)  //client role
  {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
    for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
    {
      if(NULL != pt_con_ctr->chr_queue[i])
      {
        _pt_proxy_ele = l_queue_remove_if(pt_con_ctr->chr_queue[i],
                                          q_match_proxy_addr_compare,
                                          (const void *)pt_chr_proxy);

        if(_pt_proxy_ele != NULL)
        {
          if(_pt_proxy_ele == pt_con_ctr->proxy_uart_chr_rx[i])
          {
            DBG_LOG_DEBUG("Chr RX %d @0x%llx detached\n", i, _pt_proxy_ele);
            pt_con_ctr->proxy_uart_chr_rx[i] = NULL;
          }
          else if(_pt_proxy_ele == pt_con_ctr->proxy_uart_chr_tx[i])
          {
            DBG_LOG_DEBUG("Chr TX %d @0x%llx detached\n", i, _pt_proxy_ele);
            pt_con_ctr->proxy_uart_chr_tx[i] = NULL;
          }
          else
          {
            DBG_LOG_DEBUG("Chr %d @0x%llx is removed\n", i, _pt_proxy_ele);
          }
          break;
        }
      }
    }
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
#else    
    pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
    _pt_proxy_ele = l_queue_remove_if(pt_con_ctr->chr_queue,
                                      q_match_proxy_addr_compare,
                                      (const void *)pt_chr_proxy);
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);

    if(_pt_proxy_ele != NULL)
    {
      if(_pt_proxy_ele == pt_con_ctr->proxy_uart_chr_rx)
      {
        DBG_LOG_INFO("Chr UART RX would be detached");
        pt_con_ctr->proxy_uart_chr_rx = NULL;
      }
      else if(_pt_proxy_ele == pt_con_ctr->proxy_uart_chr_tx)
      {
        DBG_LOG_INFO("Chr UART TX would be detached");
        pt_con_ctr->proxy_uart_chr_tx = NULL;
      }
      DBG_LOG_INFO("Chr has been removed from the queue");
    }

    pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
    _pt_proxy_ele = l_queue_remove_if(pt_con_ctr->chr_queue,
                                      q_match_proxy_addr_compare,
                                      (const void *)pt_chr_proxy);
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);

    if(_pt_proxy_ele != NULL)
    {
      if(_pt_proxy_ele == pt_con_ctr->proxy_uart_chr_rx)
      {
        DBG_LOG_INFO("Chr UART RX would be detached");
        pt_con_ctr->proxy_uart_chr_rx = NULL;
      }
      else if(_pt_proxy_ele == pt_con_ctr->proxy_uart_chr_tx)
      {
        DBG_LOG_INFO("Chr UART TX would be detached");
        pt_con_ctr->proxy_uart_chr_tx = NULL;
      }
      DBG_LOG_INFO("Chr has been removed from the queue");
    }
#endif
  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  else if (BT_ROLE_SERVER == sys_get_mgt()->bt_role) // server role
  {
    pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
    _pt_proxy_ele = l_queue_remove_if(pt_con_ctr->svr_chr_queue,
                                      q_match_proxy_addr_compare,
                                      (const void *)pt_chr_proxy);
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);

    if(_pt_proxy_ele != NULL)
    {
      if(_pt_proxy_ele == pt_con_ctr->svr_proxy_uart_chr_rx)
      {
        DBG_LOG_INFO("Chr UART RX would be detached in server role\n");
        pt_con_ctr->svr_proxy_uart_chr_rx = NULL;
      }
      else if(_pt_proxy_ele == pt_con_ctr->svr_proxy_uart_chr_tx)
      {
        DBG_LOG_INFO("Chr UART TX would be detached in server role\n");
        pt_con_ctr->svr_proxy_uart_chr_tx = NULL;
      }
      DBG_LOG_INFO("Chr has been removed from the queue in server role\n");
    }
  }
#else
  // Add BT_EVENT_DISCONNECTION to the queue. for Both client role & server role
  // skip it if proxy is not in array
  pthread_mutex_lock(&sys_get_mgt()->mtx_adv_scanning_ctr);
  l_queue_push_tail(sys_get_mgt()->pt_adv_scanning_event_queue,
                    (void *)BT_EVENT_DISCONNECTION);
  pthread_cond_signal(&sys_get_mgt()->cond_val_adv_scanning_ctr);
  pthread_mutex_unlock(&sys_get_mgt()->mtx_adv_scanning_ctr);
#endif  
  return _ret;
}
/**
 * @brief Handle proxy "Connected" changed
 * @param pt_con_ctr: Pointer to the connection controller
 * @param pt_proxy: Pointer to changed proxy
 * @param pt_name: The changed property
 * @param pt_msg: dbus message
 * @param user_data: user data (not used)
 * @return ST_OK if everything is OK
 */
app_state_t
bt_con_n_proxy_property_changed(
  struct con_controller *pt_con_ctr,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_name,
  struct l_dbus_message *pt_msg,
  void *user_data)
{
  app_state_t _ret = ST_OK;
  bool tp_connected = false;
  app_management *_pt_app_mgt = sys_get_mgt();

  if(!strcmp(pt_name, "Connected"))
  {
    if(!l_dbus_message_get_arguments(pt_msg, "b", &tp_connected))
    {
      DBG_LOG_ERR("Failed to get property \"Connected\"");
      _ret = ST_ERR;
      return _ret;
    }

    if(tp_connected)
    {
      // connection
      // Set "connection in progress" to false and attach the
      // connected_proxy
      DBG_LOG_INFO("Connected");
      if(_pt_app_mgt->bt_role == BT_ROLE_CLIENT)
      {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        // update con_controller on connection
        bool free_flag = false;
        bool match_flag = false;
        u_int8_t tmp_idx = CONFIG_BLE_CONNECTION_MAX_NUM + 1; // init it to an 'illegal' one first
        pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
        for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
        {
          if(pt_proxy == pt_con_ctr->pt_cond_var[i].proxy)
          {
            pt_con_ctr->connected_proxy[i] = pt_proxy;
            pt_con_ctr->con_proxy_idx = i;
            CON_BIT_SET(pt_con_ctr->con_status_bit,i);
            match_flag = true;
            tmp_idx = i;
            break;
          }
        }
        if(match_flag)
        {
          DBG_LOG_INFO("con:match_flag:%d,proxy@0x%llx\n", (uint8_t)match_flag, pt_proxy);
        } 
        else
        {
          DBG_LOG_WARN("con:no matched proxy@0x%llx skip it!\n", pt_proxy);
          _ret = ST_ERR;
        }
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
        // update app_management on connection
        pthread_mutex_lock(&_pt_app_mgt->mtx_for_connected_proxy);
        if(match_flag)
        {
          _pt_app_mgt->hld_con_proxy_idx = tmp_idx;
          _pt_app_mgt->hld_connected_proxy[tmp_idx] = pt_proxy;
          CON_BIT_SET(_pt_app_mgt->hld_con_status_bit,tmp_idx);
        }
        if(match_flag)
        {
          DBG_LOG_INFO("match_flag:%d,proxy@0x%llx\n", (uint8_t)match_flag, pt_proxy);
        }
        else
        {
          DBG_LOG_WARN("app:no matched proxy@0x%llx skip it!\n", pt_proxy);
          _ret = ST_ERR;
        }
        _pt_app_mgt->is_con_in_progress_flg = false;
        pthread_mutex_unlock(&_pt_app_mgt->mtx_for_connected_proxy);
        if(ST_OK == _ret)
        {
          pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
          bt_con_n_set_inuse(pt_con_ctr, pt_proxy, true);
          pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
          // Add BT_EVENT_CONNECTION to the queue
          pthread_mutex_lock(&_pt_app_mgt->mtx_adv_scanning_ctr);
          l_queue_push_tail(_pt_app_mgt->pt_adv_scanning_event_queue,
                            (void *)BT_EVENT_CONNECTION);
          pthread_cond_signal(&_pt_app_mgt->cond_val_adv_scanning_ctr);
          pthread_mutex_unlock(&_pt_app_mgt->mtx_adv_scanning_ctr);
        }
#else
        pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
        pt_con_ctr->connected_proxy = pt_proxy;
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
        pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
        bt_con_n_set_inuse(pt_con_ctr, pt_proxy, true);
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
        // Add BT_EVENT_CONNECTION to the queue
        pthread_mutex_lock(&_pt_app_mgt->mtx_adv_scanning_ctr);
        l_queue_push_tail(_pt_app_mgt->pt_adv_scanning_event_queue,
                          (void *)BT_EVENT_CONNECTION);
        pthread_cond_signal(&_pt_app_mgt->cond_val_adv_scanning_ctr);
        pthread_mutex_unlock(&_pt_app_mgt->mtx_adv_scanning_ctr);
#endif
      }
      else if(_pt_app_mgt->bt_role == BT_ROLE_SERVER)
      {
        sys_add_proxy(_pt_app_mgt, pt_proxy);
        // Add BT_EVENT_CONNECTION to the queue
        pthread_mutex_lock(&_pt_app_mgt->mtx_adv_scanning_ctr);
        l_queue_push_tail(_pt_app_mgt->pt_adv_scanning_event_queue,
                          (void *)BT_EVENT_CONNECTION);
        pthread_cond_signal(&_pt_app_mgt->cond_val_adv_scanning_ctr);
        pthread_mutex_unlock(&_pt_app_mgt->mtx_adv_scanning_ctr);
      }
    }
    else
    {
      // disconnection
      DBG_LOG_INFO("Disconnected\n");
      if(_pt_app_mgt->bt_role == BT_ROLE_CLIENT)
      {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        bool match_flag = false;
        u_int8_t dis_idx = CONFIG_BLE_CONNECTION_MAX_NUM; // init it to an 'illegal' one first
        // update con_controller on dis-connection
        pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
        for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
        {
          if(pt_proxy == pt_con_ctr->connected_proxy[i])
          {
            dis_idx = i;
            bt_common_rm_devices(sys_get_proxy_adapter1(sys_get_mgt()), pt_con_ctr->connected_proxy[dis_idx]);
            match_flag = true;
            if(i != pt_con_ctr->con_proxy_idx)
            {
              DBG_LOG_WARN("--wrong idx:i%d:%d\n", i, pt_con_ctr->con_proxy_idx);
              // update the index here enen if it may be updted to other value
              pt_con_ctr->con_proxy_idx = i;
            }
            pt_con_ctr->connected_proxy[i] = NULL;
            CON_BIT_CLEAR(pt_con_ctr->con_status_bit, i);
            // clear mac addr on disconnection
            memset(&pt_con_ctr->con_mac_addr[i], 0, MAX_ID_STR_LENGTH);
            break;
          }
        }
        if(!match_flag)
        {
          // Notes(to be confirmed):
          // 1. try to 'remove' device even if it's not in connected_proxy list we recorded
          //    as it may be the cause of 'sensor may not be connected any more after connected' issue
          DBG_LOG_WARN("--proxy @0x%llx no found, do nothing!\n", pt_proxy);
          // bt_common_rm_devices(sys_get_proxy_adapter1(sys_get_mgt()), pt_proxy);
          _ret = ST_ERR;
        }
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
        // clear flag directly in case it's dis-connected here
        pthread_mutex_lock(&pt_con_ctr->mtx_for_service_flg);
        if(dis_idx < CONFIG_BLE_CONNECTION_MAX_NUM)
        {
          pt_con_ctr->fg_is_services_resolved[dis_idx] = false;
          DBG_LOG_WARN("service flag %d is cleared as dis-con!\n", dis_idx);
        }
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_service_flg);
        match_flag = false;
        // update app_management on dis-connection
        pthread_mutex_lock(&_pt_app_mgt->mtx_for_connected_proxy);
        for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
        {
          if(pt_proxy == _pt_app_mgt->hld_connected_proxy[i])
          {
            match_flag = true;
            if(i != _pt_app_mgt->hld_con_proxy_idx)
            {
              DBG_LOG_WARN("--wrong idx:i%d:%d\n", i, _pt_app_mgt->hld_con_proxy_idx);
              // do not update the index here as it may be updted to other value
              // _pt_app_mgt->hld_con_proxy_idx = i;
            }
            _pt_app_mgt->hld_connected_proxy[i] = NULL;
            CON_BIT_CLEAR(_pt_app_mgt->hld_con_status_bit, i);
            pt_con_ctr->pt_cond_var[i].proxy = NULL;
            break;
          }
        }
        if(!match_flag)
        {
          DBG_LOG_WARN("--proxy @0x%llx no found, do nothing!\n", pt_proxy);
          _ret = ST_ERR;
        }
        _pt_app_mgt->is_con_in_progress_flg = false;
        pthread_mutex_unlock(&_pt_app_mgt->mtx_for_connected_proxy);
      // Add BT_EVENT_DISCONNECTION to the queue
      // skip it if proxy is not in array
        if(ST_OK == _ret)
        {
          pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
          bt_con_n_set_inuse(pt_con_ctr, pt_proxy, false);
          pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
          pthread_mutex_lock(&_pt_app_mgt->mtx_adv_scanning_ctr);
          l_queue_push_tail(_pt_app_mgt->pt_adv_scanning_event_queue,
                            (void *)BT_EVENT_DISCONNECTION);
          pthread_cond_signal(&_pt_app_mgt->cond_val_adv_scanning_ctr);
          pthread_mutex_unlock(&_pt_app_mgt->mtx_adv_scanning_ctr);
        }
        // for multi-con case, return here as it already sends BT_EVENT_DISCONNECTION evt above
        return _ret;
#else
        pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
        bt_common_rm_devices(sys_get_proxy_adapter1(sys_get_mgt()), pt_con_ctr->connected_proxy);
        pt_con_ctr->connected_proxy = NULL;
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
        pthread_mutex_lock(&pt_con_ctr->mtx_for_dev_queue);
        bt_con_n_set_inuse(pt_con_ctr, pt_proxy, false);
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_dev_queue);
#endif
      }
      else if(_pt_app_mgt->bt_role == BT_ROLE_SERVER)
      {
        // to be confirmed: whether to clear hld_server_connected_proxy here or not.
        // as it may be used for clean-up
        pthread_mutex_lock(&_pt_app_mgt->mtx_for_connected_proxy);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        _pt_app_mgt->hld_server_connected_proxy = NULL;
#else
        _pt_app_mgt->hld_connected_proxy = NULL;
#endif
        pthread_mutex_unlock(&_pt_app_mgt->mtx_for_connected_proxy);
        // Add BT_EVENT_DISCONNECTION to the queue. for Both client role & server role
        // skip it if proxy is not in array  
        pthread_mutex_lock(&_pt_app_mgt->mtx_adv_scanning_ctr);
        l_queue_push_tail(_pt_app_mgt->pt_adv_scanning_event_queue,
                          (void *)BT_EVENT_DISCONNECTION);
        pthread_cond_signal(&_pt_app_mgt->cond_val_adv_scanning_ctr);
        pthread_mutex_unlock(&_pt_app_mgt->mtx_adv_scanning_ctr);
      }
    }
  }
  return _ret;
}
/**
 * @brief Handle proxy "ServicesResolved" changed
 * @param pt_con_ctr: Pointer to the connection controller
 * @param pt_proxy: Pointer to changed proxy
 * @param pt_name: The changed property
 * @param pt_msg: dbus message
 * @param user_data: user data (not used)
 * @return ST_OK if everything is OK
 */
app_state_t
bt_con_n_service_property_changed(
  struct con_controller *pt_con_ctr,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_name,
  struct l_dbus_message *pt_msg,
  void *user_data)
{
  app_state_t _ret = ST_OK;
  bool tp_service_resolved = false;
  app_management *_pt_app_mgt = sys_get_mgt();

  if(!strcmp(pt_name, "ServicesResolved"))
  {
    if(!l_dbus_message_get_arguments(pt_msg, "b", &tp_service_resolved))
    {
      DBG_LOG_ERR("Failed to get property \"ServicesResolved\"");
      _ret = ST_ERR;
      return _ret;
    }

    if(tp_service_resolved)
    {
      if(_pt_app_mgt->bt_role == BT_ROLE_CLIENT)
      {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        const char *path = l_dbus_proxy_get_path(pt_proxy);
        DBG_LOG_INFO("ServicesResolved, path:%s, proxy@0x%llx\n", path, pt_proxy);
        pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
        uint8_t idx = CONFIG_BLE_CONNECTION_MAX_NUM;
        // check whether there is 'free' item for further connections
        for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
        {
          if(pt_proxy == pt_con_ctr->connected_proxy[i])
          {
            idx = i;
            break;
          }
        }
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
        if(idx < CONFIG_BLE_CONNECTION_MAX_NUM)
        {
          pthread_mutex_lock(&pt_con_ctr->mtx_for_service_flg);
          pt_con_ctr->fg_is_services_resolved[idx] = true;
          pt_con_ctr->fg_service_idx = idx;
          pthread_mutex_unlock(&pt_con_ctr->mtx_for_service_flg);
        }
#endif
      }
      else if(_pt_app_mgt->bt_role == BT_ROLE_SERVER)
      {
        //TODO: nothing to do for server role so far
      }
      // All the service has been discovered
      pthread_mutex_lock(&_pt_app_mgt->mtx_adv_scanning_ctr);
      l_queue_push_tail(_pt_app_mgt->pt_adv_scanning_event_queue,
                        (void *)BT_EVENT_SERVICES_RESOLVED);
      pthread_cond_signal(&_pt_app_mgt->cond_val_adv_scanning_ctr);
      pthread_mutex_unlock(&_pt_app_mgt->mtx_adv_scanning_ctr);
    }
  }

  return _ret;
}
/**
 * @brief Set the use status of device in the queue. If the proxy does not
 * exist in the device queue, then just ignore (no error would be
 * returned). Note that Locking the device queue is required before using
 * this function.
 * @param pt_con_ctr: Pointer to the connection controller
 * @param pt_proxy: Pointer to device proxy (to change the use status)
 * @param inuse: The use status
 * @return ST_OK if everything is OK
 */
static app_state_t
bt_con_n_set_inuse(
  struct con_controller *pt_con_ctr,
  struct l_dbus_proxy *pt_proxy,
  bool inuse)
{
  app_state_t _ret = ST_OK;
  struct dev_queue_ele *_pt_proxy_ele = NULL;

  if(NULL == pt_con_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  _pt_proxy_ele =
    (struct dev_queue_ele *)l_queue_find(
      pt_con_ctr->dev_queue,
      q_match_dev_proxy_compare,
      (const void *)pt_proxy);
  if(NULL != _pt_proxy_ele)
  {
    _pt_proxy_ele->in_use = inuse;
  }

  return _ret;
}
// typedef
// bool (*l_queue_remove_func_t) (void *data, void *user_data);
/**
 * @brief: The callback called by l_queue_foreach_remove
 * @return: true if pointer a is timeout (compared with the reference time
 * pointed by b) and is not in use.
 */
static bool
q_match_dev_proxy_timeout(
  void *data,
  void *user_data)
{
  bool tpret = false;

  time_t proxyTime = ((struct dev_queue_ele *)data)->timestamp;
  bool inuse = ((struct dev_queue_ele *)data)->in_use;
  time_t currentTime = *(time_t *)user_data;
  const char *_pt_proxy_addr = NULL;

  if(currentTime < proxyTime)
  {
    // Something must be wrong ...
    DBG_LOG_ERR("Current time is earlier the proxy adding time");
    l_free(data);
    tpret = true;
  }
  else
  {
    if(currentTime - proxyTime > BT_CONNECTION_NEW_SCANNED_ELE_TTL_S)
    {
      if(inuse == false)
      {

        if(!l_dbus_proxy_get_property(
             ((struct dev_queue_ele *)data)->proxy, "Address", "s",
             &_pt_proxy_addr))
        {
          DBG_LOG_ERR("Failed to get proxy property");
        }
        else
        {
          DBG_LOG_INFO("Removed device %s", _pt_proxy_addr);
        }

        l_free(data);
        tpret = true;
      }
      else
      {
        tpret = false;
      }
    }
    else
    {
      tpret = false;
    }
  }
  return tpret;
}
/**
 * @brief Refresh device queue (remove the device proxy which is timeout
 * and is not in use from the queue). Note that Locking the device queue
 * is required before using this function.
 * @param pt_con_ctr: Pointer to the connection controller
 * @return ST_OK if everything is OK
 */
app_state_t
bt_con_n_dev_queue_periodic_refresh(struct con_controller *pt_con_ctr)
{
  app_state_t _ret = ST_OK;
  time_t currentTime;
  uint32_t removedCnt = 0;

  if(NULL == pt_con_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }
  currentTime = time(NULL);
  removedCnt = l_queue_foreach_remove(pt_con_ctr->dev_queue,
                                      q_match_dev_proxy_timeout,
                                      (const void *)&currentTime);

  DBG_LOG_INFO("%d devices have been removed due to timeout ...",
                removedCnt);

  return ST_ERR;
}
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
/* @brief Get current connection number by counting connected_proxy.
 * @param pt_con_ctr: Pointer to the connection controller
 * @param pt_con_num: Pointer to the number of current connections.
 * @return 0 if everything is OK
 */
app_state_t
bt_con_n_connect_num_fetch(
  struct con_controller *pt_con_ctr,
  uint8_t *pt_con_num)
{
  uint8_t _cnt = 0;
  if(NULL == pt_con_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return ST_ERR;
  }
  if(0 == pthread_mutex_trylock(&pt_con_ctr->mtx_for_connected_proxy))
  {
    for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
    {
      // target proxy was already connected. skip further steps
      if(NULL != pt_con_ctr->connected_proxy[i])
      {
        _cnt++;
      }
    }
    *pt_con_num = _cnt;
    DBG_LOG_INFO("===con fetch:%d - %d= %d ==\n", _cnt, *pt_con_num, pt_con_ctr->con_status_bit); // debug only
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
  } else
  {
    DBG_LOG_WARN("fail to get cnt(fetch lock)");
    return ST_ERR;
  }
}
/* @brief Get the mac address from path.
 * @param *path: Pointer to the path of proxy
 * @param mac_addr: Pointer to mac address
 */
static void
get_mac_addr_from_path(
  char *path,
  char *mac_addr)
{
  const char *mac_pattern = "dev_C4_BD_6A_";
  const char *mac_format = "C4:BD:6A:xx:xx:xx";
  if((NULL == path) || (NULL == mac_addr))
  {
    DBG_LOG_ERR("Unexpected NULL\n");
    return;
  }
  char *pt_str = strstr(path, mac_pattern);
  if(NULL != pt_str)
  {
    memcpy(mac_addr, mac_format, strlen(mac_format));
    *(mac_addr+9) = *(pt_str+13);
    *(mac_addr+10) = *(pt_str+14);
    *(mac_addr+12) = *(pt_str+16);
    *(mac_addr+13) = *(pt_str+17);
    *(mac_addr+15) = *(pt_str+19);
    *(mac_addr+16) = *(pt_str+20);
  }
}
/* @brief Get the index of connection by matching MAC addr fetched from pt_proxy.
 * Notes:  DO NOT call this function in locking 'mtx_for_connected_proxy' scope
 * @param pt_con_ctr: Pointer to the connection controller
 * @param pt_proxy: Pointer to the target proxy
 * @param pt_con_idx: Pointer to the index of current connections.
 * @return 0 if everything is OK
 */
app_state_t
bt_con_n_connect_idx_fetch(
  struct con_controller *pt_con_ctr,
  struct l_dbus_proxy *pt_proxy,
  uint8_t *pt_con_idx)
{
  const char *path = l_dbus_proxy_get_path(pt_proxy);
  char tmp_mac_addr[MAX_ID_STR_LENGTH] = {0};
  if((NULL == pt_con_ctr) || (NULL == pt_proxy))
  {
    DBG_LOG_ERR("Unexpected NULL\n");
    return ST_ERR;
  }

  pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
  for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
  {
    if(pt_proxy == pt_con_ctr->connected_proxy[i])
    {
      *pt_con_idx = i;
      DBG_LOG_INFO("proxy matched connected slot:%d\n", i);
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
      return ST_OK;
    }
  }
  for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
  {
    if(pt_proxy == pt_con_ctr->pt_cond_var[i].proxy)
    {
      *pt_con_idx = i;
      DBG_LOG_INFO("proxy matched cond slot:%d\n", i);
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
      return ST_OK;
    }
  }
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);

  get_mac_addr_from_path((char *)path, tmp_mac_addr);
  if(('C' != tmp_mac_addr[0]) || ('4' != tmp_mac_addr[1]) || (':' != tmp_mac_addr[2]))
  {
    DBG_LOG_ERR("Fail to get mac addr:%s\n", tmp_mac_addr);
    return ST_ERR;
  }

  pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
  for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
  {
    if((NULL == pt_con_ctr->connected_proxy[i]) &&
       (NULL == pt_con_ctr->pt_cond_var[i].proxy))
    {
      continue;
    }
    if(0 == strncmp(tmp_mac_addr,
                    (const char *)&pt_con_ctr->con_mac_addr[i],
                    strlen(tmp_mac_addr)))
    {
      *pt_con_idx = i;
      DBG_LOG_INFO("active mac addr matched:%s i:%d\n",
                    pt_con_ctr->con_mac_addr[i],
                    i);
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
      return ST_OK;
    }
  }
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
  DBG_LOG_WARN("fail to get idx\n");
  return ST_ERR;
}
#endif
