#ifndef IPCMBS_H
#define IPCMBS_H

#include "ppGW.h"
#include "stdint.h"
#include <auxiliar.h>
#include <stdint.h>
uint8_t ipc_sql_update_item(const char *tb, uint32_t keyID, uint32_t dataNO,
                            gw_pro_data_t *data, struct msgbuf *msgbufp);
uint8_t ipc_sql_delete_item(const char *tb, uint32_t keyID,
                            struct msgbuf *msgbufp);
uint8_t ipc_sql_query_item(const char *tb, uint32_t keyID,
                           struct msgbuf *msgbufp);
uint8_t ipc_sql_filter_item(const char *tb, uint32_t dataNO,
                            gw_pro_sqlite_cond_element_t *filterData,
                            struct msgbuf *msgbufp);
uint8_t ipc_sql_report_new_item(const char *tb, uint32_t keyID, uint8_t op,
                                int qid, gw_pro_process_type_t to);
uint8_t ipc_sql_response(gw_pro_command_t cmd, uint32_t cmdId, 
                         gw_pro_response_t res, int qid,
                         uint32_t dataNO, void *data, gw_pro_process_type_t to);
#endif
