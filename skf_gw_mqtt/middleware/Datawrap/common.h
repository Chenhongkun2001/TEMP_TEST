#ifndef COMMON_H
#define COMMON_H

#include "global.h"
// Protobuf
#include "pb_decode.h"
#include "pb_encode.h"
// Froto
#include "Common.pb.h"
#include "DeviceAppBulletGateway.pb.h"

#include "mqtt.h"

#define CONFIG_MQTT_REQUEST_MAX_TIMEOUT_MS (15000)
#define swap16(x)                                                              \
  ((((uint16_t)(x) & 0xff00) >> 8) | (((uint16_t)(x) & 0x00ff) << 8))
#define swap32(x)                                                              \
  ((((uint32_t)(x) & 0xff000000) >> 24) |                                      \
   (((uint32_t)(x) & 0x00ff0000) >> 8) | (((uint32_t)(x) & 0x0000ff00) << 8) | \
   (((uint32_t)(x) & 0x000000ff) << 24))
#define MAX_REPLY_INFO_STRING_SIZE (80)

#define COMMON_PEER_ADDR_LEN (50)
#define OTA_URL_ADDR_LEN (2048)//1024
#define MQTT_TOPIC_LEN (50)
#define COMMON_CHECK_VALUE_LEN (4)

// #define CONFIG_SYSTEM_DEFAULT_FIRMWARE_BLOCK_SIZE (2816) #define
// CONFIG_SYSTEM_DEFAULT_FIRMWARE_PACKAGE_INFO_START (1040) #define
// CONFIG_SYSTEM_DEFAULT_FIRMWARE_DATA_START         (3072)

#define CONFIG_SKIP_PACKAGE_HEADER
typedef struct {
  uint32_t size;
  uint8_t *buffer;
} protobuf_encode_general_bytes_t;

typedef struct
{
  uint32_t cnt;
  uint64_t time[10];
}protobuf_encode_timearray_t;

typedef struct
{
  uint16_t order;
  uint16_t total;
  char type[20];  // String
  char manufacturer[10];  // String
  char name[20];  // String
  char bleName[10];  // String
  uint8_t mac[15];
  uint32_t nameId;
#if (MODBUS_FEATURE_ENABLE == 1)
  char description[70]; // String and read only 
                        // (i.e., gateway does not 
                        // parse this field from cloud)
#endif /* MODBUS_FEATURE_ENABLE == 1 */
}protobuf_encode_config_pairs_child_t;

typedef enum {
  SKFChina_DataSelectionDisseminate = 0,
  SKFChina_ConfigRetrieve,
  SKFChina_ConfigDisseminate,
  SKFChina_CommandDisseminate,
  SKFChina_VersionRetrieve,
  SKFChina_FUOTANotifiyDisseminate,
  SKFChina_ImageBlockDisseminate,
  SKFChina_FrotoAckMsg,
  SKFChina_DebugMessage,
  SKFChina_com_Unknow,
} SKFChina_msg_type_t;
typedef enum {
  SKFChina_mqtt = 0,
  SKFChina_http,
  SKFChina_req_Unknow,
} SKFChina_req_method_t;

typedef union {
  uint8_t md5_value[COMMON_CHECK_VALUE_LEN];
  uint8_t sha1_value[COMMON_CHECK_VALUE_LEN];
  uint32_t elf_hash_value;
} check_value_t;

typedef union {
  // bool cloud_url;
  uint8_t cloud_url[COMMON_PEER_ADDR_LEN];
  uint8_t gateway_id[COMMON_PEER_ADDR_LEN];
  uint8_t mobile_id[COMMON_PEER_ADDR_LEN];
} peer_addr_t;

typedef struct {
  SKFChina_Common_HardwareType hardware_type;
  uint32_t hardware_version;
  uint32_t firmware_version;
  uint32_t Image_bytes;
} firmware_package_info_t;

extern SKFChina_App_AppMessage SKF_BullGateway;
// extern SKFChina_App_AppMessage SKF_BullGateway_tx;
extern peer_addr_t peer_addr;
extern check_value_t local_check_value;
extern uint8_t sensor_id[32];
extern char device_id[32];
extern uint8_t measure_type[SENSOR_DATA_NUM];
extern uint8_t measure_type_size;
extern uint8_t acked_message_seq_no[5];
extern uint8_t ota_url[OTA_URL_ADDR_LEN];
extern uint8_t up_url[OTA_URL_ADDR_LEN];
extern uint8_t mqtt_topic[MQTT_TOPIC_LEN];
extern uint8_t image_block_content[100];
extern uint32_t ostream_data_buffer_size;
extern uint8_t ostream_data_buffer[CONFIG_DATA_BUFFER_LEN];
extern firmware_package_info_t update_package_info;
extern firmware_package_info_t current_package_info;
extern uint32_t image_offset;
extern char topic_buffer[200];
extern int image_block_req_semaphore_handle;
extern uint8_t config_item[100];
extern uint8_t fscheduler_item[100];

#endif
