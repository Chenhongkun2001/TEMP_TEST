/*APIs for SKF protocol Froto
*/
#include "util_froto.h"
#include "DeviceAppBulletGateway.pb.h"
#include "ConfigurationAndCommand.pb.h"
#include "Froto.pb.h"
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "pb_common.h"
#include "util_dbg.h"
#include <ell/ell.h>
#include <ell/queue.h>
#include <pthread.h>

#include "app_froto.h"
#include "util_froto_datawrap.h"
#include "app_pro_general.h"

DBG_LOCAL_LOG_DEBUG


/**************************************************************/
/**********************CRC32 Calculation***********************/
/**************************************************************/
// Borrow code from https://blog.csdn.net/gongmin856/article/details/77101397
static const uint32_t g_crc32tab[] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL,
  0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
  0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L,
  0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L,
  0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L,
  0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL,
  0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L,
  0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L,
  0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L,
  0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
  0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L,
  0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL,
  0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL,
  0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL,
  0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L,
  0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L,
  0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL,
  0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
  0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL,
  0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L,
  0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL,
  0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L,
  0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L,
  0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L,
  0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L,
  0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,
  0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL,
  0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L,
  0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L,
  0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL,
  0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L,
  0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL,
  0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L,
  0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
  0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L,
  0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L,
  0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL,
  0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L,
  0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL,
  0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL,
  0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L,
  0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
  0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL,
  0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L,
  0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L,
  0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L,
  0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL,
  0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L,
  0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L,
  0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
  0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L,
  0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL
};

/**
 * @brief Caculate CRC32 value
 * @param buf: The input
 * @param size: The size (in byte) of the input buffer
 */
uint32_t
util_froto_bullet_crc32(
  const uint8_t *buf,
  uint32_t size)
{

  uint32_t i, crc;

  crc = 0xFFFFFFFF;
  for(i = 0; i < size; i++)
  {
    crc = g_crc32tab[(crc ^ buf[i]) & 0xff] ^ (crc >> 8);
  }

  return crc ^ 0xFFFFFFFF;
}


/**************************************************************/
/****************Configuration Hash Calculation****************/
/**************************************************************/
#define MAX_HASH_BUFFER (512U)

static void memcpy_e(
  uint8_t *pt_des,
  uint8_t *pt_src,
  const uint32_t kpsz);
/**
 * @brief: To calculate the hash value (based on the given @p inputData ).
 *         @p length represents the length (in byte) of @p inputData .
 * @return: The calculated hash value.
 */
uint32_t
util_froto_bullet_elfhash(
  const uint8_t *inputData,
  uint32_t length)
{
  uint32_t hash = 0;
  uint32_t x = 0;

  if(inputData == NULL)
  {
    return 0;
  }

  DBG_LOG_DEBUG("++++++++elfhash input: %d B+++++++\n\r", length);
  util_dbg_buf_dump(inputData, length);
  DBG_LOG_DEBUG("++++++++elfhash input end++++++++\n\r");

  for(uint16_t i = 0; i < length; ++inputData, ++i)
  {
    hash = (hash << 4) + ((*inputData) & 0xff);
    if((x = hash & 0xF0000000L) != 0)
    {
      hash ^= (x >> 24);
    }
    hash &= ~x;
  }
  return hash;
}
/**
 * @brief: To achieve an endianness memory copy.
 */
static void
memcpy_e(
  uint8_t *pt_des,
  uint8_t *pt_src,
  const uint32_t kpsz)
{
  uint16_t kp_endian_chk = 0xAABB;
  uint8_t *pt_u8 = (uint8_t *)(&kp_endian_chk);

  if((NULL == pt_des) || (NULL == pt_src))
  {
    DBG_LOG_ERR("invalid parameter");
    return;
  }
  if(0 == kpsz)
  {
    return;
  }

  if((*pt_u8) == 0xBB)
  {
    // Little-endian: do nothing, just copy
    memcpy(pt_des, pt_src, kpsz);
  }
  else
  {
    // Big-endian: convert to the little-endian order
    for(uint32_t i = 0; i < kpsz; i++)
    {
      pt_des[i] = pt_src[(kpsz - 1 - i) % kpsz];
    }
  }
}

/**
 * @brief Calculate the hash value of the Bullet sensor configuration.
 * Notice that there is no NULL pointer check in the function. Please
 * guarantee that pointers are valid.
 * @param @p pt_sensor_conf: a pointer to the sensor config
 * @param @p clientID: a pointer to the gateway's ID (i.e., the BLE
 * MAC address of the gateway)
 * @return the hash value
 */
uint32_t
util_froto_cal_config_hash_value(
  sensorConfig_t *pt_sensor_conf,
  char *clientID)
{
  uint8_t hashBuffer[MAX_HASH_BUFFER] = { 0 };
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
  memcpy_e(&hashBuffer[offset],
           &AlarmThrestemp_tmp,
           sizeof(AlarmThrestemp_tmp));
  offset += sizeof(AlarmThrestemp_tmp);

  /* System */
  // Memory ID: 59
  // batteryAlarmThreshold
  float batteryAlarmThrePercent_tmp;
  batteryAlarmThrePercent_tmp =
    pt_sensor_conf->bulletSensorConfig.sysConfig.batteryAlarmThrePercent *
    1.0;
  memcpy_e(&hashBuffer[offset],
           &batteryAlarmThrePercent_tmp,
           sizeof(batteryAlarmThrePercent_tmp));
  offset += sizeof(batteryAlarmThrePercent_tmp);

  // Memory ID: 61
  // sensorMode
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode,
           sizeof(pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode);
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
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.
                  pre_acq_vib_fs_hz));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz);

  // Memory ID: 64
  // PRE_ACQ_VIB_N
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_n,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.
                  pre_acq_vib_n));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_n);

  // Memory ID: 65
  // PRE_ACQ_VIB_AXIS_ACQ_EVAL
  uint32_t pre_acq_vib_axis_acq_eval_tmp =
    pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_axis_acq_eval;
  memcpy_e(&hashBuffer[offset],
           &pre_acq_vib_axis_acq_eval_tmp,
           sizeof(pre_acq_vib_axis_acq_eval_tmp));
  offset += sizeof(pre_acq_vib_axis_acq_eval_tmp);

  // Memory ID: 66
  // PRE_ACQ_VIB_RANGE
  uint32_t pre_acq_vib_range_tmp =
    pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_range;
  memcpy_e(&hashBuffer[offset],
           &pre_acq_vib_range_tmp,
           sizeof(pre_acq_vib_range_tmp));
  offset += sizeof(pre_acq_vib_range_tmp);

  // Memory ID: 67
  // PRE_ACQ_VIB_RANGE
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.
                  pre_acq_mag_fs_hz));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz);

  // Memory ID: 68
  // PRE_ACQ_MAG_N
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_n,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.
                  pre_acq_mag_n));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_n);

  // Memory ID: 69
  // PRE_ACQ_MAG_AXIS_ACQ_EVAL
  uint32_t pre_acq_mag_axis_acq_eval_tmp =
    pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_axis_acq_eval;
  memcpy_e(&hashBuffer[offset],
           &pre_acq_mag_axis_acq_eval_tmp,
           sizeof(pre_acq_mag_axis_acq_eval_tmp));
  offset += sizeof(pre_acq_mag_axis_acq_eval_tmp);

  // Memory ID: 70
  // ACQ_VIB_AXIS_ACQ_EVAL
  uint32_t acq_vib_axis_acq_eval_tmp =
    pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval;
  memcpy_e(&hashBuffer[offset],
           &acq_vib_axis_acq_eval_tmp,
           sizeof(acq_vib_axis_acq_eval_tmp));
  offset += sizeof(acq_vib_axis_acq_eval_tmp);

  // Memory ID: 71
  // ACQ_VIB_RANGE
  uint32_t acq_vib_range_tmp =
    pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_range;
  memcpy_e(&hashBuffer[offset],
           &acq_vib_range_tmp,
           sizeof(acq_vib_range_tmp));
  offset += sizeof(acq_vib_range_tmp);

  // Memory ID: 72
  // ACQ_MAG_FS_HZ
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_fs_hz,
           sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.
                  acq_mag_fs_hz));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_fs_hz);

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
  memcpy_e(&hashBuffer[offset],
           &acq_mag_axis_acq_eval_tmp,
           sizeof(acq_mag_axis_acq_eval_tmp));
  offset += sizeof(acq_mag_axis_acq_eval_tmp);

  /*Algorithm*/
  // Memory ID: 75
  // GEE_COEF
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF));
  offset += sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF);

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
  memcpy_e(&hashBuffer[offset],
           &MEAS_POSITION_tmp,
           sizeof(MEAS_POSITION_tmp));
  offset += sizeof(MEAS_POSITION_tmp);

  // Memory ID: 78
  // MEAS_LOAD
  uint32_t MEAS_LOAD_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_LOAD;
  memcpy_e(&hashBuffer[offset],
           &MEAS_LOAD_tmp,
           sizeof(MEAS_LOAD_tmp));
  offset += sizeof(MEAS_LOAD_tmp);

  // Memory ID: 79
  // MEAS_AXIS_VIB
  uint32_t MEAS_AXIS_VIB_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB;
  memcpy_e(&hashBuffer[offset],
           &MEAS_AXIS_VIB_tmp,
           sizeof(MEAS_AXIS_VIB_tmp));
  offset += sizeof(MEAS_AXIS_VIB_tmp);

  // Memory ID: 80
  // MEAS_AXIS_VIB
  uint32_t MEAS_AXIS_MAG_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG;
  memcpy_e(&hashBuffer[offset],
           &MEAS_AXIS_MAG_tmp,
           sizeof(MEAS_AXIS_MAG_tmp));
  offset += sizeof(MEAS_AXIS_MAG_tmp);

  // Memory ID: 81
  // VIB_START_FG
  uint8_t VIB_START_FG_tmp;
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_START_FG)
  {
    VIB_START_FG_tmp = 1;
  }
  else
  {
    VIB_START_FG_tmp = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &VIB_START_FG_tmp,
           sizeof(VIB_START_FG_tmp));
  offset += sizeof(VIB_START_FG_tmp);

  // Memory ID: 82
  // VIB_RMS_START_TL
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_RMS_START_TL,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  VIB_RMS_START_TL));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_RMS_START_TL);

  // Memory ID: 83
  // MAG_START_FG
  uint8_t MAG_START_FG_tmp;
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_START_FG)
  {
    MAG_START_FG_tmp = 1;
  }
  else
  {
    MAG_START_FG_tmp = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &MAG_START_FG_tmp,
           sizeof(MAG_START_FG_tmp));
  offset += sizeof(MAG_START_FG_tmp);

  // Memory ID: 84
  // MAG_RMS_START_TL
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_START_TL,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  MAG_RMS_START_TL));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_START_TL);

  // Memory ID: 85
  // MAG_STABLE_FG
  uint8_t MAG_STABLE_FG_tmp;
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_STABLE_FG)
  {
    MAG_STABLE_FG_tmp = 1;
  }
  else
  {
    MAG_STABLE_FG_tmp = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &MAG_STABLE_FG_tmp,
           sizeof(MAG_STABLE_FG_tmp));
  offset += sizeof(MAG_STABLE_FG_tmp);

  // Memory ID: 86
  // MAG_RMS_VAR_TH
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  MAG_RMS_VAR_TH));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH);

  // Memory ID: 87
  // RPM_VAR_RANGE
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.RPM_VAR_RANGE,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  RPM_VAR_RANGE));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.RPM_VAR_RANGE);

  // Memory ID: 88
  // DEC_LOGIC_TEMP_M
  uint32_t DEC_LOGIC_TEMP_M_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M;
  memcpy_e(&hashBuffer[offset],
           &DEC_LOGIC_TEMP_M_tmp,
           sizeof(DEC_LOGIC_TEMP_M_tmp));
  offset += sizeof(DEC_LOGIC_TEMP_M_tmp);

  // Memory ID: 89
  // DEC_LOGIC_TEMP_N
  uint32_t DEC_LOGIC_TEMP_N_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N;
  memcpy_e(&hashBuffer[offset],
           &DEC_LOGIC_TEMP_N_tmp,
           sizeof(DEC_LOGIC_TEMP_N_tmp));
  offset += sizeof(DEC_LOGIC_TEMP_N_tmp);

  // Memory ID: 90
  // DEC_LOGIC_LEARN_NUM_TEMP
  uint32_t DEC_LOGIC_LEARN_NUM_TEMP_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_TEMP;
  memcpy_e(&hashBuffer[offset],
           &DEC_LOGIC_LEARN_NUM_TEMP_tmp,
           sizeof(DEC_LOGIC_LEARN_NUM_TEMP_tmp));
  offset += sizeof(DEC_LOGIC_LEARN_NUM_TEMP_tmp);

  // Memory ID: 91
  // DEC_LOGIC_VIB_M
  uint32_t DEC_LOGIC_VIB_M_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M;
  memcpy_e(&hashBuffer[offset],
           &DEC_LOGIC_VIB_M_tmp,
           sizeof(DEC_LOGIC_VIB_M_tmp));
  offset += sizeof(DEC_LOGIC_VIB_M_tmp);

  // Memory ID: 92
  // DEC_LOGIC_VIB_N
  uint32_t DEC_LOGIC_VIB_N_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N;
  memcpy_e(&hashBuffer[offset],
           &DEC_LOGIC_VIB_N_tmp,
           sizeof(DEC_LOGIC_VIB_N_tmp));
  offset += sizeof(DEC_LOGIC_VIB_N_tmp);

  // Memory ID: 93
  // DEC_LOGIC_LEARN_NUM_VIB
  uint32_t DEC_LOGIC_LEARN_NUM_VIB_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB;
  memcpy_e(&hashBuffer[offset],
           &DEC_LOGIC_LEARN_NUM_VIB_tmp,
           sizeof(DEC_LOGIC_LEARN_NUM_VIB_tmp));
  offset += sizeof(DEC_LOGIC_LEARN_NUM_VIB_tmp);

  // Memory ID: 94
  // DEC_LOGIC_LEARN_NUM_MAG
  uint32_t DEC_LOGIC_LEARN_NUM_MAG_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG;
  memcpy_e(&hashBuffer[offset],
           &DEC_LOGIC_LEARN_NUM_MAG_tmp,
           sizeof(DEC_LOGIC_LEARN_NUM_MAG_tmp));
  offset += sizeof(DEC_LOGIC_LEARN_NUM_MAG_tmp);

  // Memory ID: 95, 96, 97, 98, 99, 100, 108, 109, 110
  // FUNC_ANOM_TEMP, FUNC_ANOM_OV, FUNC_ANOM_MECH, FUNC_ANOM_BRG,
  // FUNC_ANOM_LUB, FUNC_ANOM_MTR, FUNC_ANOM_GEAR, FUNC_ANOM_FAN,
  // FUNC_ANOM_PUMP
  uint8_t generalFg;
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP)
  {
    generalFg = 1;
  }
  else
  {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &generalFg,
           sizeof(generalFg));
  offset += sizeof(generalFg);
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_OV)
  {
    generalFg = 1;
  }
  else
  {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &generalFg,
           sizeof(generalFg));
  offset += sizeof(generalFg);
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH)
  {
    generalFg = 1;
  }
  else
  {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &generalFg,
           sizeof(generalFg));
  offset += sizeof(generalFg);
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG)
  {
    generalFg = 1;
  }
  else
  {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &generalFg,
           sizeof(generalFg));
  offset += sizeof(generalFg);
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB)
  {
    generalFg = 1;
  }
  else
  {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &generalFg,
           sizeof(generalFg));
  offset += sizeof(generalFg);
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR)
  {
    generalFg = 1;
  }
  else
  {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &generalFg,
           sizeof(generalFg));
  offset += sizeof(generalFg);
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR)
  {
    generalFg = 1;
  }
  else
  {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &generalFg,
           sizeof(generalFg));
  offset += sizeof(generalFg);
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN)
  {
    generalFg = 1;
  }
  else
  {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &generalFg,
           sizeof(generalFg));
  offset += sizeof(generalFg);
  if(pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP)
  {
    generalFg = 1;
  }
  else
  {
    generalFg = 0;
  }
  memcpy_e(&hashBuffer[offset],
           &generalFg,
           sizeof(generalFg));
  offset += sizeof(generalFg);

  // Memory ID: 111
  // ASSET_LEVEL
  uint32_t ASSET_LEVEL_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.ASSET_LEVEL;
  memcpy_e(&hashBuffer[offset],
           &ASSET_LEVEL_tmp,
           sizeof(ASSET_LEVEL_tmp));
  offset += sizeof(ASSET_LEVEL_tmp);

  // Memory ID: 112
  // FLEX_TYPE
  uint32_t FLEX_TYPE_tmp =
    pt_sensor_conf->bulletSensorConfig.algoConfig.FLEX_TYPE;
  memcpy_e(&hashBuffer[offset],
           &FLEX_TYPE_tmp,
           sizeof(FLEX_TYPE_tmp));
  offset += sizeof(FLEX_TYPE_tmp);

  // Memory ID: 113
  // BORE_DIAMETER_MM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  BORE_DIAMETER_MM));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM);

  // Memory ID: 114
  // RUN_SPEED_RPM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  RUN_SPEED_RPM));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM);

  // Memory ID: 115
  // BRG_INFO_BPFO
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFO,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  BRG_INFO_BPFO));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFO);

  // Memory ID: 116
  // BRG_INFO_BPFI
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFI,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  BRG_INFO_BPFI));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFI);

  // Memory ID: 117
  // BRG_INFO_BPFO
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BSF,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  BRG_INFO_BSF));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BSF);

  // Memory ID: 118
  // BRG_INFO_FTF
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_FTF,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  BRG_INFO_FTF));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_FTF);

  // Memory ID: 119
  // MTR_INFO_FL
  uint32_t MTR_INFO_FL_tmp =
    (uint32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_FL);
  memcpy_e(&hashBuffer[offset],
           &MTR_INFO_FL_tmp,
           sizeof(MTR_INFO_FL_tmp));
  offset += sizeof(MTR_INFO_FL_tmp);

  // Memory ID: 120
  // MTR_INFO_BAR
  uint32_t MTR_INFO_BAR_tmp =
    (uint32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_BAR);
  memcpy_e(&hashBuffer[offset],
           &MTR_INFO_BAR_tmp,
           sizeof(MTR_INFO_BAR_tmp));
  offset += sizeof(MTR_INFO_BAR_tmp);

  // Memory ID: 121
  // GEAR_INFO_TOOTH
  uint32_t GEAR_INFO_TOOTH_tmp =
    (uint32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.
               GEAR_INFO_TOOTH);
  memcpy_e(&hashBuffer[offset],
           &GEAR_INFO_TOOTH_tmp,
           sizeof(GEAR_INFO_TOOTH_tmp));
  offset += sizeof(GEAR_INFO_TOOTH_tmp);

  // Memory ID: 122
  // FAN_INFO_BLADE
  uint32_t FAN_INFO_BLADE_tmp =
    (uint32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.FAN_INFO_BLADE);
  memcpy_e(&hashBuffer[offset],
           &FAN_INFO_BLADE_tmp,
           sizeof(FAN_INFO_BLADE_tmp));
  offset += sizeof(FAN_INFO_BLADE_tmp);

  // Memory ID: 123
  // PUMP_INFO_VANE
  uint32_t PUMP_INFO_VANE_tmp =
    (uint32_t)(pt_sensor_conf->bulletSensorConfig.algoConfig.PUMP_INFO_VANE);
  memcpy_e(&hashBuffer[offset],
           &PUMP_INFO_VANE_tmp,
           sizeof(PUMP_INFO_VANE_tmp));
  offset += sizeof(PUMP_INFO_VANE_tmp);

  // Memory ID: 124
  // TEMP_OV_ALERT_CDEGREE
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  TEMP_OV_ALERT_CDEGREE));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
           TEMP_OV_ALERT_CDEGREE);

  // Memory ID: 125
  // ACC_OV_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  ACC_OV_ALERT));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALERT);

  // Memory ID: 126
  // ACC_OV_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  ACC_OV_ALARM));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALARM);

  // Memory ID: 127
  // ACC_HAL_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  ACC_HAL_ALERT));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALERT);

  // Memory ID: 128
  // ACC_HAL_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  ACC_HAL_ALARM));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALARM);

  // Memory ID: 129
  // VEL_OV_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  VEL_OV_ALERT));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALERT);

  // Memory ID: 130
  // VEL_OV_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  VEL_OV_ALARM));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALARM);

  // Memory ID: 131
  // VEL_HAL_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  VEL_HAL_ALERT));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALERT);

  // Memory ID: 132
  // VEL_HAL_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  VEL_HAL_ALARM));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALARM);

  // Memory ID: 133
  // ENV_OV_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  ENV_OV_ALERT));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALERT);

  // Memory ID: 134
  // ENV_OV_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  ENV_OV_ALARM));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALARM);

  // Memory ID: 135
  // ENV_HAL_ALERT
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALERT,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  ENV_HAL_ALERT));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALERT);

  // Memory ID: 136
  // ENV_HAL_ALARM
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALARM,
           sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.
                  ENV_HAL_ALARM));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALARM);

  /* System */
  // Memory ID: 137
  // txPowerAdv
  int32_t txPowerAdv_tmp =
    pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAdv;
  memcpy_e(&hashBuffer[offset],
           &txPowerAdv_tmp,
           sizeof(txPowerAdv_tmp));
  offset += sizeof(txPowerAdv_tmp);

  // Memory ID: 138
  // dataRate_auxAdv
  uint32_t dataRateAuxAdv_tmp =
    pt_sensor_conf->bulletSensorConfig.sysConfig.dataRateAuxAdv;
  memcpy_e(&hashBuffer[offset],
           &dataRateAuxAdv_tmp,
           sizeof(dataRateAuxAdv_tmp));
  offset += sizeof(dataRateAuxAdv_tmp);

  // Memory ID: 139
  // txPower_auxAdv
  int32_t txPowerAuxAdv_tmp =
    pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAuxAdv;
  memcpy_e(&hashBuffer[offset],
           &txPowerAuxAdv_tmp,
           sizeof(txPowerAuxAdv_tmp));
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
  sscanf(clientID,
         "%02x%02x%02x%02x%02x%02x",
         &gwMACAddr.addr.addrArray[0],
         &gwMACAddr.addr.addrArray[1],
         &gwMACAddr.addr.addrArray[2],
         &gwMACAddr.addr.addrArray[3],
         &gwMACAddr.addr.addrArray[4],
         &gwMACAddr.addr.addrArray[5]);
  memcpy_e(&hashBuffer[offset],
           gwMACAddr.addr.addrArray,
           sizeof(gwMACAddr.addr.addrArray));
  offset += sizeof(gwMACAddr.addr.addrArray);

  // Scheduler
  // Memory ID: 143
  // period_quickPolling
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.schConfig.period_quickPolling_s,
           sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
                  period_quickPolling_s));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
           period_quickPolling_s);

  // Memory ID: 144
  // period_regularSensing
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.schConfig.period_regularSensing_s,
           sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
                  period_regularSensing_s));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
           period_regularSensing_s);

  // Memory ID: 145
  // period_comm
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.schConfig.period_comm_s,
           sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
                  period_comm_s));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.period_comm_s);

  // Memory ID: 146
  // referenceTime_quickPolling
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.schConfig.refTime_quickPolling_s,
           sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
                  refTime_quickPolling_s));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
           refTime_quickPolling_s);

  // Memory ID: 147
  // referenceTime_regularSensing
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.schConfig.refTime_regularSensing_s,
           sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
                  refTime_regularSensing_s));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
           refTime_regularSensing_s);

  // Memory ID: 148
  // referenceTime_period_comm
  uint64_t refTime_period_comm_s_tmp =
    pt_sensor_conf->bulletSensorConfig.schConfig.refTime_period_comm_s;
  memcpy_e(&hashBuffer[offset],
           &refTime_period_comm_s_tmp,
           sizeof(refTime_period_comm_s_tmp));
  offset += sizeof(refTime_period_comm_s_tmp);

  // Memory ID: 149
  // waveDataAcqReferenceTime
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.schConfig.refTime_wave_s,
           sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
                  refTime_wave_s));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.refTime_wave_s);

  // Memory ID: 150
  // waveDataAcqPeriod
  memcpy_e(&hashBuffer[offset],
           &pt_sensor_conf->bulletSensorConfig.schConfig.period_wave_s,
           sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.
                  period_wave_s));
  offset +=
    sizeof(pt_sensor_conf->bulletSensorConfig.schConfig.period_wave_s);

  hash_value = util_froto_bullet_elfhash(hashBuffer, offset);

  return hash_value;
}

/**************************************************************/
/*******************Configuration for Froto********************/
/**************************************************************/
#define MAX_CONTENT_REPEAT (16)

/**
 * @brief enum of configuration item type
 */
enum conf_content_type
{
  SENSOR_CONF_CONTENT_UNDEFINED = 0,
  SENSOR_CONF_CONTENT_BYTES = 1,
  SENSOR_CONF_CONTENT_TIMEARRAY = 2,
  SENSOR_CONF_CONTENT_WAKEUP_TO_SENSEDESC = 3,
  SENSOR_CONF_CONTENT_BOOL_ARRAY = 4,
  SENSOR_CONF_CONTENT_UINT32 = 5,
  SENSOR_CONF_CONTENT_INT32 = 6,
  SENSOR_CONF_CONTENT_FLOAT = 7,
  SENSOR_CONF_CONTENT_WORK_MODE_PAIR = 8,
  SENSOR_CONF_CONTENT_GATEWAY_INFO_PAIR = 9,
  SENSOR_CONF_CONTENT_BOOL = 10  // bool and uint8
};

union config_content
{
  //pb_callback_t general_config_content;
  struct
  {
    uint8_t num;
    uint8_t repeat[MAX_CONTENT_REPEAT];
  } general_config_content;

  //SKFChina_ConfigurationAndCommand_TimeArray time_config_content;
  struct
  {
    uint8_t num;
    uint64_t repeat[MAX_CONTENT_REPEAT];
  } time_config_content;

  // Used for fScheduler (not available for Bullet so far)
  SKFChina_ConfigurationAndCommand_WakeUpToSenseDesc
    measurement_description;

  //SKFChina_ConfigurationAndCommand_BoolArray bool_config_content;
  struct
  {
    uint8_t num;
    bool repeat[MAX_CONTENT_REPEAT];
  } bool_config_content;

  uint32_t general_config_content_uint32;
  int32_t general_config_content_int32;
  float general_config_content_float;

  //SKFChina_ConfigurationAndCommand_WorkModePair work_mode_content;
  struct
  {
    SKFChina_Common_WorkMode work_mode;

    uint8_t num;
    uint8_t para[MAX_CONTENT_REPEAT];
  } work_mode_content;

  //SKFChina_ConfigurationAndCommand_GatewayInfoPair gateway_info_content;
  struct
  {
    uint8_t num;
    uint8_t mac_address[MAX_CONTENT_REPEAT];

    uint32_t name_id;
  } gateway_info_content;

  bool general_config_content_bool;
};

struct config_content_packet
{
  bool is_data_valid;  // set to true when the following data is valid
  union config_content conf_data;
};

static struct config_content_packet pick_conf_content_by_item_id(
  SKFChina_Common_SpecificConfigItem kp_conf_item,
  const sensorConfig_t *pt_sensor_conf);
static enum conf_content_type map_conf_item_to_content_type(
  SKFChina_Common_SpecificConfigItem kp_conf_item);

/**
 * @brief To load the configuration (before encoding a Froto packet).
 * This can be used, for instance, to set configuration to a sensor.
 * @param kp_conf_item The configuration item defined by Froto
 * @param pt_sensor_conf A pointer to sensor config
 * @return a structure used to store the configure
 */
static struct config_content_packet
pick_conf_content_by_item_id(
  SKFChina_Common_SpecificConfigItem kp_conf_item,
  const sensorConfig_t *pt_sensor_conf)
{
  struct config_content_packet kpret = { .is_data_valid = false, 0 };

  if(NULL == pt_sensor_conf)
  {
    DBG_LOG_ERR("Unexpected pointer\n\r");
    return kpret;
  }
  switch(kp_conf_item)
  {
    case
      SKFChina_Common_SpecificConfigItem_BATTERY_LOW_ALARM_THRESHOLD_PERCENTAGE
      :
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.sysConfig.
        batteryAlarmThrePercent;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_WORK_MODE:
    {
      // SENSOR_CONF_CONTENT_WORK_MODE_PAIR
	  // N.B.: For 1.0.x, DO NOT add new feature any more.
	  // So DO NOT support STATUS_SENSING_FEATURE_ENABLE
      kpret.conf_data.work_mode_content.work_mode =
        pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode;
      kpret.conf_data.work_mode_content.num = 1;
      kpret.conf_data.work_mode_content.para[0] = 0;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_ADV_DBM:
    {
      // SENSOR_CONF_CONTENT_INT32
      kpret.conf_data.general_config_content_int32 =
        pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAdv;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_AUX_ADV_KBPS:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.sysConfig.dataRateAuxAdv;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_AUX_ADV_DBM:
    {
      // SENSOR_CONF_CONTENT_INT32
      kpret.conf_data.general_config_content_int32 =
        pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAuxAdv;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_CONN_KBPS:
    {
      // SENSOR_CONF_CONTENT_UINT32
      DBG_LOG_WARN("Not available so far\r\n");
      kpret.is_data_valid = false;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_CONN_DBM:
    {
      // SENSOR_CONF_CONTENT_INT32
      DBG_LOG_WARN("Not available so far\r\n");
      kpret.is_data_valid = false;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_CURRENT_TIME:
    {
      // SENSOR_CONF_CONTENT_TIMEARRAY
      kpret.conf_data.time_config_content.num = 1;
      // No timezone
      kpret.conf_data.time_config_content.repeat[0] = time(NULL);
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME:
    {
      // SENSOR_CONF_CONTENT_TIMEARRAY
      kpret.conf_data.time_config_content.num = 1;
      kpret.conf_data.time_config_content.repeat[0] =
        pt_sensor_conf->lastEditTimeS;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_GATEWAY_ADDR:
    {
      // SENSOR_CONF_CONTENT_GATEWAY_INFO_PAIR
      kpret.conf_data.gateway_info_content.num = 6;
      macAddr_t gwMACAddr;
      sscanf(app_pro_gen_get_gateway_idstr(app_pro_gen_get_ctr()),
             "%02x%02x%02x%02x%02x%02x",
             &gwMACAddr.addr.addrArray[0],
             &gwMACAddr.addr.addrArray[1],
             &gwMACAddr.addr.addrArray[2],
             &gwMACAddr.addr.addrArray[3],
             &gwMACAddr.addr.addrArray[4],
             &gwMACAddr.addr.addrArray[5]);
      memcpy(&kpret.conf_data.gateway_info_content.mac_address,
             gwMACAddr.addr.addrArray,
             sizeof(gwMACAddr.addr.addrArray));
      // Skip the name id
      kpret.conf_data.gateway_info_content.name_id = 0;
      kpret.is_data_valid = true;
      break;
    }

    case SKFChina_Common_SpecificConfigItem_QUICKPOLLING_PERIOD_S:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.schConfig.period_quickPolling_s;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_REGULARSENSING_PERIOD_S:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.schConfig.
        period_regularSensing_s;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_COMMUNICATION_PERIOD_S:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.schConfig.period_comm_s;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S:
    {
      // SENSOR_CONF_CONTENT_TIMEARRAY
      kpret.conf_data.time_config_content.num = 1;
      kpret.conf_data.time_config_content.repeat[0] =
        pt_sensor_conf->bulletSensorConfig.schConfig.refTime_quickPolling_s;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S:
    {
      // SENSOR_CONF_CONTENT_TIMEARRAY
      kpret.conf_data.time_config_content.num = 1;
      kpret.conf_data.time_config_content.repeat[0] =
        pt_sensor_conf->bulletSensorConfig.schConfig.
        refTime_regularSensing_s;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S:
    {
      // SENSOR_CONF_CONTENT_TIMEARRAY
      kpret.conf_data.time_config_content.num = 1;
      kpret.conf_data.time_config_content.repeat[0] =
        pt_sensor_conf->bulletSensorConfig.schConfig.refTime_period_comm_s;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_PERIOD_S:
    {
      // SENSOR_CONF_CONTENT_UINT32
	  kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.schConfig.period_wave_s;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S:
    {
      // SENSOR_CONF_CONTENT_TIMEARRAY
      kpret.conf_data.time_config_content.num = 1;
      kpret.conf_data.time_config_content.repeat[0] =
        pt_sensor_conf->bulletSensorConfig.schConfig.refTime_wave_s;
      kpret.is_data_valid = true;
      break;
    }

    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_FS_HZ:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_N:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_n;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_AXIS_ACQ_EVAL:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.
        pre_acq_vib_axis_acq_eval;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_RANGE:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_range;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_FS_HZ:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_N:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_n;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_AXIS_ACQ_EVAL:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.
        pre_acq_mag_axis_acq_eval;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_RATE_HZ:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.fAccHz;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_PTS:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.nAcc;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_AXIS:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_RANGE_G:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_range;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_FS_HZ:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_fs_hz;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_N:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_n;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_AXIS_ACQ_EVAL:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FS_COEF:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.senConfig.fs_coef;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_GEE_COEF:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_V_COEF:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.V_COEF;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_POSITION:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_POSITION;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_LOAD:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_LOAD;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_AXIS_VIB:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_AXIS_MAG:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VIB_START_FG:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_START_FG;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VIB_RMS_START_TL:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_RMS_START_TL;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_START_FG:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_START_FG;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_RMS_START_TL:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_START_TL;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_STABLE_FG:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_STABLE_FG;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_RMS_VAR_TH:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_RPM_VAR_RANGE:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.RPM_VAR_RANGE;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_M:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_N:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_TEMP:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.
        DEC_LOGIC_LEARN_NUM_TEMP;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_M:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_N:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_VIB:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.
        DEC_LOGIC_LEARN_NUM_VIB;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_MAG:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.
        DEC_LOGIC_LEARN_NUM_MAG;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_TEMP:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_OV:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_OV;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MECH:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_BRG:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_LUB:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MTR:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_GEAR:
    {
      //tpret = SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_FAN:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_PUMP:
    {
      // SENSOR_CONF_CONTENT_BOOL
      kpret.conf_data.general_config_content_bool =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ASSET_LEVEL:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.ASSET_LEVEL;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FLEX_TYPE:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FLEX_TYPE;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BORE_DIAMETER_MM:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_RUN_SPEED_RPM:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFO:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFO;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFI:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFI;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BSF:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BSF;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_FTF:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_FTF;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MTR_INFO_FL:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_FL;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MTR_INFO_BAR:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_BAR;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_GEAR_INFO_TOOTH:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FAN_INFO_BLADE:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.FAN_INFO_BLADE;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PUMP_INFO_VANE:
    {
      // SENSOR_CONF_CONTENT_UINT32
      kpret.conf_data.general_config_content_uint32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.PUMP_INFO_VANE;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TEMP_OV_ALERT_CDEGREE:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE;
      kpret.is_data_valid = true;
      break;
    }
    case
      SKFChina_Common_SpecificConfigItem_TEMPERATURE_ALARM_THRESHOLD_CDGREE
      :
    {
      // SENSOR_CONF_CONTENT_INT32
      kpret.conf_data.general_config_content_int32 =
        pt_sensor_conf->bulletSensorConfig.algoConfig.AlarmThrestemp;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_OV_ALERT:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALERT;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_OV_ALARM:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALARM;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_HAL_ALERT:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALERT;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_HAL_ALARM:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALARM;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_OV_ALERT:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALERT;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_OV_ALARM:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALARM;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_HAL_ALERT:
    {
      //tpret = SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALERT;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_HAL_ALARM:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALARM;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_OV_ALERT:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.is_data_valid = true;
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALERT;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_OV_ALARM:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALARM;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_HAL_ALERT:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALERT;
      kpret.is_data_valid = true;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_HAL_ALARM:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      kpret.conf_data.general_config_content_float =
        pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALARM;
      kpret.is_data_valid = true;
      break;
    }
    default:
    {
      kpret.is_data_valid = false;
      DBG_LOG_ERR("Unknown configuration item!\r\n");
      break;
    }
  }
  return kpret;
}
/**
 * @brief To get configuration item type
 */
static enum conf_content_type
map_conf_item_to_content_type(
  SKFChina_Common_SpecificConfigItem kp_conf_item)
{
  enum conf_content_type tpret = SENSOR_CONF_CONTENT_UNDEFINED;

  switch(kp_conf_item)
  {
    case
      SKFChina_Common_SpecificConfigItem_BATTERY_LOW_ALARM_THRESHOLD_PERCENTAGE
      :
    {
      // SENSOR_CONF_CONTENT_FLOAT
      tpret = SENSOR_CONF_CONTENT_FLOAT;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_WORK_MODE:
    {
      // SENSOR_CONF_CONTENT_WORK_MODE_PAIR
      tpret = SENSOR_CONF_CONTENT_WORK_MODE_PAIR;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_ADV_DBM:
    {
      // SENSOR_CONF_CONTENT_INT32
      tpret = SENSOR_CONF_CONTENT_INT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_AUX_ADV_KBPS:
    {
      // SENSOR_CONF_CONTENT_UINT32
      tpret = SENSOR_CONF_CONTENT_UINT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_AUX_ADV_DBM:
    {
      // SENSOR_CONF_CONTENT_INT32
      tpret = SENSOR_CONF_CONTENT_INT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_CONN_KBPS:
    {
      // SENSOR_CONF_CONTENT_UINT32
      tpret = SENSOR_CONF_CONTENT_UINT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_CONN_DBM:
    {
      // SENSOR_CONF_CONTENT_INT32
      tpret = SENSOR_CONF_CONTENT_INT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_CURRENT_TIME:
    case SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME:
    {
      // SENSOR_CONF_CONTENT_TIMEARRAY
      tpret = SENSOR_CONF_CONTENT_TIMEARRAY;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_GATEWAY_ADDR:
    {
      // SENSOR_CONF_CONTENT_GATEWAY_INFO_PAIR
      tpret = SENSOR_CONF_CONTENT_GATEWAY_INFO_PAIR;
      break;
    }

    case SKFChina_Common_SpecificConfigItem_QUICKPOLLING_PERIOD_S:
    case SKFChina_Common_SpecificConfigItem_REGULARSENSING_PERIOD_S:
    case SKFChina_Common_SpecificConfigItem_COMMUNICATION_PERIOD_S:
	case SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_PERIOD_S:
    {
      // SENSOR_CONF_CONTENT_UINT32
      tpret = SENSOR_CONF_CONTENT_UINT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S:
    case SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S:
    case SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S:
    case SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S:
    {
      // SENSOR_CONF_CONTENT_TIMEARRAY
      tpret = SENSOR_CONF_CONTENT_TIMEARRAY;
      break;
    }

    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_FS_HZ:
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_N:
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_AXIS_ACQ_EVAL:
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_RANGE:
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_FS_HZ:
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_N:
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_AXIS_ACQ_EVAL:
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_RATE_HZ:
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_PTS:
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_AXIS:
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_RANGE_G:
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_FS_HZ:
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_N:
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_AXIS_ACQ_EVAL:
    {
      // SENSOR_CONF_CONTENT_UINT32
      tpret = SENSOR_CONF_CONTENT_UINT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FS_COEF:
    case SKFChina_Common_SpecificConfigItem_GEE_COEF:
    case SKFChina_Common_SpecificConfigItem_V_COEF:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      tpret = SENSOR_CONF_CONTENT_FLOAT;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_POSITION:
    case SKFChina_Common_SpecificConfigItem_MEAS_LOAD:
    case SKFChina_Common_SpecificConfigItem_MEAS_AXIS_VIB:
    case SKFChina_Common_SpecificConfigItem_MEAS_AXIS_MAG:
    {
      // SENSOR_CONF_CONTENT_UINT32
      tpret = SENSOR_CONF_CONTENT_UINT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VIB_START_FG:
    {
      // SENSOR_CONF_CONTENT_BOOL
      tpret = SENSOR_CONF_CONTENT_BOOL;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VIB_RMS_START_TL:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      tpret = SENSOR_CONF_CONTENT_FLOAT;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_START_FG:
    {
      // SENSOR_CONF_CONTENT_BOOL
      tpret = SENSOR_CONF_CONTENT_BOOL;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_RMS_START_TL:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      tpret = SENSOR_CONF_CONTENT_FLOAT;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_STABLE_FG:
    {
      // SENSOR_CONF_CONTENT_BOOL
      tpret = SENSOR_CONF_CONTENT_BOOL;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_RMS_VAR_TH:
    case SKFChina_Common_SpecificConfigItem_RPM_VAR_RANGE:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      tpret = SENSOR_CONF_CONTENT_FLOAT;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_M:
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_N:
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_TEMP:
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_M:
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_N:
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_VIB:
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_MAG:
    {
      // SENSOR_CONF_CONTENT_UINT32
      tpret = SENSOR_CONF_CONTENT_UINT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_TEMP:
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_OV:
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MECH:
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_BRG:
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_LUB:
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MTR:
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_GEAR:
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_FAN:
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_PUMP:
    {
      // SENSOR_CONF_CONTENT_BOOL
      tpret = SENSOR_CONF_CONTENT_BOOL;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ASSET_LEVEL:
    case SKFChina_Common_SpecificConfigItem_FLEX_TYPE:
    {
      // SENSOR_CONF_CONTENT_UINT32
      tpret = SENSOR_CONF_CONTENT_UINT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BORE_DIAMETER_MM:
    case SKFChina_Common_SpecificConfigItem_RUN_SPEED_RPM:
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFO:
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFI:
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BSF:
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_FTF:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      tpret = SENSOR_CONF_CONTENT_FLOAT;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MTR_INFO_FL:
    case SKFChina_Common_SpecificConfigItem_MTR_INFO_BAR:
    case SKFChina_Common_SpecificConfigItem_GEAR_INFO_TOOTH:
    case SKFChina_Common_SpecificConfigItem_FAN_INFO_BLADE:
    case SKFChina_Common_SpecificConfigItem_PUMP_INFO_VANE:
    {
      // SENSOR_CONF_CONTENT_UINT32
      tpret = SENSOR_CONF_CONTENT_UINT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TEMP_OV_ALERT_CDEGREE:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      tpret = SENSOR_CONF_CONTENT_FLOAT;
      break;
    }
    case
      SKFChina_Common_SpecificConfigItem_TEMPERATURE_ALARM_THRESHOLD_CDGREE
      :
    {
      // SENSOR_CONF_CONTENT_INT32
      tpret = SENSOR_CONF_CONTENT_INT32;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_OV_ALERT:
    case SKFChina_Common_SpecificConfigItem_ACC_OV_ALARM:
    case SKFChina_Common_SpecificConfigItem_ACC_HAL_ALERT:
    case SKFChina_Common_SpecificConfigItem_ACC_HAL_ALARM:
    case SKFChina_Common_SpecificConfigItem_VEL_OV_ALERT:
    case SKFChina_Common_SpecificConfigItem_VEL_OV_ALARM:
    case SKFChina_Common_SpecificConfigItem_VEL_HAL_ALERT:
    case SKFChina_Common_SpecificConfigItem_VEL_HAL_ALARM:
    case SKFChina_Common_SpecificConfigItem_ENV_OV_ALERT:
    case SKFChina_Common_SpecificConfigItem_ENV_OV_ALARM:
    case SKFChina_Common_SpecificConfigItem_ENV_HAL_ALERT:
    case SKFChina_Common_SpecificConfigItem_ENV_HAL_ALARM:
    {
      // SENSOR_CONF_CONTENT_FLOAT
      tpret = SENSOR_CONF_CONTENT_FLOAT;
      break;
    }
    default:
    {
      DBG_LOG_ERR("Unknown configuration item!\r\n");
      break;
    }
  }
  return tpret;
}



/*to operate a PB encoding/decoding test
ret@ST_OK is returned when things go well
*/
app_state_t util_froto_test(void)
{
	app_state_t tpret = ST_OK;
	pb_byte_t kp_buf[0x1000] = {0};
	pb_ostream_t kp_ostream = pb_ostream_from_buffer( kp_buf, sizeof(kp_buf));
	SKFChina_App_AppMessage kp_msg = SKFChina_App_AppMessage_init_default;
	kp_msg.appVer = BULLET_SENSOR_FROTO_APP_VERSION;
	if(false == pb_encode( &kp_ostream, SKFChina_App_AppMessage_fields, (void*)&kp_msg))
	{
		DBG_LOG_ERR("fail to encode message");
		tpret = ST_ERR;
		return tpret;
	}
	else
	{
		DBG_LOG_INFO("bytes_written is %d", kp_ostream.bytes_written);
	}

	// to decode
	SKFChina_App_AppMessage kp_msg_d = SKFChina_App_AppMessage_init_zero;
	pb_istream_t kp_istream = pb_istream_from_buffer( kp_buf, kp_ostream.bytes_written);
	if(false == pb_decode( &kp_istream, SKFChina_App_AppMessage_fields, (void *)&kp_msg_d))
	{
		DBG_LOG_ERR("fail to decode the message");
		tpret = ST_ERR;
		return tpret;
	}
	else
	{
		DBG_LOG_INFO("the appver from message decoded is %d", kp_msg_d.appVer);
	}
	return tpret;
}



/*to encode message config_dissem
ret@ pointer points to the buffer where the encoding-data is 
NOTE!!! Remember to release the memory when you do not need it any more
*/
pb_byte_t * util_froto_encode_msg(const SKFChina_App_AppMessage *pt_msg, size_t *kp_bytes_written)
{
	pb_byte_t*pt_ret = NULL;
	uint32_t kp_bufsz = sizeof(SKFChina_App_AppMessage);
	pb_ostream_t kp_ostream = {0};
	
	if(pt_msg == NULL)
	{
		DBG_LOG_ERR(" unexpected pointer NULL");
		pt_ret = NULL;
		return pt_ret;
	}

	#warning "TODO====to make sure the message has valid value!"
	
	DBG_LOG_INFO("the buffer size for encoding %d", kp_bufsz);

	pt_ret = (pb_byte_t*)l_malloc( kp_bufsz);
	if(pt_ret == NULL)
	{
		DBG_LOG_ERR("fail to allocate memory for encoding-op");
		return pt_ret;
	}
	memset( pt_ret, 0, kp_bufsz);

	kp_ostream = pb_ostream_from_buffer( pt_ret, kp_bufsz);
	if(false == pb_encode( &kp_ostream, SKFChina_App_AppMessage_fields, (const void*)pt_msg))
	{
		l_free( pt_ret);

		DBG_LOG_ERR("fail to encode message, %s", kp_ostream.errmsg);
		
		pt_ret = NULL;
		return pt_ret;
	}


	DBG_LOG_INFO("the message(tag %d) is encoded successfully, bufsz %d, bytes_writtent %d", pt_msg->which__messages, kp_bufsz, kp_ostream.bytes_written);

	*kp_bytes_written = kp_ostream.bytes_written;
	
	return pt_ret;
}

/*to release the buffer returned by util_froto_encode_msg
ret@none
*/
void util_froto_release_encoded_msg(pb_byte_t*pt_buf)
{
	if(pt_buf)
	{
		l_free( pt_buf);
	}
}


void util_froto_release_encoded_msg_v2(struct encoded_froto_msg_pkt *pt_encoded_pkt)
{
	if(NULL == pt_encoded_pkt)
	{
		return;
	}
	if(true == pt_encoded_pkt->is_valid)
	{
		DBG_LOG_INFO("to release the encoded msg pkt @ 0x%x", pt_encoded_pkt->ptbuf);
		l_free( pt_encoded_pkt->ptbuf);
	}
}

/*
bool (*encode)(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
*/
static bool encode_uint32_t(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	SKFChina_Froto_SeqNoElement *pt_seq_no = (SKFChina_Froto_SeqNoElement*)*arg;
	DBG_LOG_DEBUG("to encode %d", pt_seq_no->seqNo);
	#if(1)	
		//return pb_encode_tag_for_field( stream, field) && pb_encode_fixed32( stream, (const void*)&pt_u32);
		return encode_submessage( stream, SKFChina_Froto_FrotoHeader_fields, SKFChina_Froto_SeqNoElement_fields, pt_seq_no);
	#else
		return pb_encode_svarint( stream, pt_u32);
	#endif
}




/*to fill the froto-header
ret@ST_OK is returned when things go well
*/
app_state_t util_froto_fill_header(SKFChina_Froto_FrotoHeader*pt_fheader, protobuf_encode_general_bytes_t*pt_s_id_buf, SKFChina_Froto_SeqNoElement *pt_seq_no)
{
	app_state_t tpret = ST_OK;
	if(pt_fheader == NULL)
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		return tpret;
	}
	pt_fheader->version = 1;

	#warning "===to fill this member"
	pt_fheader->sensor_id.arg = pt_s_id_buf;
	pt_fheader->sensor_id.funcs.encode = protobuf_encode_bytes_callback;

	pt_fheader->which_peer_addr = SKFChina_Froto_FrotoHeader_cloud_tag;
	#warning "===to fill"
	pt_fheader->peer_addr.cloud = true;

	pt_fheader->is_up = false;

	pt_fheader->message_seq_no = 999;

	pt_fheader->has_ack_window_size_message = false;
	pt_fheader->ack_window_size_message = 666;

	pt_fheader->long_packet_id = 666;

	pt_fheader->has_current_block = false;

	pt_fheader->current_block = 0;
	
	pt_fheader->has_is_big_endian = false;
	pt_fheader->is_big_endian = false;

	#warning "===to fill"
	#if(1)
		pt_fheader->acked_message_seq_number.arg = pt_seq_no;
		pt_fheader->acked_message_seq_number.funcs.encode = encode_uint32_t;
	#endif
	return tpret;
}

/*
bool (*encode)(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
*/
static bool protobuf_encode_bool_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	#if(0)
	for (uint8_t i = 0; i < 4; i++) {
        pb_encode_tag(stream_p, PB_WT_VARINT, 1);
        pb_write(stream_p, &testboolarr[i], 1);
    }
    return true;
	#else
	protobuf_encode_general_bytes_t *pt_protobuf = (protobuf_encode_general_bytes_t*)*arg;
	if(pt_protobuf == NULL)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	DBG_LOG_DEBUG("the bool buf size %d", pt_protobuf->size);
	
	//for(uint32_t i = 0; i < pt_protobuf->size; i++)	
	for(uint32_t i = 0; i < pt_protobuf->size; i++)
	{
		pb_encode_tag( stream_p, PB_WT_VARINT, 1);
		pb_write( stream_p, &pt_protobuf->buffer[i], 1);
	}
	DBG_LOG_DEBUG("bool_encoded successfully");
	return true;
	#endif
}


static bool protobuf_encode_configpair_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{    
	#if(1)
    	SKFChina_ConfigurationAndCommand_ConfigPair *ConfigPair             = (SKFChina_ConfigurationAndCommand_ConfigPair *)(*arg);
		return encode_submessage(stream_p, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, SKFChina_ConfigurationAndCommand_ConfigPair_fields, ConfigPair);
	#else
		#warning "Note!==== double check when things go wrong."
		return pb_encode_tag_for_field( stream_p, field) && pb_encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_ConfigPair_fields, arg);
	#endif
}

static bool protobuf_encode_data_pair_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	bool tpret = false;
	SKFChina_SensingDataUpload_DataPair *pt_dtpair = (SKFChina_SensingDataUpload_DataPair*)*arg;
	tpret = encode_submessage( stream_p, SKFChina_SensingDataUpload_DataUpload_fields, SKFChina_SensingDataUpload_DataPair_fields, pt_dtpair);
	return tpret;
}

bool protobuf_encode_int_array_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	uint64_t tpval = 666;
	uint64_t *pt_u64 = NULL;
	if((NULL == arg))
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	pt_u64 = (uint64_t*)*arg;
	
	DBG_LOG_DEBUG("to encode svarint");
	for(int i = 0; i < 2; i++)
	{
		//if(false == pb_encode_tag_for_field(stream_p, field))
		if(false == pb_encode_tag( stream_p, PB_WT_VARINT, 1))
		{
			DBG_LOG_ERR("fail to encode tag, %s", stream_p->errmsg);
			return false;
		}
		DBG_LOG_DEBUG("the svarint %d", *pt_u64);
		if(false == pb_write(stream_p, (const pb_byte_t *)&tpval , 8)) //( stream_p, 666))
		{
			DBG_LOG_ERR("fail to encode svarint, %s", stream_p->errmsg);
			return false;
		}
	}
	DBG_LOG_DEBUG("exit encoding svarint");
	
	return true;
	
}



static bool protobuf_encode_config_set_time_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{

	#if(0)
	uint64_t kpu64 = 666;	
	SKFChina_ConfigurationAndCommand_TimeArray kp_timearray = {0};
	kp_timearray.time.arg = &kpu64;
	kp_timearray.time.funcs.encode = protobuf_encode_int_array_callback; //protobuf_encode_svarint_callback;
	return encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_TimeArray_fields, SKFChina_ConfigurationAndCommand_TimeArrayElement_fields, &kp_timearray);
	#else
	
	uint64_t kpu64 = *((time_t*)*arg);	
	SKFChina_ConfigurationAndCommand_TimeArray kp_timearray = {0};
	//kp_timearray.time.arg = &kpu64;
	//kp_timearray.time.funcs.encode = protobuf_encode_svarint_callback(pb_ostream_t * stream_p, const pb_field_t * field, void * const * arg)//protobuf_encode_int_array_callback; //protobuf_encode_svarint_callback;
	for(int i = 0; i < 1; i++)
	{
		 encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_TimeArray_fields, SKFChina_ConfigurationAndCommand_TimeArrayElement_fields, &kpu64);
	}	
	return true;
	
	#endif
}


static bool decode_bool_callback(pb_istream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	bool tp_bval = false;
	DBG_LOG_DEBUG("stream_p->bytes_left = %d", stream_p->bytes_left);

	if(!pb_decode( stream_p, SKFChina_ConfigurationAndCommand_BoolArrayElement_fields, &tp_bval))
	{
		DBG_LOG_ERR("fail to decode BoolArrayElement_fiels %s", stream_p->errmsg);
		return false;
	}
	DBG_LOG_DEBUG("BoolArrayElement is decoded, val %d", tp_bval);
	return true;
}

static bool decode_configpair_bool_arr_callback(pb_istream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	pb_istream_t kp_istream = pb_istream_from_buffer( stream_p->state, stream_p->bytes_left);
	SKFChina_ConfigurationAndCommand_ConfigPair *pt_cfgpair_buf = (SKFChina_ConfigurationAndCommand_ConfigPair*)*arg;
	memset( pt_cfgpair_buf, 0, sizeof(SKFChina_ConfigurationAndCommand_ConfigPair));
	if(!pb_decode( &kp_istream, SKFChina_ConfigurationAndCommand_ConfigPair_fields, pt_cfgpair_buf))
	{
		DBG_LOG_ERR("fail to decode ConfigPair");
		return false;
	}
	if(pt_cfgpair_buf->which_config_item != SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag)
	{
		DBG_LOG_ERR("tag %d is expected", SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag);
		return false;
	}
	if(pt_cfgpair_buf->config_item.fscheduler_config_item != SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WSSTAR_ARRAY)
	{
		DBG_LOG_ERR("tag %d is expected", SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WSSTAR_ARRAY);
		return false;
	}

	pt_cfgpair_buf->config_content.bool_config_content.YesOrNo.arg = &pt_cfgpair_buf->config_item.fscheduler_config_item;
	pt_cfgpair_buf->config_content.bool_config_content.YesOrNo.funcs.decode = decode_bool_callback;

	decode_one_filed( stream_p, SKFChina_ConfigurationAndCommand_ConfigPair_bool_config_content_tag, SKFChina_ConfigurationAndCommand_BoolArray_fields, &pt_cfgpair_buf->config_content.bool_config_content.YesOrNo);

	return pb_decode( stream_p, SKFChina_ConfigurationAndCommand_ConfigPair_fields,  pt_cfgpair_buf);
	
}


static bool decode_bytes_callback(pb_istream_t *stream_p, const pb_field_t *field, void **arg)
{
    if (arg != NULL) {
        protobuf_encode_general_bytes_t *temp_protobuf_decode_bytes = (protobuf_encode_general_bytes_t *)(*arg);
        if (stream_p->bytes_left > temp_protobuf_decode_bytes->size)
            return false; /* overflow */
		
		DBG_LOG_DEBUG("Bytes_left %d", stream_p->bytes_left);

		if (pb_read(stream_p, temp_protobuf_decode_bytes->buffer, stream_p->bytes_left))
        {
        	DBG_LOG_DEBUG("%X-%X",temp_protobuf_decode_bytes->buffer[0], temp_protobuf_decode_bytes->buffer[1]);
            return true;
        }
    }

    return false;
}


/*to decode msg config_dissem
pt_dtbuf : the buffer where the encoded data is
*/
static app_state_t msg_config_dissem_decode(pb_byte_t*pt_dtbuf, uint32_t kp_msglen)
{
	app_state_t tpret = ST_OK;
	pb_istream_t kp_istream = {0};
	pb_istream_t kp_substream = {0};
	
	SKFChina_App_AppMessage kp_msg_d ;//= SKFChina_App_AppMessage_init_default;
	SKFChina_ConfigurationAndCommand_ConfigDisseminate kp_config_info = SKFChina_ConfigurationAndCommand_ConfigDisseminate_init_zero;
	
	
	DBG_LOG_INFO("going to decode %d @ 0x%x", kp_msglen, pt_dtbuf);

	kp_istream = pb_istream_from_buffer( pt_dtbuf, kp_msglen);


	#if(1)
		if(false == pb_decode( &kp_istream, SKFChina_App_AppMessage_fields, &kp_msg_d))
		{
			DBG_LOG_ERR("fail to decode AppMessage, %s", kp_istream.errmsg);
			return ST_ERR;
		}
		else
		{
			DBG_LOG_INFO("decoding successfully, which_message %d, appVer %d", kp_msg_d.which__messages, kp_msg_d.appVer);
		}
		if(kp_msg_d.which__messages != SKFChina_App_AppMessage_config_dissem_tag)
		{
			DBG_LOG_ERR("message with tag %d is expected", SKFChina_App_AppMessage_config_dissem_tag);
			return ST_ERR;
		}

		protobuf_encode_general_bytes_t kp_sensor_id_a = {0};
		kp_sensor_id_a.size = 0x10;
		kp_sensor_id_a.buffer = l_malloc(kp_sensor_id_a.size);
		memset( kp_sensor_id_a.buffer, 0, kp_sensor_id_a.size);

		protobuf_encode_general_bytes_t kp_sensor_id_b = {0};
		kp_sensor_id_b.size = 0x10;
		kp_sensor_id_b.buffer = l_malloc( kp_sensor_id_b.size);
		memset(kp_sensor_id_b.buffer, 0, kp_sensor_id_b.size);

		SKFChina_ConfigurationAndCommand_ConfigPair kp_config_pair = {0};

		DBG_LOG_DEBUG("istream->bytes_left = %d", kp_istream.bytes_left);
		memset( &kp_istream, 0, sizeof(kp_istream));
		kp_istream = pb_istream_from_buffer( pt_dtbuf, kp_msglen);
		
		decode_unionmessage_type( &kp_istream, SKFChina_App_AppMessage_fields);
		if(!pb_make_string_substream( &kp_istream, &kp_substream))
		{
			DBG_LOG_ERR("fail to make string substream, %s", kp_istream.errmsg);
			tpret = ST_ERR;
			goto EXIT;
		}

		kp_msg_d._messages.config_dissem.sensor_id.arg = &kp_sensor_id_a;
		kp_msg_d._messages.config_dissem.sensor_id.funcs.decode = decode_bytes_callback;

		kp_msg_d._messages.config_dissem.header.sensor_id.arg = &kp_sensor_id_b;
		kp_msg_d._messages.config_dissem.header.sensor_id.funcs.decode = decode_bytes_callback;

		kp_msg_d._messages.config_dissem.config_pair.arg = &kp_config_pair;
		kp_msg_d._messages.config_dissem.config_pair.funcs.decode = decode_configpair_bool_arr_callback;

		if(false == pb_decode( &kp_substream, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, &kp_msg_d._messages.config_dissem))
		{
			DBG_LOG_ERR("fail to decode configpair");
			tpret = ST_ERR;
			goto EXIT;
		}
		DBG_LOG_INFO("the ConfigPair is decoded successfully! hash_value %d", kp_msg_d._messages.config_hash_upload.config_hash_value);
	#else

	decode_unionmessage_type( &kp_istream, SKFChina_App_AppMessage_fields);
	if(!pb_make_string_substream( &kp_istream, &kp_substream))
	{
		DBG_LOG_ERR("make_string_substream fail");
		return ST_ERR;
	}
	DBG_LOG_INFO("Going to decode");
		char kp_buf_sensor_id[32] = {0};
		protobuf_encode_general_bytes_t kp_sensor_id = {0};
		kp_sensor_id.buffer = kp_buf_sensor_id;
		kp_sensor_id.size = sizeof(kp_buf_sensor_id);

		
		char kp_buf_sensor_id_2[32] = {0};
		protobuf_encode_general_bytes_t kp_sensor_id_2 = {0};
		kp_sensor_id.buffer = kp_buf_sensor_id_2;
		kp_sensor_id.size = sizeof(kp_buf_sensor_id_2);

		SKFChina_ConfigurationAndCommand_ConfigPair kp_config_pair = SKFChina_ConfigurationAndCommand_ConfigPair_init_zero;

		kp_config_info.sensor_id.arg = &kp_buf_sensor_id_2;
		kp_config_info.sensor_id.funcs.decode = protobuf_decode_bytes_callback;

		kp_config_info.header.sensor_id.arg = &kp_buf_sensor_id;
		kp_config_info.header.sensor_id.funcs.decode = protobuf_decode_bytes_callback;

		kp_config_pair.config_content.bool_config_content.YesOrNo.arg = ;
		kp_config_pair.config_content.bool_config_content.YesOrNo.funcs.decode = ;
		
		#error "TODO====continue from here==============="	
		pb_close_string_substream( &kp_istream, &kp_substream);
	#endif
	EXIT:
		if(kp_sensor_id_a.buffer)
		{
			l_free( kp_sensor_id_a.buffer);
			kp_sensor_id_a.buffer = NULL;
		}
		if(kp_sensor_id_b.buffer)
		{
			l_free(kp_sensor_id_b.buffer);
			kp_sensor_id_b.buffer = NULL;
		}
		pb_close_string_substream( &kp_istream, &kp_substream);
		return tpret;

}


#if(0)
/*to fill data structure for message config-dissem
ret@ST_OK when things go well
*/
app_state_t util_froto_fill_msg_config_dissem(void)
{
	app_state_t tpret = ST_OK;

	size_t kp_bytes_written = 0;
	
	pb_byte_t *pt_encoded_bytes = NULL;


	SKFChina_App_AppMessage kp_msg = {0};
	
	// the seq-no
	SKFChina_Froto_SeqNoElement kp_seq_no = {.seqNo = 123};
	
	// to fill the sensor id
	protobuf_encode_general_bytes_t kp_sensor_id = {0};
	kp_sensor_id.size = 6;
	#warning "TODO===to release the memory!"
	kp_sensor_id.buffer = l_malloc(kp_sensor_id.size); 
	memset( kp_sensor_id.buffer, 0, kp_sensor_id.size);
	snprintf( kp_sensor_id.buffer, kp_sensor_id.size, "%s", "ID123");

	// the config-pair
	protobuf_encode_general_bytes_t  kp_bool_arr = {0};
	kp_bool_arr.size = 4;
	#warning "TODO===to release the memory!"
	kp_bool_arr.buffer = (void*)l_malloc(kp_bool_arr.size * sizeof(bool));
	memset( kp_bool_arr.buffer, 0, kp_bool_arr.size * sizeof(bool));
	
	SKFChina_ConfigurationAndCommand_ConfigPair kp_config_pair;
	kp_config_pair.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag;
	kp_config_pair.config_item.fscheduler_config_item = SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WSSTAR_ARRAY;
	kp_config_pair.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_bool_config_content_tag;
	
	kp_config_pair.config_content.bool_config_content.YesOrNo.funcs.encode = protobuf_encode_bool_callback;
	kp_config_pair.config_content.bool_config_content.YesOrNo.arg = &kp_bool_arr;
	kp_config_pair.has_memory_id = false;
	kp_config_pair.memory_id = 105;
	
	#if(1) // for test
		// the header 

	
		kp_msg.appVer = 1;
		kp_msg.which__messages = SKFChina_App_AppMessage_config_dissem_tag;
		// the config_dissem
		kp_msg._messages.config_dissem.has_header = true;

		#warning "===to fill the header"
		//kp_msg._messages.config_dissem.header;
		util_froto_fill_header( &kp_msg._messages.config_dissem.header, &kp_sensor_id, &kp_seq_no);
			

		kp_msg._messages.config_dissem.appVer = 1;
		
		#warning " === to fill the sensor_id"
		kp_msg._messages.config_dissem.sensor_id.arg = &kp_sensor_id;
		kp_msg._messages.config_dissem.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

		kp_msg._messages.config_dissem.product = SKFChina_Common_ProductType_BULLET_NODE;

		#warning " === to fill config_pair"
		#if(1)
			kp_msg._messages.config_dissem.config_pair.arg = &kp_config_pair;
			kp_msg._messages.config_dissem.config_pair.funcs.encode = protobuf_encode_configpair_callback;
		#endif

		kp_msg._messages.config_dissem.has_is_big_endian = false;
		kp_msg._messages.config_dissem.is_big_endian = false;
	#endif

	DBG_LOG_INFO("Going to encode msg");
	
	pt_encoded_bytes = util_froto_encode_msg(&kp_msg, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		DBG_LOG_INFO("the message is encoded successfully!");
		#warning "TODO=========to send the message"
		#if(1) // for test
			DBG_LOG_INFO("BKP317 kp_bytes_written %d @ 0x%x", kp_bytes_written, pt_encoded_bytes);
			util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
			msg_config_dissem_decode( pt_encoded_bytes, kp_bytes_written);
		#endif
	}
	else
	{
		DBG_LOG_ERR("fail to encode config_dissem message");
		tpret = ST_ERR;
		goto EXIT;
	}
	util_froto_release_encoded_msg(pt_encoded_bytes);

	EXIT:
		// to release the memroy
		if(kp_sensor_id.buffer)
		{
			l_free( kp_sensor_id.buffer);
			kp_sensor_id.buffer = NULL;
		}
		if(kp_bool_arr.buffer)
		{
			l_free( kp_bool_arr.buffer);
			kp_bool_arr.buffer = NULL;
		}
		return tpret;
}
#else


#if(1)
	
/*ref@SKFChina_ConfigurationAndCommand_ConfigPair;
	    bytes                             general_config_content = 2;
        TimeArray                         time_config_content = 3;
        WakeUpToSenseDesc                 measurement_description = 4;
        BoolArray                         bool_config_content = 5;
        uint32                            general_config_content_uint32 = 7;
        int32                             general_config_content_int32 = 8;
        float                             general_config_content_float = 9;
        WorkModePair                      work_mode_content = 11;
        GatewayInfoPair                   gateway_info_content = 12;
        bool                              general_config_content_bool = 13;
*/
#endif

static bool to_encode_config_pair_content_general_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	bool tpret = true;
	protobuf_encode_general_bytes_t *pt_bytes_pkt = (protobuf_encode_general_bytes_t*)*arg;
	if((NULL == stream_p)||(NULL == field)||(NULL == arg))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}
	if(!pb_encode_tag_for_field( stream_p, field))
	{
		DBG_LOG_ERR(" fail to find tag for field");
		tpret = false;
		return tpret;
	}

	if(!pb_encode_string( stream_p, pt_bytes_pkt->buffer, pt_bytes_pkt->size))
	{
		DBG_LOG_ERR("fail to encode string");
		tpret = false;
		return tpret;
	}
	return tpret;
}

static bool to_encode_config_pair_content_time_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	bool tpret = true;
	struct config_content_packet *pt_content_pkt = NULL;
	if((NULL == stream_p)||(field == NULL)||(arg == NULL))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}
	pt_content_pkt = (struct config_content_packet*)*arg;

	if(pt_content_pkt->conf_data.time_config_content.num <= 0)
	{
		DBG_LOG_ERR("invalid data num");
		tpret = false;
		return tpret;
	}

	for(uint32_t i = 0; i < pt_content_pkt->conf_data.time_config_content.num; i++)
	{
		if(false == encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_TimeArray_fields, SKFChina_ConfigurationAndCommand_TimeArrayElement_fields, &pt_content_pkt->conf_data.time_config_content.repeat[i]))
		{
			DBG_LOG_ERR("fail to encode time array");
			tpret = false;
			break;
		}
	}
	return tpret;
}

static bool to_encode_config_pair_content_boolarray_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	bool tpret = true;
	struct config_content_packet *pt_content_pkt = NULL;
	if((NULL == stream_p)||(field == NULL)||(arg == NULL))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}
	pt_content_pkt = (struct config_content_packet*)*arg;
	if(pt_content_pkt->conf_data.bool_config_content.num <= 0)
	{
		DBG_LOG_ERR("invalid element num");
		tpret = false;
		return tpret;
	}
	for(uint32_t i = 0; i < pt_content_pkt->conf_data.bool_config_content.num; i++)
	{
		if(false == encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_BoolArray_fields, SKFChina_ConfigurationAndCommand_BoolArrayElement_fields, (void *)pt_content_pkt->conf_data.bool_config_content.repeat[i]))
		{
			DBG_LOG_ERR("fail to encode bool array");
			tpret = false;
			break;
		}
	}
	return tpret;
}

static bool to_encode_config_pair_content_workmode_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	bool tpret = true;
	struct config_content_packet *pt_config_pkt = NULL;
	if((NULL == stream_p)||(NULL == field)||(NULL == arg))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}
	pt_config_pkt = (struct config_content_packet*)*arg;
	if(pt_config_pkt->conf_data.work_mode_content.num <= 0)
	{
		DBG_LOG_ERR("invalid element numbers");
		tpret = false;
		return tpret;
	}
	if(!pb_encode_tag_for_field( stream_p, field))
	{
		DBG_LOG_ERR("fail to encode tag for field");
		tpret = false;
		return tpret;
	}
	if(!pb_encode_string( stream_p, pt_config_pkt->conf_data.work_mode_content.para, pt_config_pkt->conf_data.work_mode_content.num))
	{
		DBG_LOG_ERR("fail to encode bytes");
		tpret = false;
		return tpret;
	}
	return tpret;
}

static bool to_encode_config_pair_content_gwinfo_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	bool tpret = true;
	struct config_content_packet *pt_config_pkt = NULL;
	if((NULL == stream_p)||(NULL == field)||(NULL == arg))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}
	pt_config_pkt = (struct config_content_packet*)*arg;

	if(pt_config_pkt->conf_data.gateway_info_content.num <= 0)
	{
		DBG_LOG_ERR("unexpected num value");
		tpret = false;
		return tpret;
	}
	if(!pb_encode_tag_for_field( stream_p, field))
	{
		DBG_LOG_ERR("fail to encode tag for field");
		tpret = false;
		return tpret;
	}
	if(!pb_encode_string( stream_p, pt_config_pkt->conf_data.gateway_info_content.mac_address, pt_config_pkt->conf_data.gateway_info_content.num))
	{
		DBG_LOG_ERR("fail to encode bytes");
		tpret = false;
		return tpret;
	}
	return tpret;
}

/*to get the 
*/

static bool protobuf_encode_configpair_callback_v2(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	bool tpret = true;
	struct conf_dissem_info *pt_conf_dissem = (struct conf_dissem_info*)*arg;
	SKFChina_Common_SpecificConfigItem kp_specific_conf_item = 0;
	enum conf_content_type kp_content_type = SENSOR_CONF_CONTENT_UNDEFINED;
	struct config_content_packet kp_content_pkt = {0};

	SKFChina_ConfigurationAndCommand_ConfigPair kp_cfg_pair = {0};
		
	if(NULL == pt_conf_dissem)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		goto EXIT;
	}
	
	for(uint32_t i = 0; i < pt_conf_dissem->item_num; i++)
	{		
		memset( &kp_content_pkt, 0, sizeof(struct config_content_packet));// clean the content buffer
		memset( &kp_cfg_pair, 0, sizeof(SKFChina_ConfigurationAndCommand_ConfigPair));

		kp_specific_conf_item = pt_conf_dissem->conf_item_array.specific[i % MAX_CONF_ITEM_DISSEM];		
		kp_content_pkt = pick_conf_content_by_item_id( kp_specific_conf_item, pt_conf_dissem->pt_sensor_conf);
		kp_content_type = map_conf_item_to_content_type(kp_specific_conf_item);

		if(true != kp_content_pkt.is_data_valid)
		{
			DBG_LOG_WARN("fail to get the content pkt for conf-item %d", kp_specific_conf_item);
			continue;
		}
		
		DBG_LOG_DEBUG("to encode conf_item=%d,content_type=%d ", kp_specific_conf_item,kp_content_type);			
		switch(kp_content_type)
		{
			case SENSOR_CONF_CONTENT_BYTES:// = 1,
			{
				protobuf_encode_general_bytes_t tp_bytes_pkt = {0};

				if(kp_content_pkt.conf_data.general_config_content.num <= 0)
				{
					DBG_LOG_ERR("well this is not suppose to happen");
					break;
				}
				tp_bytes_pkt.buffer = (uint8_t*)l_malloc(kp_content_pkt.conf_data.general_config_content.num);
				if(NULL == tp_bytes_pkt.buffer)
				{
					DBG_LOG_ERR("malloc fail");
					break;
				}
				memset( tp_bytes_pkt.buffer, 0, kp_content_pkt.conf_data.general_config_content.num);
				memcpy( tp_bytes_pkt.buffer, kp_content_pkt.conf_data.general_config_content.repeat, kp_content_pkt.conf_data.general_config_content.num);
				
				// which item
				kp_cfg_pair.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
				kp_cfg_pair.config_item.specific_config_item = kp_specific_conf_item;
				// which content
				kp_cfg_pair.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_tag;
				kp_cfg_pair.config_content.general_config_content.arg = &tp_bytes_pkt;
				kp_cfg_pair.config_content.general_config_content.funcs.encode = to_encode_config_pair_content_general_callback;
				
				encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, SKFChina_ConfigurationAndCommand_ConfigPair_fields, (void *)&kp_cfg_pair);	

				// to rleease the memory
				l_free( tp_bytes_pkt.buffer);				
				break;
			}
			case SENSOR_CONF_CONTENT_TIMEARRAY:// = 2,
			{

				// which item
				kp_cfg_pair.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
				kp_cfg_pair.config_item.specific_config_item = kp_specific_conf_item;
				// which content
				kp_cfg_pair.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
				kp_cfg_pair.config_content.time_config_content.time.arg = &kp_content_pkt;
				kp_cfg_pair.config_content.time_config_content.time.funcs.encode = to_encode_config_pair_content_time_callback;
				
				encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, SKFChina_ConfigurationAndCommand_ConfigPair_fields,  (void *)&kp_cfg_pair);
	
				break;
			}
			case SENSOR_CONF_CONTENT_WAKEUP_TO_SENSEDESC:// = 3,
			{
				DBG_LOG_ERR("You are in trouble when you see this output!");
				break;
			}
			case SENSOR_CONF_CONTENT_BOOL_ARRAY:// = 4,
			{
				// which item
				kp_cfg_pair.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
				kp_cfg_pair.config_item.specific_config_item = kp_specific_conf_item;
				// which content
				kp_cfg_pair.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_bool_config_content_tag;
				kp_cfg_pair.config_content.bool_config_content.YesOrNo.arg = &kp_content_pkt;
				kp_cfg_pair.config_content.bool_config_content.YesOrNo.funcs.encode = to_encode_config_pair_content_boolarray_callback;
				
				encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, SKFChina_ConfigurationAndCommand_ConfigPair_fields,  (void *)&kp_cfg_pair);
				break;
			}
			case SENSOR_CONF_CONTENT_UINT32:// = 5,
			{
				// which item
				kp_cfg_pair.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
				kp_cfg_pair.config_item.specific_config_item = kp_specific_conf_item;
				//which content
				kp_cfg_pair.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
				kp_cfg_pair.config_content.general_config_content_uint32 = kp_content_pkt.conf_data.general_config_content_uint32;
				encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, SKFChina_ConfigurationAndCommand_ConfigPair_fields,  (void *)&kp_cfg_pair);								
				break;
			}
			case SENSOR_CONF_CONTENT_INT32:// = 6,
			{
				// which item
				kp_cfg_pair.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
				kp_cfg_pair.config_item.specific_config_item = kp_specific_conf_item;
				// which content
				kp_cfg_pair.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag;
				kp_cfg_pair.config_content.general_config_content_int32 = kp_content_pkt.conf_data.general_config_content_int32;
				encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, SKFChina_ConfigurationAndCommand_ConfigPair_fields,  (void *)&kp_cfg_pair);																
				break;
			}
			case SENSOR_CONF_CONTENT_FLOAT:// = 7,
			{
				// which item
				kp_cfg_pair.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
				kp_cfg_pair.config_item.specific_config_item = kp_specific_conf_item;
				// which content
				kp_cfg_pair.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
				kp_cfg_pair.config_content.general_config_content_float = kp_content_pkt.conf_data.general_config_content_float;
				encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, SKFChina_ConfigurationAndCommand_ConfigPair_fields,  (void *)&kp_cfg_pair);	
				break;
			}
			case SENSOR_CONF_CONTENT_WORK_MODE_PAIR:// = 8,
			{
				// which item
				kp_cfg_pair.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
				kp_cfg_pair.config_item.specific_config_item = kp_specific_conf_item;
				// which content
				kp_cfg_pair.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag;
				kp_cfg_pair.config_content.work_mode_content.work_mode = kp_content_pkt.conf_data.work_mode_content.work_mode;
				kp_cfg_pair.config_content.work_mode_content.parameter.arg = &kp_content_pkt;
				kp_cfg_pair.config_content.work_mode_content.parameter.funcs.encode = to_encode_config_pair_content_workmode_callback;
				encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, SKFChina_ConfigurationAndCommand_ConfigPair_fields,  (void *)&kp_cfg_pair);					
				break;
			}
			case SENSOR_CONF_CONTENT_GATEWAY_INFO_PAIR:// = 9,
			{
				// which item
				kp_cfg_pair.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
				kp_cfg_pair.config_item.specific_config_item = kp_specific_conf_item;
				// which content
				kp_cfg_pair.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag;
				kp_cfg_pair.config_content.gateway_info_content.name_id = kp_content_pkt.conf_data.gateway_info_content.name_id;
				kp_cfg_pair.config_content.gateway_info_content.mac_address.arg = &kp_content_pkt;
				kp_cfg_pair.config_content.gateway_info_content.mac_address.funcs.encode = to_encode_config_pair_content_gwinfo_callback;				
				encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, SKFChina_ConfigurationAndCommand_ConfigPair_fields,  (void *)&kp_cfg_pair);					
				break;
			}
			case SENSOR_CONF_CONTENT_BOOL:// = 10 // bool and uint8
			{
				// which item
				kp_cfg_pair.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
				kp_cfg_pair.config_item.specific_config_item = kp_specific_conf_item;
				// which content
				kp_cfg_pair.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
				kp_cfg_pair.config_content.general_config_content_bool = kp_content_pkt.conf_data.general_config_content_bool;
				encode_submessage( stream_p, SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields, SKFChina_ConfigurationAndCommand_ConfigPair_fields,  (void *)&kp_cfg_pair);	
				break;
			}
			default:
			{	
				DBG_LOG_WARN("unexpected content type, double check to make things go well");
				break;
			}
		}
	}
	EXIT:
		return tpret;
}

/*to fill froto msg to be sent to sensor to set specific configuration
pt_conf_id_array@the config-item to set
kp_array_sz @ the array size
pt_sensor_conf@ the configuraion to be set to sensor
pt_sensor_macstr@the sensor mac-string
pt_gw_macstr @ the gateway mac-string
kp_seq_no @ the message seq-no

ret@ the encoded msg is returned when things go well

NOTE!! the following function only deal with SKFChina_Common_SpecificConfigItem
*/
struct encoded_froto_msg_pkt  util_froto_fill_msg_specific_config_dissem(const struct conf_dissem_info*pt_conf_dissem)
{
	struct encoded_froto_msg_pkt kp_ret ={.is_valid = false, 0};
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	if(NULL == pt_conf_dissem)
	{
		DBG_LOG_ERR("unexpected NULL");
		goto EXIT;
	}
	if((0 == pt_conf_dissem->item_num)||(NULL == pt_conf_dissem->pt_sensor_conf))
	{
		DBG_LOG_ERR("invalid parameter is given");
		goto EXIT;
	}
	
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};
	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	//snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s","C4BD6A100001");
	#if(0)
		idstr_to_bytes_array( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH, pt_conf_dissem->sensor_macstr);
		kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);
	#else
		kp_sensor_id_info.size = idstr_to_bytes_array( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH, pt_conf_dissem->sensor_macstr);
	#endif
	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	//snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", "112233445566");
	#if(0)	
		idstr_to_bytes_array( kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, pt_conf_dissem->gw_macstr);
		kp_gw_id_info.size = strlen(kp_gw_id_info.buffer);
	#else
		kp_gw_id_info.size = idstr_to_bytes_array( kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, pt_conf_dissem->gw_macstr); 
	#endif

	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_config_dissem_tag;
	// the header
	kp_msg_config._messages.config_dissem.header.version = 1;
	kp_msg_config._messages.config_dissem.header.is_up = false;
	kp_msg_config._messages.config_dissem.header.message_seq_no = pt_conf_dissem->seq_no;
	kp_msg_config._messages.config_dissem.header.time_to_live = 1;
	kp_msg_config._messages.config_dissem.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.config_dissem.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.config_dissem.header.total_block = 1;
	kp_msg_config._messages.config_dissem.header.has_ack_window_size_message = false;
	kp_msg_config._messages.config_dissem.header.long_packet_id = 1;
	kp_msg_config._messages.config_dissem.header.has_current_block = false;
	kp_msg_config._messages.config_dissem.header.has_is_big_endian = false;
	
	kp_msg_config._messages.config_dissem.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.config_dissem.header.peer_addr.gateway_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.config_dissem.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;


	
	kp_msg_config._messages.config_dissem.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.config_dissem.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	
	// the palyload
	#if(0)
	kp_msg_config._messages.config_dissem.has_header = true;
	kp_msg_config._messages.config_dissem.appVer = 1;
	kp_msg_config._messages.config_dissem.product = SKFChina_Common_ProductType_BULLET_NODE;
	kp_msg_config._messages.config_dissem.has_is_big_endian = false;
	#else
		kp_msg_config._messages.config_dissem.has_header = true;
		kp_msg_config._messages.config_dissem.appVer = 1;
		//kp_msg_config._messages.config_dissem.product = SKFChina_Common_ProductType_BULLET_NODE;
		kp_msg_config._messages.config_dissem.has_is_big_endian = false;
		//DBG_LOG_ERR("BKP3208"); 
		enum sensortype kp_stype = app_pro_gen_get_active_sensor_type_v2();
		if(SS_TYPE_PREDICTP == kp_stype)
		{
			kp_msg_config._messages.config_dissem.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;
		}
		else
		{
			kp_msg_config._messages.config_dissem.product = SKFChina_Common_ProductType_BULLET_NODE;
		}
	#endif
	
	#if(0)
	kp_msg_config._messages.config_dissem.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.config_dissem.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	
	// the configuration-pair
	kp_cfg_pair_set_time.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
	kp_cfg_pair_set_time.config_item.specific_config_item = SKFChina_Common_SpecificConfigItem_CURRENT_TIME;
	kp_cfg_pair_set_time.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
	kp_cfg_pair_set_time.config_content.time_config_content.time.arg = &kp_tstamp;	
	kp_cfg_pair_set_time.config_content.time_config_content.time.funcs.encode = protobuf_encode_config_set_time_callback;

	kp_msg_config._messages.config_dissem.config_pair.arg = &kp_cfg_pair_set_time;
	kp_msg_config._messages.config_dissem.config_pair.funcs.encode = protobuf_encode_configpair_callback;
	#else
		kp_msg_config._messages.config_dissem.config_pair.arg = pt_conf_dissem;
		kp_msg_config._messages.config_dissem.config_pair.funcs.encode = protobuf_encode_configpair_callback_v2;
	#endif
	
	//DBG_LOG_INFO("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		kp_ret.ptbuf = pt_encoded_bytes;
		kp_ret.len = kp_bytes_written;
		kp_ret.is_valid = true;
		
		#if(1) // for debug only			
			DBG_LOG_DEBUG("msg config_dissem to set time is encoded successfully");
			util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
			//the folling is only for debug
			//util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);		
			//util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);
		#endif
	}
	EXIT:
		// to release the memory
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}

		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}
		
		return kp_ret;
}

#endif


/*to fill msg-config-dissem for time sychronization
retz@pointer points to the buffer where the encoded Froto message is

NOTE!!! DO NOT forget to release the buffer returned by this function. ref@util_froto_release_encoded_msg

*/
struct encoded_froto_msg_pkt util_froto_fill_msg_config_dissem_set_time( const uint8_t*pt_gwid_str, const uint8_t*pt_sensor_id, uint32_t kp_seq_no)
{
	struct encoded_froto_msg_pkt kp_ret ={.is_valid = false, 0};
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	//the configuration-pair
	time_t kp_tstamp = time(NULL);
	SKFChina_ConfigurationAndCommand_ConfigPair kp_cfg_pair_set_time = {0}; 
	
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};
	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	//snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s","C4BD6A100001");
	#if(0)
		idstr_to_bytes_array( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH, pt_sensor_id);
		kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);
	#else
		kp_sensor_id_info.size = idstr_to_bytes_array( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH, pt_sensor_id);
	#endif
	
	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	//snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", "112233445566");
	#if(0)
		idstr_to_bytes_array( kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, pt_gwid_str);
		kp_gw_id_info.size = strlen(kp_gw_id_info.buffer);
	#else
		kp_gw_id_info.size = idstr_to_bytes_array( kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, pt_gwid_str);
	#endif

	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_config_dissem_tag;
	// the header
	kp_msg_config._messages.config_dissem.header.version = 1;
	kp_msg_config._messages.config_dissem.header.is_up = false;
	kp_msg_config._messages.config_dissem.header.message_seq_no = kp_seq_no;
	kp_msg_config._messages.config_dissem.header.time_to_live = 1;
	kp_msg_config._messages.config_dissem.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.config_dissem.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.config_dissem.header.total_block = 1;
	kp_msg_config._messages.config_dissem.header.has_ack_window_size_message = false;
	kp_msg_config._messages.config_dissem.header.long_packet_id = 1;
	kp_msg_config._messages.config_dissem.header.has_current_block = false;
	kp_msg_config._messages.config_dissem.header.has_is_big_endian = false;
	
	kp_msg_config._messages.config_dissem.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.config_dissem.header.peer_addr.gateway_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.config_dissem.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;


	
	kp_msg_config._messages.config_dissem.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.config_dissem.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

	
	// the palyload
	kp_msg_config._messages.config_dissem.has_header = true;
	kp_msg_config._messages.config_dissem.appVer = 1;
	#if(0)
	kp_msg_config._messages.config_dissem.product = SKFChina_Common_ProductType_BULLET_NODE;
	#else
		enum sensortype kp_stype = app_pro_gen_get_active_sensor_type_v2();
		if(SS_TYPE_PREDICTP == kp_stype)
		{
			kp_msg_config._messages.config_dissem.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;					
		}
		else
		{
			kp_msg_config._messages.config_dissem.product = SKFChina_Common_ProductType_BULLET_NODE;				
		}
	#endif
	kp_msg_config._messages.config_dissem.has_is_big_endian = false;


	
	#if(1)
	kp_msg_config._messages.config_dissem.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.config_dissem.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

	// the configuration-pair
	kp_cfg_pair_set_time.which_config_item = SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
	kp_cfg_pair_set_time.config_item.specific_config_item = SKFChina_Common_SpecificConfigItem_CURRENT_TIME;
	kp_cfg_pair_set_time.which_config_content = SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
	kp_cfg_pair_set_time.config_content.time_config_content.time.arg = &kp_tstamp;	
	kp_cfg_pair_set_time.config_content.time_config_content.time.funcs.encode = protobuf_encode_config_set_time_callback;

	kp_msg_config._messages.config_dissem.config_pair.arg = &kp_cfg_pair_set_time;
	kp_msg_config._messages.config_dissem.config_pair.funcs.encode = protobuf_encode_configpair_callback;

	#endif
	
	//DBG_LOG_INFO("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		kp_ret.ptbuf = pt_encoded_bytes;
		kp_ret.len = kp_bytes_written;
		kp_ret.is_valid = true;
		
		#if(1) // for debug only			
			DBG_LOG_DEBUG("msg config_dissem to set time is encoded successfully");
			util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
			//the folling is only for debug
			//util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);		
			//util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);
		#endif
	}

	EXIT:
		// to release the memory
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}

		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}
		
		return kp_ret;
}


struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection_retrieve_battery(void)
{
	struct encoded_froto_msg_pkt kp_ret = {.is_valid = false, 0};
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	//the configuration-pair
	uint64_t kp_tstamp = 1000;
	SKFChina_ConfigurationAndCommand_ConfigPair kp_cfg_pair_set_time = {0}; 
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};

	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s","C4BD6A100001");
	kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);
	
	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", "112233445566");
	kp_gw_id_info.size = strlen(kp_gw_id_info.buffer);

	// the measure type
	protobuf_encode_general_bytes_t kp_m_type = {0};
	kp_m_type.size = 1;
	kp_m_type.buffer = l_malloc(kp_m_type.size);
	kp_m_type.buffer[0] = SKFChina_Common_MeasurementType_REMAINING_VOLUME;
	//kp_m_type.buffer[1] = SKFChina_Common_MeasurementType_VOLTAGE_CURRENT;

	// the sensor
	protobuf_encode_general_bytes_t kp_sensor = {0};
	kp_sensor.size = 4;
	kp_sensor.buffer = l_malloc(kp_sensor.size);
	kp_sensor.buffer[0] = SKFChina_Common_SensorType_AD22122_VIBRATION;
	kp_sensor.buffer[1] = SKFChina_Common_SensorType_BQ34210_BATTERY;
	kp_sensor.buffer[2] = SKFChina_Common_SensorType_EFR32BG24_2_4GHZ_RADIO;
	kp_sensor.buffer[3] = SKFChina_Common_SensorType_ADXL382_VIBRATION;

	
	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_data_selection_tag;

	// the header
	kp_msg_config._messages.config_dissem.has_header = true;
	
	kp_msg_config._messages.data_selection.header.version = 1;
	kp_msg_config._messages.data_selection.header.is_up = false;
	kp_msg_config._messages.data_selection.header.message_seq_no = 1;
	kp_msg_config._messages.data_selection.header.time_to_live = 1;
	kp_msg_config._messages.data_selection.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.data_selection.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.data_selection.header.total_block = 1;
	kp_msg_config._messages.data_selection.header.has_ack_window_size_message = false;
	kp_msg_config._messages.data_selection.header.long_packet_id = 1;
	kp_msg_config._messages.data_selection.header.has_current_block = false;
	kp_msg_config._messages.data_selection.header.has_is_big_endian = false;
	
	kp_msg_config._messages.data_selection.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.data_selection.header.peer_addr.mobile_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.data_selection.header.peer_addr.mobile_id.funcs.encode = protobuf_encode_bytes_callback;

	kp_msg_config._messages.data_selection.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

		
	// the palyload
	#if(0)
	kp_msg_config._messages.data_selection.appVer = 1;
	kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
	kp_msg_config._messages.data_selection.has_is_big_endian = false;
	#else
		kp_msg_config._messages.data_selection.appVer = 1;
		//kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
		kp_msg_config._messages.data_selection.has_is_big_endian = false;

		uint8_t kp_id_strbuf[MAX_BT_ADDRSTR] = {0};
		enum sensortype kp_stype = SS_TYPE_UNKNOWN;

		app_pro_gen_get_active_client_id_str( kp_id_strbuf, MAX_BT_ADDRSTR);
		kp_stype = app_pro_gen_get_active_sensor_type( app_pro_gen_get_ctr(), kp_id_strbuf);
		if(SS_TYPE_PREDICTP == kp_stype)
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;	
		}
		else
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;					
		}
	#endif
	
	kp_msg_config._messages.data_selection.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	// measure-type
	#warning "TO double check!"
	kp_msg_config._messages.data_selection.measure_type.arg = &kp_m_type;
	kp_msg_config._messages.data_selection.measure_type.funcs.encode = protobuf_encode_repeated_varint_callback;

	// the sensor
	kp_msg_config._messages.data_selection.sensor.arg = &kp_sensor;
	kp_msg_config._messages.data_selection.sensor.funcs.encode = protobuf_encode_repeated_varint_callback;

	kp_msg_config._messages.data_selection.sample_time_start = 3600;
	kp_msg_config._messages.data_selection.sample_time_end = 7200;
	
	
	
		
	DBG_LOG_DEBUG("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		kp_ret.ptbuf = pt_encoded_bytes;
		kp_ret.len = kp_bytes_written;
		kp_ret.is_valid = true;

		#if(0)// for debug only
			DBG_LOG_INFO("msg config_dissem to set time is encoded successfully");
			util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);
			util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
			// for test only		
			util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);		
		#endif
		/*		
			.............			
		*/
	}
	
	EXIT:
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}
		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}
		if(kp_m_type.buffer)
		{
			l_free(kp_m_type.buffer);
			kp_m_type.buffer = NULL;
		}
		if(kp_sensor.buffer)
		{
			l_free(kp_sensor.buffer);
			kp_sensor.buffer = NULL;
		}
		return kp_ret;
}


/*to fill msg for retrieving data
*/
app_state_t util_froto_fill_msg_data_selection_retrieve_data(void)
{
	app_state_t tpret = ST_OK;
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	//the configuration-pair
	uint64_t kp_tstamp = 1000;
	SKFChina_ConfigurationAndCommand_ConfigPair kp_cfg_pair_set_time = {0}; 
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};

	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s","C4BD6A100001");
	kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);

	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", "112233445566");
	kp_gw_id_info.size = strlen(kp_gw_id_info.buffer);

	// the measure type
	protobuf_encode_general_bytes_t kp_m_type = {0};
	kp_m_type.size = 2;
	kp_m_type.buffer = l_malloc(kp_m_type.size);
	kp_m_type.buffer[0] = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR;
	kp_m_type.buffer[1] = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV;

	// the sensor
	protobuf_encode_general_bytes_t kp_sensor = {0};
	kp_sensor.size = 4;
	kp_sensor.buffer = l_malloc(kp_sensor.size);
	kp_sensor.buffer[0] = SKFChina_Common_SensorType_AD22122_VIBRATION;
	kp_sensor.buffer[1] = SKFChina_Common_SensorType_BQ34210_BATTERY;
	kp_sensor.buffer[2] = SKFChina_Common_SensorType_EFR32BG24_2_4GHZ_RADIO;
	kp_sensor.buffer[3] = SKFChina_Common_SensorType_ADXL382_VIBRATION;

	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_data_selection_tag;

	// the header
	kp_msg_config._messages.config_dissem.has_header = true;
	
	kp_msg_config._messages.data_selection.header.version = 1;
	kp_msg_config._messages.data_selection.header.is_up = false;
	kp_msg_config._messages.data_selection.header.message_seq_no = 1;
	kp_msg_config._messages.data_selection.header.time_to_live = 1;
	kp_msg_config._messages.data_selection.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.data_selection.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.data_selection.header.total_block = 1;
	kp_msg_config._messages.data_selection.header.has_ack_window_size_message = false;
	kp_msg_config._messages.data_selection.header.long_packet_id = 1;
	kp_msg_config._messages.data_selection.header.has_current_block = false;
	kp_msg_config._messages.data_selection.header.has_is_big_endian = false;

	kp_msg_config._messages.data_selection.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.data_selection.header.peer_addr.gateway_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.data_selection.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;
	
	kp_msg_config._messages.data_selection.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

		
	// the palyload
	#if(0)
	kp_msg_config._messages.data_selection.appVer = 1;
	kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
	kp_msg_config._messages.data_selection.has_is_big_endian = false;
	#else
		kp_msg_config._messages.data_selection.appVer = 1;
		//kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
		kp_msg_config._messages.data_selection.has_is_big_endian = false;

		uint8_t kp_id_strbuf[MAX_BT_ADDRSTR] = {0};
		enum sensortype kp_stype = SS_TYPE_UNKNOWN;
		app_pro_gen_get_active_client_id_str( kp_id_strbuf, MAX_BT_ADDRSTR);
		kp_stype = app_pro_gen_get_active_sensor_type( app_pro_gen_get_ctr(), kp_id_strbuf);
		if(SS_TYPE_PREDICTP == kp_stype)
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;
		}
		else
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
		}
	#endif
	
	kp_msg_config._messages.data_selection.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	// measure-type
	kp_msg_config._messages.data_selection.measure_type.arg = &kp_m_type;
	kp_msg_config._messages.data_selection.measure_type.funcs.encode = protobuf_encode_repeated_varint_callback;

	// the sensor
	kp_msg_config._messages.data_selection.sensor.arg = &kp_sensor;
	kp_msg_config._messages.data_selection.sensor.funcs.encode = protobuf_encode_repeated_varint_callback;

	kp_msg_config._messages.data_selection.sample_time_start = 3600;
	kp_msg_config._messages.data_selection.sample_time_end = 7200;
	
	
	
		
	DBG_LOG_DEBUG("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		DBG_LOG_INFO("msg config_dissem to set time is encoded successfully");		
		util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);
		util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);

		// for test only		
		util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);		
		/*
		
			.............
			
		*/

		// to release the memory
		util_froto_release_encoded_msg(pt_encoded_bytes);
	}
	
	EXIT:
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}

		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}

		if(kp_m_type.buffer)
		{
			l_free(kp_m_type.buffer);
			kp_m_type.buffer = NULL;
		}

		if(kp_sensor.buffer)
		{
			l_free(kp_sensor.buffer);
			kp_sensor.buffer = NULL;
		}
		
		return tpret;
}



#if(1)
/*to fill msg for retrieving data
kp_mtype@the measrement type
pt_gwid_str@the gateway ID string
kp_seq_no@ the message seq-no


ret@ the encoded message is returned
NOTE!!! Remember to release the msg returned when you do not need it anymore
*/
struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection(SKFChina_Common_MeasurementType kp_mtype,const uint8_t*pt_gwid_str, uint32_t kp_seq_no)
{
	struct encoded_froto_msg_pkt tpret = {.is_valid = false, 0};
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};

	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s","C4BD6A100001");
	kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);

	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", pt_gwid_str);
	kp_gw_id_info.size = strlen(kp_gw_id_info.buffer);

	// the measure type
	protobuf_encode_general_bytes_t kp_m_type = {0};
	kp_m_type.size = 1;
	kp_m_type.buffer = l_malloc(kp_m_type.size);
	kp_m_type.buffer[0] = kp_mtype;

	// the sensor
	protobuf_encode_general_bytes_t kp_sensor = {0};
	kp_sensor.size = 2;
	kp_sensor.buffer = l_malloc(kp_sensor.size);
	kp_sensor.buffer[0] = SKFChina_Common_SensorType_AD22122_VIBRATION;
	kp_sensor.buffer[1] = SKFChina_Common_SensorType_ADXL382_VIBRATION;

	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_data_selection_tag;

	// the header
	kp_msg_config._messages.config_dissem.has_header = true;
	
	kp_msg_config._messages.data_selection.header.version = 1;
	kp_msg_config._messages.data_selection.header.is_up = false;
	kp_msg_config._messages.data_selection.header.message_seq_no = kp_seq_no;
	kp_msg_config._messages.data_selection.header.time_to_live = 1;
	kp_msg_config._messages.data_selection.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.data_selection.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.data_selection.header.total_block = 1;
	kp_msg_config._messages.data_selection.header.has_ack_window_size_message = false;
	kp_msg_config._messages.data_selection.header.long_packet_id = 1;
	kp_msg_config._messages.data_selection.header.has_current_block = false;
	kp_msg_config._messages.data_selection.header.has_is_big_endian = false;

	kp_msg_config._messages.data_selection.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.data_selection.header.peer_addr.gateway_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.data_selection.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;
	
	kp_msg_config._messages.data_selection.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

		
	// the palyload
	#if(0)
	kp_msg_config._messages.data_selection.appVer = 1;
	kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
	kp_msg_config._messages.data_selection.has_is_big_endian = false;
	#else
		kp_msg_config._messages.data_selection.appVer = 1;
		//kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
		kp_msg_config._messages.data_selection.has_is_big_endian = false;

		uint8_t kp_id_strbuf[MAX_BT_ADDRSTR] = {0};
		enum sensortype kp_stype = SS_TYPE_UNKNOWN;
		app_pro_gen_get_active_client_id_str( kp_id_strbuf, MAX_BT_ADDRSTR);
		kp_stype = app_pro_gen_get_active_sensor_type( app_pro_gen_get_ctr(), kp_id_strbuf);
		if(SS_TYPE_PREDICTP == kp_stype)	
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;
		}
		else
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
		}
	#endif
	
	kp_msg_config._messages.data_selection.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	// measure-type
	kp_msg_config._messages.data_selection.measure_type.arg = &kp_m_type;
	kp_msg_config._messages.data_selection.measure_type.funcs.encode = protobuf_encode_repeated_varint_callback;

	// the sensor
	kp_msg_config._messages.data_selection.sensor.arg = &kp_sensor;
	kp_msg_config._messages.data_selection.sensor.funcs.encode = protobuf_encode_repeated_varint_callback;

	kp_msg_config._messages.data_selection.sample_time_start = 3600;
	kp_msg_config._messages.data_selection.sample_time_end = 7200;
	
	DBG_LOG_DEBUG("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		tpret.len = kp_bytes_written;
		tpret.ptbuf = pt_encoded_bytes;
		tpret.is_valid = true;
		
		#if(1)// for debug only
			DBG_LOG_INFO("msg to request for data : %d", kp_mtype);		
			//util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);
			util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
			// for test only		
			//util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);		
			// to release the memory
			//util_froto_release_encoded_msg(pt_encoded_bytes);
		#endif
	}
	
	EXIT:
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}

		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}

		if(kp_m_type.buffer)
		{
			l_free(kp_m_type.buffer);
			kp_m_type.buffer = NULL;
		}

		if(kp_sensor.buffer)
		{
			l_free(kp_sensor.buffer);
			kp_sensor.buffer = NULL;
		}
		
		return tpret;
}

#endif


#if(1)
/*to fill msg for retrieving data
kp_mtype@the measrement type
pt_gwid_str@the gateway ID string
kp_seq_no@ the message seq-no


ret@ the encoded message is returned
NOTE!!! Remember to release the msg returned when you do not need it anymore
*/
struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection_v2(SKFChina_Common_MeasurementType kp_mtype,const uint8_t*pt_gwid_str, uint32_t kp_seq_no)
{
	struct encoded_froto_msg_pkt tpret = {.is_valid = false, 0};
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};

	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s","C4BD6A100001");
	kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);

	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", pt_gwid_str);
	kp_gw_id_info.size = strlen(kp_gw_id_info.buffer);

	#if(1)// for debug
		// the measure type
		protobuf_encode_general_bytes_t kp_m_type = {0};
		kp_m_type.size = 3;
		kp_m_type.buffer = l_malloc(kp_m_type.size);
		kp_m_type.buffer[0] = SKFChina_Common_MeasurementType_REMAINING_VOLUME;
		kp_m_type.buffer[1] = SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT;		
		kp_m_type.buffer[2] = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI;
	#endif

	#if(1)
		// the sensor
		protobuf_encode_general_bytes_t kp_sensor = {0};
		kp_sensor.size = 2;
		kp_sensor.buffer = l_malloc(kp_sensor.size);
		kp_sensor.buffer[0] = SKFChina_Common_SensorType_NRF52_2_4GHZ_RADIO;
		kp_sensor.buffer[1] = SKFChina_Common_SensorType_ADXL382_VIBRATION;
	#endif
	

	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_data_selection_tag;

	// the header
	kp_msg_config._messages.config_dissem.has_header = true;
	
	kp_msg_config._messages.data_selection.header.version = 1;
	kp_msg_config._messages.data_selection.header.is_up = false;
	kp_msg_config._messages.data_selection.header.message_seq_no = kp_seq_no;
	kp_msg_config._messages.data_selection.header.time_to_live = 1;
	kp_msg_config._messages.data_selection.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.data_selection.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.data_selection.header.total_block = 1;
	kp_msg_config._messages.data_selection.header.has_ack_window_size_message = false;
	kp_msg_config._messages.data_selection.header.long_packet_id = 1;
	kp_msg_config._messages.data_selection.header.has_current_block = false;
	kp_msg_config._messages.data_selection.header.has_is_big_endian = false;

	kp_msg_config._messages.data_selection.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.data_selection.header.peer_addr.gateway_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.data_selection.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;
	
	kp_msg_config._messages.data_selection.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

		
	// the palyload
	#if(0)
	kp_msg_config._messages.data_selection.appVer = 1;
	kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
	kp_msg_config._messages.data_selection.has_is_big_endian = false;
	#else
		kp_msg_config._messages.data_selection.appVer = 1;
		//kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
		kp_msg_config._messages.data_selection.has_is_big_endian = false;

		uint8_t kp_id_strbuf[MAX_BT_ADDRSTR] = {0};
		enum sensortype kp_stype = SS_TYPE_UNKNOWN;
		app_pro_gen_get_active_client_id_str( kp_id_strbuf, MAX_BT_ADDRSTR);
		kp_stype = app_pro_gen_get_active_sensor_type( app_pro_gen_get_ctr(), kp_id_strbuf);
		if(SS_TYPE_PREDICTP == kp_stype)
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;
		}
		else
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
		}
	#endif
	
	kp_msg_config._messages.data_selection.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	// measure-type
	kp_msg_config._messages.data_selection.measure_type.arg = &kp_m_type;
	kp_msg_config._messages.data_selection.measure_type.funcs.encode = protobuf_encode_repeated_mtype_callback;

	// the sensor
	kp_msg_config._messages.data_selection.sensor.arg = &kp_sensor;
	kp_msg_config._messages.data_selection.sensor.funcs.encode = protobuf_encode_repeated_sensor_callback;

	kp_msg_config._messages.data_selection.sample_time_start = 3600;
	kp_msg_config._messages.data_selection.sample_time_end = 7200;
	
	DBG_LOG_DEBUG("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		tpret.len = kp_bytes_written;
		tpret.ptbuf = pt_encoded_bytes;
		tpret.is_valid = true;
		
		#if(1)// for debug only
			DBG_LOG_INFO("msg to request for data : %d", kp_mtype);		
			//util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);
			util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
			// for test only		
			//util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);		
			// to release the memory
			//util_froto_release_encoded_msg(pt_encoded_bytes);
		#endif
	}
	
	EXIT:
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}

		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}

		if(kp_m_type.buffer)
		{
			l_free(kp_m_type.buffer);
			kp_m_type.buffer = NULL;
		}

		if(kp_sensor.buffer)
		{
			l_free(kp_sensor.buffer);
			kp_sensor.buffer = NULL;
		}
		
		return tpret;
}

#endif

#if(1)
/*to fill msg for retrieving data
kp_mtype@the measrement type
pt_gwid_str@the gateway ID string
kp_seq_no@ the message seq-no


ret@ the encoded message is returned
NOTE!!! Remember to release the msg returned when you do not need it anymore
*/
struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection_v3(SKFChina_Common_MeasurementType *pt_mtype_array, const uint32_t kp_arr_len,const uint8_t*pt_gwid_str, uint32_t kp_seq_no)
{
	struct encoded_froto_msg_pkt tpret = {.is_valid = false, 0};
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};

	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s","C4BD6A100001");
	kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);

	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", pt_gwid_str);
	kp_gw_id_info.size = strlen(kp_gw_id_info.buffer);

	#if(1)// for debug
		// the measure type
		protobuf_encode_general_bytes_t kp_m_type = {0};
		kp_m_type.size = kp_arr_len;
		kp_m_type.buffer = l_malloc(kp_m_type.size);
		for(uint32_t i = 0; i < kp_m_type.size; i++)
		{
			kp_m_type.buffer[i] = pt_mtype_array[i];
		}
	#endif

	#if(1)
		// the sensor
		protobuf_encode_general_bytes_t kp_sensor = {0};
		kp_sensor.size = 4;
		kp_sensor.buffer = l_malloc(kp_sensor.size);
		kp_sensor.buffer[0] = SKFChina_Common_SensorType_NRF52_2_4GHZ_RADIO;
		kp_sensor.buffer[1] = SKFChina_Common_SensorType_NTC_TEMPERATURE;
		kp_sensor.buffer[2] = SKFChina_Common_SensorType_IIM4235X_VIBRATION;
		kp_sensor.buffer[3] = SKFChina_Common_SensorType_ADXL382_VIBRATION;
	#endif
	

	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_data_selection_tag;

	// the header
	kp_msg_config._messages.config_dissem.has_header = true;
	
	kp_msg_config._messages.data_selection.header.version = 1;
	kp_msg_config._messages.data_selection.header.is_up = false;
	kp_msg_config._messages.data_selection.header.message_seq_no = kp_seq_no;
	kp_msg_config._messages.data_selection.header.time_to_live = 1;
	kp_msg_config._messages.data_selection.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.data_selection.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.data_selection.header.total_block = 1;
	kp_msg_config._messages.data_selection.header.has_ack_window_size_message = false;
	kp_msg_config._messages.data_selection.header.long_packet_id = 1;
	kp_msg_config._messages.data_selection.header.has_current_block = false;
	kp_msg_config._messages.data_selection.header.has_is_big_endian = false;

	kp_msg_config._messages.data_selection.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.data_selection.header.peer_addr.gateway_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.data_selection.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;
	
	kp_msg_config._messages.data_selection.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

		
	// the palyload
	#if(0) // the original
	kp_msg_config._messages.data_selection.appVer = 1;
	kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
	kp_msg_config._messages.data_selection.has_is_big_endian = false;
	#else
		uint8_t kp_id_strbuf[MAX_BT_ADDRSTR] = {0};
		enum sensortype kp_stype = SS_TYPE_UNKNOWN;

		kp_msg_config._messages.data_selection.appVer = 1;
		//kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
		kp_msg_config._messages.data_selection.has_is_big_endian = false;	

		app_pro_gen_get_active_client_id_str( kp_id_strbuf, MAX_BT_ADDRSTR);
		kp_stype = app_pro_gen_get_active_sensor_type( app_pro_gen_get_ctr(), kp_id_strbuf);
		if(SS_TYPE_PREDICTP == kp_stype)
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;
		}
		else
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;					
		}
	#endif
	
	kp_msg_config._messages.data_selection.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	// measure-type
	kp_msg_config._messages.data_selection.measure_type.arg = &kp_m_type;
	kp_msg_config._messages.data_selection.measure_type.funcs.encode = protobuf_encode_repeated_mtype_callback;

	// the sensor
	kp_msg_config._messages.data_selection.sensor.arg = &kp_sensor;
	kp_msg_config._messages.data_selection.sensor.funcs.encode = protobuf_encode_repeated_sensor_callback;

	kp_msg_config._messages.data_selection.sample_time_start = 3600;
	kp_msg_config._messages.data_selection.sample_time_end = 7200;
	
	DBG_LOG_DEBUG("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		tpret.len = kp_bytes_written;
		tpret.ptbuf = pt_encoded_bytes;
		tpret.is_valid = true;
		
		#if(1)// for debug only
			// DBG_LOG_INFO("msg to request for data : %d", kp_mtype);		
			//util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);
			util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
			// for test only		
			//util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);		
			// to release the memory
			//util_froto_release_encoded_msg(pt_encoded_bytes);
		#endif
	}
	
	EXIT:
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}

		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}

		if(kp_m_type.buffer)
		{
			l_free(kp_m_type.buffer);
			kp_m_type.buffer = NULL;
		}

		if(kp_sensor.buffer)
		{
			l_free(kp_sensor.buffer);
			kp_sensor.buffer = NULL;
		}
		
		return tpret;
}

#endif

#if(1)
/*to fill msg for retrieving data
kp_mtype@the measrement type
pt_gwid_str@the gateway ID string
kp_seq_no@ the message seq-no


ret@ the encoded message is returned
NOTE!!! Remember to release the msg returned when you do not need it anymore
*/
//struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection_with_mtype_msg(SKFChina_Common_MeasurementType *pt_mtype_array, const uint32_t kp_arr_len,const uint8_t*pt_gwid_str, uint32_t kp_seq_no)
struct encoded_froto_msg_pkt util_froto_fill_msg_data_selection_with_mtype_msg(SKFChina_SensingDataUpload_MeasurementTypeMsg kp_mtype_msg,const uint8_t*pt_gwid_str, uint32_t kp_seq_no)
{
	struct encoded_froto_msg_pkt tpret = {.is_valid = false, 0};
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};


	//SKFChina_Common_MeasurementType pt_mtype_array[1] = {0};
	//const uint32_t kp_arr_len = 1;

	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s","C4BD6A100001");
	kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);

	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", pt_gwid_str);
	kp_gw_id_info.size = strlen(kp_gw_id_info.buffer);

	#if(0)// for debug
		pt_mtype_array[0] = kp_mtype_msg.measure_type;
		
		// the measure type
		protobuf_encode_general_bytes_t kp_m_type = {0};
		kp_m_type.size = kp_arr_len;
		kp_m_type.buffer = l_malloc(kp_m_type.size);
		for(uint32_t i = 0; i < kp_m_type.size; i++)
		{
			kp_m_type.buffer[i] = pt_mtype_array[i];
		}
	#endif

	#if(1)
		// the sensor
		protobuf_encode_general_bytes_t kp_sensor = {0};
		kp_sensor.size = 4;
		kp_sensor.buffer = l_malloc(kp_sensor.size);
		kp_sensor.buffer[0] = SKFChina_Common_SensorType_NRF52_2_4GHZ_RADIO;
		kp_sensor.buffer[1] = SKFChina_Common_SensorType_NTC_TEMPERATURE;
		kp_sensor.buffer[2] = SKFChina_Common_SensorType_IIM4235X_VIBRATION;
		kp_sensor.buffer[3] = SKFChina_Common_SensorType_ADXL382_VIBRATION;
	#endif
	

	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_data_selection_tag;

	// the header
	kp_msg_config._messages.config_dissem.has_header = true;
	
	kp_msg_config._messages.data_selection.header.version = 1;
	kp_msg_config._messages.data_selection.header.is_up = false;
	kp_msg_config._messages.data_selection.header.message_seq_no = kp_seq_no;
	kp_msg_config._messages.data_selection.header.time_to_live = 1;
	kp_msg_config._messages.data_selection.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.data_selection.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.data_selection.header.total_block = 1;
	kp_msg_config._messages.data_selection.header.has_ack_window_size_message = false;
	kp_msg_config._messages.data_selection.header.long_packet_id = 1;
	kp_msg_config._messages.data_selection.header.has_current_block = false;
	kp_msg_config._messages.data_selection.header.has_is_big_endian = false;

	kp_msg_config._messages.data_selection.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.data_selection.header.peer_addr.gateway_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.data_selection.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;
	
	kp_msg_config._messages.data_selection.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

		
	// the palyload
	#if(0)
	kp_msg_config._messages.data_selection.appVer = 1;
	kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;
	kp_msg_config._messages.data_selection.has_is_big_endian = false;
	#else
		kp_msg_config._messages.data_selection.appVer = 1;
		//kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;
		kp_msg_config._messages.data_selection.has_is_big_endian = false;

		uint8_t kp_id_strbuf[MAX_BT_ADDRSTR] = {0};
		enum sensortype kp_stype = SS_TYPE_UNKNOWN;

		app_pro_gen_get_active_client_id_str( kp_id_strbuf, MAX_BT_ADDRSTR);
		kp_stype = app_pro_gen_get_active_sensor_type( app_pro_gen_get_ctr(), kp_id_strbuf);
		if(SS_TYPE_PREDICTP == kp_stype)
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;
		}
		else
		{
			kp_msg_config._messages.data_selection.product = SKFChina_Common_ProductType_BULLET_NODE;
		}
	#endif
	
	kp_msg_config._messages.data_selection.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.data_selection.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	// measure-type
	kp_msg_config._messages.data_selection.measure_type.arg = &kp_mtype_msg;
	kp_msg_config._messages.data_selection.measure_type.funcs.encode = protobuf_encode_repeated_mtype_by_msg_callback;

	// the sensor
	kp_msg_config._messages.data_selection.sensor.arg = &kp_sensor;
	kp_msg_config._messages.data_selection.sensor.funcs.encode = protobuf_encode_repeated_sensor_callback;

	kp_msg_config._messages.data_selection.sample_time_start = 3600;
	kp_msg_config._messages.data_selection.sample_time_end = 7200;
	
	DBG_LOG_DEBUG("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		tpret.len = kp_bytes_written;
		tpret.ptbuf = pt_encoded_bytes;
		tpret.is_valid = true;
		
		#if(1)// for debug only
			// DBG_LOG_INFO("msg to request for data : %d", kp_mtype);		
			//util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);
			util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
			// for test only		
			//util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);		
			// to release the memory
			//util_froto_release_encoded_msg(pt_encoded_bytes);
		#endif
	}
	
	EXIT:
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}

		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}

		//if(kp_m_type.buffer)
		//{
		//	l_free(kp_m_type.buffer);
		//	kp_m_type.buffer = NULL;
		//}

		if(kp_sensor.buffer)
		{
			l_free(kp_sensor.buffer);
			kp_sensor.buffer = NULL;
		}
		
		return tpret;
}

#endif


/*to fill msg for command_dissem
*/
app_state_t util_froto_fill_msg_command_dissem(void)
{
	app_state_t tpret = ST_OK;
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	//the configuration-pair
	uint64_t kp_tstamp = 1000;
	SKFChina_ConfigurationAndCommand_ConfigPair kp_cfg_pair_set_time = {0}; 
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};

	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s","C4BD6A100001");
	kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);

	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", "112233445566");
	kp_gw_id_info.size = strlen(kp_gw_id_info.buffer);


	// the measure type
	protobuf_encode_general_bytes_t kp_m_type = {0};
	kp_m_type.size = 2;
	kp_m_type.buffer = l_malloc(kp_m_type.size);
	kp_m_type.buffer[0] = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR;
	kp_m_type.buffer[1] = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV;

	// the sensor
	protobuf_encode_general_bytes_t kp_sensor = {0};
	kp_sensor.size = 3;
	kp_sensor.buffer = l_malloc(kp_sensor.size);
	kp_sensor.buffer[0] = SKFChina_Common_SensorType_AD22122_VIBRATION;
	kp_sensor.buffer[1] = SKFChina_Common_SensorType_BQ34210_BATTERY;
	kp_sensor.buffer[2] = SKFChina_Common_SensorType_EFR32BG24_2_4GHZ_RADIO;


	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_command_dissem_tag;

	// the header
	kp_msg_config._messages.config_dissem.has_header = true;
	
	kp_msg_config._messages.command_dissem.header.version = 1;
	kp_msg_config._messages.command_dissem.header.is_up = false;
	kp_msg_config._messages.command_dissem.header.message_seq_no = 1;
	kp_msg_config._messages.command_dissem.header.time_to_live = 1;
	kp_msg_config._messages.command_dissem.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.command_dissem.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.command_dissem.header.total_block = 1;
	kp_msg_config._messages.command_dissem.header.has_ack_window_size_message = false;
	kp_msg_config._messages.command_dissem.header.long_packet_id = 1;
	kp_msg_config._messages.command_dissem.header.has_current_block = false;
	kp_msg_config._messages.command_dissem.header.has_is_big_endian = false;
	
	kp_msg_config._messages.command_dissem.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.command_dissem.header.peer_addr.mobile_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.command_dissem.header.peer_addr.mobile_id.funcs.encode = protobuf_encode_bytes_callback;

	kp_msg_config._messages.command_dissem.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.command_dissem.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

		
	// the palyload
	
	kp_msg_config._messages.command_dissem.appVer = 1;
	#if(0)
	kp_msg_config._messages.command_dissem.product = SKFChina_Common_ProductType_BULLET_NODE;
	#else
		enum sensortype kp_stype = app_pro_gen_get_active_sensor_type_v2();
		if(SS_TYPE_PREDICTP == kp_stype)
		{
			kp_msg_config._messages.command_dissem.product = SKFChina_Common_ProductType_BULLET_NODE_PRO;				
		}
		else
		{		
			kp_msg_config._messages.command_dissem.product = SKFChina_Common_ProductType_BULLET_NODE;
		}
	#endif
	kp_msg_config._messages.command_dissem.has_is_big_endian = false;
	kp_msg_config._messages.command_dissem.is_big_endian = false;
	
	kp_msg_config._messages.command_dissem.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.command_dissem.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	// measure-type
	kp_msg_config._messages.data_selection.measure_type.arg = &kp_m_type;
	kp_msg_config._messages.data_selection.measure_type.funcs.encode = protobuf_encode_repeated_varint_callback;

	// the sensor
	kp_msg_config._messages.data_selection.sensor.arg = &kp_sensor;
	kp_msg_config._messages.data_selection.sensor.funcs.encode = protobuf_encode_repeated_varint_callback;

	kp_msg_config._messages.command_dissem.has_command_pair = true;
	kp_msg_config._messages.command_dissem.command_pair.command = SKFChina_Common_Command_IMMEDIATE_QUICK_LED_BLINK;	
	kp_msg_config._messages.command_dissem.command_pair.which_command_para = SKFChina_ConfigurationAndCommand_CommandPair_general_config_content_int32_tag;
	kp_msg_config._messages.command_dissem.command_pair.command_para.general_config_content_int32 = 1234;
	
	DBG_LOG_DEBUG("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		DBG_LOG_INFO("msg config_dissem to set time is encoded successfully");
		util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);

		// for test only		
		util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);		
		/*
			.............
			
		*/

		// to release the memory
		util_froto_release_encoded_msg(pt_encoded_bytes);
	}
	
	EXIT:
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}

		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}

		if(kp_m_type.buffer)
		{
			l_free(kp_m_type.buffer);
			kp_m_type.buffer = NULL;
		}

		if(kp_sensor.buffer)
		{
			l_free(kp_sensor.buffer);
			kp_sensor.buffer = NULL;
		}
		
		return tpret;
}





/*to fill msg for version_retrieve
*/
struct encoded_froto_msg_pkt util_froto_fill_msg_version_retrieve(const uint8_t *pt_gwid_str, const uint32_t kp_seq_no)
{
	struct encoded_froto_msg_pkt kpret = {.is_valid = false, 0};
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	//the configuration-pair
	uint64_t kp_tstamp = 1000;
	SKFChina_ConfigurationAndCommand_ConfigPair kp_cfg_pair_set_time = {0}; 
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};

	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s","C4BD6A100001");
	kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);

	
	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", pt_gwid_str);
	kp_gw_id_info.size = strlen(kp_gw_id_info.buffer);


	
	// the measure type
	protobuf_encode_general_bytes_t kp_m_type = {0};
	kp_m_type.size = 2;
	kp_m_type.buffer = l_malloc(kp_m_type.size);
	kp_m_type.buffer[0] = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR;
	kp_m_type.buffer[1] = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV;

	// the sensor
	protobuf_encode_general_bytes_t kp_sensor = {0};
	kp_sensor.size = 3;
	kp_sensor.buffer = l_malloc(kp_sensor.size);
	kp_sensor.buffer[0] = SKFChina_Common_SensorType_AD22122_VIBRATION;
	kp_sensor.buffer[1] = SKFChina_Common_SensorType_BQ34210_BATTERY;
	kp_sensor.buffer[2] = SKFChina_Common_SensorType_EFR32BG24_2_4GHZ_RADIO;


	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_version_retrieve_tag;

	// the header
	kp_msg_config._messages.version_retrieve.has_header = true;
	
	kp_msg_config._messages.version_retrieve.header.version = 1;
	kp_msg_config._messages.version_retrieve.header.is_up = false;
	kp_msg_config._messages.version_retrieve.header.message_seq_no = kp_seq_no;
	kp_msg_config._messages.version_retrieve.header.time_to_live = 1;
	kp_msg_config._messages.version_retrieve.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.version_retrieve.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.version_retrieve.header.total_block = 1;
	kp_msg_config._messages.version_retrieve.header.has_ack_window_size_message = false;
	kp_msg_config._messages.version_retrieve.header.long_packet_id = 1;
	kp_msg_config._messages.version_retrieve.header.has_current_block = false;
	kp_msg_config._messages.version_retrieve.header.has_is_big_endian = false;

	kp_msg_config._messages.version_retrieve.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.version_retrieve.header.peer_addr.mobile_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.version_retrieve.header.peer_addr.mobile_id.funcs.encode = protobuf_encode_bytes_callback;

	kp_msg_config._messages.version_retrieve.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.version_retrieve.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

		
	// the palyload
	
	kp_msg_config._messages.version_retrieve.appVer = 1;
	kp_msg_config._messages.version_retrieve.has_is_big_endian = false;
	kp_msg_config._messages.version_retrieve.is_big_endian = false;
	
	kp_msg_config._messages.version_retrieve.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.version_retrieve.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

	#if(0)
	// measure-type
	kp_msg_config._messages.data_selection.measure_type.arg = &kp_m_type;
	kp_msg_config._messages.data_selection.measure_type.funcs.encode = protobuf_encode_repeated_varint_callback;
	// the sensor
	kp_msg_config._messages.data_selection.sensor.arg = &kp_sensor;
	kp_msg_config._messages.data_selection.sensor.funcs.encode = protobuf_encode_repeated_varint_callback;
	#endif

	kp_msg_config._messages.version_retrieve.payload = SKFChina_FirmwareUpdateOverTheAir_RetrievePayload_CURRENT_VERSION;
	
	DBG_LOG_DEBUG("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		kpret.ptbuf = pt_encoded_bytes;
		kpret.len = kp_bytes_written;
		kpret.is_valid = true;
		
		#if(1) // for debug only
		util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);		
		//util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);
		// for test only	
		//util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);		
		#endif
		/* .............	*/

	}
	
	EXIT:
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}

		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}

		if(kp_m_type.buffer)
		{
			l_free( kp_m_type.buffer);
			kp_m_type.buffer = NULL;
		}

		if(kp_sensor.buffer)
		{
			l_free(kp_sensor.buffer);
			kp_sensor.buffer = NULL;
		}
		
		return kpret;
}



/*to encode the specific config item
*/
static bool protobuf_encode_specific_config_item_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{
	bool tpret = true;
	const struct config_to_retrieve *pt_conf = (struct config_to_retrieve*)*arg;
	//uint32_t kp_u32 = 0;
	SKFChina_ConfigurationAndCommand_SpecificConfigItemMsg kp_specific_msg = {0};
	
	if(NULL == arg)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}
	
	if(SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG != pt_conf->payload_info)
	{
		DBG_LOG_ERR("invalid payload type");
		tpret = false;
		return tpret;
	}

	if(pt_conf->unit_num > MAX_RETR_CONF_ARRAY)
	{
		DBG_LOG_WARN(" invalid configuration unit");
		tpret = false;
		return tpret;
	}

	for(uint32_t i = 0; i < pt_conf->unit_num; i++)
	{
		kp_specific_msg.specific_config_item = pt_conf->conf.specific_conf_array[i % MAX_RETR_CONF_ARRAY];
		DBG_LOG_DEBUG("the conf idx to encode is %d", kp_specific_msg.specific_config_item);
		
		tpret = encode_submessage_v2( stream_p, SKFChina_ConfigurationAndCommand_ConfigRetrieve_fields, SKFChina_ConfigurationAndCommand_SpecificConfigItemMsg_fields, (void *)&kp_specific_msg.specific_config_item);
		if(false == tpret)
		{
			break;
		}
	}
	
	return tpret;
}


/*
bool (*encode)(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
*/
static bool protobuf_encode_fscheduler_config_item_callback(pb_ostream_t *stream_p, const pb_field_t *field, void *const *arg)
{	
    bool tpret = true;
	const struct config_to_retrieve *pt_conf = (struct config_to_retrieve*)*arg;
	uint32_t kp_u32 = 0;

	if(NULL == arg)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}
	
    if(SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG != pt_conf->payload_info)
    {
    	DBG_LOG_ERR("unexpected payload info");
    	tpret = false;
		return tpret;
    }

	if(pt_conf->unit_num > MAX_RETR_CONF_ARRAY)
	{
		DBG_LOG_ERR("unexpected unit num");
		tpret = false;
		return tpret;
	}
	for(uint32_t i = 0; i < pt_conf->unit_num; i++)
	{
		kp_u32 = pt_conf->conf.fsched_conf_array[i % MAX_RETR_CONF_ARRAY];
		tpret = encode_submessage_v2( stream_p, SKFChina_ConfigurationAndCommand_ConfigRetrieve_fields, SKFChina_ConfigurationAndCommand_fSchedulerConfigItemMsg_fields, (void *)&kp_u32);
		if(false == tpret)
		{
			break;
		}
	}
	return tpret;
}

/*tp_dtbuf@the buffer where the ID array is returned
kp_bufsz@ the buffer size
pt_str@the ID string , in the format of "C4BD6A100001" HEX
*/
uint8_t idstr_to_bytes_array( uint8_t *pt_dtbuf, uint8_t kp_bufsz, uint8_t *pt_str)
{
	uint8_t i = 0, tpidx = 0, tpval = 0;
	if((NULL == pt_dtbuf)||(0 == kp_bufsz)||(NULL == pt_str))
	{
		return 0;
	}
	tpval = 0;
	while(pt_str[i])
	{
		//DBG_LOG_INFO("%d", pt_str[i]);
		if((pt_str[i] >= 'A')&&( pt_str[i] <= 'F'))
		{
			tpval = pt_str[i] - 'A' + 10;
		}
		else if((pt_str[i] >= '0')&&( pt_str[i] <= '9'))
		{
			tpval = pt_str[i] - '0';
		}
		else if((pt_str[i] >= 'a')&&( pt_str[i] <= 'f'))
		{
			tpval = pt_str[i] - 'a' + 10;
		}
		else
		{
			break;
		}		
		//DBG_LOG_INFO("%d", tpval);
		if(i % 2 == 0)
		{
			pt_dtbuf[tpidx % kp_bufsz] = tpval; 
		}
		else
		{
			pt_dtbuf[tpidx % kp_bufsz] = pt_dtbuf[tpidx % kp_bufsz]*0x10 + tpval;
			tpidx ++;
		}
		i++;
	}

	tpval = strlen(pt_str);

	util_dbg_buf_dump( pt_dtbuf, kp_bufsz);
	
	return (tpval / 2 + tpval % 2);
}


/*write IP string to bytes array
kp_bufsz@ the buffer size
pt_str@the ID string , in the format of "xxx.xxx.xxx.xxx" like 192.168.xx.xx  otc
ret@ the number of bytes get is returned
NOTE!! GIVE the valid IP string, otherwise things might go wrong
*/
uint32_t ipstr_to_bytes_array( uint8_t *pt_dtbuf, uint8_t kp_bufsz, uint8_t *pt_str)
{
	uint32_t tpret = 0, i = 0;
	uint8_t kp_buf[0x10] = {0}, idx = 0; 
	if((NULL == pt_dtbuf)||(0 == kp_bufsz)||(NULL == pt_str))
	{
		DBG_LOG_ERR("invalid parameter");
		return 0;
	}
	i = 0;
	idx = 0;
	tpret = 0;
	while(pt_str[i])
	{
		if((pt_str[i] >= '0')&&(pt_str[i] <= '9'))
		{
			kp_buf[idx % sizeof(kp_buf)] = pt_str[i];
			idx ++;
		}
		else if(idx > 0)
		{
			DBG_LOG_DEBUG("string to int %s", kp_buf);
			pt_dtbuf[tpret % kp_bufsz] = atoi(kp_buf);
			tpret++;

			memset( kp_buf, 0, sizeof(kp_buf));
			idx = 0;
		}
		
		if((pt_str[i+1] == 0)&&(idx > 0))
		{ // the end of string			
			DBG_LOG_DEBUG("string to int %s", kp_buf);
			pt_dtbuf[tpret % kp_bufsz] = atoi(kp_buf);
			tpret++;

			memset( kp_buf, 0, sizeof(kp_buf));
			idx = 0;
		}
		
		i++;
	}

	util_dbg_buf_dump( pt_dtbuf, kp_bufsz);
	
	return tpret;
}

/*to fill message config-retrieve
*/
struct encoded_froto_msg_pkt util_froto_fill_msg_config_retrieve(const struct config_to_retrieve *pt_conf, uint8_t*pt_sensorid, uint8_t*pt_gwid)
{
	struct encoded_froto_msg_pkt kpret = {.is_valid = false, 0};

	size_t kp_bytes_written = 0;	
	pb_byte_t *pt_encoded_bytes = NULL;
	
	SKFChina_App_AppMessage kp_msg = {0};
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};

	if((NULL == pt_conf)||(NULL == pt_sensorid)||(NULL == pt_sensorid))
	{
		DBG_LOG_ERR("invalid parameter");
		return kpret;
	}

	#if(0)
	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s",pt_sensorid);
	kp_sensor_id_info.size = strlen(kp_sensor_id_info.buffer);
	#else
		kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
		memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
		kp_sensor_id_info.size = idstr_to_bytes_array( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH, pt_sensorid);
		util_dbg_buf_dump( kp_sensor_id_info.buffer, kp_sensor_id_info.size);
	#endif
	
	
	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	kp_gw_id_info.size = idstr_to_bytes_array( kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, pt_gwid);


	// to fill the msg structure before encoding it
	kp_msg.appVer = 1;
	kp_msg.which__messages = SKFChina_App_AppMessage_config_retrieve_tag;

	// the header
	kp_msg._messages.config_retrieve.has_header = true;
	
	kp_msg._messages.config_retrieve.header.version = 1;
	kp_msg._messages.config_retrieve.header.is_up = false;
	kp_msg._messages.config_retrieve.header.message_seq_no = pt_conf->msg_seq_no;
	kp_msg._messages.config_retrieve.header.time_to_live = 1;
	kp_msg._messages.config_retrieve.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg._messages.config_retrieve.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg._messages.config_retrieve.header.total_block = 1;
	kp_msg._messages.config_retrieve.header.has_ack_window_size_message = false;
	kp_msg._messages.config_retrieve.header.long_packet_id = 1;
	kp_msg._messages.config_retrieve.header.has_current_block = false;
	kp_msg._messages.config_retrieve.header.has_is_big_endian = false;

	kp_msg._messages.config_retrieve.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg._messages.config_retrieve.header.peer_addr.mobile_id.arg = &kp_gw_id_info;
	kp_msg._messages.config_retrieve.header.peer_addr.mobile_id.funcs.encode = protobuf_encode_bytes_callback;

	kp_msg._messages.config_retrieve.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg._messages.config_retrieve.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

	//the playload
	kp_msg._messages.config_retrieve.appVer = 1;
	kp_msg._messages.config_retrieve.sensor_id.arg = &kp_sensor_id_info;
	kp_msg._messages.config_retrieve.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	
	//kp_msg._messages.config_retrieve.payload = SKFChina_ConfigurationAndCommand_RetrievePayload_GATEWAY_CONFIG_FILE_CONFIG;	
	kp_msg._messages.config_retrieve.payload = pt_conf->payload_info;
	if(SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG == pt_conf->payload_info)
	{
		kp_msg._messages.config_retrieve.specific_config_item.arg = pt_conf;
		kp_msg._messages.config_retrieve.specific_config_item.funcs.encode = protobuf_encode_specific_config_item_callback;
	}
	else if(SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG == pt_conf->payload_info)
	{
		kp_msg._messages.config_retrieve.fscheduler_config_item.arg = pt_conf;
		kp_msg._messages.config_retrieve.fscheduler_config_item.funcs.encode = protobuf_encode_fscheduler_config_item_callback;
	}
	else
	{
		DBG_LOG_WARN(" Double check to make sure you have nothing to do here!");
	}
	
	kp_msg._messages.config_retrieve.has_is_big_endian = false;
	kp_msg._messages.config_retrieve.has_memory_id = false;

	// the httpURL
	//kp_msg._messages.config_retrieve.httpPostUrl.arg = ;
	//kp_msg._messages.config_retrieve.httpPostUrl.funcs.encode = 

	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg, &kp_bytes_written);
	if(pt_encoded_bytes)
	{// the message is encoded successfully
		kpret.ptbuf = pt_encoded_bytes;
		kpret.len = kp_bytes_written;
		kpret.is_valid = true;

		#if(1)// for dbg only
			DBG_LOG_INFO("msg to retrieve sensor-conf:");
			util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
		#endif
	}

	EXIT:
		if(kp_sensor_id_info.buffer)
		{
			l_free( kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}
		if(kp_gw_id_info.buffer)
		{
			l_free( kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}
		
		return kpret;
}


/*
*/
struct encoded_froto_msg_pkt util_froto_fill_msg_fuota_notify_dissem(const uint8_t*pt_gwid_str, const uint32_t kp_seq_no, struct longdata_xfer_ctr*pt_img_dissem_ctr)
{
	struct encoded_froto_msg_pkt kpret = {.is_valid = false, 0};
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};

	uint32_t kp_ftsk_id = 0;
	
	//the configuration-pair
	uint64_t kp_tstamp = 1000;
	SKFChina_ConfigurationAndCommand_ConfigPair kp_cfg_pair_set_time = {0}; 
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};

	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	//snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s", "C4BD6A100001");
	kp_sensor_id_info.size = idstr_to_bytes_array( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH, "C4BD6A100001");

	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);

	//snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", pt_gwid_str);
	kp_gw_id_info.size = idstr_to_bytes_array( kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, pt_gwid_str);
	
	pthread_mutex_lock( &pt_img_dissem_ctr->mtx);
	if(false == pt_img_dissem_ctr->is_in_process)
	{	
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
		DBG_LOG_ERR("the controller is inactive");
		goto EXIT;
	}
	kp_ftsk_id = pt_img_dissem_ctr->f_tsk_id;
	pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);

	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_fuota_notify_dissem_tag;

	// the header
	kp_msg_config._messages.fuota_notify_dissem.has_header = true;
	
	kp_msg_config._messages.fuota_notify_dissem.header.version = 1;
	kp_msg_config._messages.fuota_notify_dissem.header.is_up = false;
	kp_msg_config._messages.fuota_notify_dissem.header.message_seq_no = kp_seq_no;
	kp_msg_config._messages.fuota_notify_dissem.header.time_to_live = 1;
	kp_msg_config._messages.fuota_notify_dissem.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_DISSEMINATE;
	kp_msg_config._messages.fuota_notify_dissem.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.fuota_notify_dissem.header.total_block = 1;
	kp_msg_config._messages.fuota_notify_dissem.header.has_ack_window_size_message = false;
	kp_msg_config._messages.fuota_notify_dissem.header.long_packet_id = 1;
	kp_msg_config._messages.fuota_notify_dissem.header.has_current_block = false;
	kp_msg_config._messages.fuota_notify_dissem.header.has_is_big_endian = false;

	kp_msg_config._messages.fuota_notify_dissem.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.fuota_notify_dissem.header.peer_addr.mobile_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.fuota_notify_dissem.header.peer_addr.mobile_id.funcs.encode = protobuf_encode_bytes_callback;

	kp_msg_config._messages.fuota_notify_dissem.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.fuota_notify_dissem.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
		
	// the palyload
	kp_msg_config._messages.fuota_notify_dissem.appVer = 1;
	kp_msg_config._messages.fuota_notify_dissem.has_is_big_endian = false;
	kp_msg_config._messages.fuota_notify_dissem.is_big_endian = false;
	
	kp_msg_config._messages.fuota_notify_dissem.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.fuota_notify_dissem.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

	//kp_msg_config._messages.fuota_notify_dissem.fuota_task_id = 100;	
	kp_msg_config._messages.fuota_notify_dissem.fuota_task_id = kp_ftsk_id;//pt_img_dissem_ctr->f_tsk_id;
	#if(0)
		kp_msg_config._messages.fuota_notify_dissem.target_hardware_type = SKFChina_Common_HardwareType_C_NRF52840_P_BULLETNODE;
	#else
		enum sensortype kp_stype = app_pro_gen_get_active_sensor_type_v2();
		if(SS_TYPE_PREDICTP == kp_stype)
		{			
			kp_msg_config._messages.fuota_notify_dissem.target_hardware_type = SKFChina_Common_HardwareType_C_NRF52840_P_BULLETNODEPRO;
		}
		else
		{
			kp_msg_config._messages.fuota_notify_dissem.target_hardware_type = SKFChina_Common_HardwareType_C_NRF52840_P_BULLETNODE;					
		}
	#endif
		
	kp_msg_config._messages.fuota_notify_dissem.target_hardware_version = 65536;//65536;
	kp_msg_config._messages.fuota_notify_dissem.firmware_version = pt_img_dissem_ctr->img_ver;//65537;
	kp_msg_config._messages.fuota_notify_dissem.has_build_id = true;
	kp_msg_config._messages.fuota_notify_dissem.build_id = 123;
	kp_msg_config._messages.fuota_notify_dissem.force_fuota = pt_img_dissem_ctr->force_fuota;
	kp_msg_config._messages.fuota_notify_dissem.update_type = SKFChina_Common_FUOTAType_WHOLE;
	kp_msg_config._messages.fuota_notify_dissem.update_method =  SKFChina_Common_CommunicationType_BLECONNECTION_COMM; 
	
	kp_msg_config._messages.fuota_notify_dissem.which_fuota_method = SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_froto_over_ble_ots_tag;
	kp_msg_config._messages.fuota_notify_dissem.fuota_method.fuota_with_froto_over_ble_ots.isBLEUart = true;
	kp_msg_config._messages.fuota_notify_dissem.fuota_method.fuota_with_froto_over_ble_ots.TODO.arg = NULL;
	kp_msg_config._messages.fuota_notify_dissem.fuota_method.fuota_with_froto_over_ble_ots.TODO.funcs.encode = NULL;


	kp_msg_config._messages.fuota_notify_dissem.which_check_value = SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_elf_hash_value_tag ;
	kp_msg_config._messages.fuota_notify_dissem.check_value.elf_hash_value = 0x11223344;

	
	kp_msg_config._messages.fuota_notify_dissem.supplementary_info.arg = NULL;
	kp_msg_config._messages.fuota_notify_dissem.supplementary_info.funcs.encode = NULL;

	kp_msg_config._messages.fuota_notify_dissem.has_encryp = true;
	kp_msg_config._messages.fuota_notify_dissem.encryp = SKFChina_Common_Encryption_NO_ENCRYPTION;
	 
	
	
	//DBG_LOG_INFO("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		kpret.ptbuf = pt_encoded_bytes;
		kpret.len = kp_bytes_written;
		kpret.is_valid = true;
	
		#if(1) // for debug only
		DBG_LOG_INFO("msg config_dissem to set time is encoded successfully");
		util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);		
		//util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);
		// for test only		
		//util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);		
		#endif
		/*.............*/
		
	}
	
	EXIT:
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}

		if(kp_gw_id_info.buffer)
		{
			l_free( kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}
		
		return kpret;
}



extern uint32_t g_dbg_block_idx; // the block index;
#define SZ_IMG_BLOCK (50U) // the image block size

struct encoded_froto_msg_pkt util_froto_fill_msg_img_block_dissem(struct longdata_xfer_ctr*pt_img_dissem_ctr, const uint8_t*pt_gwid_str, uint32_t kp_seq_no)
{
	struct encoded_froto_msg_pkt kpret = {.is_valid = false, 0};
	pb_byte_t *pt_encoded_bytes = NULL;
	
	size_t kp_bytes_written = 0;
	SKFChina_App_AppMessage kp_msg_config = {0};
	
	//the configuration-pair
	uint64_t kp_tstamp = 1000;
	SKFChina_ConfigurationAndCommand_ConfigPair kp_cfg_pair_set_time = {0}; 
	uint32_t kp_total_blk = 0, kp_cur_blk = 0, kp_ftsk_id = 0;	
	protobuf_encode_general_bytes_t kp_img_content = {0};
	
	// the sensor id info for msg header
	protobuf_encode_general_bytes_t kp_sensor_id_info = {0};

	kp_sensor_id_info.buffer = l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	//snprintf( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH,"%s", "C4BD6A100001");
	kp_sensor_id_info.size = idstr_to_bytes_array( kp_sensor_id_info.buffer, MAX_ID_STR_LENGTH, "C4BD6A100001");

	
	// the GW ID info for msg header
	protobuf_encode_general_bytes_t kp_gw_id_info = {0};
	kp_gw_id_info.buffer = l_malloc( MAX_ID_STR_LENGTH);
	memset(kp_gw_id_info.buffer, 0, MAX_ID_STR_LENGTH);
	//snprintf(kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, "%s", pt_gwid_str);
	kp_gw_id_info.size = idstr_to_bytes_array( kp_gw_id_info.buffer, MAX_ID_STR_LENGTH, pt_gwid_str);


	// the image content	
	//static uint32_t kpu32 = 1;
	#if(1)
		pthread_mutex_lock( &pt_img_dissem_ctr->mtx);
		if(false == pt_img_dissem_ctr->is_in_process)
		{			
			pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
			DBG_LOG_ERR("the controller is inactive");
			//kpret = NULL;
			goto EXIT;
		}
		kp_img_content.size = pt_img_dissem_ctr->dtlen;
		kp_img_content.buffer = l_malloc(kp_img_content.size);
		if(NULL == kp_img_content.buffer)
		{			
			pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
			DBG_LOG_ERR("malloc fail, sz %d", kp_img_content.size);
			//kpret = NULL;
			goto EXIT;
		}
		memset( kp_img_content.buffer, 0, kp_img_content.size);
		memcpy( kp_img_content.buffer, pt_img_dissem_ctr->pkt_buf, kp_img_content.size);

		kp_cur_blk = pt_img_dissem_ctr->cur_blk_idx;
		pt_img_dissem_ctr->cur_blk_idx++;

		kp_total_blk = pt_img_dissem_ctr->total_blk;

		kp_ftsk_id = pt_img_dissem_ctr->f_tsk_id;
	
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
	#else
	#endif


	// to fill the msg structure before encoding it
	kp_msg_config.appVer = 1;
	kp_msg_config.which__messages = SKFChina_App_AppMessage_image_block_dissem_tag;

	// the header
	kp_msg_config._messages.image_block_dissem.has_header = true;
	
	kp_msg_config._messages.image_block_dissem.header.version = 1;
	kp_msg_config._messages.image_block_dissem.header.is_up = false;
	kp_msg_config._messages.image_block_dissem.header.message_seq_no = kp_seq_no;
	kp_msg_config._messages.image_block_dissem.header.time_to_live = 1;
	kp_msg_config._messages.image_block_dissem.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_BULK_DISSEMINATE;
	kp_msg_config._messages.image_block_dissem.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_config._messages.image_block_dissem.header.total_block = kp_total_blk;
	kp_msg_config._messages.image_block_dissem.header.has_ack_window_size_message = false;
	kp_msg_config._messages.image_block_dissem.header.long_packet_id = 1;
	kp_msg_config._messages.image_block_dissem.header.has_current_block = true;
	kp_msg_config._messages.image_block_dissem.header.current_block = kp_cur_blk;
	kp_msg_config._messages.image_block_dissem.header.has_is_big_endian = false;
	
	kp_msg_config._messages.image_block_dissem.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_config._messages.image_block_dissem.header.peer_addr.mobile_id.arg = &kp_gw_id_info;
	kp_msg_config._messages.image_block_dissem.header.peer_addr.mobile_id.funcs.encode = protobuf_encode_bytes_callback;

	kp_msg_config._messages.image_block_dissem.header.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.image_block_dissem.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
		
	// the palyload
	#if(1)
	kp_msg_config._messages.image_block_dissem.appVer = 1;
	kp_msg_config._messages.image_block_dissem.has_is_big_endian = false;
	kp_msg_config._messages.image_block_dissem.is_big_endian = false;
	
	kp_msg_config._messages.image_block_dissem.sensor_id.arg = &kp_sensor_id_info;
	kp_msg_config._messages.image_block_dissem.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

	kp_msg_config._messages.image_block_dissem.which_task_id = SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_fuota_task_id_tag;
	kp_msg_config._messages.image_block_dissem.task_id.fuota_task_id = kp_ftsk_id;

	kp_msg_config._messages.image_block_dissem.offset = kp_cur_blk*FILE_PKT_SIZE;

	kp_msg_config._messages.image_block_dissem.image_content.arg = &kp_img_content;
	kp_msg_config._messages.image_block_dissem.image_content.funcs.encode = protobuf_encode_bytes_callback;
	
	//kp_msg_config._messages.image_block_dissem.hash_value = 11223344;
	kp_msg_config._messages.image_block_dissem.hash_value = util_froto_bullet_elfhash((const char *)kp_img_content.buffer, kp_img_content.size);
	#endif 

	util_dbg_buf_dump( kp_img_content.buffer, kp_img_content.size);
	
	// DBG_LOG_INFO("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_config, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		kpret.ptbuf = pt_encoded_bytes;
		kpret.len = kp_bytes_written;
		kpret.is_valid = true;
	
		#if(1) // only for debug
		DBG_LOG_INFO("msg config_dissem to set time is encoded successfully");
		// the following is for debug		
		util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
		// for  test only
		//util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);		

		//util_dbg_fill_froto_msg( &g_froto_encoded_msg, pt_encoded_bytes, kp_bytes_written);
		//DBG_LOG_INFO("BKP");
		#endif
		/*	
		.............
			
		*/
	}
	
	EXIT:
		DBG_LOG_DEBUG("=====================================BKP1216");
		if(kp_sensor_id_info.buffer)
		{
			l_free(kp_sensor_id_info.buffer);
			kp_sensor_id_info.buffer = NULL;
		}
		if(kp_img_content.buffer)
		{
			l_free(kp_img_content.buffer);
			kp_img_content.buffer = NULL;
		}
		if(kp_gw_id_info.buffer)
		{
			l_free(kp_gw_id_info.buffer);
			kp_gw_id_info.buffer = NULL;
		}
		
		DBG_LOG_DEBUG("=======================================BKP1125");
		
		return kpret;
}

/*function to fill message data-uploading
ret@ST_OK
*/
struct encoded_froto_msg_pkt util_froto_fill_msg_up_data_upload(void)
{
	struct encoded_froto_msg_pkt kpret = {.is_valid = false, 0};

	SKFChina_App_AppMessage kp_msg_dtupload = {0};

	SKFChina_SensingDataUpload_DataPair kp_dtpair = {0};
	protobuf_encode_general_bytes_t kp_sensor_id = {0};
	protobuf_encode_general_bytes_t kp_mac_addr = {0};

	uint8_t*pt_encoded_bytes = NULL;
	uint32_t kp_bytes_written = 0;

	protobuf_encode_general_bytes_t kp_bat_volume = {0};
	float kp_fval = 96.88f;
	kp_bat_volume.buffer = &kp_fval;
	kp_bat_volume.size = sizeof(kp_fval);
	
	kp_sensor_id.buffer = (uint8_t*)l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id.buffer, MAX_ID_STR_LENGTH, 0);
	snprintf( kp_sensor_id.buffer, MAX_ID_STR_LENGTH, "%s", "GW007");
	kp_sensor_id.size = strlen(kp_sensor_id.buffer);

	kp_mac_addr.buffer = (uint8_t*)l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_mac_addr.buffer, 0, MAX_ID_STR_LENGTH);
	snprintf( kp_mac_addr.buffer, MAX_ID_STR_LENGTH, "%s", "112233445566");
	kp_mac_addr.size = strlen(kp_mac_addr.buffer);
	
	// fill the data-pair
	kp_dtpair.has_measurement = true;
	kp_dtpair.measurement.measure_type = SKFChina_Common_MeasurementType_REMAINING_VOLUME;
	kp_dtpair.measurement.product = SKFChina_Common_ProductType_BULLET_GATEWAY;
	kp_dtpair.measurement.measure_seq_no = 0;
	kp_dtpair.measurement.data_format = SKFChina_Common_Format_FORMAT_FLOAT32;
	//sensor ID
	kp_dtpair.measurement.sensor_id.arg = &kp_sensor_id;
	kp_dtpair.measurement.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	//mac-addr
	kp_dtpair.measurement.mac_address.arg = &kp_mac_addr;
	kp_dtpair.measurement.mac_address.funcs.encode = protobuf_encode_bytes_callback;
	//_________
	kp_dtpair.has_measurement_data = true;
	kp_dtpair.measurement_data.start_point = 0;
	kp_dtpair.measurement_data.data.arg = &kp_bat_volume;
	kp_dtpair.measurement_data.data.funcs.encode = protobuf_encode_bytes_callback;
	kp_dtpair.measurement_data.crc32_value = 11223344;
	
	
	// to fill the msg-structure
	kp_msg_dtupload.appVer = 1;
	kp_msg_dtupload.which__messages = SKFChina_App_AppMessage_data_upload_tag;

	kp_msg_dtupload._messages.data_upload.has_header = true;
	//fill the head structure
	kp_msg_dtupload._messages.data_upload.header.version = 1;
	kp_msg_dtupload._messages.data_upload.header.is_up = true;
	kp_msg_dtupload._messages.data_upload.header.time_to_live = 1;
	kp_msg_dtupload._messages.data_upload.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD;
	kp_msg_dtupload._messages.data_upload.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_dtupload._messages.data_upload.header.total_block = 1;
	kp_msg_dtupload._messages.data_upload.header.has_ack_window_size_message = false;
	kp_msg_dtupload._messages.data_upload.header.long_packet_id = 1;
	kp_msg_dtupload._messages.data_upload.header.has_current_block = true;
	kp_msg_dtupload._messages.data_upload.header.current_block = 1;
	kp_msg_dtupload._messages.data_upload.header.has_is_big_endian = false;
	//___
	kp_msg_dtupload._messages.data_upload.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_dtupload._messages.data_upload.header.peer_addr.gateway_id.arg = &kp_sensor_id;
	kp_msg_dtupload._messages.data_upload.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;

	kp_msg_dtupload._messages.data_upload.header.sensor_id.arg = &kp_sensor_id;
	kp_msg_dtupload._messages.data_upload.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	
	
	kp_msg_dtupload._messages.data_upload.appVer = 1;
	kp_msg_dtupload._messages.data_upload.data_pair.arg = &kp_dtpair;
	kp_msg_dtupload._messages.data_upload.data_pair.funcs.encode = protobuf_encode_data_pair_callback;
	
	kp_msg_dtupload._messages.data_upload.has_is_big_endian = false;
	kp_msg_dtupload._messages.data_upload.is_big_endian = false;

	//to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_dtupload, &kp_bytes_written);
	if(pt_encoded_bytes)
	{
		kpret.ptbuf = pt_encoded_bytes;
		kpret.len = kp_bytes_written;
		kpret.is_valid = true;
		
		#if(1)// only for debug
			util_dbg_buf_dump( pt_encoded_bytes, kp_bytes_written);
			// to decode
			util_froto_msg_decoding_test( pt_encoded_bytes, kp_bytes_written, SKFChina_App_AppMessage_fields);
		#endif
	}
	EXIT:
		if(kp_sensor_id.buffer)
		{
			l_free(kp_sensor_id.buffer);
			kp_sensor_id.buffer = NULL;
		}
		if(kp_mac_addr.buffer)
		{
			l_free( kp_mac_addr.buffer);
			kp_mac_addr.buffer = NULL;
		}
		return kpret;
}


/*function to fill message image-block-request
ret@ST_OK
*/
struct encoded_froto_msg_pkt  util_froto_fill_msg_up_image_block_request(const uint32_t kp_tskid, const uint32_t kp_msg_seq_no)
{
	struct encoded_froto_msg_pkt  kpret = {.is_valid = false, 0};
	
	SKFChina_App_AppMessage kp_msg_img_blk_req = {0};

	protobuf_encode_general_bytes_t kp_sensor_id = {0};
	uint8_t *pt_encoded_bytes = NULL;
	uint32_t kp_num_written = 0;
	

	kp_sensor_id.buffer = (uint8_t*)l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id.buffer, MAX_ID_STR_LENGTH, 0);
	snprintf( kp_sensor_id.buffer, MAX_ID_STR_LENGTH, "%s", "GW007");
	kp_sensor_id.size = strlen(kp_sensor_id.buffer);

	// to fill the msg-structure
	kp_msg_img_blk_req.appVer = 1;
	kp_msg_img_blk_req.which__messages = SKFChina_App_AppMessage_image_block_request_tag;
	// the head
	kp_msg_img_blk_req._messages.image_block_request.has_header = true;
	//fill the head structure
	kp_msg_img_blk_req._messages.image_block_request.header.version = 1;
	kp_msg_img_blk_req._messages.image_block_request.header.is_up = true;
	kp_msg_img_blk_req._messages.image_block_request.header.time_to_live = 1;
	kp_msg_img_blk_req._messages.image_block_request.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD;
	kp_msg_img_blk_req._messages.image_block_request.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_img_blk_req._messages.image_block_request.header.total_block = 1;
	kp_msg_img_blk_req._messages.image_block_request.header.has_ack_window_size_message = false;
	kp_msg_img_blk_req._messages.image_block_request.header.long_packet_id = 1;
	kp_msg_img_blk_req._messages.image_block_request.header.has_current_block = true;
	kp_msg_img_blk_req._messages.image_block_request.header.current_block = 1;
	kp_msg_img_blk_req._messages.image_block_request.header.has_is_big_endian = false;
	//___
	kp_msg_img_blk_req._messages.image_block_request.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_img_blk_req._messages.image_block_request.header.peer_addr.gateway_id.arg = &kp_sensor_id;
	kp_msg_img_blk_req._messages.image_block_request.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;
	
	kp_msg_img_blk_req._messages.image_block_request.header.sensor_id.arg = &kp_sensor_id;
	kp_msg_img_blk_req._messages.image_block_request.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;

	kp_msg_img_blk_req._messages.image_block_request.header.message_seq_no = kp_msg_seq_no;
	

	// the msg body
	kp_msg_img_blk_req._messages.image_block_request.appVer = 1;
	kp_msg_img_blk_req._messages.image_block_request.offset = 88;
	kp_msg_img_blk_req._messages.image_block_request.size  = 0;
	kp_msg_img_blk_req._messages.image_block_request.has_is_big_endian = false;
	kp_msg_img_blk_req._messages.image_block_request.is_big_endian = false;
	kp_msg_img_blk_req._messages.image_block_request.which_task_id = SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_file_task_id_tag;
	//kp_msg_img_blk_req._messages.image_block_request.task_id.fuota_task_id = ;
	kp_msg_img_blk_req._messages.image_block_request.task_id.file_task_id = kp_tskid; //;

	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_img_blk_req, &kp_num_written);
	if(pt_encoded_bytes)
	{
		kpret.ptbuf = pt_encoded_bytes;
		kpret.len = kp_num_written;
		kpret.is_valid = true;
	
		#if(0) // only for debug
			util_dbg_buf_dump( pt_encoded_bytes, kp_num_written);
			// to decode
			util_froto_msg_decoding_test( pt_encoded_bytes, kp_num_written, SKFChina_App_AppMessage_fields);
		#endif
	}
	EXIT:
		if(kp_sensor_id.buffer)
		{
			l_free( kp_sensor_id.buffer);
			kp_sensor_id.buffer = NULL;
		}
		
	return kpret;
}



/*fill uploading message for file-notify-upload
ret@ST_OK
*/
struct encoded_froto_msg_pkt util_froto_fill_msg_up_file_notify_upload(const uint32_t kp_tskid, const uint32_t kp_total_blk)
{
	struct encoded_froto_msg_pkt kpret = {.is_valid = false, 0};
	SKFChina_App_AppMessage kp_msg_file_notify_upload = {0};
	protobuf_encode_general_bytes_t kp_sensor_id = {0};
	uint8_t*pt_encoded_bytes = NULL;
	uint32_t kp_num_written = 0;

	kp_sensor_id.buffer = (uint8_t*)l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id.buffer, MAX_ID_STR_LENGTH, 0);
	snprintf( kp_sensor_id.buffer, MAX_ID_STR_LENGTH, "%s", "GW007");
	kp_sensor_id.size = strlen(kp_sensor_id.buffer);

	// to fill the msg-structure
	kp_msg_file_notify_upload.appVer = 1;
	kp_msg_file_notify_upload.which__messages = SKFChina_App_AppMessage_file_notify_upload_tag;
	// the head
	kp_msg_file_notify_upload._messages.file_notify_upload.has_header = true;
	//fill the head structure
	kp_msg_file_notify_upload._messages.file_notify_upload.header.version = 1;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.is_up = true;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.time_to_live = 1;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.total_block = kp_total_blk;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.has_ack_window_size_message = false;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.long_packet_id = 1;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.has_current_block = true;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.current_block = 0;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.has_is_big_endian = false;
	//___
	kp_msg_file_notify_upload._messages.file_notify_upload.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.peer_addr.gateway_id.arg = &kp_sensor_id;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;
	
	kp_msg_file_notify_upload._messages.file_notify_upload.header.sensor_id.arg = &kp_sensor_id;
	kp_msg_file_notify_upload._messages.file_notify_upload.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	// fill the msg-body
	kp_msg_file_notify_upload._messages.file_notify_upload.appVer = 1;
	
	kp_msg_file_notify_upload._messages.file_notify_upload.sensor_id.arg = &kp_sensor_id;
	kp_msg_file_notify_upload._messages.file_notify_upload.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	
	kp_msg_file_notify_upload._messages.file_notify_upload.file_type = SKFChina_Common_FileType_GATEWAY_CONFIG_FILE;
	kp_msg_file_notify_upload._messages.file_notify_upload.task_id = kp_tskid;//123;
	kp_msg_file_notify_upload._messages.file_notify_upload.update_method = SKFChina_Common_CommunicationType_BLECONNECTION_COMM;
	#if(1)
	kp_msg_file_notify_upload._messages.file_notify_upload.which_fuota_method = SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_froto_over_ble_ots_tag;
	kp_msg_file_notify_upload._messages.file_notify_upload.fuota_method.fuota_with_froto_over_ble_ots.isBLEUart = true; 
	kp_msg_file_notify_upload._messages.file_notify_upload.fuota_method.fuota_with_froto_over_ble_ots.TODO.arg = &kp_sensor_id;
	kp_msg_file_notify_upload._messages.file_notify_upload.fuota_method.fuota_with_froto_over_ble_ots.TODO.funcs.encode = protobuf_encode_bytes_callback;
	#endif
		
	kp_msg_file_notify_upload._messages.file_notify_upload.which_check_value = SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_elf_hash_value_tag;
	kp_msg_file_notify_upload._messages.file_notify_upload.check_value.elf_hash_value = 11223344;

	kp_msg_file_notify_upload._messages.file_notify_upload.has_encryp = false;
	kp_msg_file_notify_upload._messages.file_notify_upload.has_is_big_endian= false;

	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_file_notify_upload,&kp_num_written);
	if(pt_encoded_bytes)
	{
		kpret.ptbuf = pt_encoded_bytes;
		kpret.len = kp_num_written;
		kpret.is_valid = true;
	
		#if(0) // for debug only
		util_dbg_buf_dump( pt_encoded_bytes, kp_num_written);
		//to decode
		util_froto_msg_decoding_test( pt_encoded_bytes, kp_num_written, SKFChina_App_AppMessage_fields);
		#endif
	}
	EXIT:
		if(kp_sensor_id.buffer)
		{
			l_free( kp_sensor_id.buffer);
			kp_sensor_id.buffer = NULL;
		}
		return kpret;
}



/*fill uploading message for image-block-upload
ret@ST_OK
*/
extern protobuf_encode_general_bytes_t g_dbg_file_content ; // for debug only

struct encoded_froto_msg_pkt util_froto_fill_msg_up_image_block_upload(const uint32_t kp_tskid, const uint32_t kp_total_blk, const uint32_t kp_cur_blk)
{
	struct encoded_froto_msg_pkt kpret = {.is_valid = false, 0};

	SKFChina_App_AppMessage kp_msg_img_blk_upload = {0};
	protobuf_encode_general_bytes_t kp_sensor_id = {0};
	uint8_t*pt_encoded_bytes = NULL;
	uint32_t kp_num_written = 0;

	kp_sensor_id.buffer = (uint8_t*)l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id.buffer, 0,  MAX_ID_STR_LENGTH);
	snprintf( kp_sensor_id.buffer, MAX_ID_STR_LENGTH, "%s", "GW007");
	kp_sensor_id.size = strlen(kp_sensor_id.buffer);

	// to fill the msg-structure
	kp_msg_img_blk_upload.appVer = 1;
	kp_msg_img_blk_upload.which__messages = SKFChina_App_AppMessage_image_block_upload_tag;
	// the head
	kp_msg_img_blk_upload ._messages.image_block_upload.has_header = true;
	//fill the head structure
	kp_msg_img_blk_upload._messages.image_block_upload.header.version = 1;
	kp_msg_img_blk_upload._messages.image_block_upload.header.is_up = true;
	kp_msg_img_blk_upload._messages.image_block_upload.header.time_to_live = 1;
	kp_msg_img_blk_upload._messages.image_block_upload.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD;
	kp_msg_img_blk_upload._messages.image_block_upload.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_img_blk_upload._messages.image_block_upload.header.total_block = kp_total_blk;
	kp_msg_img_blk_upload._messages.image_block_upload.header.has_ack_window_size_message = false;
	kp_msg_img_blk_upload._messages.image_block_upload.header.long_packet_id = 1;
	kp_msg_img_blk_upload._messages.image_block_upload.header.has_current_block = true;
	kp_msg_img_blk_upload._messages.image_block_upload.header.current_block = kp_cur_blk;
	kp_msg_img_blk_upload._messages.image_block_upload.header.has_is_big_endian = false;
	kp_msg_img_blk_upload._messages.image_block_upload.header.message_seq_no = 66;
	//___
	kp_msg_img_blk_upload._messages.image_block_upload.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_img_blk_upload._messages.image_block_upload.header.peer_addr.gateway_id.arg = &kp_sensor_id;
	kp_msg_img_blk_upload._messages.image_block_upload.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;
	
	kp_msg_img_blk_upload._messages.image_block_upload.header.sensor_id.arg = &kp_sensor_id;
	kp_msg_img_blk_upload._messages.image_block_upload.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	// fill the msg-body
	kp_msg_img_blk_upload._messages.image_block_upload.appVer = 1;
	kp_msg_img_blk_upload._messages.image_block_upload.sensor_id.arg = &kp_sensor_id;
	kp_msg_img_blk_upload._messages.image_block_upload.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	kp_msg_img_blk_upload._messages.image_block_upload.which_task_id = SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_file_task_id_tag;
	kp_msg_img_blk_upload._messages.image_block_upload.task_id.file_task_id = kp_tskid;
	kp_msg_img_blk_upload._messages.image_block_upload.offset = 0;
	// the content
	kp_msg_img_blk_upload._messages.image_block_upload.image_content.arg = &g_dbg_file_content;
	kp_msg_img_blk_upload._messages.image_block_upload.image_content.funcs.encode = protobuf_encode_bytes_callback;
	kp_msg_img_blk_upload._messages.image_block_upload.hash_value = util_froto_bullet_elfhash( g_dbg_file_content.buffer, g_dbg_file_content.size);

	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_img_blk_upload, &kp_num_written);
	if(pt_encoded_bytes)
	{
		kpret.ptbuf = pt_encoded_bytes;
		kpret.len = kp_num_written;
		kpret.is_valid = true;
	
		#if(0) // for debug only
			util_dbg_buf_dump( pt_encoded_bytes, kp_num_written);
			util_froto_msg_decoding_test( pt_encoded_bytes, kp_num_written, SKFChina_App_AppMessage_fields);
		#endif
	}
	EXIT:
		if(kp_sensor_id.buffer)
		{
			l_free( kp_sensor_id.buffer);
			kp_sensor_id.buffer = NULL;
		}
		return kpret;
}

/*fill uploading message for current-version-upload
*/
struct encoded_froto_msg_pkt util_froto_fill_msg_up_current_version_upload(void)
{
	struct encoded_froto_msg_pkt kpret = {.is_valid = false, 0};
	
	SKFChina_App_AppMessage kp_msg_cur_version_upload = {0};
	protobuf_encode_general_bytes_t kp_sensor_id = {0};
	uint8_t*pt_encoded_bytes = NULL;
	uint32_t kp_num_written = 0;

	kp_sensor_id.buffer = (uint8_t*)l_malloc(MAX_ID_STR_LENGTH);
	memset( kp_sensor_id.buffer, MAX_ID_STR_LENGTH, 0);
	snprintf( kp_sensor_id.buffer, MAX_ID_STR_LENGTH, "%s", "GW007");
	kp_sensor_id.size = strlen(kp_sensor_id.buffer);	

	// to fill the msg-structure
	kp_msg_cur_version_upload.appVer = 1;
	kp_msg_cur_version_upload.which__messages = SKFChina_App_AppMessage_current_version_upload_tag;
	// the head
	kp_msg_cur_version_upload._messages.current_version_upload.has_header = true;
	//fill the head structure
	kp_msg_cur_version_upload._messages.current_version_upload.header.version = 1;
	kp_msg_cur_version_upload._messages.current_version_upload.header.is_up = true;
	kp_msg_cur_version_upload._messages.current_version_upload.header.time_to_live = 1;
	kp_msg_cur_version_upload._messages.current_version_upload.header.primitive_type = SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD;
	kp_msg_cur_version_upload._messages.current_version_upload.header.message_type = SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;
	kp_msg_cur_version_upload._messages.current_version_upload.header.total_block = 1;
	kp_msg_cur_version_upload._messages.current_version_upload.header.has_ack_window_size_message = false;
	kp_msg_cur_version_upload._messages.current_version_upload.header.long_packet_id = 1;
	kp_msg_cur_version_upload._messages.current_version_upload.header.has_current_block = true;
	kp_msg_cur_version_upload._messages.current_version_upload.header.current_block = 1;
	kp_msg_cur_version_upload._messages.current_version_upload.header.has_is_big_endian = false;
	//___
	kp_msg_cur_version_upload._messages.current_version_upload.header.which_peer_addr = SKFChina_Froto_FrotoHeader_gateway_id_tag;
	kp_msg_cur_version_upload._messages.current_version_upload.header.peer_addr.gateway_id.arg = &kp_sensor_id;
	kp_msg_cur_version_upload._messages.current_version_upload.header.peer_addr.gateway_id.funcs.encode = protobuf_encode_bytes_callback;
	
	kp_msg_cur_version_upload._messages.current_version_upload.header.sensor_id.arg = &kp_sensor_id;
	kp_msg_cur_version_upload._messages.current_version_upload.header.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	// fill the msg-body
	kp_msg_cur_version_upload._messages.current_version_upload.appVer = 1;
	kp_msg_cur_version_upload._messages.current_version_upload.sensor_id.arg = &kp_sensor_id;
	kp_msg_cur_version_upload._messages.current_version_upload.sensor_id.funcs.encode = protobuf_encode_bytes_callback;
	kp_msg_cur_version_upload._messages.current_version_upload.hardware_type = SKFChina_Common_HardwareType_C_AM6231_P_BULLETGW;
	kp_msg_cur_version_upload._messages.current_version_upload.hardware_type = 9999;
	kp_msg_cur_version_upload._messages.current_version_upload.firmware_version = 6666;
	kp_msg_cur_version_upload._messages.current_version_upload.has_build_id = true;
	kp_msg_cur_version_upload._messages.current_version_upload.build_id = 54321;
	
	kp_msg_cur_version_upload._messages.current_version_upload.has_current_version_supplementary_update_info = false;
	kp_msg_cur_version_upload._messages.current_version_upload.has_is_big_endian = false;
	// to encode
	pt_encoded_bytes = util_froto_encode_msg( &kp_msg_cur_version_upload, &kp_num_written);
	if(pt_encoded_bytes)
	{
		kpret.ptbuf = pt_encoded_bytes;
		kpret.len = kp_num_written;
		kpret.is_valid = true;
	
		#if(1) // for debug only
			util_dbg_buf_dump( pt_encoded_bytes, kp_num_written);
			// to decode 
			util_froto_msg_decoding_test( pt_encoded_bytes, kp_num_written, SKFChina_App_AppMessage_fields);
		#endif
	}
	EXIT:
		if(kp_sensor_id.buffer)
		{
			l_free(kp_sensor_id.buffer);
			kp_sensor_id.buffer = NULL;
		}

	return kpret;
}
//====================================================================







/*to get the message tag
ret@ the message tag is returned; negative value is returned when fail to get the message tag
*/
int32_t util_froto_pick_appmsg_tag( const pb_byte_t*pt_msgbuf, size_t kp_len)
{
	SKFChina_App_AppMessage kp_msg= SKFChina_App_AppMessage_init_zero;
	pb_istream_t kp_istream = {0};
	kp_istream = pb_istream_from_buffer( pt_msgbuf, kp_len);
	if(false == pb_decode( &kp_istream, SKFChina_App_AppMessage_fields, &kp_msg))
	{
		DBG_LOG_ERR("fail to decode AppMessage");
		return -1;
	}
	DBG_LOG_INFO("the AppMessage tag is %d", kp_msg.which__messages);
	return kp_msg.which__messages;
}

/*
return encode_submessage( stream, SKFChina_Froto_FrotoHeader_fields, SKFChina_Froto_SeqNoElement_fields, pt_seq_no);
*/
static bool decode_uint32_t(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	SKFChina_Froto_SeqNoElement *pt_buf = (SKFChina_Froto_SeqNoElement*)*arg;
	bool tpret = decode_submessage_contents( stream, SKFChina_Froto_SeqNoElement_fields, pt_buf);
	DBG_LOG_DEBUG("the ret of decode_uint32_t %d, errmsg %s, %d", tpret, stream->errmsg, pt_buf->seqNo);
	return tpret;
}


/*to decode AppMessate_config_hash_upload
ret@ST_OK when things go well
Note!!! make sure the message has the tag SKFChina_AppMessage_config_hash_upload_tag
*/
app_state_t util_froto_decoder_for_msg_config_hash_upload(const pb_byte_t*pt_msgbuf, size_t kp_len)
{
	app_state_t tpret = ST_OK;
	
	SKFChina_App_AppMessage kp_msg = SKFChina_App_AppMessage_init_zero;
	pb_istream_t kp_istream = {0};
	pb_istream_t kp_substream  = {0};
	
	protobuf_encode_general_bytes_t kp_sensor_id_a = {0};
	kp_sensor_id_a.size = 0x20;
	kp_sensor_id_a.buffer = l_malloc(kp_sensor_id_a.size);
	memset( kp_sensor_id_a.buffer, 0, kp_sensor_id_a.size);
	protobuf_encode_general_bytes_t kp_sensor_id_b = {0};
	kp_sensor_id_b.size = 0x20;
	kp_sensor_id_b.buffer = l_malloc(kp_sensor_id_b.size);
	memset( kp_sensor_id_b.buffer, 0, kp_sensor_id_b.size);

	protobuf_encode_general_bytes_t kp_gw_id = {0};
	kp_gw_id.size = 0x20;
	kp_gw_id.buffer = l_malloc(kp_sensor_id_b.size);
	memset(kp_gw_id.buffer, 0, kp_gw_id.size);
	
	
	kp_istream = pb_istream_from_buffer( pt_msgbuf, kp_len);
	if(false == pb_decode( &kp_istream, SKFChina_App_AppMessage_fields, &kp_msg))
	{
		DBG_LOG_ERR("fail to decode AppMsg");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	DBG_LOG_INFO("which_msg = %d , %d",kp_msg.which__messages, kp_msg._messages.config_hash_upload.header.which_peer_addr);
	DBG_LOG_DEBUG("version = %d, msg_seq_no %d", kp_msg._messages.config_hash_upload.header.version, kp_msg._messages.config_hash_upload.header.message_seq_no);
	#if(0)
		if(false == decode_submessage_contents( &kp_istream, SKFChina_ConfigurationAndCommand_ConfigHashUpload_fields, &kp_msg._messages.config_hash_upload))
		{
			DBG_LOG_ERR("fail to decode submessage, bytes_left %d, err %s", kp_istream.bytes_left, kp_istream.errmsg);
			tpret = ST_ERR;
			goto EXIT;
		}	
		DBG_LOG_INFO("appVer = %d, config_hash =%d", kp_msg._messages.config_hash_upload.appVer, kp_msg._messages.config_hash_upload.config_hash_value);
	#else
	memset( &kp_istream, 0, sizeof(kp_istream));
	kp_istream = pb_istream_from_buffer( pt_msgbuf, kp_len);
	SKFChina_Froto_SeqNoElement kp_seq_no = {0};

	kp_msg._messages.config_hash_upload.sensor_id.arg = &kp_sensor_id_a;
	kp_msg._messages.config_hash_upload.sensor_id.funcs.decode = decode_bytes_callback;

	kp_msg._messages.config_hash_upload.header.sensor_id.arg = &kp_sensor_id_b;
	kp_msg._messages.config_hash_upload.header.sensor_id.funcs.decode = decode_bytes_callback;

	kp_msg._messages.config_hash_upload.header.peer_addr.gateway_id.arg = &kp_gw_id;
	kp_msg._messages.config_hash_upload.header.peer_addr.gateway_id.funcs.decode = decode_bytes_callback;


	#if(0)
		kp_msg._messages.config_hash_upload.header.acked_message_seq_no.arg = &kp_seq_no;
		kp_msg._messages.config_hash_upload.header.acked_message_seq_no.funcs.decode = decode_uint32_t;
	#else
	#endif
	util_dbg_buf_dump( pt_msgbuf, kp_len);
	
	decode_unionmessage_type( &kp_istream, SKFChina_App_AppMessage_fields);	
	if(false == pb_make_string_substream( &kp_istream, &kp_substream))
	{
		DBG_LOG_ERR("fail to make substream");
		tpret = ST_ERR;
		goto EXIT;

		
	}
	#if(0)
	if(false == decode_one_filed( &kp_substream, SKFChina_ConfigurationAndCommand_ConfigHashUpload_header_tag, SKFChina_Froto_FrotoHeader_fields, &(kp_msg._messages.config_hash_upload.header)))
	{
		DBG_LOG_ERR("fail to decode one field, err %s", kp_substream.errmsg);
		tpret = ST_ERR;
		goto EXIT;
	}
	#endif
	
	DBG_LOG_DEBUG("substream->bytes_left = %d", kp_substream.bytes_left);
	if(false == pb_decode( &kp_substream, SKFChina_ConfigurationAndCommand_ConfigHashUpload_fields, &kp_msg._messages.config_hash_upload))
	{
		DBG_LOG_WARN("fail to decode msg ConfigHashUpload,errinfo:%s", kp_substream.errmsg);		
		pb_close_string_substream( &kp_istream, &kp_substream); 	
		tpret = ST_ERR;
		goto EXIT;
	}

	DBG_LOG_INFO("the message info appVer %d, which_message %d", kp_msg.appVer, kp_msg.which__messages);
	DBG_LOG_INFO("ConfigHashUpload decoded successfully!, hval = %d", kp_msg._messages.config_hash_upload.config_hash_value);
	util_dbg_buf_dump( kp_sensor_id_a.buffer, kp_sensor_id_a.size);
	util_dbg_buf_dump( kp_sensor_id_b.buffer, kp_sensor_id_b.size);
	util_dbg_buf_dump( kp_gw_id.buffer, kp_gw_id.size);
	
	pb_close_string_substream( &kp_istream, &kp_substream);		
	#endif

	
	EXIT:
		if(kp_sensor_id_a.buffer)
		{
			l_free(kp_sensor_id_a.buffer);
			kp_sensor_id_a.buffer = NULL;
		}
		if(kp_sensor_id_b.buffer)
		{
			l_free(kp_sensor_id_b.buffer);
			kp_sensor_id_b.buffer = NULL;
		}
		if(kp_gw_id.buffer)
		{
			l_free(kp_gw_id.buffer);
			kp_gw_id.buffer = NULL;
		}
		return tpret;
	
}




#if(1)// real thing



/**
 * @brief To read proto stream (pointed by @p pt_istream ) to the message
 *        tree (pointed by @p pt_head ). Finally, the tree would look like
 *        as followings:
 *
 *        head -> next (for non-submessage) -> ...
 *          |
 *          -> branch (for submessage) -> next (for non-submessage) -> ...
 *                |
 *                -> branch (for submessage) -> ...
 *               ...
 *
 * @param pt_head : INPUT, a pointer to the message tree root
 * @param pt_istream : INPUT, a pointer to the proto message
 * @param hld_msgdesc : INPUT, a pointer to the proto description
 */
bool
util_froto_format_msg_info(
  struct msg_tree_node *pt_head,
  pb_istream_t *pt_istream,
  const pb_msgdesc_t *hld_msgdesc)
{
  bool tpret = true;

  struct msg_tree_node *pt_node = NULL;
  struct msg_tree_node *pt_tail = NULL;

  pb_istream_t kp_substream = { 0 };

  pb_wire_type_t kp_wire_type = 0;
  uint32_t kp_tag = 0;
  bool kp_eof = false;

  pb_field_iter_t kp_iter = { 0 };

  if((pt_head == NULL) || (pt_istream == NULL) || (hld_msgdesc == NULL))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = false;
    return tpret;
  }
  if(pt_head->role != ND_ROOT)
  {
    DBG_LOG_ERR("A node with role ND_ROOT (%d) is expected", ND_ROOT);
    tpret = false;
    return tpret;
  }

  while(pb_decode_tag(pt_istream, &kp_wire_type, &kp_tag, &kp_eof))
  {
    if(false == pb_field_iter_begin(&kp_iter, hld_msgdesc, NULL))
    {
      DBG_LOG_ERR("Failed to initialize pb_field_iter_t pointer");
      tpret = false;
      break;
    }
#if 0 // Just for debug
    DBG_LOG_DEBUG("kp_wire_type = %d, kp_tag = %d, kp_eof = %d",
                  kp_wire_type,
                  kp_tag,
                  kp_eof);
#endif
    if(false == pb_field_iter_find(&kp_iter, kp_tag))
    {
      DBG_LOG_ERR("Failed to find iter with tag %d", kp_tag);
      tpret = false;
      break;
    }

    if(kp_wire_type == PB_WT_STRING)
    {
      // STRING, BYTES, SUBMESSAGE, FIXED_LENGTH_BYTES
      memset(&kp_substream, 0, sizeof(pb_istream_t));
      if(false == pb_make_string_substream(pt_istream, &kp_substream))
      {
        DBG_LOG_ERR("Failed to make substream");
        tpret = false;
        break;
      }

      pt_node =
        (struct msg_tree_node *)l_malloc(sizeof(struct msg_tree_node));
      if(NULL == pt_node)
      {
        DBG_LOG_ERR("Failed to malloc");
        tpret = false;
        break;
      }
      memset(pt_node, 0, sizeof(struct msg_tree_node));

      if(kp_iter.submsg_desc)
      {
        // SUBMESSAGE
        pt_node->role = ND_ROOT;
        pt_node->tag = kp_tag;
        pt_node->next = NULL;

        if(NULL == pt_head->nd_data.branch)
        {
          // Link to the branch if branch is NULL
          // In principle, all the submessage would go this
          pt_head->nd_data.branch = pt_node;
          pt_tail = pt_node;
          pt_node = NULL;
        }
        else
        {
          // Otherwise, link to the next
          // In principle, all non-submessage would go this
          pt_tail->next = pt_node;
          pt_tail = pt_node;
          pt_node = NULL;
        }
        if(false ==
           util_froto_format_msg_info(pt_tail, &kp_substream,
                                      kp_iter.submsg_desc))
        {
          DBG_LOG_ERR("Should never reach here!");
          tpret = false;
          break;
        }
      }
      else
      {
        // Otherwise, STRING, BYTES, or FIXED_LENGTH_BYTES
        uint32_t tplen = kp_substream.bytes_left >
                         0 ? kp_substream.bytes_left : 1;  // Just to avoid
                                                           // malloc(0)
        pt_node->nd_data.dtinfo.dtbuf = (void *)l_malloc(tplen);
        if(NULL == pt_node->nd_data.dtinfo.dtbuf)
        {
          DBG_LOG_ERR("Failed to malloc");
          tpret = false;
          break;
        }
        memset(pt_node->nd_data.dtinfo.dtbuf, 0, tplen);
        if(false ==
           pb_read(&kp_substream, pt_node->nd_data.dtinfo.dtbuf,
                   kp_substream.bytes_left))
        {
          DBG_LOG_ERR("Failed to read from stream");
          tpret = false;
          break;
        }

        pt_node->role = ND_INFO;
        pt_node->tag = kp_tag;
        pt_node->next = NULL;
        pt_node->nd_data.dtinfo.type = kp_wire_type;
        pt_node->nd_data.dtinfo.size = tplen;

        if(NULL == pt_head->nd_data.branch)
        {
          // Link to the branch if branch is NULL
          // In principle, all the submessage would go this
          pt_head->nd_data.branch = pt_node;
          pt_tail = pt_node;
          pt_node = NULL;
        }
        else
        {
          // Otherwise, link to the next
          // In principle, all non-submessage would go this
          pt_tail->next = pt_node;
          pt_tail = pt_node;
          pt_node = NULL;
        }
      }

      if(false == pb_close_string_substream(pt_istream, &kp_substream))
      {
        DBG_LOG_ERR("Failed to close substream");
        tpret = false;
        break;
      }
    }
    else if(kp_wire_type == PB_WT_VARINT)
    {
      // BOOL, VARINT, UVARINT, SVARINT
      pt_node =
        (struct msg_tree_node *)l_malloc(sizeof(struct msg_tree_node));
      if(NULL == pt_node)
      {
        DBG_LOG_ERR("Failed to malloc");
        tpret = false;
        break;
      }
      memset(pt_node, 0, sizeof(struct msg_tree_node));

      pt_node->nd_data.dtinfo.dtbuf = (void *)l_malloc(sizeof(int64_t));
      if(NULL == pt_node->nd_data.dtinfo.dtbuf)
      {
        DBG_LOG_ERR("Failed to malloc");
        tpret = false;
        break;
      }
      memset(pt_node->nd_data.dtinfo.dtbuf, 0, sizeof(int64_t));

      if(false ==
         pb_decode_varint(pt_istream,
                          (int64_t *)pt_node->nd_data.dtinfo.dtbuf))
      {
        DBG_LOG_ERR("Failed to decode varint");
        tpret = false;
        break;
      }

      pt_node->role = ND_INFO;
      pt_node->tag = kp_tag;
      pt_node->next = NULL;
      pt_node->nd_data.dtinfo.type = kp_wire_type;
      pt_node->nd_data.dtinfo.size = sizeof(int64_t);

      if(NULL == pt_head->nd_data.branch)
      {
        // Link to the branch if branch is NULL
        // In principle, all the submessage would go this
        pt_head->nd_data.branch = pt_node;
        pt_tail = pt_node;
        pt_node = NULL;
      }
      else
      {
        // Otherwise, link to the next
        // In principle, all non-submessage would go this
        pt_tail->next = pt_node;
        pt_tail = pt_node;
        pt_node = NULL;
      }
    }
    else if(kp_wire_type == PB_WT_32BIT)
    {
      // FIXED32
      pt_node =
        (struct msg_tree_node *)l_malloc(sizeof(struct msg_tree_node));
      if(NULL == pt_node)
      {
        DBG_LOG_ERR("Failed to malloc");
        tpret = false;
        break;
      }
      memset(pt_node, 0, sizeof(struct msg_tree_node));

      pt_node->nd_data.dtinfo.dtbuf = (void *)l_malloc(sizeof(int32_t));
      if(NULL == pt_node->nd_data.dtinfo.dtbuf)
      {
        DBG_LOG_ERR("Failed to malloc");
        tpret = false;
        break;
      }
      memset(pt_node->nd_data.dtinfo.dtbuf, 0, sizeof(int32_t));

      if(false ==
         pb_decode_fixed32(pt_istream,
                           (uint32_t *)pt_node->nd_data.dtinfo.dtbuf))
      {
        DBG_LOG_ERR("Failed to decode fixed32");
        tpret = false;
        break;
      }

      pt_node->role = ND_INFO;
      pt_node->tag = kp_tag;
      pt_node->next = NULL;
      pt_node->nd_data.dtinfo.type = kp_wire_type;
      pt_node->nd_data.dtinfo.size = sizeof(int32_t);

      if(NULL == pt_head->nd_data.branch)
      {
        // Link to the branch if branch is NULL
        // In principle, all the submessage would go this
        pt_head->nd_data.branch = pt_node;
        pt_tail = pt_node;
        pt_node = NULL;
      }
      else
      {
        // Otherwise, link to the next
        // In principle, all non-submessage would go this
        pt_tail->next = pt_node;
        pt_tail = pt_node;
        pt_node = NULL;
      }
    }
    else
    {
      // FIXED64
      // TODO: To support FIXED64
      DBG_LOG_WARN("So far, FIXED64 is not supported");
      if(false == pb_skip_field(pt_istream, kp_wire_type))
      {
        DBG_LOG_ERR("Failed to skip the field with FIXED64 (%d)",
                    kp_wire_type);
        tpret = false;
        break;
      }
    }
  }

  if(pt_node)
  {
    // If everything is OK, pt_node should be NULL
    // Otherwise, error happens
    // Memory should be released when error happened.
    if(pt_node->nd_data.dtinfo.dtbuf)
    {
      l_free(pt_node->nd_data.dtinfo.dtbuf);
      pt_node->nd_data.dtinfo.dtbuf = NULL;
    }
    l_free(pt_node);
    pt_node = NULL;
  }

  if((kp_eof == false) && (tpret == true))
  {
    DBG_LOG_ERR("A EOF is expected to be true");
    tpret = false;
  }

  return tpret;
}




/*to dupm a message tree
*/
void util_froto_dump_msg_tree(const struct msg_tree_node*pt_root)
{
	struct msg_tree_node *pt_node = NULL;
	if(pt_root == NULL)
	{
		DBG_LOG_INFO("unexpected NULL");
		return;
	}
	if(pt_root->role != ND_ROOT)
	{
		DBG_LOG_ERR("a root node is expected");
		return;
	}	
	//DBG_LOG_INFO("=============================BKP");
	pt_node = pt_root->nd_data.branch;
	while(pt_node)
	{
		DBG_LOG_INFO("0x%x-0x%x-0x%x", pt_node, pt_node->nd_data.branch, pt_node->next);
		if((pt_node->role == ND_ROOT)&&(pt_node->nd_data.branch))
		{//
			DBG_LOG_INFO("ND_ROOT(%d), tag: %d", pt_node->role, pt_node->tag);
			util_froto_dump_msg_tree( pt_node);
		}
		else
		{ // 
			DBG_LOG_INFO("ND_INFO(%d), tag =  %d, dt_size = %d, dtbuf[0] = 0x%x",pt_node->role, pt_node->tag, pt_node->nd_data.dtinfo.size, ((uint8_t*)pt_node->nd_data.dtinfo.dtbuf)[0]);
		}
		//DBG_LOG_INFO("=============================BKP");
		pt_node = pt_node->next;
	}
	//DBG_LOG_INFO("=============================BKP");		
	return;
}

/*to find the the message node on the tree
hld_root ~ the root of message tree that we are going to search for message-node
kp_nd_path ~ the path to the msg-node
ret@pointer points to the node is returned,NULL is returned when fail to locate the node

NOTE !!! DO NOT TRY to release the NODE even the memory is from the heap, call "util_froto_release_msg_tree" to have the job done 
*/
struct msg_tree_node *util_froto_locate_msg_node(const struct msg_tree_node*hld_root, const struct node_path kp_nd_path)
{
	struct msg_tree_node *pt_ret = NULL;

	struct msg_tree_node*pt_node = NULL;
	struct node_path tp_nd_path = {0};
	
	if((NULL == hld_root)||(hld_root->role != ND_ROOT))
	{
		DBG_LOG_ERR("invalid parameter");
		return pt_ret;
	}
	if(kp_nd_path.kpdepth <= 0)
	{
		DBG_LOG_ERR("invalid nodep ath");
		return pt_ret;
	}
	
	DBG_LOG_INFO("the node depth is %d", kp_nd_path.kpdepth);
	pt_node = hld_root->nd_data.branch;
	while(pt_node)
	{
		if(pt_node->tag == kp_nd_path.path[0])
		{// we found the node
			{// the branch where the node is attached
				if(kp_nd_path.kpdepth > 1)
				{
					for(uint32_t i = 0; i < (kp_nd_path.kpdepth - 1); i++)
					{// pick the node-path info
						tp_nd_path.path[i % MAX_TREE_NODE_DEPTH] = kp_nd_path.path[(i+1)%MAX_TREE_NODE_DEPTH];
					}
					tp_nd_path.kpdepth = kp_nd_path.kpdepth - 1;

					pt_ret = util_froto_locate_msg_node( pt_node, tp_nd_path);
				}
				else if(kp_nd_path.kpdepth == 1)
				{ // found the node
					pt_ret = pt_node;
				}
				else
				{
					DBG_LOG_ERR("this is not suppose to happen");
				}
				break;
			}
		}
		pt_node = pt_node->next;
	}

	return pt_ret;
}


#if(1)
/*
try to locate the the message nodes
hld_root ~ the root of message tree going to search for the message node
kp_nd_path ~ keep path info to the node
pt_node_queue ~ we put all the node found on the queue . 
ret@true is returned when things go well
NOTE !!! there might be more than on nodes on the tree we want to find
*/
bool util_froto_locate_msg_node_v2(const struct msg_tree_node*hld_root, const struct node_path kp_nd_path, struct l_queue *pt_node_queue)
{
	bool tpret = true;

	struct msg_tree_node*pt_node = NULL;
	struct node_path tp_nd_path = {0};

	if((NULL == hld_root)||(hld_root->role != ND_ROOT)||(pt_node_queue == NULL))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = false;
		return tpret;
	}
	if(kp_nd_path.kpdepth <= 0)
	{
		DBG_LOG_ERR("invalid nodep ath");
		tpret = false;
		return tpret;
	}
	
	//DBG_LOG_INFO("the node depth is %d", kp_nd_path.kpdepth);
	pt_node = hld_root->nd_data.branch;
	while(pt_node)
	{
		if(pt_node->tag == kp_nd_path.path[0])
		{// we found the node
			{// the branch where the node is attached
				if(kp_nd_path.kpdepth > 1)
				{
					for(uint32_t i = 0; i < (kp_nd_path.kpdepth - 1); i++)
					{// pick the node-path info
						tp_nd_path.path[i % MAX_TREE_NODE_DEPTH] = kp_nd_path.path[(i+1)%MAX_TREE_NODE_DEPTH];
					}
					tp_nd_path.kpdepth = kp_nd_path.kpdepth - 1;

					tpret = util_froto_locate_msg_node_v2( pt_node, tp_nd_path, pt_node_queue);
					if(tpret == false)
					{
						break;
					}
				}
				else if(kp_nd_path.kpdepth == 1)
				{ // found the node, try to push it on the queue
					 if(false == l_queue_push_tail(pt_node_queue, pt_node))
					 {
					 	DBG_LOG_ERR("fail to push node on the queue");
						tpret = false;
						//break;
					 }
				}
				else
				{
					DBG_LOG_ERR("this is not suppose to happen");
				}
				//break;
			}
		}
		pt_node = pt_node->next;
	}

	return tpret;
}

#endif


/*call this function to release the tree created by util_froto_format_msg_info
ret@TRUE is returned when things go well

NOTE!!! the tree is destroyed from the root

*/
bool util_froto_release_msg_tree(struct msg_tree_node*pt_root)
{
	struct msg_tree_node *pt_next = NULL;
	//msg_tree_node *pt_branch = NULL;

	struct msg_tree_node *hld_node = NULL;
	//DBG_LOG_WARN("=============continue to complete this============");
	if((pt_root == NULL)||(pt_root->role != ND_ROOT))
	{
		DBG_LOG_ERR("unexpected pointer");
		return false;
	}

	pt_next = pt_root->nd_data.branch;
	pt_root->nd_data.branch = NULL;
	
	while(pt_next)
	{
		if(pt_next->role == ND_INFO)
		{// ND_INFO, just releae and switch to the next 
			hld_node = pt_next;
			pt_next = pt_next->next; // switch to the next node;
			// to releae the node
			//DBG_LOG_INFO("to relase node, role = %d, tag = %d, dtsize = %d", hld_node->role, hld_node->tag, hld_node->nd_data.dtinfo.size);
			if(hld_node->nd_data.dtinfo.dtbuf)
			{ // release the data first
				l_free(hld_node->nd_data.dtinfo.dtbuf);
				hld_node->nd_data.dtinfo.dtbuf = NULL;
			}
			l_free(hld_node);
			hld_node = NULL;
		}
		else if(pt_next->role == ND_ROOT)
		{ // the ND_ROOT
			hld_node = pt_next;
			pt_next = pt_next->next;
			// to release the branch
			//DBG_LOG_INFO("to release node, role = %d, tag = %d", hld_node->role, hld_node->tag);
			util_froto_release_msg_tree( hld_node);
			// now to release the node
			if(hld_node->nd_data.branch)
			{
				DBG_LOG_ERR("Oops! this is not supposed to happen");
			}
			l_free(hld_node);
			hld_node = NULL;
		}
		else
		{//
			DBG_LOG_ERR("This is not supposed to happen, role = %d", pt_next->role);
			break;
		}
	}
	return true;
}

#endif




/*to run a msg decoding test; just for debug
pt_dtbuf ~ the buffer where the protobuf encoded-data is
kplen ~ the length of valid data in the buffer
*/
void util_froto_msg_decoding_test(const pb_byte_t*pt_dtbuf, const uint32_t kplen, const pb_msgdesc_t *hld_msgdesc)
{
	pb_istream_t kp_istream = {0};
	struct node_path kp_ndpath = {0};
	struct msg_tree_node hld_root = {0};
	struct msg_tree_node *pt_msgnd = NULL;

	
	hld_root.role = ND_ROOT;
	
	if((pt_dtbuf == NULL)||(0 == kplen)||(NULL == hld_msgdesc))
	{
		DBG_LOG_ERR("invalid parameter");
		return;
	}
	kp_istream = pb_istream_from_buffer( pt_dtbuf, kplen);
	
	if(false == util_froto_format_msg_info( &hld_root, &kp_istream,  hld_msgdesc))
	{
		DBG_LOG_ERR("fail to format msg info");
		util_froto_release_msg_tree( &hld_root);
		return;
	}
	util_froto_dump_msg_tree(&hld_root);

	// try to locate a node
	kp_ndpath.path[0] = 14;
	kp_ndpath.path[1] = 1;
	kp_ndpath.path[2] = 9;
	kp_ndpath.kpdepth = 3;
	pt_msgnd = util_froto_locate_msg_node( &hld_root, kp_ndpath);
	if(pt_msgnd)
	{
		DBG_LOG_INFO("ndoe with tag %d is found, role %d", pt_msgnd->tag, pt_msgnd->role);
		util_dbg_buf_dump((uint8_t *)(pt_msgnd->nd_data.dtinfo.dtbuf), pt_msgnd->nd_data.dtinfo.size);
	}
	else
	{
		DBG_LOG_ERR("fail to locate msg node");
	}
	kp_ndpath.path[0] = 14;
	kp_ndpath.path[1] = 1;
	//kp_ndpath.path[2] = 9;
	kp_ndpath.kpdepth = 2;
	pt_msgnd = util_froto_locate_msg_node( &hld_root, kp_ndpath);
	if(pt_msgnd)
	{
		DBG_LOG_INFO("ndoe with tag %d is found, role %d", pt_msgnd->tag, pt_msgnd->role);
	}
	else
	{
		DBG_LOG_ERR("fail to locate msg node");
	}

	#if(1)
		// test case for more than one nodes with the same path on the tree;
		struct l_queue *hld_node_queue = l_queue_new();
		kp_ndpath.path[0] = 7;
		kp_ndpath.path[1] = 5;
		kp_ndpath.path[2] = 3;
		kp_ndpath.path[3] = 1;
		kp_ndpath.path[4] = 1;
		kp_ndpath.kpdepth = 5;
		if(false == util_froto_locate_msg_node_v2(&hld_root, kp_ndpath, hld_node_queue))
		{
			DBG_LOG_INFO("fail to find the node!");
		}
		else
		{
			DBG_LOG_INFO("the number of node we found is %d", l_queue_length(hld_node_queue));
			struct l_queue_entry *pt_queue_entries = l_queue_get_entries( hld_node_queue);
			struct msg_tree_node *pt_node = 0;
			for(uint32_t i = 0; i < l_queue_length(hld_node_queue); i++)
			{
				pt_node = pt_queue_entries->data;
				pt_queue_entries = pt_queue_entries->next;
				DBG_LOG_DEBUG(" value of node %d  is %d", i,*((uint32_t*)(pt_node->nd_data.dtinfo.dtbuf)));
			}
		}
		l_queue_destroy(hld_node_queue, NULL);
		DBG_LOG_DEBUG("BKP");
	#endif
	
	util_froto_release_msg_tree(&hld_root);
}














