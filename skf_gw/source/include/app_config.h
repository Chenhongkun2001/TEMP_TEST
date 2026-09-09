/**
 * @file    app_config.h
 * @author  Victor Yan
 * @date    2024-10-17
 * @brief   Define the UUIDs used by BLE UART
 * @details
 */
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

// Debug configuration
#ifndef DBG_CT
#define DBG_CT 1  // Set to zero when you want to disable debug support
#endif

// BLE GATT UUID
#ifndef NORDIC_UART_SERVICE_UART
#define NORDIC_UART_SERVICE_UART "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#endif
// TX to the client side (from sensor to the gateway)
#ifndef NORDIC_UART_CHARAC_TX_UUID
#define NORDIC_UART_CHARAC_TX_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#endif
// RX from the client side (from the gateway to sensor)
#ifndef NORDIC_UART_CHARAC_RX_UUID
#define NORDIC_UART_CHARAC_RX_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#endif
#ifndef NORDIC_UART_DESC_TX_UUID
#define NORDIC_UART_DESC_TX_UUID "00002902-0000-1000-8000-00805f9b34fb"
#endif

#ifndef CONFIG_BLE_CONNECTION_MAX_NUM
// Notes: make sure it's power of 2(2, 4, 8 ... at most 16 so far) please	
#define CONFIG_BLE_CONNECTION_MAX_NUM (4)
#endif
#endif /* __APP_CONFIG_H__ */
