#include "app_hci_evt_mgmt.h"
#include "util_dbg.h"
#include "util_froto.h"

#include "bt.h"
#include "display.h"

#include "app_db.h"
#include <string.h>

#include "unittest.h"

DBG_LOCAL_LOG_DEBUG


#define BT_EIR_FLAGS			0x01
#define BT_EIR_UUID16_SOME		0x02
#define BT_EIR_UUID16_ALL		0x03
#define BT_EIR_UUID32_SOME		0x04
#define BT_EIR_UUID32_ALL		0x05
#define BT_EIR_UUID128_SOME		0x06
#define BT_EIR_UUID128_ALL		0x07
#define BT_EIR_NAME_SHORT		0x08
#define BT_EIR_NAME_COMPLETE		0x09
#define BT_EIR_TX_POWER			0x0a
#define BT_EIR_CLASS_OF_DEV		0x0d
#define BT_EIR_SSP_HASH_P192		0x0e
#define BT_EIR_SSP_RANDOMIZER_P192	0x0f
#define BT_EIR_DEVICE_ID		0x10
#define BT_EIR_SMP_TK			0x10
#define BT_EIR_SMP_OOB_FLAGS		0x11
#define BT_EIR_PERIPHERAL_CONN_INTERVAL	0x12
#define BT_EIR_SERVICE_UUID16		0x14
#define BT_EIR_SERVICE_UUID128		0x15
#define BT_EIR_SERVICE_DATA		0x16
#define BT_EIR_PUBLIC_ADDRESS		0x17
#define BT_EIR_RANDOM_ADDRESS		0x18
#define BT_EIR_GAP_APPEARANCE		0x19
#define BT_EIR_ADVERTISING_INTERVAL	0x1a
#define BT_EIR_LE_DEVICE_ADDRESS	0x1b
#define BT_EIR_LE_ROLE			0x1c
#define BT_EIR_SSP_HASH_P256		0x1d
#define BT_EIR_SSP_RANDOMIZER_P256	0x1e
#define BT_EIR_SERVICE_UUID32		0x1f
#define BT_EIR_SERVICE_DATA32		0x20
#define BT_EIR_SERVICE_DATA128		0x21
#define BT_EIR_LE_SC_CONFIRM_VALUE	0x22
#define BT_EIR_LE_SC_RANDOM_VALUE	0x23
#define BT_EIR_URI			0x24
#define BT_EIR_INDOOR_POSITIONING	0x25
#define BT_EIR_TRANSPORT_DISCOVERY	0x26
#define BT_EIR_LE_SUPPORTED_FEATURES	0x27
#define BT_EIR_CHANNEL_MAP_UPDATE_IND	0x28
#define BT_EIR_MESH_PROV		0x29
#define BT_EIR_MESH_DATA		0x2a
#define BT_EIR_MESH_BEACON		0x2b
#define BT_EIR_3D_INFO_DATA		0x3d
#define BT_EIR_MANUFACTURER_DATA	0xff

// A callback called by l_queue_find
// typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
static bool q_match_sit_dtpacket(
  const void *a,
  const void *b);

static struct sit_dtpacket_saving_controller g_sit_dtpacket_saving_ctr = {0};

bool app_hci_init_dtpacket_saving_ctr(struct sit_dtpacket_saving_controller *pt_dtpacket_saving_ctr)
{
	bool tpret = false;

	if(NULL == pt_dtpacket_saving_ctr)
	{
		tpret = false;
		return tpret;
	}

	pt_dtpacket_saving_ctr->pt_queue = l_queue_new();
	if(NULL == pt_dtpacket_saving_ctr->pt_queue)
	{
		tpret = false;
		DBG_LOG_ERR("fail to create queue");
		goto EXIT;
	}

	pt_dtpacket_saving_ctr->pt_waiting_queue = l_queue_new();
	if(NULL == pt_dtpacket_saving_ctr->pt_waiting_queue)
	{
		tpret = false;
		DBG_LOG_ERR("fail to create queue");
		goto EXIT;
	}

	//
	if(pthread_mutex_init( &pt_dtpacket_saving_ctr->mtx_for_white_list, NULL))
	{
		tpret = false;
		DBG_LOG_ERR("fail to init mutex");
		goto EXIT;
	}
	
	EXIT:
		return tpret;
}

static void app_hci_deinit_dtpacket_saving_ctr(struct sit_dtpacket_saving_controller *pt_dtpacket_saving_ctr)
{
	if(NULL == pt_dtpacket_saving_ctr)
	{
		return;
	}
	l_queue_destroy( pt_dtpacket_saving_ctr->pt_queue, l_free);
	l_queue_destroy( pt_dtpacket_saving_ctr->pt_waiting_queue, l_free);

	// to destroy mutex
	pthread_mutex_destroy( &pt_dtpacket_saving_ctr->mtx_for_white_list);
	
	return;
}

struct sit_dtpacket_saving_controller*app_hci_get_dtpacket_saving_ctr(void)
{
	return &g_sit_dtpacket_saving_ctr;
}

/**
 * @brief Update white list (a list of MAC address and a list of namdId for
 *        Insight-T)
 * @param pt_dtpacket_saving_ctr: The object to save the white list of
 *        Insight-T
 * @param pt_addr_array: The list of MAC address
 * @param nameId: The list of name ID
 * @param kp_arr_sz: Number of elements in the list
 */
void
app_hci_update_bt_white_list(
  struct sit_dtpacket_saving_controller *pt_dtpacket_saving_ctr,
  const struct bt_addr *pt_addr_array,
  const uint32_t *nameId,
  uint8_t kp_arr_sz)
{
  if((NULL == pt_dtpacket_saving_ctr) || (NULL == pt_addr_array))
  {
    DBG_LOG_ERR("Invalid parameter");
    return;
  }

  kp_arr_sz = kp_arr_sz >
              MAX_BT_WHITE_LIST ? MAX_BT_WHITE_LIST : kp_arr_sz;

  pthread_mutex_lock(&pt_dtpacket_saving_ctr->mtx_for_white_list);

  memset(pt_dtpacket_saving_ctr->bt_white_list, 0,
         sizeof(pt_dtpacket_saving_ctr->bt_white_list[0]) *
         MAX_BT_WHITE_LIST);
  memset(pt_dtpacket_saving_ctr->nameId, 0,
         sizeof(pt_dtpacket_saving_ctr->nameId[0]) * MAX_BT_WHITE_LIST);
  memcpy(pt_dtpacket_saving_ctr->bt_white_list, pt_addr_array,
         sizeof(pt_dtpacket_saving_ctr->bt_white_list[0]) * kp_arr_sz);
  memcpy(pt_dtpacket_saving_ctr->nameId, nameId,
         sizeof(pt_dtpacket_saving_ctr->nameId[0]) * kp_arr_sz);

  pthread_mutex_unlock(&pt_dtpacket_saving_ctr->mtx_for_white_list);

  return;
}


/**
 * @brief A callback called by l_queue_find
 * @return True if addresses pointed by @p a and @p b are identical
 */
// typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
static bool
q_match_sit_dtpacket(
  const void *a,
  const void *b)
{
  bool tpret = true;
  struct sit_dtpacket const *pt_dtpkt = NULL;
  uint8_t const *pt_btaddr = NULL;

  if((NULL == a) || (NULL == b))
  {
    tpret = false;
    return tpret;
  }

  pt_dtpkt = (struct sit_dtpacket *)a;
  pt_btaddr = (uint8_t *)b;

  if(0 != memcmp(pt_dtpkt->addr, pt_btaddr, 6))
  {
    // Bluetooth address is not identical
    tpret = false;
  }
  return tpret;
}

/*append data-packet to queue
NOTE!!! this function is not thread safe
*/
static app_state_t app_hci_append_dtpacket( struct sit_dtpacket_saving_controller*pt_dtpacket_saving_ctr, struct msg_adv_report*pt_adv_rpt, struct sit_ad_packet*pt_ad_pkt)
{
	app_state_t tpret = ST_OK;
	struct sit_dtpacket *pt_dtpkt = NULL;
	struct sit_dtpacket *hld_dtpkt = NULL;

	uint8_t kp_btaddr[6] = {0};
	
	if((NULL == pt_dtpacket_saving_ctr)||(NULL == pt_adv_rpt)||(NULL == pt_ad_pkt))
	{
		tpret = ST_ERR;
		goto ERR;
	}

	//to modify the byte order of BT-address
	kp_btaddr[0] = pt_adv_rpt->addr[5];
	kp_btaddr[1] = pt_adv_rpt->addr[4];
	kp_btaddr[2] = pt_adv_rpt->addr[3];
	kp_btaddr[3] = pt_adv_rpt->addr[2];
	kp_btaddr[4] = pt_adv_rpt->addr[1];
	kp_btaddr[5] = pt_adv_rpt->addr[0];

	//DBG_LOG_ERR("BKP");
	//util_dbg_buf_dump(kp_btaddr, 6);

	hld_dtpkt = l_queue_find( pt_dtpacket_saving_ctr->pt_queue, q_match_sit_dtpacket, (const void *)kp_btaddr);
	if(hld_dtpkt)
	{// there is unit match the BT-address on the queue
		// to update the data-unit
		memset( hld_dtpkt, 0, sizeof(struct sit_dtpacket));
		memcpy(hld_dtpkt->addr, kp_btaddr, 6);
		hld_dtpkt->addr_type = pt_adv_rpt->addr_type;
		hld_dtpkt->event_type = pt_adv_rpt->event_type;
		hld_dtpkt->rssi = pt_adv_rpt->rssi;
		memcpy(&hld_dtpkt->ad_data, pt_ad_pkt, sizeof(struct sit_ad_packet));

		hld_dtpkt->tstamp = time(NULL);
	}
	else
	{// create a new unit and push it to the queue
		pt_dtpkt = (struct sit_dtpacket*)l_malloc(sizeof(struct sit_dtpacket));
		if(NULL == pt_ad_pkt)
		{
			DBG_LOG_ERR("malloc fail");
		}
		memset( pt_dtpkt, 0, sizeof(struct sit_dtpacket));
		memcpy( pt_dtpkt->addr, kp_btaddr, 6);
		pt_dtpkt->addr_type = pt_adv_rpt->addr_type;
		pt_dtpkt->event_type = pt_adv_rpt->event_type;
		pt_dtpkt->rssi = pt_adv_rpt->rssi;
		memcpy( &pt_dtpkt->ad_data, pt_ad_pkt, sizeof(struct sit_ad_packet));

		pt_dtpkt->tstamp = time(NULL);

		//add the data-unit to the queue
		if(false == l_queue_push_tail( pt_dtpacket_saving_ctr->pt_queue, (void *)pt_dtpkt))
		{
			DBG_LOG_ERR("fail to push unit to queue");
			tpret = ST_ERR;
			goto ERR;
		}
	}
	
	return tpret;

	ERR:
		if(pt_dtpkt)
		{
			l_free( pt_dtpkt);
		}
		return tpret;

}

/*
typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
*/
static bool q_match_remove_adv_dataunit(const void *a, const void *b)
{
	bool tpret = true;
	struct sit_dtpacket *pt_dtpkt = NULL;
	uint32_t kp_tstamp = 0;
	if((NULL == a)||(0 == b))
	{
		tpret = false;
		return tpret;
	}
	pt_dtpkt = (struct sit_dtpacket*)a;
	kp_tstamp = (uint32_t)b;

	if((kp_tstamp - pt_dtpkt->tstamp) < SIT_ADV_WAITTING_GAP)
	{
		tpret = false;
	}
	return tpret;
}

/*try to write Insight-T advertising data to database 

NOTE!!! this function is not thread safe
b. to write VCC/M_temp/E_temp/RSSI to DB
*/
#ifndef SIT_DATA_NUM
	#define SIT_DATA_NUM (4U)
#endif
static app_state_t app_hci_save_sit_adv_data_all(struct sit_dtpacket_saving_controller*pt_dtpacket_saving_ctr, struct msg_format_controller * pt_msg_format_ctr)
{
	app_state_t tpret = ST_OK;
	struct sit_dtpacket *pt_sit_dtpkt = NULL;
	
	static struct sit_dtpacket *hld_dtpkt_to_written = NULL;
	static uint8_t hld_process_idx = 0;
	
	const uint32_t kp_tsamp = time(NULL);
	
	if(NULL == pt_dtpacket_saving_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	while(1)
	{
	 	pt_sit_dtpkt = (struct sit_dtpacket*)l_queue_remove_if( pt_dtpacket_saving_ctr->pt_queue, q_match_remove_adv_dataunit, (const void *)kp_tsamp);
		if(NULL == pt_sit_dtpkt)
		{
			break;
		}

		#if(1) // only for debug
			if(l_queue_length( pt_dtpacket_saving_ctr->pt_waiting_queue) > 0x100)
			{
				DBG_LOG_ERR("too much data on waiting queue, we just discard the coming data");
				l_free( pt_sit_dtpkt);
				pt_sit_dtpkt = NULL;
				continue;
			}
			else
			{
				DBG_LOG_WARN("to put dtunit on waiting queue");
			}
		#endif
		
		if(false == l_queue_push_tail( pt_dtpacket_saving_ctr->pt_waiting_queue, (void *)pt_sit_dtpkt))
		{
			DBG_LOG_ERR("fail to put dataunit on waiting queue, before writting to database");
			l_free( pt_sit_dtpkt);
			pt_sit_dtpkt = NULL;
		}
	}

	//DBG_LOG_WARN("Insight-T detected %d, the dtunit on waiting queue %d, dtpkt_to_written@0x%x", l_queue_length( pt_dtpacket_saving_ctr->pt_queue), l_queue_length( pt_dtpacket_saving_ctr->pt_waiting_queue), hld_dtpkt_to_written);
	#if(0)// for debug only , to dump the queue
		struct l_queue_entry *pt_entry = l_queue_get_entries( pt_dtpacket_saving_ctr->pt_queue);
		struct sit_dtpacket *pt_temp_dtpkt = NULL;
		uint32_t tp_cnt = 0;
		while(pt_entry)
		{
			pt_temp_dtpkt = (struct sit_dtpacket*)pt_entry->data;
			//DBG_LOG_INFO("I_T %d %.2X%.2X%.2X%.2X%.2X%.2X", tp_cnt, pt_temp_dtpkt->addr[0],pt_temp_dtpkt->addr[1],pt_temp_dtpkt->addr[2],pt_temp_dtpkt->addr[3],pt_temp_dtpkt->addr[4],pt_temp_dtpkt->addr[5]);			
			printf("\nI_T %d %.2X%.2X%.2X%.2X%.2X%.2X\n", tp_cnt, pt_temp_dtpkt->addr[0],pt_temp_dtpkt->addr[1],pt_temp_dtpkt->addr[2],pt_temp_dtpkt->addr[3],pt_temp_dtpkt->addr[4],pt_temp_dtpkt->addr[5]);
			tp_cnt++;
			pt_entry = pt_entry->next;
		}
	#endif
	//to write data from Insight-T to database
	#if(0)
	if(NULL != hld_dtpkt_to_written)
	{
		if(ST_OK != app_db_write_sit_data_to_db( app_db_get_dbop_controller(),  pt_msg_format_ctr, hld_dtpkt_to_written))
		{
			tpret = ST_ERR;
			goto EXIT;		
		}
		else
		{
			l_free(hld_dtpkt_to_written);
			hld_dtpkt_to_written = NULL;
		}
	}
	while(1)
	{
		hld_dtpkt_to_written = l_queue_pop_head( pt_dtpacket_saving_ctr->pt_waiting_queue);
		if(NULL == hld_dtpkt_to_written)
		{
			break;
		}
		if(ST_OK != app_db_write_sit_data_to_db( app_db_get_dbop_controller(),  pt_msg_format_ctr, hld_dtpkt_to_written))
		{// fail to write
			tpret = ST_ERR;
			goto EXIT;
		}
		else
		{
			l_free(hld_dtpkt_to_written);
			hld_dtpkt_to_written = NULL;
		}
	}
	#else
		if(NULL == hld_dtpkt_to_written)
		{
			hld_dtpkt_to_written = l_queue_pop_head( pt_dtpacket_saving_ctr->pt_waiting_queue);
			if(NULL == hld_dtpkt_to_written)
			{// no data on queue for writting
				tpret = ST_ERR;
				goto EXIT;
			}
			hld_process_idx = SIT_DT_MTEMP;
		}
		while(1)
		{
			if(NULL == hld_dtpkt_to_written)
			{
				break;
			}
			switch(hld_process_idx)
			{
				case SIT_DT_MTEMP: // 0
				{ // machine temperature
					if(ST_OK != app_db_deliver_sensordata_sit_to_db_noblock( app_db_get_dbop_controller(), hld_dtpkt_to_written, SIT_DT_MTEMP))
					{
						tpret = ST_ERR;
						goto EXIT;
					}
					hld_process_idx = SIT_DT_ETEMP;
				}
				case SIT_DT_ETEMP: //1
				{ // environment temperature
					if(ST_OK != app_db_deliver_sensordata_sit_to_db_noblock( app_db_get_dbop_controller(), hld_dtpkt_to_written, SIT_DT_ETEMP))
					{
						tpret = ST_ERR;
						goto EXIT;
					}
					hld_process_idx = SIT_DT_RSSI;
				}
				case SIT_DT_RSSI://2
				{ // signal strength
					if(ST_OK != app_db_deliver_sensordata_sit_to_db_noblock( app_db_get_dbop_controller(), hld_dtpkt_to_written, SIT_DT_RSSI))
					{
						tpret = ST_ERR;
						goto EXIT;
					}
					hld_process_idx = SIT_DT_VCC;
				}
				case SIT_DT_VCC://3:
				{
					if(ST_OK != app_db_deliver_sensordata_sit_to_db_noblock( app_db_get_dbop_controller(), hld_dtpkt_to_written, SIT_DT_VCC))
					{
						tpret = ST_ERR;
						goto EXIT;
					}

					//all data is written ,release the memory and pop the next for writting
					l_free(hld_dtpkt_to_written);
					hld_dtpkt_to_written = l_queue_pop_head( pt_dtpacket_saving_ctr->pt_waiting_queue);
					hld_process_idx = SIT_DT_MTEMP;
					break;
				}
				default:
				{
					DBG_LOG_ERR("You are not supposed to see this output");
					if(hld_dtpkt_to_written)
					{
						l_free(hld_dtpkt_to_written);
						hld_dtpkt_to_written = NULL;
					}
					hld_process_idx = 0;
					break;
				}
			}
		}
	#endif
	
	EXIT:
		return tpret;
}












/*global variable for HCI event report*/
int g_pipefd_hci_event_report[2];

int app_hci_get_pipefd_for_writting(void)
{
	return g_pipefd_hci_event_report[1];
}


int app_hci_get_pipefd_for_reading(void)
{
	return g_pipefd_hci_event_report[0];
}


/*to create a pipe for IPC
NOTE!!! call this function, before you fork
*/
app_state_t app_hci_create_pipe_fds(void)
{
	app_state_t tpret = ST_OK;
	int tpint = 0;

	tpint = pipe(g_pipefd_hci_event_report);
	if(tpint != 0)
	{
		DBG_LOG_ERR("fail to create pipe for IPC, for %s", strerror(errno));
		tpret = ST_ERR;
	}
	return tpret;
}


/*kpfd ~ the pipe-fd for data writting
pt_msg_adv_report ~ the msg-info to be reported
ret@
*/
app_state_t app_hci_report_event(int kpfd, enum pipe_msg_type mtype, const union pipe_msg_info *pt_pmsg)
{
	app_state_t tpret = ST_OK;
	struct pipe_msg kp_pmsg = {0};
	ssize_t tp_szval = 0;

	if((kpfd < 0)||(NULL == pt_pmsg))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		goto EXIT;
	}

	
	kp_pmsg.type = mtype;
	memcpy( &kp_pmsg.msg, pt_pmsg, sizeof(union pipe_msg_info));
	kp_pmsg.crc32 = util_froto_bullet_crc32((const uint8_t *)&kp_pmsg.msg, sizeof(union pipe_msg_info));
		
	// to send the msg
	tp_szval = write( kpfd, &kp_pmsg, sizeof(struct pipe_msg));
	if(tp_szval != sizeof(struct pipe_msg))
	{
		DBG_LOG_ERR("pipe writting fail, for %s", strerror(errno));
		tpret = ST_ERR;
		goto EXIT;
	}
	
	EXIT:
		return tpret;
}

/*
*/
app_state_t app_hci_fill_adv_report_info( const struct bt_hci_evt_le_adv_report*pt_evtinfo, union pipe_msg_info *pt_pmsg)
{
	app_state_t tpret = ST_OK;
	int32_t tpi32 = 0;
	if((NULL == pt_evtinfo)||(NULL == pt_pmsg))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	//to set the value
	memset( pt_pmsg, 0, sizeof(union pipe_msg_info)); // clean the buffer
	
	pt_pmsg->adv_report.event_type = pt_evtinfo->event_type;
	pt_pmsg->adv_report.addr_type = pt_evtinfo->addr_type;
	memcpy(pt_pmsg->adv_report.addr, pt_evtinfo->addr, sizeof(pt_pmsg->adv_report.addr));
	pt_pmsg->adv_report.dtlen = pt_evtinfo->data_len;
	tpi32 = pt_evtinfo->data_len > sizeof(pt_pmsg->adv_report.raw_pkt) ? sizeof(pt_pmsg->adv_report.raw_pkt) : pt_evtinfo->data_len;
	memcpy( pt_pmsg->adv_report.raw_pkt, pt_evtinfo->data, tpi32);
	//RSSI
	pt_pmsg->adv_report.rssi = *((int8_t*)(pt_evtinfo->data + pt_evtinfo->data_len));
	
	return tpret;
}


/*to check if the  msg packet is valid
*/
static bool is_pmsg_packet_valid(const struct pipe_msg* pt_pmsg)
{
	bool tpret = true;
	uint32_t tpu32 = 0;

	if(NULL == pt_pmsg)
	{
		DBG_LOG_ERR("unexpected NULL");
		return tpret;
	}

	tpu32 = util_froto_bullet_crc32((const uint8_t *)&pt_pmsg->msg, sizeof(union pipe_msg_info));
	if(tpu32 != pt_pmsg->crc32)
	{
		tpret = false;
	}
	return tpret;
}




/* 
*/
static bool does_hci_event_match_filter(const struct pipe_msg*pt_pmsg, uint32_t * returnedNameId)
{
	bool tpret = true;
	
	const struct sit_dtpacket_saving_controller *pt_dtpacket_saving_ctr = app_hci_get_dtpacket_saving_ctr();
	bool is_big_endian = sys_is_platform_big_endian();

	uint8_t kp_mac_array[6] = {0};
	uint8_t *ptu8 = NULL;
	
	switch(pt_pmsg->type)
	{
		case P_MSG_ADV_REPORT:
		{
			#if(0)
			// address pattern checking
			if((0xC4 != pt_pmsg->msg.adv_report.addr[5])||(0xBD != pt_pmsg->msg.adv_report.addr[4])||(0x6A != pt_pmsg->msg.adv_report.addr[3]))
			{
				tpret = false;
			}
			#else
				tpret = false;
				
				//DBG_LOG_WARN("MAX-string  0x%X-0x%X-0x%X\n", pt_dtpacket_saving_ctr->bt_white_list[0].nap, pt_dtpacket_saving_ctr->bt_white_list[0].uap, pt_dtpacket_saving_ctr->bt_white_list->lap);
				
				// use the white list to filter the ADV-packet
				pthread_mutex_lock( &pt_dtpacket_saving_ctr->mtx_for_white_list);

				for(uint32_t i = 0; i < MAX_BT_WHITE_LIST; i++)
				{
					memset( kp_mac_array, 0, sizeof(kp_mac_array));
					if(is_big_endian)
					{
						//nap
						ptu8 = (uint8_t*)&pt_dtpacket_saving_ctr->bt_white_list[i].nap;
						kp_mac_array[5] = ptu8[0];
						kp_mac_array[4] = ptu8[1];
						//uap
						kp_mac_array[3] = pt_dtpacket_saving_ctr->bt_white_list[i].uap;
						//lap
						ptu8 = (uint8_t*)&pt_dtpacket_saving_ctr->bt_white_list[i].lap;
						kp_mac_array[2] = ptu8[0];
						kp_mac_array[1] = ptu8[1];
						kp_mac_array[0] = ptu8[2];
						// DBG_LOG_DEBUG("Whitelist: 0x%X-0x%X-0x%X-0x%X-0x%X-0x%X\n", kp_mac_array[0], kp_mac_array[1], kp_mac_array[2], kp_mac_array[3], kp_mac_array[4], kp_mac_array[5]);
					}
					else
					{
						//nap
						ptu8 = (uint8_t*)&pt_dtpacket_saving_ctr->bt_white_list[i].nap;
						kp_mac_array[5] = ptu8[1];
						kp_mac_array[4] = ptu8[0];
						//uap
						kp_mac_array[3] = pt_dtpacket_saving_ctr->bt_white_list[i].uap;
						//lap
						ptu8 = (uint8_t*)&pt_dtpacket_saving_ctr->bt_white_list[i].lap;
						kp_mac_array[2] = ptu8[2];
						kp_mac_array[1] = ptu8[1];
						kp_mac_array[0] = ptu8[0];
						// DBG_LOG_DEBUG("Whitelist: 0x%X-0x%X-0x%X-0x%X-0x%X-0x%X\n", kp_mac_array[0], kp_mac_array[1], kp_mac_array[2], kp_mac_array[3], kp_mac_array[4], kp_mac_array[5]);
					}                               
					if(0 == memcmp( kp_mac_array, pt_pmsg->msg.adv_report.addr, sizeof(kp_mac_array)))
					{// the MAC match
						if(returnedNameId != NULL)
						{
							*returnedNameId = pt_dtpacket_saving_ctr->nameId[i];
						}
						tpret = true;
						break;
					}
				}
				
				pthread_mutex_unlock( &pt_dtpacket_saving_ctr->mtx_for_white_list);
			#endif

			break;
		}
		/*...*/
		default:
		{
			tpret = false;
			break;
		}
	}
	
	return tpret;
}

static bool to_decode_eir_manufacture_data( struct sit_ad_packet *pt_ad_pkt, const uint8_t *pt_data, const uint8_t data_len);


//static void print_eir(const uint8_t *eir, uint8_t eir_len, bool le)
static bool app_hci_decode_eir(const uint8_t *eir, uint8_t eir_len, bool le, struct sit_ad_packet*pt_ad_pkt)
{
	bool tpret = true;
	uint16_t len = 0;

	uint8_t kp_cnt = 0;

	if(NULL == pt_ad_pkt)
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = false;
		return tpret;
	}
	// to clean the buffer
	memset( pt_ad_pkt, 0, sizeof(struct sit_ad_packet));

	if (eir_len == 0)
	{
		//DBG_LOG_ERR("invalid EIR len");
		tpret = false;
		return tpret;
	}
	
	while (len < eir_len - 1) {
		uint8_t field_len = eir[0];
		const uint8_t *data = &eir[2];
		uint8_t data_len;
		char name[239], label[100];
		// uint8_t flags;
		// uint8_t mask;

		/* Check for the end of EIR */
		if (field_len == 0)
			break;

		len += field_len + 1;

		/* Do not continue EIR Data parsing if got incorrect length */
		if (len > eir_len) {
			len -= field_len + 1;
			break;
		}

		data_len = field_len - 1;

		switch (eir[1]) {
		case BT_EIR_FLAGS:
			// flags = *data;

			//print_field("Flags: 0x%2.2x", flags);
			//mask = print_bitfield(2, flags, eir_flags_table);
			//if (mask)
			//{
				//print_text(COLOR_UNKNOWN_SERVICE_CLASS,"  Unknown flags (0x%2.2x)", mask);
			//	;
			//}
			break;

		case BT_EIR_UUID16_SOME:
			if (data_len < sizeof(uint16_t))
				break;
			//print_uuid16_list("16-bit Service UUIDs (partial)",data, data_len);
			break;

		case BT_EIR_UUID16_ALL:
			if (data_len < sizeof(uint16_t))
				break;
			//print_uuid16_list("16-bit Service UUIDs (complete)",data, data_len);
			break;

		case BT_EIR_UUID32_SOME:
			if (data_len < sizeof(uint32_t))
				break;
			//print_uuid32_list("32-bit Service UUIDs (partial)",data, data_len);
			break;

		case BT_EIR_UUID32_ALL:
			if (data_len < sizeof(uint32_t))
				break;
			//print_uuid32_list("32-bit Service UUIDs (complete)",data, data_len);
			break;

		case BT_EIR_UUID128_SOME:
			if (data_len < 16)
				break;
			//print_uuid128_list("128-bit Service UUIDs (partial)",data, data_len);
			break;

		case BT_EIR_UUID128_ALL:
			if (data_len < 16)
				break;
			//print_uuid128_list("128-bit Service UUIDs (complete)",data, data_len);
			break;

		case BT_EIR_NAME_SHORT:
			memset(name, 0, sizeof(name));
			memcpy(name, data, data_len);
			//print_field("Name (short): %s", name);
			break;

		case BT_EIR_NAME_COMPLETE:
			memset(name, 0, sizeof(name));
			memcpy(name, data, data_len);
			//print_field("Name (complete): %s", name);
			#if(1) // to fill the name
				if(MAX_PKT_INFO > data_len)
				{
					kp_cnt = kp_cnt + 1;
					memcpy( pt_ad_pkt->local_name, data, data_len);
					if((pt_ad_pkt->local_name[0] != 0x53)||(pt_ad_pkt->local_name[1] != 0x49)||(pt_ad_pkt->local_name[2] != 0x54))
					{
						//DBG_LOG_ERR("unexpected local name");
						tpret = false;
					}
				}
				else
				{
					//DBG_LOG_ERR("invalid name length %d", data_len);
					tpret = false;
				}
			#endif	
			break;

		case BT_EIR_TX_POWER:
			if (data_len < 1)
				break;
			//print_field("TX power: %d dBm", (int8_t) *data);
			break;

		case BT_EIR_CLASS_OF_DEV:
			if (data_len < 3)
				break;
			//print_dev_class(data);
			break;

		case BT_EIR_SSP_HASH_P192:
			if (data_len < 16)
				break;
			//print_hash_p192(data);
			break;

		case BT_EIR_SSP_RANDOMIZER_P192:
			if (data_len < 16)
				break;
			//print_randomizer_p192(data);
			break;

		case BT_EIR_DEVICE_ID:
			/* SMP TK has the same value as Device ID */
			if (le)
			{
				//print_hex_field("SMP TK", data, data_len);
			}
			else if (data_len >= 8)
			{
				//print_device_id(data, data_len);
			}
			break;

		case BT_EIR_SMP_OOB_FLAGS:
			//print_field("SMP OOB Flags: 0x%2.2x", *data);
			break;

		case BT_EIR_PERIPHERAL_CONN_INTERVAL:
			if (data_len < 4)
				break;
			//print_field("Peripheral Conn. Interval: ""0x%4.4x - 0x%4.4x",get_le16(&data[0]),get_le16(&data[2]));
			break;

		case BT_EIR_SERVICE_UUID16:
			if (data_len < sizeof(uint16_t))
				break;
			//print_uuid16_list("16-bit Service UUIDs",data, data_len);
			break;

		case BT_EIR_SERVICE_UUID128:
			if (data_len < 16)
				break;
			//print_uuid128_list("128-bit Service UUIDs",data, data_len);
			break;

		case BT_EIR_SERVICE_DATA:
			if (data_len < 2)
				break;
			//print_service_data(data, data_len);
			break;

		case BT_EIR_RANDOM_ADDRESS:
			if (data_len < 6)
				break;
			//print_addr("Random Address", data, 0x01);
			break;

		case BT_EIR_PUBLIC_ADDRESS:
			if (data_len < 6)
				break;
			//print_addr("Public Address", data, 0x00);
			break;

		case BT_EIR_GAP_APPEARANCE:
			if (data_len < 2)
				break;
			//print_appearance(get_le16(data));
			break;

		case BT_EIR_SSP_HASH_P256:
			if (data_len < 16)
				break;
			//print_hash_p256(data);
			break;

		case BT_EIR_SSP_RANDOMIZER_P256:
			if (data_len < 16)
				break;
			//print_randomizer_p256(data);
			break;

		case BT_EIR_TRANSPORT_DISCOVERY:
			//print_transport_data(data, data_len);
			break;

		case BT_EIR_3D_INFO_DATA:
			//print_hex_field("3D Information Data", data, data_len);
			if (data_len < 2)
				break;

			// flags = *data;

			//print_field("  Features: 0x%2.2x", flags);

			//mask = print_bitfield(4, flags, eir_3d_table);
			//if (mask)
			//{
				//print_text(COLOR_UNKNOWN_FEATURE_BIT,"Unknown features (0x%2.2x)", mask);
			//	;
			//}
			//print_field("  Path Loss Threshold: %d", data[1]);
			break;

		case BT_EIR_MESH_DATA:
			//print_mesh_data(data, data_len);
			break;

		case BT_EIR_MESH_PROV:
			//print_mesh_prov(data, data_len);
			break;

		case BT_EIR_MESH_BEACON:
			//print_mesh_beacon(data, data_len);
			break;

		case BT_EIR_MANUFACTURER_DATA:
			if (data_len < 2)
				break;
			//print_manufacturer_data(data, data_len);
			kp_cnt = kp_cnt + 1;
			tpret = to_decode_eir_manufacture_data( pt_ad_pkt, data, data_len);
			break;

		default:
			sprintf(label, "Unknown EIR field 0x%2.2x", eir[1]);
			print_hex_field(label, data, data_len);
			break;
		}

		eir += field_len + 1;


		if(false == tpret)
		{// break out immediately when something goes wrong
			break;
		}
	}

	if (len < eir_len && eir[0] != 0)
	{
		//packet_hexdump(eir, eir_len - len);
		;
	}

	if(kp_cnt != 2)
	{
		//DBG_LOG_ERR("EIR tag name and manufacture-data is expected");
		tpret = false;
	}

	return tpret;
}


static bool to_decode_eir_manufacture_data( struct sit_ad_packet *pt_ad_pkt, const uint8_t *pt_data, const uint8_t data_len)
{
	bool tpret = true;
	
	if((NULL == pt_ad_pkt)||(NULL == pt_data)||((0x0C - 1) != data_len))
	{
		DBG_LOG_ERR("invalid pareameter");
		tpret = false;
		return tpret;
	}

	//0,1 the company ID
	if((0x0E != pt_data[0])||(0x04 != pt_data[1]))
	{
		tpret = false;
		DBG_LOG_ERR("unexpectec company ID");
		return tpret;
	}

	//2 the Frame-type
	if(0x8B != pt_data[2])
	{
		DBG_LOG_ERR("unexpected frame type");
		tpret = false;
		return tpret;
	}

	//3 ADCCNT
	pt_ad_pkt->advcnt = pt_data[3];

	//4  temperature
	pt_ad_pkt->sensor_tpval = pt_data[4];

	//5,6 VCC
	pt_ad_pkt->vcc_adc = pt_data[6] * 0xFF + pt_data[5];
	pt_ad_pkt->vcc_val = pt_ad_pkt->vcc_adc * SIT_VCC_GAIN;
	if((pt_ad_pkt->vcc_val <= 0)||(pt_ad_pkt->vcc_val >= 3.6f))
	{
		DBG_LOG_ERR("the vcc value is out of range, %f", pt_ad_pkt->vcc_val);
		tpret = false;
		return tpret;
	}

	//7 , HW version
	pt_ad_pkt->hw_version = pt_data[7];

	//8 , SW version
	pt_ad_pkt->sw_version = pt_data[8];

	//9, 10 
	pt_ad_pkt->wire_bytes = pt_data[10] * 0xFF + pt_data[9];
	pt_ad_pkt->m_tempval = pt_ad_pkt->wire_bytes * SIT_MTEMP_GAIN - 50.0625f;
	if(pt_ad_pkt->m_tempval < 30.0f)
	{// T = tp + (tp - 30) * 0.005f
		pt_ad_pkt->m_tempval = pt_ad_pkt->m_tempval + (pt_ad_pkt->m_tempval - 30.0f) * 0.005f;
	}
	else if(pt_ad_pkt->m_tempval > 100.0f)
	{// T = tp + (100 - tp) * 0.012f
		pt_ad_pkt->m_tempval = pt_ad_pkt->m_tempval + (100.0f - pt_ad_pkt->m_tempval) * 0.012f;
	}
	else
	{// T = tp
		pt_ad_pkt->m_tempval = pt_ad_pkt->m_tempval;
	}
	if((pt_ad_pkt->m_tempval < -45.0f)||(pt_ad_pkt->m_tempval > 150.0f))
	{
		DBG_LOG_ERR("MeasurementTemperature is out of range %f", pt_ad_pkt->m_tempval);
		tpret = false;
		return tpret;
	}

	return tpret;
}





/*handler to deal with HCI event report
*/
void thandler_hci_event(void *para)
{
	int kp_pfd = app_hci_get_pipefd_for_reading();
	struct sit_dtpacket_saving_controller *pt_dtpacket_saving_ctr = app_hci_get_dtpacket_saving_ctr();
	
	//app_hci_init_dtpacket_saving_ctr( pt_dtpacket_saving_ctr);
	
	
	struct pipe_msg kp_pmsg = {0};
	ssize_t kp_szval = 0;

	struct sit_ad_packet kp_sit_adpkt = {0};
	struct msg_format_controller kp_msg_format_ctr = {0};

	uint32_t _returnedNameId = 0;
	while(1)
	{	
		// TODO:  check the status of "restart_dbus_required == true"
		// to see whether it need decode&store data of -T sensor?? 
		// to be confirmed.
		memset( &kp_pmsg, 0, sizeof(struct pipe_msg));
		kp_szval = read( kp_pfd, &kp_pmsg, sizeof(struct pipe_msg));
		if(sizeof(struct pipe_msg) != kp_szval)
		{
			DBG_LOG_ERR("unexpected bytes is read");
			continue;
		}
#if (COLLECT_INSIGHT_T == 0)
		continue;
#endif

		#if(0)// only for debug
			static uint32_t tpcnt = 0;
			tpcnt ++;
			if(0 == tpcnt % 20)
			{
				DBG_LOG_WARN("ADV-packet is received\n");
			}
			continue; // for debug
		#endif
		
		#if(1)	
			// try to flush the data on waiting queue
			app_hci_save_sit_adv_data_all( pt_dtpacket_saving_ctr, &kp_msg_format_ctr);
		#endif

		if(false == is_pmsg_packet_valid((const struct pipe_msg *)&kp_pmsg))
		{
			DBG_LOG_ERR("TODO=====the msg data in the pipe need to be aligned");
			continue;
		}

		if(false == does_hci_event_match_filter((const struct pipe_msg *)&kp_pmsg, &_returnedNameId))
		{
			continue;
		}
				
		//DBG_LOG_WARN("the msg type received is %d", kp_pmsg.type);
		
		switch(kp_pmsg.type)
		{
			case P_MSG_ADV_REPORT:
			{
#if (1)
				DBG_LOG_DEBUG(" the ADV RPT info :\n");
				util_dbg_buf_dump( kp_pmsg.msg.adv_report.addr, 6);
#else
				//DBG_LOG_INFO(" the ADV RPT info :");
				//util_dbg_buf_dump( kp_pmsg.msg.adv_report.addr, 6);
#endif
				//util_dbg_buf_dump( kp_pmsg.msg.adv_report.raw_pkt, kp_pmsg.msg.adv_report.dtlen);
				memset( &kp_sit_adpkt, 0, sizeof(struct sit_ad_packet));
				
				if(false == app_hci_decode_eir( kp_pmsg.msg.adv_report.raw_pkt, kp_pmsg.msg.adv_report.dtlen, true, &kp_sit_adpkt))
				{
					//DBG_LOG_ERR("BKP %d", tpbval);
					break;
				}
				kp_sit_adpkt.nameId = _returnedNameId;

				// the Insight-T advertising data
				#if(0)
				DBG_LOG_WARN("================the data from Insight-T==============");
				DBG_LOG_INFO("Local_name %s", kp_sit_adpkt.local_name);
				DBG_LOG_INFO("ADV-counter %d", kp_sit_adpkt.advcnt);
				DBG_LOG_INFO("Tempearture %d", kp_sit_adpkt.sensor_tpval);
				DBG_LOG_INFO("VCC_ADC 0x%x", kp_sit_adpkt.vcc_adc);
				DBG_LOG_INFO("VCC_VAL %f", kp_sit_adpkt.vcc_val);
				DBG_LOG_INFO("HW version %x", kp_sit_adpkt.hw_version);
				DBG_LOG_INFO("SW version %x", kp_sit_adpkt.sw_version);
				DBG_LOG_INFO("wire bytes 0x%x", kp_sit_adpkt.wire_bytes);
				DBG_LOG_INFO("M_temperature %f", kp_sit_adpkt.m_tempval);
				#else
					//DBG_LOG_INFO("RSSI %d, %d", kp_pmsg.msg.adv_report.rssi);
					if(ST_OK != app_hci_append_dtpacket( pt_dtpacket_saving_ctr, &kp_pmsg.msg.adv_report, &kp_sit_adpkt))
					{
						DBG_LOG_ERR("fail to push ADV-packet to queue");
					}
					#if(0)
					//app_hci_save_sit_adv_data( pt_dtpacket_saving_ctr, &kp_msg_format_ctr);
					#else
						app_hci_save_sit_adv_data_all( pt_dtpacket_saving_ctr, &kp_msg_format_ctr);
						DBG_LOG_DEBUG("Insight-T detected %d, the dtunit on waiting queue %d\n", l_queue_length( pt_dtpacket_saving_ctr->pt_queue), l_queue_length( pt_dtpacket_saving_ctr->pt_waiting_queue));
					#endif
				#endif
				/*.........................*/

				break;
			}
			/*....*/
			default:
			{
				DBG_LOG_ERR("unexpected msg type");
				break;
			}
		}
	}
}

// Deprecated
#if 0
/*try to write Insight-T advertising data to database

   NOTE!!! this function is not thread safe
   2. only temperature is written to DB
 */

static app_state_t
app_hci_save_sit_adv_data(
  struct sit_dtpacket_saving_controller *pt_dtpacket_saving_ctr,
  struct msg_format_controller *pt_msg_format_ctr)
{
  app_state_t tpret = ST_OK;
  struct sit_dtpacket *pt_sit_dtpkt = NULL;

  static struct sit_dtpacket *hld_dtpkt_to_written = NULL;

  const uint32_t kp_tsamp = time(NULL);

  if(NULL == pt_dtpacket_saving_ctr)
  {
    DBG_LOG_ERR("unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  while(1)
  {
    pt_sit_dtpkt = (struct sit_dtpacket *)l_queue_remove_if(
      pt_dtpacket_saving_ctr->pt_queue, q_match_remove_adv_dataunit,
      (const void *)kp_tsamp);
    if(NULL == pt_sit_dtpkt)
    {
      break;
    }

#if (1)  // only for debug
    if(l_queue_length(pt_dtpacket_saving_ctr->pt_waiting_queue) > 0x100)
    {
      DBG_LOG_ERR(
        "too much data on waiting queue, we just discard the coming data");
      l_free(pt_sit_dtpkt);
      pt_sit_dtpkt = NULL;
      continue;
    }
    else
    {
      DBG_LOG_WARN("to put dtunit on waiting queue");
    }
#endif

    if(false ==
       l_queue_push_tail(pt_dtpacket_saving_ctr->pt_waiting_queue,
                         (void *)pt_sit_dtpkt))
    {
      DBG_LOG_ERR(
        "fail to put dataunit on waiting queue, before writting to database");
      l_free(pt_sit_dtpkt);
      pt_sit_dtpkt = NULL;
    }
  }

  DBG_LOG_WARN(
    "Insight-T detected %d, the dtunit on waiting queue %d, dtpkt_to_written@0x%x",
    l_queue_length(pt_dtpacket_saving_ctr->pt_queue),
    l_queue_length(
      pt_dtpacket_saving_ctr->pt_waiting_queue), hld_dtpkt_to_written);
#if (0)  // for debug only , to dump the queue
  struct l_queue_entry *pt_entry = l_queue_get_entries(
    pt_dtpacket_saving_ctr->pt_queue);
  struct sit_dtpacket *pt_temp_dtpkt = NULL;
  uint32_t tp_cnt = 0;
  while(pt_entry)
  {
    pt_temp_dtpkt = (struct sit_dtpacket *)pt_entry->data;
    //DBG_LOG_INFO("I_T %d %.2X%.2X%.2X%.2X%.2X%.2X", tp_cnt,
    // pt_temp_dtpkt->addr[0],pt_temp_dtpkt->addr[1],pt_temp_dtpkt->addr[2],pt_temp_dtpkt->addr[3],pt_temp_dtpkt->addr[4],pt_temp_dtpkt->addr[5]);
    printf("\nI_T %d %.2X%.2X%.2X%.2X%.2X%.2X\n", tp_cnt,
           pt_temp_dtpkt->addr[0], pt_temp_dtpkt->addr[1],
           pt_temp_dtpkt->addr[2], pt_temp_dtpkt->addr[3],
           pt_temp_dtpkt->addr[4], pt_temp_dtpkt->addr[5]);
    tp_cnt++;
    pt_entry = pt_entry->next;
  }
#endif
  //to write data from Insight-T to database

  if(NULL != hld_dtpkt_to_written)
  {
    if(ST_OK !=
       app_db_write_sit_data_to_db(app_db_get_dbop_controller(),
                                   pt_msg_format_ctr,
                                   hld_dtpkt_to_written))
    {
      tpret = ST_ERR;
      goto EXIT;
    }
    else
    {
      l_free(hld_dtpkt_to_written);
      hld_dtpkt_to_written = NULL;
    }
  }
  while(1)
  {
    hld_dtpkt_to_written = l_queue_pop_head(
      pt_dtpacket_saving_ctr->pt_waiting_queue);
    if(NULL == hld_dtpkt_to_written)
    {
      break;
    }
    if(ST_OK !=
       app_db_write_sit_data_to_db(app_db_get_dbop_controller(),
                                   pt_msg_format_ctr,
                                   hld_dtpkt_to_written))
    {  // fail to write
      tpret = ST_ERR;
      goto EXIT;
    }
    else
    {
      l_free(hld_dtpkt_to_written);
      hld_dtpkt_to_written = NULL;
    }
  }

EXIT:
  return tpret;
}
#endif // Deprecated




