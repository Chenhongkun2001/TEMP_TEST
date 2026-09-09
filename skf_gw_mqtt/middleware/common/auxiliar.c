#include "auxiliar.h"
#include "global.h"
#include "stdlib.h"
#include "string.h"
#include <ppGW.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
DBG_LOCAL_LOG_DEBUG

configure_content_t GatewayConfig[] = {
    {
        "INT PRIMARY KEY DEFAULT 0",
        "nameId",
    },
    {
        "TEXT DEFAULT ' '",
        "name",
    },
    {
        "TEXT DEFAULT ' '",
        "macAddr",
    },
    {
        "TEXT DEFAULT 'SKF'",
        "manufacturer",
    },
    {
        "INT DEFAULT 0",
        "ApplicationVersion",
    },
    {
        "INT DEFAULT 0",
        "LinuxKernelVersion",
    },
    {
        "INT DEFAULT 0",
        "version",
    },
    {
        "INT DEFAULT 0",
        "lastEditTime",
    },
    {
        "TEXT DEFAULT ' '",
        "type",
    },
    {
        "INT DEFAULT 0",
        "gatewayMode",
    },
    {
        "INT DEFAULT 0",
        "BLE1TxPower",
    },
    {
        "INT DEFAULT 0",
        "BLE2TxPower",
    },
    {
        "INT DEFAULT 0",
        "BLE1Antenna",
    },
    {
        "INT DEFAULT 0",
        "BLE2Antenna",
    },
    {
        "INT DEFAULT 0",
        "BLEFilterListNameNumber",
    },
    {
        "INT DEFAULT 0",
        "BLEDuplicatedDrop",
    },
    {
        "BOOLEAN DEFAULT false",
        "timeStamp",
    },
    {
        "INT DEFAULT 0",
        "timeSetting",
    },
    {
        "TEXT DEFAULT ' '",
        "timeNTPUrl",
    },
    {
        "INT DEFAULT 0",
        "MQTTSecurityType",
    },
    {
        "TEXT DEFAULT ' '",
        "MQTTHost",
    },
    {
        "INT DEFAULT 0",
        "MQTTPort",
    },
    {
        "BOOLEAN DEFAULT false",
        "DHCPEnable",
    },
    {
        "TEXT DEFAULT ' '",
        "staticIPAddr",
    },
    {
        "TEXT DEFAULT ' '",
        "netMask",
    },
    {
        "TEXT DEFAULT ' '",
        "DNSServer",
    },
    {
        "TEXT DEFAULT ' '",
        "gatewayAddr",
    },
#if (ENABLE_MODBUS_FEATURE == 1)
    {
        "INT DEFAULT 115200",
        "baudrateSlave",
    },
    {
        "INT DEFAULT 0",
        "slaveAddrSlave",
    },
    {
        "INT DEFAULT 0",
        "paritySlave",
    },
    {
        "INT DEFAULT 0",
        "stopSlave",
    },
    {
        "INT DEFAULT 0",
        "registerMode",
    },
    {
        "TEXT DEFAULT ' '",
        "description",
    },
#endif
};

configure_content_t DeviceList[] = {
    {
        "INTEGER PRIMARY KEY AUTOINCREMENT",
        "nameId",
    },
    {
        "TEXT DEFAULT ' '",
        "name",
    },
    {
        "TEXT DEFAULT ' ' ",
        "BLESensorName",
    },
    {
        "TEXT DEFAULT ' '",
        "macAddr",
    },
    {
        "TEXT DEFAULT 'SKF'",
        "manufacturer",
    },
    {
        "TEXT DEFAULT 'BulletSensor'",
        "type",
    },
#if (ENABLE_MODBUS_FEATURE == 1)
    {
        "TEXT DEFAULT ' '",
        "description",
    },
#endif
};
configure_content_t SensorConfig[] = {
    {
        "INT PRIMARY KEY DEFAULT 0",
        "nameId",
    },
    {
        "TEXT DEFAULT ' '",
        "name",
    },
    {
        "TEXT DEFAULT ' '",
        "BLESensorName",
    },
    {
        "TEXT DEFAULT ' '",
        "macAddr",
    },
    {
        "TEXT DEFAULT 'SKF'",
        "manufacturer",
    },
    {
        "TEXT DEFAULT ' '",
        "FirmwareVersion",
    },
    {
        "INT DEFAULT 0",
        "version",
    },
    {
        "INT DEFAULT 0",
        "lastEditTime",
    },
    {
        "BOOLEAN DEFAULT false",
        "haveConfiguredToSensor",
    },
    {
        "INT DEFAULT 0",
        "whenConfiguredToSensor",
    },
    {
        "TEXT DEFAULT ' '",
        "type",
    },
    {
        "INT DEFAULT 0",
        "sensorMode",
    },
    {
        "REAL DEFAULT 0.0",
        "batteryAlarmThreshold",
    },
    {
        "INT DEFAULT 0",
        "txPower_adv",
    },
    {
        "INT DEFAULT 0",
        "dataRate_auxAdv",
    },
    {
        "INT DEFAULT 0",
        "txPower_auxAdv",
    },
    {
        "INT DEFAULT 0",
        "period_quickPolling",
    },
    {
        "INT DEFAULT 0",
        "referenceTime_quickPolling",
    },
    {
        "INT DEFAULT 0",
        "period_regularSensing",
    },
    {
        "INT DEFAULT 0",
        "referenceTime_regularSensing",
    },
    {
        "INT DEFAULT 0",
        "period_comm",
    },
    {
        "INT DEFAULT 0",
        "referenceTime_period_comm",
    },
    {
        "INT DEFAULT 0",
        "PRE_ACQ_VIB_FS_HZ",
    },
    {
        "INT DEFAULT 0",
        "PRE_ACQ_VIB_N",
    },
    {
        "INT DEFAULT 0",
        "PRE_ACQ_VIB_AXIS_ACQ_EVAL",
    },
    {
        "INT DEFAULT 0",
        "PRE_ACQ_VIB_RANGE",
    },
    {
        "INT DEFAULT 0",
        "PRE_ACQ_MAG_FS_HZ",
    },
    {
        "INT DEFAULT 0",
        "PRE_ACQ_MAG_N",
    },
    {
        "INT DEFAULT 0",
        "PRE_ACQ_MAG_AXIS_ACQ_EVAL",
    },
    {
        "INT DEFAULT 0",
        "Facc",
    },
    {
        "INT DEFAULT 0",
        "Nacc",
    },
    {
        "INT DEFAULT 0",
        "ACQ_VIB_AXIS_ACQ_EVAL",
    },
    {
        "INT DEFAULT 0",
        "ACQ_VIB_RANGE",
    },
    {
        "INT DEFAULT 0",
        "ACQ_MAG_FS_HZ",
    },
    {
        "INT DEFAULT 0",
        "ACQ_MAG_N",
    },
    {
        "INT DEFAULT 0",
        "ACQ_MAG_AXIS_ACQ_EVAL",
    },
    {
        "REAL DEFAULT 0.364",
        "FS_COEF",
    },
    {
        "REAL DEFAULT 0.0",
        "GEE_COEF",
    },
    {
        "REAL DEFAULT 0.0",
        "V_COEF",
    },
    {
        "INT DEFAULT 0",
        "MEAS_POSITION",
    },
    {
        "INT DEFAULT 0",
        "MEAS_LOAD",
    },
    {
        "INT DEFAULT 0",
        "MEAS_AXIS_VIB",
    },
    {
        "INT DEFAULT 0",
        "MEAS_AXIS_MAG",
    },
    {
        "BOOLEAN DEFAULT false",
        "VIB_START_FG",
    },
    {
        "REAL DEFAULT 0.0",
        "VIB_RMS_START_TL",
    },
    {
        "BOOLEAN DEFAULT false",
        "MAG_START_FG",
    },
    {
        "REAL DEFAULT 0.0",
        "MAG_RMS_START_TL",
    },
    {
        "BOOLEAN DEFAULT false",
        "MAG_STABLE_FG",
    },
    {
        "REAL DEFAULT 0.0",
        "MAG_RMS_VAR_TH",
    },
    {
        "REAL DEFAULT 0.0",
        "RPM_VAR_RANGE",
    },
    {
        "INT DEFAULT 0",
        "DEC_LOGIC_TEMP_M",
    },
    {
        "INT DEFAULT 0",
        "DEC_LOGIC_TEMP_N",
    },
    {
        "INT DEFAULT 0",
        "DEC_LOGIC_LEARN_NUM_TEMP",
    },
    {
        "INT DEFAULT 0",
        "DEC_LOGIC_VIB_M",
    },
    {
        "INT DEFAULT 0",
        "DEC_LOGIC_VIB_N",
    },
    {
        "INT DEFAULT 0",
        "DEC_LOGIC_LEARN_NUM_VIB",
    },
    {
        "INT DEFAULT 0",
        "DEC_LOGIC_LEARN_NUM_MAG",
    },
    {
        "BOOLEAN DEFAULT false",
        "FUNC_ANOM_TEMP",
    },
    {
        "BOOLEAN DEFAULT false",
        "FUNC_ANOM_OV",
    },
    {
        "BOOLEAN DEFAULT false",
        "FUNC_ANOM_MECH",
    },
    {
        "BOOLEAN DEFAULT false",
        "FUNC_ANOM_BRG",
    },
    {
        "BOOLEAN DEFAULT false",
        "FUNC_ANOM_LUB",
    },
    {
        "BOOLEAN DEFAULT false",
        "FUNC_ANOM_MTR",
    },
    {
        "BOOLEAN DEFAULT false",
        "FUNC_ANOM_GEAR",
    },
    {
        "BOOLEAN DEFAULT false",
        "FUNC_ANOM_FAN",
    },
    {
        "BOOLEAN DEFAULT false",
        "FUNC_ANOM_PUMP",
    },
    {
        "INT DEFAULT 0",
        "ASSET_LEVEL",
    },
    {
        "INT DEFAULT 0",
        "FLEX_TYPE",
    },
    {
        "REAL DEFAULT 0.0",
        "BORE_DIAMETER_MM",
    },
    {
        "REAL DEFAULT 0.0",
        "RUN_SPEED_RPM",
    },
    {
        "REAL DEFAULT 0.0",
        "BRG_INFO_BPFO",
    },
    {
        "REAL DEFAULT 0.0",
        "BRG_INFO_BPFI",
    },
    {
        "REAL DEFAULT 0.0",
        "BRG_INFO_BSF",
    },
    {
        "REAL DEFAULT 0.0",
        "BRG_INFO_FTF",
    },
    {
        "INT DEFAULT 0",
        "MTR_INFO_FL",
    },
    {
        "INT DEFAULT 0",
        "MTR_INFO_BAR",
    },
    {
        "INT DEFAULT 0",
        "GEAR_INFO_TOOTH",
    },
    {
        "INT DEFAULT 0",
        "FAN_INFO_BLADE",
    },
    {
        "INT DEFAULT 0",
        "PUMP_INFO_VANE",
    },
    {
        "REAL DEFAULT 0.0",
        "TEMP_OV_ALERT_CDEGREE",
    },
    {
        "INT DEFAULT 0",
        "AlarmThrestemp",
    },
    {
        "REAL DEFAULT 0.0",
        "ACC_OV_ALERT",
    },
    {
        "REAL DEFAULT 0.0",
        "ACC_OV_ALARM",
    },
    {
        "REAL DEFAULT 0.0",
        "ACC_HAL_ALERT",
    },
    {
        "REAL DEFAULT 0.0",
        "ACC_HAL_ALARM",
    },
    {
        "REAL DEFAULT 0.0",
        "VEL_OV_ALERT",
    },
    {
        "REAL DEFAULT 0.0",
        "VEL_OV_ALARM",
    },
    {
        "REAL DEFAULT 0.0",
        "VEL_HAL_ALERT",
    },
    {
        "REAL DEFAULT 0.0",
        "VEL_HAL_ALARM",
    },
    {
        "REAL DEFAULT 0.0",
        "ENV_OV_ALERT",
    },
    {
        "REAL DEFAULT 0.0",
        "ENV_OV_ALARM",
    },
    {
        "REAL DEFAULT 0.0",
        "ENV_HAL_ALERT",
    },
    {
        "REAL DEFAULT 0.0",
        "ENV_HAL_ALARM",
    },
    {
        "INT DEFAULT 0",
        "waveDataAcqPeriod",
    },
    {
        "INT DEFAULT 0",
        "waveDataAcqReference",
    },
    {
        "INT DEFAULT 0",
        "FUOTA_Version",
    },
    {
        "TEXT DEFAULT ' '",
        "FUOTA_Filename",
    },
#if (ENABLE_MODBUS_FEATURE == 1)
    // {
    //     "TEXT DEFAULT ' '",
    //     "description",
    // },
#endif
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
    {
        "INT DEFAULT 0",
        "sensorModeParameter",
    },
#endif
};
configure_content_t SensorData[] = {
    {
        "INTEGER PRIMARY KEY AUTOINCREMENT",
        "SequenceNumber",
    },
    {
        "INT DEFAULT 0",
        "nameId",
    },
    {
        "TEXT DEFAULT ' '",
        "BLESensorName",
    },
    {
        "TEXT DEFAULT ' '",
        "macAddr",
    },
    {
        "TEXT DEFAULT ' '",
        "type",
    },
    {
        "TEXT DEFAULT 'SKF'",
        "manufacturer",
    },
    {
        "INT DEFAULT 0",
        "measureseqno",
    },
    {
        "INT DEFAULT 0",
        "sampletime",
    },
    {
        "INT DEFAULT 0",
        "ReceivedTimestamp",
    },
    {
        "INT DEFAULT 0",
        "measurement",
    },
    {
        "INT DEFAULT 0",
        "DataType",
    },
    {
        "TEXT DEFAULT ' '",
        "Format",
    },
    {
        "INT DEFAULT 0",
        "range",
    },
    {
        "INT DEFAULT 0",
        "unit",
    },
    {
        "INT DEFAULT 0",
        "measurelengthsample",
    },
    {
        "INT DEFAULT 0",
        "totaldatalengthsample",
    },
    {
        "INT DEFAULT 0",
        "dimension",
    },
    {
        "INT DEFAULT 0",
        "dataformat",
    },
    {
        "INT DEFAULT 0",
        "sampleperiods",
    },
    {
        "INT DEFAULT 0",
        "encryp",
    },
    {
        "TEXT DEFAULT ' '",
        "value",
    },
    {
        "REAL DEFAULT 0.0",
        "sample_rate",
    },
    {
        "INT DEFAULT 0",
        "product",
    },
    {
        "INT DEFAULT 0",
        "sensor",
    },
    {
        "BOOLEAN DEFAULT false",
        "Sent",
    },
};

uint8_t send_msg(int qid, struct msgbuf *msg, size_t __msgsz) {
  if (msgsnd(qid, msg, __msgsz, 0) == -1) {
    LOG_ERR(OUTPOINT, "msgsnd error\r\n");
    return 1;
  }
  LOG_INFO(OUTPOINT, "sent ok!\r\n");
  return 0;
}
uint8_t recv_msg(int qid, struct msgbuf *msg, uint8_t msgtyp) {
  int msgbyte = msgrcv(qid, msg, MSG_BUF_LEN, msgtyp, 0);
  if (msgbyte == -1) {
    LOG_WARN(OUTPOINT, "Get the msg without data!\r\n");
    return 1;
  }
  return 0;
}
uint8_t recv_msg1(int qid, struct msgbuf *msg, uint8_t msgtyp) {
  int msgbyte = msgrcv(qid, msg, MSG_BUF_LEN, msgtyp, IPC_NOWAIT);
  if (msgbyte == -1) {
    LOG_WARN(OUTPOINT, "Get the msg without data!\r\n");
    return 1;
  }
  return 0;
}

void Calculate_query_data_size(char *data, uint32_t *size) {
  //   *size = 0;
  // if (0 == strncmp(data, "6", 1) || 0 == strncmp(data, "5", 1)) {
  //   size++;
  // } else if () {
  //   size++;
  // }
}

uint8_t MYULT_EncodeVersion(char *string, uint32_t *version) {

  uint8_t temp[10] = {0};
  uint32_t encoded_version;

  /* major */
  char *start = string;
  char *end = NULL;
  char *wanted = ".";
  if (NULL == (end = strstr(start, wanted))) {
    return 1;
  }
  memset(temp, 0, sizeof(temp));
  memcpy(temp, start, end - start);
  encoded_version = ((atoi((char *)temp)) & 0xffff) << 16; // 24
  /* minor */
  start = end + strlen(wanted);
  if (NULL == (end = strstr(start, wanted))) {
    return 1;
  }
  memset(temp, 0, sizeof(temp));
  memcpy(temp, start, end - start);
  encoded_version += ((atoi((char *)temp)) & 0xff) << 8; // 16
  /* reversion */
  start = end + strlen(wanted);
  encoded_version += ((atoi(start)) & 0xff);

  *version = encoded_version;

  return 0;
}
uint8_t Gain_clientID(char *macaddr, char *clientID) {
  char *p;
  char tempstr[50] = {0};
  snprintf(tempstr, sizeof(tempstr) - 1, "%s", macaddr);
  p = strtok(tempstr, "-");
  while (p != NULL) {
    strcat(clientID, p);
    p = strtok(NULL, "-");
  }
  return 0;
}
uint8_t Gain_passwd(char *clientID, char *passwd, uint8_t len) {
  char temp[2] = {0};
  char tempstr[50] = {0};
  uint8_t idlen = 0;
  snprintf(tempstr, sizeof(tempstr) - 1, "%s", clientID);
  idlen = strlen(tempstr);
  temp[0] = tempstr[idlen - 3];
  temp[1] = tempstr[idlen - 4];
  tempstr[idlen - 3] = tempstr[idlen - 1];
  tempstr[idlen - 4] = tempstr[idlen - 2];
  tempstr[idlen - 1] = temp[0];
  tempstr[idlen - 2] = temp[1];
  snprintf(passwd, len, "%s", tempstr);
  return 0;
}

void gain_tablename(uint32_t tb, char *table_name, uint16_t len) {
  switch (tb) {
  case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE: {
    snprintf(table_name, len, "%s", GATEWAY_CONFIGURE_TABLE);
  } break;
  case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE: {
    snprintf(table_name, len, "%s", DEVICE_LIST_TABLE);
  } break;
  case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE: {
    snprintf(table_name, len, "%s", SENSOR_CONFIGURE_TABLE);
  } break;
  case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE: {
    snprintf(table_name, len, "%s", SENSOR_DATA_TABLE);
  } break;
  }
}
uint32_t revert_tablename(char *table_name) {
  if (0 == strcmp(table_name, GATEWAY_CONFIGURE_TABLE))
    return GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
  else if (0 == strcmp(table_name, DEVICE_LIST_TABLE))
    return GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  else if (0 == strcmp(table_name, SENSOR_CONFIGURE_TABLE))
    return GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE;
  else if (0 == strcmp(table_name, SENSOR_DATA_TABLE))
    return GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
  return 0;
}

int findMax(int arr[], int low, int high) {
  if (low == high) {
    return arr[low];
  }

  int mid = low + (high - low) / 2;

  if (arr[mid] < arr[mid + 1]) {
    return findMax(arr, mid + 1, high);
  } else {
    return findMax(arr, low, mid);
  }
}

// void int_to_str(int64_t num, char *mystr) {
//   if(num < 0)
//   mystr[i++] ="-";

// }
// void uint_to_str(uint64_t num, char *mystr) {}

void get_curr_time(char *buf, uint8_t buf_len) {
  time_t t = time(NULL) + COVER_TO_BEIJING_TIME;
  struct tm info;
  if(localtime_r(&t, &info) != NULL)
  {
    strftime(buf, buf_len, "%Y-%m-%d %H:%M:%S", &info);
  }
}