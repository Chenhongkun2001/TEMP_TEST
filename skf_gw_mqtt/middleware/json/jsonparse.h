#ifndef JSONPARSE_H
#define JSONPARSE_H
#include "stdint.h"

struct disabledGroup {
  uint16_t GW[50];
  uint16_t PredictSensor[50];
  uint16_t PredictSensorPro[50];
  uint16_t InsightT[50];
  uint16_t gwCnt;
  uint16_t predictSensorCnt;
  uint16_t predictSensorProCnt;
  uint16_t insightTCnt;
};

#if 1
void parse_config(const char *filename);
void gwconfig_parse(char *input_json);
void sensorconfig_parse(char *input_json);
int config_file_parse(const char *filename, char **jsonStr);
void magicfile_parse(char *input_json);
#else
int json_parse(char *value, char *sql, uint8_t file);
int test();
#endif
#endif
