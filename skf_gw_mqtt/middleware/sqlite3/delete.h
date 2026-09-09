#ifndef DELETE_H
#define DELETE_H
#include "stdint.h"
int sqlite_delete(const char *dbname, char *tb, uint32_t keyID,
                  uint32_t dataNum, gw_pro_sqlite_cond_element_t * deleteData);
#endif
