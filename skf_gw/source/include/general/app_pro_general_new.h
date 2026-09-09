/**
 * @file    app_pro_general_new.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-02-11
 * @brief   Header of app_pro_general_new.c
 * @details
 */
#ifndef __APP_PRO_GENERAL_NEW_H__
#define __APP_PRO_GENERAL_NEW_H__

void app_pro_general_new(pid_t pid);

#if (SKF_GW_NEW == 1)
// For calculate the duration of connection
struct gen_con_record_data
{
  // Flag to indicate whether this item is occupied or not
  uint8_t used_flag;
  // Last 3-byte of connected sensor's mac addr (e.g., '110186' of C4BD6A110186)
  // uint8_t mac_addr_l3[3]; 
  // Start time (in ms) of current connection
  uint64_t start_time_ms; 
  // The duration (in ms) of connection setup (sync with that of sensor, currentTime - @p start_time_ms)
  uint32_t con_setup_time_ms;
  // The duration (in ms) of service flag was triggered (currentTime - @p start_time_ms) 
  // uint32_t svr_offset_ms; 
  // The duration (in ms) of starting to collect data
  uint32_t collect_data_offset_ms; 
  // The time of collection data (currentTime - start to collect data time)
  uint32_t collect_data_time_ms;
  // The duration (in ms) of FUOTA (currentTime - start_time_ms)
  // uint32_t ota_offset_ms;
  // The duration (in ms) of the whole connection (currentTime - start_time_ms)
  uint32_t con_lifetime_ms;
};

uint32_t app_pro_gen_allocate_msg_seq_no(void);
struct pro_general_controller *app_pro_gen_get_ctr(void);
#endif

void app_pro_update_dev_list(void);
void monitor_child_process(void);

#endif /* __APP_PRO_GENERAL_NEW_H__ */
