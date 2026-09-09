/**
 * @file    addr.h
 * @author  Xiaoyuan (Sean) Ma (zz2134)
 * @date    2024-04-26
 * @brief   MACROs, definitions for common types for address (e.g., MAC
 * address or IP address)
 * @details
 */
#ifndef __ADDR_H__
#define __ADDR_H__

#include "stdint.h"

typedef struct
{
  union
  {
    uint8_t addrArray[6];
  }addr;
}macAddr_t;

typedef struct
{
  union
  {
    uint8_t addrArray[4];
  }addr;
}ipAddr_t;

#endif /* __ADDR_H__ */
