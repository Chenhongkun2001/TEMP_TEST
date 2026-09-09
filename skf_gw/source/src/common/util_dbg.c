/**
 * @file    util_dbg.c
 * @author  Victor Yan
 * @date    2024-05-25
 * @brief   Implementations of message dump
 * @details
 */
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include "app_config.h"
#include "sensorConfig.h"
#include "util_dbg.h"
#include <unistd.h>

DBG_LOCAL_LOG_DEBUG


#if (DBG_CT == 1)

char *g_pt_buf = NULL; // FIXME: Deprecated
bool g_flg = false; // FIXME: Deprecated

struct froto_msg g_froto_encoded_msg = { 0 };
sensorConfig_t g_dbg_sensor_conf = { 0 };

#define COVER_TO_BEIJING_TIME 28800
/**
 * @brief To output current time.
 * @param pt_buf: The pointer to the buffer
 * @param buf_len: The length of the buffer (in byte)
 */
void
util_dbg_get_curr_time(
  char *pt_buf,
  uint8_t buf_len) {
  time_t t = time(NULL) + COVER_TO_BEIJING_TIME;
  struct tm info;
  if(localtime_r(&t, &info) != NULL)
  {
    strftime(pt_buf, buf_len, "%Y-%m-%d %H:%M:%S", &info);
  }
}

/**
 * @brief To dump the buffer in hex (only dump when DEBUG_OPEN is 1).
 * @param pt_buf: The input buffer
 * @param kplen: The length of the input buffer (in byte)
 */
void
util_dbg_buf_dump(
  uint8_t const *pt_buf,
  uint32_t kplen)
{
  uint32_t i = 0;

  DBG_LOG_DEBUG("\n ==============%u bytes @ 0x%p===============", 
                kplen,
                pt_buf);
  for(i = 0; i < kplen; i++)
  {
    DBG_LOG_DEBUG_RAW("%02x", pt_buf[i]);
  }
  DBG_LOG_DEBUG_RAW("\r\n");
  DBG_LOG_DEBUG(" ============================================");
}
/**
 * @brief To copy message from pt_buf to pt_msg, and set @p flg of
 *        @p pt_msg to true to indicate the message is locked. The output
 *        pt_msg is valid only when the function returns true.
 * @param pt_msg: The output structure
 * @param pt_buf: The input buffer
 * @param kplen: The length of the input buffer @p pt_buf (in byte)
 */
bool
util_dbg_fill_froto_msg(
  struct froto_msg *pt_msg,
  uint8_t const *pt_buf,
  uint32_t kplen)
{
  if((pt_msg == NULL) || (pt_buf == NULL))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return false;
  }
  if((kplen == 0) || (kplen > sizeof(pt_msg->buf)))
  {
	  DBG_LOG_ERR("Invalid parameter");
    return false;
  }

  memset(pt_msg->buf, 0, kplen);
  memcpy(pt_msg->buf, pt_buf, kplen);

  pt_msg->flg = true;
  pt_msg->len = kplen;
  return true;
}
/**
 * @brief To release the pt_msg (by setting set @p flg of
 *        @p pt_msg to false)
 */
void
util_dbg_release_froto_msg(struct froto_msg *pt_msg)
{
  if(pt_msg == NULL)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }
  if(pt_msg->buf != NULL)
  {
	// Do nothing because buf here is an array
  }
  pt_msg->flg = false;
  pt_msg->len = 0;
}
// TODO: This function can be used system calling "cp" directly
/**
 * @brief To copy the file @p spath to @p dpath . Note that this function
 *        does NOT have return, which means to ensure the file
 *        accessibility is required before calling.
 */
void
util_dbg_copy_file(
  const char *spath,
  const char *dpath)
{
  int kp_sfd = -1, kp_dfd = -1;
  char kp_buf[0x100] = { 0 };
  ssize_t kp_szval = 0;

  if((NULL == spath) || (NULL == dpath))
  {
    DBG_LOG_ERR("Unexpected NULL");
    goto EXIT;
  }
  DBG_LOG_INFO("To copy file %s to %s", spath, dpath);

  // Open source file
  kp_sfd = open(spath, O_RDONLY);
  if(kp_sfd < 0)
  {
    DBG_LOG_ERR("Failed to open file %s", spath);
    goto EXIT;
  }

  // Open destination file
  kp_dfd = open(dpath, O_CREAT | O_WRONLY | O_TRUNC,
                S_IRWXU | S_IRWXG | S_IRWXO);
  if(kp_dfd < 0)
  {
    DBG_LOG_ERR("Failed to open file %s", dpath);
    goto EXIT;
  }

  // Copy: read a line and write to the destination file
  while(1)
  {
    memset(kp_buf, 0, sizeof(kp_buf));
    kp_szval = read(kp_sfd, kp_buf, sizeof(kp_buf));
    if(kp_szval < 0)
    {
      DBG_LOG_ERR("Failed to read %s (%d), for %s", spath, kp_sfd,
                  strerror(errno));
      goto EXIT;
    }
    if(kp_szval == 0)
    {
      // Nothing to read means copy operation completed
      break;
    }
    if(write(kp_dfd, kp_buf, kp_szval) < 0)
    {
      DBG_LOG_ERR("Failed to write %s (%d), for %s", dpath, kp_dfd,
                  strerror(errno));
      goto EXIT;
    }
  }

EXIT:
  if(kp_sfd >= 0)
  {
    close(kp_sfd);
  }
  if(kp_dfd >= 0)
  {
    close(kp_dfd);
  }
  return;
}

#endif /* DBG_CT == 1 */
