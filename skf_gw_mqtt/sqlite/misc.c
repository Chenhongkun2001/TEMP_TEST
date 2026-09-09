#include "global.h"
#include "ppGW.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <stdlib.h>
DBG_LOCAL_LOG_DEBUG

extern gw_pro_command_sqlite_query_item_response_t query_data;
extern gw_pro_command_sqlite_filter_response_t filter_data;
extern bool itemfalg;
extern uint8_t global_tb_name;
extern char gwconfigmac[50];
extern uint32_t gwconfigid;
bool testflag = false;
char filename[GW_PRO_MAX_STRING_LEN_BYTE] = {0};
extern uint64_t autoid;

int autoid_cb(void *NotUsed, int argc, char **argv, char **azColName) {
  for (int i = 0; i < argc; ++i) {
    if (argv[i] == NULL)
      continue;
    if (0 == strcmp(azColName[i], "id")) {
      autoid = atoi(argv[i]);
      // LOG_DEBUG(OUTPOINT, "argv[%d]:%s\r\n", i, argv[i]);
    }
  }
  return 0;
}
int Item_check_cb(void *NotUsed, int argc, char **argv, char **azColName) {

  if (argc != 0)
    itemfalg = true;
  if (GW_PRO_TABLE_IDX_GW_CONFIG_TABLE == global_tb_name) {
    for (int i = 0; i < argc; ++i) {
      if (0 == strcmp(azColName[i], "macAddr")) {
        snprintf(gwconfigmac, sizeof(gwconfigmac) - 1, "%s", argv[i]);
      } else if (0 == strcmp(azColName[i], "nameId")) {
        gwconfigid = atoi(argv[i]);
      }
    }
  }

  // LOG_INFO(OUTPOINT, "argc:%d\r\n", argc);
  return 0;
}

int gateway_cb(void *NotUsed, int argc, char **argv, char **azColName) {

  for (int i = 0; i < argc; ++i) {
    if (argv[i] == NULL ||
        query_data.dataNum >= GW_PRO_MAX_DATA_FIELD_PER_OPERATION)
      continue;
    if (0 == strcmp(azColName[i], "nameId")) {
      query_data.inquiredData[query_data.dataNum].field = 1;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "name")) {
      query_data.inquiredData[query_data.dataNum].field = 2;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "macAddr")) {
      query_data.inquiredData[query_data.dataNum].field = 3;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "manufacturer")) {
      query_data.inquiredData[query_data.dataNum].field = 4;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "version")) {
      query_data.inquiredData[query_data.dataNum].field = 7;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "lastEditTime")) {
      query_data.inquiredData[query_data.dataNum].field = 8;
      query_data.inquiredData[query_data.dataNum].data.data_uint64 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "type")) {
      query_data.inquiredData[query_data.dataNum].field = 9;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "gatewayMode")) {
      query_data.inquiredData[query_data.dataNum].field = 10;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "BLE1TxPower")) {
      query_data.inquiredData[query_data.dataNum].field = 11;
      query_data.inquiredData[query_data.dataNum].data.data_int8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "BLE2TxPower")) {
      query_data.inquiredData[query_data.dataNum].field = 12;
      query_data.inquiredData[query_data.dataNum].data.data_int8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "BLE1Antenna")) {
      query_data.inquiredData[query_data.dataNum].field = 13;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "BLE2Antenna")) {
      query_data.inquiredData[query_data.dataNum].field = 14;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "BLEDuplicatedDrop")) {
      query_data.inquiredData[query_data.dataNum].field = 16;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "timeStamp")) {
      query_data.inquiredData[query_data.dataNum].field = 17;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "timeSetting")) {
      query_data.inquiredData[query_data.dataNum].field = 18;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "timeNTPUrl")) {
      query_data.inquiredData[query_data.dataNum].field = 19;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "MQTTSecurityType")) {
      query_data.inquiredData[query_data.dataNum].field = 20;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "MQTTHost")) {
      query_data.inquiredData[query_data.dataNum].field = 21;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "MQTTPort")) {
      query_data.inquiredData[query_data.dataNum].field = 22;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "DHCPEnable")) {
      query_data.inquiredData[query_data.dataNum].field = 23;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "staticIPAddr")) {
      query_data.inquiredData[query_data.dataNum].field = 24;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "netMask")) {
      query_data.inquiredData[query_data.dataNum].field = 25;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "DNSServer")) {
      query_data.inquiredData[query_data.dataNum].field = 26;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "gatewayAddr")) {
      query_data.inquiredData[query_data.dataNum].field = 27;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
#if (ENABLE_MODBUS_FEATURE == 1)
    } else if (0 == strcmp(azColName[i], "baudrateSlave")) {
      query_data.inquiredData[query_data.dataNum].field = 28;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "slaveAddrSlave")) {
      query_data.inquiredData[query_data.dataNum].field = 29;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "paritySlave")) {
      query_data.inquiredData[query_data.dataNum].field = 30;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "stopSlave")) {
      query_data.inquiredData[query_data.dataNum].field = 31;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "registerMode")) {
      query_data.inquiredData[query_data.dataNum].field = 32;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "description")) {
      query_data.inquiredData[query_data.dataNum].field = 33;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
#endif
    }
    query_data.dataNum++;
    // LOG_INFO(OUTPOINT, "gw_cb:%s = %s\n", azColName[i],
    //            (argv[i] ? argv[i] : "NULL"));
  }
  return 0;
}
int devicelist_cb(void *NotUsed, int argc, char **argv, char **azColName) {
  for (int i = 0; i < argc; ++i) {
    if (argv[i] == NULL ||
        query_data.dataNum >= GW_PRO_MAX_DATA_FIELD_PER_OPERATION)
      continue;
    if (0 == strcmp(azColName[i], "nameId")) {
      query_data.inquiredData[query_data.dataNum].field = 1;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "name")) {
      query_data.inquiredData[query_data.dataNum].field = 2;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "BLESensorName")) {
      query_data.inquiredData[query_data.dataNum].field = 3;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "macAddr")) {
      query_data.inquiredData[query_data.dataNum].field = 4;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "manufacturer")) {
      query_data.inquiredData[query_data.dataNum].field = 5;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "type")) {
      query_data.inquiredData[query_data.dataNum].field = 6;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
#if (ENABLE_MODBUS_FEATURE == 1)
    } else if (0 == strcmp(azColName[i], "description")) {
      query_data.inquiredData[query_data.dataNum].field = 7;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
#endif
    }
    query_data.dataNum++;
    // LOG_INFO(OUTPOINT, "devicelist_cb:%s = %s\n", azColName[i],
    //            (argv[i] ? argv[i] : "NULL"));
  }
  return 0;
}
int sensor_cb(void *NotUsed, int argc, char **argv, char **azColName) {
  for (int i = 0; i < argc; ++i) {
    if (argv[i] == NULL ||
        query_data.dataNum >= GW_PRO_MAX_DATA_FIELD_PER_OPERATION)
      continue;
    if (0 == strcmp(azColName[i], "nameId")) {
      query_data.inquiredData[query_data.dataNum].field = 1;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "name")) {
      query_data.inquiredData[query_data.dataNum].field = 2;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "BLESensorName")) {
      query_data.inquiredData[query_data.dataNum].field = 3;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "macAddr")) {
      query_data.inquiredData[query_data.dataNum].field = 4;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "manufacturer")) {
      query_data.inquiredData[query_data.dataNum].field = 5;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "FirmwareVersion")) {
      query_data.inquiredData[query_data.dataNum].field = 6;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "version")) {
      query_data.inquiredData[query_data.dataNum].field = 7;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "lastEditTime")) {
      query_data.inquiredData[query_data.dataNum].field = 8;
      query_data.inquiredData[query_data.dataNum].data.data_uint64 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "haveConfiguredToSensor")) {
      query_data.inquiredData[query_data.dataNum].field = 9;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "whenConfiguredToSensor")) {
      query_data.inquiredData[query_data.dataNum].field = 10;
      query_data.inquiredData[query_data.dataNum].data.data_uint64 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "type")) {
      query_data.inquiredData[query_data.dataNum].field = 11;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "sensorMode")) {
      query_data.inquiredData[query_data.dataNum].field = 12;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
      // snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
      //          GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "batteryAlarmThreshold")) {
      query_data.inquiredData[query_data.dataNum].field = 13;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "txPower_adv")) {
      query_data.inquiredData[query_data.dataNum].field = 14;
      query_data.inquiredData[query_data.dataNum].data.data_int8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "dataRate_auxAdv")) {
      query_data.inquiredData[query_data.dataNum].field = 15;
      query_data.inquiredData[query_data.dataNum].data.data_uint16 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "txPower_auxAdv")) {
      query_data.inquiredData[query_data.dataNum].field = 16;
      query_data.inquiredData[query_data.dataNum].data.data_int8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "period_quickPolling")) {
      query_data.inquiredData[query_data.dataNum].field = 17;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "referenceTime_quickPolling")) {
      query_data.inquiredData[query_data.dataNum].field = 18;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "period_regularSensing")) {
      query_data.inquiredData[query_data.dataNum].field = 19;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "referenceTime_regularSensing")) {
      query_data.inquiredData[query_data.dataNum].field = 20;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "period_comm")) {
      query_data.inquiredData[query_data.dataNum].field = 21;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "referenceTime_period_comm")) {
      query_data.inquiredData[query_data.dataNum].field = 22;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "PRE_ACQ_VIB_FS_HZ")) {
      query_data.inquiredData[query_data.dataNum].field = 23;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "PRE_ACQ_VIB_N")) {
      query_data.inquiredData[query_data.dataNum].field = 24;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "PRE_ACQ_VIB_AXIS_ACQ_EVAL")) {
      query_data.inquiredData[query_data.dataNum].field = 25;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "PRE_ACQ_VIB_RANGE")) {
      query_data.inquiredData[query_data.dataNum].field = 26;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "PRE_ACQ_MAG_FS_HZ")) {
      query_data.inquiredData[query_data.dataNum].field = 27;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "PRE_ACQ_MAG_N")) {
      query_data.inquiredData[query_data.dataNum].field = 28;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "PRE_ACQ_MAG_AXIS_ACQ_EVAL")) {
      query_data.inquiredData[query_data.dataNum].field = 29;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "Facc")) {
      query_data.inquiredData[query_data.dataNum].field = 30;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "Nacc")) {
      query_data.inquiredData[query_data.dataNum].field = 31;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "ACQ_VIB_AXIS_ACQ_EVAL")) {
      query_data.inquiredData[query_data.dataNum].field = 32;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "ACQ_VIB_RANGE")) {
      query_data.inquiredData[query_data.dataNum].field = 33;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "ACQ_MAG_FS_HZ")) {
      query_data.inquiredData[query_data.dataNum].field = 34;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "ACQ_MAG_N")) {
      query_data.inquiredData[query_data.dataNum].field = 35;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "ACQ_MAG_AXIS_ACQ_EVAL")) {
      query_data.inquiredData[query_data.dataNum].field = 36;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FS_COEF")) {
      query_data.inquiredData[query_data.dataNum].field = 37;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "GEE_COEF")) {
      query_data.inquiredData[query_data.dataNum].field = 38;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "V_COEF")) {
      query_data.inquiredData[query_data.dataNum].field = 39;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "MEAS_POSITION")) {
      query_data.inquiredData[query_data.dataNum].field = 40;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "MEAS_LOAD")) {
      query_data.inquiredData[query_data.dataNum].field = 41;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "MEAS_AXIS_VIB")) {
      query_data.inquiredData[query_data.dataNum].field = 42;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "MEAS_AXIS_MAG")) {
      query_data.inquiredData[query_data.dataNum].field = 43;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "VIB_START_FG")) {
      query_data.inquiredData[query_data.dataNum].field = 44;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "VIB_RMS_START_TL")) {
      query_data.inquiredData[query_data.dataNum].field = 45;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "MAG_START_FG")) {
      query_data.inquiredData[query_data.dataNum].field = 46;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "MAG_RMS_START_TL")) {
      query_data.inquiredData[query_data.dataNum].field = 47;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "MAG_STABLE_FG")) {
      query_data.inquiredData[query_data.dataNum].field = 48;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "MAG_RMS_VAR_TH")) {
      query_data.inquiredData[query_data.dataNum].field = 49;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "RPM_VAR_RANGE")) {
      query_data.inquiredData[query_data.dataNum].field = 50;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "DEC_LOGIC_TEMP_M")) {
      query_data.inquiredData[query_data.dataNum].field = 51;
      query_data.inquiredData[query_data.dataNum].data.data_uint16 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "DEC_LOGIC_TEMP_N")) {
      query_data.inquiredData[query_data.dataNum].field = 52;
      query_data.inquiredData[query_data.dataNum].data.data_uint16 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "DEC_LOGIC_LEARN_NUM_TEMP")) {
      query_data.inquiredData[query_data.dataNum].field = 53;
      query_data.inquiredData[query_data.dataNum].data.data_uint16 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "DEC_LOGIC_VIB_M")) {
      query_data.inquiredData[query_data.dataNum].field = 54;
      query_data.inquiredData[query_data.dataNum].data.data_uint16 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "DEC_LOGIC_VIB_N")) {
      query_data.inquiredData[query_data.dataNum].field = 55;
      query_data.inquiredData[query_data.dataNum].data.data_uint16 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "DEC_LOGIC_LEARN_NUM_VIB")) {
      query_data.inquiredData[query_data.dataNum].field = 56;
      query_data.inquiredData[query_data.dataNum].data.data_uint16 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "DEC_LOGIC_LEARN_NUM_MAG")) {
      query_data.inquiredData[query_data.dataNum].field = 57;
      query_data.inquiredData[query_data.dataNum].data.data_uint16 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUNC_ANOM_TEMP")) {
      query_data.inquiredData[query_data.dataNum].field = 58;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUNC_ANOM_OV")) {
      query_data.inquiredData[query_data.dataNum].field = 59;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUNC_ANOM_MECH")) {
      query_data.inquiredData[query_data.dataNum].field = 60;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUNC_ANOM_BRG")) {
      query_data.inquiredData[query_data.dataNum].field = 61;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUNC_ANOM_LUB")) {
      query_data.inquiredData[query_data.dataNum].field = 62;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUNC_ANOM_MTR")) {
      query_data.inquiredData[query_data.dataNum].field = 63;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUNC_ANOM_GEAR")) {
      query_data.inquiredData[query_data.dataNum].field = 64;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUNC_ANOM_FAN")) {
      query_data.inquiredData[query_data.dataNum].field = 65;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUNC_ANOM_PUMP")) {
      query_data.inquiredData[query_data.dataNum].field = 66;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "ASSET_LEVEL")) {
      query_data.inquiredData[query_data.dataNum].field = 67;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FLEX_TYPE")) {
      query_data.inquiredData[query_data.dataNum].field = 68;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "BORE_DIAMETER_MM")) {
      query_data.inquiredData[query_data.dataNum].field = 69;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "RUN_SPEED_RPM")) {
      query_data.inquiredData[query_data.dataNum].field = 70;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "BRG_INFO_BPFO")) {
      query_data.inquiredData[query_data.dataNum].field = 71;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "BRG_INFO_BPFI")) {
      query_data.inquiredData[query_data.dataNum].field = 72;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "BRG_INFO_BSF")) {
      query_data.inquiredData[query_data.dataNum].field = 73;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "BRG_INFO_FTF")) {
      query_data.inquiredData[query_data.dataNum].field = 74;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "MTR_INFO_FL")) {
      query_data.inquiredData[query_data.dataNum].field = 75;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "MTR_INFO_BAR")) {
      query_data.inquiredData[query_data.dataNum].field = 76;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "GEAR_INFO_TOOTH")) {
      query_data.inquiredData[query_data.dataNum].field = 77;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FAN_INFO_BLADE")) {
      query_data.inquiredData[query_data.dataNum].field = 78;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "PUMP_INFO_VANE")) {
      query_data.inquiredData[query_data.dataNum].field = 79;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "TEMP_OV_ALERT_CDEGREE")) {
      query_data.inquiredData[query_data.dataNum].field = 80;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "AlarmThrestemp")) {
      query_data.inquiredData[query_data.dataNum].field = 81;
      query_data.inquiredData[query_data.dataNum].data.data_int32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "ACC_OV_ALERT")) {
      query_data.inquiredData[query_data.dataNum].field = 82;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "ACC_OV_ALARM")) {
      query_data.inquiredData[query_data.dataNum].field = 83;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "ACC_HAL_ALERT")) {
      query_data.inquiredData[query_data.dataNum].field = 84;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "ACC_HAL_ALARM")) {
      query_data.inquiredData[query_data.dataNum].field = 85;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "VEL_OV_ALERT")) {
      query_data.inquiredData[query_data.dataNum].field = 86;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "VEL_OV_ALARM")) {
      query_data.inquiredData[query_data.dataNum].field = 87;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "VEL_HAL_ALERT")) {
      query_data.inquiredData[query_data.dataNum].field = 88;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "VEL_HAL_ALARM")) {
      query_data.inquiredData[query_data.dataNum].field = 89;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "ENV_OV_ALERT")) {
      query_data.inquiredData[query_data.dataNum].field = 90;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "ENV_OV_ALARM")) {
      query_data.inquiredData[query_data.dataNum].field = 91;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "ENV_HAL_ALERT")) {
      query_data.inquiredData[query_data.dataNum].field = 92;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "ENV_HAL_ALARM")) {
      query_data.inquiredData[query_data.dataNum].field = 93;
      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "waveDataAcqPeriod")) {
      query_data.inquiredData[query_data.dataNum].field = 94;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "waveDataAcqReference")) {
      query_data.inquiredData[query_data.dataNum].field = 95;
      query_data.inquiredData[query_data.dataNum].data.data_uint64 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUOTA_Version")) {
      query_data.inquiredData[query_data.dataNum].field = 96;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "FUOTA_Filename")) {
      query_data.inquiredData[query_data.dataNum].field = 97;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
#if (ENABLE_MODBUS_FEATURE == 1)
    // skip it at the moment
    // } else if (0 == strcmp(azColName[i], "description")) {
    //   query_data.inquiredData[query_data.dataNum].field = 98;
    //   snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
    //            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
#endif
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
    } else if (0 == strcmp(azColName[i], "sensorModeParameter")) {
      query_data.inquiredData[query_data.dataNum].field = 98;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atof(argv[i]);
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
    }
    query_data.dataNum++;
    // LOG_INFO(OUTPOINT, "sensor_cb:%s = %s\n", azColName[i],
    //            (argv[i] ? argv[i] : "NULL"));
  }
  return 0;
}
int sensordata_cb(void *NotUsed, int argc, char **argv, char **azColName) {
  char strtemp[70] = {0};
  for (int i = 0; i < argc; ++i) {
    if (argv[i] == NULL ||
        query_data.dataNum >= GW_PRO_MAX_DATA_FIELD_PER_OPERATION)
      continue;
    if (0 == strcmp(azColName[i], "nameId")) {
      query_data.inquiredData[query_data.dataNum].field = 2;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "SequenceNumber")) {
      query_data.inquiredData[query_data.dataNum].field = 1;
      query_data.inquiredData[query_data.dataNum].data.data_uint64 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "BLESensorName")) {
      query_data.inquiredData[query_data.dataNum].field = 3;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "macAddr")) {
      query_data.inquiredData[query_data.dataNum].field = 4;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "manufacturer")) {
      query_data.inquiredData[query_data.dataNum].field = 6;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "type")) {
      query_data.inquiredData[query_data.dataNum].field = 5;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
    } else if (0 == strcmp(azColName[i], "measureseqno")) {
      query_data.inquiredData[query_data.dataNum].field = 7;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "sampletime")) {
      query_data.inquiredData[query_data.dataNum].field = 8;
      query_data.inquiredData[query_data.dataNum].data.data_uint64 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "ReceivedTimestamp")) {
      query_data.inquiredData[query_data.dataNum].field = 9;
      query_data.inquiredData[query_data.dataNum].data.data_uint64 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "measurement")) {
      query_data.inquiredData[query_data.dataNum].field = 10;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "DataType")) {
      query_data.inquiredData[query_data.dataNum].field = 11;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "Format")) {
      query_data.inquiredData[query_data.dataNum].field = 12;
      snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
      if (0 == strcmp(argv[i], "Digit")) {
        testflag = true;
      } else {
        testflag = false;
      }
    } else if (0 == strcmp(azColName[i], "range")) {
      query_data.inquiredData[query_data.dataNum].field = 13;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "unit")) {
      query_data.inquiredData[query_data.dataNum].field = 14;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "measurelengthsample")) {
      query_data.inquiredData[query_data.dataNum].field = 15;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "totaldatalengthsample")) {
      query_data.inquiredData[query_data.dataNum].field = 16;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "dimension")) {
      query_data.inquiredData[query_data.dataNum].field = 17;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "dataformat")) {
      query_data.inquiredData[query_data.dataNum].field = 18;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "sampleperiods")) {
      query_data.inquiredData[query_data.dataNum].field = 19;
      query_data.inquiredData[query_data.dataNum].data.data_uint32 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "encryp")) {
      query_data.inquiredData[query_data.dataNum].field = 20;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "value")) {
      query_data.inquiredData[query_data.dataNum].field = 21;
      if (testflag) {
        query_data.inquiredData[query_data.dataNum].data.data_float =
            atof(argv[i]);
        // LOG_DEBUG(OUTPOINT, "query data:%f\r\n",
        //           query_data.inquiredData[query_data.dataNum].data.data_float);
      } else {
        snprintf(query_data.inquiredData[query_data.dataNum].data.data_char,
                 GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
        snprintf(filename, GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", argv[i]);
      }

    } else if (0 == strcmp(azColName[i], "sample_rate")) {
      query_data.inquiredData[query_data.dataNum].field = 22;

      query_data.inquiredData[query_data.dataNum].data.data_float =
          atof(argv[i]);
    } else if (0 == strcmp(azColName[i], "product")) {
      query_data.inquiredData[query_data.dataNum].field = 23;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    } else if (0 == strcmp(azColName[i], "sensor")) {
      query_data.inquiredData[query_data.dataNum].field = 24;
      query_data.inquiredData[query_data.dataNum].data.data_uint8 =
          atoi(argv[i]);
    }
    query_data.dataNum++;
    // LOG_INFO(OUTPOINT, "data_cb:%s = %s\n", azColName[i],
    //          (argv[i] ? argv[i] : "NULL"));
  }
  return 0;
}

int fliter_cb(void *NotUsed, int argc, char **argv, char **azColName) {
  for (int i = 0; i < argc; ++i) {
    if (argv[i] == NULL ||
        filter_data.dataNum >= GW_PRO_MAX_DATA_ITEM_PER_OPERATION)
      continue;
    filter_data.filteredItemIdx[filter_data.dataNum++] = atoi(argv[i]);
    // LOG_DEBUG(OUTPOINT, "argv[%d]:%s\r\n", i, argv[i]);
  }
  return 0;
}
