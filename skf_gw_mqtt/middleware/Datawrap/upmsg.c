#include "MQTTClient.h"
#include "common.h"
#include "http.h"
#include "mqtt.h"
#include <cJSON.h>
#include "global.h"
#include "ipcmbs.h"
#include "jsonbuild.h"
#include "jsonparse.h"
#include "ppGW.h"
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <upmsg.h>
#include <ell/ell.h>
DBG_LOCAL_LOG_INFO;

#define CONFIG_UP_VIA_MQTT_NUM (60)  // Need to be >= 40 (The max. number
                                     // of devices governed by the gateway)

typedef union
{
  float float_value;
  uint32_t uint32_value;
  int32_t int32_value;
  uint64_t uint64_value;
  bool bool_value;
  char *char_ptr;
}config_pair_value_t;

typedef struct
{
  SKFChina_Common_SpecificConfigItem specific_config_item;
  uint16_t memory_id;
  uint16_t type;
  uint32_t size;
  config_pair_value_t value[10];
}protobuf_encode_config_pairs_element_t;

typedef struct
{
  protobuf_encode_config_pairs_element_t pair[CONFIG_UP_VIA_MQTT_NUM];
  uint32_t size;
}protobuf_encode_config_pairs_t;

static protobuf_encode_general_bytes_t id_arg = {0};
static protobuf_encode_general_bytes_t peer_addr_arg = {0};
static protobuf_encode_general_bytes_t id2_arg = {0};
static protobuf_encode_general_bytes_t hash_bitmap_arg = {0};
extern struct disabledGroup disabledData;
extern struct disabledGroup disabledConfig;
static SKFChina_SensingDataUpload_DataPair sensing_datapair = {0};
extern SKFChina_SensingDataUpload_Measurement middle_measure[SENSOR_DATA_NUM];
extern SKFChina_SensingDataUpload_Measurement wave_middle_measure;
extern gw_pro_data_t middle_data[SENSOR_DATA_NUM];
MQTTClient_message publishmsg = MQTTClient_message_initializer;
static gw_pro_sqlite_cond_element_t
    filterData[GW_PRO_MAX_DATA_FIELD_PER_OPERATION];
static gw_pro_command_sqlite_filter_response_t filter_rx;
static uint8_t tempdata[DATAPAIR_MAX_LEN] = {0};
static uint32_t wavelen = 0;
static uint32_t bulk_blocknum = 0;
uint8_t upwave_type = 0;
static uint8_t wavetype = 0;
static char json[CONFIG_JSON_MAX_LEN] = {0};
static uint64_t message_seq_no;
static char alarmcode[GW_PRO_MAX_STRING_LEN_BYTE] = {0};
extern int sql_msgid;
static struct msgbuf msgbufp = {0};
static gw_pro_message_header_t *pdata =
    (gw_pro_message_header_t *)msgbufp.mtext;
static FILE *upfp = NULL;
static char newjsonfile[100] = {0};
bool datupload_flag = false;
bool waveupload_flag = false;

static bool gateway_config_froto_generate(
  gw_pro_data_t *data,
  uint32_t dataNO,
  SKFChina_App_AppMessage SKF_BullGateway_tx,
  uint32_t childNum,
  struct l_queue *childrenList,
  char *send_devid,
  char *send_topic);
static void children_generate(
  gw_pro_data_t *data,
  uint32_t dataNO,
  protobuf_encode_config_pairs_child_t *child);
static bool sensor_config_froto_generate(
  gw_pro_data_t *data,
  uint32_t dataNO,
  SKFChina_App_AppMessage SKF_BullGateway_tx,
  char *send_devid,
  char *send_topic);
static bool protobuf_encode_configpair_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg);
static bool protobuf_encode_configpair_timearray_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg);
static bool protobuf_encode_configpair_childrenlist_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg);
static bool isDisabled(
  uint32_t item,
  uint16_t *disabledArray,
  uint32_t disabledNum);

static bool
protobuf_encode_configpair_childrenlist_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg)
{
  bool rt = true;
  SKFChina_ConfigurationAndCommand_ChildNode cNode;
  protobuf_encode_config_pairs_element_t *config_pair_pt =
    (protobuf_encode_config_pairs_element_t *)(*arg);
  protobuf_encode_config_pairs_child_t *child = NULL;
  struct l_queue *childrenList = NULL;
  struct l_queue_entry *entry = NULL;
  protobuf_encode_general_bytes_t childtype, childblename, childName,
                                  childmanu, childmac;
#if (MODBUS_FEATURE_ENABLE == 1)
  protobuf_encode_general_bytes_t childDesc;
#endif /* MODBUS_FEATURE_ENABLE == 1 */

  if(arg != NULL)
  {
    childrenList =
      (struct l_queue *)config_pair_pt->value[0].char_ptr;
    entry = l_queue_get_entries(childrenList);
    for(uint16_t cnt = 0;
        (cnt < config_pair_pt->size) && (entry != NULL);
        cnt++, entry = entry->next)
    {
      memset(&cNode, 0, sizeof(cNode));
      child = entry->data;
      if(child == NULL)
      {
        LOG_WARN(OUTPOINT, "Failed to pop from queue\r\n");
        return false;
      }

      cNode.order = cnt + 1;
      cNode.totalChildren = config_pair_pt->size;
      cNode.nameId = child->nameId;
      memset(&childmanu, 0, sizeof(childmanu));
      if(strlen(child->manufacturer) > 0)
      {
        childmanu.buffer = child->manufacturer;
        if(strlen(child->manufacturer) > sizeof(child->manufacturer))
        {
          childmanu.size = sizeof(child->manufacturer);
        }
        else
        {
          childmanu.size = strlen(child->manufacturer);
        }
        protobuf_encode_bytes(&(cNode.manufacturer), &childmanu);
      }
      memset(&childmac, 0, sizeof(childmac));
      if(strlen(child->mac) > 0)
      {
        childmac.buffer = child->mac;
        if(strlen(child->mac) > sizeof(child->mac))
        {
          childmac.size = sizeof(child->mac);
        }
        else
        {
          childmac.size = strlen(child->mac);
        }
        protobuf_encode_bytes(&(cNode.macAddr), &childmac);
      }
      else
      {
        childmac.buffer = child->mac;
        childmac.size = 0;
        protobuf_encode_bytes(&(cNode.macAddr), &childmac);
      }
      memset(&childtype, 0, sizeof(childtype));
      if(strlen(child->type) > 0)
      {
        childtype.buffer = child->type;
        if(strlen(child->type) > sizeof(child->type))
        {
          childtype.size = sizeof(child->type);
        }
        else
        {
          childtype.size = strlen(child->type);
        }
        protobuf_encode_bytes(&(cNode.type), &childtype);
      }
      memset(&childblename, 0, sizeof(childblename));
      if(strlen(child->bleName) > 0)
      {
        childblename.buffer = child->bleName;
        if(strlen(child->bleName) > sizeof(child->bleName))
        {
          childblename.size = sizeof(child->bleName);
        }
        else
        {
          childblename.size = strlen(child->bleName);
        }
        protobuf_encode_bytes(&(cNode.BLESensorName), &childblename);
      }
      memset(&childName, 0, sizeof(childName));
      if(strlen(child->name) > 0)
      {
        childName.buffer = child->name;
        if(strlen(child->name) > sizeof(child->name))
        {
          childName.size = sizeof(child->name);
        }
        else
        {
          childName.size = strlen(child->name);
        }
        protobuf_encode_bytes(&(cNode.name), &childName);
      }
#if (MODBUS_FEATURE_ENABLE == 1)
      memset(&childDesc, 0, sizeof(childDesc));
      if(strlen(child->description) > 0)
      {
        childDesc.buffer = child->description;
        if(strlen(child->description) > sizeof(child->description))
        {
          childDesc.size = sizeof(child->description);
        }
        else
        {
          childDesc.size = strlen(child->description);
        }
        protobuf_encode_bytes(&(cNode.description), &childDesc);
      }
#endif /* MODBUS_FEATURE_ENABLE == 1 */
      rt = pb_encode_tag_for_field(stream_p, field);
      if(rt == false)
      {
        LOG_WARN(OUTPOINT, "Failed to encode tag\r\n");
        return false;
      }

      rt = pb_encode_submessage(stream_p,
                                SKFChina_ConfigurationAndCommand_ChildNode_fields,
                                &cNode);
      if(rt == false)
      {
        LOG_WARN(OUTPOINT, "Failed to encode submessage\r\n");
        return false;
      }
    }
    return true;
  }
  else
  {
    LOG_WARN(OUTPOINT, "Arg error\r\n");
    return false;
  }
}
static bool
protobuf_encode_configpair_timearray_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg)
{
  bool rt = true;
  SKFChina_ConfigurationAndCommand_TimeArrayElement t;

  protobuf_encode_config_pairs_element_t *config_pairs_pt =
    (protobuf_encode_config_pairs_element_t *)(*arg);
  protobuf_encode_timearray_t timeArray;

  if(arg != NULL)
  {
    timeArray.cnt = config_pairs_pt->size / sizeof(uint64_t);
    for(uint16_t cnt = 0; cnt < timeArray.cnt; cnt++)
    {
      timeArray.time[cnt] = config_pairs_pt->value[cnt].uint64_value;
    }

    for(uint16_t cnt = 0; cnt < timeArray.cnt; cnt++)
    {
      t.time = timeArray.time[cnt];

      rt = pb_encode_tag_for_field(stream_p, field);
      if(rt == false)
      {
        LOG_WARN(OUTPOINT, "Failed to encode tag\r\n");
        return false;
      }

      rt = pb_encode_submessage(stream_p,
                                SKFChina_ConfigurationAndCommand_TimeArrayElement_fields,
                                &t);
      if(rt == false)
      {
        LOG_WARN(OUTPOINT, "Failed to encode submessage\r\n");
        return false;
      }
    }
    return true;
  }
  else
  {
    LOG_WARN(OUTPOINT, "Arg error\r\n");
    return false;
  }
}
static bool
protobuf_encode_configpair_callback(
  pb_ostream_t *stream_p,
  const pb_field_t *field,
  void *const *arg) {
  SKFChina_ConfigurationAndCommand_ConfigPair cfg_pair = { 0 };
  protobuf_encode_config_pairs_t *config_pair_pt =
    (protobuf_encode_config_pairs_t *)(*arg);

  for(uint32_t cnt = 0; cnt < config_pair_pt->size; cnt++)
  {
    if(config_pair_pt->pair[cnt].memory_id != 0)
    {
      cfg_pair.memory_id = config_pair_pt->pair[cnt].memory_id;
      cfg_pair.has_memory_id = true;
    }
    else
    {
      cfg_pair.has_memory_id = false;
    }

    cfg_pair.which_config_item =
      SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag;
    cfg_pair.config_item.specific_config_item =
      config_pair_pt->pair[cnt].specific_config_item;

    cfg_pair.which_config_content = config_pair_pt->pair[cnt].type;
    switch(config_pair_pt->pair[cnt].type)
    {
      case
        SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag
        :
        cfg_pair.config_content.general_config_content_bool =
          config_pair_pt->pair[cnt].value[0].bool_value;
        break;
      case
        SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag
        :
        cfg_pair.config_content.general_config_content_uint32 =
          config_pair_pt->pair[cnt].value[0].uint32_value;
        break;
      case
        SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag
        :
        cfg_pair.config_content.general_config_content_int32 =
          config_pair_pt->pair[cnt].value[0].int32_value;
        break;
      case
        SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag
        :
        cfg_pair.config_content.general_config_content_float =
          config_pair_pt->pair[cnt].value[0].float_value;
        break;
      case
        SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag
        :
        cfg_pair.config_content.time_config_content.time.arg =
          &(config_pair_pt->pair[cnt]);
        cfg_pair.config_content.time_config_content.time.funcs.encode =
          protobuf_encode_configpair_timearray_callback;
        break;
      case
        SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag:
      {
        protobuf_encode_general_bytes_t param;
        cfg_pair.config_content.work_mode_content.work_mode =
          config_pair_pt->pair[cnt].value[0].uint32_value;
        param.buffer = &(config_pair_pt->pair[cnt].value[1].uint32_value);
        param.size = 1;
        protobuf_encode_bytes(&(cfg_pair.config_content.work_mode_content.
                                parameter),
                              &param);
        break;
      }
      case
        SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_tag
        :
      {
        protobuf_encode_general_bytes_t d;
        d.buffer = config_pair_pt->pair[cnt].value[0].char_ptr;
        d.size = config_pair_pt->pair[cnt].size;
        protobuf_encode_bytes(&(cfg_pair.config_content.
                                general_config_content),
                              &d);
        break;
      }
      case SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag:
      {
        protobuf_encode_general_bytes_t ip;
        // TODO: so far only support IPV4
        cfg_pair.config_content.ip_addr.isIpv4 = true;
        ip.buffer = config_pair_pt->pair[cnt].value[0].char_ptr;
        ip.size = config_pair_pt->pair[cnt].size;
        protobuf_encode_bytes(&(cfg_pair.config_content.ip_addr.addr),
                              &ip);
        break;
      }
      case SKFChina_ConfigurationAndCommand_ConfigPair_mqtt_sec_type_tag:
      {
        protobuf_encode_general_bytes_t param;
        cfg_pair.config_content.mqtt_sec_type.secType =
          config_pair_pt->pair[cnt].value[0].uint32_value;

        param.buffer = &(config_pair_pt->pair[cnt].value[1].uint32_value);
        param.size = 1;
        protobuf_encode_bytes(&(cfg_pair.config_content.mqtt_sec_type.
                                parameter),
                              &param);
        break;
      }
      case SKFChina_ConfigurationAndCommand_ConfigPair_children_list_tag:
      {
        cfg_pair.config_content.children_list.child.arg =
          &(config_pair_pt->pair[cnt]);
        cfg_pair.config_content.children_list.child.funcs.encode =
          protobuf_encode_configpair_childrenlist_callback;
        break;
      }
      case
        SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag
        :
      default:
        LOG_WARN(OUTPOINT, "Config_content (%d) is not supported\r\n",
                 config_pair_pt->pair[cnt].type);
        break;
    }
    /* encode */
    if(!pb_encode_tag_for_field(stream_p, field))
    {
      LOG_WARN(OUTPOINT, "Failed to encode tag\r\n");
      return false;
    }
    if(!pb_encode_submessage(stream_p,
                             SKFChina_ConfigurationAndCommand_ConfigPair_fields,
                             &cfg_pair))
    {
      LOG_WARN(OUTPOINT, "Failed to encode submessage\r\n");
      return false;
    }
  }
}




static bool protobuf_encode_sensingpair_callback(pb_ostream_t *stream_p,
                                                 const pb_field_t *field,
                                                 void *const *arg) {
  protobuf_encode_general_bytes_t *local_id_arg =
      (protobuf_encode_general_bytes_t *)(*arg);
  uint8_t tmpint = 0;
  for (uint8_t i = 0; i < SENSOR_DATA_NUM; i++) {
    if (SKFChina_Common_MeasurementType_UNKNOWN_MEASUREMENT_TYPE ==
        measure_type[i])
      continue;
    memset(&sensing_datapair, 0, sizeof(SKFChina_SensingDataUpload_DataPair));
    sensing_datapair.has_measurement = true;
    sensing_datapair.has_measurement_data = true;

    memcpy(&(sensing_datapair.measurement), &middle_measure[i],
           sizeof(SKFChina_SensingDataUpload_Measurement));
    if((sensing_datapair.measurement.product != SKFChina_Common_ProductType_BULLET_NODE) &&
       (sensing_datapair.measurement.product != SKFChina_Common_ProductType_BULLET_NODE_PRO) &&
       (sensing_datapair.measurement.product != SKFChina_Common_ProductType_INSIGHT_T))
    {
      sensing_datapair.measurement.product = SKFChina_Common_ProductType_UNKNOWN_PRODUCT;
    }
    sensing_datapair.measurement.measure_type = measure_type[i];
    protobuf_encode_bytes(&(sensing_datapair.measurement.sensor_id),
                          local_id_arg);
    sensing_datapair.measurement.sample_time *= 1000;
    switch (sensing_datapair.measurement.measure_type) {
    case SKFChina_Common_MeasurementType_REMAINING_VOLUME: {

      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 6;
      id2_arg.size = 1;
      tmpint = (uint8_t)middle_data[i].data.data_float;
      id2_arg.buffer = &tmpint;
    } break;
      /* Enviromental temperature */
    case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 3;
      switch(sensing_datapair.measurement.data_format)
      {
        case SKFChina_Common_Format_FORMAT_FLOAT32:
          id2_arg.size = sizeof(float);
          id2_arg.buffer = &middle_data[i].data.data_float;
          break;
        case SKFChina_Common_Format_FORMAT_INT8:
        case SKFChina_Common_Format_FORMAT_UINT8:
        default:
          id2_arg.size = 1;
          tmpint = (uint8_t)middle_data[i].data.data_float;
          id2_arg.buffer = &tmpint;
          break;
      }
    } break;
    case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MAX: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 43;
      id2_arg.size = 1;
      tmpint = (uint8_t)middle_data[i].data.data_float;
      id2_arg.buffer = &tmpint;
    } break;
    case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 44;
      id2_arg.size = 1;
      tmpint = (uint8_t)middle_data[i].data.data_float;
      id2_arg.buffer = &tmpint;
    } break;
      /* Board temperature */
    case SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_CURRENT: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 5;
      // measure_data_unit8 = 26;
      switch(sensing_datapair.measurement.data_format)
      {
        case SKFChina_Common_Format_FORMAT_FLOAT32:
          id2_arg.size = sizeof(float);
          id2_arg.buffer = &middle_data[i].data.data_float;
          break;
        case SKFChina_Common_Format_FORMAT_INT8:
        case SKFChina_Common_Format_FORMAT_UINT8:
        default:
          id2_arg.size = 1;
          tmpint = (uint8_t)middle_data[i].data.data_float;
          id2_arg.buffer = &tmpint;
          break;
      }
    } break;
    case SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 13;
      // measure_data_unit8 = -70;
      id2_arg.size = 1;
      tmpint = (uint8_t)middle_data[i].data.data_float;
      id2_arg.buffer = &tmpint;
    } break;
      /* Advanced algorithm */
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_RUN_CODE: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 23;
      // measure_data_unit8 = 17;
      id2_arg.size = 1;
      tmpint = (uint8_t)middle_data[i].data.data_float;
      id2_arg.buffer = &tmpint;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_CONDITON_CODE: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 24;
      // measure_data_unit8 = 18;
      id2_arg.size = 1;
      tmpint = (uint8_t)middle_data[i].data.data_float;
      id2_arg.buffer = &tmpint;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE: {
      FILE *fpcode = NULL;
      uint8_t alarmcodedata[DATAPAIR_MAX_LEN] = {0};
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 25;
      // for (uint8_t i = 0; i < 9; i++) {
      //   tempdata[i] = 14 + i;
      // }
      fpcode = fopen(alarmcode, "rb");
      if (fpcode == NULL) {
        LOG_WARN(OUTPOINT, "Failed to open file %s\r\n", alarmcode);
        // goto exit;
      } else {
        id2_arg.size = fread(alarmcodedata, 1, DATAPAIR_MAX_LEN, fpcode);
        id2_arg.buffer = alarmcodedata;
        fclose(fpcode);
      }
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_MAG_PRE: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 26;
      // measure_data_float = 27.459f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_VIB_PRE: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 27;
      // measure_data_float = 29.356f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_START: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 28;
      // measure_data_float = 16.356f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_END: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 29;
      // measure_data_float = 16.985f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 30;
      // measure_data_float = 56.985f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH: {

      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 31;
      // measure_data_float = 10.354f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 32;
      // measure_data_float = 6.354f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 33;
      // measure_data_float = 32.675f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 34;
      // measure_data_float = 32.994f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 35;
      // measure_data_float = 32.994f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 36;
      // measure_data_float = 12.994f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 37;
      // measure_data_float = 59.456f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 38;
      // measure_data_float = 99.456f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 39;
      // measure_data_float = 7.29f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 40;
      // measure_data_float = 1.456f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 41;
      // measure_data_float = 78.645f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 42;
      // measure_data_float = 83.25f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    case SKFChina_Common_MeasurementType_VOLTAGE_CURRENT: {
      sensing_datapair.measurement.has_memory_id = true;
      sensing_datapair.measurement.memory_id = 14;
      // measure_data_float = 83.25f;
      id2_arg.size = sizeof(float);
      id2_arg.buffer = &middle_data[i].data.data_float;
    } break;
    default:
      LOG_WARN(OUTPOINT, "Unknown Type!\r\n");
      break;
    }
    protobuf_encode_bytes(&(sensing_datapair.measurement_data.data), &id2_arg);
    /* encode */
    if (!pb_encode_tag_for_field(stream_p, field)) {
      return false;
    }
    if (!pb_encode_submessage(stream_p,
                              SKFChina_SensingDataUpload_DataPair_fields,
                              &sensing_datapair)) {
      return false;
    }
  }
  return true;
}
static bool protobuf_encode_wavepair_callback(pb_ostream_t *stream_p,
                                              const pb_field_t *field,
                                              void *const *arg) {
  SKFChina_SensingDataUpload_DataPair datapair = {0};
  protobuf_encode_general_bytes_t *local_id_arg =
      (protobuf_encode_general_bytes_t *)(*arg);
  // id_arg.size = strlen((char *)device_id);
  // id_arg.buffer = device_id;
  memset(&datapair, 0, sizeof(SKFChina_SensingDataUpload_DataPair));
  datapair.has_measurement = true;
  datapair.has_measurement_data = true;

  memcpy(&(datapair.measurement), &wave_middle_measure,
         sizeof(SKFChina_SensingDataUpload_Measurement));
  if((datapair.measurement.product != SKFChina_Common_ProductType_BULLET_NODE) &&
     (datapair.measurement.product != SKFChina_Common_ProductType_BULLET_NODE_PRO) &&
     (datapair.measurement.product != SKFChina_Common_ProductType_INSIGHT_T))
  {
    datapair.measurement.product = SKFChina_Common_ProductType_UNKNOWN_PRODUCT;
  }
  datapair.measurement.measure_type = wavetype;
  protobuf_encode_bytes(&(datapair.measurement.sensor_id), local_id_arg);
  datapair.measurement.sample_time *= 1000;
  switch (datapair.measurement.measure_type) {
  // todo
  /* Vibration */
  case SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE: {
    datapair.measurement.has_memory_id = true;
    datapair.measurement.memory_id = 20;
    id2_arg.size = wavelen;
    id2_arg.buffer = tempdata;
    LOG_DEBUG(OUTPOINT, "wave type:%d\r\n",
              SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE);
  } break;
  case SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE: {
    datapair.measurement.has_memory_id = true;
    datapair.measurement.memory_id = 21;
    id2_arg.size = wavelen;
    id2_arg.buffer = tempdata;
    LOG_DEBUG(OUTPOINT, "wave type:%d\r\n",
              SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE);
  } break;
  case SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE: {
    datapair.measurement.has_memory_id = true;
    datapair.measurement.memory_id = 22;
    id2_arg.size = wavelen;
    id2_arg.buffer = tempdata;
    LOG_DEBUG(OUTPOINT, "wave type:%d\r\n",
              SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE);
  } break;
  case SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE: {
    datapair.measurement.has_memory_id = true;
    datapair.measurement.memory_id = 0; // No memory id
    id2_arg.size = wavelen;
    id2_arg.buffer = tempdata;
    LOG_DEBUG(OUTPOINT, "wave type:%d\r\n",
              SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE);
  } break;
  default:
    LOG_WARN(OUTPOINT, "Unknown Type!\r\n");
    break;
  }
  protobuf_encode_bytes(&(datapair.measurement_data.data), &id2_arg);
  /* encode */
  if (!pb_encode_tag_for_field(stream_p, field)) {
    return false;
  }
  if (!pb_encode_submessage(
          stream_p, SKFChina_SensingDataUpload_DataPair_fields, &datapair)) {
    return false;
  }
  return true;
}

// up msg
void Simple_upload(pb_size_t which_msg,
                   SKFChina_App_AppMessage SKF_BullGateway_tx, char *send_topic,
                   char *devid) {
  char send_devid[32] = {0};
  memcpy(send_devid, devid, sizeof(send_devid));
  sem_wait(&sem);
  uint8_t ostream_data_buffer[CONFIG_DATA_BUFFER_LEN] = {0};
  uint32_t ostream_data_buffer_size = CONFIG_DATA_BUFFER_LEN;
  if(message_seq_no == 0)
    message_seq_no = time(NULL);
#ifndef CONFIG_SKIP_PACKAGE_HEADER
  ostream_data_buffer[0] = 0xaa;
  ostream_data_buffer[1] = 0x02;
  ostream_data_buffer[0] = 0x03;

  pb_ostream_t ostream = pb_ostream_from_buffer(&ostream_data_buffer[3],
                                                ostream_data_buffer_size - 3);
#else
  pb_ostream_t ostream =
      pb_ostream_from_buffer(ostream_data_buffer, ostream_data_buffer_size);
#endif
  switch (which_msg) {
    //   version upload
  case SKFChina_App_AppMessage_current_version_upload_tag: {
    //  header
    SKF_BullGateway_tx._messages.current_version_upload.header.version = 1;
    SKF_BullGateway_tx._messages.current_version_upload.header.which_peer_addr =
        SKFChina_Froto_FrotoHeader_cloud_url_tag;
    SKF_BullGateway_tx._messages.current_version_upload.header.is_up = true;
    SKF_BullGateway_tx._messages.current_version_upload.header.message_seq_no =
        message_seq_no++; // start from  NO.1
    SKF_BullGateway_tx._messages.current_version_upload.header.time_to_live =
        1; // default
    SKF_BullGateway_tx._messages.current_version_upload.header.primitive_type =
        SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD; // default
    SKF_BullGateway_tx._messages.current_version_upload.header.message_type =
        SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE; // default
    SKF_BullGateway_tx._messages.current_version_upload.header.total_block =
        1; // default
    SKF_BullGateway_tx._messages.current_version_upload.header
        .has_ack_window_size_message = false; // default // default
    SKF_BullGateway_tx._messages.current_version_upload.header.long_packet_id =
        1; // default
    SKF_BullGateway_tx._messages.current_version_upload.header
        .has_current_block = false; // default
    SKF_BullGateway_tx._messages.current_version_upload.header
        .has_is_big_endian = false; // little endian // default
    SKF_BullGateway_tx._messages.current_version_upload.header.which_peer_addr =
        SKFChina_Froto_FrotoHeader_cloud_tag;
    SKF_BullGateway_tx._messages.current_version_upload.header.peer_addr.cloud =
        true;
    id_arg.size = strlen(send_devid);
    id_arg.buffer = send_devid;
    protobuf_encode_bytes(
        &(SKF_BullGateway_tx._messages.current_version_upload.header.sensor_id),
        &id_arg);
    // payload
    SKF_BullGateway_tx._messages.current_version_upload.has_header = true;
    SKF_BullGateway_tx._messages.current_version_upload.appVer = 1;
    SKF_BullGateway_tx._messages.current_version_upload.has_build_id = false;
    SKF_BullGateway_tx._messages.current_version_upload
        .has_current_version_supplementary_update_info = false;
    SKF_BullGateway_tx._messages.current_version_upload.has_is_big_endian =
        false;
    protobuf_encode_bytes(
        &(SKF_BullGateway_tx._messages.current_version_upload.sensor_id),
        &id_arg);
  } break;
    // data upload
  case SKFChina_App_AppMessage_data_upload_tag: {
    snprintf(alarmcode, sizeof(alarmcode) - 1, "%s",
             middle_data[0].data.data_char);
    //  header
    SKF_BullGateway_tx._messages.data_upload.header.version = 1;
    SKF_BullGateway_tx._messages.data_upload.header.which_peer_addr =
        SKFChina_Froto_FrotoHeader_cloud_url_tag;
    SKF_BullGateway_tx._messages.data_upload.header.is_up = true;
    SKF_BullGateway_tx._messages.data_upload.header.time_to_live = 1; // default
    SKF_BullGateway_tx._messages.data_upload.header.primitive_type =
        SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD; // default
    SKF_BullGateway_tx._messages.data_upload.header.message_type =
        SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;                  // default
    SKF_BullGateway_tx._messages.data_upload.header.total_block = 1; // default
    SKF_BullGateway_tx._messages.data_upload.header
        .has_ack_window_size_message = false; // default

    SKF_BullGateway_tx._messages.data_upload.header.has_current_block =
        false; // default
    SKF_BullGateway_tx._messages.data_upload.header.has_is_big_endian =
        true; // little endian // default
    SKF_BullGateway_tx._messages.data_upload.header.is_big_endian = false;
    SKF_BullGateway_tx._messages.data_upload.header.message_seq_no =
        (message_seq_no++);
    SKF_BullGateway_tx._messages.data_upload.header.long_packet_id =
        SKF_BullGateway_tx._messages.data_upload.header
            .message_seq_no; // default
    id_arg.size = strlen(send_devid);
    id_arg.buffer = send_devid;
    protobuf_encode_bytes(
        &(SKF_BullGateway_tx._messages.data_upload.header.sensor_id), &id_arg);
    SKF_BullGateway_tx._messages.data_upload.header.which_peer_addr =
        SKFChina_Froto_FrotoHeader_cloud_tag;
    SKF_BullGateway_tx._messages.data_upload.header.peer_addr.cloud = true;
    // payload
    SKF_BullGateway_tx._messages.data_upload.has_header = true;
    SKF_BullGateway_tx._messages.data_upload.appVer = 1;
    SKF_BullGateway_tx._messages.data_upload.has_is_big_endian = true;
    SKF_BullGateway_tx._messages.data_upload.is_big_endian = false;
    SKF_BullGateway_tx._messages.data_upload.data_pair.funcs.encode =
        protobuf_encode_sensingpair_callback;
    // middle_measure
    SKF_BullGateway_tx._messages.data_upload.data_pair.arg = &id_arg;
  } break;

  case SKFChina_App_AppMessage_fuota_status_tag: {
    //  header
    SKF_BullGateway_tx._messages.fuota_status.header.version = 1;
    SKF_BullGateway_tx._messages.fuota_status.header.which_peer_addr =
        SKFChina_Froto_FrotoHeader_gateway_id_tag;
    SKF_BullGateway_tx._messages.fuota_status.header.is_up = true;
    SKF_BullGateway_tx._messages.fuota_status.header.message_seq_no =
        message_seq_no++; // start from  NO.1
    SKF_BullGateway_tx._messages.fuota_status.header.time_to_live =
        1; // default
    SKF_BullGateway_tx._messages.fuota_status.header.primitive_type =
        SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD; // default
    SKF_BullGateway_tx._messages.fuota_status.header.message_type =
        SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;                   // default
    SKF_BullGateway_tx._messages.fuota_status.header.total_block = 1; // default
    SKF_BullGateway_tx._messages.fuota_status.header
        .has_ack_window_size_message = false; // default
    SKF_BullGateway_tx._messages.fuota_status.header.long_packet_id =
        1; // default
    SKF_BullGateway_tx._messages.fuota_status.header.has_current_block =
        false; // default
    SKF_BullGateway_tx._messages.fuota_status.header.has_is_big_endian =
        false; // little endian                                     // default
    id_arg.size = strlen(send_devid);
    id_arg.buffer = send_devid;
    protobuf_encode_bytes(
        &(SKF_BullGateway_tx._messages.current_version_upload.header.sensor_id),
        &id_arg);
    // protobuf_encode_general_bytes_t peer_addr_arg;
    peer_addr_arg.size = COMMON_PEER_ADDR_LEN;
    peer_addr_arg.buffer = peer_addr.gateway_id;
    protobuf_encode_bytes(&(SKF_BullGateway_tx._messages.current_version_upload
                                .header.peer_addr.gateway_id),
                          &peer_addr_arg);
    // payload
    SKF_BullGateway_tx._messages.fuota_status.has_header = true;
    SKF_BullGateway_tx._messages.fuota_status.appVer = 1;
    SKF_BullGateway_tx._messages.fuota_status.update_status =
        SKFChina_FirmwareUpdateOverTheAir_UpdateStatus_FUOTA_SUCCESSFUL;
    SKF_BullGateway_tx._messages.fuota_status.received_bytes =
        update_package_info.Image_bytes;
    SKF_BullGateway_tx._messages.fuota_status.has_is_big_endian = false;
    id2_arg.size = strlen(send_devid);
    id2_arg.buffer = send_devid;
    protobuf_encode_bytes(
        &(SKF_BullGateway_tx._messages.fuota_status.sensor_id), &id2_arg);
  } break;
    //  config hash
  case SKFChina_App_AppMessage_config_hash_upload_tag: {
    //  header
    SKF_BullGateway_tx._messages.config_hash_upload.header.version = 1;
    SKF_BullGateway_tx._messages.config_hash_upload.header.which_peer_addr =
        SKFChina_Froto_FrotoHeader_cloud_url_tag;
    SKF_BullGateway_tx._messages.config_hash_upload.header.is_up = true;
    SKF_BullGateway_tx._messages.config_hash_upload.header.message_seq_no =
        message_seq_no++; // start from  NO.1
    SKF_BullGateway_tx._messages.config_hash_upload.header.time_to_live =
        1; // default
    SKF_BullGateway_tx._messages.config_hash_upload.header.primitive_type =
        SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD; // default
    SKF_BullGateway_tx._messages.config_hash_upload.header.message_type =
        SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE; // default
    SKF_BullGateway_tx._messages.config_hash_upload.header.total_block =
        1; // default
    SKF_BullGateway_tx._messages.config_hash_upload.header
        .has_ack_window_size_message = false; // default // default
    SKF_BullGateway_tx._messages.config_hash_upload.header.long_packet_id =
        1; // default
    SKF_BullGateway_tx._messages.config_hash_upload.header.has_current_block =
        false; // default
    SKF_BullGateway_tx._messages.config_hash_upload.header.has_is_big_endian =
        false; // little endian                                     // default

    id_arg.size = strlen(send_devid);
    id_arg.buffer = send_devid;
    protobuf_encode_bytes(
        &(SKF_BullGateway_tx._messages.config_hash_upload.header.sensor_id),
        &id_arg);
    SKF_BullGateway_tx._messages.config_hash_upload.header.which_peer_addr =
        SKFChina_Froto_FrotoHeader_cloud_tag;
    SKF_BullGateway_tx._messages.config_hash_upload.header.peer_addr.cloud =
        true;
    SKF_BullGateway_tx._messages.config_hash_upload.has_header = true;
    SKF_BullGateway_tx._messages.config_hash_upload.appVer = 1;
    SKF_BullGateway_tx._messages.config_hash_upload.has_last_edit_time = true;
    SKF_BullGateway_tx._messages.config_hash_upload.has_is_big_endian = false;

    id2_arg.size = strlen(send_devid);
    id2_arg.buffer = send_devid;
    protobuf_encode_bytes(
        &(SKF_BullGateway_tx._messages.config_hash_upload.sensor_id), &id2_arg);
    
    if(strstr(send_topic, GATAWAY_NAME))
    {
      // For the gateway's configuration bitmap
      if(disabledConfig.gwCnt > 0)
      {
        hash_bitmap_arg.size = 
          sizeof(disabledConfig.GW[0]) * disabledConfig.gwCnt;
        hash_bitmap_arg.buffer = (uint8_t *)disabledConfig.GW;
        protobuf_encode_bytes(
          &(SKF_BullGateway_tx._messages.config_hash_upload.config_hash_bitmap), 
          &hash_bitmap_arg);
      }
    }else if(strstr(send_topic, SENSOR_NAME))
    {
      // For the PredictSensor's configuration bitmap
      if(disabledConfig.predictSensorCnt > 0)
      {
        hash_bitmap_arg.size = 
          sizeof(disabledConfig.PredictSensor[0]) * disabledConfig.predictSensorCnt;
        hash_bitmap_arg.buffer = (uint8_t *)disabledConfig.PredictSensor;
        protobuf_encode_bytes(
          &(SKF_BullGateway_tx._messages.config_hash_upload.config_hash_bitmap), 
          &hash_bitmap_arg);
      }
    }else if(strstr(send_topic, SENSOR_T_NAME))
    {
      // For the Insight-T's configuration bitmap
      if(disabledConfig.insightTCnt > 0)
      {
        hash_bitmap_arg.size = 
          sizeof(disabledConfig.InsightT[0]) * disabledConfig.insightTCnt;
        hash_bitmap_arg.buffer = (uint8_t *)disabledConfig.InsightT;
        protobuf_encode_bytes(
          &(SKF_BullGateway_tx._messages.config_hash_upload.config_hash_bitmap), 
          &hash_bitmap_arg);
      }
    }
    // TODO: More products can be added here if necessary

  } break;
    //   return the ack info
  case SKFChina_App_AppMessage_command_dissem_tag: {
    //  header
    SKF_BullGateway_tx._messages.ack.header.version = 1;
    SKF_BullGateway_tx._messages.ack.header.which_peer_addr =
        SKFChina_Froto_FrotoHeader_cloud_url_tag;
    SKF_BullGateway_tx._messages.ack.header.is_up = true;
    SKF_BullGateway_tx._messages.ack.header.message_seq_no =
        message_seq_no++; // start from  NO.1
    SKF_BullGateway_tx._messages.ack.header.time_to_live = 1; // default
    SKF_BullGateway_tx._messages.ack.header.primitive_type =
        SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD; // default
    SKF_BullGateway_tx._messages.ack.header.message_type =
        SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;          // default
    SKF_BullGateway_tx._messages.ack.header.total_block = 1; // default
    SKF_BullGateway_tx._messages.ack.header.has_ack_window_size_message =
        false;                                                  // default
    SKF_BullGateway_tx._messages.ack.header.long_packet_id = 1; // default
    SKF_BullGateway_tx._messages.ack.header.has_current_block =
        false; // default
    SKF_BullGateway_tx._messages.ack.header.has_is_big_endian =
        false; // little endian // default

    id_arg.size = strlen(send_devid);
    id_arg.buffer = send_devid;
    protobuf_encode_bytes(&(SKF_BullGateway_tx._messages.ack.header.sensor_id),
                          &id_arg);
    SKF_BullGateway_tx._messages.ack.header.which_peer_addr =
        SKFChina_Froto_FrotoHeader_cloud_tag;
    SKF_BullGateway_tx._messages.ack.header.peer_addr.cloud = true;
    // payload
  } break;
  case SKFChina_App_AppMessage_config_retrieve_tag: {
    switch (SKF_BullGateway._messages.config_retrieve.payload) {
    case SKFChina_ConfigurationAndCommand_RetrievePayload_SENSOR_CONFIG_FILE_CONFIG: {
      // if (!SKF_BullGateway._messages.config_retrieve.has_name_id) {
      //   goto exit;
      // }
      filterData[0].condition = gw_pro_sqlite_cond_str_equal;
      filterData[0].field = 4;
      // snprintf(filterData[0].condParam.string_value.s,
      //          GW_PRO_MAX_STRING_LEN_BYTE - 1, "'%s'", "C4BD6A123457");
      snprintf(filterData[0].condParam.string_value.s,
               GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", topic_clientID);

      if (ipc_sql_filter_item(DEVICE_LIST_TABLE, 1, filterData, &msgbufp) ||
          pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
          pdata->command_or_response.responseMsg.response ==
              gw_pro_response_failed) {
        LOG_WARN(OUTPOINT, "Failed to get data from Dev table\r\n");
        return;
      }
      memset(&filter_rx, 0, sizeof(filter_rx));
      memcpy(&filter_rx,
             &pdata->command_or_response.responseMsg.responseInfo
                  .sqlite_filter_item_reponse,
             sizeof(gw_pro_command_sqlite_filter_response_t));
      if(strlen(up_url) != 0)
      {
        // If url is provided, then uploading sensor's configuration via http
#if 1
        memset(&msgbufp, 0, sizeof(msgbufp));
        if (ipc_sql_query_item(SENSOR_CONFIGURE_TABLE,
                              filter_rx.filteredItemIdx[0], &msgbufp) ||
            pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
            pdata->command_or_response.responseMsg.response ==
                gw_pro_response_failed) {
          LOG_WARN(OUTPOINT, "Failed to get data from sensor table\r\n");
          goto exit;
        }
        if (pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response.dataNum == 0) {
          snprintf(json, sizeof(json) - 1, DEFAULT_CONFIG_FILE_CONTENT,
                  topic_clientID);
        } else {
          sensor_config_build(pdata->command_or_response.responseMsg.responseInfo
                                  .sqlite_update_item_response.inquiredData,
                              pdata->command_or_response.responseMsg.responseInfo
                                  .sqlite_update_item_response.dataNum,
                              json, sizeof(json) - 1);
        }

        if (0 == strlen(json)) {
          LOG_WARN(OUTPOINT, "Invalid json data\r\n");
          goto exit;
        }
        snprintf(newjsonfile, sizeof(newjsonfile) - 1, "%s_%s", topic_clientID,
                SENSOR_CONFIG_FILE_NAME);
        upfp = fopen(newjsonfile, "w");
        if (upfp == NULL) {
          LOG_WARN(OUTPOINT, "Failed to open sensorconfig.json\r\n");
          goto exit;
        }
        fputs(json, upfp);
        fclose(upfp);
#endif
        LOG_INFO(OUTPOINT, "send to upload sensorconfig file!\r\n");
#ifdef CONFIG_HTTP_UPLOAD_FILE
        snprintf(upfilename, sizeof(upfilename) - 1, "%s",
                "\"file=@sensorConfig.json\"");
#else
        snprintf(upfilename, sizeof(upfilename) - 1,
                "\"file=@%s_sensorConfig.json\"", topic_clientID);
#endif
        enable_http_task(MAX_UPLOAD_ARGS);
        sleep(1);
        goto exit;
      }else{
        // If url is NOT provided, then uploading sensor's configuration via mqtt
        memset(&msgbufp, 0, sizeof(msgbufp));
        if (ipc_sql_query_item(SENSOR_CONFIGURE_TABLE,
                              filter_rx.filteredItemIdx[0], &msgbufp) ||
            pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
            pdata->command_or_response.responseMsg.response ==
                gw_pro_response_failed) {
          LOG_WARN(OUTPOINT, "Failed to get data from sensor table\r\n");
          goto exit;
        }
        if (pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response.dataNum == 0) {
          // If nothing in the database (which probably means it is a new sensor)
          if(sensor_config_froto_generate(pdata->command_or_response.responseMsg.responseInfo
                                      .sqlite_update_item_response.inquiredData,
                                       pdata->command_or_response.responseMsg.responseInfo
                                      .sqlite_update_item_response.dataNum,
                                      SKF_BullGateway_tx,
                                      send_devid,
                                      send_topic) != true)
          {
            LOG_WARN(OUTPOINT, "Failed to generate and send a default sensor configuration\r\n");
          }
        } else {
          if(sensor_config_froto_generate(pdata->command_or_response.responseMsg.responseInfo
                                      .sqlite_update_item_response.inquiredData,
                                       pdata->command_or_response.responseMsg.responseInfo
                                      .sqlite_update_item_response.dataNum,
                                      SKF_BullGateway_tx,
                                      send_devid,
                                      send_topic) != true)
          {
            LOG_WARN(OUTPOINT, "Failed to generate and send a sensor configuration\r\n");
          }
        }
        sleep(1);
        goto exit;
      }
    } break;
    case SKFChina_ConfigurationAndCommand_RetrievePayload_GATEWAY_CONFIG_FILE_CONFIG: {
      if (strcmp(topic_clientID, clientID)) {
        goto exit;
      }
      if(strlen(up_url) != 0)
      {
        // If url is provided, then uploading gateway's configuration via http
        // need a url ,then here should start a http function to post file
#if 1
        gw_config_build(pdata, json, sizeof(json) - 1);
        if (0 == strlen(json)) {
          LOG_WARN(OUTPOINT, "Invalid json data\r\n");
          goto exit;
        }
        snprintf(newjsonfile, sizeof(newjsonfile) - 1, "%s_%s", topic_clientID,
                GATEWAY_CONFIG_FILE_NAME);
        upfp = fopen(newjsonfile, "w");
        if (upfp == NULL) {
          LOG_WARN(OUTPOINT, "Failed to open sensorconfig.json\r\n");
          goto exit;
        }
        fputs(json, upfp);
        fclose(upfp);
#if (0) // for verification only, original file would be deleted by ~/tmp/cleanup.sh soon
        LOG_INFO(OUTPOINT, "save upload gateway file for test!\r\n");
        FILE *tmpfp = NULL;
        tmpfp = fopen("/home/root/skf_gw_mqtt/test_gwConfig.json", "w");
        if (tmpfp == NULL) {
          LOG_WARN(OUTPOINT, "Failed to open gwconfig.json\r\n");
          goto exit;
        }
        fputs(json, tmpfp);
        fclose(tmpfp);
#endif
#endif
        LOG_INFO(OUTPOINT, "send to upload gateway file!\r\n");
#ifdef CONFIG_HTTP_UPLOAD_FILE
        snprintf(upfilename, sizeof(upfilename) - 1, "%s",
                "\"file=@gwConfig.json\"");
#else
        snprintf(upfilename, sizeof(upfilename) - 1, "\"file=@%s_gwConfig.json\"",
                topic_clientID);
#endif
        enable_http_task(MAX_UPLOAD_ARGS);
        sleep(1);
        goto exit;
      }else{
        // If url is NOT provided, then uploading gateway's configuration via mqtt
        gw_pro_command_sqlite_query_item_response_t _resp;

        memset(&msgbufp, 0, sizeof(msgbufp));
        if (ipc_sql_query_item(GATEWAY_CONFIGURE_TABLE, 0, &msgbufp) ||
          pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
          pdata->command_or_response.responseMsg.response ==
          gw_pro_response_failed) {
          LOG_WARN(OUTPOINT, "Failed to get data from gateway table\r\n");
          goto exit;
        }
        memcpy(&_resp, &(pdata->command_or_response.responseMsg.responseInfo.
          sqlite_update_item_response), sizeof(_resp));
        
        filterData[0].condition = gw_pro_sqlite_cond_all;
        filterData[0].field = 1;
        if (ipc_sql_filter_item(DEVICE_LIST_TABLE, 1, filterData, &msgbufp) ||
          pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
          pdata->command_or_response.responseMsg.response ==
              gw_pro_response_failed) {
          LOG_WARN(OUTPOINT, "Failed to get data from Dev table\r\n");
          return;
        }
        memset(&filter_rx, 0, sizeof(filter_rx));
        memcpy(&filter_rx,
             &pdata->command_or_response.responseMsg.responseInfo
                  .sqlite_filter_item_reponse,
             sizeof(gw_pro_command_sqlite_filter_response_t));
        
        struct l_queue *childrenList = NULL;      
        if(filter_rx.dataNum > 0)
        {
          childrenList = l_queue_new();
          if(childrenList == NULL)
          {
            LOG_WARN(OUTPOINT, "Failed to create queue\r\n");
            return;
          }
        }
        for(uint16_t i = 0; i < filter_rx.dataNum; i++)
        {
          memset(&msgbufp, 0, sizeof(msgbufp));
          if (ipc_sql_query_item(DEVICE_LIST_TABLE, 
                                 filter_rx.filteredItemIdx[i],
                                &msgbufp) ||
              pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
              pdata->command_or_response.responseMsg.response ==
              gw_pro_response_failed) {
            LOG_WARN(OUTPOINT, "Failed to get data from dev table\r\n");
            return;
          }
          protobuf_encode_config_pairs_child_t * child = 
            malloc(sizeof(protobuf_encode_config_pairs_child_t));
          if(child == NULL)
          {
            LOG_WARN(OUTPOINT, "Failed to malloc\r\n");
            return;
          }
          children_generate(&pdata->command_or_response.responseMsg.responseInfo.
                            sqlite_update_item_response.inquiredData,
                            pdata->command_or_response.responseMsg.responseInfo.
                            sqlite_update_item_response.dataNum,
                            child);

          if(l_queue_push_tail(childrenList, child) != true)
          {
            LOG_WARN(OUTPOINT, "Failed to push to the queue\r\n");
            return;
          }
        }

        if(gateway_config_froto_generate(_resp.inquiredData,
                                         _resp.dataNum,
                                         SKF_BullGateway_tx,
                                         filter_rx.dataNum,
                                         childrenList,
                                         send_devid,
                                         send_topic) != true)
        {
          LOG_WARN(OUTPOINT, "Failed to generate and send a gateway configuration\r\n");
        }
        
        do{
          protobuf_encode_config_pairs_child_t * child;
          child = l_queue_pop_head(childrenList);
          if(child != NULL)
          {
            free(child);
          }else{
            l_queue_destroy(childrenList, NULL);
            break;
          }
        }while(1);
     
        sleep(1);
        goto exit;
      }
    } break;
    default:
      LOG_WARN(OUTPOINT, "Invalid file config requestment!\r\n");
      break;
    }
  } break;
  }
  // encode the BullGateway
  SKF_BullGateway_tx.appVer = 1;
  SKF_BullGateway_tx.which__messages = which_msg;
  pb_encode(&ostream, SKFChina_App_AppMessage_fields, &SKF_BullGateway_tx);
  // send to peer
  publishmsg.payload = ostream_data_buffer;
  publishmsg.payloadlen = ostream.bytes_written;
  // publishmsg.qos = mqtt_qos_one;
  publishmsg.qos = mqtt_qos_two;
  publishmsg.retained = 0;
  LOG_DEBUG(OUTPOINT, "msg: ");
  for (int i = 0; i < publishmsg.payloadlen; i++) {
    printf("%02x ", ((char *)publishmsg.payload)[i]);
  }
  printf("\r\n");
  for (uint8_t i = 0; i < MAX_REPUB_NUM; i++) {
    if (MQTTCLIENT_SUCCESS == mqtt_send(&publishmsg, send_topic)) {
      datupload_flag = true;
      break;
    }
  }
exit:
  sem_post(&sem);
}
void Simple_bulk_upload(pb_size_t which_msg,
                        SKFChina_App_AppMessage SKF_BullGateway_tx,
                        char *send_topic, char *devid) {
  char filename[100] = {0};
  uint8_t local_wave = 0;
  bool send_ok = true;
  // char send_topic[200] = {0};
  char send_devid[32] = {0};
  snprintf(filename, sizeof(filename) - 1, "%s", middle_data[0].data.data_char);
  // memcpy(send_topic, topic_buffer, sizeof(send_topic));
  memcpy(send_devid, devid, sizeof(send_devid));
  local_wave = upwave_type;
  sem_wait(&sem);
  wavetype = local_wave;
  uint32_t filesize = 0;
  struct stat statbuf = {0};
  FILE *fp1 = NULL;
  uint32_t bulk_index = 0;
  uint32_t temp_long_packet_id = 0;
  bool long_packet_id_flag = true;
  uint8_t ostream_data_buffer[CONFIG_DATA_BUFFER_LEN] = {0};
  uint32_t ostream_data_buffer_size = CONFIG_DATA_BUFFER_LEN;
#ifndef CONFIG_SKIP_PACKAGE_HEADER
  ostream_data_buffer[0] = 0xaa;
  ostream_data_buffer[1] = 0x02;
  ostream_data_buffer[0] = 0x03;
  pb_ostream_t ostream = pb_ostream_from_buffer(&ostream_data_buffer[3],
                                                ostream_data_buffer_size - 3);
#else
  pb_ostream_t ostream = {0};
#endif
  if(message_seq_no == 0)
    message_seq_no = time(NULL);
  stat(filename, &statbuf);
  filesize = statbuf.st_size;
  // if (filesize == 0)
  //   goto exit;
  fp1 = fopen(filename, "rb");
  if (fp1 == NULL) {
    LOG_WARN(OUTPOINT, "Failed to open file %s\r\n", filename);
    goto exit;
  }
  memcpy(&wave_middle_measure, &middle_measure[0], sizeof(wave_middle_measure));
  bulk_blocknum = filesize / DATAPAIR_MAX_LEN;
  if (filesize % DATAPAIR_MAX_LEN)
    bulk_blocknum++;
  switch (which_msg) {
  case SKFChina_App_AppMessage_data_upload_tag: {
    send_ok = true;
    do {
      ostream =
          pb_ostream_from_buffer(ostream_data_buffer, ostream_data_buffer_size);
      //  header
      SKF_BullGateway_tx._messages.data_upload.header.version = 1;
      SKF_BullGateway_tx._messages.data_upload.header.which_peer_addr =
          SKFChina_Froto_FrotoHeader_cloud_url_tag;
      SKF_BullGateway_tx._messages.data_upload.header.is_up = true;
      SKF_BullGateway_tx._messages.data_upload.header.message_seq_no =
          message_seq_no++; // start from  NO.1
      SKF_BullGateway_tx._messages.data_upload.header.time_to_live =
          1; // default
      SKF_BullGateway_tx._messages.data_upload.header.primitive_type =
          SKFChina_Froto_FrotoPmtType_SIMPLE_BULK_UPLOAD; // default
      SKF_BullGateway_tx._messages.data_upload.header.message_type =
          SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE; // default
      SKF_BullGateway_tx._messages.data_upload.header.total_block =
          bulk_blocknum; // default
      SKF_BullGateway_tx._messages.data_upload.header.current_block =
          bulk_index;
      SKF_BullGateway_tx._messages.data_upload.header
          .has_ack_window_size_message = false; // default
      SKF_BullGateway_tx._messages.data_upload.header.has_current_block =
          true; // default
      SKF_BullGateway_tx._messages.data_upload.header.has_is_big_endian =
          true; // little endian // default
      SKF_BullGateway_tx._messages.data_upload.header.is_big_endian = false;
      SKF_BullGateway_tx._messages.data_upload.header.message_seq_no =
          (message_seq_no++);
      if (long_packet_id_flag) {
        temp_long_packet_id =
            SKF_BullGateway_tx._messages.data_upload.header.message_seq_no;
        long_packet_id_flag = false;
      }
      SKF_BullGateway_tx._messages.data_upload.header.long_packet_id =
          temp_long_packet_id; // default
      id_arg.size = strlen(send_devid);
      id_arg.buffer = send_devid;
      protobuf_encode_bytes(
          &(SKF_BullGateway_tx._messages.data_upload.header.sensor_id),
          &id_arg);
      SKF_BullGateway_tx._messages.data_upload.header.which_peer_addr =
          SKFChina_Froto_FrotoHeader_cloud_tag;
      SKF_BullGateway_tx._messages.data_upload.header.peer_addr.cloud = true;
      // payload
      wavelen = fread(tempdata, 1, DATAPAIR_MAX_LEN, fp1);
      SKF_BullGateway_tx._messages.data_upload.has_header = true;
      SKF_BullGateway_tx._messages.data_upload.appVer = 1;
      SKF_BullGateway_tx._messages.data_upload.has_is_big_endian = true;
      SKF_BullGateway_tx._messages.data_upload.is_big_endian = false;
      SKF_BullGateway_tx._messages.data_upload.data_pair.funcs.encode =
          protobuf_encode_wavepair_callback;
      // middle_measure
      SKF_BullGateway_tx._messages.data_upload.data_pair.arg = &id_arg;
      // SKF_BullGateway_tx._messages.data_upload.data_pair.arg =
      //     &sensing_datapair;
      // encode the BullGateway
      SKF_BullGateway_tx.appVer = 1;
      SKF_BullGateway_tx.which__messages = which_msg;
      pb_encode(&ostream, SKFChina_App_AppMessage_fields, &SKF_BullGateway_tx);
      // send to peer
      publishmsg.payload = ostream_data_buffer;
      publishmsg.payloadlen = ostream.bytes_written;
      // publishmsg.qos = mqtt_qos_one;
      publishmsg.qos = mqtt_qos_two;
      publishmsg.retained = 0;
      LOG_DEBUG(OUTPOINT, "msg(package[%d]): ", bulk_index);
      for (int i = 0; i < publishmsg.payloadlen; i++) {
        printf("%02x ", ((char *)publishmsg.payload)[i]);
      }
      printf("\r\n");
      for (uint8_t i = 0; i < MAX_REPUB_NUM; i++) {
        if (MQTTCLIENT_SUCCESS == mqtt_send(&publishmsg, send_topic)) {
          break;
        } else {
          if (i + 1 < MAX_REPUB_NUM)
            sleep(10);
          else
            send_ok = false;
        }
      }
      bulk_index++;
      if(bulk_index >= bulk_blocknum)
      {
        break;
      }
      usleep(100000);
    } while (!feof(fp1) && send_ok);
    if (send_ok)
      waveupload_flag = true;
    fclose(fp1);
  } break;
  }
exit:
  sem_post(&sem);
}

static bool
sensor_config_froto_generate(
  gw_pro_data_t *data,
  uint32_t dataNO,
  SKFChina_App_AppMessage SKF_BullGateway_tx,
  char *send_devid,
  char *send_topic) {
  uint8_t ostream_data_buffer[CONFIG_DATA_BUFFER_LEN] = { 0 };
  uint32_t ostream_data_buffer_size = CONFIG_DATA_BUFFER_LEN;

#ifndef CONFIG_SKIP_PACKAGE_HEADER
  ostream_data_buffer[0] = 0xaa;
  ostream_data_buffer[1] = 0x02;
  ostream_data_buffer[0] = 0x03;

  pb_ostream_t ostream = pb_ostream_from_buffer(&ostream_data_buffer[3],
                                                ostream_data_buffer_size -
                                                3);
#else
  pb_ostream_t ostream =
    pb_ostream_from_buffer(ostream_data_buffer, ostream_data_buffer_size);
#endif
  protobuf_encode_config_pairs_t config_pair;
  uint32_t config_pair_cnt = 0;
  bool ret = true;
  char _send_topic[200];
  COINFIG_MQTT_PUBLISH_CONFIG_TOPIC(_send_topic, SENSOR_NAME,
                                    send_devid);

  //  header
  SKF_BullGateway_tx._messages.specific_config_upload.header.version = 1;
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  which_peer_addr =
    SKFChina_Froto_FrotoHeader_cloud_url_tag;
  SKF_BullGateway_tx._messages.specific_config_upload.header.is_up = true;
  SKF_BullGateway_tx._messages.specific_config_upload.header.time_to_live
    = 1;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  primitive_type =
    SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.message_type
    =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.total_block =
    1;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header
  .has_ack_window_size_message = false;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  long_packet_id =
    1;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  has_current_block =
    false;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  has_is_big_endian =
    false;  // little endian (default)

  id_arg.size = strlen(send_devid);
  id_arg.buffer = send_devid;
  protobuf_encode_bytes(
    &(SKF_BullGateway_tx._messages.specific_config_upload.header.sensor_id),
    &id_arg);
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  which_peer_addr =
    SKFChina_Froto_FrotoHeader_cloud_tag;
  SKF_BullGateway_tx._messages.specific_config_upload.header.peer_addr.
  cloud =
    true;
  SKF_BullGateway_tx._messages.specific_config_upload.has_header = true;

  SKF_BullGateway_tx._messages.specific_config_upload.appVer = 1;
  id2_arg.size = strlen(send_devid);
  id2_arg.buffer = send_devid;
  protobuf_encode_bytes(
    &(SKF_BullGateway_tx._messages.specific_config_upload.sensor_id),
    &id2_arg);
  SKF_BullGateway_tx._messages.specific_config_upload.product =
    SKFChina_Common_ProductType_UNKNOWN_PRODUCT;
  SKF_BullGateway_tx._messages.specific_config_upload.has_is_big_endian =
    false;

  SKF_BullGateway_tx.appVer = 1;
  SKF_BullGateway_tx.which__messages =
    SKFChina_App_AppMessage_specific_config_upload_tag;

  SKF_BullGateway_tx._messages.specific_config_upload.config_pair.arg =
    (void *)&config_pair;
  SKF_BullGateway_tx._messages.specific_config_upload.config_pair.funcs.
  encode = protobuf_encode_configpair_callback;

  if(dataNO != 0)
  {
    for(uint32_t i = 0; i < dataNO; i++)
    {
      LOG_DEBUG(OUTPOINT, "data[i].field = %d\r\n", data[i].field);
      switch(data[i].field)
      {
        case 8:
        {
          // "INT DEFAULT 0",
          // "lastEditTime",
          config_pair.pair[config_pair_cnt].memory_id = 205;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint64_value);
          config_pair.pair[config_pair_cnt].value[0].uint64_value =
            data[i].data.data_uint64;
          config_pair_cnt++;
          break;
        }
        case 11: 
        {
          // "TEXT DEFAULT ' '",
          // "type",
          LOG_DEBUG(OUTPOINT, "type:%s\r\n", data[i].data.data_char);
          if (strcmp("PredictSensor", data[i].data.data_char))
          {
            SKF_BullGateway_tx._messages.specific_config_upload.product =
              SKFChina_Common_ProductType_BULLET_NODE;
          }else{
            if (strcmp("PredictSensorPro", data[i].data.data_char))
            {
              SKF_BullGateway_tx._messages.specific_config_upload.product = 
                SKFChina_Common_ProductType_BULLET_NODE_PRO;
            }else{
              if (strcmp("Insight-T", data[i].data.data_char))
              {
                SKF_BullGateway_tx._messages.specific_config_upload.product = 
                  SKFChina_Common_ProductType_INSIGHT_T;
              }else{
                SKF_BullGateway_tx._messages.specific_config_upload.product = 
                  SKFChina_Common_ProductType_UNKNOWN_PRODUCT;
              }
            }
          }
          break;
        }
        case 12:
        {
          // "INT DEFAULT 0",
          // "sensorMode",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_WORK_MODE,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 61;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_WORK_MODE;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value)
            *
            2;
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          // For the parameter of work mode
          config_pair.pair[config_pair_cnt].value[1].uint32_value = 0;
          config_pair_cnt++;
          break;
        }
        case 13:
        {
          // "REAL DEFAULT 0.0",
          // "batteryAlarmThreshold",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_BATTERY_LOW_ALARM_THRESHOLD_PERCENTAGE,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 59;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_BATTERY_LOW_ALARM_THRESHOLD_PERCENTAGE;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 14:
        {
          // "INT DEFAULT 0",
          // "txPower_adv",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_ADV_DBM,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 137;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_ADV_DBM;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].int32_value);
          config_pair.pair[config_pair_cnt].value[0].int32_value =
            data[i].data.data_int32;
          config_pair_cnt++;
          break;
        }
        case 15:
        {
          // "INT DEFAULT 0",
          // "dataRate_auxAdv",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_AUX_ADV_KBPS,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 138;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_AUX_ADV_KBPS;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 16:
        {
          // "INT DEFAULT 0",
          // "txPower_auxAdv",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_AUX_ADV_DBM,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 139;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_AUX_ADV_DBM;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].int32_value);
          config_pair.pair[config_pair_cnt].value[0].int32_value =
            data[i].data.data_int32;
          config_pair_cnt++;
          break;
        }
        case 17:
        {
          // "INT DEFAULT 0",
          // "period_quickPolling",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_QUICKPOLLING_PERIOD_S,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 143;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_QUICKPOLLING_PERIOD_S;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 18:
        {
          // "INT DEFAULT 0",
          // "referenceTime_quickPolling",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 146;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint64_value);
          config_pair.pair[config_pair_cnt].value[0].uint64_value =
            data[i].data.data_uint64;
          config_pair_cnt++;
          break;
        }
        case 19:
        {
          // "INT DEFAULT 0",
          // "period_regularSensing",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_REGULARSENSING_PERIOD_S,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 144;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_REGULARSENSING_PERIOD_S;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 20:
        {
          // "INT DEFAULT 0",
          // "referenceTime_regularSensing",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 147;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint64_value);
          config_pair.pair[config_pair_cnt].value[0].uint64_value =
            data[i].data.data_uint64;
          config_pair_cnt++;
          break;
        }
        case 21:
        {
          // "INT DEFAULT 0",
          // "period_comm",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_COMMUNICATION_PERIOD_S,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 145;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_COMMUNICATION_PERIOD_S;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 22:
        {
          // "INT DEFAULT 0",
          // "referenceTime_period_comm",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 148;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint64_value);
          config_pair.pair[config_pair_cnt].value[0].uint64_value =
            data[i].data.data_uint64;
          config_pair_cnt++;
          break;
        }
        case 23:
        {
          // "INT DEFAULT 0",
          // "PRE_ACQ_VIB_FS_HZ",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_FS_HZ,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 63;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_FS_HZ;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 24:
        {
          // "INT DEFAULT 0",
          // "PRE_ACQ_VIB_N",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_N,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 64;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_N;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 25:
        {
          // "INT DEFAULT 0",
          // "PRE_ACQ_VIB_AXIS_ACQ_EVAL",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_AXIS_ACQ_EVAL,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 65;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_AXIS_ACQ_EVAL;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 26:
        {
          // "INT DEFAULT 0",
          // "PRE_ACQ_VIB_RANGE",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_RANGE,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 66;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_RANGE;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 27:
        {
          // "INT DEFAULT 0",
          // "PRE_ACQ_MAG_FS_HZ",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_FS_HZ,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 67;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_FS_HZ;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 28:
        {
          // "INT DEFAULT 0",
          // "PRE_ACQ_MAG_N",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_N,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 68;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_N;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 29:
        {
          // "INT DEFAULT 0",
          // "PRE_ACQ_MAG_AXIS_ACQ_EVAL",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_AXIS_ACQ_EVAL,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 69;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_AXIS_ACQ_EVAL;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 30:
        {
          // "INT DEFAULT 0",
          // "Facc",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_RATE_HZ,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 50;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_RATE_HZ;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 31:
        {
          // "INT DEFAULT 0",
          // "Nacc",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_PTS,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 51;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_PTS;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 32:
        {
          // "INT DEFAULT 0",
          // "ACQ_VIB_AXIS_ACQ_EVAL",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACC_SENSOR_AXIS,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 70;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACC_SENSOR_AXIS;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 33:
        {
          // "INT DEFAULT 0",
          // "ACQ_VIB_RANGE",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACC_SENSOR_RANGE_G,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 71;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACC_SENSOR_RANGE_G;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 34:
        {
          // "INT DEFAULT 0",
          // "ACQ_MAG_FS_HZ",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACQ_MAG_FS_HZ,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 72;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACQ_MAG_FS_HZ;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 35:
        {
          // "INT DEFAULT 0",
          // "ACQ_MAG_N",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACQ_MAG_N,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 73;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACQ_MAG_N;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 36:
        {
          // "INT DEFAULT 0",
          // "ACQ_MAG_AXIS_ACQ_EVAL",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACQ_MAG_AXIS_ACQ_EVAL,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 74;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACQ_MAG_AXIS_ACQ_EVAL;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 37:
        {
          // "REAL DEFAULT 0.364",
          // "FS_COEF",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FS_COEF,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 62;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FS_COEF;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 38:
        {
          // "REAL DEFAULT 0.0",
          // "GEE_COEF",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_GEE_COEF,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 75;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_GEE_COEF;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 39:
        {
          // "REAL DEFAULT 0.0",
          // "V_COEF",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_V_COEF,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 76;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_V_COEF;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 40:
        {
          // "INT DEFAULT 0",
          // "MEAS_POSITION",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_MEAS_POSITION,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 77;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_MEAS_POSITION;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 41:
        {
          // "INT DEFAULT 0",
          // "MEAS_LOAD",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_MEAS_LOAD,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 78;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_MEAS_LOAD;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 42:
        {
          // "INT DEFAULT 0",
          // "MEAS_AXIS_VIB",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_MEAS_AXIS_VIB,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 79;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_MEAS_AXIS_VIB;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 43:
        {
          // "INT DEFAULT 0",
          // "MEAS_AXIS_MAG",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_MEAS_AXIS_MAG,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 80;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_MEAS_AXIS_MAG;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 44:
        {
          // "BOOLEAN DEFAULT false",
          // "VIB_START_FG",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_VIB_START_FG,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 81;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_VIB_START_FG;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 45:
        {
          // "REAL DEFAULT 0.0",
          // "VIB_RMS_START_TL",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_VIB_RMS_START_TL,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 82;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_VIB_RMS_START_TL;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 46:
        {
          // "BOOLEAN DEFAULT false",
          // "MAG_START_FG",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_MAG_START_FG,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 83;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_MAG_START_FG;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 47:
        {
          // "REAL DEFAULT 0.0",
          // "MAG_RMS_START_TL",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_MAG_RMS_START_TL,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 84;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_MAG_RMS_START_TL;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 48:
        {
          // "BOOLEAN DEFAULT false",
          // "MAG_STABLE_FG",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_MAG_STABLE_FG,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 85;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_MAG_STABLE_FG;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 49:
        {
          // "REAL DEFAULT 0.0",
          // "MAG_RMS_VAR_TH",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_MAG_RMS_VAR_TH,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 86;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_MAG_RMS_VAR_TH;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 50:
        {
          // "REAL DEFAULT 0.0",
          // "RPM_VAR_RANGE",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_RPM_VAR_RANGE,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 87;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_RPM_VAR_RANGE;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 51:
        {
          // "INT DEFAULT 0",
          // "DEC_LOGIC_TEMP_M",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_M,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 88;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_M;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 52:
        {
          // "INT DEFAULT 0",
          // "DEC_LOGIC_TEMP_N",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_N,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 89;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_N;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 53:
        {
          // "INT DEFAULT 0",
          // "DEC_LOGIC_LEARN_NUM_TEMP",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_TEMP,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 90;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_TEMP;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 54:
        {
          // "INT DEFAULT 0",
          // "DEC_LOGIC_VIB_M",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_M,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 91;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_M;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 55:
        {
          // "INT DEFAULT 0",
          // "DEC_LOGIC_VIB_N",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_N,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 92;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_N;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 56:
        {
          // "INT DEFAULT 0",
          // "DEC_LOGIC_LEARN_NUM_VIB",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_VIB,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 93;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_VIB;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 57:
        {
          // "INT DEFAULT 0",
          // "DEC_LOGIC_LEARN_NUM_MAG",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_MAG,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 94;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_MAG;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 58:
        {
          // "BOOLEAN DEFAULT false",
          // "FUNC_ANOM_TEMP",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FUNC_ANOM_TEMP,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 95;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FUNC_ANOM_TEMP;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 59:
        {
          // "BOOLEAN DEFAULT false",
          // "FUNC_ANOM_OV",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FUNC_ANOM_OV,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 96;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FUNC_ANOM_OV;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 60:
        {
          // "BOOLEAN DEFAULT false",
          // "FUNC_ANOM_MECH",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MECH,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 97;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MECH;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 61:
        {
          // "BOOLEAN DEFAULT false",
          // "FUNC_ANOM_BRG",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FUNC_ANOM_BRG,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 98;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FUNC_ANOM_BRG;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 62:
        {
          // "BOOLEAN DEFAULT false",
          // "FUNC_ANOM_LUB",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FUNC_ANOM_LUB,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 99;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FUNC_ANOM_LUB;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 63:
        {
          // "BOOLEAN DEFAULT false",
          // "FUNC_ANOM_MTR",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MTR,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 100;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MTR;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 64:
        {
          // "BOOLEAN DEFAULT false",
          // "FUNC_ANOM_GEAR",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FUNC_ANOM_GEAR,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 108;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FUNC_ANOM_GEAR;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 65:
        {
          // "BOOLEAN DEFAULT false",
          // "FUNC_ANOM_FAN",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FUNC_ANOM_FAN,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 109;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FUNC_ANOM_FAN;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 66:
        {
          // "BOOLEAN DEFAULT false",
          // "FUNC_ANOM_PUMP",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FUNC_ANOM_PUMP,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 110;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FUNC_ANOM_PUMP;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
          if(data[i].data.data_uint8)
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = true;
          }
          else
          {
            config_pair.pair[config_pair_cnt].value[0].bool_value = false;
          }
          config_pair_cnt++;
          break;
        }
        case 67:
        {
          // "INT DEFAULT 0",
          // "ASSET_LEVEL",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ASSET_LEVEL,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 111;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ASSET_LEVEL;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 68:
        {
          // "INT DEFAULT 0",
          // "FLEX_TYPE",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FLEX_TYPE,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 112;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FLEX_TYPE;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 69:
        {
          // "REAL DEFAULT 0.0",
          // "BORE_DIAMETER_MM",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_BORE_DIAMETER_MM,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 113;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_BORE_DIAMETER_MM;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 70:
        {
          // "REAL DEFAULT 0.0",
          // "RUN_SPEED_RPM",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_RUN_SPEED_RPM,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 114;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_RUN_SPEED_RPM;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 71:
        {
          // "REAL DEFAULT 0.0",
          // "BRG_INFO_BPFO",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFO,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 115;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFO;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 72:
        {
          // "REAL DEFAULT 0.0",
          // "BRG_INFO_BPFI",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFI,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 116;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFI;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 73:
        {
          // "REAL DEFAULT 0.0",
          // "BRG_INFO_BSF",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_BRG_INFO_BSF,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 117;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_BRG_INFO_BSF;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 74:
        {
          // "REAL DEFAULT 0.0",
          // "BRG_INFO_FTF",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_BRG_INFO_FTF,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 118;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_BRG_INFO_FTF;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 75:
        {
          // "INT DEFAULT 0",
          // "MTR_INFO_FL",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_MTR_INFO_FL,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 119;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_MTR_INFO_FL;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 76:
        {
          // "INT DEFAULT 0",
          // "MTR_INFO_BAR",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_MTR_INFO_BAR,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 120;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_MTR_INFO_BAR;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 77:
        {
          // "INT DEFAULT 0",
          // "GEAR_INFO_TOOTH",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_GEAR_INFO_TOOTH,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 121;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_GEAR_INFO_TOOTH;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 78:
        {
          // "INT DEFAULT 0",
          // "FAN_INFO_BLADE",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_FAN_INFO_BLADE,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 122;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_FAN_INFO_BLADE;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 79:
        {
          // "INT DEFAULT 0",
          // "PUMP_INFO_VANE",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_PUMP_INFO_VANE,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 123;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_PUMP_INFO_VANE;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 80:
        {
          // "REAL DEFAULT 0.0",
          // "TEMP_OV_ALERT_CDEGREE",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_TEMP_OV_ALERT_CDEGREE,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 124;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_TEMP_OV_ALERT_CDEGREE;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 81:
        {
          // "INT DEFAULT 0",
          // "AlarmThrestemp",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_TEMPERATURE_ALARM_THRESHOLD_CDGREE,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 56;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_TEMPERATURE_ALARM_THRESHOLD_CDGREE;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].int32_value);
          config_pair.pair[config_pair_cnt].value[0].int32_value =
            data[i].data.data_int32;
          config_pair_cnt++;
          break;
        }
        case 82:
        {
          // "REAL DEFAULT 0.0",
          // "ACC_OV_ALERT",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACC_OV_ALERT,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 125;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACC_OV_ALERT;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 83:
        {
          // "REAL DEFAULT 0.0",
          // "ACC_OV_ALARM",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACC_OV_ALARM,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 126;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACC_OV_ALARM;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 84:
        {
          // "REAL DEFAULT 0.0",
          // "ACC_HAL_ALERT",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACC_HAL_ALERT,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 127;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACC_HAL_ALERT;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 85:
        {
          // "REAL DEFAULT 0.0",
          // "ACC_HAL_ALARM",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ACC_HAL_ALARM,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 128;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ACC_HAL_ALARM;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 86:
        {
          // "REAL DEFAULT 0.0",
          // "VEL_OV_ALERT",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_VEL_OV_ALERT,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 129;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_VEL_OV_ALERT;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 87:
        {
          // "REAL DEFAULT 0.0",
          // "VEL_OV_ALARM",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_VEL_OV_ALARM,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 130;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_VEL_OV_ALARM;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 88:
        {
          // "REAL DEFAULT 0.0",
          // "VEL_HAL_ALERT",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_VEL_HAL_ALERT,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 131;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_VEL_HAL_ALERT;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 89:
        {
          // "REAL DEFAULT 0.0",
          // "VEL_HAL_ALARM",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_VEL_HAL_ALARM,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 132;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_VEL_HAL_ALARM;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 90:
        {
          // "REAL DEFAULT 0.0",
          // "ENV_OV_ALERT",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ENV_OV_ALERT,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 133;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ENV_OV_ALERT;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 91:
        {
          // "REAL DEFAULT 0.0",
          // "ENV_OV_ALARM",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ENV_OV_ALARM,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 134;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ENV_OV_ALARM;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 92:
        {
          // "REAL DEFAULT 0.0",
          // "ENV_HAL_ALERT",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ENV_HAL_ALERT,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 135;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ENV_HAL_ALERT;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 93:
        {
          // "REAL DEFAULT 0.0",
          // "ENV_HAL_ALARM",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_ENV_HAL_ALARM,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 136;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_ENV_HAL_ALARM;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].float_value);
          config_pair.pair[config_pair_cnt].value[0].float_value =
            data[i].data.data_float;
          config_pair_cnt++;
          break;
        }
        case 94:
        {
          // "INT DEFAULT 0",
          // "waveDataAcqPeriod",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_PERIOD_S,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 150;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_PERIOD_S;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
          config_pair.pair[config_pair_cnt].value[0].uint32_value =
            data[i].data.data_uint32;
          config_pair_cnt++;
          break;
        }
        case 95:
        {
          // "INT DEFAULT 0",
          // "waveDataAcqReference",
          if(true ==
             isDisabled(
               SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S,
               disabledConfig.PredictSensor,
               disabledConfig.predictSensorCnt))
          {
            // It is disabled ...
            break;
          }
          config_pair.pair[config_pair_cnt].memory_id = 149;
          config_pair.pair[config_pair_cnt].specific_config_item =
            SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S;
          config_pair.pair[config_pair_cnt].type =
            SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
          config_pair.pair[config_pair_cnt].size =
            sizeof(config_pair.pair[config_pair_cnt].value[0].uint64_value);
          config_pair.pair[config_pair_cnt].value[0].uint64_value =
            data[i].data.data_uint64;
          config_pair_cnt++;
          break;
        }
        default:
        {
          // Do nothing, just skip
          break;
        }
      }
      if(config_pair_cnt == (CONFIG_UP_VIA_MQTT_NUM - 1))
      {
        config_pair.size = config_pair_cnt;
        SKF_BullGateway_tx._messages.specific_config_upload.header.
        message_seq_no = message_seq_no++;  // start from  NO.1
        pb_encode(&ostream, SKFChina_App_AppMessage_fields,
                  &SKF_BullGateway_tx);
        // send to peer
        publishmsg.payload = ostream_data_buffer;
        publishmsg.payloadlen = ostream.bytes_written;
        // publishmsg.qos = mqtt_qos_one;
        publishmsg.qos = mqtt_qos_two;
        publishmsg.retained = 0;
        LOG_DEBUG(OUTPOINT, "msg: ");
        for(int i = 0; i < publishmsg.payloadlen; i++)
        {
          printf("%02x ", ((char *)publishmsg.payload)[i]);
        }
        printf("\r\n");
        for(uint8_t i = 0; i < MAX_REPUB_NUM; i++)
        {
          if(MQTTCLIENT_SUCCESS == mqtt_send(&publishmsg, _send_topic))
          {
            datupload_flag = true;
            break;
          }
        }
        memset(ostream_data_buffer, 0, CONFIG_DATA_BUFFER_LEN);
        ostream = pb_ostream_from_buffer(ostream_data_buffer,
                                         ostream_data_buffer_size);
        config_pair_cnt = 0;
      }
    }
  }
  else
  {
    // If no configuration (probably it's a new added sensor)
    // "INT DEFAULT 0",
    // "lastEditTime",
    config_pair.pair[config_pair_cnt].memory_id = 205;
    config_pair.pair[config_pair_cnt].specific_config_item =
      SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME;
    config_pair.pair[config_pair_cnt].type =
      SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
    config_pair.pair[config_pair_cnt].size =
      sizeof(config_pair.pair[config_pair_cnt].value[0].uint64_value);
    config_pair.pair[config_pair_cnt].value[0].uint64_value =
      0;
    config_pair_cnt++;
  }

  if(config_pair_cnt != 0)
  {
    config_pair.size = config_pair_cnt;
    SKF_BullGateway_tx._messages.specific_config_upload.header.
    message_seq_no = message_seq_no++;  // start from  NO.1
    pb_encode(&ostream, SKFChina_App_AppMessage_fields,
              &SKF_BullGateway_tx);
    // send to peer
    publishmsg.payload = ostream_data_buffer;
    publishmsg.payloadlen = ostream.bytes_written;
    // publishmsg.qos = mqtt_qos_one;
    publishmsg.qos = mqtt_qos_two;
    publishmsg.retained = 0;
    LOG_DEBUG(OUTPOINT, "msg: ");
    for(int i = 0; i < publishmsg.payloadlen; i++)
    {
      printf("%02x ", ((char *)publishmsg.payload)[i]);
    }
    printf("\r\n");
    for(uint8_t i = 0; i < MAX_REPUB_NUM; i++)
    {
      if(MQTTCLIENT_SUCCESS == mqtt_send(&publishmsg, _send_topic))
      {
        datupload_flag = true;
        break;
      }
    }
    memset(ostream_data_buffer, 0, CONFIG_DATA_BUFFER_LEN);
    ostream = pb_ostream_from_buffer(ostream_data_buffer,
                                     ostream_data_buffer_size);
    config_pair_cnt = 0;
  }

  // TODO: So far, always return true
  return ret;
}
static bool
gateway_config_froto_generate(
  gw_pro_data_t *data,
  uint32_t dataNO,
  SKFChina_App_AppMessage SKF_BullGateway_tx,
  uint32_t childNum,
  struct l_queue *childrenList,
  char *send_devid,
  char *send_topic) {
  uint8_t ostream_data_buffer[CONFIG_DATA_BUFFER_LEN] = { 0 };
  uint32_t ostream_data_buffer_size = CONFIG_DATA_BUFFER_LEN;

#ifndef CONFIG_SKIP_PACKAGE_HEADER
  ostream_data_buffer[0] = 0xaa;
  ostream_data_buffer[1] = 0x02;
  ostream_data_buffer[0] = 0x03;

  pb_ostream_t ostream = pb_ostream_from_buffer(&ostream_data_buffer[3],
                                                ostream_data_buffer_size -
                                                3);
#else
  pb_ostream_t ostream =
    pb_ostream_from_buffer(ostream_data_buffer, ostream_data_buffer_size);
#endif
  protobuf_encode_config_pairs_t config_pair;
  uint32_t config_pair_cnt = 0;
  bool ret = true;
  char _send_topic[200];
  COINFIG_MQTT_PUBLISH_CONFIG_TOPIC(_send_topic, GATAWAY_NAME,
                                    send_devid);

  //  header
  SKF_BullGateway_tx._messages.specific_config_upload.header.version = 1;
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  which_peer_addr =
    SKFChina_Froto_FrotoHeader_cloud_url_tag;
  SKF_BullGateway_tx._messages.specific_config_upload.header.is_up = true;
  SKF_BullGateway_tx._messages.specific_config_upload.header.time_to_live
    = 1;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  primitive_type =
    SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.message_type
    =
      SKFChina_Froto_FrotoMsgType_NORMAL_MESSAGE;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.total_block =
    1;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header
  .has_ack_window_size_message = false;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  long_packet_id =
    1;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  has_current_block =
    false;  // default
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  has_is_big_endian =
    false;  // little endian (default)

  id_arg.size = strlen(send_devid);
  id_arg.buffer = send_devid;
  protobuf_encode_bytes(
    &(SKF_BullGateway_tx._messages.specific_config_upload.header.sensor_id),
    &id_arg);
  SKF_BullGateway_tx._messages.specific_config_upload.header.
  which_peer_addr =
    SKFChina_Froto_FrotoHeader_cloud_tag;
  SKF_BullGateway_tx._messages.specific_config_upload.header.peer_addr.
  cloud =
    true;
  SKF_BullGateway_tx._messages.specific_config_upload.has_header = true;

  SKF_BullGateway_tx._messages.specific_config_upload.appVer = 1;
  id2_arg.size = strlen(send_devid);
  id2_arg.buffer = send_devid;
  protobuf_encode_bytes(
    &(SKF_BullGateway_tx._messages.specific_config_upload.sensor_id),
    &id2_arg);
  SKF_BullGateway_tx._messages.specific_config_upload.product =
    SKFChina_Common_ProductType_BULLET_GATEWAY;
  SKF_BullGateway_tx._messages.specific_config_upload.has_is_big_endian =
    false;

  SKF_BullGateway_tx.appVer = 1;
  SKF_BullGateway_tx.which__messages =
    SKFChina_App_AppMessage_specific_config_upload_tag;

  SKF_BullGateway_tx._messages.specific_config_upload.config_pair.arg =
    (void *)&config_pair;
  SKF_BullGateway_tx._messages.specific_config_upload.config_pair.funcs.
  encode = protobuf_encode_configpair_callback;

  // Children list
  if(true !=
     isDisabled(
       SKFChina_Common_SpecificConfigItem_CHILDREN,
       disabledConfig.GW,
       disabledConfig.gwCnt))
  {
    if(childNum > 0)
    {
      config_pair.pair[config_pair_cnt].memory_id = 166;
      config_pair.pair[config_pair_cnt].specific_config_item =
        SKFChina_Common_SpecificConfigItem_CHILDREN;
      config_pair.pair[config_pair_cnt].type =
        SKFChina_ConfigurationAndCommand_ConfigPair_children_list_tag;
      config_pair.pair[config_pair_cnt].size = childNum;
      config_pair.pair[config_pair_cnt].value[0].char_ptr = childrenList;
      config_pair_cnt++;
    }
    if(config_pair_cnt > 0)
    {
      config_pair.size = config_pair_cnt;
      SKF_BullGateway_tx._messages.specific_config_upload.header.
      message_seq_no = message_seq_no++;  // start from  NO.1
      pb_encode(&ostream, SKFChina_App_AppMessage_fields,
                &SKF_BullGateway_tx);
      // send to peer
      publishmsg.payload = ostream_data_buffer;
      publishmsg.payloadlen = ostream.bytes_written;
      // publishmsg.qos = mqtt_qos_one;
      publishmsg.qos = mqtt_qos_two;
      publishmsg.retained = 0;
      LOG_DEBUG(OUTPOINT, "msg: ");
      for(int i = 0; i < publishmsg.payloadlen; i++)
      {
        printf("%02x ", ((char *)publishmsg.payload)[i]);
      }
      printf("\r\n");
      for(uint8_t i = 0; i < MAX_REPUB_NUM; i++)
      {
        if(MQTTCLIENT_SUCCESS == mqtt_send(&publishmsg, _send_topic))
        {
          datupload_flag = true;
          break;
        }
      }
      memset(ostream_data_buffer, 0, CONFIG_DATA_BUFFER_LEN);
      ostream = pb_ostream_from_buffer(ostream_data_buffer,
                                       ostream_data_buffer_size);
      config_pair_cnt = 0;
    }
  }

  for(uint32_t i = 0; i < dataNO; i++)
  {
    LOG_DEBUG(OUTPOINT, "data[i].field = %d\r\n", data[i].field);
    switch(data[i].field)
    {
      case 8:
      {
        // "INT DEFAULT 0",
        // "lastEditTime",
        config_pair.pair[config_pair_cnt].memory_id = 205;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag;
        config_pair.pair[config_pair_cnt].size =
          sizeof(config_pair.pair[config_pair_cnt].value[0].uint64_value);
        config_pair.pair[config_pair_cnt].value[0].uint64_value =
          data[i].data.data_uint64;
        config_pair_cnt++;
        break;
      }
      case 18:
      {
        // "INT DEFAULT 0",
        // "timeSetting",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_NTP_SERVER_SETTING,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 151;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_NTP_SERVER_SETTING;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        config_pair.pair[config_pair_cnt].size =
          sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
        if(data[i].data.data_uint8)
        {
          config_pair.pair[config_pair_cnt].value[0].bool_value = true;
        }
        else
        {
          config_pair.pair[config_pair_cnt].value[0].bool_value = false;
        }
        config_pair_cnt++;
        break;
      }
      case 19:
      {
        // "TEXT DEFAULT ' '",
        // "timeNTPUrl",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_NTP_SERVER_URL,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 152;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_NTP_SERVER_URL;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_tag;
        config_pair.pair[config_pair_cnt].size = strlen(
          data[i].data.data_char);
        config_pair.pair[config_pair_cnt].value[0].char_ptr =
          &(data[i].data.data_char);
        config_pair_cnt++;
        break;
      }
      case 20:
      {
        // "INT DEFAULT 0",
        // "MQTTSecurityType",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_SECURITY_TYPE,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 153;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_SECURITY_TYPE;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_mqtt_sec_type_tag;
        config_pair.pair[config_pair_cnt].size =
          sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value) *
          2;
        config_pair.pair[config_pair_cnt].value[0].uint32_value =
          data[i].data.data_uint32;
        // TODO: Do not support parameter so far
        config_pair.pair[config_pair_cnt].value[1].uint32_value = 0;
        config_pair_cnt++;
        break;
      }
      case 21:
      {
        // "TEXT DEFAULT ' '",
        // "MQTTHost",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_URL,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 154;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_URL;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_tag;
        config_pair.pair[config_pair_cnt].size = strlen(
          data[i].data.data_char);
        config_pair.pair[config_pair_cnt].value[0].char_ptr =
          &(data[i].data.data_char);
        config_pair_cnt++;
        break;
      }
      case 22:
      {
        // "INT DEFAULT 0",
        // "MQTTPort",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_PORT,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 155;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_PORT;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        config_pair.pair[config_pair_cnt].size =
          sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
        config_pair.pair[config_pair_cnt].value[0].uint32_value =
          data[i].data.data_uint32;
        config_pair_cnt++;
        break;
      }
      case 23:
      {
        // "BOOLEAN DEFAULT false",
        // "DHCPEnable",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_DHCP_ENABLE,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 156;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_DHCP_ENABLE;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag;
        config_pair.pair[config_pair_cnt].size =
          sizeof(config_pair.pair[config_pair_cnt].value[0].bool_value);
        if(data[i].data.data_uint8)
        {
          config_pair.pair[config_pair_cnt].value[0].bool_value = true;
        }
        else
        {
          config_pair.pair[config_pair_cnt].value[0].bool_value = false;
        }
        config_pair_cnt++;
        break;
      }
      case 24:
      {
        // "TEXT DEFAULT ' '",
        // "staticIPAddr",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_STATIC_IP_ADDR,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 157;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_STATIC_IP_ADDR;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag;
        config_pair.pair[config_pair_cnt].size = strlen(
          data[i].data.data_char);
        config_pair.pair[config_pair_cnt].value[0].char_ptr =
          &(data[i].data.data_char);
        config_pair_cnt++;
        break;
      }
      case 25:
      {
        // "TEXT DEFAULT ' '",
        // "netMask",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_NETMASK,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 158;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_NETMASK;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag;
        config_pair.pair[config_pair_cnt].size = strlen(
          data[i].data.data_char);
        config_pair.pair[config_pair_cnt].value[0].char_ptr =
          &(data[i].data.data_char);
        config_pair_cnt++;
        break;
      }
      case 26:
      {
        // "TEXT DEFAULT ' '",
        // "DNSServer",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_DNSSERVER_ADDR,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 159;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_DNSSERVER_ADDR;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag;
        config_pair.pair[config_pair_cnt].size = strlen(
          data[i].data.data_char);
        config_pair.pair[config_pair_cnt].value[0].char_ptr =
          &(data[i].data.data_char);
        config_pair_cnt++;
        break;
      }
      case 27:
      {
        // "TEXT DEFAULT ' '",
        // "gatewayAddr",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_GATEWAY_IP_ADDR,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 160;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_GATEWAY_IP_ADDR;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag;
        config_pair.pair[config_pair_cnt].size = strlen(
          data[i].data.data_char);
        config_pair.pair[config_pair_cnt].value[0].char_ptr =
          &(data[i].data.data_char);
        config_pair_cnt++;
        break;
      }
#if (MODBUS_FEATURE_ENABLE == 1)
      case 28:
      {
        // "INT DEFAULT 115200",
        // "baudrateSlave",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_BAUDRATE_SLAVE,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 161;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_BAUDRATE_SLAVE;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        config_pair.pair[config_pair_cnt].size =
          sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
        config_pair.pair[config_pair_cnt].value[0].uint32_value =
          data[i].data.data_uint32;
        config_pair_cnt++;
        break;
      }
      case 29:
      {
        // "INT DEFAULT 0",
        // "slaveAddrSlave",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_ADDR_SLAVE,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 162;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_ADDR_SLAVE;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        config_pair.pair[config_pair_cnt].size =
          sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
        config_pair.pair[config_pair_cnt].value[0].uint32_value =
          data[i].data.data_uint32;
        config_pair_cnt++;
        break;
      }
      case 30:
      {
        // "INT DEFAULT 0",
        // "paritySlave",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_PARITY_SLAVE,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 163;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_PARITY_SLAVE;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        config_pair.pair[config_pair_cnt].size =
          sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
        config_pair.pair[config_pair_cnt].value[0].uint32_value =
          data[i].data.data_uint32;
        config_pair_cnt++;
        break;
      }
      case 31:
      {
        // "INT DEFAULT 0",
        // "stopSlave",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_STOP_SLAVE,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 164;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_STOP_SLAVE;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        config_pair.pair[config_pair_cnt].size =
          sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
        config_pair.pair[config_pair_cnt].value[0].uint32_value =
          data[i].data.data_uint32;
        config_pair_cnt++;
        break;
      }
      case 32:
      {
        // "INT DEFAULT 0",
        // "registerMode",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_REGISTER_MODE,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 165;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_REGISTER_MODE;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag;
        config_pair.pair[config_pair_cnt].size =
          sizeof(config_pair.pair[config_pair_cnt].value[0].uint32_value);
        config_pair.pair[config_pair_cnt].value[0].uint32_value =
          data[i].data.data_uint32;
        config_pair_cnt++;
        break;
      }
      case 33:
      {
        // "TEXT DEFAULT ' '",
        // "description",
        if(true ==
           isDisabled(
             SKFChina_Common_SpecificConfigItem_DESCRIPTION,
             disabledConfig.GW,
             disabledConfig.gwCnt))
        {
          // It is disabled ...
          break;
        }
        config_pair.pair[config_pair_cnt].memory_id = 167;
        config_pair.pair[config_pair_cnt].specific_config_item =
          SKFChina_Common_SpecificConfigItem_DESCRIPTION;
        config_pair.pair[config_pair_cnt].type =
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_tag;
        config_pair.pair[config_pair_cnt].size = strlen(
          data[i].data.data_char);
        config_pair.pair[config_pair_cnt].value[0].char_ptr =
          &(data[i].data.data_char);
        config_pair_cnt++;
        break;
      }
#endif /* MODBUS_FEATURE_ENABLE == 1 */
      default:
      {
        // Do nothing, just skip
        break;
      }
    }
    if(config_pair_cnt == (CONFIG_UP_VIA_MQTT_NUM - 1))
    {
      config_pair.size = config_pair_cnt;
      SKF_BullGateway_tx._messages.specific_config_upload.header.
      message_seq_no = message_seq_no++;  // start from  NO.1
      pb_encode(&ostream, SKFChina_App_AppMessage_fields,
                &SKF_BullGateway_tx);
      // send to peer
      publishmsg.payload = ostream_data_buffer;
      publishmsg.payloadlen = ostream.bytes_written;
      // publishmsg.qos = mqtt_qos_one;
      publishmsg.qos = mqtt_qos_two;
      publishmsg.retained = 0;
      LOG_DEBUG(OUTPOINT, "msg: ");
      for(int i = 0; i < publishmsg.payloadlen; i++)
      {
        printf("%02x ", ((char *)publishmsg.payload)[i]);
      }
      printf("\r\n");
      for(uint8_t i = 0; i < MAX_REPUB_NUM; i++)
      {
        if(MQTTCLIENT_SUCCESS == mqtt_send(&publishmsg, _send_topic))
        {
          datupload_flag = true;
          break;
        }
      }
      memset(ostream_data_buffer, 0, CONFIG_DATA_BUFFER_LEN);
      ostream = pb_ostream_from_buffer(ostream_data_buffer,
                                       ostream_data_buffer_size);
      config_pair_cnt = 0;
    }
  }

  if(config_pair_cnt != 0)
  {
    config_pair.size = config_pair_cnt;
    SKF_BullGateway_tx._messages.specific_config_upload.header.
    message_seq_no = message_seq_no++;  // start from  NO.1
    pb_encode(&ostream, SKFChina_App_AppMessage_fields,
              &SKF_BullGateway_tx);
    // send to peer
    publishmsg.payload = ostream_data_buffer;
    publishmsg.payloadlen = ostream.bytes_written;
    // publishmsg.qos = mqtt_qos_one;
    publishmsg.qos = mqtt_qos_two;
    publishmsg.retained = 0;
    LOG_DEBUG(OUTPOINT, "msg: ");
    for(int i = 0; i < publishmsg.payloadlen; i++)
    {
      printf("%02x ", ((char *)publishmsg.payload)[i]);
    }
    printf("\r\n");
    for(uint8_t i = 0; i < MAX_REPUB_NUM; i++)
    {
      if(MQTTCLIENT_SUCCESS == mqtt_send(&publishmsg, _send_topic))
      {
        datupload_flag = true;
        break;
      }
    }
    memset(ostream_data_buffer, 0, CONFIG_DATA_BUFFER_LEN);
    ostream = pb_ostream_from_buffer(ostream_data_buffer,
                                     ostream_data_buffer_size);
    config_pair_cnt = 0;
  }

  // TODO: So far, always return true
  return ret;
}
static void
children_generate(
  gw_pro_data_t *data,
  uint32_t dataNO,
  protobuf_encode_config_pairs_child_t *child)
{
  for(uint32_t i = 0; i < dataNO; i++)
  {
    switch(data[i].field)
    {
      case 1:
      {
        // name ID
        child->nameId = data[i].data.data_uint32;
        break;
      }
      case 2:
      {
        // name
        strncpy(child->name, data[i].data.data_char, sizeof(child->name));
        break;
      }
      case 3:
      {
        // BLE name
        strncpy(child->bleName, data[i].data.data_char,
                sizeof(child->bleName));
        break;
      }
      case 4:
      {
        // MAC addr
        strncpy(child->mac, data[i].data.data_char,
                sizeof(child->mac));
        break;
      }
      case 5:
      {
        // Manufacturer
        strncpy(child->manufacturer, data[i].data.data_char,
                sizeof(child->manufacturer));
        break;
      }
      case 6:
      {
        // Sensor Type
        strncpy(child->type, data[i].data.data_char, sizeof(child->type));
        break;
      }
      case 7:
      {
        // Description
#if (MODBUS_FEATURE_ENABLE == 1)
        strncpy(child->description, data[i].data.data_char, sizeof(child->description));
#endif /* MODBUS_FEATURE_ENABLE == 1 */
      }
      default:
        // Do nothing
        break;
    }
  }
}
static bool
isDisabled(
  uint32_t item,
  uint16_t *disabledArray,
  uint32_t disabledNum)
{
  for(uint32_t cnt = 0; cnt < disabledNum; cnt++)
  {
    if(item == disabledArray[cnt])
    {
      return true;
    }
  }
  return false;
}