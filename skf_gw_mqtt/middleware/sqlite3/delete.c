#include "sqlite3.h"
#include <global.h>
#include <ppGW.h>
#include <stdio.h>
DBG_LOCAL_LOG_DEBUG
static int delete_item_parse(char *tb, uint32_t dataNO,
                             gw_pro_sqlite_cond_element_t *filterData,
                             char *sql, uint32_t len);
static int constrcut_sql(char *tb, uint32_t keyID, 
                         uint32_t dataNO, 
                         gw_pro_sqlite_cond_element_t *filterData,
                         char *sql, uint32_t len);
int sqlite_delete(const char *dbname, char *tb, uint32_t keyID,
                  uint32_t dataNum, gw_pro_sqlite_cond_element_t * deleteData) {
  sqlite3 *db;
  int ret = 0;
  char *err_msg = NULL;
  char sql[512] = {0};
  if (constrcut_sql(tb, keyID, dataNum, deleteData, sql, sizeof(sql) - 1)) {
    return gw_pro_response_failed;
  }
  if ((ret = sqlite3_open(dbname, &db)) != SQLITE_OK) {
    LOG_WARN(OUTPOINT, "open error!\n");
    return gw_pro_response_failed;
  } else {
    LOG_INFO(OUTPOINT, "open database successfully\n");
  }
  LOG_INFO(OUTPOINT, "sql:%s\r\n", sql);
  if ((ret = sqlite3_exec(db, sql, NULL, NULL, &err_msg)) != SQLITE_OK) {
    LOG_WARN(OUTPOINT, "Sql error is =%s \n", err_msg);
    
    snprintf(sql, sizeof(sql) - 1, "vacuum;");
    LOG_INFO(OUTPOINT, "sql:%s\r\n", sql);
    sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    LOG_WARN(OUTPOINT, "Sql error is =%s \n", err_msg);

    sqlite3_close(db);
    
    return gw_pro_response_failed;
  } else {
    snprintf(sql, sizeof(sql) - 1, "vacuum;");
    LOG_INFO(OUTPOINT, "sql:%s\r\n", sql);
    sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    LOG_WARN(OUTPOINT, "Sql error is =%s \n", err_msg);

    LOG_INFO(OUTPOINT, "Delete successfully\n");
  }

  sqlite3_close(db);
  return gw_pro_response_success;
}

static int constrcut_sql(char *tb, uint32_t keyID, 
                          uint32_t dataNO, gw_pro_sqlite_cond_element_t *filterData,
                          char *sql, uint32_t len) {
  if(dataNO == 0)
  {
    if (keyID == 0)
    {
      snprintf(sql, len, "delete from %s;", tb);
    }else{
      snprintf(sql, len, "delete from %s where nameId = %d;", tb, keyID);
    }
  }else{
    // "keyID" would be ignored as long as dataNo is not zero
    if (strcmp(tb, SENSOR_DATA_TABLE)) {
      LOG_WARN(OUTPOINT, "Delete filter not support for the table\r\n");
      return gw_pro_response_failed;
    }
    return delete_item_parse(tb, dataNO, filterData, sql, len);
  }
  return gw_pro_response_success;
}

static int delete_item_parse(char *tb, uint32_t dataNO,
                             gw_pro_sqlite_cond_element_t *filterData,
                             char *sql, uint32_t len) {
  char tempstr[100] = {0};
  snprintf(sql, len, "delete from %s where ", tb);
  for (uint16_t i = 0; i < dataNO; i++) {
    if (0 == i % 2) {
      if (gw_pro_sqlite_cond_str_equal != filterData[i].condition &&
          gw_pro_sqlite_cond_value_range != filterData[i].condition) {
        LOG_WARN(OUTPOINT, "Invalid parameter for delete filter!\r\n");
        return gw_pro_response_failed;
      }
    } else {
      if (gw_pro_sqlite_cond_or == filterData[i].condition) {
        strcat(sql, " or ");
      } else if (gw_pro_sqlite_cond_and == filterData[i].condition) {
        strcat(sql, " and ");
      } else {
        LOG_WARN(OUTPOINT, "Invalid parameter for delete filter!\r\n");
        return gw_pro_response_failed;
      }
      continue;
    }
    switch (filterData[i].field) {
    case 2: {
      snprintf(tempstr, sizeof(tempstr) - 1, "nameId >= %d and nameId <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 7: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "measureseqno >= %d and measureseqno <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 8: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "sampletime >= %ld and sampletime <= %ld",
               filterData[i].condParam.range_uint64.min,
               filterData[i].condParam.range_uint64.max);
    } break;
    case 9: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "ReceivedTimestamp >= %ld and ReceivedTimestamp <= %ld",
               filterData[i].condParam.range_uint64.min,
               filterData[i].condParam.range_uint64.max);
    } break;
    case 10: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "measurement >= %d and measurement <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 11: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "DataType >= %d and DataType <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 13: {
      snprintf(tempstr, sizeof(tempstr) - 1, "range >= %d and range <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 14: {
      snprintf(tempstr, sizeof(tempstr) - 1, "unit >= %d and unit <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 15: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "measurelengthsample >= %d and measurelengthsample <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 16: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "totaldatalengthsample >= %d and totaldatalengthsample <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 17: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "dimension >= %d and dimension <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 18: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "dataformat >= %d and dataformat <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 19: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "sampleperiods >= %d and sampleperiods <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 20: {
      snprintf(tempstr, sizeof(tempstr) - 1, "encryp >= %d and encryp <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 22: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "sample_rate >= %f and sample_rate <= %f",
               filterData[i].condParam.range_float.min,
               filterData[i].condParam.range_float.max);
    } break;
    case 23: {
      snprintf(tempstr, sizeof(tempstr) - 1, "product >= %d and product <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    case 24: {
      snprintf(tempstr, sizeof(tempstr) - 1, "sensor >= %d and sensor <= %d",
               filterData[i].condParam.range_uint32.min,
               filterData[i].condParam.range_uint32.max);
    } break;
    // boolean
    case 25: {
      snprintf(tempstr, sizeof(tempstr) - 1,
               "sent =%s order by SequenceNumber desc limit 400",
               filterData[i].condParam.range_uint32.min == 0 ? "false"
                                                             : "true");
    } break;
      // string
    case 3: {
      snprintf(tempstr, sizeof(tempstr) - 1, "BLESensorName = '%s'",
               filterData[i].condParam.string_value.s);
    } break;
    case 4: {

      snprintf(tempstr, sizeof(tempstr) - 1, "macAddr = '%s'",
               filterData[i].condParam.string_value.s); // COLLATE NOCASE
      // snprintf(tempstr, sizeof(tempstr) - 1, "macAddr = '%s' COLLATE NOCASE",
      //          filterData[i].condParam.string_value.s); // COLLATE NOCASE
    } break;
    case 5: {
      snprintf(tempstr, sizeof(tempstr) - 1, "type = '%s'",
               filterData[i].condParam.string_value.s);
    } break;
    case 6: {
      snprintf(tempstr, sizeof(tempstr) - 1, "manufacturer = '%s'",
               filterData[i].condParam.string_value.s);
    } break;
    case 12: {
      snprintf(tempstr, sizeof(tempstr) - 1, "Format = '%s'",
               filterData[i].condParam.string_value.s);
    } break;
    case 21: {
      snprintf(tempstr, sizeof(tempstr) - 1, "value = '%s'",
               filterData[i].condParam.string_value.s);
    } break;
    }
    strcat(sql, tempstr);
  }
  strcat(sql, ";");
  return gw_pro_response_success;
}
