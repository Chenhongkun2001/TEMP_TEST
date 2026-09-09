#ifndef HTTP_H
#define HTTP_H
#include "stdint.h"

typedef struct _downloadparameter {
  uint8_t argno;
  uint32_t sensor_firmware_version;
  uint32_t downloadtype;
  char local_topic[50];
  char folder_dir[100];
} downloadparameter_t;

int enable_http_task(uint8_t argno);
#endif
