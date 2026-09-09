/* purpose of this process:
1.load/set app configuration

2.communicate to sensors;

3.communicate to APP

4.DB operation

5. Protocol 

......

*/
#include "app_pro_general.h"

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>

#include "pb.h"
#include "DeviceAppBulletGateway.pb.h"

#include "app_general_def.h"
#include "util_dbg.h"
#include "util_froto.h"
#include "app_waiting_queue.h"
#include "util_froto.h"
#include "app_froto.h"
#include "app_db.h"

#include "global.h"
#include "ppGW.h"
#include "list.h"
#include "gwConfig.h"
#include "sensorConfig.h"

#include "app_gw_scheduler.h"
#include "app_cjson.h"
#include "app_tim_service.h"
#include "app_io.h"
#include "app_common.h"

#include "app_hci_evt_mgmt.h"
#include "bt_hcitool.h"

DBG_LOCAL_LOG_DEBUG


#define SENSOR_LIST_READING_FROM_DB (0)
#define SENSOR_LIST_DEFINED_BY_MACRO (1)

#define SENSOR_LIST_MODE SENSOR_LIST_READING_FROM_DB
//#define SENSOR_LIST_MODE SENSOR_LIST_DEFINED_BY_MACRO

#if (SENSOR_LIST_MODE == SENSOR_LIST_DEFINED_BY_MACRO)
// C4:BD:6A:11:00:00, C4:BD:6A:11:00:01, C4:BD:6A:11:00:02
#define SENSOR_LIST_MACRO {{0xC4BD, 0x6A, 0x110000},\
                           {0xC4BD, 0x6A, 0x110001},\
						   {0xC4BD, 0x6A, 0x110021}}
#define SENSOR_NAMEID_MACRO { 101, 102, 103}
#endif

#define PRODUCT_MODE (1)
#define ALL_DATA_SELECTION_TEST_MODE (2)
#define LONG_DATA_SELECTION_TEST_MODE (3)
#define CURRENT_MODE PRODUCT_MODE


//extern uint32_t g_tpval;


/*data structure for process-general controlling*/ 
static struct pro_general_controller g_pro_general_ctr = {0};

/*define queue for data to be sent to process-BLE
*/
static struct waiting_queue_controller g_wq_ipc_msg_ctr = {0};

/*to get the waiting message controller
*/
struct waiting_queue_controller *app_pro_gen_get_wq_ipc_msg_ctr(void)
{
	return &g_wq_ipc_msg_ctr;
}


static void pro_general_create_threads(
  struct pro_general_controller *pt_ctr);

/*to initialize the process controller
*/
void app_pro_gen_init_ctr(struct pro_general_controller*pt_ctr)
{
	pt_ctr->hld_msg_from_pro_ble = -1;
	pt_ctr->hld_msg_to_pro_ble = -1;

	// set the default BT role
	pt_ctr->bt_role = BT_ROLE_DEFAULT;

	// 
	pthread_mutex_init( &pt_ctr->mtx_for_bt_sw, NULL);
	pt_ctr->bt_sw_in_progress = false;

	// 
	pthread_mutex_init(&pt_ctr->mtx_for_seq_no, NULL);
	pt_ctr->seq_no_counter = 0;
	
	//
	
	pthread_mutex_init(&((sys_get_gwConfig_and_dev())->mtx_for_gw_conf_info), NULL);
	memset( &((sys_get_gwConfig_and_dev())->gw_conf_info), 0, sizeof(gwConfig_t));
	#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)	
		INIT_LIST_HEAD( &((sys_get_gwConfig_and_dev())->gw_conf_info.gwConfig.childrenList));
	#endif


	//
	memset( pt_ctr->dev_array, 0, sizeof(struct devinfo_node)*MAX_BT_WHITE_LIST);

	return;
}

/*to deinitialize the process controller, to release some resource here
*/
void app_pro_gen_deinit_ctr(struct pro_general_controller*pt_ctr)
{

	// to destroy the mutex
	pthread_mutex_destroy( &pt_ctr->mtx_for_bt_sw);

	pthread_mutex_destroy( &pt_ctr->mtx_for_seq_no);

	pthread_mutex_lock( &((sys_get_gwConfig_and_dev())->mtx_for_gw_conf_info));
	app_cjson_release_chilren_list(&((sys_get_gwConfig_and_dev())->gw_conf_info));
	pthread_mutex_unlock( &((sys_get_gwConfig_and_dev())->mtx_for_gw_conf_info));
	pthread_mutex_destroy( &((sys_get_gwConfig_and_dev())->mtx_for_gw_conf_info));

	return;
}

#if (SKF_GW_NEW != 1)
/**
 * @brief To allocate an unique message sequence number (increasing by 1
 * once the routine is called)
 * @return An allocated message sequence number
 */
uint32_t
app_pro_gen_allocate_msg_seq_no(void)
{
  uint32_t tpret = 0;
  struct pro_general_controller *pt_ctr = app_pro_gen_get_ctr();

  pthread_mutex_lock(&pt_ctr->mtx_for_seq_no);
  pt_ctr->seq_no_counter++;
  tpret = pt_ctr->seq_no_counter;
  pthread_mutex_unlock(&pt_ctr->mtx_for_seq_no);

  return tpret;
}
struct pro_general_controller *
app_pro_gen_get_ctr(void)
{
  return &g_pro_general_ctr;
}
#endif


app_state_t app_pro_gen_set_active_client_id_str( uint8_t*pt_client_addr_str, uint8_t kp_buf_sz)
{
	uint32_t i = 0, idx = 0;
	if((NULL == pt_client_addr_str)||(0 == kp_buf_sz))
	{
		return ST_ERR;
	}
	while(pt_client_addr_str[i % kp_buf_sz])
	{
		if(pt_client_addr_str[i % kp_buf_sz] != ':')
		{
			g_pro_general_ctr.active_client_id_str[idx % MAX_BT_ADDRSTR] = pt_client_addr_str[i % kp_buf_sz];
			idx ++;
		}
		i++;
		if(i > kp_buf_sz)
		{
			break;
		}
	}
	g_pro_general_ctr.active_client_id_str[ MAX_BT_ADDRSTR-1 ] = 0;
	
	DBG_LOG_INFO("the active client id str is set to %s", g_pro_general_ctr.active_client_id_str);
	return ST_OK;
}

//
void app_pro_gen_get_active_client_id_str(uint8_t*pt_active_id_str_buf, uint8_t kp_bufsz)
{
	if((NULL == pt_active_id_str_buf)||(0 == kp_bufsz))
	{
		return;
	}
	snprintf( pt_active_id_str_buf, kp_bufsz, "%s", g_pro_general_ctr.active_client_id_str);
	DBG_LOG_INFO("the client_id_str get is : %s", pt_active_id_str_buf);
	return;
}

/*
*/
enum bluetooth_role app_pro_gen_get_bluetooth_role( struct pro_general_controller*pt_ctr)
{
	enum bluetooth_role tpret = BT_ROLE_UNKNOWN;
	if(NULL == pt_ctr)
	{
		DBG_LOG_ERR("unexpected BT role");
		return tpret;
	}
	return pt_ctr->bt_role;
}



/*to set the msgid for IPC
*/
app_state_t app_pro_gen_set_msgid( struct pro_general_controller *pt_ctr, int msgid_in, int msgid_out)
{
	if((msgid_in < 0)||( msgid_out < 0))
	{
		DBG_LOG_ERR("invalid parameter");
		return ST_ERR;
	}
	pt_ctr->hld_msg_from_pro_ble = msgid_in;
	pt_ctr->hld_msg_to_pro_ble = msgid_out;
	return ST_OK;
}


/*to get the msgid
*/
int app_pro_gen_get_msgid_to_pro_ble(struct pro_general_controller*pt_ctr)
{
	return pt_ctr->hld_msg_to_pro_ble;
}

int app_pro_get_msgid_from_pro_ble(struct pro_general_controller*pt_ctr)
{
	return pt_ctr->hld_msg_from_pro_ble;
}

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
/* TODO: to get the BT connections' count ? to be confirmed.
*/
bool app_pro_gen_get_connection_state(struct pro_general_controller*pt_gen_ctr, uint8_t idx)
{
	return pt_gen_ctr->bt_connection_flg[idx];
}
#else
/*to get the BT connection flag
*/
bool app_pro_gen_get_connection_state(struct pro_general_controller*pt_gen_ctr)
{
	return pt_gen_ctr->bt_connection_flg;
}
#endif	// "SKF_GW_CONFIG_MULTI_CONNECTION == 1"

/*
*/
void app_pro_gen_set_gateway_idstr(struct pro_general_controller*pt_gen_ctr, const uint8_t *pt_idstr)
{
	if((pt_gen_ctr == NULL)||(pt_idstr == NULL))
	{
		DBG_LOG_ERR("unexpected NULL");
		return;
	}
	snprintf( pt_gen_ctr->gw_id_str, MAX_ID_STR_LENGTH,"%s", pt_idstr);
	return;
}

/*
*/
uint8_t * app_pro_gen_get_gateway_idstr(struct pro_general_controller*pt_gen_ctr)
{
	if(NULL == pt_gen_ctr)
	{
		return NULL;
	}
	return pt_gen_ctr->gw_id_str;
}


/*
pt_gen_ctr@ the general controller
pt_stype @ the sensor type ref@TYPE_STR_INSIGHT_???
pt_name_id @ the array buffer for device namdID (set to NULL ,when you DO NOT care about the name ID)
pt_addr_array @ the array buffer for device MAC info
array_sz @ the array buffer size , NOTE !!! the number of array item
ret@ the number of MAC found which match to "pt_stype"
*/
uint32_t app_pro_gen_get_mac_array(const struct pro_general_controller*pt_gen_ctr, const uint8_t*pt_stype, uint32_t *pt_name_id, struct bt_addr*pt_addr_array,const uint32_t array_sz)
{
	uint32_t kpidx = 0;
	const struct devinfo_node *hld_p_dev_array = pt_gen_ctr->dev_array;
	
	if((NULL == pt_gen_ctr)||(NULL == pt_stype)||(NULL == pt_addr_array)||(array_sz <= 0))
	{
		DBG_LOG_ERR("invalid parameter");
		return 0; 
	}

	for(uint32_t i = 0;i < MAX_BT_WHITE_LIST; i++)
	{
		if((strlen(pt_stype) == strlen(hld_p_dev_array[i].stype)) && (0 == memcmp( pt_stype, hld_p_dev_array[i].stype, strlen(pt_stype))))
		{ // match
			pt_addr_array[kpidx % array_sz].nap = ((uint16_t)hld_p_dev_array[i].mac_bytes[0])*0x100 +\
												  hld_p_dev_array[i].mac_bytes[1];

			pt_addr_array[kpidx % array_sz].uap = hld_p_dev_array[i].mac_bytes[2];

			pt_addr_array[kpidx % array_sz].lap = ((uint32_t)hld_p_dev_array[i].mac_bytes[3])*0x100*0x100 +\
												  ((uint32_t)hld_p_dev_array[i].mac_bytes[4])*0x100 + \
												  hld_p_dev_array[i].mac_bytes[5];
			if(pt_name_id)
			{
				pt_name_id[kpidx % array_sz] = hld_p_dev_array[i].nameid;
			}
			
			kpidx = kpidx + 1;
		}
	}
	
	return kpidx;
}


/*
pt_gen_ctr @ 
pt_mac_str @ the buffer where the MAC string of active sensor is
return @ the sensor type is returned

NOTE!!! the format of MAC string is "112233445566", no colon in the middle ref@xx->active_client_id_str
*/
enum sensortype app_pro_gen_get_active_sensor_type(const struct pro_general_controller*pt_gen_ctr , const uint8_t *pt_mac_str)
{
	enum sensortype tpret = SS_TYPE_UNKNOWN;
	uint8_t strbuf[MAX_BT_ADDRSTR] = {0};
	//DBG_LOG_ERR("BKP338");
	if((NULL == pt_gen_ctr)||(NULL == pt_mac_str))
	{
		DBG_LOG_ERR("unexpected NULL");
		return tpret;
	}
	
	//DBG_LOG_ERR("BKP338");
	for(uint32_t i = 0; i < MAX_BT_WHITE_LIST; i++)
	{
		memset( strbuf, 0, MAX_BT_ADDRSTR);
		snprintf( strbuf, MAX_BT_ADDRSTR, "%.2X%.2X%.2X%.2X%.2X%.2X", pt_gen_ctr->dev_array[i%MAX_BT_WHITE_LIST].mac_bytes[0],\
														  pt_gen_ctr->dev_array[i%MAX_BT_WHITE_LIST].mac_bytes[1],\
														  pt_gen_ctr->dev_array[i%MAX_BT_WHITE_LIST].mac_bytes[2],\
														  pt_gen_ctr->dev_array[i%MAX_BT_WHITE_LIST].mac_bytes[3],\
														  pt_gen_ctr->dev_array[i%MAX_BT_WHITE_LIST].mac_bytes[4],\
														  pt_gen_ctr->dev_array[i%MAX_BT_WHITE_LIST].mac_bytes[5]);

		//DBG_LOG_ERR("strbuf %s", strbuf);
		
		// to check if the MAC string match
		if((strlen(strbuf) != strlen( pt_mac_str))||(0 != memcmp( strbuf, pt_mac_str, strlen(strbuf))))
		{ // DO NOT MATCH
			continue;
		}

		
		if((strlen(TYPE_STR_INSIGHT_T) == strlen(pt_gen_ctr->dev_array[i%MAX_BT_WHITE_LIST].stype))&&\
			(0 == memcmp( TYPE_STR_INSIGHT_T, pt_gen_ctr->dev_array[i % MAX_BT_WHITE_LIST].stype, strlen(TYPE_STR_INSIGHT_T))))
		{
			tpret = SS_TYPE_INSIGHT_T;
		}
		else if((strlen(TYPE_STR_INSIGHT_P)==strlen(pt_gen_ctr->dev_array[i % MAX_BT_WHITE_LIST].stype))&&\
			(0 == memcmp( TYPE_STR_INSIGHT_P, pt_gen_ctr->dev_array[i % MAX_BT_WHITE_LIST].stype, strlen(TYPE_STR_INSIGHT_P))))
		{
			tpret = SS_TYPE_PREDICT;
		}
		else if((strlen(TYPE_STR_INSIGHT_PP) == strlen(pt_gen_ctr->dev_array[i % MAX_BT_WHITE_LIST].stype))&&\
			(0 == memcmp(TYPE_STR_INSIGHT_PP, pt_gen_ctr->dev_array[i % MAX_BT_WHITE_LIST].stype, strlen(TYPE_STR_INSIGHT_PP))))
		{
			tpret = SS_TYPE_PREDICTP;
		}
		else
		{
			tpret = SS_TYPE_UNKNOWN;
		}
		
		DBG_LOG_ERR("id_str %s vs %s, the sensor_type %d, stype %s", pt_mac_str, strbuf,tpret, pt_gen_ctr->dev_array[i%MAX_BT_WHITE_LIST].stype);

		break;
	}
	
	return tpret;
}

/* to get the active sensor type
ret@ SS_TYPE_???
*/
enum sensortype app_pro_gen_get_active_sensor_type_v2(void)
{
	enum sensortype tpret = SS_TYPE_UNKNOWN;
	uint8_t kp_id_strbuf[MAX_BT_ADDRSTR] = {0};

	app_pro_gen_get_active_client_id_str( kp_id_strbuf, MAX_BT_ADDRSTR);
	//DBG_LOG_ERR("ctr %s", kp_id_strbuf); 
	tpret = app_pro_gen_get_active_sensor_type( app_pro_gen_get_ctr(), kp_id_strbuf);
	
	return tpret;
}
/*to update the gw-conf except child-list
*/
app_state_t app_pro_gen_update_gwconf_except_childlist(gwconf_and_dev_t *pt_gwconf_and_dev, gwConfig_t *pt_gwcfg)
{
	app_state_t tpret = ST_OK;
	#if(GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
		uint16_t kp_child_num = 0;
		struct list_head kp_childlist;
		INIT_LIST_HEAD( &kp_childlist);
	#else
		#error "some modification is expected here"
	#endif
	if((NULL == pt_gwconf_and_dev)||(NULL == pt_gwcfg))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}

	#if(GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
		// to back up the child-list
		pthread_mutex_lock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
		kp_child_num = pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenNumber;
		// next
		kp_childlist.next = pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList.next;
		pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList.next->prev = &kp_childlist;
		//previous
		kp_childlist.prev = pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList.prev;
		pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList.prev->next = &kp_childlist;
		
		pthread_mutex_unlock( &pt_gwconf_and_dev->mtx_for_gw_conf_info);
	#else
		#error "some modification is expected here"
	#endif

	// to update the gw-conf
	pthread_mutex_lock(&pt_gwconf_and_dev->mtx_for_gw_conf_info);
	memset( &pt_gwconf_and_dev->gw_conf_info, 0, sizeof(gwConfig_t));
	memcpy( &pt_gwconf_and_dev->gw_conf_info, pt_gwcfg, sizeof(gwConfig_t));
	// to recover the child-list
	#if(GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
		pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenNumber = kp_child_num;
		// next
		pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList.next = kp_childlist.next;
		kp_childlist.next->prev = &pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList;
		// previous
		pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList.prev = kp_childlist.prev;
		kp_childlist.prev->next = &pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList;
	#else	
		#error "some modification is expected here"
	#endif
	pthread_mutex_unlock( &pt_gwconf_and_dev->mtx_for_gw_conf_info);

	DBG_LOG_ERR("the NTP server is updated to %s, from %s", pt_gwconf_and_dev->gw_conf_info.gwConfig.timeConfig.timeNtpUrl, pt_gwcfg->gwConfig.timeConfig.timeNtpUrl);

	
	EXIT:
		return tpret;
}

/*to update the child-list for general controller

NOTE!!! only the child-list part is updated

*/
app_state_t app_pro_gen_update_gwconf_childlist(gwconf_and_dev_t *pt_gwconf_and_dev, gwConfig_t *pt_gwcfg)
{
	app_state_t tpret = ST_OK;

	if((NULL == pt_gwconf_and_dev)||(NULL == pt_gwcfg))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}

	DBG_LOG_ERR("BKP324");
	
	pthread_mutex_lock( &pt_gwconf_and_dev->mtx_for_gw_conf_info);
	
	// to release the old one , before we set to the new
	app_cjson_release_chilren_list( &pt_gwconf_and_dev->gw_conf_info);
	pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenNumber = pt_gwcfg->gwConfig.childrenNumber;
	//next
	pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList.next = pt_gwcfg->gwConfig.childrenList.next;
	pt_gwcfg->gwConfig.childrenList.next->prev = &pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList;
	//prev
	pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList.prev = pt_gwcfg->gwConfig.childrenList.prev;
	pt_gwcfg->gwConfig.childrenList.prev->next = &pt_gwconf_and_dev->gw_conf_info.gwConfig.childrenList;

	pthread_mutex_unlock( &pt_gwconf_and_dev->mtx_for_gw_conf_info);	

	DBG_LOG_ERR("BKP341");
	
	EXIT:
		return tpret;
}

#if(1)
// create threads

	// #if(0)
	// 	/*to detect the state of IO
	// 	just wait for the mement and swtich
	// 	*/
	// 	static void *thandler_io_state(void*pt_para)
	// 	{
	// 		uint32_t kp_cnt = 0;
	// 		struct pro_general_controller *pt_gen_ctr = NULL;
	// 		enum bluetooth_role kp_btrole = BT_ROLE_DEFAULT;	
	// 		struct ipc_msg_v2 kp_ipc_msg = {0};

	// 		pt_gen_ctr = app_pro_gen_get_ctr();
	// 		while(1)
	// 		{		
	// 			DBG_LOG_INFO("===this is thread-io\n");		
	// 			sleep(180);

	// 			#if(1) // for debug only
	// 				//continue;
	// 			#endif

				
	// 			#warning "TODO === Add tasks you need to do here"
	// 			// to run a nano-PB encoding/decoding test
	// 			//util_froto_test();

	// 			// send message to switch the BT mode(client/server)
	// 			// to check if the BT mode switch is in progress, just ignore the siginal when it is in progress
	// 			DBG_LOG_INFO("to check if there is any SW in progress , in progress %d", pt_gen_ctr->bt_sw_in_progress);
	// 			pthread_mutex_lock( &pt_gen_ctr->mtx_for_bt_sw);
	// 			if(pt_gen_ctr->bt_sw_in_progress)
	// 			{// BT sw is in progress
	// 				pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);
	// 				DBG_LOG_INFO("BT SW is in progress");
	// 				continue;
	// 			}
	// 			else
	// 			{
	// 				pt_gen_ctr->bt_sw_in_progress = true;
	// 			}
	// 			pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);

	// 			DBG_LOG_INFO(" to put the switch command on the queue");
				
	// 			memset( &kp_ipc_msg, 0, sizeof(struct ipc_msg_v2));
	// 			kp_ipc_msg.mtype = M_TYPE_COMMAND;
	// 			if(kp_btrole == BT_ROLE_CLIENT)
	// 			{
	// 				kp_btrole = BT_ROLE_SERVER;		
	// 				kp_ipc_msg.mtext.cmd_info.cmd = CMD_SW_TO_SERVER;					
	// 			}
	// 			else
	// 			{
	// 				kp_btrole = BT_ROLE_CLIENT;
	// 				kp_ipc_msg.mtext.cmd_info.cmd = CMD_SW_TO_CLIENT;					
	// 			}
				
	// 			kp_ipc_msg.mtext.cmd_info.msgid = kp_cnt;
	// 			kp_ipc_msg.mtext.cmd_info.para.tpu32 = 666;
	// 			if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	// 			{
	// 				DBG_LOG_ERR("fail to put CMD_SW_TO_xxx on queue");
	// 				// to reset the in_progress flag
	// 				pthread_mutex_lock( &pt_gen_ctr->mtx_for_bt_sw);
	// 				pt_gen_ctr->bt_sw_in_progress = false;
	// 				pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);
	// 			}
	// 			else
	// 			{
	// 				DBG_LOG_INFO("***CMD_SW_TO_XXX is put on queue");
	// 			}
	// 		}
	// 	}
	// #else
	// 	/*1.wait for the key-press, switch to BT-server
	// 	  2.switch back to BT-client when BT connection is lost or timeout for no connection from APP
	// 	*/
	// 	static void *thandler_io_state(void*pt_para)
	// 	{
	// 		uint32_t kp_cnt = 0;
	// 		bool flg_is_connected_to_client = false;
	// 		struct pro_general_controller *pt_gen_ctr = NULL;
	// 		struct ipc_msg_v2 kp_ipc_msg = {0};

	// 		pt_gen_ctr = app_pro_gen_get_ctr();
	// 		while(1)
	// 		{		
	// 			DBG_LOG_INFO("===this is thread-io\n");		
	// 			if(K_STATE_RELEASED != app_io_key_get_state())
	// 			{
	// 				sleep(1);
	// 				continue;
	// 			}
				
	// 			if(BT_ROLE_SERVER == app_pro_gen_get_bluetooth_role( pt_gen_ctr))
	// 			{
	// 				DBG_LOG_WARN(" the BT-role is %d, nothing to do", pt_gen_ctr->bt_role)
	// 				continue;
	// 			}
				
	// 			// send message to switch the BT mode(client/server)
	// 			// to check if the BT mode switch is in progress, just ignore the siginal when it is in progress
	// 			DBG_LOG_INFO("to check if there is any SW in progress , in progress %d", pt_gen_ctr->bt_sw_in_progress);
	// 			pthread_mutex_lock( &pt_gen_ctr->mtx_for_bt_sw);
	// 			if(pt_gen_ctr->bt_sw_in_progress)
	// 			{// BT sw is in progress
	// 				pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);
	// 				DBG_LOG_INFO("BT SW is in progress");
	// 				continue;
	// 			}
	// 			else
	// 			{
	// 				pt_gen_ctr->bt_sw_in_progress = true;
	// 			}
	// 			pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);

	// 			flg_is_connected_to_client = app_pro_gen_get_connection_state( pt_gen_ctr);

	// 			DBG_LOG_INFO(" to put the switch command on the queue to switch to BT-server, flg_connected=%d", flg_is_connected_to_client);
	// 			memset( &kp_ipc_msg, 0, sizeof(struct ipc_msg_v2));
	// 			kp_ipc_msg.mtype = M_TYPE_COMMAND;
	// 			kp_ipc_msg.mtext.cmd_info.cmd = CMD_SW_TO_SERVER;					
								
	// 			kp_ipc_msg.mtext.cmd_info.msgid = app_pro_gen_allocate_msg_seq_no();
	// 			kp_ipc_msg.mtext.cmd_info.para.tpu32 = 666;
	// 			if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	// 			{
	// 				DBG_LOG_ERR("fail to put CMD_SW_TO_xxx on queue");
	// 				// to reset the in_progress flag
	// 				pthread_mutex_lock( &pt_gen_ctr->mtx_for_bt_sw);
	// 				pt_gen_ctr->bt_sw_in_progress = false;
	// 				pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);

	// 				// the msg is not sent successfully, no need to run the following procedure anymore
	// 				continue;
	// 			}
	// 			else
	// 			{
	// 				DBG_LOG_INFO("***CMD_SW_TO_XXX is put on queue");
	// 			}

	// 			if(flg_is_connected_to_client)
	// 			{// wait for disconnection from sensor
	// 				kp_cnt = 0;
	// 				DBG_LOG_WARN("to wait for disconnection from sensor @ %d", time(NULL));
	// 				while(true == app_pro_gen_get_connection_state( pt_gen_ctr))
	// 				{
	// 					sleep(1);
	// 					if(kp_cnt >= MAX_BT_BROADCAST)
	// 					{// disconnection is expected to happen
	// 						DBG_LOG_ERR("Well! something might be wrong when you see this output");
	// 						break;
	// 					}
	// 					kp_cnt++;
	// 				}
	// 			}

	// 			/*to wait for APP opeation*/
	// 			DBG_LOG_INFO("You only have 60S to create a connection to GW from APP @ %d", time(NULL));
	// 			kp_cnt = 0;
	// 			while(true != app_pro_gen_get_connection_state( pt_gen_ctr))
	// 			{
	// 				kp_cnt++;
	// 				sleep(1);
	// 				if(kp_cnt >= MAX_BT_BROADCAST)
	// 				{
	// 					break;
	// 				}
	// 			}

	// 			// to wait for APP to disconnect from GW
	// 			DBG_LOG_WARN(" there is a risk that we run into an endless loop here @ %d" , time(NULL));
	// 			while(false != app_pro_gen_get_connection_state( pt_gen_ctr))
	// 			{
	// 				sleep(1);
	// 			}

	// 			// to switch back BT-client if necessary
	// 			if(BT_ROLE_SERVER != app_pro_gen_get_bluetooth_role( pt_gen_ctr))
	// 			{
	// 				DBG_LOG_WARN(" You fail to switch to BT-server when you see this output");

	// 				//to reset the in_process flag
	// 				pthread_mutex_lock( &pt_gen_ctr->mtx_for_bt_sw);
	// 				pt_gen_ctr->bt_sw_in_progress = false;
	// 				pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);
	// 				continue;
	// 			}
	// 			else
	// 			{// to switch back BT-client
	// 				DBG_LOG_INFO("to check if there is any SW in progress , in progress %d", pt_gen_ctr->bt_sw_in_progress);
					
	// 				pthread_mutex_lock( &pt_gen_ctr->mtx_for_bt_sw);
	// 				if(pt_gen_ctr->bt_sw_in_progress)
	// 				{// BT sw is in progress
	// 					pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);
	// 					DBG_LOG_INFO("BT SW is in progress");
	// 					continue;
	// 				}
	// 				else
	// 				{
	// 					pt_gen_ctr->bt_sw_in_progress = true;
	// 				}
	// 				pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);
					
				
	// 				DBG_LOG_INFO(" to put the switch command on the queue to wait switch to BT-client @ %d", time(NULL));
	// 				memset( &kp_ipc_msg, 0, sizeof(struct ipc_msg_v2));
	// 				kp_ipc_msg.mtype = M_TYPE_COMMAND;
	// 				kp_ipc_msg.mtext.cmd_info.cmd = CMD_SW_TO_CLIENT;

	// 				kp_ipc_msg.mtext.cmd_info.msgid = app_pro_gen_allocate_msg_seq_no();
	// 				kp_ipc_msg.mtext.cmd_info.para.tpu32 = 666; // any value you want
	// 				if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	// 				{
	// 					// to relset the flag
	// 					pthread_mutex_lock( &pt_gen_ctr->mtx_for_bt_sw);
	// 					pt_gen_ctr->bt_sw_in_progress = false;
	// 					pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);
	// 					continue;	
	// 				}
					
	// 			}
			
	// 		}
	// 	}
	// #endif
static struct msg_format_controller g_dbg_msg_format_ctr = {0};
static bool key_pressed_fg = false;

static void *thandler_io_state(void *pt_para);
static void *thandler_io_state_assist(void *pt_para);
static void *thandler_db_operator(void *pt_para);
static void *thandler_operator(void *pt_para);

/**
 * @brief The thread monitoring the key-press.
 */
static void *
thandler_io_state_assist(void *pt_para)
{
  while(1)
  {
    if(K_STATE_RELEASED == app_io_key_get_state())
    {
      key_pressed_fg = true;
      DBG_LOG_DEBUG("Key pressed\n\r");
    }
  }
}
/**
 * @brief The thread monitoring the key-press. To switch to BT-Server once
 * the key pressed and to switch back to BT-Client when the BT
 * connection terminates or advertising timeout happens (without APP
 * connection).
 */
static void *
thandler_io_state(void *pt_para)
{
  uint32_t kp_cnt = 0;
  bool flg_is_connected_to_client = false;
  struct pro_general_controller *pt_gen_ctr = NULL;
  int16_t countdown_s = 0;
  uint8_t state = 0;
  struct ipc_msg_v2 kp_ipc_msg = { 0 };

  pt_gen_ctr = app_pro_gen_get_ctr();
  while(1)
  {
    DBG_LOG_INFO("The thread of IO is running\n\r");

    sleep(1);
    if(countdown_s > 0)
    {
      countdown_s--;
    }

    if(key_pressed_fg == true)
    {
      // If the key has been pressed ...
      key_pressed_fg = false;
      countdown_s = MAX_BT_BROADCAST;
      DBG_LOG_DEBUG("Set countdown_s to %u\n\r", MAX_BT_BROADCAST);
      state = 1;  // Switch to "1" state: switch to server mode
    }

    DBG_LOG_DEBUG("state %d\n\r", state);
    switch(state)
    {
      case 1:  // Once the key has been pressed
      {
        if(countdown_s <= 0)
        {
          // Timeout
          countdown_s = 0;
          state = 99;
          // Force to post a msg to the queue to switch to client
          pt_gen_ctr->bt_sw_in_progress = false;
          DBG_LOG_DEBUG("Switch to server role timeout!\n\r");
        }

        // To check BT - role:BT_ROLE_SERVER or BT_ROLE_CLIENT
        if(BT_ROLE_SERVER ==
           app_pro_gen_get_bluetooth_role(pt_gen_ctr))
        {
          // If BT-role has been switched to the server role ...
          DBG_LOG_DEBUG("The BT-role has been server\n\r");
          state = 2;
        }
        else
        {
          // Otherwise, switch to the server role
          DBG_LOG_DEBUG("To check if there is any SW in progress, %d\n\r",
                        pt_gen_ctr->bt_sw_in_progress);
          pthread_mutex_lock(&pt_gen_ctr->mtx_for_bt_sw);
          if(pt_gen_ctr->bt_sw_in_progress)
          {
            // BT sw is in progress
            pthread_mutex_unlock(&pt_gen_ctr->mtx_for_bt_sw);
            state = 1;
            DBG_LOG_INFO("BT SW is in progress\n\r");
          }
          else
          {
            // Otherwise, send message to switch BT-role to the server role
            pt_gen_ctr->bt_sw_in_progress = true;
            memset(&kp_ipc_msg, 0, sizeof(kp_ipc_msg));
            kp_ipc_msg.mtype = M_TYPE_COMMAND;
            kp_ipc_msg.mtext.cmd_info.cmd = CMD_SW_TO_SERVER;
            kp_ipc_msg.mtext.cmd_info.msgid =
              app_pro_gen_allocate_msg_seq_no();
            kp_ipc_msg.mtext.cmd_info.para.tpu32 = 0;  // No parameter is
            										   // required
            if(ST_OK !=
               app_wq_post_to_waiting_queue(app_pro_gen_get_wq_ipc_msg_ctr(),
                                            (uint8_t *)&kp_ipc_msg,
                                            sizeof(kp_ipc_msg)))
            {
              // To reset the in_progress flag
              pt_gen_ctr->bt_sw_in_progress = false;
              pthread_mutex_unlock(&pt_gen_ctr->mtx_for_bt_sw);

              // To clear the countdown
              countdown_s = 0;
              state = 99;

              DBG_LOG_ERR(
                "Failed to post CMD_SW_TO_SERVER on the queue\n\r");
            }
            else
            {
              pthread_mutex_unlock(&pt_gen_ctr->mtx_for_bt_sw);
              state = 1;
              DBG_LOG_DEBUG("CMD_SW_TO_SERVER has been posted\n\r");
            }
          }
        }
      }
      break;
      case 2:  // Server role (but has not been connected)
      {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
		if(false == app_pro_gen_get_connection_state(pt_gen_ctr, 0))	// idx to be confirmed.
#else
        if(app_pro_gen_get_connection_state(pt_gen_ctr) == false)
#endif
        {
          // If has not been connected ...
          if(countdown_s <= 0)
          {
            // Timeout
            countdown_s = 0;
            state = 99;
            // Force to post a msg to the queue to switch to client
            pt_gen_ctr->bt_sw_in_progress = false;
            DBG_LOG_DEBUG("Server role timeout!\n\r");
          }
          else
          {
            // Keep waiting
            state = 2;
            DBG_LOG_DEBUG("Server role waiting for connection, %d\n\r",
                          countdown_s);
          }
        }
        else
        {
          // Just has been connected
          state = 3;
          DBG_LOG_DEBUG(
            "Connection has been established in server role\n\r");
        }
      }
      break;
      case 3:  // Server role (has been connected)
      {
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
		if(false == app_pro_gen_get_connection_state(pt_gen_ctr, 0))	// idx to be confirmed.
#else
        if(app_pro_gen_get_connection_state(pt_gen_ctr) == false)
#endif
        {
          // If disconnected, then switch to client role
          state = 99;
          // Force to post a msg to the queue to switch to client
          pt_gen_ctr->bt_sw_in_progress = false;
          countdown_s = 0;
          DBG_LOG_DEBUG("Disconnection happens in server role\n\r");
        }
        else
        {
          // Do nothing
          state = 3;
          DBG_LOG_DEBUG("In a connection in server role\n\r");
        }
      }
      break;
      case 99:
      default:
      {
        countdown_s = 0;
        // To switch back to BT-client from BT-Server
        if(BT_ROLE_SERVER != app_pro_gen_get_bluetooth_role(pt_gen_ctr))
        {
          // To reset the in_process flag
          pthread_mutex_lock(&pt_gen_ctr->mtx_for_bt_sw);
          pt_gen_ctr->bt_sw_in_progress = false;
          pthread_mutex_unlock(&pt_gen_ctr->mtx_for_bt_sw);
          state = 99;
          DBG_LOG_DEBUG("Client role\n\r");
        }
        else
        {
          // If BT-role has not been the client role, to check whether it
          // is in progress.
          DBG_LOG_INFO(
            "To check if there is any SW in progress, %d\n\r",
            pt_gen_ctr->bt_sw_in_progress);
          pthread_mutex_lock(&pt_gen_ctr->mtx_for_bt_sw);
          if(pt_gen_ctr->bt_sw_in_progress)
          {
            // BT sw is in progress
            pthread_mutex_unlock(&pt_gen_ctr->mtx_for_bt_sw);
            state = 99;
            DBG_LOG_INFO("BT SW is in progress\n\r");
          }
          else
          {
            pt_gen_ctr->bt_sw_in_progress = true;
            memset(&kp_ipc_msg, 0, sizeof(kp_ipc_msg));
            kp_ipc_msg.mtype = M_TYPE_COMMAND;
            kp_ipc_msg.mtext.cmd_info.cmd = CMD_SW_TO_CLIENT;
            kp_ipc_msg.mtext.cmd_info.msgid =
              app_pro_gen_allocate_msg_seq_no();
            kp_ipc_msg.mtext.cmd_info.para.tpu32 = 0;  // No parameter is
                                                       // required
            if(ST_OK !=
               app_wq_post_to_waiting_queue(
                 app_pro_gen_get_wq_ipc_msg_ctr(),
                 (uint8_t *)&kp_ipc_msg,
                 sizeof(kp_ipc_msg)))
            {
              // To reset the flag
              pt_gen_ctr->bt_sw_in_progress = false;
              pthread_mutex_unlock(&pt_gen_ctr->mtx_for_bt_sw);

              // To clear the countdown
              countdown_s = 0;
              state = 99;

              DBG_LOG_ERR(
                "Failed to post CMD_SW_TO_CLIENT on the queue\n\r");
            }
            else
            {
              pthread_mutex_unlock(&pt_gen_ctr->mtx_for_bt_sw);
              state = 99;
              DBG_LOG_INFO("CMD_SW_TO_CLIENT has been posted\n\r");
            }
          }
        }
      }
      break;
    }
  }

  DBG_LOG_ERR("Never should reach here\n\r");
  return;
}
/**
 * @brief Thread to deal with the dataase operation
 */
static void *
thandler_db_operator(void *pt_para)
{
  const struct pro_general_controller *pt_gen_ctr = app_pro_gen_get_ctr();
  struct database_operation_controller *pt_db_op_ctr =
    app_db_get_database_op_ctr();
  struct dbop_controller *pt_dbop_ctr = app_db_get_dbop_controller();
  ssize_t kp_szval = 0;
  struct msgbuf kp_msg = { 0 };

  while(1)
  {
    DBG_LOG_INFO(
      "Wait message from the sql process, by msgid %d",
      pt_db_op_ctr->msgid_from_db);
    memset(&kp_msg, 0, sizeof(struct msgbuf));
    kp_szval = msgrcv(pt_db_op_ctr->msgid_from_db,
                      &kp_msg,
                      MSG_BUF_LEN,
                      0,
                      0);
    if(kp_szval < 0)
    {
      DBG_LOG_ERR("msgrcv failure, for %s", strerror(errno));
      continue;
    }
    DBG_LOG_DEBUG(
      "Message has been received from sql process，bytes = %d",
      kp_szval);

    app_db_handler_for_msg_from_database_v2(pt_dbop_ctr,
                                            &kp_msg);
  }
}



/*thread to send the data to be sent to process-BLE
*/
static void*thandler_post_ipc_msg_to_queue(void*pt_para)
{
	const int hld_msgid_to_ble = app_pro_gen_get_msgid_to_pro_ble(app_pro_gen_get_ctr());
	struct data_unit *pt_dtunit = NULL;
	struct ipc_msg_v2 *pt_ipc_msg = {0};
	
	DBG_LOG_INFO("thread for data-packet posting is running, msgid_to_ble %d", hld_msgid_to_ble);
	while(1)
	{
		DBG_LOG_INFO("BKP");
		// wait for data
		pt_dtunit = (struct data_unit*)app_wq_wait_for_data_trans_queue(app_pro_gen_get_wq_ipc_msg_ctr());
		if(NULL == pt_dtunit)
		{
			DBG_LOG_WARN("Double check when you see this");
			continue;
		}
		DBG_LOG_INFO("BKP pt_dtuinit@0x%llx, ipc_msg_size %lu", (uint64_t)pt_dtunit, sizeof(struct ipc_msg_v2));		
		DBG_LOG_INFO("pt_dtunit->len %u", pt_dtunit->len);

		pt_ipc_msg = (struct ipc_msg_v2*)pt_dtunit->pt_buf;
		DBG_LOG_INFO("Going to put IPC msg-type = %ld , len = %u on the queue", pt_ipc_msg->mtype, pt_dtunit->len);
		
		// now to put the data unit on the message queue
		#if(0)
		if(0 != msgsnd(hld_msgid_to_ble, (const void*)pt_dtunit->pt_buf, sizeof(union msg_load), IPC_NOWAIT))
		{
			DBG_LOG_ERR("fail to put msg on queue %d", hld_msgid_to_ble);
		}
		#else // keep trying when we fail to put msg on queue
			while(1)
			{ 
				if(0 != msgsnd(hld_msgid_to_ble, (const void*)pt_dtunit->pt_buf, sizeof(union msg_load), IPC_NOWAIT))
				{
					if(EAGAIN == errno)
					{
						usleep(5000);
					}
					else
					{						
						DBG_LOG_ERR("fail to put msg on queue %d, for %s", hld_msgid_to_ble, strerror(errno));
						break;
					}
				}
				else
				{
					break;
				}
			}
		#endif
		app_wq_release_data_unit( pt_dtunit);
	}
}

extern sensorConfig_t g_dbg_sensor_conf;
/**
 * @brief Thread of BLE high-level application (as a client to connect
 * sensors)
 */
static void *
thandler_operator(void *pt_para)
{
  app_state_t kp_state = ST_ERR;
  uint8_t abnormalCnt = 0;

#if ((CURRENT_MODE == ALL_DATA_SELECTION_TEST_MODE) || \
  (CURRENT_MODE == LONG_DATA_SELECTION_TEST_MODE))
  SKFChina_Common_MeasurementType kp_mtype_array[3] = { 0 };
#endif
  // Initialization of the timeout timer for short data transfer (e.g.,
  // used by reading a temperature value or configuring the sample period
  // with Froto over BLE)
  app_froto_init_shortdata_xfer_timer_ctr(app_froto_get_shortdata_xfer_timer_ctr());

  // Initialization of the timeout timer for long data transfer (e.g., used
  // by reading a piece of waveform or disseminate a copy of FUOTA image
  // with Froto over BLE)
  app_froto_init_longdata_xfer_timer_ctr(app_froto_get_longdata_xfer_timer_ctr());

  // Load the list and set to the BLE process
#if (SENSOR_LIST_MODE == SENSOR_LIST_READING_FROM_DB)
  uint32_t tpu32 = 0;
  struct bt_addr kp_white_list_p[MAX_BT_WHITE_LIST] = { 0 };
  struct bt_addr kp_white_list_t[MAX_BT_WHITE_LIST] = { 0 };
  uint32_t kp_white_list_nameid_p[MAX_BT_WHITE_LIST] = { 0 };
  uint32_t kp_white_list_nameid_t[MAX_BT_WHITE_LIST] = { 0 };

#if (SKF_GW_NEW != 1)
  app_db_fetch_devlist(app_db_get_dbop_controller(),
					  app_pro_gen_get_ctr()->dev_array,
					  MAX_BT_WHITE_LIST);
#endif

  app_pro_gen_get_mac_array(
	app_pro_gen_get_ctr(), TYPE_STR_INSIGHT_T, kp_white_list_nameid_t, kp_white_list_t,
	MAX_BT_WHITE_LIST);
  tpu32 = app_pro_gen_get_mac_array(
	app_pro_gen_get_ctr(), TYPE_STR_INSIGHT_P, NULL, kp_white_list_p,
	MAX_BT_WHITE_LIST);
  app_pro_gen_get_mac_array(
	app_pro_gen_get_ctr(), TYPE_STR_INSIGHT_PP, NULL,
	&kp_white_list_p[tpu32 % MAX_BT_WHITE_LIST],
	(MAX_BT_WHITE_LIST - tpu32) >
	MAX_BT_WHITE_LIST ? MAX_BT_WHITE_LIST : (MAX_BT_WHITE_LIST - tpu32));
#elif (SENSOR_LIST_MODE == SENSOR_LIST_DEFINED_BY_MACRO)
  struct bt_addr kp_white_list_p[] = SENSOR_LIST_MACRO;
  struct bt_addr kp_white_list_t[] = SENSOR_LIST_MACRO;
  uint32_t kp_white_list_nameid_t[] = SENSOR_NAMEID_MACRO;
#else
  #error "Please provide SENSOR_LIST_MODE!"
#endif

  {
	uint32_t retry_cnt = 0;
	do{
		if(ST_OK != app_sch_set_bt_white_list(kp_white_list_p,
								sizeof(kp_white_list_p) /
								sizeof(kp_white_list_p[0])))
		{
			DBG_LOG_WARN("Failed to set BLE white list to the process BLE, have a try again ...");
			sleep(1);
			retry_cnt++;
		}else{
			break;
		}
		if(retry_cnt >= 3)
		{
			exit(1);
		}
	}while(1);
  }
  
  app_hci_update_bt_white_list(app_hci_get_dtpacket_saving_ctr(), 
							   kp_white_list_t, 
							   kp_white_list_nameid_t,
							   sizeof(kp_white_list_t) /
							   sizeof(kp_white_list_t[0]));

  // Configure the gateway
  app_db_fetch_gwconf_and_dev_from_db(
    app_db_get_dbop_controller(), sys_get_gwConfig_and_dev());
  // Set NTP
  app_com_setup_system_timesyncd(sys_get_gwConfig_and_dev());

#if(0) //debug only for database table operation		
  struct database_operation_controller *pt_db_op_ctr = app_db_get_database_op_ctr();		
  struct db_op_msg *pt_op_msg = NULL;
  gwConfig_t kp_gw_cfg = {0};
  memset( &kp_gw_cfg, 0, sizeof(gwConfig_t));
  printf("==========================================to update the device info list@0x%x, num=%d\n", kp_gw_cfg.gwConfig.childrenList, kp_gw_cfg.gwConfig.childrenNumber); 				
  #if(GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
	INIT_LIST_HEAD( &kp_gw_cfg.gwConfig.childrenList);
  #endif

  printf("==========================================to update the device info list@0x%x, num=%d\n", kp_gw_cfg.gwConfig.childrenList, kp_gw_cfg.gwConfig.childrenNumber); 
  app_cjson_jsonstr_to_gwconfig( &kp_gw_cfg, FPATH_CONF);		
  printf("==========================================to update the device info list@0x%x, num=%d\n", kp_gw_cfg.gwConfig.childrenList, kp_gw_cfg.gwConfig.childrenNumber); 

  // to trig the database operation
  pt_op_msg = (struct db_op_msg*)l_malloc(sizeof(struct db_op_msg));
  if(pt_op_msg)
  {
	memset( pt_op_msg, 0, sizeof(struct db_op_msg));
	pt_op_msg->cmd = gw_pro_command_sqlite_update_item;
	pt_op_msg->table_idx = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
	pt_op_msg->cmd_para.pt_gwconf = (gwConfig_gwConfig_t*)l_malloc(sizeof(gwConfig_gwConfig_t));
	memset( pt_op_msg->cmd_para.pt_gwconf, 0, sizeof(gwConfig_gwConfig_t));
	memcpy( pt_op_msg->cmd_para.pt_gwconf, &kp_gw_cfg.gwConfig, sizeof(gwConfig_gwConfig_t));

	pthread_mutex_lock( &pt_db_op_ctr->mtx);
	l_queue_push_tail( pt_db_op_ctr->pt_dtitem_queue, (void *)pt_op_msg);
	pthread_cond_signal( &pt_db_op_ctr->cond_dt_available);
	pthread_mutex_unlock( &pt_db_op_ctr->mtx);
  }
  // to trig the database operation, to insert the child-dev into DB				
  sleep(10);
  DBG_LOG_INFO("==========================================to update the device info list@0x%x, num=%d", kp_gw_cfg.gwConfig.childrenList, kp_gw_cfg.gwConfig.childrenNumber); 
  gwConfig_gwConfig_children_t *pt_child_ndtemp = NULL, *pt_cur_child_nd = NULL; 
  if(!list_empty(&kp_gw_cfg.gwConfig.childrenList))
  {
	DBG_LOG_INFO("BKP");
	list_for_each_entry_safe( pt_cur_child_nd, pt_child_ndtemp, &kp_gw_cfg.gwConfig.childrenList, childrenNode)
	{
		pt_op_msg = (struct db_op_msg*)l_malloc(sizeof(struct db_op_msg));
		if(NULL == pt_op_msg)
		{
			continue;
		}
		if(false == app_pro_gen_get_connection_state( app_pro_gen_get_ctr()))
		{
			DBG_LOG_WARN("No BT connection yet");
			continue;
		}
		#if(0)
			sleep(5);
			uint32_t tpu32 = 0;
			tpu32 = time(NULL);
			// for image dissemination test, to speed up the data transportation
			app_froto_set_force_fuota( app_froto_get_img_dissem_ctr());
			if(ST_OK == app_froto_img_dissemination_with_block( app_froto_get_img_dissem_ctr(), app_sch_get_sensor_op_controller(), "/home/root/fimage_500k", 0x7788))
			//if(1)
			{
				DBG_LOG_INFO("the FW image dissemination is done successfully, time taken = %d", time(NULL) - tpu32);
			}
			else
			{
				DBG_LOG_ERR("fail to disseminate FW image, time taken = %d", time(NULL) - tpu32);
			}
			
			sleep(60);
		#endif
	}
#endif
  while(1)
  {
    DBG_LOG_DEBUG("Bluetooth role is: %s\n",BT_ROLE_CLIENT ==app_pro_gen_get_bluetooth_role(app_pro_gen_get_ctr()) ? "Client" : "Server");
    sleep(3);
    if(BT_ROLE_CLIENT !=
       app_pro_gen_get_bluetooth_role(app_pro_gen_get_ctr()))
    {
      // The following part in the loop is only for client role (to connect
      // sensors). If the role is not "client", just skip the following
      // part of the loop.
      continue;
    }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
	if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
    {
      // The following part in the loop is only for connection. If no
      // connection established (and the role is client), just keep
      // scanning and do nothing here.
      DBG_LOG_WARN("No BT connection yet");
      continue;
    }
	
	#if(0)
	// for debug only
		DBG_LOG_ERR("====A connection to sensor is created");
		continue;
	#endif

	
#if(0)
	struct pro_general_controller *pt_gen_ctr = app_pro_gen_get_ctr();
	gwConfig_gwConfig_children_t *pt_list_node_temp = NULL;
	gwConfig_gwConfig_children_t *pt_list_node_current = NULL;
	// for gw-conf reading test
	if(ST_OK != app_db_fetch_gwconf_and_dev_from_db( app_db_get_dbop_controller(), app_pro_gen_get_ctr()))
	{
		DBG_LOG_ERR("fail to read gwconf from DB");			
	}
	else
	{
		DBG_LOG_WARN("gw_conf nameId %d, child-number = %d", pt_gen_ctr->gw_conf_info.gwConfig.nameId, pt_gen_ctr->gw_conf_info.gwConfig.childrenNumber);
		#if(1)
			list_for_each_entry_safe( pt_list_node_current, pt_list_node_temp, &pt_gen_ctr->gw_conf_info.gwConfig.childrenList, childrenNode)
			{
				DBG_LOG_WARN("list-node nameId = %d", pt_list_node_current->nameId);
			}
		#endif
		DBG_LOG_INFO("NTP server %s", pt_gen_ctr->gw_conf_info.gwConfig.timeConfig.timeNtpUrl);				
		//app_com_set_time_zone(IDX_TIM_ZONE_E8);
		//app_com_setup_ntp_service( pt_gen_ctr);				
		app_com_setup_system_timesyncd( pt_gen_ctr);
		
		//set network  configuration
		//app_com_set_network_configuration( pt_gen_ctr);
	}
	sleep(600);
#elif(0)
	// to set configuration to sensor
	sensorConfig_t kp_sensor_conf_info = {0};
	sleep(3); 
	kp_sensor_conf_info.lastEditTimeS = 666;
	app_sch_set_sensor_conf( app_sch_get_sensor_op_controller(), &kp_sensor_conf_info);
	sleep(30);
#elif(0)//to synchronize time with sensor
	app_sch_synchronize_time( app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), DBG_SENSOR_ID_STR);
	sleep(10);
#elif(0)// to read sensor-configuration from database		
	sensorConfig_t kp_sensor_conf = {0};
	memset( &g_dbg_msg_format_ctr, 0, sizeof(struct msg_format_controller));
	app_db_read_sensor_conf_from_db( app_db_get_dbop_controller(), &g_dbg_msg_format_ctr, &kp_sensor_conf, 0);
	sleep(30);
#elif(0) // to write sensor configuration to database
	g_dbg_sensor_conf.nameId++;
	app_db_deliver_sensor_conf_to_db( app_db_get_dbop_controller(), &g_dbg_sensor_conf);
#elif(0)// to read sensor configuration
	// for sensor configuration synchronization
	uint32_t kp_hashval = 0, kp_edit_time = 0;
	static uint32_t kp_cnt = 0;
	sensorConfig_t kp_sensor_conf = {0};
	sleep(5);
	// set configuration to sensor
	kp_sensor_conf.lastEditTimeS = 666;			

	app_sch_init_sensor_config_for_test( &kp_sensor_conf);// set configuration to default
	
	//app_sch_set_sensor_conf( app_sch_get_sensor_op_controller(), &kp_sensor_conf);
	//memset( &kp_sensor_conf, 0, sizeof(sensorConfig_t));
	//sleep(5);
	app_sch_read_conf_hash_and_last_edit_time( app_sch_get_sensor_op_controller(), &kp_hashval, &kp_edit_time);

	//sleep(30);
	//continue;

	sleep(5);
	memset( &kp_sensor_conf, 0, sizeof(sensorConfig_t));			
	kp_sensor_conf.lastEditTimeS = kp_edit_time;
	app_sch_read_sensor_conf_v2( app_sch_get_sensor_op_controller(), &kp_sensor_conf);
	sleep(5);			

	//sleep(30);
	//continue; // for debug

	
	kp_cnt ++;
	kp_sensor_conf.nameId = kp_cnt;
	app_db_deliver_sensor_conf_to_db( app_db_get_dbop_controller(), &kp_sensor_conf);

	sleep(5);
	//app_db_read_sensor_conf_from_db( app_db_get_dbop_controller(), &g_dbg_msg_format_ctr, &kp_sensor_conf, 0);
	sleep(30);
#elif(0) // for data selection test			
	sleep(5); 
	kp_mtype_array[0] = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE;
	kp_state = app_froto_send_data_selection_request( kp_mtype_array, 1, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), app_pro_gen_allocate_msg_seq_no());
	if(ST_OK == kp_state)
	{
		DBG_LOG_INFO("to wait for the receivation of data");
		sleep(6);
	}
	continue;
	
	sleep(5); 
	kp_mtype_array[0] = SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT;
	kp_state = app_froto_send_data_selection_request( kp_mtype_array, 1, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), app_pro_gen_allocate_msg_seq_no());
	if(ST_OK == kp_state)
	{
		DBG_LOG_INFO("to wait for the receivation of data");
		sleep(6);
	}
	continue; // for debug only
	
	kp_mtype_array[0] = SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE;
	kp_state = app_froto_send_data_selection_request( kp_mtype_array, 1, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), app_pro_gen_allocate_msg_seq_no());
	if(ST_OK == kp_state)
	{
		DBG_LOG_INFO("to wait for the receivation of waveform");
		sleep(200);
	}
#elif(0)
	sleep(5);
	DBG_LOG_ERR("========to request for ACC_Z");
	SKFChina_SensingDataUpload_MeasurementTypeMsg kp_mtype_msg = {0};
	while(1)
	{
		kp_mtype_msg.measure_type = SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE;
		kp_mtype_msg.has_dimension = true;
		kp_mtype_msg.dimension = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X; 
		//util_froto_fill_msg_data_selection_with_mtype_msg( kp_mtype_msg, "A:B:C:1:2:3", 777);

		//kp_mtype_array[0] = SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE;
		//kp_mtype_array[0] = SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE;
		//kp_mtype_array[0] = SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE;
		//kp_state = app_froto_send_data_selection_request( kp_mtype_array, 1, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), app_pro_gen_allocate_msg_seq_no());
		kp_state = app_froto_send_data_selection_request_by_mtype_msg( kp_mtype_msg, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), app_pro_gen_allocate_msg_seq_no());
		if(ST_OK == kp_state)
		{
			DBG_LOG_INFO("request for ACC_X is sent");
			sleep(3);
			break;
		}
		else
		{
			sleep(3);
		}
		if(false == app_pro_gen_get_connection_state( app_pro_gen_get_ctr()))
		{
			DBG_LOG_ERR("no valid connection available");
			break;
		}
	}
	//for ACC_Y
	memset( &kp_mtype_msg, 0, sizeof(SKFChina_SensingDataUpload_MeasurementTypeMsg));
	kp_mtype_msg.measure_type = SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE;
	kp_mtype_msg.has_dimension = true;
	kp_mtype_msg.dimension = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y;
	while(1)
	{
		kp_state = app_froto_send_data_selection_request_by_mtype_msg( kp_mtype_msg, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), app_pro_gen_allocate_msg_seq_no());
		if(ST_OK == kp_state)
		{
			DBG_LOG_INFO("request for ACC_Y is sent");
			sleep(3);
			break;
		}
		else
		{
			sleep(3);
		}
		if(false == app_pro_gen_get_connection_state( app_pro_gen_get_ctr()))
		{
			DBG_LOG_ERR("no valid connection available");
			break;
		}
	}
	//for ACC_Z
	memset( &kp_mtype_msg, 0, sizeof(SKFChina_SensingDataUpload_MeasurementTypeMsg));
	kp_mtype_msg.measure_type = SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE;
	kp_mtype_msg.has_dimension = true;
	kp_mtype_msg.dimension = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Z;
	while(1)
	{
		kp_state = app_froto_send_data_selection_request_by_mtype_msg( kp_mtype_msg, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), app_pro_gen_allocate_msg_seq_no());
		if(ST_OK == kp_state)
		{
			DBG_LOG_INFO("request for ACC_Z is sent");
			sleep(3);
			break;
		}
		else
		{
			sleep(3);
		}
		if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
		{
			DBG_LOG_ERR("no valid connection available");
			break;
		}
	}

	//______________________
	//app_db_load_sensor_data(sensorConfig_t * sensorConfig)
	
#elif(CURRENT_MODE == PRODUCT_MODE)
    // The main loop when a sensor has been connected ...
	// Check it again
	if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
         app_pro_gen_get_ctr()))
    {
      continue;
    }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
	if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
    {
      DBG_LOG_WARN("No BT connection yet");
	  abnormalCnt = 0;
      continue;
    }

	sleep(3);

    uint32_t tp_version = 0;
    uint32_t kp_hashval = 0;
    uint32_t kp_edit_time = 0;
    sensorConfig_t kp_sensor_conf = { 0 };
    struct device_info kp_dev_info = { 0 };
    int32_t dbFilterNum = 0;
    uint32_t nameIdBuf[5];
    macAddr_t addrTmp;
    gw_pro_command_sqlite_filter_param_t filter;
    uint8_t tp_addrstr_buf[MAX_BT_ADDRSTR] = { 0 };
	uint8_t retryCnt;
	app_state_t res;
	
	if(abnormalCnt == 0)
	{
		abnormalCnt++;
	}
	else
	{
		exit(1);
	}
	// 0 Set the timer (to make sure that the connection params can been applied)
	// to set Bluetooth PHY to 1M
	struct bluetooth_connection_param con_param;
	con_param.phy = LE_PHY_1M;
	con_param.min_con_interval = 12;
	con_param.max_con_interval = 12; // 15 mS
	// At most 29 connection events can be silent for the slave if 
	// nothing received or to send (thereby saving energy)
	con_param.con_latency = 29;
	// The connection timeout (timeout would happen if nothing received
	// from slave during two slave-latency connection events)
	con_param.supervision_timeout = (1 + 29) * (12 * 1.25) * 2 + 1;
	con_param.min_ce_len = 12; // 7.5 mS
	con_param.max_ce_len = con_param.min_con_interval * 2;
#if 0 // The max BLE packet length can not be modified at the sensor
	con_param.tx_octets = 251; // The max length of a BLE packet is maximal (i.e., 251 bytes)
	con_param.tx_time = 0x4290; // The max tx time of a packet (to set the max. time)
#endif
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	bt_hcitool_set_bluetooth_connection_param(&con_param, NULL);
#else
	bt_hcitool_set_bluetooth_connection_param(&con_param);
#endif
	// And configure the txpower
#if (NRF52840NIC == 1)
	{
		sleep(2);
		uint8_t _retry = 0;

		// Set tx power
		do{
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
		if(ST_OK ==
			bt_hcitool_set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, 8, NULL))
#else
		if(ST_OK ==
			bt_hcitool_set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, 8))
#endif
		{
			DBG_LOG_INFO("Tx-power has been set successfully");
			break;
		}
		if(_retry + 1 < 3)
		{
			DBG_LOG_WARN("Failed to set tx-power and retry ...");
			usleep(1000000 * (1 << _retry)); // delay a period of time and
			// have another try
		}
		else
		{
			DBG_LOG_ERR("Failed to set tx-power ...");
			// Disconnect ...
			goto DISCONNECT;
		}
		_retry++;
		}while(_retry < 3);
	}
#endif /* NRF52840NIC == 1 */

	// 1 Set the time to the sensor
    sleep(2);
	app_pro_gen_get_active_client_id_str(tp_addrstr_buf, MAX_BT_ADDRSTR);
	retryCnt = 0;
	do{
	  res = app_sch_synchronize_time(app_pro_gen_get_gateway_idstr(
                                     app_pro_gen_get_ctr()), tp_addrstr_buf);
	  retryCnt++;
	  if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
         app_pro_gen_get_ctr()))
	  {
	    goto DISCONNECT;
	  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	  if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
	  if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
	  {
	    DBG_LOG_WARN("No BT connection yet");
	    goto DISCONNECT;
	  }
	  if((res != ST_OK) && (retryCnt >= 3))
	  {
	  	DBG_LOG_WARN("Failed to set time");
	    goto DISCONNECT;
	  }
	  if(res != ST_OK)
	  {
	  	DBG_LOG_WARN("Retry to set time");
	  }
	  sleep(1 + retryCnt);
	  
	}while(res != ST_OK);

    // 2 Update configuration
	retryCnt = 0;
	do{
	  res = app_froto_retrieve_sensor_fw_version_info(
      		  app_sch_get_sensor_op_controller(), &tp_version);
	  retryCnt++;
	  if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
         app_pro_gen_get_ctr()))
	  {
	    goto DISCONNECT;
	  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	  if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
	  if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
	  {
	    DBG_LOG_WARN("No BT connection yet");
	    goto DISCONNECT;
	  }
	  if((res != ST_OK) && (retryCnt >= 3))
	  {
		DBG_LOG_WARN("Failed to get the sensor version");
	    goto DISCONNECT;
	  }
	  if(res != ST_OK)
	  {
	  	DBG_LOG_WARN("Retry to get the sensor version");
	  }
	  sleep(1 + retryCnt);
	}while(res != ST_OK);
	DBG_LOG_DEBUG("Firmware version of sensor is %d.%d.%d\r\n",
                  (tp_version & 0xff0000) >> 16,
                  (tp_version & 0xff00) >> 8,
                  (tp_version & 0xff));
	
	retryCnt = 0;
	do{
	  res = app_sch_read_conf_hash_and_last_edit_time(
      		  app_sch_get_sensor_op_controller(), 
			  &kp_hashval, &kp_edit_time);
	  retryCnt++;
	  if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
         app_pro_gen_get_ctr()))
	  {
	    goto DISCONNECT;
	  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	  if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
	  if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
	  {
	    DBG_LOG_WARN("No BT connection yet");
	    goto DISCONNECT;
	  }
	  if((res != ST_OK) && (retryCnt >= 3))
	  {
		DBG_LOG_WARN("Failed to get config hash from sensor");
	    goto DISCONNECT;
	  }
	  if(res != ST_OK)
	  {
	  	DBG_LOG_WARN("Retry to get config hash from sensor");
	  }
	  sleep(1 + retryCnt);
	}while(res != ST_OK);
    
    filter.table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
    filter.dataNum = 1;
    filter.filterData[0].condition = gw_pro_sqlite_cond_str_equal;
    filter.filterData[0].field = 4;  // MAC address of sensor
    snprintf(filter.filterData[0].condParam.string_value.s,
             sizeof(filter.filterData[0].condParam.string_value.s),
             "%s", tp_addrstr_buf);
    DBG_LOG_DEBUG("Filtered based on the given MAC Address: %s\r\n",
                  filter.filterData[0].condParam.string_value.s);
    dbFilterNum = app_db_fetch_filtered_index(app_db_get_dbop_controller(),
                                        &filter,
                                        sizeof(nameIdBuf),
										nameIdBuf);
    if(dbFilterNum == 1)
    {
      // In principle, the number of the filtered name ID should be only 1
      // (based on the given MAC address)
      DBG_LOG_DEBUG("The filtered nameID: %d\r\n", nameIdBuf[0]);
      memset(&g_sys_sensor_fuota_info, 0, sizeof(g_sys_sensor_fuota_info));
      memset(&kp_sensor_conf, 0, sizeof(kp_sensor_conf));
      if(ST_OK ==
         app_db_fetch_sensor_conf_from_db(app_db_get_dbop_controller(),
                                         &kp_sensor_conf, NULL,
                                         nameIdBuf[0]))
      {
        DBG_LOG_DEBUG("Hash computed based on sensor: %d", kp_hashval);
        DBG_LOG_DEBUG("Hash computed based on db: %d",
                      util_froto_cal_config_hash_value(
                        &kp_sensor_conf,
                        app_pro_gen_get_gateway_idstr(
                          app_pro_gen_get_ctr())));
        if(kp_hashval != util_froto_cal_config_hash_value(
             &kp_sensor_conf,
             app_pro_gen_get_gateway_idstr(app_pro_gen_get_ctr())))
        {
          DBG_LOG_DEBUG("DB lastEditTimeS: %d",
                        kp_sensor_conf.lastEditTimeS);
          DBG_LOG_DEBUG("Sensor lastEditTimeS: %d", kp_edit_time);
          if(kp_sensor_conf.lastEditTimeS <= kp_edit_time)
          {
            DBG_LOG_DEBUG(
              "Configuration in database needs to be updated\r\n");
            // Configuration in sensor is the newer
            memset(&kp_sensor_conf, 0, sizeof(kp_sensor_conf));
            if(ST_OK != app_sch_read_sensor_conf_v2(
                 app_sch_get_sensor_op_controller(), &kp_sensor_conf))
            {
              DBG_LOG_WARN("No BT connection yet");
              goto DISCONNECT;
            }
            kp_sensor_conf.nameId = nameIdBuf[0];
            DBG_LOG_DEBUG(
              "Updated MAC Address: %s!\r\n", tp_addrstr_buf);
            // NOTICE: the unaligned address may cause memory anomaly, so
            // use addrTmp as an intermediate variable
            sscanf(tp_addrstr_buf, "%02x%02x%02x%02x%02x%02x",
                   &(addrTmp.addr.addrArray[0]),
                   &(addrTmp.addr.addrArray[1]),
                   &(addrTmp.addr.addrArray[2]),
                   &(addrTmp.addr.addrArray[3]),
                   &(addrTmp.addr.addrArray[4]),
                   &(addrTmp.addr.addrArray[5]));
            memcpy(kp_sensor_conf.macAddr.addr.addrArray,
                   addrTmp.addr.addrArray,
                   sizeof(kp_sensor_conf.macAddr.addr.addrArray));
            DBG_LOG_DEBUG("FUOTA file: %s!\r\n", g_sys_sensor_fuota_info.fpath);
			g_sys_sensor_fuota_info.currentVer = tp_version;
            app_db_deliver_sensor_conf_to_db(
              app_db_get_dbop_controller(), &kp_sensor_conf, NULL);

            DBG_LOG_DEBUG("Hash computed based on db: %d", util_froto_cal_config_hash_value(
                            &kp_sensor_conf, app_pro_gen_get_gateway_idstr(
                              app_pro_gen_get_ctr())));
          }
          else
          {
            // Configuration in database is the newer
            DBG_LOG_DEBUG("Configuration in sensor needs to be updated\r\n");
			
			// In fact, there is no need to write the database
		    // (Just for updating firmware version of sensor)
			kp_sensor_conf.nameId = nameIdBuf[0];
            DBG_LOG_DEBUG(
              "Updated MAC Address: %s!\r\n", tp_addrstr_buf);
            // NOTICE: the unaligned address may cause memory anomaly, so
            // use addrTmp as an intermediate variable
            sscanf(tp_addrstr_buf, "%02x%02x%02x%02x%02x%02x",
                   &(addrTmp.addr.addrArray[0]),
                   &(addrTmp.addr.addrArray[1]),
                   &(addrTmp.addr.addrArray[2]),
                   &(addrTmp.addr.addrArray[3]),
                   &(addrTmp.addr.addrArray[4]),
                   &(addrTmp.addr.addrArray[5]));
            memcpy(kp_sensor_conf.macAddr.addr.addrArray,
                   addrTmp.addr.addrArray,
                   sizeof(kp_sensor_conf.macAddr.addr.addrArray));
            DBG_LOG_DEBUG("FUOTA file: %s!\r\n", g_sys_sensor_fuota_info.fpath);
			g_sys_sensor_fuota_info.currentVer = tp_version;
			app_db_deliver_sensor_conf_to_db(
              app_db_get_dbop_controller(), &kp_sensor_conf, NULL);
			  
            if(ST_OK != app_sch_set_sensor_conf(
                 app_sch_get_sensor_op_controller(), &kp_sensor_conf))
            {
              DBG_LOG_WARN("No BT connection yet");
              goto DISCONNECT;
            }
          }
        }
        else
        {
		  // In fact, there is no need to write the database
		  // (Just for updating firmware version of sensor)
		  kp_sensor_conf.nameId = nameIdBuf[0];
          DBG_LOG_DEBUG(
            "Updated MAC Address: %s!\r\n", tp_addrstr_buf);
          // NOTICE: the unaligned address may cause memory anomaly, so
          // use addrTmp as an intermediate variable
          sscanf(tp_addrstr_buf, "%02x%02x%02x%02x%02x%02x",
                 &(addrTmp.addr.addrArray[0]),
                 &(addrTmp.addr.addrArray[1]),
                 &(addrTmp.addr.addrArray[2]),
                 &(addrTmp.addr.addrArray[3]),
                 &(addrTmp.addr.addrArray[4]),
                 &(addrTmp.addr.addrArray[5]));
          memcpy(kp_sensor_conf.macAddr.addr.addrArray,
                 addrTmp.addr.addrArray,
                 sizeof(kp_sensor_conf.macAddr.addr.addrArray));
          DBG_LOG_DEBUG("FUOTA file: %s!\r\n", g_sys_sensor_fuota_info.fpath);
		  g_sys_sensor_fuota_info.currentVer = tp_version;
		  app_db_deliver_sensor_conf_to_db(
            app_db_get_dbop_controller(), &kp_sensor_conf, NULL);

          DBG_LOG_DEBUG("No further configuration is required\r\n");
        }
      }
      else
      {
        // No item in the database, creating an item is required
        DBG_LOG_DEBUG(
          "No item in the database, creating an item is required\r\n");
        if(ST_OK != app_sch_read_sensor_conf_v2(
             app_sch_get_sensor_op_controller(), &kp_sensor_conf))
        {
          DBG_LOG_WARN("No BT connection yet");
          goto DISCONNECT;
        }
        kp_sensor_conf.nameId = nameIdBuf[0];
        DBG_LOG_DEBUG(
          "Updated MAC Address: %s!\r\n", tp_addrstr_buf);
        // NOTICE: the unaligned address may cause memory anomaly, so use
        // addrTmp as an intermediate variable
        sscanf(tp_addrstr_buf, "%02x%02x%02x%02x%02x%02x",
               &(addrTmp.addr.addrArray[0]),
               &(addrTmp.addr.addrArray[1]),
               &(addrTmp.addr.addrArray[2]),
               &(addrTmp.addr.addrArray[3]),
               &(addrTmp.addr.addrArray[4]),
               &(addrTmp.addr.addrArray[5]));
        memcpy(kp_sensor_conf.macAddr.addr.addrArray,
               addrTmp.addr.addrArray,
               sizeof(kp_sensor_conf.macAddr.addr.addrArray));
        DBG_LOG_DEBUG("FUOTA file: %s!\r\n", g_sys_sensor_fuota_info.fpath);
		g_sys_sensor_fuota_info.currentVer = tp_version;
        app_db_deliver_sensor_conf_to_db(
          app_db_get_dbop_controller(), &kp_sensor_conf, NULL);
      }
    }
    else
    {
      DBG_LOG_ERR(
        "Failed to filter, filter condition: %s, dbFilterNum: %d\r\n",
        filter.filterData[0].condParam.string_value.s,
        dbFilterNum);
    }
    if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
         app_pro_gen_get_ctr()))
    {
      goto DISCONNECT;
    }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
    if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
    {
      DBG_LOG_WARN("No BT connection yet");
      goto DISCONNECT;
    }

    // 3 Collect sensing data
    sleep(2);
    if(ST_OK != app_db_load_sensor_data(&kp_sensor_conf))
    {
      DBG_LOG_WARN("No BT connection yet");
      goto DISCONNECT;
    }
    if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
         app_pro_gen_get_ctr()))
    {
      goto DISCONNECT;
    }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
    if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
    {
      DBG_LOG_WARN("No BT connection yet");
      goto DISCONNECT;
    }

    // 4 FUOTA
    sleep(1);
    filter.table = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
    filter.dataNum = 1;
    filter.filterData[0].condition = gw_pro_sqlite_cond_str_equal;
    filter.filterData[0].field = 4;  // MAC address of sensor
    snprintf(filter.filterData[0].condParam.string_value.s,
             sizeof(filter.filterData[0].condParam.string_value.s),
             "%s", tp_addrstr_buf);
    DBG_LOG_DEBUG("Filtered based on the given MAC Address: %s\r\n",
                  filter.filterData[0].condParam.string_value.s);
    dbFilterNum = app_db_fetch_filtered_index(app_db_get_dbop_controller(),
                                        &filter,
                                        sizeof(nameIdBuf), 
										nameIdBuf);
    if(dbFilterNum == 1)
    {
      // In principle, the number of the filtered name ID should be only 1
      // (based on the given MAC address)
      DBG_LOG_DEBUG("The filtered nameID: %d\r\n", nameIdBuf[0]);
      memset(&g_sys_sensor_fuota_info, 0, sizeof(g_sys_sensor_fuota_info));
      memset(&kp_sensor_conf, 0, sizeof(kp_sensor_conf));
      if(ST_OK ==
         app_db_fetch_sensor_conf_from_db(app_db_get_dbop_controller(),
                                         &kp_sensor_conf, NULL,
                                         nameIdBuf[0]))
      {
		if(access(g_sys_sensor_fuota_info.fpath, F_OK | R_OK) != 0)
		{
		  DBG_LOG_DEBUG("FUOTA file: %s does not exist or is not readable!\r\n", 
		                g_sys_sensor_fuota_info.fpath);
		  goto DISCONNECT;
		}
        DBG_LOG_DEBUG("FUOTA version of sensor is %d.%d.%d\r\n",
                      (g_sys_sensor_fuota_info.ver & 0xff0000) >> 16,
                      (g_sys_sensor_fuota_info.ver & 0xff00) >> 8,
                      (g_sys_sensor_fuota_info.ver & 0xff));
        if(g_sys_sensor_fuota_info.currentVer != tp_version)
        {
          g_sys_sensor_fuota_info.currentVer = tp_version;
          kp_sensor_conf.nameId = nameIdBuf[0];
          DBG_LOG_DEBUG(
            "Updated MAC Address: %s!\r\n", tp_addrstr_buf);
          // NOTICE: the unaligned address may cause memory anomaly, so use
          // addrTmp as an intermediate variable
          sscanf(tp_addrstr_buf, "%02x%02x%02x%02x%02x%02x",
                 &(addrTmp.addr.addrArray[0]),
                 &(addrTmp.addr.addrArray[1]),
                 &(addrTmp.addr.addrArray[2]),
                 &(addrTmp.addr.addrArray[3]),
                 &(addrTmp.addr.addrArray[4]),
                 &(addrTmp.addr.addrArray[5]));
          memcpy(kp_sensor_conf.macAddr.addr.addrArray,
                 addrTmp.addr.addrArray,
                 sizeof(kp_sensor_conf.macAddr.addr.addrArray));
          DBG_LOG_DEBUG("FUOTA file: %s!\r\n", g_sys_sensor_fuota_info.fpath);
          app_db_deliver_sensor_conf_to_db(
            app_db_get_dbop_controller(), &kp_sensor_conf, NULL);
        }
        if(g_sys_sensor_fuota_info.ver != tp_version)
        {
          // When firmware version is not the FUOTA version
          DBG_LOG_DEBUG(
            "To disseminate FUOTA image: %s, version: %d.%d.%d\r\n",
            g_sys_sensor_fuota_info.fpath, (g_sys_sensor_fuota_info.ver & 0xff0000) >> 16,
            (g_sys_sensor_fuota_info.ver & 0xff00) >> 8,
            (g_sys_sensor_fuota_info.ver & 0xff));
          app_froto_set_force_fuota(app_froto_get_longdata_xfer_timer_ctr());

		  struct bluetooth_connection_param con_param_for_fuota;
		  con_param_for_fuota.phy = LE_PHY_1M;
		  con_param_for_fuota.min_con_interval = 12; //
		  con_param_for_fuota.max_con_interval = 12; // 15 mS
		  // No slave latency is allowed because here we require a max. throughput (from 
		  // gateway, client to the sensor, server) to run a FUOTA
		  con_param_for_fuota.con_latency = 0; 
		  con_param_for_fuota.supervision_timeout 
		  	= (1 + 0) * (12 * 1.25) * 2 + 1;
		  con_param_for_fuota.min_ce_len = 12; // 7.5 mS
		  con_param_for_fuota.max_ce_len = con_param_for_fuota.min_con_interval * 2;
#if 0 // The max BLE packet length can not be modified at the sensor
		  con_param_for_fuota.tx_octets = 251;
		  con_param_for_fuota.tx_time = 0x4290;
#endif
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
		  bt_hcitool_set_bluetooth_connection_param(&con_param_for_fuota, NULL);
#else
		  bt_hcitool_set_bluetooth_connection_param(&con_param_for_fuota);
#endif
#if (NRF52840NIC == 1)
		  sleep(2); // nrf52840 NIC needs more time to set these parameters
#endif /* NRF52840NIC == 1 */

          if(ST_OK ==
             app_froto_img_dissemination_with_block(
               app_froto_get_longdata_xfer_timer_ctr(),
               app_sch_get_sensor_op_controller(),
               g_sys_sensor_fuota_info.fpath,
               g_sys_sensor_fuota_info.ver))
          {
            DBG_LOG_INFO("FUOTA image dissemination has been done\r\n");
          }
          else
          {
            DBG_LOG_ERR("Failed to disseminate the FUOTA image\r\n");
			// TODO: Just a workaround
            exit(1);
			goto DISCONNECT;
          }
        }
        else
        {
          DBG_LOG_DEBUG("No FUOTA is required\r\n");
        }
      }
      else
      {
        DBG_LOG_ERR("Failed to read sensor config info\r\n");
        if(ST_OK ==
           app_db_read_dev_info_from_db(app_db_get_dbop_controller(),
                                        &g_dbg_msg_format_ctr,
                                        &kp_dev_info,
                                        nameIdBuf[0]))
        {
          DBG_LOG_DEBUG("Try to create the item to the sensor table\r\n");
          kp_sensor_conf.nameId = kp_dev_info.nameid;
          // NOTICE: the unaligned address may cause memory anomaly, so use
          // addrTmp as an intermediate variable
          sscanf(tp_addrstr_buf, "%02x%02x%02x%02x%02x%02x",
                 &(addrTmp.addr.addrArray[0]),
                 &(addrTmp.addr.addrArray[1]),
                 &(addrTmp.addr.addrArray[2]),
                 &(addrTmp.addr.addrArray[3]),
                 &(addrTmp.addr.addrArray[4]),
                 &(addrTmp.addr.addrArray[5]));
          memcpy(kp_sensor_conf.macAddr.addr.addrArray,
                 addrTmp.addr.addrArray,
                 sizeof(kp_sensor_conf.macAddr.addr.addrArray));
          kp_sensor_conf.version = 1;
          kp_sensor_conf.haveConfiguredToSensor = false;
          kp_sensor_conf.whenConfiguredToSensor = 0;
          memcpy(kp_sensor_conf.type, kp_dev_info.sensor_type,
                 sizeof(kp_sensor_conf.type));
          memcpy(kp_sensor_conf.manufacturer, kp_dev_info.manufacturer,
                 sizeof(kp_sensor_conf.manufacturer));
          memset(&g_sys_sensor_fuota_info, 0, sizeof(g_sys_sensor_fuota_info));
          g_sys_sensor_fuota_info.currentVer = tp_version;
          DBG_LOG_DEBUG("FUOTA file: %s!\r\n", g_sys_sensor_fuota_info.fpath);
          app_db_deliver_sensor_conf_to_db(app_db_get_dbop_controller(),
                                         &kp_sensor_conf, NULL);
        }
        else
        {
          DBG_LOG_ERR("Failed to read the device from the dev table\r\n");
        }
      }
    }
    else
    {
      DBG_LOG_ERR(
        "Failed to filter, filter condition: %s, dbFilterNum: %d\r\n",
        filter.filterData[0].condParam.string_value.s,
        dbFilterNum);
    }
    if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
         app_pro_gen_get_ctr()))
    {
      goto DISCONNECT;
    }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
    if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
    {
      DBG_LOG_WARN("No BT connection yet");
      goto DISCONNECT;
    }

DISCONNECT:
    // Disconnect
    app_sch_switch_bt_connection(NULL);
#elif(0)
			uint32_t tp_version = 0;
			app_froto_retrieve_sensor_fw_version_info( app_sch_get_sensor_op_controller(), &tp_version);
			app_froto_img_dissemination_with_block( app_froto_get_longdata_xfer_timer_ctr(), app_sch_get_sensor_op_controller(), "/home/root/fimage", 0x10001);
			sleep(30);
#elif(1)// for IMG dissemination test
			uint32_t tp_version = 0;
			sensorConfig_t kp_sensor_conf = {0};
			struct device_info kp_dev_info = {0};
			
			// to write to table Dev
			kp_dev_info.nameid = 666;
			snprintf( kp_dev_info.name,GW_PRO_MAX_STRING_LEN_BYTE,"%s","BUF00001");
			snprintf( kp_dev_info.sensor_name,GW_PRO_MAX_STRING_LEN_BYTE,"%s","BUF00001"); 
			snprintf( kp_dev_info.mac_addr, GW_PRO_MAX_STRING_LEN_BYTE,"%s","C4BD6A110031");
			snprintf( kp_dev_info.manufacturer, GW_PRO_MAX_STRING_LEN_BYTE, "%s","SKF");
			snprintf( kp_dev_info.sensor_type, GW_PRO_MAX_STRING_LEN_BYTE,"%s", "BULLET");
			app_db_write_dev_info_to_db( app_db_get_dbop_controller(), &kp_dev_info);
			sleep(5);
			
			// to write record to table Sensor
			memset( &kp_sensor_conf, 0, sizeof(sensorConfig_t));
			// to fill the FW info for OTA
			memset( &g_sys_sensor_fuota_info, 0, sizeof(struct fouta_fw_info));
			g_sys_sensor_fuota_info.ver = 65537;
			snprintf( g_sys_sensor_fuota_info.fpath, MAX_FPATH, "%s", "/home/root/fimage");
			app_sch_init_sensor_config_for_test(&kp_sensor_conf);

			DBG_LOG_INFO("the conf-hash is 0x%X", util_froto_cal_config_hash_value( &kp_sensor_conf));
			sleep(60);
			continue;
			
			// to write sensor-configuration to DB
			if(ST_OK != app_db_fetch_sensor_conf_from_db( app_db_get_dbop_controller(), &kp_sensor_conf, 666))
			{// when no record with the given nameID, then we write a new one
				app_db_deliver_sensor_conf_to_db( app_db_get_dbop_controller(), &kp_sensor_conf);
			}
			sleep(5);
			//continue;
			
			
			// to read sensor-configuration from DB
			memset( &g_sys_sensor_fuota_info, 0, sizeof(g_sys_sensor_fuota_info));
			memset( &kp_sensor_conf, 0, sizeof(kp_sensor_conf));
			
			if(ST_OK != app_db_fetch_sensor_conf_from_db( app_db_get_dbop_controller(), &kp_sensor_conf, 666))
			{
				DBG_LOG_ERR("fail to read sensor-conf info");
				continue;
			}
			
			DBG_LOG_INFO("the sensor-conf with nameId %d is read", kp_sensor_conf.nameId);
			sleep(10);		
			app_froto_retrieve_sensor_fw_version_info( app_sch_get_sensor_op_controller(), &tp_version);
			DBG_LOG_ERR("the sensor firmware version is %d", tp_version);
			if(kp_sensor_conf.version != tp_version)
			{ // to update the FW version
				g_sys_sensor_fw_version_info = tp_version;
				kp_sensor_conf.macAddr.addr.addrArray[0] = 0xC4;
				kp_sensor_conf.macAddr.addr.addrArray[1] = 0xBD;
				kp_sensor_conf.macAddr.addr.addrArray[2] = 0x6A;
				kp_sensor_conf.macAddr.addr.addrArray[3] = 0x11;
				kp_sensor_conf.macAddr.addr.addrArray[4] = 0x00;
				kp_sensor_conf.macAddr.addr.addrArray[5] = 0x31;
				app_db_deliver_sensor_conf_to_db( app_db_get_dbop_controller(), &kp_sensor_conf);
			}
			
			// to compare the FW version
			DBG_LOG_WARN("the FW version in DB %d, the FW version from Sensor %d", g_sys_sensor_fuota_info.ver, tp_version);
			sleep(10);
			if(g_sys_sensor_fuota_info.ver == tp_version)
			{// when the version info match
				continue; 
			}
			//sleep(60);
			//continue;
			// to disseminate the FW image when the version info does not match
			
			//app_froto_trig_img_dissemination( app_froto_get_longdata_xfer_timer_ctr(), "/home/root/fimage",  0x12345678);
			DBG_LOG_WARN("to disseminate FUOTA image: %s , version %d", g_sys_sensor_fuota_info.fpath, g_sys_sensor_fuota_info.ver);
			 app_froto_img_dissemination_with_block( app_froto_get_longdata_xfer_timer_ctr(), app_sch_get_sensor_op_controller(), g_sys_sensor_fuota_info.fpath,  g_sys_sensor_fuota_info.ver);
			///app_sch_set_sensor_conf( app_sch_get_sensor_op_controller(), &kp_sensor_conf_info)
			sleep(60);
#elif(0) // for database operation test
			app_db_operation_test( gw_pro_command_sqlite_query_item);
			sleep(60);
#elif(0)
			app_sch_set_active_btdevice("C4:BD:6A:10:00:01");
			sleep(10);
#endif
	}
}

/** @brief Create threads in the process "app_pro_general". In the current
 * implementation, five threads are created for IO operation, DB operation,
 * DB data update, posting messages to the BLE process, and debug/test,
 * repectively. TODO: Different threads may be created according to @param
 * pt_ctr in the future.
 */
static void
pro_general_create_threads(struct pro_general_controller *pt_ctr)
{
  int tp_int = -1;
  pthread_t kp_thread_io = -1;
  pthread_t kp_thread_db_op = -1;
  pthread_t kp_thread_db_data_update = -1;
  pthread_t kp_thread_post_to_ble = -1;
  pthread_t kp_thread_tim_service = -1;
  pthread_t tp_thread_dbg_test = -1;
  	
  // Create an IO thread
  tp_int = pthread_create(&kp_thread_io, NULL, thandler_io_state, NULL);
  if(0 != tp_int)
  {
    DBG_LOG_ERR("Failed to create an IO pthread.");
    exit(EXIT_FAILURE);
  }
  tp_int = pthread_create(&kp_thread_io, NULL, thandler_io_state_assist, NULL);
  if(0 != tp_int)
  {
    DBG_LOG_ERR("Failed to create an assited IO pthread.");
    exit(EXIT_FAILURE);
  }
  // Create a database thread to operate with the database
  tp_int = pthread_create(&kp_thread_db_op, NULL, thandler_db_operator,
                          NULL);
  if(0 != tp_int)
  {
    DBG_LOG_ERR("Fail to create a thread for DB operation.");
    exit(EXIT_FAILURE);
  }
  // Create a database thread to operate with the database
  tp_int = pthread_create(&kp_thread_db_data_update, NULL,
                          thandler_db_data_update, NULL);
  if(0 != tp_int)
  {
    DBG_LOG_ERR("Fail to create a thread for DB data update.");
    exit(EXIT_FAILURE);
  }
  // threads to post message packet to process BLE
  tp_int = pthread_create(&kp_thread_post_to_ble, NULL,
                          thandler_post_ipc_msg_to_queue, NULL);
  if(0 != tp_int)
  {
    DBG_LOG_ERR(
      "Fail to create a thread for posting messages to the BLE process.");
    exit(EXIT_FAILURE);
  }
  // Thread for debug/test
  tp_int = pthread_create(&tp_thread_dbg_test, NULL, thandler_operator,
                          NULL);
  if(0 != tp_int)
  {
    DBG_LOG_ERR("Fail to create a thread for debug/test.");
    exit(EXIT_FAILURE);
  }
  // thread to check the waiting object waiting on queue, release them when they are timeout
  tp_int = pthread_create(&kp_thread_tim_service, NULL, thandler_tim_service,
  						  NULL);
  if(0 != tp_int)
  {
	DBG_LOG_ERR("fail to create thread for time service");
	exit(EXIT_FAILURE);
  }

	// thread for LED-control
	tp_int = pthread_create( &tp_thread_dbg_test, NULL, thandler_for_led_control, NULL);
	if(0 != tp_int)
	{
		DBG_LOG_ERR("fail to create thread for LED-control");
		exit(EXIT_FAILURE);
	}

	// threads to deal with modification-report from DB
	tp_int = pthread_create( &tp_thread_dbg_test, NULL, thandler_db_modification_report, NULL);
	if(0 != tp_int)
	{
		DBG_LOG_ERR("fail to create thread for modification report");
		exit(EXIT_FAILURE);
	}
	tp_int = pthread_create( &tp_thread_dbg_test, NULL, thandler_db_modification_readout, NULL);
	if(0 != tp_int)
	{
		DBG_LOG_ERR("fail to create thread for modification readout");
		exit(EXIT_FAILURE);
	}

	// threads to deal with BT HCI event
	tp_int = pthread_create( &tp_thread_dbg_test, NULL, thandler_hci_event, NULL);
	if(0 != tp_int)
	{
		DBG_LOG_ERR("fail to create thread for HCI event handler");
		exit(EXIT_FAILURE);				
	}
	
}
#endif

static app_state_t handler_for_cmd_notification(struct ipc_msg_v2*pt_msg)
{
	app_state_t tpret = ST_OK;
	struct pro_general_controller *pt_gen_ctr = NULL;
	pt_gen_ctr = app_pro_gen_get_ctr();
	
	switch(pt_msg->mtext.feedback_info.info.event_notification_info.bt_event)
	{	
		case BT_EVENT_CONNECTION:
		{ // the BT connection is created
			app_db_clear_db_modification_readout_enable_fg();
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
			pt_gen_ctr->bt_connection_flg[0] = true;	// just make it works. not valid any more
#else
			pt_gen_ctr->bt_connection_flg = true;
#endif
			app_pro_gen_set_active_client_id_str( pt_msg->mtext.feedback_info.info.event_notification_info.info.active_bt_device, MAX_BT_ADDRSTR);

			//to post LED event
			app_io_post_led_event( LED_EV_BLE_ADV_RL);			
			app_io_post_led_event( LED_EV_BLE_CON);


			// to set Bluetooth PHY to 1M			
			// bt_hcitool_set_bluetooth_phy();
			struct bluetooth_connection_param con_param;
			con_param.phy = LE_PHY_1M;
			con_param.min_con_interval = 12;
			con_param.max_con_interval = 12; // 15 mS
			// At most 29 connection events can be silent for the slave if 
			// nothing received or to send (thereby saving energy)
			con_param.con_latency = 29;
			// The connection timeout (timeout would happen if nothing received
			// from slave during two slave-latency connection events)
			con_param.supervision_timeout = (1 + 29) * (12 * 1.25) * 2 + 1;
			con_param.min_ce_len = 12; // 7.5 mS
			con_param.max_ce_len = con_param.min_con_interval * 2;
#if 0 // The max BLE packet length can not be modified at the sensor
			con_param.tx_octets = 251; // The max length of a BLE packet is maximal (i.e., 251 bytes)
			con_param.tx_time = 0x4290; // The max tx time of a packet (to set the max. time)
#endif
			// bt_hcitool_set_bluetooth_connection_param(&con_param);

			break;
		}
		case BT_EVENT_DISCONNECTION:
		{ // BT disconnection happened
			app_db_set_db_modification_readout_enable_fg();
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
			// TODO: maybe need update LED status here
			pt_gen_ctr->bt_connection_flg[0] = false;
#else
			pt_gen_ctr->bt_connection_flg = false;
#endif

			app_froto_update_image_dissem_waiting_obj( app_sch_get_sensor_op_controller(), time(NULL));
#if (SKF_GW_NEW != 1)
			app_tim_set_ble_reading_timeout_fg(true);
#endif
			// to post LED event
			app_io_post_led_event( LED_EV_BLE_CON_RL);			
			
			break;
		}
		case BT_EVENT_SW_TO_CLIENT:
		{ // BT is swtiched to Client when receive this notification
			DBG_LOG_INFO("the BT mode is swtiched to CLIENT successfully");
			app_db_set_db_modification_readout_enable_fg();
			pthread_mutex_lock( &pt_gen_ctr->mtx_for_bt_sw);
			pt_gen_ctr->bt_sw_in_progress = false;
			pt_gen_ctr->bt_role = BT_ROLE_CLIENT;
			pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);

			// to post the LED event
			app_io_post_led_event( LED_EV_BLE_CON_RL);
			app_io_post_led_event( LED_EV_BLE_ADV_RL);
			
			break;
		}
		case BT_EVENT_SW_TO_SERVER:
		{ // BT is switched to SERVER when we receive this notification
			DBG_LOG_INFO("the BT mode is switched to SERVER successfully");
			app_db_set_db_modification_readout_enable_fg();
			pthread_mutex_lock( &pt_gen_ctr->mtx_for_bt_sw);
			pt_gen_ctr->bt_sw_in_progress = false;
			pt_gen_ctr->bt_role = BT_ROLE_SERVER;
			pthread_mutex_unlock( &pt_gen_ctr->mtx_for_bt_sw);

			//to post the LED event
			app_io_post_led_event( LED_EV_BLE_CON_RL);
			app_io_post_led_event( LED_EV_BLE_ADV);

			break;
		}
		case BT_EVENT_ADD_ADAPTER:
		{
			app_db_set_db_modification_readout_enable_fg();
			// to acquire the GW-ID
			int tp_msgid = -1;
			struct ipc_msg_v2 tp_msg = {0};
			tp_msg.mtype = M_TYPE_COMMAND;
			tp_msg.mtext.cmd_info.cmd = CMD_ACQUIRE_GWID;
			tp_msg.mtext.cmd_info.msgid = 0;
			tp_msgid = app_pro_gen_get_msgid_to_pro_ble(app_pro_gen_get_ctr());
			if(tp_msgid < 0)
			{
				DBG_LOG_ERR("fail to get valid msg-id");
				break;
			}
			if(0 != msgsnd(tp_msgid, (void*)&tp_msg, sizeof(union msg_load), IPC_NOWAIT))
			{
				DBG_LOG_ERR("fail to put msg on the queue");
				break;
			}
			break;
		}
		default:
		{
			break;
		}
	}
	return tpret;
}

/*handler to deal with all feedback info from pro-BLE
ret@ST_OK
*/
static app_state_t app_pro_cmd_feedback_handler(struct ipc_msg_v2 kp_msg)
{
	app_state_t tpret = ST_OK;
	if(M_TYPE_FEEDBACK != kp_msg.mtype )
	{
		DBG_LOG_ERR("invalid msg type is given");
		tpret = ST_ERR;
		return tpret;
	}
	switch(kp_msg.mtext.feedback_info.cmd)
	{
		case CMD_SW_TO_CLIENT:
		{
			DBG_LOG_WARN("***********to complete");
			break;
		}
		case CMD_SW_TO_SERVER:
		{
			DBG_LOG_WARN(" the feedback-info get is msgid %d, info %d", kp_msg.mtext.feedback_info.msgid, kp_msg.mtext.feedback_info.info.tpu32);
			
			break;
		}
		case CMD_NOTIFY:
		{
			DBG_LOG_INFO("BT event received is %d", kp_msg.mtext.feedback_info.info.event_notification_info.bt_event);
			handler_for_cmd_notification(&kp_msg);
			break;
		}
		case CMD_ACQUIRE_GWID:
		{
			app_pro_gen_set_gateway_idstr( app_pro_gen_get_ctr(), kp_msg.mtext.feedback_info.info.gw_id);			
			DBG_LOG_INFO("******************************the GW_ID is set to : %s", app_pro_gen_get_gateway_idstr(app_pro_gen_get_ctr()));
			break;
		}
		case CMD_SET_ACTIVE_BTCLIENT:
		{
			DBG_LOG_INFO(" the active BT device is set %d" , kp_msg.mtext.feedback_info.info.tpu32);
			break;
		}
		default:
		{
			DBG_LOG_ERR("unknown CMD");
			tpret = ST_ERR;
			break;
		}
	}

	// to signal the receivation of feedback
	app_sch_signal_and_pop_resp_waiting_obj( app_sch_get_resp_waiting_ctr(), &kp_msg.mtext.feedback_info);
	
	
	return tpret;
}




void
app_pro_general(void)
{

  key_t kp_key = -1;
  int hld_msg_to_ble = -1, hld_msg_from_ble = -1;

  // To create the message queue for IPC
  kp_key = ftok(FPATH_FOR_IPC0, PROJECT_ID_FOR_ICP);
  hld_msg_from_ble = msgget(kp_key, PERMISSION_FLAG_FOR_IPC0 | IPC_CREAT);
  if(hld_msg_from_ble == -1)
  {
    DBG_LOG_ERR("Fail to create msg-queue\n");
    exit(EXIT_FAILURE);
  }
  kp_key = ftok(FPATH_FOR_IPC1, PROJECT_ID_FOR_ICP);
  hld_msg_to_ble = msgget(kp_key, PERMISSION_FLAG_FOR_IPC1 | IPC_CREAT);
  if(hld_msg_to_ble == -1)
  {
    DBG_LOG_ERR("Fail to ceate msg-queue");
    exit(EXIT_FAILURE);
  }
  DBG_LOG_INFO("msg_to_ble %d, msg_from_ble %d\n", hld_msg_to_ble,
               hld_msg_from_ble);

	// NOTE!!! Pay attention to the order of initialization, sometimes it matters!!!!
	app_pro_gen_init_ctr( app_pro_gen_get_ctr());
	app_pro_gen_set_msgid( app_pro_gen_get_ctr(), hld_msg_from_ble, hld_msg_to_ble);
	//to init the waiting-queue for IPC message
	app_wq_init_ctr(app_pro_gen_get_wq_ipc_msg_ctr());

	// to init the controller for file transportation
	app_froto_init_file_trans_ctr( app_froto_get_file_recv_ctr());
	app_froto_init_file_trans_ctr( app_froto_get_file_send_ctr());

	// to init database operation controller
	app_db_init_database_op_ctr( app_db_get_database_op_ctr());
	app_db_init_dbop_controller( app_db_get_dbop_controller());
	// to init message sender for IPC from pro_general to pro_db
	struct database_operation_controller *pt_db_op_ctr = app_db_get_database_op_ctr();
#if 0 // Disabled as we are using the IPC communication in a block manner
	app_com_init_ipc_msg_sender(
		app_com_get_ipc_msg_sender_for_gen_to_db(), 
		pt_db_op_ctr->msgid_to_db, 
		sizeof(gw_pro_message_header_t));
#endif
	

	//to init the resp-waiting controller
	app_sch_init_resp_waiting_ctr( app_sch_get_resp_waiting_ctr());

	//to init controller for communication to sensor
	app_sch_init_sensor_op_controller( app_sch_get_sensor_op_controller());

	//for DB-rpeort handling
	app_db_init_report_handler( app_db_get_report_handler());

	// to init the LED controller
	app_io_init_led_controller(app_io_get_led_ctr());

	// to initialize the controller for Insight-T data saving	
	app_hci_init_dtpacket_saving_ctr( app_hci_get_dtpacket_saving_ctr());

	
	// to create some threads
	pro_general_create_threads( app_pro_gen_get_ctr());

	//________________________________________________________________________________
	
	uint32_t *pt_u32 = 0;
	struct ipc_msg_v2 kp_msg = {0};
	kp_msg.mtype = M_TYPE_COMMAND;
	kp_msg.mtext.cmd_info.cmd = CMD_SW_TO_CLIENT;
	while(1)
	{ // send message to process-ble for test, just one shoot
		//printf("This is the process-general , global-tpval@0x%x = %d\n", &g_tpval, g_tpval);
		DBG_LOG_INFO("This is the process-general, msg_to_ble %d, msg_from_ble %d\n", hld_msg_to_ble, hld_msg_from_ble);
		
		if(0 != msgsnd( hld_msg_to_ble, (void*)&kp_msg, sizeof(union msg_load ), IPC_NOWAIT))
		{
			printf("fail to send to msg-queue\n");
			DBG_LOG_ERR("==========================fail to send to msg-queue");
			exit(EXIT_FAILURE);
		}
		//		
		break;
		//sleep(10);
	}

	ssize_t kp_sz = 0;
	while(1)
	{
		memset( &kp_msg, 0, sizeof(struct ipc_msg_v2));
		kp_sz = msgrcv( hld_msg_from_ble, &kp_msg, sizeof(union msg_load), 0, 0);
		if(kp_sz > 0)
		{
			util_dbg_buf_dump( kp_msg.mtext.proto_pkt.dtbuf, kp_msg.mtext.proto_pkt.len);			
			DBG_LOG_INFO("message received type %d, total-bytes %d", kp_msg.mtype, kp_sz);
			switch(kp_msg.mtype)
			{
				case M_TYPE_PROTO_DATA:
				{ // the Froto-message
					//util_froto_msg_decoding_test( kp_msg.mtext.proto_pkt.dtbuf, kp_msg.mtext.proto_pkt.len, SKFChina_App_AppMessage_fields);					
					app_froto_incomming_msg_handler( kp_msg.mtext.proto_pkt.dtbuf, kp_msg.mtext.proto_pkt.len);
					break;
				}
				case M_TYPE_COMMAND:
				{
					DBG_LOG_WARN("Oops! we DO NOT expect to recv CMD here!");
					break;
				}
				case M_TYPE_FEEDBACK:
				{
					app_pro_cmd_feedback_handler( kp_msg);
					break;
				}
				default:
				{
					DBG_LOG_ERR("unknonwn message type");
					break;
				}
				
			}
		}
		else
		{
			DBG_LOG_WARN("fail to receive msg from queue, for %s", strerror(errno));
		}
	}


	app_froto_definit_file_trans_ctr( app_froto_get_file_recv_ctr());
	app_froto_definit_file_trans_ctr( app_froto_get_file_send_ctr());

	exit(EXIT_SUCCESS);
}



