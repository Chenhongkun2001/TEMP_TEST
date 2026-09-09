#include "sqlite3.h"
#include <auxiliar.h>
#include <global.h>
#include <ppGW.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
DBG_LOCAL_LOG_DEBUG
// static char queryitem[2][2048] = {0}; // 0--filed,1-value
static void constrcut_sql(char *tb, uint32_t keyID, char *sql, uint32_t len);
int sqlite_query(const char *dbname, char *tb, uint32_t keyID,
                 sqlite_query_callback callback) {
  sqlite3 *db = NULL;
  char *err_msg = NULL;
  char sql[512] = {0};
  constrcut_sql(tb, keyID, sql, sizeof(sql) - 1);
  int rc = sqlite3_open(dbname, &db);
  if (rc != SQLITE_OK) {

    LOG_WARN(OUTPOINT, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);

    return gw_pro_response_failed;
  }
  // memset(queryitem, 0, sizeof(queryitem));
  LOG_INFO(OUTPOINT, "sql:%s\r\n", sql);
  rc = sqlite3_exec(db, sql, callback, NULL, &err_msg);
  if (rc != SQLITE_OK) {
    LOG_WARN(OUTPOINT, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);

    return gw_pro_response_failed;
  }
  sqlite3_close(db);
  return gw_pro_response_success;
}

static void constrcut_sql(char *tb, uint32_t keyID, char *sql, uint32_t len) {
  if (keyID == 0) {
    snprintf(sql, len, "select * from %s limit 1;", tb);
    return;
  }
  snprintf(sql, len, "select * from %s where  %s= %d;", tb,
           strcmp(tb, SENSOR_DATA_TABLE) ? "nameId" : "SequenceNumber", keyID);
}

int sqlite_gain_autoid(const char *dbname, char *tb,
                       sqlite_query_callback callback) {
  sqlite3 *db = NULL;
  char *err_msg = NULL;
  char sql[512] = {0};
  snprintf(sql, sizeof(sql) - 1, "select max(%s) as id from %s",
           SensorData[0].filedname, tb);
  int rc = sqlite3_open(dbname, &db);
  if (rc != SQLITE_OK) {
    LOG_WARN(OUTPOINT, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return gw_pro_response_failed;
  }
  // memset(queryitem, 0, sizeof(queryitem));
  LOG_INFO(OUTPOINT, "sql:%s\r\n", sql);
  rc = sqlite3_exec(db, sql, callback, NULL, &err_msg);
  if (rc != SQLITE_OK) {
    LOG_WARN(OUTPOINT, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);

    return gw_pro_response_failed;
  }
  sqlite3_close(db);
  return gw_pro_response_success;
}
