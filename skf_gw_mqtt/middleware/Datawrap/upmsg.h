#ifndef UPMSG_H
#define UPMSG_H
#include "common.h"
#include "datawrap.h"

#define TACK_TIMER_PERIOD (2000) // ms

void Simple_upload(pb_size_t which_msg,
                   SKFChina_App_AppMessage SKF_BullGateway_tx, char *send_topic,
                   char *devid);
void Simple_bulk_upload(pb_size_t which_msg,
                        SKFChina_App_AppMessage SKF_BullGateway_tx,
                        char *send_topic, char *devid);
void Simple_request(pb_size_t which_msg, SKFChina_req_method_t req_method);
uint32_t Calculate_config_hash();
#endif
