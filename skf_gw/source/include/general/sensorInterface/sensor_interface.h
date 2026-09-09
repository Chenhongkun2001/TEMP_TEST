/**
 * @file    sensor_interface.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Definition of sensor interface
 * @details
 */
#ifndef __SENSOR_INTERFACE_H__
#define __SENSOR_INTERFACE_H__

#include <stdint.h>
#include <stdbool.h>

typedef bool ( *si_exchange_general_info_t )(
  struct sensor_interface *si,
  void *user_data,
  uint8_t *err_code);
typedef bool ( *si_collect_data_t )(
  struct sensor_interface *si,
  void *user_data,
  uint8_t *err_code);
typedef bool ( *si_send_command_t )(
  struct sensor_interface *si,
  void *user_data,
  uint8_t *err_code);
typedef bool ( *si_update_config_t )(
  struct sensor_interface *si,
  void *user_data,
  uint8_t *err_code);
typedef bool ( *si_fuota_t )(
  struct sensor_interface *si,
  void *user_data,
  uint8_t *err_code);
typedef bool ( *send_msg_t )(
  struct sensor_interface *si,
  void *data_to_be_sent,
  uint32_t dataLen,
  uint8_t *err_code);
typedef bool ( *read_msg_t )(
  struct sensor_interface *si,
  void *read_data,
  uint32_t *dataLen,
  uint8_t *err_code);

struct sensor_interface
{
  uint8_t sensorAddr[6]; // MAC address of sensor
  uint8_t gwAddr[6];     // MAC address of gateway
  uint32_t sensorNameId;
  uint8_t sensorManufacturer[10]; // Identical to the definition of
                                  // manufacturer in app_db.h
  uint8_t sensorPrductType[50];   // Identical to the definition of type in
                                  // app_db.h
  uint8_t sensorBLEName[15];
  bool hwTypeValid;
  uint8_t hwType;
  bool hwVerValid;
  uint32_t hwVer;
  bool fwVerValid;
  uint32_t fwVer;
  bool configHashValid;
  uint32_t configHash;
  bool configHashDBValid;
  uint32_t configHashDB;
  bool lastEditTimeValid;
  uint64_t lastEditTime;
  bool lastEditTimeDBValid;
  uint64_t lastEditTimeDB;
  void *channel;
  bool imageFwValid;
  uint32_t imageFwVer;
  uint8_t imageFwVerPath[MAX_FPATH];
  void * userData;
  si_exchange_general_info_t exchangeGeneralInfo; // e.g., To set time
  si_collect_data_t collectData;
  si_send_command_t sendCmd;
  si_update_config_t updateConfig;
  si_fuota_t fuota;
  send_msg_t send;
  read_msg_t read;
};

#endif /* __SENSOR_INTERFACE_H__ */
