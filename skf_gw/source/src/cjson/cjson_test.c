/**
 * @file    main.c
 * @author  Xiaoyuan (Sean) Ma (zz2134)
 * @date    2024-04-26
 * @brief   The entrance of cjsonGwAppCoDec
 * @details
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gwConfig.h"
#include "sensorConfig.h"
#include "jsonBuild.h"
#include "jsonParse.h"

// DBG_LOCAL_LOG_DEBUG


#define MAX_JSON_FILE_SIZE_BYTE (5000)

// Just for test
gwConfig_t gwConfig_1;
gwConfig_t gwConfig_2;

sensorConfig_t sensorConfig_1;
sensorConfig_t sensorConfig_2;

#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
static gwConfig_gwConfig_children_t *makeChildrenForGw(
  const char *type,
  const char *manufacturer,
  const char *name,
  const uint32_t nameId,
  const char *bleName,
  macAddr_t *macAddr);
#endif
static void generateGwConfigForTest(gwConfig_t *gwConfig);
static void generateSensorConfigForTest(sensorConfig_t *sensorConfig);

#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
static gwConfig_gwConfig_children_t *
makeChildrenForGw(
  const char *type,
  const char *manufacturer,
  const char *name,
  const uint32_t nameId,
  const char *bleName,
  macAddr_t *macAddr)
{
  gwConfig_gwConfig_children_t *_childrenListNode = NULL;

  _childrenListNode = malloc(sizeof(gwConfig_gwConfig_children_t));
  if(_childrenListNode == NULL)
  {
    // Failed to make a node
    return _childrenListNode;
  }
  strncpy(_childrenListNode->type, type, sizeof(_childrenListNode->type));
  strncpy(_childrenListNode->manufacturer, manufacturer,
          sizeof(_childrenListNode->manufacturer));
  strncpy(_childrenListNode->name, name, sizeof(_childrenListNode->name));
  _childrenListNode->nameId = nameId;
  strncpy(_childrenListNode->bleName, bleName,
          sizeof(_childrenListNode->bleName));
  memcpy(_childrenListNode->macAddr.addr.addrArray,
         macAddr->addr.addrArray,
         sizeof(_childrenListNode->macAddr.addr.addrArray));
  return _childrenListNode;
}
#endif
static void
generateGwConfigForTest(gwConfig_t *gwConfig)
{
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  macAddr_t _tmpMac;
  gwConfig_gwConfig_children_t *_childrenListNode;
#endif

  gwConfig->version = 1;
  gwConfig->lastEditTimeS = 1714270542;
  strncpy(gwConfig->gwConfig.type, "BulletGW",
          sizeof(gwConfig->gwConfig.type));
  strncpy(gwConfig->gwConfig.manufacturer, "SKF",
          sizeof(gwConfig->gwConfig.manufacturer));
  strncpy(gwConfig->gwConfig.name, "FAN zone 1",
          sizeof(gwConfig->gwConfig.name));
  gwConfig->gwConfig.nameId = 123;
  gwConfig->gwConfig.macAddr.addr.addrArray[0] = 0xc4;
  gwConfig->gwConfig.macAddr.addr.addrArray[1] = 0xbd;
  gwConfig->gwConfig.macAddr.addr.addrArray[2] = 0x6a;
  gwConfig->gwConfig.macAddr.addr.addrArray[3] = 0x00;
  gwConfig->gwConfig.macAddr.addr.addrArray[4] = 0x01;
  gwConfig->gwConfig.macAddr.addr.addrArray[5] = 0x02;
  gwConfig->gwConfig.gatewayMode = GWCONFIG_GWCONFIG_GATEWAYMODE_REGULAR;
  gwConfig->gwConfig.bleConfig.ble1TxPower =
    GWCONFIG_BLECONFIG_TXPOWER_NEG_4_DBM;
  gwConfig->gwConfig.bleConfig.ble2TxPower =
    GWCONFIG_BLECONFIG_TXPOWER_0_DBM;
  gwConfig->gwConfig.bleConfig.ble1Antenna =
    GWCONFIG_BLECONFIG_BUILTIN_ANTENNA;
  gwConfig->gwConfig.bleConfig.ble2Antenna =
    GWCONFIG_BLECONFIG_EXTERNAL_ANTENNA;
  gwConfig->gwConfig.bleConfig.bleDuplicatedDrop_ms = 1001;
  gwConfig->gwConfig.timeConfig.needTimestampForRecBeacon = false;
  gwConfig->gwConfig.timeConfig.timeSetting =
    GWCONFIG_TIMECONFIG_TIMESETTING_VIA_NTP;
  strncpy(gwConfig->gwConfig.timeConfig.timeNtpUrl, "ntp1.nim.ac.cn",
          sizeof(gwConfig->gwConfig.timeConfig.timeNtpUrl));
  gwConfig->gwConfig.mqttConfig.mqttSecurityType =
    GWCONFIG_MQTTCONFIG_SECURITYTYPE_USERNAME_PSW;
  strncpy(gwConfig->gwConfig.mqttConfig.MQTTHost, "mqtt-dp.skf4u.com",
          sizeof(gwConfig->gwConfig.mqttConfig.MQTTHost));
  gwConfig->gwConfig.mqttConfig.MQTTPort = 1883,
  gwConfig->gwConfig.ipConfig.DHCPEnable = false,
  gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[0] = 192;
  gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[1] = 168;
  gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[2] = 1;
  gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[3] = 123;
  gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[0] = 255;
  gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[1] = 255;
  gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[2] = 255;
  gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[3] = 0;
  gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[0] = 114;
  gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[1] = 114;
  gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[2] = 114;
  gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[3] = 114;
  gwConfig->gwConfig.childrenNumber = 3;

#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  INIT_LIST_HEAD(&(gwConfig->gwConfig.childrenList));
  _tmpMac.addr.addrArray[0] = 0xc4;
  _tmpMac.addr.addrArray[1] = 0xbd;
  _tmpMac.addr.addrArray[2] = 0x6a;
  _tmpMac.addr.addrArray[3] = 0x12;
  _tmpMac.addr.addrArray[4] = 0x34;
  _tmpMac.addr.addrArray[5] = 0x57;
  if((_childrenListNode =
        makeChildrenForGw("BulletSensor", "SKF", "FAN point #1", 311978255,
                          "FANSEN3457",
                          &_tmpMac)) == NULL)
  {
    printf("Failed to make a node for the children list\n");
  }
  list_add_tail(&(_childrenListNode->childrenNode),
                &(gwConfig->gwConfig.childrenList));
  _tmpMac.addr.addrArray[0] = 0xc4;
  _tmpMac.addr.addrArray[1] = 0xbd;
  _tmpMac.addr.addrArray[2] = 0x6a;
  _tmpMac.addr.addrArray[3] = 0x12;
  _tmpMac.addr.addrArray[4] = 0x34;
  _tmpMac.addr.addrArray[5] = 0x58;
  if((_childrenListNode =
        makeChildrenForGw("BulletSensor", "SKF", "FAN point #2", 311978256,
                          "FANSEN3458",
                          &_tmpMac)) == NULL)
  {
    printf("Failed to make a node for the children list\n");
  }
  list_add_tail(&(_childrenListNode->childrenNode),
                &(gwConfig->gwConfig.childrenList));
  _tmpMac.addr.addrArray[0] = 0xc4;
  _tmpMac.addr.addrArray[1] = 0xbd;
  _tmpMac.addr.addrArray[2] = 0x6a;
  _tmpMac.addr.addrArray[3] = 0x12;
  _tmpMac.addr.addrArray[4] = 0x34;
  _tmpMac.addr.addrArray[5] = 0x59;
  if((_childrenListNode =
        makeChildrenForGw("BulletSensor", "SKF", "FAN point #3", 311978257,
                          "FANSEN3459",
                          &_tmpMac)) == NULL)
  {
    printf("Failed to make a node for the children list\n");
  }
  list_add_tail(&(_childrenListNode->childrenNode),
                &(gwConfig->gwConfig.childrenList));
#else
  strncpy(gwConfig->gwConfig.children[0].type, "BulletSensor",
          sizeof(gwConfig->gwConfig.children[0].type));
  strncpy(gwConfig->gwConfig.children[0].manufacturer, "SKF",
          sizeof(gwConfig->gwConfig.children[0].manufacturer));
  strncpy(gwConfig->gwConfig.children[0].name, "FAN point #1",
          sizeof(gwConfig->gwConfig.children[0].name));
  gwConfig->gwConfig.children[0].nameId = 311978255,
  strncpy(gwConfig->gwConfig.children[0].bleName, "FANSEN3457",
          sizeof(gwConfig->gwConfig.children[0].bleName));
  gwConfig->gwConfig.children[0].macAddr.addr.addrArray[0] = 0xc4;
  gwConfig->gwConfig.children[0].macAddr.addr.addrArray[1] = 0xbd;
  gwConfig->gwConfig.children[0].macAddr.addr.addrArray[2] = 0x6a;
  gwConfig->gwConfig.children[0].macAddr.addr.addrArray[3] = 0x12;
  gwConfig->gwConfig.children[0].macAddr.addr.addrArray[4] = 0x34;
  gwConfig->gwConfig.children[0].macAddr.addr.addrArray[5] = 0x57;
  strncpy(gwConfig->gwConfig.children[1].type, "BulletSensor",
          sizeof(gwConfig->gwConfig.children[1].type));
  strncpy(gwConfig->gwConfig.children[1].manufacturer, "SKF",
          sizeof(gwConfig->gwConfig.children[1].manufacturer));
  strncpy(gwConfig->gwConfig.children[1].name, "FAN point #2",
          sizeof(gwConfig->gwConfig.children[1].name));
  gwConfig->gwConfig.children[1].nameId = 311978255,
  strncpy(gwConfig->gwConfig.children[1].bleName, "FANSEN3458",
          sizeof(gwConfig->gwConfig.children[1].bleName));
  gwConfig->gwConfig.children[1].macAddr.addr.addrArray[0] = 0xc4;
  gwConfig->gwConfig.children[1].macAddr.addr.addrArray[1] = 0xbd;
  gwConfig->gwConfig.children[1].macAddr.addr.addrArray[2] = 0x6a;
  gwConfig->gwConfig.children[1].macAddr.addr.addrArray[3] = 0x12;
  gwConfig->gwConfig.children[1].macAddr.addr.addrArray[4] = 0x34;
  gwConfig->gwConfig.children[1].macAddr.addr.addrArray[5] = 0x58;
  strncpy(gwConfig->gwConfig.children[2].type, "BulletSensor",
          sizeof(gwConfig->gwConfig.children[2].type));
  strncpy(gwConfig->gwConfig.children[2].manufacturer, "SKF",
          sizeof(gwConfig->gwConfig.children[2].manufacturer));
  strncpy(gwConfig->gwConfig.children[2].name, "FAN point #3",
          sizeof(gwConfig->gwConfig.children[2].name));
  gwConfig->gwConfig.children[2].nameId = 311978255,
  strncpy(gwConfig->gwConfig.children[2].bleName, "FANSEN3459",
          sizeof(gwConfig->gwConfig.children[2].bleName));
  gwConfig->gwConfig.children[2].macAddr.addr.addrArray[0] = 0xc4;
  gwConfig->gwConfig.children[2].macAddr.addr.addrArray[1] = 0xbd;
  gwConfig->gwConfig.children[2].macAddr.addr.addrArray[2] = 0x6a;
  gwConfig->gwConfig.children[2].macAddr.addr.addrArray[3] = 0x12;
  gwConfig->gwConfig.children[2].macAddr.addr.addrArray[4] = 0x34;
  gwConfig->gwConfig.children[2].macAddr.addr.addrArray[5] = 0x59;
#endif
}
static void
generateSensorConfigForTest(sensorConfig_t *sensorConfig)
{
  sensorConfig->version = 1;
  sensorConfig->lastEditTimeS = 1714270542;
  sensorConfig->haveConfiguredToSensor = true;
  sensorConfig->whenConfiguredToSensor = 1714270543;
  strncpy(sensorConfig->type,
          "BulletSensor",
          sizeof(sensorConfig->type));
  strncpy(sensorConfig->manufacturer,
          "SKF",
          sizeof(sensorConfig->manufacturer));
  sensorConfig->nameId = 1234;
  sensorConfig->macAddr.addr.addrArray[0] = 0xc4;
  sensorConfig->macAddr.addr.addrArray[1] = 0xbd;
  sensorConfig->macAddr.addr.addrArray[2] = 0x6a;
  sensorConfig->macAddr.addr.addrArray[3] = 0x12;
  sensorConfig->macAddr.addr.addrArray[4] = 0x34;
  sensorConfig->macAddr.addr.addrArray[5] = 0x59;
  sensorConfig->bulletSensorConfig.sysConfig.sensorMode =
    BULLET_SENSOR_MODE_NORMAL;
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
  sensorConfig->bulletSensorConfig.sysConfig.sensorModeParameter =
    0;
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
  sensorConfig->bulletSensorConfig.sysConfig.batteryAlarmThrePercent = 20;
  sensorConfig->bulletSensorConfig.sysConfig.currentTimeS = 1709958366;
  sensorConfig->bulletSensorConfig.sysConfig.txPowerAdv =
    SYSCONFIG_BLETXPOWER_NEG_4_DBM;
  sensorConfig->bulletSensorConfig.sysConfig.dataRateAuxAdv =
    SYSCONFIG_BLEDATARATE_2MBPS;
  sensorConfig->bulletSensorConfig.sysConfig.txPowerAuxAdv =
    SYSCONFIG_BLETXPOWER_NEG_8_DBM;
  sensorConfig->bulletSensorConfig.schConfig.period_quickPolling_s = 0;
  sensorConfig->bulletSensorConfig.schConfig.refTime_quickPolling_s = 0;
  sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s =
    28800;
  sensorConfig->bulletSensorConfig.schConfig.refTime_regularSensing_s = 0;
  sensorConfig->bulletSensorConfig.schConfig.period_comm_s = 28800;
  sensorConfig->bulletSensorConfig.schConfig.refTime_period_comm_s = 0;
  sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz = 3200;
  sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_n = 3200;
  sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_axis_acq_eval =
    SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X;
  sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_range =
    SENSINGCONFIG_VIBRANGE_16GEE;
  sensorConfig->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz = 1000;
  sensorConfig->bulletSensorConfig.senConfig.pre_acq_mag_n = 1000;
  sensorConfig->bulletSensorConfig.senConfig.pre_acq_mag_axis_acq_eval =
    SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y;
  sensorConfig->bulletSensorConfig.senConfig.fAccHz = 32000;
  sensorConfig->bulletSensorConfig.senConfig.nAcc = 64000;
  sensorConfig->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval =
    SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X;
  sensorConfig->bulletSensorConfig.senConfig.acq_vib_range =
    SENSINGCONFIG_VIBRANGE_16GEE;
  sensorConfig->bulletSensorConfig.senConfig.acq_mag_fs_hz = 1000;
  sensorConfig->bulletSensorConfig.senConfig.acq_mag_n = 3000;
  sensorConfig->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval =
    SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y;
  sensorConfig->bulletSensorConfig.senConfig.fs_coef = 3.2;
  sensorConfig->bulletSensorConfig.algoConfig.GEE_COEF = 9.81;
  sensorConfig->bulletSensorConfig.algoConfig.V_COEF = 1000;
  sensorConfig->bulletSensorConfig.algoConfig.MEAS_POSITION = 1;
  sensorConfig->bulletSensorConfig.algoConfig.MEAS_LOAD = 0;
  sensorConfig->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB =
    SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X;
  sensorConfig->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG =
    SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y;
  sensorConfig->bulletSensorConfig.algoConfig.VIB_START_FG = true;
  sensorConfig->bulletSensorConfig.algoConfig.VIB_RMS_START_TL = 0.002;
  sensorConfig->bulletSensorConfig.algoConfig.MAG_START_FG = true;
  sensorConfig->bulletSensorConfig.algoConfig.MAG_RMS_START_TL = 0.1;
  sensorConfig->bulletSensorConfig.algoConfig.MAG_STABLE_FG = true;
  sensorConfig->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH = 0.05;
  sensorConfig->bulletSensorConfig.algoConfig.RPM_VAR_RANGE = 0.1;
  sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M = 3;
  sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N = 6;
  sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_TEMP =
    20;
  sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M = 3;
  sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N = 5;
  sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB = 5;
  sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG = 5;
  sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP = true;
  sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_OV = true;
  sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH = false;
  sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG = false;
  sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB = false;
  sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR = false;
  sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR = false;
  sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN = false;
  sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP = false;
  sensorConfig->bulletSensorConfig.algoConfig.ASSET_LEVEL = 1;
  sensorConfig->bulletSensorConfig.algoConfig.FLEX_TYPE = 2;
  sensorConfig->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM = 0;
  sensorConfig->bulletSensorConfig.algoConfig.RUN_SPEED_RPM = 0;
  sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_BPFO = 3.584;
  sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_BPFI = 5.416;
  sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_BSF = 4.7105;
  sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_FTF = 0.398;
  sensorConfig->bulletSensorConfig.algoConfig.MTR_INFO_FL = 50;
  sensorConfig->bulletSensorConfig.algoConfig.MTR_INFO_BAR = 10;
  sensorConfig->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH = 17;
  sensorConfig->bulletSensorConfig.algoConfig.FAN_INFO_BLADE = 2;
  sensorConfig->bulletSensorConfig.algoConfig.PUMP_INFO_VANE = 2;
  sensorConfig->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE = 60;
  sensorConfig->bulletSensorConfig.algoConfig.AlarmThrestemp = 80;
  sensorConfig->bulletSensorConfig.algoConfig.ACC_OV_ALERT = 0.2;
  sensorConfig->bulletSensorConfig.algoConfig.ACC_OV_ALARM = 0.5;
  sensorConfig->bulletSensorConfig.algoConfig.ACC_HAL_ALERT = 0.2;
  sensorConfig->bulletSensorConfig.algoConfig.ACC_HAL_ALARM = 0.5;
  sensorConfig->bulletSensorConfig.algoConfig.VEL_OV_ALERT = 0.2;
  sensorConfig->bulletSensorConfig.algoConfig.VEL_OV_ALARM = 0.5;
  sensorConfig->bulletSensorConfig.algoConfig.VEL_HAL_ALERT = 0.2;
  sensorConfig->bulletSensorConfig.algoConfig.VEL_HAL_ALARM = 0.5;
  sensorConfig->bulletSensorConfig.algoConfig.ENV_OV_ALERT = 0.2;
  sensorConfig->bulletSensorConfig.algoConfig.ENV_OV_ALARM = 1.5;
  sensorConfig->bulletSensorConfig.algoConfig.ENV_HAL_ALERT = 0.15;
  sensorConfig->bulletSensorConfig.algoConfig.ENV_HAL_ALARM = 0.3;
}
int test_for_cjson(void)
{
  char *json;
  FILE *fp;

  printf("cJSON test case for GW application ...\n");

  json = malloc(MAX_JSON_FILE_SIZE_BYTE);
  printf("generate gwConfig.json string based on gwConfig_1 ...\n");
  generateGwConfigForTest(&gwConfig_1);
  generateGwConfigJson(&gwConfig_1, json, NULL);
  printf("%s", json);
  fp = fopen("outputGwConfig_1.json", "a+");
  if(fp == NULL)
  {
    perror("Error: Failed to open outputGwConfig_1.json\n");
    return -1;
  }
  fseek(fp, 0, SEEK_SET);
  fwrite(json, strlen(json), 1, fp);
  fclose(fp);
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  INIT_LIST_HEAD(&(gwConfig_2.gwConfig.childrenList));
#endif
  parseGwConfigJson(json, &gwConfig_2, NULL);
  printf("generate gwConfig.json string based on gwConfig_2 ...\n");
  generateGwConfigJson(&gwConfig_2, json, NULL);
  printf("%s", json);
  fp = fopen("outputGwConfig_2.json", "a+");
  if(fp == NULL)
  {
    perror("Error: Failed to open outputGwConfig_2.json\n");
    return -1;
  }
  fseek(fp, 0, SEEK_SET);
  fwrite(json, strlen(json), 1, fp);
  fclose(fp);

  printf("generate sensorConfig.json string based on sensorConfig_1 ...\n");
  generateSensorConfigForTest(&sensorConfig_1);
  generateSensorConfigJson(&sensorConfig_1, json, NULL);
  printf("%s", json);
  fp = fopen("outputSensorConfig_1.json", "a+");
  if(fp == NULL)
  {
    perror("Error: Failed to open outputSensorConfig_1.json\n");
    return -1;
  }
  fseek(fp, 0, SEEK_SET);
  fwrite(json, strlen(json), 1, fp);
  fclose(fp);
  parseSensorConfigJson(json, &sensorConfig_2, NULL);
  printf("generate sensorConfig.json string based on sensorConfig_2 ...\n");
  generateSensorConfigJson(&sensorConfig_2, json, NULL);
  printf("%s", json);
  fp = fopen("outputSensorConfig_2.json", "a+");
  if(fp == NULL)
  {
    perror("Error: Failed to open outputSensorConfig_2.json\n");
    return -1;
  }
  fseek(fp, 0, SEEK_SET);
  fwrite(json, strlen(json), 1, fp);
  fclose(fp);

  free(json);
  return 0;
}
