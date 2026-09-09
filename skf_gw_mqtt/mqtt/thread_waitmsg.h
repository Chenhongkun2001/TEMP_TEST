#ifndef _THREAD_WAITMSG_H
#define _THREAD_WAITMSG_H
#include "global.h"
#include "ppGW.h"
#ifdef CONFIG_REPORT_FALG
void *wait_uplaod_timer(void *arg);
#else
// void wait_from_sqlite();
void *wait_from_sqlite(void *arg);
#endif
void report_datachange_cloud(gw_pro_data_t *data, uint32_t dataNO);
#endif
