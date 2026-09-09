/**
 * @file    sensor_interface_insight_predict.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Definition of sensor interface for InsightPredict
 * @details
 */
#ifndef __SENSOR_INTERFACE_INSIGHT_PREDICT_H__
#define __SENSOR_INTERFACE_INSIGHT_PREDICT_H__

#include "sys_def.h"
#include "sensor_interface.h"
#include "Common.pb.h"

/*************************************************************************/
/***************Compatibility with existing implementation****************/
// Note this part is identical to the exisiting definition in app_froto.h
// This part can be optimized after switching to the refactored
// implementation completely
// This part should be modified carefully before switching
/**********************Compatibility part start***************************/
/*************************************************************************/
#ifndef DB_MAX_LENGTH_SENSOR_NAME
#define DB_MAX_LENGTH_SENSOR_NAME GW_PRO_MAX_STRING_LEN_BYTE
#endif
#ifndef DB_MAX_LENGTH_MAC_ADDR
#define DB_MAX_LENGTH_MAC_ADDR GW_PRO_MAX_STRING_LEN_BYTE
#endif
#ifndef DB_MAX_SENSOR_TYPE_STR
#define DB_MAX_SENSOR_TYPE_STR GW_PRO_MAX_STRING_LEN_BYTE
#endif
#ifndef DB_MAX_LENGTH_MANUFACTURER
#define DB_MAX_LENGTH_MANUFACTURER GW_PRO_MAX_STRING_LEN_BYTE
#endif
#ifndef DB_MAX_DT_STORATE_FORM_BUF
#define DB_MAX_DT_STORATE_FORM_BUF 0x10
#endif
#ifndef DB_MAX_DT_ITEM_VAL_ARRAY
#define DB_MAX_DT_ITEM_VAL_ARRAY 0x10
#endif
#ifndef DB_FROTO_ALARM_CODE_SIZE
#define DB_FROTO_ALARM_CODE_SIZE (9U)
#endif
#ifndef DB_STORAGE_FORM_DIGIT
#define DB_STORAGE_FORM_DIGIT "Digit"
#endif
#ifndef DB_STORAGE_FORM_FILENAME
#define DB_STORAGE_FORM_FILENAME "Filename"
#endif
#ifndef DB_WAVEDATA_FILENAME_PREFIX
#define DB_WAVEDATA_FILENAME_PREFIX "/var/lib/skf-gateway/data/wave_"
#endif
#ifndef DB_ALARMDATA_FILENAME_PREFIX
#define DB_ALARMDATA_FILENAME_PREFIX "/var/lib/skf-gateway/data/alarm_code_"
#endif

struct db_waveform_file
{
  int fd;                 // File descriptor
  char fpath[MAX_FPATH];  // Path
};

union db_data_item_value
{
  struct db_waveform_file fpath; // For data stored in a file (e.g.,
                                 // waveform)
  uint8_t byte_array[DB_MAX_DT_ITEM_VAL_ARRAY];
  uint64_t valu64;
  uint32_t valu32;
  uint16_t valu16;
  uint8_t valu8;
  float valufloat;
  double valudouble;
};

struct db_data_item
{
  uint32_t idx; // The item index (which has been deprecated because the
                // index would be populated by the database process
                // automatically)
  uint32_t name_id; // Name ID
  uint8_t sensor_name[DB_MAX_LENGTH_SENSOR_NAME]; // Sensor name
  uint8_t sensor_mac[DB_MAX_LENGTH_MAC_ADDR];     // Sensor MAC addr
  uint8_t sensor_type[DB_MAX_SENSOR_TYPE_STR];    // Sensor type
  uint8_t manufacturer[DB_MAX_LENGTH_MANUFACTURER]; // Manufacturer
  uint32_t meas_id;                               // Measurement sequence
                                                  // number
  uint64_t meas_ts;                               // Measurement time
  uint64_t received_ts;                           // Received time
  SKFChina_Common_AlarmAndNotify alarm;           // Alarm
  SKFChina_Common_MeasurementType data_type;      // Meausrement type
  uint8_t format[DB_MAX_DT_STORATE_FORM_BUF];     // Digit / Filename
  SKFChina_Common_Range range;                    // Range
  SKFChina_Common_Unit unit;                      // Unit of result
  uint32_t measure_length_sample;                 // Length of the sample
                                                  // on which the
                                                  // measurement is based
  uint32_t total_data_length_sample;              // The length of the data
  uint32_t dimension;                             // Dimension of the
                                                  // measurement (if
                                                  // applicable)
  SKFChina_Common_Format data_format;             // Data format
  uint32_t sample_period_s;                       // Sample period (in S)
  SKFChina_Common_Encryption encryp;              // Encrypted or not

  union db_data_item_value value;                 // The value stored in
                                                  // database
  uint32_t crc32;                                 // Deprecated

  float sample_rate_hz;                           // Sample rate (in Hz) on
                                                  // which the measurement
                                                  // is based
  SKFChina_Common_SensorType froto_sensor_type;   // Sensor type
  SKFChina_Common_ProductType product_type;       // Product type
};

/*************************************************************************/
/***********************Compatibility part end****************************/
/*************************************************************************/

struct InsightPInterface_userdata
{
  uint64_t lastRegularSensingTime_s;
  uint32_t waveformPeriod_s;
  uint32_t regularSensingPeriod_s;
  uint32_t waveformRef_s;
  uint32_t regularSensingRef_s;
  uint32_t regularSensingMeas_id; // identify manual triggered case  >= 32768
};

bool registerSensorIntfInsightPredict(
  struct sensor_interface *sensorIntf,
  uint8_t *errCode);
bool unregisterSensorIntfInsightPredict(
  struct sensor_interface *sensorIntf,
  uint8_t *errCode);

#endif /* __SENSOR_INTERFACE_INSIGHT_PREDICT_H__ */
