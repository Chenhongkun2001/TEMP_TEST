#ifndef __BT_HCITOOL_H
#define __BT_HCITOOL_H

#include "sys_def.h"

enum ble_phy
{
  LE_PHY_1M = 0,
  LE_PHY_2M = 1,
  LE_PHY_CODED_S2 = 2,
  LE_PHY_CODED_S8 = 3,
};

enum bt_vspecific_handle_type
{
  BT_HCI_VS_LL_HANDLE_TYPE_ADV = 0x00,
  BT_HCI_VS_LL_HANDLE_TYPE_SCAN = 0x01,
  BT_HCI_VS_LL_HANDLE_TYPE_CONN = 0x02,
};

#ifndef MAX_BT_TXPOWER_LEVEL
#define MAX_BT_TXPOWER_LEVEL (126)
#endif

#ifndef MIN_BT_TXPOWER_LEVEL
#define MIN_BT_TXPOWER_LEVEL (-127)
#endif

// These functions are not thread-safe since a global variable @p
// g_con_handle is used.
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
app_state_t bt_hcitool_set_tx_power(
  const enum bt_vspecific_handle_type htype,
  const int8_t plevel,
  uint8_t *con_id);
#else 
app_state_t bt_hcitool_set_tx_power(
  const enum bt_vspecific_handle_type htype,
  const int8_t plevel);
#endif
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
app_state_t bt_hcitool_get_tx_power(
  const enum bt_vspecific_handle_type htype,
  int8_t *ptlevel,
  uint8_t *con_id);
#else
app_state_t bt_hcitool_get_tx_power(
  const enum bt_vspecific_handle_type htype,
  int8_t *ptlevel);
#endif

// TODO: uncleaned

/*
NOTE !!! Supervision_Timeout in millisecods shall be larger than (1 + connection_latency) * connection_interval_max * 2
where connection_interval_max is given in milliseconds
*/
struct bluetooth_connection_param{
	enum ble_phy phy;
	
	uint16_t min_con_interval;// range @ ( 0x0006 , 0x0C80); Time = N*1.25ms -> (7.5mS, 4S)
	uint16_t max_con_interval; // range @ (0x0006 , 0x0C80); Time = N *1.25mS -> (7.5mS, 4S)
	uint16_t con_latency; // range @ (0x0000, 0x01F3)
	uint16_t supervision_timeout; //range @ ( 0x000A, 0x0C80), Time = N*10mS -> (100mS, 32S)
	uint16_t min_ce_len; // (0x0000, 0xFFFF) , Time = N *0.625mS
	uint16_t max_ce_len; // (0x0000, 0xFFFF) , Time=N*0.625mS
	
	uint16_t tx_octets; // range (@0x001B ,  0x00FB)
	uint16_t tx_time; //range @ (0x0148 , 0x4290)
};



/*NOTE!! this funciton is not thread safe, for the g_con_handle*/
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
app_state_t bt_hcitool_set_bluetooth_phy(uint8_t *con_id);
#else
app_state_t bt_hcitool_set_bluetooth_phy(void);
#endif

/*NOTE!! this funciton is not thread safe, for the g_con_handle*/
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
app_state_t bt_hcitool_set_bluetooth_connection_param(const struct bluetooth_connection_param *con_param, uint8_t *con_id);
#else
app_state_t bt_hcitool_set_bluetooth_connection_param(const struct bluetooth_connection_param *con_param);
#endif

#endif


