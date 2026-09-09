/**
 * @file    app_common.c
 * @author  Victor Yan
 * @date    2024-10-17
 * @brief   Implementation of common APIs (BLE sending, NTP setting, and
 *          network setting, etc.).
 * @details
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/msg.h>
#include "app_common.h"
#include "sys_def.h"
#include "util_dbg.h"
#include "libusb-1.0/libusb.h"

DBG_LOCAL_LOG_DEBUG


/* Variable for BLE data transfer controlling: writing in client mode (from
   sever to client) */
static struct bluetooth_trans_controller g_bt_trans_ctr = { 0 };
/* Variable for BLE data transfer controlling: notification in server mode
   (from sever to client) */
/* We put the data for writting and notification on different queue, in
   case that BLE client and server co-exist */
static struct bluetooth_trans_controller g_bt_notification_trans_ctr =
{ 0 };

struct bluetooth_trans_controller *
app_com_get_data_trans_ctr(void)
{
  return &g_bt_trans_ctr;
}
struct bluetooth_trans_controller *
app_com_get_notification_trans_ctr(void)
{
  return &g_bt_notification_trans_ctr;
}
/**
 * @brief To initialize the BLE data transfer controller
 */
app_state_t
app_com_init_data_trans_ctr(
  struct bluetooth_trans_controller *pt_data_trans_ctr)
{
  app_state_t tpret = ST_OK;

  if(NULL == pt_data_trans_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  pt_data_trans_ctr->pt_data_queue = l_queue_new();

  if(pthread_mutex_init(&pt_data_trans_ctr->mtx, NULL))
  {
    DBG_LOG_ERR("Failed to initialize mutex");
    tpret = ST_ERR;
    goto ERR;
  }

  if(sem_init(&pt_data_trans_ctr->sem_data_num, 0, 0))
  {
    DBG_LOG_ERR("Failed to initialize semaphore");
    tpret = ST_ERR;
    goto ERR;
  }

  return tpret;

ERR:
  if(pt_data_trans_ctr->pt_data_queue)
  {
    l_queue_destroy(pt_data_trans_ctr->pt_data_queue, NULL);
  }
  sem_destroy(&pt_data_trans_ctr->sem_data_num);
  pthread_mutex_destroy(&pt_data_trans_ctr->mtx);

  return tpret;
}
/**
 * @brief To de-initialize the BLE data transfer controller
 */
void
app_com_destroy_data_trans_ctr(
  struct bluetooth_trans_controller *pt_trans_ctr)
{
  if(NULL == pt_trans_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }

  l_queue_destroy(pt_trans_ctr->pt_data_queue, app_com_release_data_unit);
  sem_destroy(&pt_trans_ctr->sem_data_num);
  pthread_mutex_destroy(&pt_trans_ctr->mtx);
  
  return;
}
/**
 * @brief Post data to the queue
 * @param pt_trans_ctr The BLE transfer controller
 * @param pt_dtbuf The pointer to the buffer to be sent
 * @param kplen The length (in byte) of @p pt_dtbuf
 * @return @p ST_OK when things go well
 */
app_state_t
app_com_post_to_data_trans_queue(
  struct bluetooth_trans_controller *pt_trans_ctr,
  uint8_t *pt_dtbuf,
  const uint32_t kplen)
{
  app_state_t tpret = ST_OK;
  struct data_unit *pt_dtunit = NULL;

  if((NULL == pt_trans_ctr) || (NULL == pt_dtbuf) || (0 == kplen))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  pt_dtunit = (struct data_unit *)l_malloc(sizeof(struct data_unit));
  if(NULL == pt_dtbuf)
  {
    DBG_LOG_ERR("Failed to malloc");
    goto ERR;
  }
  memset(pt_dtunit, 0, sizeof(struct data_unit));

  pt_dtunit->pt_buf = (uint8_t *)l_malloc(kplen);
  if(NULL == pt_dtunit->pt_buf)
  {
    DBG_LOG_ERR("Failed to malloc");
    goto ERR;
  }
  memset(pt_dtunit->pt_buf, 0, kplen);
  memcpy(pt_dtunit->pt_buf, pt_dtbuf, kplen);
  pt_dtunit->len = kplen;

  // Post to the queue
  pthread_mutex_lock(&pt_trans_ctr->mtx);
  l_queue_push_tail(pt_trans_ctr->pt_data_queue, pt_dtunit);
  sem_post(&pt_trans_ctr->sem_data_num);
  pthread_mutex_unlock(&pt_trans_ctr->mtx);

  return tpret;
ERR:
  if(pt_dtunit)
  {
    if(pt_dtunit->pt_buf)
    {
      l_free(pt_dtunit->pt_buf);
      pt_dtunit->pt_buf = NULL;
    }
    l_free(pt_dtunit);
    pt_dtunit = NULL;
  }

  tpret = ST_ERR;
  return tpret;
}
/**
 * @brief Wait data in a block manner and pop data from the queue.
 * @param pt_trans_ctr The BLE transfer controller
 * @return data if there exists in the queue; or NULL if the queue is empty
 *         or error happened.
 */
struct data_unit *
app_com_wait_for_data_trans_queue(
  struct bluetooth_trans_controller *pt_trans_ctr)
{
  struct data_unit *pt_dtunit = NULL;
  uint32_t num;

  if(NULL == pt_trans_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return NULL;
  }

  sem_getvalue(&pt_trans_ctr->sem_data_num, &num);
  DBG_LOG_DEBUG("pt_trans_ctr (0x%lx) The SEM_NUM %d\n", 
                pt_trans_ctr, num);
  // Wait the message
  sem_wait(&pt_trans_ctr->sem_data_num);

  // Pop the message from queue
  pthread_mutex_lock(&pt_trans_ctr->mtx);
  pt_dtunit = l_queue_pop_head(pt_trans_ctr->pt_data_queue);

  pthread_mutex_unlock(&pt_trans_ctr->mtx);

  return pt_dtunit;
}
/**
 * @brief Free the memory of data
 */
void
app_com_release_data_unit(struct data_unit *pt_dtunit)
{
  if(NULL == pt_dtunit)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }

  if(pt_dtunit->pt_buf)
  {
    l_free(pt_dtunit->pt_buf);
    pt_dtunit->pt_buf = NULL;
  }

  l_free(pt_dtunit);
}

/**
 * @brief: To initialize data structure of pthread condition. In this
 *         function, the mutex, condition, @p is_op_done, and
 *		     timeout-related would be initialized.
 */
app_state_t
app_com_init_pthread_cond_var_ctr(struct pthread_cond_var *pt_cond_var_ctr)
{
  app_state_t tpret = ST_OK;

  if(NULL == pt_cond_var_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  if(CONFIG_COND_VAR_MAGIC_INIT == pt_cond_var_ctr->magic)
  {
    DBG_LOG_WARN("cond_var initied - MAGIC!\n");
    return tpret; // init already
  }
#endif

  // Initialize the mutex first (to avoid race condition)
  if(pthread_mutex_init(&pt_cond_var_ctr->mtx, NULL))
  {
    DBG_LOG_ERR("Failed to initialize mutex");
    tpret = ST_ERR;
    return tpret;
  }

  // Initialize the thread condition signal
  if(pthread_condattr_init(&pt_cond_var_ctr->cattr))
  {
    DBG_LOG_ERR("Failed to initialize condition attribute");
    tpret = ST_ERR;
    return tpret;
  }
  if(pthread_condattr_setclock(&pt_cond_var_ctr->cattr, CLOCK_MONOTONIC))
  {
    DBG_LOG_ERR("Failed to set clock of condition attribute");
    tpret = ST_ERR;
    return tpret;
  }
  if(pthread_cond_init(&pt_cond_var_ctr->cond_var,
                       &pt_cond_var_ctr->cattr))
  {
    DBG_LOG_ERR("Failed to initialize thread condition variable");
    tpret = ST_ERR;
    return tpret;
  }

  pt_cond_var_ctr->is_op_done = false;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pt_cond_var_ctr->magic = CONFIG_COND_VAR_MAGIC_INIT;
  pt_cond_var_ctr->proxy = NULL;
  pt_cond_var_ctr->write_proxy = NULL;
#endif
  DBG_LOG_DEBUG("cond_var initied - done!\n");

  return tpret;
}
/**
 * @brief: To de-initialize data structure of pthread condition.
 */
app_state_t
app_com_deinit_pthread_cond_var_ctr(
  struct pthread_cond_var *pt_cond_var_ctr)
{
  app_state_t tpret = ST_OK;

  if(NULL == pt_cond_var_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  if(CONFIG_COND_VAR_MAGIC_DEINIT == pt_cond_var_ctr->magic)
  {
    DBG_LOG_DEBUG("deinit already ...\n");
    return tpret; // deinit already
  }
#endif
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  pt_cond_var_ctr->is_op_done = false;
  pt_cond_var_ctr->magic = CONFIG_COND_VAR_MAGIC_DEINIT;
  // Do not set to NULL here in order to use it to distinguish a 'new' proxy being set in case 'timeout'
  // same for write_proxy.
  // pt_cond_var_ctr->proxy = NULL;
#endif
  // make sure it's unlocked before destroy
  int s = 0;
  for(uint8_t i=0;i<20;i++)
  {
    s = pthread_mutex_trylock(&pt_cond_var_ctr->mtx);
    if(0 == s)
    { // normal case
      pthread_cond_signal( &pt_cond_var_ctr->cond_var);
      pthread_mutex_unlock(&pt_cond_var_ctr->mtx);
    }
    s = pthread_mutex_trylock(&pt_cond_var_ctr->mtx);
    if(0 == s)
    {
      pthread_cond_destroy(&pt_cond_var_ctr->cond_var);
      pthread_mutex_unlock(&pt_cond_var_ctr->mtx);
      pthread_mutex_destroy(&pt_cond_var_ctr->mtx);
      DBG_LOG_DEBUG("mutex destroied!-- %d\n", i);
      break;
    }
    sleep(1);
  }
  if(EBUSY == s)
  {
    DBG_LOG_ERR("fail to lock cond_var!\n");
    exit(EXIT_FAILURE);
  }
  return tpret;
}

// Disabled as we are using the IPC communication in a block manner
#if 0
static struct ipc_msg_sender g_msg_sender_for_ble_to_gen = { 0 };
static struct ipc_msg_sender g_msg_sender_for_gen_to_db = { 0 };

/**
 * @brief To get the IPC message sending handler (from BLE to General)
 * /
struct ipc_msg_sender *
app_com_get_ipc_msg_sender_for_ble_to_gen(void)
{
  struct ipc_msg_sender *ptret = &g_msg_sender_for_ble_to_gen;
  return ptret;
}
/**
 * @brief To get the IPC message sending handler (from General to database)
 * /
struct ipc_msg_sender *
app_com_get_ipc_msg_sender_for_gen_to_db(void)
{
  struct ipc_msg_sender *ptret = &g_msg_sender_for_gen_to_db;
  return ptret;
}
/**
 * @brief Initialize the IPC sender handler
 * @param pt_msg_sender The handler to be initialized
 * @param msgid The IPC message ID
 * @param sz_msgunit The size of message
 * @return: @p ST_OK if everything is OK.
 */
app_state_t
app_com_init_ipc_msg_sender(
  struct ipc_msg_sender *pt_msg_sender,
  int msgid,
  uint32_t sz_msgunit)
{
  app_state_t tpret = ST_OK;
  int tpint = 0;

  if((NULL == pt_msg_sender) || (sz_msgunit <= 0))
  {
    tpret = ST_ERR;
    DBG_LOG_ERR("Unexpected NULL");
    return tpret;
  }

  pt_msg_sender->msgid_for_sending = msgid;

  pt_msg_sender->hld_sz_msgunit = sz_msgunit;
  pt_msg_sender->pt_msg_to_be_sent = NULL;

  tpint = pthread_mutex_init(&pt_msg_sender->mtx, NULL);
  if(tpint != 0)
  {
    tpret = ST_ERR;
    DBG_LOG_ERR("Failed to initialize mutex, for %s", strerror(tpint));
    return tpret;
  }

  pt_msg_sender->msg_waiting_queue = l_queue_new();
  if(NULL == pt_msg_sender->msg_waiting_queue)
  {
    tpret = ST_ERR;
    DBG_LOG_ERR("Failed to create queue");
    return tpret;
  }

  return tpret;
}
/**
 * @brief De-initialize the IPC sender handler
 * @param pt_msg_sender The handler to be de-initialized
 * @return: @p ST_OK if everything is OK.
 */
app_state_t
app_com_deinit_ipc_msg_sender(struct ipc_msg_sender *pt_msg_sender)
{
  app_state_t tpret = ST_OK;

  if(NULL == pt_msg_sender)
  {
    tpret = ST_ERR;
    DBG_LOG_ERR("Unexpected NULL");
    return tpret;
  }

  // Destroy the mutex
  pthread_mutex_destroy(&pt_msg_sender->mtx);

  // And free the queue
  l_queue_destroy(pt_msg_sender->msg_waiting_queue, l_free);
  pt_msg_sender->msg_waiting_queue = NULL;

  // Free the memory for the message to be sent
  if(pt_msg_sender->pt_msg_to_be_sent)
  {
    l_free(pt_msg_sender->pt_msg_to_be_sent);
    pt_msg_sender->pt_msg_to_be_sent = NULL;
  }
}
/**
 * @brief To send IPC messages in a non-block manner. Whenever this
 *        function is called, current message pointed by @p pt_msg would be
 *        attempted to be sent. Then messages in the queue (which have not
 *        been sent successfully) would be attempted to be sent. Once a
 *        message (in the queue) has been sent successfully, the message
 *        would be removed from the queue. In this way, this function
 *        guarantees the reliablity.
 * @param pt_msg_sender The handler to be initialized
 * @param pt_msg Pointer to the message to be sent
 * @param msg_size The size of message in byte (which should be identical
 *        to @p hld_sz_msgunit in @p pt_msg_sender )
 * @return: @p ST_OK if everything is OK.
 */
app_state_t
app_com_send_ipc_msg(
  struct ipc_msg_sender *pt_msg_sender,
  void *pt_msg,
  uint32_t msg_size)
{
  app_state_t tpret = ST_OK;

  uint8_t *pt_dtbuf = NULL;

  if(NULL == pt_msg_sender)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }
  if((pt_msg) && (pt_msg_sender->hld_sz_msgunit != msg_size))
  {
    DBG_LOG_ERR("Unexpected message size");
    tpret = ST_ERR;
    return tpret;
  }

  // Put msg on waiting queue
  do {
    if(NULL == pt_msg)
    {
      break;
    }
    pt_dtbuf = (uint8_t *)l_malloc(msg_size);
    if(NULL == pt_dtbuf)
    {
      DBG_LOG_ERR("Failed to malloc");
      break;
    }
    memset(pt_dtbuf, 0, msg_size);
    memcpy(pt_dtbuf, pt_msg, msg_size);

    pthread_mutex_lock(&pt_msg_sender->mtx);
    l_queue_push_tail(pt_msg_sender->msg_waiting_queue, pt_dtbuf);
    pthread_mutex_unlock(&pt_msg_sender->mtx);
  } while(0);

  DBG_LOG_DEBUG(
    "Number of message waiting on queue %d, msg_to_be_sent @ 0x%lx\n",
    l_queue_length(pt_msg_sender->msg_waiting_queue),
    pt_msg_sender->pt_msg_to_be_sent);

  // Try to send IPC message (If there is something to be sent)
  // Otherwise try to send IPC message in the queue (if there exists)
  pthread_mutex_lock(&pt_msg_sender->mtx);
  if(NULL != pt_msg_sender->pt_msg_to_be_sent)
  {
    if(0 !=
       msgsnd(pt_msg_sender->msgid_for_sending,
              pt_msg_sender->pt_msg_to_be_sent,
              pt_msg_sender->hld_sz_msgunit,
              IPC_NOWAIT))
    {
      // Failed to send the IPC message which is perhaps caused by
      // "Resource temporarily unavailable". When "Resource temporarily
      // unavailable" happens, actually we can try to send it again in a
      // while.
      DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
      pthread_mutex_unlock(&pt_msg_sender->mtx);
      tpret = ST_ERR;
      goto EXIT;
    }
    else
    {
      l_free(pt_msg_sender->pt_msg_to_be_sent);
      pt_msg_sender->pt_msg_to_be_sent = NULL;
    }
  }
  pthread_mutex_unlock(&pt_msg_sender->mtx);

  while(1)
  {
    pthread_mutex_lock(&pt_msg_sender->mtx);
    pt_msg_sender->pt_msg_to_be_sent = l_queue_pop_head(
      pt_msg_sender->msg_waiting_queue);
    if(NULL == pt_msg_sender->pt_msg_to_be_sent)
    {
      pthread_mutex_unlock(&pt_msg_sender->mtx);
      break;
    }

    if(0 !=
       msgsnd(pt_msg_sender->msgid_for_sending,
              pt_msg_sender->pt_msg_to_be_sent,
              pt_msg_sender->hld_sz_msgunit,
              IPC_NOWAIT))
    {
      // Failed to send the IPC message which is perhaps caused by
      // "Resource temporarily unavailable". When "Resource temporarily
      // unavailable" happens, actually we can try to send it again in a
      // while.
      DBG_LOG_WARN("msgsnd failure, for %s\r\n", strerror(errno));
      pthread_mutex_unlock(&pt_msg_sender->mtx);
      tpret = ST_ERR;
      goto EXIT;
    }
    else
    {
      l_free(pt_msg_sender->pt_msg_to_be_sent);
      pt_msg_sender->pt_msg_to_be_sent = NULL;
    }
    pthread_mutex_unlock(&pt_msg_sender->mtx);
  }

EXIT:
  return tpret;
}
#endif 
/**
 * @brief: To configure the network (i.e., static or DHCP, static IP
 *         address, mask, DNS address, and gateway)
 * @return: @p ST_OK if everything is OK.
 */
app_state_t
app_com_set_network_configuration(
  const gwconf_and_dev_t *pt_gwconf_and_dev)
{
  app_state_t tpret = ST_OK;

  int32_t tp_int32 = 0;
  // A valid IP string would not be longer than 16 bytes
  uint8_t kp_ipstr[30] = { 0 };
 // uint8_t kp_mskstr[16] = { 0 };
  uint8_t kp_dnstr[16] = { 0 };
  uint8_t kp_gwstr[16] = { 0 };
  uint8_t kp_linebuf[0x200] = { 0 };
  uint8_t kp_cmdstr[0x200] = { 0 };
  bool dhcpEnabled = false;

  if(NULL == pt_gwconf_and_dev)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  // Read the IP information
  pthread_mutex_lock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
  if(pt_gwconf_and_dev->
     gw_conf_info.gwConfig.ipConfig.DHCPEnable != true)
  {
    dhcpEnabled = false;
    uint8_t cidr = 0;
    do{
      if((pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.netMask.addr.addrArray[cidr / 8] >> (7 - (cidr % 8))) & 0x01)
      {
        cidr++;
      }else{
        break;
      }
    }while(cidr < 32);
    snprintf(kp_ipstr,
             sizeof(kp_ipstr),
             "%d.%d.%d.%d/%d",
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.staticIPAddr.addr.addrArray[0],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.staticIPAddr.addr.addrArray[1],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.staticIPAddr.addr.addrArray[2],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.staticIPAddr.addr.addrArray[3],
             cidr);
    /*snprintf(kp_mskstr,
             sizeof(kp_mskstr),
             "%d.%d.%d.%d",
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.netMask.addr.addrArray[0],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.netMask.addr.addrArray[1],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.netMask.addr.addrArray[2],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.netMask.addr.addrArray[3]);*/
    snprintf(kp_dnstr,
             sizeof(kp_dnstr),
             "%d.%d.%d.%d",
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.dnsServer.addr.addrArray[0],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.dnsServer.addr.addrArray[1],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.dnsServer.addr.addrArray[2],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.dnsServer.addr.addrArray[3]);
    snprintf(kp_gwstr,
             sizeof(kp_gwstr),
             "%d.%d.%d.%d",
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.gatewayAddr.addr.addrArray[0],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.gatewayAddr.addr.addrArray[1],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.gatewayAddr.addr.addrArray[2],
             pt_gwconf_and_dev->
             gw_conf_info.gwConfig.ipConfig.gatewayAddr.addr.addrArray[3]);
  }
  else
  {
    dhcpEnabled = true;
  }
  pthread_mutex_unlock(
    &pt_gwconf_and_dev->mtx_for_gw_conf_info);

  if(dhcpEnabled)
  {
    // Remove all the configuration files containing "eth1"
    memset(kp_cmdstr, 0, sizeof(kp_cmdstr));
    snprintf(kp_cmdstr, sizeof(kp_cmdstr),
             "cd /etc/systemd/network && grep -l \"eth1\" * | xargs rm\n");
    DBG_LOG_INFO("Execute %s", kp_cmdstr);
    tp_int32 = system(kp_cmdstr);
    if(WIFEXITED(tp_int32))
    {
      if(0 == WEXITSTATUS(tp_int32))
      {
        DBG_LOG_INFO("Execute the command successfully");
      }
      else
      {
        DBG_LOG_INFO(
          "Failed to execute the command (maybe there's nothing to clean), %d",
          WEXITSTATUS(tp_int32));
      }
    }
    else
    {
      DBG_LOG_ERR("Failed to execute the command, %d",
                   WEXITSTATUS(tp_int32));
      tpret = ST_ERR;
      goto EXIT;
    }

    DBG_LOG_INFO("DHCP has been enabled successfully");
  }
  else
  {
    // First, remove all the configuration files containing "eth1"
    memset(kp_cmdstr, 0, sizeof(kp_cmdstr));
    snprintf(kp_cmdstr, sizeof(kp_cmdstr),
             "cd /etc/systemd/network && grep -l \"eth1\" * | xargs rm\n");
    DBG_LOG_INFO("Execute %s", kp_cmdstr);
    tp_int32 = system(kp_cmdstr);
    if(WIFEXITED(tp_int32))
    {
      if(0 == WEXITSTATUS(tp_int32))
      {
        DBG_LOG_INFO("Execute the command successfully");
      }
      else
      {
        DBG_LOG_ERR(
          "Failed to execute the command (maybe there's nothing to clean), %d",
          WEXITSTATUS(tp_int32));
      }
    }
    else
    {
      DBG_LOG_INFO("Failed to execute the command, %d",
                   WEXITSTATUS(tp_int32));
      tpret = ST_ERR;
      goto EXIT;
    }
    // Then, generate /etc/systemd/network/10-eth.network
    memset(kp_linebuf, 0, sizeof(kp_linebuf));
    snprintf(kp_linebuf,
             sizeof(kp_linebuf),
             "[Match]\nName=eth1\nKernelCommandLine=!root=/dev/nfs\n\n"
             "[Network]\nAddress=%s\n"
             "Gateway=%s\nDNS=%s\nConfigureWithoutCarrier=true\n"
             "IgnoreCarrierLoss=true\n",
             kp_ipstr, 
             kp_gwstr,
             kp_dnstr);
    memset(kp_cmdstr, 0, sizeof(kp_cmdstr));
    snprintf(kp_cmdstr, sizeof(kp_cmdstr),
             "echo \"%s\" > /etc/systemd/network/10-eth.network",
             kp_linebuf);
    DBG_LOG_INFO("Execute %s", kp_cmdstr);
    tp_int32 = system(kp_cmdstr);
    if(tp_int32 != 0)
    {
      DBG_LOG_ERR("Failed to execute %s",
                  kp_cmdstr);
      tpret = ST_ERR;
      goto EXIT;
    }

    DBG_LOG_INFO("DHCP has been disabled successfully");
  }

EXIT:
  return tpret;
}
/**
 * @brief: To configure the NTP service (i.e., to start the system
 *         time-sychronization service and enable date synchronized with
 *         the NTP server).
 * @return: @p ST_OK if everything is OK.
 */
app_state_t
app_com_setup_system_timesyncd(const gwconf_and_dev_t *pt_gwconf_and_dev)
{
  app_state_t tpret = ST_OK;

  int32_t tp_int32 = 0;
  char kp_ntp_server[0x100];
  char kp_cmdstr[0x1000];
  char kp_linebuf[0x100];

  if(NULL == pt_gwconf_and_dev)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  if(pt_gwconf_and_dev->gw_conf_info.gwConfig.timeConfig.timeSetting ==
     GWCONFIG_TIMECONFIG_TIMESETTING_VIA_NTP)
  {
    // Need to sync via NTP

    // Generate NTP URL
    pthread_mutex_lock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
    memset(kp_ntp_server, sizeof(kp_ntp_server), 0);
    snprintf(kp_ntp_server, sizeof(kp_ntp_server), "%s",
             pt_gwconf_and_dev->gw_conf_info.gwConfig.timeConfig.timeNtpUrl);
    pthread_mutex_unlock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
    DBG_LOG_INFO("Set the NTP server to %s", kp_ntp_server);

    // Generate the service configuration
    memset(kp_linebuf, 0, sizeof(kp_linebuf));
    snprintf(kp_linebuf, sizeof(kp_linebuf),
             "[Time]\nNTP=%s\nFallbackNTP=ntp.tencent.com\nRootDistanceMaxSec=20\n",
             kp_ntp_server);
    DBG_LOG_DEBUG("The service configuration is\n%s", kp_linebuf);

    // Write to timesyncd.conf
    memset(kp_cmdstr, 0, sizeof(kp_cmdstr));
    snprintf(kp_cmdstr, sizeof(kp_cmdstr),
             "echo \"%s\" > /etc/systemd/timesyncd.conf", kp_linebuf);
    DBG_LOG_DEBUG("Write to timesyncd.conf\n %s", kp_cmdstr);
    tp_int32 = system(kp_cmdstr);
    if(tp_int32 != 0)
    {
      DBG_LOG_ERR("Failed to execute %s", kp_cmdstr);
      tpret = ST_ERR;
      goto EXIT;
    }

    // To (re-)start the NTP services
    memset(kp_cmdstr, 0, sizeof(kp_cmdstr));
    snprintf(kp_cmdstr, sizeof(kp_cmdstr), "%s",
             "systemctl stop systemd-timesyncd");
    DBG_LOG_DEBUG("Execute %s", kp_cmdstr);
    tp_int32 = system(kp_cmdstr);
    if(tp_int32 != 0)
    {
      DBG_LOG_ERR("Failed to execute %s", kp_cmdstr);
      tpret = ST_ERR;
      goto EXIT;
    }
    memset(kp_cmdstr, 0, sizeof(kp_cmdstr));
    snprintf(kp_cmdstr, sizeof(kp_cmdstr), "%s",
             "systemctl start systemd-timesyncd");
    DBG_LOG_INFO("Execute %s", kp_cmdstr);
    tp_int32 = system(kp_cmdstr);
    if(tp_int32 != 0)
    {
      DBG_LOG_ERR("Failed to execute %s", kp_cmdstr);
      tpret = ST_ERR;
      goto EXIT;
    }

    // To enable date synced with NTP
    memset(kp_cmdstr, 0, sizeof(kp_cmdstr));
    snprintf(kp_cmdstr, sizeof(kp_cmdstr), "%s",
             "timedatectl set-ntp true");
    DBG_LOG_INFO("Execute %s", kp_cmdstr);
    tp_int32 = system(kp_cmdstr);
    if(tp_int32 != 0)
    {
      DBG_LOG_ERR("Failed to execute %s", kp_cmdstr);
      tpret = ST_ERR;
      goto EXIT;
    }

    DBG_LOG_INFO("NTP has been enabled successfully");
  }
  else
  {
    // Otherwise, do nothing

    // To disable date synced with NTP
    memset(kp_cmdstr, 0, sizeof(kp_cmdstr));
    snprintf(kp_cmdstr, sizeof(kp_cmdstr), "%s",
             "timedatectl set-ntp false");
    DBG_LOG_INFO("Execute %s", kp_cmdstr);
    tp_int32 = system(kp_cmdstr);
    if(tp_int32 != 0)
    {
      DBG_LOG_ERR("Failed to execute %s", kp_cmdstr);
      tpret = ST_ERR;
      goto EXIT;
    }

    DBG_LOG_INFO("NTP has been disabled successfully");
  }

EXIT:
  return tpret;
}

/**
 * @brief Get disk available space (MB)
 * @param pDisk The input path
 * @param availSpaceMB Returned available space in MB
 * @return @p ST_OK if everthing is OK.
 */
app_state_t
DH_GetDiskAvailSpaceMB(
  char *pDisk,
  uint64_t *availSpaceMB) {
  app_state_t tpret = ST_OK;

  uint64_t _availspaceMB = 0;
  uint64_t _totalspaceMB = 0;
  struct statfs disk_statfs;
  float _availSpacePercent = 0;

  if(statfs(pDisk, &disk_statfs) == 0)
  {
    _totalspaceMB =
      (((uint64_t)disk_statfs.f_bsize * (uint64_t)disk_statfs.f_blocks) /
       (uint64_t)1048576);
    _availspaceMB =
      (((uint64_t)disk_statfs.f_bsize * (uint64_t)disk_statfs.f_bavail) /
       (uint64_t)1048576);
    _availSpacePercent = ((float)_availspaceMB / (float)_totalspaceMB) *
                         100;

    DBG_LOG_INFO("Space info: Total %lld MB, Avail %lld MB (%f%%)",
                  _totalspaceMB,
                  _availspaceMB,
                  _availSpacePercent);
  }
  else
  {
    DBG_LOG_WARN("Failed to get free space");
    return ST_ERR;
  }

  if(availSpaceMB != NULL)
  {
    *availSpaceMB = _availspaceMB;
  }
  return tpret;
}

#if 0 // Do NOT support timezone configuration

#ifndef NUM_TIM_ZONE
#define NUM_TIM_ZONE (25U)
#endif

#ifndef SZ_TIMEZONE_INFO
#define SZ_TIMEZONE_INFO (0x100)
#endif

struct time_zone_info
{
  uint32_t info_len;  // Timezone file length
  uint8_t *zone_nmstr;  // Timezone name
  uint8_t dtbuf[SZ_TIMEZONE_INFO];  // Timezone content
};

// TODO: Only +8 has been filled
const struct time_zone_info g_time_zone_array[NUM_TIM_ZONE] = {
  { 0 },  //-12
  { 0 },  //-11
  { 0 },  //10
  { 0 },  //9
  { 0 },  //8
  { 0 },  //7
  { 0 },  //6
  { 0 },  //5
  { 0 },  //4
  { 0 },  //3
  { 0 },  //2
  { 0 },  //-1
  { 0 },  //+/-0
  { 0 },  //+1
  { 0 },  //2
  { 0 },  //3
  { 0 },  //4
  { 0 },  //5
  { 0 },  //6
  { 0 },  //+7
  { .info_len = 117,
    .zone_nmstr = "GMT-8",
    .dtbuf =
    { 0x54, 0x5A, 0x69, 0x66, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x70, 0x80, 0x00, 0x00,
      0x2B, 0x30, 0x38, 0x00, 0x54, 0x5A, 0x69, 0x66, 0x32, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
      0x70, 0x80, 0x00, 0x00, 0x2B, 0x30, 0x38, 0x00, 0x0A, 0x3C,
      0x2B, 0x30, 0x38, 0x3E, 0x2D, 0x38, 0x0A },  // E8
  },  //+8
  { 0 },  //9
  { 0 },  //10
  { 0 },  //11
  { 0 },  //12
};

/**
 * @brief To set up the time zone
 */
app_state_t
app_com_set_time_zone(const enum tim_zone_idx zone_idx)
{
  app_state_t tpret = ST_OK;
  struct time_zone_info *pt_zone_info = NULL;
  uint8_t kp_strbuf[0x100] = { 0 };
  int32_t tp_int32 = 0;
  int32_t kp_fd = 0;

  pt_zone_info = &g_time_zone_array[zone_idx % NUM_TIM_ZONE];
  DBG_LOG_INFO("Try to set time-zone to %d(%s)", zone_idx,
               pt_zone_info->zone_nmstr);

  // To create a directory and its parent directories if they do not exist
  tp_int32 = system("mkdir -p /usr/share/zoneinfo/Etc/");
  if(tp_int32 != 0)
  {
    DBG_LOG_ERR("Failed to execute mkdir -p /usr/share/zoneinfo/Etc/");
    tpret = ST_ERR;
    goto EXIT;
  }

  if((strlen(pt_zone_info->zone_nmstr) <= 0) ||
     (pt_zone_info->info_len <= 0))
  {
    DBG_LOG_ERR("Invalid time-zone info");
    tpret = ST_ERR;
    goto EXIT;
  }

  memset(kp_strbuf, 0, sizeof(kp_strbuf));
  snprintf(kp_strbuf,
           sizeof(kp_strbuf),
           "/usr/share/zoneinfo/Etc/%s",
           pt_zone_info->zone_nmstr);
  if(access(kp_strbuf, F_OK) != 0)
  {
    // If the file does NOT exist
    DBG_LOG_INFO("To create file %s", kp_strbuf);
    kp_fd = open(kp_strbuf, O_CREAT | O_RDWR | O_TRUNC,
                 S_IRWXU | S_IRWXG | S_IRWXO);
    if(kp_fd < 0)
    {
      DBG_LOG_ERR("Failed to open file, for %s", strerror(errno));
      tpret = ST_ERR;
      goto EXIT;
    }
    if((pt_zone_info->info_len > 0) &&
       (write(kp_fd, pt_zone_info->dtbuf, pt_zone_info->info_len) < 0))
    {
      DBG_LOG_ERR("Failed to write time zone info to the file, for %s",
                  strerror(errno));
      tpret = ST_ERR;

      close(kp_fd);
      goto EXIT;
    }
    close(kp_fd);
  }

  // To set the timezone
  memset(kp_strbuf, 0, sizeof(kp_strbuf));
  snprintf(kp_strbuf, sizeof(kp_strbuf), "timedatectl set-timezone Etc/%s",
           pt_zone_info->zone_nmstr);
  DBG_LOG_INFO("To execute %s", kp_strbuf);
  tp_int32 = system(kp_strbuf);
  if(0 != tp_int32)
  {
    DBG_LOG_ERR("Failed to set time-zone");
    tpret = ST_ERR;
    goto EXIT;
  }

EXIT:
  return tpret;
}

#endif





/* be called to initialize the GPIO for LTE module control
ret@ error_code
*/

app_state_t app_com_init_gpio_for_lte_ctr(void)
{
    app_state_t tpret = ST_OK;
 
    if(access("/sys/class/gpio/gpio86/value",
              F_OK) != 0)
    {
        DBG_LOG_ERR(
            "GPIO86 LTE control is not initialized");
 
        tpret = ST_ERR;
    }
 
    return tpret;
}

/*to turn on/off the LTE power supply
nstate@ true - power on; false - power off
ret@error_code
*/
app_state_t app_com_set_lte_power_supply(const bool nstate)
{
	app_state_t tpret = ST_OK;
	char *cmdstr = "echo %d > /sys/class/gpio/gpio86/value";
	char strbuf[128] = {0};
	if(nstate)
	{
		snprintf( strbuf,sizeof(strbuf), cmdstr, 1);
	}
	else
	{
		snprintf( strbuf,sizeof(strbuf), cmdstr, 0);
	}

	if(0 != system(strbuf))
	{
		tpret = ST_ERR;
		DBG_LOG_ERR("fail to execute: %s", strbuf);
	}
	return tpret;
}


#ifndef LTE_USB_IVENDOR	
	#define LTE_USB_IVENDOR	(0x2C7C)
#endif

#ifndef LTE_USB_IPRODUCT
	#define LTE_USB_IPRODUCT	(0x0125)
#endif

/*to check is the LTE module is attached to the system
ret@bool
*/
bool app_com_is_lte_module_attached(void)
{
	bool tpret = false;
	libusb_context *pt_context = NULL;
	libusb_device **ptlist;
	ssize_t kpcnt = 0;
	size_t kpidx = 0;
	
	struct libusb_device *ptdev;
	struct libusb_device_descriptor kpdes;
	char chbuf[0x100] = {0};
	struct libusb_device_handle *pthandle;

	
	//to check if the LTE module is attached or not
	if(0 != libusb_init( &pt_context))
	{
		DBG_LOG_ERR("libusb_init fail");
		tpret = false;
		goto ERR;
	}

	kpcnt = libusb_get_device_list( pt_context, &ptlist);
	for(kpidx = 0; kpidx < kpcnt; kpidx++)
	{
		ptdev = ptlist[kpidx];
		if(0 != libusb_get_device_descriptor( ptdev, &kpdes))
		{
			DBG_LOG_ERR("fail to get USB desciptor");
			continue;
		}
		if(0 != libusb_open( ptdev, &pthandle))
		{
			DBG_LOG_ERR("libusb_open fail");
			continue;
		}

		// the USB INFO
		DBG_LOG_INFO("idx %d device:0x%04X:0x%04X", kpidx, kpdes.idVendor, kpdes.idProduct);

		memset( chbuf, 0, sizeof(chbuf));
		if(libusb_get_string_descriptor_ascii( pthandle, kpdes.iManufacturer, (unsigned char *)chbuf, sizeof(chbuf)) < 0)
		{
			//libusb_close( pthandle);
			DBG_LOG_ERR("fail to get usb manufacture info");
			//continue;
		}
		else
		{
			DBG_LOG_INFO("manufacture:%s", chbuf);
		}
		//
		memset( chbuf, 0, sizeof(chbuf));
		if( libusb_get_string_descriptor_ascii( pthandle, kpdes.iProduct, (unsigned char *)chbuf, sizeof(chbuf)) < 0)
		{
			//libusb_close( pthandle);
			DBG_LOG_ERR("fail to get product info");
			//continue;
		}
		else
		{
			DBG_LOG_INFO("product:%s", chbuf);	
		}
		libusb_close( pthandle);

		DBG_LOG_INFO("ivendor 0x%x vs 0x%x iproduct 0x%x vs 0x%x", kpdes.idVendor, LTE_USB_IVENDOR, kpdes.idProduct, LTE_USB_IPRODUCT);
		//if((LTE_USB_IVENDOR == kpdes.idVendor) && (LTE_USB_IPRODUCT == kpdes.idProduct))
		//if(LTE_USB_IVENDOR == kpdes.idVendor)
		//tpu16 = kpdes.idProduct;
		//DBG_LOG_ERR("%d vs %d B %d", LTE_USB_IPRODUCT, kpdes.idProduct, (LTE_USB_IPRODUCT == kpdes.idProduct) ? 1 : 0);
		//if(0x2 == kpdes.idProduct)
		if((LTE_USB_IVENDOR == kpdes.idVendor) && (LTE_USB_IPRODUCT == kpdes.idProduct))
		{
			tpret = true;
			DBG_LOG_WARN("the LTE module is attached to the system");
			break;
		}
	}

	// to deinitialized the libusb
	libusb_exit(pt_context);
	
	return tpret;
	ERR:
		if(pt_context)
		{
			libusb_exit(pt_context);
		}
		return tpret;
}




/* set LTE 4G to connection/disconnection
nstate@true -  connect LTE network; false - disconnect from LTE network
ret@error_code
*/

#define CMD_STR_LAUNCH_QCM	"tpid=`pidof quectel-CM` \n echo \"${tpid}\" \n if [ \"${tpid}\" != \"\" ] \n then \n echo \"quectel-CM is running\" \n else \n /usr/bin/quectel-CM & \n fi"
#define CMD_STR_KILL_QCM	"tpid=`pidof quectel-CM` \n echo \"${tpid}\" \n if [ \"${tpid}\" != \"\" ] \n then \n killall -s 9 quectel-CM \n else \n echo \"no quectel-CM to be killed\" \n fi"

app_state_t app_com_set_lte_con_state(const bool nstate)
{
	app_state_t tpret = ST_OK;

	if(true != app_com_is_lte_module_attached())
	{
		DBG_LOG_ERR("the LTE module is not attached yet");
		tpret = ST_ERR;
		return tpret;
	}


	if(nstate)
	{
		DBG_LOG_DEBUG("to execute:%s", CMD_STR_LAUNCH_QCM);
		if(0 != system(CMD_STR_LAUNCH_QCM))
		{
			DBG_LOG_ERR("system fail");
			tpret = ST_ERR;
		}
	}
	else
	{
		DBG_LOG_DEBUG("to execute:%s", CMD_STR_KILL_QCM);
		if(0 != system(CMD_STR_KILL_QCM))
		{
			DBG_LOG_ERR("system fail");
			tpret = ST_ERR;
		}
	}

	//............

	return tpret;
}

#if(0)
enum net_id{
	NET_UNKNOWN = 0,
	NET_ETH0 = 1,
	NET_ETH1 = 2,
	NET_LTE = 3,
	NET_WIFI = 4,
};
#endif

/* to pick the network with better quality
ret@net_id
*/
enum net_id app_com_get_the_best_network(void)
{
	enum net_id tpret = NET_UNKNOWN;

	//.........

	return tpret;
}



#define CMD_STR_GET_WWAN_GWIP "tpoutput=/tmp/udhcpc_output; tpscript=\"/tmp/udhcpc.sh\"; echo \" udhcpc -f -n -q -t 5 -i wwan0 2>&1\" > $tpscript; sh $tpscript > $tpoutput; sed -nr \'/server/ s/.*server([^\"]+).*/\\1/p\' $tpoutput"
#define CMD_STR_GET_ETH0_GWIP "tpoutput=/tmp/udhcpc_output; tpscript=\"/tmp/udhcpc.sh\"; echo \" udhcpc -f -n -q -t 5 -i eth0 2>&1\" > $tpscript; sh $tpscript > $tpoutput; sed -nr \'/server/ s/.*server([^\"]+).*/\\1/p\' $tpoutput"
#define CMD_STR_GET_ETH1_GWIP "tpoutput=/tmp/udhcpc_output; tpscript=\"/tmp/udhcpc.sh\"; echo \" udhcpc -f -n -q -t 5 -i eth1 2>&1\" > $tpscript; sh $tpscript > $tpoutput; sed -nr \'/server/ s/.*server([^\"]+).*/\\1/p\' $tpoutput"
#define CMD_STR_GET_WIFI_GWIP "tpoutput=/tmp/udhcpc_output; tpscript=\"/tmp/udhcpc.sh\"; echo \" udhcpc -f -n -q -t 5 -i mlan0 2>&1\" > $tpscript; sh $tpscript > $tpoutput; sed -nr \'/server/ s/.*server([^\"]+).*/\\1/p\' $tpoutput"


/*set up the route, let the data transportation go throuth the network which is the best
ret@error_code
*/
app_state_t app_com_set_up_route_for_dhcp(const enum net_id net)
{
	app_state_t tpret = ST_OK;
	FILE *ptfile = NULL;
	uint8_t strbuf[0x100] = {0};
	uint8_t *pt_cmdstr = NULL;

	switch(net)
	{
		case NET_ETH0:
		{
			pt_cmdstr = CMD_STR_GET_ETH0_GWIP;
			break;
		}
		case NET_ETH1:
		{
			pt_cmdstr = CMD_STR_GET_ETH1_GWIP;
			break;
		}
		case NET_LTE:
		{
			pt_cmdstr = CMD_STR_GET_WWAN_GWIP;
			break;
		}
		case NET_WIFI:
		{
			pt_cmdstr = CMD_STR_GET_WIFI_GWIP;
			break;
		}
		default:
		{
			DBG_LOG_ERR("unknown network interface");
			tpret = ST_ERR;
			goto EXIT;
			break;
		}
	}
	

	DBG_LOG_INFO("to execute:%s", pt_cmdstr);
	ptfile = popen( pt_cmdstr, "r");
	if(NULL == ptfile)
	{
		DBG_LOG_ERR("popen fail");
		tpret = ST_ERR;
		goto EXIT;
	}

	if(fgets((void*)strbuf, sizeof(strbuf),ptfile) <= 0)
	{
		DBG_LOG_ERR("fread fail , for %s", strerror(errno));
		tpret = ST_ERR;
		goto EXIT;
	}
	DBG_LOG_INFO("gwip : %s, len %d", strbuf, strlen(strbuf));
	for(uint32_t i = (strlen(strbuf) - 1);i > 0; i--)
	{
		if((strbuf[i] != '\n')&&(strbuf[i] != '\n'))
		{
			break;
		}
		strbuf[i] = 0;
	}

	// to set up the route
	if(ST_OK != app_com_set_up_default_route( net, strbuf))
	{
		DBG_LOG_ERR("app_com_set_up_default route fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	EXIT:
	if(ptfile)
	{
		pclose(ptfile);
		ptfile = NULL;
	}
	return tpret;
}


#ifndef NET_ETH0_INAME	// netowrk interface name 
	#define NET_ETH0_INAME "eth0"
#endif
#ifndef NET_ETH1_INAME	
	#define NET_ETH1_INAME	"eth1"
#endif
#ifndef NET_LTE_INAME	
	#define NET_LTE_INAME	"wwan0"
#endif
#ifndef NET_WIFI_INAME	
	#define NET_WIFI_INAME	"mlan0"
#endif

/*to set up the default route
net@the network for data transportation
pt_gwip@the gateway IP for the network chosen
ret@app_state_t
*/
app_state_t app_com_set_up_default_route( const enum net_id net, const char*pt_gwip)
{
	app_state_t tpret = ST_OK;
	const char*pt_net_iname = NULL;
	const char*hld_cmdstr_add_defroute = "route add default gw %s %s";
	const char*hld_cmdstr_del_defroute = "route del default";
	char strbuf[0x100] = {0};
	if(NULL == pt_gwip)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	if(strlen(pt_gwip) < 7)
	{//0.0.0.0 -> 7
	//Actually, more validation is expected here to make sure the GW ip string is valid. Too lazy to do it(~ _ ~)
		DBG_LOG_ERR("invalid GW IP str");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	switch(net)
	{
	 	case NET_ETH0:
	 	{
	 		pt_net_iname = NET_ETH0_INAME;
	 		break;
	 	}
		case NET_ETH1:
		{
			pt_net_iname = NET_ETH1_INAME;
			break;
		}
		case NET_LTE:
		{
			pt_net_iname = NET_LTE_INAME;
			break;
		}
		case NET_WIFI:
		{
			pt_net_iname = NET_WIFI_INAME;
			break;
		}
		default:
		{
			DBG_LOG_ERR("unknown network");
			tpret = ST_ERR;
			goto EXIT;
		}
	}

	//to delete the default first
	DBG_LOG_WARN("to execute : %s", hld_cmdstr_del_defroute);
	if(0 != system(hld_cmdstr_del_defroute))
	{
		DBG_LOG_ERR("fail to delete the default route");
		tpret = ST_ERR;
		goto EXIT;
	}
	//
	snprintf( strbuf, sizeof(strbuf), hld_cmdstr_add_defroute, pt_gwip, pt_net_iname);
	DBG_LOG_WARN("to execute:%s", strbuf);
	if(0 != system(strbuf))
	{
		DBG_LOG_ERR("fail to add default route");
		tpret = ST_ERR;
		goto EXIT;
	}
	DBG_LOG_INFO("the default route is modified successfully");
	EXIT:
		return tpret;
}



/*to add interface wwan0 to the management of service systemd-networkd
ret@app_state_t
*/
app_state_t app_com_set_wwan0_managed(void)
{
	app_state_t tpret = ST_OK;
	const uint8_t *pt_fpath = "/etc/systemd/network/20-wwan0.network";
	const uint8_t *pt_confinfo = "[Match]\nName=wwan*\n[Network]\nDHCP=no\nIgnoreCarrierLoss=yes\n[Link]\nRequiredForOnline=no\n";
	const uint8_t *pt_cmdstr = "chmod 0644 /etc/systemd/network/20-wwan0.network ;  systemctl restart systemd-networkd";
	int32_t kpfd = -1;

	// to write the configuration file
	kpfd = open( pt_fpath, O_CREAT|O_RDWR, 0644);
	if(kpfd < 0)
	{
		DBG_LOG_ERR("fail to open %s, for %s", pt_fpath, strerror(errno))
		tpret = ST_ERR;
		return tpret;
	}
	write(kpfd, pt_confinfo, strlen(pt_confinfo));
	close(kpfd);
	// to restart the service systemd-networkd
	DBG_LOG_INFO("to execute: %s", pt_cmdstr);
	if(0 != system(pt_cmdstr))
	{
		DBG_LOG_ERR("system fail");
		tpret = ST_ERR;
		return tpret;
	}
	DBG_LOG_INFO("wwan0 should be under the management of service systemd-networkd now");
	return tpret;
}





