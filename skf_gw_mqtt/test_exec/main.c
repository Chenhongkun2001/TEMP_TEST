
#include "ipcmbs.h"
#include <createtable.h>
#include <delete.h>
#include <global.h>
#include <hash.h>
#include <insert.h>
// #include <misc.h>
#include "hash.h"
#include "jsonparse.h"
#include <ppGW.h>
#include <query.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>
#include <update.h>
DBG_LOCAL_LOG_DEBUG

int sql_msgid = 0;
int mqtt_msgid = 0;
int ble_msgid = 0;
gw_pro_process_type_t set_from_process = gw_pro_process_ble;
static gw_pro_sqlite_cond_element_t
    filterData[GW_PRO_MAX_DATA_FIELD_PER_OPERATION] = {0};
static gw_pro_command_sqlite_filter_response_t filter_rx = {0};
gw_pro_command_sqlite_query_item_response_t query_data = {0};

static struct msgbuf msgbufp = {0};
static gw_pro_message_header_t *pdata =
    (gw_pro_message_header_t *)msgbufp.mtext;
char clientID[50] = {0};

// for logging
// only for making build stream works so far
uint8_t g_log_level = CONFIG_LOG_LEVEL_SET;

// data
int main(int argc, char *argv[]) {
  if (argc > 3) {
    LOG_WARN(OUTPOINT, "Too much args!\r\n");
    return 1;
  }
  // create_sql(DATABASE_NAME);
  // sleep(2);
  // // test();
  mqtt_msgid = msgget(MSG_MQTT_QUEUE_KEY, IPC_CREAT | MSG_QUEUE_FLAG);
  sql_msgid = msgget(MSG_SQL_QUEUE_KEY, IPC_CREAT | MSG_QUEUE_FLAG);
  ble_msgid = msgget(MSG_BLE_QUEUE_KEY, IPC_CREAT | MSG_QUEUE_FLAG);
  if (argc == 1) {
    parse_config(GATEWAY_CONFIG_FILE_NAME);
    sleep(2);
    parse_config(SENSOR_CONFIG_FILE_NAME);
  } else if (!strcmp(argv[1], "d")) {
    gw_pro_data_t data[20] = {0};
    while (1) {
      for (uint16_t i = 0; i < 2000; i++) {
        data[0].field = 2;
        data[0].data.data_uint32 = 62586977;
        data[1].field = 11;
        data[1].data.data_uint8 = 13;
        data[2].field = 12;
        sprintf(data[2].data.data_char, "%s", "Digit");
        data[3].field = 21;
        data[3].data.data_uint8 = -12 + i;
        data[4].field = 18;
        data[4].data.data_uint8 = 5;
        data[5].field = 4;
        sprintf(data[5].data.data_char, "%s", "C4BD6A123459");
        ipc_sql_update_item(SENSOR_DATA_TABLE, data[0].data.data_uint32, 20,
                            data, NULL);
        LOG_WARN(OUTPOINT, "send no:%d\r\n", i);
      }
      sleep(3);
    }
  } else if (!strcmp(argv[1], "d1")) {
    gw_pro_data_t data[20] = {0};
    // while (1) {
    for (uint16_t i = 0; i < 1; i++) {
      data[0].field = 2;
      data[0].data.data_uint32 = 1465778209;
      data[1].field = 11;
      data[1].data.data_uint8 = 46; // 13:uint8   46:float
      data[2].field = 12;
      sprintf(data[2].data.data_char, "%s", "Digit");
      data[3].field = 21;
      data[3].data.data_float = 0.25f;
      data[4].field = 18;
      data[4].data.data_uint8 = 7; // 5-uint8;7-float
      data[5].field = 4;
      sprintf(data[5].data.data_char, "%s", "C4BD6A123459");
      ipc_sql_update_item(SENSOR_DATA_TABLE, data[0].data.data_uint32, 20, data,
                          NULL);
      LOG_WARN(OUTPOINT, "send no:%d\r\n", i);
    }
    // sleep(3);
    // }
  } else if (!strcmp(argv[1], "w")) {
    gw_pro_data_t data[20] = {0};
    // while (1) {
    data[0].field = 2;
    data[0].data.data_uint32 = 62586977;
    data[1].field = 11;
    data[1].data.data_uint8 = 3; // 1/3/4
    data[2].field = 12;
    sprintf(data[2].data.data_char, "%s", "Filename");
    data[3].field = 21;
    sprintf(data[3].data.data_char, "%s", "sdata");
    data[5].field = 4;
    sprintf(data[5].data.data_char, "%s", "C4BD6A123457");
    ipc_sql_update_item(SENSOR_DATA_TABLE, data[0].data.data_uint32, 20, data,
                        NULL);
    // sleep(3);
    // }
  } else if (!strcmp(argv[1], "a")) {
    gw_pro_data_t data[20] = {0};
    // while (1) {
    data[0].field = 2;
    data[0].data.data_uint32 = 311978256;
    data[1].field = 11;
    data[1].data.data_uint8 = 45; // alarm code
    data[2].field = 12;
    sprintf(data[2].data.data_char, "%s", "Filename");
    data[3].field = 21;
    sprintf(data[3].data.data_char, "%s", "test1.txt");
    data[5].field = 4;
    sprintf(data[5].data.data_char, "%s", "C4BD6A123457");
    ipc_sql_update_item(SENSOR_DATA_TABLE, data[0].data.data_uint32, 20, data,
                        NULL);
    // sleep(3);
    // }
  } else if (!strcmp(argv[1], "q")) {
    memset(&msgbufp, 0, sizeof(msgbufp));
    ipc_sql_query_item(SENSOR_DATA_TABLE, atoi(argv[2]), &msgbufp);
  } else if (!strcmp(argv[1], "h")) {
    gw_pro_command_sqlite_query_item_response_t query_response = {0};
    memset(&msgbufp, 0, sizeof(msgbufp));
    if (ipc_sql_query_item(GATEWAY_CONFIGURE_TABLE, 0, &msgbufp) ||
        pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
        pdata->command_or_response.responseMsg.response ==
            gw_pro_response_failed ||
        pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response.dataNum == 0) {
      LOG_WARN(OUTPOINT, "Failed to get data from gateway table\r\n");
      return 1;
    }
    LOG_DEBUG(OUTPOINT, "%d\r\n",
              pdata->command_or_response.responseMsg.responseInfo
                  .sqlite_update_item_response.dataNum);
    for (uint32_t i = 0; i < pdata->command_or_response.responseMsg.responseInfo
                                 .sqlite_update_item_response.dataNum;
         i++) {
      switch (pdata->command_or_response.responseMsg.responseInfo
                  .sqlite_update_item_response.inquiredData[i]
                  .field) {
      case 3: {
        if (!strlen(pdata->command_or_response.responseMsg.responseInfo
                        .sqlite_update_item_response.inquiredData[i]
                        .data.data_char)) {
          LOG_WARN(OUTPOINT,
                   "Failed to get MAC address from gateway table\r\n");
          break;
        }
        snprintf(clientID, sizeof(clientID) - 1, "%s",
                 pdata->command_or_response.responseMsg.responseInfo
                     .sqlite_update_item_response.inquiredData[i]
                     .data.data_char);
      } break;
      default:
        break;
      }
    }
    memset(&msgbufp, 0, sizeof(msgbufp));
    if (ipc_sql_query_item(SENSOR_CONFIGURE_TABLE, atoi(argv[2]), &msgbufp) ||
        pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
        pdata->command_or_response.responseMsg.response ==
            gw_pro_response_failed ||
        pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response.dataNum == 0) {
      LOG_WARN(OUTPOINT, "Failed to get data from sensor config table\r\n");
      return 1;
    }
    memset(&query_response, 0, sizeof(query_response));
    memcpy(&query_response,
           &pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response,
           sizeof(gw_pro_command_sqlite_query_item_response_t));
    uint32_t hash = calculate_config_hash_from_db(&query_response, clientID);
    LOG_WARN(OUTPOINT, "Hash: %d\r\n", hash);
  } else if (!strcmp(argv[1], "i")) {
    filterData[0].field = 11;
    filterData[0].condition = gw_pro_sqlite_cond_value_range;
    filterData[0].condParam.range_uint32.max =
        filterData[0].condParam.range_uint32.min = 46; // 13/45
    filterData[1].condition = gw_pro_sqlite_cond_and;
    filterData[2].field = 4;
    filterData[2].condition = gw_pro_sqlite_cond_str_equal;
    snprintf(filterData[2].condParam.string_value.s,
             GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", "C4BD6A123459");
    // filterData[3].condition = gw_pro_sqlite_cond_and;
    // filterData[4].field = 21;
    // filterData[4].condition = gw_pro_sqlite_cond_value_range;
    // filterData[4].condParam.range_uint32.max =
    //     SKF_BullGateway._messages.data_selection.sample_time_end;
    // filterData[4].condParam.range_uint32.min =
    //     SKF_BullGateway._messages.data_selection.sample_time_start;
    if (ipc_sql_filter_item(SENSOR_DATA_TABLE, 3, filterData, &msgbufp) ||
        pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
        pdata->command_or_response.responseMsg.response ==
            gw_pro_response_failed ||
        pdata->command_or_response.responseMsg.responseInfo
                .sqlite_filter_item_reponse.dataNum == 0) {
      LOG_WARN(OUTPOINT, "Failed to get datas from sensor data table\r\n");
      return 1;
    }
    memset(&filter_rx, 0, sizeof(filter_rx));
    memcpy(&filter_rx,
           &pdata->command_or_response.responseMsg.responseInfo
                .sqlite_filter_item_reponse,
           sizeof(gw_pro_command_sqlite_filter_response_t));
    LOG_WARN(OUTPOINT, "datanum:%d\r\n",
             pdata->command_or_response.responseMsg.responseInfo
                 .sqlite_filter_item_reponse.dataNum);
    uint32_t tempid =
        findMax((int *)filter_rx.filteredItemIdx, 0, filter_rx.dataNum - 1);
    LOG_WARN(OUTPOINT, "id:%d\r\n", tempid);
    memset(&msgbufp, 0, sizeof(msgbufp));
    if (ipc_sql_query_item(SENSOR_DATA_TABLE, tempid, &msgbufp) ||
        pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
        pdata->command_or_response.responseMsg.response ==
            gw_pro_response_failed) {
      LOG_WARN(OUTPOINT, "Failed to get datas from sensor table\r\n");
      return 1;
    }
    memcpy(&query_data,
           &pdata->command_or_response.responseMsg.responseInfo
                .sqlite_update_item_response,
           sizeof(gw_pro_command_sqlite_query_item_response_t));

  } else {
    LOG_WARN(OUTPOINT, "Invalid arg!\r\n");
  }
  LOG_INFO(OUTPOINT, "just for test purpose!\r\n");
  return 0;
}
