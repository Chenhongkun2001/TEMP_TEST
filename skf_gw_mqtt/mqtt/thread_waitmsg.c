#include "thread_waitmsg.h"
#include "auxiliar.h"
#include "cJSON.h"
#include "common.h"
#include "global.h"
#include "http.h"
#include "ipcmbs.h"
#include "mqtt.h"
#include "ppGW.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "upmsg.h"
#include "jsonparse.h"
#include <assert.h>
#include <jsonbuild.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
DBG_LOCAL_LOG_DEBUG

#define CONFIG_UPLOAD_TIMER_INTERVAL 5 // unit:second
#define CONFIG_UPLOAD_TIMER_STEP (5) // unit:second, seconds for each adjustment
#define CONFIG_UPLOAD_TIMER_LIMIT (60) // unit:second, maximum seconds for the timer
#define CONFIG_UPLOAD_DATA_ITEM_LIMIT 200

extern int sql_msgid;
extern int mqtt_msgid;
extern int ble_msgid;
extern MQTTClient client;
extern pid_t mqtt_pid;
extern char mqtt_addr[100];
extern bool mqtt_isConnected;
extern sem_t updatasem;
extern bool datupload_flag;
extern bool waveupload_flag;
extern struct disabledGroup disabledData;
extern struct disabledGroup disabledConfig;
static struct msgbuf msgbufp = {0};
extern SKFChina_SensingDataUpload_Measurement middle_measure[SENSOR_DATA_NUM];
extern gw_pro_data_t middle_data[SENSOR_DATA_NUM];
static gw_pro_sqlite_cond_element_t
    filterData[GW_PRO_MAX_DATA_FIELD_PER_OPERATION];
static gw_pro_command_sqlite_filter_response_t filter_rx;
static gw_pro_data_t update_data[GW_PRO_MAX_DATA_FIELD_PER_OPERATION] = {0};
static bool continue_flag = false;
static uint8_t time_sec = CONFIG_UPLOAD_TIMER_INTERVAL;
extern bool reporting_data_flag;
#ifndef CONFIG_REPORT_FALG
static reportrx_t report_rx = {0};
static char table_name[50] = {0};
static char gwjson[CONFIG_JSON_MAX_LEN] = {0};
static char sensorjson[MSG_BUF_LEN] = {0};
static FILE *gwfp = NULL;
static FILE *sensorfp = NULL;
static pthread_t tid = 0;
static char newjsonfile[100] = {0};
static gw_pro_message_header_t *report_pdata =
    (gw_pro_message_header_t *)msgbufp.mtext;
#endif

#ifdef CONFIG_REPORT_FALG
void *wait_uplaod_timer(void *arg) {
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)msgbufp.mtext;
  bool valid_data = false;
  while (!mqtt_isConnected) {
    sleep(2);
  }
  while (1) {
    LOG_INFO(OUTPOINT, "wait upload timer\r\n");
    uint16_t filterDataCnt;
    filterDataCnt = 0;
    if(disabledData.insightTCnt > 0)
    {
      filterData[filterDataCnt].field = 5; // Type
      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_str_equal;
      snprintf(filterData[filterDataCnt].condParam.string_value.s,
             sizeof(filterData[filterDataCnt].condParam.string_value.s),
             "Insight-T");
      filterDataCnt++;
      
      for(uint16_t cnt = 0; cnt < disabledData.insightTCnt; cnt++)
      {
        filterData[filterDataCnt].condition = gw_pro_sqlite_cond_and;
        filterDataCnt++;

        filterData[filterDataCnt].field = 11; // DataType
        filterData[filterDataCnt].condition = gw_pro_sqlite_cond_value_not_equal;
        filterData[filterDataCnt].condParam.range_uint32.min = 
          disabledData.InsightT[cnt];
        filterDataCnt++;
      }

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_and;
      filterDataCnt++;

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_value_range;
      filterData[filterDataCnt].field = 25; // sending flag
      filterData[filterDataCnt].condParam.range_uint32.min = 0;
      filterDataCnt++;
    }else{
      filterData[filterDataCnt].field = 5; // Type
      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_str_equal;
      snprintf(filterData[filterDataCnt].condParam.string_value.s,
             sizeof(filterData[filterDataCnt].condParam.string_value.s),
             "Insight-T");
      filterDataCnt++;

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_and;
      filterDataCnt++;

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_value_range;
      filterData[filterDataCnt].field = 25; // sending flag
      filterData[filterDataCnt].condParam.range_uint32.min = 0;
      filterDataCnt++;
    }

    filterData[filterDataCnt].condition = gw_pro_sqlite_cond_or;
    filterDataCnt++;

    if(disabledData.predictSensorCnt > 0)
    {
      filterData[filterDataCnt].field = 5; // Type
      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_str_equal;
      snprintf(filterData[filterDataCnt].condParam.string_value.s,
             sizeof(filterData[filterDataCnt].condParam.string_value.s),
             "PredictSensor");
      filterDataCnt++;
      
      for(uint16_t cnt = 0; cnt < disabledData.predictSensorCnt; cnt++)
      {
        filterData[filterDataCnt].condition = gw_pro_sqlite_cond_and;
        filterDataCnt++;

        filterData[filterDataCnt].field = 11; // DataType
        filterData[filterDataCnt].condition = gw_pro_sqlite_cond_value_not_equal;
        filterData[filterDataCnt].condParam.range_uint32.min = 
          disabledData.PredictSensor[cnt];
        filterDataCnt++;
      }

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_and;
      filterDataCnt++;

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_value_range;
      filterData[filterDataCnt].field = 25; // sending flag
      filterData[filterDataCnt].condParam.range_uint32.min = 0;
      filterDataCnt++;
    }else{
      filterData[filterDataCnt].field = 5; // Type
      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_str_equal;
      snprintf(filterData[filterDataCnt].condParam.string_value.s,
             sizeof(filterData[filterDataCnt].condParam.string_value.s),
             "PredictSensor");
      filterDataCnt++;

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_and;
      filterDataCnt++;

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_value_range;
      filterData[filterDataCnt].field = 25; // sending flag
      filterData[filterDataCnt].condParam.range_uint32.min = 0;
      filterDataCnt++;
    }

    filterData[filterDataCnt].condition = gw_pro_sqlite_cond_or;
    filterDataCnt++;

    if(disabledData.predictSensorProCnt > 0)
    {
      filterData[filterDataCnt].field = 5; // Type
      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_str_equal;
      snprintf(filterData[filterDataCnt].condParam.string_value.s,
             sizeof(filterData[filterDataCnt].condParam.string_value.s),
             "PredictSensorPro");
      filterDataCnt++;
      
      for(uint16_t cnt = 0; cnt < disabledData.predictSensorProCnt; cnt++)
      {
        filterData[filterDataCnt].condition = gw_pro_sqlite_cond_and;
        filterDataCnt++;

        filterData[filterDataCnt].field = 11; // DataType
        filterData[filterDataCnt].condition = gw_pro_sqlite_cond_value_not_equal;
        filterData[filterDataCnt].condParam.range_uint32.min = 
          disabledData.PredictSensorPro[cnt];
        filterDataCnt++;
      }

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_and;
      filterDataCnt++;

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_value_range;
      filterData[filterDataCnt].field = 25; // sending flag
      filterData[filterDataCnt].condParam.range_uint32.min = 0;
      filterDataCnt++;
    }else{
      filterData[filterDataCnt].field = 5; // Type
      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_str_equal;
      snprintf(filterData[filterDataCnt].condParam.string_value.s,
             sizeof(filterData[filterDataCnt].condParam.string_value.s),
             "PredictSensorPro");
      filterDataCnt++;

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_and;
      filterDataCnt++;

      filterData[filterDataCnt].condition = gw_pro_sqlite_cond_value_range;
      filterData[filterDataCnt].field = 25; // sending flag
      filterData[filterDataCnt].condParam.range_uint32.min = 0;
      filterDataCnt++;
    }

    // TODO: More products can be added here if necessary

    if (ipc_sql_filter_item(SENSOR_DATA_TABLE, filterDataCnt, filterData, &msgbufp) ||
        pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
        pdata->command_or_response.responseMsg.response ==
            gw_pro_response_failed) {
      LOG_WARN(OUTPOINT, "Failed to get datas from sensordata table\r\n");
      goto next_loop;
    }
    memset(&filter_rx, 0, sizeof(filter_rx));
    memcpy(&filter_rx,
           &pdata->command_or_response.responseMsg.responseInfo
                .sqlite_filter_item_reponse,
           sizeof(gw_pro_command_sqlite_filter_response_t));
    uint32_t dataNumLimit = CONFIG_UPLOAD_DATA_ITEM_LIMIT;
    if(filter_rx.dataNum < dataNumLimit) {
      dataNumLimit = filter_rx.dataNum;
      time_sec += CONFIG_UPLOAD_TIMER_STEP;
      time_sec = (time_sec > CONFIG_UPLOAD_TIMER_LIMIT) ? CONFIG_UPLOAD_TIMER_LIMIT : time_sec;
      continue_flag = false;
    } else {
      time_sec -= CONFIG_UPLOAD_TIMER_STEP;
      time_sec = (time_sec < CONFIG_UPLOAD_TIMER_INTERVAL) ? CONFIG_UPLOAD_TIMER_INTERVAL : time_sec;
      continue_flag = true;
    }
    for (uint32_t i = 0; i < dataNumLimit; i++) {
      valid_data = false;
      if (ipc_sql_query_item(SENSOR_DATA_TABLE, filter_rx.filteredItemIdx[i],
                             &msgbufp) ||
          pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
          pdata->command_or_response.responseMsg.response ==
              gw_pro_response_failed) {
        LOG_WARN(OUTPOINT, "Failed to get datas from sensordata table\r\n");
        continue;
      }
      for (uint8_t j = 0;
           j < pdata->command_or_response.responseMsg.responseInfo
                   .sqlite_update_item_response.dataNum;
           j++) {
        if (pdata->command_or_response.responseMsg.responseInfo
                    .sqlite_update_item_response.inquiredData[j]
                    .field == 1 &&
            pdata->command_or_response.responseMsg.responseInfo
                    .sqlite_update_item_response.inquiredData[j]
                    .data.data_uint64 == filter_rx.filteredItemIdx[i]) {
          valid_data = true;
          break;
        }
      }
      if (!valid_data)
        continue;
      LOG_INFO(OUTPOINT, "from sensordata table\r\n");
      uint8_t tmp = 30; // wait for 30S at most. to be confirmed
      while(reporting_data_flag && (tmp > 0)) {
        tmp--;
        LOG_INFO(OUTPOINT, "reporting data ongoing! sleep 1S\r\n");
        sleep(1);
      }
      report_datachange_cloud(
          pdata->command_or_response.responseMsg.responseInfo
              .sqlite_update_item_response.inquiredData,
          pdata->command_or_response.responseMsg.responseInfo
              .sqlite_update_item_response.dataNum);
    }
  next_loop:
    if(!continue_flag)
      sleep(time_sec);
  }
}

#else
void sig_usr() {
  {
    gain_tablename(report_rx.tbname, table_name, sizeof(table_name) - 1);
    if (strlen(mqtt_addr) <= 8)
      return;
    memset(&msgbufp, 0, sizeof(msgbufp));
    if (ipc_sql_query_item(table_name, report_rx.Idx, &msgbufp) ||
        report_pdata->command_or_response.responseMsg.command !=
            gw_pro_command_sqlite_query_item ||
        report_pdata->command_or_response.responseMsg.response !=
            gw_pro_response_success) {
      LOG_WARN(OUTPOINT, "Faild to query excepted data!\r\n");
      return;
    }
    if (report_pdata->command_or_response.responseMsg.responseInfo
            .sqlite_update_item_response.dataNum == 0) {
      LOG_INFO(OUTPOINT, "Success to delete data!\r\n");
      return;
    }
    switch (report_rx.tbname) {
    case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:
    case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE: {
      if (!strlen(up_url)) {
        LOG_WARN(OUTPOINT, "Invalid url!\r\n");
        return;
      }
      LOG_INFO(OUTPOINT, "send to upload file!\r\n");
#if 1
      gw_config_build(report_pdata, gwjson, sizeof(gwjson) - 1);
      if (0 == strlen(gwjson)) {
        LOG_WARN(OUTPOINT, "Invalid json data\r\n");
        return;
      }
      snprintf(newjsonfile, sizeof(newjsonfile) - 1, "%s_%s", topic_clientID,
               GATEWAY_CONFIG_FILE_NAME);
      gwfp = fopen(newjsonfile, "w");
      if (gwfp == NULL) {
        LOG_WARN(OUTPOINT, "Failed to open sensorconfig.json\r\n");
        return;
      }
      fputs(gwjson, gwfp);
      fclose(gwfp);
#endif
#ifdef CONFIG_HTTP_UPLOAD_FILE
      snprintf(upfilename, sizeof(upfilename) - 1, "%s",
               "\"file=@gwConfig.json\"");
#else
      snprintf(upfilename, sizeof(upfilename) - 1, "\"file=@%s_gwConfig.json\"",
               topic_clientID);
#endif
      enable_http_task(MAX_UPLOAD_ARGS);
      sleep(1);
    } break;
    case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE: {
      if (!strlen(up_url)) {
        LOG_WARN(OUTPOINT, "Invalid url!\r\n");
        return;
      }
      LOG_INFO(OUTPOINT, "send to upload file!\r\n");
#if 1
      if (report_pdata->command_or_response.responseMsg.responseInfo
              .sqlite_update_item_response.dataNum == 0) {
        snprintf(sensorjson, sizeof(sensorjson) - 1,
                 DEFAULT_CONFIG_FILE_CONTENT, topic_clientID);
      } else {
        sensor_config_build(
            report_pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response.inquiredData,
            report_pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response.dataNum,
            sensorjson, sizeof(sensorjson) - 1);
      }
      snprintf(newjsonfile, sizeof(newjsonfile) - 1, "%s_%s", topic_clientID,
               SENSOR_CONFIG_FILE_NAME);
      sensorfp = fopen(newjsonfile, "w");
      if (sensorfp == NULL) {
        LOG_WARN(OUTPOINT, "Error:sensor_config\r\n");
        return;
      }
      fseek(sensorfp, 0, SEEK_SET);
      fwrite(sensorjson, strlen(sensorjson), 1, sensorfp);
      fclose(sensorfp);
#endif
#ifdef CONFIG_HTTP_UPLOAD_FILE
      snprintf(upfilename, sizeof(upfilename) - 1, "%s",
               : "\"file=@sensorConfig.json\"");
#else
      snprintf(upfilename, sizeof(upfilename) - 1,
               "\"file=@%s_sensorConfig.json\"", topic_clientID);
#endif
      enable_http_task(MAX_UPLOAD_ARGS);
      sleep(1);
    } break;
    case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE: {
      LOG_INFO(OUTPOINT, "from data table\r\n");
      report_datachange_cloud(
          report_pdata->command_or_response.responseMsg.responseInfo
              .sqlite_update_item_response.inquiredData,
          report_pdata->command_or_response.responseMsg.responseInfo
              .sqlite_update_item_response.dataNum);

    } break;
    }
  }
}
void *wait_send(void *arg) {
  sig_usr();
  return NULL;
}
void *wait_from_sqlite(void *arg) {
  struct msgbuf msgbufprx = {0};
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)msgbufprx.mtext;
  LOG_INFO(OUTPOINT, "start wait thread!\r\n");
  while (1) {
    LOG_INFO(OUTPOINT, "wait sql data\r\n");
    if (recv_msg(mqtt_msgid, &msgbufprx, gw_pro_command) ||
        pdata->command_or_response.commandMsg.command !=
            gw_pro_command_report_new_modification)
      continue;
    report_rx.tbname = pdata->command_or_response.commandMsg.param
                           .report_new_modification_param.table;
    report_rx.Idx = pdata->command_or_response.commandMsg.param
                        .report_new_modification_param.itemIdx;
    LOG_DEBUG(OUTPOINT, "report:%d,%d\r\n", report_rx.Idx, report_rx.tbname);
    if (!mqtt_isConnected)
      goto nextloop;
    if (!MQTTClient_isConnected(client)) {
      mqtt_isConnected = false;
      goto nextloop;
    }
    int res = pthread_create(&tid, NULL, wait_send, NULL);
    assert(res == 0);
    usleep(200000);
    continue;
  nextloop:
    LOG_WARN(OUTPOINT, "The network is not conected,Failed to send msg!\r\n");
    continue;
  }
  LOG_ERR(OUTPOINT, "This is process should not run here!\r\n");
}
#endif

void report_datachange_cloud(gw_pro_data_t *data, uint32_t dataNO) {
  uint8_t upload_type = 0;
  uint8_t valtype = 0;
  uint64_t SequenceNumber = 0;
  uint64_t nameId = 0;
  char local_topic[200] = {0};
  char local_devid[32] = {0};
  bool is_sensorT = false;
  SKFChina_App_AppMessage SKF_BullGateway_tx = {0};
  sem_wait(&updatasem);
  memset(middle_measure, 0, sizeof(middle_measure));
  memset(middle_data, 0, sizeof(middle_data));
  memset(measure_type, 0, sizeof(measure_type));
  for (uint32_t i = 0; i < dataNO; i++) {
    switch (data[i].field) {
    case 1: {
      SequenceNumber = data[i].data.data_uint64;
    } break;
    case 2: {
      nameId = data[i].data.data_uint64;
    } break;
    case 4: {
      snprintf(local_topic, sizeof(local_topic) - 1, "%s",
               data[i].data.data_char);
      snprintf(local_devid, sizeof(local_devid) - 1, "%s",
               data[i].data.data_char);
    } break;
    case 5: {
      middle_measure[0].has_sensor = true;
      middle_measure[0].sensor = data[i].data.data_uint8;
    } break;
    case 7: {
      middle_measure[0].measure_seq_no = data[i].data.data_uint32;
    } break;
    case 8: {
      middle_measure[0].sample_time = data[i].data.data_uint64;
    } break;
    case 10: {
      middle_measure[0].has_alarm_or_notify = true;
      middle_measure[0].alarm_or_notify.measurement = data[i].data.data_uint8;
    } break;
    case 11: {
      upload_type = 1;
      if (data[i].data.data_uint8 >
          SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE) {
        LOG_WARN(OUTPOINT, "Invalid item\r\n");
        break;
      }
      measure_type[0] = data[i].data.data_uint8;
      if (measure_type[0] ==
              SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE ||
          measure_type[0] ==
              SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE ||
          measure_type[0] ==
              SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE ||
          measure_type[0] ==
              SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE) {
        upload_type = 2;
        upwave_type = measure_type[0];
      }
    } break;
    case 12: {
      if (!strcmp(data[i].data.data_char, "Digit"))
        valtype = 1;
    } break;
    case 13: {
      middle_measure[0].has_range = true;
      middle_measure[0].range = data[i].data.data_uint8;
    } break;
    case 14: {
      middle_measure[0].has_unit = true;
      middle_measure[0].unit = data[i].data.data_uint8;
    } break;
    case 15: {
      middle_measure[0].has_measure_length_sample = true;
      middle_measure[0].measure_length_sample = data[i].data.data_uint32;
    } break;
    case 16: {
      middle_measure[0].has_total_data_length_sample = true;
      middle_measure[0].total_data_length_sample = data[i].data.data_uint32;
    } break;
    case 17: {
      middle_measure[0].has_dimension = true;
      middle_measure[0].dimension = data[i].data.data_uint32;
    } break;
    case 18: {
      middle_measure[0].data_format = data[i].data.data_uint8;
    } break;
    case 19: {
      middle_measure[0].has_sample_period_s = true;
      middle_measure[0].sample_period_s = data[i].data.data_uint32;
    } break;
    case 20: {
      middle_measure[0].has_encryp = true;
      middle_measure[0].encryp = data[i].data.data_uint8;
    } break;
    case 21: {
      memcpy(&middle_data[0].data, &data[i].data, sizeof(data[i].data));
      if (valtype) {
        LOG_DEBUG(OUTPOINT, "middle_data:%f\r\n",
                  middle_data[0].data.data_float);
      } else
        LOG_DEBUG(OUTPOINT, "middle_data:%s\r\n",
                  middle_data[0].data.data_char);
    } break;
    case 22: {
      middle_measure[0].has_sample_rate_hz = true;
      middle_measure[0].sample_rate_hz = data[i].data.data_float;
    } break;
    case 23: {
      middle_measure[0].product = data[i].data.data_uint8;
      if(data[i].data.data_uint8 == SKFChina_Common_ProductType_INSIGHT_T)
      {
        is_sensorT = true;
      }
    } break;
    case 24: {
      middle_measure[0].has_sensor = true;
      middle_measure[0].sensor = data[i].data.data_uint8;
    } break;
    }
  }
  if (upload_type != 1 && upload_type != 2) {
    LOG_INFO(OUTPOINT, "no uploading data\r\n");
  } else {
    char tmpstr[100] = {0};
    snprintf(tmpstr, sizeof(tmpstr) - 1, "%s/%s", clientID, local_topic);
    memset(local_topic, 0, sizeof(local_topic));
    if(is_sensorT)
    {
      // For Insight-T
      COINFIG_MQTT_PUBLISH_DATA_TOPIC(local_topic, SENSOR_T_NAME, tmpstr);
    }else{
      // For PredictSensor, PredictSensorPro, ...
      COINFIG_MQTT_PUBLISH_DATA_TOPIC(local_topic, SENSOR_NAME, tmpstr);
    }
    memset((void *)&SKF_BullGateway_tx, 0, sizeof(SKF_BullGateway_tx));
    LOG_DEBUG(OUTPOINT, "uplodingID:%ld,%ld\r\n", SequenceNumber, nameId);
    if (upload_type == 1) {
      Simple_upload(SKFChina_App_AppMessage_data_upload_tag, SKF_BullGateway_tx,
                    local_topic, local_devid);
      if (datupload_flag) {
        LOG_DEBUG(OUTPOINT, "writeback flag!\r\n");
        update_data[0].field = 25;
        update_data[0].data.data_uint8 = 1;
        update_data[1].field = 1;
        update_data[1].data.data_uint64 = SequenceNumber;
        if (ipc_sql_update_item(SENSOR_DATA_TABLE, nameId, 2, update_data,
                                NULL)) {
          LOG_WARN(OUTPOINT, "Failed to update %ld into sensordata table\r\n",
                   SequenceNumber);
        }
        datupload_flag = false;
      }
    } else {
      Simple_bulk_upload(SKFChina_App_AppMessage_data_upload_tag,
                         SKF_BullGateway_tx, local_topic, local_devid);
      if (waveupload_flag) {
        update_data[0].field = 25;
        update_data[0].data.data_uint8 = 1;
        update_data[1].field = 1;
        update_data[1].data.data_uint64 = SequenceNumber;
        LOG_DEBUG(OUTPOINT, "writeback wave flag!\r\n");
        if (ipc_sql_update_item(SENSOR_DATA_TABLE, nameId, 2, update_data,
                                NULL)) {
          LOG_WARN(OUTPOINT, "Failed to update %ld into sensordata table\r\n",
                   SequenceNumber);
        }
        waveupload_flag = false;
      }
    }
  }
  sem_post(&updatasem);
}
