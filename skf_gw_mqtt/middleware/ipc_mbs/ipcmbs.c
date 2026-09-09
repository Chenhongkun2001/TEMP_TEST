#include "ipcmbs.h"
#include "global.h"
#include "stdio.h"
#include "unistd.h"
#include <stdint.h>
#include <string.h>
#include <time.h>
DBG_LOCAL_LOG_DEBUG

extern int mqtt_msgid;
extern int sql_msgid;
extern int ble_msgid;
extern gw_pro_process_type_t set_from_process;

static uint32_t ipcMsgNo = 0;

static uint8_t msgbuf_tx_init(gw_pro_message_header_t *pdata, const char *tb,
                              uint32_t *table) {
  pdata->from = set_from_process;
  pdata->msgTimestamp_s = time(NULL);
  pdata->seqNo = 1;
  pdata->current_package = 1;
  pdata->total_package = 1;
  if (0 == strcmp(tb, GATEWAY_CONFIGURE_TABLE))
    *table = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
  else if (0 == strcmp(tb, DEVICE_LIST_TABLE)) {
    *table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
  } else if (0 == strcmp(tb, SENSOR_CONFIGURE_TABLE)) {
    *table = GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE;
  } else if (0 == strcmp(tb, SENSOR_DATA_TABLE)) {
    *table = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
  } else {
    LOG_WARN(OUTPOINT, "Invalid table!\r\n");
    return 1;
  }
  return 0;
}

static uint8_t msgbuf_res_init(gw_pro_message_header_t *pdata) {
  pdata->from = set_from_process;
  pdata->msgTimestamp_s = time(NULL);
  pdata->seqNo = 1;
  pdata->current_package = 1;
  pdata->total_package = 1;
  return 0;
}
uint8_t ipc_sql_update_item(const char *tb, uint32_t keyID, uint32_t dataNO,
                            gw_pro_data_t *data, struct msgbuf *msgbufp) {
  uint8_t status = 0;
  struct msgbuf txbuf = {0};
  struct msgbuf rxbuf = {0};
  msgbufp = &rxbuf;
  int qid = 0;
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)txbuf.mtext;
  if (msgbuf_tx_init(pdata, tb,
                     &pdata->command_or_response.commandMsg.param
                          .sqlite_update_item_param.table))
    return 1;
  pdata->seqNo = ipcMsgNo++;
  pdata->command_or_response.commandMsg.command =
      gw_pro_command_sqlite_update_item;
  pdata->command_or_response.commandMsg.commandId =
      pdata->seqNo;
  pdata->command_or_response.commandMsg.param.sqlite_update_item_param.itemIdx =
      keyID;
  pdata->command_or_response.commandMsg.param.sqlite_update_item_param.dataNum =
      dataNO;
  pdata->comRespFg = GW_PRO_COMRESPFG_COMMAND;
  for (uint8_t i = 0; i < dataNO; i++) {
    pdata->command_or_response.commandMsg.param.sqlite_update_item_param
        .updateData[i]
        .field = data[i].field;
    memcpy(
        &(pdata->command_or_response.commandMsg.param.sqlite_update_item_param
              .updateData[i]
              .data.data_char),
        &(data[i].data.data_char), GW_PRO_MAX_STRING_LEN_BYTE);
  }
  txbuf.mtype = gw_pro_command;
  if (pdata->from == gw_pro_process_mqtt) {
    qid = mqtt_msgid;
  } else {
    qid = ble_msgid;
  }
  LOG_DEBUG(OUTPOINT, "Prepare to send msg to sql:\r\n");
  if (!send_msg(sql_msgid, &txbuf, sizeof(gw_pro_message_header_t))) {
    status = recv_msg(qid, &rxbuf, gw_pro_response);
  }
  return status;
}
uint8_t ipc_sql_delete_item(const char *tb, uint32_t keyID,
                            struct msgbuf *msgbufp) {
  struct msgbuf txbuf = {0};
  int qid = 0;
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)txbuf.mtext;
  memset(&txbuf, 0, sizeof(txbuf));

  if (msgbuf_tx_init(pdata, tb,
                     &pdata->command_or_response.commandMsg.param
                          .sqlite_delete_item_param.table))
    return 1;
  pdata->seqNo = ipcMsgNo++;
  pdata->command_or_response.commandMsg.command =
      gw_pro_command_sqlite_delete_item;
  pdata->command_or_response.commandMsg.commandId =
      pdata->seqNo;
  pdata->command_or_response.commandMsg.param.sqlite_delete_item_param.itemIdx =
      keyID;
  pdata->command_or_response.commandMsg.param.sqlite_delete_item_param.dataNum =
      0;
  pdata->comRespFg = GW_PRO_COMRESPFG_COMMAND;
  txbuf.mtype = gw_pro_command;
  if (pdata->from == gw_pro_process_mqtt) {
    qid = mqtt_msgid;
  } else {
    qid = ble_msgid;
  }
  LOG_DEBUG(OUTPOINT, "Prepare to send msg sql:\r\n");
  if (!send_msg(sql_msgid, &txbuf, sizeof(gw_pro_message_header_t))) {
    recv_msg(qid, msgbufp, gw_pro_response);
    return 0;
  }
  return 1;
}
uint8_t ipc_sql_query_item(const char *tb, uint32_t keyID,
                           struct msgbuf *msgbufp) {
  uint8_t status = 0;
  struct msgbuf txbuf = {0};
  int qid = 0;
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)txbuf.mtext;
  memset(&txbuf, 0, sizeof(txbuf));

  if (msgbuf_tx_init(pdata, tb,
                     &pdata->command_or_response.commandMsg.param
                          .sqlite_query_item_param.table))
    return 1;
  pdata->seqNo = ipcMsgNo++;
  pdata->command_or_response.commandMsg.command =
      gw_pro_command_sqlite_query_item;
  pdata->command_or_response.commandMsg.commandId =
      pdata->seqNo;
  pdata->command_or_response.commandMsg.param.sqlite_query_item_param.itemIdx =
      keyID;
  pdata->comRespFg = GW_PRO_COMRESPFG_COMMAND;
  txbuf.mtype = gw_pro_command;
  if (pdata->from == gw_pro_process_mqtt) {
    qid = mqtt_msgid;
  } else {
    qid = ble_msgid;
  }
  LOG_DEBUG(OUTPOINT, "Prepare to send msg sql:\r\n");
  if (!(status =
            send_msg(sql_msgid, &txbuf, sizeof(gw_pro_message_header_t)))) {
    status = recv_msg(qid, msgbufp, gw_pro_response);
  }
  return status;
}
uint8_t ipc_sql_filter_item(const char *tb, uint32_t dataNO,
                            gw_pro_sqlite_cond_element_t *filterData,
                            struct msgbuf *msgbufp) {
  uint8_t status = 0;
  struct msgbuf txbuf = {0};
  int qid = 0;
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)txbuf.mtext;
  memset(&txbuf, 0, sizeof(txbuf));
  if (msgbuf_tx_init(pdata, tb,
                     &pdata->command_or_response.commandMsg.param
                          .sqlite_filter_item_param.table))
    return 1;
  pdata->seqNo = ipcMsgNo++;
  pdata->command_or_response.commandMsg.command = gw_pro_command_sqlite_filter;
  pdata->command_or_response.commandMsg.commandId =
      pdata->seqNo;
  pdata->command_or_response.commandMsg.param.sqlite_filter_item_param.dataNum =
      dataNO;
  pdata->comRespFg = GW_PRO_COMRESPFG_COMMAND;
  for (uint16_t i = 0; i < dataNO; i++) {
    pdata->command_or_response.commandMsg.param.sqlite_filter_item_param
        .filterData[i]
        .condition = filterData[i].condition;
    pdata->command_or_response.commandMsg.param.sqlite_filter_item_param
        .filterData[i]
        .field = filterData[i].field;
    memcpy(&pdata->command_or_response.commandMsg.param.sqlite_filter_item_param
                .filterData[i]
                .condParam,
           &filterData[i].condParam, sizeof(filterData[i].condParam));
  }
  txbuf.mtype = gw_pro_command;
  if (pdata->from == gw_pro_process_mqtt) {
    qid = mqtt_msgid;
  } else {
    qid = ble_msgid;
  }
  LOG_DEBUG(OUTPOINT, "Prepare to send msg sql:\r\n");
  if (!(status =
            send_msg(sql_msgid, &txbuf, sizeof(gw_pro_message_header_t)))) {
    status = recv_msg(qid, msgbufp, gw_pro_response);
  }
  return status;
}

// from sqlite
uint8_t ipc_sql_report_new_item(const char *tb, uint32_t keyID, uint8_t op,
                                int qid, gw_pro_process_type_t to) {
  uint8_t status = 0;
  struct msgbuf txbuf = {0};
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)txbuf.mtext;
  memset(&txbuf, 0, sizeof(txbuf));
  if (msgbuf_tx_init(pdata, tb,
                     &pdata->command_or_response.commandMsg.param
                          .report_new_modification_param.table))
    return 1;
  pdata->seqNo = ipcMsgNo++;
  pdata->from = gw_pro_process_sqlite;
  pdata->command_or_response.commandMsg.command =
      gw_pro_command_report_new_modification;
  pdata->command_or_response.commandMsg.commandId =
      pdata->seqNo;
  pdata->comRespFg = GW_PRO_COMRESPFG_COMMAND;
  pdata->command_or_response.commandMsg.param.report_new_modification_param
      .itemIdx = keyID;
  pdata->command_or_response.commandMsg.param.report_new_modification_param
      .operation = op;
  pdata->command_or_response.commandMsg.param.report_new_modification_param
      .timestamp = time(NULL);
  txbuf.mtype = gw_pro_command;
  LOG_DEBUG(OUTPOINT, "Prepare to send msg %s:\r\n",
            qid == mqtt_msgid ? "mqtt" : "ble");
  status = send_msg(qid, &txbuf, sizeof(gw_pro_message_header_t));
  return status;
}

// response
uint8_t ipc_sql_response(gw_pro_command_t cmd, uint32_t cmdId,
                         gw_pro_response_t res, int qid,
                         uint32_t dataNO, void *data,
                         gw_pro_process_type_t to) {
  uint8_t status = 0;
  struct msgbuf txbuf = {0};
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)txbuf.mtext;
  gw_pro_data_t *temp = (gw_pro_data_t *)data;
  uint32_t *inttemp = (uint32_t *)data;
  memset(&txbuf, 0, sizeof(txbuf));

  msgbuf_res_init(pdata);
  pdata->seqNo = ipcMsgNo++;
  pdata->command_or_response.responseMsg.command = cmd;
  pdata->command_or_response.responseMsg.commandId = cmdId;
  pdata->command_or_response.responseMsg.response = res;
  pdata->comRespFg = GW_PRO_COMRESPFG_RESPONSE;
  switch (cmd) {
  case gw_pro_command_sqlite_query_item: {
    pdata->command_or_response.responseMsg.responseInfo
        .sqlite_update_item_response.dataNum = dataNO;
    for (uint16_t i = 0; i < dataNO; i++) {
      pdata->command_or_response.responseMsg.responseInfo
          .sqlite_update_item_response.inquiredData[i]
          .field = temp[i].field;
      memcpy(&(pdata->command_or_response.responseMsg.responseInfo
                   .sqlite_update_item_response.inquiredData[i]
                   .data.data_char),
             &(temp[i].data.data_char), GW_PRO_MAX_STRING_LEN_BYTE);
    }
  } break;
  case gw_pro_command_sqlite_filter: {
    pdata->command_or_response.responseMsg.responseInfo
        .sqlite_filter_item_reponse.dataNum = dataNO;
    for (uint16_t i = 0; i < dataNO; i++) {
      pdata->command_or_response.responseMsg.responseInfo
          .sqlite_filter_item_reponse.filteredItemIdx[i] = inttemp[i];
    }
  } break;
  default:
    break;
  }
  txbuf.mtype = gw_pro_response;
  LOG_DEBUG(OUTPOINT, "Prepare to send msg %s:\r\n",
            qid == mqtt_msgid ? "mqtt" : "ble");
  status = send_msg(qid, &txbuf, sizeof(gw_pro_message_header_t));
  return status;
}
