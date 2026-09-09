#ifndef __APP_PRO_GENERAL_H
#define __APP_PRO_GENERAL_H
#include "app_general_def.h"


void app_pro_general(void);

struct waiting_queue_controller *app_pro_gen_get_wq_ipc_msg_ctr(void);

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
// TODO : need update to return count later ? try to keep the same first
bool app_pro_gen_get_connection_state(struct pro_general_controller*pt_gen_ctr, u_int8_t idx);
#else
bool app_pro_gen_get_connection_state(struct pro_general_controller*pt_gen_ctr);
#endif
void app_pro_gen_set_gateway_idstr(struct pro_general_controller*pt_gen_ctr, const uint8_t *pt_idstr);
uint8_t * app_pro_gen_get_gateway_idstr(struct pro_general_controller*pt_gen_ctr);

#if (SKF_GW_NEW != 1)
uint32_t app_pro_gen_allocate_msg_seq_no(void);
struct pro_general_controller *app_pro_gen_get_ctr(void);
#endif

app_state_t app_pro_gen_update_gwconf_except_childlist(gwconf_and_dev_t *pt_gwconf_and_dev, gwConfig_t *pt_gwcfg);
app_state_t app_pro_gen_update_gwconf_childlist(gwconf_and_dev_t *pt_gwconf_and_dev, gwConfig_t *pt_gwcfg);
enum bluetooth_role app_pro_gen_get_bluetooth_role( struct pro_general_controller*pt_ctr);
void app_pro_gen_get_active_client_id_str(uint8_t*pt_active_id_str_buf, uint8_t kp_bufsz);


enum sensortype app_pro_gen_get_active_sensor_type(const struct pro_general_controller*pt_gen_ctr , const uint8_t *pt_mac_str);
enum sensortype app_pro_gen_get_active_sensor_type_v2(void);


#endif

