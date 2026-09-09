#include "jsonparse.h"
#include "cJSON.h"
#include "global.h"
// #include "query.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
// #include <insert.h>
// #include <query.h>
#include "ipcmbs.h"
#include "jsonbuild.h"
#include "ppGW.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#if (ENABLE_MODBUS_FEATURE == 1)
#include "gwConfig.h"
#endif
DBG_LOCAL_LOG_DEBUG

struct disabledGroup disabledData, disabledConfig;

static uint8_t dataNO = 0;
static uint32_t keyID = 0;
static char *jsonstr = NULL;
static gw_pro_data_t update_data[GW_PRO_MAX_DATA_FIELD_PER_OPERATION] = {0};
// for children
static uint8_t child_dataNO = 0;
static uint32_t child_keyID = 0;
static char parentstr[100] = {0};
static gw_pro_data_t child_update_data[GW_PRO_MAX_DATA_FIELD_PER_OPERATION] = {
    0};
static bool tryto_update_child = false;
#if (ENABLE_MODBUS_FEATURE == 1)
static dev_fetch_flag = false;
static uint8_t dev_cnt_former = 0;
static uint8_t dev_cnt_current = 0;
uint32_t g_dev_nameid_list[GW_MAX_CHILDREN_NUM] = {0};
uint8_t g_dev_cnt = 0;
static uint32_t dev_nameid_list_former[GW_MAX_CHILDREN_NUM] = {0};
static uint32_t dev_nameid_list_current[GW_MAX_CHILDREN_NUM] = {0};
// static gw_pro_sqlite_cond_element_t gp_filterData[GW_PRO_MAX_DATA_FIELD_PER_OPERATION] = {0};
// static gw_pro_command_sqlite_filter_response_t gp_filter_rx = {0};
// static struct msgbuf gp_msgbufp = {0};
// static gw_pro_message_header_t *pdata =
//     (gw_pro_message_header_t *)gp_msgbufp.mtext;
#endif

static void parse_sensoritem(cJSON *item);
static void parse_gwitem(cJSON *item);
void parse_config(const char *filename) {
  dataNO = 0;
  keyID = 0;
  memset(update_data, 0, sizeof(update_data));
  memset(parentstr, 0, sizeof(parentstr));
  config_file_parse(filename, &jsonstr);
  // update_data[dataNO].field = 8;
  // update_data[dataNO++].data.data_uint64 = time(NULL); // time(NULL)
  if (NULL != strstr(filename, GATEWAY_CONFIG_FILE_NAME)) {
    memset(child_update_data, 0, sizeof(child_update_data));
    // for children
    child_dataNO = 0;
    child_keyID = 0;
#if (ENABLE_MODBUS_FEATURE != 1)
    tryto_update_child = false;
#else
#if (0)
    gw_pro_command_sqlite_filter_response_t gp_filter_rx = {0};
    struct msgbuf gp_msgbufp = {0};
    gw_pro_message_header_t *pdata =
    (gw_pro_message_header_t *)gp_msgbufp.mtext;
#endif
    LOG_INFO(OUTPOINT, "[shm gw_parse_conf] before 1\r\n");
    if(!dev_fetch_flag) { // fetch the nameIDs in Dev table first if it's restarted.
      dev_cnt_former = 0;
#if (1)
      dev_cnt_former = g_dev_cnt;
      memset(&dev_nameid_list_former, 0, sizeof(dev_nameid_list_former));
      for (uint16_t i = 0; i < g_dev_cnt; i++) {
        dev_nameid_list_former[i] = g_dev_nameid_list[i];
      }
#else
      gp_filterData[0].condition = gw_pro_sqlite_cond_all;
      gp_filterData[0].field = 1;
      sleep(1);
      memset(&gp_msgbufp, 0, sizeof(gp_msgbufp));
      if (ipc_sql_filter_item(DEVICE_LIST_TABLE, 1, gp_filterData, &gp_msgbufp) ||
      // set dataNO to 0 to use fixed filter to fetch nameIDs
      // if (ipc_sql_filter_item(DEVICE_LIST_TABLE, 0, gp_filterData, &gp_msgbufp) ||
          pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
          pdata->command_or_response.responseMsg.response ==
              gw_pro_response_failed) {
        LOG_WARN(OUTPOINT, "Failed to get nameids from Dev table\r\n");
        return;
      }
      // memset(&gp_filter_rx, 0, sizeof(gp_filter_rx));
      // memcpy(&gp_filter_rx,
      //        &pdata->command_or_response.responseMsg.responseInfo
      //             .sqlite_filter_item_reponse,
      //        sizeof(gw_pro_command_sqlite_filter_response_t));
      for (uint32_t i = 0; i < pdata->command_or_response.responseMsg.responseInfo.sqlite_filter_item_reponse.dataNum;i++) {
            if(0 != pdata->command_or_response.responseMsg.responseInfo.sqlite_filter_item_reponse.filteredItemIdx[i]) {
              dev_nameid_list_former[i] = pdata->command_or_response.responseMsg.responseInfo.sqlite_filter_item_reponse.filteredItemIdx[i];
              dev_cnt_former++;
            }
          }
      sleep(1);
#endif
      dev_cnt_current = 0;
      dev_fetch_flag = true;
#if (1)
      LOG_DEBUG(OUTPOINT, "init Dev cnt:%d, list:\r\n", dev_cnt_former);
      for(uint8_t i=0;i<dev_cnt_former;i++) {
        printf("%d ", dev_nameid_list_former[i]);
      }
      printf("\r\n");
#endif
    } else {
      // clear count, dev_list of 'former' case & copy 'current' into 'former' here
      dev_cnt_former = dev_cnt_current;
      memset(dev_nameid_list_former, 0, sizeof(dev_nameid_list_former));
      memcpy(dev_nameid_list_former, dev_nameid_list_current, sizeof(dev_nameid_list_former));
      memset(dev_nameid_list_current, 0, sizeof(dev_nameid_list_current));
      dev_cnt_current = 0;
#if (1) // debug
      LOG_DEBUG(OUTPOINT, "former Dev cnt:%d, list:\r\n", dev_cnt_former);
      for(uint8_t i=0;i<dev_cnt_former;i++) {
        printf("%d ", dev_nameid_list_former[i]);
      }
      printf("\r\n");
#endif
    }
    LOG_INFO(OUTPOINT, "[shm gw_parse_conf] before 2\r\n");
#endif
    gwconfig_parse(jsonstr);
#if (ENABLE_MODBUS_FEATURE == 1)
    LOG_INFO(OUTPOINT, "[shm gw_parse_conf] after 1\r\n");
    bool tmp_flag = false;
    for(uint8_t i=0;i<dev_cnt_former;i++) {
      tmp_flag = false;
      for(uint8_t j=0;j<dev_cnt_current;j++) {
        if((0 != dev_nameid_list_former[i]) && (dev_nameid_list_former[i] == dev_nameid_list_current[j])) {
          tmp_flag = true;
          break;
        }
      }
      if((!tmp_flag) && (0 != dev_nameid_list_former[i])) { //nameID not in current list, delete it here
        if (ipc_sql_delete_item(DEVICE_LIST_TABLE, dev_nameid_list_former[i], NULL)) {
          LOG_WARN(OUTPOINT, "Failed to delete %d from Dev table\r\n", dev_nameid_list_former[i]);
        }
        LOG_INFO(OUTPOINT, "delete %d from Dev table!\r\n", dev_nameid_list_former[i]);
        dev_nameid_list_former[i] = 0;
      }
    }
    LOG_INFO(OUTPOINT, "[shm gw_parse_conf] after 2\r\n");
#endif
    if (ipc_sql_update_item(GATEWAY_CONFIGURE_TABLE, keyID, dataNO, update_data,
                            NULL)) {
      LOG_WARN(OUTPOINT, "Failed to update %d into Gateway table\r\n", keyID);
    }
    LOG_INFO(OUTPOINT, "update %d into Gateway table with %d\r\n", keyID,
             dataNO);
  } else if (NULL != strstr(filename, SENSOR_CONFIG_FILE_NAME)) {
    usleep(20000);
    sensorconfig_parse(jsonstr);
    LOG_INFO(OUTPOINT, "update %d into sensor table with %d\r\n", keyID,
             dataNO);
    if (ipc_sql_update_item(SENSOR_CONFIGURE_TABLE, keyID, dataNO, update_data,
                            NULL)) {
      LOG_WARN(OUTPOINT, "Failed to update %d into sensor table\r\n", keyID);
    }
  } else if (NULL != strstr(filename, MAGIC_FILE_NAME)) {
    LOG_INFO(OUTPOINT, "%s has been found\r\n", MAGIC_FILE_NAME);
    memset(&disabledData, 0, sizeof(disabledData));
    memset(&disabledConfig, 0, sizeof(disabledConfig));
    magicfile_parse(jsonstr);
    LOG_INFO(OUTPOINT, "disabledData.GW (%u)\r\n", disabledData.gwCnt);
    for(uint16_t i = 0; i < disabledData.gwCnt; i++)
    {
      LOG_INFO(OUTPOINT, "%u\r\n", disabledData.GW[i]);
    }
    LOG_INFO(OUTPOINT, "disabledData.PredictSensor (%u)\r\n", disabledData.predictSensorCnt);
    for(uint16_t i = 0; i < disabledData.predictSensorCnt; i++)
    {
      LOG_INFO(OUTPOINT, "%u\r\n", disabledData.PredictSensor[i]);
    }
    LOG_INFO(OUTPOINT, "disabledData.InsightT (%u)\r\n", disabledData.insightTCnt);
    for(uint16_t i = 0; i < disabledData.insightTCnt; i++)
    {
      LOG_INFO(OUTPOINT, "%u\r\n", disabledData.InsightT[i]);
    }
    LOG_INFO(OUTPOINT, "disabledData.PredictSensorPro (%u)\r\n", disabledData.predictSensorProCnt);
    for(uint16_t i = 0; i < disabledData.predictSensorProCnt; i++)
    {
      LOG_INFO(OUTPOINT, "%u\r\n", disabledData.PredictSensorPro[i]);
    }
    LOG_INFO(OUTPOINT, "disabledConfig.GW (%u)\r\n", disabledConfig.gwCnt);
    for(uint16_t i = 0; i < disabledConfig.gwCnt; i++)
    {
      LOG_INFO(OUTPOINT, "%u\r\n", disabledConfig.GW[i]);
    }
    LOG_INFO(OUTPOINT, "disabledConfig.PredictSensor (%u)\r\n", disabledConfig.predictSensorCnt);
    for(uint16_t i = 0; i < disabledConfig.predictSensorCnt; i++)
    {
      LOG_INFO(OUTPOINT, "%u\r\n", disabledConfig.PredictSensor[i]);
    }
    LOG_INFO(OUTPOINT, "disabledConfig.InsightT (%u)\r\n", disabledConfig.insightTCnt);
    for(uint16_t i = 0; i < disabledConfig.insightTCnt; i++)
    {
      LOG_INFO(OUTPOINT, "%u\r\n", disabledConfig.InsightT[i]);
    }
    LOG_INFO(OUTPOINT, "disabledConfig.PredictSensorPro (%u)\r\n", disabledConfig.predictSensorProCnt);
    for(uint16_t i = 0; i < disabledConfig.predictSensorProCnt; i++)
    {
      LOG_INFO(OUTPOINT, "%u\r\n", disabledConfig.PredictSensorPro[i]);
    }
  } else {
    LOG_WARN(OUTPOINT, "Invalid file:%s\r\n", filename);
  }
  free(jsonstr);
  jsonstr = NULL;
}

void gwconfig_parse(char *input_json) {
  cJSON *_root = NULL;
  if (input_json == NULL) {
    LOG_WARN(OUTPOINT, "Invalid json string\r\n");
    return;
  }
  _root = cJSON_Parse(input_json);
  if (_root == NULL) {
    // the json string seems invalid
    LOG_WARN(OUTPOINT, "Failed to  parse json\r\n");
    return;
  }
  parse_gwitem(_root);
  cJSON_Delete(_root);
}

void sensorconfig_parse(char *input_json) {
  cJSON *_root = NULL;
  if (input_json == NULL) {
    LOG_WARN(OUTPOINT, "Invalid json string\r\n");
    return;
  }
  // LOG_INFO(OUTPOINT, "file:%s\r\n",input_json);
  _root = cJSON_Parse(input_json);
  if (_root == NULL) {
    // the json string seems invalid
    LOG_WARN(OUTPOINT, "Failed to  parse json\r\n");
    return;
  }
  parse_sensoritem(_root);
  cJSON_Delete(_root);
}

void parse_sensoritem(cJSON *item) {
  cJSON *tmp = NULL;
  int rootsize = cJSON_GetArraySize(item);
  for (int i = 0; i < rootsize; i++) {
    if (NULL == (tmp = cJSON_GetArrayItem(item, i)))
      continue;
    switch (tmp->type) {
    case cJSON_False:
    case cJSON_True: {
      if (0 == strcmp(tmp->string, "haveConfiguredToSensor")) {
        update_data[dataNO].field = 9;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "VIB_START_FG")) {
        update_data[dataNO].field = 44;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "MAG_START_FG")) {
        update_data[dataNO].field = 46;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "MAG_STABLE_FG")) {
        update_data[dataNO].field = 48;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FUNC_ANOM_TEMP")) {
        update_data[dataNO].field = 58;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FUNC_ANOM_OV")) {
        update_data[dataNO].field = 59;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FUNC_ANOM_MECH")) {
        update_data[dataNO].field = 60;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FUNC_ANOM_BRG")) {
        update_data[dataNO].field = 61;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FUNC_ANOM_LUB")) {
        update_data[dataNO].field = 62;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FUNC_ANOM_MTR")) {
        update_data[dataNO].field = 63;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FUNC_ANOM_GEAR")) {
        update_data[dataNO].field = 64;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FUNC_ANOM_FAN")) {
        update_data[dataNO].field = 65;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FUNC_ANOM_PUMP")) {
        update_data[dataNO].field = 66;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      }
      // LOG_INFO(OUTPOINT, "%s,%d\r\n", tmp->string, tmp->valueint);
      // LOG_DEBUG(OUTPOINT, "field:%d,%s\r\n", update_data[dataNO -
      // 1].field,tmp->string);
    } break;
    case cJSON_Number: {
      // LOG_INFO(OUTPOINT, "%s,%f\r\n", tmp->string, tmp->valuedouble);
      if (0 == strcmp(tmp->string, "version")) {
        update_data[dataNO].field = 7;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (!strcmp(tmp->string, "sensorMode")) {
        update_data[dataNO].field = 12;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
        // snprintf(update_data[dataNO++].data.data_char,
        //          GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
      } else if (0 == strcmp(tmp->string, "lastEditTime")) {
        update_data[dataNO].field = 8;
        // update_data[dataNO++].data.data_uint64 = time(NULL);
        update_data[dataNO++].data.data_uint64 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "whenConfiguredToSensor")) {
        update_data[dataNO].field = 10;
        update_data[dataNO++].data.data_uint64 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "nameId")) {
        keyID = tmp->valueint;
        update_data[dataNO].field = 1;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "batteryAlarmThreshold")) {
        update_data[dataNO].field = 13;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "txPower_adv")) {
        update_data[dataNO].field = 14;
        update_data[dataNO++].data.data_int8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "dataRate_auxAdv")) {
        update_data[dataNO].field = 15;
        update_data[dataNO++].data.data_uint16 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "txPower_auxAdv")) {
        update_data[dataNO].field = 16;
        update_data[dataNO++].data.data_int8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "period_quickPolling")) {
        update_data[dataNO].field = 17;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "referenceTime_quickPolling")) {
        // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
			  // but locally use a uint32_t to store
        update_data[dataNO].field = 18;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "period_regularSensing")) {
        update_data[dataNO].field = 19;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "referenceTime_regularSensing")) {
        // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
			  // but locally use a uint32_t to store
        update_data[dataNO].field = 20;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "period_comm")) {
        update_data[dataNO].field = 21;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "referenceTime_period_comm")) {
        // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
			  // but locally use a uint32_t to store
        update_data[dataNO].field = 22;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "PRE_ACQ_VIB_FS_HZ")) {
        update_data[dataNO].field = 23;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "PRE_ACQ_VIB_N")) {
        update_data[dataNO].field = 24;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "PRE_ACQ_VIB_AXIS_ACQ_EVAL")) {
        update_data[dataNO].field = 25;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "PRE_ACQ_VIB_RANGE")) {
        update_data[dataNO].field = 26;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "PRE_ACQ_MAG_FS_HZ")) {
        update_data[dataNO].field = 27;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "PRE_ACQ_MAG_N")) {
        update_data[dataNO].field = 28;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "PRE_ACQ_MAG_AXIS_ACQ_EVAL")) {
        update_data[dataNO].field = 29;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "Facc")) {
        update_data[dataNO].field = 30;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "Nacc")) {
        update_data[dataNO].field = 31;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "ACQ_VIB_AXIS_ACQ_EVAL")) {
        update_data[dataNO].field = 32;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "ACQ_VIB_RANGE")) {
        update_data[dataNO].field = 33;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "ACQ_MAG_FS_HZ")) {
        update_data[dataNO].field = 34;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "ACQ_MAG_N")) {
        update_data[dataNO].field = 35;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "ACQ_MAG_AXIS_ACQ_EVAL")) {
        update_data[dataNO].field = 36;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FS_COEF")) {
        update_data[dataNO].field = 37;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "GEE_COEF")) {
        update_data[dataNO].field = 38;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "V_COEF")) {
        update_data[dataNO].field = 39;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "MEAS_POSITION")) {
        update_data[dataNO].field = 40;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "MEAS_LOAD")) {
        update_data[dataNO].field = 41;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "MEAS_AXIS_VIB")) {
        update_data[dataNO].field = 42;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "MEAS_AXIS_MAG")) {
        update_data[dataNO].field = 43;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "VIB_RMS_START_TL")) {
        update_data[dataNO].field = 45;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "MAG_RMS_START_TL")) {
        update_data[dataNO].field = 47;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "MAG_RMS_VAR_TH")) {
        update_data[dataNO].field = 49;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "RPM_VAR_RANGE")) {
        update_data[dataNO].field = 50;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "DEC_LOGIC_TEMP_M")) {
        update_data[dataNO].field = 51;
        update_data[dataNO++].data.data_uint16 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "DEC_LOGIC_TEMP_N")) {
        update_data[dataNO].field = 52;
        update_data[dataNO++].data.data_uint16 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "DEC_LOGIC_LEARN_NUM_TEMP")) {
        update_data[dataNO].field = 53;
        update_data[dataNO++].data.data_uint16 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "DEC_LOGIC_VIB_M")) {
        update_data[dataNO].field = 54;
        update_data[dataNO++].data.data_uint16 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "DEC_LOGIC_VIB_N")) {
        update_data[dataNO].field = 55;
        update_data[dataNO++].data.data_uint16 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "DEC_LOGIC_LEARN_NUM_VIB")) {
        update_data[dataNO].field = 56;
        update_data[dataNO++].data.data_uint16 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "DEC_LOGIC_LEARN_NUM_MAG")) {
        update_data[dataNO].field = 57;
        update_data[dataNO++].data.data_uint16 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "ASSET_LEVEL")) {
        update_data[dataNO].field = 67;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FLEX_TYPE")) {
        update_data[dataNO].field = 68;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "BORE_DIAMETER_MM")) {
        update_data[dataNO].field = 69;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
        // LOG_DEBUG(OUTPOINT, "BORE_DIAMETER_MM,%f\r\n", update_data[dataNO-1].data.data_float);
      } else if (0 == strcmp(tmp->string, "RUN_SPEED_RPM")) {
        update_data[dataNO].field = 70;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "BRG_INFO_BPFO")) {
        update_data[dataNO].field = 71;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "BRG_INFO_BPFI")) {
        update_data[dataNO].field = 72;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "BRG_INFO_BSF")) {
        update_data[dataNO].field = 73;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "BRG_INFO_FTF")) {
        update_data[dataNO].field = 74;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "MTR_INFO_FL")) {
        update_data[dataNO].field = 75;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "MTR_INFO_BAR")) {
        update_data[dataNO].field = 76;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "GEAR_INFO_TOOTH")) {
        update_data[dataNO].field = 77;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FAN_INFO_BLADE")) {
        update_data[dataNO].field = 78;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "PUMP_INFO_VANE")) {
        update_data[dataNO].field = 79;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "TEMP_OV_ALERT_CDEGREE")) {
        update_data[dataNO].field = 80;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "AlarmThrestemp")) {
        update_data[dataNO].field = 81;
        update_data[dataNO++].data.data_int32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "ACC_OV_ALERT")) {
        update_data[dataNO].field = 82;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "ACC_OV_ALARM")) {
        update_data[dataNO].field = 83;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "ACC_HAL_ALERT")) {
        update_data[dataNO].field = 84;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "ACC_HAL_ALARM")) {
        update_data[dataNO].field = 85;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "VEL_OV_ALERT")) {
        update_data[dataNO].field = 86;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "VEL_OV_ALARM")) {
        update_data[dataNO].field = 87;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "VEL_HAL_ALERT")) {
        update_data[dataNO].field = 88;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "VEL_HAL_ALARM")) {
        update_data[dataNO].field = 89;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "ENV_OV_ALERT")) {
        update_data[dataNO].field = 90;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "ENV_OV_ALARM")) {
        update_data[dataNO].field = 91;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "ENV_HAL_ALERT")) {
        update_data[dataNO].field = 92;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "ENV_HAL_ALARM")) {
        update_data[dataNO].field = 93;
        update_data[dataNO++].data.data_float = tmp->valuedouble;
      } else if (0 == strcmp(tmp->string, "waveDataAcqPeriod")) {
        update_data[dataNO].field = 94;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "waveDataAcqReference")) {
        // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
			  // but locally use a uint32_t to store
        update_data[dataNO].field = 95;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "FUOTA_Version")) {
        update_data[dataNO].field = 96;
        update_data[dataNO++].data.data_uint32 = tmp->valueint;
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
      } else if (0 == strcmp(tmp->string, "sensorModeParameter")) {
        update_data[dataNO].field = 98;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
      }
      // LOG_DEBUG(OUTPOINT, "field:%d,%s\r\n", update_data[dataNO -
      // 1].field,tmp->string);
    } break;
    case cJSON_String: {
      // LOG_INFO(OUTPOINT, "%s,%s\r\n", tmp->string, tmp->valuestring);
      if (!strcmp(tmp->string, "type")) {
        update_data[dataNO].field = 11;
        snprintf(update_data[dataNO++].data.data_char,
                 GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
      } else if (!strcmp(tmp->string, "manufacturer")) {
        update_data[dataNO].field = 5;
        snprintf(update_data[dataNO++].data.data_char,
                 GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
      } else if (!strcmp(tmp->string, "macAddr")) {
        update_data[dataNO].field = 4;
        snprintf(update_data[dataNO++].data.data_char,
                 GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
      }
      //  else if (!strcmp(tmp->string, "sensorMode")) {
      //   update_data[dataNO].field = 12;
      //   snprintf(update_data[dataNO++].data.data_char,
      //            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
      // }
      else if (!strcmp(tmp->string, "FUOTA_Filename")) {
        update_data[dataNO].field = 97;
        snprintf(update_data[dataNO++].data.data_char,
                 GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
#if (ENABLE_MODBUS_FEATURE == 1)
      // } else if (!strcmp(tmp->string, "description")) {
      //   update_data[dataNO].field = 98;
      //   snprintf(update_data[dataNO++].data.data_char,
      //            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
#endif
      }
      // LOG_DEBUG(OUTPOINT, "field:%d,%s\r\n", update_data[dataNO -
      // 1].field,tmp->string);
    } break;
    case cJSON_Array:
    case cJSON_Object: {
      // if (tmp->string != NULL) {
      //   snprintf(parentstr, "%s", tmp->string);
      // }
      parse_sensoritem(tmp);
    } break;
    default:
      LOG_WARN(OUTPOINT, "Invalid type!\r\n");
      break;
    }
  }
}
void parse_gwitem(cJSON *item) {
  cJSON *tmp = NULL;
  int rootsize = cJSON_GetArraySize(item);
  for (int i = 0; i < rootsize; i++) {
    if (NULL == (tmp = cJSON_GetArrayItem(item, i)))
      continue;
    switch (tmp->type) {
    case cJSON_False:
    case cJSON_True: {
      if (0 == strcmp(tmp->string, "timeStamp")) {
        update_data[dataNO].field = 17;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      } else if (0 == strcmp(tmp->string, "DHCPEnable")) {
        update_data[dataNO].field = 23;
        update_data[dataNO++].data.data_uint8 = tmp->valueint;
      }
      // LOG_DEBUG(OUTPOINT, "field:%d,%s\r\n", update_data[dataNO -
      // 1].field,tmp->string);
    } break;
    case cJSON_Number: {
      if (0 == strcmp(parentstr, "children")) {
        if (0 == strcmp(tmp->string, "nameId")) {
          child_keyID = tmp->valueint;
          child_update_data[child_dataNO].field = 1;
          child_update_data[child_dataNO++].data.data_uint32 = tmp->valueint;
        }
      } else {
        if (0 == strcmp(tmp->string, "nameId")) {
          keyID = tmp->valueint;
          update_data[dataNO].field = 1;
          update_data[dataNO++].data.data_uint32 = tmp->valueint;
        }
        if (0 == strcmp(tmp->string, "childrenNumber")) {
          update_data[dataNO].field = 15;
          update_data[dataNO++].data.data_uint32 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "version")) {
          update_data[dataNO].field = 7;
          update_data[dataNO++].data.data_uint8 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "lastEditTime")) {
          update_data[dataNO].field = 8;
          update_data[dataNO].data.data_uint64 = tmp->valueint; // time(NULL)
          // A magic operation: increase the last edit time, then a natural uplink would happen ...
          // Notice: the magic operation only required for the gateway
          update_data[dataNO].data.data_uint64++;
          // LOG_DEBUG(OUTPOINT, "magic: %d\r\n", update_data[dataNO].data.data_uint64);
          dataNO++;
        } else if (0 == strcmp(tmp->string, "gatewayMode")) {

          update_data[dataNO].field = 10;
          update_data[dataNO++].data.data_uint8 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "BLE1TxPower")) {

          update_data[dataNO].field = 11;
          update_data[dataNO++].data.data_int8 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "BLE2TxPower")) {

          update_data[dataNO].field = 12;
          update_data[dataNO++].data.data_int8 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "BLE1Antenna")) {
          update_data[dataNO].field = 13;
          update_data[dataNO++].data.data_uint8 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "BLE2Antenna")) {

          update_data[dataNO].field = 14;
          update_data[dataNO++].data.data_uint8 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "BLEDuplicatedDrop")) {

          update_data[dataNO].field = 16;
          update_data[dataNO++].data.data_uint32 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "timeSetting")) {

          update_data[dataNO].field = 18;
          update_data[dataNO++].data.data_uint8 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "MQTTSecurityType")) {

          update_data[dataNO].field = 20;
          update_data[dataNO++].data.data_uint8 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "MQTTPort")) {

          update_data[dataNO].field = 22;
          update_data[dataNO++].data.data_uint32 = tmp->valueint;
#if (ENABLE_MODBUS_FEATURE == 1)
        } else if (0 == strcmp(tmp->string, "baudrateSlave")) {

          update_data[dataNO].field = 28;
          update_data[dataNO++].data.data_uint32 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "slaveAddrSlave")) {

          update_data[dataNO].field = 29;
          update_data[dataNO++].data.data_uint32 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "paritySlave")) {

          update_data[dataNO].field = 30;
          update_data[dataNO++].data.data_uint8 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "stopSlave")) {

          update_data[dataNO].field = 31;
          update_data[dataNO++].data.data_uint8 = tmp->valueint;
        } else if (0 == strcmp(tmp->string, "registerMode")) {

          update_data[dataNO].field = 32;
          update_data[dataNO++].data.data_uint8 = tmp->valueint;
#endif
        }
      }
      // LOG_DEBUG(OUTPOINT, "field:%d,%s\r\n", update_data[dataNO -
      // 1].field,tmp->string);
    } break;
    case cJSON_String: {
      if (0 == strcmp(parentstr, "children")) {
        if (0 == strcmp(tmp->string, "name")) {
          child_update_data[child_dataNO].field = 2;
          snprintf(child_update_data[child_dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "BLESensorName")) {
          child_update_data[child_dataNO].field = 3;
          snprintf(child_update_data[child_dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "macAddr")) {
          child_update_data[child_dataNO].field = 4;
          snprintf(child_update_data[child_dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "manufacturer")) {
          child_update_data[child_dataNO].field = 5;
          snprintf(child_update_data[child_dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "type")) {
          child_update_data[child_dataNO].field = 6;
          snprintf(child_update_data[child_dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
#if (ENABLE_MODBUS_FEATURE == 1)
        // skip the description from DP
        // } else if (0 == strcmp(tmp->string, "description")) {
        //   child_update_data[child_dataNO].field = 7;
        //   snprintf(child_update_data[child_dataNO++].data.data_char,
        //            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
#endif
        }
      } else {
        if (0 == strcmp(tmp->string, "name")) {
          update_data[dataNO].field = 2;
          snprintf(update_data[dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "macAddr")) {
          update_data[dataNO].field = 3;
          snprintf(update_data[dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "manufacturer")) {
          update_data[dataNO].field = 4;
          snprintf(update_data[dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "type")) {
          update_data[dataNO].field = 9;
          snprintf(update_data[dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "timeNTPUrl")) {
          update_data[dataNO].field = 19;
          snprintf(update_data[dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "MQTTHost")) {
          update_data[dataNO].field = 21;
          snprintf(update_data[dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "staticIPAddr")) {
          update_data[dataNO].field = 24;
          snprintf(update_data[dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "netMask")) {
          update_data[dataNO].field = 25;
          snprintf(update_data[dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "DNSServer")) {
          update_data[dataNO].field = 26;
          snprintf(update_data[dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
        } else if (0 == strcmp(tmp->string, "gatewayAddr")) {
          update_data[dataNO].field = 27;
          snprintf(update_data[dataNO++].data.data_char,
                   GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
#if (ENABLE_MODBUS_FEATURE == 1)
        // skip description from DP
        // } else if (0 == strcmp(tmp->string, "description")) {
        //   update_data[dataNO].field = 33;
        //   snprintf(update_data[dataNO++].data.data_char,
        //            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", tmp->valuestring);
#endif
        }
      }
      // LOG_DEBUG(OUTPOINT, "field:%d,%s\r\n", update_data[dataNO -
      // 1].field,tmp->string);
    } break;
    case cJSON_Array:
    case cJSON_Object: {
      if (tmp->string != NULL) {
        snprintf(parentstr, sizeof(parentstr) - 1, "%s", tmp->string);
      }
      parse_gwitem(tmp);
      if (i + 1 == rootsize) {
        parentstr[0] = 0;
      }
    } break;
    default:
      LOG_WARN(OUTPOINT, "Invalid type!\r\n");
      break;
    }
  }
  if (0 == strcmp(parentstr, "children")) {
#if (ENABLE_MODBUS_FEATURE == 1)
    // TODO: check whether there are duplicated one or not (not necessary so far? to be confirmed!!!)
    if (0 != child_keyID) { // check nameID is valid (skip?)
      dev_nameid_list_current[dev_cnt_current] = child_keyID;
      dev_cnt_current++;
    }
    // workround: add 'description' field here as DP would not deliver it so far
    if(6 == child_dataNO) {
      child_update_data[child_dataNO].field = 7;  // field 7 is for description in dev table
      child_dataNO++;
    }
    // for both update and insert cases.
    if (ipc_sql_update_item(DEVICE_LIST_TABLE, child_keyID, child_dataNO,
                            child_update_data, NULL)) {
      LOG_WARN(OUTPOINT, "Failed to update %d into Dev table\r\n", child_keyID);
    }
    LOG_INFO(OUTPOINT, "update %d into Dev table with %d\r\n", child_keyID,
             child_dataNO);
    // update field for 'type'
    for(uint32_t tmp = 0; tmp < child_dataNO; tmp++)
    {
      if(child_update_data[tmp].field == 6)
        child_update_data[tmp].field = 11;
      // skip this 'attr' at the moment
      // if(child_update_data[tmp].field == 7) // the field of 'description' is 7 for dev table while it's 98 for sensor table
      //   child_update_data[tmp].field = 98;
    }
    if (ipc_sql_update_item(SENSOR_CONFIGURE_TABLE, child_keyID, child_dataNO,
                            child_update_data, NULL)) {
      LOG_WARN(OUTPOINT, "Failed to update %d into Sensor table\r\n", child_keyID);
    }
    LOG_INFO(OUTPOINT, "update %d into Sensor table with %d\r\n", child_keyID,
             child_dataNO);
    child_keyID = 0;
    child_dataNO = 0;
    memset(child_update_data, 0, sizeof(child_update_data));
#else
    if (tryto_update_child == false) {
      if (ipc_sql_delete_item(DEVICE_LIST_TABLE, 0, NULL)) {
        LOG_WARN(OUTPOINT, "Failed to clean sensors that was removed!\r\n");
      }
      tryto_update_child = true;
    }
    if (ipc_sql_update_item(DEVICE_LIST_TABLE, child_keyID, child_dataNO,
                            child_update_data, NULL)) {
      LOG_WARN(OUTPOINT, "Failed to update %d into Dev table\r\n", child_keyID);
    }
    LOG_INFO(OUTPOINT, "update %d into Dev table with %d\r\n", child_keyID,
             child_dataNO);
    for(uint32_t _tmp = 0; _tmp < child_dataNO; _tmp++)
    {
      if(child_update_data[_tmp].field == 6)
        child_update_data[_tmp].field = 11;
    }
    if (ipc_sql_update_item(SENSOR_CONFIGURE_TABLE, child_keyID, child_dataNO,
                            child_update_data, NULL)) {
      LOG_WARN(OUTPOINT, "Failed to update %d into Sensor table\r\n", child_keyID);
    }
    LOG_INFO(OUTPOINT, "update %d into Sensor table with %d\r\n", child_keyID,
             child_dataNO);
    child_keyID = 0;
    child_dataNO = 0;
    memset(child_update_data, 0, sizeof(child_update_data));
#endif
  }
}

int config_file_parse(const char *filename, char **jsonStr) {
  FILE *fd = NULL;
  int fileSize = 0;
  int rd_size = 0;
  struct stat statbuf = {0};
  stat(filename, &statbuf);
  fileSize = statbuf.st_size;
  *jsonStr = (char *)malloc(sizeof(char) * fileSize + 1);
  memset(*jsonStr, 0, fileSize + 1);
  LOG_INFO(OUTPOINT, ":%d\n", fileSize);
  fd = fopen(filename, "r");
  if (fd == NULL) {
    LOG_WARN(OUTPOINT, "Open file fail!\r\n");
    return -1;
  }
  rd_size = fread(*jsonStr, sizeof(char), fileSize, fd);
  if (rd_size != fileSize) {
    LOG_WARN(OUTPOINT, "failed to read json file!%d,%d\r\n", fileSize, rd_size);
    fclose(fd);
    return -1;
  }
  fclose(fd);
  return 0;
}

void parse_magicfileitem(cJSON *item) {
  cJSON *tmp = NULL;
  int rootsize = cJSON_GetArraySize(item);
  for (int i = 0; i < rootsize; i++) {
    if (NULL == (tmp = cJSON_GetArrayItem(item, i)))
      continue;
    switch (tmp->type) {
    case cJSON_Number: {
      if (0 == strcmp(parentstr, "disabledGatewayData")) {
        disabledData.GW[disabledData.gwCnt % (sizeof(disabledData.GW)/sizeof(disabledData.GW[0]))] = tmp->valueint;
        disabledData.gwCnt++;
      } else if (0 == strcmp(parentstr, "disabledPredictSensorData")){
        disabledData.PredictSensor[disabledData.predictSensorCnt % (sizeof(disabledData.PredictSensor)/sizeof(disabledData.PredictSensor[0]))] = tmp->valueint;
        disabledData.predictSensorCnt++;
      } else if (0 == strcmp(parentstr, "disabledInsightTData")){
        disabledData.InsightT[disabledData.insightTCnt % (sizeof(disabledData.InsightT)/sizeof(disabledData.InsightT[0]))] = tmp->valueint;
        disabledData.insightTCnt++;
      } else if (0 == strcmp(parentstr, "disabledPredictSensorProData")){
        disabledData.PredictSensorPro[disabledData.predictSensorProCnt % (sizeof(disabledData.PredictSensorPro)/sizeof(disabledData.PredictSensorPro[0]))] = tmp->valueint;
        disabledData.predictSensorProCnt++;
      } else if (0 == strcmp(parentstr, "disabledGatewayConfig")){
        disabledConfig.GW[disabledConfig.gwCnt % (sizeof(disabledConfig.GW)/sizeof(disabledConfig.GW[0]))] = tmp->valueint;
        disabledConfig.gwCnt++;
      } else if (0 == strcmp(parentstr, "disabledPredictSensorConfig")){
        disabledConfig.PredictSensor[disabledConfig.predictSensorCnt % (sizeof(disabledConfig.PredictSensor)/sizeof(disabledConfig.PredictSensor[0]))] = tmp->valueint;
        disabledConfig.predictSensorCnt++;
      } else if (0 == strcmp(parentstr, "disabledInsightTConfig")){
        disabledConfig.InsightT[disabledConfig.insightTCnt % (sizeof(disabledConfig.InsightT)/sizeof(disabledConfig.InsightT[0]))] = tmp->valueint;
        disabledConfig.insightTCnt++;
      } else if (0 == strcmp(parentstr, "disabledPredictSensorProConfig")){
        disabledConfig.PredictSensorPro[disabledConfig.predictSensorProCnt % (sizeof(disabledConfig.PredictSensorPro)/sizeof(disabledConfig.PredictSensorPro[0]))] = tmp->valueint;
        disabledConfig.predictSensorProCnt++;
      }
    } break;
    case cJSON_Array:
    case cJSON_Object: {
      if (tmp->string != NULL) {
        snprintf(parentstr, sizeof(parentstr) - 1, "%s", tmp->string);
      }
      parse_magicfileitem(tmp);
      if (i + 1 == rootsize) {
        parentstr[0] = 0;
      }
    } break;
    default:
      LOG_WARN(OUTPOINT, "Invalid type!\r\n");
      break;
    }
  }
}

void magicfile_parse(char *input_json)
{
  cJSON *_root = NULL;
  if (input_json == NULL) {
    LOG_WARN(OUTPOINT, "Invalid json string\r\n");
    return;
  }
  // LOG_INFO(OUTPOINT, "file:%s\r\n",input_json);
  _root = cJSON_Parse(input_json);
  if (_root == NULL) {
    // the json string seems invalid
    LOG_WARN(OUTPOINT, "Failed to  parse json\r\n");
    return;
  }
  parse_magicfileitem(_root);
  cJSON_Delete(_root);
}
