/**
 * @file    bt_gatt_new.c
 * @author  Xiaoyuan (Sean) Ma, Tongde (Victor) Yan
 * @date    2025-04-17
 * @brief   Implementation of BLE gatt read and write.
 * @details
 */
#include "sys_def.h"
#include "app_common.h"
#include "bt_gatt_new.h"
#include "util_dbg.h"
#include "bt_gatt.h" // Just a temporary solution (would be removed soon)
#include <sys/socket.h>   // for 'struct msghdr'
#if(SKF_GW_NEW == 1)
#include <sys/ipc.h>
#endif

DBG_LOCAL_LOG_DEBUG

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
#include <sys/msg.h>

struct l_io *sock_io_new_v1(int fd, void *user_data);

static struct sock_io g_notify_io[CONFIG_BLE_CONNECTION_MAX_NUM];
struct sock_io * get_g_notify_io_new(uint8_t idx)
{
  if(idx < CONFIG_BLE_CONNECTION_MAX_NUM)
  {
	  return &g_notify_io[idx];
  }
  else
  {
    DBG_LOG_ERR("wrong idx:%d\n",idx);
    return NULL;
  }
}
#else
static struct sock_io *pt_g_notify_io;
#endif
// typedef
// void (*l_dbus_message_func_t) (struct l_dbus_message *message, void
// *user_data);
/**
 * @brief: The callback called by AcquireNotify DBUS method: setup. Here to
 * build a DBUS message manually
 * @return: NULL
 */
static void
msg_setup_for_charac_acquire_notify(
  struct l_dbus_message *message,
  void *user_data)
{
  struct l_dbus_message_builder *pt_builder = NULL;

  pt_builder = l_dbus_message_builder_new(message);

  l_dbus_message_builder_enter_array(pt_builder, "{sv}");
  l_dbus_message_builder_enter_dict(pt_builder, "sv");
  l_dbus_message_builder_leave_dict(pt_builder);
  l_dbus_message_builder_leave_array(pt_builder);
  l_dbus_message_builder_finalize(pt_builder);
  l_dbus_message_builder_destroy(pt_builder);

  return;
}
// typedef
// void ( *l_dbus_client_proxy_result_func_t )(struct l_dbus_proxy *proxy,
// struct l_dbus_message *result, void *user_data);
/**
 * @brief: The callback called by AcquireNotify DBUS method: reply. Here to
 * bind the socket io.
 * @return: NULL
 */
static void
msg_reply_for_charac_acquire_notify(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data)
{
  if(l_dbus_message_is_error(result))
  {
    const char *_pt_name = NULL, *_pt_text = NULL;

    // DBUS error happens
    l_dbus_message_get_error(result, &_pt_name, &_pt_text);
    DBG_LOG_ERR("Failed to acquire notification, err_info: %s-%s",
                _pt_name,
                _pt_text);
    return;
  }

  if(user_data)
  {
    struct sock_io *pt_sock_io = NULL;
    int kp_fd = -1;
    uint16_t kp_mtu = 0;

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    pt_sock_io = (struct sock_io *)user_data;
    struct con_controller *_pt_con_ctrl = bt_con_n_get_con_ctr();

    if(pt_sock_io->io)
    {
      l_io_destroy(pt_sock_io->io);
      pt_sock_io->io = NULL;
    }
    pt_sock_io->mtu = 0;

    if(l_dbus_message_get_arguments(result, "hq", &kp_fd,
                                    &kp_mtu) == false)
    {
      DBG_LOG_ERR("Invalid AcquireNotify response");
      return;
    }

    pt_sock_io->mtu = kp_mtu;
    // pass pt_sock_io with proxy (pt_con_ctr->proxy_uart_chr_tx[idx]) here.
    pt_sock_io->io = sock_io_new_v1(kp_fd, (void *)pt_sock_io);   
    DBG_LOG_DEBUG("pt_sock_io @ %p pt_sock_io->io @ %p\n", pt_sock_io, pt_sock_io->io);
    DBG_LOG_INFO("AcquireNotify is successful (fd %d, mtu %d)", kp_fd,
                kp_mtu);
#else
    pt_sock_io = (struct sock_io *)user_data;
    if(pt_sock_io->io)
    {
      l_io_destroy(pt_sock_io->io);
      pt_sock_io->io = NULL;
    }
    pt_sock_io->mtu = 0;

    if(l_dbus_message_get_arguments(result, "hq", &kp_fd,
                                    &kp_mtu) == false)
    {
      DBG_LOG_ERR("Invalid AcquireNotify response");
      return;
    }

    pt_sock_io->mtu = kp_mtu;
    // pass pt_sock_io with proxy (pt_con_ctr->proxy_uart_chr_tx[idx]) here.
    pt_sock_io->io = sock_io_new(kp_fd, NULL);
    DBG_LOG_DEBUG("pt_sock_io @ %p", pt_sock_io);
    DBG_LOG_DEBUG("pt_sock_io->io @ %p", pt_sock_io->io);
    DBG_LOG_INFO("AcquireNotify is successful (fd %d, mtu %d)", kp_fd,
                kp_mtu);
#endif
    return;
  }
  else
  {
    DBG_LOG_ERR("No socket IO to be bound");
  }
}
/**
 * @brief To acquire the notification of the given (characteristic) proxy
 * @param pt_con_ctr: The pointer to the connect controller (a data
 * structure maintained by application)
 * @return ST_OK if the given proxy has been acquire notification
 */
app_state_t
bt_gatt_n_acquire_notify(struct l_dbus_proxy *pt_proxy)
{
  app_state_t _ret = ST_OK;
  const char *_pt_interface = NULL;

  if(pt_proxy == NULL)
  {
    DBG_LOG_ERR("Unexpected NULL\n");
    _ret = ST_ERR;
    return _ret;
  }

  _pt_interface = l_dbus_proxy_get_interface(pt_proxy);
  if(NULL == _pt_interface)
  {
    DBG_LOG_ERR("Failed to get proxy interface\n");
    _ret = ST_ERR;
    return _ret;
  }

  if(strcmp(_pt_interface, "org.bluez.GattCharacteristic1"))
  {
    DBG_LOG_WARN("Chr proxy is expected\n");
    _ret = ST_ERR;
    return _ret;
  }

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  uint8_t idx = CONFIG_BLE_CONNECTION_MAX_NUM;
  struct con_controller *_pt_con_ctrl = bt_con_n_get_con_ctr();
  pthread_mutex_lock(&_pt_con_ctrl->mtx_for_chr_queue);
  for(uint8_t i=0;i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
  {
    if(pt_proxy == _pt_con_ctrl->proxy_uart_chr_tx[i])
    {
      idx = i;
      break;
    }
  }
  pthread_mutex_unlock(&_pt_con_ctrl->mtx_for_chr_queue);
  if(idx >= CONFIG_BLE_CONNECTION_MAX_NUM)
  {
    DBG_LOG_WARN("Chr proxy not found!\n");
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Chr tx proxy idx:%d, proxy @%p\n",idx, pt_proxy);
  struct sock_io *notify_io = get_g_notify_io_new(idx);
  if(0 == l_dbus_proxy_method_call(pt_proxy, "AcquireNotify",
                                  msg_setup_for_charac_acquire_notify,
                                  msg_reply_for_charac_acquire_notify,
                                  notify_io, NULL))
  {
    notify_io->proxy = pt_proxy;
    DBG_LOG_ERR("Failed to call dbus method \"AcquireNotify\"");
    _ret = ST_ERR;
    return _ret;
  }
  pthread_mutex_lock(&_pt_con_ctrl->mtx_for_sock_io);
  notify_io->proxy = pt_proxy;
  pthread_mutex_unlock(&_pt_con_ctrl->mtx_for_sock_io);
#else
  pt_g_notify_io = get_g_notify_io();
  if(0 == l_dbus_proxy_method_call(pt_proxy, "AcquireNotify",
                                  msg_setup_for_charac_acquire_notify,
                                  msg_reply_for_charac_acquire_notify,
                                  pt_g_notify_io, NULL))
  {
    DBG_LOG_ERR("Failed to call dbus method \"AcquireNotify\"");
    _ret = ST_ERR;
    return _ret;
  }
  pt_g_notify_io->proxy = pt_proxy;
#endif
  return _ret;
}
/**
 * @brief To check whether the given (characteristic) proxy has been
 * aquired notification
 * @param pt_con_ctr: The pointer to the connect controller (a data
 * structure maintained by application)
 * @return ST_OK if the given proxy has been acquire notification
 */
static bool
bt_gatt_n_is_notification_acquired(struct l_dbus_proxy *pt_proxy)
{
  bool _ret = true;

  if(pt_proxy == NULL)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = false;
    return _ret;
  }

  if(!l_dbus_proxy_get_property(pt_proxy, "NotifyAcquired", "b", &_ret))
  {
    DBG_LOG_ERR("Failed to get property");
    _ret = false;
    return _ret;
  }

  DBG_LOG_INFO("%s , property NotifyAcquired %d",
                l_dbus_proxy_get_path(pt_proxy), _ret);
  return _ret;
}
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
/**
 * @brief Acquire the notification of peer's TX characteristic. This
 * routine should be called after connection has been.
 * @param pt_con_ctr: The pointer to the connect controller (a data
 * structure maintained by application)
 * @param idx: The index of connection/proxy/char etc.
 * @return ST_OK if everthing is OK.
 */
app_state_t
bt_gatt_n_aquire_peer_tx_notification(
  const struct con_controller *pt_con_ctr,
  uint8_t idx)
#else
/**
 * @brief Acquire the notification of peer's TX characteristic. This
 * routine should be called after connection has been.
 * @param pt_con_ctr: The pointer to the connect controller (a data
 * structure maintained by application)
 * @return ST_OK if everthing is OK.
 */
app_state_t
bt_gatt_n_aquire_peer_tx_notification(
  const struct con_controller *pt_con_ctr)
#endif
{
  app_state_t _ret = ST_OK;
  app_management *_pt_app_mgt = sys_get_mgt();

  if(NULL == pt_con_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
  if(NULL == pt_con_ctr->proxy_uart_chr_tx[idx])
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
    DBG_LOG_ERR("Chr UART TX %d is not ready\n", idx);
    _ret = ST_ERR;
    return _ret;
  }
  if(true ==
    bt_gatt_n_is_notification_acquired(pt_con_ctr->proxy_uart_chr_tx[idx]))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
    DBG_LOG_WARN("The noficiation of peer's TX %d has been acquired\n", idx);
    _ret = ST_ERR;
    return _ret;
  }
  // MUST release mtx_for_chr_queue lock here as bt_gatt_n_acquire_notify() would try to lock it too
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
  if(ST_OK != bt_gatt_n_acquire_notify(pt_con_ctr->proxy_uart_chr_tx[idx]))
  {
    DBG_LOG_ERR("Failed to acquire peer's TX %d notification\n",idx);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Peer's TX notification is acquired successfully\n");
#else
  pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
  if(NULL == pt_con_ctr->proxy_uart_chr_tx)
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
    DBG_LOG_ERR("Chr UART TX is not ready");
    _ret = ST_ERR;
    return _ret;
  }
  if(true ==
    bt_gatt_n_is_notification_acquired(pt_con_ctr->proxy_uart_chr_tx))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
    DBG_LOG_WARN("The noficiation of peer's TX has been acquired");
    _ret = ST_ERR;
    return _ret;
  }
  if(ST_OK != bt_gatt_n_acquire_notify(pt_con_ctr->proxy_uart_chr_tx))
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
    DBG_LOG_ERR("Failed to acquire peer's TX notification");
    _ret = ST_ERR;
    return _ret;
  }
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
  DBG_LOG_INFO("Peer's TX notification is acquired successfully");
#endif
  return _ret;
}
// typedef
// void (*l_dbus_message_func_t) (struct l_dbus_message *message, void
// *user_data);
/**
 * @brief: The callback called by WriteValue DBUS method: setup. Here to
 * build a DBUS message manually
 * @return: NULL
 */
static void
msg_setup_for_charac_write_value(
  struct l_dbus_message *message,
  void *user_data)
{
  struct l_dbus_message_builder *pt_builder = NULL;
  struct attr_value *pt_attr_val = NULL;
  DBG_LOG_INFO("setup for char_wr_val!\n");

  pt_builder = l_dbus_message_builder_new(message);
  if(pt_builder == NULL)
  {
    DBG_LOG_ERR("Should not reach here!");
    return;
  }

  if(user_data == NULL)
  {
    DBG_LOG_ERR("Should not reach here!");
    return;
  }

  pt_attr_val = (struct attr_value *)user_data;

  l_dbus_message_builder_enter_array(pt_builder, "y");

  // Data
  for(uint32_t i = 0; i < pt_attr_val->len; i++)
  {
    l_dbus_message_builder_append_basic(pt_builder, 'y',
                                        (const void *)&pt_attr_val->ptbuf[i]);
  }

  l_dbus_message_builder_leave_array(pt_builder);

  l_dbus_message_builder_enter_array(pt_builder, "{sv}");
  l_dbus_message_builder_enter_dict(pt_builder, "sv");

  // Offset
  l_dbus_message_builder_append_basic(pt_builder, 's',
                                      (const void *)"offset");
  l_dbus_message_builder_enter_variant(pt_builder, "q");

  uint16_t tpu16 = 0;

  l_dbus_message_builder_append_basic(pt_builder, 'q',
                                      (const void *)&tpu16);
  l_dbus_message_builder_leave_variant(pt_builder);

  l_dbus_message_builder_leave_dict(pt_builder);
  l_dbus_message_builder_leave_array(pt_builder);

  l_dbus_message_builder_finalize(pt_builder);
  l_dbus_message_builder_destroy(pt_builder);

  return;
}
// typedef
// void ( *l_dbus_client_proxy_result_func_t )(struct l_dbus_proxy *proxy,
// struct l_dbus_message *result, void *user_data);
/**
 * @brief: The callback called by WriteValue DBUS method: reply. Here to
 * check whether the writing operation has been performed properly
 * @return: NULL
 */
static void
msg_reply_for_charac_write_value(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data)
{
  // To set the condition when BT write value operation has been done, no
  // matter failure or success
  DBG_LOG_DEBUG("reply for char_wr_val @%p!\n", proxy);
  if(user_data)
  {
    struct attr_value *pt_attr_value = NULL;

    pt_attr_value = (struct attr_value *)user_data;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    if(pt_attr_value->pt_cond_var == NULL)
    {
      DBG_LOG_ERR("pointer is NULL!\n");
      return;
    }
    if(CONFIG_COND_VAR_MAGIC_INIT != pt_attr_value->pt_cond_var->magic)
    {
      DBG_LOG_ERR("signal for former con, skip it(magic)!\n");
      return;
    }
#endif
    if(pthread_mutex_trylock(&pt_attr_value->pt_cond_var->mtx))
    {
      DBG_LOG_ERR(
        "Failed to acquire the lock (perhaps timeout has happened)");
      return;
    }
    else
    {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      // check whether the signal is for current connection
      // Notes: the 'write_proxy' of pt_cond_var should be updated when the connection was setup
      if((NULL != pt_attr_value->pt_cond_var->write_proxy) && (proxy != pt_attr_value->pt_cond_var->write_proxy))
      {
        DBG_LOG_ERR("signal for former con, skip it(mis-match) proxy:@%p, cond proxy:@%p!\n",
                    proxy,
                    pt_attr_value->pt_cond_var->write_proxy);
        pthread_mutex_unlock(&pt_attr_value->pt_cond_var->mtx);
        return;
      }
#endif
      pt_attr_value->pt_cond_var->is_op_done = true;
      pthread_cond_signal(&pt_attr_value->pt_cond_var->cond_var);
      pthread_mutex_unlock(&pt_attr_value->pt_cond_var->mtx);
    }
  }

  if(l_dbus_message_is_error(result))
  {
    const char *_pt_name = NULL, *_pt_text = NULL;

    // DBUS error happens
    l_dbus_message_get_error(result, &_pt_name, &_pt_text);
    DBG_LOG_ERR("Failed to write value, err_info: %s-%s", _pt_name,
                _pt_text);
    return;
  }

  DBG_LOG_INFO("BT write value is successful @%p!\n", proxy);
  return;
}
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
/**
 * @brief: The callback called by WriteValue DBUS method: destroy
 * @return: NULL
 */
static void
destroy_for_charac_write_value(void *user_data)
{
  DBG_LOG_DEBUG("BT write value destroy is called!");
  return;
}
static struct attr_value attr_val;
/**
 * @brief Write data to a BLE peer by writing to the UART RX
 * characteristic. This routine should be called after connection has been.
 * Note that there is a timeout timer would be triggered in the routine.
 * Return @p ST_ERR if a writing data timeout happens.
 * @param pt_con_ctr: The pointer to the connect controller (a data
 * structure maintained by application).
 * @param pt_cond_var: The condition variable. Note that the condition
 * variable should not be allocated dynamically because the variable would
 * be triggered after timeout expired when the routine has been exited.
 * @param ptdata: The pointer to the data buffer to be written
 * @param ptdata: The length (in byte) of the data buffer
 * @return ST_OK if everthing is OK.
 */
app_state_t
bt_gatt_n_write_data_to_peer_rx(
  struct con_controller *pt_con_ctr,
  struct pthread_cond_var *pt_cond_var,
  const uint8_t *ptdata,
  const uint32_t kplen)

{
  app_state_t _ret = ST_OK;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  uint8_t idx = (uint8_t)pt_cond_var->para;
  DBG_LOG_DEBUG("gatt wr, idx:%d\n", idx);
  if(idx >= CONFIG_BLE_CONNECTION_MAX_NUM)
  {
    DBG_LOG_ERR("Unexpected idx %d\n", idx);
    _ret = ST_ERR;
    goto EXIT;
  }
#endif

  if((NULL == pt_con_ctr) || (NULL == ptdata) ||
     (kplen == 0) || (NULL == pt_cond_var))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    goto EXIT;
  }
  memset(&attr_val, 0, sizeof(struct attr_value));

  attr_val.ptbuf = (uint8_t *)l_malloc(kplen);
  if(NULL == attr_val.ptbuf)
  {
    memset(&attr_val, 0, sizeof(struct attr_value));
    DBG_LOG_ERR("Failed to malloc");
    _ret = ST_ERR;
    goto EXIT;
  }
  memset(attr_val.ptbuf, 0, kplen);
  memcpy(attr_val.ptbuf, ptdata, kplen);
  attr_val.len = kplen;
  attr_val.pt_cond_var = pt_cond_var;
  attr_val.pt_cond_var->is_op_done = false;

  // Write to peer's UART_RX
  pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  if(NULL == pt_con_ctr->proxy_uart_chr_rx[idx])
#else
  if(NULL == pt_con_ctr->proxy_uart_chr_rx)
#endif
  {
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
    l_free(attr_val.ptbuf);
    attr_val.ptbuf = NULL;
    memset(&attr_val, 0, sizeof(struct attr_value));

    DBG_LOG_ERR("No valid RX CHR for writing");
    _ret = ST_ERR;
    goto EXIT;
  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  // set the proxy for further checking 'user_data' in callback()
  attr_val.pt_cond_var->write_proxy = pt_con_ctr->proxy_uart_chr_rx[idx];
  struct l_dbus_proxy *_uart_chr_rx = pt_con_ctr->proxy_uart_chr_rx[idx]; // use tx proxy to have a try
  if(0 == l_dbus_proxy_method_call(_uart_chr_rx,
                                  "WriteValue",
                                  msg_setup_for_charac_write_value,
                                  msg_reply_for_charac_write_value,
                                  (void *)&attr_val,
                                  destroy_for_charac_write_value))
#else
  if(0 == l_dbus_proxy_method_call(pt_con_ctr->proxy_uart_chr_rx,
                                   "WriteValue",
                                   msg_setup_for_charac_write_value,
                                   msg_reply_for_charac_write_value,
                                   (void *)&attr_val,
                                   destroy_for_charac_write_value))
#endif
  {
    // Failed to call method
    pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
    l_free(attr_val.ptbuf);
    attr_val.ptbuf = NULL;
    memset(&attr_val, 0, sizeof(struct attr_value));
    DBG_LOG_ERR("Failed to call dbus method \"WriteValue\"");

    _ret = ST_ERR;
    goto EXIT;
  }
  pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
  // to wait for the writting to be done
  pthread_mutex_lock(&pt_cond_var->mtx);
  clock_gettime(CLOCK_MONOTONIC, &pt_cond_var->tv.tv_sec); //CLOCK_REALTIME   CLOCK_MONOTONIC
  pt_cond_var->tv.tv_sec = pt_cond_var->tv.tv_sec + 5;
  while(1)
  {
    if(false == pt_cond_var->is_op_done)
    {
      int cond_rt;

      cond_rt = pthread_cond_timedwait(&pt_cond_var->cond_var,
                                       &pt_cond_var->mtx, &pt_cond_var->tv);
      if((0 != cond_rt) && (ETIMEDOUT == cond_rt))
      {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        DBG_LOG_ERR("Data writting timeout happened, idx:%d\n", idx);
#else
        DBG_LOG_ERR("Data writting timeout happened\n");
#endif
        _ret = ST_ERR;
        break;
      }else{
        if(false == pt_cond_var->is_op_done)
        {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
          DBG_LOG_ERR("Unknown failure %d idx:%d\n", cond_rt, idx);
#else
          DBG_LOG_ERR("Unknown failure %d \n", cond_rt);
#endif
          _ret = ST_ERR;
          break;
        }
      }
    }
    else
    {
      // minimal 'de-init' cond_var by clearing 'is_op_done' here in case it's set to 'true' somewhere else
      pt_cond_var->is_op_done = false;
      DBG_LOG_DEBUG("GATT:SEND MSG [FOR DEBUG]");
      util_dbg_buf_dump(
        (uint8_t const *)attr_val.ptbuf,
        attr_val.len);
      DBG_LOG_INFO("Data is written to the peer device");
      break;
    }
  }
  l_free(attr_val.ptbuf);
  attr_val.ptbuf = NULL;
  memset(&attr_val, 0, sizeof(struct attr_value));

  pthread_mutex_unlock(&pt_cond_var->mtx);
EXIT:
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  DBG_LOG_INFO("gatt wr exit, idx:%d\n", idx);
#else
  DBG_LOG_INFO("gatt wr exit\n");
#endif
  return _ret;
}

static bool sock_read_v1(struct l_io *io, void *user_data)
{
	struct sock_io *_pt_sock_io = (struct sock_io *)user_data;
  struct l_io *tmp_io = io;
	struct msghdr msg;
	struct iovec iov;
  struct con_controller *_pt_con_ctrl = bt_con_n_get_con_ctr();
	uint8_t buf[MAX_ATTR_VAL_LEN];
	int fd = l_io_get_fd(tmp_io);
	ssize_t bytes_read;
  msg.msg_iovlen = 1;
			
  DBG_LOG_DEBUG("sock read!fd:%d, io@0x%llx, sock_io@0x%llx, buf@%p\n", fd, tmp_io, _pt_sock_io, buf);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pthread_mutex_lock(&_pt_con_ctrl->mtx_for_sock_io);
	if ((tmp_io != _pt_sock_io->io) || 
      (NULL == _pt_sock_io->proxy))
	{
    pthread_mutex_unlock(&_pt_con_ctrl->mtx_for_sock_io);
    DBG_LOG_ERR("io mis-match or proxy is NULL! io@0x%llx, proxy@0x%llx\n", _pt_sock_io->io, _pt_sock_io->proxy);
		return false;
	}
#endif

  memset(buf, 0, MAX_ATTR_VAL_LEN);
	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	bytes_read = recvmsg(fd, &msg, MSG_DONTWAIT);
	if (bytes_read < 1) {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    pthread_mutex_unlock(&_pt_con_ctrl->mtx_for_sock_io);
#endif
		//bt_shell_printf("recvmsg: %s", strerror(errno));
		DBG_LOG_ERR("recvmsg (%d) : %s\n", bytes_read, strerror(errno));
		return false;
	}
  DBG_LOG_INFO(
    "BLE_GATT_RX bytes:%zd first:0x%02X",
    bytes_read,
    buf[0]);
#if(SKF_GW_NEW == 1)
  DBG_LOG_INFO(" %d sock_read!\n", bytes_read);
  // send the data received to process-general
  int kp_msgid = sys_get_msgid_to_pro_general( sys_get_mgt());
  struct ipc_msg_v3 kp_msg = {0};
  if((kp_msgid >= 0)&&(bytes_read <= MAX_IPC_MSG_PROTO_DATA))
  {
    kp_msg.type = M_TYPE_PROTO_DATA;
    memcpy( kp_msg.payload.proto_pkt.dtbuf, buf, bytes_read);
    kp_msg.payload.proto_pkt.len = bytes_read;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1) 
    pthread_mutex_unlock(&_pt_con_ctrl->mtx_for_sock_io);
#endif
    pthread_mutex_lock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
    l_queue_push_tail(sys_get_mgt()->pt_adv_scanning_event_queue,
              (void *)BT_EVENT_DO_NOTHING);
    pthread_cond_signal(&(sys_get_mgt()->cond_val_adv_scanning_ctr));
    pthread_mutex_unlock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
    // for debug only

    uint16_t _cnt = 300;
    do {
      if(0 != msgsnd( kp_msgid, &kp_msg,sizeof(union msg_payload), IPC_NOWAIT))
      {
        _cnt--;
        if(EAGAIN == errno)
        {
          DBG_LOG_WARN("Failed to send data, %s, cnt%d\n", strerror(errno), _cnt);
        }
        else
        {
          DBG_LOG_ERR("Failed to send data, %s,breaak!\n", strerror(errno));
          return false;
        }
        usleep(500000);
      } else
      {
        util_dbg_buf_dump(
        (uint8_t const *)buf,
        bytes_read);
        DBG_LOG_DEBUG("send data pass @%p!\n", tmp_io);
        break;
      }
    } while(_cnt > 0);
  }
  else
  {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    pthread_mutex_unlock(&_pt_con_ctrl->mtx_for_sock_io);
#endif
    DBG_LOG_ERR("wrong msgid %d or too much data!\n", kp_msgid);
  }
#else
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pthread_mutex_unlock(&_pt_con_ctrl->mtx_for_sock_io);
#endif
#endif
	return true;
}

/*typedef void (*l_io_disconnect_cb_t) (struct l_io *io, void *user_data);
*/
static void sock_hup_v1(struct l_io *io, void *user_data)
{
  struct l_io *pt_io = io;
	struct sock_io *_pt_sock_io = (struct sock_io *)user_data;
  struct con_controller *_pt_con_ctrl = bt_con_n_get_con_ctr();
  DBG_LOG_INFO("sock hup @0x%p proxy. io @0x%p pt_io:0x%p\n", _pt_sock_io->proxy, _pt_sock_io->io, pt_io);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pthread_mutex_lock(&_pt_con_ctrl->mtx_for_sock_io);
#endif
  _pt_sock_io->proxy = NULL;
  _pt_sock_io->mtu = 0;
  if(NULL != _pt_sock_io->io)
  {
    if(pt_io != _pt_sock_io->io)
    {
      DBG_LOG_WARN("io mis-match!\n");
    }
    // maybe it's not necessary to destroy io here (to be confirmed!!!)
    // key log: 'double free or corruption (fasttop)'
    // l_io_destroy(_pt_sock_io->io);
    _pt_sock_io->io = NULL;
  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pthread_mutex_unlock(&_pt_con_ctrl->mtx_for_sock_io);
#endif
  DBG_LOG_INFO("hup sock_io @ %p", _pt_sock_io);
	return ;
}

//
struct l_io *sock_io_new_v1(int fd, void *user_data)
{
	struct l_io *io;
  struct con_controller *_pt_con_ctrl = bt_con_n_get_con_ctr();

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pthread_mutex_lock(&_pt_con_ctrl->mtx_for_sock_io);
#endif
	io = l_io_new(fd);

	l_io_set_close_on_destroy(io, true);

	l_io_set_read_handler(io, sock_read_v1, user_data, NULL);

	l_io_set_disconnect_handler(io, sock_hup_v1, user_data, NULL);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pthread_mutex_unlock(&_pt_con_ctrl->mtx_for_sock_io);
#endif
	DBG_LOG_INFO("io @ %p", io);
	return io;
}
// typedef
// struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
// struct l_dbus_message *message, void *user_data);
// Callback to be called by "release" method is called
static struct l_dbus_message *
app_register_cb_method_release(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  void *user_data)
{
  if(false == l_dbus_object_remove_interface(pt_dbus, BT_OBJ_PATH_APP,
                                             "org.bluez.GattProfile1"))
  {
    DBG_LOG_ERR("Failed to remove org.bluez.GattProfile1 from %s",
                BT_OBJ_PATH_APP);
  }

  return l_dbus_message_new_method_return(message);
}
// typedef
// bool (*l_dbus_property_get_cb_t) (struct l_dbus *, struct l_dbus_message
// *message, struct l_dbus_message_builder *builder, void *user_data);
// struct l_dbus_message *message, void *user_data);
// Callback to be called by "get" the configured property is called (i.e.,
// configure the property to dbus)
static bool
app_register_cb_get_uuids(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_builder *builder,
  void *user_data)
{
  l_dbus_message_builder_enter_array(builder, "s");
  l_dbus_message_builder_append_basic(builder, 's',
                                      "register-application");
  l_dbus_message_builder_leave_array(builder);
  return true;
}
// typedef
// void (*l_dbus_interface_setup_func_t) (struct l_dbus_interface *);
// Add release methor and uuid property to the interface (of profile)
// Callback to be called by l_dbus_register_interface when registering gatt
// profile
static void
setup_func_for_register_application(struct l_dbus_interface *pt_interface)
{
  DBG_LOG_INFO("Register method/property for BLE application");
  // method
  l_dbus_interface_method(pt_interface, "Release", 0,
                          app_register_cb_method_release, NULL, NULL);
  // the property
  l_dbus_interface_property(pt_interface, "UUIDs", 0, "as",
                            app_register_cb_get_uuids, NULL);
  return;
}
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
// Do nothing
// Callback to be called by l_dbus_register_interface when registering gatt
// profile
static void
destroy_for_register_application(void *user_data)
{
  DBG_LOG_DEBUG("Do nothing ...");
  return;
}
// typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
// void *user_data);
// Setup callback called when RegisterApplication method is called
static void
msg_setup_for_register_application(
  struct l_dbus_message *message,
  void *user_data)
{
  struct l_dbus_message_builder *pt_builder = NULL;

  DBG_LOG_INFO("TO setup message for application register");
  pt_builder = l_dbus_message_builder_new(message);
  l_dbus_message_builder_append_basic(pt_builder, 'o', "/");
  l_dbus_message_builder_enter_array(pt_builder, "{sv}");
  l_dbus_message_builder_enter_dict(pt_builder, "sv");
  l_dbus_message_builder_leave_dict(pt_builder);
  l_dbus_message_builder_leave_array(pt_builder);

  l_dbus_message_builder_finalize(pt_builder);
  l_dbus_message_builder_destroy(pt_builder);
  return;
}
// typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy
// *proxy, struct l_dbus_message *result, void *user_data);
// Reply callback called when RegisterApplication method is called
static void
msg_reply_for_register_application(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data)
{
  if(l_dbus_message_is_error(result))
  {
    const char *_pt_name = NULL, *_pt_text = NULL;

    // DBUS error happens
    l_dbus_message_get_error(result, &_pt_name, &_pt_text);
    DBG_LOG_ERR("Failed to register BLE application, err_info: %s-%s",
                _pt_name,
                _pt_text);
    return;
  }
  else
  {
    DBG_LOG_INFO("BLE application has been registered successfully");
  }
  return;
}
/**
 * @brief Register the application (including the profile) to the gatt
 * manager
 * @param pt_dbus The dbus
 * @param pt_proxy The proxy of gatt manager
 * @return @ST_OK if things go well
 */
static app_state_t
gatt_register_application(
  struct l_dbus *pt_dbus,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t _ret = ST_OK;

  if((pt_dbus == NULL) || (pt_proxy == NULL))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  // Register profile
  if(false == l_dbus_register_interface(pt_dbus, "org.bluez.GattProfile1",
                                        setup_func_for_register_application,
                                        destroy_for_register_application,
                                        true))
  {
    DBG_LOG_ERR("Failed to register org.bluez.GattProfile1");
    _ret = ST_ERR;
    return _ret;
  }

  if(false == l_dbus_object_add_interface(pt_dbus, BT_OBJ_PATH_APP,
                                          "org.bluez.GattProfile1", NULL))
  {
    DBG_LOG_ERR(
      "Failed to add an instance org.bluez.GattProfile1 to object %s",
      BT_OBJ_PATH_APP);
    l_dbus_unregister_interface(pt_dbus, "org.bluez.GattProfile1");
    _ret = ST_ERR;
    return _ret;
  }

  if(false == l_dbus_object_add_interface(pt_dbus, BT_OBJ_PATH_APP,
                                          DBUS_INTERFACE_PROPERTIES, NULL))
  {
    l_dbus_object_remove_interface(pt_dbus, BT_OBJ_PATH_APP,
                                   "org.bluez.GattProfile1");
    l_dbus_unregister_interface(pt_dbus, "org.bluez.GattProfile1");

    DBG_LOG_ERR("Failed to register %s", DBUS_INTERFACE_PROPERTIES);
    _ret = ST_ERR;
    return _ret;
  }

  // Register the application
  if(0 == l_dbus_proxy_method_call(pt_proxy, "RegisterApplication",
                                   msg_setup_for_register_application,
                                   msg_reply_for_register_application,
                                   NULL, destroy_for_register_application))
  {
    DBG_LOG_ERR("Failed to register application %s", BT_OBJ_PATH_APP);

    l_dbus_object_remove_interface(pt_dbus, BT_OBJ_PATH_APP,
                                   DBUS_INTERFACE_PROPERTIES);
    l_dbus_object_remove_interface(pt_dbus, BT_OBJ_PATH_APP,
                                   "org.bluez.GattProfile1");
    l_dbus_unregister_interface(pt_dbus, "org.bluez.GattProfile1");
    _ret = ST_ERR;
    return _ret;
  }
  return _ret;
}
/**
 * @brief Register the application (including profile, services, and
 * characteristics) to the gatt manager in order to provide BLE service
 * (e.g., when the gateway is connected by a mobile phone)
 * @param pt_dbus The dbus
 * @param pt_proxy The proxy of gatt manager
 * @return @ST_OK if things go well
 */
app_state_t
bt_gatt_n_register_local_service(
  struct l_dbus *pt_dbus,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t _ret = ST_OK;

  if(pt_dbus == NULL)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  // To add "org.freedesktop.DBus.ObjectManager" to "/"
  if(!l_dbus_object_manager_enable(pt_dbus, "/"))
  {
    DBG_LOG_ERR("Failed to enable object manager");
    _ret = ST_ERR;
    return _ret;
  }

  /***************************************************/
  /********************UART Service*******************/
  /***************************************************/
  // Register the UART service
  if(ST_OK != bt_gatt_register_service(pt_dbus, NORDIC_UART_SERVICE_UART,
                                       0, true))
  {
    DBG_LOG_ERR("Failed to register service %s", NORDIC_UART_SERVICE_UART);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Service %s has been registered successfully\n",
               NORDIC_UART_SERVICE_UART);
  // Register the characteristics (TX, RX)
  // TX: Server -> Client
  if(ST_OK != bt_gatt_register_characteristic(pt_dbus,
                                              NORDIC_UART_CHARAC_TX_UUID,
                                              "notify", 0))
  {
    DBG_LOG_ERR("Failed to register characteristic %s",
                NORDIC_UART_CHARAC_TX_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Characteristic %s has been registered successfully\n",
               NORDIC_UART_CHARAC_TX_UUID);

  // RX: Client -> Server
  if(ST_OK != bt_gatt_register_characteristic(pt_dbus,
                                              NORDIC_UART_CHARAC_RX_UUID,
                                              "write", 0))
  {
    DBG_LOG_ERR("Failed to register characteristic %s",
                NORDIC_UART_CHARAC_RX_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Characteristic %s has been registered successfully\n",
               NORDIC_UART_CHARAC_RX_UUID);

  /***************************************************/
  /********************DIS Service********************/
  /***************************************************/
  // Register the DIS service
  if(ST_OK != bt_gatt_register_service(pt_dbus, BLE_DEV_INFO_SERVICE_UUID,
                                       0, true))
  {
    DBG_LOG_ERR("Failed to register service %s",
                BLE_DEV_INFO_SERVICE_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Service %s has been registered successfully\n",
               BLE_DEV_INFO_SERVICE_UUID);
  // Register the characteristics (Hardware version, Manufacturer, and
  // Firmware version)
  // Hardware version
  if(ST_OK != bt_gatt_register_characteristic(pt_dbus,
                                              BLE_HARDWARE_VERSION_CHR_UUID,
                                              "read", 0))
  {
    DBG_LOG_ERR("Failed to register characteristic %s",
                BLE_HARDWARE_VERSION_CHR_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Characteristic %s has been registered successfully\n",
               BLE_HARDWARE_VERSION_CHR_UUID);
  // Manufacturer
  if(ST_OK != bt_gatt_register_characteristic(pt_dbus,
                                              BLE_MANUFACTURER_NM_CHR_UUID,
                                              "read", 0))
  {
    DBG_LOG_ERR("Failed to register characteristic %s",
                BLE_MANUFACTURER_NM_CHR_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Characteristic %s has been registered successfully\n",
               BLE_MANUFACTURER_NM_CHR_UUID);
  // Firmware version
  if(ST_OK != bt_gatt_register_characteristic(pt_dbus,
                                              BLE_FIRMWARE_VERSION_CHR_UUID,
                                              "read", 0))
  {
    DBG_LOG_ERR("Failed to register characteristic %s",
                BLE_FIRMWARE_VERSION_CHR_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Characteristic %s has been registered successfully\n",
               BLE_FIRMWARE_VERSION_CHR_UUID);

  /***************************************************/
  /******************SKF CNEA Profile*****************/
  /***************************************************/
  // Register the profile and then register the application
  if(ST_OK != gatt_register_application(pt_dbus, pt_proxy))
  {
    DBG_LOG_ERR("Failed to register profile");
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Profile has been registered successfully\n");

  return _ret;
}
/**
 * @brief Unregister the application (including profile, services, and
 * characteristics) to the gatt manager in order to provide BLE service
 * (e.g., when the gateway is connected by a mobile phone)
 * @param pt_dbus The dbus
 * @param pt_proxy The proxy of gatt manager
 * @return @ST_OK if things go well
 */
app_state_t
bt_gatt_n_unregister_local_service(
  struct l_dbus *pt_dbus,
  struct l_dbus_proxy *pt_proxy)
{
  app_state_t _ret = ST_OK;

  if(pt_dbus == NULL)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  // // To add "org.freedesktop.DBus.ObjectManager" to "/"
  // if(!l_dbus_object_manager_enable(pt_dbus, "/"))
  // {
  //   DBG_LOG_ERR("Failed to enable object manager");
  //   _ret = ST_ERR;
  //   return _ret;

  /***************************************************/
  /********************UART Service*******************/
  /***************************************************/
  // Unregister the characteristics (TX, RX)
  // TX: Server -> Client
  if(ST_OK != bt_gatt_unregister_characteristic(pt_dbus,
                                                NORDIC_UART_CHARAC_TX_UUID))
  {
    DBG_LOG_ERR("Failed to unregister characteristic %s",
                NORDIC_UART_CHARAC_TX_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Characteristic %s has been unregistered successfully",
               NORDIC_UART_CHARAC_TX_UUID);

  // RX: Client -> Server
  if(ST_OK != bt_gatt_unregister_characteristic(pt_dbus,
                                                NORDIC_UART_CHARAC_RX_UUID))
  {
    DBG_LOG_ERR("Failed to unregister characteristic %s",
                NORDIC_UART_CHARAC_RX_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Characteristic %s has been unregistered successfully",
               NORDIC_UART_CHARAC_RX_UUID);
  // Unregister the UART service
  if(ST_OK != bt_gatt_unregister_service(pt_dbus, NORDIC_UART_SERVICE_UART))
  {
     DBG_LOG_ERR("Failed to unregister service %s", NORDIC_UART_SERVICE_UART);
     _ret = ST_ERR;
     return _ret;
   }
   DBG_LOG_INFO("Service %s has been unregistered successfully",
                NORDIC_UART_SERVICE_UART);

  /***************************************************/
  /********************DIS Service********************/
  /***************************************************/
  // Unregister the characteristics (Hardware version, Manufacturer, and
  // Firmware version)
  // Hardware version
  if(ST_OK != bt_gatt_unregister_characteristic(pt_dbus,
                                                BLE_HARDWARE_VERSION_CHR_UUID))
  {
    DBG_LOG_ERR("Failed to unregister characteristic %s",
                BLE_HARDWARE_VERSION_CHR_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Characteristic %s has been unregistered successfully",
               BLE_HARDWARE_VERSION_CHR_UUID);
  // Manufacturer
  if(ST_OK != bt_gatt_unregister_characteristic(pt_dbus,
                                                BLE_MANUFACTURER_NM_CHR_UUID))
  {
    DBG_LOG_ERR("Failed to unregister characteristic %s",
                BLE_MANUFACTURER_NM_CHR_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Characteristic %s has been unregistered successfully",
               BLE_MANUFACTURER_NM_CHR_UUID);
  // Firmware version
  if(ST_OK != bt_gatt_unregister_characteristic(pt_dbus,
                                                BLE_FIRMWARE_VERSION_CHR_UUID))
  {
    DBG_LOG_ERR("Failed to unregister characteristic %s",
                BLE_FIRMWARE_VERSION_CHR_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Characteristic %s has been unregistered successfully",
               BLE_FIRMWARE_VERSION_CHR_UUID);
  // Unregister the DIS service
  if(ST_OK != bt_gatt_unregister_service(pt_dbus, BLE_DEV_INFO_SERVICE_UUID))
  {
    DBG_LOG_ERR("Failed to unregister service %s",
                BLE_DEV_INFO_SERVICE_UUID);
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_INFO("Service %s has been unregistered successfully",
                BLE_DEV_INFO_SERVICE_UUID);

  /***************************************************/
  /******************SKF CNEA Profile*****************/
  /***************************************************/
  // Register the profile and then register the application
  // if(ST_OK != gatt_register_application(pt_dbus, pt_proxy))
  // {
  //   DBG_LOG_ERR("Failed to register profile");
  //   _ret = ST_ERR;
  //   return _ret;
  // }
  // DBG_LOG_INFO("Profile has been registered successfully");

  return _ret;
}
/**
 * @brief Write data to a BLE peer by notifying the UART TX characteristic.
 * This routine should be called after connection has been.
 * Note that there is a timeout timer would be triggered in the routine.
 * Return @p ST_ERR if a writing data timeout happens.
 * @param pt_con_ctr: The pointer to the application manager (a data
 * structure maintained by application).
 * @param pt_cond_var: The condition variable. Note that the condition
 * variable should not be allocated dynamically because the variable would
 * be triggered after timeout expired when the routine has been exited.
 * @param ptdata: The pointer to the data buffer to be written
 * @param ptdata: The length (in byte) of the data buffer
 * @return ST_OK if everthing is OK.
 */
app_state_t
bt_gatt_n_write_data_to_local_tx(
  app_management *pt_app_mgt,
  struct pthread_cond_var *pt_cond_var,
  const uint8_t *ptdata,
  const uint32_t kplen)
{
  app_state_t _ret = ST_OK;

  if((NULL == pt_app_mgt) || (NULL == ptdata) ||
     (kplen == 0) || (NULL == pt_cond_var))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    goto EXIT;
  }

  pt_cond_var->is_op_done = false;

  if(ST_OK != bt_gatt_charac_send_notification(pt_app_mgt,
                                               ptdata,
                                               kplen,
                                               pt_cond_var))
  {
    DBG_LOG_ERR("Failed to notify");
    _ret = ST_ERR;
    goto EXIT;
  }
  pthread_mutex_lock(&pt_cond_var->mtx);
  clock_gettime(CLOCK_MONOTONIC, &pt_cond_var->tv);
  pt_cond_var->tv.tv_sec = pt_cond_var->tv.tv_sec + 5;
  while(1)
  {
    if(false == pt_cond_var->is_op_done)
    {
      int cond_rt;

      cond_rt = pthread_cond_timedwait(&pt_cond_var->cond_var,
                                       &pt_cond_var->mtx,
                                       &pt_cond_var->tv);
      if((0 != cond_rt) && (ETIMEDOUT == cond_rt))
      {
        DBG_LOG_ERR("Notify timeout happened\n");
        _ret = ST_ERR;
        break;
      }
    }
    else
    {
      DBG_LOG_INFO("SEND MSG in svr role!\n");
      util_dbg_buf_dump(
        (uint8_t const *)ptdata,
        kplen);
      DBG_LOG_DEBUG("Data is notified to the peer device in svr role!\n");
      break;
    }
  }
  pthread_mutex_unlock(&pt_cond_var->mtx);
EXIT:
  return _ret;
}
/**
 * @brief Find the connected device proxy from the queue and set the proxy
 * to @p hld_connected_proxy governed by the application manager.
 * @param pt_app_mgt The global application manager
 * @param needNotify True if need to notify @p pt_adv_scanning_event_queue
 * that there is a connection event
 * @return ST_OK if everthing is OK.
 */
app_state_t
bt_gatt_n_find_connected_dev_proxy(
  app_management *pt_app_mgt,
  bool needNotify)
{
  app_state_t _ret = ST_OK;

  struct l_queue_entry *_pt_entries = NULL;
  struct l_dbus_proxy **_ppt_proxy = NULL;
  struct l_dbus_proxy *_pt_proxy = NULL;
  struct l_dbus_proxy *_hld_pt_proxy = NULL;
  uint32_t tp_cnt = 0;

  if(NULL == pt_app_mgt)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    goto EXIT;
  }

  // Find the proxy of connected device in the queue ...
  pthread_mutex_lock(&pt_app_mgt->mtx_q_devices);
  _pt_entries = l_queue_get_entries(sys_get_devices_queue(pt_app_mgt));
  while(_pt_entries)
  {
    _ppt_proxy = (struct l_dbus_proxy **)(_pt_entries->data);
    if(_ppt_proxy)
    {
      _pt_proxy = *_ppt_proxy;
      if(_pt_proxy && (true == bt_con_is_dev_connected(_pt_proxy)))
      {
        _hld_pt_proxy = _pt_proxy;
        tp_cnt = tp_cnt + 1;
      }
    }

    _pt_entries = _pt_entries->next;
  }
  pthread_mutex_unlock(&pt_app_mgt->mtx_q_devices);

  if(NULL == _hld_pt_proxy)
  {
    // There is no device being connected
    _ret = ST_ERR;
    goto EXIT;
  }

  if(1 != tp_cnt)
  {
    DBG_LOG_WARN("More than 1 device is being connected %d\n", tp_cnt);
  }

  DBG_LOG_INFO("The path of connected proxy is %s\n",
               l_dbus_proxy_get_path(_hld_pt_proxy));
  if(pt_app_mgt->bt_role == BT_ROLE_CLIENT)
  {
    // set proxy for client role
    sys_set_connected_proxy(pt_app_mgt, _hld_pt_proxy);
  }
  else if(pt_app_mgt->bt_role == BT_ROLE_SERVER)
  {
    // This function would be called in server role only currently
    // set proxy before sending BT_EVENT_CONNECTION out.
    pthread_mutex_lock(&pt_app_mgt->mtx_for_connected_proxy);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    pt_app_mgt->hld_server_connected_proxy = _hld_pt_proxy;
#else
    pt_app_mgt->hld_connected_proxy = _hld_pt_proxy;
#endif
    pthread_mutex_unlock(&pt_app_mgt->mtx_for_connected_proxy);
  }

  if(needNotify)
  {
    pthread_mutex_lock(&pt_app_mgt->mtx_adv_scanning_ctr);
    l_queue_push_tail(pt_app_mgt->pt_adv_scanning_event_queue,
                      (void *)BT_EVENT_CONNECTION);
    pthread_cond_signal(&pt_app_mgt->cond_val_adv_scanning_ctr);
    pthread_mutex_unlock(&pt_app_mgt->mtx_adv_scanning_ctr);
  }

EXIT:
  return _ret;
}
