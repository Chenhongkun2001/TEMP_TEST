#ifndef GLOBAL_H
#define GLOBAL_H
// #include "createtable.h"
#include "ppGW.h"
#include "stdint.h"
#include "stdio.h"
#include "time.h"
#include <auxiliar.h>
#include <semaphore.h>
#define INFO_OPEN 1
#define WARN_OPEN 1
#define DEBUG_OPEN 1
#define ERR_OPEN 1
#define OUTPOINT stderr
#define BUFF_TIME_LEN 30
#define LOG_OUTPUT(OUTPOINT, ...) printf(__VA_ARGS__)

// #define DBG_LOCAL_LOG_ENABLE  // skip it as it's not necessary for release version
/* log level definition */
#define CONFIG_LOG_LEVEL_ERROR 0
#define CONFIG_LOG_LEVEL_WARN  1
#define CONFIG_LOG_LEVEL_INFO  2
#define CONFIG_LOG_LEVEL_DEBUG 3
#define CONFIG_LOG_LEVEL_ALL   4
/* log level setting, default: CONFIG_LOG_LEVEL_INFO */
#ifndef CONFIG_LOG_LEVEL_SET
#define CONFIG_LOG_LEVEL_SET CONFIG_LOG_LEVEL_INFO
#endif
#if (0) // for origianl modbus
#ifndef DBG_LOCAL_LOG_ENABLE
#define DBG_LOCAL_LOG_ENABLE const static uint8_t l_log_ct = 1;	
#endif
 
#ifndef DBG_LOCAL_LOG_DISABLE
#define DBG_LOCAL_LOG_DISABLE	const static uint8_t l_log_ct = 0;
#endif
#endif
// Notes: you may put one of the following 4 MACROs in each *.c file
// to set log output level in each specific file
#ifndef DBG_LOCAL_LOG_ERROR
#define DBG_LOCAL_LOG_ERROR const static uint8_t l_log_lev = CONFIG_LOG_LEVEL_ERROR;	
#endif 
#ifndef DBG_LOCAL_LOG_WARN
#define DBG_LOCAL_LOG_WARN	const static uint8_t l_log_lev = CONFIG_LOG_LEVEL_WARN;
#endif
#ifndef DBG_LOCAL_LOG_INFO
#define DBG_LOCAL_LOG_INFO const static uint8_t l_log_lev  = CONFIG_LOG_LEVEL_INFO;	
#endif 
#ifndef DBG_LOCAL_LOG_DEBUG
#define DBG_LOCAL_LOG_DEBUG	const static uint8_t l_log_lev = CONFIG_LOG_LEVEL_DEBUG;
#endif
extern uint8_t g_log_level;

#define BUFF_TIME_LEN 30
#define LOG_DEBUG(OUTPOINT, ...)                                                                                                      \
  do {                                                                                                                                \
    if((g_log_level < CONFIG_LOG_LEVEL_DEBUG) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_DEBUG))) break;          \
    char buff[BUFF_TIME_LEN] = {0};                                                                                                   \
    get_curr_time(buff, BUFF_TIME_LEN);                                                                                               \
    printf("\033[0;33m[Debug:%s]file(%s),line(%d): ", buff, (strchr(__FILE__, '/') ? strrchr(__FILE__, '/') : __FILE__), __LINE__);   \
    printf(__VA_ARGS__);                                                                                                              \
    printf("\r\n\033[0;0m");                                                                                                          \
  } while (0);
#define LOG_INFO(OUTPOINT, ...)                                                                                                       \
  do {                                                                                                                                \
    if((g_log_level < CONFIG_LOG_LEVEL_INFO) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_INFO))) break;            \
    char buff[BUFF_TIME_LEN] = {0};                                                                                                   \
    get_curr_time(buff, BUFF_TIME_LEN);                                                                                               \
    printf("\033[0;36m[Info:%s]file(%s),line(%d):", buff, (strchr(__FILE__, '/') ? strrchr(__FILE__, '/') : __FILE__), __LINE__);     \
    printf(__VA_ARGS__);                                                                                                              \
    printf("\r\n\033[0;0m");                                                                                                          \
  } while (0);
#define LOG_WARN(OUTPOINT, ...)                                                                                                       \
  do {                                                                                                                                \
    if((g_log_level < CONFIG_LOG_LEVEL_WARN) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_WARN))) break;            \
    char buff[BUFF_TIME_LEN] = {0};                                                                                                   \
    get_curr_time(buff, BUFF_TIME_LEN);                                                                                               \
    printf("\033[0;32m[Warn:%s]file(%s),line(%d): ", buff, (strchr(__FILE__, '/') ? strrchr(__FILE__, '/') : __FILE__), __LINE__);    \
    printf(__VA_ARGS__);                                                                                                              \
    printf("\r\n\033[0;0m");                                                                                                          \
  } while (0);
#define LOG_ERR(OUTPOINT, ...)                                                                                                        \
  do {                                                                                                                                \
    if((g_log_level < CONFIG_LOG_LEVEL_ERROR) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_ERROR))) break;          \
    char buff[BUFF_TIME_LEN] = {0};                                                                                                   \
    get_curr_time(buff, BUFF_TIME_LEN);                                                                                               \
    printf("\033[0;31m[Err:%s]file(%s),line(%d): ", buff, (strchr(__FILE__, '/') ? strrchr(__FILE__, '/') : __FILE__), __LINE__);     \
    printf(__VA_ARGS__);                                                                                                              \
    printf("\r\n\033[0;0m");                                                                                                          \
  } while (0);

#if 0
#if 0
#define LOG_INFO(OUTPOINT, ...)                                                \
  if (INFO_OPEN) {                                                             \
    char buff[BUFF_TIME_LEN] = {0};                                            \
    get_curr_time(buff, BUFF_TIME_LEN);                                        \
    printf("\033[0;36m[Info:%s]file(%s),line(%d):\033[0;0m ", buff, __FILE__,  \
           __LINE__);                                                          \
    printf(__VA_ARGS__);                                                       \
  }

#define LOG_WARN(OUTPOINT, ...)                                                \
  if (WARN_OPEN) {                                                             \
    char buff[BUFF_TIME_LEN] = {0};                                            \
    get_curr_time(buff, BUFF_TIME_LEN);                                        \
    printf("\033[0;32m[Warn:%s]file(%s),line(%d): ", buff, __FILE__,           \
           __LINE__);                                                          \
    printf(__VA_ARGS__);                                                       \
    printf("\033[0;0m");                                                       \
  }
#define LOG_ERR(OUTPOINT, ...)                                                 \
  if (ERR_OPEN) {                                                              \
    char buff[BUFF_TIME_LEN] = {0};                                            \
    get_curr_time(buff, BUFF_TIME_LEN);                                        \
    printf("\033[0;31m[Err:%s]file(%s),line(%d): ", buff, __FILE__, __LINE__); \
    printf(__VA_ARGS__);                                                       \
    printf("\033[0;0m");                                                       \
  }
#define LOG_DEBUG(OUTPOINT, ...)                                               \
  if (DEBUG_OPEN) {                                                            \
    char buff[BUFF_TIME_LEN] = {0};                                            \
    get_curr_time(buff, BUFF_TIME_LEN);                                        \
    printf("\033[0;33m[Debug:%s]file(%s),line(%d): ", buff, __FILE__,          \
           __LINE__);                                                          \
    printf(__VA_ARGS__);                                                       \
    printf("\033[0;0m");                                                       \
  }
#else
#define LOG_INFO(OUTPOINT, ...)                                                \
  if (INFO_OPEN) {                                                             \
    fprintf(OUTPOINT, "[Info]file(%s),line(%d): ", __FILE__, __LINE__);        \
    fprintf(OUTPOINT, __VA_ARGS__);                                            \
  }
#define LOG_WARN(OUTPOINT, ...)                                                \
  if (WARN_OPEN) {                                                             \
    fprintf(OUTPOINT, "[Warn]file(%s),line(%d): ", __FILE__, __LINE__);        \
    fprintf(OUTPOINT, __VA_ARGS__);                                            \
  }
#define LOG_ERR(OUTPOINT, ...)                                                 \
  if (ERR_OPEN) {                                                              \
    fprintf(OUTPOINT, "[Err]file(%s),line(%d): ", __FILE__, __LINE__);         \
    fprintf(OUTPOINT, __VA_ARGS__);                                            \
  }
#define LOG_DEBUG(OUTPOINT, ...)                                               \
  if (DEBUG_OPEN) {                                                            \
    fprintf(OUTPOINT, "[Debug]file(%s),line(%d): ", __FILE__, __LINE__);       \
    fprintf(OUTPOINT, __VA_ARGS__);                                            \
  }
#endif

#endif

// #define TEST_MAC
#define CONFIG_REPORT_TEST
#define CONFIG_REPORT_FALG
#define GATEWAY_CONFIGURE_TABLE "Gateway"
#define DEVICE_LIST_TABLE "Dev"
#define SENSOR_CONFIGURE_TABLE "Sensor"
#define SENSOR_DATA_TABLE "SensorData"

#define SKF_GW_DATA_DIR "/var/lib/skf-gateway"
#define SKF_GW_CONFIG_DIR SKF_GW_DATA_DIR "/config"
#define SKF_GW_SENSOR_DIR SKF_GW_DATA_DIR "/data"
#define SKF_GW_UPDATE_DIR SKF_GW_DATA_DIR "/update"

#define SKF_GW_RUN_DIR "/run/skf-gateway"
#define SKF_GW_IPC_DIR SKF_GW_RUN_DIR "/ipc"
#define DATABASE_NAME SKF_GW_DATA_DIR "/skf.db"

// #define DATABASE_NAME SKF_GW_DATA_DIR "/var/lib/skf-gateway/data/skf.db"

#define MSG_BUF_LEN 8192
#define SENSOR_DATA_NUM 64
#define SUB_TOPIC_NUM 8
#define MAX_CHILD_NUM 20
#define MAX_SEND_TIME 3
#define MAX_REPUB_NUM 3
#define MAX_UPLOAD_ARGS 6
#define MAX_DOWNLOAD_ARGS 5
#define DATAPAIR_MAX_LEN 2048
#define CONFIG_JSON_MAX_LEN 20480
#define COVER_TO_BEIJING_TIME 28800

#define GATEWAY_CONFIG_FILE_NAME "gwConfig.json"
#define SENSOR_CONFIG_FILE_NAME "sensorConfig.json"
#define MAGIC_FILE_NAME "magicFile.json"
#define OTHERS_FILE_NAME "others.json"
#define GATEWAY_FIRM_FILE_NAME "gateway.tar.gz"
#define SENSOR_FIRM_FILE_NAME "sensor.bin"

#ifndef PATH_MFILE_DEFAULT
#define PATH_MFILE_DEFAULT SKF_GW_CONFIG_DIR "magicFile.json"
#endif
#ifndef PATH_MFILE_USER
#define PATH_MFILE_USER	SKF_GW_CONFIG_DIR "magicFile.json.user"
#endif

#define VIBRATION_ACC_WAVE_FILE "vaw_file.txt"
#define VIBRATION_ENV3_WAVE_FILE "ve3w_file.txt"
#define VIBRATION_VELOCITY_WAVE_FILE "vvw_file.txt"
#define MSG_SQL_QUEUE_KEY (0x12)
#define MSG_MQTT_QUEUE_KEY (0x13)
#define MSG_BLE_QUEUE_KEY (0x14)
#define MSG_QUEUE_FLAG (0600) 
// mqtt_op/sql_op/skf_gw_1 run as the same dedicated user, so the msg queue can be shared by them
#define DEFAULT_CONFIG_FILE_CONTENT                                            \
  "{\"lastEditTime\": -1,\"macAddr\": \"%s\",\"bulletSensorConfig\": "         \
  "{\"sysConfig\": {},\"scheduleConfig\": {},\"sensingConfig\": "              \
  "{},\"algorithmConfig\": {}}}"

// #define CONFIG_HTTP_UPLOAD_FILE

#define CONFIG_HARDWARE_POC_VERSION "0.1.0" /* string, must be x.x.x format */
#define CONFIG_HARDWARE_DV_VERSION "0.2.0"  /* string, must be x.x.x format */
#define CONFIG_HARDWARE_DEFAULT_VERSION CONFIG_HARDWARE_DV_VERSION
/* application version */
#define CONFIG_APPLICATION_VERSION "1.0.1" /* string, must be x.x.x format for test version*/
// #define CONFIG_APPLICATION_VERSION "2.0.1" /* string, must be x.x.x format */

struct msgbuf {
  long mtype;              // the receiver
  char mtext[MSG_BUF_LEN]; /* message data */
};

typedef enum _msgtype {
  gw_pro_command = 1,
  gw_pro_response,
} gw_msg_type_t;

typedef struct _reportrx {
  uint32_t tbname;
  uint32_t Idx;
} reportrx_t;

typedef int (*sqlite_query_callback)(void *, int, char **, char **);
// enum _msgtype {
//   // sqlite_insert_msgtype = 1,
//   sqlite_update_msgtype = 1,
//   sqlite_delete_msgtype,

//   sqlite_query_msgtype,
//   sqlite_unknown_msgtype,
// };
// enum _jsonmsgtype {
//   json_data_msgtype = 1,
//   json_config_msgtype,
//   json_unknown_msgtype,
// };

// enum _processtype {
//   process_ble = 1,
//   process_mqtt,
//   process_sql,
//   process_unknown,
// };

typedef enum _jsonfile {
  Unknown_file = 0,
  Gw_confiog_file,
  Sensor_confiog_file,
} jsonfile_type_t;

extern char topic_clientID[50];
extern char topic_device_type[50];
extern char clientID[50];
// extern char child_clientID[MAX_CHILD_NUM][50];
extern char upfilename[100];
// extern char *downargs[MAX_DOWNLOAD_ARGS];
// extern char *upargs[MAX_UPLOAD_ARGS];
extern sem_t sem;
extern uint8_t upwave_type;
#endif
