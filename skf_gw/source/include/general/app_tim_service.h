/**
 * @file    app_tim_service.h
 * @author  Tongde (Victor) Yan
 * @date    2025-11-04
 * @brief   Header of app_tim_service.c
 * @details
 */
#ifndef __APP_TIME_SERVICE_H__
#define __APP_TIME_SERVICE_H__

#include "sys_def.h"

// Only for old version. To be deprecated soon ...
#if (SKF_GW_NEW != 1)
bool app_tim_get_ble_reading_timeout_fg(void);
void app_tim_set_ble_reading_timeout_fg(bool fg);
#endif
void *thandler_tim_service(void *pt_para);

#endif /* __APP_TIME_SERVICE_H__ */
