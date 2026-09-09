/**
 * @file    unittest.h
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-02-12
 * @brief   MACROs and delecartions of unit test
 * @details
 */
#ifndef __UNITTEST_H__
#define __UNITTEST_H__

#define UNITTEST_POWER_ON_POWER_OFF_BLE (1)
#define UNITTEST_SCAN_ON_SCAN_OFF_BLE (2)
#define UNITTEST_SCAN_BASED_ON_LIST_BLE (3)
#define UNITTEST_SCAN_AND_PARSE_INSIGHT_T (4) // Scanning and parse Insight-T
#define UNITTEST_CONNECTION_DISCONNECTION (5) // Switch between BT connection and disconnection
#define UNITTEST_ADVERTISING (6) // BLE advertisement test
#define UNITTEST_IOKEY_STAT_MACHINE (7) // IO-key(button) trigger statue change  
#define UNITTEST_MULTI_CONNECTION (8) // Set up multi-connection (at most 4)  
#define UNITTEST_FROTO_DECODER (9) // Froto Decoder
#define UNITTEST_FROTO_ENCODER (10) // Froto Encoder
#define UNITTEST_FROTO_CODEC (11) // Froto Codec
#define UNITTEST_BLE_UART (12) // BLE UART with Froto codec (communication with sensor)
#define UNITTEST_BLE_SERVER	(13) // BLE Server with Froto codec (communication with cellphone)
#define UNITTEST_MULTI_CON_SCAN_AND_PARSE_INSIGHT_T (14) // merge UNITTEST_SCAN_AND_PARSE_INSIGHT_T and UNITTEST_MULTI_CONNECTION
#define UNITTEST_UNIVERSE (15) // Comprehensive test (including completed functions in products)

#endif /* __UNITTEST_H__ */
