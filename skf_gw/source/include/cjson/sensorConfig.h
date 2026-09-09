/**
 * @file    sensorConfig.h
 * @author  Xiaoyuan (Sean) Ma (zz2134)
 * @date    2024-04-26
 * @brief   MACROs, definitions for sensorConfig
 * @details
 */
#ifndef __SENSORCONFIG_H__
#define __SENSORCONFIG_H__

#include "stdint.h"
#include "stdbool.h"
#include "addr.h"
#include "ppGW.h"

// BLE Tx Power
#define SYSCONFIG_BLETXPOWER_0_DBM (0)
#define SYSCONFIG_BLETXPOWER_NEG_4_DBM (-4)
#define SYSCONFIG_BLETXPOWER_NEG_8_DBM (-8)
// BLE AUX ADV Data Rate
#define SYSCONFIG_BLEDATARATE_1MBPS (1000)
#define SYSCONFIG_BLEDATARATE_2MBPS (2000)
#define SYSCONFIG_BLEDATARATE_500KBPS (500)
#define SYSCONFIG_BLEDATARATE_125KBPS (125)
// Sensor's Axis
#define SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X (1)
#define SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y (2)
#define SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Z (4)
// Vibration Sensor's Range
#define SENSINGCONFIG_VIBRANGE_2GEE (2)
#define SENSINGCONFIG_VIBRANGE_4GEE (4)
#define SENSINGCONFIG_VIBRANGE_8GEE (8)
#define SENSINGCONFIG_VIBRANGE_16GEE (16)
// BULLET_SENSOR_MODE
#define BULLET_SENSOR_MODE_FLIGHT (1)
#define BULLET_SENSOR_MODE_FLIGHT_NAME "0"
#define BULLET_SENSOR_MODE_STANDBY (2)
#define BULLET_SENSOR_MODE_STANDBY_NAME "1"
#define BULLET_SENSOR_MODE_NORMAL (3)
#define BULLET_SENSOR_MODE_NORMAL_NAME "2"
#define BULLET_SENSOR_MODE_FCT (4)
#define BULLET_SENSOR_MODE_FCT_NAME "4"

typedef struct
{
  uint8_t sensorMode;  // The work mode of sensor:
                       // BULLET_SENSOR_MODE_FLIGHT,
                       // BULLET_SENSOR_MODE_STANDBY,
                       // BULLET_SENSOR_MODE_NORMAL,
                       // or BULLET_SENSOR_MODE_FCT.
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
  uint8_t sensorModeParameter;
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
  float batteryAlarmThrePercent;  // A low battery alarm threshold (in
                                    // percentage). For instance, 20 means
                                    // low battery alarm would be reported
                                    // when the battery volume is lower
                                    // than 20%.
  uint64_t currentTimeS;  // In second. A unix timestamp (GMT time), used
                          // to set time of the sensor, which can be
                          // regarded as the last communication time.
  int8_t txPowerAdv;  // Transmit power (dBm) of BLE advertising,
                      // SYSCONFIG_BLETXPOWER_0_DBM,
                      // SYSCONFIG_BLETXPOWER_NEG_4_DBM, or
                      // SYSCONFIG_BLETXPOWER_NEG_8_DBM
  uint16_t dataRateAuxAdv;  // Data rate of BLE advertising (as a
                            // non-standard auxiliary advertising to
                            // improve
                            // communication range):
                            // SYSCONFIG_BLEDATARATE_1MBPS,
                            // SYSCONFIG_BLEDATARATE_2MBPS,
                            // SYSCONFIG_BLEDATARATE_500KBPS, or
                            // SYSCONFIG_BLEDATARATE_125KBPS
  int8_t txPowerAuxAdv;  // Transmit power of BLE advertising (as a
                         // non-standard auxiliary advertising):
                         // SYSCONFIG_BLETXPOWER_0_DBM,
                         // SYSCONFIG_BLETXPOWER_NEG_4_DBM, or
                         // SYSCONFIG_BLETXPOWER_NEG_8_DBM
}sensorConfig_bulletSensorConfig_sysConfig_t;

typedef struct
{
  uint32_t period_quickPolling_s;  // Quick polling period in second. Zero
                                   // represents no periodic quick polling.
  uint32_t refTime_quickPolling_s;  // A unix timestamp (GMT time), used as
                                    // the start time of periodic quick
                                    // polling.
  uint32_t period_regularSensing_s;  // Regular sensing period in second.
                                     // Zero represents periodic regular
                                     // sensing.
  uint32_t refTime_regularSensing_s;  // A unix timestamp (GMT time), used
                                      // as the start time of periodic
                                      // regular sensing.
  uint32_t period_comm_s;  // BLE communication period in second. Zero is
                           // not allowed.
  uint32_t refTime_period_comm_s;  // A unix timestamp (GMT time), used as
                                   // the start time of periodic BLE
                                   // communication.
  uint32_t period_wave_s;  // Waveform aquisition period in second. Zero
                           // represents no need to upload waveform.
  uint32_t refTime_wave_s;  // A unix timestamp (GMT time), used as the
                            // start time of waveform uploading.
}sensorConfig_bulletSensorConfig_schCfg_t;

typedef struct
{
  uint32_t pre_acq_vib_fs_hz;  // The vibration sensing frequency in Hz for
                               // the pre-acquisition.
  uint32_t pre_acq_vib_n;  // The number of vibration sensing for the
                           // pre-acquisition.
  uint8_t pre_acq_vib_axis_acq_eval;  // A mask to indicate which axis
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
  uint32_t pre_acq_mag_fs_hz;  // The magnetic field sensing frequency in
                               // Hz for the pre-acquisition.
  uint32_t pre_acq_mag_n;  // The number of magnetic field sensing for the
                           // pre-acquisition.
  uint8_t pre_acq_mag_axis_acq_eval;  // A mask to indicate which axis
                                      // (axes) is/are sampled during
                                      // magnetic field sensing of the
                                      // pre-acquisition: a mask of
                                      // SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X
                                      // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y
                                      // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Z
  uint32_t fAccHz;  // The vibration sensing frequency in Hz for the
                    // acquisition
  uint32_t nAcc;  // The number of vibration sensing frequency for the
                  // acquisition.
  uint8_t acq_vib_axis_acq_eval;  // A mask to indicate which axis
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
  uint32_t acq_mag_fs_hz;  // The magnetic field sensing frequency in
                           // Hz for the acquisition.
  uint32_t acq_mag_n;  // The number of magnetic field sensing for the
                       // acquisition.
  uint8_t acq_mag_axis_acq_eval;  // A mask to indicate which axis
                                  // (axes) is/are sampled during
                                  // magnetic field sensing of the
                                  // acquisition: a mask of
                                  // SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X
                                  // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y
                                  // / SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Z
  float fs_coef;  // A fixed value for bullet sensor. (3.2)
}sensorConfig_bulletSensorConfig_senCfg_t;

typedef struct
{
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
}sensorConfig_bulletSensorConfig_algoCfg_t;

typedef struct
{
  sensorConfig_bulletSensorConfig_sysConfig_t sysConfig;
  sensorConfig_bulletSensorConfig_schCfg_t schConfig;
  sensorConfig_bulletSensorConfig_senCfg_t senConfig;
  sensorConfig_bulletSensorConfig_algoCfg_t algoConfig;
}sensorConfig_bulletSensorConfig_t;

typedef struct
{
  uint8_t version;
  uint64_t lastEditTimeS;  // In second.
  bool haveConfiguredToSensor;  // To indicate whether current
                                // configuration has been applied to the
                                // sensor
  uint64_t whenConfiguredToSensor;  // A unix timestamp (GMT time)
                                    // representing when the sensor has
                                    // been configured. This field is valid
                                    // when haveConfiguredToSensor is true
  char type[50];  // The product name/type
  char manufacturer[10];  // The manufacturer of the product
  uint32_t nameId;  // A 4-byte hash ID to avoid the same name. nameId is
                    // the index.
  macAddr_t macAddr;  // BLE MAC address of the sensor (separated by
                      // dashes in JSON).
#if (ENABLE_MODBUS_FEATURE == 1)
  //  skip it at the moment
  // char description[GW_PRO_MAX_STRING_LEN_BYTE];  // The description field of device in "CH00-AD0000-L00-PRS" format
#endif
  sensorConfig_bulletSensorConfig_t bulletSensorConfig;
}sensorConfig_t;

#endif /* __SENSORCONFIG_H__ */
