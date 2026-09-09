/*
 * head file sh_mem.h 
 * History:
 *  1. [24-11-13]: init version
 * 
 */
#ifndef SH_MEM_H
#define SH_MEM_H

#include "ppGW.h"
// for gwConfig_t
#include "gwConfig.h"
#include "Common.pb.h"
// #include "sys_def.h"
#include "stdint.h"
#include <auxiliar.h>
#include <stdint.h>

#include <sys/types.h>
//#include <sys/wait.h>
// #include <segment.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

/* 
 * =================== MACROes ==============
 */
// common part
#ifndef MAX_NUM_40
#define MAX_NUM_40  1
#endif
#if (MAX_NUM_40 == 1)
#define BULLET_GW_SENSOR_NUM_MAX            (40)      // [verify 80 chn]the max number of sensors that a bullet gateway can support. skip it if it's defined somewhere else
#define MODBUS_REG_SENSOR_CHANNEL_MAX       (0x28)    // [verify 80 chn] 
#define APP_SHM_DEV_TABLE_ITEM_MAX          (40)      // [verify 80 chn]40, maximum item number in Dev table in share memory
#define APP_SHM_SENSORDATA_TABLE_ITEM_MAX   (0x28)    // [verify 80 chn]40, maximum item number in SensorData table in share memory
#else
#define BULLET_GW_SENSOR_NUM_MAX            (80)      // the max number of sensors that a bullet gateway can support. skip it if it's defined somewhere else
#define MODBUS_REG_SENSOR_CHANNEL_MAX       (0x50)    // TODO: at most 24 channels for predict sensor?? 
#define APP_SHM_DEV_TABLE_ITEM_MAX          (80)      // [verify 80 chn]40, maximum item number in Dev table in share memory
#define APP_SHM_SENSORDATA_TABLE_ITEM_MAX   (0x50)    // [verify 80 chn]40, maximum item number in SensorData table in share memory
#endif
#define SHMEM_NAMED_SEMAPHORE_FILENAME  "/namedsemafile"   // named semaphore file name in /dev/shm directory
#define PROJECT_ID_FOR_SHM              (67)  // project id used by ftok()
#define FPATH_FOR_SHM                   "/run/skf-gateway/ipc/shm_key"  // file path used by ftok()
#define FPATH_FOR_CH_NAMEID_FILE        "/run/skf-gateway/ipc/ch_nid_file"  // file path used by ftok()
#define PERMISSION_FLAG_FOR_SHM         (0600)

// product type used in 'description' field of dev table.
#define MODBUS_PRODUCT_TYPE_PREDICT_SENSOR        "PRS" // predict sensor type
#define MODBUS_PRODUCT_TYPE_PRO_SENSOR            "PRO" // predict-pro sensor type
#define MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR      "ITS" // insight_T sensor type
#define MODBUS_PRODUCT_TYPE_GATEWAY               "GWY" // gateway type
// TODO define some more types here
// for building sensor name
#define DEV_SENSOR_NAME_DEFAULT                   "CH00-AD0000-L00-PRS"
#define SENSOR_NAME_BUILD_STRING                  "CH%02x-AD%04x-L%02x-%s"
#define SENSOR_NAME_CHN_OFFSET                    (0x2)   // channel offset of sensor name
#define SENSOR_NAME_CHN_LENGTH                    (0x2)   // channel length of sensor name
#define SENSOR_NAME_START_ADDR_OFFSET             (0x7)   // channel start address offset of sensor name
#define SENSOR_NAME_START_ADDR_LENGTH             (0x4)   // channel start address length of sensor name
#define SENSOR_NAME_REG_LEN_OFFSET                (0xD)   // register length offset of sensor name
#define SENSOR_NAME_REG_LEN_LENGTH                (0x2)   // register length length of sensor name
#define SENSOR_NAME_TYPE_OFFSET                   (0x10)   // sensor type offset of sensor name
#define SENSOR_NAME_TYPE_LENGTH                   (0x3)   // sensor type length of sensor name
#define SENSOR_NAME_TOTAL_LENGTH                  (sizeof(DEV_SENSOR_NAME_DEFAULT))   // total length of sensor name

// // register mode definations
// #define CONFIG_REG_ADDR_EXTENSIBLE_MODE (1)
// #define CONFIG_REG_ADDR_EFFICIENT_MODE  (2)
// #define MODBUS_SLAVE_REG_ADDRESS_MODE   CONFIG_REG_ADDR_EXTENSIBLE_MODE
// // #define MODBUS_SLAVE_REG_ADDRESS_MODE   CONFIG_REG_ADDR_EFFICIENT_MODE
// #if (MODBUS_SLAVE_REG_ADDRESS_MODE == CONFIG_REG_ADDR_EXTENSIBLE_MODE)   //CONFIG_REG_ADDR_EXTENSIBLE_MODE
/*
 * definations for name in 'Dev' table.
 * format:  for example, "CH02-AD000F-L2F-PRS"
 *         02  --  channel #
 *       000f  --  start address of channel
 *         2f  --  length (total register number)
 *        PRS  --  sensor product(a string to identify sensor type)
 * notes:
 *       1. use Extensible mode at the moment;
 *       2. 40 channels at most;
 *       3. layout: 1 ch for gw, 39 chs for predict sensor or insight-T(not diffrentiate sensor types with the same reg size) in fixed order;
 *       4. total reg address size should be limited to 65536 bytes.
 * 
 * layout(TODO, modify it later if necessary):
 * NOTES : the number here takes byte as unit. it SHOULD BE converted into WORD ( the number/2 ) before MODBUS using it.
 * 0x0              0x80                                         0x1400 (0x80 + 0x80 * 0x28)
 *  |   ch0          | ch1 - ch40 (total: 40)                      |
 *  |   gateway      | predict sensor or insight-T sensor          |
 *  |  len:0x80      | len: 0x80 per channel                       |
 */
#define MODBUS_REG_GATEWAY_START_ADDR_OFFSET      (0x0)
#define MODBUS_REG_GATEWAY_LENGTH                 (0x80)  // 128-byte 
#define MODBUS_REG_SENSOR_START_ADDR_OFFSET       (MODBUS_REG_GATEWAY_START_ADDR_OFFSET + MODBUS_REG_GATEWAY_LENGTH)
#define MODBUS_REG_SENSOR_LENGTH                  (0x80)    // 128-byte per channel
#define MODBUS_REG_ADDR_END                       (MODBUS_REG_SENSOR_START_ADDR_OFFSET + MODBUS_REG_SENSOR_LENGTH * MODBUS_REG_SENSOR_CHANNEL_MAX)
#define MODBUS_REG_ALL_CHANNEL                    (MODBUS_REG_SENSOR_CHANNEL_MAX + 0x1)
// for extensible mode
#define DEV_REG_ADDR_LENGTH_EXT_MODE              (0x80)   // register address length(fixed:0x80) of sensor & gateway
// more macroes for sensor name in efficient mode
#define DEV_REG_ADDR_LENGTH_GW                    (0x3)   // register address length(3) of gateway
#define DEV_REG_ADDR_LENGTH_INSIGHT_T             (0x8)   // register address length(8) of insight-T sensor
#define DEV_REG_ADDR_LENGTH_INSIGHT_P             (0x30)  // register address length(48 - 0x30) of insight-P sensor
#define DEV_REG_ADDR_LENGTH_INSIGHT_PRO           (0x30)  // register address length(48 - 0x30) of insight-Pro sensor

// #else   //CONFIG_REG_ADDR_EFFICIENT_MODE
// // TODO
// /*
//  * definations for name in 'Dev' table.
//  * format:  for example, "CH02-AD000F-L2F-PRS"
//  *         02  --  channel #
//  *       000f  --  start address of channel
//  *         2f  --  length (total register number)
//  *        PRS  --  sensor product(a string to identify sensor type)
//  * notes:
//  *       1. use Extensible mode at the moment;
//  *       2. 40 channels at most;
//  *       3. layout: 1 ch for gw, 24 chs for predict sensor, and 15 chs for insight-T in fixed order;
//  *       4. total reg address size should be limited to 65536 bytes.
//  * 
//  * layout(TODO, modify it later if necessary):
//  * 0x0              0x100                      0x3100 (0x100 + 0x200 * 0x18)
//  *  |   ch0          | ch1 - ch24 (total: 24)   | ch25 - ch39 (total: 15)  |
//  *  |   gateway      | predict sensor           | insight-T sensor         |
//  *  |  len:0x100     | len: 0x200 per channel   | len: 0x100 per channel   |
//  */
// #define MODBUS_REG_GATEWAY_START_ADDR_OFFSET      (0x0)
// #define MODBUS_REG_GATEWAY_LENGTH                 (0x100)  // 256-byte 
// #define MODBUS_REG_PRS_START_ADDR_OFFSET          (MODBUS_REG_GATEWAY_START_ADDR_OFFSET + MODBUS_REG_GATEWAY_LENGTH)
// #define MODBUS_REG_PRS_LENGTH                     (0x200)    // 512-byte per channel
// #define MODBUS_REG_PRS_CHANNEL_MAX                (0x18)    // TODO: at most 24 channels for predict sensor?? 
// #define MODBUS_REG_IST_START_ADDR_OFFSET          (MODBUS_REG_PRS_START_ADDR_OFFSET + MODBUS_REG_PRS_LENGTH * MODBUS_REG_PRS_CHANNEL_MAX)
// #define MODBUS_REG_IST_LENGTH                     (0x100)    // 256-byte per channel
// #define MODBUS_REG_IST_CHANNEL_MAX                (0x0f)    // TODO: at most 15 channels for predict sensor?? 
// // TODO define some more types here
// #define MODBUS_REG_ADDR_END                       (MODBUS_REG_IST_START_ADDR_OFFSET + MODBUS_REG_IST_LENGTH * MODBUS_REG_IST_CHANNEL_MAX)
// #define MODBUS_REG_ALL_CHANNEL                    (MODBUS_REG_PRS_CHANNEL_MAX + MODBUS_REG_IST_CHANNEL_MAX + 1)
// #endif  // End of MODBUS_SLAVE_REG_ADDRESS_MODE

// #if (MODBUS_REG_ADDR_END > (0x10000))   // report error in case size larger than 65536 bytes
// #error "total modbus reg size should be less than 65536 bytes!\n"
// #endif
// // #if (MAX_NUM_40 == 1)
// #if (MODBUS_REG_ALL_CHANNEL > (40))   // [verify 80 chn]report error in case channel number larger than 40
// // #else
// // #if (MODBUS_REG_ALL_CHANNEL > (80))   // report error in case channel number larger than 40
// // #endif
// #error "total modbus channel number should be less than 40!\n"
// #endif

/*
 *  Notes(TODO): the layout should be updated later as the PIECE SIZE is updated from 0x200 to 0x400
 * layout of share memory(TODO, modify it later if necessary):
 * 0x0                0x800(2k)                   0xA800 (42k,0x800 + 0x400 * 40)                           0x14A800(1162k, 0xA800 + 0x400 * 0x1c * 0x28)
 *  |   1 item only     |   40 items at most         | 40 items at most                                                      |
 *  |  Gateway table    |      Dev table             | latest item of each sensor in Sensordata table                        |
 *  |  size:0x800(2k)   | size: 0x400(1k) per item   | size: 0x400(1k) * 0x1c(28 pieces) = 0x8000(28k bytes) per item        |
 * notes:
 *       1. 1 item only for Gateway table;
 *       2. 40 items at most in Dev table;
 *       3. 1 item only for Sensordata table as we only focus on the latest item;
 *       4. total size of share memory MUST BE 4k-aligned.
 */
#define APP_SHM_GATEWAY_TABLE_ADDR_OFFSET       (0x0)      // offset address for item in Gateway table in share memory
#define APP_SHM_GATEWAY_TABLE_ITEM_SIZE         (0x800)   // 2k bytes, total size for item in Gateway table in share memory
#define APP_SHM_GATEWAY_TABLE_ITEM_MAX          (1)        // 1, maximum item number in Gateway table in share memory
// offset: 4k -- base offset address for items in Dev table in share memory
#define APP_SHM_DEV_TABLE_ADDR_OFFSET           (APP_SHM_GATEWAY_TABLE_ADDR_OFFSET + APP_SHM_GATEWAY_TABLE_ITEM_SIZE * APP_SHM_GATEWAY_TABLE_ITEM_MAX) 
#define APP_SHM_DEV_TABLE_ITEM_SIZE             (0x400)   // 1k bytes, size for each item in Dev table in share memory
// offset: 44k -- base offset address for items in SensorData table in share memory
#define APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET    (APP_SHM_DEV_TABLE_ADDR_OFFSET + APP_SHM_DEV_TABLE_ITEM_SIZE * APP_SHM_DEV_TABLE_ITEM_MAX) 
// #define APP_SHM_SENSORDATA_TABLE_PIECE_SIZE     (0x200)   // 512 bytes, size for each piece data( 1 record in SensorData table) in share memory
#define APP_SHM_SENSORDATA_TABLE_PIECE_SIZE     (0x400)   // 1k bytes, size for each piece data( 1 record in SensorData table) in share memory

/*
 * Notes:
 *    1. 'APP_SHM_SENSORDATA_TABLE_PIECE_MAX' should be ' >= (max[APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRS, APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRO] + 2)'
 *       as we reserve 2 items for: 
 *          i : reg bit_mapping;
 *          ii: items we don't care(skipped).
 *    2. please properly set this number to make sure the total size of sharememory is 4k(page-size)-aligned
 */
#define APP_SHM_SENSORDATA_TABLE_PIECE_MAX      (0x1c)      // 28, maximum piece number for 1 item/set of sensor data. in SensorData table in share memory

#define APP_SHM_SENSORDATA_TABLE_ITEM_SIZE      (APP_SHM_SENSORDATA_TABLE_PIECE_SIZE * APP_SHM_SENSORDATA_TABLE_PIECE_MAX)   // 4k bytes, size for each item in SensorData table in share memory
#define APP_SHM_MAGIC_DATA_OFFSET               (APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + APP_SHM_SENSORDATA_TABLE_ITEM_SIZE * APP_SHM_SENSORDATA_TABLE_ITEM_MAX) 
#define APP_SHM_MAGIC_DATA_SIZE                 (0x800) // 2k + 2k = 4k
#define APP_SHM_MAGIC_DATA                      "SKF_MAGIC" 
// total size: 1164k (2k + 40k + 1120k + 2k, MUST BE 4k(page-size)-aligned)
#define APP_SHM_TOTAL_SIZE                      (APP_SHM_MAGIC_DATA_OFFSET + APP_SHM_MAGIC_DATA_SIZE) 

// maximum piece number for sensor types
// Notes: DO NOT count the items(say, LAST_VIB_SENSING_TIME, LAST_TEMP_SENSING_TIME, and LAST_TEMP_SENSING_TIME_T) for 'timeStamp' here.
#define APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRS  (25)      // 25, maximum piece number for 1 item/set of sensor data. FOR predict sensor type
#define APP_SHM_SENSORDATA_TABLE_PIECE_MAX_PRO  (25)      // 25, maximum piece number for 1 item/set of sensor data. FOR pro sensor type
#define APP_SHM_SENSORDATA_TABLE_PIECE_MAX_ITS  (4)       // 4, maximum piece number for 1 item/set of sensor data. FOR insight_T sensor type
#define APP_SHM_SENSOR_TIMER_INTERVAL_PRS_S     (131)      // 131S timer for predict sensor
#define APP_SHM_SENSOR_TIMER_INTERVAL_PRO_S     (131)      // 131S timer for pro sensor
#define APP_SHM_SENSOR_TIMER_INTERVAL_ITS_S     (6)       // 6S timer for insight_T sensor

#ifndef MAX_DT_STORATE_FORM_BUF
	#define MAX_DT_STORATE_FORM_BUF           (0x10)
#endif
// index for 'timeStamp' items in bit_mapping
#define SHM_SD_IDX_LAST_VIB_SENSING_TIME      (25)  // -P/-PRO
#define SHM_SD_IDX_LAST_TEMP_SENSING_TIME     (26)  // -P/-PRO
#define SHM_SD_IDX_LAST_TEMP_SENSING_TIME_T   (4)   // insight-T

/* 
 * =================== structures ==============
 */

/*
 * data union for measure data in 'SensorData' table.
 * Notes: sync the data types with those defined in 'data' of ppGW.h file
 */
typedef union {
  char data_char[GW_PRO_MAX_STRING_LEN_BYTE];
  float data_float;
  uint8_t data_uint8;
  int8_t data_int8;
  uint16_t data_uint16;
  int16_t data_int16;
  uint32_t data_uint32;
  int32_t data_int32;
  uint64_t data_uint64;
  int64_t data_int64;
} measure_data_t;

/*
 * data structure for channel and nameid mapping in share memory.
 * Notes: add more items later if necessary
 * TODO: -- maybe need add 1 more attr 'uint8_t flag' into this structure later,
 *       in order to record whether it's already being occupied or not.
 *       -- back it up into file to make sure the ch-nameid mapping is fixed ?? to be confirmed.
 *        or, alternatively, backup items into file on each 'delete' operation only??
 */
typedef struct {
  uint8_t validflag;  // whether the 'dev' is valid or not. for 'delete' operation
  uint8_t ch;   // channel index, 0 is fixed for gateway
  uint32_t nameid;
} chn_nameid_mapping_t;

/*
 * data structure for channel and nameid mapping in share memory for reg address efficient mode.
 * Notes: add more items later if necessary
 */
typedef struct {
  uint8_t validflag;  // whether the 'dev' is valid or not. for 'delete' operation
  uint8_t ch;   // channel index, 0 is fixed for gateway
  uint16_t offset;  // address offset of current channel
  uint32_t nameid;
  uint8_t len;  // the length of reg address: 3(GW), 8(-T), or 48(-P)
} efficient_chn_nameid_mapping_t;

/*
 * data structure for items in 'Dev' table.
 * Notes: delete it if it exists somewhere else
 */
typedef struct _deviceTableData {
  uint32_t nameId;  // A 4-byte hash ID to avoid the same name. nameId is
  char name[GW_PRO_MAX_STRING_LEN_BYTE];  // A readable name of the sensor
  char BLESensorName[GW_PRO_MAX_STRING_LEN_BYTE];  // BLE name of the sensor (not mandatory)
  char macAddr[GW_PRO_MAX_STRING_LEN_BYTE];  // BLE MAC address of the sensor (separated by dashes in JSON).
  char manufacturer[GW_PRO_MAX_STRING_LEN_BYTE];  // The manufacturer of the product
  char type[GW_PRO_MAX_STRING_LEN_BYTE];  // The product name/type
  char description[GW_PRO_MAX_STRING_LEN_BYTE];  // The description field of device in "CH00-AD0000-L00-PRS" format
} deviceTableData_t;

/*
 * data structure for items in 'Gateway' table. re-use gwConfig_t in gwConfig.h file directly here
 * Notes: delete it if it exists somewhere else
 */
typedef gwConfig_t gatewayTableData_t;

/*
 * data structure for items in 'SensorData' table.
 * Notes: delete it if it exists somewhere else
 *        use 'dataType' as index to speed up the convertion in share memory
 *        total 447-byte == sizeof(sensorDataTableData_t) set piece size to 512-byte
 */
typedef struct _sensorDataTableData {
  uint32_t sequenceNumber;  //  1 A 4-byte sequence number as index
  uint32_t nameId;  // 2 A 4-byte hash ID to avoid the same name. nameId is
  char BLESensorName[GW_PRO_MAX_STRING_LEN_BYTE];  // 3 BLE name of the sensor (not mandatory)
  char macAddr[GW_PRO_MAX_STRING_LEN_BYTE];  // 4 to be confirmed. BLE MAC address of the sensor (separated by dashes in JSON).
  char type[GW_PRO_MAX_STRING_LEN_BYTE];  // 5 sensor type
  char manufacturer[GW_PRO_MAX_STRING_LEN_BYTE];  // 6 The manufacturer of the product
  uint32_t measureSeqno;  // 7 measurement sequence number, measure_seq_no in Froto
  uint64_t sampleTime;  // 8 measurement sample time
  uint64_t receivedTimestamp;  // 9 
  SKFChina_Common_AlarmAndNotify measurement;  // 10 measurement in AlarmAndNotifyInfo of Froto
  SKFChina_Common_MeasurementType dataType; // 11 type of measured data
  char format[MAX_DT_STORATE_FORM_BUF]; // 12 2 values: 'Digit' / 'Filename'
  SKFChina_Common_Range range;   // 13 range in Froto
  SKFChina_Common_Unit unit;   // 14 unit in Froto	
  uint32_t measureLengthSample;   // 15 measure_length_sample in Froto
  uint32_t totalDataLengthSample;   // 16 total_data_length_sample in Froto
	uint32_t dimension;			   // 17 dimension in Froto
  SKFChina_Common_Format dataFormat;	   // 18 data_format in Froto
  uint32_t samplePeriods;	   // 19 sample_period_s in Froto
  SKFChina_Common_Encryption encryption;   // 20 encryp in Froto
	measure_data_t value; // 21 for scalar data, ref@data_type to decice the format of vlaue
	float sampleRate;   // 22 sample_rate_hz in Froto
	SKFChina_Common_ProductType product; // 23 Product
	SKFChina_Common_SensorType sensor; // 24 Froto Sensor
  uint8_t sent;  // 25 To indicate whether the data has been sent: 1 for sent; 0 for others.
} sensorDataTableData_t;

/*
 * define data types in different tables. (to be confirmed)
 */
 //typedef enum _appShmDataType {   /* 0-2 used */
 //  app_shm_data_type_gateway = 0,
 //  app_shm_data_type_dev = 1,
 //  app_shm_data_type_sensordata = 2,
 //  app_shm_data_type_max = 3
 //} appShmDataType_t;

/*
 * data structure for items of 'gateway' & 'dev' tables in share memory.
 */
typedef struct _appShmDataItem {   /* 0-2 used */
  //appShmDataType_t appShmDataType;
  uint32_t appShmDataType;
  union {
    deviceTableData_t devTableData;
    gatewayTableData_t gwTableData;
#if (0)
    // comment it out to save share memory. 512-byte per piece of sensor data
    sensorDataTableData_t sensorDataTableData;
#endif
  };
} appShmDataItem_t;

/*
 * register modes for modbus.
 */
typedef enum _regAddressMode {   /* 0-7 used */
  MODBUS_REG_ADDR_EXT_BW_BB_MODE = 0, // 0, extensible mode(big word, big byte)
  MODBUS_REG_ADDR_EFF_BW_BB_MODE,     // 1, efficient mode(big word, big byte)
  MODBUS_REG_ADDR_EXT_BW_LB_MODE,     // 2, extensible mode(big word, little byte)
  MODBUS_REG_ADDR_EFF_BW_LB_MODE,     // 3, efficient mode(big word, little byte)
  MODBUS_REG_ADDR_EXT_LW_BB_MODE,     // 4, extensible mode(little word, big byte)
  MODBUS_REG_ADDR_EFF_LW_BB_MODE,     // 5, efficient mode(little word, big byte)
  MODBUS_REG_ADDR_EXT_LW_LB_MODE,     // 6, extensible mode(little word, little byte)
  MODBUS_REG_ADDR_EFF_LW_LB_MODE,     // 7, efficient mode(little word, little byte)
  MODBUS_REG_ADDR_MAX
} regAddressMode_t;

/*
 * parity mode for modbus.
 */
typedef enum _parityMode {   /* 0-2 used */
  MODBUS_PARITY_MODE_NONE = 0,
  MODBUS_PARITY_MODE_ODD,
  MODBUS_PARITY_MODE_EVEN,
  MODBUS_PARITY_MODE_MAX
} parityMode_t;

/*
 * stop-bit for modbus.
 */
typedef enum _stopBitMode {   /* 0-2 used */
  MODBUS_STOP_1_BIT = 0,
  MODBUS_STOP_15_BIT,
  MODBUS_STOP_2_BIT,
  MODBUS_STOP_MAX
} stopBitMode_t;

/*
 * sensor type in sensor data.
 */
typedef enum _sensorType {   /* 0-2 used */
  MODBUS_SENSOR_TYPE_T = 0,
  MODBUS_SENSOR_TYPE_P,
  MODBUS_SENSOR_TYPE_PRO,
  MODBUS_SENSOR_TYPE_MAX
} sensorType_t;

/*
 * sensor data for items in share memory.
 */
typedef struct _appShmSensorDataInstance {
  uint32_t sd_nameid;
  bool sd_valid_flag;
  uint8_t sd_chn_idx;
  uint8_t sd_piece_cnt;
  uint8_t sd_piece_max;
  sensorType_t sd_sensor_type;
  timer_t sd_timer_id;
#if (1)
  sensorDataTableData_t sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX];
#else
  appShmDataItem_t sd_data_buf[APP_SHM_SENSORDATA_TABLE_PIECE_MAX];
#endif
} appShmSensorDataInstance_t;

/* 
 * =================== variables ==============
 */
extern char g_dev_sensor_name[GW_PRO_MAX_STRING_LEN_BYTE];  // to be confirmed

/* 
 * =================== APIs ==============
 */
/*
 * overview of share memory & semaphore
 * example:
 *      1. on modbus_op view:
 *         key = app_shmem_ftok() 
 *              ||
 *         app_shmem_shmget(key)
 *              ||
 *         app_shmem_sem_open()
 *              ||
 *         app_shmem_shmat(NULL, SHM_RDONLY)
 *              ||
 *         app_shmem_sem_wait()                                 --------
 *              ||                                                      |
 *         read data from share memory('addr' as start address) --------|- repeatable
 *              ||                                                      |
 *         app_shmem_sem_post()                                 --------|
 *              ||
 *         app_shmem_sem_close()
 * 
 *      2. on sql_op view:
 *         key = app_shmem_ftok() 
 *              ||
 *         app_shmem_shmget(key)
 *              ||
 *         app_shmem_sem_open()
 *              ||
 *         app_shmem_shmat(NULL, 0)
 *              ||
 *         app_shmem_sem_wait()                                  --------
 *              ||                                                       |
 *         write data into share memory('addr' as start address) --------|- repeatable
 *              ||                                                       |
 *         app_shmem_sem_post()                                  --------|
 *              ||
 *         app_shmem_sem_close()
 *              ||
 *         app_shmem_shmctl(shmid, IPC_RMID, NULL)
 *              ||
 *         app_shmem_shmdt
 * 
 *   ==================== Test code =====================
 * 
 * Here are key test codes for verification:
 * // TODO: fetch the current data in db into share memory first
 * int shm_id = 0;
 * char test_str_gw[] = "hello gateway!";
 * char test_str_dev[] = "hello device list!";
 * char test_str_sd[] = "hello sensor data!";
 * char test_read_buf[48] = {0};
 * t_shm_key = app_shmem_ftok();
 * if(-1 == t_shm_key) {
 *   printf("generate key failed!\r\n");
 *   return 1;
 * }
 * if(-1 == app_shmem_shmget(t_shm_key)) {
 *   printf("get share memory failed!\r\n");
 *   return 1;
 * }
 * if(-1 == app_shmem_sem_open()) {
 *   printf("open named semaphore failed!\r\n");
 *   return 1;
 * }
 *#ifdef SHM_DEBUG
 * printf("+=%d=+\r\n",cnt++);
 *#endif
 *
 * t_shm_addr = app_shmem_shmat(NULL, 0); // to be confirmed
 * if(NULL == t_shm_addr) {
 *   app_shmem_sem_close();
 *   LOG_WARN(OUTPOINT, "map share memory error!\r\n");
 *   return 1;
 * }
 *#ifdef SHM_DEBUG
 * printf("+=%d=+\r\n",cnt++);
 *#endif
 * // cleanup once it's re-started
 * memset(t_shm_addr, 0, APP_SHM_TOTAL_SIZE);
 * app_shmem_sem_post();
 *
 *#ifdef SHM_DEBUG
 * printf("+=%d=+\r\n",cnt++);
 * memset(test_read_buf, 0, 48);
 * printf("testing share memory: gateway area! \r\n");
 * app_shmem_sem_wait();
 * memcpy((void *)(t_shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), (void *)test_str_gw, sizeof(test_str_gw));
 * app_shmem_sem_post();
 * printf("+=%d=+\r\n",cnt++);
 * app_shmem_sem_wait();
 * memcpy((void *)test_read_buf, (void *)(t_shm_addr + APP_SHM_GATEWAY_TABLE_ADDR_OFFSET), sizeof(test_read_buf));
 * app_shmem_sem_post();
 * printf("string read: %s\r\n", test_read_buf);
 *
 * system("ipcs -a");
 *
 * memset(test_read_buf, 0, 48);
 * memset(t_shm_addr, 0, APP_SHM_TOTAL_SIZE);
 * printf("testing share memory: device list area! \r\n");
 * app_shmem_sem_wait();
 * memcpy((void *)(t_shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + 2 * APP_SHM_DEV_TABLE_ITEM_SIZE), (void *)test_str_dev, sizeof(test_str_dev));
 * app_shmem_sem_post();
 * printf("+=%d=+\r\n",cnt++);
 * app_shmem_sem_wait();
 * memcpy((void *)test_read_buf, (void *)(t_shm_addr + APP_SHM_DEV_TABLE_ADDR_OFFSET + 2 * APP_SHM_DEV_TABLE_ITEM_SIZE), sizeof(test_read_buf));
 * app_shmem_sem_post();
 * printf("string read: %s\r\n", test_read_buf);
 *
 * system("ipcs");
 *
 * memset(test_read_buf, 0, 48);
 * memset(t_shm_addr, 0, APP_SHM_TOTAL_SIZE);
 * printf("testing share memory: sensor data area! \r\n");
 * app_shmem_sem_wait();
 * memcpy((void *)(t_shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + 2 * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + 7 * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE), (void *)test_str_sd, sizeof(test_str_sd));
 * app_shmem_sem_post();
 * printf("+=%d=+\r\n",cnt++);
 * app_shmem_sem_wait();
 * memcpy((void *)test_read_buf, (void *)(t_shm_addr + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + 2 * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + 7 * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE), sizeof(test_read_buf));
 * app_shmem_sem_post();
 * printf("string read: %s\r\n", test_read_buf);
 *#endif
 *
 * app_shmem_sem_close();
 * app_shmem_shmctl(IPC_RMID, NULL);
 * app_shmem_shmdt(t_shm_addr);
 *
 *   ==================== Key log =====================
 * 
 * root@OK62xx:~/skf_gw_mqtt/test# ./shmem 
 * [Info:2024-12-03 13:40:40]file(/home/forlinx/bingliang/bullet_gw/gw_shm/GW/skf_gw_mqtt/test_shm/main.c),line(382): start sqlite!
 * [Debug:2024-12-03 13:40:40]file(sh_mem.c),line(41): shm create key:1126826019!
 * [Debug:2024-12-03 13:40:40]file(sh_mem.c),line(134): shm allocated:7!
 * [Debug:2024-12-03 13:40:40]file(sh_mem.c),line(73): create named sem pass!
 * +=0=+
 * +=1=+
 * +=2=+
 * testing share memory: gateway area! 
 * +=3=+
 * string read: hello gateway!
 *
 * ------ Message Queues --------
 * key        msqid      owner      perms      used-bytes   messages    
 * 0x00000013 0          root       777        0            0           
 * 0x00000012 1          root       777        0            0           
 * 0x00000014 2          root       777        0            0           
 * 0x422a002a 3          root       777        0            0           
 * 0x422a002b 4          root       777        0            0           
 *
 * ------ Shared Memory Segments --------
 * key        shmid      owner      perms      bytes      nattch     status      
 * 0x432a0023 7          root       0          905216     1                       
 *
 * ------ Semaphore Arrays --------
 * key        semid      owner      perms      nsems     
 *
 * testing share memory: device list area! 
 * +=4=+
 * string read: hello device list!
 *
 * ------ Message Queues --------
 * key        msqid      owner      perms      used-bytes   messages    
 * 0x00000013 0          root       777        0            0           
 * 0x00000012 1          root       777        0            0           
 * 0x00000014 2          root       777        0            0           
 * 0x422a002a 3          root       777        0            0           
 * 0x422a002b 4          root       777        0            0           
 *
 * ------ Shared Memory Segments --------
 * key        shmid      owner      perms      bytes      nattch     status      
 * 0x432a0023 7          root       0          905216     1                       
 *
 * ------ Semaphore Arrays --------
 * key        semid      owner      perms      nsems     
 *
 * testing share memory: sensor data area! 
 * +=5=+
 * string read: hello sensor data!
 * +=6=+
 * root@OK62xx:~/skf_gw_mqtt/test# 
 * 
 *   ==================== End of log =====================
 *
 * Notes: 
 *      1. use named semaphore in order to 'share' it with other processes.
 *      2. use system V style shm APIs to improve the performance of accessing data
 *      3. all the processes would have the same key(based on the same pathname and proj_id) to 
 *         share the same named semaphore
 */
key_t app_shmem_ftok(void);   // skip pathname and proj_id parameter here(compared with ftok()).

int app_shmem_sem_open(void);     // skip name, oflag, mode, and value parameters here(compared with sem_open()).

int app_shmem_sem_post(void);   // skip sem parameter here(compared with sem_post()).

/**
 * @brief lock a semaphore(sem_wait())
 * @return  : on success, return 0
 *            on failure -1 is returned with <errno> set 
 */
int app_shmem_sem_wait(void);   // skip sem parameter here(compared with sem_wait()).
/**
 * @brief lock a semaphore in trywait way(sem_trywait())
 * @return  : on success, return 0
 *            on failure -1 is returned with <errno> set to EAGAIN 
 */
int app_shmem_sem_trywait(void);   // skip sem parameter here(compared with sem_trywait()).

int app_shmem_sem_close(void);   // skip sem parameter here(compared with sem_close()).

int app_shmem_shmget(key_t shm_key);   // skip size and flag parameters here(compared with shmget()).

/* 
 * Notes(to be confirmed):
 * @shmflag: SHM_RDONLY -- Attach the segment for read-only access. If this flag is not specified,
 *                         the segment is attached for read and write access, and the process must 
 *                         have read and write permission for the segment.
 *           for consumer(modbus_op): app_shmem_shmat(address, SHM_RDONLY);
 *           for producer(sql_op):    app_shmem_shmat(address, NULL);
 */
void *app_shmem_shmat(void *addr, int shmflag);  // skip size parameter here(compared with shmat()).

/* 
 * Notes(to be confirmed):
 * @cmd: IPC_RMID -- The caller must ensure that a segment is eventually destroyed;  other‐
 *                   wise its pages that were faulted in will remain in memory or swap.
 *                  refer to /proc/sys/kernel/shm_rmid_forced in proc(5) later if necessary
 *           for caller: app_shmem_shmctl(shmid, IPC_RMID, NULL);
 */
int app_shmem_shmctl(int cmd, struct shmid_ds *buf);  // skip shmid parameter here (compared with shmdtctl()).  

int app_shmem_shmdt(void *addr);  // exactly same as shmdt().

#endif  // end of SH_MEM_H
