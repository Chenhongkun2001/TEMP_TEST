
#include "common.h"
#include <downmsg.h>
#include <global.h>
#include <http.h>
#include <mqtt.h>
#include <stdio.h>
#include <string.h>
#include "ipcmbs.h"
#include "jsonparse.h"
#include <ell/ell.h>
DBG_LOCAL_LOG_INFO;

static pb_istream_t substream;
static const pb_msgdesc_t *type;
static protobuf_encode_general_bytes_t arg;
static protobuf_encode_general_bytes_t arg1;
static protobuf_encode_general_bytes_t check_value;
static protobuf_encode_general_bytes_t ota_arg;
static protobuf_encode_general_bytes_t mqtt_topic_arg;
static uint8_t measure_type_index = 0;
static uint8_t temp_url[OTA_URL_ADDR_LEN] = {0};
uint8_t downloadtype = 0;
// extern msgbuf_Typedef *msgbuf_txrxp;

extern struct disabledGroup disabledData;
extern struct disabledGroup disabledConfig;
static uint32_t ipc_data_idx;
static gw_pro_data_t ipc_data[GW_PRO_MAX_DATA_FIELD_PER_OPERATION] =
{ 0 };
static uint32_t ipc_children_data_idx;
static gw_pro_data_t ipc_children_data[GW_PRO_MAX_DATA_FIELD_PER_OPERATION]
  = { 0 };
static bool devCanBeDeletedFg = true;
static gw_pro_sqlite_cond_element_t filterData[
  GW_PRO_MAX_DATA_FIELD_PER_OPERATION];
static gw_pro_command_sqlite_filter_response_t filter_rx;
static struct msgbuf msgbufp = { 0 };
static gw_pro_message_header_t *pdata =
  (gw_pro_message_header_t *)msgbufp.mtext;

static bool isDisabled(
  uint32_t item,
  uint16_t *disabledArray,
  uint32_t disabledNum);
static void generate_database_ipc_msg_for_sensor(
  SKFChina_ConfigurationAndCommand_ConfigPair *cfg_pair_pt,
  gw_pro_data_t *ipc_data,
  uint32_t ipc_data_size,
  protobuf_encode_timearray_t *timearray);
static void generate_database_ipc_msg_for_gateway(
  SKFChina_ConfigurationAndCommand_ConfigPair *cfg_pair_pt,
  gw_pro_data_t *ipc_data,
  uint32_t ipc_data_size,
  gw_pro_data_t *ipc_children_data,
  uint32_t ipc_children_data_size,
  protobuf_encode_timearray_t *timearray,
  protobuf_encode_config_pairs_child_t *child,
  uint8_t *buf,
  uint32_t bufSize,
  bool isIpv4);
static bool protobuf_decode_configpair_timearray_callback(
  pb_istream_t *stream,
  const pb_field_t *field,
  void **arg);
static bool protobuf_decode_configpair_childrenlist_callback(
  pb_istream_t *stream,
  const pb_field_t *field,
  void **arg);
static bool protobuf_decode_configpair_callback(
  pb_istream_t *stream,
  const pb_field_t *field,
  void **arg);

static bool protobuf_decode_measure_type_callback(pb_istream_t *stream,
                                                  const pb_field_t *field,
                                                  void **arg) {

  //  pb_read(stream, &measure_type[measure_type_index++],
  //  sizeof(measure_type[0]))
  arg1.buffer = &measure_type[measure_type_index++];
  arg1.size = sizeof(measure_type[0]);
  protobuf_decode_bytes(
      &(SKF_BullGateway._messages.data_selection.measure_type), &arg1);
  return true;
}
static void
generate_database_ipc_msg_for_gateway(
  SKFChina_ConfigurationAndCommand_ConfigPair *cfg_pair_pt,
  gw_pro_data_t *ipc_data,
  uint32_t ipc_data_size,
  gw_pro_data_t *ipc_children_data,
  uint32_t ipc_children_data_size,
  protobuf_encode_timearray_t *timearray,
  protobuf_encode_config_pairs_child_t *child,
  uint8_t *buf,
  uint32_t bufSize,
  bool isIpv4)
{
  switch(cfg_pair_pt->config_item.specific_config_item)
  {
    case SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      if(timearray->cnt > 0)
      {
        ipc_data[ipc_data_idx].field = 8;
        ipc_data[ipc_data_idx].data.data_uint64 = timearray->time[0];
        // A magic operation: increase the last edit time, then a natural uplink would happen ...
        // Notice: the magic operation only required for the gateway
        ipc_data[ipc_data_idx].data.data_uint64++;

        ipc_data_idx++;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_NTP_SERVER_SETTING:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_NTP_SERVER_SETTING,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 18;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_NTP_SERVER_URL:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_NTP_SERVER_URL,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 19;
      memset(ipc_data[ipc_data_idx].data.data_char, 0,
             sizeof(ipc_data[ipc_data_idx].data.data_char));
      snprintf(ipc_data[ipc_data_idx].data.data_char,
               sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
               "%s", buf);
      ipc_data_idx++;
      break;
    }
    case
      SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_SECURITY_TYPE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_mqtt_sec_type_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_SECURITY_TYPE,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 20;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.mqtt_sec_type.secType;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_URL:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_URL,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 21;
      memset(ipc_data[ipc_data_idx].data.data_char, 0,
             sizeof(ipc_data[ipc_data_idx].data.data_char));
      snprintf(ipc_data[ipc_data_idx].data.data_char,
               sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
               "%s", buf);
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_PORT:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_PORT,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 22;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DHCP_ENABLE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_DHCP_ENABLE,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 23;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_STATIC_IP_ADDR:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_STATIC_IP_ADDR,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      if(isIpv4 == false)
      {
        // So far, only support IPv4 ...
        break;
      }
      ipc_data[ipc_data_idx].field = 24;
      memset(ipc_data[ipc_data_idx].data.data_char, 0,
             sizeof(ipc_data[ipc_data_idx].data.data_char));
      snprintf(ipc_data[ipc_data_idx].data.data_char,
               sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
               "%s", buf);
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_NETMASK:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_NETMASK,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      if(isIpv4 == false)
      {
        // So far, only support IPv4 ...
        break;
      }
      ipc_data[ipc_data_idx].field = 25;
      memset(ipc_data[ipc_data_idx].data.data_char, 0,
             sizeof(ipc_data[ipc_data_idx].data.data_char));
      snprintf(ipc_data[ipc_data_idx].data.data_char,
               sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
               "%s", buf);
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DNSSERVER_ADDR:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_DNSSERVER_ADDR,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      if(isIpv4 == false)
      {
        // So far, only support IPv4 ...
        break;
      }
      ipc_data[ipc_data_idx].field = 26;
      memset(ipc_data[ipc_data_idx].data.data_char, 0,
             sizeof(ipc_data[ipc_data_idx].data.data_char));
      snprintf(ipc_data[ipc_data_idx].data.data_char,
               sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
               "%s", buf);
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_GATEWAY_IP_ADDR:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_GATEWAY_IP_ADDR,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      if(isIpv4 == false)
      {
        // So far, only support IPv4 ...
        break;
      }
      ipc_data[ipc_data_idx].field = 27;
      memset(ipc_data[ipc_data_idx].data.data_char, 0,
             sizeof(ipc_data[ipc_data_idx].data.data_char));
      snprintf(ipc_data[ipc_data_idx].data.data_char,
               sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
               "%s", buf);
      ipc_data_idx++;
      break;
    }
#if (MODBUS_FEATURE_ENABLE == 1)
    case SKFChina_Common_SpecificConfigItem_BAUDRATE_SLAVE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_BAUDRATE_SLAVE,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 28;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ADDR_SLAVE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ADDR_SLAVE,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 29;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PARITY_SLAVE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_PARITY_SLAVE,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 30;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_STOP_SLAVE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_STOP_SLAVE,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 31;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_REGISTER_MODE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_REGISTER_MODE,
           disabledConfig.GW,
           disabledConfig.gwCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 32;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
#endif /* MODBUS_FEATURE_ENABLE == 1 */
    default:
    {
      // Do nothing, just skip
      LOG_WARN(OUTPOINT, "Unknown specific config item\r\n");
      break;
    }
  }

  if(ipc_data_idx >= (ipc_data_size - 4))  // 4: The number of reserved
                                           // fields
  {
    filterData[0].condition = gw_pro_sqlite_cond_str_equal;
    filterData[0].field = 4;

    snprintf(filterData[0].condParam.string_value.s,
             GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", sensor_id);
    LOG_INFO(OUTPOINT, "Current decoded MAC: %s\r\n",
             filterData[0].condParam.string_value.s);

    if(ipc_sql_filter_item(GATEWAY_CONFIGURE_TABLE, 1, filterData,
                           &msgbufp) ||
       pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
       pdata->command_or_response.responseMsg.response ==
       gw_pro_response_failed)
    {
      LOG_WARN(OUTPOINT, "Failed to get data from Dev table\r\n");
      return;
    }
    memset(&filter_rx, 0, sizeof(filter_rx));
    memcpy(&filter_rx,
           &pdata->command_or_response.responseMsg.responseInfo
           .sqlite_filter_item_reponse,
           sizeof(gw_pro_command_sqlite_filter_response_t));

    // Reserved fields
    // nameID
    ipc_data[ipc_data_idx].field = 1;
    ipc_data[ipc_data_idx].data.data_uint32 = filter_rx.filteredItemIdx[0];
    ipc_data_idx++;
    // macAddr
    ipc_data[ipc_data_idx].field = 3;
    snprintf(ipc_data[ipc_data_idx].data.data_char,
             sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
             "%s", sensor_id);
    ipc_data_idx++;
    // type
    ipc_data[ipc_data_idx].field = 9;
    snprintf(ipc_data[ipc_data_idx].data.data_char,
             sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
             "GATEWAY");
    ipc_data_idx++;
    // manufacturer
    ipc_data[ipc_data_idx].field = 4;
    snprintf(ipc_data[ipc_data_idx].data.data_char,
             sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
             "SKF");
    ipc_data_idx++;

    LOG_INFO(OUTPOINT, "update %d into gateway table with %d\r\n",
             filter_rx.filteredItemIdx[0],
             ipc_data_idx);
    if(ipc_sql_update_item(GATEWAY_CONFIGURE_TABLE,
                           filter_rx.filteredItemIdx[0], ipc_data_idx,
                           ipc_data,
                           NULL))
    {
      LOG_WARN(OUTPOINT, "Failed to update %d into gateway table\r\n",
               filter_rx.filteredItemIdx[0]);
    }
    ipc_data_idx = 0;
    memset(ipc_data, 0, sizeof(gw_pro_data_t) * ipc_data_size);
  }
}
static void
generate_database_ipc_msg_for_sensor(
  SKFChina_ConfigurationAndCommand_ConfigPair *cfg_pair_pt,
  gw_pro_data_t *ipc_data,
  uint32_t ipc_data_size,
  protobuf_encode_timearray_t *timearray)
{
  switch(cfg_pair_pt->config_item.specific_config_item)
  {
    case SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      if(timearray->cnt > 0)
      {
        ipc_data[ipc_data_idx].field = 8;
        ipc_data[ipc_data_idx].data.data_uint64 = timearray->time[0];
        ipc_data_idx++;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_WORK_MODE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_WORK_MODE,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }

      ipc_data[ipc_data_idx].field = 12;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.work_mode_content.work_mode;
      ipc_data_idx++;

      break;
    }
    case
      SKFChina_Common_SpecificConfigItem_BATTERY_LOW_ALARM_THRESHOLD_PERCENTAGE
      :
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_BATTERY_LOW_ALARM_THRESHOLD_PERCENTAGE,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 13;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_ADV_DBM:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_ADV_DBM,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 14;
      ipc_data[ipc_data_idx].data.data_int32 =
        cfg_pair_pt->config_content.general_config_content_int32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_AUX_ADV_KBPS:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_AUX_ADV_KBPS,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 15;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_AUX_ADV_DBM:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_AUX_ADV_DBM,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 16;
      ipc_data[ipc_data_idx].data.data_int32 =
        cfg_pair_pt->config_content.general_config_content_int32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_QUICKPOLLING_PERIOD_S:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_QUICKPOLLING_PERIOD_S,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 17;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      if(timearray->cnt > 0)
      {
        ipc_data[ipc_data_idx].field = 18;
        ipc_data[ipc_data_idx].data.data_uint64 = timearray->time[0];
        ipc_data_idx++;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_REGULARSENSING_PERIOD_S:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_REGULARSENSING_PERIOD_S,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 19;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      if(timearray->cnt > 0)
      {
        ipc_data[ipc_data_idx].field = 20;
        ipc_data[ipc_data_idx].data.data_uint64 = timearray->time[0];
        ipc_data_idx++;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_COMMUNICATION_PERIOD_S:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_COMMUNICATION_PERIOD_S,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 21;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      if(timearray->cnt > 0)
      {
        ipc_data[ipc_data_idx].field = 22;
        ipc_data[ipc_data_idx].data.data_uint64 = timearray->time[0];
        ipc_data_idx++;
      }
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_FS_HZ:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_FS_HZ,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 23;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_N:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_N,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 24;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_AXIS_ACQ_EVAL:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_AXIS_ACQ_EVAL,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 25;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_RANGE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_RANGE,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 26;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_FS_HZ:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_FS_HZ,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 27;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_N:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_N,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 28;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_AXIS_ACQ_EVAL:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_AXIS_ACQ_EVAL,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 29;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_RATE_HZ:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_RATE_HZ,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 30;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_PTS:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_PTS,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 31;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_AXIS:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACC_SENSOR_AXIS,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 32;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_RANGE_G:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACC_SENSOR_RANGE_G,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 33;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_FS_HZ:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACQ_MAG_FS_HZ,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 34;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_N:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACQ_MAG_N,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 35;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACQ_MAG_AXIS_ACQ_EVAL:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACQ_MAG_AXIS_ACQ_EVAL,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 36;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FS_COEF:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FS_COEF,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 37;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_GEE_COEF:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_GEE_COEF,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 38;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_V_COEF:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_V_COEF,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 39;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_POSITION:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_MEAS_POSITION,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 40;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_LOAD:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_MEAS_LOAD,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 41;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_AXIS_VIB:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_MEAS_AXIS_VIB,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 42;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MEAS_AXIS_MAG:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_MEAS_AXIS_MAG,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 43;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VIB_START_FG:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_VIB_START_FG,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 44;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VIB_RMS_START_TL:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_VIB_RMS_START_TL,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 45;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_START_FG:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_MAG_START_FG,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 46;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_RMS_START_TL:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_MAG_RMS_START_TL,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 47;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_STABLE_FG:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_MAG_STABLE_FG,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 48;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MAG_RMS_VAR_TH:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_MAG_RMS_VAR_TH,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 49;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_RPM_VAR_RANGE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_RPM_VAR_RANGE,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 50;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_M:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_M,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 51;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_N:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_N,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 52;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_TEMP:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_TEMP,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 53;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_M:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_M,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 54;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_N:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_N,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 55;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_VIB:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_VIB,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 56;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_MAG:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_MAG,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 57;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_TEMP:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FUNC_ANOM_TEMP,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 58;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_OV:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FUNC_ANOM_OV,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 59;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MECH:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MECH,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 60;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_BRG:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FUNC_ANOM_BRG,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 61;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_LUB:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FUNC_ANOM_LUB,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 62;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MTR:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MTR,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 63;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_GEAR:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FUNC_ANOM_GEAR,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 64;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_FAN:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FUNC_ANOM_FAN,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 65;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_PUMP:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FUNC_ANOM_PUMP,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 66;
      if(cfg_pair_pt->config_content.general_config_content_bool)
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 1;
      }
      else
      {
        ipc_data[ipc_data_idx].data.data_uint8 = 0;
      }
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ASSET_LEVEL:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ASSET_LEVEL,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 67;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FLEX_TYPE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FLEX_TYPE,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 68;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BORE_DIAMETER_MM:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_BORE_DIAMETER_MM,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 69;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_RUN_SPEED_RPM:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_RUN_SPEED_RPM,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 70;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFO:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFO,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 71;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFI:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFI,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 72;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_BSF:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_BRG_INFO_BSF,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 73;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_BRG_INFO_FTF:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_BRG_INFO_FTF,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 74;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MTR_INFO_FL:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_MTR_INFO_FL,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 75;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_MTR_INFO_BAR:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_MTR_INFO_BAR,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 76;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_GEAR_INFO_TOOTH:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_GEAR_INFO_TOOTH,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 77;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_FAN_INFO_BLADE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_FAN_INFO_BLADE,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 78;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_PUMP_INFO_VANE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_PUMP_INFO_VANE,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 79;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_TEMP_OV_ALERT_CDEGREE:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_TEMP_OV_ALERT_CDEGREE,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 80;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case
      SKFChina_Common_SpecificConfigItem_TEMPERATURE_ALARM_THRESHOLD_CDGREE
      :
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_TEMPERATURE_ALARM_THRESHOLD_CDGREE,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 81;
      ipc_data[ipc_data_idx].data.data_int32 =
        cfg_pair_pt->config_content.general_config_content_int32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_OV_ALERT:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACC_OV_ALERT,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 82;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_OV_ALARM:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACC_OV_ALARM,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 83;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_HAL_ALERT:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACC_HAL_ALERT,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 84;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ACC_HAL_ALARM:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ACC_HAL_ALARM,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 85;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_OV_ALERT:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_VEL_OV_ALERT,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 86;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_OV_ALARM:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_VEL_OV_ALARM,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 87;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_HAL_ALERT:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_VEL_HAL_ALERT,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 88;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_VEL_HAL_ALARM:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_VEL_HAL_ALARM,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 89;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_OV_ALERT:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ENV_OV_ALERT,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 90;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_OV_ALARM:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ENV_OV_ALARM,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 91;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_HAL_ALERT:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ENV_HAL_ALERT,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 92;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_ENV_HAL_ALARM:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_ENV_HAL_ALARM,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 93;
      ipc_data[ipc_data_idx].data.data_float =
        cfg_pair_pt->config_content.general_config_content_float;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_PERIOD_S:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_PERIOD_S,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      ipc_data[ipc_data_idx].field = 94;
      ipc_data[ipc_data_idx].data.data_uint32 =
        cfg_pair_pt->config_content.general_config_content_uint32;
      ipc_data_idx++;
      break;
    }
    case SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S:
    {
      if(cfg_pair_pt->which_config_content !=
         SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
      {
        // The type is invalid
        break;
      }
      if(true ==
         isDisabled(
           SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S,
           disabledConfig.PredictSensor,
           disabledConfig.predictSensorCnt))
      {
        // It is disabled ...
        break;
      }
      if(timearray->cnt > 0)
      {
        ipc_data[ipc_data_idx].field = 95;
        ipc_data[ipc_data_idx].data.data_uint64 = timearray->time[0];
        ipc_data_idx++;
      }
      break;
    }
    default:
    {
      // Do nothing, just skip
      LOG_WARN(OUTPOINT, "Unknown specific config item\r\n");
      break;
    }
  }

  if(ipc_data_idx >= ipc_data_size)
  {
    filterData[0].condition = gw_pro_sqlite_cond_str_equal;
    filterData[0].field = 4;

    snprintf(filterData[0].condParam.string_value.s,
             GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", sensor_id);
    LOG_INFO(OUTPOINT, "Current decoded MAC: %s\r\n",
             filterData[0].condParam.string_value.s);

    if(ipc_sql_filter_item(DEVICE_LIST_TABLE, 1, filterData, &msgbufp) ||
       pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
       pdata->command_or_response.responseMsg.response ==
       gw_pro_response_failed)
    {
      LOG_WARN(OUTPOINT, "Failed to get data from Dev table\r\n");
      return;
    }
    memset(&filter_rx, 0, sizeof(filter_rx));
    memcpy(&filter_rx,
           &pdata->command_or_response.responseMsg.responseInfo
           .sqlite_filter_item_reponse,
           sizeof(gw_pro_command_sqlite_filter_response_t));

    LOG_INFO(OUTPOINT, "update %d into sensor table with %d\r\n",
             filter_rx.filteredItemIdx[0],
             ipc_data_idx);
    if(ipc_sql_update_item(SENSOR_CONFIGURE_TABLE,
                           filter_rx.filteredItemIdx[0], ipc_data_idx,
                           ipc_data,
                           NULL))
    {
      LOG_WARN(OUTPOINT, "Failed to update %d into sensor table\r\n",
               filter_rx.filteredItemIdx[0]);
    }
    ipc_data_idx = 0;
    memset(ipc_data, 0, sizeof(gw_pro_data_t) * ipc_data_size);
  }
}
static bool
protobuf_decode_configpair_timearray_callback(
  pb_istream_t *stream,
  const pb_field_t *field,
  void **arg)
{
  bool rt;
  pb_istream_t _stream = pb_istream_from_buffer(stream->state,
                                                stream->bytes_left);
  pb_istream_t _stream_before_decode;  // For copy of stream
  SKFChina_ConfigurationAndCommand_TimeArrayElement t;
  protobuf_encode_timearray_t *timearray_pt =
    (protobuf_encode_timearray_t *)(*arg);

  // Make a copy of stream
  memcpy(&_stream_before_decode, &_stream, sizeof(pb_istream_t));
#if 1  // Just for debug
  {
    uint32_t _size = _stream.bytes_left;
    uint8_t stream_buf[50];
    pb_read(&_stream, stream_buf, _size);
    LOG_DEBUG(OUTPOINT, "The string: ");
    for(uint32_t cnt = 0; cnt < _size; cnt++)
    {
      printf("%02x ", stream_buf[cnt]);
    }
    printf("\r\n");
  }
  memcpy(&_stream, &_stream_before_decode, sizeof(pb_istream_t));
#endif

  if(arg != NULL)
  {
    rt = pb_decode(&_stream,
                   SKFChina_ConfigurationAndCommand_TimeArrayElement_fields,
                   &t);
    if(rt == false)
    {
      LOG_WARN(OUTPOINT, "Failed to decode field\r\n");
      return false;
    }

    if(timearray_pt->cnt <= sizeof(timearray_pt->time) - 1)
    {
      timearray_pt->time[timearray_pt->cnt] = t.time;
      LOG_DEBUG(OUTPOINT, "time (%d) in the array %lld\r\n",
                timearray_pt->cnt,
                timearray_pt->time[timearray_pt->cnt]);
      timearray_pt->cnt++;
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
protobuf_decode_configpair_childrenlist_callback(
  pb_istream_t *stream,
  const pb_field_t *field,
  void **arg)
{
  bool rt;
  pb_istream_t _stream = pb_istream_from_buffer(stream->state,
                                                stream->bytes_left);
  pb_istream_t _stream_before_decode;  // For copy of stream
  SKFChina_ConfigurationAndCommand_ChildNode c;
  protobuf_encode_config_pairs_child_t *child_pt =
    (protobuf_encode_config_pairs_child_t *)(*arg);
  protobuf_encode_general_bytes_t child_type, child_manu, child_name,
                                  child_sensorname, child_macaddr;

  // Make a copy of stream
  memcpy(&_stream_before_decode, &_stream, sizeof(pb_istream_t));
#if 1  // Just for debug
  {
    uint32_t _size = _stream.bytes_left;
    uint8_t stream_buf[50];
    pb_read(&_stream, stream_buf, _size);
    LOG_DEBUG(OUTPOINT, "The string: ");
    for(uint32_t cnt = 0; cnt < _size; cnt++)
    {
      printf("%02x ", stream_buf[cnt]);
    }
    printf("\r\n");
  }
  memcpy(&_stream, &_stream_before_decode, sizeof(pb_istream_t));
#endif

  if(arg != NULL)
  {
    child_type.buffer = child_pt->type;
    child_type.size = sizeof(child_pt->type);
    protobuf_decode_bytes(&(c.type),
                          &child_type);
    child_manu.buffer = child_pt->manufacturer;
    child_manu.size = sizeof(child_pt->manufacturer);
    protobuf_decode_bytes(&(c.manufacturer),
                          &child_manu);
    child_name.buffer = child_pt->name;
    child_name.size = sizeof(child_pt->name);
    protobuf_decode_bytes(&(c.name),
                          &child_name);
    child_sensorname.buffer = child_pt->bleName;
    child_sensorname.size = sizeof(child_pt->bleName);
    protobuf_decode_bytes(&(c.BLESensorName),
                          &child_sensorname);
    child_macaddr.buffer = child_pt->mac;
    child_macaddr.size = sizeof(child_pt->mac);
    protobuf_decode_bytes(&(c.macAddr),
                          &child_macaddr);
    rt = pb_decode(&_stream,
                   SKFChina_ConfigurationAndCommand_ChildNode_fields,
                   &c);
    if(rt == false)
    {
      LOG_WARN(OUTPOINT, "Failed to decode field\r\n");
      return false;
    }
    child_pt->total = c.totalChildren;
    child_pt->order = c.order;
    child_pt->nameId = c.nameId;

    // Write to the database
    if(true ==
       isDisabled(
         SKFChina_Common_SpecificConfigItem_CHILDREN,
         disabledConfig.GW,
         disabledConfig.gwCnt))
    {
      // It is disabled ...
      return true;
    }

    ipc_children_data_idx = 0;
    memset(ipc_children_data, 0, sizeof(ipc_children_data));
    if(strlen(child_pt->bleName) > 0)
    {
      ipc_children_data[ipc_children_data_idx].field = 3;
      snprintf(ipc_children_data[ipc_children_data_idx].data.data_char,
               sizeof(ipc_children_data[ipc_children_data_idx].data.
                      data_char) - 1,
               "%s", child_pt->bleName);
      LOG_DEBUG(OUTPOINT, "bleName: %s\r\n", child_pt->bleName);
      ipc_children_data_idx++;
    }
    if(strlen(child_pt->mac) > 0)
    {
      ipc_children_data[ipc_children_data_idx].field = 4;
      snprintf(ipc_children_data[ipc_children_data_idx].data.data_char,
               sizeof(ipc_children_data[ipc_children_data_idx].data.
                      data_char) - 1,
               "%s", child_pt->mac);
      LOG_DEBUG(OUTPOINT, "bleMAC: %s\r\n", child_pt->mac);
      ipc_children_data_idx++;
    }
    if(strlen(child_pt->manufacturer) > 0)
    {
      ipc_children_data[ipc_children_data_idx].field = 5;
      snprintf(ipc_children_data[ipc_children_data_idx].data.data_char,
               sizeof(ipc_children_data[ipc_children_data_idx].data.
                      data_char) - 1,
               "%s", child_pt->manufacturer);
      LOG_DEBUG(OUTPOINT, "manufacturer: %s\r\n", child_pt->manufacturer);
      ipc_children_data_idx++;
    }
    if(strlen(child_pt->name) > 0)
    {
      ipc_children_data[ipc_children_data_idx].field = 2;
      snprintf(ipc_children_data[ipc_children_data_idx].data.data_char,
               sizeof(ipc_children_data[ipc_children_data_idx].data.
                      data_char) - 1,
               "%s", child_pt->name);
      LOG_DEBUG(OUTPOINT, "name: %s\r\n", child_pt->name);
      ipc_children_data_idx++;
    }
    if(strlen(child_pt->type) > 0)
    {
      ipc_children_data[ipc_children_data_idx].field = 6;
      snprintf(ipc_children_data[ipc_children_data_idx].data.data_char,
               sizeof(ipc_children_data[ipc_children_data_idx].data.
                      data_char) - 1,
               "%s", child_pt->type);
      LOG_DEBUG(OUTPOINT, "type: %s\r\n", child_pt->type);
      ipc_children_data_idx++;
    }
    ipc_children_data[ipc_children_data_idx].field = 1;
    ipc_children_data[ipc_children_data_idx].data.data_uint32 =
      child_pt->nameId;
    LOG_DEBUG(OUTPOINT, "nameId: %d\r\n", child_pt->nameId);
    ipc_children_data_idx++;

    if(devCanBeDeletedFg)
    {
      if(ipc_sql_delete_item(DEVICE_LIST_TABLE, 0, NULL))
      {
        LOG_WARN(OUTPOINT,
                 "Failed to clean sensors that was removed!\r\n");
      }
      devCanBeDeletedFg = false;
    }
    if(ipc_sql_update_item(DEVICE_LIST_TABLE, child_pt->nameId,
                           ipc_children_data_idx,
                           ipc_children_data, NULL))
    {
      LOG_WARN(OUTPOINT, "Failed to update %d into Dev table\r\n",
               child_pt->nameId);
    }
    LOG_INFO(OUTPOINT, "update %d into Dev table with %d\r\n",
             child_pt->nameId,
             ipc_children_data_idx);
    for(uint32_t tmp = 0; tmp < ipc_children_data_idx; tmp++)
    {
      if(ipc_children_data[tmp].field == 6)
      {
        ipc_children_data[tmp].field = 11;
      }
    }
    if(ipc_sql_update_item(SENSOR_CONFIGURE_TABLE, child_pt->nameId,
                           ipc_children_data_idx,
                           ipc_children_data, NULL))
    {
      LOG_WARN(OUTPOINT, "Failed to update %d into Sensor table\r\n",
               child_pt->nameId);
    }
    LOG_INFO(OUTPOINT, "update %d into Sensor table with %d\r\n",
             child_pt->nameId,
             ipc_children_data_idx);

    ipc_children_data_idx = 0;
    memset(ipc_children_data, 0, sizeof(ipc_children_data));
    return true;
  }
  else
  {
    LOG_WARN(OUTPOINT, "Arg error\r\n");
    return false;
  }
}
static bool
protobuf_decode_configpair_callback(
  pb_istream_t *stream,
  const pb_field_t *field,
  void **arg) {
  pb_istream_t _stream = pb_istream_from_buffer(stream->state,
                                                stream->bytes_left);
  pb_istream_t _substream;
  pb_istream_t _stream_before_decode, _stream_after_decode;  // For copies
                                                             // of stream
  protobuf_encode_config_pairs_child_t child;
  protobuf_encode_timearray_t timearray;
  protobuf_encode_general_bytes_t d;
  uint8_t _buf[50];
  SKFChina_ConfigurationAndCommand_ConfigPair cfg_pair;
  bool rt = true;

  // Make a copy of stream
  memcpy(&_stream_before_decode, &_stream, sizeof(pb_istream_t));
  LOG_INFO(OUTPOINT, "Current decoded MAC: %s\r\n", sensor_id);
#if 1  // Just for debug
  {
    uint32_t _size = _stream.bytes_left;
    uint8_t stream_buf[3000];
    pb_read(&_stream, stream_buf, _size);
    LOG_DEBUG(OUTPOINT, "The string: ");
    for(uint32_t cnt = 0; cnt < _size; cnt++)
    {
      printf("%02x ", stream_buf[cnt]);
    }
    printf("\r\n");
  }
  memcpy(&_stream, &_stream_before_decode, sizeof(pb_istream_t));
#endif

  // This is a bug to deal with "bytes" in "oneof"
  // We have to put the following code before calling pb_decode
  d.buffer = _buf;
  d.size = sizeof(_buf);
  memset(d.buffer, 0, d.size);
  protobuf_decode_bytes(&(cfg_pair.config_content.general_config_content),
                        &d);

  // Decode the message (i.e., the config pair field)
  rt = pb_decode(&_stream,
                 SKFChina_ConfigurationAndCommand_ConfigPair_fields,
                 &cfg_pair);
  if(rt == false)
  {
    LOG_WARN(OUTPOINT, "Failed to decode field\r\n");
    return false;
  }

  // Make a copy of stream
  memcpy(&_stream_after_decode, &_stream, sizeof(pb_istream_t));

  LOG_DEBUG(OUTPOINT, "which item %d\r\n", cfg_pair.which_config_item);
  LOG_DEBUG(OUTPOINT, "which content %d\r\n",
            cfg_pair.which_config_content);

  if(cfg_pair.which_config_content ==
     SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
  {
    // Recover the stream after the decode (and prepare for one more decode
    // for the "oneof")
    memcpy(&_stream, &_stream_before_decode, sizeof(pb_istream_t));

    LOG_DEBUG(OUTPOINT,
              "bytes_left (before decoding a specific field) %d\r\n",
              _stream.bytes_left);

    decode_unionmessage_type(&_stream,
                             SKFChina_ConfigurationAndCommand_ConfigPair_fields);
    if(!pb_make_string_substream(&_stream, &_substream))
    {
      return;
    }
    _substream = pb_istream_from_buffer(_substream.state,
                                        _substream.bytes_left);

    cfg_pair.config_content.time_config_content.time.funcs.decode =
      protobuf_decode_configpair_timearray_callback;
    cfg_pair.config_content.time_config_content.time.arg = &timearray;
    memset(&timearray, 0, sizeof(protobuf_encode_timearray_t));

    rt = pb_decode(&_substream,
                   SKFChina_ConfigurationAndCommand_TimeArray_fields,
                   &(cfg_pair.config_content.time_config_content.time));
    if(rt == false)
    {
      LOG_WARN(OUTPOINT, "Failed to decode field\r\n");
      return false;
    }

    LOG_DEBUG(OUTPOINT,
              "bytes_left (after decoding a specific field) %d\r\n",
              _stream.bytes_left);

    memcpy(&_stream, &_stream_after_decode, sizeof(pb_istream_t));
  }
  else if(cfg_pair.which_config_content ==
          SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag)
  {
    // Recover the stream after the decode (and prepare for one more decode
    // for the "oneof")
    memcpy(&_stream, &_stream_before_decode, sizeof(pb_istream_t));

    LOG_DEBUG(OUTPOINT,
              "bytes_left (before decoding a specific field) %d\r\n",
              _stream.bytes_left);

    decode_unionmessage_type(&_stream,
                             SKFChina_ConfigurationAndCommand_ConfigPair_fields);
    if(!pb_make_string_substream(&_stream, &_substream))
    {
      return;
    }
    _substream = pb_istream_from_buffer(_substream.state,
                                        _substream.bytes_left);

    // This is a bug to deal with "bytes" in "oneof"
    // We have to put the following code before calling pb_decode
    d.buffer = _buf;
    d.size = sizeof(_buf);
    memset(d.buffer, 0, d.size);
    protobuf_decode_bytes(&(cfg_pair.config_content.ip_addr.addr), &d);

    rt = pb_decode(&_substream,
                   SKFChina_ConfigurationAndCommand_IpAddr_fields,
                   &(cfg_pair.config_content.ip_addr));
    if(rt == false)
    {
      LOG_WARN(OUTPOINT, "Failed to decode field\r\n");
      return false;
    }

    LOG_DEBUG(OUTPOINT,
              "bytes_left (after decoding a specific field) %d\r\n",
              _stream.bytes_left);
    LOG_DEBUG(OUTPOINT, "Decoded bytes: %s\r\n", d.buffer);
    if(cfg_pair.config_content.ip_addr.isIpv4 == true)
    {
      LOG_DEBUG(OUTPOINT, "Ipv4\r\n");
    }
    else
    {
      LOG_DEBUG(OUTPOINT, "Ipv6\r\n");
    }

    memcpy(&_stream, &_stream_after_decode, sizeof(pb_istream_t));
  }
  else if(cfg_pair.which_config_content ==
          SKFChina_ConfigurationAndCommand_ConfigPair_children_list_tag)
  {
    // Recover the stream after the decode (and prepare for one more decode
    // for the "oneof")
    memcpy(&_stream, &_stream_before_decode, sizeof(pb_istream_t));

    LOG_DEBUG(OUTPOINT,
              "bytes_left (before decoding a specific field) %d\r\n",
              _stream.bytes_left);

    decode_unionmessage_type(&_stream,
                             SKFChina_ConfigurationAndCommand_ConfigPair_fields);
    if(!pb_make_string_substream(&_stream, &_substream))
    {
      return;
    }
    _substream = pb_istream_from_buffer(_substream.state,
                                        _substream.bytes_left);

    cfg_pair.config_content.children_list.child.funcs.decode =
      protobuf_decode_configpair_childrenlist_callback;
    cfg_pair.config_content.children_list.child.arg = &child;
    memset(&child, 0, sizeof(protobuf_encode_config_pairs_child_t));

    rt = pb_decode(&_substream,
                   SKFChina_ConfigurationAndCommand_ChildrenList_fields,
                   &(cfg_pair.config_content.children_list.child));
    if(rt == false)
    {
      LOG_WARN(OUTPOINT, "Failed to decode field\r\n");
      return false;
    }

    LOG_DEBUG(OUTPOINT,
              "bytes_left (after decoding a specific field) %d\r\n",
              _stream.bytes_left);

    memcpy(&_stream, &_stream_after_decode, sizeof(pb_istream_t));
  }
  else if(cfg_pair.which_config_content == 0)
  {
    // This is a bug to deal with "bytes" in "oneof"
    // which_config_content would be set to zero when decoding "bytes"
    // in "oneof"
    // So we here, revise which_config_content manually
    cfg_pair.which_config_content =
      SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_tag;
    LOG_DEBUG(OUTPOINT, "Decoded bytes: %s\r\n", d.buffer);
    d.size = strlen(d.buffer);
  }

  // Memory ID
  if(cfg_pair.has_memory_id)
  {
    // Do nothing but output for debug
    LOG_DEBUG(OUTPOINT,
              "memory_id = %d\r\n", cfg_pair.memory_id);
  }
  // Check config content
  switch(cfg_pair.which_config_item)
  {
    case
      SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag
      :
    {
      // Check config content
      switch(cfg_pair.which_config_content)
      {
        case SKFChina_ConfigurationAndCommand_ConfigPair_children_list_tag
          :
        case
          SKFChina_ConfigurationAndCommand_ConfigPair_ip_addr_tag
          :
        case
          SKFChina_ConfigurationAndCommand_ConfigPair_mqtt_sec_type_tag
          :
        case
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_tag
          :
        case
          SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag
          :
        case
          SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag
          :
        case
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag
          :
        case
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag
          :
        case
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag
          :
        case
          SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag
          :
        {
          LOG_DEBUG(OUTPOINT,
                    "SKF_BullGateway._messages.command_dissem.product: %d\r\n",
                    SKF_BullGateway._messages.command_dissem.product);
          if((SKF_BullGateway._messages.command_dissem.product !=
             SKFChina_Common_ProductType_BULLET_GATEWAY) && 
             (SKF_BullGateway._messages.command_dissem.product !=
             SKFChina_Common_ProductType_BULLET_GATEWAY_PRO))
          {
            LOG_DEBUG(OUTPOINT, "sensor config needs to be updated\r\n");
            generate_database_ipc_msg_for_sensor(&cfg_pair, ipc_data,
                                                 sizeof(ipc_data) /
                                                 sizeof(ipc_data[0]),
                                                 &timearray);
          }
          else
          {
            LOG_DEBUG(OUTPOINT, "gateway config needs to be updated\r\n");
            generate_database_ipc_msg_for_gateway(&cfg_pair,
                                                  ipc_data,
                                                  sizeof(ipc_data) /
                                                  sizeof(ipc_data[0]),
                                                  ipc_children_data,
                                                  sizeof(ipc_children_data) /
                                                  sizeof(ipc_children_data[
                                                           0]),
                                                  &timearray,
                                                  &child,
                                                  d.buffer,
                                                  d.size,
                                                  cfg_pair.config_content.ip_addr.isIpv4);
          }
          break;
        }
        default:
          LOG_WARN(OUTPOINT, "Unknown config content (%d)\r\n",
                   cfg_pair.config_item);
          break;
      }
      break;
    }
    case
      SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag
      :
    default:
      LOG_WARN(OUTPOINT, "Unknown config item\r\n");
      break;
  }

  return true;
}

// down msg
void Simple_disseminate(pb_size_t which_msg, pb_istream_t *stream) {
  // SKFChina_App_AppMessage SKF_BullGateway_tx = {0};
  pb_istream_t _substream;
  decode_unionmessage_type(stream, SKFChina_App_AppMessage_fields);
  if (!pb_make_string_substream(stream, &substream))
    return;
  _substream = pb_istream_from_buffer(substream.state, substream.bytes_left);
  switch (which_msg) {
    // version
  case SKFChina_App_AppMessage_version_retrieve_tag: {
    arg.buffer = sensor_id;
    arg.size = sizeof(sensor_id);
    protobuf_decode_bytes(
        &(SKF_BullGateway._messages.version_retrieve.sensor_id), &arg);
    if (!decode_submessage_contents(
            stream, SKFChina_FirmwareUpdateOverTheAir_VersionRetrieve_fields,
            &(SKF_BullGateway._messages.version_retrieve))) {
      return;
    }
    if (0 != strcmp(device_id, sensor_id)) {
      LOG_WARN(OUTPOINT,
               "VR sensor id is not equal esensor id in the header\r\n");
      return;
    }
    if ((SKF_BullGateway._messages.version_retrieve.has_is_big_endian) &&
        (SKF_BullGateway._messages.version_retrieve.is_big_endian)) {
      SKF_BullGateway._messages.version_retrieve.payload =
          swap32(SKF_BullGateway._messages.version_retrieve.payload);
    }
    // reply the firmware version
    // Simple_upload(which_msg,SKF_BullGateway_tx);
  } break;

  // data selection
  case SKFChina_App_AppMessage_data_selection_tag: {
    if ((SKF_BullGateway._messages.command_dissem.product !=
        SKFChina_Common_ProductType_BULLET_GATEWAY) && 
        (SKF_BullGateway._messages.command_dissem.product !=
        SKFChina_Common_ProductType_BULLET_GATEWAY_PRO)) {
      LOG_WARN(OUTPOINT, "The product type is wrong!\r\n");
      return;
    }
    arg.buffer = sensor_id;
    arg.size = sizeof(sensor_id);
    protobuf_decode_bytes(&(SKF_BullGateway._messages.data_selection.sensor_id),
                          &arg);
    // arg1.buffer = &measure_type;
    // arg1.size = sizeof(measure_type);
    // protobuf_decode_bytes(
    //     &(SKF_BullGateway._messages.data_selection.measure_type), &arg1);
    measure_type_index = 0;
    SKF_BullGateway._messages.data_selection.measure_type.funcs.decode =
        protobuf_decode_measure_type_callback;
    SKF_BullGateway._messages.data_selection.measure_type.arg = NULL;
    if (true ==
        pb_decode(&_substream,
                  SKFChina_SensingDataUpload_DataSelectionDisseminate_fields,
                  &SKF_BullGateway._messages.data_selection)) {
      LOG_INFO(OUTPOINT, "decode Data Selection Disseminate field success\r\n");
    }
    if (0 != strcmp(device_id, sensor_id)) {
      LOG_WARN(OUTPOINT, "Sensor id is not equal esensor id in the body\r\n");
      return;
    }
    // QUERY DATA from database

    // Simple_upload(which_msg,SKF_BullGateway_tx);
  } break;
    // fuota
  case SKFChina_App_AppMessage_fuota_notify_dissem_tag: {
    downloadtype = 1;
    ota_arg.buffer = temp_url;
    ota_arg.size = OTA_URL_ADDR_LEN;
    memset(ota_url, 0, OTA_URL_ADDR_LEN);
    memset(temp_url, 0, OTA_URL_ADDR_LEN);
    switch (SKF_BullGateway._messages.fuota_notify_dissem.which_fuota_method) {
    case SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_http_tag: {
      protobuf_decode_bytes(&(SKF_BullGateway._messages.fuota_notify_dissem
                                  .fuota_method.fuota_with_http.url),
                            &ota_arg);
      type = SKFChina_FirmwareUpdateOverTheAir_FUOTAWithHttp_fields;
    } break;
    case SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_froto_over_mqtt_tag: {
      protobuf_decode_bytes(&(SKF_BullGateway._messages.fuota_notify_dissem
                                  .fuota_method.fuota_with_froto_over_mqtt.url),
                            &ota_arg);
      type = SKFChina_FirmwareUpdateOverTheAir_FUOTAWithFrotoOverMqtt_fields;
      mqtt_topic_arg.buffer = mqtt_topic;
      mqtt_topic_arg.size = MQTT_TOPIC_LEN;
      protobuf_decode_bytes(
          &(SKF_BullGateway._messages.fuota_notify_dissem.fuota_method
                .fuota_with_froto_over_mqtt.mqtt_topic),
          &mqtt_topic_arg);
    } break;
    case SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_with_froto_over_ble_ots_tag: {
      type = SKFChina_FirmwareUpdateOverTheAir_FUOTAWithFrotoOverBLEOts_fields;
    } break;

    default:
      break;
    }
    decode_submessage_contents(
        &_substream, SKFChina_Froto_FrotoHeader_fields,
        &(SKF_BullGateway._messages.file_notify_dissem.header));
    decode_unionmessage_type(
        &_substream,
        SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fields);
    decode_one_filed(
        &_substream,
        SKF_BullGateway._messages.fuota_notify_dissem.which_fuota_method, type,
        &(SKF_BullGateway._messages.fuota_notify_dissem.fuota_method));
    // give signal to request ota
    // supplementary_info.buffer = &(SKF_BullGateway._messages.last_fuota_upload
    //                                   .last_update_supplementary_info);
    // supplementary_info.size =
    // sizeof(SKF_BullGateway._messages.last_fuota_upload
    //                                      .last_update_supplementary_info);
    // protobuf_decode_bytes(
    //     &(SKF_BullGateway._messages.fuota_notify_dissem.supplementary_info),
    //     &supplementary_info);

    arg.buffer = sensor_id;
    arg.size = sizeof(sensor_id);
    protobuf_decode_bytes(
        &(SKF_BullGateway._messages.fuota_notify_dissem.sensor_id), &arg);

    if (SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_elf_hash_value_tag !=
        SKF_BullGateway._messages.fuota_notify_dissem.which_check_value) {
      check_value.buffer = &(local_check_value.md5_value);
      check_value.size = COMMON_CHECK_VALUE_LEN;
      protobuf_decode_bytes(&(SKF_BullGateway._messages.fuota_notify_dissem
                                  .check_value.md5_value),
                            &check_value);
    }
    if (true ==
        pb_decode(
            &_substream,
            SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fields,
            &SKF_BullGateway._messages.fuota_notify_dissem)) {
      LOG_INFO(OUTPOINT, "decode fuota_notify_dissem field success\r\n");
    }
    // if (0 != strcmp(device_id, sensor_id)) {
    //   LOG_WARN(OUTPOINT,
    //            "VR sensor id is not equal esensor id in the body\r\n");
    //   return;
    // }
    snprintf((char *)ota_url, OTA_URL_ADDR_LEN - 1, "'%s'", temp_url);
    if ((SKF_BullGateway._messages.fuota_notify_dissem.has_is_big_endian) &&
        (SKF_BullGateway._messages.fuota_notify_dissem.is_big_endian)) {
      SKF_BullGateway._messages.fuota_notify_dissem.target_hardware_type =
          swap32(SKF_BullGateway._messages.fuota_notify_dissem
                     .target_hardware_type);
      SKF_BullGateway._messages.fuota_notify_dissem.target_hardware_version =
          swap32(SKF_BullGateway._messages.fuota_notify_dissem
                     .target_hardware_version);
      SKF_BullGateway._messages.fuota_notify_dissem.firmware_version = swap32(
          SKF_BullGateway._messages.fuota_notify_dissem.firmware_version);
      SKF_BullGateway._messages.fuota_notify_dissem.force_fuota =
          swap32(SKF_BullGateway._messages.fuota_notify_dissem.force_fuota);
      SKF_BullGateway._messages.fuota_notify_dissem.update_method =
          swap32(SKF_BullGateway._messages.fuota_notify_dissem.update_method);
      SKF_BullGateway._messages.fuota_notify_dissem.update_type =
          swap32(SKF_BullGateway._messages.fuota_notify_dissem.update_type);
      SKF_BullGateway._messages.fuota_notify_dissem.has_encryp =
          swap32(SKF_BullGateway._messages.fuota_notify_dissem.has_encryp);
      if (SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_elf_hash_value_tag ==
          SKF_BullGateway._messages.fuota_notify_dissem.which_check_value) {
        SKF_BullGateway._messages.fuota_notify_dissem.check_value
            .elf_hash_value =
            swap32(SKF_BullGateway._messages.fuota_notify_dissem.check_value
                       .elf_hash_value);
      }
      SKF_BullGateway._messages.fuota_notify_dissem.has_encryp =
          swap32(SKF_BullGateway._messages.fuota_notify_dissem.has_encryp);
    }

  } break;
    // file
  case SKFChina_App_AppMessage_file_notify_dissem_tag: {
    downloadtype = 2;
    ota_arg.buffer = temp_url;
    ota_arg.size = OTA_URL_ADDR_LEN;
    memset(ota_url, 0, OTA_URL_ADDR_LEN);
    memset(temp_url, 0, OTA_URL_ADDR_LEN);
    switch (SKF_BullGateway._messages.file_notify_dissem.which_fuota_method) {
    case SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_froto_over_ble_ots_tag: {
      type = SKFChina_FirmwareUpdateOverTheAir_FUOTAWithFrotoOverBLEOts_fields;
      // todo
    } break;
    case SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_http_tag: {
      protobuf_decode_bytes(&(SKF_BullGateway._messages.file_notify_dissem
                                  .fuota_method.fuota_with_http.url),
                            &ota_arg);
      type = SKFChina_FirmwareUpdateOverTheAir_FUOTAWithHttp_fields;
    } break;
    case SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_froto_over_mqtt_tag: {
      protobuf_decode_bytes(&(SKF_BullGateway._messages.file_notify_dissem
                                  .fuota_method.fuota_with_froto_over_mqtt.url),
                            &ota_arg);
      type = SKFChina_FirmwareUpdateOverTheAir_FUOTAWithFrotoOverMqtt_fields;
      mqtt_topic_arg.buffer = mqtt_topic;
      mqtt_topic_arg.size = MQTT_TOPIC_LEN;
      protobuf_decode_bytes(
          &(SKF_BullGateway._messages.file_notify_dissem.fuota_method
                .fuota_with_froto_over_mqtt.mqtt_topic),
          &mqtt_topic_arg);
      // should enable download with http
    } break;

    default:
      break;
    }
    arg.buffer = sensor_id;
    arg.size = sizeof(sensor_id);
    protobuf_decode_bytes(
        &(SKF_BullGateway._messages.file_notify_dissem.sensor_id), &arg);
    decode_submessage_contents(
        &_substream, SKFChina_Froto_FrotoHeader_fields,
        &(SKF_BullGateway._messages.file_notify_dissem.header));
    decode_unionmessage_type(
        &_substream,
        SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fields);
    decode_one_filed(
        &_substream,
        SKF_BullGateway._messages.file_notify_dissem.which_fuota_method, type,
        &(SKF_BullGateway._messages.file_notify_dissem.fuota_method));
    if (SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_elf_hash_value_tag !=
        SKF_BullGateway._messages.file_notify_dissem.which_check_value) {
      check_value.buffer = &(local_check_value.md5_value);
      check_value.size = COMMON_CHECK_VALUE_LEN;
      protobuf_decode_bytes(
          &(SKF_BullGateway._messages.file_notify_dissem.check_value.md5_value),
          &check_value);
    }
    if (true ==
        pb_decode(
            &_substream,
            SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fields,
            &SKF_BullGateway._messages.file_notify_dissem)) {
      LOG_INFO(OUTPOINT, "decode fuota_notify_dissem field success\r\n");
    }
    // if (0 != strcmp(device_id, sensor_id)) {
    //   LOG_WARN(OUTPOINT,
    //            "VR sensor id is not equal esensor id in the body\r\n");
    //   return;
    // }
    snprintf((char *)ota_url, OTA_URL_ADDR_LEN - 1, "'%s'", temp_url);
  } break;
    // config retrieve
  case SKFChina_App_AppMessage_config_retrieve_tag: {

    arg.buffer = sensor_id;
    arg.size = sizeof(sensor_id);
    ota_arg.buffer = temp_url;
    ota_arg.size = OTA_URL_ADDR_LEN;
    memset(up_url, 0, OTA_URL_ADDR_LEN);
    memset(temp_url, 0, OTA_URL_ADDR_LEN);
    protobuf_decode_bytes(
        &(SKF_BullGateway._messages.config_retrieve.sensor_id), &arg);
    protobuf_decode_bytes(
        &(SKF_BullGateway._messages.config_retrieve.httpPostUrl), &ota_arg);
    if (true ==
        pb_decode(&_substream,
                  SKFChina_ConfigurationAndCommand_ConfigRetrieve_fields,
                  &SKF_BullGateway._messages.config_retrieve)) {
      LOG_INFO(OUTPOINT, "decode config_retrieve field success\r\n");
    }
    snprintf((char *)up_url, OTA_URL_ADDR_LEN - 1, "'%s'", temp_url);
    LOG_DEBUG(OUTPOINT, "up_url:%s\r\n", up_url);
    // if (0 != strcmp(device_id, sensor_id)) {
    //   LOG_INFO(OUTPOINT,
    //            "VR sensor id is not equal esensor id in the header\r\n");
    //   return;
    // }
    // LOG_DEBUG(OUTPOINT, "up_url:%s\r\n", up_url);
    // // if (0 != strcmp(device_id, sensor_id)) {
    // //   LOG_INFO(OUTPOINT,
    // //            "VR sensor id is not equal esensor id in the header\r\n");
    // //   return;
    // // }
  } break;
  case SKFChina_App_AppMessage_config_dissem_tag: {
    arg.buffer = sensor_id;
    arg.size = sizeof(sensor_id);
    protobuf_decode_bytes(&(SKF_BullGateway._messages.config_dissem.sensor_id),
                          &arg);
    SKF_BullGateway._messages.config_dissem.config_pair.funcs.decode =
      protobuf_decode_configpair_callback;
    SKF_BullGateway._messages.config_dissem.config_pair.arg = NULL;

    ipc_data_idx = 0;
    memset(ipc_data, 0, sizeof(ipc_data));
    devCanBeDeletedFg = true;
    if (true ==
        pb_decode(&_substream,
                  SKFChina_ConfigurationAndCommand_ConfigDisseminate_fields,
                  &SKF_BullGateway._messages.config_dissem)) {
      LOG_INFO(OUTPOINT, "decode Configuration Disseminate field success\r\n");

      if(ipc_data_idx > 0)
      {
        LOG_INFO(OUTPOINT, "ipc_data_idx %d\r\n", ipc_data_idx);
        if((SKF_BullGateway._messages.command_dissem.product !=
            SKFChina_Common_ProductType_BULLET_GATEWAY) && 
           (SKF_BullGateway._messages.command_dissem.product !=
             SKFChina_Common_ProductType_BULLET_GATEWAY_PRO))
        {
          LOG_DEBUG(OUTPOINT, "sensor config needs to be updated\r\n");
          filterData[0].condition = gw_pro_sqlite_cond_str_equal;
          filterData[0].field = 4;
          
          snprintf(filterData[0].condParam.string_value.s,
                    GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", sensor_id);

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

          LOG_INFO(OUTPOINT, "update %d into sensor table with %d\r\n", 
                  filter_rx.filteredItemIdx[0],
                  ipc_data_idx);
          if (ipc_sql_update_item(SENSOR_CONFIGURE_TABLE, 
                                  filter_rx.filteredItemIdx[0], 
                                  ipc_data_idx,
                                  ipc_data,
                                  NULL)) {
            LOG_WARN(OUTPOINT, "Failed to update %d into sensor table\r\n", 
                                  filter_rx.filteredItemIdx[0]);
          }
        }else{
          LOG_DEBUG(OUTPOINT, "gateway config needs to be updated\r\n");
          filterData[0].condition = gw_pro_sqlite_cond_str_equal;
          filterData[0].field = 4;

          snprintf(filterData[0].condParam.string_value.s,
                  GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", sensor_id);
          LOG_INFO(OUTPOINT, "Current decoded MAC: %s\r\n",
                  filterData[0].condParam.string_value.s);

          if(ipc_sql_filter_item(GATEWAY_CONFIGURE_TABLE, 1, filterData,
                                 &msgbufp) ||
             pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
             pdata->command_or_response.responseMsg.response ==
             gw_pro_response_failed)
          {
            LOG_WARN(OUTPOINT, "Failed to get data from Dev table\r\n");
            return;
          }
          memset(&filter_rx, 0, sizeof(filter_rx));
          memcpy(&filter_rx,
                &pdata->command_or_response.responseMsg.responseInfo
                .sqlite_filter_item_reponse,
                sizeof(gw_pro_command_sqlite_filter_response_t));

          // Reserved fields
          // nameID
          ipc_data[ipc_data_idx].field = 1;
          ipc_data[ipc_data_idx].data.data_uint32 = filter_rx.filteredItemIdx[0];
          ipc_data_idx++;
          // macAddr
          ipc_data[ipc_data_idx].field = 3;
          snprintf(ipc_data[ipc_data_idx].data.data_char, 
                  sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
                  "%s", sensor_id);
          ipc_data_idx++;
          // type
          ipc_data[ipc_data_idx].field = 9;
          snprintf(ipc_data[ipc_data_idx].data.data_char, 
                  sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
                  "GATEWAY");
          ipc_data_idx++;
          // manufacturer
          ipc_data[ipc_data_idx].field = 4;
          snprintf(ipc_data[ipc_data_idx].data.data_char, 
                  sizeof(ipc_data[ipc_data_idx].data.data_char) - 1,
                  "SKF");
          ipc_data_idx++;
          
          LOG_INFO(OUTPOINT, "update %d into gateway table with %d\r\n",
                  filter_rx.filteredItemIdx[0],
                  ipc_data_idx);
          if(ipc_sql_update_item(GATEWAY_CONFIGURE_TABLE,
                                filter_rx.filteredItemIdx[0], ipc_data_idx,
                                ipc_data,
                                NULL))
          {
            LOG_WARN(OUTPOINT, "Failed to update %d into gateway table\r\n",
                    filter_rx.filteredItemIdx[0]);
          }
        }
        ipc_data_idx = 0;
        memset(ipc_data, 0, sizeof(ipc_data));
      }
     }
  } break;
  default:
    break;
  }
  pb_close_string_substream(stream, &substream);
}

void Reliable_disseminate(pb_size_t which_msg, pb_istream_t *stream) {
  pb_istream_t _substream;
  _substream = pb_istream_from_buffer(stream->state, stream->bytes_left);
  // SKFChina_App_AppMessage SKF_BullGateway_tx = {0};
  switch (which_msg) {
    // commond info
  case SKFChina_App_AppMessage_command_dissem_tag: {
    if ((SKF_BullGateway._messages.command_dissem.product !=
        SKFChina_Common_ProductType_BULLET_GATEWAY) &&
        (SKF_BullGateway._messages.command_dissem.product !=
        SKFChina_Common_ProductType_BULLET_GATEWAY_PRO)) {
      LOG_WARN(OUTPOINT, "The product type is wrong!\r\n");
      return;
    }
    if (!SKF_BullGateway._messages.command_dissem.has_command_pair) {
      LOG_WARN(OUTPOINT, "No command!\r\n");
      return;
    }
    arg.buffer = sensor_id;
    arg.size = sizeof(sensor_id);
    protobuf_decode_bytes(&(SKF_BullGateway._messages.command_dissem.sensor_id),
                          &arg);
    if (true ==
        pb_decode(&_substream,
                  SKFChina_ConfigurationAndCommand_CommandDisseminate_fields,
                  &SKF_BullGateway._messages.command_dissem)) {
      LOG_INFO(OUTPOINT, "decode Command Disseminate field success\r\n");
    }
    if (0 != strcmp(device_id, sensor_id)) {
      LOG_INFO(OUTPOINT, "Sensor id is not equal esensor id in the body\r\n");
      return;
    }

    // To do
    switch (SKF_BullGateway._messages.command_dissem.command_pair.command) {
    case SKFChina_Common_Command_CLEAR_TEMPERATURE_HISTORY: {
      // if (send_msg(msgid, &msgbufp, strlen(msgbufp.mtext))) {
      //   LOG_INFO(OUTPOINT, "%s,%d\r\n", __FILE__, __LINE__);
      //   break;
      // }
      // if (recv_msg(msgid, &msgbufp, process_mqtt)) {
      //   LOG_INFO(OUTPOINT, "%s,%d\r\n", __FILE__, __LINE__);
      //   break;
      // }
      // msgbuf_txrxp = (msgbuf_Typedef *)msgbufp.mtext;
      // if (msgbuf_txrxp->msginfo.status) {
      //   LOG_INFO(OUTPOINT,
      //              "Gateway Failed to clean temperature hisroty!\r\n");
      //   break;
      // }
      LOG_INFO(OUTPOINT, "Gateway will clean all temperature hisroty!\r\n");
    } break;

    default:
      LOG_WARN(OUTPOINT, "Invalid command for gateway!\r\n");
      break;
    }
    // memset((void *)&SKF_BullGateway_tx, 0, sizeof(SKF_BullGateway_tx));
    // Simple_upload(which_msg, SKF_BullGateway_tx);
  } break;
  default:
    break;
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