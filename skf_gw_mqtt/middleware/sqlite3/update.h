#ifndef UPDATE_H
#define UPDATE_H
#include "ppGW.h"
#include "stdint.h"
int sqlite_update(const char *dbname, uint32_t dataNO, uint32_t keyID,
                  gw_pro_data_t *dataptr, uint32_t tbindex, char *tb);
#endif
