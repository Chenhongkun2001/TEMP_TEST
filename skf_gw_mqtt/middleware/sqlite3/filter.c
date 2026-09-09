#include "sqlite3.h"
#include "auxiliar.h"
#include "global.h"
#include "ppGW.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
DBG_LOCAL_LOG_DEBUG
#if 1
static int constrcut_sql(char *tb, uint32_t dataNO,
                         gw_pro_sqlite_cond_element_t *filterData, char *sql,
                         uint32_t len);
static int filter_item_parse(char *tb, uint32_t dataNO,
                             gw_pro_sqlite_cond_element_t *filterData,
                             char *sql, uint32_t len);
int sqlite_filter(const char *dbname, char *tb, uint32_t dataNO,
                  gw_pro_sqlite_cond_element_t *filterData,
                  sqlite_query_callback callback) {
  sqlite3 *db = NULL;
  char *err_msg = NULL;
  char sql[2048] = {0};
  if (constrcut_sql(tb, dataNO, filterData, sql, sizeof(sql) - 1)) {
    return gw_pro_response_failed;
  }
  int rc = sqlite3_open(dbname, &db);
  if (rc != SQLITE_OK) {

    LOG_INFO(OUTPOINT, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);

    return gw_pro_response_failed;
  }
  LOG_INFO(OUTPOINT, "sql:%s\r\n", sql);
  rc = sqlite3_exec(db, sql, callback, NULL, &err_msg);
  if (rc != SQLITE_OK) {

    LOG_INFO(OUTPOINT, "Failed to select data\n");
    LOG_INFO(OUTPOINT, "SQL error: %s\n", err_msg);

    sqlite3_free(err_msg);
    sqlite3_close(db);

    return gw_pro_response_failed;
  }
  sqlite3_close(db);
  return gw_pro_response_success;
}
static int constrcut_sql(char *tb, uint32_t dataNO,
                         gw_pro_sqlite_cond_element_t *filterData, char *sql,
                         uint32_t len) {
  if (dataNO == 0) {
    // snprintf(sql, sizeof(sql) - 1, "select nameID from %s;", tb);
    snprintf(sql, GW_PRO_MAX_STRING_LEN_BYTE, "select nameID from %s;", tb);  // fix the length issue. only partial of sql cmd being copied
    return gw_pro_response_success;
  }
  switch (filterData[0].condition) {
  case gw_pro_sqlite_cond_latest_one: {
    if (strcmp(tb, SENSOR_DATA_TABLE)) {
      return gw_pro_response_failed;
    }
    snprintf(sql, len,
             "select SequenceNumber from %s order by sampletime desc limit 1;",
             tb);
    return gw_pro_response_success;
  } break;
  case gw_pro_sqlite_cond_earliest_one: {
    if (strcmp(tb, SENSOR_DATA_TABLE)) {
      return gw_pro_response_failed;
    }
    snprintf(sql, len,
             "select SequenceNumber from %s order by sampletime asc limit 1;",
             tb);
    return gw_pro_response_success;
  } break;
  case gw_pro_sqlite_cond_all: {
    if (strcmp(tb, SENSOR_DATA_TABLE))
      snprintf(sql, len, "select nameId from %s;", tb);
    else
      snprintf(sql, len, "select SequenceNumber from %s;", tb);
    return gw_pro_response_success;
  } break;
  case gw_pro_sqlite_cond_value_not_equal:
  case gw_pro_sqlite_cond_str_equal:
    if (strcmp(tb, SENSOR_DATA_TABLE)) {
      snprintf(sql, len, "select nameId from %s where macAddr = '%s'", tb,
               filterData[0].condParam.string_value.s);
      // snprintf(sql, len, "select nameId from %s where macAddr = '%s' COLLATE
      // NOCASE", tb,
      //          filterData[0].condParam.string_value.s);
      break;
    }
  case gw_pro_sqlite_cond_value_range: {
    if (strcmp(tb, SENSOR_DATA_TABLE)) {
      return gw_pro_response_failed;
    }
    return filter_item_parse(tb, dataNO, filterData, sql, len);

  } break;
  default:
    LOG_WARN(OUTPOINT, "Invalid paramenter for filter!\r\n");
    break;
  }

  return gw_pro_response_success;
}

static int filter_item_parse(char *tb, uint32_t dataNO,
                             gw_pro_sqlite_cond_element_t *filterData,
                             char *sql, uint32_t len) {
  char tempstr[100] = {0};
  bool orderFg = false;
  snprintf(sql, len, "select SequenceNumber from %s where ", tb);
  for (uint16_t i = 0; i < dataNO; i++) {
    if (0 == i % 2) {
      if (gw_pro_sqlite_cond_str_equal != filterData[i].condition &&
          gw_pro_sqlite_cond_value_range != filterData[i].condition &&
          gw_pro_sqlite_cond_value_not_equal != filterData[i].condition) {
        LOG_WARN(OUTPOINT, "Invalid paramenter for filter!\r\n");
        return gw_pro_response_failed;
      }
    } else {
      if (gw_pro_sqlite_cond_or == filterData[i].condition) {
        strcat(sql, " or ");
      } else if (gw_pro_sqlite_cond_and == filterData[i].condition) {
        strcat(sql, " and ");
      } else {
        LOG_WARN(OUTPOINT, "Invalid paramenter for filter!\r\n");
        return gw_pro_response_failed;
      }
      continue;
    }
    if((filterData[i].condition == gw_pro_sqlite_cond_str_equal) ||
       (filterData[i].condition == gw_pro_sqlite_cond_value_range))
    {
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
                "sent = %s",
                filterData[i].condParam.range_uint32.min == 0 ? "false"
                                                              : "true");
        orderFg = true;
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
    }else if (filterData[i].condition == gw_pro_sqlite_cond_value_not_equal) {
      switch (filterData[i].field) {
      case 11: {
        snprintf(tempstr, sizeof(tempstr) - 1,
                "DataType != %d",
                filterData[i].condParam.range_uint32.min);
      } break;
      }
    }
    strcat(sql, tempstr);
  }
  if(orderFg == true)
  {
    strcat(sql, " order by SequenceNumber desc limit 400;");
  }else{
    strcat(sql, ";");
  }
  return gw_pro_response_success;
}
#endif
