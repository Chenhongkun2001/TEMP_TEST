/**
 * @file    service_interface_mobile_phone.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-04-24
 * @brief   Definition of service interface for mobile phone
 * @details
 */
#ifndef __SERVICE_INTERFACE_MOBILE_PHONE_H__
#define __SERVICE_INTERFACE_MOBILE_PHONE_H__

#include "froto/app_froto_common.h"

#ifndef GW_CONF_FILE_PATH
#define GW_CONF_FILE_PATH "/var/lib/skf-gateway/config/"
#endif /* GW_CONF_FILE_PATH */

struct service_interface *getMobileServiceIntf(void);

extern bool isBLEMobileServerRunning;

void *thandler_BLEMobileServer(void *pt_para);

// Service
struct service_interface
{
  froto_after_parse_t data_upload_processor;
  froto_after_parse_t config_hash_upload_processor;
  froto_after_parse_t specific_config_upload_processor;
  froto_after_parse_t current_version_upload_processor;
  froto_after_parse_t fuota_status_processor;
  froto_after_parse_t image_block_request_processor;
  froto_after_parse_t config_dissem_processor;
  froto_after_parse_t file_notify_dissem_processor;
  froto_after_parse_t image_block_dissem_processor;
  froto_after_parse_t config_retrieve_processor;
  froto_after_parse_t image_block_retrieve_processor;
};

#endif /* __SERVICE_INTERFACE_MOBILE_PHONE_H__ */
