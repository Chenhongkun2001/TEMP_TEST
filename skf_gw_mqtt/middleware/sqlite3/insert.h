#ifndef INSERT_H
#define INSERT_H
#include "ppGW.h"
#include "stdint.h"
int sqlite_insert(const char *dbname, uint32_t dataNO, uint32_t keyID,
                  gw_pro_data_t *dataptr, uint32_t tbindex, char *tb);
int gen_sqlite_insert(const char *dbname, char *sql);
#endif
