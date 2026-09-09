/**
 * @file    util_dbg.h
 * @author  Victor Yan
 * @date    2024-05-25
 * @brief   MACROs and delecartions of logs
 * @details
 */
#ifndef __UTIL_DBG_H__
#define __UTIL_DBG_H__
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "app_config.h"


void util_dbg_get_curr_time(
  char *pt_buf,
  uint8_t buf_len);

/**
 * logging policy selection:
 * 1. at developing phase, we prefer to control logging at file/component level;
 * 2. at release phase, we prefer to control logging at project level and control DEBUG & INFO log at file level.
 */
#define CONFIG_DBG_LOG_DEVELOP  (1)
#define CONFIG_DBG_LOG_RELEASE  (2)
#ifndef CONFIG_DBG_LOG_SET
// #define CONFIG_DBG_LOG_SET  CONFIG_DBG_LOG_DEVELOP
#define CONFIG_DBG_LOG_SET  CONFIG_DBG_LOG_RELEASE
#endif

#if (CONFIG_DBG_LOG_SET == CONFIG_DBG_LOG_DEVELOP)  // for developing
// Logging configurations
#ifndef DEBUG_OPEN_CONF
#define DEBUG_OPEN (1)
#else
#define DEBUG_OPEN DEBUG_OPEN_CONF
#endif
#ifndef INFO_OPEN_CONF
#define INFO_OPEN (1)
#else
#define INFO_OPEN INFO_OPEN_CONF
#endif
#ifndef WARN_OPEN_CONF
#define WARN_OPEN (1)
#else
#define WARN_OPEN WARN_OPEN_CONF
#endif
#ifndef ERR_OPEN_CONF
#define ERR_OPEN (1)
#else
#define ERR_OPEN ERR_OPEN_CONF
#endif
#ifndef OUTPOINT_CONF
#define OUTPOINT stderr
#else
#define OUTPOINT OUTPOINT_CONF
#endif

// NOTICE: you have to put one of the following two MACROs in each *.c file
// to enable or disable the log output in each specific file
#ifndef DBG_LOCAL_LOG_ENABLE
#define DBG_LOCAL_LOG_ENABLE const static uint8_t l_log_ct = 1;	
#endif
 
#ifndef DBG_LOCAL_LOG_DISABLE
#define DBG_LOCAL_LOG_DISABLE	const static uint8_t l_log_ct = 0;
#endif

#define LOG_OUTPUT(OUTPOINT, ...) printf(__VA_ARGS__)

#if (1)  // Better visualized but not support logging to a file
#define BUFF_TIME_LEN 30

#define UTIL_LOG_DEBUG(OUTPOINT, ...) \
  if(DEBUG_OPEN && l_log_ct) { \
    char buff[BUFF_TIME_LEN] = {0}; \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN); \
    printf("\033[0;33m[Debug:%s]file(%s),line(%d): ", buff, strrchr(__FILE__, '/'), __LINE__); \
    printf(__VA_ARGS__); \
    printf("\r\n\033[0;0m"); \
  }
#define UTIL_LOG_DEBUG_RAW(OUTPOINT, ...) \
  if(DEBUG_OPEN && l_log_ct) { \
    printf(__VA_ARGS__); \
  }
#define UTIL_LOG_INFO(OUTPOINT, ...) \
  if(INFO_OPEN && l_log_ct) { \
    char buff[BUFF_TIME_LEN] = {0}; \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN); \
    printf("\033[0;36m[Info:%s]file(%s),line(%d):\033[0;0m ", buff, strrchr(__FILE__, '/'), __LINE__); \
    printf(__VA_ARGS__); \
    printf("\r\n\033[0;0m"); \
  }
#define UTIL_LOG_INFO_RAW(OUTPOINT, ...) \
  if(INFO_OPEN && l_log_ct) { \
    printf(__VA_ARGS__); \
  }
#define UTIL_LOG_WARN(OUTPOINT, ...) \
  if(WARN_OPEN && l_log_ct) { \
    char buff[BUFF_TIME_LEN] = {0}; \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN); \
    printf("\033[0;32m[Warn:%s]file(%s),line(%d): ", buff, strrchr(__FILE__, '/'), __LINE__); \
    printf(__VA_ARGS__); \
    printf("\r\n\033[0;0m"); \
  }
#define UTIL_LOG_WARN_RAW(OUTPOINT, ...) \
  if(WARN_OPEN && l_log_ct) { \
    printf(__VA_ARGS__); \
  }
#define UTIL_LOG_ERR(OUTPOINT, ...) \
  if(ERR_OPEN && l_log_ct) { \
    char buff[BUFF_TIME_LEN] = {0}; \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN); \
    printf("\033[0;31m[Err:%s]file(%s),line(%d): ", buff, strrchr(__FILE__, '/'), __LINE__); \
    printf(__VA_ARGS__); \
    printf("\r\n\033[0;0m"); \
  }
#define UTIL_LOG_ERR_RAW(OUTPOINT, ...) \
  if(ERR_OPEN && l_log_ct) { \
    printf(__VA_ARGS__); \
  }

#else // Support logging to a file

#define LOG_DEBUG(OUTPOINT, ...) \
  if(DEBUG_OPEN) { \
    char buff[BUFF_TIME_LEN] = {0}; \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN); \
    fprintf(OUTPOINT, "[Debug:%s]file(%s),line(%d): ", buff, strrchr(__FILE__, '/'), __LINE__); \
    fprintf(OUTPOINT, __VA_ARGS__); \
    fprintf("\r\n"); \
  }
#define LOG_DEBUG_RAW(OUTPOINT, ...) \
  if(DEBUG_OPEN) { \
    fprintf(OUTPOINT, __VA_ARGS__); \
  }
#define LOG_INFO(OUTPOINT, ...) \
  if(INFO_OPEN) { \
    char buff[BUFF_TIME_LEN] = {0}; \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN); \
    fprintf(OUTPOINT, "[Info:%s]file(%s),line(%d): ", buff, strrchr(__FILE__, '/'), __LINE__); \
    fprintf(OUTPOINT, __VA_ARGS__); \
    fprintf("\r\n"); \
  }
#define LOG_INFO_RAW(OUTPOINT, ...) \
  if(INFO_OPEN) { \
    fprintf(OUTPOINT, __VA_ARGS__); \
  }
#define LOG_WARN(OUTPOINT, ...) \
  if(WARN_OPEN) { \
    char buff[BUFF_TIME_LEN] = {0}; \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN); \
    fprintf(OUTPOINT, "[Warn:%s]file(%s),line(%d): ", buff, strrchr(__FILE__, '/'), __LINE__); \
    fprintf(OUTPOINT, __VA_ARGS__); \
    fprintf("\r\n"); \
  }
#define LOG_WARN_RAW(OUTPOINT, ...) \
  if(WARN_OPEN) { \
    fprintf(OUTPOINT, __VA_ARGS__); \
  }
#define LOG_ERR(OUTPOINT, ...) \
  if(ERR_OPEN) { \
    char buff[BUFF_TIME_LEN] = {0}; \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN); \
    fprintf(OUTPOINT, "[Err:%s]file(%s),line(%d): ", buff, strrchr(__FILE__, '/'), __LINE__); \
    fprintf(OUTPOINT, __VA_ARGS__); \
    fprintf("\r\n"); \
  }
#define LOG_ERR_RAW(OUTPOINT, ...) \
  if(ERR_OPEN) { \
    fprintf(OUTPOINT, __VA_ARGS__); \
  }

#endif
#endif  /* CONFIG_DBG_LOG_SET == CONFIG_DBG_LOG_DEVELOP*/

#if (CONFIG_DBG_LOG_SET == CONFIG_DBG_LOG_RELEASE)  // for release
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
#define UTIL_LOG_DEBUG(OUTPOINT, ...)                         \
  do {                                                        \
    if((g_log_level < CONFIG_LOG_LEVEL_DEBUG) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_DEBUG))) break; \
    char buff[BUFF_TIME_LEN] = {0};                           \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN);              \
    printf("\033[0;33m[Debug:%s]file(%s),line(%d): ", buff, (strchr(__FILE__, '/') ? strrchr(__FILE__, '/') : __FILE__), __LINE__); \
    printf(__VA_ARGS__);                                      \
    printf("\r\n\033[0;0m");                                  \
  } while (0);
#define UTIL_LOG_DEBUG_RAW(OUTPOINT, ...)                     \
  do {                                                        \
    if((g_log_level < CONFIG_LOG_LEVEL_DEBUG) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_DEBUG))) break;  \
    printf(__VA_ARGS__);                                      \
  } while (0);
#define UTIL_LOG_INFO(OUTPOINT, ...)                          \
  do {                                                        \
    if((g_log_level < CONFIG_LOG_LEVEL_INFO) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_INFO))) break; \
    char buff[BUFF_TIME_LEN] = {0};                           \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN);              \
    printf("\033[0;36m[Info:%s]file(%s),line(%d): ", buff, (strchr(__FILE__, '/') ? strrchr(__FILE__, '/') : __FILE__), __LINE__); \
    printf(__VA_ARGS__);                                      \
    printf("\r\n\033[0;0m");                                  \
  } while (0);
#define UTIL_LOG_INFO_RAW(OUTPOINT, ...)                      \
  do {                                                        \
    if((g_log_level < CONFIG_LOG_LEVEL_INFO) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_INFO))) break;   \
    printf(__VA_ARGS__);                                      \
  } while (0);
#define UTIL_LOG_WARN(OUTPOINT, ...)                          \
  do {                                                        \
    if((g_log_level < CONFIG_LOG_LEVEL_WARN) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_WARN))) break;   \
    char buff[BUFF_TIME_LEN] = {0};                           \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN);              \
    printf("\033[0;32m[Warn:%s]file(%s),line(%d): ", buff, (strchr(__FILE__, '/') ? strrchr(__FILE__, '/') : __FILE__), __LINE__); \
    printf(__VA_ARGS__);                                      \
    printf("\r\n\033[0;0m");                                  \
  } while (0);
#define UTIL_LOG_WARN_RAW(OUTPOINT, ...)                      \
  do {                                                        \
    if((g_log_level < CONFIG_LOG_LEVEL_WARN) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_WARN))) break;   \
    printf(__VA_ARGS__);                                      \
  } while (0);
#define UTIL_LOG_ERR(OUTPOINT, ...)                           \
  do {                                                        \
    if((g_log_level < CONFIG_LOG_LEVEL_ERROR) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_ERROR))) break;  \
    char buff[BUFF_TIME_LEN] = {0};                           \
    util_dbg_get_curr_time(buff, BUFF_TIME_LEN);              \
    printf("\033[0;31m[Err:%s]file(%s),line(%d): ", buff, (strchr(__FILE__, '/') ? strrchr(__FILE__, '/') : __FILE__), __LINE__); \
    printf(__VA_ARGS__);                                      \
    printf("\r\n\033[0;0m");                                  \
  } while (0);
#define UTIL_LOG_ERR_RAW(OUTPOINT, ...)                       \
  do {                                                        \
    if((g_log_level < CONFIG_LOG_LEVEL_ERROR) || ((g_log_level > l_log_lev) && (l_log_lev < CONFIG_LOG_LEVEL_ERROR))) break;  \
    printf(__VA_ARGS__);                                      \
  } while (0);
#endif  /* CONFIG_DBG_LOG_SET == CONFIG_DBG_LOG_RELEASE*/

#if (DBG_CT == 1)
#define DBG_LOG_DEBUG(__fmt, ...) UTIL_LOG_DEBUG(OUTPOINT, __fmt,##__VA_ARGS__)
#define DBG_LOG_DEBUG_RAW(__fmt, ...) UTIL_LOG_DEBUG_RAW(OUTPOINT, __fmt, \
                                                    ##__VA_ARGS__)
#define DBG_LOG_INFO(__fmt, ...) UTIL_LOG_INFO(OUTPOINT, __fmt,##__VA_ARGS__)
#define DBG_LOG_INFO_RAW(__fmt, ...) UTIL_LOG_INFO_RAW(OUTPOINT, __fmt, \
                                                  ##__VA_ARGS__)
#define DBG_LOG_WARN(__fmt, ...) UTIL_LOG_WARN(OUTPOINT, __fmt,##__VA_ARGS__)
#define DBG_LOG_WARN_RAW(__fmt, ...) UTIL_LOG_WARN_RAW(OUTPOINT, __fmt, \
                                                  ##__VA_ARGS__)
#define DBG_LOG_ERR(__fmt, ...) UTIL_LOG_ERR(OUTPOINT, __fmt,##__VA_ARGS__)
#define DBG_LOG_ERR_RAW(__fmt, ...) UTIL_LOG_ERR_RAW(OUTPOINT, __fmt, \
                                                ##__VA_ARGS__)
extern char *g_pt_buf;  // FIXME: Deprecated
extern bool g_flg;  // FIXME: Deprecated

struct froto_msg
{
  bool flg;
  uint32_t len;
  uint8_t buf[512];
};
extern struct froto_msg g_froto_encoded_msg;

#ifndef DBG_SENSOR_ID_STR
#define DBG_SENSOR_ID_STR "C4BD6A100001"  // this is a temperory solution
#endif

void util_dbg_buf_dump(
  uint8_t const *pt_buf,
  uint32_t kplen);
bool util_dbg_fill_froto_msg(
  struct froto_msg *pt_msg,
  uint8_t const *pt_buf,
  uint32_t kplen);
void util_dbg_release_froto_msg(struct froto_msg *pt_msg);
void util_dbg_copy_file(
  const char *spath,
  const char *dpath);

#else
#define DBG_LOG_DEBUG(__fmt, ...)
#define DBG_LOG_DEBUG_RAW(__fmt, ...)
#define DBG_LOG_INFO(__fmt, ...)
#define DBG_LOG_INFO_RAW(__fmt, ...)
#define DBG_LOG_WARN(__fmt, ...)
#define DBG_LOG_WARN_RAW(__fmt, ...)
#define DBG_LOG_ERR(__fmt, ...)
#define DBG_LOG_ERR_RAW(__fmt, ...)
#endif

#endif /* __UTIL_DBG_H__ */
