#ifndef __APP_CJSON_H
#define __APP_CJSON_H

#include "gwConfig.h"
#include "sys_def.h"

bool app_cjson_jsonstr_to_gwconfig( gwConfig_t *pt_gw_cfg, const uint8_t*f_path);
bool app_cjson_gwconfig_to_jsonstr( const gwConfig_t *pt_gwconf, uint8_t *pt_fpath);

app_state_t app_cjson_release_chilren_list(gwConfig_t *pt_gwconf);


#endif

