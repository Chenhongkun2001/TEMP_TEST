/**
 * @file    jsonParse.h
 * @author  Xiaoyuan (Sean) Ma (zz2134)
 * @date    2024-04-26
 * @brief   MACROs, definitions for gwConfig.json/sensorConfig.json
 * decoders
 * @details
 */
#ifndef __JSONPARSE_H__
#define __JSONPARSE_H__

#include "gwConfig.h"
#include "sensorConfig.h"
#include "jsonName.h"

void parseGwConfigJson(
  char *inputJson,
  gwConfig_t *gwConfig,
  int8_t *ret);
void parseSensorConfigJson(
  char *inputJson,
  sensorConfig_t *sensorConfig,
  int8_t *ret);
#endif /* __JSONPARSE_H__ */
