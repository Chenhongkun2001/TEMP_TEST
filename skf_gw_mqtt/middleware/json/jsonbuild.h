#ifndef JSONBUILD_H
#define JSONBUILD_H
#include "cJSON.h"
#include <auxiliar.h>
#include <ppGW.h>
#include <stdint.h>

// int json_build_gwConfig(char *json);
// int json_build_senConfig(char *json);
#if (ENABLE_MODBUS_FEATURE == 1)
void gw_config_partial(gw_pro_data_t *data, uint32_t dataNO, uint32_t tb_name,
                       cJSON *root, cJSON *child, cJSON *blecfg, cJSON *gwcfg,
                       cJSON *timecfg, cJSON *mqttcfg, cJSON *ipcfg, cJSON *modbuscfg);
#else
void gw_config_partial(gw_pro_data_t *data, uint32_t dataNO, uint32_t tb_name,
                       cJSON *root, cJSON *child, cJSON *blecfg, cJSON *gwcfg,
                       cJSON *timecfg, cJSON *mqttcfg, cJSON *ipcfg);
#endif
void gw_config_build(gw_pro_message_header_t *pdata, char *json,
                     uint32_t json_len);
void sensor_config_build(gw_pro_data_t *data, uint32_t dataNO, char *json,
                         uint32_t len);

#endif
