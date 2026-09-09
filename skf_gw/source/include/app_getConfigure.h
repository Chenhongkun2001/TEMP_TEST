/**
 * @file    app_getConfigure.h
 * @author  Xiaoyuan (Sean) Ma (zz2134)
 * @date    2024-05-31
 * @brief   MACROs, definitions for APIs to get GW configurations
 * @details
 */
#ifndef __APP_GETCONFIGURE_H__
#define __APP_GETCONFIGURE_H__

#include "sys_def.h"

bool getChildMACList(
  struct bt_addr *list,
  uint32_t list_sz,
  uint32_t * childNumber);

#endif /* __APP_GETCONFIGURE_H__ */
