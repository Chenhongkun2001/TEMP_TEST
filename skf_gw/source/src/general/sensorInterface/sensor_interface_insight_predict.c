/**
 * @file    sensor_interface_insight_predict.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Implementation of Insight Predict Interfaces.
 * @details
 */
#include <pthread.h>
#include <fcntl.h>
#include "sensorInterface/sensor_interface_insight_predict.h"
#include "froto/app_froto_new.h"
#include "froto/decoder/app_froto_data_upload_decoder.h"
#include "froto/decoder/app_froto_fuota_decoder.h"
#include "froto/decoder/app_froto_config_decoder.h"
#include "froto/encoder/app_froto_data_selection_encoder.h"
#include "froto/encoder/app_froto_fuota_encoder.h"
#include "froto/encoder/app_froto_config_encoder.h"
#include "froto/encoder/app_froto_command_encoder.h"
#include "Common.pb.h"
#include "util_dbg.h"
#include "unittest.h"
#include "app_common.h"
#include "app_db.h"

DBG_LOCAL_LOG_DEBUG

#if (SKF_GW_NEW == 1)
#ifdef UNITTEST
#if (UNITTEST == UNITTEST_FROTO_CODEC)
#define REGISTER_EMULATED_SENDING (1)
#endif /* UNITTEST == UNITTEST_FROTO_CODEC */
#else /* UNITTEST */
#define REGISTER_EMULATED_SENDING (0)
#endif /* UNITTEST */

#define INSIGHT_PREDICT_WAVEFORM_DATA_START (0)
#define INSIGHT_PREDICT_WAVEFORM_DATA_END (5)
#define INSIGHT_PREDICT_DATA_COLLECT_MAX_NUMBER (3)
#define INSIGHT_PREDICT_CONFIG_RETRIEVE_MAX_NUMBER (3)
#define INSIGHT_PREDICT_CONFIG_DISSEM_MAX_NUMBER (3)

// Whether collect the ACC waveform (0 indicates no need to collect)
#ifndef SENSOR_DATA_COL_RULE_REQUIRING_ACC_WAVE
#define SENSOR_DATA_COL_RULE_REQUIRING_ACC_WAVE (1)
#endif
// Whether collect the ENV3 waveform (0 indicates no need to collect)
#ifndef SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE
#define SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE (1)
#endif
// Whether collect the VEL waveform (0 indicates no need to collect)
#ifndef SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE
#define SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE (1)
#endif
// Whether collect the ACC status waveform (0 indicates no need to collect)
#ifndef SENSOR_DATA_COL_RULE_REQUIRING_ACC_STATUS_WAVE
#define SENSOR_DATA_COL_RULE_REQUIRING_ACC_STATUS_WAVE (1)
#endif

// The array of all the supported measurement type
static const SKFChina_Common_MeasurementType collectDataGroup[] = {
  SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE,
  SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE,
  SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE,
  SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE, // x
  SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE, // y
  SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE, // z
  SKFChina_Common_MeasurementType_REMAINING_VOLUME,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_RUN_CODE,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_CONDITON_CODE,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_MAG_PRE,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_VIB_PRE,
  SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT,
  SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MAX,
  SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_START,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_END,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV,
  SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR,
  SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT,
};
// The array of the corresponding sensor type
static const SKFChina_Common_SensorType sensorType[] = {
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_UNKNOWN_SENSOR,
  SKFChina_Common_SensorType_UNKNOWN_SENSOR,
  SKFChina_Common_SensorType_UNKNOWN_SENSOR,
  SKFChina_Common_SensorType_UNKNOWN_SENSOR,
  SKFChina_Common_SensorType_MMC56X3_MAG,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_NTC_TEMPERATURE,
  SKFChina_Common_SensorType_NTC_TEMPERATURE,
  SKFChina_Common_SensorType_NTC_TEMPERATURE,
  SKFChina_Common_SensorType_UNKNOWN_SENSOR,
  SKFChina_Common_SensorType_UNKNOWN_SENSOR,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_IIM4235X_VIBRATION,
  SKFChina_Common_SensorType_NRF52_2_4GHZ_RADIO,
};
// The array of all the supported configurations
static const SKFChina_Common_SpecificConfigItem specificItemGroup[] = {
  SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME,
  SKFChina_Common_SpecificConfigItem_CURRENT_TIME,
  SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_RATE_HZ,
  SKFChina_Common_SpecificConfigItem_ACC_SENSOR_AXIS,
  SKFChina_Common_SpecificConfigItem_ACC_SENSOR_RANGE_G,
  SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_PTS,
  SKFChina_Common_SpecificConfigItem_TEMPERATURE_ALARM_THRESHOLD_CDGREE,
  SKFChina_Common_SpecificConfigItem_BORE_DIAMETER_MM,
  SKFChina_Common_SpecificConfigItem_BATTERY_LOW_ALARM_THRESHOLD_PERCENTAGE,
  SKFChina_Common_SpecificConfigItem_WORK_MODE,
  SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_ADV_DBM,
  SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_AUX_ADV_DBM,
  SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_AUX_ADV_KBPS,
  SKFChina_Common_SpecificConfigItem_QUICKPOLLING_PERIOD_S,
  SKFChina_Common_SpecificConfigItem_REGULARSENSING_PERIOD_S,
  SKFChina_Common_SpecificConfigItem_COMMUNICATION_PERIOD_S,
  SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S,
  SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S,
  SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S,
  SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_PERIOD_S,
  SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S,
  SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_FS_HZ,
  SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_N,
  SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_AXIS_ACQ_EVAL,
  SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_RANGE,
  SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_FS_HZ,
  SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_N,
  SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_AXIS_ACQ_EVAL,
  SKFChina_Common_SpecificConfigItem_ACQ_MAG_FS_HZ,
  SKFChina_Common_SpecificConfigItem_ACQ_MAG_N,
  SKFChina_Common_SpecificConfigItem_ACQ_MAG_AXIS_ACQ_EVAL,
  SKFChina_Common_SpecificConfigItem_FS_COEF,
  SKFChina_Common_SpecificConfigItem_GEE_COEF,
  SKFChina_Common_SpecificConfigItem_V_COEF,
  SKFChina_Common_SpecificConfigItem_MEAS_POSITION,
  SKFChina_Common_SpecificConfigItem_MEAS_LOAD,
  SKFChina_Common_SpecificConfigItem_MEAS_AXIS_VIB,
  SKFChina_Common_SpecificConfigItem_MEAS_AXIS_MAG,
  SKFChina_Common_SpecificConfigItem_VIB_START_FG,
  SKFChina_Common_SpecificConfigItem_VIB_RMS_START_TL,
  SKFChina_Common_SpecificConfigItem_MAG_START_FG,
  SKFChina_Common_SpecificConfigItem_MAG_RMS_START_TL,
  SKFChina_Common_SpecificConfigItem_MAG_STABLE_FG,
  SKFChina_Common_SpecificConfigItem_MAG_RMS_VAR_TH,
  SKFChina_Common_SpecificConfigItem_RPM_VAR_RANGE,
  SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_M,
  SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_N,
  SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_TEMP,
  SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_M,
  SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_N,
  SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_VIB,
  SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_MAG,
  SKFChina_Common_SpecificConfigItem_FUNC_ANOM_TEMP,
  SKFChina_Common_SpecificConfigItem_FUNC_ANOM_OV,
  SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MECH,
  SKFChina_Common_SpecificConfigItem_FUNC_ANOM_BRG,
  SKFChina_Common_SpecificConfigItem_FUNC_ANOM_LUB,
  SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MTR,
  SKFChina_Common_SpecificConfigItem_FUNC_ANOM_GEAR,
  SKFChina_Common_SpecificConfigItem_FUNC_ANOM_FAN,
  SKFChina_Common_SpecificConfigItem_FUNC_ANOM_PUMP,
  SKFChina_Common_SpecificConfigItem_ASSET_LEVEL,
  SKFChina_Common_SpecificConfigItem_FLEX_TYPE,
  SKFChina_Common_SpecificConfigItem_RUN_SPEED_RPM,
  SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFO,
  SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFI,
  SKFChina_Common_SpecificConfigItem_BRG_INFO_BSF,
  SKFChina_Common_SpecificConfigItem_BRG_INFO_FTF,
  SKFChina_Common_SpecificConfigItem_MTR_INFO_FL,
  SKFChina_Common_SpecificConfigItem_MTR_INFO_BAR,
  SKFChina_Common_SpecificConfigItem_GEAR_INFO_TOOTH,
  SKFChina_Common_SpecificConfigItem_FAN_INFO_BLADE,
  SKFChina_Common_SpecificConfigItem_PUMP_INFO_VANE,
  SKFChina_Common_SpecificConfigItem_TEMP_OV_ALERT_CDEGREE,
  SKFChina_Common_SpecificConfigItem_ACC_OV_ALERT,
  SKFChina_Common_SpecificConfigItem_ACC_OV_ALARM,
  SKFChina_Common_SpecificConfigItem_ACC_HAL_ALERT,
  SKFChina_Common_SpecificConfigItem_ACC_HAL_ALARM,
  SKFChina_Common_SpecificConfigItem_VEL_OV_ALERT,
  SKFChina_Common_SpecificConfigItem_VEL_OV_ALARM,
  SKFChina_Common_SpecificConfigItem_VEL_HAL_ALERT,
  SKFChina_Common_SpecificConfigItem_VEL_HAL_ALARM,
  SKFChina_Common_SpecificConfigItem_ENV_OV_ALERT,
  SKFChina_Common_SpecificConfigItem_ENV_OV_ALARM,
  SKFChina_Common_SpecificConfigItem_ENV_HAL_ALERT,
  SKFChina_Common_SpecificConfigItem_ENV_HAL_ALARM,
};
// The array of all the supported command
static const SKFChina_Common_Command commandGroup[] = {
  SKFChina_Common_Command_CLEAR_TEMPERATURE_HISTORY,
  SKFChina_Common_Command_CLOSE_SESSION,
};

static bool ver_info_mtx_initialized = false;
static pthread_mutex_t ver_info_mtx;
static protobuf_codec_current_version_t ver_info;

static bool data_item_info_mtx_initialized = false;
static pthread_mutex_t data_item_info_mtx;
static uint32_t data_item_name_id;
static uint8_t data_item_sensor_name[DB_MAX_LENGTH_SENSOR_NAME];
static uint8_t data_item_sensor_type[DB_MAX_SENSOR_TYPE_STR];
static uint8_t data_item_manufacturer[DB_MAX_LENGTH_MANUFACTURER];
static uint64_t lastVibSensingTime_s;
static uint32_t lastVibSensingMeas_id;
static struct db_data_item *data_item_info;
protobuf_codec_data_pair_ele_data_t *bulkData;

static bool sensor_config_hash_info_mtx_initialized = false;
static pthread_mutex_t sensor_config_hash_info_mtx;
static protobuf_codec_config_hash_t sensor_config_hash_info;

static bool sensor_config_info_mtx_initialized = false;
static pthread_mutex_t sensor_config_info_mtx;
static protobuf_codec_specific_config_t sensor_config_info;

static bool exchangeGenInfo(
  struct sensor_interface *sensorIntf,
  void *user_data,
  uint8_t *err_code);
static bool collectData(
  struct sensor_interface *sensorIntf,
  void *user_data,
  uint8_t *err_code);
static bool sendCmd(
  struct sensor_interface *sensorIntf,
  void *user_data,
  uint8_t *err_code);
static bool updateConfig(
  struct sensor_interface *sensorIntf,
  void *user_data,
  uint8_t *err_code);
static bool fuota(
  struct sensor_interface *sensorIntf,
  void *user_data,
  uint8_t *err_code);

#if (REGISTER_EMULATED_SENDING == 1)
extern bool sentFg;

static bool sendMsg(
  struct sensor_interface *si,
  void *data_to_be_sent,
  uint32_t dataLen,
  uint8_t *err_code);
#endif /* REGISTER_EMULATED_SENDING == 1 */
static bool dbFormatIsFile(const SKFChina_Common_MeasurementType measType);
static bool map_conf_item_to_content(
  SKFChina_Common_SpecificConfigItem kp_conf_item,
  sensorConfig_t *sensorCfg,
  protobuf_codec_config_pair_ele_t *ele,
  bool fromFrotoToDB);
static bool current_version_upload_processor(
  void *parsed_data,
  uint8_t *err_code);
static bool config_hash_upload_processor(
  void *parsed_data,
  uint8_t *err_code);
static bool config_upload_processor(
  void *parsed_data,
  uint8_t *err_code);
static bool simple_data_upload_processor(
  void *parsed_data,
  uint8_t *err_code);
static bool bulk_data_upload_first_pkt_processor(
  void *parsed_data,
  uint8_t *err_code);
static bool bulk_data_upload_remained_pkt_processor(
  void *parsed_data,
  uint8_t *err_code);
static bool image_block_request_processor(
  void *parsed_data,
  uint8_t *err_code);

/**
 * @brief Register a sensor interface (instance) for InsightPredict. An
 * interface would be malloc-ed.
 * @param sensorIntf The returned (registered) interface (only when the
 * routine returns true).
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
bool
registerSensorIntfInsightPredict(
  struct sensor_interface *sensorIntf,
  uint8_t *errCode)
{
  sensorIntf->exchangeGeneralInfo = exchangeGenInfo;
  sensorIntf->collectData = collectData;
  sensorIntf->sendCmd = sendCmd;
  sensorIntf->updateConfig = updateConfig;
  sensorIntf->fuota = fuota;
  sensorIntf->hwTypeValid = false;
  sensorIntf->hwVerValid = false;
  sensorIntf->fwVerValid = false;
  sensorIntf->configHashValid = false;
  sensorIntf->configHashDBValid = false;
  sensorIntf->lastEditTimeValid = false;
  sensorIntf->lastEditTimeDBValid = false;

  if(ver_info_mtx_initialized == false)
  {
    if(pthread_mutex_init(&ver_info_mtx, NULL))
    {
      DBG_LOG_ERR("Failed to initialize mutex");
      return false;
    }
    ver_info_mtx_initialized = true;
  }
  if(data_item_info_mtx_initialized == false)
  {
    if(pthread_mutex_init(&data_item_info_mtx, NULL))
    {
      DBG_LOG_ERR("Failed to initialize mutex");
      return false;
    }
    data_item_info_mtx_initialized = true;
  }
  if(sensor_config_hash_info_mtx_initialized == false)
  {
    if(pthread_mutex_init(&sensor_config_hash_info_mtx, NULL))
    {
      DBG_LOG_ERR("Failed to initialize mutex");
      return false;
    }
    sensor_config_hash_info_mtx_initialized = true;
  }
  if(sensor_config_info_mtx_initialized == false)
  {
    if(pthread_mutex_init(&sensor_config_info_mtx, NULL))
    {
      DBG_LOG_ERR("Failed to initialize mutex");
      return false;
    }
    sensor_config_info_mtx_initialized = true;
  }
#if (REGISTER_EMULATED_SENDING == 1)
  sensorIntf->send = sendMsg;
#endif /* REGISTER_EMULATED_SENDING == 1 */
  return true;
}
/**
 * @brief Unregister a sensor interface (instance) for InsightPredict. The
 * interface would be freed.
 * @param sensorIntf The pointer to the interface (instance) to be
 * unregistered.
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
bool
unregisterSensorIntfInsightPredict(
  struct sensor_interface *sensorIntf,
  uint8_t *errCode)
{
  sensorIntf->exchangeGeneralInfo = NULL;
  sensorIntf->collectData = NULL;
  sensorIntf->sendCmd = NULL;
  sensorIntf->updateConfig = NULL;
  sensorIntf->fuota = NULL;
  sensorIntf->hwTypeValid = false;
  sensorIntf->hwVerValid = false;
  sensorIntf->fwVerValid = false;
  sensorIntf->configHashValid = false;
  sensorIntf->configHashDBValid = false;
  sensorIntf->lastEditTimeValid = false;
  sensorIntf->lastEditTimeDBValid = false;
#if (REGISTER_EMULATED_SENDING == 1)
  sensorIntf->send = NULL;
#endif /* REGISTER_EMULATED_SENDING == 1 */
  app_froto_gabage_clean(
    sensorIntf->sensorAddr,
    sizeof(sensorIntf->sensorAddr),
    NULL);
}
/**
 * @brief This is the general information exchange instance of
 * InsightPredict. In this routine, 1) current time would be set to the
 * sensor and 2) current version would be read.
 * @param sensorIntf The pointer to the interface (instance) to be
 * unregistered.
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
static bool
exchangeGenInfo(
  struct sensor_interface *sensorIntf,
  void *user_data,
  uint8_t *err_code)
{
  bool rt = true;
  uint8_t encodedBuf[200];
  uint32_t encodedPacketLen;
  uint8_t readBuf[200];
  uint32_t readPacketLen;
  uint32_t usedSeqNo;
  SKFChina_Froto_FrotoMsgType msgType;

  uint32_t retrieveTimeout_s = 20;  // orig: 5S  about 6 * 1.5 S
  const uint32_t retry = 3;
  uint8_t _attempt;

  // Check parameters
  if(sensorIntf == NULL)
  {
    rt = false;
    goto _EXIT;
  }

  /******************************************************/
  /**** 1/2 Current time would be set to the sensor *****/
  /******************************************************/
  DBG_LOG_INFO("Set time");

  protobuf_codec_config_pair_ele_t _configPairEle[1];
  protobuf_codec_config_pair_t _configPair;
  uint64_t configTime[1];

  encodedPacketLen = 0;
  usedSeqNo = 0;
  msgType = SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;

  _configPairEle[0].which_config_item =
    SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
  _configPairEle[0].config_item.specific_config_item =
    SKFChina_Common_SpecificConfigItem_CURRENT_TIME;
  _configPairEle[0].has_memory_id = false;
  _configPairEle[0].which_config_content =
    SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
  _configPairEle[0].content.time.time = configTime;
  _configPairEle[0].content.time.size = 1;
  _configPairEle[0].content.time.cnt = 1;

  _configPair.buffer = _configPairEle;
  _configPair.size = sizeof(_configPairEle) / sizeof(_configPairEle[0]);
  _configPair.cnt = sizeof(_configPairEle) / sizeof(_configPairEle[0]);

  configTime[0] = time(NULL);
  app_froto_n_encode_msg_config_dissem(
    SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
    &_configPair,
    SKFChina_Common_ProductType_BULLET_NODE,
    sensorIntf->gwAddr,
    sizeof(sensorIntf->gwAddr),
    sensorIntf->sensorAddr,
    sizeof(sensorIntf->sensorAddr),
    NULL,
    encodedBuf,
    sizeof(encodedBuf),
    &encodedPacketLen,
    &usedSeqNo,
    &msgType,
    NULL);
  if(app_froto_n_simple_send(
       encodedBuf,
       encodedPacketLen,
       (void *)sensorIntf,
       sensorIntf->send,
       NULL) == false)
  {
    rt = false;
    goto _EXIT;
  }

  usleep(1000000);

  /******************************************************/
  /********* 2/2 Current version would be read **********/
  /******************************************************/
  DBG_LOG_INFO("Read version");
  _attempt = 0;
_VERSION_RETRIEVE:
  encodedPacketLen = 0;
  usedSeqNo = 0;
  msgType = SKFChina_Froto_FrotoMsgType_UNKNOWN_MESSAGETYPE;

#if (REGISTER_EMULATED_SENDING == 1)
  uint32_t seqNo = 3;
#endif /* REGISTER_EMULATED_SENDING == 1 */

  app_froto_n_encode_msg_version_retrieve(
    SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
    SKFChina_FirmwareUpdateOverTheAir_RetrievePayload_CURRENT_VERSION,
    sensorIntf->gwAddr,
    sizeof(sensorIntf->gwAddr),
    sensorIntf->sensorAddr,
    sizeof(sensorIntf->sensorAddr),
#if (REGISTER_EMULATED_SENDING == 1)
    &seqNo,
#else
    NULL,
#endif
    encodedBuf,
    sizeof(encodedBuf),
    &encodedPacketLen,
    &usedSeqNo,
    &msgType,
    NULL);

  readPacketLen = 0;

  if(app_froto_n_send_with_reply(
       encodedBuf,
       encodedPacketLen,
       (void *)sensorIntf,
       retrieveTimeout_s,
       sensorIntf->send,
       readBuf,
       sizeof(readBuf),
       &readPacketLen,
       sensorIntf->sensorAddr,
       sizeof(sensorIntf->sensorAddr),
       SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
       SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE,
       false,
       0,
       usedSeqNo,
       NULL) == true)
  {
    // Just for Debug
    DBG_LOG_INFO("readBuf");
    util_dbg_buf_dump(
      readBuf,
      readPacketLen);

    // Put hardware type, hardware version, and firmware version to
    // @p sensorIntf
    pb_istream_t _istream;
    SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
    SKFChina_Froto_FrotoHeader *_Froto_header;

    if(ver_info_mtx_initialized)
    {
      pthread_mutex_lock(&ver_info_mtx);
      ver_info.sensorID = NULL;
      ver_info.sensorIDLen = 0;
      if(true == app_froto_current_version_upload_parser(readBuf,
                                                        readPacketLen,
                                                        &_istream,
                                                        &_SKF_Froto_App,
                                                        &_Froto_header,
                                                        current_version_upload_processor,
                                                        NULL))
      {
        if(sizeof(sensorIntf->sensorAddr) == ver_info.sensorIDLen)
        {
          if(memcmp(sensorIntf->sensorAddr, ver_info.sensorID,
                    ver_info.sensorIDLen) == 0)
          {
            sensorIntf->hwTypeValid = true;
            sensorIntf->hwType = ver_info.hardware_type;
            sensorIntf->hwVerValid = true;
            sensorIntf->hwVer = ver_info.hardware_version;
            sensorIntf->fwVerValid = true;
            sensorIntf->fwVer = ver_info.firmware_version;
          }
        }
      }
      l_free(ver_info.sensorID);
      ver_info.sensorIDLen = 0;
      pthread_mutex_unlock(&ver_info_mtx);
    }
  }
  else
  {
    if(_attempt < retry)
    {
      _attempt++;
      DBG_LOG_WARN("Get version retry %d\n", _attempt);
      sleep(2);
      goto _VERSION_RETRIEVE;
    }else{
      rt = false;
      goto _EXIT;
    }
  }

_EXIT:
  return rt;
}
/**
 * @brief This is the data collection instance of InsightPredict. In this
 * routine, 1) collect short data first and 2) collect long data (i.e.,
 * waveform) depends on the corresponding waveform configuration.
 * @param sensorIntf The pointer to the interface (instance) to be
 * unregistered.
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
static bool
collectData(
  struct sensor_interface *sensorIntf,
  void *user_data,
  uint8_t *err_code)
{
  bool rt = true;
  uint8_t encodedBuf[200];
  uint32_t encodedPacketLen;
  uint8_t readBuf[412];
  uint32_t readPacketLen;
  uint32_t usedSeqNo;
  SKFChina_Froto_FrotoMsgType msgType;

  const retrieveTimeout_s = 20;  // original 3
  const uint32_t short_data_retry = 3;
  const uint32_t long_data_retry = 3;
  uint8_t _attempt = 0;

#if (REGISTER_EMULATED_SENDING == 1)
  uint32_t seqNo = 7;
#endif /* REGISTER_EMULATED_SENDING == 1 */

  // Check parameters
  if(sensorIntf == NULL)
  {
    rt = false;
    goto _EXIT;
  }
  if(data_item_info_mtx_initialized == false)
  {
    rt = false;
    goto _EXIT;
  }

  /******************************************************/
  /************** 1/2 Collect short data ****************/
  /******************************************************/
  DBG_LOG_INFO("Collect short data");
  _attempt = 0;
  for(uint32_t i = INSIGHT_PREDICT_WAVEFORM_DATA_END + 1,
      delta = INSIGHT_PREDICT_DATA_COLLECT_MAX_NUMBER;
      i < sizeof(collectDataGroup) / sizeof(collectDataGroup[0]);
      )
  {
    if((i + INSIGHT_PREDICT_DATA_COLLECT_MAX_NUMBER) >
       sizeof(collectDataGroup) / sizeof(collectDataGroup[0]))
    {
      delta = sizeof(collectDataGroup) / sizeof(collectDataGroup[0]) - i;
    }
    _attempt = 0;
_SHORT_DATA_RETRY:
    app_froto_n_encode_msg_data_selection(
      SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
      &collectDataGroup[i],
      NULL,
      &sensorType[i],
      delta,
      SKFChina_Common_ProductType_BULLET_NODE,
      NULL,
      sensorIntf->gwAddr,
      sizeof(sensorIntf->gwAddr),
      sensorIntf->sensorAddr,
      sizeof(sensorIntf->sensorAddr),
#if (REGISTER_EMULATED_SENDING == 1)
      &seqNo,
#else
      NULL,
#endif
      encodedBuf,
      sizeof(encodedBuf),
      &encodedPacketLen,
      &usedSeqNo,
      &msgType,
      NULL);

#if (REGISTER_EMULATED_SENDING == 1)
    seqNo++;
#endif

    i = i + delta;

    readPacketLen = 0;

    if(app_froto_n_send_with_reply(
         encodedBuf,
         encodedPacketLen,
         (void *)sensorIntf,
         retrieveTimeout_s,
         sensorIntf->send,
         readBuf,
         sizeof(readBuf),
         &readPacketLen,
         sensorIntf->sensorAddr,
         sizeof(sensorIntf->sensorAddr),
         SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
         SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE,
         false,
         0,
         usedSeqNo,
         NULL) == true)
    {
      // Just for Debug
      DBG_LOG_INFO("readBuf");
      util_dbg_buf_dump(
        readBuf,
        readPacketLen);

      // Parse and store to database
      pb_istream_t _istream;
      SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
      SKFChina_Froto_FrotoHeader *_Froto_header;

      pthread_mutex_lock(&data_item_info_mtx);
      data_item_name_id = sensorIntf->sensorNameId;
      strncpy(data_item_sensor_name, sensorIntf->sensorBLEName,
              sizeof(sensorIntf->sensorBLEName));
      strncpy(data_item_sensor_type, sensorIntf->sensorPrductType,
              sizeof(sensorIntf->sensorPrductType));
      strncpy(data_item_manufacturer, sensorIntf->sensorManufacturer,
              sizeof(sensorIntf->sensorManufacturer));
      lastVibSensingTime_s = 0;
      lastVibSensingMeas_id = 0;
      app_froto_data_upload_parser(readBuf,
                                   readPacketLen,
                                   &_istream,
                                   &_SKF_Froto_App,
                                   &_Froto_header,
                                   simple_data_upload_processor,
                                   NULL);
      if(sensorIntf->userData != NULL)
      {
        if(lastVibSensingTime_s != 0)
        {
          ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
          lastRegularSensingTime_s = lastVibSensingTime_s;
        }
        if(lastVibSensingMeas_id != 0)
        {
          ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
          regularSensingMeas_id = lastVibSensingMeas_id;
        }
      }
      pthread_mutex_unlock(&data_item_info_mtx);
    }
    else
    {
      if(_attempt < short_data_retry)
      {
        _attempt++;
        i = i - delta;
        DBG_LOG_WARN("Short data collect retry %d\n", _attempt);
        sleep(2);
        goto _SHORT_DATA_RETRY;
      }else{
        rt = false;
        goto _EXIT;
      }
    }
  }

  /******************************************************/
  /******** 2/2 Collect long data (i.e., waveform) ******/
  /******************************************************/
  DBG_LOG_INFO("Collect long data");

  struct pthread_cond_var cond_var;
  void *state = NULL;
  struct db_data_item *data_item = NULL;
  protobuf_codec_data_pair_ele_data_t *bulkDataBuf = NULL;
  uint32_t totalBlock;

#if (REGISTER_EMULATED_SENDING == 1)
  seqNo = 16;
#endif /* REGISTER_EMULATED_SENDING == 1 */
#if (REGISTER_EMULATED_SENDING != 1)
  if(sensorIntf->userData != NULL)
  {
    uint64_t kp_tstamp = time(NULL);
    uint64_t expLastRegularSensingTs;
    uint64_t lastRegularSensingTs =
      ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
      lastRegularSensingTime_s;
    uint32_t period_regularSensing_s =
      ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
      regularSensingPeriod_s;
    uint32_t period_wave_s =
      ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
      waveformPeriod_s;
    uint32_t refTime_regularSensing_s =
      ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
      regularSensingRef_s;
    uint32_t refTime_wave_s =
      ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
      waveformRef_s;
    uint32_t lastRegularSensingMeasID = 
      ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
      regularSensingMeas_id;

    DBG_LOG_DEBUG("lastRegularSensingTs: %u", lastRegularSensingTs);
    DBG_LOG_DEBUG("period_regularSensing_s: %u", period_regularSensing_s);
    DBG_LOG_DEBUG("period_wave_s: %u", period_wave_s);
    DBG_LOG_DEBUG("refTime_regularSensing_s: %u",
                  refTime_regularSensing_s);
    DBG_LOG_DEBUG("refTime_wave_s: %u", refTime_wave_s);
    DBG_LOG_DEBUG("current time: %d", kp_tstamp);
    DBG_LOG_INFO("measurement id: %d", lastRegularSensingMeasID);

    if((kp_tstamp > refTime_regularSensing_s) &&
       (kp_tstamp > refTime_wave_s))
    {
      expLastRegularSensingTs = 
        refTime_regularSensing_s + 
        ((kp_tstamp - refTime_regularSensing_s) / 
        period_regularSensing_s) * period_regularSensing_s;
      DBG_LOG_DEBUG("lastRegularSensingTs (computed): %u", expLastRegularSensingTs);
    }

    if(lastRegularSensingTs < expLastRegularSensingTs)
    {
      // If lastRegularSensing is earlier than expLastRegularSensingTs,
      // then lastRegularSensing would be updated.
      lastRegularSensingTs = expLastRegularSensingTs;
      ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
      lastRegularSensingTime_s = lastRegularSensingTs;
    }      

    if(((lastRegularSensingTs != 0) &&
        (period_regularSensing_s != 0) &&
        (period_wave_s != 0) &&
        (kp_tstamp > refTime_regularSensing_s) &&
        (kp_tstamp > refTime_wave_s)) ||
       (lastRegularSensingMeasID >= 32768))
    {
      if(
        (((lastRegularSensingTs) >=
          ((lastRegularSensingTs - refTime_wave_s) / period_wave_s) *
          period_wave_s + refTime_wave_s) &&
         ((lastRegularSensingTs - period_regularSensing_s) <
          ((lastRegularSensingTs - refTime_wave_s) / period_wave_s) *
          period_wave_s + refTime_wave_s)
        ) ||
        (lastRegularSensingMeasID >= 32768))
      {
        // Collect the sensor
        DBG_LOG_INFO("Collecting waveform is required");
      }
      else
      {
        // No need to collect sensor
        DBG_LOG_DEBUG("No waveform is required to collect");
        goto _EXIT;
      }
    }
    else
    {
      // No need to collect sensor
      DBG_LOG_DEBUG("No waveform is required to collect");
      goto _EXIT;
    }
  }
#endif
  uint8_t _dimension = SKFChina_Common_Dimension_UNKNOWN_DIMENSION;
  uint8_t *_pt_dimension = NULL;
  bool _skip_pkt_fg = false;
  _attempt = 0;
  for(uint32_t i = INSIGHT_PREDICT_WAVEFORM_DATA_START;
      i < INSIGHT_PREDICT_WAVEFORM_DATA_END + 1;
      i++)
  {
    _attempt = 0;
#if (SENSOR_DATA_COL_RULE_REQUIRING_ACC_WAVE != 1)
    if(collectDataGroup[i] ==
       SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE)
    {
      _pt_dimension = NULL;
      _dimension = SKFChina_Common_Dimension_UNKNOWN_DIMENSION;
      continue;
    }
#else
    if(collectDataGroup[i] ==
       SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE)
    {
      _pt_dimension = NULL;
      _dimension = SKFChina_Common_Dimension_UNKNOWN_DIMENSION;
    }
#endif
#if (SENSOR_DATA_COL_RULE_REQUIRING_ENV3_WAVE != 1)
    if(collectDataGroup[i] ==
       SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE)
    {
      _pt_dimension = NULL;
      _dimension = SKFChina_Common_Dimension_UNKNOWN_DIMENSION;
      continue;
    }
#else
    if(collectDataGroup[i] ==
       SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE)
    {
      _pt_dimension = NULL;
      _dimension = SKFChina_Common_Dimension_UNKNOWN_DIMENSION;
    }
#endif
#if (SENSOR_DATA_COL_RULE_REQUIRING_VEL_WAVE != 1)
    if(collectDataGroup[i] ==
       SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE)
    {
      _pt_dimension = NULL;
      _dimension = SKFChina_Common_Dimension_UNKNOWN_DIMENSION;
      continue;
    }
#else
    if(collectDataGroup[i] ==
       SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE)
    {
      _pt_dimension = NULL;
      _dimension = SKFChina_Common_Dimension_UNKNOWN_DIMENSION;
    }
#endif
#if (SENSOR_DATA_COL_RULE_REQUIRING_ACC_STATUS_WAVE != 1)
    if(collectDataGroup[i] ==
       SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE)
    {
      _pt_dimension = NULL;
      _dimension = SKFChina_Common_Dimension_UNKNOWN_DIMENSION;
      continue;
    }
#else
    if(collectDataGroup[i] ==
       SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE)
    {
      _pt_dimension = &_dimension;
      switch(_dimension)
      {
        case SKFChina_Common_Dimension_DIMENSION_X:
          _dimension = SKFChina_Common_Dimension_DIMENSION_Y;
          break;
        case SKFChina_Common_Dimension_DIMENSION_Y:
          _dimension = SKFChina_Common_Dimension_DIMENSION_Z;
          break;
        case SKFChina_Common_Dimension_UNKNOWN_DIMENSION:
        default:
          _dimension = SKFChina_Common_Dimension_DIMENSION_X;
          break;
      }
    }
#endif
_LONG_DATA_RETRY:
    _skip_pkt_fg = false;
#if (REGISTER_EMULATED_SENDING != 1)
    // Check whether the waveform has existed in database
    if(sensorIntf->userData != NULL)
    {
      struct db_data_item di;
      uint8_t sensorAddrStr[13];

      snprintf(sensorAddrStr, sizeof(sensorAddrStr),
               "%02X%02X%02X%02X%02X%02X",
               sensorIntf->sensorAddr[0],
               sensorIntf->sensorAddr[1],
               sensorIntf->sensorAddr[2],
               sensorIntf->sensorAddr[3],
               sensorIntf->sensorAddr[4],
               sensorIntf->sensorAddr[5]);
      di.meas_ts =
        ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
        lastRegularSensingTime_s;
      di.data_type = collectDataGroup[i];
      if(_pt_dimension == NULL)
      {
        di.dimension = 
          SKFChina_Common_Dimension_UNKNOWN_DIMENSION;
      }else{
        di.dimension = *_pt_dimension;
      }
      
      if(true == is_dtitem_in_db(&di, sensorAddrStr))
      {
        DBG_LOG_INFO("Waveform with type [%d] has existed",
                      collectDataGroup[i]);
        continue;
      }
    }
#endif
    froto_data_selection_filter_t _ft;
    _ft.start_time_s = ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
        lastRegularSensingTime_s;
    _ft.end_time_s = time(NULL);
    app_froto_n_encode_msg_data_selection(
      SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
      &collectDataGroup[i],
      _pt_dimension,
      &sensorType[i],
      1,
      SKFChina_Common_ProductType_BULLET_NODE,
      &_ft,
      sensorIntf->gwAddr,
      sizeof(sensorIntf->gwAddr),
      sensorIntf->sensorAddr,
      sizeof(sensorIntf->sensorAddr),
#if (REGISTER_EMULATED_SENDING == 1)
      &seqNo,
#else
      NULL,
#endif
      encodedBuf,
      sizeof(encodedBuf),
      &encodedPacketLen,
      &usedSeqNo,
      &msgType,
      NULL);

#if (REGISTER_EMULATED_SENDING == 1)
    seqNo++;
#endif

    readPacketLen = 0;

    if(app_froto_n_send_with_multiple_replies_first_half(
         encodedBuf,
         encodedPacketLen,
         (void *)sensorIntf,
         retrieveTimeout_s,
         sensorIntf->send,
         readBuf,
         sizeof(readBuf),
         &readPacketLen,
         sensorIntf->sensorAddr,
         sizeof(sensorIntf->sensorAddr),
         SKFChina_Froto_FrotoPmtType_SIMPLE_BULK_UPLOAD,
         SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE,
         false,
         0,
         usedSeqNo,
         &cond_var,
         &state,
         NULL) == true)
    {
      // Just for Debug
      DBG_LOG_INFO("readBuf");
      util_dbg_buf_dump(
        readBuf,
        readPacketLen);

      // Parse and store to file
      pb_istream_t _istream;
      SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
      SKFChina_Froto_FrotoHeader *_Froto_header;

      pthread_mutex_lock(&data_item_info_mtx);
      data_item_info = NULL;
      data_item_name_id = sensorIntf->sensorNameId;
      strncpy(data_item_sensor_name, sensorIntf->sensorBLEName,
              sizeof(sensorIntf->sensorBLEName));
      strncpy(data_item_sensor_type, sensorIntf->sensorPrductType,
              sizeof(sensorIntf->sensorPrductType));
      strncpy(data_item_manufacturer, sensorIntf->sensorManufacturer,
              sizeof(sensorIntf->sensorManufacturer));
      app_froto_data_upload_parser(readBuf,
                                   readPacketLen,
                                   &_istream,
                                   &_SKF_Froto_App,
                                   &_Froto_header,
                                   bulk_data_upload_first_pkt_processor,
                                   NULL);
      data_item = data_item_info;
      pthread_mutex_unlock(&data_item_info_mtx);

      totalBlock = _Froto_header->total_block;

      if(data_item == NULL)
      {
        DBG_LOG_WARN(
            "A packet with nothing received ...");  
        continue;
      }

      if(totalBlock == 1)
      {
        // If there is only one packet ...
        close(data_item->value.fpath.fd);
        if(data_item->data_type != 
          SKFChina_Common_MeasurementType_UNKNOWN_MEASUREMENT_TYPE)
        {
          struct database_operation_controller *pt_db_op_ctr =
            NULL;
          struct db_op_msg *pt_op_msg = NULL;

          DBG_LOG_INFO(
            "Trigger the thread to update sensor data to database\n");
          pt_db_op_ctr = app_db_get_database_op_ctr();
          pthread_mutex_lock(&pt_db_op_ctr->mtx);

          pt_op_msg =
            (struct db_op_msg *)l_malloc(sizeof(struct db_op_msg));
          if(pt_op_msg)
          {
            memset(pt_op_msg, 0, sizeof(struct db_op_msg));
            pt_op_msg->cmd = gw_pro_command_sqlite_update_item;
            pt_op_msg->table_idx =
              GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
            pt_op_msg->cmd_para.pt_dtitem = data_item;
            l_queue_push_tail(pt_db_op_ctr->pt_dtitem_queue,
                              (void *)pt_op_msg);
          }
          else
          {
            DBG_LOG_ERR("Failed to malloc");
            l_free(data_item);
          }

          DBG_LOG_DEBUG("To signal data available");
          pthread_cond_signal(&pt_db_op_ctr->cond_dt_available);
          pthread_mutex_unlock(&pt_db_op_ctr->mtx);
        }else{
          DBG_LOG_WARN("Unknown measurement type. Maybe no data is found in the given time range.");
          l_free(data_item);
        }
      }
      else
      {
        for(uint32_t blkCnt = 1; blkCnt < totalBlock; blkCnt++)
        {
#if (REGISTER_EMULATED_SENDING == 1)
          sentFg = true;
#endif /* REGISTER_EMULATED_SENDING == 1 */

          if(app_froto_n_send_with_multiple_replies_second_half(
               retrieveTimeout_s,
               readBuf,
               sizeof(readBuf),
               &readPacketLen,
               sensorIntf->sensorAddr,
               sizeof(sensorIntf->sensorAddr),
               SKFChina_Froto_FrotoPmtType_SIMPLE_BULK_UPLOAD,
               SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE,
               false,
               0,
               usedSeqNo,
               &cond_var,
               &state,
               ((blkCnt + 1) >= totalBlock),
               NULL) == true)
          {
            // Just for Debug
            DBG_LOG_INFO("readBuf");
            util_dbg_buf_dump(
              readBuf,
              readPacketLen);

            // Parse and store to file
            // Save to data if the all the packets have been received
            pb_istream_t _istream;
            SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
            SKFChina_Froto_FrotoHeader *_Froto_header;

            pthread_mutex_lock(&data_item_info_mtx);
            bulkData = NULL;
            app_froto_data_upload_parser(readBuf,
                                         readPacketLen,
                                         &_istream,
                                         &_SKF_Froto_App,
                                         &_Froto_header,
                                         bulk_data_upload_remained_pkt_processor,
                                         NULL);
            bulkDataBuf = bulkData;
            if(bulkData != NULL)
            {
              DBG_LOG_DEBUG("bulkData->data @ %p, bulkData->dataLen %d", 
                            bulkData->data, bulkData->dataLen);
            }
            pthread_mutex_unlock(&data_item_info_mtx);

            if(bulkDataBuf == NULL)
            {
              DBG_LOG_ERR("bulkDataBuf is NULL for unknown reason");
              DBG_LOG_ERR("bulkData @ %p, bulkDataBuf @ %p", bulkData, bulkDataBuf);
            }
            else
            {
              if(_Froto_header->current_block == blkCnt)
              {
                if(_skip_pkt_fg == false)
                {
                  // This is the expected block
                  write(data_item->value.fpath.fd,
                        bulkDataBuf->data,
                        bulkDataBuf->dataLen);
                  l_free(bulkDataBuf->data);
                  l_free(bulkDataBuf);
                  if((blkCnt + 1) >= totalBlock)
                  {
                    // The last packet
                    close(data_item->value.fpath.fd);
                    {
                      struct database_operation_controller *pt_db_op_ctr =
                        NULL;
                      struct db_op_msg *pt_op_msg = NULL;

                      DBG_LOG_INFO(
                        "Trigger the thread to update sensor data to database");
                      pt_db_op_ctr = app_db_get_database_op_ctr();
                      pthread_mutex_lock(&pt_db_op_ctr->mtx);

                      pt_op_msg =
                        (struct db_op_msg *)l_malloc(sizeof(struct db_op_msg));
                      if(pt_op_msg)
                      {
                        memset(pt_op_msg, 0, sizeof(struct db_op_msg));
                        pt_op_msg->cmd = gw_pro_command_sqlite_update_item;
                        pt_op_msg->table_idx =
                          GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
                        pt_op_msg->cmd_para.pt_dtitem = data_item;
                        l_queue_push_tail(pt_db_op_ctr->pt_dtitem_queue,
                                          (void *)pt_op_msg);
                      }
                      else
                      {
                        DBG_LOG_ERR("Failed to malloc");
                        l_free(data_item);
                      }

                      DBG_LOG_DEBUG("To signal data available");
                      pthread_cond_signal(&pt_db_op_ctr->cond_dt_available);
                      pthread_mutex_unlock(&pt_db_op_ctr->mtx);
                    }
                  }
                }
              }
              else
              {
                if(_skip_pkt_fg == false)
                {
                  DBG_LOG_ERR(
                    "The packet block number is not as expected, then abort receiving");
                  l_free(bulkDataBuf->data);
                  l_free(bulkDataBuf);
                  // do a basic sanity-check as it may coredump occasionally ==> "No such file or directory"
                  // data_item would be NULL in case the first pkt was not recv-ed.
                  if(NULL != data_item)
                  {
                    close(data_item->value.fpath.fd);
                    l_free(data_item);
                  }
                  else
                  {
                    DBG_LOG_ERR("data_item is null! The fist packet missed, then abort receiving!\n");
                  }
                  _skip_pkt_fg = true;
                }
              }
            }
          }
          else
          {
            // do a basic sanity-check as it may coredump occasionally ==> "No such file or directory"
            // data_item would be NULL in case the first pkt was not recv-ed.
            if(_skip_pkt_fg == false)
            {
              if(NULL != data_item)
              {
                close(data_item->value.fpath.fd);
                l_free(data_item);
              }
              else
              {
                DBG_LOG_ERR("data_item is null! then abort receiving!\n");
              }
            }
            if(_attempt < long_data_retry)
            {
              _attempt++;
              DBG_LOG_WARN("Long data collect retry %d\n", _attempt);
              sleep(2);
              goto _LONG_DATA_RETRY;
            }else{
              rt = false;
              goto _EXIT;
            }
          }
        }
        if(_skip_pkt_fg == true)
        {
          if(_attempt < long_data_retry)
          {
            _attempt++;
            DBG_LOG_WARN("_skip_pkt_fg is true and long data collect retry %d\n", _attempt);
            sleep(2);
            goto _LONG_DATA_RETRY;
          }else{
            DBG_LOG_ERR("_skip_pkt_fg is true and no more long data collect retry %d\n", _attempt);
            rt = false;
            goto _EXIT;
          }
        }
      }
    }else{
      if(_attempt < long_data_retry)
      {
        _attempt++;
        DBG_LOG_WARN("_skip_pkt_fg is true and long data collect retry %d\n", _attempt);
        sleep(2);
        goto _LONG_DATA_RETRY;
      }else{
        DBG_LOG_ERR("_skip_pkt_fg is true and no more long data collect retry %d\n", _attempt);
        rt = false;
        goto _EXIT;
      }
    }
  }

_EXIT:
  return rt;
}
/**
 * @brief This is the configuration update instance of InsightPredict. In
 * this routine, 1) configuration hash would be read from sensor first, and
 * 2) update the sensor configuration or read sensor configuration back to
 * database depends on hash value and the last edit time.
 * @param sensorIntf The pointer to the interface (instance) to be
 * unregistered.
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
static bool
sendCmd(
  struct sensor_interface *sensorIntf,
  void *user_data,
  uint8_t *err_code)
{
  bool rt = true;
  uint8_t encodedBuf[200];
  uint32_t encodedPacketLen;
  uint32_t usedSeqNo;
  SKFChina_Froto_FrotoMsgType msgType;

  // Check parameters
  if(sensorIntf == NULL)
  {
    rt = false;
    goto _EXIT;
  }
  if(sensor_config_hash_info_mtx_initialized == false)
  {
    rt = false;
    goto _EXIT;
  }
  if(sensor_config_info_mtx_initialized == false)
  {
    rt = false;
    goto _EXIT;
  }
#if (REGISTER_EMULATED_SENDING == 1)
  uint32_t seqNo = 201;
#endif /* REGISTER_EMULATED_SENDING == 1 */

  protobuf_codec_command_pair_t _commandPair;

  for(uint32_t i = 0; i < sizeof(commandGroup) / sizeof(commandGroup[0]);
      i++)
  {
    _commandPair.command = commandGroup[i];
    // There is no parameter required so far, just set them to unsigned
    // int32 zeros
    _commandPair.which_command_para =
      SKFChina_ConfigurationAndCommand_CommandPair_general_config_content_uint32_tag;
    _commandPair.para.para_uint32 = 0;

    app_froto_n_encode_msg_command_dissem(
      SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
      &_commandPair,
      SKFChina_Common_ProductType_BULLET_NODE,
      sensorIntf->gwAddr,
      sizeof(sensorIntf->gwAddr),
      sensorIntf->sensorAddr,
      sizeof(sensorIntf->sensorAddr),
#if (REGISTER_EMULATED_SENDING == 1)
      &seqNo,
#else
      NULL,
#endif
      encodedBuf,
      sizeof(encodedBuf),
      &encodedPacketLen,
      &usedSeqNo,
      &msgType,
      NULL);

    if(app_froto_n_simple_send(
         encodedBuf,
         encodedPacketLen,
         (void *)sensorIntf,
         sensorIntf->send,
         NULL) == false)
    {
      rt = false;
      goto _EXIT;
    }

    usleep(500000); // There is an interval between two commands
  }

_EXIT:
  return rt;
}
/**
 * @brief This is the configuration update instance of InsightPredict. In
 * this routine, 1) configuration hash would be read from sensor first, and
 * 2) update the sensor configuration or read sensor configuration back to
 * database depends on hash value and the last edit time.
 * @param sensorIntf The pointer to the interface (instance) to be
 * unregistered.
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
static bool
updateConfig(
  struct sensor_interface *sensorIntf,
  void *user_data,
  uint8_t *err_code)
{
  bool rt = true;
  uint8_t encodedBuf[200];
  uint32_t encodedPacketLen;
  uint8_t readBuf[200];
  uint32_t readPacketLen;
  uint32_t usedSeqNo;
  SKFChina_Froto_FrotoMsgType msgType;
  sensorConfig_t sensorCfg;
  struct database_operation_controller *pt_db_op_ctr = NULL;
  struct dbop_controller *pt_dbop_ctr = NULL;

  pt_db_op_ctr = app_db_get_database_op_ctr();
  pt_dbop_ctr = app_db_get_dbop_controller();

  uint32_t retrieveTimeout_s = 5; // original is 3
  const uint32_t retry = 3;
  uint32_t _attempt = 0;

  // Check parameters
  if(sensorIntf == NULL)
  {
    rt = false;
    goto _EXIT;
  }
  if(sensor_config_hash_info_mtx_initialized == false)
  {
    rt = false;
    goto _EXIT;
  }
  if(sensor_config_info_mtx_initialized == false)
  {
    rt = false;
    goto _EXIT;
  }

  struct fouta_fw_info fw_info;

  memset(&fw_info, 0, sizeof(fw_info));
  if(ST_OK !=
     app_db_fetch_sensor_conf_from_db(app_db_get_dbop_controller(),
                                      &sensorCfg, &fw_info,
                                      sensorIntf->sensorNameId))
  {
    rt = false;
    goto _EXIT;
  }
  // Update to the cache
  if(sensorIntf->userData != NULL)
  {
    ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
    waveformPeriod_s =
      sensorCfg.bulletSensorConfig.schConfig.period_wave_s;
    ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
    regularSensingPeriod_s =
      sensorCfg.bulletSensorConfig.schConfig.period_regularSensing_s;
    ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
    waveformRef_s = sensorCfg.bulletSensorConfig.schConfig.refTime_wave_s;
    ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
    regularSensingRef_s =
      sensorCfg.bulletSensorConfig.schConfig.refTime_regularSensing_s;
  }
  strncpy(sensorIntf->sensorManufacturer, sensorCfg.manufacturer,
          sizeof(sensorCfg.manufacturer));
  strncpy(sensorIntf->sensorPrductType, sensorCfg.type,
          sizeof(sensorCfg.type));

  if((strlen(fw_info.fpath) != 0) &&
     (strlen(fw_info.fpath) < (sizeof(sensorIntf->imageFwVerPath) - 1)))
  {
    sensorIntf->imageFwValid = true;
    sensorIntf->imageFwVer = fw_info.ver;
    strncpy(sensorIntf->imageFwVerPath, fw_info.fpath,
            sizeof(sensorIntf->imageFwVerPath));
  }
  if(sensorIntf->fwVerValid)
  {
    fw_info.currentVer = sensorIntf->fwVer;
  }
  sensorIntf->lastEditTimeDBValid = true;
  sensorIntf->lastEditTimeDB = sensorCfg.lastEditTimeS;

  uint8_t gwAddrStr[13];

  snprintf(gwAddrStr, sizeof(gwAddrStr),
           "%02x%02x%02x%02x%02x%02x",
           sensorIntf->gwAddr[0],
           sensorIntf->gwAddr[1],
           sensorIntf->gwAddr[2],
           sensorIntf->gwAddr[3],
           sensorIntf->gwAddr[4],
           sensorIntf->gwAddr[5]);

  sensorIntf->configHashDBValid = true;
  sensorIntf->configHashDB = util_froto_cal_config_hash_value(&sensorCfg,
                                                              gwAddrStr);
  DBG_LOG_INFO("Local config hash is %llu", sensorIntf->configHashDB);

  // Write back to DB to make sure that current version is exactly the version of sensor
  // sensorCfg has been filled, then save to database
  pthread_mutex_lock(&sensor_config_info_mtx);
  app_db_deliver_sensor_conf_to_db(pt_dbop_ctr, &sensorCfg, &fw_info);
  pthread_mutex_unlock(&sensor_config_info_mtx);

  /******************************************************/
  /************* 1/2 Check the config hash **************/
  /******************************************************/
  DBG_LOG_INFO("Check config hash");

  protobuf_codec_config_pair_t _configPair;

#if (REGISTER_EMULATED_SENDING == 1)
  uint32_t seqNo = 101;
#endif /* REGISTER_EMULATED_SENDING == 1 */
  _attempt = 0;
_CONFIG_HASH_RETRIEVE:
  app_froto_n_encode_msg_config_retrieve(
    SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
    SKFChina_ConfigurationAndCommand_RetrievePayload_CURRENT_CONFIG_HASH,
    sensorIntf->sensorNameId,
    true,
    NULL,
    NULL,
    0,
    sensorIntf->gwAddr,
    sizeof(sensorIntf->gwAddr),
    sensorIntf->sensorAddr,
    sizeof(sensorIntf->sensorAddr),
#if (REGISTER_EMULATED_SENDING == 1)
    &seqNo,
#else
    NULL,
#endif
    encodedBuf,
    sizeof(encodedBuf),
    &encodedPacketLen,
    &usedSeqNo,
    &msgType,
    NULL);

  readPacketLen = 0;

  if(app_froto_n_send_with_reply(
       encodedBuf,
       encodedPacketLen,
       (void *)sensorIntf,
       retrieveTimeout_s,
       sensorIntf->send,
       readBuf,
       sizeof(readBuf),
       &readPacketLen,
       sensorIntf->sensorAddr,
       sizeof(sensorIntf->sensorAddr),
       SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
       SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE,
       false,
       0,
       usedSeqNo,
       NULL) == true)
  {
    // Just for Debug
    DBG_LOG_INFO("readBuf");
    util_dbg_buf_dump(
      readBuf,
      readPacketLen);

    // Parse and store to database
    pb_istream_t _istream;
    SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
    SKFChina_Froto_FrotoHeader *_Froto_header;

    pthread_mutex_lock(&sensor_config_hash_info_mtx);
    sensor_config_hash_info.sensorID = NULL;
    sensor_config_hash_info.sensorIDLen = 0;
    if(true == app_froto_config_hash_upload_parser(readBuf,
                                                   readPacketLen,
                                                   &_istream,
                                                   &_SKF_Froto_App,
                                                   &_Froto_header,
                                                   config_hash_upload_processor,
                                                   NULL))
    {
      if(sizeof(sensorIntf->sensorAddr) ==
         sensor_config_hash_info.sensorIDLen)
      {
        if(memcmp(sensorIntf->sensorAddr, sensor_config_hash_info.sensorID,
                  sensor_config_hash_info.sensorIDLen) == 0)
        {
          sensorIntf->configHashValid = true;
          sensorIntf->configHash = sensor_config_hash_info.hash;
          if(sensor_config_hash_info.has_last_edit_time)
          {
            sensorIntf->lastEditTimeValid = true;
            sensorIntf->lastEditTime =
              sensor_config_hash_info.last_edit_time;
          }
        }
      }
    }
    l_free(sensor_config_hash_info.sensorID);
    sensor_config_hash_info.sensorIDLen = 0;
    pthread_mutex_unlock(&sensor_config_hash_info_mtx);
  }
  else
  {
    if(_attempt < retry)
    {
      _attempt++;
      DBG_LOG_WARN("Config hash retrieve retry %d\n", _attempt);
      sleep(2);
      goto _CONFIG_HASH_RETRIEVE;
    }else{
      rt = false;
      goto _EXIT;
    }
  }

  if((sensorIntf->configHashDBValid == true) &&
     (sensorIntf->configHashValid == true) &&
     (sensorIntf->configHashDB == sensorIntf->configHash) &&
     (sensorIntf->lastEditTimeDBValid == true) &&
     (sensorIntf->lastEditTimeValid == true) &&
     (sensorIntf->lastEditTimeDB == sensorIntf->lastEditTime))
  {
    // Do nothing only when everything is valid and identical
    goto _EXIT;
  }

  /******************************************************/
  /****** 2/2a Write the configuration to sensor ********/
  /* If config hash is different from the local stored  */
  /* hash and the sensor's configuration is updated     */
  /* earlier.                                           */
  /******************************************************/
  if((sensorIntf->configHashDBValid == true) &&
     (sensorIntf->configHashValid == true) &&
     (sensorIntf->configHashDB != sensorIntf->configHash) &&
     (sensorIntf->lastEditTimeDBValid == true) &&
     (sensorIntf->lastEditTimeValid == true) &&
     (sensorIntf->lastEditTimeDB > sensorIntf->lastEditTime))
  {
    _attempt = 0;
    for(uint32_t i = 2, // Skip the first two time-related configurations
        delta = INSIGHT_PREDICT_CONFIG_DISSEM_MAX_NUMBER;
        i < sizeof(specificItemGroup) / sizeof(specificItemGroup[0]);
       )
    {
      if((i + INSIGHT_PREDICT_CONFIG_DISSEM_MAX_NUMBER) >
         sizeof(specificItemGroup) / sizeof(specificItemGroup[0]))
      {
        delta = sizeof(specificItemGroup) / sizeof(specificItemGroup[0]) -
                i;
      }
      _attempt = 0;

      protobuf_codec_config_pair_ele_t _configPairEle[
        INSIGHT_PREDICT_CONFIG_DISSEM_MAX_NUMBER];
      protobuf_codec_config_pair_t _configPair;

      // Prepare the data
      for(uint32_t m = 0; m < delta; m++)
      {
        _configPairEle[m].which_config_item =
          SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
        _configPairEle[m].config_item.specific_config_item =
          specificItemGroup[m + i];
        _configPairEle[m].has_memory_id = false;
        map_conf_item_to_content(
          _configPairEle[m].config_item.specific_config_item,
          &sensorCfg,
          &_configPairEle[m],
          false);
      }
      _configPair.buffer = _configPairEle;
      _configPair.size = delta;
      _configPair.cnt = delta;
_CONFIG_DISSEM:
      app_froto_n_encode_msg_config_dissem(
        SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
        &_configPair,
        SKFChina_Common_ProductType_BULLET_NODE,
        sensorIntf->gwAddr,
        sizeof(sensorIntf->gwAddr),
        sensorIntf->sensorAddr,
        sizeof(sensorIntf->sensorAddr),
        NULL,
        encodedBuf,
        sizeof(encodedBuf),
        &encodedPacketLen,
        &usedSeqNo,
        &msgType,
        NULL);

      for(uint32_t m = i; m < (delta + i); m++)
      {
        if(_configPairEle[m].which_config_item ==
           SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag)
        {
          if((_configPairEle[m].config_item.specific_config_item ==
              SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S)
             ||
             (_configPairEle[m].config_item.specific_config_item ==
              SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S)
             ||
             (_configPairEle[m].config_item.specific_config_item ==
              SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S)
             ||
             (_configPairEle[m].config_item.specific_config_item ==
              SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S))
          {
            l_free(_configPairEle[m].content.time.time);
          }
        }
      }

      i = i + delta;

      if(app_froto_n_send_with_reply(
           encodedBuf,
           encodedPacketLen,
           (void *)sensorIntf,
           retrieveTimeout_s,
           sensorIntf->send,
           readBuf,
           sizeof(readBuf),
           &readPacketLen,
           sensorIntf->sensorAddr,
           sizeof(sensorIntf->sensorAddr),
           SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
           SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE,
           false,
           0,
           usedSeqNo,
           NULL) == true)
      {
        // Just for Debug
        DBG_LOG_INFO("readBuf");
        util_dbg_buf_dump(
          readBuf,
          readPacketLen);

        // Parse and store to database
        pb_istream_t _istream;
        SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
        SKFChina_Froto_FrotoHeader *_Froto_header;

        pthread_mutex_lock(&sensor_config_hash_info_mtx);
        sensor_config_hash_info.sensorID = NULL;
        sensor_config_hash_info.sensorIDLen = 0;
        if(true == app_froto_config_hash_upload_parser(readBuf,
                                                       readPacketLen,
                                                       &_istream,
                                                       &_SKF_Froto_App,
                                                       &_Froto_header,
                                                       config_hash_upload_processor,
                                                       NULL))
        {
          if(sizeof(sensorIntf->sensorAddr) ==
             sensor_config_hash_info.sensorIDLen)
          {
            if(memcmp(sensorIntf->sensorAddr,
                      sensor_config_hash_info.sensorID,
                      sensor_config_hash_info.sensorIDLen) == 0)
            {
              sensorIntf->configHashValid = true;
              sensorIntf->configHash = sensor_config_hash_info.hash;
              if(sensor_config_hash_info.has_last_edit_time)
              {
                sensorIntf->lastEditTimeValid = true;
                sensorIntf->lastEditTime =
                  sensor_config_hash_info.last_edit_time;
              }
              // Calculate the configuration hash value based on updated
              // configurations
              if(util_froto_cal_config_hash_value(&sensorCfg, gwAddrStr) ==
                 sensorIntf->configHash)
              {
                // If the calculated hash value is identical to the
                // received one, then just update the last edit time
                DBG_LOG_INFO(
                  "The received hash value has been identical to the local one");
                l_free(sensor_config_hash_info.sensorID);
                sensor_config_hash_info.sensorIDLen = 0;
                pthread_mutex_unlock(&sensor_config_hash_info_mtx);
                break;
              }
              else
              {
                // Otherwise, continue disseminating the following
                // configurations
                DBG_LOG_INFO(
                  "The received hash value is not identical to the local one");
              }
            }
          }
        }
        l_free(sensor_config_hash_info.sensorID);
        sensor_config_hash_info.sensorIDLen = 0;
        pthread_mutex_unlock(&sensor_config_hash_info_mtx);
      }
      else
      {
        if(_attempt < retry)
        {
          _attempt++;
          i = i - delta;
          DBG_LOG_WARN("Config dissem retry %d\n", _attempt);
          sleep(2);
          goto _CONFIG_DISSEM;
        }else{
          rt = false;
          goto _EXIT;
        }
      }
    }

    // Finally update the last edit time ...
    {
      protobuf_codec_config_pair_ele_t _configPairEle[1];
      protobuf_codec_config_pair_t _configPair;

      // Prepare the data
      _configPairEle[0].which_config_item =
        SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
      _configPairEle[0].config_item.specific_config_item =
        specificItemGroup[0];
      _configPairEle[0].has_memory_id = false;
      map_conf_item_to_content(
        _configPairEle[0].config_item.specific_config_item,
        &sensorCfg,
        &_configPairEle[0],
        false);

      _configPair.buffer = _configPairEle;
      _configPair.size = 1;
      _configPair.cnt = 1;

      app_froto_n_encode_msg_config_dissem(
        SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
        &_configPair,
        SKFChina_Common_ProductType_BULLET_NODE,
        sensorIntf->gwAddr,
        sizeof(sensorIntf->gwAddr),
        sensorIntf->sensorAddr,
        sizeof(sensorIntf->sensorAddr),
        NULL,
        encodedBuf,
        sizeof(encodedBuf),
        &encodedPacketLen,
        &usedSeqNo,
        &msgType,
        NULL);

      for(uint32_t m = 0; m < 1; m++)
      {
        if(_configPairEle[m].which_config_item ==
           SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag)
        {
          if((_configPairEle[m].config_item.specific_config_item ==
              SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S)
             ||
             (_configPairEle[m].config_item.specific_config_item ==
              SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S)
             ||
             (_configPairEle[m].config_item.specific_config_item ==
              SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S)
             ||
             (_configPairEle[m].config_item.specific_config_item ==
              SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S))
          {
            l_free(_configPairEle[m].content.time.time);
          }
        }
      }

      if(app_froto_n_simple_send(
           encodedBuf,
           encodedPacketLen,
           (void *)sensorIntf,
           sensorIntf->send,
           NULL) == false)
      {
        rt = false;
        goto _EXIT;
      }
    }

    goto _EXIT;
  }

  /******************************************************/
  /****** 2/2b Read back sensor's configuration *********/
  /* In the reset cases with different config hash and  */
  /* local stored hash.                                 */
  /******************************************************/
  _attempt = 0;
  for(uint32_t i = 2, // Skip the first two time-related configurations
      delta = INSIGHT_PREDICT_CONFIG_RETRIEVE_MAX_NUMBER;
      i < sizeof(specificItemGroup) / sizeof(specificItemGroup[0]);
     )
  {
    if((i + INSIGHT_PREDICT_CONFIG_RETRIEVE_MAX_NUMBER) >
       sizeof(specificItemGroup) / sizeof(specificItemGroup[0]))
    {
      delta = sizeof(specificItemGroup) / sizeof(specificItemGroup[0]) -
              i;
    }
    _attempt = 0;
_CONFIG_RETRIEVE:
    app_froto_n_encode_msg_config_retrieve(
      SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
      SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG,
      sensorIntf->sensorNameId,
      true,
      &specificItemGroup[i],
      NULL,
      delta,
      sensorIntf->gwAddr,
      sizeof(sensorIntf->gwAddr),
      sensorIntf->sensorAddr,
      sizeof(sensorIntf->sensorAddr),
      NULL,
      encodedBuf,
      sizeof(encodedBuf),
      &encodedPacketLen,
      &usedSeqNo,
      &msgType,
      NULL);

    i = i + delta;

    readPacketLen = 0;

    if(app_froto_n_send_with_reply(
         encodedBuf,
         encodedPacketLen,
         (void *)sensorIntf,
         retrieveTimeout_s,
         sensorIntf->send,
         readBuf,
         sizeof(readBuf),
         &readPacketLen,
         sensorIntf->sensorAddr,
         sizeof(sensorIntf->sensorAddr),
         SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
         SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE,
         false,
         0,
         usedSeqNo,
         NULL) == true)
    {
      // Just for Debug
      DBG_LOG_INFO("readBuf");
      util_dbg_buf_dump(
        readBuf,
        readPacketLen);

      // Parse and store to database
      pb_istream_t _istream;
      SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
      SKFChina_Froto_FrotoHeader *_Froto_header;

      pthread_mutex_lock(&sensor_config_info_mtx);
      sensor_config_info.sensorID = NULL;
      sensor_config_info.sensorIDLen = 0;
      if(true == app_froto_specific_config_upload_parser(readBuf,
                                                         readPacketLen,
                                                         &_istream,
                                                         &_SKF_Froto_App,
                                                         &_Froto_header,
                                                         config_upload_processor,
                                                         NULL))
      {
        if(sizeof(sensorIntf->sensorAddr) ==
           sensor_config_info.sensorIDLen)
        {
          if(memcmp(sensorIntf->sensorAddr,
                    sensor_config_info.sensorID,
                    sensor_config_info.sensorIDLen) == 0)
          {
            // Mapping to sensorCfg, the structure used by database
            for(uint32_t m = 0; m < sensor_config_info.config_pair.cnt;
                m++)
            {
              if(
                SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag
                == sensor_config_info.config_pair.buffer[m].
                which_config_item)
              {
                map_conf_item_to_content(
                  sensor_config_info.config_pair.buffer[m].config_item.
                  specific_config_item,
                  &sensorCfg,
                  &sensor_config_info.config_pair.buffer[m],
                  true);
              }
            }
          }
        }
      }
      for(uint32_t i = 0; i < sensor_config_info.config_pair.cnt; i++)
      {
        if(sensor_config_info.config_pair.buffer[i].which_config_content
           ==
           SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
        {
          l_free(
            sensor_config_info.config_pair.buffer[i].content.gateway_info
            .mac_addr);
          sensor_config_info.config_pair.buffer[i].content.gateway_info.
          mac_addr = NULL;
          sensor_config_info.config_pair.buffer[i].content.gateway_info.
          mac_addrLen = 0;
        }
        if(sensor_config_info.config_pair.buffer[i].which_config_content
           ==
           SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
        {
          l_free(
            sensor_config_info.config_pair.buffer[i].content.work_mode.
            parameters);
          sensor_config_info.config_pair.buffer[i].content.work_mode.
          parameters = NULL;
          sensor_config_info.config_pair.buffer[i].content.work_mode.
          parametersLen = 0;
        }
        if(sensor_config_info.config_pair.buffer[i].which_config_content
           ==
           SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
        {
          l_free(
            sensor_config_info.config_pair.buffer[i].content.time.time);
          sensor_config_info.config_pair.buffer[i].content.time.time =
            NULL;
          sensor_config_info.config_pair.buffer[i].content.time.size = 0;
          sensor_config_info.config_pair.buffer[i].content.time.cnt = 0;
        }
      }
      l_free(sensor_config_info.sensorID);
      sensor_config_info.sensorIDLen = 0;
      pthread_mutex_unlock(&sensor_config_info_mtx);
    }
    else
    {
      if(_attempt < retry)
      {
        _attempt++;
        i = i - delta;
        DBG_LOG_WARN("Config retrieve retry %d\n", _attempt);
        sleep(2);
        goto _CONFIG_RETRIEVE;
      }else{
        rt = false;
        goto _EXIT;
      }
    }
  }

  // Finally read back the last edit time
  {
    _attempt = 0;
_CONFIG_RETRIEVE_FINAL:
    app_froto_n_encode_msg_config_retrieve(
      SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
      SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG,
      sensorIntf->sensorNameId,
      true,
      &specificItemGroup[0],
      NULL,
      1,
      sensorIntf->gwAddr,
      sizeof(sensorIntf->gwAddr),
      sensorIntf->sensorAddr,
      sizeof(sensorIntf->sensorAddr),
      NULL,
      encodedBuf,
      sizeof(encodedBuf),
      &encodedPacketLen,
      &usedSeqNo,
      &msgType,
      NULL);

    readPacketLen = 0;

    if(app_froto_n_send_with_reply(
         encodedBuf,
         encodedPacketLen,
         (void *)sensorIntf,
         retrieveTimeout_s,
         sensorIntf->send,
         readBuf,
         sizeof(readBuf),
         &readPacketLen,
         sensorIntf->sensorAddr,
         sizeof(sensorIntf->sensorAddr),
         SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
         SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE,
         false,
         0,
         usedSeqNo,
         NULL) == true)
    {
      // Just for Debug
      DBG_LOG_INFO("readBuf");
      util_dbg_buf_dump(
        readBuf,
        readPacketLen);

      // Parse and store to database
      pb_istream_t _istream;
      SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
      SKFChina_Froto_FrotoHeader *_Froto_header;

      pthread_mutex_lock(&sensor_config_info_mtx);
      sensor_config_info.sensorID = NULL;
      sensor_config_info.sensorIDLen = 0;
      if(true == app_froto_specific_config_upload_parser(readBuf,
                                                         readPacketLen,
                                                         &_istream,
                                                         &_SKF_Froto_App,
                                                         &_Froto_header,
                                                         config_upload_processor,
                                                         NULL))
      {
        if(sizeof(sensorIntf->sensorAddr) ==
           sensor_config_info.sensorIDLen)
        {
          if(memcmp(sensorIntf->sensorAddr,
                    sensor_config_info.sensorID,
                    sensor_config_info.sensorIDLen) == 0)
          {
            // Mapping to sensorCfg, the structure used by database
            for(uint32_t m = 0; m < sensor_config_info.config_pair.cnt;
                m++)
            {
              if(
                SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag
                == sensor_config_info.config_pair.buffer[m].
                which_config_item)
              {
                map_conf_item_to_content(
                  sensor_config_info.config_pair.buffer[m].config_item.
                  specific_config_item,
                  &sensorCfg,
                  &sensor_config_info.config_pair.buffer[m],
                  true);
              }
            }
          }
        }
      }
      for(uint32_t i = 0; i < sensor_config_info.config_pair.cnt; i++)
      {
        if(sensor_config_info.config_pair.buffer[i].which_config_content
           ==
           SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
        {
          l_free(
            sensor_config_info.config_pair.buffer[i].content.gateway_info
            .mac_addr);
          sensor_config_info.config_pair.buffer[i].content.gateway_info.
          mac_addr = NULL;
          sensor_config_info.config_pair.buffer[i].content.gateway_info.
          mac_addrLen = 0;
        }
        if(sensor_config_info.config_pair.buffer[i].which_config_content
           ==
           SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
        {
          l_free(
            sensor_config_info.config_pair.buffer[i].content.work_mode.
            parameters);
          sensor_config_info.config_pair.buffer[i].content.work_mode.
          parameters = NULL;
          sensor_config_info.config_pair.buffer[i].content.work_mode.
          parametersLen = 0;
        }
        if(sensor_config_info.config_pair.buffer[i].which_config_content
           ==
           SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
        {
          l_free(
            sensor_config_info.config_pair.buffer[i].content.time.time);
          sensor_config_info.config_pair.buffer[i].content.time.time =
            NULL;
          sensor_config_info.config_pair.buffer[i].content.time.size = 0;
          sensor_config_info.config_pair.buffer[i].content.time.cnt = 0;
        }
      }
      l_free(sensor_config_info.sensorID);
      sensor_config_info.sensorIDLen = 0;
      pthread_mutex_unlock(&sensor_config_info_mtx);
    }
    else
    {
      if(_attempt < retry)
      {
        _attempt++;
        DBG_LOG_WARN("Config final retrieve retry %d\n", _attempt);
        sleep(2);
        goto _CONFIG_RETRIEVE_FINAL;
      }else{
        rt = false;
        goto _EXIT;
      }
    }
  }
  // sensorCfg has been filled, then save to database
  pthread_mutex_lock(&sensor_config_info_mtx);
  app_db_deliver_sensor_conf_to_db(pt_dbop_ctr, &sensorCfg, &fw_info);
  pthread_mutex_unlock(&sensor_config_info_mtx);
  // Update to the cache
  if(sensorIntf->userData != NULL)
  {
    ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
    waveformPeriod_s =
      sensorCfg.bulletSensorConfig.schConfig.period_wave_s;
    ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
    regularSensingPeriod_s =
      sensorCfg.bulletSensorConfig.schConfig.period_regularSensing_s;
    ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
    waveformRef_s = sensorCfg.bulletSensorConfig.schConfig.refTime_wave_s;
    ((struct InsightPInterface_userdata *)(sensorIntf->userData))->
    regularSensingRef_s =
      sensorCfg.bulletSensorConfig.schConfig.refTime_regularSensing_s;
  }

_EXIT:
  return rt;
}
/**
 * @brief To get configuration item type
 */
static bool
map_conf_item_to_content(
  SKFChina_Common_SpecificConfigItem kp_conf_item,
  sensorConfig_t *sensorCfg,
  protobuf_codec_config_pair_ele_t *ele,
  bool fromFrotoToDB)
{
  // SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag
  // SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag
  // SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag
  // SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag
  // SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag
  // SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag
  // SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag
  bool _rt = true;

  switch(kp_conf_item)
  {
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_RATE_HZ:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.fAccHz =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.fAccHz;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_AXIS:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval;
      }
      break;
    }
    case
      SKFChina_Common_SpecificConfigItem_ACC_SENSOR_RANGE_G:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.acq_vib_range =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.acq_vib_range;
      }

      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_PTS:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.nAcc =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.nAcc;
      }
      break;
    }
    case
      SKFChina_Common_SpecificConfigItem_TEMPERATURE_ALARM_THRESHOLD_CDGREE
      :
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.AlarmThrestemp =
          ele->content.content_int32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag;
        ele->content.content_int32 =
          sensorCfg->bulletSensorConfig.algoConfig.AlarmThrestemp;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BORE_DIAMETER_MM:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM;
      }
      break;
    }
    case
      SKFChina_Common_SpecificConfigItem_BATTERY_LOW_ALARM_THRESHOLD_PERCENTAGE
      :
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.sysConfig.batteryAlarmThrePercent =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.sysConfig.batteryAlarmThrePercent;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_WORK_MODE:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.sysConfig.sensorMode =
          ele->content.work_mode.work_mode;
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
        if(ele->content.work_mode.parametersLen == 0)
        {
          sensorCfg->bulletSensorConfig.sysConfig.sensorModeParameter = 0;
        }else{
          if(ele->content.work_mode.parameters == NULL)
          {
            sensorCfg->bulletSensorConfig.sysConfig.sensorModeParameter = 0;
          }else{
            sensorCfg->bulletSensorConfig.sysConfig.sensorModeParameter = 
              ele->content.work_mode.parameters[0];
          }
        }
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag;
        ele->content.work_mode.work_mode =
          sensorCfg->bulletSensorConfig.sysConfig.sensorMode;
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
        ele->content.work_mode.parametersLen = 1;
        ele->content.work_mode.parameters = 
          &(sensorCfg->bulletSensorConfig.sysConfig.sensorModeParameter);
#else /* STATUS_SENSING_FEATURE_ENABLE == 1 */
        ele->content.work_mode.parametersLen = 0;
        ele->content.work_mode.parameters = NULL;
#endif /* STATUS_SENSING_FEATURE_ENABLE != 1 */
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_ADV_DBM:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.sysConfig.txPowerAdv =
          ele->content.content_int32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag;
        ele->content.content_int32 =
          sensorCfg->bulletSensorConfig.sysConfig.txPowerAdv;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_AUX_ADV_DBM:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.sysConfig.txPowerAuxAdv =
          ele->content.content_int32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag;
        ele->content.content_int32 =
          sensorCfg->bulletSensorConfig.sysConfig.txPowerAuxAdv;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_AUX_ADV_KBPS:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.sysConfig.dataRateAuxAdv =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.sysConfig.dataRateAuxAdv;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_CURRENT_TIME:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.sysConfig.currentTimeS =
          ele->content.time.time[0];
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
        ele->content.time.cnt = 1;
        ele->content.time.size = 1;
        ele->content.time.time =
          &(sensorCfg->bulletSensorConfig.sysConfig.currentTimeS);
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->lastEditTimeS = ele->content.time.time[0];
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
        ele->content.time.cnt = 1;
        ele->content.time.size = 1;
        ele->content.time.time = &(sensorCfg->lastEditTimeS);
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_QUICKPOLLING_PERIOD_S:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.schConfig.period_quickPolling_s =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.schConfig.period_quickPolling_s;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_REGULARSENSING_PERIOD_S:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.schConfig.period_regularSensing_s =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.schConfig.period_regularSensing_s;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_COMMUNICATION_PERIOD_S:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.schConfig.period_comm_s =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.schConfig.period_comm_s;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S:
    {
      // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY
      // variable,
      // but locally use a uint32_t to store
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.schConfig.refTime_quickPolling_s =
          ele->content.time.time[0];
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
        ele->content.time.cnt = 1;
        ele->content.time.size = 1;
        ele->content.time.time = l_malloc(sizeof(uint64_t));
        *ele->content.time.time =
          sensorCfg->bulletSensorConfig.schConfig.refTime_quickPolling_s;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S:
    {
      // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY
      // variable,
      // but locally use a uint32_t to store
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.schConfig.refTime_regularSensing_s =
          ele->content.time.time[0];
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
        ele->content.time.cnt = 1;
        ele->content.time.size = 1;
        ele->content.time.time = l_malloc(sizeof(uint64_t));
        *ele->content.time.time =
          sensorCfg->bulletSensorConfig.schConfig.refTime_regularSensing_s;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S:
    {
      // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY
      // variable,
      // but locally use a uint32_t to store
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.schConfig.refTime_period_comm_s =
          ele->content.time.time[0];
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
        ele->content.time.cnt = 1;
        ele->content.time.size = 1;
        ele->content.time.time = l_malloc(sizeof(uint64_t));
        *ele->content.time.time =
          sensorCfg->bulletSensorConfig.schConfig.refTime_period_comm_s;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_PERIOD_S:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.schConfig.period_wave_s =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.schConfig.period_wave_s;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S:
    {
      // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY
      // variable,
      // but locally use a uint32_t to store
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.schConfig.refTime_wave_s =
          ele->content.time.time[0];
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
        ele->content.time.cnt = 1;
        ele->content.time.size = 1;
        ele->content.time.time = l_malloc(sizeof(uint64_t));
        *ele->content.time.time =
          sensorCfg->bulletSensorConfig.schConfig.refTime_wave_s;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_FS_HZ:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_N:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.pre_acq_vib_n =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.pre_acq_vib_n;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_AXIS_ACQ_EVAL:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.pre_acq_vib_axis_acq_eval =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.pre_acq_vib_axis_acq_eval;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_RANGE:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.pre_acq_vib_range =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.pre_acq_vib_range;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_FS_HZ:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_N:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.pre_acq_mag_n =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.pre_acq_mag_n;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_AXIS_ACQ_EVAL:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.pre_acq_mag_axis_acq_eval =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.pre_acq_mag_axis_acq_eval;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_FS_HZ:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.acq_mag_fs_hz =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.acq_mag_fs_hz;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_N:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.acq_mag_n =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.acq_mag_n;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_AXIS_ACQ_EVAL:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FS_COEF:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.senConfig.fs_coef =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.senConfig.fs_coef;
      }
      break;
    }
    // the following are for algorithm
    case SKFChina_Common_SpecificConfigItem_GEE_COEF:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.GEE_COEF =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.GEE_COEF;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_V_COEF:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.V_COEF =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.V_COEF;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_POSITION:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.MEAS_POSITION =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.MEAS_POSITION;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_LOAD:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.MEAS_LOAD =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.MEAS_LOAD;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_AXIS_VIB:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_AXIS_MAG:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VIB_START_FG:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.VIB_START_FG =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.VIB_START_FG;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VIB_RMS_START_TL:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.VIB_RMS_START_TL =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.VIB_RMS_START_TL;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_START_FG:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.MAG_START_FG =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.MAG_START_FG;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_RMS_START_TL:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.MAG_RMS_START_TL =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.MAG_RMS_START_TL;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_STABLE_FG:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.MAG_STABLE_FG =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.MAG_STABLE_FG;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_RMS_VAR_TH:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_RPM_VAR_RANGE:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.RPM_VAR_RANGE =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.RPM_VAR_RANGE;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_M:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_N:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_TEMP:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_TEMP =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_TEMP;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_M:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_N:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_VIB:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_MAG:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_TEMP:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_OV:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_OV =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_OV;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MECH:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_BRG:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_LUB:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MTR:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_GEAR:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_FAN:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_PUMP:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP =
          ele->content.content_bool;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        ele->content.content_bool =
          sensorCfg->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ASSET_LEVEL:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.ASSET_LEVEL =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.ASSET_LEVEL;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FLEX_TYPE:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FLEX_TYPE =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.FLEX_TYPE;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_RUN_SPEED_RPM:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.RUN_SPEED_RPM =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.RUN_SPEED_RPM;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFO:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.BRG_INFO_BPFO =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.BRG_INFO_BPFO;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFI:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.BRG_INFO_BPFI =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.BRG_INFO_BPFI;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BSF:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.BRG_INFO_BSF =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.BRG_INFO_BSF;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_FTF:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.BRG_INFO_FTF =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.BRG_INFO_FTF;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MTR_INFO_FL:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.MTR_INFO_FL =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.MTR_INFO_FL;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MTR_INFO_BAR:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.MTR_INFO_BAR =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.MTR_INFO_BAR;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_GEAR_INFO_TOOTH:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FAN_INFO_BLADE:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.FAN_INFO_BLADE =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.FAN_INFO_BLADE;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PUMP_INFO_VANE:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.PUMP_INFO_VANE =
          ele->content.content_uint32;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        ele->content.content_uint32 =
          sensorCfg->bulletSensorConfig.algoConfig.PUMP_INFO_VANE;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TEMP_OV_ALERT_CDEGREE:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_OV_ALERT:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.ACC_OV_ALERT =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.ACC_OV_ALERT;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_OV_ALARM:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.ACC_OV_ALARM =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.ACC_OV_ALARM;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_HAL_ALERT:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.ACC_HAL_ALERT =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.ACC_HAL_ALERT;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_HAL_ALARM:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.ACC_HAL_ALARM =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.ACC_HAL_ALARM;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_OV_ALERT:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.VEL_OV_ALERT =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.VEL_OV_ALERT;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_OV_ALARM:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.VEL_OV_ALARM =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.VEL_OV_ALARM;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_HAL_ALERT:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.VEL_HAL_ALERT =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.VEL_HAL_ALERT;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_HAL_ALARM:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.VEL_HAL_ALARM =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.VEL_HAL_ALARM;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_OV_ALERT:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.ENV_OV_ALERT =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.ENV_OV_ALERT;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_OV_ALARM:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.ENV_OV_ALARM =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.ENV_OV_ALARM;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_HAL_ALERT:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.ENV_HAL_ALERT =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.ENV_HAL_ALERT;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_HAL_ALARM:
    {
      if(fromFrotoToDB)
      {
        sensorCfg->bulletSensorConfig.algoConfig.ENV_HAL_ALARM =
          ele->content.content_float;
      }
      else
      {
        ele->which_config_content =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
        ele->content.content_float =
          sensorCfg->bulletSensorConfig.algoConfig.ENV_HAL_ALARM;
      }
      break;
    }
    default:
    {
      DBG_LOG_ERR(
        "Unknown configuration item, uint32_t would be adopted!\r\n");
      _rt = false;
      break;
    }
  }
  return _rt;
}
/**
 * @brief This is the FUOTA instance of InsightPredict. In this routine, 1)
 * disseminate a notification of the FUOTA and 2) disseminiate the fuota
 * image once the version of sensor is different from the image version.
 * @param sensorIntf The pointer to the interface (instance) to be
 * unregistered.
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return False only when communication error happened.
 */
static bool
fuota(
  struct sensor_interface *sensorIntf,
  void *user_data,
  uint8_t *err_code)
{
  bool rt = true;
  uint8_t encodedBuf[256];
  uint32_t encodedPacketLen;
  uint8_t readBuf[200];
  uint32_t readPacketLen;
  uint32_t usedSeqNo;
  SKFChina_Froto_FrotoMsgType msgType;

  uint32_t fuotaTaskId = time(NULL) % 10000;

  DBG_LOG_INFO("FUOTA is called, %d",
                sensorIntf->sensorNameId);
  if((sensorIntf->hwTypeValid == false) ||
     (sensorIntf->hwVerValid == false) ||
     (sensorIntf->fwVerValid == false) ||
     (sensorIntf->imageFwValid == false))
  {
    // The hardware type, hardware version, firmware version, and image
    // must be valid
    // rt = false;
    DBG_LOG_ERR("FUOTA error: %d %d %d %d",
                  sensorIntf->hwTypeValid,
                  sensorIntf->hwVerValid,
                  sensorIntf->fwVerValid,
                  sensorIntf->imageFwValid);
    goto _EXIT;
  }

  // Check whether FUOTA is required
  if((sensorIntf->fwVer == sensorIntf->imageFwVer) ||
     ((sensorIntf->fwVer != sensorIntf->imageFwVer) &&
      (strlen(sensorIntf->imageFwVerPath) == 0)))
  {
    // rt = false;
    DBG_LOG_ERR("FUOTA error: %d %d %s",
                sensorIntf->fwVer,
                sensorIntf->imageFwVer,
                sensorIntf->imageFwVerPath);
    goto _EXIT;
  }

  // TODO: Here force FUOTA is enabled
  bool forceFUOTA = true;
  uint32_t hash = 0;
  uint32_t offset = 0;
  uint32_t longPktId = 0;
  uint32_t currentBlockCnt = 0;
  uint32_t totalBlock = 0;
  uint8_t blockContent[FILE_PKT_SIZE];
  uint32_t blockContentLen = 0;
  uint32_t retrieveTimeout_s = 9; // original is 5

  int imageFwfd = -1;

  if(sensorIntf->fwVerValid == true)
  {
    /******************************************************/
    /*********** 1/2 Notify FUOTA dissemination ***********/
    /******************************************************/
    hash = 0; // The image has included the hash value

    // To notify there would be a image to be disseminated
    app_froto_n_encode_msg_fuota_notify_dissem(
      SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE,
      fuotaTaskId,
      sensorIntf->hwType,
      sensorIntf->hwVer,
      sensorIntf->fwVer,
      forceFUOTA,
      SKFChina_Common_CommunicationType_BLECONNECTION_COMM,
      SKFChina_Common_FUOTAType_WHOLE,
      SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_froto_over_ble_ots_tag,
      NULL,
      0,
      NULL,
      0,
      true,
      SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_elf_hash_value_tag,
      &hash,
      sizeof(hash),
      SKFChina_Common_Encryption_NO_ENCRYPTION,
      sensorIntf->gwAddr,
      sizeof(sensorIntf->gwAddr),
      sensorIntf->sensorAddr,
      sizeof(sensorIntf->sensorAddr),
      NULL,
      encodedBuf,
      sizeof(encodedBuf),
      &encodedPacketLen,
      &usedSeqNo,
      &msgType,
      NULL);

    if(app_froto_n_simple_send(
         encodedBuf,
         encodedPacketLen,
         (void *)sensorIntf,
         sensorIntf->send,
         NULL) == false)
    {
      rt = false;
      goto _EXIT;
    }
    usleep(1000000);

    /******************************************************/
    /************* 2/2 Disseminate the image **************/
    /******************************************************/
    // Open the image
    imageFwfd = open(sensorIntf->imageFwVerPath, O_RDONLY);
    if(imageFwfd < 0)
    {
      DBG_LOG_ERR("Failed to open file %s, for %s",
                  sensorIntf->imageFwVerPath,
                  strerror(errno));
      // rt = false;
      goto _EXIT;
    }

    off_t fsize = lseek(imageFwfd, 0, SEEK_END);

    if(fsize < 0)
    {
      DBG_LOG_ERR("Failed to lseek, for %s", strerror(errno));
      // rt = false;
      goto _EXIT;
    }
    lseek(imageFwfd, 0, SEEK_SET);  // Reset the file pointer

    totalBlock = fsize / FILE_PKT_SIZE;
    if(fsize % FILE_PKT_SIZE)
    {
      totalBlock++;
    }

    // If totalBlock is 1
    if(totalBlock == 1)
    {
      close(imageFwfd);
    }

    // Disseminate the image
    for(currentBlockCnt = 0, offset = 0; currentBlockCnt < totalBlock;
        currentBlockCnt++)
    {
      blockContentLen = read(imageFwfd, blockContent, FILE_PKT_SIZE);
      hash = util_froto_bullet_elfhash(blockContent, blockContentLen);

      app_froto_n_encode_msg_image_block_dissem(
        SKFChina_Froto_FrotoPmtType_SIMPLE_BULK_DISSEMINATE,
        offset,
        true,
        fuotaTaskId,
        blockContent,
        blockContentLen,
        hash,
        totalBlock,
        currentBlockCnt,
        longPktId,
        sensorIntf->gwAddr,
        sizeof(sensorIntf->gwAddr),
        sensorIntf->sensorAddr,
        sizeof(sensorIntf->sensorAddr),
        NULL,
        encodedBuf,
        sizeof(encodedBuf),
        &encodedPacketLen,
        &usedSeqNo,
        &msgType,
        NULL);

      offset += blockContentLen;
      if(currentBlockCnt == 0)
      {
        longPktId = usedSeqNo;
      }

      if(currentBlockCnt + 1 >= totalBlock)
      {
        close(imageFwfd);
        if(app_froto_n_send_with_reply(
             encodedBuf,
             encodedPacketLen,
             (void *)sensorIntf,
             retrieveTimeout_s,
             sensorIntf->send,
             readBuf,
             sizeof(readBuf),
             &readPacketLen,
             sensorIntf->sensorAddr,
             sizeof(sensorIntf->sensorAddr),
             SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
             SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE,
             false,
             0,
             usedSeqNo,
             NULL) == true)
        {
          // Just for Debug
          DBG_LOG_INFO("readBuf");
          util_dbg_buf_dump(
            readBuf,
            readPacketLen);

          // Parse and store to database
          pb_istream_t _istream;
          SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
          SKFChina_Froto_FrotoHeader *_Froto_header;

          if(true == app_froto_image_block_request_parser(readBuf,
                                                          readPacketLen,
                                                          &_istream,
                                                          &_SKF_Froto_App,
                                                          &_Froto_header,
                                                          image_block_request_processor,
                                                          NULL))
          {
            DBG_LOG_INFO(
              "The last packet of FUOTA has been sent with a reply");
          }
        }
        else
        {
          rt = false;
          goto _EXIT;
        }
      }
      else
      {
        if(app_froto_n_simple_send(
             encodedBuf,
             encodedPacketLen,
             (void *)sensorIntf,
             sensorIntf->send,
             NULL) == false)
        {
          rt = false;
          goto _EXIT;
        }
      }
    }
  }
_EXIT:
  return rt;
}
#if (REGISTER_EMULATED_SENDING == 1)
static bool
sendMsg(
  struct sensor_interface *si,
  void *data_to_be_sent,
  uint32_t dataLen,
  uint8_t *err_code)
{
  DBG_LOG_INFO("SEND MSG [FOR DEBUG]");
  util_dbg_buf_dump(
    (uint8_t const *)data_to_be_sent,
    dataLen);
  sentFg = true;
  return true;
}
#endif /* REGISTER_EMULATED_SENDING == 1 */
static bool
current_version_upload_processor(
  void *parsed_data,
  uint8_t *err_code)
{
  protobuf_codec_current_version_t *pt = parsed_data;

  ver_info.firmware_version = pt->firmware_version;
  ver_info.hardware_version = pt->hardware_version;
  ver_info.hardware_type = pt->hardware_type;
  ver_info.sensorIDLen = pt->sensorIDLen;
  ver_info.sensorID = l_malloc(pt->sensorIDLen);
  memcpy(ver_info.sensorID, pt->sensorID, ver_info.sensorIDLen);
}
static bool
config_hash_upload_processor(
  void *parsed_data,
  uint8_t *err_code)
{
  protobuf_codec_config_hash_t *pt = parsed_data;

  sensor_config_hash_info.has_last_edit_time = pt->has_last_edit_time;
  sensor_config_hash_info.last_edit_time = pt->last_edit_time;
  sensor_config_hash_info.hash = pt->hash;
  sensor_config_hash_info.sensorIDLen = pt->sensorIDLen;
  sensor_config_hash_info.sensorID = l_malloc(pt->sensorIDLen);
  memcpy(sensor_config_hash_info.sensorID, pt->sensorID,
         sensor_config_hash_info.sensorIDLen);
}
static bool
config_upload_processor(
  void *parsed_data,
  uint8_t *err_code)
{
  protobuf_codec_specific_config_t *pt =
    (protobuf_codec_specific_config_t *)parsed_data;

  sensor_config_info.sensorIDLen = pt->sensorIDLen;
  sensor_config_info.sensorID = l_malloc(pt->sensorIDLen);
  memcpy(sensor_config_info.sensorID, pt->sensorID,
         sensor_config_info.sensorIDLen);
  sensor_config_info.config_pair.buffer = l_malloc((pt->config_pair.cnt) *
                                                   sizeof(
                                                     protobuf_codec_config_pair_ele_t));
  sensor_config_info.config_pair.cnt = pt->config_pair.cnt;
  sensor_config_info.config_pair.size = pt->config_pair.size;
  for(uint32_t i = 0; i < pt->config_pair.cnt; i++)
  {
    sensor_config_info.config_pair.buffer[i].which_config_item =
      pt->config_pair.buffer[i].which_config_item;
    if(sensor_config_info.config_pair.buffer[i].which_config_item ==
       SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag)
    {
      sensor_config_info.config_pair.buffer[i].config_item.
      specific_config_item =
        pt->config_pair.buffer[i].config_item.specific_config_item;
    }
    else if(sensor_config_info.config_pair.buffer[i].which_config_item ==
            SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag)
    {
      sensor_config_info.config_pair.buffer[i].config_item.
      fscheduler_config_item =
        pt->config_pair.buffer[i].config_item.fscheduler_config_item;
    }
    sensor_config_info.config_pair.buffer[i].has_memory_id =
      pt->config_pair.buffer[i].has_memory_id;
    sensor_config_info.config_pair.buffer[i].memory_id =
      pt->config_pair.buffer[i].memory_id;
    sensor_config_info.config_pair.buffer[i].which_config_content =
      pt->config_pair.buffer[i].which_config_content;
    if(sensor_config_info.config_pair.buffer[i].which_config_content ==
       SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
    {
      sensor_config_info.config_pair.buffer[i].content.content_uint32 =
        pt->config_pair.buffer[i].content.content_uint32;
    }
    else if(sensor_config_info.config_pair.buffer[i].which_config_content
            ==
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag)
    {
      sensor_config_info.config_pair.buffer[i].content.content_int32 =
        pt->config_pair.buffer[i].content.content_int32;
    }
    else if(sensor_config_info.config_pair.buffer[i].which_config_content
            ==
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
    {
      sensor_config_info.config_pair.buffer[i].content.content_float =
        pt->config_pair.buffer[i].content.content_float;
    }
    else if(sensor_config_info.config_pair.buffer[i].which_config_content
            ==
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
    {
      sensor_config_info.config_pair.buffer[i].content.content_bool =
        pt->config_pair.buffer[i].content.content_bool;
    }
    else if(sensor_config_info.config_pair.buffer[i].which_config_content
            ==
            SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
    {
      sensor_config_info.config_pair.buffer[i].content.time.time =
        l_malloc(sizeof(uint64_t) *
                 (pt->config_pair.buffer[i].content.time.cnt));
      sensor_config_info.config_pair.buffer[i].content.time.cnt =
        pt->config_pair.buffer[i].content.time.cnt;
      sensor_config_info.config_pair.buffer[i].content.time.size =
        pt->config_pair.buffer[i].content.time.size;
      for(uint32_t m = 0;
          m < sensor_config_info.config_pair.buffer[i].content.time.cnt;
          m++)
      {
        sensor_config_info.config_pair.buffer[i].content.time.time[m] =
          pt->config_pair.buffer[i].content.time.time[m];
      }
    }
    else if(sensor_config_info.config_pair.buffer[i].which_config_content
            ==
            SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
    {
      sensor_config_info.config_pair.buffer[i].content.work_mode.parameters
        = l_malloc(
            pt->config_pair.buffer[i].content.work_mode.parametersLen);
      sensor_config_info.config_pair.buffer[i].content.work_mode.
      parametersLen =
        pt->config_pair.buffer[i].content.work_mode.parametersLen;
      for(uint32_t m = 0;
          m <
          sensor_config_info.config_pair.buffer[i].content.work_mode.
          parametersLen;
          m++)
      {
        sensor_config_info.config_pair.buffer[i].content.work_mode.
        parameters[m] =
          pt->config_pair.buffer[i].content.work_mode.parameters[m];
      }
      sensor_config_info.config_pair.buffer[i].content.work_mode.work_mode
        = pt->config_pair.buffer[i].content.work_mode.work_mode;
    }
    else if(sensor_config_info.config_pair.buffer[i].which_config_content
            ==
            SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag)
    {
      sensor_config_info.config_pair.buffer[i].content.gateway_info.
      mac_addr =
        l_malloc(
          pt->config_pair.buffer[i].content.gateway_info.mac_addrLen);
      sensor_config_info.config_pair.buffer[i].content.gateway_info.
      mac_addrLen =
        pt->config_pair.buffer[i].content.gateway_info.mac_addrLen;
      for(uint32_t m = 0;
          m <
          sensor_config_info.config_pair.buffer[i].content.gateway_info.
          mac_addrLen;
          m++)
      {
        sensor_config_info.config_pair.buffer[i].content.gateway_info.
        mac_addr[m] =
          pt->config_pair.buffer[i].content.gateway_info.mac_addr[m];
      }
      sensor_config_info.config_pair.buffer[i].content.gateway_info.name_id
        = pt->config_pair.buffer[i].content.gateway_info.name_id;
    }
    else
    {
    }
  }
}
static bool
simple_data_upload_processor(
  void *parsed_data,
  uint8_t *err_code)
{
  protobuf_codec_data_pair_t *pt =
    (protobuf_codec_data_pair_t *)parsed_data;
  struct l_queue *pt_data_item_queue = l_queue_new();

  for(uint32_t i = 0; i < pt->cnt; i++)
  {
    if((pt->buffer[i].has_measurement) &&
       (pt->buffer[i].data.data != NULL) &&
       (pt->buffer[i].data.crc32_valid == true))
    {
      struct db_data_item *data_item =
        l_malloc(sizeof(struct db_data_item));

      data_item->name_id = data_item_name_id;
      memset(data_item->sensor_name, 0, sizeof(data_item->sensor_name));
      strncpy(data_item->sensor_name, data_item_sensor_name,
              sizeof(data_item->sensor_name));
      memset(data_item->sensor_mac, 0, sizeof(data_item->sensor_mac));
      snprintf(data_item->sensor_mac, sizeof(data_item->sensor_mac),
               "%02X%02X%02X%02X%02X%02X",
               pt->buffer[i].measurement.sensorID[0],
               pt->buffer[i].measurement.sensorID[1],
               pt->buffer[i].measurement.sensorID[2],
               pt->buffer[i].measurement.sensorID[3],
               pt->buffer[i].measurement.sensorID[4],
               pt->buffer[i].measurement.sensorID[5]);

      memset(data_item->sensor_type, 0,
             sizeof(data_item->sensor_type));
      strncpy(data_item->sensor_type, data_item_sensor_type,
              sizeof(data_item->sensor_type));
      memset(data_item->manufacturer, 0, sizeof(data_item->manufacturer));
      strncpy(data_item->manufacturer, data_item_manufacturer,
              sizeof(data_item->manufacturer));

      memset(&(data_item->value), 0, sizeof(data_item->value));

      data_item->meas_id = pt->buffer[i].measurement.measure_seq_no;
      data_item->meas_ts = pt->buffer[i].measurement.sample_time;
      data_item->received_ts = time(NULL);
      data_item->alarm = pt->buffer[i].measurement.alarm;
      data_item->data_type = pt->buffer[i].measurement.measureType;
      if(pt->buffer[i].measurement.has_range)
      {
        data_item->range = pt->buffer[i].measurement.range;
      }
      else
      {
        data_item->range = SKFChina_Common_Range_UNKNOWN_RANGE;
      }
      if(pt->buffer[i].measurement.has_unit)
      {
        data_item->unit = pt->buffer[i].measurement.unit;
      }
      else
      {
        data_item->unit = SKFChina_Common_Unit_UNKNOWN_UNIT;
      }
      if(pt->buffer[i].measurement.has_measure_length_sample)
      {
        data_item->measure_length_sample =
          pt->buffer[i].measurement.measure_length_sample;
      }
      else
      {
        data_item->measure_length_sample = 0;
      }
      if(pt->buffer[i].measurement.has_total_data_length_sample)
      {
        data_item->total_data_length_sample =
          pt->buffer[i].measurement.total_data_length_sample;
      }
      else
      {
        data_item->total_data_length_sample = 0;
      }
      if(pt->buffer[i].measurement.has_sample_dimension)
      {
        data_item->dimension =
          pt->buffer[i].measurement.sample_dimension;
      }
      else
      {
        data_item->dimension = 0;
      }
      data_item->data_format = pt->buffer[i].measurement.data_format;
      if(pt->buffer[i].measurement.has_sample_period_s)
      {
        data_item->sample_period_s =
          pt->buffer[i].measurement.sample_period_s;
      }
      else
      {
        data_item->sample_period_s = 0;
      }
      if(pt->buffer[i].measurement.has_encryp)
      {
        data_item->encryp =
          pt->buffer[i].measurement.encryp;
      }
      else
      {
        data_item->encryp =
          SKFChina_Common_Encryption_UNKNOWN_ENCRYPTION;
      }
      data_item->crc32 = pt->buffer[i].data.crc32;
      if(pt->buffer[i].measurement.has_sample_rate_hz)
      {
        data_item->sample_rate_hz =
          pt->buffer[i].measurement.sample_rate_hz;
      }
      else
      {
        data_item->sample_rate_hz = 0;
      }
      if(pt->buffer[i].measurement.has_sensor_type)
      {
        data_item->froto_sensor_type =
          pt->buffer[i].measurement.sensorType;
      }
      else
      {
        data_item->froto_sensor_type =
          SKFChina_Common_SensorType_UNKNOWN_SENSOR;
      }
      data_item->product_type = pt->buffer[i].measurement.productType;
      if(dbFormatIsFile(data_item->data_type))
      {
        snprintf(data_item->format, sizeof(data_item->format), "%s",
                 DB_STORAGE_FORM_FILENAME);
        if(data_item->data_type ==
           SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE)
        {
          snprintf(data_item->value.fpath.fpath, MAX_FPATH,
                   "%s%d_%d_%s",
                   DB_ALARMDATA_FILENAME_PREFIX,
                   data_item->received_ts,
                   data_item->data_type,
                   data_item->sensor_mac);
          data_item->value.fpath.fd = -1;

          // Open the file
          data_item->value.fpath.fd =
            open(data_item->value.fpath.fpath,
                 O_CREAT | O_RDWR,
                 S_IRUSR | S_IWUSR);

          if(data_item->value.fpath.fd >= 0)
          {
            write(data_item->value.fpath.fd,
                  pt->buffer[i].data.data,
                  pt->buffer[i].data.dataLen >
                  DB_FROTO_ALARM_CODE_SIZE ? DB_FROTO_ALARM_CODE_SIZE :
                  pt->buffer[i].data.dataLen);
            DBG_LOG_DEBUG("The alarm code read from sensor is");
            util_dbg_buf_dump(pt->buffer[i].data.data,
                              pt->buffer[i].data.dataLen >
                              DB_FROTO_ALARM_CODE_SIZE ?
                              DB_FROTO_ALARM_CODE_SIZE :
                              pt->buffer[i].data.dataLen);
            DBG_LOG_DEBUG(
              "The alarm code has been written to file %s successfully\n",
              data_item->value.fpath.fpath);
            close(data_item->value.fpath.fd);
            l_queue_push_tail(pt_data_item_queue, (void *)data_item);
          }
          else
          {
            l_free(data_item);
            DBG_LOG_WARN("Failed to store to a file\n");
          }
        }
        else
        {
          l_free(data_item);
          DBG_LOG_WARN("Unsupported measurement type\n");
        }
      }
      else
      {
        snprintf(data_item->format, sizeof(data_item->format), "%s",
                 DB_STORAGE_FORM_DIGIT);
        switch(pt->buffer[i].measurement.data_format)
        {
          case SKFChina_Common_Format_FORMAT_UINT16:
          case SKFChina_Common_Format_FORMAT_INT16:
            data_item->value.valu16 = *(uint16_t *)pt->buffer[i].data.data;
            break;
          case SKFChina_Common_Format_FORMAT_UINT32:
          case SKFChina_Common_Format_FORMAT_INT32:
            data_item->value.valu32 = *(uint32_t *)pt->buffer[i].data.data;
            break;
          case SKFChina_Common_Format_FORMAT_UINT8:
          case SKFChina_Common_Format_FORMAT_INT8:
            data_item->value.valu8 = *(uint8_t *)pt->buffer[i].data.data;
            break;
          case SKFChina_Common_Format_FORMAT_FLOAT32:
            data_item->value.valufloat = *(float *)pt->buffer[i].data.data;
            break;
          case SKFChina_Common_Format_FORMAT_DOUBLE64:
            data_item->value.valudouble =
              *(double *)pt->buffer[i].data.data;
            break;
          case SKFChina_Common_Format_UNKNOWN_FORMAT:
          default:
            // Only taking one byte is the safest way
            DBG_LOG_WARN("Unsupported format\n");
            data_item->value.valu8 = *(uint8_t *)pt->buffer[i].data.data;
            break;
        }
        l_queue_push_tail(pt_data_item_queue, (void *)data_item);
        if(data_item->data_type ==
           SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV)
        {
          lastVibSensingTime_s = data_item->meas_ts;
          lastVibSensingMeas_id = data_item->meas_id;
        }
      }
    }
    else
    {
      // Do nothing since there is no measurement information
      DBG_LOG_WARN("Invalid data\n");
    }
  }

  {
    struct db_data_item *data_item = NULL;
    struct database_operation_controller *pt_db_op_ctr = NULL;
    struct db_op_msg *pt_op_msg = NULL;

    DBG_LOG_INFO("Trigger the thread to update sensor data to database\n");
    pt_db_op_ctr = app_db_get_database_op_ctr();
    pthread_mutex_lock(&pt_db_op_ctr->mtx);
    while(1)
    {
      data_item =
        (struct db_data_item *)l_queue_pop_head(pt_data_item_queue);
      if(NULL == data_item)
      {
        break;
      }
      pt_op_msg = (struct db_op_msg *)l_malloc(sizeof(struct db_op_msg));
      if(pt_op_msg)
      {
        memset(pt_op_msg, 0, sizeof(struct db_op_msg));
        pt_op_msg->cmd = gw_pro_command_sqlite_update_item;
        pt_op_msg->table_idx = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
        pt_op_msg->cmd_para.pt_dtitem = data_item;
        l_queue_push_tail(pt_db_op_ctr->pt_dtitem_queue,
                          (void *)pt_op_msg);
      }
      else
      {
        DBG_LOG_ERR("Failed to malloc");
        l_free(data_item);
      }
    }
    DBG_LOG_DEBUG("To signal data available");
    pthread_cond_signal(&pt_db_op_ctr->cond_dt_available);
    pthread_mutex_unlock(&pt_db_op_ctr->mtx);

    l_queue_destroy(pt_data_item_queue, l_free);
  }
}
static bool
bulk_data_upload_first_pkt_processor(
  void *parsed_data,
  uint8_t *err_code)
{
  protobuf_codec_data_pair_t *pt = parsed_data;

  for(uint32_t i = 0; i < pt->cnt; i++)
  {
    if((pt->buffer[i].has_measurement) &&
       (pt->buffer[i].data.data != NULL) &&
       (pt->buffer[i].data.crc32_valid == true) &&
       (pt->buffer[i].data.start_point == 0))
    {
      struct db_data_item *data_item =
        l_malloc(sizeof(struct db_data_item));

      data_item->name_id = data_item_name_id;
      memset(data_item->sensor_name, 0, sizeof(data_item->sensor_name));
      strncpy(data_item->sensor_name, data_item_sensor_name,
              sizeof(data_item->sensor_name));
      memset(data_item->sensor_mac, 0, sizeof(data_item->sensor_mac));
      snprintf(data_item->sensor_mac, sizeof(data_item->sensor_mac),
               "%02X%02X%02X%02X%02X%02X",
               pt->buffer[i].measurement.sensorID[0],
               pt->buffer[i].measurement.sensorID[1],
               pt->buffer[i].measurement.sensorID[2],
               pt->buffer[i].measurement.sensorID[3],
               pt->buffer[i].measurement.sensorID[4],
               pt->buffer[i].measurement.sensorID[5]);

      memset(data_item->sensor_type, 0,
             sizeof(data_item->sensor_type));
      strncpy(data_item->sensor_type, data_item_sensor_type,
              sizeof(data_item->sensor_type));
      memset(data_item->manufacturer, 0, sizeof(data_item->manufacturer));
      strncpy(data_item->manufacturer, data_item_manufacturer,
              sizeof(data_item->manufacturer));

      data_item->meas_id = pt->buffer[i].measurement.measure_seq_no;
      data_item->meas_ts = pt->buffer[i].measurement.sample_time;
      data_item->received_ts = time(NULL);
      data_item->alarm = pt->buffer[i].measurement.alarm;
      data_item->data_type = pt->buffer[i].measurement.measureType;
      if(pt->buffer[i].measurement.has_range)
      {
        data_item->range = pt->buffer[i].measurement.range;
      }
      else
      {
        data_item->range = SKFChina_Common_Range_UNKNOWN_RANGE;
      }
      if(pt->buffer[i].measurement.has_unit)
      {
        data_item->unit = pt->buffer[i].measurement.unit;
      }
      else
      {
        data_item->unit = SKFChina_Common_Unit_UNKNOWN_UNIT;
      }
      if(pt->buffer[i].measurement.has_measure_length_sample)
      {
        data_item->measure_length_sample =
          pt->buffer[i].measurement.measure_length_sample;
      }
      else
      {
        data_item->measure_length_sample = 0;
      }
      if(pt->buffer[i].measurement.has_total_data_length_sample)
      {
        data_item->total_data_length_sample =
          pt->buffer[i].measurement.total_data_length_sample;
      }
      else
      {
        data_item->total_data_length_sample = 0;
      }
      if(pt->buffer[i].measurement.has_sample_dimension)
      {
        data_item->dimension =
          pt->buffer[i].measurement.sample_dimension;
      }
      else
      {
        data_item->dimension = 0;
      }
      data_item->data_format = pt->buffer[i].measurement.data_format;
      if(pt->buffer[i].measurement.has_sample_period_s)
      {
        data_item->sample_period_s =
          pt->buffer[i].measurement.sample_period_s;
      }
      else
      {
        data_item->sample_period_s = 0;
      }
      if(pt->buffer[i].measurement.has_encryp)
      {
        data_item->encryp =
          pt->buffer[i].measurement.encryp;
      }
      else
      {
        data_item->encryp =
          SKFChina_Common_Encryption_UNKNOWN_ENCRYPTION;
      }
      data_item->crc32 = pt->buffer[i].data.crc32;
      if(pt->buffer[i].measurement.has_sample_rate_hz)
      {
        data_item->sample_rate_hz =
          pt->buffer[i].measurement.sample_rate_hz;
      }
      else
      {
        data_item->sample_rate_hz = 0;
      }
      if(pt->buffer[i].measurement.has_sensor_type)
      {
        data_item->froto_sensor_type =
          pt->buffer[i].measurement.sensorType;
      }
      else
      {
        data_item->froto_sensor_type =
          SKFChina_Common_SensorType_UNKNOWN_SENSOR;
      }
      data_item->product_type = pt->buffer[i].measurement.productType;
      if(dbFormatIsFile(data_item->data_type))
      {
        snprintf(data_item->format, sizeof(data_item->format), "%s",
                 DB_STORAGE_FORM_FILENAME);
        if((data_item->data_type ==
            SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE) ||
           (data_item->data_type ==
            SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE) ||
           (data_item->data_type ==
            SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE) ||
           (data_item->data_type ==
            SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE))
        {
          snprintf(data_item->value.fpath.fpath, MAX_FPATH,
                   "%s%d_%d_%s",
                   DB_WAVEDATA_FILENAME_PREFIX,
                   data_item->received_ts,
                   data_item->data_type,
                   data_item->sensor_mac);
          data_item->value.fpath.fd = -1;

          // Open the file
          data_item->value.fpath.fd = open(data_item->value.fpath.fpath,
                                           O_CREAT | O_RDWR,
                                           S_IRUSR | S_IWUSR);
          if(data_item->value.fpath.fd >= 0)
          {
            write(data_item->value.fpath.fd,
                  pt->buffer[i].data.data,
                  pt->buffer[i].data.dataLen);
            DBG_LOG_INFO(
              "Waveform has been written to file %s successfully\n",
              data_item->value.fpath.fpath);
            data_item_info = data_item;
          }
          else
          {
            l_free(data_item);
            DBG_LOG_WARN("Failed to store to a file\n");
          }
        }
      }
      else
      {
        l_free(data_item);
        DBG_LOG_WARN("Unsupported measurement type\n");
      }
    }
    else
    {
      DBG_LOG_WARN("Invalid data\n");
    }
  }
}
static bool
bulk_data_upload_remained_pkt_processor(
  void *parsed_data,
  uint8_t *err_code)
{
  protobuf_codec_data_pair_t *pt = parsed_data;

  if(pt->cnt == 1)
  {
    if((pt->buffer[0].has_measurement == false) &&
       (pt->buffer[0].data.data != NULL) &&
       (pt->buffer[0].data.crc32_valid == true))
    {
      bulkData = l_malloc(sizeof(protobuf_codec_data_pair_ele_data_t));

      bulkData->crc32 = pt->buffer[0].data.crc32;
      bulkData->crc32_valid = pt->buffer[0].data.crc32_valid;
      bulkData->dataLen = pt->buffer[0].data.dataLen;
      bulkData->data = l_malloc(bulkData->dataLen);
      memcpy(bulkData->data, pt->buffer[0].data.data, bulkData->dataLen);
      bulkData->start_point = pt->buffer[0].data.start_point;
    }
  }
}
static bool
dbFormatIsFile(const SKFChina_Common_MeasurementType measType)
{
  bool _ret = false;

  switch(measType)
  {
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE:
    case SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE:
    case SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE:
    case SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE:
    case SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE:
    {
      _ret = true;
      break;
    }
    default:
    {
      _ret = false;
    }
  }
  return _ret;
}
static bool
image_block_request_processor(
  void *parsed_data,
  uint8_t *err_code)
{
  protobuf_codec_image_block_request_status_t *pt =
    (protobuf_codec_image_block_request_status_t *)parsed_data;

  if(pt->is_fuota_task == true)
  {
    if(pt->size == 0)
    {
      DBG_LOG_INFO(
        "Sensor (%02x:%02x:%02x:%02x:%02x:%02x) has been completed FUOTA (%d)",
        pt->sensorID[0], pt->sensorID[1], pt->sensorID[2], pt->sensorID[3],
        pt->sensorID[4], pt->sensorID[5], pt->task_id);
    }
  }
}
#endif /* SKF_GW_NEW == 1 */