#include "sqlite3.h"
#include <auxiliar.h>
#include <createtable.h>
#include <global.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
DBG_LOCAL_LOG_DEBUG

static char sql[2048] = {0};
static int tb_feild_cb(void *NotUsed, int argc, char **argv, char **azColName);

struct datatable datatab[] = {
    {
        DEVICE_LIST_TABLE,
        DeviceList,
        sizeof(DeviceList) / sizeof(configure_content_t),
    },
    {
        GATEWAY_CONFIGURE_TABLE,
        GatewayConfig,
        sizeof(GatewayConfig) / sizeof(configure_content_t),
    },
    {
        SENSOR_CONFIGURE_TABLE,
        SensorConfig,
        sizeof(SensorConfig) / sizeof(configure_content_t),
    },
    {
        SENSOR_DATA_TABLE,
        SensorData,
        sizeof(SensorData) / sizeof(configure_content_t),
    },
};

static int arrlen = sizeof(datatab) / sizeof(struct datatable);
static char feildstr[2048] = {0};
static bool dbflag = false;
// int main(void) {
int create_sql(const char *dbname) {
  char tmpstr[100] = {0};
  sqlite3 *db = NULL;
  char *err = NULL;
  // Create the database
  LOG_INFO(OUTPOINT, "Try to create a new db\r\n");
  if (F_OK == access(dbname, F_OK)) {
    dbflag = true;
    LOG_INFO(OUTPOINT,
             "The database already exists. You do not need to create it\r\n");
    // return 0;
  }
  LOG_INFO(OUTPOINT, "%s\r\n", dbname);
  int td = sqlite3_open(dbname, &db);
  if (td != SQLITE_OK) {
    LOG_WARN(OUTPOINT, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }
  sleep(2);
  LOG_INFO(OUTPOINT, "Try to create tables\r\n");
  // Create the tables
  for (int i = 0; i < arrlen; i++) {
    // snprintf(sql, sizeof(sql) - 1, "DROP TABLE IF EXISTS %s; CREATE
    // TABLE %s(",
    snprintf(sql, sizeof(sql) - 1, "create table if not exists %s(",
             datatab[i].tablename);
    for (int j = 0; j < datatab[i].filed_size; j++) {
      if (j == 0)
        snprintf(tmpstr, sizeof(tmpstr) - 1, "%s %s",
                 datatab[i].filedinfo[j].filedname,
                 datatab[i].filedinfo[j].type);
      else
        snprintf(tmpstr, sizeof(tmpstr) - 1, ",%s %s",
                 datatab[i].filedinfo[j].filedname,
                 datatab[i].filedinfo[j].type);
      strcat(sql, tmpstr);
    }
    strcat(sql, ");");
    //   create the tables
    // LOG_INFO(OUTPOINT, "creat sql:%s\r\n", sql)
    td = sqlite3_exec(db, (const char *)sql, NULL, NULL, &err);
    if (td != SQLITE_OK) {
      LOG_WARN(OUTPOINT, "SQL error: %s\n", err);
      sqlite3_free(err);
      sqlite3_close(db);
      return 1;
    }
  }
  if (dbflag) {
    for (int i = 0; i < arrlen; i++) {
      memset(feildstr, 0, sizeof(feildstr));
      snprintf(sql, sizeof(sql) - 1, "pragma table_info('%s');",
               datatab[i].tablename);
      td = sqlite3_exec(db, (const char *)sql, tb_feild_cb, NULL, &err);
      if (td != SQLITE_OK) {
        LOG_WARN(OUTPOINT, "SQL error: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 1;
      }
      LOG_INFO(OUTPOINT, "feildstr:%s\r\n", feildstr);
      for (int j = 0; j < datatab[i].filed_size; j++) {
        if (NULL == strstr(feildstr, datatab[i].filedinfo[j].filedname)) {
          LOG_INFO(OUTPOINT, "new feild:%s\r\n",
                   datatab[i].filedinfo[j].filedname);
          snprintf(sql, sizeof(sql) - 1, "ALTER TABLE %s ADD COLUMN %s %s;",
                   datatab[i].tablename, datatab[i].filedinfo[j].filedname,
                   datatab[i].filedinfo[j].type);
          td = sqlite3_exec(db, (const char *)sql, NULL, NULL, &err);
          if (td != SQLITE_OK) {
            LOG_WARN(OUTPOINT, "SQL error: %s\n", err);
            sqlite3_free(err);
            sqlite3_close(db);
            return 1;
          }
        }
      }
    }
  }
  sqlite3_close(db);
  return 0;
}

static int tb_feild_cb(void *NotUsed, int argc, char **argv, char **azColName) {
  for (int i = 0; i < argc; ++i) {
    if (!strcmp(azColName[i], "name")) {
      strcat(feildstr, argv[i]);
      strcat(feildstr, " ");
    }
    // printf("i: %d, %s = %s\n", i, azColName[i], argv[i]);
  }
  return 0;
}
