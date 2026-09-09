#ifndef FILTER_H
#define FILTER_H
#include "global.h"
#include "ppGW.h"
#include "stdint.h"
int sqlite_filter(const char *dbname, char *tb, uint32_t dataNO,
                  gw_pro_sqlite_cond_element_t *filterData,
                  sqlite_query_callback callback);
#endif
