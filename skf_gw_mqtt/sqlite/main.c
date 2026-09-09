/**
 * @file    main.c
 * @brief   main process of sql(insert, update, filter, & delete operations).
 * @details :
 * history :
 *         1). [2025-04-25 v1.0.8]set baudrate=9600, slaveaddr=1 by default
 *             in case they are not available in 'gateway' table.
 * 
 */

#include "ipcmbs.h"
#include <createtable.h>
#include <delete.h>
#include <global.h>
#include <insert.h>
#include <misc.h>
#include <ppGW.h>
#include <query.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>
#include <update.h>
// #define ENABLE_MODBUS_FEATURE (1)
#if (ENABLE_MODBUS_FEATURE == 1)
#include "sh_mem.h"
// #include "sys_def.h"
#include <signal.h>
#include <time.h>
#include <errno.h>
#endif
DBG_LOCAL_LOG_DEBUG

int sql_msgid = 0;
int mqtt_msgid = 0;
int ble_msgid = 0;
struct msgbuf msgbufp = {0};
static char table_name[30] = {0};
extern bool testflag;
extern char filename[100];
gw_pro_command_sqlite_query_item_response_t query_data = {0};
gw_pro_command_sqlite_filter_response_t filter_data = {0};
bool itemfalg = false;
static bool exeflag = false;
uint8_t global_tb_name = 0;
char gwconfigmac[50] = {0};
uint32_t gwconfigid = 0;
gw_pro_process_type_t set_from_process = gw_pro_process_sqlite;
uint64_t autoid = 0;
#if (ENABLE_MODBUS_FEATURE == 1)
// Notes: these 2 macroes are copied from skf_gw/sys_def.h file. consider to put them into somewhere else later.
// Supported sensors
#ifndef TYPE_STR_INSIGHT_T
#define TYPE_STR_INSIGHT_T	"Insight-T"
#endif
#ifndef TYPE_STR_INSIGHT_P
#define TYPE_STR_INSIGHT_P	"PredictSensor"
#endif
#ifndef TYPE_STR_INSIGHT_P_PRO
#define TYPE_STR_INSIGHT_P_PRO	"PredictSensorPro"
#endif

appShmDataItem_t shm_data = {0};  // for 'gateway' & 'dev'
sensorDataTableData_t shm_sensor_data = {0};  // for 'sensordata'
void *shm_addr = NULL;
/* add 1 into count for that of gateway */
chn_nameid_mapping_t chn_nameid_map[BULLET_GW_SENSOR_NUM_MAX + 1] = {0};
// uint32_t nameid_list[BULLET_GW_SENSOR_NUM_MAX] = {0};
uint8_t assigned_chn_cnt = 0;
bool modbus_feature_enable = false;
key_t shm_key = -1;
uint8_t g_former_ch_idx = 0;
uint8_t g_current_ch_idx = 0;
uint32_t g_former_nameid = 0;
uint8_t g_sensor_data_piece_cnt = 0;
// appShmDataItem_t g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX] = {0};
sensorDataTableData_t g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX] = {0};
timer_t g_timer_id = NULL;
uint8_t g_sensor_data_piece_max = 0;
/*
 * add more variables to differentiate the sensor data of insight-P
 * as they(sensordata of insight-P) may be 'disturbed' by sensordata of insight-T
 */
#if (1)
//to be replaced by new added
uint8_t g_former_ch_idx_p = 0;
uint8_t g_current_ch_idx_p =0; //may not necessary. to be confirmed
uint32_t g_former_nameid_p = 0;
uint8_t g_sensor_data_piece_cnt_p = 0;
// appShmDataItem_t g_shm_data_buf_p[APP_SHM_SENSORDATA_TABLE_PIECE_MAX] = {0};
sensorDataTableData_t g_shm_data_buf_p[APP_SHM_SENSORDATA_TABLE_PIECE_MAX] = {0};
timer_t g_timer_id_p = NULL;
uint8_t g_sensor_data_piece_max_p = 0;
sensorType_t g_sensor_type_u8 = MODBUS_SENSOR_TYPE_T; // MODBUS_SENSOR_TYPE_T -- insight-T; 1 -- insight-P
// new added
#define CONFIG_BLE_CONNECTION_MAX_NUM 4 // to be resolved. 
// double the number of CONFIG_BLE_CONNECTION_MAX_NUM
#define CONFIG_SHM_SENSOR_DATA_MAX_NUM (CONFIG_BLE_CONNECTION_MAX_NUM * 2)
uint8_t g_sd_idx = 0; // fixed for each piece of sensor data
appShmSensorDataInstance_t g_sd_instance[CONFIG_SHM_SENSOR_DATA_MAX_NUM] = {0};
#endif
/* end of new variable addition*/
// more variables for efficient mode
efficient_chn_nameid_mapping_t efficient_chn_nameid_map[BULLET_GW_SENSOR_NUM_MAX + 1] = {0};

/*
 * register modes for sqlite to simplify identifying the reg mode.
 * Notes: DO NOT use these MACROes for regMode in sharememory
 */
typedef enum _regAddressModeSql {   /* 0-1 used */
  MODBUS_REG_ADDR_EXTENSIBLE_MODE = 0,
  MODBUS_REG_ADDR_EFFICIENT_MODE,
  MODBUS_REG_ADDR_MODE_MAX
} regAddressModeSql_t;
regAddressModeSql_t g_modbus_reg_addr_mode = MODBUS_REG_ADDR_EXTENSIBLE_MODE;
/*
 * An array to map 'each piece of sensorData's measurementType' into the 'index' of sd_data_buf in 'appShmSensorDataInstance_t'
 * Notes:
 *  1. it's for sensorData only;
 *  2. 'APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1' is reserved for reg bit-map. MUST NOT use this index # in this array please;
 *  3. 'APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2' is also reserved, for items 'not available' or we don't care;
 *  4. define 2 more macroes 'SHM_SD_IDX_LAST_VIB_SENSING_TIME' & 'SHM_SD_IDX_LAST_TEMP_SENSING_TIME' seperately as there are no
 *     'measurementType' for them. update them if necessary
 *  
 *  index - measureType mapping list:
 *  first uint16_t 1: updated, 0: not updated.
 *  Bit 0: REMAINING_VOLUME
 *  Bit 1: ADVANCED_ALGO_MACHINE_RUN_CODE
 *  Bit 2: ADVANCED_ALGO_MACHINE_CONDITON_CODE
 *  Bit 3: ADVANCED_ALGO_ALARM_CODE
 *  Bit 4: ADVANCED_ALGO_RMS_MAG_PRE
 *  Bit 5: ADVANCED_ALGO_RMS_VIB_PRE
 *  Bit 6: ENVIROMENTAL_TEMPERATURE_CURRENT
 *  Bit 7: ENVIROMENTAL_TEMPERATURE_MAX
 *  Bit 8: ENVIROMENTAL_TEMPERATURE_MIN
 *  Bit 9: ADVANCED_ALGO_EST_RPM_START
 *  Bit 10: ADVANCED_ALGO_EST_RPM_END
 *  Bit 11: ADVANCED_ALGO_VEL_OV
 *  Bit 12: ADVANCED_ALGO_VEL_RSH
 *  Bit 13: ADVANCED_ALGO_VEL_MTR
 *  Bit 14: ADVANCED_ALGO_VEL_FAN
 *  Bit 15: ADVANCED_ALGO_VEL_PUMP
 *  
 *  second uint16_t 1: updated, 0: not updated.
 *  Bit 0: ADVANCED_ALGO_ENV_OV             ==> bit 16
 *  Bit 1: ADVANCED_ALGO_ENV_BPFI           ==> bit 17
 *  Bit 2: ADVANCED_ALGO_ENV_BPFO           ==> bit 18
 *  Bit 3: ADVANCED_ALGO_ENV_BSF            ==> bit 19
 *  Bit 4: ADVANCED_ALGO_ENV_FTF            ==> bit 20
 *  Bit 5: ADVANCED_ALGO_ENV_RSH            ==> bit 21
 *  Bit 6: ADVANCED_ALGO_ACC_OV             ==> bit 22
 *  Bit 7: ADVANCED_ALGO_ACC_GEAR           ==> bit 23
 *  Bit 8: BLE_RSSI_CURRENT                 ==> bit 24
 *  Bit 9: last vibration sensing time      ==> bit 25
 *  Bit 10: last temperature sensing time   ==> bit 26
 *  Bit 11 - 15: Reserved (set to 0)
 */

    /* 0 - 63 used */
const uint8_t shm_sd_measType_to_idx[SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE + 1] = {
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_UNKNOWN_MEASUREMENT_TYPE = 0,
    // /* Vibration */
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE = 1,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_ENV2_WAVE = 2,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE = 3,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE = 4,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_ACC_P2P = 5,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_ACC_RMS = 6,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_ENV2_P2P = 7,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_ENV2_RMS = 8,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_ENV3_P2P = 9,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_ENV3_RMS = 10,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_P2P = 11,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_RMS = 12,
    // /* Enviromental temperature */
  6,  // SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT = 13,
  7,  // SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MAX = 14,
  8,  // SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN = 15,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_HISTORY = 16,
    // /* Enviromental humidity */
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_ENVIROMENTAL_HUMIDITY_CURRENT = 17,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_ENVIROMENTAL_HUMIDITY_MAX = 18,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_ENVIROMENTAL_HUMIDITY_MIN = 19,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_ENVIROMENTAL_HUMIDITY_HISTORY = 20,
    // /* Board temperature */
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_CURRENT = 21,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_MAX = 22,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_MIN = 23,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_HISTORY = 24,
    // /* Board humidity */
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_BOARD_HUMIDITY_CURRENT = 25,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_BOARD_HUMIDITY_MAX = 26,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_BOARD_HUMIDITY_MIN = 27,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_BOARD_HUMIDITY_HISTORY = 28,
    // /* Battery */
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_VOLTAGE_CURRENT = 29,
  0,  // SKFChina_Common_MeasurementType_REMAINING_VOLUME = 30, /* namely RSOC (relative state-of-charge) */
    // /* Rotation speed */
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_ROTATION_SPEED = 31,
    // /* Communication */
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_CELLULAR_RSRP_CURRENT = 32,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_CELLULAR_RSRQ_CURRENT = 33,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_CELLULAR_SINR_CURRENT = 34,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_CELLULAR_RSSI_CURRENT = 35,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_NB_IOT_CELEVEL_CURRENT = 36,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_STATE_OF_HEALTH_CURRENT = 37,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_AVERAGE_TIME_TO_FULL = 38,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_AVERAGE_TIME_TO_EMPTY = 39,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_BATTERY_SURFACE_TEMPERATURE_CURRENT = 40,
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2,  // SKFChina_Common_MeasurementType_BATTERY_MNGMENT_IC_TEMPERATURE_CURRENT = 41,
  24,  // SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT = 42,
    // /* Advanced algorithm */
  1,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_RUN_CODE = 43,
  2,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_CONDITON_CODE = 44,
  3,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE = 45,
  4,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_MAG_PRE = 46,
  5,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_VIB_PRE = 47,
  9,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_START = 48,
  10,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_END = 49,
  11,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV = 50,
  12,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH = 51,
  13,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR = 52,
  14,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN = 53,
  15,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP = 54,
  16,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV = 55,
  17,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI = 56,
  18,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO = 57,
  19,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF = 58,
  20,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF = 59,
  21,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH = 60,
  22,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV = 61,
  23,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR = 62
  APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2  // SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE = 63,
};
#endif

#if (ENABLE_MODBUS_FEATURE == 1)
static uint8_t shm_get_sd_idx_by_nameId(uint32_t nId)
{
  for(uint8_t i=0;i<CONFIG_SHM_SENSOR_DATA_MAX_NUM;i++)
  {
    if(nId == g_sd_instance[i].sd_nameid)
    {
      return i;
    }
  }
  return CONFIG_SHM_SENSOR_DATA_MAX_NUM;
}
/*
 *  set sensorData's update_flag based on index
 */
void shm_set_sd_bit_map(sensorType_t sType, uint64_t *sd_bit_map, uint8_t idx)
{
  uint8_t idx_max = 0;
  switch (sType)
  {
    case MODBUS_SENSOR_TYPE_T:
      // 1 more item for timeStamp(SHM_SD_IDX_LAST_TEMP_SENSING_TIME_T)
      idx_max = APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS + 1; 
      break;
    case MODBUS_SENSOR_TYPE_P:
      // 2 more items for timeStamp(SHM_SD_IDX_LAST_VIB_SENSING_TIME, SHM_SD_IDX_LAST_TEMP_SENSING_TIME)
      idx_max = APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRS + 2;
      break;
    case MODBUS_SENSOR_TYPE_PRO:
      // 2 more items for timeStamp(SHM_SD_IDX_LAST_VIB_SENSING_TIME, SHM_SD_IDX_LAST_TEMP_SENSING_TIME)
      idx_max = APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRO + 2;
      break;  
    default:
      break;
  }
  if((NULL == sd_bit_map) || (idx >= idx_max))
    return;
  *sd_bit_map |= (0x1UL << idx);
#if (1)
  printf("[bit-map] 0x%016x,idx:%d \n", *sd_bit_map, idx);
#endif
}
/*
 *  get register length based on sensor type
 */
uint8_t shm_get_register_len(char *sType)
{
  return (0 == strcmp(sType, TYPE_STR_INSIGHT_P)) ? DEV_REG_ADDR_LENGTH_INSIGHT_P : ((0 == strcmp(sType, TYPE_STR_INSIGHT_P_PRO)) ? DEV_REG_ADDR_LENGTH_INSIGHT_PRO : DEV_REG_ADDR_LENGTH_INSIGHT_T);
}
/*
 *  get sensor type string for 'description' based on sensor type
 */
char *shm_get_sensor_type_str(char *sType)
{
  return (0 == strcmp(sType, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : ((0 == strcmp(sType, TYPE_STR_INSIGHT_P_PRO)) ? MODBUS_PRODUCT_TYPE_PRO_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
}
/*
 *  Notes:
 *     It's for insight-T only
 *  set sensorData's update_flag based on index
 *  Bit 0: VOLTAGE_CURRENT
 *  Bit 1: BOARD_TEMPERATURE_CURRENT
 *  Bit 2: ENVIROMENTAL_TEMPERATURE_CURRENT
 *  Bit 3: BLE_RSSI_CURRENT
 *  Bit 4: last temperature sensing time
 */
uint8_t shm_get_measure_type_idx_t(SKFChina_Common_MeasurementType mType)
{
  uint8_t idx = APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 2;
  switch (mType)
  {
    case SKFChina_Common_MeasurementType_VOLTAGE_CURRENT:
      idx = 0;
      break;
    case SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_CURRENT:
      idx = 1;
      break;
    case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT:
      idx = 2;
      break;  
    case SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT:
      idx = 3;
      break;  
    default:
      break;
  }
  return idx;
}

/*
 *  Notes: DO NOT use it for insight-T sensor.
 *  update the bit_map for 'SHM_SD_IDX_LAST_VIB_SENSING_TIME' and 'SHM_SD_IDX_LAST_TEMP_SENSING_TIME'
 *  based on following rules:
 *   i : SHM_SD_IDX_LAST_TEMP_SENSING_TIME:
 *        6,   // SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT = 13,
 *   ii: SHM_SD_IDX_LAST_VIB_SENSING_TIME:
 *        11,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV    = 50,
 *        12,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH   = 51,
 *        13,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR   = 52,
 *        14,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN   = 53,
 *        15,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP  = 54,
 *        16,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV    = 55,
 *        17,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI  = 56,
 *        18,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO  = 57,
 *        19,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF   = 58,
 *        20,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF   = 59,
 *        21,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH   = 60,
 *        22,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV    = 61,
 *        23,  // SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR  = 62
 *  @parameters: 
 *       sType: MODBUS_SENSOR_TYPE_P - 1; MODBUS_SENSOR_TYPE_PRO - 2
 *       idx : index of g_sd_instance
 */
// TODO: update the NA value.
#define CONFIG_FLOAT_NA (0x7fc00000)
void shm_update_sd_bit_map(sensorType_t sType, uint8_t idx)
{
  if((idx < CONFIG_SHM_SENSOR_DATA_MAX_NUM) &&
     (true == g_sd_instance[idx].sd_valid_flag) &&
     ((MODBUS_SENSOR_TYPE_P == g_sd_instance[idx].sd_sensor_type) || (MODBUS_SENSOR_TYPE_PRO == g_sd_instance[idx].sd_sensor_type))) {
      sensorDataTableData_t *sensor_data_p = NULL;
      // check SHM_SD_IDX_LAST_TEMP_SENSING_TIME
      sensor_data_p = (sensorDataTableData_t *)(&g_sd_instance[idx].sd_data_buf[shm_sd_measType_to_idx[SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT]]);
      if(0 != sensor_data_p->sampleTime) {
          shm_set_sd_bit_map(sType, (uint64_t *)(&g_sd_instance[idx].sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1]), SHM_SD_IDX_LAST_TEMP_SENSING_TIME);
      }
      // check SHM_SD_IDX_LAST_VIB_SENSING_TIME
      uint8_t i   = shm_sd_measType_to_idx[SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV]; // 11
      uint8_t max = shm_sd_measType_to_idx[SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR] + 1; // 23 + 1 = 24
      for(;i<max;i++) {
        sensor_data_p = (sensorDataTableData_t *)(&g_sd_instance[idx].sd_data_buf[i]);
        if((0 != sensor_data_p->sampleTime) &&
           (CONFIG_FLOAT_NA != *(uint32_t *)(&sensor_data_p->value))) {
            shm_set_sd_bit_map(sType, (uint64_t *)(&g_sd_instance[idx].sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1]), SHM_SD_IDX_LAST_VIB_SENSING_TIME);
            break;
        }
      }
  }
}

// timer handler for predict sensor
void shm_timer_handler_p(union sigval value)
{
  int ret = 0;
  bool tmp_flag = false;
  timer_t *timerId = (timer_t *)value.sival_ptr;
  uint8_t sd_idx = CONFIG_SHM_SENSOR_DATA_MAX_NUM;
#if (1)
  // for debug only, remove it later
  printf("[shm timer_p] enter handler\n");
#endif
  // use timer_id to identify the chn/nameId
  for(uint8_t i=0;i<CONFIG_SHM_SENSOR_DATA_MAX_NUM;i++)
  {
    if((*timerId == g_sd_instance[i].sd_timer_id) &&
        (true == g_sd_instance[i].sd_valid_flag))
    {
      sd_idx = i;
      break;
    }
  }
  if(sd_idx >= CONFIG_SHM_SENSOR_DATA_MAX_NUM)
  {
    LOG_WARN(OUTPOINT, "[shm timer_p]:invalid timerId:0x%p exit!\r\n", *timerId);
    goto _TIME_HANDLER_P_EXIT;
  }
  timer_delete(g_sd_instance[sd_idx].sd_timer_id);
  shm_update_sd_bit_map(g_sd_instance[sd_idx].sd_sensor_type, sd_idx);
  LOG_INFO(OUTPOINT, "[shm timer_p]:cnt:%d max:%d ch:%d nid:%d!\r\n", g_sd_instance[sd_idx].sd_piece_cnt, g_sd_instance[sd_idx].sd_piece_max, g_sd_instance[sd_idx].sd_chn_idx, g_sd_instance[sd_idx].sd_nameid);
  if((g_sd_instance[sd_idx].sd_piece_cnt > 0) && (g_sd_instance[sd_idx].sd_piece_cnt <= APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRS)) {
    for(uint8_t i=0;i<2;i++) {
      ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
      if(0 == ret) {
        for(uint8_t j=0;j<g_sd_instance[sd_idx].sd_piece_max;j++) {
          memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                 (void *)&g_sd_instance[sd_idx].sd_data_buf[j],
                  sizeof(shm_sensor_data));
        }
        // copy the reg bit_mapping into share memory
        // last item of 'sd_data_buf'                    
        memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
              (void *)&g_sd_instance[sd_idx].sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                sizeof(uint64_t));
        app_shmem_sem_post();
        tmp_flag = true;
        break;
      }
    }
    if(!tmp_flag) { // write directly in case failed 3 times.
      for(uint8_t j=0;j<g_sd_instance[sd_idx].sd_piece_max;j++) {
        memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                (void *)&g_sd_instance[sd_idx].sd_data_buf[j],
                sizeof(shm_sensor_data));
      }
      // copy the reg bit_mapping into share memory
      // last item of 'sd_data_buf'                    
      memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
            (void *)&g_sd_instance[sd_idx].sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
              sizeof(uint64_t));
      LOG_WARN(OUTPOINT, "[shm timer_p]force write shm(ch:%d,nid:%d)!\r\n", g_sd_instance[sd_idx].sd_chn_idx, g_sd_instance[sd_idx].sd_nameid);
    }
#if (1) // for debug only remove it later, log the first piece data only for save time
    printf("timer_p:(idx:%d):\r\n",g_sd_instance[sd_idx].sd_chn_idx);
    uint8_t *tmp = shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (g_sd_instance[sd_idx].sd_piece_cnt - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
    // for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
    for(uint16_t i=0;i<16;i++)
      printf(" %02x", tmp[i]);
    printf("\r\ntimer_p: end\r\n");
#endif
    // clear the index & cache here as the data is written into shm successfully here
    memset((void *)&g_sd_instance[sd_idx], 0, sizeof(appShmSensorDataInstance_t));
  }
_TIME_HANDLER_P_EXIT:
}
// TODO: add 1 more timer handler for pro sensor later.
#if (0)
static void shm_timer_handler_p(int sig, siginfo_t *si, void *unused)
{
  int ret = 0;
  bool tmp_flag = false;
  timer_delete(g_timer_id_p);
  LOG_INFO(OUTPOINT, "[shm timer_p]:cnt:%d max:%d ch:%d nid:%d!\r\n", g_sensor_data_piece_cnt_p, g_sensor_data_piece_max_p, g_current_ch_idx_p, g_former_nameid_p);
  if((g_sensor_data_piece_cnt_p > 0) && (g_sensor_data_piece_cnt_p <= g_sensor_data_piece_max_p)) {
    for(uint8_t i=0;i<2;i++) {
      ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
      if(0 == ret) {
        for(uint8_t j=0;j<g_sensor_data_piece_cnt_p;j++) {
          memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx_p - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                 (void *)&g_shm_data_buf_p[j],
                  sizeof(shm_sensor_data));
        }
        app_shmem_sem_post();
        tmp_flag = true;
        break;
      }
    }
    if(!tmp_flag) { // write directly in case failed 3 times.
        for(uint8_t j=0;j<g_sensor_data_piece_cnt_p;j++) {
          memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx_p - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                 (void *)&g_shm_data_buf_p[j],
                  sizeof(shm_sensor_data));
        }
      LOG_WARN(OUTPOINT, "[shm timer_p]force write shm(ch:%d,nid:%d)!\r\n", g_current_ch_idx_p, g_former_nameid_p);
    }
#if (1) // for debug only remove it later, log the first piece data only for save time
    printf("timer_p:(idx:%d):\r\n",g_current_ch_idx_p);
    uint8_t *tmp = shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx_p - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (g_sensor_data_piece_cnt_p - 1) * sizeof(shm_sensor_data);
    // for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
    for(uint16_t i=0;i<16;i++)
      printf(" %02x", tmp[i]);
    printf("\r\ntimer_p: end\r\n");
#endif
    // clear the index & cache here as the data is written into shm successfully here
    memset((void *)&g_shm_data_buf_p[0], 0, sizeof(shm_sensor_data) * g_sensor_data_piece_max_p);
    // Notes: 
    g_sensor_data_piece_cnt_p = 0;

  }
}
#endif // end of 0
static void shm_timer_handler(int sig, siginfo_t *si, void *unused)
{
  int ret = 0;
  bool tmp_flag = false;
  timer_delete(g_timer_id);
  shm_set_sd_bit_map(MODBUS_SENSOR_TYPE_T,
                      (uint64_t *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                      SHM_SD_IDX_LAST_TEMP_SENSING_TIME_T);
  LOG_INFO(OUTPOINT, "[shm timer]:cnt:%d max:%d ch:%d nid:%d!\r\n", g_sensor_data_piece_cnt, g_sensor_data_piece_max, g_current_ch_idx, shm_sensor_data.nameId);
  if((g_sensor_data_piece_cnt > 0) && (g_sensor_data_piece_cnt <= g_sensor_data_piece_max)) {
    for(uint8_t i=0;i<2;i++) {
      ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
      if(0 == ret) {
        for(uint8_t j=0;j<APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS;j++) {
          memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                 (void *)&g_shm_data_buf[j],
                  sizeof(shm_sensor_data));
        }
        // copy the reg bit_mapping into share memory
        // last item of 'sd_data_buf'                    
        memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
              (void *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                sizeof(uint64_t));
        app_shmem_sem_post();
        tmp_flag = true;
        break;
      }
    }
    if(!tmp_flag) { // write directly in case failed 3 times.
      for(uint8_t j=0;j<APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS;j++) {
        memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                (void *)&g_shm_data_buf[j],
                sizeof(shm_sensor_data));
      }
      // copy the reg bit_mapping into share memory
      // last item of 'sd_data_buf'                    
      memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
            (void *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
              sizeof(uint64_t));
      LOG_WARN(OUTPOINT, "[shm timer]force write shm(ch:%d,nid:%d)!\r\n", g_current_ch_idx, shm_sensor_data.nameId);
    }
#if (1) // for debug only remove it later, log the first piece data only for save time
    printf("timer-T:(idx:%d):\r\n",g_current_ch_idx);
    uint8_t *tmp = shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (g_sensor_data_piece_cnt - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
    // for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
    for(uint16_t i=0;i<16;i++)
      printf(" %02x", tmp[i]);
    printf("\r\ntimer-T:end\r\n");
#endif
    // clear the index & cache here as the data is written into shm successfully here
    memset((void *)&g_shm_data_buf[0], 0, sizeof(shm_sensor_data) * APP_SHM_SENSORDATA_TABLE_PIECE_MAX);
    // Notes: 
    g_sensor_data_piece_cnt = 0;

  }
}
static void table_query_data_convert(uint32_t tb_idx,
                  gw_pro_command_sqlite_query_item_response_t *queried_data,
                  void *converted_data_p)
                  // appShmDataItem_t *converted_data)
{
  switch (tb_idx) {
  case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE: {
    appShmDataItem_t *converted_data = NULL;
    converted_data = (appShmDataItem_t *)converted_data_p;
    if(queried_data->dataNum != (sizeof(GatewayConfig) / sizeof(GatewayConfig[0]))) {
    // if(queried_data->dataNum != 32) {
      LOG_WARN(OUTPOINT, "gw table wrong item num:%d -- %d!\r\n", queried_data->dataNum, sizeof(GatewayConfig) / sizeof(GatewayConfig[0]));
      // TODO
    }
    converted_data->appShmDataType = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
    for(uint8_t i=0;i<queried_data->dataNum;i++) {
      switch (queried_data->inquiredData[i].field)
      {
      case 1: // nameId
        converted_data->gwTableData.gwConfig.nameId = queried_data->inquiredData[i].data.data_uint32; //field id 1
        break;
      case 2: // name
        snprintf(converted_data->gwTableData.gwConfig.name,
                sizeof(converted_data->gwTableData.gwConfig.name), "%s",
                queried_data->inquiredData[i].data.data_char); //field id 2
        break;
      case 3: // macAddr
        snprintf((char *)(converted_data->gwTableData.gwConfig.macAddr.addr.addrArray),
                sizeof(converted_data->gwTableData.gwConfig.macAddr.addr.addrArray), "%s",
                queried_data->inquiredData[i].data.data_char); //field id 3
        break;
      case 4: // manufacturer
        snprintf(converted_data->gwTableData.gwConfig.manufacturer,
                sizeof(converted_data->gwTableData.gwConfig.manufacturer), "%s",
                queried_data->inquiredData[i].data.data_char); //field id 4
        break;
      case 5: // ApplicationVersion
        break;
      case 6: // LinuxKernelVersion
        break;
      case 7: // version
        converted_data->gwTableData.version = queried_data->inquiredData[i].data.data_uint8; //field id 7 version
        break;
      case 8: // lastEditTimeS
        converted_data->gwTableData.lastEditTimeS = queried_data->inquiredData[i].data.data_uint64; //field id 8 lastEditTime
        break;
      case 9: // type
        snprintf(converted_data->gwTableData.gwConfig.type,
              sizeof(converted_data->gwTableData.gwConfig.type), "%s",
              queried_data->inquiredData[i].data.data_char); //field id 9
        break;
      case 10: // gatewayMode
        converted_data->gwTableData.gwConfig.gatewayMode = queried_data->inquiredData[i].data.data_uint8; //field id 10 gatewayMode
        break;
      case 11: // ble1TxPower
        converted_data->gwTableData.gwConfig.bleConfig.ble1TxPower = queried_data->inquiredData[i].data.data_uint8; //field id 11 ble1TxPower
        break;
      case 12: // ble2TxPower
        converted_data->gwTableData.gwConfig.bleConfig.ble2TxPower = queried_data->inquiredData[i].data.data_uint8; //field id 12 ble2TxPower
        break;
      case 13: // ble1Antenna
        converted_data->gwTableData.gwConfig.bleConfig.ble1Antenna = queried_data->inquiredData[i].data.data_uint8; //field id 13 ble1Antenna
        break;
      case 14: // ble2Antenna
        converted_data->gwTableData.gwConfig.bleConfig.ble2Antenna = queried_data->inquiredData[i].data.data_uint8; //field id 14 ble2Antenna
        break;
      case 15: // BLEFilterListNameNumber
        break;
      case 16: // bleDuplicatedDrop_ms
        converted_data->gwTableData.gwConfig.bleConfig.bleDuplicatedDrop_ms = queried_data->inquiredData[i].data.data_uint32; //field id 16 bleDuplicatedDrop
        break;
      case 17: // needTimestampForRecBeacon
        converted_data->gwTableData.gwConfig.timeConfig.needTimestampForRecBeacon = queried_data->inquiredData[i].data.data_uint8; //field id 17 timestamp. to be confirmed.
        break;
      case 18: // timeSetting
        converted_data->gwTableData.gwConfig.timeConfig.timeSetting = queried_data->inquiredData[i].data.data_uint8; //field id 18 timeSetting
        break;
      case 19: // timeNtpUrl
        snprintf(converted_data->gwTableData.gwConfig.timeConfig.timeNtpUrl,
                256 - 1, "%s",
                queried_data->inquiredData[i].data.data_char); //field id 19
        break;
      case 20: // mqttSecurityType
        converted_data->gwTableData.gwConfig.mqttConfig.mqttSecurityType = queried_data->inquiredData[i].data.data_uint8; //field id 20 mqttSecurityType
        break;
      case 21: // MQTTHost
        snprintf(converted_data->gwTableData.gwConfig.mqttConfig.MQTTHost,
                256 - 1, "%s",
                queried_data->inquiredData[i].data.data_char); //field id 21
        break;
      case 22: // MQTTPort
        converted_data->gwTableData.gwConfig.mqttConfig.MQTTPort = queried_data->inquiredData[i].data.data_uint32; //field id 22 MQTTPort
        break;
      case 23: // DHCPEnable
        converted_data->gwTableData.gwConfig.ipConfig.DHCPEnable = queried_data->inquiredData[i].data.data_uint8; //field id 23 DHCPEnable
        break;
      case 24: // staticIPAddr
        snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.staticIPAddr.addr.addrArray,
                sizeof(converted_data->gwTableData.gwConfig.ipConfig.staticIPAddr.addr.addrArray), "%s",
                queried_data->inquiredData[i].data.data_char); //field id 24
        break;
      case 25: // netMask
        snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.netMask.addr.addrArray,
                sizeof(converted_data->gwTableData.gwConfig.ipConfig.netMask.addr.addrArray), "%s",
                queried_data->inquiredData[i].data.data_char); //field id 25
        break;
      case 26: // dnsServer
        snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.dnsServer.addr.addrArray,
                sizeof(converted_data->gwTableData.gwConfig.ipConfig.dnsServer.addr.addrArray), "%s",
                queried_data->inquiredData[i].data.data_char); //field id 26
        break;
      case 27: // gatewayAddr
        snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.gatewayAddr.addr.addrArray,
                sizeof(converted_data->gwTableData.gwConfig.ipConfig.gatewayAddr.addr.addrArray), "%s",
                queried_data->inquiredData[i].data.data_char); //field id 27
        break;
      case 28: // baudrateSlave
        converted_data->gwTableData.gwConfig.modbusConfig.baudrateSlave = queried_data->inquiredData[i].data.data_uint32; //field id 28 baudrateSlave
        break;
      case 29: // slaveAddrSlave
        converted_data->gwTableData.gwConfig.modbusConfig.slaveAddrSlave = queried_data->inquiredData[i].data.data_uint32; //field id 29 slaveAddrSlave
        break;
      case 30: // paritySlave
        converted_data->gwTableData.gwConfig.modbusConfig.paritySlave = queried_data->inquiredData[i].data.data_uint8; //field id 30 paritySlave
        break;
      case 31: // stopSlave
        converted_data->gwTableData.gwConfig.modbusConfig.stopSlave = queried_data->inquiredData[i].data.data_uint8; //field id 31 stopSlave
        break;
      case 32: // registerMode
        converted_data->gwTableData.gwConfig.modbusConfig.registerMode = queried_data->inquiredData[i].data.data_uint8; //field id 32 registerMode
        break;
      case 33: // description
        snprintf(converted_data->gwTableData.gwConfig.description,
                sizeof(converted_data->gwTableData.gwConfig.description), "%s",
                queried_data->inquiredData[i].data.data_char); //field id 33 description
        break;
      default:
        LOG_WARN(OUTPOINT, "gateway wrong field num:%d!\r\n", queried_data->inquiredData[i].field);
        break;
      }
    }
#if (1) // for debug only, remove it later
    if(queried_data->dataNum == 25) { // init modbus params in case they are not available on debug purpose
      converted_data->gwTableData.gwConfig.modbusConfig.baudrateSlave = 9600; //field id 28 baudrateSlave
      converted_data->gwTableData.gwConfig.modbusConfig.slaveAddrSlave = 1; //field id 29 slaveAddrSlave
      converted_data->gwTableData.gwConfig.modbusConfig.paritySlave = 0; //field id 30 paritySlave
      converted_data->gwTableData.gwConfig.modbusConfig.registerMode = 0; //field id 31 registerMode
      converted_data->gwTableData.gwConfig.modbusConfig.stopSlave = 0; //field id 32 stopSlave
    }
    LOG_DEBUG(OUTPOINT, "Gateway query converted:\r\n \
                          nameid: %d\r\n \
                          name:%s\r\n \
                          macAddr:%s\r\n \
                          manufacturer: %d\r\n \
                          version:%d\r\n \
                          lastEditTimeS:%lld\r\n \
                          type: %s\r\n \
                          gatewayMode:%d\r\n \
                          ble1TxPower:%d\r\n \
                          ble2TxPower: %d\r\n \
                          ble1Antenna:%d\r\n \
                          ble2Antenna:%d\r\n \
                          bleDuplicatedDrop_ms: %d\r\n \
                          needTimestampForRecBeacon:%d\r\n \
                          timeSetting:%d\r\n \
                          timeNtpUrl: %s\r\n \
                          mqttSecurityType:%d\r\n \
                          MQTTHost:%s\r\n \
                          MQTTPort: %d\r\n \
                          DHCPEnable:%d\r\n \
                          staticIPAddr:%s\r\n \
                          netMask: %s\r\n \
                          dnsServer:%s\r\n \
                          gatewayAddr:%s\r\n \
                          baudrateSlave:%d\r\n \
                          slaveAddrSlave:%d\r\n \
                          paritySlave: %d\r\n \
                          registerMode:%d\r\n \
                          stopSlave:%d\r\n \
                          description: %s\r\n",
                          converted_data->gwTableData.gwConfig.nameId,
                          converted_data->gwTableData.gwConfig.name,
                          converted_data->gwTableData.gwConfig.macAddr.addr.addrArray,
                          converted_data->gwTableData.gwConfig.manufacturer,
                          converted_data->gwTableData.version,
                          converted_data->gwTableData.lastEditTimeS,
                          converted_data->gwTableData.gwConfig.type,
                          converted_data->gwTableData.gwConfig.gatewayMode,
                          converted_data->gwTableData.gwConfig.bleConfig.ble1TxPower,
                          converted_data->gwTableData.gwConfig.bleConfig.ble2TxPower,
                          converted_data->gwTableData.gwConfig.bleConfig.ble1Antenna,
                          converted_data->gwTableData.gwConfig.bleConfig.ble2Antenna,
                          converted_data->gwTableData.gwConfig.bleConfig.bleDuplicatedDrop_ms,
                          converted_data->gwTableData.gwConfig.timeConfig.needTimestampForRecBeacon,
                          converted_data->gwTableData.gwConfig.timeConfig.timeSetting,
                          converted_data->gwTableData.gwConfig.timeConfig.timeNtpUrl,
                          converted_data->gwTableData.gwConfig.mqttConfig.mqttSecurityType,
                          converted_data->gwTableData.gwConfig.mqttConfig.MQTTHost,
                          converted_data->gwTableData.gwConfig.mqttConfig.MQTTPort,
                          converted_data->gwTableData.gwConfig.ipConfig.DHCPEnable,
                          converted_data->gwTableData.gwConfig.ipConfig.staticIPAddr.addr.addrArray,
                          converted_data->gwTableData.gwConfig.ipConfig.netMask.addr.addrArray,
                          converted_data->gwTableData.gwConfig.ipConfig.dnsServer.addr.addrArray,
                          converted_data->gwTableData.gwConfig.ipConfig.gatewayAddr.addr.addrArray,
                          converted_data->gwTableData.gwConfig.modbusConfig.baudrateSlave,
                          converted_data->gwTableData.gwConfig.modbusConfig.slaveAddrSlave,
                          converted_data->gwTableData.gwConfig.modbusConfig.paritySlave,
                          converted_data->gwTableData.gwConfig.modbusConfig.registerMode,
                          converted_data->gwTableData.gwConfig.modbusConfig.stopSlave,
                          converted_data->gwTableData.gwConfig.description);
#endif
  } break;
  case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE: {
    appShmDataItem_t *converted_data = NULL;
    converted_data = (appShmDataItem_t *)converted_data_p;
    if(queried_data->dataNum != (sizeof(DeviceList) / sizeof(DeviceList[0]))) {
      LOG_WARN(OUTPOINT, "dev table wrong item num:%d!\r\n", queried_data->dataNum);
      // TODO
    }
    converted_data->appShmDataType = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
    for(uint8_t i=0;i<queried_data->dataNum;i++) {
      switch (queried_data->inquiredData[i].field) {
        case 1: // nameId
          converted_data->devTableData.nameId = queried_data->inquiredData[i].data.data_uint32; //field id 1 nameId
          break;
        case 2: // name
          snprintf(converted_data->devTableData.name,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 2 name
          break;
        case 3: // BLESensorName
          snprintf(converted_data->devTableData.BLESensorName,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 3 BLESensorName
          break;
        case 4: // macAddr
          snprintf(converted_data->devTableData.macAddr,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 4 macAddr
          break;
        case 5: // manufacturer
          snprintf(converted_data->devTableData.manufacturer,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 5 manufacturer
          break;
        case 6: // type
          snprintf(converted_data->devTableData.type,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 6 type
          break;
        case 7: // description
          snprintf(converted_data->devTableData.description,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 7 description
          break;
        default: // not found
          LOG_WARN(OUTPOINT, "dev wrong field num:%d!\r\n", queried_data->inquiredData[i].field);
          break;
      }
    }
#if (1) // for debug only, remove it later
    LOG_DEBUG(OUTPOINT, "Dev query converted: \r\n \
                          nameid: %d\r\n \
                          name:%s\r\n \
                          BLESensorName:%s\r\n \
                          macAddr: %s\r\n \
                          manufacturer:%s\r\n \
                          type:%s\r\n \
                          description:%s\r\n",
                          converted_data->devTableData.nameId,
                          converted_data->devTableData.name,
                          converted_data->devTableData.BLESensorName,
                          converted_data->devTableData.macAddr,
                          converted_data->devTableData.manufacturer,
                          converted_data->devTableData.type,
                          converted_data->devTableData.description);
#endif
  } break;
  case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE: {
    if(queried_data->dataNum != (sizeof(SensorConfig) / sizeof(SensorConfig[0]))) {
      LOG_WARN(OUTPOINT, "sensorConfig table wrong item num:%d!\r\n", queried_data->dataNum);
      // TODO
    }
    // converted_data->appShmDataType = GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE;
    // TODO
  } break;
  case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE: {
    sensorDataTableData_t *converted_data = NULL;
    converted_data = (sensorDataTableData_t *)converted_data_p;
    bool testflag = false;
    if(queried_data->dataNum != (sizeof(SensorData) / sizeof(SensorData[0]))) {
      LOG_WARN(OUTPOINT, "sensordata table wrong item num:%d!\r\n", queried_data->dataNum);
      // TODO
    }
    // converted_data->appShmDataType = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
    for(uint8_t i=0;i<queried_data->dataNum;i++) {
      switch (queried_data->inquiredData[i].field) {
        case 1: // sequenceNumber
          converted_data->sequenceNumber = queried_data->inquiredData[i].data.data_uint64; //field id 1 sequenceNumber
          break;
        case 2: // nameId
          converted_data->nameId = queried_data->inquiredData[i].data.data_uint32; //field id 2 nameId
          break;
        case 3: // BLESensorName
          snprintf(converted_data->BLESensorName,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 3 BLESensorName
          break;
        case 4: // macAddr
          snprintf(converted_data->macAddr,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 4 macAddr
          break;
        case 5: // type
          snprintf(converted_data->type,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 5 type
          break;
        case 6: // manufacturer
          snprintf(converted_data->manufacturer,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 6 manufacturer
          break;
        case 7: // measureSeqno
          converted_data->measureSeqno = queried_data->inquiredData[i].data.data_uint32; //field id 7 measureSeqno
          break;
        case 8: // sampleTime
          converted_data->sampleTime = queried_data->inquiredData[i].data.data_uint64; //field id 8 sampleTime
          break;
        case 9: // receivedTimestamp
          converted_data->receivedTimestamp = queried_data->inquiredData[i].data.data_uint64; //field id 9 receivedTimestamp
          break;
        case 10: // measurement
          converted_data->measurement = queried_data->inquiredData[i].data.data_uint8; //field id 10 measurement
          break;
        case 11: // dataType
          converted_data->dataType = queried_data->inquiredData[i].data.data_uint8; //field id 11 dataType
          break;
        case 12: // format
          snprintf(converted_data->format,
                  sizeof(converted_data->format), "%s",
                  queried_data->inquiredData[i].data.data_char); //field id 12 format    
          if (0 == strcmp(queried_data->inquiredData[i].data.data_char, "Digit")) {
            testflag = true;
          }
          break;
        case 13: // range
          converted_data->range = queried_data->inquiredData[i].data.data_uint8; //field id 13 range
          break;
        case 14: // unit
          converted_data->unit = queried_data->inquiredData[i].data.data_uint8; //field id 14 unit
          break;
        case 15: // measureLengthSample
          converted_data->measureLengthSample = queried_data->inquiredData[i].data.data_uint32; //field id 15 measureLengthSample
          break;
        case 16: // totalDataLengthSample
          converted_data->totalDataLengthSample = queried_data->inquiredData[i].data.data_uint32; //field id 16 totalDataLengthSample
          break;
        case 17: // dimension
          converted_data->dimension = queried_data->inquiredData[i].data.data_uint32; //field id 17 dimension
          break;
        case 18: // dataFormat
          converted_data->dataFormat = queried_data->inquiredData[i].data.data_uint8; //field id 18 dataFormat
          break;
        case 19: // samplePeriods
          converted_data->samplePeriods = queried_data->inquiredData[i].data.data_uint32; //field id 19 samplePeriods
          break;
        case 20: // encryption
          converted_data->encryption = queried_data->inquiredData[i].data.data_uint8; //field id 20 encryption
          break;
        case 21: // value
          if(testflag) {
            converted_data->value.data_float = queried_data->inquiredData[i].data.data_float; //field id 21 value
          } else {
            snprintf(converted_data->value.data_char,
                    GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                    queried_data->inquiredData[i].data.data_char); //field id 21 value
          }
          break;
        case 22: // sampleRate
          converted_data->sampleRate = queried_data->inquiredData[i].data.data_float; //field id 22 sampleRate
          break;
        case 23: // product
          converted_data->product = queried_data->inquiredData[i].data.data_uint8; //field id 23 product
          break;
        case 24: // sensor
          converted_data->sensor = queried_data->inquiredData[i].data.data_uint8; //field id 24 sensor
          break;
        default: // not found
          LOG_WARN(OUTPOINT, "sensordata wrong field num:%d!\r\n", queried_data->inquiredData[i].field);
          break;
      }
    }    
#if (0) // for debug only, remove it later
    LOG_DEBUG(OUTPOINT, "sensordata query converted:\r\n \
                          sequenceNumber: %d\r\n \
                          nameid: %d\r\n \
                          BLESensorName:%s\r\n \
                          macAddr:%s\r\n \
                          type: %s\r\n \
                          manufacturer: %s\r\n \
                          measureSeqno:%d\r\n \
                          sampleTime:%lld\r\n \
                          receivedTimestamp:%lld\r\n \
                          measurement:%d\r\n \
                          dataType: %d\r\n \
                          format:%s\r\n \
                          range:%d\r\n \
                          unit: %d\r\n \
                          measureLengthSample:%d\r\n \
                          totalDataLengthSample:%d\r\n \
                          dimension: %d\r\n \
                          dataFormat:%d\r\n \
                          samplePeriods:%d\r\n \
                          encryption: %d\r\n \
                          sampleRate:%f\r\n \
                          product: %d\r\n \
                          sensor:%d\r\n",
                          converted_data->sequenceNumber,
                          converted_data->nameId,
                          converted_data->BLESensorName,
                          converted_data->macAddr,
                          converted_data->type,
                          converted_data->manufacturer,
                          converted_data->measureSeqno,
                          converted_data->sampleTime,
                          converted_data->receivedTimestamp,
                          converted_data->measurement,
                          converted_data->dataType,
                          converted_data->format,
                          converted_data->range,
                          converted_data->unit,
                          converted_data->measureLengthSample,
                          converted_data->totalDataLengthSample,
                          converted_data->dimension,
                          converted_data->dataFormat,
                          converted_data->samplePeriods,
                          converted_data->encryption,
                          converted_data->sampleRate,
                          converted_data->product,
                          converted_data->sensor);
    if(testflag) {
      printf("value:%f\r\n", converted_data->value);
    } else {
      printf("value:%s\r\n", converted_data->value);
    }

#endif
  } break;
  }
}

static void table_update_data_convert(gw_pro_command_sqlite_update_item_param_t *updated_data,
                  void *converted_data_p)
                  // appShmDataItem_t *converted_data)
{
#if (1)
  switch (updated_data->table) {
  case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE: {
    appShmDataItem_t *converted_data = NULL;
    converted_data = (appShmDataItem_t *)converted_data_p;
    if(updated_data->dataNum != (sizeof(GatewayConfig) / sizeof(GatewayConfig[0]))) {
    // if(updated_data->dataNum != 32) {
      LOG_WARN(OUTPOINT, "gw table wrong item num:%d -- %d!\r\n", updated_data->dataNum, sizeof(GatewayConfig) / sizeof(GatewayConfig[0]));
      // TODO
    }
    converted_data->appShmDataType = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
    for(uint8_t i=0;i<updated_data->dataNum;i++) {
      switch (updated_data->updateData[i].field)
      {
      case 1: // nameId
        converted_data->gwTableData.gwConfig.nameId = updated_data->updateData[i].data.data_uint32; //field id 1
        break;
      case 2: // name
        snprintf(converted_data->gwTableData.gwConfig.name,
                sizeof(converted_data->gwTableData.gwConfig.name), "%s",
                updated_data->updateData[i].data.data_char); //field id 2
        break;
      case 3: // macAddr
        snprintf((char *)(converted_data->gwTableData.gwConfig.macAddr.addr.addrArray),
                sizeof(converted_data->gwTableData.gwConfig.macAddr.addr.addrArray), "%s",
                updated_data->updateData[i].data.data_char); //field id 3
        break;
      case 4: // manufacturer
        snprintf(converted_data->gwTableData.gwConfig.manufacturer,
                sizeof(converted_data->gwTableData.gwConfig.manufacturer), "%s",
                updated_data->updateData[i].data.data_char); //field id 4
        break;
      case 5: // ApplicationVersion
        break;
      case 6: // LinuxKernelVersion
        break;
      case 7: // version
        converted_data->gwTableData.version = updated_data->updateData[i].data.data_uint8; //field id 7 version
        break;
      case 8: // lastEditTimeS
        converted_data->gwTableData.lastEditTimeS = updated_data->updateData[i].data.data_uint64; //field id 8 lastEditTime
        break;
      case 9: // type
        snprintf(converted_data->gwTableData.gwConfig.type,
              sizeof(converted_data->gwTableData.gwConfig.type), "%s",
              updated_data->updateData[i].data.data_char); //field id 9
        break;
      case 10: // gatewayMode
        converted_data->gwTableData.gwConfig.gatewayMode = updated_data->updateData[i].data.data_uint8; //field id 10 gatewayMode
        break;
      case 11: // ble1TxPower
        converted_data->gwTableData.gwConfig.bleConfig.ble1TxPower = updated_data->updateData[i].data.data_uint8; //field id 11 ble1TxPower
        break;
      case 12: // ble2TxPower
        converted_data->gwTableData.gwConfig.bleConfig.ble2TxPower = updated_data->updateData[i].data.data_uint8; //field id 12 ble2TxPower
        break;
      case 13: // ble1Antenna
        converted_data->gwTableData.gwConfig.bleConfig.ble1Antenna = updated_data->updateData[i].data.data_uint8; //field id 13 ble1Antenna
        break;
      case 14: // ble2Antenna
        converted_data->gwTableData.gwConfig.bleConfig.ble2Antenna = updated_data->updateData[i].data.data_uint8; //field id 14 ble2Antenna
        break;
      case 15: // BLEFilterListNameNumber
        break;
      case 16: // bleDuplicatedDrop_ms
        converted_data->gwTableData.gwConfig.bleConfig.bleDuplicatedDrop_ms = updated_data->updateData[i].data.data_uint32; //field id 16 bleDuplicatedDrop
        break;
      case 17: // needTimestampForRecBeacon
        converted_data->gwTableData.gwConfig.timeConfig.needTimestampForRecBeacon = updated_data->updateData[i].data.data_uint8; //field id 17 timestamp. to be confirmed.
        break;
      case 18: // timeSetting
        converted_data->gwTableData.gwConfig.timeConfig.timeSetting = updated_data->updateData[i].data.data_uint8; //field id 18 timeSetting
        break;
      case 19: // timeNtpUrl
        snprintf(converted_data->gwTableData.gwConfig.timeConfig.timeNtpUrl,
                256 - 1, "%s",
                updated_data->updateData[i].data.data_char); //field id 19
        break;
      case 20: // mqttSecurityType
        converted_data->gwTableData.gwConfig.mqttConfig.mqttSecurityType = updated_data->updateData[i].data.data_uint8; //field id 20 mqttSecurityType
        break;
      case 21: // MQTTHost
        snprintf(converted_data->gwTableData.gwConfig.mqttConfig.MQTTHost,
                256 - 1, "%s",
                updated_data->updateData[i].data.data_char); //field id 21
        break;
      case 22: // MQTTPort
        converted_data->gwTableData.gwConfig.mqttConfig.MQTTPort = updated_data->updateData[i].data.data_uint32; //field id 22 MQTTPort
        break;
      case 23: // DHCPEnable
        converted_data->gwTableData.gwConfig.ipConfig.DHCPEnable = updated_data->updateData[i].data.data_uint8; //field id 23 DHCPEnable
        break;
      case 24: // staticIPAddr
        snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.staticIPAddr.addr.addrArray,
                sizeof(converted_data->gwTableData.gwConfig.ipConfig.staticIPAddr.addr.addrArray), "%s",
                updated_data->updateData[i].data.data_char); //field id 24
        break;
      case 25: // netMask
        snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.netMask.addr.addrArray,
                sizeof(converted_data->gwTableData.gwConfig.ipConfig.netMask.addr.addrArray), "%s",
                updated_data->updateData[i].data.data_char); //field id 25
        break;
      case 26: // dnsServer
        snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.dnsServer.addr.addrArray,
                sizeof(converted_data->gwTableData.gwConfig.ipConfig.dnsServer.addr.addrArray), "%s",
                updated_data->updateData[i].data.data_char); //field id 26
        break;
      case 27: // gatewayAddr
        snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.gatewayAddr.addr.addrArray,
                sizeof(converted_data->gwTableData.gwConfig.ipConfig.gatewayAddr.addr.addrArray), "%s",
                updated_data->updateData[i].data.data_char); //field id 27
        break;
      case 28: // baudrateSlave
        converted_data->gwTableData.gwConfig.modbusConfig.baudrateSlave = updated_data->updateData[i].data.data_uint32; //field id 28 baudrateSlave
        break;
      case 29: // slaveAddrSlave
        converted_data->gwTableData.gwConfig.modbusConfig.slaveAddrSlave = updated_data->updateData[i].data.data_uint32; //field id 29 slaveAddrSlave
        break;
      case 30: // paritySlave
        converted_data->gwTableData.gwConfig.modbusConfig.paritySlave = updated_data->updateData[i].data.data_uint8; //field id 30 paritySlave
        break;
      case 31: // stopSlave
        converted_data->gwTableData.gwConfig.modbusConfig.stopSlave = updated_data->updateData[i].data.data_uint8; //field id 31 stopSlave
        break;
      case 32: // registerMode
        converted_data->gwTableData.gwConfig.modbusConfig.registerMode = updated_data->updateData[i].data.data_uint8; //field id 32 registerMode
        break;
      case 33: // description
        snprintf(converted_data->gwTableData.gwConfig.description,
                sizeof(converted_data->gwTableData.gwConfig.description), "%s",
                updated_data->updateData[i].data.data_char); //field id 33 description      
      default:
        LOG_WARN(OUTPOINT, "gateway wrong field num:%d!\r\n", updated_data->updateData[i].field);
        break;
      }
    }
#if (1) // for debug only, remove it later
    // if(updated_data->dataNum == 25) { // init modbus params in case they are not available on debug purpose
    //   converted_data->gwTableData.gwConfig.modbusConfig.baudrateSlave = 115200; //field id 28 baudrateSlave
    //   converted_data->gwTableData.gwConfig.modbusConfig.slaveAddrSlave = 0; //field id 29 slaveAddrSlave
    //   converted_data->gwTableData.gwConfig.modbusConfig.paritySlave = 0; //field id 30 paritySlave
    //   converted_data->gwTableData.gwConfig.modbusConfig.registerMode = 0; //field id 31 registerMode
    //   converted_data->gwTableData.gwConfig.modbusConfig.stopSlave = 0; //field id 32 stopSlave
    // }
    LOG_DEBUG(OUTPOINT, "Gateway update converted: \r\n \
                          nameid: %d\r\n \
                          name:%s\r\n \
                          macAddr:%s\r\n \
                          manufacturer: %d\r\n \
                          version:%d\r\n \
                          lastEditTimeS:%lld\r\n \
                          type: %s\r\n \
                          gatewayMode:%d\r\n \
                          ble1TxPower:%d\r\n \
                          ble2TxPower: %d\r\n \
                          ble1Antenna:%d\r\n \
                          ble2Antenna:%d\r\n \
                          bleDuplicatedDrop_ms: %d\r\n \
                          needTimestampForRecBeacon:%d\r\n \
                          timeSetting:%d\r\n \
                          timeNtpUrl: %s\r\n \
                          mqttSecurityType:%d\r\n \
                          MQTTHost:%s\r\n \
                          MQTTPort: %d\r\n \
                          DHCPEnable:%d\r\n \
                          staticIPAddr:%s\r\n \
                          netMask: %s\r\n \
                          dnsServer:%s\r\n \
                          gatewayAddr:%s\r\n \
                          baudrateSlave:%d\r\n \
                          slaveAddrSlave:%d\r\n \
                          paritySlave: %d\r\n \
                          stopSlave:%d\r\n \
                          registerMode:%d\r\n \
                          description:%s\r\n",
                          converted_data->gwTableData.gwConfig.nameId,
                          converted_data->gwTableData.gwConfig.name,
                          converted_data->gwTableData.gwConfig.macAddr.addr.addrArray,
                          converted_data->gwTableData.gwConfig.manufacturer,
                          converted_data->gwTableData.version,
                          converted_data->gwTableData.lastEditTimeS,
                          converted_data->gwTableData.gwConfig.type,
                          converted_data->gwTableData.gwConfig.gatewayMode,
                          converted_data->gwTableData.gwConfig.bleConfig.ble1TxPower,
                          converted_data->gwTableData.gwConfig.bleConfig.ble2TxPower,
                          converted_data->gwTableData.gwConfig.bleConfig.ble1Antenna,
                          converted_data->gwTableData.gwConfig.bleConfig.ble2Antenna,
                          converted_data->gwTableData.gwConfig.bleConfig.bleDuplicatedDrop_ms,
                          converted_data->gwTableData.gwConfig.timeConfig.needTimestampForRecBeacon,
                          converted_data->gwTableData.gwConfig.timeConfig.timeSetting,
                          converted_data->gwTableData.gwConfig.timeConfig.timeNtpUrl,
                          converted_data->gwTableData.gwConfig.mqttConfig.mqttSecurityType,
                          converted_data->gwTableData.gwConfig.mqttConfig.MQTTHost,
                          converted_data->gwTableData.gwConfig.mqttConfig.MQTTPort,
                          converted_data->gwTableData.gwConfig.ipConfig.DHCPEnable,
                          converted_data->gwTableData.gwConfig.ipConfig.staticIPAddr.addr.addrArray,
                          converted_data->gwTableData.gwConfig.ipConfig.netMask.addr.addrArray,
                          converted_data->gwTableData.gwConfig.ipConfig.dnsServer.addr.addrArray,
                          converted_data->gwTableData.gwConfig.ipConfig.gatewayAddr.addr.addrArray,
                          converted_data->gwTableData.gwConfig.modbusConfig.baudrateSlave,
                          converted_data->gwTableData.gwConfig.modbusConfig.slaveAddrSlave,
                          converted_data->gwTableData.gwConfig.modbusConfig.paritySlave,
                          converted_data->gwTableData.gwConfig.modbusConfig.registerMode,
                          converted_data->gwTableData.gwConfig.modbusConfig.stopSlave,
                          converted_data->gwTableData.gwConfig.description);
#endif
  } break;
  case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE: {
    appShmDataItem_t *converted_data = NULL;
    converted_data = (appShmDataItem_t *)converted_data_p;
    if(updated_data->dataNum != (sizeof(DeviceList) / sizeof(DeviceList[0]))) {
      LOG_WARN(OUTPOINT, "dev table wrong item num:%d!\r\n", updated_data->dataNum);
      // TODO
    }
    converted_data->appShmDataType = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
    for(uint8_t i=0;i<updated_data->dataNum;i++) {
      switch (updated_data->updateData[i].field) {
        case 1: // nameId
          converted_data->devTableData.nameId = updated_data->updateData[i].data.data_uint32; //field id 1 nameId
          break;
        case 2: // name
          snprintf(converted_data->devTableData.name,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  updated_data->updateData[i].data.data_char); //field id 2 name
          break;
        case 3: // BLESensorName
          snprintf(converted_data->devTableData.BLESensorName,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  updated_data->updateData[i].data.data_char); //field id 3 BLESensorName
          break;
        case 4: // macAddr
          snprintf(converted_data->devTableData.macAddr,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  updated_data->updateData[i].data.data_char); //field id 4 macAddr
          break;
        case 5: // manufacturer
          snprintf(converted_data->devTableData.manufacturer,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  updated_data->updateData[i].data.data_char); //field id 5 manufacturer
          break;
        case 6: // type
          snprintf(converted_data->devTableData.type,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  updated_data->updateData[i].data.data_char); //field id 6 type
          break;
        case 7: // description
          snprintf(converted_data->devTableData.description,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  updated_data->updateData[i].data.data_char); //field id 7 description
          break;
        default: // not found
          LOG_WARN(OUTPOINT, "dev wrong field num:%d!\r\n", updated_data->updateData[i].field);
          break;
      }
    }
#if (1) // for debug only, remove it later
    LOG_DEBUG(OUTPOINT, "Dev query converted: \r\n \
                          nameid: %d\r\n \
                          name:%s\r\n \
                          BLESensorName:%s\r\n \
                          macAddr: %s\r\n \
                          manufacturer:%s\r\n \
                          type:%s\r\n \
                          description:%s\r\n",
                          converted_data->devTableData.nameId,
                          converted_data->devTableData.name,
                          converted_data->devTableData.BLESensorName,
                          converted_data->devTableData.macAddr,
                          converted_data->devTableData.manufacturer,
                          converted_data->devTableData.type,
                          converted_data->devTableData.description);
#endif
  } break;
  case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE: {
    if(updated_data->dataNum != (sizeof(SensorConfig) / sizeof(SensorConfig[0]))) {
      LOG_WARN(OUTPOINT, "sensorConfig table wrong item num:%d!\r\n", updated_data->dataNum);
      // TODO
    }
    // converted_data->appShmDataType = GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE;
    // TODO
  } break;
  case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE: {
    sensorDataTableData_t *converted_data = NULL;
    converted_data = (sensorDataTableData_t *)converted_data_p;
    bool testflag = false;
    if(updated_data->dataNum != (sizeof(SensorData) / sizeof(SensorData[0]))) {
      LOG_WARN(OUTPOINT, "sensordata table wrong item num:%d!\r\n", updated_data->dataNum);
      // TODO
    }
    // converted_data->appShmDataType = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
    for(uint8_t i=0;i<updated_data->dataNum;i++) {
      switch (updated_data->updateData[i].field) {
        case 1: // sequenceNumber
          converted_data->sequenceNumber = updated_data->updateData[i].data.data_uint64; //field id 1 sequenceNumber
          break;
        case 2: // nameId
          converted_data->nameId = updated_data->updateData[i].data.data_uint32; //field id 2 nameId
          break;
        case 3: // BLESensorName
          snprintf(converted_data->BLESensorName,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  updated_data->updateData[i].data.data_char); //field id 3 BLESensorName
          break;
        case 4: // macAddr
          snprintf(converted_data->macAddr,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  updated_data->updateData[i].data.data_char); //field id 4 macAddr
          break;
        case 5: // type
          snprintf(converted_data->type,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  updated_data->updateData[i].data.data_char); //field id 5 type
          break;
        case 6: // manufacturer
          snprintf(converted_data->manufacturer,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                  updated_data->updateData[i].data.data_char); //field id 6 manufacturer
          break;
        case 7: // measureSeqno
          converted_data->measureSeqno = updated_data->updateData[i].data.data_uint32; //field id 7 measureSeqno
          break;
        case 8: // sampleTime
          converted_data->sampleTime = updated_data->updateData[i].data.data_uint64; //field id 8 sampleTime
          break;
        case 9: // receivedTimestamp
          converted_data->receivedTimestamp = updated_data->updateData[i].data.data_uint64; //field id 9 receivedTimestamp
          break;
        case 10: // measurement
          converted_data->measurement = updated_data->updateData[i].data.data_uint8; //field id 10 measurement
          break;
        case 11: // dataType
          converted_data->dataType = updated_data->updateData[i].data.data_uint8; //field id 11 dataType
          break;
        case 12: // format
          snprintf(converted_data->format,
                  sizeof(converted_data->format), "%s",
                  updated_data->updateData[i].data.data_char); //field id 12 format    
          if (0 == strcmp(updated_data->updateData[i].data.data_char, "Digit")) {
            testflag = true;
          }
          break;
        case 13: // range
          converted_data->range = updated_data->updateData[i].data.data_uint8; //field id 13 range
          break;
        case 14: // unit
          converted_data->unit = updated_data->updateData[i].data.data_uint8; //field id 14 unit
          break;
        case 15: // measureLengthSample
          converted_data->measureLengthSample = updated_data->updateData[i].data.data_uint32; //field id 15 measureLengthSample
          break;
        case 16: // totalDataLengthSample
          converted_data->totalDataLengthSample = updated_data->updateData[i].data.data_uint32; //field id 16 totalDataLengthSample
          break;
        case 17: // dimension
          converted_data->dimension = updated_data->updateData[i].data.data_uint32; //field id 17 dimension
          break;
        case 18: // dataFormat
          converted_data->dataFormat = updated_data->updateData[i].data.data_uint8; //field id 18 dataFormat
          break;
        case 19: // samplePeriods
          converted_data->samplePeriods = updated_data->updateData[i].data.data_uint32; //field id 19 samplePeriods
          break;
        case 20: // encryption
          converted_data->encryption = updated_data->updateData[i].data.data_uint8; //field id 20 encryption
          break;
        case 21: // value
          if(testflag) {
            converted_data->value.data_float = updated_data->updateData[i].data.data_float; //field id 21 value
          } else {
            snprintf(converted_data->value.data_char,
                    GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
                    updated_data->updateData[i].data.data_char); //field id 21 value
          }
          break;
        case 22: // sampleRate
          converted_data->sampleRate = updated_data->updateData[i].data.data_float; //field id 22 sampleRate
          break;
        case 23: // product
          converted_data->product = updated_data->updateData[i].data.data_uint8; //field id 23 product
          break;
        case 24: // sensor
          converted_data->sensor = updated_data->updateData[i].data.data_uint8; //field id 24 sensor
          break;
        default: // not found
          LOG_WARN(OUTPOINT, "sensordata wrong field num:%d!\r\n", updated_data->updateData[i].field);
          break;
      }
    }    
#if (0) // for debug only, remove it later
    LOG_DEBUG(OUTPOINT, "sensordata update converted:\r\n \
                          sequenceNumber: %d\r\n \
                          nameid: %d\r\n \
                          BLESensorName:%s\r\n \
                          macAddr:%s\r\n \
                          type: %s\r\n \
                          manufacturer: %s\r\n \
                          measureSeqno:%d\r\n \
                          sampleTime:%lld\r\n \
                          receivedTimestamp:%lld\r\n \
                          measurement:%d\r\n \
                          dataType: %d\r\n \
                          format:%s\r\n \
                          range:%d\r\n \
                          unit: %d\r\n \
                          measureLengthSample:%d\r\n \
                          totalDataLengthSample:%d\r\n \
                          dimension: %d\r\n \
                          dataFormat:%d\r\n \
                          samplePeriods:%d\r\n \
                          encryption: %d\r\n \
                          sampleRate:%f\r\n \
                          product: %d\r\n \
                          sensor:%d\r\n",
                          converted_data->sequenceNumber,
                          converted_data->nameId,
                          converted_data->BLESensorName,
                          converted_data->macAddr,
                          converted_data->type,
                          converted_data->manufacturer,
                          converted_data->measureSeqno,
                          converted_data->sampleTime,
                          converted_data->receivedTimestamp,
                          converted_data->measurement,
                          converted_data->dataType,
                          converted_data->format,
                          converted_data->range,
                          converted_data->unit,
                          converted_data->measureLengthSample,
                          converted_data->totalDataLengthSample,
                          converted_data->dimension,
                          converted_data->dataFormat,
                          converted_data->samplePeriods,
                          converted_data->encryption,
                          converted_data->sampleRate,
                          converted_data->product,
                          converted_data->sensor);
    if(testflag) {
      printf("value:%f\r\n", converted_data->value);
    } else {
      printf("value:%s\r\n", converted_data->value);
    }

#endif
  } break;
  }
#else
  switch (updated_data->table) {
  case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE: {
    if(updated_data->dataNum != (sizeof(GatewayConfig) / sizeof(GatewayConfig[0]))) {
      LOG_WARN(OUTPOINT, "gw table wrong item num:%d!\r\n", updated_data->dataNum);
      // TODO
    }
    converted_data->appShmDataType = updated_data->table;
    converted_data->gwTableData.gwConfig.nameId = updated_data->updateData[0].data.data_uint32; //field id 1
#if (0)
    snprintf(converted_data->gwTableData.gwConfig.name,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[1].data.data_char); //field id 2
    snprintf((char *)converted_data->gwTableData.gwConfig.macAddr.addr.addrArray,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[2].data.data_char); //field id 3
    snprintf(converted_data->gwTableData.gwConfig.manufacturer,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[3].data.data_char); //field id 4
    // skip field 5,6 to be confirmed
    converted_data->gwTableData.version = updated_data->updateData[6].data.data_uint8; //field id 7 version
    converted_data->gwTableData.lastEditTimeS = updated_data->updateData[7].data.data_uint64; //field id 8 lastEditTime
    snprintf(converted_data->gwTableData.gwConfig.type,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[8].data.data_char); //field id 9
#else
    snprintf(converted_data->gwTableData.gwConfig.name,
            sizeof(converted_data->gwTableData.gwConfig.name), "%s",
            updated_data->updateData[1].data.data_char); //field id 2
    snprintf((char *)converted_data->gwTableData.gwConfig.macAddr.addr.addrArray,
            sizeof(converted_data->gwTableData.gwConfig.macAddr.addr.addrArray), "%s",
            updated_data->updateData[2].data.data_char); //field id 3
    snprintf(converted_data->gwTableData.gwConfig.manufacturer,
            sizeof(converted_data->gwTableData.gwConfig.manufacturer), "%s",
            updated_data->updateData[3].data.data_char); //field id 4
    // skip field 5,6 to be confirmed
    converted_data->gwTableData.version = updated_data->updateData[6].data.data_uint8; //field id 7 version
    converted_data->gwTableData.lastEditTimeS = updated_data->updateData[7].data.data_uint64; //field id 8 lastEditTime
    snprintf(converted_data->gwTableData.gwConfig.type,
            sizeof(converted_data->gwTableData.gwConfig.type), "%s",
            updated_data->updateData[8].data.data_char); //field id 9
#endif
    converted_data->gwTableData.gwConfig.gatewayMode = updated_data->updateData[9].data.data_uint8; //field id 10 gatewayMode
    converted_data->gwTableData.gwConfig.bleConfig.ble1TxPower = updated_data->updateData[10].data.data_uint8; //field id 11 ble1TxPower
    converted_data->gwTableData.gwConfig.bleConfig.ble2TxPower = updated_data->updateData[11].data.data_uint8; //field id 12 ble2TxPower
    converted_data->gwTableData.gwConfig.bleConfig.ble1Antenna = updated_data->updateData[12].data.data_uint8; //field id 13 ble1Antenna
    converted_data->gwTableData.gwConfig.bleConfig.ble2Antenna = updated_data->updateData[13].data.data_uint8; //field id 14 ble2Antenna
    // skip field 15 to be confirmed
    converted_data->gwTableData.gwConfig.bleConfig.bleDuplicatedDrop_ms = updated_data->updateData[15].data.data_uint32; //field id 16 bleDuplicatedDrop
    converted_data->gwTableData.gwConfig.timeConfig.needTimestampForRecBeacon = updated_data->updateData[16].data.data_uint8; //field id 17 timestamp. to be confirmed.
    converted_data->gwTableData.gwConfig.timeConfig.timeSetting = updated_data->updateData[17].data.data_uint8; //field id 18 timeSetting
    snprintf(converted_data->gwTableData.gwConfig.timeConfig.timeNtpUrl,
            256 - 1, "%s",
            updated_data->updateData[18].data.data_char); //field id 19
    converted_data->gwTableData.gwConfig.mqttConfig.mqttSecurityType = updated_data->updateData[19].data.data_uint8; //field id 20 mqttSecurityType
    snprintf(converted_data->gwTableData.gwConfig.mqttConfig.MQTTHost,
            256 - 1, "%s",
            updated_data->updateData[20].data.data_char); //field id 21
    converted_data->gwTableData.gwConfig.mqttConfig.MQTTPort = updated_data->updateData[21].data.data_uint32; //field id 22 MQTTPort
    converted_data->gwTableData.gwConfig.ipConfig.DHCPEnable = updated_data->updateData[22].data.data_uint8; //field id 23 DHCPEnable
#if (0)
    snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.staticIPAddr.addr.addrArray,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[23].data.data_char); //field id 24
    snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.netMask.addr.addrArray,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[24].data.data_char); //field id 25
    snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.dnsServer.addr.addrArray,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[25].data.data_char); //field id 26
    snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.gatewayAddr.addr.addrArray,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[26].data.data_char); //field id 27
#else
    snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.staticIPAddr.addr.addrArray,
            sizeof(converted_data->gwTableData.gwConfig.ipConfig.staticIPAddr.addr.addrArray), "%s",
            updated_data->updateData[23].data.data_char); //field id 24
    snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.netMask.addr.addrArray,
            sizeof(converted_data->gwTableData.gwConfig.ipConfig.netMask.addr.addrArray), "%s",
            updated_data->updateData[24].data.data_char); //field id 25
    snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.dnsServer.addr.addrArray,
            sizeof(converted_data->gwTableData.gwConfig.ipConfig.dnsServer.addr.addrArray), "%s",
            updated_data->updateData[25].data.data_char); //field id 26
    snprintf((char *)converted_data->gwTableData.gwConfig.ipConfig.gatewayAddr.addr.addrArray,
            sizeof(converted_data->gwTableData.gwConfig.ipConfig.gatewayAddr.addr.addrArray), "%s",
            updated_data->updateData[26].data.data_char); //field id 27
#endif
    converted_data->gwTableData.gwConfig.mqttConfig.MQTTPort = updated_data->updateData[27].data.data_uint32; //field id 28 MQTTPort
    converted_data->gwTableData.gwConfig.modbusConfig.baudrateSlave = updated_data->updateData[28].data.data_uint32; //field id 29 baudrateSlave
    converted_data->gwTableData.gwConfig.modbusConfig.slaveAddrSlave = updated_data->updateData[29].data.data_uint32; //field id 30 slaveAddrSlave
    converted_data->gwTableData.gwConfig.modbusConfig.paritySlave = updated_data->updateData[30].data.data_uint8; //field id 31 paritySlave
    converted_data->gwTableData.gwConfig.modbusConfig.registerMode = updated_data->updateData[31].data.data_uint8; //field id 32 registerMode
    converted_data->gwTableData.gwConfig.modbusConfig.stopSlave = updated_data->updateData[32].data.data_uint8; //field id 33 stopSlave
  } break;
  case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE: {
    if(updated_data->dataNum != (sizeof(DeviceList) / sizeof(DeviceList[0]))) {
      LOG_WARN(OUTPOINT, "dev table wrong item num:%d!\r\n", updated_data->dataNum);
      // TODO
    }
    converted_data->appShmDataType = updated_data->table;
    converted_data->devTableData.nameId = updated_data->updateData[0].data.data_uint32; //field id 1 nameId
    snprintf(converted_data->devTableData.name,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[1].data.data_char); //field id 2 name
    snprintf(converted_data->devTableData.BLESensorName,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[2].data.data_char); //field id 3 BLESensorName
    snprintf(converted_data->devTableData.macAddr,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[3].data.data_char); //field id 4 macAddr
    snprintf(converted_data->devTableData.manufacturer,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[4].data.data_char); //field id 5 manufacturer
    snprintf(converted_data->devTableData.type,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[5].data.data_char); //field id 6 type
  } break;
  case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE: {
    if(updated_data->dataNum != (sizeof(SensorConfig) / sizeof(SensorConfig[0]))) {
      LOG_WARN(OUTPOINT, "sensorConfig table wrong item num:%d!\r\n", updated_data->dataNum);
      // TODO
    }
    converted_data->appShmDataType = updated_data->table;
    // TODO
  } break;
  case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE: {
    bool testflag = false;
    if(updated_data->dataNum != (sizeof(SensorData) / sizeof(SensorData[0]))) {
      LOG_WARN(OUTPOINT, "gw table wrong item num:%d!\r\n", updated_data->dataNum);
      // TODO
    }
    converted_data->appShmDataType = updated_data->table;
    converted_data->sequenceNumber = updated_data->updateData[0].data.data_uint64; //field id 1 sequenceNumber
    converted_data->nameId = updated_data->updateData[1].data.data_uint32; //field id 2 nameId
    snprintf(converted_data->BLESensorName,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[2].data.data_char); //field id 3 BLESensorName
    snprintf(converted_data->macAddr,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[3].data.data_char); //field id 4 macAddr
    snprintf(converted_data->type,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[4].data.data_char); //field id 5 type
    snprintf(converted_data->manufacturer,
            GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
            updated_data->updateData[5].data.data_char); //field id 6 manufacturer
    converted_data->measureSeqno = updated_data->updateData[6].data.data_uint32; //field id 7 measureSeqno
    converted_data->sampleTime = updated_data->updateData[7].data.data_uint64; //field id 8 sampleTime
    converted_data->receivedTimestamp = updated_data->updateData[8].data.data_uint64; //field id 9 receivedTimestamp
    converted_data->measurement = updated_data->updateData[9].data.data_uint8; //field id 10 measurement
    converted_data->dataType = updated_data->updateData[10].data.data_uint8; //field id 11 dataType
    snprintf(converted_data->format,
            sizeof(converted_data->format), "%s",
            updated_data->updateData[11].data.data_char); //field id 12 format
    if (0 == strcmp(updated_data->updateData[11].data.data_char, "Digit")) {
      testflag = true;
    }
    converted_data->range = updated_data->updateData[12].data.data_uint8; //field id 13 range
    converted_data->unit = updated_data->updateData[13].data.data_uint8; //field id 14 unit
    converted_data->measureLengthSample = updated_data->updateData[14].data.data_uint32; //field id 15 measureLengthSample
    converted_data->totalDataLengthSample = updated_data->updateData[15].data.data_uint32; //field id 16 totalDataLengthSample
    converted_data->dimension = updated_data->updateData[16].data.data_uint32; //field id 17 dimension
    converted_data->dataFormat = updated_data->updateData[17].data.data_uint8; //field id 18 dataFormat
    converted_data->samplePeriods = updated_data->updateData[18].data.data_uint32; //field id 19 samplePeriods
    converted_data->encryption = updated_data->updateData[19].data.data_uint8; //field id 20 encryption
    if(testflag) {
      converted_data->value.data_float = updated_data->updateData[20].data.data_float; //field id 21 value
    } else {
      snprintf(converted_data->value.data_char,
              GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s",
              updated_data->updateData[20].data.data_char); //field id 21 value
    }
    converted_data->sampleRate = updated_data->updateData[21].data.data_float; //field id 22 sampleRate
    converted_data->product = updated_data->updateData[22].data.data_uint8; //field id 23 product
    converted_data->sensor = updated_data->updateData[23].data.data_uint8; //field id 24 sensor
  } break;
  }
#endif  // end of #if(1) within table_update_data_convert()
}
#endif
// for logging
uint8_t g_log_level = CONFIG_LOG_LEVEL_SET;
#define LOG_LEVEL_PARAMETER_IDX (1)

int
main(
  int argc,
  char **argv)
{
  gw_pro_response_t status = gw_pro_response_success;
  gw_pro_process_type_t to = gw_pro_process_mqtt;
  sqlite_query_callback query_cb = NULL;
  sqlite_query_callback update_cb = NULL;
  LOG_INFO(OUTPOINT, "sql process started ...\n\r");
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
  int qid = 0;
  uint16_t data_num = 0;
  int report_qid = 0;
  char tempmac[50] = {0};
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)msgbufp.mtext;
  int ret = -1;
  create_sql(DATABASE_NAME);
  mqtt_msgid = msgget(MSG_MQTT_QUEUE_KEY, IPC_CREAT | MSG_QUEUE_FLAG);
  sql_msgid = msgget(MSG_SQL_QUEUE_KEY, IPC_CREAT | MSG_QUEUE_FLAG);
  ble_msgid = msgget(MSG_BLE_QUEUE_KEY, IPC_CREAT | MSG_QUEUE_FLAG);
  if (sql_msgid == -1 || mqtt_msgid == -1) {
    LOG_WARN(OUTPOINT, "Get the msg queue failed\r\n");
    return 1;
  }
  LOG_INFO(OUTPOINT, "start sqlite!\r\n");
#if (ENABLE_MODBUS_FEATURE == 1)
  bool w_flag = false;  // true: the data was written into shm successfully, no further 'copy' needed after SQL operation accordingly
  bool find_flag = false; // true: nameId can be found in the 'chn_nameId' list
  bool sd_conn_flag = false; // true: for sensorData table, multi-connection only
  bool update_flag = false;
  bool backup_flag = false;
  uint8_t ch_idx = 0; // channel index;
  // 2 more variables for efficient mode
  uint16_t reg_addr_offset = 0;
  uint8_t addr_len = 0;
  struct sigevent shm_evp;
  struct itimerspec shm_ts;
/**
 * notes:
 *   1. for Insight-T sensor, 4 fields sensor data only:
 *      21, 13, 42, 29
 *
 */
// TODO: fetch the current data in db into share memory first
  int shm_id = 0;
  int stat = 0;
  shm_key = app_shmem_ftok();
  if(-1 == shm_key) {
    LOG_WARN(OUTPOINT, "generate key failed!\r\n");
    return 1;
  }
  if(-1 == app_shmem_shmget(shm_key)) {
    LOG_ERR(OUTPOINT, "[shm]fail to get shm! reboot!\r\n");
    system("/sbin/reboot");  // reboot directly
    return 1;
  }
  if(-1 == app_shmem_sem_open()) {
    LOG_WARN(OUTPOINT, "get named semaphore failed!\r\n");
    app_shmem_shmctl(IPC_RMID, NULL);
    return 1;
  }

  shm_addr = app_shmem_shmat(NULL, 0); // to be confirmed
  if(NULL == shm_addr) {
    app_shmem_sem_close();
    LOG_WARN(OUTPOINT, "map share memory error!\r\n");
    app_shmem_shmctl(IPC_RMID, NULL);
    return 1;
  }
  // check the size of share memory. restart sql if mis-matched
  // this may be caused by version mis-match between sql_op and modbus_slave
#if (0) // comment it out at the moment
  struct shmid_ds shmbuf;
  if((-1 == app_shmem_shmctl(IPC_RMID, &shmbuf)) ||
     (APP_SHM_TOTAL_SIZE != shmbuf.shm_segsz)) {
    LOG_WARN(OUTPOINT, "shm size(0x%x) mis-match,check versions of sql_op & modbus_slave please!\r\n", APP_SHM_TOTAL_SIZE);
    app_shmem_sem_close();
    app_shmem_shmctl(IPC_RMID, NULL);
    return 1;
  }
#endif

  // cleanup once it's re-started
  if(0 != strcmp((char *)(shm_addr + APP_SHM_MAGIC_DATA_OFFSET),APP_SHM_MAGIC_DATA)) {
    memset(shm_addr, 0, APP_SHM_TOTAL_SIZE);
    memcpy((shm_addr + APP_SHM_MAGIC_DATA_OFFSET) , APP_SHM_MAGIC_DATA , sizeof(APP_SHM_MAGIC_DATA));
  }
  // fetch gateway table
  memset(&query_data, 0, sizeof(query_data));
  memset(&shm_data, 0, sizeof(shm_data));
  status = sqlite_query(DATABASE_NAME, GATEWAY_CONFIGURE_TABLE, 0, gateway_cb);
  modbus_feature_enable = true; // enable modbus feature by default
  // Notes: do a check of size of share memory to avoid overlapping here
  LOG_INFO(OUTPOINT, "[shm check]sizeOfshmdata:%d, sizeOfdev:%d, sizeOfpiecedata:%d!\r\n", sizeof(shm_data), APP_SHM_DEV_TABLE_ITEM_SIZE, APP_SHM_SENSORDATA_TABLE_PIECE_SIZE);
  if((sizeof(shm_sensor_data) > APP_SHM_SENSORDATA_TABLE_PIECE_SIZE) || (sizeof(shm_data) > APP_SHM_DEV_TABLE_ITEM_SIZE)) {
    LOG_ERR(OUTPOINT, "[shm check]Error: size exceed to limit!shm_data:%d, shm_sensor_data:%d\r\n", sizeof(shm_data), sizeof(shm_sensor_data));
    app_shmem_sem_close();
    app_shmem_shmctl(IPC_RMID, NULL);
    return 1;
  }
  LOG_DEBUG(OUTPOINT, "[shm]query gw data!\r\n");
  if((gw_pro_response_success == status) &&
     (0 != query_data.inquiredData[0].data.data_int32)) { // nameid of returned item is not 0, that is, item for gateway exists in gateway table.
    // table_query_data_convert(GW_PRO_TABLE_IDX_GW_CONFIG_TABLE, &query_data, &shm_data);
    table_query_data_convert(GW_PRO_TABLE_IDX_GW_CONFIG_TABLE, &query_data, (void *)(&shm_data));
    LOG_DEBUG(OUTPOINT, "[shm]gw data converted!\r\n");
    // modbus feature enabled?
    if(0 == shm_data.gwTableData.gwConfig.modbusConfig.baudrateSlave) {
      LOG_INFO(OUTPOINT, "gw baudrate is 0, description:%s, disable modbus feature!\r\n", shm_data.gwTableData.gwConfig.description);
#if (1) // on debug purpose. update it later
      modbus_feature_enable = false;
      goto SKIP_FETCH_DB_DATA;
#else
      modbus_feature_enable = true;
      // g_modbus_reg_addr_mode = MODBUS_REG_ADDR_EFFICIENT_MODE;
#endif
    } else {
#if (0) // on debug purpose. update it later
      shm_data.gwTableData.gwConfig.modbusConfig.registerMode = (uint8_t)MODBUS_REG_ADDR_EFFICIENT_MODE;
#endif
      switch (shm_data.gwTableData.gwConfig.modbusConfig.registerMode)
      {
      case MODBUS_REG_ADDR_EXT_BW_BB_MODE: // extensible mode
      case MODBUS_REG_ADDR_EXT_BW_LB_MODE:
      case MODBUS_REG_ADDR_EXT_LW_BB_MODE:
      case MODBUS_REG_ADDR_EXT_LW_LB_MODE:
        LOG_INFO(OUTPOINT, "[shm]gw in ext mode, BR:%d!\r\n", shm_data.gwTableData.gwConfig.modbusConfig.baudrateSlave);
        // channel 0 is fixed for gateway
        chn_nameid_map[0].ch = 0;
        chn_nameid_map[0].validflag = 1;
        chn_nameid_map[0].nameid = shm_data.gwTableData.gwConfig.nameId;
        assigned_chn_cnt = 1;
        g_modbus_reg_addr_mode = MODBUS_REG_ADDR_EXTENSIBLE_MODE;
        break;
      case MODBUS_REG_ADDR_EFF_BW_BB_MODE: // efficient mode
      case MODBUS_REG_ADDR_EFF_BW_LB_MODE:
      case MODBUS_REG_ADDR_EFF_LW_BB_MODE:
      case MODBUS_REG_ADDR_EFF_LW_LB_MODE:
        LOG_INFO(OUTPOINT, "[shm]gw in efficient mode, BR:%d!\r\n", shm_data.gwTableData.gwConfig.modbusConfig.baudrateSlave);
        // channel 0 is fixed for gateway
        efficient_chn_nameid_map[0].ch = 0;
        efficient_chn_nameid_map[0].validflag = 1;
        efficient_chn_nameid_map[0].nameid = shm_data.gwTableData.gwConfig.nameId;
        efficient_chn_nameid_map[0].len = DEV_REG_ADDR_LENGTH_GW; // 3 for gateway in efficient mode
        efficient_chn_nameid_map[0].offset = MODBUS_REG_GATEWAY_START_ADDR_OFFSET;  // start from 0
        assigned_chn_cnt = 1;
        g_modbus_reg_addr_mode = MODBUS_REG_ADDR_EFFICIENT_MODE;
        break;      
      default:
        LOG_ERR(OUTPOINT, "[shm]wrong mode(%d), force set to ext mode(0) by default! BR:%d!\r\n", shm_data.gwTableData.gwConfig.modbusConfig.registerMode, shm_data.gwTableData.gwConfig.modbusConfig.baudrateSlave);
        break;
      }
    }
    LOG_INFO(OUTPOINT, "fetch gw data! id:%d, description:%s\r\n", shm_data.gwTableData.gwConfig.nameId, shm_data.gwTableData.gwConfig.description);
    memcpy((void *)(shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), (void *)&shm_data, sizeof(shm_data));
#if (1) // for debug only remove it later
    printf("gw data:\r\n");
    uint8_t *tmp = shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET;
    for(uint16_t i=0;i<sizeof(shm_data);i++)
      printf(" %02x", tmp[i]);
    printf("\r\nend\r\n");
#endif
  } else {  // skip fetching data from db in case starting from scratch
    LOG_WARN(OUTPOINT, "[shm]no gw item found! skip fetching further data!\r\n");
    goto SKIP_FETCH_DB_DATA;
  }
  // fetch dev table
  // TODO: fetch items in bulk way instead of fetching them 1 by 1
  // filter all the nameids of Dev first
  gw_pro_sqlite_cond_element_t sqliteFilterData = {0};  // 
  memset(&filter_data, 0, sizeof(filter_data));
  status = sqlite_filter(DATABASE_NAME, DEVICE_LIST_TABLE, 0, &sqliteFilterData, fliter_cb);
  if(filter_data.dataNum > BULLET_GW_SENSOR_NUM_MAX) {
    LOG_WARN(OUTPOINT, "[shm]dev item num(%d) exceed limit(%d)!\r\n", filter_data.dataNum, BULLET_GW_SENSOR_NUM_MAX);
    filter_data.dataNum = BULLET_GW_SENSOR_NUM_MAX;
  }
#if (1) // for debug only remove it later
  printf("dev nid list:\r\n");  // check whether they would be lited in fixed order
  for(uint8_t i=0;i<filter_data.dataNum;i++)
    printf("%d, ", filter_data.filteredItemIdx[i]);
  printf("\r\nend\r\n");
#endif
#if (0) // TODO:
  // if file exists, that means 1 or more 'del' operations happened. we need 'restore' the 'deleted' items accordingly
  if(-1 != access(FPATH_FOR_CH_NAMEID_FILE, 0)) { 
    open(FPATH_FOR_CH_NAMEID_FILE);
    read out the records for further restore;
    TODO;
  }
#endif

  // fetch items 1 by 1
  for(uint8_t i=0; i<BULLET_GW_SENSOR_NUM_MAX; i++) {
    if(0 == filter_data.filteredItemIdx[i]) {
      LOG_DEBUG(OUTPOINT, "[shm]dev item num(%d) end!\r\n", i);
      break;
    }
    memset(&query_data, 0, sizeof(query_data));
    memset(&shm_data, 0, sizeof(shm_data));
    status = sqlite_query(DATABASE_NAME, DEVICE_LIST_TABLE, filter_data.filteredItemIdx[i], devicelist_cb);
    if(gw_pro_response_success == status) { // nameid of returned item is not 0, that is, item for device exists in Dev table.
      // table_query_data_convert(GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE, &query_data, &shm_data);
      table_query_data_convert(GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE, &query_data, (void *)(&shm_data));
      LOG_INFO(OUTPOINT, "fetch dev data! idx:%d, id:%d, description:%s\r\n", i, shm_data.devTableData.nameId, shm_data.devTableData.description);
      uint8_t ch = 0;
      uint8_t len = 0;
      uint16_t offset = 0;
      char tmpstr[8] = {0}; // max is 4+1, so 8 is enough here
      // have a basic check first
      if((shm_data.devTableData.description[0] == 'C') && (shm_data.devTableData.description[1] == 'H')) { // description in "CHxxxx" format
        memcpy(tmpstr, &shm_data.devTableData.description[SENSOR_NAME_CHN_OFFSET], SENSOR_NAME_CHN_LENGTH);
        ch = (uint8_t)strtol(tmpstr, NULL, 16);
        memset(tmpstr, 0, 8);
        memcpy(tmpstr, &shm_data.devTableData.description[SENSOR_NAME_START_ADDR_OFFSET], SENSOR_NAME_START_ADDR_LENGTH);
        offset = (uint16_t)strtol(tmpstr, NULL, 16);
        memset(tmpstr, 0, 8);
        memcpy(tmpstr, &shm_data.devTableData.description[SENSOR_NAME_REG_LEN_OFFSET], SENSOR_NAME_REG_LEN_LENGTH);
        len = (uint8_t)strtol(tmpstr, NULL, 16);
        memset(tmpstr, 0, 8);
      } else {
        LOG_WARN(OUTPOINT, "dev description is not perfixed with CH! nid:%d, skip it!\r\n", shm_data.devTableData.nameId);
        continue;
      }
#define CONFIG_TO_BE_CONFIRMED
      switch (g_modbus_reg_addr_mode)
      {
      case MODBUS_REG_ADDR_EXTENSIBLE_MODE: // extensible mode
        // TODO: check whether the format is valid or not. skip it if not
#ifdef CONFIG_TO_BE_CONFIRMED
        // check validation by 'len'
        if((DEV_REG_ADDR_LENGTH_EXT_MODE    != len) &&
           (DEV_REG_ADDR_LENGTH_INSIGHT_T   != len) &&
           (DEV_REG_ADDR_LENGTH_INSIGHT_P   != len) &&
           (DEV_REG_ADDR_LENGTH_INSIGHT_PRO != len)) {
          LOG_WARN(OUTPOINT, "ext:len(0x%02x) in description is not 0x%02x! nid:%d, skip it!\r\n", len, DEV_REG_ADDR_LENGTH_EXT_MODE, shm_data.devTableData.nameId);
          continue;
        }
#else
        if(DEV_REG_ADDR_LENGTH_EXT_MODE != len) {
          LOG_WARN(OUTPOINT, "length(0x%02x) in description is not 0x%02x! nid:%d, skip it!\r\n", len, DEV_REG_ADDR_LENGTH_EXT_MODE, shm_data.devTableData.nameId);
          continue;
        }
#endif
#if (1)
        // TODO: check whether there is duplicated item or not later
        if((ch > 0) && (ch < (BULLET_GW_SENSOR_NUM_MAX + 1))) {
          if((1 == chn_nameid_map[ch].validflag) && (ch == chn_nameid_map[ch].ch)) { // to be confirmed
            // LOG_WARN(OUTPOINT, "conflict dev: chn:%d is alreay asigned to nid:%d, replace former one!\r\n", ch, chn_nameid_map[ch].nameid);
            LOG_WARN(OUTPOINT, "ext: conflict dev: chn:%d is alreay asigned to nid:%d, skip this dev!\r\n", ch, chn_nameid_map[ch].nameid);
            continue;
          }
          chn_nameid_map[ch].validflag = 1;
          chn_nameid_map[ch].ch = ch;
          chn_nameid_map[ch].nameid = shm_data.devTableData.nameId;
          LOG_DEBUG(OUTPOINT, "ext: dev i:%d, chn:%d, nid:%d\r\n", i, ch, chn_nameid_map[ch].nameid);
        } else {
          LOG_WARN(OUTPOINT, "ext: Wrong dev i:%d, chn:%d, nid:%d\r\n", i, ch, shm_data.devTableData.nameId);
          continue;
        }
#else
        chn_nameid_map[i+1].ch = (uint8_t)strtol(tmpstr, NULL, 16);
        chn_nameid_map[i+1].nameid = shm_data.devTableData.nameId;
        LOG_DEBUG(OUTPOINT, "dev idx:%d, chn:%d\r\n", i, chn_nameid_map[i+1].ch);
#endif
        assigned_chn_cnt++;
        break;
      case MODBUS_REG_ADDR_EFFICIENT_MODE: // efficient mode                                                                                                                                                                 fficient mode
        // TODO: check whether the format is valid or not. skip it if not
#ifdef CONFIG_TO_BE_CONFIRMED
        // check validation by 'len'
        if((DEV_REG_ADDR_LENGTH_EXT_MODE    != len) &&
           (DEV_REG_ADDR_LENGTH_INSIGHT_T   != len) &&
           (DEV_REG_ADDR_LENGTH_INSIGHT_P   != len) &&
           (DEV_REG_ADDR_LENGTH_INSIGHT_PRO != len)) {
          LOG_WARN(OUTPOINT, "eff: len(0x%02x) in description is not 0x%02x! nid:%d, skip it!\r\n", len, DEV_REG_ADDR_LENGTH_EXT_MODE, shm_data.devTableData.nameId);
          continue;
        }
#else
        if((DEV_REG_ADDR_LENGTH_INSIGHT_T != len) && (DEV_REG_ADDR_LENGTH_INSIGHT_P != len)) {
          LOG_WARN(OUTPOINT, "length(0x%02x) in description is invalid! nid:%d, skip it!\r\n", len, shm_data.devTableData.nameId);
          continue;
        }
#endif
       if((ch > 0) && (ch < (BULLET_GW_SENSOR_NUM_MAX + 1))) {
          if((1 == efficient_chn_nameid_map[ch].validflag) && (ch == efficient_chn_nameid_map[ch].ch)) { // to be confirmed
            LOG_WARN(OUTPOINT, "eff: conflict dev: chn:%d is alreay asigned to nid:%d, skip this dev!!!\r\n", ch, efficient_chn_nameid_map[ch].nameid);
            continue;
          }
          // TODO: do more verification later ??? (regAddr and length)
          efficient_chn_nameid_map[ch].validflag = 1;
          efficient_chn_nameid_map[ch].ch = ch;
          efficient_chn_nameid_map[ch].nameid = shm_data.devTableData.nameId;
          efficient_chn_nameid_map[ch].offset = offset;
          efficient_chn_nameid_map[ch].len = len;
          LOG_DEBUG(OUTPOINT, "eff: dev i:%d, chn:%d, nid:%d, offset:0x%04x, len:0x%02x\r\n", i, ch, efficient_chn_nameid_map[ch].nameid, offset, len);
        } else {
          LOG_WARN(OUTPOINT, "eff: Wrong dev i:%d, chn:%d, nid:%d\r\n", i, ch, shm_data.devTableData.nameId);
          continue;
        }
        assigned_chn_cnt++;
        break;  
      default:
        LOG_ERR(OUTPOINT, "[shm init]wrong mode(%d)\r\n", g_modbus_reg_addr_mode);
        break;
      }
      memcpy((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), (void *)&shm_data, sizeof(shm_data));
    } else {  // skip fetching data from db in case starting from scratch
      LOG_WARN(OUTPOINT, "[shm]no more dev item found! skip fetching further data!\r\n");
      goto SKIP_FETCH_DB_DATA;
    }
  }
  // skip fetching sensor & sensorData table at the moment.
  // TODO: not necessary. to be confirmed
SKIP_FETCH_DB_DATA:
  // release semaphore untill we fetch the data from db
  if(modbus_feature_enable) {
    app_shmem_sem_post();
  }
#endif
  LOG_INFO(OUTPOINT, "[sharemmemory]sql_op enter loop!\r\n");
while (1) {
    // wait the msg
    LOG_INFO(OUTPOINT, "wait rev!\r\n");
    itemfalg = false;
    exeflag = false;
    memset(&msgbufp, 0, sizeof(msgbufp));
    if (recv_msg(sql_msgid, &msgbufp, 0))
      continue;
    switch (pdata->command_or_response.commandMsg.command) {
    case gw_pro_command_sqlite_update_item: {
      LOG_INFO(OUTPOINT, "update\r\n");
      global_tb_name = pdata->command_or_response.commandMsg.param
                           .sqlite_update_item_param.table;
      if (global_tb_name == GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE ||
          global_tb_name == GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE)
        snprintf(table_name, sizeof(table_name) - 1, "%s", DEVICE_LIST_TABLE);
      else
        gain_tablename(global_tb_name, table_name, sizeof(table_name) - 1);
      if (global_tb_name == GW_PRO_TABLE_IDX_GW_CONFIG_TABLE)
        pdata->command_or_response.commandMsg.param.sqlite_update_item_param
            .itemIdx = 0;
      // To speed up data insertion, no need to check whether the item 
      // has been existed
      if (global_tb_name != GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE)
        status = sqlite_query(DATABASE_NAME, table_name,
                              pdata->command_or_response.commandMsg.param
                                .sqlite_update_item_param.itemIdx,
                              Item_check_cb);
      else
        itemfalg = true;
      if (GW_PRO_TABLE_IDX_GW_CONFIG_TABLE == global_tb_name && itemfalg) {
        for (uint8_t i = 0; i < pdata->command_or_response.commandMsg.param
                                    .sqlite_update_item_param.dataNum;
             i++) {
          if (3 == pdata->command_or_response.commandMsg.param
                       .sqlite_update_item_param.updateData[i]
                       .field) {
            memset(tempmac, 0, sizeof(tempmac));
            Gain_clientID(pdata->command_or_response.commandMsg.param
                              .sqlite_update_item_param.updateData[i]
                              .data.data_char,
                          tempmac);
            if (strcmp(tempmac, gwconfigmac)) {
              LOG_INFO(OUTPOINT, "Invalid gwconfig!\r\n");
              goto res;
            }
            status = sqlite_delete(DATABASE_NAME, 
                                   table_name,
                                   gwconfigid,
                                   0,
                                   &pdata->command_or_response.commandMsg.param
                                   .sqlite_delete_item_param.deleteData);
            itemfalg = false;
          }
        }
      }

      if (status)
        goto res;
      if (global_tb_name == GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE) {
        if (!itemfalg)
          goto res;
        itemfalg = false;
        snprintf(table_name, sizeof(table_name) - 1, "%s", SENSOR_DATA_TABLE);
        data_num = pdata->command_or_response.commandMsg.param
                       .sqlite_update_item_param.dataNum;
        for (uint8_t i = 0; i < data_num; i++) {
          if ((pdata->command_or_response.commandMsg.param
                       .sqlite_update_item_param.updateData[i]
                       .field == 1 &&
               pdata->command_or_response.commandMsg.param
                       .sqlite_update_item_param.updateData[i]
                       .data.data_uint64 > 0)) {
            itemfalg = true;
          }
          if (pdata->command_or_response.commandMsg.param
                  .sqlite_update_item_param.updateData[i]
                  .field == 25) {
            pdata->command_or_response.commandMsg.param.sqlite_update_item_param
                .itemIdx = pdata->command_or_response.commandMsg.param
                               .sqlite_update_item_param.updateData[1]
                               .data.data_uint64;
            pdata->command_or_response.commandMsg.param.sqlite_update_item_param
                .dataNum = 1;
          }
        }

      } else if (global_tb_name == GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE) {
        if (!itemfalg)
          goto res;
        itemfalg = false;
        snprintf(table_name, sizeof(table_name) - 1, "%s",
                 SENSOR_CONFIGURE_TABLE);
        status = sqlite_query(DATABASE_NAME, table_name,
                              pdata->command_or_response.commandMsg.param
                                  .sqlite_update_item_param.itemIdx,
                              Item_check_cb);
        if (status)
          goto res;
      }
      exeflag = true;
      if (itemfalg) {
        itemfalg = false;
#if (ENABLE_MODBUS_FEATURE == 1)
// TODO: build, update data of share memory here
        if(modbus_feature_enable) { // update
          find_flag = false;
          update_flag = false;
          // only handle 'gateway' & 'dev' tables on 'update' operation
          if((GW_PRO_TABLE_IDX_GW_CONFIG_TABLE == global_tb_name) ||
             (GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE == global_tb_name)) {
            memset(&shm_data, 0, sizeof(shm_data));
            table_update_data_convert(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param, (void *)(&shm_data));
          }
          switch (global_tb_name)
          {
            case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:  // 'Gateway'
#if (1) // for debug only remove it later
              printf("gw update!\r\n");
#endif
              switch (g_modbus_reg_addr_mode)
              {
                case MODBUS_REG_ADDR_EXTENSIBLE_MODE:
                  chn_nameid_map[0].nameid = shm_data.gwTableData.gwConfig.nameId;
                  chn_nameid_map[0].ch = 0;
                  break;
                case MODBUS_REG_ADDR_EFFICIENT_MODE:
                  efficient_chn_nameid_map[0].nameid = shm_data.gwTableData.gwConfig.nameId;
                  efficient_chn_nameid_map[0].ch = 0;
                  efficient_chn_nameid_map[0].len = DEV_REG_ADDR_LENGTH_GW;
                  efficient_chn_nameid_map[0].offset = MODBUS_REG_GATEWAY_START_ADDR_OFFSET;
                  efficient_chn_nameid_map[0].validflag = 1;
                  break;                
                default:
                  LOG_ERR(OUTPOINT, "[shm gw update]wrong mode(%d)\r\n", g_modbus_reg_addr_mode);
                  break;
              }
              // udpate description of gateway if necessary
              if(shm_data.gwTableData.gwConfig.description != strstr(shm_data.gwTableData.gwConfig.description, "CH00-AD")) {
                uint8_t i;
                bool description_flag = false;
                for(i=0;i<pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum;i++) {
                  if(33 == pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field) { // field 33 is for 'description' in Gateway table
                    description_flag = true;
                    break;
                  }
                }
                // add 'description' attr in case it's not exist
                if(!description_flag) {
                  pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field = 33;
                  pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum += 1;
                }
                LOG_WARN(OUTPOINT, "[shm-update]wrong gateway description format(%s)! correct it!\r\n", shm_data.gwTableData.gwConfig.description);
                switch (g_modbus_reg_addr_mode)
                {
                case MODBUS_REG_ADDR_EXTENSIBLE_MODE:
                  // channel 0 fixed for gateway by updating it's description
                  memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 1 for 'description'
                  snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                            GW_PRO_MAX_STRING_LEN_BYTE,
                            SENSOR_NAME_BUILD_STRING,
                            0,
                            MODBUS_REG_GATEWAY_START_ADDR_OFFSET,
                            MODBUS_REG_SENSOR_LENGTH,
                            MODBUS_PRODUCT_TYPE_GATEWAY);
                  memset(&shm_data.gwTableData.gwConfig.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                  snprintf(&shm_data.gwTableData.gwConfig.description,
                            GW_PRO_MAX_STRING_LEN_BYTE,
                            SENSOR_NAME_BUILD_STRING,
                            0,
                            MODBUS_REG_GATEWAY_START_ADDR_OFFSET,
                            MODBUS_REG_SENSOR_LENGTH,
                            MODBUS_PRODUCT_TYPE_GATEWAY);
                  break;
                case MODBUS_REG_ADDR_EFFICIENT_MODE:
                  // channel 0 fixed for gateway by updating it's description
                  memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 1 for 'description'
                  snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                            GW_PRO_MAX_STRING_LEN_BYTE,
                            SENSOR_NAME_BUILD_STRING,
                            0,
                            MODBUS_REG_GATEWAY_START_ADDR_OFFSET,
                            DEV_REG_ADDR_LENGTH_GW,
                            MODBUS_PRODUCT_TYPE_GATEWAY);
                  memset(&shm_data.gwTableData.gwConfig.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                  snprintf(&shm_data.gwTableData.gwConfig.description,
                            GW_PRO_MAX_STRING_LEN_BYTE,
                            SENSOR_NAME_BUILD_STRING,
                            0,
                            MODBUS_REG_GATEWAY_START_ADDR_OFFSET,
                            DEV_REG_ADDR_LENGTH_GW,
                            MODBUS_PRODUCT_TYPE_GATEWAY);
                  break;                
                default:
                  LOG_ERR(OUTPOINT, "[shm gw update]wrong mode(%d)\r\n", g_modbus_reg_addr_mode);
                  break;
                }
                LOG_DEBUG(OUTPOINT, "[shm-update]gateway description updated:(%s)!\r\n", shm_data.gwTableData.gwConfig.description);
              }

              w_flag = false;
              ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
              if(0 == ret) {
                memcpy((void *)(shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), (void *)&shm_data, sizeof(shm_data));
#if (1) // for debug only remove it later
                printf("gw update data:\r\n");
                uint8_t *tmp = shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET;
                // for(uint16_t i=0;i<sizeof(shm_data);i++)
                for(uint16_t i=0;i<32;i++)
                  printf(" %02x", tmp[i]);
                printf("\r\ngw update end\r\n");
#endif
                app_shmem_sem_post();
                w_flag = true;
              }
              break;
            case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE:  // 'Dev'
#if (1) // for debug only remove it later
              printf("dev update!\r\n");
#endif
              // Notes: chn 0 is fixed for gateway, so we start idx from 1 here
              ch_idx = BULLET_GW_SENSOR_NUM_MAX + 1;
              reg_addr_offset = 0;
              addr_len = 0;
              switch (g_modbus_reg_addr_mode)
              {
              case MODBUS_REG_ADDR_EXTENSIBLE_MODE: //extensible mode
                for(uint8_t i=1;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++){
                  if((shm_data.devTableData.nameId == chn_nameid_map[i].nameid) &&
                      (1 == chn_nameid_map[i].validflag)){
                    find_flag = true; // find assigned channel
                    ch_idx = chn_nameid_map[i].ch;  // TODO: check whether it's valid
                    break;
                  }                
                }
                if(find_flag) { // found
                  // TODO: update 'description'. add more corner cases later if necessary
                  // if((shm_data.devTableData.description[0] != 'C') || (shm_data.devTableData.description[1] != 'H') || update_flag) {
                  // if((shm_data.devTableData.description[0] != 'C') || (shm_data.devTableData.description[1] != 'H')) {
                    // LOG_WARN(OUTPOINT, "[shm update]wrong description format(%d)!\r\n", ch_idx);
                    LOG_INFO(OUTPOINT, "[shm update]ext dev idx(%d)!\r\n", ch_idx);
#if (1) // 
                    uint8_t i;
                    for(i=0;i<pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum;i++) {
                      if(7 == pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field) { // field 7 is for 'description' in dev table
                        LOG_DEBUG(OUTPOINT, "[shm update]find description(%d)!\r\n", i);
                        // assign new channel for dev by updating it's description
                        memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 7 for 'description'
                        snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                                  GW_PRO_MAX_STRING_LEN_BYTE,
                                  SENSOR_NAME_BUILD_STRING,
                                  ch_idx,
                                  MODBUS_REG_SENSOR_START_ADDR_OFFSET + (ch_idx - 1) * MODBUS_REG_SENSOR_LENGTH,
                                  MODBUS_REG_SENSOR_LENGTH,
                                  // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                  shm_get_sensor_type_str(&shm_data.devTableData.type));
                        memset(&shm_data.devTableData.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                        snprintf(&shm_data.devTableData.description,
                                  GW_PRO_MAX_STRING_LEN_BYTE,
                                  SENSOR_NAME_BUILD_STRING,
                                  ch_idx,
                                  MODBUS_REG_SENSOR_START_ADDR_OFFSET + (ch_idx - 1) * MODBUS_REG_SENSOR_LENGTH,
                                  MODBUS_REG_SENSOR_LENGTH,
                                  // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                  shm_get_sensor_type_str(&shm_data.devTableData.type));
                        break;
                      }
                    }
#endif
                } else { // not found. it should not happen ?? to be confirmed.
                  // update channel index and total channel number
                  if(assigned_chn_cnt > BULLET_GW_SENSOR_NUM_MAX) {
                    LOG_WARN(OUTPOINT, "[shm update]dev item num up to limit(%d)!\r\n", BULLET_GW_SENSOR_NUM_MAX);
                    ch_idx = BULLET_GW_SENSOR_NUM_MAX;
                  } else {  // make sure ch0 would not be assigned to device here.
  #if (1)
                    for(uint8_t i=1;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++){
                      if((0 == chn_nameid_map[i].nameid) && (0 == chn_nameid_map[i].ch)){
                        ch_idx = i;
                        if(i == assigned_chn_cnt) {
                          assigned_chn_cnt++;
                          LOG_INFO(OUTPOINT, "[shm update dev ext]assign a brand-new ch:(%d), total:%d!\r\n", i, assigned_chn_cnt);
                          break;
                        } else {
                          // TODO: may need to check the size here
                          assigned_chn_cnt++;
                          LOG_WARN(OUTPOINT, "[shm update dev ext]assign former ch:(%d), total:%d!\r\n", i, assigned_chn_cnt);
                          break;
                        }
                      }
                    }
  #else
                    if(0 == assigned_chn_cnt) {
                      ch_idx = 1;
                      assigned_chn_cnt = 2; // count in the one for gateway(CH0)
                    } else {
                      ch_idx = assigned_chn_cnt;
                      assigned_chn_cnt++;
                    }
  #endif
                  }
                  if(ch_idx <= BULLET_GW_SENSOR_NUM_MAX) {
                    chn_nameid_map[ch_idx].nameid = shm_data.devTableData.nameId;
                    chn_nameid_map[ch_idx].ch = ch_idx;
                    chn_nameid_map[ch_idx].validflag = 1;
                    // assigned_chn_cnt++; // skip it as it's updated former.
                  } else {
                    LOG_WARN(OUTPOINT, "[shm update]dev no more item available! limit(%d)!\r\n", BULLET_GW_SENSOR_NUM_MAX);
                  }
                  uint8_t i;
                  for(i=0;i<pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum;i++) {
                    if(7 == pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field) { // field 7 is for 'description' in Dev table
                      LOG_DEBUG(OUTPOINT, "[shm update new]find description(%d)!\r\n", i);
                      // assign new channel for dev by updating it's description
                      memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 6 for 'description'
                      snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                MODBUS_REG_SENSOR_START_ADDR_OFFSET + (ch_idx - 1) * MODBUS_REG_SENSOR_LENGTH,
                                MODBUS_REG_SENSOR_LENGTH,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      memset(&shm_data.devTableData.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                      snprintf(&shm_data.devTableData.description,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                MODBUS_REG_SENSOR_START_ADDR_OFFSET + (ch_idx - 1) * MODBUS_REG_SENSOR_LENGTH,
                                MODBUS_REG_SENSOR_LENGTH,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      break;
                    }
                  }
                }
                break;
              case MODBUS_REG_ADDR_EFFICIENT_MODE: //efficient mode
                for(uint8_t i=1;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++){
                  if((shm_data.devTableData.nameId == efficient_chn_nameid_map[i].nameid) &&
                      (1 == efficient_chn_nameid_map[i].validflag)){
                    find_flag = true; // find assigned channel
                    ch_idx = efficient_chn_nameid_map[i].ch;  // TODO: check whether it's valid
                    reg_addr_offset = efficient_chn_nameid_map[i].offset;
                    addr_len = efficient_chn_nameid_map[i].len;
                    // check the length
                    if((DEV_REG_ADDR_LENGTH_INSIGHT_T   != addr_len) && 
                       (DEV_REG_ADDR_LENGTH_INSIGHT_P   != addr_len) &&
                       (DEV_REG_ADDR_LENGTH_INSIGHT_PRO != addr_len) &&
                       (DEV_REG_ADDR_LENGTH_EXT_MODE    != addr_len)) {
                      LOG_ERR(OUTPOINT, "[shm update]wrong addr len(0x%02x, ch:%d,offset:0x%04x,nid:%d), correct it!\r\n", addr_len, ch_idx, reg_addr_offset, efficient_chn_nameid_map[i].nameid);
                      // addr_len = (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? DEV_REG_ADDR_LENGTH_INSIGHT_P : DEV_REG_ADDR_LENGTH_INSIGHT_T;
                      addr_len = shm_get_register_len(&shm_data.devTableData.type);
                      efficient_chn_nameid_map[i].len = addr_len;
                    }
                    break;
                  }                
                }
                if(find_flag) { // found
                  // TODO: update 'description'. add more corner cases later if necessary
                  // if((shm_data.devTableData.description[0] != 'C') || (shm_data.devTableData.description[1] != 'H') || update_flag) {
                  // if((shm_data.devTableData.description[0] != 'C') || (shm_data.devTableData.description[1] != 'H')) {
                    // LOG_WARN(OUTPOINT, "[shm update]wrong description format(%d)!\r\n", ch_idx);
                  LOG_INFO(OUTPOINT, "[shm update eff dev]update description format(%d)!\r\n", ch_idx);
#if (1) // 
                  uint8_t i;
                  for(i=0;i<pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum;i++) {
                    if(7 == pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field) { // field 7 is for 'description' in dev table
                      LOG_DEBUG(OUTPOINT, "[shm update]find description(%d)!\r\n", i);
                      // assign new channel for dev by updating it's description
                      memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 7 for 'description'
                      snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                reg_addr_offset,
                                addr_len,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      memset(&shm_data.devTableData.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                      snprintf(&shm_data.devTableData.description,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                reg_addr_offset,
                                addr_len,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      break;
                    }
                  }
#endif
                } else { // not found. it should not happen ?? to be confirmed.
                  // update channel index and total channel number
                  if(assigned_chn_cnt > BULLET_GW_SENSOR_NUM_MAX) {
                    LOG_WARN(OUTPOINT, "[shm update]dev item num up to limit(%d)!\r\n", BULLET_GW_SENSOR_NUM_MAX);
                    ch_idx = BULLET_GW_SENSOR_NUM_MAX;
                    w_flag = true;
                    goto UPDATE_DEV_DB_EXIT;  // DO NOT update share memory in case too much channel
                  } else {  // make sure ch0 would not be assigned to device here.
                    bool find_flag = false; // record whether there is occupied channel after i
                    uint8_t former_ch = 0;
                    uint8_t former_len = DEV_REG_ADDR_LENGTH_GW;  // 0x03
                    uint16_t former_offset = MODBUS_REG_GATEWAY_START_ADDR_OFFSET; // 0
                    // addr_len = (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? DEV_REG_ADDR_LENGTH_INSIGHT_P : DEV_REG_ADDR_LENGTH_INSIGHT_T;
                    addr_len = shm_get_register_len(&shm_data.devTableData.type);
                    for(uint8_t i=1,j=0;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++){
                      // channel is not occupied
                      if((0 == efficient_chn_nameid_map[i].validflag) &&
                         (0 == efficient_chn_nameid_map[i].ch) &&
                         (0 == efficient_chn_nameid_map[i].nameid)){
                        //TODO: check whether it can be assigned or not. 
                        // try to find next occupied ch
                        for(j=i;j<(BULLET_GW_SENSOR_NUM_MAX + 1);j++){
                          if(efficient_chn_nameid_map[j].validflag){
                            find_flag = true;
                            break;
                          }
                        }
                        // check the len
                        if(find_flag) {
                          if((efficient_chn_nameid_map[j].offset - former_offset - former_len) < addr_len) { // no enough 'space'
                            i = j;  // jump to j directly here
                            continue;
                          } else {
                            // assign the channel
                            LOG_INFO(OUTPOINT, "[shm update]assigned former ch:(%d)!\r\n", i);
                            ch_idx = i;
                            efficient_chn_nameid_map[ch_idx].ch = ch_idx;
                            efficient_chn_nameid_map[ch_idx].len = addr_len;
                            efficient_chn_nameid_map[ch_idx].nameid = shm_data.devTableData.nameId;
                            efficient_chn_nameid_map[ch_idx].offset = former_offset + former_len;
                            efficient_chn_nameid_map[ch_idx].validflag = 1;
                            assigned_chn_cnt++;
                            break;
                          }
                        } else {
                          // assign the channel directly in case no more ch assigned behind it
                          LOG_INFO(OUTPOINT, "[shm update]assigned new ch:(%d)!\r\n", i);
                          ch_idx = i;
                          efficient_chn_nameid_map[ch_idx].ch = ch_idx;
                          efficient_chn_nameid_map[ch_idx].len = addr_len;
                          efficient_chn_nameid_map[ch_idx].nameid = shm_data.devTableData.nameId;
                          efficient_chn_nameid_map[ch_idx].offset = former_offset + former_len;
                          efficient_chn_nameid_map[ch_idx].validflag = 1;
                          assigned_chn_cnt++;
                          break;
                        }
                      } else {
                        // update former* values in case ch is occupied
                        former_ch = i;
                        former_len = efficient_chn_nameid_map[i].len;
                        former_offset = efficient_chn_nameid_map[i].offset;
                      }
                    }
                  }
                  // update description
                  uint8_t i;
                  for(i=0;i<pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum;i++) {
                    if(7 == pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field) { // field 7 is for 'description' in Dev table
                      LOG_DEBUG(OUTPOINT, "[shm update new]find description(%d)!\r\n", i);
                      // assign new channel for dev by updating it's description
                      memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 6 for 'description'
                      snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                efficient_chn_nameid_map[ch_idx].offset,
                                efficient_chn_nameid_map[ch_idx].len,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      memset(&shm_data.devTableData.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                      snprintf(&shm_data.devTableData.description,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                efficient_chn_nameid_map[ch_idx].offset,
                                efficient_chn_nameid_map[ch_idx].len,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      break;
                    }
                  }
                }
                break;              
              default:
                LOG_ERR(OUTPOINT, "[shm dev update]wrong mode(%d)\r\n", g_modbus_reg_addr_mode);
                break;
              }
              // try to write data into share memory
              w_flag = false;
              ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
              if(0 == ret) {  //TODO
                memcpy((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), (void *)&shm_data, sizeof(shm_data));
#if (1) // for debug only remove it later
                printf("dev update data(idx:%d):\r\n",ch_idx);
                uint8_t *tmp = shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE;
                // for(uint16_t i=0;i<sizeof(shm_data);i++)
                for(uint16_t i=0;i<32;i++)
                  printf(" %02x", tmp[i]);
                printf("\r\ndev update end\r\n");
#endif
                app_shmem_sem_post();
                w_flag = true;
              }
              break;
            case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE:  // 'Sensor'
              /* TODO */
              break;
            case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE:  // 'SensorData'
#if (1) // for debug only remove it later
              printf("SensorData update!\r\n");
#endif
              // usually it should not happen!!
              LOG_WARN(OUTPOINT, "[shm update]sensorData, skip copy into shm!\r\n");
              /* code */
              break;          
            default:
              break;
          }
        }
UPDATE_DEV_DB_EXIT:
#endif
        status = sqlite_update(DATABASE_NAME,
                               pdata->command_or_response.commandMsg.param
                                   .sqlite_update_item_param.dataNum,
                               pdata->command_or_response.commandMsg.param
                                   .sqlite_update_item_param.itemIdx,
                               pdata->command_or_response.commandMsg.param
                                   .sqlite_update_item_param.updateData,
                               pdata->command_or_response.commandMsg.param
                                   .sqlite_update_item_param.table,
                               table_name);
#if (ENABLE_MODBUS_FEATURE == 1)
// TODO: build, insert data of share memory here( use a list to cache the items of SensorData for performance ?)
        if(modbus_feature_enable && !w_flag) {
          switch (pdata->command_or_response.commandMsg.param.sqlite_update_item_param.table)
          {
            case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:  // 'Gateway'
#if (1)
              for(uint8_t i=0;i<2;i++) {
                ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                if(0 == ret) {
                  memcpy((void *)(shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), (void *)&shm_data, sizeof(shm_data));
                  app_shmem_sem_post();
                  w_flag = true;
                  break;
                }
              }
              if(!w_flag) { // write directly in case failed 3 times.
                memcpy((void *)(shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), (void *)&shm_data, sizeof(shm_data));
              }
#else
              app_shmem_sem_wait();
              memcpy((void *)(shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), (void *)&shm_data, sizeof(shm_data));
              app_shmem_sem_post();
#endif
              break;
            case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE:  // 'Dev'
#if (1)
              for(uint8_t i=0;i<2;i++) {
                ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                if(0 == ret) {
                  memcpy((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), (void *)&shm_data, sizeof(shm_data));
                  app_shmem_sem_post();
                  w_flag = true;
                  break;
                }
              }
              if(!w_flag) { // write directly in case failed 3 times.
                memcpy((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), (void *)&shm_data, sizeof(shm_data));
              }
#else
              app_shmem_sem_wait();
              memcpy((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), (void *)&shm_data, sizeof(shm_data));
              app_shmem_sem_post();
#endif
              break;
            case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE:  // 'Sensor'
              /* TODO: */
              break;
            case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE:  // 'SensorData'
#if (1)       // Notes: skip the copy here!!!
              // for(uint8_t i=0;i<2;i++) {
              //   ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
              //   if(0 == ret) {
              //     memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (shm_data.dataType - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE),
              //             (void *)&shm_data, sizeof(shm_data));
              //     app_shmem_sem_post();
              //     w_flag = true;
              //     break;
              //   }
              // }
              // if(!w_flag) { // write directly in case failed 3 times.
              //   memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (shm_data.dataType - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE),
              //             (void *)&shm_data, sizeof(shm_data));
              // }
#else
              app_shmem_sem_wait();
              memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (shm_data.dataType - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                      (void *)&shm_data, sizeof(shm_data));
              app_shmem_sem_post();
#endif
              break;          
            default:
              break;
          }
        }
#endif
      } else {
#if (ENABLE_MODBUS_FEATURE == 1)
// TODO: build, insert data of share memory here( use a list to cache the items of SensorData for performance ?)
        if(modbus_feature_enable) { // insert
          find_flag = false;
          update_flag = false;
          // only handle 'gateway' & 'dev' tables on 'update' operation
          if((GW_PRO_TABLE_IDX_GW_CONFIG_TABLE == global_tb_name) ||
             (GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE == global_tb_name)) {
            memset(&shm_data, 0, sizeof(shm_data));
            table_update_data_convert(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param, (void *)(&shm_data));
          } else if (GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE == global_tb_name) {
            memset(&shm_sensor_data, 0, sizeof(shm_sensor_data));
            table_update_data_convert(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param, (void *)(&shm_sensor_data));
          }
          switch (global_tb_name)
          {
            case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:  // 'Gateway'
#if (1) // for debug only remove it later
              printf("gw insert!\r\n");
#endif
              switch (g_modbus_reg_addr_mode)
              {
                case MODBUS_REG_ADDR_EXTENSIBLE_MODE:
                  chn_nameid_map[0].nameid = shm_data.gwTableData.gwConfig.nameId;
                  chn_nameid_map[0].ch = 0;
                  break;
                case MODBUS_REG_ADDR_EFFICIENT_MODE:
                  efficient_chn_nameid_map[0].nameid = shm_data.gwTableData.gwConfig.nameId;
                  efficient_chn_nameid_map[0].ch = 0;
                  efficient_chn_nameid_map[0].len = DEV_REG_ADDR_LENGTH_GW;
                  efficient_chn_nameid_map[0].offset = MODBUS_REG_GATEWAY_START_ADDR_OFFSET;
                  efficient_chn_nameid_map[0].validflag = 1;
                  break;                
                default:
                  LOG_ERR(OUTPOINT, "[shm gw insert]wrong mode(%d)\r\n", g_modbus_reg_addr_mode);
                  break;
              }
              // update description of gateway if necessary
              if((NULL == shm_data.gwTableData.gwConfig.description) ||
                (shm_data.gwTableData.gwConfig.description != strstr(shm_data.gwTableData.gwConfig.description, "CH00-AD"))) {
                LOG_WARN(OUTPOINT, "[shm]wrong gateway description format(%s)! correct it!\r\n", shm_data.gwTableData.gwConfig.description);
                uint8_t i;
                bool description_flag = false;
                for(i=0;i<pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum;i++) {
                  if(33 == pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field) { // field 33 is for 'description' in Gateway table
                    description_flag = true;
                    break;
                  }
                }
                // add 'description' attr in case it's not exist
                if(!description_flag) {
#if (1) // for debug only remove it later
                  printf("[gw insert] descrip missed, add it!\r\n");
#endif
                  pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field = 33;
                  pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum += 1;
                }
                switch (g_modbus_reg_addr_mode)
                {
                case MODBUS_REG_ADDR_EXTENSIBLE_MODE:
                  // channel 0 fixed for gateway by updating it's description
                  memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 1 for 'description'
                  snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                            GW_PRO_MAX_STRING_LEN_BYTE,
                            SENSOR_NAME_BUILD_STRING,
                            0,
                            MODBUS_REG_GATEWAY_START_ADDR_OFFSET,
                            MODBUS_REG_SENSOR_LENGTH,
                            MODBUS_PRODUCT_TYPE_GATEWAY);
                  memset(&shm_data.gwTableData.gwConfig.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                  snprintf(&shm_data.gwTableData.gwConfig.description,
                            GW_PRO_MAX_STRING_LEN_BYTE,
                            SENSOR_NAME_BUILD_STRING,
                            0,
                            MODBUS_REG_GATEWAY_START_ADDR_OFFSET,
                            MODBUS_REG_SENSOR_LENGTH,
                            MODBUS_PRODUCT_TYPE_GATEWAY);
                  break;
                case MODBUS_REG_ADDR_EFFICIENT_MODE:
                  // channel 0 fixed for gateway by updating it's description
                  memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 1 for 'description'
                  snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                            GW_PRO_MAX_STRING_LEN_BYTE,
                            SENSOR_NAME_BUILD_STRING,
                            0,
                            MODBUS_REG_GATEWAY_START_ADDR_OFFSET,
                            DEV_REG_ADDR_LENGTH_GW,
                            MODBUS_PRODUCT_TYPE_GATEWAY);
                  memset(&shm_data.gwTableData.gwConfig.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                  snprintf(&shm_data.gwTableData.gwConfig.description,
                            GW_PRO_MAX_STRING_LEN_BYTE,
                            SENSOR_NAME_BUILD_STRING,
                            0,
                            MODBUS_REG_GATEWAY_START_ADDR_OFFSET,
                            DEV_REG_ADDR_LENGTH_GW,
                            MODBUS_PRODUCT_TYPE_GATEWAY);
                  break;                
                default:
                  LOG_ERR(OUTPOINT, "[shm gw insert]wrong mode(%d)\r\n", g_modbus_reg_addr_mode);
                  break;
                }
                LOG_DEBUG(OUTPOINT, "[shm]gateway description inserted:(%s)!\r\n", shm_data.gwTableData.gwConfig.description);
              }

              w_flag = false;
              ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
              if(0 == ret) {
                memcpy((void *)(shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), (void *)&shm_data, sizeof(shm_data));
#if (1) // for debug only remove it later
                printf("gw insert data:\r\n");
                uint8_t *tmp = shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET;
                // for(uint16_t i=0;i<sizeof(shm_data);i++)
                for(uint16_t i=0;i<32;i++)
                  printf(" %02x", tmp[i]);
                printf("\r\ngw insert end\r\n");
#endif
                app_shmem_sem_post();
                w_flag = true;
              }
              break;
            case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE:  // 'Dev'
#if (1) // for debug only remove it later
              printf("dev insert!\r\n");
#endif
              // Notes: chn 0 is fixed for gateway, so we start idx from 1 here
              ch_idx = BULLET_GW_SENSOR_NUM_MAX + 1;
              reg_addr_offset = 0;
              addr_len = 0;
              switch (g_modbus_reg_addr_mode)
              {
              case MODBUS_REG_ADDR_EXTENSIBLE_MODE: //extensible mode
                for(uint8_t i=1;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++){
                  if((shm_data.devTableData.nameId == chn_nameid_map[i].nameid) &&
                      (1 == chn_nameid_map[i].validflag)){
                    find_flag = true; // find assigned channel
                    ch_idx = chn_nameid_map[i].ch;  // TODO: check whether it's valid
                    break;
                  }                
                }
                if(find_flag) { // found
                  // TODO: update 'description'. add more corner cases later if necessary
                  // if((shm_data.devTableData.description[0] != 'C') || (shm_data.devTableData.description[1] != 'H') || update_flag) {
                  // if((shm_data.devTableData.description[0] != 'C') || (shm_data.devTableData.description[1] != 'H')) {
                    // LOG_WARN(OUTPOINT, "[shm update]wrong description format(%d)!\r\n", ch_idx);
                    LOG_INFO(OUTPOINT, "[shm insert ext dev]nameId(0x%0x) already assigned ch(%d) keep it!\r\n", shm_data.devTableData.nameId, ch_idx);
#if (1)  
                    uint8_t i;
                    for(i=0;i<pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum;i++) {
                      if(7 == pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field) { // field 7 is for 'description' in dev table
                        LOG_DEBUG(OUTPOINT, "[shm insert]find description(%d)!\r\n", i);
                        // assign new channel for dev by updating it's description
                        memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 7 for 'description'
                        snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                                  GW_PRO_MAX_STRING_LEN_BYTE,
                                  SENSOR_NAME_BUILD_STRING,
                                  ch_idx,
                                  MODBUS_REG_SENSOR_START_ADDR_OFFSET + (ch_idx - 1) * MODBUS_REG_SENSOR_LENGTH,
                                  MODBUS_REG_SENSOR_LENGTH,
                                  // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                  shm_get_sensor_type_str(&shm_data.devTableData.type));
                        memset(&shm_data.devTableData.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                        snprintf(&shm_data.devTableData.description,
                                  GW_PRO_MAX_STRING_LEN_BYTE,
                                  SENSOR_NAME_BUILD_STRING,
                                  ch_idx,
                                  MODBUS_REG_SENSOR_START_ADDR_OFFSET + (ch_idx - 1) * MODBUS_REG_SENSOR_LENGTH,
                                  MODBUS_REG_SENSOR_LENGTH,
                                  // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                  shm_get_sensor_type_str(&shm_data.devTableData.type));
                        break;
                      }
                    }
#endif
                } else { // not found. 
                  // update channel index and total channel number
                  if(assigned_chn_cnt > BULLET_GW_SENSOR_NUM_MAX) {
                    LOG_WARN(OUTPOINT, "[shm insert dev ext]dev item num up to limit(%d)!\r\n", BULLET_GW_SENSOR_NUM_MAX);
                    ch_idx = BULLET_GW_SENSOR_NUM_MAX;
                  } else {  // make sure ch0 would not be assigned to device here.
#if (1)
                    for(uint8_t i=1;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++){
                      if((0 == chn_nameid_map[i].nameid) && (0 == chn_nameid_map[i].ch)){
                        ch_idx = i;
                        if(i == assigned_chn_cnt) {
                          assigned_chn_cnt++;
                          LOG_INFO(OUTPOINT, "[shm insert dev ext]assign brand-new ch:(%d) total:%d!\r\n", i, assigned_chn_cnt);
                          break;
                        } else {
                          // TODO: check the offset and len here if we need ext & eff devs co-exist
                          // check whether the len would be overlapped
                          assigned_chn_cnt++;
                          LOG_INFO(OUTPOINT, "[shm insert dev ext]assign pick-hole ch:(%d) total:%d!\r\n", i, assigned_chn_cnt);
                          break;
                        }
                      }
                    }
#else
                    if(0 == assigned_chn_cnt) {
                      ch_idx = 1;
                      assigned_chn_cnt = 2; // count in the one for gateway(CH0)
                    } else {
                      ch_idx = assigned_chn_cnt;
                      assigned_chn_cnt++;
                    }
#endif
                  }
                  if(ch_idx <= BULLET_GW_SENSOR_NUM_MAX) {
                    chn_nameid_map[ch_idx].nameid = shm_data.devTableData.nameId;
                    chn_nameid_map[ch_idx].ch = ch_idx;
                    chn_nameid_map[ch_idx].validflag = 1;
                  } else {
                    LOG_WARN(OUTPOINT, "[shm insert]dev no more item available! limit(%d)!\r\n", BULLET_GW_SENSOR_NUM_MAX);
                  }
                  uint8_t i;
                  for(i=0;i<pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum;i++) {
                    if(7 == pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field) { // field 7 is for 'description' in Dev table
                      LOG_DEBUG(OUTPOINT, "[shm insert new]find description(%d)!\r\n", i);
                      // assign new channel for dev by updating it's description
                      memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 6 for 'description'
                      snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                MODBUS_REG_SENSOR_START_ADDR_OFFSET + (ch_idx - 1) * MODBUS_REG_SENSOR_LENGTH,
                                MODBUS_REG_SENSOR_LENGTH,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      memset(&shm_data.devTableData.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                      snprintf(&shm_data.devTableData.description,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                MODBUS_REG_SENSOR_START_ADDR_OFFSET + (ch_idx - 1) * MODBUS_REG_SENSOR_LENGTH,
                                MODBUS_REG_SENSOR_LENGTH,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      break;
                    }
                  }
                }
                break;
              case MODBUS_REG_ADDR_EFFICIENT_MODE: //efficient mode
                for(uint8_t i=1;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++){
                  if((shm_data.devTableData.nameId == efficient_chn_nameid_map[i].nameid) &&
                      (1 == efficient_chn_nameid_map[i].validflag)){
                    find_flag = true; // find assigned channel
                    ch_idx = efficient_chn_nameid_map[i].ch;  // TODO: check whether it's valid
                    reg_addr_offset = efficient_chn_nameid_map[i].offset;
                    addr_len = efficient_chn_nameid_map[i].len;
                    // check the length: update it into eff 'len' in case it's illegal
                    if((DEV_REG_ADDR_LENGTH_INSIGHT_T   != addr_len) && 
                       (DEV_REG_ADDR_LENGTH_INSIGHT_P   != addr_len) &&
                       (DEV_REG_ADDR_LENGTH_INSIGHT_PRO != addr_len) &&
                       (DEV_REG_ADDR_LENGTH_EXT_MODE    != addr_len)) {
                      LOG_ERR(OUTPOINT, "[shm insert eff dev]wrong addr len(0x%02x, ch:%d,offset:0x%04x,nid:%d), correct it!\r\n", addr_len, ch_idx, reg_addr_offset, efficient_chn_nameid_map[i].nameid);
                      // addr_len = (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? DEV_REG_ADDR_LENGTH_INSIGHT_P : DEV_REG_ADDR_LENGTH_INSIGHT_T;
                      addr_len = shm_get_register_len(&shm_data.devTableData.type);
                      efficient_chn_nameid_map[i].len = addr_len;
                    }
                    break;
                  }
                }
                if(find_flag) { // found
                  // TODO: update 'description'. add more corner cases later if necessary
                  // if((shm_data.devTableData.description[0] != 'C') || (shm_data.devTableData.description[1] != 'H') || update_flag) {
                  // if((shm_data.devTableData.description[0] != 'C') || (shm_data.devTableData.description[1] != 'H')) {
                    // LOG_WARN(OUTPOINT, "[shm update]wrong description format(%d)!\r\n", ch_idx);
                  LOG_INFO(OUTPOINT, "[shm insert eff dev]nameId(0x%0x) already assigned ch(%d) keep it!\r\n", shm_data.devTableData.nameId, ch_idx);
#if (1)
                  uint8_t i;
                  for(i=0;i<pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum;i++) {
                    if(7 == pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field) { // field 7 is for 'description' in dev table
                      LOG_DEBUG(OUTPOINT, "[shm insert]find description(%d)!\r\n", i);
                      // assign new channel for dev by updating it's description
                      memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 7 for 'description'
                      snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                reg_addr_offset,
                                addr_len,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      memset(&shm_data.devTableData.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                      snprintf(&shm_data.devTableData.description,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                reg_addr_offset,
                                addr_len,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      break;
                    }
                  }
#endif
                } else { // not found. it should not happen ?? to be confirmed.
                  // update channel index and total channel number
                  if(assigned_chn_cnt > BULLET_GW_SENSOR_NUM_MAX) {
                    LOG_WARN(OUTPOINT, "[shm insert eff]dev item num up to limit(%d)!\r\n", BULLET_GW_SENSOR_NUM_MAX);
                    ch_idx = BULLET_GW_SENSOR_NUM_MAX;
                    w_flag = true;
                    goto UPDATE_DEV_DB_EXIT;  // DO NOT update share memory in case too much channel
                  } else {  // make sure ch0 would not be assigned to device here.
                    bool fnd_flag = false; // record whether there is occupied channel after i
                    uint8_t former_ch = 0;
                    uint8_t former_len = DEV_REG_ADDR_LENGTH_GW;  // 0x03
                    uint16_t former_offset = MODBUS_REG_GATEWAY_START_ADDR_OFFSET; // 0
                    // addr_len = (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? DEV_REG_ADDR_LENGTH_INSIGHT_P : DEV_REG_ADDR_LENGTH_INSIGHT_T;
                    addr_len = shm_get_register_len(&shm_data.devTableData.type);
                    for(uint8_t i=1,j=0;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++){
                      // channel is not occupied
                      if((0 == efficient_chn_nameid_map[i].validflag) &&
                         (0 == efficient_chn_nameid_map[i].ch) &&
                         (0 == efficient_chn_nameid_map[i].nameid)){
                        //TODO: check whether it can be assigned or not. 
                        // try to find next occupied ch
                        for(j=i;j<(BULLET_GW_SENSOR_NUM_MAX + 1);j++){
                          if(efficient_chn_nameid_map[j].validflag){
                            fnd_flag = true;
                            break;
                          }
                        }
                        // check the len
                        if(fnd_flag) {
                          if((efficient_chn_nameid_map[j].offset - former_offset - former_len) < addr_len) { // no enough 'space'
                            i = j;  // jump to j directly here
                            continue;
                          } else {
                            // assign the channel
                            LOG_INFO(OUTPOINT, "[shm insert eff dev]assign pick-hole ch:(%d)!\r\n", i);
                            ch_idx = i;
                            efficient_chn_nameid_map[ch_idx].ch = ch_idx;
                            efficient_chn_nameid_map[ch_idx].len = addr_len;
                            efficient_chn_nameid_map[ch_idx].nameid = shm_data.devTableData.nameId;
                            efficient_chn_nameid_map[ch_idx].offset = former_offset + former_len;
                            efficient_chn_nameid_map[ch_idx].validflag = 1;
                            assigned_chn_cnt++;
                            break;
                          }
                        } else {
                          // assign the channel directly in case no more ch assigned behind it
                          LOG_INFO(OUTPOINT, "[shm insert eff dev]assign brand-new ch:(%d)!\r\n", i);
                          ch_idx = i;
                          efficient_chn_nameid_map[ch_idx].ch = ch_idx;
                          efficient_chn_nameid_map[ch_idx].len = addr_len;
                          efficient_chn_nameid_map[ch_idx].nameid = shm_data.devTableData.nameId;
                          efficient_chn_nameid_map[ch_idx].offset = former_offset + former_len;
                          efficient_chn_nameid_map[ch_idx].validflag = 1;
                          assigned_chn_cnt++;
                          break;
                        }
                      } else {
                        // update former* values in case ch is occupied
                        former_ch = i;
                        former_len = efficient_chn_nameid_map[i].len;
                        former_offset = efficient_chn_nameid_map[i].offset;
                      }
                    }
                  }
                  // update description
                  uint8_t i;
                  for(i=0;i<pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum;i++) {
                    if(7 == pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].field) { // field 7 is for 'description' in Dev table
                      LOG_DEBUG(OUTPOINT, "[shm insert new]find description(%d)!\r\n", i);
                      // assign new channel for dev by updating it's description
                      memset(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data, 0, GW_PRO_MAX_STRING_LEN_BYTE);  // field index is 6 for 'description'
                      snprintf(&pdata->command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i].data,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                efficient_chn_nameid_map[ch_idx].offset,
                                efficient_chn_nameid_map[ch_idx].len,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      memset(&shm_data.devTableData.description, 0, GW_PRO_MAX_STRING_LEN_BYTE); 
                      snprintf(&shm_data.devTableData.description,
                                GW_PRO_MAX_STRING_LEN_BYTE,
                                SENSOR_NAME_BUILD_STRING,
                                ch_idx,
                                efficient_chn_nameid_map[ch_idx].offset,
                                efficient_chn_nameid_map[ch_idx].len,
                                // (0 == strcmp(&shm_data.devTableData.type, TYPE_STR_INSIGHT_P)) ? MODBUS_PRODUCT_TYPE_PREDICT_SENSOR : MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR);
                                shm_get_sensor_type_str(&shm_data.devTableData.type));
                      break;
                    }
                  }
                }
                break;              
              default:
                LOG_ERR(OUTPOINT, "[shm dev insert]wrong mode(%d)\r\n", g_modbus_reg_addr_mode);
                break;
              }
              // try to write data into share memory
              w_flag = false;
              ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
              if(0 == ret) {  //TODO
                memcpy((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), (void *)&shm_data, sizeof(shm_data));
#if (1) // for debug only remove it later
                printf("dev insert data(idx:%d, description:%s, dataNum:%d):\r\n",ch_idx, shm_data.devTableData.description, pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum);
                uint8_t *tmp = shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE;
                // for(uint16_t i=0;i<sizeof(shm_data);i++)
                for(uint16_t i=0;i<32;i++)
                  printf(" %02x", tmp[i]);
                printf("\r\ndev insert end\r\n");
#endif
                app_shmem_sem_post();
                w_flag = true;
              }

              break;
            case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE:  // 'Sensor'
              /* TODO: */
              break;
            case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE:  // 'SensorData'
#if (1) // for debug only remove it later
              printf("SData insert!\r\n");
#endif
              // skip the dataType 1(VIBRATION_ACC_WAVE),3(VIBRATION_ENV3_WAVE),and 4(VIBRATION_VELOCITY_WAVE) as they are not expected.
              if((1 == shm_sensor_data.dataType) ||
                 (3 == shm_sensor_data.dataType) ||
                 (4 == shm_sensor_data.dataType)) {
                  printf("[shm insert] skip dataType:%d\r\n", shm_sensor_data.dataType);
                  w_flag = true;  // skip further writing by setting this flag
                  break;
                 }
              // identify reg_mode, find channel index
              sd_conn_flag = false;
              switch (g_modbus_reg_addr_mode)
              {
                case MODBUS_REG_ADDR_EXTENSIBLE_MODE:
                  // for(uint8_t i=1;i<assigned_chn_cnt;i++){
                  for(uint8_t i=1;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++){
#if (0) // for debug only
                    printf("[shm insert] i:%d, ch:%d, nid:%d\r\n", i, chn_nameid_map[i].ch, chn_nameid_map[i].nameid);
#endif
                    if((shm_sensor_data.nameId == chn_nameid_map[i].nameid) &&
                       (1 == chn_nameid_map[i].validflag)){
                      find_flag = true; // find assigned channel
                      g_former_ch_idx = g_current_ch_idx;
                      // g_former_ch_idx_p = g_current_ch_idx_p;
                      g_current_ch_idx = chn_nameid_map[i].ch;
                      break;
                    }                
                  }
                  break;
                case MODBUS_REG_ADDR_EFFICIENT_MODE:
                  for(uint8_t i=1;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++){
#if (0) // for debug only
                    printf("[shm insert] i:%d, ch:%d, nid:%d\r\n", i, efficient_chn_nameid_map[i].ch, efficient_chn_nameid_map[i].nameid);
#endif
                    if((shm_sensor_data.nameId == efficient_chn_nameid_map[i].nameid) &&
                       (1 == efficient_chn_nameid_map[i].validflag)){
                      find_flag = true; // find assigned channel
                      g_former_ch_idx = g_current_ch_idx;
                      // g_former_ch_idx_p = g_current_ch_idx_p;
                      g_current_ch_idx = efficient_chn_nameid_map[i].ch;
                      break;
                    }                
                  }
                  break;                
                default:
                  LOG_ERR(OUTPOINT, "[shm gw insert]wrong mode(%d)\r\n", g_modbus_reg_addr_mode);
                  break;
              }
              if(!find_flag) {
                LOG_WARN(OUTPOINT, "[shm insert]no chn assigned(nid:%d sMac:%s), skip data item!\r\n", shm_sensor_data.nameId,
                            shm_sensor_data.macAddr);
                w_flag = true;  // skip further writing by setting this flag
              } else {
#if (1)
#if (0) // to be removed later. 20260109
                g_sensor_data_piece_max = (0 == strcmp(shm_sensor_data.type, TYPE_STR_INSIGHT_P)) ? APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRS : APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS;
                // differentiate the sensor type here. Do nothing for insight-T.
                if(APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRS == g_sensor_data_piece_max) { // insight-P 
                  // g_former_ch_idx_p = g_former_ch_idx;
                  g_current_ch_idx_p = g_current_ch_idx;
                  g_sensor_data_piece_max_p = g_sensor_data_piece_max;
                  g_sensor_type_u8 = 1; // insight-P 
                } else {
                  g_sensor_type_u8 = 0; // insight-T
                }
#else
                if(0 == strcmp(shm_sensor_data.type, TYPE_STR_INSIGHT_P)) { // predict sensor
                  g_sensor_data_piece_max = APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRS;
                  g_sensor_data_piece_max_p = g_sensor_data_piece_max;
                  g_current_ch_idx_p = g_current_ch_idx;
                  g_sensor_type_u8 = MODBUS_SENSOR_TYPE_P; // insight-P
                } else if (0 == strcmp(shm_sensor_data.type, TYPE_STR_INSIGHT_P_PRO)) {  // predict pro sensor
                  // TODO: do nothing else for brand-new sensor type here
                  g_sensor_type_u8 = MODBUS_SENSOR_TYPE_PRO; // predict-Pro
                } else {
                  g_sensor_data_piece_max = APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS;
                  g_sensor_type_u8 = MODBUS_SENSOR_TYPE_T; // insight-T
                }
#endif

                // already recv sensor data of this sensor before
                // cache sensor data & check whether it's the latest one of current data set or not (delete timer and store data into shm if yes)
                if(MODBUS_SENSOR_TYPE_P == g_sensor_type_u8) { // insight-P
                  sd_conn_flag = false;
                  for(uint8_t i=0;i<CONFIG_SHM_SENSOR_DATA_MAX_NUM;i++) {
                    if((true == g_sd_instance[i].sd_valid_flag) &&
                       (shm_sensor_data.nameId == g_sd_instance[i].sd_nameid) &&
                       (g_current_ch_idx == g_sd_instance[i].sd_chn_idx))
                       {
                          g_sd_idx = i;
                          sd_conn_flag = true;
                          break;
                       }
                  }
                  if(sd_conn_flag) {  // 'connected' sensor: at least 1 or more pieces of sensorData was stored into g_sd_instance[g_sd_idx].sd_data_buf
                    printf("[insert-p]nid:%d cnt:%d max:%d ch:%d!\r\n", shm_sensor_data.nameId,
                                 g_sd_instance[g_sd_idx].sd_piece_cnt,
                                 g_sd_instance[g_sd_idx].sd_piece_max,
                                 g_sd_instance[g_sd_idx].sd_chn_idx);
                    // copy this piece of sensordata and set corresponding bit
                    memcpy((void *)(&g_sd_instance[g_sd_idx].sd_data_buf[shm_sd_measType_to_idx[shm_sensor_data.dataType]]), (void *)&shm_sensor_data, sizeof(shm_sensor_data));
                    shm_set_sd_bit_map(g_sd_instance[g_sd_idx].sd_sensor_type,
                                        (uint64_t *)&g_sd_instance[g_sd_idx].sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                        shm_sd_measType_to_idx[shm_sensor_data.dataType]);
#if (0) // for debug only remove it later, log the first piece data only for save time
                    uint8_t *tmp = (void *)(&g_sd_instance[g_sd_idx].sd_data_buf[shm_sd_measType_to_idx[shm_sensor_data.dataType]]);
                    // for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
                    for(uint16_t i=0;i<16;i++)
                      printf(" %02x", tmp[i]);
                    printf("\r\n insert_p:end\r\n");
#endif
                    g_sd_instance[g_sd_idx].sd_piece_cnt++;
                    // piece_cnt reaches the max
                    if(g_sd_instance[g_sd_idx].sd_piece_cnt >= g_sd_instance[g_sd_idx].sd_piece_max) {  // create timer too in case it's the first piece data of the same sensor
                      //delete timer & try to copy data into shm, but not clear the index & cache as they may be used later
#if (0)
                      // do nothing here to check whether timer works fine or not
                      w_flag = true;
#else
                      // delete timer
                      timer_delete(g_sd_instance[g_sd_idx].sd_timer_id);
                      // check timestamp(idx: 25, 26) & set bit if necessary.
                      shm_update_sd_bit_map(g_sd_instance[g_sd_idx].sd_sensor_type, g_sd_idx);
                      // try to write data into share memory
                      ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                      if(0 == ret) {
                        for(uint8_t j=0;j<g_sd_instance[g_sd_idx].sd_piece_max;j++) {
                          memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[g_sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                                (void *)&g_sd_instance[g_sd_idx].sd_data_buf[j],
                                  sizeof(shm_sensor_data));
                        }
                        // copy the reg bit_mapping into share memory
                        // last item of 'sd_data_buf'                    
                        memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[g_sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                              (void *)&g_sd_instance[g_sd_idx].sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                sizeof(uint64_t));
#if (1) // for debug only remove it later, log the first piece data only for save time
                        LOG_INFO(OUTPOINT, "insert-P last piece(idx:%d):\r\n",g_sd_instance[g_sd_idx].sd_piece_max);
                        uint8_t *tmp = shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[g_sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (g_sd_instance[g_sd_idx].sd_piece_max - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
                        // for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
                        for(uint16_t i=0;i<16;i++)
                          printf(" %02x", tmp[i]);
                        // printf("\r\nend\r\n");
#endif
                        app_shmem_sem_post();
                        w_flag = true;
                        // clear the index & cache here as the data is written into shm successfully here
                        memset((void *)&g_sd_instance[g_sd_idx], 0, sizeof(appShmSensorDataInstance_t));
                      }
#endif
                    } else {  // sd_piece_cnt < sd_piece_max : SD collecting is still ongoing(not full so far)
                      // set it to skip further 'write' op
                      w_flag = true;
                    }
                  } else {  // 'fresh' -P sensor : it's not found in 'g_sd_instance' array (or, it's the first piece of SD of this sensor)
                    // find a 'empty' item
                    g_sd_idx = CONFIG_SHM_SENSOR_DATA_MAX_NUM;
                    for(uint8_t i=0;i<CONFIG_SHM_SENSOR_DATA_MAX_NUM;i++) {
                      if((false == g_sd_instance[i].sd_valid_flag) &&
                        (0 == g_sd_instance[i].sd_nameid) &&
                        (0 == g_sd_instance[i].sd_chn_idx))
                        {
                            g_sd_idx = i;
                            break;
                        }
                    }
                    if(g_sd_idx >= CONFIG_SHM_SENSOR_DATA_MAX_NUM) {
                      LOG_WARN(OUTPOINT, "[shm insert]fail to find a free SD instance(nid:%d), skip!\r\n", shm_sensor_data.nameId);
                      w_flag = true;  // skip further writing by setting this flag
                    } else {
                      // update 'attr's accordingly
                      g_sd_instance[g_sd_idx].sd_chn_idx      = g_current_ch_idx;
                      g_sd_instance[g_sd_idx].sd_nameid       = shm_sensor_data.nameId;
                      g_sd_instance[g_sd_idx].sd_valid_flag   = true;
                      g_sd_instance[g_sd_idx].sd_piece_cnt    = 0;
                      g_sd_instance[g_sd_idx].sd_piece_max    = APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRS;
                      g_sd_instance[g_sd_idx].sd_sensor_type  = MODBUS_SENSOR_TYPE_P;
                      g_sd_instance[g_sd_idx].sd_timer_id     = NULL;
                      // create new timer for instance g_sd_instance[g_sd_idx]
                      shm_evp.sigev_notify = SIGEV_THREAD;
                      shm_evp.sigev_notify_function = shm_timer_handler_p;
                      shm_evp.sigev_notify_attributes = NULL;
                      shm_evp.sigev_value.sival_ptr = &g_sd_instance[g_sd_idx].sd_timer_id;
                      ret = timer_create(CLOCK_REALTIME, &shm_evp, &g_sd_instance[g_sd_idx].sd_timer_id);
                      if(0 != ret){
                        LOG_ERR(OUTPOINT, "[shm]create timer error(%d), try again!\r\n", errno);
                        ret = timer_create(CLOCK_REALTIME, &shm_evp, &g_sd_instance[g_sd_idx].sd_timer_id);
                        if(0 != ret){
                          LOG_ERR(OUTPOINT, "[shm]create timer error(%d), failed!\r\n", errno);
                          return EXIT_FAILURE;
                        }
                      }
                      // start timer
                      // shm_ts.it_value.tv_sec = (0 == strcmp(shm_sensor_data.type, TYPE_STR_INSIGHT_P)) ? APP_SHM_SENSOR_TIMER_INTERVAL_PRS_S : APP_SHM_SENSOR_TIMER_INTERVAL_ITS_S;
                      shm_ts.it_value.tv_sec = APP_SHM_SENSOR_TIMER_INTERVAL_PRS_S;
                      shm_ts.it_value.tv_nsec = 0;
                      shm_ts.it_interval.tv_nsec = 0;
                      shm_ts.it_interval.tv_nsec = 0;
                      if(0 != timer_settime(g_sd_instance[g_sd_idx].sd_timer_id, 0, &shm_ts, NULL)) {
                        LOG_ERR(OUTPOINT, "[shm]start timer error(%d), try again!\r\n", errno);
                        if(0 != timer_settime(g_sd_instance[g_sd_idx].sd_timer_id, 0, &shm_ts, NULL)) {
                          LOG_ERR(OUTPOINT, "[shm]start timer error(%d), failed!\r\n", errno);
                          return EXIT_FAILURE;
                        }
                      }
                      // copy this piece of sensordata and set corresponding bit
                      memcpy((void *)(&g_sd_instance[g_sd_idx].sd_data_buf[shm_sd_measType_to_idx[shm_sensor_data.dataType]]), (void *)&shm_sensor_data, sizeof(shm_sensor_data));
                      shm_set_sd_bit_map(g_sd_instance[g_sd_idx].sd_sensor_type,
                                          (uint64_t *)&g_sd_instance[g_sd_idx].sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                          shm_sd_measType_to_idx[shm_sensor_data.dataType]);
#if (0) // for debug only remove it later, log the first piece data only for save time
                      uint8_t *tmp = (void *)(&g_sd_instance[g_sd_idx].sd_data_buf[shm_sd_measType_to_idx[shm_sensor_data.dataType]]);
                      // for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
                      for(uint16_t i=0;i<16;i++)
                        printf(" %02x", tmp[i]);
                      printf("\r\n insert_p:end\r\n");
#endif
                      g_sd_instance[g_sd_idx].sd_piece_cnt++;
                      w_flag = true;  // skip further writing by setting this flag
                    }
                  } // end of 'fresh' -P sensor
                } else if (MODBUS_SENSOR_TYPE_PRO == g_sensor_type_u8) { // PRO sensor
                  // TODO
                } else { // insight-T
                  if(g_former_nameid == shm_sensor_data.nameId) {
                    printf("[insert-T]nid:%d cnt:%d max:%d!\r\n", shm_sensor_data.nameId, g_sensor_data_piece_cnt, g_sensor_data_piece_max);
                    if(g_sensor_data_piece_cnt < (g_sensor_data_piece_max - 1)) { // cache sensor data only, not store it into shm here
                      uint8_t _idx = shm_get_measure_type_idx_t(shm_sensor_data.dataType);
                      if(_idx < APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS) {
                        memcpy((void *)&g_shm_data_buf[_idx], (void *)&shm_sensor_data, sizeof(shm_sensor_data));
                        shm_set_sd_bit_map(MODBUS_SENSOR_TYPE_T,
                                            (uint64_t *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                            _idx);
                        g_sensor_data_piece_cnt++;
                        if(1 == g_sensor_data_piece_cnt) {  // create timer too in case it's the first piece data of the same sensor
                          // delete timer first if exist
                          timer_delete(g_timer_id);
                          // create new timer
                          shm_evp.sigev_notify = SIGEV_THREAD;
                          shm_evp.sigev_notify_function = shm_timer_handler;
                          shm_evp.sigev_notify_attributes = NULL;
                          shm_evp.sigev_value.sival_int = 0;
                          ret = timer_create(CLOCK_REALTIME, &shm_evp, &g_timer_id);
                          if(0 != ret){
                            LOG_ERR(OUTPOINT, "[shm]create timer error(%d), try again!\r\n", errno);
                            ret = timer_create(CLOCK_REALTIME, &shm_evp, &g_timer_id);
                            if(0 != ret){
                              LOG_ERR(OUTPOINT, "[shm]create timer error(%d), failed!\r\n", errno);
                              return EXIT_FAILURE;
                            }
                          }
                          // start timer
                          // shm_ts.it_value.tv_sec = (MODBUS_SENSOR_TYPE_PRO == g_sensor_type_u8) ? APP_SHM_SENSOR_TIMER_INTERVAL_PRO_S : (g_sensor_type_u8 ? APP_SHM_SENSOR_TIMER_INTERVAL_PRS_S : APP_SHM_SENSOR_TIMER_INTERVAL_ITS_S);
                          shm_ts.it_value.tv_sec = APP_SHM_SENSOR_TIMER_INTERVAL_ITS_S;
                          shm_ts.it_value.tv_nsec = 0;
                          shm_ts.it_interval.tv_nsec = 0;
                          shm_ts.it_interval.tv_nsec = 0;
                          if(0 != timer_settime(g_timer_id, 0, &shm_ts, NULL)) {
                            LOG_ERR(OUTPOINT, "[shm]start timer error(%d), try again!\r\n", errno);
                            if(0 != timer_settime(g_timer_id, 0, &shm_ts, NULL)) {
                              LOG_ERR(OUTPOINT, "[shm]start timer error(%d), failed!\r\n", errno);
                              return EXIT_FAILURE;
                            }
                          }
                        }
                      } else {
                        LOG_WARN(OUTPOINT, "[shm insert-T]unexpected SD type(nid:%d, type:%d), skip!\r\n", shm_sensor_data.nameId, shm_sensor_data.dataType);
                      }
                      w_flag = true;  // skip further writing by setting this flag
                    } else {  // piece_cnt reaches the max
                      g_sensor_data_piece_cnt = g_sensor_data_piece_max - 1;  // make sure index would not overstep
                      //delete timer & try to copy data into shm, but not clear the index & cache as they may be used later
                      // delete timer
                      timer_delete(g_timer_id);
                      uint8_t _idx = shm_get_measure_type_idx_t(shm_sensor_data.dataType);
                      if(_idx < APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS) {
                        memcpy((void *)&g_shm_data_buf[_idx], (void *)&shm_sensor_data, sizeof(shm_sensor_data));
                        shm_set_sd_bit_map(MODBUS_SENSOR_TYPE_T,
                                            (uint64_t *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                            _idx);
                        g_sensor_data_piece_cnt++;
                        shm_set_sd_bit_map(MODBUS_SENSOR_TYPE_T,
                                            (uint64_t *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                            SHM_SD_IDX_LAST_TEMP_SENSING_TIME_T);
                        // try to write data into share memory
                        ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                        if(0 == ret) {
                          for(uint8_t j=0;j<g_sensor_data_piece_cnt;j++) {
                            memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                                   (void *)&g_shm_data_buf[j],
                                   sizeof(shm_sensor_data));
                          }
                          // copy the reg bit_mapping into share memory
                          // last item of 'sd_data_buf'                    
                          memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                                (void *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                  sizeof(uint64_t));
#if (1) // for debug only remove it later, log the first piece data only for save time
                          printf("insert-T last piece(idx:%d):\r\n",g_current_ch_idx);
                          uint8_t *tmp = shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (g_sensor_data_piece_cnt - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
                          // for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
                          for(uint16_t i=0;i<16;i++)
                            printf(" %02x", tmp[i]);
                          // printf("\r\nsensordata insert end\r\n");
#endif
                          app_shmem_sem_post();
                          w_flag = true;
                          // clear the index & cache here as the data is written into shm successfully here
                          memset((void *)&g_shm_data_buf[0], 0, sizeof(shm_sensor_data) * APP_SHM_SENSORDATA_TABLE_PIECE_MAX);
                          // Notes: 
                          g_sensor_data_piece_cnt = 0;
                        }
                      } else {
                        LOG_WARN(OUTPOINT, "[shm insert-T max]unexpected SD type(nid:%d, type:%d), skip!\r\n", shm_sensor_data.nameId, shm_sensor_data.dataType);
                        w_flag = true;  // skip further writing by setting this flag
                      }
                    }
                  } else {  // nameid not same : start recv the first sensor data of another sensor 
                    printf("[insert-T]nid:%d cnt:%d max:%d!\r\n", shm_sensor_data.nameId, g_sensor_data_piece_cnt, g_sensor_data_piece_max);
                    // delete the original timer first no matter whether there are piece data cached before
                    timer_delete(g_timer_id);
                    g_former_nameid = shm_sensor_data.nameId;
                    // no need to consider sensorTypes other than 'insight-T' as we only 'trace' nameId of insight-T here
                    // that is, 'g_former_nameid' is for insight-T only 
                    if(0 == g_sensor_data_piece_cnt) {  // start from scratch, the first piece of data of another sensor
                      uint8_t _idx = shm_get_measure_type_idx_t(shm_sensor_data.dataType);
                      if(_idx < APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS) {
                        memcpy((void *)&g_shm_data_buf[_idx], (void *)&shm_sensor_data, sizeof(shm_sensor_data));
                        shm_set_sd_bit_map(MODBUS_SENSOR_TYPE_T,
                                            (uint64_t *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                            _idx);
                        g_sensor_data_piece_cnt++;
                      } else {
                        LOG_WARN(OUTPOINT, "[shm insert-T new]unexpected SD type(nid:%d, type:%d), skip!\r\n", shm_sensor_data.nameId, shm_sensor_data.dataType);
                      }
                      w_flag = true;  // skip further writing by setting this flag
                    } else {  // write the former pieces of data into share memory before moving on further caching
                      backup_flag = false;
                      // try to write data into share memory even if it's only partial of 1 set data
                      ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                      if(0 == ret) {
                        for(uint8_t j=0;j<g_sensor_data_piece_cnt;j++) {
                          memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                                (void *)&g_shm_data_buf[j],
                                  sizeof(shm_sensor_data));
                        }
                        shm_set_sd_bit_map(MODBUS_SENSOR_TYPE_T,
                                            (uint64_t *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                            SHM_SD_IDX_LAST_TEMP_SENSING_TIME_T);
                        // copy the reg bit_mapping into share memory
                        // last item of 'sd_data_buf'                    
                        memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                              (void *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                sizeof(uint64_t));
      #if (1) // for debug only remove it later, log the last piece data only for save time
                        printf("insert-T (partial ch:%d, cnt:%d):\r\n",g_former_ch_idx, g_sensor_data_piece_cnt);
                        uint8_t *tmp = shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (g_sensor_data_piece_cnt - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
                        // for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
                        for(uint16_t i=0;i<16;i++)
                          printf(" %02x", tmp[i]);
                        // printf("\r\n end\r\n");
      #endif
                        app_shmem_sem_post();
                        w_flag = true;
                        // clear the index & cache here as the data is written into shm successfully here
                        memset((void *)&g_shm_data_buf[0], 0, sizeof(shm_sensor_data) * APP_SHM_SENSORDATA_TABLE_PIECE_MAX);
                        // Notes: write new piece data into cache directly as former data is stored into shm successfully
                        uint8_t _idx = shm_get_measure_type_idx_t(shm_sensor_data.dataType);
                        if(_idx < APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS) {
                          memcpy((void *)&g_shm_data_buf[_idx], (void *)&shm_sensor_data, sizeof(shm_sensor_data));
                          shm_set_sd_bit_map(MODBUS_SENSOR_TYPE_T,
                                              (uint64_t *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                              _idx);
                          g_sensor_data_piece_cnt = 1;
                        }
                      } else {  // not get semaphore,try it late
                        backup_flag = true; // need backup current piece data later (after the cache is free)
                        w_flag = false;
                      }
                    }
                    // create new timer
                    shm_evp.sigev_notify = SIGEV_THREAD;
                    shm_evp.sigev_notify_function = shm_timer_handler;
                    shm_evp.sigev_notify_attributes = NULL;
                    shm_evp.sigev_value.sival_int = 0;
                    ret = timer_create(CLOCK_REALTIME, &shm_evp, &g_timer_id);
                    if(0 != ret){
                      LOG_ERR(OUTPOINT, "[shm]create timer error(%d), try again!\r\n", errno);
                      ret = timer_create(CLOCK_REALTIME, &shm_evp, &g_timer_id);
                      if(0 != ret){
                        LOG_ERR(OUTPOINT, "[shm]create timer error(%d), failed!\r\n", errno);
                        return EXIT_FAILURE;
                      }
                    }
                    // start timer
                    // shm_ts.it_value.tv_sec = (MODBUS_SENSOR_TYPE_PRO == g_sensor_type_u8) ? APP_SHM_SENSOR_TIMER_INTERVAL_PRO_S : (g_sensor_type_u8 ? APP_SHM_SENSOR_TIMER_INTERVAL_PRS_S : APP_SHM_SENSOR_TIMER_INTERVAL_ITS_S);
                    shm_ts.it_value.tv_sec = APP_SHM_SENSOR_TIMER_INTERVAL_ITS_S;
                    shm_ts.it_value.tv_nsec = 0;
                    shm_ts.it_interval.tv_nsec = 0;
                    shm_ts.it_interval.tv_nsec = 0;
                    if(0 != timer_settime(g_timer_id, 0, &shm_ts, NULL)) {
                      LOG_ERR(OUTPOINT, "[shm]start timer error(%d), try again!\r\n", errno);
                      if(0 != timer_settime(g_timer_id, 0, &shm_ts, NULL)) {
                        LOG_ERR(OUTPOINT, "[shm]start timer error(%d), failed!\r\n", errno);
                        return EXIT_FAILURE;
                      }
                    }
                  }
                }
#else
                // get type_idx by 'DataType' 
                if(shm_sensor_data.dataType > APP_SHM_SENSORDATA_TABLE_PIECE_MAX) {
                  LOG_WARN(OUTPOINT, "[shm]DataType exceeds to limit(max:%d dt:%d), skip data item!\r\n", APP_SHM_SENSORDATA_TABLE_PIECE_MAX,
                                  shm_sensor_data.dataType);
                  w_flag = true;  // skip further writing by setting this flag
                  break;
                }
                // TODO: updte 'Value' in case it's NAN
                // try to write data into share memory
                ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                if(0 == ret) {  //TODO
                  memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (shm_sensor_data.dataType - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE),
                         (void *)&shm_sensor_data, sizeof(shm_sensor_data));
#if (1) // for debug only remove it later
                  printf("sensordata insert data(idx:%d):\r\n",ch_idx);
                  uint8_t *tmp = shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (shm_sensor_data.dataType - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
                  for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
                    printf(" %02x", tmp[i]);
                  printf("\r\nsensordata insert end\r\n");
#endif
                  app_shmem_sem_post();
                  w_flag = true;
                }
#endif
              }
              break;          
            default:
              LOG_WARN(OUTPOINT, "[shm]wrong table index(%d)!\r\n", pdata->command_or_response.commandMsg.param.sqlite_update_item_param.table);
              break;
          }
        }
#endif
        status = sqlite_insert(DATABASE_NAME,
                               pdata->command_or_response.commandMsg.param
                                   .sqlite_update_item_param.dataNum,
                               pdata->command_or_response.commandMsg.param
                                   .sqlite_update_item_param.itemIdx,
                               pdata->command_or_response.commandMsg.param
                                   .sqlite_update_item_param.updateData,
                               pdata->command_or_response.commandMsg.param
                                   .sqlite_update_item_param.table,
                               table_name);
        // To speed up data insertion, no need to check whether the item 
        // has been existed
        // if (global_tb_name == GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE) {
        //   status = sqlite_gain_autoid(DATABASE_NAME, table_name, autoid_cb);
        //   pdata->command_or_response.commandMsg.param.sqlite_update_item_param
        //       .itemIdx = autoid;
        // }
#if (ENABLE_MODBUS_FEATURE == 1)
// TODO: build, insert data of share memory here( use a list to cache the items of SensorData for performance ?)
        if(modbus_feature_enable && !w_flag) {
          switch (pdata->command_or_response.commandMsg.param.sqlite_update_item_param.table)
          {
            case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:  // 'Gateway'
#if (1)
              for(uint8_t i=0;i<2;i++) {
                ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                if(0 == ret) {
                  memcpy((void *)(shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), (void *)&shm_data, sizeof(shm_data));
                  app_shmem_sem_post();
                  w_flag = true;
                  break;
                }
              }
              if(!w_flag) { // write directly in case failed 3 times.
                memcpy((void *)(shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), (void *)&shm_data, sizeof(shm_data));
              }
#else
              app_shmem_sem_wait();
              memcpy((void *)(shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), (void *)&shm_data, sizeof(shm_data));
              app_shmem_sem_post();
#endif
              break;
            case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE:  // 'Dev'
#if (1)
              for(uint8_t i=0;i<2;i++) {
                ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                if(0 == ret) {
                  memcpy((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), (void *)&shm_data, sizeof(shm_data));
                  app_shmem_sem_post();
                  w_flag = true;
                  break;
                }
              }
              if(!w_flag) { // write directly in case failed 3 times.
                memcpy((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), (void *)&shm_data, sizeof(shm_data));
              }
#else
              app_shmem_sem_wait();
              memcpy((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), (void *)&shm_data, sizeof(shm_data));
              app_shmem_sem_post();
#endif
              break;
            case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE:  // 'Sensor'
              /* TODO: */
              break;
            case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE:  // 'SensorData'
#if (1)
              if(MODBUS_SENSOR_TYPE_P == g_sensor_type_u8) { // insight-P
#if (0)
                if(backup_flag) { // nameid changed case: it should be corner case
                  for(uint8_t i=0;i<2;i++) {
                    ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                    if(0 == ret) {
                        for(uint8_t j=0;j<g_sensor_data_piece_cnt_p;j++) {
                          memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx_p - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                                (void *)&g_shm_data_buf_p[j],
                                  sizeof(shm_sensor_data));
                        }
                      // memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE), (void *)&g_shm_data_buf[0], APP_SHM_SENSORDATA_TABLE_PIECE_SIZE * g_sensor_data_piece_cnt);
                      app_shmem_sem_post();
                      w_flag = true;
                      break;
                    }
                  }
                  if(!w_flag) { // write directly in case failed 3 times.
                    for(uint8_t j=0;j<g_sensor_data_piece_cnt_p;j++) {
                      memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx_p - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                            (void *)&g_shm_data_buf_p[j],
                              sizeof(shm_sensor_data));
                    }
                    // memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE), (void *)&g_shm_data_buf[0], APP_SHM_SENSORDATA_TABLE_PIECE_SIZE * g_sensor_data_piece_cnt);
                    LOG_WARN(OUTPOINT, "[shm_p]force write shm backup(ch:%d,nid:%d)!\r\n", g_former_ch_idx_p, g_former_nameid_p);
                  }
                  // clear the index & cache here as the data is written into shm successfully here
                  memset((void *)&g_shm_data_buf_p[0], 0, sizeof(shm_sensor_data) * g_sensor_data_piece_cnt_p);
                  // Notes: write new piece data into cache directly as former data is stored into shm successfully
                  memcpy((void *)&g_shm_data_buf_p[0], (void *)&shm_sensor_data, sizeof(shm_sensor_data));
                  g_sensor_data_piece_cnt_p = 1;
                  backup_flag = false;
                } else {
#endif
                  for(uint8_t i=0;i<2;i++) {
                    ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                    if(0 == ret) {
                     for(uint8_t j=0;j<g_sd_instance[g_sd_idx].sd_piece_max;j++) {
                        memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[g_sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                              (void *)&g_sd_instance[g_sd_idx].sd_data_buf[j],
                                sizeof(shm_sensor_data));
                      }
                      // copy the reg bit_mapping into share memory
                      // last item of 'sd_data_buf'                    
                      memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[g_sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                            (void *)&g_sd_instance[g_sd_idx].sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                              sizeof(uint64_t));
                      // memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE), (void *)&g_shm_data_buf[0], sizeof(shm_sensor_data) * g_sensor_data_piece_cnt);
                      app_shmem_sem_post();
                      w_flag = true;
                      break;
                    }
                  }
                  if(!w_flag) { // write directly in case failed 3 times.
                    for(uint8_t j=0;j<g_sd_instance[g_sd_idx].sd_piece_max;j++) {
                      memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[g_sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                            (void *)&g_sd_instance[g_sd_idx].sd_data_buf[j],
                              sizeof(shm_sensor_data));
                    }
                    // copy the reg bit_mapping into share memory
                    // last item of 'sd_data_buf'                    
                    memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[g_sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                          (void *)&g_sd_instance[g_sd_idx].sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                            sizeof(uint64_t));
                    // memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE), (void *)&g_shm_data_buf[0], sizeof(shm_sensor_data) * g_sensor_data_piece_cnt);
                    LOG_WARN(OUTPOINT, "[shm_p]force write shm(ch:%d,nid:%d)!\r\n", g_sd_instance[g_sd_idx].sd_chn_idx, shm_sensor_data.nameId);
                  }
  #if (1) // for debug only remove it later, log the first piece data only for save time
                  printf("retry_p:(idx:%d):\r\n",g_sd_instance[g_sd_idx].sd_chn_idx);
                  uint8_t *tmp = shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_sd_instance[g_sd_idx].sd_chn_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE;
                  // for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
                  for(uint16_t i=0;i<16;i++)
                    printf(" %02x", tmp[i]);
                  // printf("\r\nretry_p:end\r\n");
  #endif
                  // clear the index & cache here as the data is written into shm successfully here
                  memset((void *)&g_sd_instance[g_sd_idx], 0, sizeof(appShmSensorDataInstance_t));
              } else { // insight-T
                if(backup_flag) { // nameid changed case: it should be corner case
                  for(uint8_t i=0;i<2;i++) {
                    ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                    if(0 == ret) {
                      for(uint8_t j=0;j<APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS;j++) {
                        memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                              (void *)&g_shm_data_buf[j],
                                sizeof(shm_sensor_data));
                      }
                      // copy the reg bit_mapping into share memory
                      // last item of 'sd_data_buf'                    
                      memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                            (void *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                              sizeof(uint64_t));
                      // memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE), (void *)&g_shm_data_buf[0], sizeof(shm_sensor_data) * g_sensor_data_piece_cnt);
                      app_shmem_sem_post();
                      w_flag = true;
                      break;
                    }
                  }
                  if(!w_flag) { // write directly in case failed 3 times.
                    for(uint8_t j=0;j<APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS;j++) {
                      memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                            (void *)&g_shm_data_buf[j],
                              sizeof(shm_sensor_data));
                    }
                    // copy the reg bit_mapping into share memory
                    // last item of 'sd_data_buf'                    
                    memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                          (void *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                            sizeof(uint64_t));
                    // memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_former_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE), (void *)&g_shm_data_buf[0], APP_SHM_SENSORDATA_TABLE_PIECE_SIZE * g_sensor_data_piece_cnt);
                    LOG_WARN(OUTPOINT, "[shm]force write shm backup(ch:%d,nid:%d)!\r\n", g_former_ch_idx, g_former_nameid);
                  }
                  // clear the index & cache here as the data is written into shm successfully here
                  memset((void *)&g_shm_data_buf[0], 0, sizeof(shm_sensor_data) * APP_SHM_SENSORDATA_TABLE_PIECE_MAX);
                  // Notes: write new piece data into cache directly as former data is stored into shm successfully
                  uint8_t _idx = shm_get_measure_type_idx_t(shm_sensor_data.dataType);
                  if(_idx < APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS) {
                    memcpy((void *)&g_shm_data_buf[_idx], (void *)&shm_sensor_data, sizeof(shm_sensor_data));
                    shm_set_sd_bit_map(MODBUS_SENSOR_TYPE_T,
                                        (uint64_t *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                                        _idx);
                    g_sensor_data_piece_cnt = 1;
                  }
                  backup_flag = false;
                } else {
                  for(uint8_t i=0;i<2;i++) {
                    ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
                    if(0 == ret) {
                      for(uint8_t j=0;j<APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS;j++) {
                        memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                              (void *)&g_shm_data_buf[j],
                                sizeof(shm_sensor_data));
                      }
                      // copy the reg bit_mapping into share memory
                      // last item of 'sd_data_buf'                    
                      memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                            (void *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                              sizeof(uint64_t));
                      // memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE), (void *)&g_shm_data_buf[0], sizeof(shm_sensor_data) * g_sensor_data_piece_cnt);
                      app_shmem_sem_post();
                      w_flag = true;
                      break;
                    }
                  }
                  if(!w_flag) { // write directly in case failed 3 times.
                    for(uint8_t j=0;j<APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS;j++) {
                      memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                            (void *)&g_shm_data_buf[j],
                              sizeof(shm_sensor_data));
                    }
                    // copy the reg bit_mapping into share memory
                    // last item of 'sd_data_buf'                    
                    memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE) + (APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                          (void *)&g_shm_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1],
                            sizeof(uint64_t));
                    // memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE), (void *)&g_shm_data_buf[0], sizeof(shm_sensor_data) * g_sensor_data_piece_cnt);
                    LOG_WARN(OUTPOINT, "[shm]force write shm(ch:%d,nid:%d)!\r\n", g_current_ch_idx, shm_sensor_data.nameId);
                  }
  #if (1) // for debug only remove it later, log the first piece data only for save time
                  printf("retry-T:(idx:%d):\r\n",g_current_ch_idx);
                  uint8_t *tmp = shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (g_current_ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE;
                  // for(uint16_t i=0;i<sizeof(shm_sensor_data);i++)
                  for(uint16_t i=0;i<16;i++)
                    printf(" %02x", tmp[i]);
                  // printf("\r\nend\r\n");
  #endif
                  // clear the index & cache here as the data is written into shm successfully here
                  memset((void *)&g_shm_data_buf[0], 0, sizeof(shm_sensor_data) * APP_SHM_SENSORDATA_TABLE_PIECE_MAX);
                  // Notes: 
                  g_sensor_data_piece_cnt = 0;
                }
              }
#else
              app_shmem_sem_wait();
              memcpy((void *)(shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + (shm_sensor_data.dataType - 1) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE,
                      (void *)&shm_sensor_data, sizeof(shm_sensor_data));
              app_shmem_sem_post();
#endif
              break;          
            default:
              break;
          }
        }
#endif
      }
    res:
      // report the modification of db to the mqtt process
      if (pdata->from == gw_pro_process_mqtt) {
        to = gw_pro_process_ble;
        report_qid = ble_msgid;
        qid = mqtt_msgid;
      } else {
        to = gw_pro_process_mqtt;
        report_qid = mqtt_msgid;
        qid = ble_msgid;
      }
#ifdef CONFIG_REPORT_TEST
#ifdef CONFIG_REPORT_FALG
      if (!status && exeflag && to == gw_pro_process_ble) {
#else
      if (!status && exeflag) {
#endif
        exeflag = false;
        ipc_sql_report_new_item(table_name,
                                pdata->command_or_response.commandMsg.param
                                    .sqlite_update_item_param.itemIdx,
                                pdata->command_or_response.commandMsg.command,
                                report_qid, to);
        usleep(50000);
      }
#endif
      ipc_sql_response(pdata->command_or_response.commandMsg.command, 
                       pdata->command_or_response.commandMsg.commandId, status,
                       qid, 0, NULL, pdata->from);
    } break;
    case gw_pro_command_sqlite_query_item: {
      LOG_INFO(OUTPOINT, "query\r\n");
      gain_tablename(pdata->command_or_response.commandMsg.param
                         .sqlite_query_item_param.table,
                     table_name, sizeof(table_name) - 1);
      switch (pdata->command_or_response.commandMsg.param
                  .sqlite_query_item_param.table) {
      case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE: {

        query_cb = gateway_cb;
      } break;
      case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE: {
        query_cb = devicelist_cb;
      } break;
      case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE: {
        query_cb = sensor_cb;
      } break;
      case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE: {
        query_cb = sensordata_cb;
      } break;
      }
      memset(&query_data, 0, sizeof(query_data));
      status = sqlite_query(DATABASE_NAME, table_name,
                            pdata->command_or_response.commandMsg.param
                                .sqlite_update_item_param.itemIdx,
                            query_cb);

      if (pdata->from == gw_pro_process_mqtt) {
        qid = mqtt_msgid;
      } else {
        qid = ble_msgid;
      }
      LOG_DEBUG(OUTPOINT, "que:%d\r\n", query_data.dataNum);
      ipc_sql_response(pdata->command_or_response.commandMsg.command, 
                       pdata->command_or_response.commandMsg.commandId, status,
                       qid, query_data.dataNum, query_data.inquiredData,
                       pdata->from);
    } break;
    case gw_pro_command_sqlite_filter: {
      LOG_INFO(OUTPOINT, "filter\r\n");
      memset(&filter_data, 0, sizeof(filter_data));
      gain_tablename(pdata->command_or_response.commandMsg.param
                         .sqlite_filter_item_param.table,
                     table_name, sizeof(table_name) - 1);
      status = sqlite_filter(DATABASE_NAME, table_name,
                             pdata->command_or_response.commandMsg.param
                                 .sqlite_filter_item_param.dataNum,
                             pdata->command_or_response.commandMsg.param
                                 .sqlite_filter_item_param.filterData,
                             fliter_cb);
      if (pdata->from == gw_pro_process_mqtt) {
        qid = mqtt_msgid;
      } else {
        qid = ble_msgid;
      }
      ipc_sql_response(pdata->command_or_response.commandMsg.command, 
                       pdata->command_or_response.commandMsg.commandId, status,
                       qid, filter_data.dataNum, filter_data.filteredItemIdx,
                       pdata->from);

    } break;
    case gw_pro_command_sqlite_delete_item: {
      LOG_INFO(OUTPOINT, "delete\r\n");
      testflag = false;
      gain_tablename(pdata->command_or_response.commandMsg.param
                         .sqlite_delete_item_param.table,
                     table_name, sizeof(table_name) - 1);
      if (pdata->command_or_response.commandMsg.param.sqlite_delete_item_param
              .table == GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE) {
        status = sqlite_query(DATABASE_NAME, table_name,
                              pdata->command_or_response.commandMsg.param
                                  .sqlite_update_item_param.itemIdx,
                              sensordata_cb);
        if (testflag) {
          remove(filename);
        }
      }
#if (ENABLE_MODBUS_FEATURE == 1)
// TODO: build, insert data of share memory here( use a list to cache the items of SensorData for performance ?)
      if(modbus_feature_enable) {
        switch (pdata->command_or_response.commandMsg.param.sqlite_delete_item_param.table)
        {
          case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:  // 'Gateway'
            break;
          case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE:  // 'Dev'
            // find the nameID to be deleted
            find_flag = false;
            w_flag = false;
            switch (g_modbus_reg_addr_mode)
            {
            case MODBUS_REG_ADDR_EXTENSIBLE_MODE:
              for(uint8_t i=0;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++) {
                if(chn_nameid_map[i].nameid == pdata->command_or_response.commandMsg.param.sqlite_delete_item_param.itemIdx) {
                  chn_nameid_map[i].nameid = 0;
                  ch_idx = chn_nameid_map[i].ch;
                  chn_nameid_map[i].validflag = 0;
                  chn_nameid_map[i].ch = 0;
                  find_flag = true;
                  break;
                }
              }
              break;
            case MODBUS_REG_ADDR_EFFICIENT_MODE:
              for(uint8_t i=0;i<(BULLET_GW_SENSOR_NUM_MAX + 1);i++) {
                if(efficient_chn_nameid_map[i].nameid == pdata->command_or_response.commandMsg.param.sqlite_delete_item_param.itemIdx) {
                  efficient_chn_nameid_map[i].nameid = 0;
                  ch_idx = efficient_chn_nameid_map[i].ch;
                  efficient_chn_nameid_map[i].validflag = 0;
                  efficient_chn_nameid_map[i].ch = 0;
                  // clear flag, ch, and nameid only. DO NOT clear offset & len here
                  find_flag = true;
                  break;
                }
              }
              break;
            default:
              LOG_ERR(OUTPOINT, "[shm del dev]wrong mode(%d)\r\n", g_modbus_reg_addr_mode);
              break;
            }
            if(!find_flag) {
              LOG_WARN(OUTPOINT, "[shm]nameID:%d not found!\r\n", pdata->command_or_response.commandMsg.param.sqlite_delete_item_param.itemIdx);
              w_flag = true;
              goto NEXT_STEP;
            } else if((0 == ch_idx) || (ch_idx > BULLET_GW_SENSOR_NUM_MAX)){
              LOG_WARN(OUTPOINT, "[shm]illegal ch idx:%d skip!\r\n", ch_idx);
              w_flag = true;
              goto NEXT_STEP;
            }
            assigned_chn_cnt--;
            ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
            if(0 == ret) {
              memset((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), 0, APP_SHM_DEV_TABLE_ITEM_SIZE);
              app_shmem_sem_post();
              w_flag = true;
              break;
            }
            break;
          case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE:  // 'Sensor'
          case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE:  // 'SensorData'
            break;          
          default:
            break;
        }
      }
      NEXT_STEP:
#endif
      status = sqlite_delete(DATABASE_NAME, 
                             table_name,
                             pdata->command_or_response.commandMsg.param
                                 .sqlite_delete_item_param.itemIdx,
                             pdata->command_or_response.commandMsg.param
                                 .sqlite_delete_item_param.dataNum,
                             &pdata->command_or_response.commandMsg.param
                                 .sqlite_delete_item_param.deleteData);
#if (ENABLE_MODBUS_FEATURE == 1)
// TODO: build, insert data of share memory here( use a list to cache the items of SensorData for performance ?)
      if(modbus_feature_enable && !w_flag) {
        switch (pdata->command_or_response.commandMsg.param.sqlite_delete_item_param.table)
        {
          case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE:  // 'Gateway'
            break;
          case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE:  // 'Dev'
            for(uint8_t i=0;i<2;i++) {
              ret = app_shmem_sem_trywait();  // fetch semphore with 'trywait' to avoid blocking process 
              if(0 == ret) {
                memset((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), 0, APP_SHM_DEV_TABLE_ITEM_SIZE);
                app_shmem_sem_post();
                w_flag = true;
                break;
              }
            }
            if(!w_flag) { // clear directly in case failed 3 times.
              memset((void *)(shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + (ch_idx - 1) * APP_SHM_DEV_TABLE_ITEM_SIZE), 0, APP_SHM_DEV_TABLE_ITEM_SIZE);
            }
            break;
          case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE:  // 'Sensor'
          case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE:  // 'SensorData'
            break;          
          default:
            break;
        }
      }
#endif
      if (pdata->from == gw_pro_process_mqtt) {
        to = gw_pro_process_ble;
        report_qid = ble_msgid;
        qid = mqtt_msgid;
      } else {
        to = gw_pro_process_mqtt;
        report_qid = mqtt_msgid;
        qid = ble_msgid;
      }
#ifdef CONFIG_REPORT_TEST
#ifdef CONFIG_REPORT_FALG
      if (!status && to == gw_pro_process_ble) {
#else
      if (!status) {
#endif
        ipc_sql_report_new_item(table_name,
                                pdata->command_or_response.commandMsg.param
                                    .sqlite_update_item_param.itemIdx,
                                pdata->command_or_response.commandMsg.command,
                                report_qid, to);
        usleep(50000);
      }
#endif
      ipc_sql_response(pdata->command_or_response.commandMsg.command, 
                       pdata->command_or_response.commandMsg.commandId, status,
                       qid, 0, NULL, pdata->from);
    }
    }
    // usleep(200000);
  }
}
