#include "cJSON.h"
#include "global.h"
#include "mqtt.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
// message
#include "ppGW.h"
#include "DeviceAppBulletGateway.pb.h"
// #include <assert.h>
#include "common.h"
#include "http.h"
#include "ipcmbs.h"
#include "jsonbuild.h"
#include "jsonparse.h"
#include <pthread.h>

// #include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include "thread_waitmsg.h"
#include "upmsg.h"

// LED
#include "app_io.h"

// LTE
#include "app_lte.h"
DBG_LOCAL_LOG_DEBUG

//
pid_t pid[2] = {0};
pid_t mqtt_pid = 0;
int sql_msgid = 0;
int mqtt_msgid = 0;
int ble_msgid = 0;

// static struct msgbuf msgbufp_tx = {0};
SKFChina_SensingDataUpload_Measurement middle_measure[SENSOR_DATA_NUM] = {0};
SKFChina_SensingDataUpload_Measurement wave_middle_measure = {0};
gw_pro_data_t middle_data[SENSOR_DATA_NUM] = {0};
extern char mqtt_addr[100];
extern struct disabledGroup disabledData;
extern struct disabledGroup disabledConfig;
char rev_msgbuf[4096] = {0};
static uint8_t child_no = 0;
static void wait_mqtt_server();
static void *wait_ota(void *arg);
static pthread_t tid = 0;
static pthread_t tidLed = 0;
static pthread_t tidLedTimer = 0;
extern bool mqtt_isConnected;
static gw_pro_sqlite_cond_element_t
    filterData[GW_PRO_MAX_DATA_FIELD_PER_OPERATION];
static gw_pro_command_sqlite_filter_response_t filter_rx;
SKFChina_App_AppMessage SKF_BullGateway = {0};
sem_t otasem = {0};
sem_t updatasem = {0};
sem_t enablehttpsem = {0};
gw_pro_process_type_t set_from_process = gw_pro_process_mqtt;
// update it's name to avoid confusing
void *
thandler_tim_tick_service(void *pt_para)
{
	uint32_t kp_cnt = 0;
  
  while(1)
	{
		sleep(1);
		LOG_DEBUG(OUTPOINT, "Seconds %d\n", kp_cnt++);
		// To post an tick event to LED-controller
		app_io_post_led_event(LED_EV_TIM_TICK);
	}
	
	return NULL;
}
void clean_msgq() {
  struct msqid_ds buf = {0};
  struct msgbuf msgbufp = {0};
  msgctl(mqtt_msgid, IPC_STAT, &buf);
  LOG_DEBUG(OUTPOINT, "msgnum1:%ld\r\n", buf.msg_qnum);
  for (uint8_t i = 0; i < buf.msg_qnum; i++) {
    msgrcv(mqtt_msgid, &msgbufp, MSG_BUF_LEN, 0, 0);
  }
  msgctl(sql_msgid, IPC_STAT, &buf);
  LOG_DEBUG(OUTPOINT, "msgnum2:%ld\r\n", buf.msg_qnum);
  for (uint8_t i = 0; i < buf.msg_qnum; i++) {
    msgrcv(sql_msgid, &msgbufp, MSG_BUF_LEN, 0, 0);
  }
  msgctl(ble_msgid, IPC_STAT, &buf);
  LOG_DEBUG(OUTPOINT, "msgnum3:%ld\r\n", buf.msg_qnum);
  for (uint8_t i = 0; i < buf.msg_qnum; i++) {
    msgrcv(ble_msgid, &msgbufp, MSG_BUF_LEN, 0, 0);
  }
}
// root
// for logging
uint8_t g_log_level = CONFIG_LOG_LEVEL_SET;
#define LOG_LEVEL_PARAMETER_IDX (1)

int
main(
  int argc,
  char **argv)
{
  LOG_INFO(OUTPOINT, "mqtt process started ...\n\r");
  LOG_DEBUG(OUTPOINT, "Number of arguments (#) = %d\n\r", argc);
  for(uint32_t i = 0; i < argc; i++)
  {
    LOG_DEBUG(OUTPOINT, "argv[%d]: %s\n\r", i, argv[i]);
  }
  if(argc >= LOG_LEVEL_PARAMETER_IDX + 1)
  {
    uint8_t tmpu8 = (uint8_t)strtol(argv[LOG_LEVEL_PARAMETER_IDX], NULL, 10);
    if(tmpu8 < CONFIG_LOG_LEVEL_ALL)
    {
      g_log_level = tmpu8;
    } else
    {
      g_log_level = CONFIG_LOG_LEVEL_SET;
    }
  } else  // set to default 
  {
    g_log_level = CONFIG_LOG_LEVEL_SET;
  }
#if (GW_PRO == 1)
  LOG_INFO(OUTPOINT, "MQTT_PRO started ...\r\n");
#else
  LOG_INFO(OUTPOINT, "MQTT_BASIC started ...\r\n");
#endif
  // init GPIO for LED-control
	app_io_init_gpio_for_led();
  // to init the LED controller
	app_io_init_led_controller(app_io_get_led_ctr());
  // thread for LED-control and a timer
	int res = pthread_create( &tidLed, NULL, thandler_for_led_control, NULL);
	if(0 != res)
	{
		LOG_ERR(OUTPOINT, "Failed to create thread for LED-control");
		exit(EXIT_FAILURE);
	}
  res = pthread_create( &tidLedTimer, NULL, thandler_tim_tick_service, NULL);
	if(0 != res)
	{
		LOG_ERR(OUTPOINT, "Failed to create thread for timer");
		exit(EXIT_FAILURE);
	}
  // And no error at the beginning
  app_io_post_led_event( LED_EV_MQTT_ERR_RL );
  
  mqtt_msgid = msgget(MSG_MQTT_QUEUE_KEY, IPC_CREAT | MSG_QUEUE_FLAG);
  sql_msgid = msgget(MSG_SQL_QUEUE_KEY, IPC_CREAT | MSG_QUEUE_FLAG);
  ble_msgid = msgget(MSG_BLE_QUEUE_KEY, IPC_CREAT | MSG_QUEUE_FLAG);
  mqtt_pid = getpid();
  LOG_INFO(OUTPOINT, "mqtt pid:%d\r\n", mqtt_pid);
  if (mqtt_msgid == -1 || sql_msgid == -1) {
    LOG_WARN(OUTPOINT, "Get the msg queue failed\r\n");
  }
  clean_msgq();
  LOG_INFO(OUTPOINT, "i1 = %d,i2 = %d\r\n", pid[0], pid[1]);
  LOG_INFO(OUTPOINT, "start mqtt!\r\n");
  sleep(3);
  memset(mqtt_addr, 0, sizeof(mqtt_addr));
  memset(up_url, 0, sizeof(up_url));
  memset(ota_url, 0, sizeof(ota_url));
  sem_init(&otasem, 0, 1);
  sem_init(&updatasem, 0, 1);
  sem_init(&enablehttpsem, 0, 0);
  wait_mqtt_server();

  if ((access(PATH_MFILE_USER, F_OK)) == 0) {
    // The magic file exits ...
    // And try to load magic number from the file ... 
    parse_config(PATH_MFILE_USER);
  } else if ((access(PATH_MFILE_DEFAULT, F_OK)) == 0) {
    // The magic file exits ...
    // And try to load magic number from the file ... 
    parse_config(PATH_MFILE_DEFAULT);
  } else {
    // The magic file is not found ...
    // All data and configurations are allowed
    memset(&disabledData, 0, sizeof(disabledData));
    memset(&disabledConfig, 0, sizeof(disabledConfig));
  }

#ifdef CONFIG_REPORT_FALG
  res = pthread_create(&tid, NULL, wait_uplaod_timer, NULL);
#else
  res = pthread_create(&tid, NULL, wait_from_sqlite, NULL);
#endif
  if (res) {
    LOG_INFO(OUTPOINT, "Failed to creat a new thread!\r\n");
  }
  res = pthread_create(&tid, NULL, wait_ota, NULL);
  if (res) {
    LOG_INFO(OUTPOINT, "Failed to creat a new thread!\r\n");
  }
  mqtt_task();
  LOG_ERR(OUTPOINT, "The process should not go here!\r\n");
  return 0;
}

static void wait_mqtt_server() {
  char tempstr[10] = {0};
  struct msgbuf msgbufp = {0};
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)msgbufp.mtext;
  // wait for the valid server
  child_no = 0;
  while (1) {
    snprintf(mqtt_addr, sizeof(mqtt_addr) - 1, "%s", "mqtt://");
    LOG_INFO(OUTPOINT, "wait server\r\n");
    memset(&msgbufp, 0, sizeof(msgbufp));
    if (ipc_sql_query_item(GATEWAY_CONFIGURE_TABLE, 0, &msgbufp) ||
        pdata->command_or_response.responseMsg.response ==
            gw_pro_response_failed ||
        pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response.dataNum == 0) {
      sleep(2);
      continue;
    }
    LOG_DEBUG(OUTPOINT, "%d\r\n",
              pdata->command_or_response.responseMsg.responseInfo
                  .sqlite_update_item_response.dataNum);
    for (uint32_t i = 0; i < pdata->command_or_response.responseMsg.responseInfo
                                 .sqlite_update_item_response.dataNum;
         i++) {

      switch (pdata->command_or_response.responseMsg.responseInfo
                  .sqlite_update_item_response.inquiredData[i]
                  .field) {
      case 3: {
        if (!strlen(pdata->command_or_response.responseMsg.responseInfo
                        .sqlite_update_item_response.inquiredData[i]
                        .data.data_char))
          continue;
        snprintf(clientID, sizeof(clientID) - 1, "%s",
                 pdata->command_or_response.responseMsg.responseInfo
                     .sqlite_update_item_response.inquiredData[i]
                     .data.data_char);
      } break;
      case 21: {
        if (!strlen(pdata->command_or_response.responseMsg.responseInfo
                        .sqlite_update_item_response.inquiredData[i]
                        .data.data_char))
          continue;
        strcat(mqtt_addr, pdata->command_or_response.responseMsg.responseInfo
                              .sqlite_update_item_response.inquiredData[i]
                              .data.data_char);
        strcat(mqtt_addr, ":");
      } break;
      case 22: {
        if (pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response.inquiredData[i]
                .data.data_uint16 == 0)
          continue;
        snprintf(tempstr, sizeof(tempstr) - 1, "%d",
                 pdata->command_or_response.responseMsg.responseInfo
                     .sqlite_update_item_response.inquiredData[i]
                     .data.data_uint16);
        strcat(mqtt_addr, tempstr);
      } break;
      case 23: {
        if (pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response.inquiredData[i]
                .data.data_uint8 == 0)
          isDhcp = false;
        else
          isDhcp = true;
      } break;
      case 27: {
        if (!strlen(pdata->command_or_response.responseMsg.responseInfo
                        .sqlite_update_item_response.inquiredData[i]
                        .data.data_char))
          continue;
        snprintf(gwIp, sizeof(gwIp) - 1, "%s",
                 pdata->command_or_response.responseMsg.responseInfo
                     .sqlite_update_item_response.inquiredData[i]
                     .data.data_char);
      } break;
      }
    }
    // // get the child mac
    // memset(&filterData, 0, sizeof(filterData));
    // memset(&filter_rx, 0, sizeof(filter_rx));
    // filterData[0].condition = gw_pro_sqlite_cond_all;
    // filterData[0].field = 1;
    // if (ipc_sql_filter_item(DEVICE_LIST_TABLE, 1, filterData) ||
    //     pdata->command_or_response.responseMsg.response ==
    //         gw_pro_response_failed ||
    //     pdata->command_or_response.responseMsg.responseInfo
    //             .sqlite_update_item_response.dataNum == 0) {
    //   LOG_INFO(OUTPOINT, "Filed to get the device id from Dev table\r\n");
    // }
    // memcpy(&filter_rx,
    //        &pdata->command_or_response.responseMsg.responseInfo
    //             .sqlite_update_item_response,
    //        sizeof(gw_pro_command_sqlite_filter_response_t));
    // for (uint16_t i = 0; i < filter_rx.dataNum; i++) {
    //   if (ipc_sql_query_item(DEVICE_LIST_TABLE, 0) ||
    //       pdata->command_or_response.responseMsg.response ==
    //           gw_pro_response_failed ||
    //       pdata->command_or_response.responseMsg.responseInfo
    //               .sqlite_update_item_response.dataNum == 0) {
    //     LOG_INFO(OUTPOINT, "Filed to get the device id from Dev
    //     table\r\n");
    //   }
    //   for (uint32_t i = 0;
    //        i < pdata->command_or_response.responseMsg.responseInfo
    //                .sqlite_update_item_response.dataNum;
    //        i++) {
    //     switch (pdata->command_or_response.responseMsg.responseInfo
    //                 .sqlite_update_item_response.inquiredData[i]
    //                 .field) {
    //     case 3: {
    //       snprintf(clientID, sizeof(clientID) - 1, "%s",
    //                pdata->command_or_response.responseMsg.responseInfo
    //                    .sqlite_update_item_response.inquiredData[i]
    //                    .data.data_char);
    //     } break;
    //     case 21: {
    //       strcat(mqtt_addr,
    //       pdata->command_or_response.responseMsg.responseInfo
    //                             .sqlite_update_item_response.inquiredData[i]
    //                             .data.data_char);
    //       strcat(mqtt_addr, ":");
    //     } break;
    //     case 22: {
    //       snprintf(tempstr, sizeof(tempstr) - 1, "%d",
    //                pdata->command_or_response.responseMsg.responseInfo
    //                    .sqlite_update_item_response.inquiredData[i]
    //                    .data.data_uint16);
    //       strcat(mqtt_addr, tempstr);
    //     } break;
    //     }
    //   }
    // }
    //  LOG_DEBUG(OUTPOINT, "test1:%s\r\n",mqtt_addr);
    if (strlen(mqtt_addr) > 8)
      break;
    // LOG_DEBUG(OUTPOINT, "test2:%s\r\n",mqtt_addr);
  }
  LOG_INFO(OUTPOINT, "server:%s\r\n", mqtt_addr);
}
static void *wait_ota(void *arg) {
  struct msgbuf msgbufp = {0};
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)msgbufp.mtext;
  gw_pro_command_sqlite_query_item_response_t query_response = {0};
  char temp[100] = {0};
  char local_topic[200] = {0};
  char local_devid[32] = {0};
  SKFChina_App_AppMessage SKF_BullGateway_tx = {0};
  while (!mqtt_isConnected) {
    sleep(2);
  }
  while (1) {
    up_edit_time();
    // CONFIG_APPLICATION_VERSION
    LOG_INFO(OUTPOINT, "upload the firmware infomation!\r\n");
#if 1
    filterData[0].condition = gw_pro_sqlite_cond_all;
    filterData[0].field = 1;
    if (ipc_sql_filter_item(DEVICE_LIST_TABLE, 1, filterData, &msgbufp) ||
        pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
        pdata->command_or_response.responseMsg.response ==
            gw_pro_response_failed) {
      LOG_WARN(OUTPOINT, "Failed to get datas from Dev table\r\n");
      continue;
    }
    memset(&filter_rx, 0, sizeof(filter_rx));
    memcpy(&filter_rx,
           &pdata->command_or_response.responseMsg.responseInfo
                .sqlite_filter_item_reponse,
           sizeof(gw_pro_command_sqlite_filter_response_t));
    for (uint16_t i = 0; i < filter_rx.dataNum; i++) {
      memset(&msgbufp, 0, sizeof(msgbufp));
      if (ipc_sql_query_item(SENSOR_CONFIGURE_TABLE,
                             filter_rx.filteredItemIdx[i], &msgbufp) ||
          pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
          pdata->command_or_response.responseMsg.response ==
              gw_pro_response_failed) {
        LOG_WARN(OUTPOINT, "Failed to get datas from sensor table\r\n");
        continue;
      }
      if (pdata->command_or_response.responseMsg.responseInfo
              .sqlite_update_item_response.dataNum == 0) {
        LOG_WARN(OUTPOINT,
                 "No datas from sensor table when the nameID is %d\r\n",
                 filter_rx.filteredItemIdx[i]);
        continue;
      }
      memset(&query_response, 0, sizeof(query_response));
      memcpy(&query_response,
             &pdata->command_or_response.responseMsg.responseInfo
                  .sqlite_update_item_response,
             sizeof(gw_pro_command_sqlite_query_item_response_t));
      memset((void *)&SKF_BullGateway_tx, 0, sizeof(SKF_BullGateway_tx));

      for (uint8_t _i = 0; _i < query_response.dataNum; _i++) {
        switch (query_response.inquiredData[_i].field) {
        case 4: {
          memset(temp, 0, sizeof(temp));
          memset(local_devid, 0, sizeof(local_devid));
          snprintf(temp, sizeof(temp) - 1, "%s/%s", clientID,
                   query_response.inquiredData[_i].data.data_char);
          snprintf(local_devid, sizeof(local_devid) - 1, "%s",
                   query_response.inquiredData[_i].data.data_char);
        } break;
        // 96
        case 6: {
          // // /* get app version  just for test*/
          if (MYULT_EncodeVersion(
                  query_response.inquiredData[_i].data.data_char,
                  &SKF_BullGateway_tx._messages.current_version_upload
                       .firmware_version)) {
            LOG_WARN(OUTPOINT, "app version format error\r\n");
          }
        } break;
        default:
          break;
        }
      }
#if 0
      // // /* get app version  just for test*/
      if (MYULT_EncodeVersion(CONFIG_APPLICATION_VERSION,
                              &SKF_BullGateway_tx._messages
                                   .current_version_upload.firmware_version)) {
        LOG_WARN(OUTPOINT, "app version format error\r\n");
      }
#endif
      memset(local_topic, 0, sizeof(local_topic));
      COINFIG_MQTT_PUBLISH_FUOTA_TOPIC(local_topic, SENSOR_NAME, temp);
      Simple_upload(SKFChina_App_AppMessage_current_version_upload_tag,
                    SKF_BullGateway_tx, local_topic, local_devid);
    }
    sleep(2);
#endif
    memset(local_topic, 0, sizeof(local_topic));
    COINFIG_MQTT_PUBLISH_FUOTA_TOPIC(local_topic, GATAWAY_NAME, clientID);
    memset((void *)&SKF_BullGateway_tx, 0, sizeof(SKF_BullGateway_tx));
    /* get hardware version */
    if (MYULT_EncodeVersion(CONFIG_HARDWARE_DEFAULT_VERSION,
                            &SKF_BullGateway_tx._messages.current_version_upload
                                 .hardware_version)) {
      LOG_WARN(OUTPOINT, "hw version format error\r\n");
    }
    /* get app version */
    if (MYULT_EncodeVersion(CONFIG_APPLICATION_VERSION,
                            &SKF_BullGateway_tx._messages.current_version_upload
                                 .firmware_version)) {
      LOG_WARN(OUTPOINT, "app version format error\r\n");
    }
    Simple_upload(SKFChina_App_AppMessage_current_version_upload_tag,
                  SKF_BullGateway_tx, local_topic, clientID);
    sleep(600);
  }
}