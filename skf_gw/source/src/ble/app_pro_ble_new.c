/**
 * @file    app_pro_ble_new.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-02-11
 * @brief   Implementation of BLE process (code refactoring of
 *          app_pro_ble) including unit tests of BLE.
 * @details
 */
#include <stdlib.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "util_dbg.h"
#include "app_general_def.h"
#include "sys_def.h"
#include "app_pro_ble_new.h"
#include "bt_connection_new.h"
#include "bt_advertising_new.h"
#include "bt_gatt_new.h"
#include "unittest.h"
#include "bt_hcitool.h"
#include "bt_advertising.h"
#include "bt_common.h"

DBG_LOCAL_LOG_DEBUG

#ifdef UNITTEST
#if (UNITTEST == UNITTEST_CONNECTION_DISCONNECTION)
#define APP_PRO_BLE_NEW_CONNECTION_REQUIRED (1)
#define APP_PRO_BLE_NEW_ADV_REQUIRED (0)
#define APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED (0)
#endif /* UNITTEST == UNITTEST_CONNECTION_DISCONNECTION */
#if ((UNITTEST == UNITTEST_MULTI_CONNECTION) || (UNITTEST == UNITTEST_MULTI_CON_SCAN_AND_PARSE_INSIGHT_T))
#define APP_PRO_BLE_NEW_CONNECTION_REQUIRED (1)
#define APP_PRO_BLE_NEW_MULTI_CON_ENABLED (1)
#endif /* UNITTEST == UNITTEST_MULTI_CONNECTION */
#if (UNITTEST == UNITTEST_BLE_UART)
#define APP_PRO_BLE_NEW_CONNECTION_REQUIRED (1)
#define APP_PRO_BLE_NEW_ADV_REQUIRED (0)
#define APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED (0)
#endif /* UNITTEST == UNITTEST_BLE_UART */
#if (UNITTEST == UNITTEST_ADVERTISING)
#define APP_PRO_BLE_NEW_CONNECTION_REQUIRED (0)
#define APP_PRO_BLE_NEW_ADV_REQUIRED (1)
#define APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED (0)
#endif /* UNITTEST == UNITTEST_ADVERTISING */
#if (UNITTEST == UNITTEST_BLE_SERVER)
#define APP_PRO_BLE_NEW_CONNECTION_REQUIRED (1)
#define APP_PRO_BLE_NEW_ADV_REQUIRED (1)
#define APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED (1)
#endif /* UNITTEST == UNITTEST_BLE_SERVER */
#if (UNITTEST == UNITTEST_UNIVERSE)
#define APP_PRO_BLE_NEW_CONNECTION_REQUIRED (1)
// skip these 2 macroes at phase 1
#define APP_PRO_BLE_NEW_ADV_REQUIRED (1)
#define APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED (1)
#endif /* UNITTEST == UNITTEST_UNIVERSE */
#else /* UNITTEST */
#define APP_PRO_BLE_NEW_CONNECTION_REQUIRED (1)
#define APP_PRO_BLE_NEW_ADV_REQUIRED (1)
#define APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED (1)
#endif /* UNITTEST */
#define SKF_BT_RECOVER_REQUEST \
  "/run/skf-gateway/requests/bluetooth-recover"
 
#define SKF_BT_RECOVER_RESULT \
  "/run/skf-gateway/state/bluetooth-recover.result"


static int request_privileged_recovery(void);
static restart_dbus_required = false;
static bool _g_flag;
static bool _g_wait_for_discon_flag = false;
static chk = true;
static pid_t _g_child_pid = 0; // PID of the general process

static void *thandler_ble_mainloop(void *pt_para);
static void *thandler_ble_handleeventloop(void *pt_para);
static void *thandler_ble_periodicloop(void *pt_para);
static void *thandler_ble_bluezloop(void *pt_para);
// Callbacks of DBUS
static void do_debug(
  const char *str,
  void *user_data);
static void signal_handler(
  uint32_t signo,
  void *user_data);
static void ready_callback(void *user_data);
static void disconnect_callback(void *user_data);
static void iwd_service_appeared(
  struct l_dbus *dbus,
  void *user_data);
static void iwd_service_disappeared(
  struct l_dbus *dbus,
  void *user_data);
static void bluez_client_connected(
  struct l_dbus *dbus,
  void *user_data);
static void bluez_client_disconnected(
  struct l_dbus *dbus,
  void *user_data);
static void bluez_client_ready(
  struct l_dbus_client *client,
  void *user_data);
static void proxy_added(
  struct l_dbus_proxy *proxy,
  void *user_data);
static void proxy_removed(
  struct l_dbus_proxy *proxy,
  void *user_data);
static void property_changed(
  struct l_dbus_proxy *proxy,
  const char *name,
  struct l_dbus_message *msg,
  void *user_data);

static app_state_t notifyProcessGeneralBlock(
  const int32_t msqId,
  const enum bluetooth_event btevent,
  uint32_t msgid);
static app_state_t get_gateway_id(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_adapter);
#if (SKF_GW_NEW == 1)
extern uint32_t g_con_idx_coutnt;
#endif

static int
request_privileged_bluetooth_recovery(void)
{
  char token[96];
  char tmp_path[160];
  char result_token[96];
  struct timespec ts;
  int result_rc = -1;
 
  clock_gettime(CLOCK_MONOTONIC, &ts);
 
  snprintf(token,
           sizeof(token),
           "%ld-%ld-%ld",
           (long)getpid(),
           (long)ts.tv_sec,
           (long)ts.tv_nsec);
 
  snprintf(tmp_path,
           sizeof(tmp_path),
           "%s.tmp.%ld",
           SKF_BT_RECOVER_REQUEST,
           (long)getpid());
 
  int fd = open(tmp_path,
                O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                0600);
 
  if(fd < 0)
  {
    DBG_LOG_ERR("Failed to create Bluetooth recovery request: %s",
                strerror(errno));
    return -1;
  }
 
  if(write(fd, token, strlen(token)) != (ssize_t)strlen(token) ||
     write(fd, "\n", 1) != 1)
  {
    DBG_LOG_ERR("Failed to write Bluetooth recovery request: %s",
                strerror(errno));
 
    close(fd);
    unlink(tmp_path);
    return -1;
  }
 
  fsync(fd);
  close(fd);
 
  /*
   * Atomic rename prevents the systemd path watcher from
   * reading a partially written request.
   */
  if(rename(tmp_path, SKF_BT_RECOVER_REQUEST) != 0)
  {
    DBG_LOG_ERR("Failed to publish Bluetooth recovery request: %s",
                strerror(errno));
 
    unlink(tmp_path);
    return -1;
  }
 
  /*
   * Original recovery sequence takes several seconds.
   * Wait for the privileged helper result.
   */
  for(int i = 0; i < 30; i++)
  {
    FILE *fp = fopen(SKF_BT_RECOVER_RESULT, "r");
 
    if(fp != NULL)
    {
      result_token[0] = '\0';
      result_rc = -1;
 
      if(fscanf(fp,
                "%95s %d",
                result_token,
&result_rc) == 2)
      {
        fclose(fp);
 
        if(strcmp(result_token, token) == 0)
        {
          if(result_rc == 0)
          {
            return 0;
          }
 
          DBG_LOG_ERR(
            "Privileged Bluetooth recovery returned status %d",
            result_rc);
 
          return -1;
        }
      }
      else
      {
        fclose(fp);
      }
    }
 
    sleep(1);
  }
 
  DBG_LOG_ERR("Timed out waiting for privileged Bluetooth recovery");
 
  return -1;
}

void
app_pro_ble_new(pid_t pid)
{
  int _ret;
  // Message queues of IPC
  key_t _key = -1;
  int _msg_queue_id_to_ble = -1, _msg_queue_id_from_ble = -1;
  // Thread ID
  pthread_t _thread_ble_main_loop = 0;
  pthread_t _thread_ble_handleevent_loop = 0;
  pthread_t _thread_ble_periodic_loop = 0;
  pthread_t _thread_bluez_loop = 0;
  // Check dbus status
  uint32_t _timeoutCnt = 0;
  struct l_dbus_proxy *_pt_proxy = NULL;
  bool _startup_ble_ready = false;
  // for restore whitelist during dbus reloading. fix whitelist is cleanup issue. 
  struct bt_addr _white_list_backup[MAX_BT_WHITE_LIST] = { 0 };

  _g_child_pid = pid;

  // To create the message queues for IPC
#if (SKF_GW_NEW == 1)
  // clear the index counter in case dbus was restarted
  g_con_idx_coutnt = 0;
  _key = ftok(FPATH_FOR_SKF_GW_NEW_IPC0, PROJECT_ID_FOR_SKF_GW_NEW_IPC);
  _msg_queue_id_from_ble = msgget(_key,
                                  PERMISSION_FLAG_FOR_SKF_GW_NEW_IPC0 |
                                  IPC_CREAT);
  if(_msg_queue_id_from_ble == -1)
  {
    DBG_LOG_ERR(
      "Failed to create msg queue from BLE process to the general process\n");
    exit(EXIT_FAILURE);
  }
  _key = ftok(FPATH_FOR_SKF_GW_NEW_IPC1, PROJECT_ID_FOR_SKF_GW_NEW_IPC);
  _msg_queue_id_to_ble = msgget(_key,
                                PERMISSION_FLAG_FOR_SKF_GW_NEW_IPC1 |
                                IPC_CREAT);
  if(_msg_queue_id_to_ble == -1)
  {
    DBG_LOG_ERR(
      "Failed to create msg queue from the general process to BLE process\n");
    exit(EXIT_FAILURE);
  }
  DBG_LOG_INFO("msg_queue_id_to_ble %d", _msg_queue_id_to_ble);
  DBG_LOG_INFO("msg_queue_id_from_ble %d", _msg_queue_id_from_ble);
#endif /* SKF_GW_NEW == 1 */

#if (SKF_GW_NEW == 1)
  bt_con_n_deinit_con_controller(bt_con_n_get_con_ctr());
  sys_destroy_app_management(sys_get_mgt());
  if(sys_init_app_management(sys_get_mgt()) != ST_OK)
  {
    DBG_LOG_ERR("Failed to init app-mamagement");
    exit(EXIT_FAILURE);
  }
  // Initialize the IPC message queue IDs in @p g_app_management
  sys_init_set_msgid(sys_get_mgt(),
                     _msg_queue_id_to_ble,
                     _msg_queue_id_from_ble);

  bt_con_n_init_con_controller(bt_con_n_get_con_ctr());
#if (APP_PRO_BLE_NEW_ADV_REQUIRED == 1)
  bt_adv_n_init_adv_ctr(bt_adv_n_get_adv_ctr());
#endif /* APP_PRO_BLE_NEW_ADV_REQUIRED == 1 */
#endif /* SKF_GW_NEW == 1 */

  // Thread for the main loop
  _ret = pthread_create(&_thread_ble_main_loop,
                        NULL,
                        thandler_ble_mainloop,
                        NULL);
  if(0 != _ret)
  {
    DBG_LOG_ERR("Failed to create a thread for the main loop.");
    exit(EXIT_FAILURE);
  }
  // Thread for dealing with event from BLE dbus
  _ret = pthread_create(&_thread_ble_handleevent_loop,
                        NULL,
                        thandler_ble_handleeventloop,
                        NULL);
  if(0 != _ret)
  {
    DBG_LOG_ERR("Failed to create a thread for the main loop.");
    exit(EXIT_FAILURE);
  }
  // Thread for dealing with periodic event
  _ret = pthread_create(&_thread_ble_periodic_loop,
                        NULL,
                        thandler_ble_periodicloop,
                        NULL);
  if(0 != _ret)
  {
    DBG_LOG_ERR("Failed to create a thread for the periodic event loop.");
    exit(EXIT_FAILURE);
  }
  // Thread for bluez loop
  _ret = pthread_create(&_thread_bluez_loop,
                        NULL,
                        thandler_ble_bluezloop,
                        NULL);
  if(0 != _ret)
  {
    DBG_LOG_ERR("Failed to create a thread for the bluez loop.");
    exit(EXIT_FAILURE);
  }

  while(1)
  {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    if(bt_con_n_get_fail_count() > 5)
    {
      // restart_dbus_required = true;
      DBG_LOG_WARN("wait connect timeout up to limit 5! restart dbus!\n");     
    }
#endif
    if(restart_dbus_required == true)
    {
      // bluez dbus restart steps:
      // 1. Notify to the process general that the dbus is not ready
      // 2. Kill the bluez loop (and threads for multi-connection if available)
      // 3. Run the script to restart the dbus
      // 4. Reinitialize the application manager
      // 5. Power on and turn on scanning till the adapter is available
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      bt_con_n_set_fail_count(0);
#endif
      uint8_t _empty_service_cnt = 0;
RESTART_DBUS:
      if(_empty_service_cnt >= 2)
      {
        DBG_LOG_ERR("No service can be found. Reboot!\n");
        fflush(stdout);
        sync();
        sync();
        sync();
        system("reboot");
      }
#if (SKF_GW_NEW == 1)
      DBG_LOG_WARN("prepare to restart dbus!\n"); // log it on debug purpose
      notifyProcessGeneralBlock(
        _msg_queue_id_from_ble,
        BT_DBUS_IS_NOT_READY,
        0xffffffff);
#endif /* SKF_GW_NEW == 1 */
      if(_thread_bluez_loop != 0)
      {
#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
        struct con_controller *_pt_con_ctr = bt_con_n_get_con_ctr();
        bool _con_flag = false;
        _g_wait_for_discon_flag = true;
        for(uint8_t cnt=0;cnt<120;cnt++)
        {
          _con_flag = false;
          pthread_mutex_lock(&_pt_con_ctr->mtx_for_connected_proxy);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
          for(uint8_t i=0; i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
          {
            DBG_LOG_DEBUG("idx:(%d),proxy@%p!\n",i,_pt_con_ctr->connected_proxy[i]);
            if(NULL != _pt_con_ctr->connected_proxy[i])
            {
              _con_flag = true;
            }
          }
#endif
          pthread_mutex_unlock(&_pt_con_ctr->mtx_for_connected_proxy);
          if(_con_flag)
          {
            DBG_LOG_DEBUG("there is sensor conected(%d)!\n",cnt);
            sleep(1);
            continue;
          }
          else
          {
            DBG_LOG_INFO("no sensor is conected for now(%d)!\n",cnt);
            break;
          }
        }
        _g_wait_for_discon_flag = false;
#endif /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED != 1 */
        pthread_kill(_thread_bluez_loop, SIGTERM);
        // DBG_LOG_DEBUG("Force to stop bluez directly\n");
        // l_main_quit();
        for(uint8_t _cnt=0;_cnt<60;_cnt++)
        {
          if(_g_flag)
          {
            sleep(1);
            DBG_LOG_DEBUG("flag not cleared(%d)!\n", _cnt);
          }
          else
          {
            DBG_LOG_DEBUG("flag cleared(%d)!\n", _cnt);
            break;
          }
        }
        if(_g_flag)
        {
          DBG_LOG_ERR("Failed to terminate thread within 3S.kill it!\n");
          fflush(stdout);
          pthread_kill(_thread_bluez_loop, SIGKILL);
          sleep(1);
        }
        _thread_bluez_loop = 0;
      }
      // Restart bluez once long time no action on bluez
      sleep(1);

      DBG_LOG_INFO("Privileged Bluetooth recovery requested ...\n");
      
      if(request_privileged_bluetooth_recovery() != 0)
      {
        DBG_LOG_ERR("Failed to execute privileged Bluetooth recovery!\r\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
      }
      
      DBG_LOG_INFO("Privileged Bluetooth recovery is done ...");
      _startup_ble_ready = false;

#if (SKF_GW_NEW == 1)
      pthread_mutex_lock(&(sys_get_mgt()->mtx_for_white_list));
      sys_get_bt_white_list(_white_list_backup, MAX_BT_WHITE_LIST);
      pthread_mutex_unlock(&(sys_get_mgt()->mtx_for_white_list));
      sys_destroy_app_management(sys_get_mgt());
      DBG_LOG_INFO("deinit app_management is done ...\n");
      if(sys_init_app_management(sys_get_mgt()) != ST_OK)
      {
        DBG_LOG_ERR("Failed to init app-mamagement");
        fflush(stdout);
        exit(EXIT_FAILURE);
      }
      pthread_mutex_lock(&(sys_get_mgt()->mtx_for_white_list));
      sys_set_bt_white_list(_white_list_backup, MAX_BT_WHITE_LIST);
      pthread_mutex_unlock(&(sys_get_mgt()->mtx_for_white_list));
      DBG_LOG_INFO("white list restored after dbus restart\n");
      bt_con_n_deinit_con_controller(bt_con_n_get_con_ctr());
      DBG_LOG_INFO("deinit controller is done ...\n");
      bt_con_n_init_con_controller(bt_con_n_get_con_ctr());
      DBG_LOG_INFO("init controller is done ...\n");
      // Initialize the IPC message queue IDs in @p g_app_management
      sys_init_set_msgid(sys_get_mgt(),
                         _msg_queue_id_to_ble,
                         _msg_queue_id_from_ble);
      bt_con_n_init_con_controller(bt_con_n_get_con_ctr());
#if (APP_PRO_BLE_NEW_ADV_REQUIRED == 1)
      bt_adv_n_init_adv_ctr(bt_adv_n_get_adv_ctr());
#endif /* APP_PRO_BLE_NEW_ADV_REQUIRED == 1 */
#endif /* SKF_GW_NEW == 1 */
      // Thread for bluez loop
      _ret = pthread_create(&_thread_bluez_loop,
                            NULL,
                            thandler_ble_bluezloop,
                            NULL);
      if(0 != _ret)
      {
        DBG_LOG_ERR("Failed to create a thread for the bluez loop.\n");
        exit(EXIT_FAILURE);
      }

      sleep(10);

      _pt_proxy = sys_get_proxy_adapter1(sys_get_mgt());
      if(NULL == _pt_proxy)
      {
        DBG_LOG_ERR("This is not supposed to happen!\n");
        fflush(stdout);
        _empty_service_cnt++;
        // exit(EXIT_FAILURE);
        goto RESTART_DBUS;
      }
      bt_common_set_powered(_pt_proxy, true);
      usleep(500000);
      bt_common_turn_on_scanning(_pt_proxy, NULL);
#if (SKF_GW_NEW == 1)
      // To recover the status to "disconnection"
      notifyProcessGeneralBlock(
        _msg_queue_id_from_ble,
        BT_EVENT_DISCONNECTION,
        0xffffffff);
      // clear the index counter in case dbus was restarted
      g_con_idx_coutnt = 0;
#endif /* SKF_GW_NEW == 1 */

      restart_dbus_required = false;
      // send a BT_EVENT_SW_TO_CLIENT event to sync the role with general process
      // in case it's restarted when gw is in server role.
      notifyProcessGeneralBlock(
        _msg_queue_id_from_ble,
        BT_EVENT_SW_TO_CLIENT,
        0xffffffff);
      DBG_LOG_INFO("dbus restore is done ...\n");
    }

    // Check the BLE DBUS status ...
    _pt_proxy = sys_get_proxy_adapter1(sys_get_mgt());
    if(_timeoutCnt > 3)
    {
      DBG_LOG_ERR("Failed to get the proxy adapter!\n");
      fflush(stdout);
      sync();
      sync();
      sync();
      system("reboot");
    }
    if(NULL == _pt_proxy)
    {
      _timeoutCnt++;
#if (SKF_GW_NEW == 1)
      notifyProcessGeneralBlock(
        _msg_queue_id_from_ble,
        BT_DBUS_IS_NOT_READY,
        0xffffffff);
#endif /* SKF_GW_NEW == 1 */
      DBG_LOG_INFO("Retry to get the proxy adapter, %d s\n",
                   _timeoutCnt * 5);
    }
    else
    {
      bool _powered = false;
      bool _discovering = false;

      _timeoutCnt = 0;
#if (SKF_GW_NEW == 1)
      notifyProcessGeneralBlock(
        _msg_queue_id_from_ble,
        BT_DBUS_IS_READY,
        0xffffffff);
#endif /* SKF_GW_NEW == 1 */
      DBG_LOG_INFO("Get the proxy adapter successfully\n");

      /*
       * Ensure the BLE process reaches the initial BlueZ client state
       * even if the one-shot POWER_ON / SCAN_ON IPC commands are lost
       * during process start-up.  All D-Bus operations still run as the
       * unprivileged skfgw process; privileged HCI/recovery operations
       * remain isolated in their existing root helpers.
       */
      if(false == _startup_ble_ready)
      {
        if(false ==
           l_dbus_proxy_get_property(
             _pt_proxy,
             "Powered",
             "b",
             &_powered))
        {
          DBG_LOG_ERR("BLE_STARTUP failed to read Powered");
        }
        else if(false == _powered)
        {
          DBG_LOG_INFO("BLE_STARTUP request Powered=true");

          if(ST_OK !=
             bt_common_set_powered(
               _pt_proxy,
               true))
          {
            DBG_LOG_ERR("BLE_STARTUP Powered request failed");
          }
        }
        else if(false ==
                l_dbus_proxy_get_property(
                  _pt_proxy,
                  "Discovering",
                  "b",
                  &_discovering))
        {
          DBG_LOG_ERR("BLE_STARTUP failed to read Discovering");
        }
        else if(false == _discovering)
        {
          DBG_LOG_INFO("BLE_STARTUP request StartDiscovery");

          if(ST_OK !=
             bt_common_turn_on_scanning(
               _pt_proxy,
               NULL))
          {
            DBG_LOG_ERR("BLE_STARTUP StartDiscovery request failed");
          }
        }
        else
        {
          _startup_ble_ready = true;
          DBG_LOG_INFO("BLE_STARTUP discovery ready");
        }
      }
    }
    sleep(5);
  }
  DBG_LOG_INFO("Exit the BLE process\n");
  fflush(stdout);
  exit(EXIT_SUCCESS);
}
/**
 * @brief Thread of BLE high-level application (as a client to connect
 * sensors)
 */
static void *
thandler_ble_mainloop(void *pt_para)
{
#if (SKF_GW_NEW == 1)
  struct ipc_msg_v3 _ipc_msg = { 0 };

#endif /* SKF_GW_NEW == 1 */
  struct msqid_ds _ipc_ctrl_msg;
  ssize_t _sz;
  int _msg_queue_id_to_ble = sys_get_msgid_from_pro_general(sys_get_mgt());
  int _msg_queue_id_from_ble = sys_get_msgid_to_pro_general(sys_get_mgt());
  struct l_dbus_proxy *_pt_proxy;
  app_management *_pt_app_mgr = sys_get_mgt();

#if ((APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1) || \
  (APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1))
  struct pthread_cond_var kp_cond_var;

  if(ST_OK != app_com_init_pthread_cond_var_ctr(&kp_cond_var))
  {
    DBG_LOG_ERR("fail to initialize cond var");
  }
#endif

  DBG_LOG_INFO("Enter the main loop");
#if (1)
  uint8_t _tmp_cnt = 0;
  bool _clear_flag = false;
#endif

  while(1)
  {
#if (SKF_GW_NEW == 1)
    if(_clear_flag)
    {
      DBG_LOG_INFO("clean msg queue %d at restart!\n", _msg_queue_id_to_ble);
      if(msgctl(_msg_queue_id_to_ble, IPC_STAT, &_ipc_ctrl_msg) == -1)
      {
        DBG_LOG_ERR("Fail to get stats of IPC to_ble queue\n");
        return -1;
      }
      if(_ipc_ctrl_msg.msg_qnum > 0)
      {
        DBG_LOG_WARN("clean IPC queue to_ble %d at start-up!\n", _ipc_ctrl_msg.msg_qnum);
        for(uint8_t i=0;i<_ipc_ctrl_msg.msg_qnum;i++)
        {
          memset(&_ipc_msg, 0, sizeof(_ipc_msg));
          _sz = msgrcv(_msg_queue_id_to_ble, (void *)&_ipc_msg,
                      sizeof(_ipc_msg.payload), 0, IPC_NOWAIT);
        }
      }
      _clear_flag = false;
      DBG_LOG_INFO("clean msg queue %d done!\n", _msg_queue_id_to_ble);
    }
    // And update used volume of the IPC queue
    if(msgctl(_msg_queue_id_to_ble, IPC_STAT, &_ipc_ctrl_msg) == -1)
    {
      DBG_LOG_ERR("Failed to get stats of IPC queue");
      return -1;
    }
    else 
    {
      DBG_LOG_DEBUG("The used volume of the IPC queue: %lu B, num:%d\n",
                    _ipc_ctrl_msg.__msg_cbytes,
                    _ipc_ctrl_msg.msg_qnum);
      if(_ipc_ctrl_msg.msg_qnum >= 39)
      {
        DBG_LOG_WARN("IPC queue to_ble %d up to limit!\n", _msg_queue_id_to_ble);
      }
      memset(&_ipc_msg, 0, sizeof(_ipc_msg));
      _sz = msgrcv(_msg_queue_id_to_ble, (void *)&_ipc_msg,
                  sizeof(_ipc_msg.payload), 0, 0);
      if(_sz < 0)
      {
        DBG_LOG_ERR("msgsnd fail, for %s", strerror(errno));
        DBG_LOG_ERR("Failed to receive msg from %d", _msg_queue_id_to_ble);
        usleep(10000); //10ms
        continue;
      }
      DBG_LOG_INFO("Msg received: type %ld, load-len %ld",
                  _ipc_msg.type,
                  _sz);
      DBG_LOG_DEBUG("Msg received: msgId %ld, cmd %ld",
                    _ipc_msg.payload.cmd_info.msgid,
                    _ipc_msg.payload.cmd_info.cmd);
      // clean the msg_queue in case dbus is not ready.
      if((true == restart_dbus_required) &&
         (_ipc_ctrl_msg.msg_qnum > 0))
      {
        if(_g_wait_for_discon_flag)
        {
          if((M_TYPE_COMMAND == _ipc_msg.type) &&
            (CMD_DISCONNECTION == _ipc_msg.payload.cmd_info.cmd))
          {
            DBG_LOG_WARN("go to disconnect cmd\n");
            goto _GO_TO_HANDLE_DIS_CONNECT;
          }
        }
        DBG_LOG_WARN("Msg drop due to restarting dbus %ld %ld %ld\n",
        _ipc_msg.type, _sz, _ipc_msg.payload.cmd_info.cmd);
      }
    }
_GO_TO_HANDLE_DIS_CONNECT:
    if(_ipc_msg.type == M_TYPE_COMMAND)
    {
      switch(_ipc_msg.payload.cmd_info.cmd)
      {
        case CMD_SET_WHITE_LIST:
        {
          struct bt_addr _wl_check[MAX_BT_WHITE_LIST] = { 0 };

          // Trace 'whitelist might be NULL' issue
          // one workaround: 'ipcrm -q #' to remove the queue  
          DBG_LOG_INFO("recv CMD_SET_WHITE_LIST!\n");
          pthread_mutex_lock(&(sys_get_mgt()->mtx_for_white_list));
          sys_set_bt_white_list(
            _ipc_msg.payload.cmd_info.para.bt_white_list,
            MAX_BT_WHITE_LIST);
          sys_get_bt_white_list(
            _wl_check,
            MAX_BT_WHITE_LIST);
          pthread_mutex_unlock(&(sys_get_mgt()->mtx_for_white_list));

          for(uint32_t i = 0; i < MAX_BT_WHITE_LIST; i++)
          {
            if((_wl_check[i].nap == 0) &&
               (_wl_check[i].uap == 0) &&
               (_wl_check[i].lap == 0))
            {
              continue;
            }

            DBG_LOG_INFO(
              "BLE_RUNTIME_WHITELIST[%u]=%02X:%02X:%02X:%02X:%02X:%02X",
              i,
              (_wl_check[i].nap >> 8) & 0xFF,
              _wl_check[i].nap & 0xFF,
              _wl_check[i].uap,
              (_wl_check[i].lap >> 16) & 0xFF,
              (_wl_check[i].lap >> 8) & 0xFF,
              _wl_check[i].lap & 0xFF);
          }
          break;
        }
        case CMD_LOW_LEVEL_BLE_POWER_ON:
          _pt_proxy = sys_get_proxy_adapter1(sys_get_mgt());
          if(NULL == _pt_proxy)
          {
            DBG_LOG_ERR("This is not supposed to happen!");
            exit(EXIT_FAILURE);
          }
          bt_common_set_powered(_pt_proxy, true);
          break;
        case CMD_LOW_LEVEL_BLE_POWER_OFF:
          _pt_proxy = sys_get_proxy_adapter1(sys_get_mgt());
          if(NULL == _pt_proxy)
          {
            DBG_LOG_ERR("This is not supposed to happen!");
            exit(EXIT_FAILURE);
          }
          bt_common_set_powered(_pt_proxy, false);
          break;
        case CMD_LOW_LEVEL_BLE_SCAN_ON:
          _pt_proxy = sys_get_proxy_adapter1(sys_get_mgt());
          if(NULL == _pt_proxy)
          {
            DBG_LOG_ERR("This is not supposed to happen!");
            exit(EXIT_FAILURE);
          }
          bt_common_turn_on_scanning(_pt_proxy, NULL);
          break;
        case CMD_LOW_LEVEL_BLE_SCAN_OFF:
          _pt_proxy = sys_get_proxy_adapter1(sys_get_mgt());
          if(NULL == _pt_proxy)
          {
            DBG_LOG_ERR("This is not supposed to happen!");
            exit(EXIT_FAILURE);
          }
          bt_common_turn_off_scanning(_pt_proxy, NULL);
          break;
        case CMD_DISCONNECTION:
        {
#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
          DBG_LOG_INFO("Going to disconnect ...");
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
          bt_con_n_disconnect_from_dev(bt_con_n_get_con_ctr(), 
                                        _ipc_msg.payload.cmd_info.con_idx,
                                        _ipc_msg.payload.cmd_info.para.connect_id);
#else
          bt_con_n_disconnect_from_dev(bt_con_n_get_con_ctr());
#endif
#else /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1 */
          DBG_LOG_WARN("Do NOT support disconnection");
#endif /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED != 1 */
          break;
        }
        case CMD_LOW_LEVEL_BLE_ADV_ON:
        {
          DBG_LOG_INFO("To turn on BT advertising");

#if (APP_PRO_BLE_NEW_ADV_REQUIRED == 1)
          struct bluetooth_adv_controller *_pt_adv_ctr =
            bt_adv_n_get_adv_ctr();

          DBG_LOG_DEBUG("dbus @ 0x%llx", _pt_adv_ctr->dbus);
          DBG_LOG_DEBUG("proxy_le_adv_manager @ 0x%llx",
                        _pt_adv_ctr->proxy_le_adv_manager);
          DBG_LOG_DEBUG("gw_id_buf %s", _pt_adv_ctr->gw_id_buf);

#if 0 // Old implementation (is going to be deprecated)
          bt_adv_n_turn_on_le_advertisement(_pt_adv_ctr);
#else
          if(true != bt_ad_register_advertisement(_pt_adv_ctr))
          {
            DBG_LOG_ERR("Failed to register adv");
          }
          else
          {
            DBG_LOG_INFO("Register adv successfully");
          }
          fflush(stdout);
#endif
#endif /* APP_PRO_BLE_NEW_ADV_REQUIRED == 1 */
          break;
        }
        case CMD_LOW_LEVEL_BLE_ADV_OFF:
        {
          DBG_LOG_INFO("To turn off BT advertising");

#if (APP_PRO_BLE_NEW_ADV_REQUIRED == 1)
          struct bluetooth_adv_controller *_pt_adv_ctr =
            bt_adv_n_get_adv_ctr();

          DBG_LOG_DEBUG("dbus @ 0x%llx", _pt_adv_ctr->dbus);
          DBG_LOG_DEBUG("proxy_le_adv_manager @ 0x%llx",
                        _pt_adv_ctr->proxy_le_adv_manager);
          DBG_LOG_DEBUG("gw_id_buf %s", _pt_adv_ctr->gw_id_buf);

#if 0 // Old implementation (is going to be deprecated)
          bt_adv_n_turn_off_le_advertisement(_pt_adv_ctr);
#else
          if(true != bt_ad_unregister_advertisement(_pt_adv_ctr))
          {
            DBG_LOG_ERR("Failed to unregister adv");
          }
          else
          {
            DBG_LOG_INFO("Unregister adv successfully");
          }
          fflush(stdout);
#endif
#endif /* APP_PRO_BLE_NEW_ADV_REQUIRED == 1 */
          break;
        }
        case CMD_SW_TO_CLIENT:
        {
          DBG_LOG_INFO("To switch to BT_ROLE_CLIENT\n");
          // Switch to BT_ROLE_SERVER:
          // 1. Stop advertising (if the gateway is not in a connection)
          // 2. Disconnect from peer (if there existed)
          // 3. Turn on scan

          // TODO: Turn off adv
#if (APP_PRO_BLE_NEW_ADV_REQUIRED == 1)
          pthread_mutex_lock(&_pt_app_mgr->mtx_for_bt_sw);
          if(BT_ROLE_CLIENT == _pt_app_mgr->bt_role)
          {
            pthread_mutex_unlock(&_pt_app_mgr->mtx_for_bt_sw);
            DBG_LOG_INFO("gw already in CLIENT role!\n");
            break;
          }
          if(_pt_app_mgr->bt_sw_in_progress)
          {
            pthread_mutex_unlock(&_pt_app_mgr->mtx_for_bt_sw);
            DBG_LOG_INFO("swith to bt client role ongoing!\n");
            break;
          }
          else
          {
            _pt_app_mgr->bt_sw_in_progress = true;
            pthread_mutex_unlock(&_pt_app_mgr->mtx_for_bt_sw);
          }
          const struct bluetooth_adv_controller *_pt_adv_ctr =
            bt_adv_n_get_adv_ctr();

          DBG_LOG_DEBUG("dbus @ 0x%llx", _pt_adv_ctr->dbus);
          DBG_LOG_DEBUG("proxy_le_adv_manager @ 0x%llx",
                        _pt_adv_ctr->proxy_le_adv_manager);
          DBG_LOG_DEBUG("gw_id_buf %s", _pt_adv_ctr->gw_id_buf);

#if 0 // Old implementation (is going to be deprecated)
          bt_adv_n_turn_off_le_advertisement(_pt_adv_ctr);
#else
          if(true != bt_ad_unregister_advertisement(_pt_adv_ctr))
          {
            // stay in server role in case unregister adv failed.
            pthread_mutex_lock(&_pt_app_mgr->mtx_for_bt_sw);
            _pt_app_mgr->bt_sw_in_progress = false;
            pthread_mutex_unlock(&_pt_app_mgr->mtx_for_bt_sw);
            DBG_LOG_ERR("Failed to unregister adv in server role!\n");
#if (0) // TODO: skip this failure here. to be confirmed whether it's ok or not.
            break;
#endif
          }
          else
          {
            DBG_LOG_INFO("Unregister adv successfully\n");
          }
#endif
          usleep(500000);
#endif /* APP_PRO_BLE_NEW_ADV_REQUIRED == 1 */

#if (0) // skip it as there should be no client 'connection' in server role. 
        // do not 'touch' it here.
#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
          for(uint8_t i=0; i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
          {
            bt_con_n_disconnect_from_dev(bt_con_n_get_con_ctr(), i, NULL);
          }
#else
          bt_con_n_disconnect_from_dev(bt_con_n_get_con_ctr());
#endif /* SKF_GW_CONFIG_MULTI_CONNECTION != 1 */
#endif /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED != 1 */
#endif
          // To be confirmed: no need to dis-connect the con in server role
          _pt_proxy = sys_get_proxy_adapter1(sys_get_mgt());
          if(NULL == _pt_proxy)
          {
            DBG_LOG_ERR("This is not supposed to happen!\n");
            _pt_app_mgr->bt_sw_in_progress = false;
            exit(EXIT_FAILURE);
          }
          bt_common_turn_on_scanning(_pt_proxy, NULL);

          notifyProcessGeneralBlock(
            _msg_queue_id_from_ble,
            BT_EVENT_SW_TO_CLIENT,
            0xffffffff);
          // update the role after sending ipc msg out.
          pthread_mutex_lock(&_pt_app_mgr->mtx_for_bt_sw);
          _pt_app_mgr->bt_role = BT_ROLE_CLIENT;
          _pt_app_mgr->bt_sw_in_progress = false;
          pthread_mutex_unlock(&_pt_app_mgr->mtx_for_bt_sw);
          DBG_LOG_INFO("switch to client role successfully\n");
          break;
        }
        case CMD_SW_TO_SERVER:
        {
          DBG_LOG_INFO("To switch to BT_ROLE_SERVER\n");
          // Switch to BT_ROLE_SERVER:
          // 1. Turn off scan
          // 2. Disconnect from peer (if there existed)
          // 3. Start advertising
          _pt_proxy = sys_get_proxy_adapter1(sys_get_mgt());

          if(NULL == _pt_proxy)
          {
            DBG_LOG_WARN("Failed to get the adapter");
            break;
          }
          else
          {
            pthread_mutex_lock(&_pt_app_mgr->mtx_for_bt_sw);
            if(BT_ROLE_SERVER == _pt_app_mgr->bt_role)
            {
              pthread_mutex_unlock(&_pt_app_mgr->mtx_for_bt_sw);
              DBG_LOG_INFO("gw already in server role!\n");
              break;
            }
            if(_pt_app_mgr->bt_sw_in_progress)
            {
              pthread_mutex_unlock(&_pt_app_mgr->mtx_for_bt_sw);
              DBG_LOG_INFO("swith to bt server role ongoing!\n");
              break;
            }
            else
            {
              _pt_app_mgr->bt_sw_in_progress = true;
              pthread_mutex_unlock(&_pt_app_mgr->mtx_for_bt_sw);
            }
            bt_common_turn_off_scanning(_pt_proxy, NULL);
            usleep(500000);

#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
            for(uint8_t i=0; i<CONFIG_BLE_CONNECTION_MAX_NUM;i++)
            {
              bt_con_n_disconnect_from_dev(bt_con_n_get_con_ctr(), i, NULL);
            }
#else
            bt_con_n_disconnect_from_dev(bt_con_n_get_con_ctr());
#endif /* SKF_GW_CONFIG_MULTI_CONNECTION != 1 */
#endif /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED != 1 */

            bt_common_set_powered(_pt_proxy, false);
            usleep(500000);
            bt_common_set_powered(_pt_proxy, true);
            usleep(500000);

            // TODO: Register & Turn on adv
#if (APP_PRO_BLE_NEW_ADV_REQUIRED == 1)
          struct bluetooth_adv_controller *_pt_adv_ctr =
            bt_adv_n_get_adv_ctr();

          DBG_LOG_DEBUG("dbus @ 0x%llx", _pt_adv_ctr->dbus);
          DBG_LOG_DEBUG("proxy_le_adv_manager @ 0x%llx",
                        _pt_adv_ctr->proxy_le_adv_manager);
          DBG_LOG_DEBUG("gw_id_buf %s", _pt_adv_ctr->gw_id_buf);
          if(true != bt_ad_register_advertisement(_pt_adv_ctr))
          {
            // stay in client role in case reg adv failed
            pthread_mutex_lock(&_pt_app_mgr->mtx_for_bt_sw);
            _pt_app_mgr->bt_sw_in_progress = false;
            pthread_mutex_unlock(&_pt_app_mgr->mtx_for_bt_sw);
            bt_common_turn_on_scanning(_pt_proxy, NULL);  // to be confirmed.
            usleep(500000);
            DBG_LOG_ERR("Failed to register adv, sta\n");
            fflush(stdout);
            break;
          }
          else
          {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
            _pt_app_mgr->hld_server_connected_proxy = NULL;
#else
            _pt_app_mgr->hld_connected_proxy = NULL;
#endif
            DBG_LOG_INFO("Register adv successfully, proxy cleaned!\n");
          }
          fflush(stdout);
          pthread_mutex_lock(&_pt_app_mgr->mtx_for_bt_sw);
          _pt_app_mgr->bt_sw_in_progress = false;
          _pt_app_mgr->bt_role = BT_ROLE_SERVER;
          pthread_mutex_unlock(&_pt_app_mgr->mtx_for_bt_sw);
#endif /* APP_PRO_BLE_NEW_ADV_REQUIRED == 1 */

            notifyProcessGeneralBlock(
              _msg_queue_id_from_ble,
              BT_EVENT_SW_TO_SERVER,
              0xffffffff);
          }
          break;
        }
#if (APP_GENERAL_DEF_MULTI_CON_REQUIRED == 1)
        case CMD_UPDATE_MULTI_CONNECTIONS:
        {
          app_state_t _ret = ST_OK;
          uint8_t _con_num = 0;
          struct con_controller *_pt_con_ctr=bt_con_n_get_con_ctr();
          DBG_LOG_INFO("To handle multi connections!\n");
          if(true == restart_dbus_required)
          {
            DBG_LOG_WARN("dbus not ready! skip multi-con!\n");
            break;
          }
          _ret = bt_con_n_connect_num_fetch(_pt_con_ctr, &_con_num);
          if(_ret)
          {
            DBG_LOG_WARN("get con count error! skip it!\n");
          } else
          {
            DBG_LOG_INFO("current con count:%d!\n", _con_num);
            if(CONFIG_BLE_CONNECTION_MAX_NUM ==_con_num)
            {
              DBG_LOG_WARN("current con count reach max:%d! do nothing!\n", CONFIG_BLE_CONNECTION_MAX_NUM);
#if (0) // Notes: for test only.
        //      comment it out for test only
              DBG_LOG_INFO("manually restart dbus in case there are %d connections!\n", CONFIG_BLE_CONNECTION_MAX_NUM);
              restart_dbus_required = true;
              // bt_con_n_disconnect_from_dev(_pt_con_ctr,  0, NULL);
#endif
            }
          }
          break;
        }
#endif  // end of "UNITTEST == UNITTEST_MULTI_CONNECTION"
        default:
          DBG_LOG_WARN("The command (%d) is not supported",
                       _ipc_msg.payload.cmd_info.cmd);
          break;
      }
    }
    else if(M_TYPE_PROTO_DATA == _ipc_msg.type)
    {
#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
      if(sys_get_mgt()->bt_role == BT_ROLE_CLIENT)
      {
        struct con_controller *pt_con_ctr = bt_con_n_get_con_ctr();

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        if(true == restart_dbus_required)
        {
          DBG_LOG_WARN("dbus not ready! skip multi-con!\n");
          goto DROP;
        }
        uint8_t idx = _ipc_msg.payload.proto_pkt.con_idx;
        if(0 ==pthread_mutex_trylock(&pt_con_ctr->mtx_for_chr_queue))
        {
          if((NULL == pt_con_ctr->proxy_uart_chr_rx[idx]) ||
            (NULL == pt_con_ctr->proxy_uart_chr_tx[idx]) ||
            (0 == l_queue_length(pt_con_ctr->chr_queue[idx])))
            {
                DBG_LOG_ERR("BLE %d:Chr UART RX @ 0x%p, Chr UART TX @ 0x%p, The # of Chr is %d \n", idx,
                              pt_con_ctr->proxy_uart_chr_rx[idx],
                              pt_con_ctr->proxy_uart_chr_tx[idx],
                              l_queue_length(pt_con_ctr->chr_queue[idx]));
                pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
                goto DROP;
            }
            else
            {
                pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
            }
        }
        else
        {
          DBG_LOG_WARN("fail to lock mtx_for_chr_queue!\n");
        }
        pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
        if((idx >= CONFIG_BLE_CONNECTION_MAX_NUM) ||
        (NULL == pt_con_ctr->connected_proxy[idx]))
        {
          pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
          DBG_LOG_WARN("Con not ready, skip! idx:%d, proxy:@0x%llx\n", idx,
                      pt_con_ctr->connected_proxy[idx]);
          goto DROP;
        }
        pt_con_ctr->pt_cond_var[idx].para = (uint32_t)idx;
        // skip it as we add write_proxy into cond_var for bt_gatt_n_write_data_to_peer_rx()
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
        // Writing data to peer
        app_state_t gatt_write_rt =
          bt_gatt_n_write_data_to_peer_rx(
            pt_con_ctr,
        &pt_con_ctr->pt_cond_var[idx],
            _ipc_msg.payload.proto_pkt.dtbuf,
            _ipc_msg.payload.proto_pkt.len);
        
        if(ST_OK != gatt_write_rt)
        {
          DBG_LOG_ERR(
            "BLE_GATT_TX_FAIL idx:%d len:%u ret:%d",
            idx,
            _ipc_msg.payload.proto_pkt.len,
            gatt_write_rt);
        
          goto DROP;
        }
        
        DBG_LOG_INFO(
          "BLE_GATT_TX_OK idx:%d len:%u",
          idx,
          _ipc_msg.payload.proto_pkt.len);
        if(_tmp_cnt++ > 10)
        {
          usleep(2000);
          _tmp_cnt = 0;
        }
#else
        pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
        DBG_LOG_DEBUG("Chr UART RX @ 0x%p", pt_con_ctr->proxy_uart_chr_rx);
        DBG_LOG_DEBUG("Chr UART TX @ 0x%p", pt_con_ctr->proxy_uart_chr_tx);
        DBG_LOG_DEBUG("The # of Chr is %d",
                      l_queue_length(pt_con_ctr->chr_queue));
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
        // Writing data to peer
        bt_gatt_n_write_data_to_peer_rx(pt_con_ctr, &kp_cond_var,
                                        _ipc_msg.payload.proto_pkt.dtbuf,
                                        _ipc_msg.payload.proto_pkt.len);
#endif
        pthread_mutex_lock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
        l_queue_push_tail(sys_get_mgt()->pt_adv_scanning_event_queue,
                          (void *)BT_EVENT_DO_NOTHING);
        pthread_cond_signal(&(sys_get_mgt()->cond_val_adv_scanning_ctr));
        pthread_mutex_unlock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
DROP:
      // Do nothing ...
      }
#endif
#if (APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1)
      if(sys_get_mgt()->bt_role == BT_ROLE_SERVER)
      {
        // TODO: to be cleaned
        const char *pt_path =
          l_dbus_proxy_get_path(sys_get_connected_proxy(sys_get_mgt()));

        DBG_LOG_DEBUG("the connected proxy path is %s\n", pt_path);
        DBG_LOG_DEBUG("address @ %p\n",
                      sys_get_connected_proxy(sys_get_mgt()));
        DBG_LOG_INFO("Sending to the client ...\n");
        // util_dbg_buf_dump(_ipc_msg.payload.proto_pkt.dtbuf,
        // _ipc_msg.payload.proto_pkt.len);
        bt_gatt_n_write_data_to_local_tx(sys_get_mgt(), &kp_cond_var,
                                         _ipc_msg.payload.proto_pkt.dtbuf,
                                         _ipc_msg.payload.proto_pkt.len);
        pthread_mutex_lock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
        l_queue_push_tail(sys_get_mgt()->pt_adv_scanning_event_queue,
                          (void *)BT_EVENT_DO_NOTHING);
        pthread_cond_signal(&(sys_get_mgt()->cond_val_adv_scanning_ctr));
        pthread_mutex_unlock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
      }
#endif
    }
    else
    {
      DBG_LOG_WARN("The message type (%d) is not supported",
                   _ipc_msg.type);
    }
#endif /* SKF_GW_NEW == 1 */
  }
}
/**
 * @brief Thread to deal with BLE DBUS events
 */
static void *
thandler_ble_handleeventloop(void *pt_para)
{
  app_management *_pt_app_mgt = sys_get_mgt();
  enum bluetooth_event _hld_btevent = BT_EVENT_UNDEFINED;
  int _msg_queue_id_to_ble = sys_get_msgid_from_pro_general(sys_get_mgt());
  int _msg_queue_id_from_ble = sys_get_msgid_to_pro_general(sys_get_mgt());
  uint8_t cnt = 0;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  struct con_controller *_pt_con_ctr = bt_con_n_get_con_ctr();
  uint8_t con_idx = CONFIG_BLE_CONNECTION_MAX_NUM;
  uint8_t dis_idx = CONFIG_BLE_CONNECTION_MAX_NUM;
#endif

  while(1)
  {
    do{
      pthread_mutex_lock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
      if(restart_dbus_required == true)
      {
        pthread_mutex_unlock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
        DBG_LOG_WARN("DBUS not ready, retry later!\n");
        sleep(1);
        continue;
      }
      else
      {
        DBG_LOG_DEBUG("thandler_ble_handleeventloop start timer!\n");
        clock_gettime(CLOCK_MONOTONIC,
                    &_pt_app_mgt->cond_val_adv_scanning_ctr_tv);
        _pt_app_mgt->cond_val_adv_scanning_ctr_tv.tv_sec += 30;
      }
      if(0 == l_queue_length(_pt_app_mgt->pt_adv_scanning_event_queue))
      {
        if(pthread_cond_timedwait(&_pt_app_mgt->cond_val_adv_scanning_ctr,
                                  &_pt_app_mgt->mtx_adv_scanning_ctr,
                                  &_pt_app_mgt->
                                  cond_val_adv_scanning_ctr_tv)
           == ETIMEDOUT)
        {
          if(_hld_btevent == BT_EVENT_UNDEFINED)
          {
            // TODO: do not restart dbus in server role
            // gw has a counter countdown_s(60s)
            // mobie-app would dis-connect if there is no OPs within 10S
            if(BT_ROLE_SERVER == _pt_app_mgt->bt_role)
            {
              if(++cnt >= 20)  // no event for at least 600s+
              {
                cnt = 0;
                restart_dbus_required = true;
              }
            }
            else
            {
              /*
              * Client mode may legitimately have no BLE event for a while
              * because PredictSensor sleeps between wake-up windows.
              *
              * Do not treat an empty BLE event interval as a D-Bus failure.
              * Real D-Bus disconnection is handled by disconnect_callback().
              */
              cnt = 0;
            
              DBG_LOG_WARN(
                "No BLE event in timeout window; keep BlueZ/D-Bus running");
            }
            pthread_mutex_unlock(&(_pt_app_mgt->mtx_adv_scanning_ctr));
            DBG_LOG_WARN("thandler_ble_handleeventloop timeout!\n");
            DBG_LOG_WARN("BLE event timeout; waiting for next BLE event...");
            continue;
          }
          else
          {
            pthread_mutex_unlock(&(_pt_app_mgt->mtx_adv_scanning_ctr));
            DBG_LOG_INFO("hld_btevent = %d", _hld_btevent);
            continue;
          }
        }
        pthread_mutex_unlock(&(_pt_app_mgt->mtx_adv_scanning_ctr));
      }
      else
      {
        _hld_btevent =
          (enum bluetooth_event)l_queue_pop_head(
            _pt_app_mgt->pt_adv_scanning_event_queue);
        DBG_LOG_INFO("Triggered by BLE event %d", _hld_btevent);
        DBG_LOG_DEBUG("BLE event queue length %d",
                      l_queue_length(
                        _pt_app_mgt->pt_adv_scanning_event_queue));
        pthread_mutex_unlock(&(_pt_app_mgt->mtx_adv_scanning_ctr));
        break;
      }
    }while(1);

    switch(_hld_btevent)
    {
      case BT_EVENT_ADD_ADAPTER:
      {
        DBG_LOG_INFO("Adapter Added");
        if(ST_OK != get_gateway_id(_pt_app_mgt,
                                   _pt_app_mgt->hld_proxy_adapter1))
        {
          DBG_LOG_ERR(
            "Failed to get gateway ID, i.e., the BLE MAC address of the adapter");
          break;
        }
      }
      case BT_EVENT_ADD_DEVPROXY:
      {
        // for server role only so far ...
        DBG_LOG_INFO("Dev Proxy Added\n");
#if (APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1)
        DBG_LOG_DEBUG("sys_get_mgt()->bt_role = %d",
                      _pt_app_mgt->bt_role);
        if(_pt_app_mgt->bt_role == BT_ROLE_SERVER)
        {
          bt_gatt_n_find_connected_dev_proxy(_pt_app_mgt, true);
        }
#endif /* APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1 */
        break;
      }
      case BT_EVENT_RSSI_CHANGED:
      {
        // Do nothing so far ...
        DBG_LOG_INFO("RSSI Changed");
        break;
      }
      case BT_EVENT_DO_NOTHING:
      {
        // Do nothing so far ...
        DBG_LOG_INFO("Do nothing");
        break;
      }
#if (SKF_GW_NEW == 1)
      case BT_EVENT_CONNECTION:
      {
        DBG_LOG_INFO("A BLE device is connected to Gateway\n");
        if(restart_dbus_required == true)
        {
          DBG_LOG_WARN("BT_EVENT_CONNECTION:waiting for dbus ready!\n");  // check whether thread is still alive on debug purpose
          break;
        }
#if (APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1)
        DBG_LOG_DEBUG("sys_get_mgt()->bt_role = %d",
                      _pt_app_mgt->bt_role);
        if(_pt_app_mgt->bt_role == BT_ROLE_SERVER)
        {
          // bt_gatt_n_find_connected_dev_proxy(sys_get_mgt(), false);
          notifyProcessGeneralBlock(
            _msg_queue_id_from_ble,
            BT_EVENT_CONNECTION,
            0);
            //0xffffffff);
          fflush(stdout);
          break;
        }
#endif /* APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1 */
        struct l_dbus_proxy *_pt_proxy;
        _pt_proxy = sys_get_proxy_adapter1(_pt_app_mgt);
        if(NULL == _pt_proxy)
        {
          DBG_LOG_ERR("This is not supposed to happen!\n");
          break;
        }else
        {
          bt_common_turn_off_scanning(_pt_proxy, NULL);
          usleep(500000);
          bt_common_turn_on_scanning(_pt_proxy, NULL);
        }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        // try to create thread for each connection
        con_idx = CONFIG_BLE_CONNECTION_MAX_NUM;
        bool _flg = false;
        for(uint8_t i=0,j=1;i<3;i++)
        {
          if(pthread_mutex_trylock(&_pt_con_ctr->mtx_for_connected_proxy))
          {
            DBG_LOG_ERR("fail to lock con_proxy %d times!\n",i+1);
            usleep(1000 * j);
            j *= 2;
          } else
          {
            _flg = true;
            break;
          }
        }
        if(_flg)  // mtx_for_connected_proxy being locked.
        {
          con_idx = _pt_con_ctr->con_proxy_idx;
          // Notes: some more corner cases to be handle
          pthread_mutex_unlock(&_pt_con_ctr->mtx_for_connected_proxy);
        } else
        {
          DBG_LOG_ERR("fail to lock con_proxy, no thread created for this connection!\n");
          goto EV_CONN_EXIT;
        }

        notifyProcessGeneralBlock(
          _msg_queue_id_from_ble,
          BT_EVENT_CONNECTION,
          con_idx);
EV_CONN_EXIT:
#else
        notifyProcessGeneralBlock(
          _msg_queue_id_from_ble,
          BT_EVENT_CONNECTION,
          0xffffffff);
#endif
        break;
      }
#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
      case BT_EVENT_DISCONNECTION:
      {
        DBG_LOG_INFO("A BLE device was just disconnected ...\n");
#if (APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1)
        DBG_LOG_DEBUG("sys_get_mgt()->bt_role = %d",
                      _pt_app_mgt->bt_role);
        if(_pt_app_mgt->bt_role == BT_ROLE_SERVER)
        {
          notifyProcessGeneralBlock(
            _msg_queue_id_from_ble,
            BT_EVENT_DISCONNECTION,
            0xffffffff);
          fflush(stdout);
          break;
        }
#endif /* APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1 */
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
        // MUST send BT_EVENT_DISCONNECTION to gen_process
        // as sensor_interface depends on g_pro_general_ctr.bt_connection_flg[i]
        // to check whether current connection is still available or not 
        struct con_controller *pt_con_ctr = bt_con_n_get_con_ctr();
        dis_idx = pt_con_ctr->con_proxy_idx;
        if(dis_idx < CONFIG_BLE_CONNECTION_MAX_NUM)
        {
          notifyProcessGeneralBlock(
            _msg_queue_id_from_ble,
            BT_EVENT_DISCONNECTION,
            dis_idx);
        }
#else
        notifyProcessGeneralBlock(
          _msg_queue_id_from_ble,
          BT_EVENT_DISCONNECTION,
          0xffffffff);
#endif
          break;
      }
#endif /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1 */
#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
      case BT_EVENT_SERVICES_RESOLVED:
      {
        DBG_LOG_INFO("service resolved evt received!\n");
        if(restart_dbus_required == true)
        {
          DBG_LOG_WARN("BT_EVENT_SERVICES_RESOLVED:waiting for dbus ready!\n");  // check whether thread is still alive on debug purpose
          break;
        }
        struct con_controller *pt_con_ctr = bt_con_n_get_con_ctr();

        if(pt_con_ctr != NULL)
        {
          // delay 1S to wait for 'Chr UART TXRX' being ready.
          // for 'acquire tx notification failure' issue.
          sleep(1);
          if(_pt_app_mgt->bt_role == BT_ROLE_CLIENT)
          {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
            uint8_t serv_idx = CONFIG_BLE_CONNECTION_MAX_NUM;
            pthread_mutex_lock(&pt_con_ctr->mtx_for_service_flg);
            serv_idx = pt_con_ctr->fg_service_idx;
            pthread_mutex_unlock(&pt_con_ctr->mtx_for_service_flg);
            if(serv_idx >= CONFIG_BLE_CONNECTION_MAX_NUM)
            {
              DBG_LOG_ERR("service idx exceed!%d", serv_idx);
              break;
            }
            // Quickly double check the characteristics are valid
            pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
            if(l_queue_length(pt_con_ctr->chr_queue) <
              EXPECTED_BT_CHR_NUM)
            {
              pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
              DBG_LOG_ERR("The peer seems with too few characteristics");
              break;
            }
            pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
            app_state_t _ret = ST_OK;
            _ret = bt_gatt_n_aquire_peer_tx_notification(pt_con_ctr, serv_idx);
            if(ST_OK == _ret)
            {
              notifyProcessGeneralBlock(_msg_queue_id_from_ble,
                                        BT_EVENT_SERVICES_RESOLVED,
                                        serv_idx);
            }
            else
            {
              DBG_LOG_WARN("acquire tx notification failure (idx:%d)\n", serv_idx);
            }
#else
            // Quickly double check the characteristics are valid
            pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
            if(l_queue_length(pt_con_ctr->chr_queue) <
              EXPECTED_BT_CHR_NUM)
            {
              pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
              DBG_LOG_ERR("The peer seems with too few characteristics");
              break;
            }
            pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
            bt_gatt_n_aquire_peer_tx_notification(pt_con_ctr);
            notifyProcessGeneralBlock(_msg_queue_id_from_ble,
                                      BT_EVENT_SERVICES_RESOLVED,
                                      0xFFFFFFFF);
#endif
          }
          else if(_pt_app_mgt->bt_role == BT_ROLE_SERVER)
          {
            // TODO: to be confirmed.
            // skip this check for server role
            // Quickly double check the characteristics are valid
            // pthread_mutex_lock(&pt_con_ctr->mtx_for_chr_queue);
            // if(l_queue_length(pt_con_ctr->svr_chr_queue) <
            //   EXPECTED_BT_CHR_NUM)
            // {
            //   pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
            //   DBG_LOG_ERR("The peer seems with too few characteristics");
            //   break;
            // }
            // pthread_mutex_unlock(&pt_con_ctr->mtx_for_chr_queue);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
            bt_gatt_n_aquire_peer_tx_notification(pt_con_ctr, 0);
#else
            bt_gatt_n_aquire_peer_tx_notification(pt_con_ctr);
#endif
            // no need send it for server role. skip it. to be confirmed.
            // notifyProcessGeneralBlock(_msg_queue_id_from_ble,
            //                           BT_EVENT_SERVICES_RESOLVED,
            //                           0xFFFFFFFF);
          }
        }
        break;
      }
#endif /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1 */
#if (APP_PRO_BLE_NEW_ADV_REQUIRED == 1)
      case BT_EVENT_ADD_LEADV_MGR:
      {
        DBG_LOG_INFO("Recv BT_EVENT_ADD_LEADV_MGR!\n");
        const struct bluetooth_adv_controller *pt_adv_ctr =
          bt_adv_n_get_adv_ctr();

        if(true != bt_ad_register_object(pt_adv_ctr))
        {
          DBG_LOG_ERR("Failed to register object for adv");
        }
        fflush(stdout);
        break;
      }
#endif /* APP_PRO_BLE_NEW_ADV_REQUIRED == 1 */
#endif /* SKF_GW_NEW == 1 */
      default:
      {
        DBG_LOG_WARN("Unknown BT event (%d)", _hld_btevent);
        break;
      }
    }
    _hld_btevent = BT_EVENT_UNDEFINED;
  }
}
/**
 * @brief Thread to deal periodic events with coarse granularity (1 s)
 */
static void *
thandler_ble_periodicloop(void *pt_para)
{
  app_management *_pt_app_mgt = sys_get_mgt();
  struct l_dbus_proxy *_pt_proxy;
  struct con_controller *_pt_con_ctr = bt_con_n_get_con_ctr();
  size_t _cnt_s = 0;
  int _status = 0;

  do{
    _cnt_s++;
    sleep(1);

    if(_g_child_pid > 0)
    {
      if(waitpid(_g_child_pid, &_status, WNOHANG) == -1)
      {
        perror("waitpid failed");
        DBG_LOG_ERR("The child process (%d) exited with status (%d)\n\r",
                    _g_child_pid, _status);
        fflush(stdout);
        sync();
        sync();
        sync();
        exit(EXIT_FAILURE);
      }
    }

    // Notes: (to be confirmed)
    // add a check here to avoid having access to NULL (dbus might not be ready??)
    if(true == restart_dbus_required)
    {
      DBG_LOG_WARN("dbus restarting... _cnt_s %d\n", _cnt_s);
      continue;
    }
    // not try to connect in server role/ when switching role
    pthread_mutex_lock(&_pt_app_mgt->mtx_for_bt_sw);
    if((BT_ROLE_CLIENT != _pt_app_mgt->bt_role) ||
       (_pt_app_mgt->bt_sw_in_progress))
    {
      pthread_mutex_unlock(&_pt_app_mgt->mtx_for_bt_sw);
      DBG_LOG_INFO("periodicloop: skip connect in server role, _cnt_s %d\n", _cnt_s);
      continue;
    }
    pthread_mutex_unlock(&_pt_app_mgt->mtx_for_bt_sw);
    _pt_proxy = sys_get_proxy_adapter1(_pt_app_mgt);
    DBG_LOG_INFO("_pt_proxy @ 0x%llx, _cnt_s %d", _pt_proxy, _cnt_s);
    if((NULL != _pt_proxy) && (restart_dbus_required == false))
    {
      // Do the followings only after BLE dbus is ready ...
#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
      if(0 == (_cnt_s % 8))
      {
        int32_t _queue_len = 0;

        pthread_mutex_lock(&_pt_con_ctr->mtx_for_dev_queue);
        _queue_len = l_queue_length(_pt_con_ctr->dev_queue);
        pthread_mutex_unlock(&_pt_con_ctr->mtx_for_dev_queue);
        DBG_LOG_INFO("Length of device proxy queue is %d", _queue_len);

        bt_con_n_connect_to_dev(_pt_con_ctr);
        // add an extra check in case connection timeout! while dbug restarted
        if(false == restart_dbus_required)
        {
#if (0) // comment it out on test purpose
        // to identify whether it's the root cause of 'hung' ble_process or not.
          sys_dump_dbus_proxy_queue(_pt_app_mgt);
#endif
        }
      }
#else /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1 */
#if (SKF_GW_NEW == 1)
      // no need to call it in case 'APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 0'
      // bt_con_n_connect_to_dev(_pt_con_ctr);
      // add an extra check in case connection timeout! while dbug restarted
      if(false == restart_dbus_required)
      {
        sys_dump_dbus_proxy_queue(_pt_app_mgt);
      }
#endif /* SKF_GW_NEW == 1 */
#endif /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED != 1 */
    }
  }while(1);
}
/**
 * @brief Thread for the loop of bluez
 */
static void *
thandler_ble_bluezloop(void *pt_para)
{
  struct l_dbus *_ptdbus = NULL;
  struct l_dbus_client *_ptclient = NULL;
  unsigned int _iwd_watch_id;

  // Prepare for DBUS
  if(!l_main_init())
  {
    DBG_LOG_ERR("Failure in l_main_init");
    exit(EXIT_FAILURE);
  }
  _g_flag = true;

#if (BLUEZ_DBUS_LOG_ENABLE)
  // Set log output to /dev/log
  l_log_set_syslog();
#else /* BLUEZ_DBUS_LOG_ENABLE */
  l_log_set_null();
  DBG_LOG_INFO("The dbus log output is disabled.");
#endif /* !BLUEZ_DBUS_LOG_ENABLE */

  _ptdbus = l_dbus_new_default(L_DBUS_SYSTEM_BUS);
#if (BLUEZ_DBUS_LOG_ENABLE)
  l_dbus_set_debug(_ptdbus, do_debug, "[SKF_GW_NEW] ", NULL);
#endif /* BLUEZ_DBUS_LOG_ENABLE */
  l_dbus_set_ready_handler(_ptdbus, ready_callback, _ptdbus, NULL);
  l_dbus_set_disconnect_handler(_ptdbus, disconnect_callback, NULL, NULL);
  // Initialize the dbus pointer in @p g_app_management
  sys_init_set_dbus(sys_get_mgt(), _ptdbus);
#if (APP_PRO_BLE_NEW_ADV_REQUIRED == 1)
  bt_adv_n_set_dbus_handler(bt_adv_n_get_adv_ctr(), _ptdbus);
#endif /* APP_PRO_BLE_NEW_ADV_REQUIRED == 1 */

  // Service basic watch
  _iwd_watch_id = l_dbus_add_service_watch(_ptdbus,
                                           BLUEZ_NAME,
                                           iwd_service_appeared,
                                           iwd_service_disappeared,
                                           NULL, NULL);

  // Start a dbus client
  _ptclient = l_dbus_client_new(_ptdbus, BLUEZ_NAME, BLUEZ_PATH);
  l_dbus_client_set_connect_handler(_ptclient,
                                    bluez_client_connected,
                                    NULL,
                                    NULL);
  l_dbus_client_set_disconnect_handler(_ptclient,
                                       bluez_client_disconnected,
                                       NULL,
                                       NULL);
  l_dbus_client_set_proxy_handlers(_ptclient, proxy_added, proxy_removed,
                                   property_changed, NULL, NULL);
  l_dbus_client_set_ready_handler(_ptclient, bluez_client_ready, NULL,
                                  NULL);

  // Start the dbus loop
  l_main_run_with_signal(signal_handler, NULL);

  // Destroy
  restart_dbus_required = true;
#if (APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1)
  bt_gatt_n_unregister_local_service(sys_init_get_dbus(sys_get_mgt()),
                                     NULL);
#endif /* APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1 */
  l_dbus_remove_watch(_ptdbus, _iwd_watch_id);
  l_dbus_client_destroy(_ptclient);
  fflush(stdout);
  l_dbus_destroy(_ptdbus);

  restart_dbus_required = true;
  _g_flag = false;

#if (1)
  bool ret = false;
  ret = l_main_exit();
  DBG_LOG_INFO("l_main_exit with %d",ret);
  fflush(stdout);
#else
  l_main_exit();
#endif
}
/**
 * @brief Callback of DBUS - To log dbus (borrowed from
 * dbus_test/utest_dbus_client.c)
 */
static void
do_debug(
  const char *str,
  void *user_data)
{
  const char *prefix = user_data;

  l_info("%s%s", prefix, str);
}
/**
 * @brief Callback of DBUS - Called when a signal received (borrowed from
 * dbus_test/utest_dbus_client.c)
 */
static void
signal_handler(
  uint32_t signo,
  void *user_data)
{
  DBG_LOG_INFO("Signal %d received", signo);

  switch(signo)
  {
    case SIGINT:
    case SIGTERM:
      l_info("Terminate");
      DBG_LOG_INFO("Terminate");
      l_main_quit();
      break;
  }
}
/**
 * @brief Callback of DBUS - DBUS ready handler (borrowed from
 * dbus_test/utest_dbus_client.c)
 */
static void
ready_callback(void *user_data)
{
  l_info("ready");
  DBG_LOG_INFO("Connection is ready");
}
/**
 * @brief Callback of DBUS - DBUS disconnect handler (borrowed from
 * dbus_test/utest_dbus_client.c)
 */
static void
disconnect_callback(void *user_data)
{
  DBG_LOG_WARN("D-BUS disconnect");
  l_main_quit();
}
/**
 * @brief Callback of DBUS - Called when a bluez service appeared (borrowed
 * from dbus_test/utest_dbus_client.c)
 */
static void
iwd_service_appeared(
  struct l_dbus *dbus,
  void *user_data)
{
  l_info("Service appeared");
  DBG_LOG_DEBUG("Service appeared");
}
/**
 * @brief Callback of DBUS - Called when a bluez service disappeared
 * (borrowed from dbus_test/utest_dbus_client.c)
 */
static void
iwd_service_disappeared(
  struct l_dbus *dbus,
  void *user_data)
{
  l_info("Service disappeared");
  DBG_LOG_DEBUG("Service disappeared");
}
/**
 * @brief Callback of DBUS - Called when a bluez client connected
 * (borrowed from dbus_test/utest_dbus_client.c)
 */
static void
bluez_client_connected(
  struct l_dbus *dbus,
  void *user_data)
{
  l_info("Client connected");
  DBG_LOG_DEBUG("Client connected");
}
/**
 * @brief Callback of DBUS - Called when a bluez client disconnected
 * (borrowed from dbus_test/utest_dbus_client.c)
 */
static void
bluez_client_disconnected(
  struct l_dbus *dbus,
  void *user_data)
{
  l_info("Client disconnected");
  DBG_LOG_DEBUG("Client disconnected");
}
/**
 * @brief Callback of DBUS - Called when a bluez client is ready
 * (borrowed from dbus_test/utest_dbus_client.c)
 */
static void
bluez_client_ready(
  struct l_dbus_client *client,
  void *user_data)
{
  l_info("Client ready");
  DBG_LOG_DEBUG("Client Ready");
}
static void
proxy_added(
  struct l_dbus_proxy *proxy,
  void *user_data)
{
  const char *interface = l_dbus_proxy_get_interface(proxy);
  const char *path = l_dbus_proxy_get_path(proxy);

  l_info("Proxy added: %s %s", path, interface);
  DBG_LOG_INFO("Proxy added: %s ,%s\n", path, interface);
#if 0
  if(!strcmp(interface, "org.test"))
  {
    char *str;

    if(!l_dbus_proxy_get_property(proxy, "String", "s", &str))
    {
      return;
    }

    l_info("  the string : %s", str);
    DBG_LOG_INFO(" the string : %s", str);
  }
#else
  sys_add_proxy(sys_get_mgt(), proxy);
#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
  bt_con_n_append_chr_proxy_to_queue(bt_con_n_get_con_ctr(), proxy);
#endif /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1 */
#if (APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1)
  if(0 == strcmp(interface, "org.bluez.GattManager1"))
  {
    bt_gatt_n_register_local_service(sys_init_get_dbus(sys_get_mgt()),
                                     proxy);
  }
#endif /* APP_PRO_BLE_NEW_BEING_CONNECTED_REQUIRED == 1 */
#endif

#if (APP_PRO_BLE_NEW_ADV_REQUIRED == 1)
  bt_adv_n_set_adv_manager_proxy(bt_adv_n_get_adv_ctr(), proxy);
  bt_adv_n_set_adv_gwid_string(bt_adv_n_get_adv_ctr(), proxy);
#endif /* APP_PRO_BLE_NEW_ADV_REQUIRED == 1 */
}
static void
proxy_removed(
  struct l_dbus_proxy *proxy,
  void *user_data)
{
  l_info("Proxy removed: %s %s", l_dbus_proxy_get_path(proxy),
         l_dbus_proxy_get_interface(proxy));
  DBG_LOG_INFO("Proxy removed: %s , %s", l_dbus_proxy_get_path(proxy),
                l_dbus_proxy_get_interface(proxy));

  sys_remove_proxy(sys_get_mgt(), proxy);

#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
  bt_con_n_remove_chr_proxy_from_queue(bt_con_n_get_con_ctr(), proxy);
  bt_con_n_remove_dev_proxy(bt_con_n_get_con_ctr(), proxy);
#endif /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1 */
}
static void
property_changed(
  struct l_dbus_proxy *proxy,
  const char *name,
  struct l_dbus_message *msg,
  void *user_data)
{
  DBG_LOG_INFO("Property changed: %s (%s , %s)", name,
                l_dbus_proxy_get_path(proxy),
                l_dbus_proxy_get_interface(proxy));
#if (APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1)
  bt_con_n_proxy_property_changed(bt_con_n_get_con_ctr(), proxy, name,
                                  msg, user_data);
  bt_con_n_service_property_changed(bt_con_n_get_con_ctr(), proxy, name,
                                    msg, user_data);
  bt_con_n_append_dev_proxy(bt_con_n_get_con_ctr(), proxy, name,
                            msg, user_data);
#else
  pthread_mutex_lock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
  l_queue_push_tail(sys_get_mgt()->pt_adv_scanning_event_queue,
                    (void *)BT_EVENT_DO_NOTHING);
  pthread_cond_signal(&(sys_get_mgt()->cond_val_adv_scanning_ctr));
  pthread_mutex_unlock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
#endif /* APP_PRO_BLE_NEW_CONNECTION_REQUIRED == 1 */
}
/**
 * @brief To notify bluetooth event to the process general in a block
 * manner
 * @param msqId: The ID of message queue
 * @param btevent: The Bluetooth event
 * @param msgid: message ID
 * @return ST_OK if everthing is OK
 */
static app_state_t
notifyProcessGeneralBlock(
  const int32_t msqId,
  const enum bluetooth_event btevent,
  uint32_t msgid)
{
  app_state_t _ret = ST_OK;

#if (SKF_GW_NEW == 1)
  struct ipc_msg_v3 _ipc_msg = { 0 };

  if(msqId < 0)
  {
    DBG_LOG_ERR("Invalid msg queue ID");
    _ret = ST_ERR;
    goto EXIT;
  }

  // Prepare the message to notify the general process
  _ipc_msg.type = M_TYPE_FEEDBACK;
  _ipc_msg.payload.feedback_info.cmd = CMD_NOTIFY;
  _ipc_msg.payload.feedback_info.msgid = msgid;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  _ipc_msg.payload.feedback_info.con_idx = msgid;
#endif
  _ipc_msg.payload.feedback_info.info.event_notification_info.bt_event =
    btevent;

  switch (btevent)
  {
    case BT_EVENT_CONNECTION:
    {
      if(sys_get_mgt()->bt_role == BT_ROLE_SERVER)
      {
        // do nothing for server role. 
        DBG_LOG_DEBUG("BT_EVENT_CONNECTION in server role!\n");
        break;
      }
      // Prepare the connected device address of the message
      struct con_controller *pt_con_ctr = bt_con_n_get_con_ctr();

      memset(
        &_ipc_msg.payload.feedback_info.info.event_notification_info.info.dev
        .connected_dev,
        0,
        sizeof(_ipc_msg.payload.feedback_info.info.
              event_notification_info.info.dev.connected_dev));
      pthread_mutex_lock(&pt_con_ctr->mtx_for_connected_proxy);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      if(msgid >= CONFIG_BLE_CONNECTION_MAX_NUM)
      {
        pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
        DBG_LOG_WARN("wrong msgid - idx:%d\n", msgid);
        _ret = ST_ERR;
        goto EXIT;
      }
      if(pt_con_ctr->connected_proxy[msgid] == NULL)
#else
      if(pt_con_ctr->connected_proxy == NULL)
#endif
      {
        if(sys_get_mgt()->bt_role == BT_ROLE_CLIENT)
        {
          pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
          DBG_LOG_WARN("Failed to get the connected proxy");
          _ret = ST_ERR;
          goto EXIT;
        }else{
          DBG_LOG_WARN("Failed to get the connected proxy, but do nothing as the role is not client");
        }
      }
      else
      {
        do{
          const char *_pt_interface = NULL;
          const char *_pt_addr = NULL;

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
          _pt_interface =
            l_dbus_proxy_get_interface(pt_con_ctr->connected_proxy[msgid]);
#else
          _pt_interface =
            l_dbus_proxy_get_interface(pt_con_ctr->connected_proxy);
#endif
          if(NULL == _pt_interface)
          {
            pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
            DBG_LOG_WARN("Failed to get proxy interface");
            _ret = ST_ERR;
            goto EXIT;
          }
          if(strcmp(_pt_interface, "org.bluez.Device1"))
          {
            pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
            DBG_LOG_WARN("Dev proxy is expected");
            _ret = ST_ERR;
            goto EXIT;
          }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
          if(false == l_dbus_proxy_get_property(pt_con_ctr->connected_proxy[msgid],
            "Address", "s",
            &_pt_addr))
#else
          if(false == l_dbus_proxy_get_property(pt_con_ctr->connected_proxy,
                                                "Address", "s",
                                                &_pt_addr))
#endif
          {
            pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);
            DBG_LOG_ERR("Failed to get property");
            _ret = ST_ERR;
            goto EXIT;
          }
          DBG_LOG_INFO("The connected MAC addr: %s", _pt_addr);
          {
            uint8_t i = 0, cnt = 0;
            char _kp_strbuf[MAX_ID_STR_LENGTH] = { 0 };

            i = 0;
            cnt = 0;
            while(_pt_addr[i])
            {
              if(_pt_addr[i] != ':')
              {
                _kp_strbuf[cnt % MAX_ID_STR_LENGTH] = _pt_addr[i];
                cnt++;
              }
              i++;
            }

            uint8_t _tmp_addr[6];

            sscanf(_kp_strbuf, "%02x%02x%02x%02x%02x%02x",
                  &_tmp_addr[0],
                  &_tmp_addr[1],
                  &_tmp_addr[2],
                  &_tmp_addr[3],
                  &_tmp_addr[4],
                  &_tmp_addr[5]);

            _ipc_msg.payload.feedback_info.info.event_notification_info.info.
            dev.connected_dev.nap = _tmp_addr[0] * 0x100 + _tmp_addr[1];
            _ipc_msg.payload.feedback_info.info.event_notification_info.info.
            dev.connected_dev.uap = _tmp_addr[2];
            _ipc_msg.payload.feedback_info.info.event_notification_info.info.
            dev.connected_dev.lap = _tmp_addr[3] * 0x100 * 0x100 +
                                    _tmp_addr[4] * 0x100 +
                                    _tmp_addr[5];
          }
        }while(0);
      }
      pthread_mutex_unlock(&pt_con_ctr->mtx_for_connected_proxy);

      // Prepare the gateway address
      memset(
        &_ipc_msg.payload.feedback_info.info.event_notification_info.info.dev
        .adapterAddr,
        0,
        sizeof(&_ipc_msg.payload.feedback_info.info.event_notification_info.
              info.dev.adapterAddr));

      uint8_t _tmp_addr[6];

      DBG_LOG_DEBUG("adapterAddr = %s",
                    sys_get_mgt()->gw_id_str);

      sscanf(sys_get_mgt()->gw_id_str, "%02x%02x%02x%02x%02x%02x",
            &_tmp_addr[0],
            &_tmp_addr[1],
            &_tmp_addr[2],
            &_tmp_addr[3],
            &_tmp_addr[4],
            &_tmp_addr[5]);
      _ipc_msg.payload.feedback_info.info.event_notification_info.info.dev.
      adapterAddr.nap =
        _tmp_addr[0] * 0x100 + _tmp_addr[1];
      _ipc_msg.payload.feedback_info.info.event_notification_info.info.dev.
      adapterAddr.uap =
        _tmp_addr[2];
      _ipc_msg.payload.feedback_info.info.event_notification_info.info.dev.
      adapterAddr.lap =
        _tmp_addr[3] * 0x100 * 0x100 +
        _tmp_addr[4] * 0x100 + _tmp_addr[5];
      DBG_LOG_DEBUG("adapterAddr = %.4X : %.2X : %.6X",
                    _ipc_msg.payload.feedback_info.info.
                    event_notification_info.info.dev.
                    adapterAddr.nap,
                    _ipc_msg.payload.feedback_info.info.
                    event_notification_info.info.dev.
                    adapterAddr.uap,
                    _ipc_msg.payload.feedback_info.info.
                    event_notification_info.info.dev.
                    adapterAddr.lap);
      DBG_LOG_DEBUG("adapterAddr = %02x%02X%02X%02x%02X%02X",
                    _tmp_addr[0],
                    _tmp_addr[1],
                    _tmp_addr[2],
                    _tmp_addr[3],
                    _tmp_addr[4],
                    _tmp_addr[5]);
#if (APP_PRO_BLE_NEW_MULTI_CON_ENABLED == 1)
      // check the connect by proxy
      if(pt_con_ctr->connected_proxy[msgid] == g_con_stat_data._con_proxy)
      {
        _ipc_msg.payload.feedback_info.con_start_time_ms = g_con_stat_data.start_con_time_ms;
      }
      else
      {
        _ipc_msg.payload.feedback_info.con_start_time_ms = 0;
      }
#endif
      break;
    }
    case BT_EVENT_DISCONNECTION:
    {
      if(sys_get_mgt()->bt_role == BT_ROLE_SERVER)
      {
        // do nothing for server role. 
        DBG_LOG_DEBUG("BT_EVENT_DISCONNECTION in server role!\n");
        break;
      }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      if(msgid >= CONFIG_BLE_CONNECTION_MAX_NUM)
      {
        DBG_LOG_WARN("wrong msgid - idx:%d\n", msgid);
        _ret = ST_ERR;
        goto EXIT;
      }
#endif
      break;
    }
    case BT_EVENT_SERVICES_RESOLVED:
    {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      if(msgid >= CONFIG_BLE_CONNECTION_MAX_NUM)
      {
        DBG_LOG_WARN("wrong msgid - idx:%d\n", msgid);
        _ret = ST_ERR;
        goto EXIT;
      }
#endif
      break;
    }
    default:
      break;
  }
  if(0 != msgsnd(msqId, (const void *)&_ipc_msg, sizeof(_ipc_msg.payload),
                 0))
  {
    DBG_LOG_ERR("Failed to send msg to the process general");
    _ret = ST_ERR;
    goto EXIT;
  }
#endif /* SKF_GW_NEW == 1 */
EXIT:
  return _ret;
}
/**
 * @brief To get gateway's address and store the address to @p gw_id_str of
 * @p g_app_management in string.
 * @param pt_app_mgt: A pointer to @p g_app_management
 * @param pt_adapter: The adapter
 * @return ST_OK if everthing is OK
 */
static app_state_t
get_gateway_id(
  app_management *pt_app_mgt,
  struct l_dbus_proxy *pt_adapter)
{
  app_state_t _ret = ST_OK;
  const char *_pt_interface = NULL;
  const char *_pt_addr = NULL;
  char _kp_strbuf[MAX_ID_STR_LENGTH] = { 0 };

  if((NULL == pt_app_mgt) || (NULL == pt_adapter))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }
  _pt_interface = l_dbus_proxy_get_interface(pt_adapter);
  if(NULL == _pt_interface)
  {
    DBG_LOG_ERR("Failed to get proxy interface");
    _ret = ST_ERR;
    return _ret;
  }
  if(strcmp(_pt_interface, "org.bluez.Adapter1"))
  {
    DBG_LOG_WARN("Adapter proxy is expected");
    _ret = ST_ERR;
    return _ret;
  }
  if(false == l_dbus_proxy_get_property(pt_adapter, "Address", "s",
                                        &_pt_addr))
  {
    DBG_LOG_ERR("Failed to get property");
    _ret = ST_ERR;
    return _ret;
  }
  DBG_LOG_DEBUG("Adapter addr: %s", _pt_addr);
  {
    uint8_t i = 0, cnt = 0;

    i = 0;
    cnt = 0;
    while(_pt_addr[i])
    {
      if(_pt_addr[i] != ':')
      {
        _kp_strbuf[cnt % MAX_ID_STR_LENGTH] = _pt_addr[i];
        cnt++;
      }
      i++;
    }
  }

  _kp_strbuf[MAX_ID_STR_LENGTH - 1] = 0; // make sure 0 is the string
                                         // terminal
  sys_set_gw_id_string(_kp_strbuf);

  return _ret;
}