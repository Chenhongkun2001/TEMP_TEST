#include "jsonbuild.h"

#include "global.h"
// #include "query.h"
#include "ipcmbs.h"
#include "stdlib.h"
#include <string.h>
DBG_LOCAL_LOG_DEBUG

static gw_pro_sqlite_cond_element_t
    filterData[GW_PRO_MAX_DATA_FIELD_PER_OPERATION] = {0};
static gw_pro_command_sqlite_filter_response_t filter_rx = {0};

static void build_mac(char *str, char *mac) {
  for (uint8_t j = 0; j < 6; j++) {
    mac[(j << 1) + j] = str[j << 1];
    mac[(j << 1) + j + 1] = str[(j << 1) + 1];
    if (j < 5)
      mac[(j << 1) + j + 2] = '-';
  }
}

// tb includes the gw_config and devicelist
#if (ENABLE_MODBUS_FEATURE == 1)
void gw_config_partial(gw_pro_data_t *data, uint32_t dataNO, uint32_t tb_name,
                       cJSON *root, cJSON *child, cJSON *blecfg, cJSON *gwcfg,
                       cJSON *timecfg, cJSON *mqttcfg, cJSON *ipcfg, cJSON *modbuscfg) {
#else
void gw_config_partial(gw_pro_data_t *data, uint32_t dataNO, uint32_t tb_name,
                       cJSON *root, cJSON *child, cJSON *blecfg, cJSON *gwcfg,
                       cJSON *timecfg, cJSON *mqttcfg, cJSON *ipcfg) {
#endif
  char tmpstr[50] = {0};
  char macstr[50] = {0};
  for (uint32_t i = 0; i < dataNO; i++) {
    if (GW_PRO_TABLE_IDX_GW_CONFIG_TABLE == tb_name) {
      switch (data[i].field) {
      case 1: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint32);
        cJSON_AddNumberToObject(
            gwcfg, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;

      case 3: {
        build_mac(data[i].data.data_char, macstr);
        LOG_DEBUG(OUTPOINT, "gwmac:%s\r\n", macstr);
        cJSON_AddStringToObject(
            gwcfg, GatewayConfig[data[i].field - 1].filedname, macstr);
      } break;
      case 2:
      case 4:
#if (ENABLE_MODBUS_FEATURE == 1)
      case 9:
      case 33: {
#else
      case 9: {
#endif
        cJSON_AddStringToObject(gwcfg,
                                GatewayConfig[data[i].field - 1].filedname,
                                data[i].data.data_char);
      } break;
      //  case 5:
      // case 6:
      case 7: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
        cJSON_AddNumberToObject(
            root, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;
      case 8: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%ld", data[i].data.data_uint64);
        cJSON_AddNumberToObject(
            root, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;
      // case 9:
      case 10: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
        cJSON_AddNumberToObject(
            gwcfg, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;

      case 11:
      case 12: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_int8);
        cJSON_AddNumberToObject(
            blecfg, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;
      case 13:
      case 14: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
        cJSON_AddNumberToObject(
            blecfg, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;
      // case 15:
      case 16: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint32);
        cJSON_AddNumberToObject(
            blecfg, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;

      case 17: {
        if (data[i].data.data_uint8)
          cJSON_AddTrueToObject(timecfg,
                                GatewayConfig[data[i].field - 1].filedname);
        else
          cJSON_AddFalseToObject(timecfg,
                                 GatewayConfig[data[i].field - 1].filedname);
      } break;
      case 18: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
        cJSON_AddNumberToObject(
            timecfg, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;
      case 19: {
        cJSON_AddStringToObject(timecfg,
                                GatewayConfig[data[i].field - 1].filedname,
                                data[i].data.data_char);
      } break;

      case 20: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
        cJSON_AddNumberToObject(
            mqttcfg, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;
      case 21: {
        cJSON_AddStringToObject(mqttcfg,
                                GatewayConfig[data[i].field - 1].filedname,
                                data[i].data.data_char);
      } break;
      case 22: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint32);
        cJSON_AddNumberToObject(
            mqttcfg, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;

      case 23: {
        if (data[i].data.data_uint8)
          cJSON_AddTrueToObject(ipcfg,
                                GatewayConfig[data[i].field - 1].filedname);
        else
          cJSON_AddFalseToObject(ipcfg,
                                 GatewayConfig[data[i].field - 1].filedname);
      } break;
      case 24:
      case 25:
      case 26:
      case 27: {
        cJSON_AddStringToObject(ipcfg,
                                GatewayConfig[data[i].field - 1].filedname,
                                data[i].data.data_char);
      } break;
#if (ENABLE_MODBUS_FEATURE == 1)
      case 28:
      case 29: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint32);
        cJSON_AddNumberToObject(
            modbuscfg, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;
      case 30:
      case 31:
      case 32: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
        cJSON_AddNumberToObject(
            modbuscfg, GatewayConfig[data[i].field - 1].filedname, atof(tmpstr));
      } break;
#endif
      }
    } else {
      switch (data[i].field) {
      case 1: {
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint32);
        cJSON_AddNumberToObject(child, DeviceList[data[i].field - 1].filedname,
                                atof(tmpstr));
      } break;
      case 2:
      case 3:
      case 4:
      case 5:
#if (ENABLE_MODBUS_FEATURE == 1)
      case 6: 
      case 7: {
#else
      case 6: {
#endif
        cJSON_AddStringToObject(child, DeviceList[data[i].field - 1].filedname,
                                data[i].data.data_char);
      } break;
      }
    }
  }
}

void gw_config_build(gw_pro_message_header_t *pdata, char *json,
                     uint32_t json_len) {
  char *tempjson = NULL;
  uint32_t tb_name = 0;
  struct msgbuf msgbufp = {0};
  pdata = (gw_pro_message_header_t *)msgbufp.mtext;
  cJSON *root = cJSON_CreateObject();
  cJSON *blecfg = cJSON_CreateObject();
  cJSON *gwcfg = cJSON_CreateObject();
  cJSON *timecfg = cJSON_CreateObject();
  cJSON *mqttcfg = cJSON_CreateObject();
  cJSON *ipcfg = cJSON_CreateObject();
#if (ENABLE_MODBUS_FEATURE == 1)
  cJSON *modbuscfg = cJSON_CreateObject();
#endif
  cJSON *child = NULL;
  cJSON *children = cJSON_CreateArray();
  memset(&msgbufp, 0, sizeof(msgbufp));
  if (ipc_sql_query_item(GATEWAY_CONFIGURE_TABLE, 0, &msgbufp) ||
      pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
      pdata->command_or_response.responseMsg.response ==
          gw_pro_response_failed) {
    LOG_WARN(OUTPOINT, "Failed to get datas from gateway table\r\n");
    cJSON_Delete(root);
    cJSON_Delete(blecfg);
    cJSON_Delete(gwcfg);
    cJSON_Delete(timecfg);
    cJSON_Delete(mqttcfg);
    cJSON_Delete(ipcfg);
#if (ENABLE_MODBUS_FEATURE == 1)
    cJSON_Delete(modbuscfg);
#endif
    cJSON_Delete(children);
    return;
  }
  tb_name = revert_tablename(GATEWAY_CONFIGURE_TABLE);
  LOG_DEBUG(OUTPOINT, "tb:%d\r\n", tb_name)
#if (ENABLE_MODBUS_FEATURE == 1)
  gw_config_partial(pdata->command_or_response.responseMsg.responseInfo
                        .sqlite_update_item_response.inquiredData,
                    pdata->command_or_response.responseMsg.responseInfo
                        .sqlite_update_item_response.dataNum,
                    tb_name, root, NULL, blecfg, gwcfg, timecfg, mqttcfg,
                    ipcfg, modbuscfg);
#else
  gw_config_partial(pdata->command_or_response.responseMsg.responseInfo
                        .sqlite_update_item_response.inquiredData,
                    pdata->command_or_response.responseMsg.responseInfo
                        .sqlite_update_item_response.dataNum,
                    tb_name, root, NULL, blecfg, gwcfg, timecfg, mqttcfg,
                    ipcfg);
#endif
  cJSON_AddItemToObject(gwcfg, "BLEConfig", blecfg);
  cJSON_AddItemToObject(gwcfg, "timeConfig", timecfg);
  cJSON_AddItemToObject(gwcfg, "MQTTConfig", mqttcfg);
  cJSON_AddItemToObject(gwcfg, "IPConfig", ipcfg);
#if (ENABLE_MODBUS_FEATURE == 1)
  cJSON_AddItemToObject(gwcfg, "ModBus", modbuscfg);
#endif
  memset(filterData, 0, sizeof(filterData));
  filterData[0].condition = gw_pro_sqlite_cond_all;
  filterData[0].field = 1;
  if (ipc_sql_filter_item(DEVICE_LIST_TABLE, 1, filterData, &msgbufp) ||
      pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
      pdata->command_or_response.responseMsg.response ==
          gw_pro_response_failed) {
    LOG_WARN(OUTPOINT, "Failed to get datas from Dev table\r\n");
    cJSON_Delete(root);
    cJSON_Delete(gwcfg);
    cJSON_Delete(children);
    return;
  }
  memset(&filter_rx, 0, sizeof(filter_rx));
  memcpy(&filter_rx,
         &pdata->command_or_response.responseMsg.responseInfo
              .sqlite_filter_item_reponse,
         sizeof(gw_pro_command_sqlite_filter_response_t));
  cJSON_AddNumberToObject(gwcfg, "childrenNumber", filter_rx.dataNum);
  cJSON_AddItemToArray(children, cJSON_CreateObject());
  child = children->child;
  for (uint8_t i = 0; i < filter_rx.dataNum; i++) {
    memset(&msgbufp, 0, sizeof(msgbufp));
    if (ipc_sql_query_item(DEVICE_LIST_TABLE, filter_rx.filteredItemIdx[i],
                           &msgbufp) ||
        pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
        pdata->command_or_response.responseMsg.response ==
            gw_pro_response_failed) {
      LOG_WARN(OUTPOINT, "Failed to get datas from gateway table\r\n");
      cJSON_Delete(root);
      cJSON_Delete(gwcfg);
      cJSON_Delete(children);
      return;
    }
    tb_name = revert_tablename(DEVICE_LIST_TABLE);
#if (ENABLE_MODBUS_FEATURE == 1)
    gw_config_partial(pdata->command_or_response.responseMsg.responseInfo
                          .sqlite_update_item_response.inquiredData,
                      pdata->command_or_response.responseMsg.responseInfo
                          .sqlite_update_item_response.dataNum,
                      tb_name, NULL, child, NULL, NULL, NULL, NULL, NULL, NULL);
#else
    gw_config_partial(pdata->command_or_response.responseMsg.responseInfo
                          .sqlite_update_item_response.inquiredData,
                      pdata->command_or_response.responseMsg.responseInfo
                          .sqlite_update_item_response.dataNum,
                      tb_name, NULL, child, NULL, NULL, NULL, NULL, NULL);
#endif
    if (i + 1 < filter_rx.dataNum) {
      cJSON_AddItemToArray(children, cJSON_CreateObject());
      child = child->next;
    }
  }
  cJSON_AddItemToObject(gwcfg, "children", children);
  cJSON_AddItemToObject(root, "gwConfig", gwcfg);
  tempjson = cJSON_Print(root);
  snprintf(json, json_len, "%s", tempjson);
  // LOG_ERR(OUTPOINT, "json_len:%s\r\n", json);
  free(tempjson);
  tempjson = NULL;
  cJSON_Delete(root);
}

void sensor_config_build(gw_pro_data_t *data, uint32_t dataNO, char *json,
                         uint32_t len) {
  char *tempjson = NULL;
  char tmpstr[50] = {0};
  char macstr[50] = {0};
  cJSON *root = cJSON_CreateObject();
  cJSON *bullsensorcfg = cJSON_CreateObject();
  cJSON *syscfg = cJSON_CreateObject();
  cJSON *schedulecfg = cJSON_CreateObject();
  cJSON *sensingcfg = cJSON_CreateObject();
  cJSON *algocfg = cJSON_CreateObject();
  for (uint32_t i = 0; i < dataNO; i++) {
    switch (data[i].field) {
    case 1:
      // "INT PRIMARY KEY DEFAULT 0",
      // "nameId",
    case 96: {
      // "INT DEFAULT 0",
      // "FUOTA_Version",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint32);
      cJSON_AddNumberToObject(root, SensorConfig[data[i].field - 1].filedname,
                              atof(tmpstr));
    } break;
    case 4: {
      // "TEXT DEFAULT ' '",
      // "macAddr",
      build_mac(data[i].data.data_char, macstr);
      LOG_DEBUG(OUTPOINT, "sensormac:%s\r\n", macstr);
      cJSON_AddStringToObject(root, SensorConfig[data[i].field - 1].filedname,
                              macstr);
    } break;
    case 5:
      // "TEXT DEFAULT 'SKF'",
      // "manufacturer",
    case 97:
      // "TEXT DEFAULT ' '",
      // "FUOTA_Filename",
#if (ENABLE_MODBUS_FEATURE == 1)
    // case 98: // skip it at the moment.
      // "TEXT DEFAULT ' '",
      // "description",
#endif
    case 11: {
      // "TEXT DEFAULT ' '",
      // "type",
      cJSON_AddStringToObject(root, SensorConfig[data[i].field - 1].filedname,
                              data[i].data.data_char);
    } break;
    case 9: {
      // "BOOLEAN DEFAULT false",
      // "haveConfiguredToSensor",
      if (data[i].data.data_uint8)
        cJSON_AddTrueToObject(root, SensorConfig[data[i].field - 1].filedname);
      else
        cJSON_AddFalseToObject(root, SensorConfig[data[i].field - 1].filedname);
    } break;
    case 7: {
      // "INT DEFAULT 0",
      // "version",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
      cJSON_AddNumberToObject(root, SensorConfig[data[i].field - 1].filedname,
                              atof(tmpstr));
    } break;
    case 8:
      // "INT DEFAULT 0",
      // "lastEditTime",
    case 10: {
      // "INT DEFAULT 0",
      // "whenConfiguredToSensor",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%ld", data[i].data.data_uint64);
      cJSON_AddNumberToObject(root, SensorConfig[data[i].field - 1].filedname,
                              atof(tmpstr));
    } break;
    // case 12: {
    //   cJSON_AddStringToObject(syscfg, SensorConfig[data[i].field -
    //   1].filedname,
    //                           data[i].data.data_char);
    // } break;
    case 12: {
      // "INT DEFAULT 0",
      // "sensorMode",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
      cJSON_AddNumberToObject(syscfg, SensorConfig[data[i].field - 1].filedname,
                              atof(tmpstr));
    } break;
    case 14:
      // "INT DEFAULT 0",
      // "txPower_adv",
    case 16: {
      // "INT DEFAULT 0",
      // "txPower_auxAdv",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_int8);
      cJSON_AddNumberToObject(syscfg, SensorConfig[data[i].field - 1].filedname,
                              atof(tmpstr));
    } break;
    case 15: {
      // "INT DEFAULT 0",
      // "dataRate_auxAdv",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint16);
      cJSON_AddNumberToObject(syscfg, SensorConfig[data[i].field - 1].filedname,
                              atof(tmpstr));
    } break;
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
    case 98: {
      // "INT DEFAULT 0",
      // "sensorModeParameter",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
      cJSON_AddNumberToObject(syscfg, SensorConfig[data[i].field - 1].filedname,
                              atof(tmpstr));
    } break;
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
    case 17:
      // "INT DEFAULT 0",
      // "period_quickPolling",
    case 18:
      // "INT DEFAULT 0",
      // "referenceTime_quickPolling",
      // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
			// but locally use a uint32_t to store
    case 19:
      // "INT DEFAULT 0",
      // "period_regularSensing",
    case 20:
      // "INT DEFAULT 0",
      // "referenceTime_regularSensing",
      // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
			// but locally use a uint32_t to store
    case 21:
      // "INT DEFAULT 0",
      // "period_comm",
    case 22:
      // "INT DEFAULT 0",
      // "referenceTime_period_comm",
      // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
			// but locally use a uint32_t to store
    case 94: 
      // "INT DEFAULT 0",
      // "waveDataAcqPeriod",
    case 95: {
      // "INT DEFAULT 0",
      // "waveDataAcqReference",
      // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
			// but locally use a uint32_t to store
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint32);
      cJSON_AddNumberToObject(
          schedulecfg, SensorConfig[data[i].field - 1].filedname, atof(tmpstr));
    } break;
    case 23:
      // "INT DEFAULT 0",
      // "PRE_ACQ_VIB_FS_HZ",
    case 24:
      // "INT DEFAULT 0",
      // "PRE_ACQ_VIB_N",
    case 27:
      // "INT DEFAULT 0",
      // "PRE_ACQ_MAG_FS_HZ",
    case 28:
      // "INT DEFAULT 0",
      // "PRE_ACQ_MAG_N",
    case 30:
      // "INT DEFAULT 0",
      // "Facc",
    case 31:
      // "INT DEFAULT 0",
      // "Nacc",
    case 34:
      // "INT DEFAULT 0",
      // "ACQ_MAG_FS_HZ",
#if (0) // move these 4 fields into 'algorithmConfig' section
    case 75:
      // "INT DEFAULT 0",
      // "MTR_INFO_FL",
    case 76:
      // "INT DEFAULT 0",
      // "MTR_INFO_BAR",
    case 77:
      // "INT DEFAULT 0",
      // "GEAR_INFO_TOOTH",
    case 78:
      // "INT DEFAULT 0",
      // "FAN_INFO_BLADE",
    case 79:
      // "INT DEFAULT 0",
      // "PUMP_INFO_VANE",
#endif
    case 35: {
      // "INT DEFAULT 0",
      // "ACQ_MAG_N",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint32);
      cJSON_AddNumberToObject(
          sensingcfg, SensorConfig[data[i].field - 1].filedname, atof(tmpstr));
    } break;
    case 81: {
      // "INT DEFAULT 0",
      // "AlarmThrestemp",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_int32);
      cJSON_AddNumberToObject(
          algocfg, SensorConfig[data[i].field - 1].filedname, atof(tmpstr));
    } break;
    case 25:
      // "INT DEFAULT 0",
      // "PRE_ACQ_VIB_AXIS_ACQ_EVAL",
    case 26:
      // "INT DEFAULT 0",
      // "PRE_ACQ_VIB_RANGE",
    case 29:
      // "INT DEFAULT 0",
      // "PRE_ACQ_MAG_AXIS_ACQ_EVAL",
    case 32:
      // "INT DEFAULT 0",
      // "ACQ_VIB_AXIS_ACQ_EVAL",
    case 33:
      // "INT DEFAULT 0",
      // "ACQ_VIB_RANGE",
    case 36: {
      // "INT DEFAULT 0",
      // "ACQ_MAG_AXIS_ACQ_EVAL",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
      cJSON_AddNumberToObject(
          sensingcfg, SensorConfig[data[i].field - 1].filedname, atof(tmpstr));
    } break;
    case 37: {
      // "REAL DEFAULT 0.364",
      // "FS_COEF",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%f", data[i].data.data_float);
      cJSON_AddNumberToObject(
          sensingcfg, SensorConfig[data[i].field - 1].filedname, atof(tmpstr));
    } break;
    case 13:
      // "REAL DEFAULT 0.0",
      // "batteryAlarmThreshold",
    case 38:
      // "REAL DEFAULT 0.0",
      // "GEE_COEF",
    case 39:
      // "REAL DEFAULT 0.0",
      // "V_COEF",
    case 45:
      // "REAL DEFAULT 0.0",
      // "VIB_RMS_START_TL",
    case 47:
      // "REAL DEFAULT 0.0",
      // "MAG_RMS_START_TL",
    case 49:
      // "REAL DEFAULT 0.0",
      // "MAG_RMS_VAR_TH",
    case 50:
      // "REAL DEFAULT 0.0",
      // "RPM_VAR_RANGE",
    case 69:
      // "REAL DEFAULT 0.0",
      // "BORE_DIAMETER_MM",
    case 70:
      // "REAL DEFAULT 0.0",
      // "RUN_SPEED_RPM",
    case 71:
      // "REAL DEFAULT 0.0",
      // "BRG_INFO_BPFO",
    case 72:
      // "REAL DEFAULT 0.0",
      // "BRG_INFO_BPFI",
    case 73:
      // "REAL DEFAULT 0.0",
      // "BRG_INFO_BSF",
    case 74:
      // "REAL DEFAULT 0.0",
      // "BRG_INFO_FTF",
    case 80:
      // "REAL DEFAULT 0.0",
      // "TEMP_OV_ALERT_CDEGREE",
    case 82:
      // "REAL DEFAULT 0.0",
      // "ACC_OV_ALERT",
    case 83:
      // "REAL DEFAULT 0.0",
      // "ACC_OV_ALARM",
    case 84:
      // "REAL DEFAULT 0.0",
      // "ACC_HAL_ALERT",
    case 85:
      // "REAL DEFAULT 0.0",
      // "ACC_HAL_ALARM",
    case 86:
      // "REAL DEFAULT 0.0",
      // "VEL_OV_ALERT",
    case 87:
      // "REAL DEFAULT 0.0",
      // "VEL_OV_ALARM",
    case 88:
      // "REAL DEFAULT 0.0",
      // "VEL_HAL_ALERT",
    case 89:
      // "REAL DEFAULT 0.0",
      // "VEL_HAL_ALARM",
    case 90:
      // "REAL DEFAULT 0.0",
      // "ENV_OV_ALERT",
    case 91:
      // "REAL DEFAULT 0.0",
      // "ENV_OV_ALARM",
    case 92:
      // "REAL DEFAULT 0.0",
      // "ENV_HAL_ALERT",
    case 93: {
      // "REAL DEFAULT 0.0",
      // "ENV_HAL_ALARM",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%f", data[i].data.data_float);
      cJSON_AddNumberToObject(
          algocfg, SensorConfig[data[i].field - 1].filedname, atof(tmpstr));
    } break;
    case 44:
      // "BOOLEAN DEFAULT false",
      // "VIB_START_FG",
    case 46:
      // "BOOLEAN DEFAULT false",
      // "MAG_START_FG",
    case 48:
      // "BOOLEAN DEFAULT false",
      // "MAG_STABLE_FG",
    case 58:
      // "BOOLEAN DEFAULT false",
      // "FUNC_ANOM_TEMP",
    case 59:
      // "BOOLEAN DEFAULT false",
      // "FUNC_ANOM_OV",
    case 60:
      // "BOOLEAN DEFAULT false",
      // "FUNC_ANOM_MECH",
    case 61:
      // "BOOLEAN DEFAULT false",
      // "FUNC_ANOM_BRG",
    case 62:
      // "BOOLEAN DEFAULT false",
      // "FUNC_ANOM_LUB",
    case 63:
      // "BOOLEAN DEFAULT false",
      // "FUNC_ANOM_MTR",
    case 64:
      // "BOOLEAN DEFAULT false",
      // "FUNC_ANOM_GEAR",
    case 65:
      // "BOOLEAN DEFAULT false",
      // "FUNC_ANOM_FAN",
    case 66: {
      // "BOOLEAN DEFAULT false",
      // "FUNC_ANOM_PUMP",
      if (data[i].data.data_uint8)
        cJSON_AddTrueToObject(algocfg,
                              SensorConfig[data[i].field - 1].filedname);
      else
        cJSON_AddFalseToObject(algocfg,
                               SensorConfig[data[i].field - 1].filedname);
    } break;
    case 40:
      // "INT DEFAULT 0",
      // "MEAS_POSITION",
    case 41:
      // "INT DEFAULT 0",
      // "MEAS_LOAD",
    case 42:
      // "INT DEFAULT 0",
      // "MEAS_AXIS_VIB",
    case 43:
      // "INT DEFAULT 0",
      // "MEAS_AXIS_MAG",
    case 67:
      // "INT DEFAULT 0",
      // "ASSET_LEVEL",
    case 68: {
      // "INT DEFAULT 0",
      // "FLEX_TYPE",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint8);
      cJSON_AddNumberToObject(
          algocfg, SensorConfig[data[i].field - 1].filedname, atof(tmpstr));
    } break;
    case 51:
      // "INT DEFAULT 0",
      // "DEC_LOGIC_TEMP_M",
    case 52:
      // "INT DEFAULT 0",
      // "DEC_LOGIC_TEMP_N",
    case 53:
      // "INT DEFAULT 0",
      // "DEC_LOGIC_LEARN_NUM_TEMP",
    case 54:
      // "INT DEFAULT 0",
      // "DEC_LOGIC_VIB_M",
    case 55:
      // "INT DEFAULT 0",
      // "DEC_LOGIC_VIB_N",
    case 56:
      // "INT DEFAULT 0",
      // "DEC_LOGIC_LEARN_NUM_VIB",
    case 57: {
      // "INT DEFAULT 0",
      // "DEC_LOGIC_LEARN_NUM_MAG",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint16);
      cJSON_AddNumberToObject(
          algocfg, SensorConfig[data[i].field - 1].filedname, atof(tmpstr));
    } break;
    case 75:
      // "INT DEFAULT 0",
      // "MTR_INFO_FL",
    case 76:
      // "INT DEFAULT 0",
      // "MTR_INFO_BAR",
    case 77:
      // "INT DEFAULT 0",
      // "GEAR_INFO_TOOTH",
    case 78:
      // "INT DEFAULT 0",
      // "FAN_INFO_BLADE",
    case 79: {
      // "INT DEFAULT 0",
      // "PUMP_INFO_VANE",
      snprintf(tmpstr, sizeof(tmpstr) - 1, "%d", data[i].data.data_uint32);
      cJSON_AddNumberToObject(
          algocfg, SensorConfig[data[i].field - 1].filedname, atof(tmpstr));
    } break;
    }
  }
  cJSON_AddItemToObject(bullsensorcfg, "sysConfig", syscfg);
  cJSON_AddItemToObject(bullsensorcfg, "scheduleConfig", schedulecfg);
  cJSON_AddItemToObject(bullsensorcfg, "sensingConfig", sensingcfg);
  cJSON_AddItemToObject(bullsensorcfg, "algorithmConfig", algocfg);
  cJSON_AddItemToObject(root, "bulletSensorConfig", bullsensorcfg);
  tempjson = cJSON_Print(root);
  snprintf(json, len, "%s", tempjson);
  // LOG_ERR(OUTPOINT, "json_len:%s\r\n", json);
  free(tempjson);
  tempjson = NULL;
  cJSON_Delete(root);
}
