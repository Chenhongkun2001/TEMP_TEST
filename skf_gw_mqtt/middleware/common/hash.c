#include "global.h"
#include "ppGW.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
DBG_LOCAL_LOG_DEBUG

#define MAX_HASH_BUFFER (512U)

// Data struct definition (borrowed from skf_gw_1)
typedef struct {
  union {
    uint8_t addrArray[6];
  } addr;
} macAddr_t;

typedef struct {
  uint8_t sensorMode;              // The work mode of sensor:
                                   // BULLET_SENSOR_MODE_FLIGHT,
                                   // BULLET_SENSOR_MODE_STANDBY,
                                   // BULLET_SENSOR_MODE_NORMAL,
                                   // or BULLET_SENSOR_MODE_FCT.
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
  uint8_t sensorModeParameter;
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
  float batteryAlarmThrePercent; // A low battery alarm threshold (in
                                   // percentage). For instance, 20 means
                                   // low battery alarm would be reported
                                   // when the battery volume is lower
                                   // than 20%.
  uint64_t currentTimeS;   // In second. A unix timestamp (GMT time), used
                           // to set time of the sensor, which can be
                           // regarded as the last communication time.
  int8_t txPowerAdv;       // Transmit power (dBm) of BLE advertising,
                           // SYSCONFIG_BLETXPOWER_0_DBM,
                           // SYSCONFIG_BLETXPOWER_NEG_4_DBM, or
                           // SYSCONFIG_BLETXPOWER_NEG_8_DBM
  uint16_t dataRateAuxAdv; // Data rate of BLE advertising (as a
                           // non-standard auxiliary advertising to
                           // improve
                           // communication range):
                           // SYSCONFIG_BLEDATARATE_1MBPS,
                           // SYSCONFIG_BLEDATARATE_2MBPS,
                           // SYSCONFIG_BLEDATARATE_500KBPS, or
                           // SYSCONFIG_BLEDATARATE_125KBPS
  int8_t txPowerAuxAdv;    // Transmit power of BLE advertising (as a
                           // non-standard auxiliary advertising):
                           // SYSCONFIG_BLETXPOWER_0_DBM,
                           // SYSCONFIG_BLETXPOWER_NEG_4_DBM, or
                           // SYSCONFIG_BLETXPOWER_NEG_8_DBM
} sensorConfig_bulletSensorConfig_sysConfig_t;

typedef struct {
  uint32_t period_quickPolling_s;    // Quick polling period in second. Zero
                                     // represents no periodic quick polling.
  uint32_t refTime_quickPolling_s;   // A unix timestamp (GMT time), used as
                                     // the start time of periodic quick
                                     // polling.
  uint32_t period_regularSensing_s;  // Regular sensing period in second.
                                     // Zero represents periodic regular
                                     // sensing.
  uint32_t refTime_regularSensing_s; // A unix timestamp (GMT time), used
                                     // as the start time of periodic
                                     // regular sensing.
  uint32_t period_comm_s;         // BLE communication period in second. Zero is
                                  // not allowed.
  uint32_t refTime_period_comm_s; // A unix timestamp (GMT time), used as
                                  // the start time of periodic BLE
                                  // communication.
  uint32_t period_wave_s;         // Waveform aquisition period in second. Zero
                                  // represents no need to upload waveform.
  uint32_t refTime_wave_s;        // A unix timestamp (GMT time), used as the
                                  // start time of waveform uploading.
} sensorConfig_bulletSensorConfig_schCfg_t;

typedef struct {
  uint32_t pre_acq_vib_fs_hz; // The vibration sensing frequency in Hz for
                              // the pre-acquisition.
  uint32_t pre_acq_vib_n;     // The number of vibration sensing for the
                              // pre-acquisition.
  uint8_t pre_acq_vib_axis_acq_eval; // A mask to indicate which axis
                                     // (axes) is/are sampled during
                                     // vibration sensing of the
                                     // pre-acquisition: a mask of
                                     // SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X
                                     // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y
                                     // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Z
  uint8_t pre_acq_vib_range;  // The range of the vibration sensor (in gee)
                              // of the pre-acquisition:
                              // SENSINGCONFIG_VIBRANGE_2GEE,
                              // SENSINGCONFIG_VIBRANGE_4GEE,
                              // SENSINGCONFIG_VIBRANGE_8GEE, or
                              // SENSINGCONFIG_VIBRANGE_16GEE.
  uint32_t pre_acq_mag_fs_hz; // The magnetic field sensing frequency in
                              // Hz for the pre-acquisition.
  uint32_t pre_acq_mag_n;     // The number of magnetic field sensing for the
                              // pre-acquisition.
  uint8_t pre_acq_mag_axis_acq_eval; // A mask to indicate which axis
                                     // (axes) is/are sampled during
                                     // magnetic field sensing of the
                                     // pre-acquisition: a mask of
                                     // SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X
                                     // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y
                                     // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Z
  uint32_t fAccHz; // The vibration sensing frequency in Hz for the
                   // acquisition
  uint32_t nAcc;   // The number of vibration sensing frequency for the
                   // acquisition.
  uint8_t acq_vib_axis_acq_eval; // A mask to indicate which axis
                                 // (axes) is/are sampled during
                                 // vibration sensing of the
                                 // acquisition: a mask of
                                 // SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X
                                 // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y
                                 // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Z
  uint8_t acq_vib_range;  // The range of the vibration sensor (in gee) of
                          // the acquisition:
                          // SENSINGCONFIG_VIBRANGE_2GEE,
                          // SENSINGCONFIG_VIBRANGE_4GEE,
                          // SENSINGCONFIG_VIBRANGE_8GEE, or
                          // SENSINGCONFIG_VIBRANGE_16GEE.
  uint32_t acq_mag_fs_hz; // The magnetic field sensing frequency in
                          // Hz for the acquisition.
  uint32_t acq_mag_n;     // The number of magnetic field sensing for the
                          // acquisition.
  uint8_t acq_mag_axis_acq_eval; // A mask to indicate which axis
                                 // (axes) is/are sampled during
                                 // magnetic field sensing of the
                                 // acquisition: a mask of
                                 // SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X
                                 // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y
                                 // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Z
  float fs_coef;                 // A fixed value for bullet sensor. (3.2)
} sensorConfig_bulletSensorConfig_senCfg_t;

typedef struct {
  float GEE_COEF;
  float V_COEF;
  uint8_t MEAS_POSITION;
  uint8_t MEAS_LOAD;
  uint8_t MEAS_AXIS_VIB;
  uint8_t MEAS_AXIS_MAG;
  bool VIB_START_FG;
  float VIB_RMS_START_TL;
  bool MAG_START_FG;
  float MAG_RMS_START_TL;
  bool MAG_STABLE_FG;
  float MAG_RMS_VAR_TH;
  float RPM_VAR_RANGE;
  uint16_t DEC_LOGIC_TEMP_M;
  uint16_t DEC_LOGIC_TEMP_N;
  uint16_t DEC_LOGIC_LEARN_NUM_TEMP;
  uint16_t DEC_LOGIC_VIB_M;
  uint16_t DEC_LOGIC_VIB_N;
  uint16_t DEC_LOGIC_LEARN_NUM_VIB;
  uint16_t DEC_LOGIC_LEARN_NUM_MAG;
  bool FUNC_ANOM_TEMP;
  bool FUNC_ANOM_OV;
  bool FUNC_ANOM_MECH;
  bool FUNC_ANOM_BRG;
  bool FUNC_ANOM_LUB;
  bool FUNC_ANOM_MTR;
  bool FUNC_ANOM_GEAR;
  bool FUNC_ANOM_FAN;
  bool FUNC_ANOM_PUMP;
  uint8_t ASSET_LEVEL;
  uint8_t FLEX_TYPE;
  float BORE_DIAMETER_MM;
  float RUN_SPEED_RPM;
  float BRG_INFO_BPFO;
  float BRG_INFO_BPFI;
  float BRG_INFO_BSF;
  float BRG_INFO_FTF;
  uint32_t MTR_INFO_FL;
  uint32_t MTR_INFO_BAR;
  uint32_t GEAR_INFO_TOOTH;
  uint32_t FAN_INFO_BLADE;
  uint32_t PUMP_INFO_VANE;
  float TEMP_OV_ALERT_CDEGREE;
  int32_t AlarmThrestemp;
  float ACC_OV_ALERT;
  float ACC_OV_ALARM;
  float ACC_HAL_ALERT;
  float ACC_HAL_ALARM;
  float VEL_OV_ALERT;
  float VEL_OV_ALARM;
  float VEL_HAL_ALERT;
  float VEL_HAL_ALARM;
  float ENV_OV_ALERT;
  float ENV_OV_ALARM;
  float ENV_HAL_ALERT;
  float ENV_HAL_ALARM;
} sensorConfig_bulletSensorConfig_algoCfg_t;

typedef struct {
  sensorConfig_bulletSensorConfig_sysConfig_t sysConfig;
  sensorConfig_bulletSensorConfig_schCfg_t schConfig;
  sensorConfig_bulletSensorConfig_senCfg_t senConfig;
  sensorConfig_bulletSensorConfig_algoCfg_t algoConfig;
} sensorConfig_bulletSensorConfig_t;

typedef struct {
  uint8_t version;
  uint64_t lastEditTimeS;          // In second.
  bool haveConfiguredToSensor;     // To indicate whether current
                                   // configuration has been applied to the
                                   // sensor
  uint64_t whenConfiguredToSensor; // A unix timestamp (GMT time)
                                   // representing when the sensor has
                                   // been configured. This field is valid
                                   // when haveConfiguredToSensor is true
  char type[50];                   // The product name/type
  char manufacturer[10];           // The manufacturer of the product
  uint32_t nameId;   // A 4-byte hash ID to avoid the same name. nameId is
                     // the index.
  macAddr_t macAddr; // BLE MAC address of the sensor (separated by
                     // dashes in JSON).
  sensorConfig_bulletSensorConfig_t bulletSensorConfig;
} sensorConfig_t;

static void memcpy_e(uint8_t *pt_des, uint8_t *pt_src, const uint32_t kpsz);
static uint32_t util_froto_cal_config_hash_value(sensorConfig_t *pt_sensor_conf,
                                                 char *clientID);
/**
 * @brief: To calculate the hash value (based on the given @p inputData ).
 *         @p length represents the length (in byte) of @p inputData .
 * @return: The calculated hash value.
 */
uint32_t util_bullet_elfhash(const uint8_t *inputData, uint32_t length) {
  uint32_t hash = 0;
  uint32_t x = 0;

  if (inputData == NULL) {
    return 0;
  }

  for (uint16_t i = 0; i < length; ++inputData, ++i) {
    hash = (hash << 4) + ((*inputData) & 0xff);
    if ((x = hash & 0xF0000000L) != 0) {
      hash ^= (x >> 24);
    }
    hash &= ~x;
  }
  return hash;
}
/**
 * @brief: To achieve an endianness memory copy.
 */
static void memcpy_e(uint8_t *pt_des, uint8_t *pt_src, const uint32_t kpsz) {
  uint16_t kp_endian_chk = 0xAABB;
  uint8_t *pt_u8 = (uint8_t *)(&kp_endian_chk);

  if ((NULL == pt_des) || (NULL == pt_src)) {
    LOG_ERR(OUTPOINT, "invalid parameter");
    return;
  }
  if (0 == kpsz) {
    return;
  }

  if ((*pt_u8) == 0xBB) {
    // Little-endian: do nothing, just copy
    memcpy(pt_des, pt_src, kpsz);
  } else {
    // Big-endian: convert to the little-endian order
    for (uint32_t i = 0; i < kpsz; i++) {
      pt_des[i] = pt_src[(kpsz - 1 - i) % kpsz];
    }
  }
}
/**
 * @brief Calculate the hash value of the Bullet sensor configuration.
 * @return the hash value
 */
static uint32_t util_froto_cal_config_hash_value(sensorConfig_t *pt_sensor_conf,
                                                 char *clientID) {
  uint8_t hashBuffer[MAX_HASH_BUFFER] = {0};
  uint16_t offset = 0;
  uint32_t hash_value = 0;

  /* Sensing */
  // Memory ID: 50
  // Facc
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.fAccHz,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.fAccHz));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.fAccHz);

  // Memory ID: 51
  // Nacc
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.nAcc,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.nAcc));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.nAcc);

  /* Algorithm */
  // Memory ID: 56
  // AlarmThrestemp
  int32_t AlarmThrestemp_tmp =
      (int32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.AlarmThrestemp);
  memcpy_e(&hashBuffer[offset], &AlarmThrestemp_tmp,
           sizeof(AlarmThrestemp_tmp));
  offset += sizeof(AlarmThrestemp_tmp);

  /* System */
  // Memory ID: 59
  // batteryAlarmThreshold
  float batteryAlarmThrePercent_tmp;
  batteryAlarmThrePercent_tmp =
      pt_sensor_conf->bulletSensorConfig.sysConfig.batteryAlarmThrePercent *
      1.0;
  memcpy_e(&hashBuffer[offset], &batteryAlarmThrePercent_tmp,
           sizeof(batteryAlarmThrePercent_tmp));
  offset += sizeof(batteryAlarmThrePercent_tmp);

  // Memory ID: 61
  // sensorMode
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode,
           sizeof(pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode);
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
  // The mode parameter
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.sysConfig.sensorModeParameter,
           sizeof(pt_sensor_conf->bulletSensorConfig.sysConfig.sensorModeParameter));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.sysConfig.sensorModeParameter);
#else /* STATUS_SENSING_FEATURE_ENABLE != 1 */
  // Skip the mode-parameter
  offset += 1;
#endif /* STATUS_SENSING_FEATURE_ENABLE != 1 */


  /* Sensing */
  // Memory ID: 62
  // FS_COEF
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.fs_coef,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.fs_coef));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.fs_coef);

  // Memory ID: 63
  // PRE_ACQ_VIB_FS_HZ
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz,
      sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz));
  offset +=
      sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz);

  // Memory ID: 64
  // PRE_ACQ_VIB_N
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_n,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_n));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_n);

  // Memory ID: 65
  // PRE_ACQ_VIB_AXIS_ACQ_EVAL
  uint32_t pre_acq_vib_axis_acq_eval_tmp =
      pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_axis_acq_eval;
  memcpy_e(&hashBuffer[offset], &pre_acq_vib_axis_acq_eval_tmp,
           sizeof(pre_acq_vib_axis_acq_eval_tmp));
  offset += sizeof(pre_acq_vib_axis_acq_eval_tmp);

  // Memory ID: 66
  // PRE_ACQ_VIB_RANGE
  uint32_t pre_acq_vib_range_tmp =
      pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_range;
  memcpy_e(&hashBuffer[offset], &pre_acq_vib_range_tmp,
           sizeof(pre_acq_vib_range_tmp));
  offset += sizeof(pre_acq_vib_range_tmp);

  // Memory ID: 67
  // PRE_ACQ_VIB_RANGE
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz,
      sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz));
  offset +=
      sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz);

  // Memory ID: 68
  // PRE_ACQ_MAG_N
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_n,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_n));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_n);

  // Memory ID: 69
  // PRE_ACQ_MAG_AXIS_ACQ_EVAL
  uint32_t pre_acq_mag_axis_acq_eval_tmp =
      pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_axis_acq_eval;
  memcpy_e(&hashBuffer[offset], &pre_acq_mag_axis_acq_eval_tmp,
           sizeof(pre_acq_mag_axis_acq_eval_tmp));
  offset += sizeof(pre_acq_mag_axis_acq_eval_tmp);

  // Memory ID: 70
  // ACQ_VIB_AXIS_ACQ_EVAL
  uint32_t acq_vib_axis_acq_eval_tmp =
      pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval;
  memcpy_e(&hashBuffer[offset], &acq_vib_axis_acq_eval_tmp,
           sizeof(acq_vib_axis_acq_eval_tmp));
  offset += sizeof(acq_vib_axis_acq_eval_tmp);

  // Memory ID: 71
  // ACQ_VIB_RANGE
  uint32_t acq_vib_range_tmp =
      pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_range;
  memcpy_e(&hashBuffer[offset], &acq_vib_range_tmp, sizeof(acq_vib_range_tmp));
  offset += sizeof(acq_vib_range_tmp);

  // Memory ID: 72
  // ACQ_MAG_FS_HZ
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_fs_hz,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_fs_hz));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_fs_hz);

  // Memory ID: 73
  // ACQ_MAG_N
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_n,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_n));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_n);

  // Memory ID: 74
  // ACQ_MAG_AXIS_ACQ_EVAL
  uint32_t acq_mag_axis_acq_eval_tmp =
      pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval;
  memcpy_e(&hashBuffer[offset], &acq_mag_axis_acq_eval_tmp,
           sizeof(acq_mag_axis_acq_eval_tmp));
  offset += sizeof(acq_mag_axis_acq_eval_tmp);

  /*Algorithm*/
  // Memory ID: 75
  // GEE_COEF
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF);
  // ?????
  // Memory ID: 76
  // V_COEF
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.V_COEF,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.V_COEF));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.V_COEF);

  // Memory ID: 77
  // MEAS_POSITION
  uint32_t MEAS_POSITION_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_POSITION;
  memcpy_e(&hashBuffer[offset], &MEAS_POSITION_tmp, sizeof(MEAS_POSITION_tmp));
  offset += sizeof(MEAS_POSITION_tmp);

  // Memory ID: 78
  // MEAS_LOAD
  uint32_t MEAS_LOAD_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_LOAD;
  memcpy_e(&hashBuffer[offset], &MEAS_LOAD_tmp, sizeof(MEAS_LOAD_tmp));
  offset += sizeof(MEAS_LOAD_tmp);

  // Memory ID: 79
  // MEAS_AXIS_VIB
  uint32_t MEAS_AXIS_VIB_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB;
  memcpy_e(&hashBuffer[offset], &MEAS_AXIS_VIB_tmp, sizeof(MEAS_AXIS_VIB_tmp));
  offset += sizeof(MEAS_AXIS_VIB_tmp);

  // Memory ID: 80
  // MEAS_AXIS_VIB
  uint32_t MEAS_AXIS_MAG_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG;
  memcpy_e(&hashBuffer[offset], &MEAS_AXIS_MAG_tmp, sizeof(MEAS_AXIS_MAG_tmp));
  offset += sizeof(MEAS_AXIS_MAG_tmp);

  // Memory ID: 81
  // VIB_START_FG
  uint8_t VIB_START_FG_tmp;
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_START_FG) {
    VIB_START_FG_tmp = 1;
  } else {
    VIB_START_FG_tmp = 0;
  }
  memcpy_e(&hashBuffer[offset], &VIB_START_FG_tmp, sizeof(VIB_START_FG_tmp));
  offset += sizeof(VIB_START_FG_tmp);

  // Memory ID: 82
  // VIB_RMS_START_TL
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_RMS_START_TL,
      sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_RMS_START_TL));
  offset +=
      sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_RMS_START_TL);

  // Memory ID: 83
  // MAG_START_FG
  uint8_t MAG_START_FG_tmp;
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_START_FG) {
    MAG_START_FG_tmp = 1;
  } else {
    MAG_START_FG_tmp = 0;
  }
  memcpy_e(&hashBuffer[offset], &MAG_START_FG_tmp, sizeof(MAG_START_FG_tmp));
  offset += sizeof(MAG_START_FG_tmp);

  // Memory ID: 84
  // MAG_RMS_START_TL
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_START_TL,
      sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_START_TL));
  offset +=
      sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_START_TL);
  // 102
  // Memory ID: 85
  // MAG_STABLE_FG
  uint8_t MAG_STABLE_FG_tmp;
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_STABLE_FG) {
    MAG_STABLE_FG_tmp = 1;
  } else {
    MAG_STABLE_FG_tmp = 0;
  }
  memcpy_e(&hashBuffer[offset], &MAG_STABLE_FG_tmp, sizeof(MAG_STABLE_FG_tmp));
  offset += sizeof(MAG_STABLE_FG_tmp);

  // Memory ID: 86
  // MAG_RMS_VAR_TH
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH,
      sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH));
  offset +=
      sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH);

  // Memory ID: 87
  // RPM_VAR_RANGE
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.RPM_VAR_RANGE,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.RPM_VAR_RANGE));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.RPM_VAR_RANGE);
  // 111
  // Memory ID: 88
  // DEC_LOGIC_TEMP_M
  uint32_t DEC_LOGIC_TEMP_M_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M;
  memcpy_e(&hashBuffer[offset], &DEC_LOGIC_TEMP_M_tmp,
           sizeof(DEC_LOGIC_TEMP_M_tmp));
  offset += sizeof(DEC_LOGIC_TEMP_M_tmp);

  // Memory ID: 89
  // DEC_LOGIC_TEMP_N
  uint32_t DEC_LOGIC_TEMP_N_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N;
  memcpy_e(&hashBuffer[offset], &DEC_LOGIC_TEMP_N_tmp,
           sizeof(DEC_LOGIC_TEMP_N_tmp));
  offset += sizeof(DEC_LOGIC_TEMP_N_tmp);

  // Memory ID: 90
  // DEC_LOGIC_LEARN_NUM_TEMP
  uint32_t DEC_LOGIC_LEARN_NUM_TEMP_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_TEMP;
  memcpy_e(&hashBuffer[offset], &DEC_LOGIC_LEARN_NUM_TEMP_tmp,
           sizeof(DEC_LOGIC_LEARN_NUM_TEMP_tmp));
  offset += sizeof(DEC_LOGIC_LEARN_NUM_TEMP_tmp);

  // Memory ID: 91
  // DEC_LOGIC_VIB_M
  uint32_t DEC_LOGIC_VIB_M_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M;
  memcpy_e(&hashBuffer[offset], &DEC_LOGIC_VIB_M_tmp,
           sizeof(DEC_LOGIC_VIB_M_tmp));
  offset += sizeof(DEC_LOGIC_VIB_M_tmp);

  // Memory ID: 92
  // DEC_LOGIC_VIB_N
  uint32_t DEC_LOGIC_VIB_N_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N;
  memcpy_e(&hashBuffer[offset], &DEC_LOGIC_VIB_N_tmp,
           sizeof(DEC_LOGIC_VIB_N_tmp));
  offset += sizeof(DEC_LOGIC_VIB_N_tmp);

  // Memory ID: 93
  // DEC_LOGIC_LEARN_NUM_VIB
  uint32_t DEC_LOGIC_LEARN_NUM_VIB_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB;
  memcpy_e(&hashBuffer[offset], &DEC_LOGIC_LEARN_NUM_VIB_tmp,
           sizeof(DEC_LOGIC_LEARN_NUM_VIB_tmp));
  offset += sizeof(DEC_LOGIC_LEARN_NUM_VIB_tmp);

  // Memory ID: 94
  // DEC_LOGIC_LEARN_NUM_MAG
  uint32_t DEC_LOGIC_LEARN_NUM_MAG_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG;
  memcpy_e(&hashBuffer[offset], &DEC_LOGIC_LEARN_NUM_MAG_tmp,
           sizeof(DEC_LOGIC_LEARN_NUM_MAG_tmp));
  offset += sizeof(DEC_LOGIC_LEARN_NUM_MAG_tmp);

  // Memory ID: 95, 96, 97, 98, 99, 100, 108, 109, 110
  // FUNC_ANOM_TEMP, FUNC_ANOM_OV, FUNC_ANOM_MECH, FUNC_ANOM_BRG,
  // FUNC_ANOM_LUB, FUNC_ANOM_MTR, FUNC_ANOM_GEAR, FUNC_ANOM_FAN,
  // FUNC_ANOM_PUMP
  uint8_t generalFg;
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP) {
    generalFg = 1;
  } else {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset], &generalFg, sizeof(generalFg));
  offset += sizeof(generalFg);
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_OV) {
    generalFg = 1;
  } else {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset], &generalFg, sizeof(generalFg));
  offset += sizeof(generalFg);
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH) {
    generalFg = 1;
  } else {
    generalFg = 0;
  }
  // 142
  memcpy_e(&hashBuffer[offset], &generalFg, sizeof(generalFg));
  offset += sizeof(generalFg);
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG) {
    generalFg = 1;
  } else {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset], &generalFg, sizeof(generalFg));
  offset += sizeof(generalFg);
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB) {
    generalFg = 1;
  } else {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset], &generalFg, sizeof(generalFg));
  offset += sizeof(generalFg);
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR) {
    generalFg = 1;
  } else {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset], &generalFg, sizeof(generalFg));
  offset += sizeof(generalFg);
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR) {
    generalFg = 1;
  } else {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset], &generalFg, sizeof(generalFg));
  offset += sizeof(generalFg);
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN) {
    generalFg = 1;
  } else {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset], &generalFg, sizeof(generalFg));
  offset += sizeof(generalFg);
  if (pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP) {
    generalFg = 1;
  } else {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset], &generalFg, sizeof(generalFg));
  offset += sizeof(generalFg);

  // Memory ID: 111
  // ASSET_LEVEL
  uint32_t ASSET_LEVEL_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.ASSET_LEVEL;
  memcpy_e(&hashBuffer[offset], &ASSET_LEVEL_tmp, sizeof(ASSET_LEVEL_tmp));
  offset += sizeof(ASSET_LEVEL_tmp);

  // Memory ID: 112
  // FLEX_TYPE
  uint32_t FLEX_TYPE_tmp =
      pt_sensor_conf->bulletSensorConfig.algoConfig.FLEX_TYPE;
  memcpy_e(&hashBuffer[offset], &FLEX_TYPE_tmp, sizeof(FLEX_TYPE_tmp));
  offset += sizeof(FLEX_TYPE_tmp);

  // Memory ID: 113
  // BORE_DIAMETER_MM
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM,
      sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM));
  offset +=
      sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM);

  // Memory ID: 114
  // RUN_SPEED_RPM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM);

  // Memory ID: 115
  // BRG_INFO_BPFO
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFO,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFO));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFO);

  // Memory ID: 116
  // BRG_INFO_BPFI
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFI,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFI));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFI);

  // Memory ID: 117
  // BRG_INFO_BPFO
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BSF,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BSF));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BSF);

  // Memory ID: 118
  // BRG_INFO_FTF
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_FTF,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_FTF));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_FTF);

  // Memory ID: 119
  // MTR_INFO_FL
  uint32_t MTR_INFO_FL_tmp =
      (uint32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_FL);
  memcpy_e(&hashBuffer[offset], &MTR_INFO_FL_tmp, sizeof(MTR_INFO_FL_tmp));
  offset += sizeof(MTR_INFO_FL_tmp);

  // Memory ID: 120
  // MTR_INFO_BAR
  uint32_t MTR_INFO_BAR_tmp =
      (uint32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_BAR);
  memcpy_e(&hashBuffer[offset], &MTR_INFO_BAR_tmp, sizeof(MTR_INFO_BAR_tmp));
  offset += sizeof(MTR_INFO_BAR_tmp);

  // Memory ID: 121
  // GEAR_INFO_TOOTH
  uint32_t GEAR_INFO_TOOTH_tmp =
      (uint32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH);
  memcpy_e(&hashBuffer[offset], &GEAR_INFO_TOOTH_tmp,
           sizeof(GEAR_INFO_TOOTH_tmp));
  offset += sizeof(GEAR_INFO_TOOTH_tmp);

  // Memory ID: 122
  // FAN_INFO_BLADE
  uint32_t FAN_INFO_BLADE_tmp =
      (uint32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.FAN_INFO_BLADE);
  memcpy_e(&hashBuffer[offset], &FAN_INFO_BLADE_tmp,
           sizeof(FAN_INFO_BLADE_tmp));
  offset += sizeof(FAN_INFO_BLADE_tmp);

  // Memory ID: 123
  // PUMP_INFO_VANE
  uint32_t PUMP_INFO_VANE_tmp =
      (uint32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.PUMP_INFO_VANE);
  memcpy_e(&hashBuffer[offset], &PUMP_INFO_VANE_tmp,
           sizeof(PUMP_INFO_VANE_tmp));
  offset += sizeof(PUMP_INFO_VANE_tmp);

  // Memory ID: 124
  // TEMP_OV_ALERT_CDEGREE
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE,
      sizeof(
          pt_sensor_conf->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE));
  offset += sizeof(
      pt_sensor_conf->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE);

  // Memory ID: 125
  // ACC_OV_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALERT));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALERT);

  // Memory ID: 126
  // ACC_OV_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALARM));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALARM);

  // Memory ID: 127
  // ACC_HAL_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALERT));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALERT);

  // Memory ID: 128
  // ACC_HAL_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALARM));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALARM);

  // Memory ID: 129
  // VEL_OV_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALERT));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALERT);

  // Memory ID: 130
  // VEL_OV_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALARM));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALARM);

  // Memory ID: 131
  // VEL_HAL_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALERT));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALERT);

  // Memory ID: 132
  // VEL_HAL_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALARM));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALARM);

  // Memory ID: 133
  // ENV_OV_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALERT));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALERT);

  // Memory ID: 134
  // ENV_OV_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALARM));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALARM);

  // Memory ID: 135
  // ENV_HAL_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALERT));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALERT);

  // Memory ID: 136
  // ENV_HAL_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALARM));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALARM);

  /* System */
  // Memory ID: 137
  // txPowerAdv
  int32_t txPowerAdv_tmp =
      pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAdv;
  memcpy_e(&hashBuffer[offset], &txPowerAdv_tmp, sizeof(txPowerAdv_tmp));
  offset += sizeof(txPowerAdv_tmp);

  // Memory ID: 138
  // dataRate_auxAdv
  uint32_t dataRateAuxAdv_tmp =
      pt_sensor_conf->bulletSensorConfig.sysConfig.dataRateAuxAdv;
  memcpy_e(&hashBuffer[offset], &dataRateAuxAdv_tmp,
           sizeof(dataRateAuxAdv_tmp));
  offset += sizeof(dataRateAuxAdv_tmp);

  // Memory ID: 139
  // txPower_auxAdv
  int32_t txPowerAuxAdv_tmp =
      pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAuxAdv;
  memcpy_e(&hashBuffer[offset], &txPowerAuxAdv_tmp, sizeof(txPowerAuxAdv_tmp));
  offset += sizeof(txPowerAuxAdv_tmp);

  // Memory ID: 140
  // DataRateConn
  // Skip it
  offset += 4;

  // Memory ID: 141
  // TxPowerConn
  // Skip it
  offset += 4;

  // Memory ID: 142
  // GatewayAddr
  // For nameID, skip it
  offset += 4;
  // For gateway MAC address
  macAddr_t gwMACAddr;
  sscanf(clientID, "%02x%02x%02x%02x%02x%02x", &gwMACAddr.addr.addrArray[0],
         &gwMACAddr.addr.addrArray[1], &gwMACAddr.addr.addrArray[2],
         &gwMACAddr.addr.addrArray[3], &gwMACAddr.addr.addrArray[4],
         &gwMACAddr.addr.addrArray[5]);
  memcpy_e(&hashBuffer[offset], gwMACAddr.addr.addrArray,
           sizeof(gwMACAddr.addr.addrArray));
  offset += sizeof(gwMACAddr.addr.addrArray);

  // Scheduler
  // Memory ID: 143
  // period_quickPolling
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.schConfig.period_quickPolling_s,
      sizeof(
          pt_sensor_conf->bulletSensorConfig.schConfig.period_quickPolling_s));
  offset += sizeof(
      pt_sensor_conf->bulletSensorConfig.schConfig.period_quickPolling_s);

  // Memory ID: 144
  // period_regularSensing
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.schConfig.period_regularSensing_s,
      sizeof(pt_sensor_conf->bulletSensorConfig.schConfig
                 .period_regularSensing_s));
  offset += sizeof(
      pt_sensor_conf->bulletSensorConfig.schConfig.period_regularSensing_s);

  // Memory ID: 145
  // period_comm
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.schConfig.period_comm_s,
           sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.period_comm_s));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.period_comm_s);

  // Memory ID: 146
  // referenceTime_quickPolling
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.schConfig.refTime_quickPolling_s,
      sizeof(
          pt_sensor_conf->bulletSensorConfig.schConfig.refTime_quickPolling_s));
  offset += sizeof(
      pt_sensor_conf->bulletSensorConfig.schConfig.refTime_quickPolling_s);

  // Memory ID: 147
  // referenceTime_regularSensing
  memcpy_e(
      &hashBuffer[offset],
      &pt_sensor_conf->bulletSensorConfig.schConfig.refTime_regularSensing_s,
      sizeof(pt_sensor_conf->bulletSensorConfig.schConfig
                 .refTime_regularSensing_s));
  offset += sizeof(
      pt_sensor_conf->bulletSensorConfig.schConfig.refTime_regularSensing_s);

  // Memory ID: 148
  // referenceTime_period_comm
  uint64_t refTime_period_comm_s_tmp =
      pt_sensor_conf->bulletSensorConfig.schConfig.refTime_period_comm_s;
  memcpy_e(&hashBuffer[offset], &refTime_period_comm_s_tmp,
           sizeof(refTime_period_comm_s_tmp));
  offset += sizeof(refTime_period_comm_s_tmp);

  // Memory ID: 149
  // waveDataAcqReferenceTime
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.schConfig.refTime_wave_s,
           sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.refTime_wave_s));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.refTime_wave_s);

  // Memory ID: 150
  // waveDataAcqPeriod
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.schConfig.period_wave_s,
           sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.period_wave_s));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.period_wave_s);
#if 1
  LOG_INFO(OUTPOINT, "hash input buffer:\r\n");
  for (uint16_t i = 0; i < offset; i++) {
    printf("%02x ", hashBuffer[i]);
  }
  LOG_INFO(OUTPOINT, "\r\n");
#endif
  hash_value = util_bullet_elfhash(hashBuffer, offset);

  return hash_value;
}
uint32_t calculate_config_hash_from_db(
    gw_pro_command_sqlite_query_item_response_t *query_response,
    char *clientID) {
  sensorConfig_t sensor_conf;

  if ((query_response == NULL) || (clientID == NULL)) {
    return 0;
  }
  if (strlen(clientID) > strlen("C4BD6A000000")) {
    return 0;
  }

  memset(&sensor_conf, 0, sizeof(sensor_conf));
  for (uint8_t i = 0; i < query_response->dataNum; i++) {
    // LOG_DEBUG(OUTPOINT, "field:%d\r\n",
    //           query_response.inquiredData[i].field);
    switch (
        query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
            .field) {
    case 1: {
      sensor_conf.nameId =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 7: {
      sensor_conf.version =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 12: {
      // {"INT","sensorMode",},
      uint8_t sensorMode_tmp;
      // sscanf(
      //     query_response->inquiredData[i %
      //     GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
      //         .data.data_char,
      //     "%d", &sensorMode_tmp);
      sensorMode_tmp =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      sensor_conf.bulletSensorConfig.sysConfig.sensorMode = sensorMode_tmp;
      LOG_DEBUG(OUTPOINT, "sensorMode %d",
                sensor_conf.bulletSensorConfig.sysConfig.sensorMode);
      break;
    }
    case 13: { // { "INT","batteryAlarmThreshold",},
      sensor_conf.bulletSensorConfig.sysConfig.batteryAlarmThrePercent =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 14: { // {"INT","txPower_adv",},
      sensor_conf.bulletSensorConfig.sysConfig.txPowerAdv =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_int8;
      break;
    }
    case 15: { // BLE Aux Adv DataRate
      sensor_conf.bulletSensorConfig.sysConfig.dataRateAuxAdv =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint16;
      break;
    }
    case 16: { // Aux Adv Tx Power
      sensor_conf.bulletSensorConfig.sysConfig.txPowerAuxAdv =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_int8;
      break;
    }
    case 17: { // Quick Polling period
      sensor_conf.bulletSensorConfig.schConfig.period_quickPolling_s =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 18: { // Ref Time of Quick Polling
      sensor_conf.bulletSensorConfig.schConfig.refTime_quickPolling_s =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 19: { // Regular Sensing Period
      sensor_conf.bulletSensorConfig.schConfig.period_regularSensing_s =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 20: { // Ref time of regular sensing
      sensor_conf.bulletSensorConfig.schConfig.refTime_regularSensing_s =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 21: { // BLE communicaton Period
      sensor_conf.bulletSensorConfig.schConfig.period_comm_s =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 22: { // ref time of ble communication
      sensor_conf.bulletSensorConfig.schConfig.refTime_period_comm_s =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 23: { // sample rate of Vib sensing for pre-Acq
      sensor_conf.bulletSensorConfig.senConfig.pre_acq_vib_fs_hz =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 24: { // sample point number of vib sensign for pre-acq
      sensor_conf.bulletSensorConfig.senConfig.pre_acq_vib_n =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 25: { // axis mask of vib senginsg for pre-acq
      sensor_conf.bulletSensorConfig.senConfig.pre_acq_vib_axis_acq_eval =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 26: { // range of vib sensing for pre-acq
      sensor_conf.bulletSensorConfig.senConfig.pre_acq_vib_range =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 27: { // sample rate of mag sensing for pre-acq
      sensor_conf.bulletSensorConfig.senConfig.pre_acq_mag_fs_hz =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 28: { // sample point number of mag sensing for pre-acq
      sensor_conf.bulletSensorConfig.senConfig.pre_acq_mag_n =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 29: { // axis mask of mag sensing for pre-acq
      sensor_conf.bulletSensorConfig.senConfig.pre_acq_mag_axis_acq_eval =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 30: { // sample rate of vib sensing for acq
      sensor_conf.bulletSensorConfig.senConfig.fAccHz =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 31: { // sample poing number of vib sensing for acq
      sensor_conf.bulletSensorConfig.senConfig.nAcc =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 32: { // axis mask of vib sensing for acq
      sensor_conf.bulletSensorConfig.senConfig.acq_vib_axis_acq_eval =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 33: { // range of vib sensing for acq
      sensor_conf.bulletSensorConfig.senConfig.acq_vib_range =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 34: { // sample rage of mag sensing for acq
      sensor_conf.bulletSensorConfig.senConfig.acq_mag_fs_hz =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 35: { // sample point number of mag sensing for acq
      sensor_conf.bulletSensorConfig.senConfig.acq_mag_n =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 36: { // axis mask of mag sensing for acq
      sensor_conf.bulletSensorConfig.senConfig.acq_mag_axis_acq_eval =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 37: { // fs-coef
      sensor_conf.bulletSensorConfig.senConfig.fs_coef =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 38: { // GEE-coef
      sensor_conf.bulletSensorConfig.algoConfig.GEE_COEF =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 39: { // v-coef
               // Need a force type cast (not sure whether it is caused by the
               // database bug)
               // uint32_t V_COEF_tmp =
               // (uint32_t)(query_response
      //                ->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
      //                .data.data_float);
      sensor_conf.bulletSensorConfig.algoConfig.V_COEF =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      // LOG_ERR(OUTPOINT, "V_COEF %d\r\n", V_COEF_tmp);
      // memcpy(&sensor_conf.bulletSensorConfig.algoConfig.V_COEF, &V_COEF_tmp,
      //        sizeof(sensor_conf.bulletSensorConfig.algoConfig.V_COEF));
      LOG_DEBUG(OUTPOINT, "V_COEF %f",
                sensor_conf.bulletSensorConfig.algoConfig.V_COEF);
      break;
    }
    case 40: { // meas-position
      sensor_conf.bulletSensorConfig.algoConfig.MEAS_POSITION =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 41: { // meas-load
      sensor_conf.bulletSensorConfig.algoConfig.MEAS_LOAD =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 42: { // meas-axis-vib
      sensor_conf.bulletSensorConfig.algoConfig.MEAS_AXIS_VIB =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 43: { // meas-aix-mag
      sensor_conf.bulletSensorConfig.algoConfig.MEAS_AXIS_MAG =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 44: { // vib-start-fg
      sensor_conf.bulletSensorConfig.algoConfig.VIB_START_FG =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 45: { // vib-rms-start-tl
      sensor_conf.bulletSensorConfig.algoConfig.VIB_RMS_START_TL =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 46: { // mag_start-fg
      sensor_conf.bulletSensorConfig.algoConfig.MAG_START_FG =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 47: { // mag-rms-start-tl
      sensor_conf.bulletSensorConfig.algoConfig.MAG_RMS_START_TL =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 48: { // mag-stable-fg
      sensor_conf.bulletSensorConfig.algoConfig.MAG_STABLE_FG =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 49: { // mag-rms-var-th
      sensor_conf.bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 50: { // rpm_var_range
      sensor_conf.bulletSensorConfig.algoConfig.RPM_VAR_RANGE =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 51: { // dec-logic-temp-m
      sensor_conf.bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint16;
      break;
    }
    case 52: { // dec-logic-temp-n
      sensor_conf.bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint16;
      break;
    }
    case 53: { // dec-logic-learn-num-temp
      sensor_conf.bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_TEMP =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint16;
      break;
    }
    case 54: { // dec-logic-vib-m
      sensor_conf.bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint16;
      break;
    }
    case 55: { // dec-logici-vib-n
      sensor_conf.bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint16;
      break;
    }
    case 56: { // dec-logic leanrn-num-vib
      sensor_conf.bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint16;
      break;
    }
    case 57: { // dec_logic_learn_num_mag
      sensor_conf.bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint16;
      break;
    }
    case 58: { // func_anom_temp
      sensor_conf.bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 59: { // func_anom_ov
      sensor_conf.bulletSensorConfig.algoConfig.FUNC_ANOM_OV =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 60: { // func_anom_mech
      sensor_conf.bulletSensorConfig.algoConfig.FUNC_ANOM_MECH =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 61: { // func_anom_brg
      sensor_conf.bulletSensorConfig.algoConfig.FUNC_ANOM_BRG =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 62: { // func_anom_lub
      sensor_conf.bulletSensorConfig.algoConfig.FUNC_ANOM_LUB =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 63: { // func_anom_mtr
      sensor_conf.bulletSensorConfig.algoConfig.FUNC_ANOM_MTR =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 64: { // func_anom_gear
      sensor_conf.bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 65: { // func_anom_fan
      sensor_conf.bulletSensorConfig.algoConfig.FUNC_ANOM_FAN =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 66: { // func_anom_pump
      sensor_conf.bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 67: { // asset_levels
      sensor_conf.bulletSensorConfig.algoConfig.ASSET_LEVEL =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 68: { // flex_type
      sensor_conf.bulletSensorConfig.algoConfig.FLEX_TYPE =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint8;
      break;
    }
    case 69: { // bore_diameter_mm
      sensor_conf.bulletSensorConfig.algoConfig.BORE_DIAMETER_MM =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      LOG_DEBUG(OUTPOINT, "BORE_DIAMETER_MM %f",
                sensor_conf.bulletSensorConfig.algoConfig.BORE_DIAMETER_MM);
      break;
    }
    case 70: { // run_speed_rpm
      sensor_conf.bulletSensorConfig.algoConfig.RUN_SPEED_RPM =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      LOG_DEBUG(OUTPOINT, "RUN_SPEED_RPM %f",
                sensor_conf.bulletSensorConfig.algoConfig.RUN_SPEED_RPM);
      break;
    }
    case 71: { // brg_info_bpfo
      sensor_conf.bulletSensorConfig.algoConfig.BRG_INFO_BPFO =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 72: { // brg_info_bpfi
      sensor_conf.bulletSensorConfig.algoConfig.BRG_INFO_BPFI =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 73: { // brg_info_bsf
      sensor_conf.bulletSensorConfig.algoConfig.BRG_INFO_BSF =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 74: { // brg_info_ftf
      sensor_conf.bulletSensorConfig.algoConfig.BRG_INFO_FTF =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 75: { 
      sensor_conf.bulletSensorConfig.algoConfig.MTR_INFO_FL =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      LOG_DEBUG(OUTPOINT, "MTR_INFO_FL %d",
                sensor_conf.bulletSensorConfig.algoConfig.MTR_INFO_FL);
      break;
    }
    case 76: { 
      sensor_conf.bulletSensorConfig.algoConfig.MTR_INFO_BAR =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      LOG_DEBUG(OUTPOINT, "MTR_INFO_BAR %d",
                sensor_conf.bulletSensorConfig.algoConfig.MTR_INFO_BAR);
      break;
    }
    case 77: { 
      sensor_conf.bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      LOG_DEBUG(OUTPOINT, "GEAR_INFO_TOOTH %d",
                sensor_conf.bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH);
      break;
    }
    case 78: { 
      sensor_conf.bulletSensorConfig.algoConfig.FAN_INFO_BLADE =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      LOG_DEBUG(OUTPOINT, "FAN_INFO_BLADE %d",
                sensor_conf.bulletSensorConfig.algoConfig.FAN_INFO_BLADE);
      break;
    }
    case 79: { 
      sensor_conf.bulletSensorConfig.algoConfig.PUMP_INFO_VANE =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      LOG_DEBUG(OUTPOINT, "PUMP_INFO_VANE %d",
                sensor_conf.bulletSensorConfig.algoConfig.PUMP_INFO_VANE);
      break;
    }
    case 80: { 
      sensor_conf.bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      LOG_DEBUG(
          OUTPOINT, "TEMP_OV_ALERT_CDEGREE %f",
          sensor_conf.bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE);
      break;
    }
    case 81: { // alarm_threstemp
      sensor_conf.bulletSensorConfig.algoConfig.AlarmThrestemp =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_int32;
      break;
    }
    case 82: { // acc_ov_alert
      sensor_conf.bulletSensorConfig.algoConfig.ACC_OV_ALERT =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 83: { // acc_ov_alarm
      sensor_conf.bulletSensorConfig.algoConfig.ACC_OV_ALARM =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 84: { // acc_hal_alert
      sensor_conf.bulletSensorConfig.algoConfig.ACC_HAL_ALERT =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 85: { // acc_hal_alarm
      sensor_conf.bulletSensorConfig.algoConfig.ACC_HAL_ALARM =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 86: { // vel_ov_alert
      sensor_conf.bulletSensorConfig.algoConfig.VEL_OV_ALERT =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 87: { // vel_ov_alarm
      sensor_conf.bulletSensorConfig.algoConfig.VEL_OV_ALARM =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 88: { // vel_hal_alert
      sensor_conf.bulletSensorConfig.algoConfig.VEL_HAL_ALERT =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 89: { // vle_hal_alarm
      sensor_conf.bulletSensorConfig.algoConfig.VEL_HAL_ALARM =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 90: { // env_ov_alert
      sensor_conf.bulletSensorConfig.algoConfig.ENV_OV_ALERT =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 91: { // env_ov_alaram
      sensor_conf.bulletSensorConfig.algoConfig.ENV_OV_ALARM =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 92: { // env_HA_alert
      sensor_conf.bulletSensorConfig.algoConfig.ENV_HAL_ALERT =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 93: { // env_hal_alarm
      sensor_conf.bulletSensorConfig.algoConfig.ENV_HAL_ALARM =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_float;
      break;
    }
    case 94: { // waveDataAcqPeriod
      sensor_conf.bulletSensorConfig.schConfig.period_wave_s =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
    case 95: { // waveDataAcqReference
      sensor_conf.bulletSensorConfig.schConfig.refTime_wave_s =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
              .data.data_uint32;
      break;
    }
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
      case 98:
      {
        // "INT DEFAULT 0",
        // "sensorModeParameter",
        sensor_conf.bulletSensorConfig.sysConfig.sensorModeParameter =
          query_response->inquiredData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
            .data.data_uint8;
        break;
      }
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
    default:
      break;
    }
  }
  return util_froto_cal_config_hash_value(&sensor_conf, clientID);
}
