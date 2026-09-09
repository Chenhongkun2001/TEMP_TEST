/**
 * @file    gwConfig.h
 * @author  Xiaoyuan (Sean) Ma (zz2134)
 * @date    2024-04-26
 * @brief   MACROs, definitions for gwConfig
 * @details
 */
#ifndef __GWCONFIG_H__
#define __GWCONFIG_H__

#include "stdint.h"
#include "stdbool.h"
#include "addr.h"
#include "list.h"
#include "ppGW.h"

#ifndef GWCONFIG_CHILDREN_USING_LINKED_LIST
#define GWCONFIG_CHILDREN_USING_LINKED_LIST (1)
#endif /* GWCONFIG_CHILDREN_USING_LINKED_LIST */

#ifndef GW_MAX_CHILDREN_NUM
#define GW_MAX_CHILDREN_NUM (40)
#endif /* GW_MAX_CHILDREN_NUM */

// Gateway Mode
#define GWCONFIG_GWCONFIG_GATEWAYMODE_JUST_FORWARDING_BEACON (0)
#define GWCONFIG_GWCONFIG_GATEWAYMODE_REGULAR (1)
// BLE Tx Power
#define GWCONFIG_BLECONFIG_TXPOWER_0_DBM (0)
#define GWCONFIG_BLECONFIG_TXPOWER_NEG_4_DBM (-4)
#define GWCONFIG_BLECONFIG_TXPOWER_NEG_8_DBM (-8)
// BLE Antenna Selection
#define GWCONFIG_BLECONFIG_BUILTIN_ANTENNA (0)
#define GWCONFIG_BLECONFIG_EXTERNAL_ANTENNA (1)
// Timesetting method: via BLE or NTP server
#define GWCONFIG_TIMECONFIG_TIMESETTING_VIA_BLE (0)
#define GWCONFIG_TIMECONFIG_TIMESETTING_VIA_NTP (1)
// MQTT Security type
#define GWCONFIG_MQTTCONFIG_SECURITYTYPE_NONE (0)
#define GWCONFIG_MQTTCONFIG_SECURITYTYPE_USERNAME_PSW (1)
#define GWCONFIG_MQTTCONFIG_SECURITYTYPE_TLS (2)
#define GWCONFIG_MQTTCONFIG_SECURITYTYPE_USERNAME_PSW_TLS (3)


typedef struct
{
  // GWCONFIG_BLECONFIG_TXPOWER_0_DBM,
  // GWCONFIG_BLECONFIG_TXPOWER_NEG_4_DBM, or
  // GWCONFIG_BLECONFIG_TXPOWER_NEG_8_DBM
  int8_t ble1TxPower;  // The tx power (dBm) of BLE1
  int8_t ble2TxPower;  // The tx power (dBm) of BLE2
  // GWCONFIG_BLECONFIG_BUILTIN_ANTENNA or
  // GWCONFIG_BLECONFIG_EXTERNAL_ANTENNA
  uint8_t ble1Antenna;  // The antenna of BLE1
  uint8_t ble2Antenna;  // The antenna of BLE2
  uint32_t bleDuplicatedDrop_ms;  // The time duration (in ms) for
                                  // duplicated content dropping
}gwConfig_gwConfig_bleConfig_t;

typedef struct
{
  bool needTimestampForRecBeacon;  // Whether timestamp required for the
                                   // received beacon
  uint8_t timeSetting;  // How to get time,
                        // GWCONFIG_TIMECONFIG_TIMESETTING_VIA_BLE or
                        // GWCONFIG_TIMECONFIG_TIMESETTING_VIA_NTP
  char timeNtpUrl[256];  // NTP URL for time synchronization (only used
                         // when TimeSetting is 1)
}gwConfig_gwConfig_timeConfig_t;

typedef struct
{
  uint8_t mqttSecurityType;  // The MQTT broker security type:
                             // GWCONFIG_MQTTCONFIG_SECURITYTYPE_NONE,
                             // GWCONFIG_MQTTCONFIG_SECURITYTYPE_USERNAME_PSW,
                             // GWCONFIG_MQTTCONFIG_SECURITYTYPE_TLS, or
                             // GWCONFIG_MQTTCONFIG_SECURITYTYPE_USERNAME_PSW_TLS
  char MQTTHost[256];
  uint32_t MQTTPort;
}gwConfig_gwConfig_mqttConfig_t;

typedef struct
{
  bool DHCPEnable;  // If using static IP address, DHCPEnable should be set
                    // to false.
  ipAddr_t staticIPAddr;  // The static IP address (if DHCP is disable)
  ipAddr_t netMask;  // The net mask (if DHCP is disable)
  ipAddr_t dnsServer;  // The dns server (would be override if given)
  ipAddr_t gatewayAddr; // The gateway address
}gwConfig_gwConfig_ipConfig_t;

typedef struct
{
  char type[50];  // The product name/type
  char manufacturer[10];  // The manufacturer of the product
  char name[50];  // A readable name of the gateway
  uint32_t nameId;  // A 4-byte hash ID to avoid the same name. nameId is
                    // the index.
  char bleName[50];  // BLE name of the sensor (not mandatory)
  macAddr_t macAddr;  // BLE MAC address of the sensor (separated by dashes
                      // in JSON).
#if (ENABLE_MODBUS_FEATURE == 1)
  char description[GW_PRO_MAX_STRING_LEN_BYTE];  // The description field of device in "CH00-AD0000-L00-PRS" format
#endif
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  struct list_head childrenNode;
#endif
}gwConfig_gwConfig_children_t;

#if (ENABLE_MODBUS_FEATURE == 1)
typedef struct
{
  uint32_t baudrateSlave;  // baudrate of slave(gateway)
  uint32_t slaveAddrSlave;  // salve address
  uint8_t paritySlave;  // parity of serial port
  uint8_t stopSlave;  // stop bit of serial port
  uint8_t registerMode;  // mode of registers in gateway, extensible or efficient
}gwConfig_gwConfig_modbusConfig_t;
#endif

typedef struct
{
  char type[50];  // The product name/type
  char manufacturer[10];  // The manufacturer of the product
  char name[50];  // A readable name of the gateway
  uint32_t nameId;  // A 4-byte hash ID to avoid the same name. nameId is
                    // the index.
  macAddr_t macAddr;  // BLE MAC address of the gateway (separated by
                      // dashes in JSON).
  uint8_t gatewayMode;  // The working mode of the gateway,
                        // GWCONFIG_GWCONFIG_GATEWAYMODE_JUST_FORWARDING_BEACON
                        // or GWCONFIG_GWCONFIG_GATEWAYMODE_REGULAR
  gwConfig_gwConfig_bleConfig_t bleConfig;
  gwConfig_gwConfig_timeConfig_t timeConfig;
  gwConfig_gwConfig_mqttConfig_t mqttConfig;
  gwConfig_gwConfig_ipConfig_t ipConfig;
#if (ENABLE_MODBUS_FEATURE == 1)
  gwConfig_gwConfig_modbusConfig_t modbusConfig;
  char description[GW_PRO_MAX_STRING_LEN_BYTE];  // The description field of device in "CH00-AD0000-L00-PRS" format
#endif
  uint16_t childrenNumber;  // The number of sensors managed by the
                            // gateway
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  struct list_head childrenList;
#else
  gwConfig_gwConfig_children_t children[GW_MAX_CHILDREN_NUM];
#endif
}gwConfig_gwConfig_t;

typedef struct
{
  uint8_t version;
  uint64_t lastEditTimeS;  // In second.
  gwConfig_gwConfig_t gwConfig;
}gwConfig_t;

#endif /* __GWCONFIG_H__ */
