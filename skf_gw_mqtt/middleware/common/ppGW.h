// Some definitions for the process protocol used by the gateway.
// Version: 1.0.0
// Changelog:
// 2024-04-23 | Xiaoyuan (Sean) Ma | Draft
// 2024-05-21 | Xiaoyuan (Sean) Ma | Add gw_pro_sqlite_cond_all to
//                                   gw_pro_sqlite_condition_t.
// Last edit time: 2024-05-21

#ifndef __FLEGO_PROCESS_PROTOCOL_FOR_LINUX_GW__
#define __FLEGO_PROCESS_PROTOCOL_FOR_LINUX_GW__
#include "stdint.h"
// Table indexes:
// Table index of the Gateway Configuration Table
#ifndef GW_PRO_TABLE_IDX_GW_CONFIG_TABLE
#define GW_PRO_TABLE_IDX_GW_CONFIG_TABLE (1)
#endif
// Table index of the Mnged. Device List Table
#ifndef GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE
#define GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE (2)
#endif
// Table index of the Mnged. Sensor Config. Table
#ifndef GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE
#define GW_PRO_TABLE_IDX_MNGED_SENSOR_CONFIG_TABLE (3)
#endif
// Table index of the Sensor Data Table
#ifndef GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE
#define GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE (4)
#endif

// Flag to indicate command or response
#define GW_PRO_COMRESPFG_COMMAND (1)
#define GW_PRO_COMRESPFG_RESPONSE (2)

#define GW_PRO_MAX_STRING_LEN_BYTE (70)   //50
#define GW_PRO_MAX_DATA_FIELD_PER_OPERATION                                    \
  (100) // The max number of
        // field in the header
#define GW_PRO_MAX_DATA_ITEM_PER_OPERATION (2000)
#define GW_PRO_CUSTOMED_MSG_LEN_BYTE (1024)

// Considering the name space, all the types and enums starts with gw_pro
// (representing the process protocol used by the gateway).

typedef enum {
  gw_pro_process_ble = 0, // From the BLE process
  gw_pro_process_mqtt,    // From the MQTT process
  gw_pro_process_sqlite,  // From the Database process
  gw_pro_process_amount,  // Indicate how many types supported
} gw_pro_process_type_t;

// All the command is in a block manner. For instance, the database process
// received a command to delete an item in the database, then the database
// would delete the item, then reply the command.
typedef enum gw_pro_command_t {
  // Update the database: Update item in the database (to check whether
  // there exists the given item first and update the item or add an item).
  // Parameters (gw_pro_command_sqlite_update_item_param_t): An exact item
  // index should be given, which means only one item can be updated for
  // each update item command. The content to be updated should be included
  // in the payload.
  // Returns: If update (no matter the item existed) successfully, return
  // response_success; otherwise, return response_failed.
  gw_pro_command_sqlite_update_item = 0,
  // Query an item from the database: Readback an item in the database.
  // Parameters (gw_pro_command_sqlite_query_item_param_t): An exact item
  // index should be given, which means only one item can be readed for
  // each query item command.
  // Returns (gw_pro_command_sqlite_query_item_response_t): If the inquired
  // item exists data avaiable, return response_success; otherwise, return
  // response_failed. If read back successfully, the payload should be
  // included the inquired data.
  gw_pro_command_sqlite_query_item,
  // Filter items in the database: Return the filtered items (i.e., the
  // indexes of items) in the database.
  // Parameters (gw_pro_command_sqlite_filter_param_t): A filter condition
  // should be given.
  // Returns: If the filter applied successfully (no matter whether there
  // exists the items meeting the condition), return response_success;
  // otherwise, return response_failed. If read back successfully, the
  // payload should be included the indexes of the inquired items.
  gw_pro_command_sqlite_filter,
  // Delete items in the database: Delete the given items (if exsited).
  // Parameters (gw_pro_command_sqlite_delete_item_param_t): An exact item
  // index should be given, which means only one item can be deleted for
  // each delete item command.
  // Returns (gw_pro_command_sqlite_filter_response_t): If the given item
  // does not exist (no matter the item is deleted or does not exist at
  // all), return response_success; otherwise, return response_failed.
  gw_pro_command_sqlite_delete_item,
  // Report there are new modifications in the database: This is a command
  // from the database to other processes to notify that there are changes
  // in the database.
  // Parameters (gw_pro_command_report_new_modification_param_t): the
  // changed item's index, the operation (i.e., update or delete), and the
  // timestamp.
  // Returns: Nothing.
  gw_pro_command_report_new_modification,
  gw_pro_command_amount, // Indicate how many commands supported
} gw_pro_command_t;

// The condition to filter the items
typedef enum {
  // The latest one and the earliest one are only valid in the sensor data
  // table. Filter according to Measurement Timestamp.
  gw_pro_sqlite_cond_latest_one = 0,
  gw_pro_sqlite_cond_earliest_one,
  gw_pro_sqlite_cond_value_range, // Require
                                  // gw_pro_sqlite_cond_value_range_param
                                  // to indicate the range, [min, max]
  gw_pro_sqlite_cond_all,         // To select all
  gw_pro_sqlite_cond_str_equal,
  gw_pro_sqlite_cond_value_not_equal,
  gw_pro_sqlite_cond_and,    // To connect two condition in "and"
                             // manner
  gw_pro_sqlite_cond_or,     // To connect two condition in "or"
                             // manner
  gw_pro_sqlite_cond_amount, // Indicate how many condition supported
} gw_pro_sqlite_condition_t;

typedef enum {
  gw_pro_response_success = 0,
  gw_pro_response_failed = 1
} gw_pro_response_t;

#pragma pack(1)
typedef struct {
  uint32_t field; // The field index in the table

  union {
    char data_char[GW_PRO_MAX_STRING_LEN_BYTE];
    float data_float;
    uint8_t data_uint8;
    int8_t data_int8;
    uint16_t data_uint16;
    int16_t data_int16;
    uint32_t data_uint32;
    int32_t data_int32;
    uint64_t data_uint64;
    int64_t data_int64;
  } data;
} gw_pro_data_t;
typedef struct {
  uint32_t table;   // The table index
  uint32_t itemIdx; // The item index (the key index of a table)
  uint32_t dataNum; // How many elements in the updateData
  gw_pro_data_t updateData[GW_PRO_MAX_DATA_FIELD_PER_OPERATION];
} gw_pro_command_sqlite_update_item_param_t;
typedef struct {
  uint32_t table; // The table index
  uint32_t itemIdx;
} gw_pro_command_sqlite_query_item_param_t;

typedef struct {
  int32_t min;
  int32_t max;
} gw_pro_sqlite_cond_value_range_param_int32_t;
typedef struct {
  uint32_t min;
  uint32_t max;
} gw_pro_sqlite_cond_value_range_param_uint32_t;
typedef struct {
  int64_t min;
  int64_t max;
} gw_pro_sqlite_cond_value_range_param_int64_t;
typedef struct {
  uint64_t min;
  uint64_t max;
} gw_pro_sqlite_cond_value_range_param_uint64_t;
typedef struct {
  float min;
  float max;
} gw_pro_sqlite_cond_value_range_param_float_t;
typedef struct {
  char s[GW_PRO_MAX_STRING_LEN_BYTE];
} gw_pro_sqlite_cond_str_equal_param_char_t;
typedef struct {
  uint8_t condition; // gw_pro_sqlite_condition_t
  uint32_t field;    // The field index in the table

  // Choose corresponding type
  union {
    gw_pro_sqlite_cond_value_range_param_int32_t range_int32;
    gw_pro_sqlite_cond_value_range_param_uint32_t range_uint32;
    gw_pro_sqlite_cond_value_range_param_int64_t range_int64;
    gw_pro_sqlite_cond_value_range_param_uint64_t range_uint64;
    gw_pro_sqlite_cond_value_range_param_float_t range_float;
    gw_pro_sqlite_cond_str_equal_param_char_t string_value;
  } condParam;
} gw_pro_sqlite_cond_element_t;
typedef struct {
  uint32_t table;   // The table index
  uint32_t dataNum; // How many elements in the filter data
  // Filter condition would be organized in a reversed Polish notation
  // (https://blog.csdn.net/zcs425171513/article/details/118310303)
  // to avoid the impact brought about by "brackets".
  gw_pro_sqlite_cond_element_t filterData[GW_PRO_MAX_DATA_FIELD_PER_OPERATION];
} gw_pro_command_sqlite_filter_param_t;
typedef struct {
  uint32_t table; // The table index
  uint32_t itemIdx;
  uint32_t dataNum; // How many elements to delete
  // Filter condition would be organized in a reversed Polish notation
  // (https://blog.csdn.net/zcs425171513/article/details/118310303)
  // to avoid the impact brought about by "brackets".
  gw_pro_sqlite_cond_element_t deleteData[GW_PRO_MAX_DATA_FIELD_PER_OPERATION];
} gw_pro_command_sqlite_delete_item_param_t;
typedef struct {
  uint32_t table; // The table index
  uint32_t itemIdx;
  uint8_t operation;  // only gw_pro_command_sqlite_update_item and
                      // gw_pro_command_sqlite_delete_item are valid
  uint64_t timestamp; // The modification time
} gw_pro_command_report_new_modification_param_t;

typedef struct {
  uint8_t command;    // gw_pro_command_t
  uint32_t commandId; // A number for each command
  union {
    gw_pro_command_sqlite_update_item_param_t sqlite_update_item_param;
    gw_pro_command_sqlite_query_item_param_t sqlite_query_item_param;
    gw_pro_command_sqlite_filter_param_t sqlite_filter_item_param;
    gw_pro_command_sqlite_delete_item_param_t sqlite_delete_item_param;
    gw_pro_command_report_new_modification_param_t
        report_new_modification_param;
  } param;
} gw_pro_message_command_t;

typedef struct {
  uint32_t dataNum; // How many elements in the inquiredData
  gw_pro_data_t inquiredData[GW_PRO_MAX_DATA_FIELD_PER_OPERATION];
} gw_pro_command_sqlite_query_item_response_t;

typedef struct {
  uint32_t dataNum; // How many elements in the filteredItemIdx
  uint32_t filteredItemIdx[GW_PRO_MAX_DATA_ITEM_PER_OPERATION];
} gw_pro_command_sqlite_filter_response_t;

typedef struct {
  uint32_t commandId; // To indicate the response to which command
  uint8_t command;   // gw_pro_command_t
  uint8_t response;  // gw_pro_response_t

  union {
    gw_pro_command_sqlite_query_item_response_t sqlite_update_item_response;
    gw_pro_command_sqlite_filter_response_t sqlite_filter_item_reponse;
  } responseInfo;
} gw_pro_message_response_t;

typedef struct {
  gw_pro_process_type_t from; // Where the message comes from
  uint64_t msgTimestamp_s;    // When the message generates (the time when
                              // the total package generates)
  uint32_t seqNo;             // Sequence number of the message
  uint32_t current_package;   // Current package sequence number
  uint32_t total_package;     // The amout of package in total

  uint8_t comRespFg; // A flag to indicate command or response:
                     // GW_PRO_COMRESPFG_COMMAND or
                     // GW_PRO_COMRESPFG_RESPONSE

  union {
    gw_pro_message_command_t commandMsg;
    gw_pro_message_response_t responseMsg;
  } command_or_response;
} gw_pro_message_header_t;
typedef struct {
  gw_pro_message_header_t header;
  char customed_message_buffer[GW_PRO_CUSTOMED_MSG_LEN_BYTE];
} gw_pro_msgbuf_t;
#pragma pack()

#endif /* __FLEGO_PROCESS_PROTOCOL_FOR_LINUX_GW__ */
