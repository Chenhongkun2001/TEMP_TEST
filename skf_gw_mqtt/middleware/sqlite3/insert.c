#include "sqlite3.h"
#include <auxiliar.h>
#include <global.h>
#include <ppGW.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
DBG_LOCAL_LOG_DEBUG

static void constrcut_sql(uint32_t dataNO, gw_pro_data_t *dataptr, uint32_t tb,
                          char *temp_sql, char *sql);
static void gain_operadata(configure_content_t *tbitem, void *data,
                           char *temp_sql, char *sql);

int sqlite_insert(const char *dbname, uint32_t dataNO, uint32_t keyID,
                  gw_pro_data_t *dataptr, uint32_t tbindex, char *tb) {
  sqlite3 *db = NULL;
  char *err_msg = NULL;
  char sql[4096] = {0};
  char temp_sql[2048] = {0};
  snprintf(sql, sizeof(sql) - 1, "INSERT INTO %s ( ", tb);
  snprintf(temp_sql, sizeof(temp_sql) - 1, "%s", " VALUES (");
  constrcut_sql(dataNO, dataptr, tbindex, temp_sql, sql);
  sql[strlen(sql) - 1] = ')';
  sql[strlen(sql)] = ' ';
  temp_sql[strlen(temp_sql) - 1] = ')';
  temp_sql[strlen(temp_sql)] = ';';
  strcat(sql, temp_sql);

  int rc = sqlite3_open(dbname, &db);
  if (rc != SQLITE_OK) {
    LOG_WARN(OUTPOINT, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return gw_pro_response_failed;
  }
  LOG_INFO(OUTPOINT, "sql:%s\r\n", sql);
  if ((rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg)) != SQLITE_OK) {
    LOG_WARN(OUTPOINT, "Sql error is =%s \n", err_msg);
    return gw_pro_response_failed;
  }
  sqlite3_close(db);
  LOG_INFO(OUTPOINT, "Insert table successfully\n");
  return gw_pro_response_success;
}

static void constrcut_sql(uint32_t dataNO, gw_pro_data_t *dataptr, uint32_t tb,
                          char *temp_sql, char *sql) {
  bool valuetype = false;
  bool vartype = false;
  if (tb == GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE) {
    for (uint32_t i = 0; i < dataNO; i++) {
      if (dataptr[i].field == 12) {
        if (!strcmp(dataptr[i].data.data_char, "Digit"))
          valuetype = true;
      }
      if (dataptr[i].field == 18) {
        if (dataptr[i].data.data_uint8 == 7)
          vartype = true;
      }
    }
  }
  for (uint32_t i = 0; i < dataNO; i++) {
    if (dataptr[i].field < 1)
      continue;
    switch (tb) {
    case GW_PRO_TABLE_IDX_GW_CONFIG_TABLE: {
      gain_operadata(&GatewayConfig[dataptr[i].field - 1], &(dataptr[i].data),
                     temp_sql, sql);
    } break;
    case GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE: {
      gain_operadata(&DeviceList[dataptr[i].field - 1], &(dataptr[i].data),
                     temp_sql, sql);
    } break;
    case GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE: {
      gain_operadata(&SensorConfig[dataptr[i].field - 1], &(dataptr[i].data),
                     temp_sql, sql);
    } break;
    case GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE: {
      if (valuetype && dataptr[i].field == 21) {
        if (vartype)
          snprintf(dataptr[i].data.data_char, GW_PRO_MAX_STRING_LEN_BYTE - 1,
                   "%f", dataptr[i].data.data_float);
        else
          snprintf(dataptr[i].data.data_char, GW_PRO_MAX_STRING_LEN_BYTE - 1,
                   "%ld", dataptr[i].data.data_uint64);
        LOG_DEBUG(OUTPOINT, "datatb:%s\r\n", dataptr[i].data.data_char);
      }
      gain_operadata(&SensorData[dataptr[i].field - 1], &(dataptr[i].data),
                     temp_sql, sql);
    } break;
    }
  }
}

static void gain_operadata(configure_content_t *tbitem, void *data,
                           char *temp_sql, char *sql) {
  char tempstr[100] = {0};
  char tempstr1[100] = {0};
  strcat(sql, tbitem->filedname);
  strcat(sql, ",");
  if (NULL != strstr(tbitem->type, "INT")) {
    snprintf(tempstr, sizeof(tempstr) - 1, "%ld,", *((uint64_t *)data));
  } else if (NULL != strstr(tbitem->type, "TEXT")) {
    if (!strcmp(tbitem->filedname, "macAddr") && strstr(data, "-")) {
      Gain_clientID(data, tempstr1);
      snprintf(tempstr, sizeof(tempstr) - 1, "'%s',", tempstr1);
    } else
      snprintf(tempstr, sizeof(tempstr) - 1, "'%s',", (char *)data);
  } else if (NULL != strstr(tbitem->type, "BOOLEAN")) {
    snprintf(tempstr, sizeof(tempstr) - 1, "%d,", *((uint8_t *)data));
  } else if (NULL != strstr(tbitem->type, "REAL")) {
    snprintf(tempstr, sizeof(tempstr) - 1, "%f,", *((float *)data));
  } else {
    LOG_WARN(OUTPOINT, "Invalid type!\r\n");
    return;
  }
  strcat(temp_sql, tempstr);
}

int gen_sqlite_insert(const char *dbname, char *sql) {
  sqlite3 *db = NULL;
  char *err_msg = NULL;
  int rc = sqlite3_open(dbname, &db);
  if (rc != SQLITE_OK) {
    LOG_WARN(OUTPOINT, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return gw_pro_response_failed;
  }
  if ((rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg)) != SQLITE_OK) {
    LOG_WARN(OUTPOINT, "Sql error is =%s \n", err_msg);
    return gw_pro_response_failed;
  }
  sqlite3_close(db);
  LOG_INFO(OUTPOINT, "Insert table successfully\n");
  return gw_pro_response_success;
}
