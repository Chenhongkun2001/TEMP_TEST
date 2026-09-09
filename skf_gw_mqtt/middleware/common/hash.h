#ifndef __HASH_H__
#define __HASH_H__
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "ppGW.h"

uint32_t util_bullet_elfhash(
  const uint8_t *inputData,
  uint32_t length);
uint32_t calculate_config_hash_from_db(
  gw_pro_command_sqlite_query_item_response_t *query_response,
  char *clientID);

#endif /* __HASH_H__ */
