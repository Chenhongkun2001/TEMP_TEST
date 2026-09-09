#ifndef QUERY_H
#define QUERY_H
#include "global.h"
#include "stdint.h"
// extern char queryitem[2][2048]; // 0--filed,1-value
// int sqlite_query(const char *dbname, const char *sql);
int sqlite_query(const char *dbname, char *tb, uint32_t keyID,
                 sqlite_query_callback callback);
                 int sqlite_gain_autoid(const char *dbname, char *tb,
                       sqlite_query_callback callback);
#endif
