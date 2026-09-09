/**
 * @file    jsonBuild.h
 * @author  Xiaoyuan (Sean) Ma (zz2134)
 * @date    2024-04-26
 * @brief   MACROs, definitions for gwConfig.json/sensorConfig.json
 * encoders
 * @details
 */
#ifndef __JSONBUILD_H__
#define __JSONBUILD_H__

#include "gwConfig.h"
#include "sensorConfig.h"
#include "jsonName.h"

void generateGwConfigJson(
  gwConfig_t *gwConfig,
  char *outputJson,
  int8_t *ret);
void generateSensorConfigJson(
  sensorConfig_t *sensorConfig,
  char *outputJson,
  int8_t *ret);
#endif /* __JSONBUILD_H__ */
