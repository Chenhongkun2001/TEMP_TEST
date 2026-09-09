#ifndef __APP_HCI_EVT_MGMT_H
#define __APP_HCI_EVT_MGMT_H

#include <stdint.h>
//#include "bt.h"
#include "sys_def.h"
#include <ell/queue.h>

#ifndef HCI_EVT_LOG_CT
	#define HCI_EVT_LOG_CT (0) // MACRO to control the output of HCI event, set to none-zero the enable log output
#endif

#ifndef SIT_ADV_WAITTING_GAP
	#define SIT_ADV_WAITTING_GAP	(2U)
#endif

enum pipe_msg_type{
	P_MSG_UNDEFINED = 0,
	P_MSG_ADV_REPORT = 1,

	/*...*/
};

struct msg_adv_report{
	uint8_t event_type;
	uint8_t addr_type;
	uint8_t addr[6];
	int8_t rssi;
	uint8_t dtlen;
	uint8_t raw_pkt[0x30];
};

union pipe_msg_info{
	struct msg_adv_report adv_report; // available when msg.type == P_MSG_ADV_REPORT	
	/*...*/
};

struct pipe_msg{
	enum pipe_msg_type type;
	union pipe_msg_info msg;
	uint32_t crc32; // the CRC sum of struct member msg
};


#ifndef MAX_PKT_INFO
	#define MAX_PKT_INFO	(32U)
#endif

#ifndef SIT_VCC_GAIN
	#define SIT_VCC_GAIN	(0.00078125f)
#endif

#ifndef SIT_MTEMP_GAIN		
	#define SIT_MTEMP_GAIN	(0.0625f)
#endif

struct sit_ad_packet{
	uint8_t local_name[MAX_PKT_INFO];
	uint32_t nameId;
	uint8_t advcnt; //0x0 ~ 0xFF
	int8_t sensor_tpval; // -127 ~ 127
	
	uint16_t vcc_adc;//0 ~ 3.6
	float vcc_val; //0.00078125

	uint8_t hw_version;
	uint8_t sw_version;

	uint16_t wire_bytes;
	float m_tempval; //-45 ~ 150
};


enum sit_data_type{
	SIT_DT_MTEMP = 0, // machine temperature
	SIT_DT_ETEMP = 1, // environment temperature
	SIT_DT_RSSI = 2, // signal strength
	SIT_DT_VCC = 3 // the voltage
};


/*
*/
struct sit_dtpacket{
	uint64_t tstamp; // the time we receive the adv report

	uint8_t event_type;
	uint8_t addr_type;
	uint8_t addr[6];
	int8_t rssi;
	struct sit_ad_packet ad_data;
};

/*NOTE!!! you need to add a mutex to protect the struct-member below when they are visited by different threads
*/
struct sit_dtpacket_saving_controller{
	struct l_queue *pt_queue; // the queue unit ref@struct sit_dtpacket
	struct l_queue *pt_waiting_queue;

	pthread_mutex_t mtx_for_white_list;	
	struct bt_addr bt_white_list[MAX_BT_WHITE_LIST];
	uint32_t nameId[MAX_BT_WHITE_LIST];
};


extern int g_pipefd_hci_event_report[2];


int app_hci_get_pipefd_for_writting(void);
int app_hci_get_pipefd_for_reading(void);

//app_state_t app_hci_fill_adv_report_info( const struct bt_hci_evt_le_adv_report*pt_evtinfo, union pipe_msg_info *pt_pmsg);
app_state_t app_hci_report_event(int kpfd, enum pipe_msg_type mtype, const union pipe_msg_info *pt_pmsg);
void thandler_hci_event(void *para);

app_state_t app_hci_create_pipe_fds(void);

struct sit_dtpacket_saving_controller*app_hci_get_dtpacket_saving_ctr(void);

bool app_hci_init_dtpacket_saving_ctr(struct sit_dtpacket_saving_controller *pt_dtpacket_saving_ctr);

void app_hci_update_bt_white_list(
  struct sit_dtpacket_saving_controller *pt_dtpacket_saving_ctr,
  const struct bt_addr *pt_addr_array,
  const uint32_t *nameId,
  uint8_t kp_arr_sz);


#endif


