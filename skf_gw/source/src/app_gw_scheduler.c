#include "app_gw_scheduler.h"
#include "sys_def.h"
#include "util_dbg.h"
#include "app_general_def.h"
#include "sensorConfig.h"
#include "util_froto.h"
#include "app_froto.h"
#include "DeviceAppBulletGateway.pb.h"
#include <pthread.h>
#include "app_pro_general.h"
#include "app_pro_general_new.h"
#include "app_waiting_queue.h"

DBG_LOCAL_LOG_DEBUG


//static struct resp_waiting g_resp_waiting_obj = {0};

//static struct l_queue *g_pt_resp_waiting_queue = NULL; // we put the resp-waiting on the queue
static struct resp_waiting_cotnroller g_resp_waiting_ctr = {0}; 

void app_sch_init_resp_waiting_obj(struct resp_waiting *pt_resp_waiting_obj, uint32_t kp_seq_no)
{
	pthread_mutex_init( &pt_resp_waiting_obj->mtx, NULL);	
	pthread_cond_init( &pt_resp_waiting_obj->cond_resp_received, NULL);
	//pt_resp_waiting_obj->is_fdback_received = false;
	pt_resp_waiting_obj->cmd_resp_event = EV_CMD_RESP_DEFAULT;
	
	pt_resp_waiting_obj->msg_seq_no = kp_seq_no;
	memset( &pt_resp_waiting_obj->feedback_info, 0, sizeof(struct msg_feedback));

	pt_resp_waiting_obj->tim_val = time(NULL);
	return;
}

void app_sch_deinit_resp_waiting_obj(struct resp_waiting *pt_resp_waiting_obj)
{
	pthread_mutex_destroy( &pt_resp_waiting_obj->mtx);
	pthread_cond_destroy( &pt_resp_waiting_obj->cond_resp_received);
}


void app_sch_init_resp_waiting_ctr(struct resp_waiting_cotnroller*pt_resp_waiting_ctr)
{
	pthread_mutex_init( &pt_resp_waiting_ctr->mtx, NULL);
	pt_resp_waiting_ctr->pt_resp_waiting_queue = l_queue_new();
	return;
}


void app_sch_deinit_resp_waiting_ctr(struct resp_waiting_cotnroller*pt_resp_waiting_ctr)
{
	pthread_mutex_lock( &pt_resp_waiting_ctr->mtx);
	#warning "TODO===double check when things go wrong"
	l_queue_destroy( pt_resp_waiting_ctr->pt_resp_waiting_queue, l_free);
	pthread_mutex_unlock( &pt_resp_waiting_ctr->mtx);

	pthread_mutex_destroy( &pt_resp_waiting_ctr->mtx);
	return;
}

struct resp_waiting_cotnroller *app_sch_get_resp_waiting_ctr(void)
{
	return &g_resp_waiting_ctr;
}

void app_sch_put_waiting_obj_on_queue( struct resp_waiting*pt_resp_waiting_obj)
{
	
	pthread_mutex_lock(&g_resp_waiting_ctr.mtx);
	
	l_queue_push_tail( g_resp_waiting_ctr.pt_resp_waiting_queue, pt_resp_waiting_obj);

	pthread_mutex_unlock( &g_resp_waiting_ctr.mtx);
}

/*just remove it
*/
void app_sch_remove_waiting_obj_from_queue(struct resp_waiting*pt_resp_waiting_obj)
{
	pthread_mutex_lock( &g_resp_waiting_ctr.mtx);
	l_queue_remove( g_resp_waiting_ctr.pt_resp_waiting_queue, (void *)pt_resp_waiting_obj);
	pthread_mutex_unlock( &g_resp_waiting_ctr.mtx);
}


//typedef bool (*l_queue_match_func_t) (const void *data, const void *user_data);
static bool q_match_msg_seq_no(const void *data, const void *user_data)
{
	bool tpret = false;
	struct resp_waiting *pt_waiting_obj = (struct resp_waiting*)data;
	uint32_t kp_msg_seq_no = (uint32_t)user_data;
	if(pt_waiting_obj->msg_seq_no == kp_msg_seq_no)
	{
		tpret = true;
	}
	return tpret;
}

app_state_t app_sch_signal_and_pop_resp_waiting_obj(struct resp_waiting_cotnroller*pt_resp_waiting_ctr, struct msg_feedback *pt_fdback_info)
{
	app_state_t tpret = ST_OK;
	struct resp_waiting *pt_resp_waiting_obj = NULL;

	DBG_LOG_INFO("to signal the receivation of CMD response");
	
	pthread_mutex_lock( &pt_resp_waiting_ctr->mtx);

	pt_resp_waiting_obj = l_queue_remove_if( pt_resp_waiting_ctr->pt_resp_waiting_queue, q_match_msg_seq_no, (const void *)pt_fdback_info->msgid);
	if(pt_resp_waiting_obj)
	{
		DBG_LOG_DEBUG(" to signal the receivation of CMD response!");
		// to signal the condiction
		pthread_mutex_lock( &pt_resp_waiting_obj->mtx); // actually this is not necessary
		
		memcpy( &pt_resp_waiting_obj->feedback_info, pt_fdback_info, sizeof(struct msg_feedback));
		//pt_resp_waiting_obj->is_fdback_received = true;
		pt_resp_waiting_obj->cmd_resp_event = EV_CMD_RESP_RECEIVED;
		pthread_cond_signal( &pt_resp_waiting_obj->cond_resp_received);

		pthread_mutex_unlock( &pt_resp_waiting_obj->mtx);

	}
	else
	{
		DBG_LOG_WARN(" to double check no body is wait for this response!");
	}
	pthread_mutex_unlock( &pt_resp_waiting_ctr->mtx);

	return true;
}


/*to set the the active BT device
*/
app_state_t app_sch_set_active_btdevice(uint8_t *pt_btaddr)
{
	app_state_t tpret = ST_OK;
	struct ipc_msg_v2 kp_ipc_msg = {0};

	struct resp_waiting *pt_resp_waiting_obj = NULL;

	kp_ipc_msg.mtype = M_TYPE_COMMAND;	
	kp_ipc_msg.mtext.cmd_info.cmd = CMD_SET_ACTIVE_BTCLIENT;
	kp_ipc_msg.mtext.cmd_info.msgid = app_pro_gen_allocate_msg_seq_no();
	snprintf( kp_ipc_msg.mtext.cmd_info.para.btdev_to_active, MAX_BT_ADDRSTR,"%s", pt_btaddr);

	pt_resp_waiting_obj = (struct resp_waiting*)l_malloc( sizeof(struct resp_waiting));
	if(NULL == pt_resp_waiting_obj)
	{
		DBG_LOG_ERR("malloc fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	memset( pt_resp_waiting_obj, 0, sizeof(struct resp_waiting));

	app_sch_init_resp_waiting_obj( pt_resp_waiting_obj, kp_ipc_msg.mtext.cmd_info.msgid);	
	//app_sch_init_resp_waiting_obj( pt_resp_waiting_obj, 666); // for test only
	// to put the waiting obj on the queue
	app_sch_put_waiting_obj_on_queue( pt_resp_waiting_obj);

	// to put the message on the queue, so it can be sent out
	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	{
		DBG_LOG_ERR("Well , fail to put the msg on the queue");
		app_sch_remove_waiting_obj_from_queue( pt_resp_waiting_obj); // the obj is on the queue, we have to remove it
		
		tpret = ST_ERR;
		goto EXIT;
	}
	DBG_LOG_INFO("to wait for response");

	pthread_mutex_lock( &pt_resp_waiting_obj->mtx);
	while(1)
	{
		if(EV_CMD_RESP_DEFAULT == pt_resp_waiting_obj->cmd_resp_event)
		{
			pthread_cond_wait( &pt_resp_waiting_obj->cond_resp_received, &pt_resp_waiting_obj->mtx);
		}
		else
		{
			break;
		}
	}
	pthread_mutex_unlock( &pt_resp_waiting_obj->mtx);
	if(EV_CMD_RESP_RECEIVED == pt_resp_waiting_obj->cmd_resp_event)
	{
		DBG_LOG_ERR("the active-dev is set successfully");
	}
	else
	{
		DBG_LOG_ERR("fail to receive response from pro-BLE");
		tpret = ST_ERR;
	}
	EXIT:
		if(pt_resp_waiting_obj)
		{
			app_sch_deinit_resp_waiting_obj( pt_resp_waiting_obj);
			l_free( pt_resp_waiting_obj);
			pt_resp_waiting_obj = NULL;
		}
		return tpret;
}


/*to set the BT devices  white list
pt_btaddr_buf@the bt address array
kp_arr_sz@ the address array size
ret@ST_OK is expected if things go well
*/
#if(1)
app_state_t app_sch_set_bt_white_list(struct bt_addr *pt_btaddr_array, uint8_t kp_arr_sz)
{
	app_state_t tpret = ST_OK;
	struct ipc_msg_v2 kp_ipc_msg = {0};

	struct resp_waiting *pt_resp_waiting_obj = NULL;

	if((NULL == pt_btaddr_array)||(0 == kp_arr_sz))
	{
		DBG_LOG_ERR("invalid parameters are given");
		tpret = ST_ERR;
		goto EXIT;
	}

	kp_ipc_msg.mtype = M_TYPE_COMMAND;	
	kp_ipc_msg.mtext.cmd_info.cmd = CMD_SET_WHITE_LIST;
	kp_ipc_msg.mtext.cmd_info.msgid = app_pro_gen_allocate_msg_seq_no();
	for(uint32_t i = 0; i < kp_arr_sz; i++)
	{
		memcpy( &kp_ipc_msg.mtext.cmd_info.para.bt_white_list[i%MAX_BT_WHITE_LIST], &pt_btaddr_array[i], sizeof(struct bt_addr));		
	}
	pt_resp_waiting_obj = (struct resp_waiting*)l_malloc( sizeof(struct resp_waiting));
	if(NULL == pt_resp_waiting_obj)
	{
		DBG_LOG_ERR("malloc fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	memset( pt_resp_waiting_obj, 0, sizeof(struct resp_waiting));

	app_sch_init_resp_waiting_obj( pt_resp_waiting_obj, kp_ipc_msg.mtext.cmd_info.msgid);
	// to put the waiting obj on the queue
	app_sch_put_waiting_obj_on_queue( pt_resp_waiting_obj);

	// to put the message on the queue, so it can be sent out
	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	{
		DBG_LOG_ERR("Well , fail to put the msg on the queue");
		app_sch_remove_waiting_obj_from_queue( pt_resp_waiting_obj); // the obj is on the queue, we have to remove it
		
		tpret = ST_ERR;
		goto EXIT;
	}
	DBG_LOG_INFO("to wait for response");

	pthread_mutex_lock( &pt_resp_waiting_obj->mtx);
	while(1)
	{
		if(EV_CMD_RESP_DEFAULT == pt_resp_waiting_obj->cmd_resp_event)
		{
			pthread_cond_wait( &pt_resp_waiting_obj->cond_resp_received, &pt_resp_waiting_obj->mtx);
		}
		else
		{
			break;
		}
	}
	pthread_mutex_unlock( &pt_resp_waiting_obj->mtx);
	if(EV_CMD_RESP_RECEIVED != pt_resp_waiting_obj->cmd_resp_event)
	{
		DBG_LOG_ERR("fail to set the BT-white list");
		tpret = ST_ERR;
		goto EXIT;
	}
	DBG_LOG_INFO("the BT-white list is set successfully");
	EXIT:
		if(pt_resp_waiting_obj)
		{
			app_sch_deinit_resp_waiting_obj( pt_resp_waiting_obj);
			l_free( pt_resp_waiting_obj);
		}
		return tpret;
}

#endif




/*to be called to disconnect from the peer device and set the MAC to connect after the disconnection
pt_btaddr@the BT addr the connection swtich to

NOTE!! the function will wait for the feedback
*/
app_state_t app_sch_switch_bt_connection(uint8_t *pt_btaddr)
{
	app_state_t tpret = ST_OK;
	struct ipc_msg_v2 kp_ipc_msg = {0};

	struct resp_waiting *pt_resp_waiting_obj = NULL;
	
	// fill the msg to be sent to pro-BLE
	kp_ipc_msg.mtype = M_TYPE_COMMAND;
	kp_ipc_msg.mtext.cmd_info.cmd = CMD_DISCONNECTION;
	kp_ipc_msg.mtext.cmd_info.msgid = app_pro_gen_allocate_msg_seq_no();
	if(pt_btaddr)
	{
		snprintf( &kp_ipc_msg.mtext.cmd_info.para.btdev_to_connect, MAX_BT_ADDRSTR,"%s", pt_btaddr);
	}
	
	pt_resp_waiting_obj = (struct resp_waiting*)l_malloc( sizeof(struct resp_waiting));
	if(NULL == pt_resp_waiting_obj)
	{
		DBG_LOG_ERR(" malloc fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	memset( pt_resp_waiting_obj, 0, sizeof(struct resp_waiting));
	app_sch_init_resp_waiting_obj( pt_resp_waiting_obj, kp_ipc_msg.mtext.cmd_info.msgid);
	app_sch_put_waiting_obj_on_queue( pt_resp_waiting_obj); //to removed from the queue ref@app_sch_signal_and_pop_resp_waiting_obj 

	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	{		
		DBG_LOG_ERR("fail to put msg on the queue, to remove the waiting obj");
		// to remove the waiting obj from the queue
		app_sch_remove_waiting_obj_from_queue( pt_resp_waiting_obj);
	
		tpret = ST_ERR;
		goto EXIT;
	}
	// to wait for response
	pthread_mutex_lock( &pt_resp_waiting_obj->mtx);
	while(1)
	{
		if(EV_CMD_RESP_DEFAULT == pt_resp_waiting_obj->cmd_resp_event)
		{
			pthread_cond_wait( &pt_resp_waiting_obj->cond_resp_received, &pt_resp_waiting_obj->mtx);
		}
		else
		{
			break;
		}
	}
	pthread_mutex_unlock( &pt_resp_waiting_obj->mtx);

	if(EV_CMD_RESP_RECEIVED == pt_resp_waiting_obj->cmd_resp_event)
	{
		tpret = ST_OK;
		DBG_LOG_INFO("the CMD-resp is received from pro-BLE successfully");
	}
	else
	{
		DBG_LOG_ERR("fail to recv response from pro-BLE");
		tpret = ST_ERR;
	}
	EXIT:
		if(pt_resp_waiting_obj)
		{
			app_sch_deinit_resp_waiting_obj( pt_resp_waiting_obj);
			l_free( pt_resp_waiting_obj);
		}
		return tpret;
}




#if(1)

static struct sensor_op_controller g_sensor_op_controller = {0};

struct sensor_op_controller *app_sch_get_sensor_op_controller(void)
{
	return &g_sensor_op_controller;
}

/*
*/
app_state_t app_sch_init_sensor_op_controller(struct sensor_op_controller*pt_sensor_op_ctr)
{
	app_state_t tpret = ST_OK;

	pthread_mutex_init( &pt_sensor_op_ctr->mtx, NULL);
	pt_sensor_op_ctr->pt_sensor_op_waiting_queue = l_queue_new();

	return tpret;
}

/*
*/
app_state_t app_sch_deinit_sensor_op_controller(struct sensor_op_controller*pt_sensor_op_ctr)
{
	pthread_mutex_lock( &pt_sensor_op_ctr->mtx);
	l_queue_destroy( pt_sensor_op_ctr->pt_sensor_op_waiting_queue, l_free);
	pthread_mutex_unlock( &pt_sensor_op_ctr->mtx);
	
	pthread_mutex_destroy( &pt_sensor_op_ctr->mtx);

	return ST_OK;
}


/*to init the waiting object
*/
app_state_t app_sch_init_sensor_op_waiting_obj(struct sensor_op_waiting*pt_sensor_op_waiting_obj, uint32_t kp_msg_seq_no, enum sensor_op_code kp_op_code)
{
	pthread_mutex_init( &pt_sensor_op_waiting_obj->mtx, NULL);
	pthread_cond_init( &pt_sensor_op_waiting_obj->cond_resp_received, NULL);
	pt_sensor_op_waiting_obj->resp_event = EV_SENSOR_OP_DEFAULT;
	pt_sensor_op_waiting_obj->msg_seq_no = kp_msg_seq_no;
	pt_sensor_op_waiting_obj->is_obj_active = true;
	pt_sensor_op_waiting_obj->timval = time(NULL);

	pt_sensor_op_waiting_obj->op_code = kp_op_code;
	
	return ST_OK;
}

/* for an object ,we might need to set a new seq-no to it, call the function to do it
*/
void app_sch_set_seq_no_to_sensor_op_waiting_obj(struct sensor_op_waiting *pt_sensor_op_waiting_obj, uint32_t kp_msg_seq_no)
{
	pt_sensor_op_waiting_obj->msg_seq_no = kp_msg_seq_no;
}


/*void 
*/
void app_sch_set_timval_to_sensor_op_waiting_obj(struct sensor_op_waiting *pt_sensor_op_waiting_obj, time_t kp_timval)
{
	pt_sensor_op_waiting_obj->timval = kp_timval;
}

/*
*/
void app_sch_set_resp_event_to_sensor_op_waiting_obj(struct sensor_op_waiting *pt_sensor_op_waiting_obj, enum sensor_op_resp_event new_event)
{
	pt_sensor_op_waiting_obj->resp_event = new_event;
}

/*to deinit the waiting object
*/
void app_sch_deinit_sensor_op_waiting_obj(struct sensor_op_waiting * pt_sensor_op_waiting_obj)
{
	if(pt_sensor_op_waiting_obj->is_obj_active)
	{
		pthread_cond_destroy( &pt_sensor_op_waiting_obj->cond_resp_received);
		pthread_mutex_destroy( &pt_sensor_op_waiting_obj->mtx);
	}
	return;
}

/*to update the sensor operation waiting object
*/
void app_sch_sensor_waiting_obj_update_timval_and_msg_seq_no( struct sensor_op_waiting *pt_sensor_op_waiting_obj, time_t timval, uint32_t kp_seq_no)
{
	if(NULL == pt_sensor_op_waiting_obj)
	{
		return;
	}
	pthread_mutex_lock(&pt_sensor_op_waiting_obj->mtx);
	pt_sensor_op_waiting_obj->msg_seq_no = kp_seq_no;
	pt_sensor_op_waiting_obj->timval = timval;
	pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);
}

/**/
app_state_t app_sch_put_sensor_op_waiting_obj_on_queue( struct sensor_op_controller *pt_sensor_op_ctr, struct sensor_op_waiting*pt_sensor_op_waiting_obj)
{
	app_state_t tpret = ST_OK;
	if((NULL == pt_sensor_op_ctr)||(NULL == pt_sensor_op_waiting_obj))
	{
		tpret = ST_ERR;
		DBG_LOG_ERR("unexpected NULL");
		return tpret;
	}
	
	DBG_LOG_INFO("to  put waiting obj with seq-no %d  on queue ", pt_sensor_op_waiting_obj->msg_seq_no);
	pthread_mutex_lock( &pt_sensor_op_ctr->mtx);
	l_queue_push_tail(  pt_sensor_op_ctr->pt_sensor_op_waiting_queue, (void *)pt_sensor_op_waiting_obj);
	pthread_mutex_unlock( &pt_sensor_op_ctr->mtx);
	
	return tpret;
}

/*
typedef bool (*l_queue_match_func_t) (const void *data, const void *user_data);
*/
bool queue_match_for_sensor_waiting(const void *data, const void *user_data)
{
	bool tpret = false;
	struct sensor_op_waiting*pt_sensor_op_waiting_obj = (struct sensor_op_waiting*)data;
	uint32_t kp_seq_no = (uint32_t)user_data;
	if(kp_seq_no == pt_sensor_op_waiting_obj->msg_seq_no)
	{
		tpret = true;
	}
	return tpret;
}


#if(0)
/*
pt_sensor_op_ct r@ the sensor opeartion controller
pt_msg_root @ the message tree from sensor
*/

app_state_t app_sch_signal_and_pop_sensor_op_waiting_obj( struct sensor_op_controller *pt_sensor_op_ctr, struct msg_tree_node*pt_msg_root)
{
	app_state_t tpret = ST_OK;
	struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;	
	
	struct l_queue *pt_nd_queu = NULL;
	struct node_path kp_ndpath = {0};
	struct l_queue_entry *pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL;
	uint32_t kp_acked_seq_no = 0;
	
	if((NULL == pt_sensor_op_ctr)||(NULL == pt_msg_root))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}

	pt_nd_queu = l_queue_new();
	if(NULL == pt_nd_queu)
	{	
		DBG_LOG_ERR("fail to create queue");
		tpret = ST_ERR;
		goto EXIT;
	}
	// to fill the node path
	kp_ndpath.path[0] = 5;
	kp_ndpath.path[1] = 1;		
	kp_ndpath.path[2] = 18;
	kp_ndpath.path[3] = 1;
	kp_ndpath.kpdepth = 4;
	if(false == util_froto_locate_msg_node_v2( pt_msg_root, kp_ndpath, pt_nd_queu))
	{
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queu) <= 0)
	{
		DBG_LOG_ERR("fail to find the node msg-seq-no");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries(  pt_nd_queu);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_acked_seq_no = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	//MK441U to check if there is any waiting object on the queue with the seq-no above
	pthread_mutex_lock( &pt_sensor_op_ctr->mtx);

	pt_sensor_op_waiting_obj = l_queue_remove_if( pt_sensor_op_ctr->pt_sensor_op_waiting_queue,  queue_match_for_sensor_waiting, (const void *)kp_acked_seq_no);
	{// to signal the object waiting
		DBG_LOG_INFO("to signal the object wating for respose from senor");
		pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx); // I am pretty sure dead-lock will not happen here

		pt_sensor_op_waiting_obj->is_resp_received = true;
		pthread_cond_signal( &pt_sensor_op_waiting_obj->cond_resp_received);
		
		pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);
	}

	pthread_mutex_unlock( &pt_sensor_op_ctr->mtx);
	//MK441D_____________________________________________________


	
	EXIT:
		if(pt_nd_queu)
		{
			l_queue_destroy( pt_nd_queu, NULL);
			pt_nd_queu = NULL;
		}
	return tpret;
}

#else

static app_state_t to_fill_config_resp( struct sensor_op_waiting *pt_sensor_op_waiting_obj, const struct msg_tree_node*pt_root)
{	
	app_state_t tpret = ST_ERR;
	DBG_LOG_INFO("the payload info is = %d", pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.payload_info);
	switch(pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.payload_info)
	{
		case SKFChina_ConfigurationAndCommand_RetrievePayload_CURRENT_CONFIG_HASH:// = 1,
		{			
			tpret = app_froto_pick_conf_hash_value_and_edit_time( pt_root, pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp, MAX_RETR_CONF_ARRAY );
			break;	
		}
   	 	case SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG:// = 2,
   	 	{
   	 		; //break;
   	 	}
		case SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG: //= 3,
		{
			//DBG_LOG_WARN("BKP==to call app_froto_pick_sensor_conf_info");
			tpret =  app_froto_pick_sensor_conf_info( pt_root, pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp, MAX_RETR_CONF_ARRAY);
			break;
		}
		default:
		{
			DBG_LOG_ERR("unknown payload info %d", pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.payload_info);
			tpret = ST_ERR;
			break;
		}
	}
	return tpret;
}

/*to pick hash and last edif time
*/
static app_state_t to_pick_hash_value_and_last_edit_time_from_msg_config_hash_upload( struct sensor_op_waiting *pt_sensor_op_waiting_obj, const struct msg_tree_node*pt_root)
{
	app_state_t tpret = ST_OK;
	struct node_path kp_ndpath = {0};
	struct l_queue *pt_nd_queue = NULL;
	struct l_queue_entry *pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL;

	uint32_t kp_hashval = 0;
	uint64_t kp_edit_timval = 0;

	if((NULL == pt_sensor_op_waiting_obj)||(NULL == pt_root))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}

	pt_nd_queue = l_queue_new();
	if(NULL == pt_nd_queue)
	{
		DBG_LOG_ERR("fail to create queue");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	// to find hash value
	kp_ndpath.path[0] = SKFChina_App_AppMessage_config_hash_upload_tag;
	kp_ndpath.path[1] = SKFChina_ConfigurationAndCommand_ConfigHashUpload_config_hash_value_tag;
	kp_ndpath.kpdepth = 2;
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_ndpath, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to locate msg node");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length(pt_nd_queue) != 1)
	{
		DBG_LOG_ERR("well! only one node is expected here");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)(pt_entry->data);
	kp_hashval = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	// clean the temporary variable
	memset( &kp_ndpath, 0, sizeof(struct node_path));
	l_queue_clear( pt_nd_queue, NULL);
	// to find the edit time
	kp_ndpath.path[0] = SKFChina_App_AppMessage_config_hash_upload_tag;
	kp_ndpath.path[1] = SKFChina_ConfigurationAndCommand_ConfigHashUpload_last_edit_time_tag;
	kp_ndpath.path[2] = SKFChina_ConfigurationAndCommand_TimeArrayElement_time_tag;
	kp_ndpath.kpdepth = 3;
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_ndpath, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to edit time node");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queue) != 1)
	{
		DBG_LOG_ERR("only one msg node is epxected");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)(pt_entry->data);
	kp_edit_timval = *((uint64_t*)pt_node->nd_data.dtinfo.dtbuf);

	// to set the resp-info
	pt_sensor_op_waiting_obj->sensor_op_resp.conf_hash.hash_val = kp_hashval;
	pt_sensor_op_waiting_obj->sensor_op_resp.conf_hash.last_edit_time = kp_edit_timval;
	
	EXIT:
		if(pt_nd_queue)
		{
			l_queue_destroy( pt_nd_queue, NULL);
			pt_nd_queue = NULL;
		}
		return tpret;
}

/*
*/
static app_state_t to_pick_sensor_op_response_info( struct sensor_op_waiting *pt_sensor_op_waiting_obj, const struct msg_tree_node*pt_root)
{
	app_state_t tpret = ST_OK;
	if((NULL == pt_sensor_op_waiting_obj)||(NULL == pt_root))
	{
		DBG_LOG_ERR("unexpected NULL");
		return ST_ERR;
	}
	//DBG_LOG_WARN("the op-code is = %d", pt_sensor_op_waiting_obj->op_code);
	switch(pt_sensor_op_waiting_obj->op_code)
	{
		case SENSOR_OP_DTSELECT:// = 1, // data selection
		{
			/*...*/
			break;
		}
		case SENSOR_OP_CONF_RETRIEVE:// = 2, // configuration retrieve
		{
			tpret =  to_fill_config_resp( pt_sensor_op_waiting_obj, pt_root);
			break;
		}
		case SENSOR_OP_CONF_DISSEM:// = 3, // configuration dissemination
		{
			tpret = to_pick_hash_value_and_last_edit_time_from_msg_config_hash_upload( pt_sensor_op_waiting_obj, pt_root);
			/*...*/
			break;
		}
		//SENSOR_OP_OTA = 4, // OTA
		/*....*/
		default:
		{
			DBG_LOG_WARN("unrecoginized op-code");
			tpret = ST_ERR;
			break;
		}
	}
	return tpret;
}


/*
pt_sensor_op_ct r@ the sensor opeartion controller
pt_msg_root @ the message tree from sensor
*/
app_state_t app_sch_signal_and_pop_sensor_op_waiting_obj( struct sensor_op_controller *pt_sensor_op_ctr, struct msg_tree_node*pt_msg_root, struct node_path kp_ndpath)
{
	app_state_t tpret = ST_OK;
	struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;	
	
	struct l_queue *pt_nd_queu = NULL;
	struct l_queue_entry *pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL;
	uint32_t kp_acked_seq_no = 0;
	
	if((NULL == pt_sensor_op_ctr)||(NULL == pt_msg_root))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}

	pt_nd_queu = l_queue_new();
	if(NULL == pt_nd_queu)
	{	
		DBG_LOG_ERR("fail to create queue");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	if(false == util_froto_locate_msg_node_v2( pt_msg_root, kp_ndpath, pt_nd_queu))
	{
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queu) <= 0)
	{
		DBG_LOG_ERR("fail to find the node msg-seq-no");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries(  pt_nd_queu);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_acked_seq_no = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	//MK441U to check if there is any waiting object on the queue with the seq-no above
	pthread_mutex_lock( &pt_sensor_op_ctr->mtx);

	pt_sensor_op_waiting_obj = l_queue_remove_if( pt_sensor_op_ctr->pt_sensor_op_waiting_queue,  queue_match_for_sensor_waiting, (const void *)kp_acked_seq_no);
	if(pt_sensor_op_waiting_obj)	
	{// to signal the object waiting
		DBG_LOG_INFO("to signal the object wating for respose from senor");
		pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx); // I am pretty sure dead-lock will not happen here

		//to_fill_config_resp( pt_sensor_op_waiting_obj, pt_msg_root);
		to_pick_sensor_op_response_info( pt_sensor_op_waiting_obj, pt_msg_root);

		//pt_sensor_op_waiting_obj->is_resp_received = true;		
		pt_sensor_op_waiting_obj->resp_event = EV_SENSOR_OP_RESP_RECV;
		pthread_cond_signal( &pt_sensor_op_waiting_obj->cond_resp_received);
		
		pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);
	}
	else
	{
		DBG_LOG_WARN("no senor-op-waiting object with seq-no %d is waitign on queue", kp_acked_seq_no);
	}
	pthread_mutex_unlock( &pt_sensor_op_ctr->mtx);
	//MK441D_____________________________________________________


	
	EXIT:
		if(pt_nd_queu)
		{
			l_queue_destroy( pt_nd_queu, NULL);
			pt_nd_queu = NULL;
		}
	return tpret;
}

#endif

/*to remove the given object from the queue, just remove it ,no reasons
*/
void app_sch_remove_sensor_op_waiting_obj_from_queue(struct sensor_op_controller*pt_sensor_op_ctr, struct sensor_op_waiting *pt_sensor_op_waiting_obj)
{
	if((NULL == pt_sensor_op_ctr)||(NULL == pt_sensor_op_waiting_obj))
	{
		return;
	}
	pthread_mutex_lock( &pt_sensor_op_ctr->mtx);
	l_queue_remove( pt_sensor_op_ctr->pt_sensor_op_waiting_queue, (void *)pt_sensor_op_waiting_obj);
	pthread_mutex_unlock( &pt_sensor_op_ctr->mtx);
}	

/*to load the sensor configuraion from DataBase
pt_sensor_conf@the buffer to store the sensor configuration read from Database
*/
app_state_t to_load_sensor_configuration_from_database(sensorConfig_t *pt_sensor_conf)
{
	app_state_t tpret = ST_OK;

	/* to complete */

	return tpret;
}


/*to read sensor configuration from sensor
*/
app_state_t to_read_sensor_configuration_from_sensor(struct sensor_op_controller*pt_sensor_op_ctr ,sensorConfig_t *pt_sensor_conf)
{
	return ST_ERR;	
}


/*to synchronize the sensor configuration
*/
app_state_t app_sch_sync_sensor_configuration(void)
{
	app_state_t tpret = ST_OK;
	sensorConfig_t kp_sensor_conf = {0};
	
	if(ST_OK != to_load_sensor_configuration_from_database( &kp_sensor_conf))
	{
		DBG_LOG_ERR("fail to load sensor configuration from database");
		tpret = ST_ERR;
		return tpret;
	}

	

	

	return ST_ERR;	
}



/*to get the configuration hash and last-edit time
pt_sensor_op_ctr@the controller
pt_hashvalue@the buffer where hash is returned
pt_edit_time@the edit time is returned

ret@true when things go well

NOTE !!! this function will wait for response from the sensor
*/
app_state_t app_sch_read_conf_hash_and_last_edit_time(struct sensor_op_controller*pt_sensor_op_ctr, uint32_t *pt_hashvalue, uint32_t *pt_edit_time)
{
	app_state_t tpret = ST_OK;
	struct encoded_froto_msg_pkt kp_encoded_msg = {0};
	struct config_to_retrieve kp_config_to_retrieve = {0};
	struct ipc_msg_v2  kp_ipc_msg = {0};	

	struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;

	if((NULL == pt_sensor_op_ctr)||(NULL == pt_hashvalue)||(NULL == pt_edit_time))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	// to allocate the waiting object
	pt_sensor_op_waiting_obj = (struct sensor_op_waiting*)l_malloc(sizeof(struct sensor_op_waiting));
	if(NULL == pt_sensor_op_waiting_obj)
	{
		DBG_LOG_ERR("malloc fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	memset( pt_sensor_op_waiting_obj, 0, sizeof(struct sensor_op_waiting));
	
	//to fill the info for retrieving-operation
	kp_config_to_retrieve.payload_info = SKFChina_ConfigurationAndCommand_RetrievePayload_CURRENT_CONFIG_HASH;
	kp_config_to_retrieve.unit_num = 0;
	kp_config_to_retrieve.msg_seq_no = app_pro_gen_allocate_msg_seq_no();

	// to init the waiting object
	app_sch_init_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, kp_config_to_retrieve.msg_seq_no, SENSOR_OP_CONF_RETRIEVE);
	pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.payload_info = kp_config_to_retrieve.payload_info;
	
 	kp_encoded_msg = util_froto_fill_msg_config_retrieve( &kp_config_to_retrieve, DBG_SENSOR_ID_STR, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()));
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded msg");
		tpret = ST_ERR;
		goto EXIT;
	}

	// the IPC msg
	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to release the encoded message
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);	

	// the obj will be removed from the queue by  queue_match_for_sensor_waiting when resp is received	
	app_sch_put_sensor_op_waiting_obj_on_queue( pt_sensor_op_ctr,  pt_sensor_op_waiting_obj);

	// to put the msg on the waiting queue
	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	{
		DBG_LOG_ERR("fail to put msg on the queue");
		// the request is not sent to sensor, so to remove the obj
		app_sch_remove_sensor_op_waiting_obj_from_queue(pt_sensor_op_ctr,  pt_sensor_op_waiting_obj);
		
		tpret = ST_ERR;
		goto EXIT;
	}

	//to wait for response
	pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx);
	while(1)
	{
		//if(true != pt_sensor_op_waiting_obj->is_resp_received)
		if(EV_SENSOR_OP_DEFAULT == pt_sensor_op_waiting_obj->resp_event)			
		{
			pthread_cond_wait( &pt_sensor_op_waiting_obj->cond_resp_received, &pt_sensor_op_waiting_obj->mtx);
		}
		else
		{
			break;
		}
	}
	pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);

	if(EV_SENSOR_OP_RESP_RECV != pt_sensor_op_waiting_obj->resp_event)
	{
		DBG_LOG_WARN("unexpected sensor-op event %d", pt_sensor_op_waiting_obj->resp_event);
		tpret = ST_ERR;
		goto EXIT;
	}
	DBG_LOG_INFO("====the response is received from sensor successfully====hash-val = 0x%x=last-edit-time=%d====",\
	pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp[0].conf_val.hash_and_last_edit_time.hash_val,\
	pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp[0].conf_val.hash_and_last_edit_time.last_edit_time);
	* pt_hashvalue = pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp[0].conf_val.hash_and_last_edit_time.hash_val;
	* pt_edit_time = pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp[0].conf_val.hash_and_last_edit_time.last_edit_time;
	
	EXIT:

		// to release the waiting object
		if(pt_sensor_op_waiting_obj)
		{
			app_sch_deinit_sensor_op_waiting_obj( pt_sensor_op_waiting_obj);
			l_free( pt_sensor_op_waiting_obj);
			pt_sensor_op_waiting_obj = NULL;
			
		}
	
		return tpret;

}


// The configuration items which are not supported by bullet sensor have been commented
const SKFChina_Common_SpecificConfigItem g_specific_conf_id[] = {
    /* 0 - 12, 20 - 114 used */
    SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_RATE_HZ,// = 0,
    // SKFChina_Common_SpecificConfigItem_ACC_SENSOR_X_ENABLE,// = 1,
    // SKFChina_Common_SpecificConfigItem_ACC_SENSOR_Y_ENABLE,// = 2,
    // SKFChina_Common_SpecificConfigItem_ACC_SENSOR_Z_ENABLE,// = 3,
    SKFChina_Common_SpecificConfigItem_ACC_SENSOR_AXIS,// = 49,
    SKFChina_Common_SpecificConfigItem_ACC_SENSOR_RANGE_G,// = 4,
    // SKFChina_Common_SpecificConfigItem_ACC_SENSOR_ALARM_X_THRESHOLD_G,// = 6,
    // SKFChina_Common_SpecificConfigItem_ACC_SENSOR_ALARM_Y_THRESHOLD_G,// = 7,
    // SKFChina_Common_SpecificConfigItem_ACC_SENSOR_ALARM_Z_THRESHOLD_G,// = 8,
    SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_PTS,// = 20,
    // SKFChina_Common_SpecificConfigItem_TEMPERATURE_SAMPLE_PERIOD_S,// = 9,
    SKFChina_Common_SpecificConfigItem_TEMPERATURE_ALARM_THRESHOLD_CDGREE,// = 10,
    // SKFChina_Common_SpecificConfigItem_TEMPERATURE_SENSOR_ENABLE,// = 21,
    // SKFChina_Common_SpecificConfigItem_HUMIDITY_SAMPLE_PERIOD_S,// = 11,
    // SKFChina_Common_SpecificConfigItem_HUMIDITY_ALARM_THRESHOLD_PERCENTAGE,// = 12,
    // SKFChina_Common_SpecificConfigItem_HUMIDITY_SENSOR_ENABLE,// = 22,
    // SKFChina_Common_SpecificConfigItem_SPEED_TRIGGER_HZ,/// = 23,
    // SKFChina_Common_SpecificConfigItem_DIAMETER_WHEEL_CM,// = 24,
    SKFChina_Common_SpecificConfigItem_BORE_DIAMETER_MM,// = 85,
    SKFChina_Common_SpecificConfigItem_BATTERY_LOW_ALARM_THRESHOLD_PERCENTAGE,// = 25,
    // SKFChina_Common_SpecificConfigItem_BATTERY_FULL_NOTIFY_THRESHOLD_PERCENTAGE,// = 26,
    // SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_URL,// = 27,
    // SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_PORT,// = 28,
    // SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_USERNAME,// = 110,
    // SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_PASSWORD,// = 111,
    SKFChina_Common_SpecificConfigItem_WORK_MODE,// = 29,
    SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_ADV_DBM,// = 30,
    SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_AUX_ADV_DBM,// = 31,
    SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_AUX_ADV_KBPS,// = 32, /* kbps */
    SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_CONN_DBM,// = 33,
    SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_CONN_KBPS,// = 34, /* kbps */
    SKFChina_Common_SpecificConfigItem_CURRENT_TIME,// = 35,
    SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME,// = 112,
    SKFChina_Common_SpecificConfigItem_GATEWAY_ADDR,// = 36,
    SKFChina_Common_SpecificConfigItem_QUICKPOLLING_PERIOD_S,// = 37,
    SKFChina_Common_SpecificConfigItem_REGULARSENSING_PERIOD_S,// = 38,
    SKFChina_Common_SpecificConfigItem_COMMUNICATION_PERIOD_S,/// = 39,
    SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S,// = 40,
    SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S,// = 41,
    SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S ,//= 42,
    SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_PERIOD_S,// = 113,
    SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S,// = 114,
    SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_FS_HZ,// = 43,
    SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_N,// = 44,
    SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_AXIS_ACQ_EVAL,// = 45,
    SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_RANGE,// = 46,
    SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_FS_HZ,// = 47,
    SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_N,// = 48,
    SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_AXIS_ACQ_EVAL,// = 50,
    SKFChina_Common_SpecificConfigItem_ACQ_MAG_FS_HZ,// = 51,
    SKFChina_Common_SpecificConfigItem_ACQ_MAG_N,// = 52,
    SKFChina_Common_SpecificConfigItem_ACQ_MAG_AXIS_ACQ_EVAL,// = 53,
    SKFChina_Common_SpecificConfigItem_FS_COEF,// = 54,
    SKFChina_Common_SpecificConfigItem_GEE_COEF,// = 55,
    SKFChina_Common_SpecificConfigItem_V_COEF,// = 56,
    SKFChina_Common_SpecificConfigItem_MEAS_POSITION,// = 57,
    SKFChina_Common_SpecificConfigItem_MEAS_LOAD,// = 58,
    SKFChina_Common_SpecificConfigItem_MEAS_AXIS_VIB,// = 59,
    SKFChina_Common_SpecificConfigItem_MEAS_AXIS_MAG,// = 60,
    SKFChina_Common_SpecificConfigItem_VIB_START_FG,// = 61,
    SKFChina_Common_SpecificConfigItem_VIB_RMS_START_TL,// = 62,
    SKFChina_Common_SpecificConfigItem_MAG_START_FG,// = 63,
    SKFChina_Common_SpecificConfigItem_MAG_RMS_START_TL,// = 64,
    SKFChina_Common_SpecificConfigItem_MAG_STABLE_FG,// = 65,
    SKFChina_Common_SpecificConfigItem_MAG_RMS_VAR_TH,// = 66,
    SKFChina_Common_SpecificConfigItem_RPM_VAR_RANGE,// = 67,
    SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_M,// = 68,
    SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_N,// = 69,
    SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_TEMP,// = 70,
    SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_M,// = 71,
    SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_N,// = 72,
    SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_VIB,// = 73,
    SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_MAG,// = 74,
    SKFChina_Common_SpecificConfigItem_FUNC_ANOM_TEMP,// = 75,
    SKFChina_Common_SpecificConfigItem_FUNC_ANOM_OV,// = 76,
    SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MECH,// = 77,
    SKFChina_Common_SpecificConfigItem_FUNC_ANOM_BRG,// = 78,
    SKFChina_Common_SpecificConfigItem_FUNC_ANOM_LUB,// = 79,
    SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MTR,// = 80,
    SKFChina_Common_SpecificConfigItem_FUNC_ANOM_GEAR,// = 81,
    SKFChina_Common_SpecificConfigItem_FUNC_ANOM_FAN,// = 82,
    SKFChina_Common_SpecificConfigItem_FUNC_ANOM_PUMP,// = 83,
    SKFChina_Common_SpecificConfigItem_ASSET_LEVEL,// = 84,
    SKFChina_Common_SpecificConfigItem_FLEX_TYPE,// = 86,
    SKFChina_Common_SpecificConfigItem_RUN_SPEED_RPM,// = 87,
    SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFO,// = 88,
    SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFI,// = 89,
    SKFChina_Common_SpecificConfigItem_BRG_INFO_BSF,// = 90,
    SKFChina_Common_SpecificConfigItem_BRG_INFO_FTF,// = 91,
    SKFChina_Common_SpecificConfigItem_MTR_INFO_FL,// = 92,
    SKFChina_Common_SpecificConfigItem_MTR_INFO_BAR,// = 93,
    SKFChina_Common_SpecificConfigItem_GEAR_INFO_TOOTH,// = 94,
    SKFChina_Common_SpecificConfigItem_FAN_INFO_BLADE,// = 95,
    SKFChina_Common_SpecificConfigItem_PUMP_INFO_VANE,// = 96,
    SKFChina_Common_SpecificConfigItem_TEMP_OV_ALERT_CDEGREE,// = 97,
    SKFChina_Common_SpecificConfigItem_ACC_OV_ALERT,// = 98,
    SKFChina_Common_SpecificConfigItem_ACC_OV_ALARM ,//= 99,
    SKFChina_Common_SpecificConfigItem_ACC_HAL_ALERT,// = 100,
    SKFChina_Common_SpecificConfigItem_ACC_HAL_ALARM,// = 101,
    SKFChina_Common_SpecificConfigItem_VEL_OV_ALERT,// = 102,
    SKFChina_Common_SpecificConfigItem_VEL_OV_ALARM,// = 103,
    SKFChina_Common_SpecificConfigItem_VEL_HAL_ALERT,// = 104,
    SKFChina_Common_SpecificConfigItem_VEL_HAL_ALARM,// = 105,
    SKFChina_Common_SpecificConfigItem_ENV_OV_ALERT,// = 106,
    SKFChina_Common_SpecificConfigItem_ENV_OV_ALARM,// = 107,
    SKFChina_Common_SpecificConfigItem_ENV_HAL_ALERT,// = 108,
    SKFChina_Common_SpecificConfigItem_ENV_HAL_ALARM,// = 109
};

static SKFChina_Common_fSchedulerConfigItem g_fscheduler_config_id[] = { /* 0 - 6 used */
    /* fScheduler
 At least M sets of valid data required over N days */
    SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_M,// = 0, /* The expected items of data every N days */
    SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_N,// = 1, /* The period (in day). */
    /* (no time zone, UTC+0, cloud guarantees second must be zero): The start
 of the period. (Default 2022-07-15 00:00:00 UTC+0) */
    SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_NSTARTMOMENT,// = 2,
    /* Next 10 wake-up (to sense) moments. Follow the last update if failed 
 to update the WS-Array. Follow the built-in rule if nothing is valid 
 in the WS-Array. The built-in rule is to wake up (to sense) according
 to the preset WS moment if M items of data in N days are not met. 
 (Default null) */
    SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WSARRAY,// = 3, /* (no time zone, UTC+0) */
    /* To mark the MUSTwake-up moments restored in WS-Array. (Default null) */
    SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WSSTAR_ARRAY,// = 4, /* (no time zone, UTC+0) */
    /* To describe the sensing attribute (e.g., sample rate, sample duration,
 etc.) corresponding to WSArray. This array is application-dependent. */
    SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WSDESC_ARRAY,// = 5,
    /* Next 5 wake-up (to communicate) moments. Follow the last update if 
 failed to update the WC-Array; otherwise, the preset WC moment is
 applied. (Default null) (MAX interval: 3 days) */
    SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WCARRAY,// = 6 /* (no time zone, UTC+0) */
};


//
app_state_t app_sch_read_sensor_conf(struct sensor_op_controller*pt_sensor_op_ctr, sensorConfig_t*pt_sensor_conf)
{
	app_state_t tpret = ST_OK;
	struct encoded_froto_msg_pkt kp_encoded_msg = {0};
	struct ipc_msg_v2  kp_ipc_msg = {0};
	struct config_to_retrieve kp_config_to_retrieve = {0};
	uint32_t kp_arr_size = 0, tpidx = 0;

	// the specific configuration first
	kp_arr_size = sizeof(g_specific_conf_id) / sizeof(g_specific_conf_id[0]);
	tpidx = 0;	
	memset( &kp_config_to_retrieve, 0, sizeof(struct config_to_retrieve));		
	while(1)
	{
		kp_config_to_retrieve.conf.specific_conf_array[kp_config_to_retrieve.unit_num] = g_specific_conf_id[tpidx % kp_arr_size];		
		kp_config_to_retrieve.unit_num++;
		tpidx++;
		if((kp_config_to_retrieve.unit_num >= MAX_RETR_CONF_ARRAY)||(tpidx >= kp_arr_size))
		{
			kp_config_to_retrieve.msg_seq_no = app_pro_gen_allocate_msg_seq_no();
			kp_config_to_retrieve.payload_info = SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG;			
			//kp_config_to_retrieve.payload_info = SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG;
			kp_encoded_msg = util_froto_fill_msg_config_retrieve( &kp_config_to_retrieve, DBG_SENSOR_ID_STR,  app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()));
			
			if(false == kp_encoded_msg.is_valid)
			{
				DBG_LOG_ERR("fail to encode msg");
				break;
			}
			// fill the IPC msg
			kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
			memset(kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
			kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
			memcpy(kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_encoded_msg.len);
			// to release the msg
			util_froto_release_encoded_msg_v2( &kp_encoded_msg);

			if(ST_OK != app_wq_post_to_waiting_queue(  app_pro_gen_get_wq_ipc_msg_ctr() , (uint8_t*)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
			{
				break;
			}

			
			// continue to wait for response here
	

			// to reset buf
			memset( &kp_config_to_retrieve, 0, sizeof(struct config_to_retrieve));	
			
		}

		
		if(tpidx >= kp_arr_size)
		{
			break;
		}

		
		#if(0)
			break; // for debug only
		#endif
	}
	
	return tpret;
}


/*
*/
app_state_t to_set_sensor_conf(sensorConfig_t *pt_sensor_conf, const struct sensor_op_waiting*pt_sensor_waiting_obj, const struct config_to_retrieve*pt_conf_to_retrieve)
{
	app_state_t tpret = ST_OK;
	uint32_t i = 0;
	if((NULL == pt_sensor_conf)||(NULL == pt_sensor_waiting_obj)||(NULL == pt_conf_to_retrieve))
	{
		DBG_LOG_ERR("invalid parameter is given");
		tpret = ST_ERR;
		return tpret;
	}
	DBG_LOG_INFO("===conf-unit to retrieve %d, the following is the response :", pt_conf_to_retrieve->unit_num);
	util_dbg_buf_dump((uint8_t *)pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp, sizeof(struct sensor_conf_retrieve_resp)*MAX_RETR_CONF_ARRAY);		
#if(1)
	for(i = 0; i < pt_conf_to_retrieve->unit_num; i++)
	{
		if( SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG == pt_conf_to_retrieve->payload_info)
		{
			switch(pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp[i%MAX_RETR_CONF_ARRAY].conf_item_id.specific_item_id)
			{
	    		case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_RATE_HZ:
	    		{
	    			pt_sensor_conf->bulletSensorConfig.senConfig.fAccHz =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
	    			break;
	    		}
    			case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_X_ENABLE://,// = 1,
				{
	    			break; // X
	    		}
				case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_Y_ENABLE://,// = 2,
				{
	    			break;//X
	    		}
				case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_Z_ENABLE://,// = 3,
				{
	    			break;//X
	    		}
				case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_AXIS:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i%MAX_RETR_CONF_ARRAY].conf_val.val_u8;
	    			break;
	    		}
				case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_RANGE_G:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_range =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i%MAX_RETR_CONF_ARRAY].conf_val.val_u8;
	    			break;
	    		}
				case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_ALARM_X_THRESHOLD_G:
				{
	    			break;//XXXX
	    		}
				case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_ALARM_Y_THRESHOLD_G:
				{
	    			break;//XXXX
	    		}		
				case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_ALARM_Z_THRESHOLD_G:
				{
	    			break; //XXX
	    		}
				case SKFChina_Common_SpecificConfigItem_ACC_SENSOR_SAMPLE_PTS:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.nAcc = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
	    			break;
	    		}
				case SKFChina_Common_SpecificConfigItem_TEMPERATURE_SAMPLE_PERIOD_S:
				{
									break;//XXX
				}
				case SKFChina_Common_SpecificConfigItem_TEMPERATURE_ALARM_THRESHOLD_CDGREE:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.AlarmThrestemp =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i%MAX_RETR_CONF_ARRAY].conf_val.val_i32;
					break;
				}

    			case SKFChina_Common_SpecificConfigItem_TEMPERATURE_SENSOR_ENABLE:
				{
									break; //XXX
				}

				case SKFChina_Common_SpecificConfigItem_HUMIDITY_SAMPLE_PERIOD_S:
				{
									break;//XXX
				}

				case SKFChina_Common_SpecificConfigItem_HUMIDITY_ALARM_THRESHOLD_PERCENTAGE:
				{
									break; // XXX
				}

    			case SKFChina_Common_SpecificConfigItem_HUMIDITY_SENSOR_ENABLE:
				{
									break; //XXX
				}
				case SKFChina_Common_SpecificConfigItem_SPEED_TRIGGER_HZ:
				{
									break; //XXX
				}

				case SKFChina_Common_SpecificConfigItem_DIAMETER_WHEEL_CM:
				{
									break;//XXX
				}

				case SKFChina_Common_SpecificConfigItem_BORE_DIAMETER_MM:
				{
					//pt_sensor_conf->bulletSensorConfig.
					pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i%MAX_RETR_CONF_ARRAY].conf_val.val_float; 
					break;
				}

				case SKFChina_Common_SpecificConfigItem_BATTERY_LOW_ALARM_THRESHOLD_PERCENTAGE:
				{
					pt_sensor_conf->bulletSensorConfig.sysConfig.batteryAlarmThrePercent = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i%MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_BATTERY_FULL_NOTIFY_THRESHOLD_PERCENTAGE:
				{
									break;//XXXX
				}

				case SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_URL:
				{
									break;//XXX
				}

				case SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_PORT:
				{
									break;//XXX
				}

				case SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_USERNAME:
				{
									break;//XXX
				}

				case SKFChina_Common_SpecificConfigItem_CLOUD_BACKEND_MQTT_PASSWORD:
				{
									break; //XXX
				}

				case SKFChina_Common_SpecificConfigItem_WORK_MODE:
				{
					pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					// N.B.: For 1.0.x, DO NOT add new feature any more.
					// So DO NOT support STATUS_SENSING_FEATURE_ENABLE
					break;
				}

    			case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_ADV_DBM:
				{
					
					pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAdv =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_AUX_ADV_DBM:
				{
					
					pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAuxAdv = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_AUX_ADV_KBPS:
				{
					pt_sensor_conf->bulletSensorConfig.sysConfig.dataRateAuxAdv =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u16;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_TX_POWER_BLE_CONN_DBM:
				{
					//pt_sensor_conf->bulletSensorConfig.sysConfig. = pt_sensor_waiting_obj->conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_DATA_RATE_BLE_CONN_KBPS:
				{
					//pt_sensor_conf->bulletSensorConfig.sysConfig.
					break;
				}

				case SKFChina_Common_SpecificConfigItem_CURRENT_TIME:
				{
					pt_sensor_conf->bulletSensorConfig.sysConfig.currentTimeS = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u64;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_LATEST_CONFIG_TIME:
				{
					pt_sensor_conf->lastEditTimeS = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u64;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_GATEWAY_ADDR:
				{
					//pt_sensor_conf->bulletSensorConfig.sysConfig.
									break;
				}

				case SKFChina_Common_SpecificConfigItem_QUICKPOLLING_PERIOD_S:
				{
					pt_sensor_conf->bulletSensorConfig.schConfig.period_quickPolling_s = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_REGULARSENSING_PERIOD_S:
				{
					pt_sensor_conf->bulletSensorConfig.schConfig.period_regularSensing_s = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_COMMUNICATION_PERIOD_S:
				{
					pt_sensor_conf->bulletSensorConfig.schConfig.period_comm_s = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_QUICKPOLLING_REF_STARTTIME_S:
				{
					// Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
					// but locally use a uint32_t to store
					pt_sensor_conf->bulletSensorConfig.schConfig.refTime_quickPolling_s = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_REGULARSENSING_REF_STARTTIME_S:
				{
					// Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
					// but locally use a uint32_t to store
					pt_sensor_conf->bulletSensorConfig.schConfig.refTime_regularSensing_s = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_COMMUNICATION_REF_STARTTIME_S:
				{
					// Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
					// but locally use a uint32_t to store
					pt_sensor_conf->bulletSensorConfig.schConfig.refTime_period_comm_s = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_PERIOD_S:
				{
					pt_sensor_conf->bulletSensorConfig.schConfig.period_wave_s =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_WAVEDATA_ACQ_REF_STARTTIME_S:
				{
					// Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
					// but locally use a uint32_t to store
					pt_sensor_conf->bulletSensorConfig.schConfig.refTime_wave_s =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_FS_HZ:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_N:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_n =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_AXIS_ACQ_EVAL:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_axis_acq_eval =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_PRE_ACQ_VIB_RANGE:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_range =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_FS_HZ:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_N:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_n =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_PRE_ACQ_MAG_AXIS_ACQ_EVAL:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_axis_acq_eval =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_ACQ_MAG_FS_HZ:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_fs_hz =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_ACQ_MAG_N:
  				{
  					pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_n =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}  			
				case SKFChina_Common_SpecificConfigItem_ACQ_MAG_AXIS_ACQ_EVAL:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FS_COEF:
				{
					pt_sensor_conf->bulletSensorConfig.senConfig.fs_coef =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				// the following are for algorithm
				case SKFChina_Common_SpecificConfigItem_GEE_COEF:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}    			
				case SKFChina_Common_SpecificConfigItem_V_COEF:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.V_COEF =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_MEAS_POSITION:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_POSITION =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_MEAS_LOAD:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_LOAD =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;	
					break;
				}
				case SKFChina_Common_SpecificConfigItem_MEAS_AXIS_VIB:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_MEAS_AXIS_MAG:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_VIB_START_FG:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_START_FG =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_VIB_RMS_START_TL:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_RMS_START_TL =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_MAG_START_FG:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_START_FG =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_MAG_RMS_START_TL:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_START_TL =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_MAG_STABLE_FG:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_STABLE_FG =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool; 
					break;
				}
				case SKFChina_Common_SpecificConfigItem_MAG_RMS_VAR_TH:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_RPM_VAR_RANGE:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.RPM_VAR_RANGE =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_M:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u16;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_TEMP_N:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u16;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_TEMP:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_TEMP =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u16;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_M:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u16;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_VIB_N:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u16;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_VIB:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u16;
					break;
				}				
				case SKFChina_Common_SpecificConfigItem_DEC_LOGIC_LEARN_NUM_MAG:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u16;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_TEMP:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_OV:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_OV =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MECH:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_BRG:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_LUB:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_MTR:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_GEAR:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_FAN:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FUNC_ANOM_PUMP:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_bool;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_ASSET_LEVEL:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.ASSET_LEVEL =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FLEX_TYPE:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FLEX_TYPE =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u8;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_RUN_SPEED_RPM:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFO:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFO =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_BRG_INFO_BPFI:
				{
					 pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFI =
					   pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					   conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_BRG_INFO_BSF:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BSF =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_BRG_INFO_FTF:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_FTF =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_MTR_INFO_FL:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_FL =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_MTR_INFO_BAR:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_BAR =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_GEAR_INFO_TOOTH:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_FAN_INFO_BLADE:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.FAN_INFO_BLADE =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_PUMP_INFO_VANE:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.PUMP_INFO_VANE =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_u32;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_TEMP_OV_ALERT_CDEGREE:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_ACC_OV_ALERT:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALERT =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_ACC_OV_ALARM:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALARM =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_ACC_HAL_ALERT:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALERT =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_ACC_HAL_ALARM:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALARM =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_VEL_OV_ALERT:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALERT =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_VEL_OV_ALARM:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALARM =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_VEL_HAL_ALERT:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALERT =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_VEL_HAL_ALARM:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALARM =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_ENV_OV_ALERT:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALERT =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_ENV_OV_ALARM:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALARM = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}

				case SKFChina_Common_SpecificConfigItem_ENV_HAL_ALERT:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALERT =
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}
				case SKFChina_Common_SpecificConfigItem_ENV_HAL_ALARM:
				{
					pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALARM = 
					  pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.
					  conf_retrieve_resp[i % MAX_RETR_CONF_ARRAY].conf_val.val_float;
					break;
				}			
				default:
    			{
    				break;
    			}
}
		}
		else if(SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG == pt_conf_to_retrieve->payload_info)
		{
			switch(pt_sensor_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp[i%MAX_RETR_CONF_ARRAY].conf_item_id.scheduler_item_id)
			{ /* 0 - 6 used */
	 			   /* fScheduler
					At least M sets of valid data required over N days */
	    		case SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_M://,// = 0, /* The expected items of data every N days */
				{
					break;
				}			
				case SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_N://,// = 1, /* The period (in day). */
				{
					break;
				}

	    /* (no time zone, UTC+0, cloud guarantees second must be zero): The start
	 of the period. (Default 2022-07-15 00:00:00 UTC+0) */
	    		case SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_NSTARTMOMENT://,// = 2,
				{
					break;
				}
				/* Next 5 

				/* Next 10 wake-up (to sense) moments. Follow the last update if failed 
	 to update the WS-Array. Follow the built-in rule if nothing is valid 
	 in the WS-Array. The built-in rule is to wake up (to sense) according
	 to the preset WS moment if M items of data in N days are not met. 
	 (Default null) */
	    		case SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WSARRAY://,// = 3, /* (no time zone, UTC+0) */
				{
					break;
				}
				/* Next 5 

				/* To mark the MUSTwake-up moments restored in WS-Array. (Default null) */
	    		case SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WSSTAR_ARRAY://,// = 4, /* (no time zone, UTC+0) */
				{
					break;
				}
	    /* Next 5 

	    /* To describe the sensing attribute (e.g., sample rate, sample duration,
	 etc.) corresponding to WSArray. This array is application-dependent. */
	    		case SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WSDESC_ARRAY://,// = 5,
				{
					break;
				}
	    /* Next 5 wake-up (to communicate) moments. Follow the last update if 
	 failed to update the WC-Array; otherwise, the preset WC moment is
	 applied. (Default null) (MAX interval: 3 days) */
	    		case SKFChina_Common_fSchedulerConfigItem_FSCHEDULER_WCARRAY://,// = 6 /* (no time zone, UTC+0) */
	    		{
	    			break;
	    		}
				default:
	    		{
	    			break;
	    		}
			}
		}
		else
		{
			DBG_LOG_ERR("unknown payload info");
		}
	}
#endif
	return tpret;
}

/*the following function wait for response from sensor
*/
app_state_t app_sch_read_sensor_conf_v2(struct sensor_op_controller*pt_sensor_op_ctr, sensorConfig_t*pt_sensor_conf)
{
	app_state_t tpret = ST_OK;
	struct encoded_froto_msg_pkt kp_encoded_msg = {0};
	struct ipc_msg_v2  kp_ipc_msg = {0};
	struct config_to_retrieve kp_config_to_retrieve = {0};
	uint32_t kp_arr_size = 0, tpcnt = 0, i = 0;
	uint8_t retryCnt;

	struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;

	// to allocate a waiting object
	pt_sensor_op_waiting_obj = (struct sensor_op_waiting*)l_malloc( sizeof(struct sensor_op_waiting));
	if(NULL == pt_sensor_op_waiting_obj)
	{
		DBG_LOG_ERR("malloc fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	memset(pt_sensor_op_waiting_obj, 0, sizeof(struct sensor_op_waiting));
	app_sch_init_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, 0, SENSOR_OP_CONF_RETRIEVE);

	// the specific configuration first
	kp_arr_size = sizeof(g_specific_conf_id) / sizeof(g_specific_conf_id[0]);
	tpcnt = 0;	
	memset( &kp_config_to_retrieve, 0, sizeof(struct config_to_retrieve));		
	for(i = 0; i < kp_arr_size; i++)
	{
		if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
			app_pro_gen_get_ctr()))
		{
			tpret = ST_ERR;
			goto EXIT;
		}
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
		if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
		if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
		{
			tpret = ST_ERR;
			goto EXIT;
		}

		// to load the conf-ID to read
		kp_config_to_retrieve.conf.specific_conf_array[kp_config_to_retrieve.unit_num] = g_specific_conf_id[i % kp_arr_size];		
		kp_config_to_retrieve.unit_num++;
		tpcnt++;
		if((kp_config_to_retrieve.unit_num < MAX_RETR_CONF_ARRAY)&&(tpcnt < kp_arr_size))
		{
			continue;
		}
		
		retryCnt = 0;
		do
		//if((kp_config_to_retrieve.unit_num >= MAX_RETR_CONF_ARRAY)||(tpcnt >= kp_arr_size))
		{
			kp_config_to_retrieve.msg_seq_no = app_pro_gen_allocate_msg_seq_no();
			kp_config_to_retrieve.payload_info = SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG;			
			//kp_config_to_retrieve.payload_info = SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG;
			kp_encoded_msg = util_froto_fill_msg_config_retrieve( &kp_config_to_retrieve, DBG_SENSOR_ID_STR,  app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()));
			
			if(false == kp_encoded_msg.is_valid)
			{
				DBG_LOG_ERR("fail to encode msg");
				tpret = ST_ERR;
				break;
			}
			// fill the IPC msg
			kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
			memset(kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
			kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
			memcpy(kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_encoded_msg.len);
			// to release the msg
			util_froto_release_encoded_msg_v2( &kp_encoded_msg);

			// to put obj on the waiting queue
			pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.payload_info = SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG;
			memset( pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp, 0, sizeof(struct sensor_conf_retrieve_resp)*MAX_RETR_CONF_ARRAY);
			
			app_sch_set_seq_no_to_sensor_op_waiting_obj(pt_sensor_op_waiting_obj, kp_config_to_retrieve.msg_seq_no);
			app_sch_set_timval_to_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, time(NULL));
			app_sch_set_resp_event_to_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, EV_SENSOR_OP_DEFAULT);
			app_sch_put_sensor_op_waiting_obj_on_queue( pt_sensor_op_ctr, pt_sensor_op_waiting_obj);

			if(ST_OK != app_wq_post_to_waiting_queue(  app_pro_gen_get_wq_ipc_msg_ctr() , (uint8_t*)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
			{
				DBG_LOG_ERR("fail to put msg on the IPC queue");
				tpret = ST_ERR;
				app_sch_remove_sensor_op_waiting_obj_from_queue( pt_sensor_op_ctr, pt_sensor_op_waiting_obj);
				//break;
				goto EXIT;
			}
			retryCnt++;
			DBG_LOG_INFO("to wait for response from sensor");
			
			//MK1025U continue to wait for response here
			pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx);
			while(1)
			{
				//if(true != pt_sensor_op_waiting_obj->is_resp_received)					
				if(EV_SENSOR_OP_DEFAULT == pt_sensor_op_waiting_obj->resp_event)
				{
					
					pthread_cond_wait( &pt_sensor_op_waiting_obj->cond_resp_received, &pt_sensor_op_waiting_obj->mtx);
				}
				else
				{
					break;
				}
			}
			pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);
			//MK1025D_________________________________________________

			if(EV_SENSOR_OP_RESP_RECV == pt_sensor_op_waiting_obj->resp_event)
			{
				DBG_LOG_INFO("================the sensor config is received successfully");

				/**********to set conf************/
				//util_dbg_buf_dump((uint8_t *)pt_sensor_op_waiting_obj->conf_retrieve_resp, sizeof(struct sensor_conf_retrieve_resp)*MAX_RETR_CONF_ARRAY);
				to_set_sensor_conf( pt_sensor_conf, pt_sensor_op_waiting_obj, &kp_config_to_retrieve);
				
				// The sensor is slow , we need to wait for it
				DBG_LOG_DEBUG("Delay for a while to wait for sensor");
				usleep(150*1000U);
				memset( &kp_config_to_retrieve, 0, sizeof(struct config_to_retrieve));	
				break;
			}
			else if(EV_SENSOR_OP_TIMEOUT == pt_sensor_op_waiting_obj->resp_event)
			{
				DBG_LOG_WARN("================sensor-conf reading timeout");
				if(retryCnt >= 3)
				{
				  tpret = ST_ERR;
				  goto EXIT;
				}
			}
			else
			{
				DBG_LOG_ERR(" unexpected resp event");
				if(retryCnt >= 3)
				{
				  tpret = ST_ERR;
				  goto EXIT;
				}
			}
			if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
				app_pro_gen_get_ctr()))
			{
				tpret = ST_ERR;
				goto EXIT;
			}
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
			if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
			if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
			{
				tpret = ST_ERR;
				goto EXIT;
			}

			DBG_LOG_WARN("Delay and retry ...");
			sleep(retryCnt);
		}while(1);

	}
#if(0)
// for BULLET, no fScheduler-configuration
	DBG_LOG_INFO("*********************888***************to read fscheduler_conf");
	sleep(5);
	//to read the fscheduler_config
	kp_arr_size = sizeof(g_fscheduler_config_id) / sizeof(g_fscheduler_config_id[0]);
	tpcnt = 0;
	memset( &kp_config_to_retrieve, 0, sizeof(struct config_to_retrieve));
	for(i = 0; i < kp_arr_size; i++)
	{
		kp_config_to_retrieve.conf.specific_conf_array[kp_config_to_retrieve.unit_num] = g_fscheduler_config_id[i % kp_arr_size];
		kp_config_to_retrieve.unit_num++;
		tpcnt++;
		if((kp_config_to_retrieve.unit_num < MAX_RETR_CONF_ARRAY)&&(tpcnt < kp_arr_size))
		{
			continue;
		}

		kp_config_to_retrieve.msg_seq_no = app_pro_gen_allocate_msg_seq_no();
		kp_config_to_retrieve.payload_info = SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG;
		kp_encoded_msg =  util_froto_fill_msg_config_retrieve( &kp_config_to_retrieve, DBG_SENSOR_ID_STR, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()));
		if(false == kp_encoded_msg.is_valid)
		{
			DBG_LOG_ERR("fail to encode froto msg");
			tpret = ST_ERR;
			break;
		}
		// fill the IPC msg
		kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
		memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
		kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
		memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_encoded_msg.len);
		// to release the msg buffer
		util_froto_release_encoded_msg_v2( &kp_encoded_msg);
		
		// to put obj on the waiting queue
		pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.payload_info = SKFChina_ConfigurationAndCommand_RetrievePayload_FSCHEDULER_CURRENT_CONFIG;
		memset( pt_sensor_op_waiting_obj->sensor_op_resp.sensor_conf_retrieve.conf_retrieve_resp, 0, sizeof(struct sensor_conf_retrieve_resp)*MAX_RETR_CONF_ARRAY);
		app_sch_set_seq_no_to_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, kp_config_to_retrieve.msg_seq_no);
		app_sch_set_timval_to_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, time(NULL));
		app_sch_set_resp_event_to_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, EV_SENSOR_OP_DEFAULT);
		// to put the object on the waiting queue
		app_sch_put_sensor_op_waiting_obj_on_queue( pt_sensor_op_ctr, pt_sensor_op_waiting_obj);

		if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
		{
			DBG_LOG_ERR("fail to put msg on the IPC queue");			
			app_sch_remove_sensor_op_waiting_obj_from_queue( pt_sensor_op_ctr, pt_sensor_op_waiting_obj);
			tpret = ST_ERR;
			//break;
			goto EXIT;
		}

		//MK1106U________to wait for response______
		pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx);
		while(1)
		{
			if(EV_SENSOR_OP_DEFAULT == pt_sensor_op_waiting_obj->resp_event)
			{
				pthread_cond_wait( &pt_sensor_op_waiting_obj->cond_resp_received, &pt_sensor_op_waiting_obj->mtx);
			}
			else
			{
				break;
			}
		}
		pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);
		//MK1106D__________________________________
		if(EV_SENSOR_OP_RESP_RECV == pt_sensor_op_waiting_obj->resp_event)
		{
			DBG_LOG_INFO("************************the sensor conf is received successfully");
			/*   ...    */		
			//util_dbg_buf_dump((uint8_t *)pt_sensor_op_waiting_obj->conf_retrieve_resp, sizeof(struct sensor_conf_retrieve_resp)*MAX_RETR_CONF_ARRAY);		
			to_set_sensor_conf( pt_sensor_conf, pt_sensor_op_waiting_obj, &kp_config_to_retrieve);
			
		}
		else if(EV_SENSOR_OP_TIMEOUT == pt_sensor_op_waiting_obj->resp_event)
		{
			DBG_LOG_INFO("***********************the sensor-conf reading timeout");
		}
		else
		{
			DBG_LOG_ERR("unexpected resp event");
		}
		//to reset buf
		memset( &kp_config_to_retrieve, 0, sizeof(struct config_to_retrieve));
		
	}
#endif	
	EXIT:
		if(pt_sensor_op_waiting_obj)
		{	
			app_sch_deinit_sensor_op_waiting_obj( pt_sensor_op_waiting_obj);
			l_free( pt_sensor_op_waiting_obj);
			pt_sensor_op_waiting_obj = NULL;
		}
	return tpret;
}


/*to initialize sensorConfig for debug
*/
void app_sch_init_sensor_config_for_test(sensorConfig_t *pt_sensor_conf)
{
	if(NULL == pt_sensor_conf)
	{
		DBG_LOG_ERR("unexpected NULL");
		return;
	}
	//
	pt_sensor_conf->nameId = 666;
	snprintf(pt_sensor_conf->manufacturer, sizeof(pt_sensor_conf->manufacturer), "%s", "SKF_IOT");
	pt_sensor_conf->macAddr.addr.addrArray[0] = 0xC4;//{ 0xC4, 0xBD, 0x6A, 0x11, 0x00, 0x31};
	pt_sensor_conf->macAddr.addr.addrArray[1] = 0xBD;
	pt_sensor_conf->macAddr.addr.addrArray[2] = 0x6A;
	pt_sensor_conf->macAddr.addr.addrArray[3] = 0x11;
	pt_sensor_conf->macAddr.addr.addrArray[4] = 0x00;
	pt_sensor_conf->macAddr.addr.addrArray[5] = 0x31;
	
	snprintf( pt_sensor_conf->type, sizeof(pt_sensor_conf->type), "%s","BUF00001");
	
	//Group-system
	pt_sensor_conf->bulletSensorConfig.sysConfig.batteryAlarmThrePercent = 20;
	
	pt_sensor_conf->bulletSensorConfig.sysConfig.sensorMode = BULLET_SENSOR_MODE_NORMAL;
	// N.B.: For 1.0.x, DO NOT add new feature any more.
	// So DO NOT support STATUS_SENSING_FEATURE_ENABLE

	pt_sensor_conf->bulletSensorConfig.sysConfig.currentTimeS = time(NULL);
	pt_sensor_conf->bulletSensorConfig.sysConfig.dataRateAuxAdv = SYSCONFIG_BLEDATARATE_1MBPS;
	pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAdv = SYSCONFIG_BLETXPOWER_0_DBM;
	pt_sensor_conf->bulletSensorConfig.sysConfig.txPowerAuxAdv = SYSCONFIG_BLETXPOWER_0_DBM;
	// Group-sch-Config
	pt_sensor_conf->bulletSensorConfig.schConfig.period_comm_s = 28800;
	pt_sensor_conf->bulletSensorConfig.schConfig.period_quickPolling_s = 28800;
	pt_sensor_conf->bulletSensorConfig.schConfig.period_regularSensing_s = 28800;
	pt_sensor_conf->bulletSensorConfig.schConfig.period_wave_s = 0;
	pt_sensor_conf->bulletSensorConfig.schConfig.refTime_period_comm_s = 0;
	pt_sensor_conf->bulletSensorConfig.schConfig.refTime_quickPolling_s = 0;
	pt_sensor_conf->bulletSensorConfig.schConfig.refTime_regularSensing_s =0;
	pt_sensor_conf->bulletSensorConfig.schConfig.refTime_wave_s = 0;
	// Group-sensor
	pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y;
	pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_fs_hz = 1000;
	pt_sensor_conf->bulletSensorConfig.senConfig.acq_mag_n = 3000;
	pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X;
	pt_sensor_conf->bulletSensorConfig.senConfig.acq_vib_range = SENSINGCONFIG_VIBRANGE_16GEE;
	pt_sensor_conf->bulletSensorConfig.senConfig.fAccHz = 32000;
	pt_sensor_conf->bulletSensorConfig.senConfig.fs_coef = 3.2;
	pt_sensor_conf->bulletSensorConfig.senConfig.nAcc =64000;
	pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_axis_acq_eval = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y;
	pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz = 1000;
	pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_mag_n = 1000;
	pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_axis_acq_eval = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X;
	pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz = 3200;
	pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_n = 3200;
	pt_sensor_conf->bulletSensorConfig.senConfig.pre_acq_vib_range = SENSINGCONFIG_VIBRANGE_16GEE;
	//Group-algorithm
	pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALARM = 0.5f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_HAL_ALERT = 0.2f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALARM = 0.5f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.ACC_OV_ALERT = 0.2f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.AlarmThrestemp = 80;
	pt_sensor_conf->bulletSensorConfig.algoConfig.ASSET_LEVEL = 1;
	pt_sensor_conf->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM = 0;
	pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFI = 5.416f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BPFO = 3.584f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_BSF = 4.7105f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.BRG_INFO_FTF = 0.398f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG =5 ;
	pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_TEMP = 20;
	pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB = 5;
	pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M = 3;
	pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N = 6;
	pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M = 3;
	pt_sensor_conf->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N = 5;
	pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALARM = 0.3f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_HAL_ALERT = 0.15f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.ENV_OV_ALARM = 1.5f; 
	pt_sensor_conf->bulletSensorConfig.algoConfig.FAN_INFO_BLADE = 2;
	pt_sensor_conf->bulletSensorConfig.algoConfig.FLEX_TYPE = 2;
	pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG = false;
	pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN = false;
	pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR = false;
	pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB = false;
	pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH = false;
	pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR = false;
	pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_OV = true;
	pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP = false;
	pt_sensor_conf->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP = true;
	pt_sensor_conf->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH = 17;
	pt_sensor_conf->bulletSensorConfig.algoConfig.GEE_COEF = 9.81f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_START_TL = 0.1f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH = 0.05f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_STABLE_FG = true;
	pt_sensor_conf->bulletSensorConfig.algoConfig.MAG_START_FG = true;
	pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y;
	pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB = SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X;
	pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_LOAD = 0;
	pt_sensor_conf->bulletSensorConfig.algoConfig.MEAS_POSITION = 1;
	pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_BAR = 10;
	pt_sensor_conf->bulletSensorConfig.algoConfig.MTR_INFO_FL = 50;
	pt_sensor_conf->bulletSensorConfig.algoConfig.PUMP_INFO_VANE = 2;
	pt_sensor_conf->bulletSensorConfig.algoConfig.RPM_VAR_RANGE = 0.1f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.RUN_SPEED_RPM = 0;
	pt_sensor_conf->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE = 60;
	pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALARM = 0.2f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_HAL_ALERT = 0.2f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALARM = 0.5f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.VEL_OV_ALERT = 0.2f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_RMS_START_TL = 0.002f;
	pt_sensor_conf->bulletSensorConfig.algoConfig.VIB_START_FG = true;
	pt_sensor_conf->bulletSensorConfig.algoConfig.V_COEF = 1000;

}

/*to set configuration to sensor
/*to set sensor configuration
pt_sensor_op_ctr@the sensor-operation controller
pt_sensor_conf@the sensor-configuration going to set to sensor
ret@ST_OK is expected to be returned when things go well
*/
app_state_t app_sch_set_sensor_conf(struct sensor_op_controller*pt_sensor_op_ctr, const sensorConfig_t*pt_sensor_conf)
{
	app_state_t tpret = ST_OK;

	struct encoded_froto_msg_pkt kp_encoded_msg = {0};
	struct ipc_msg_v2 kp_ipc_msg = {0};
	struct conf_dissem_info kp_conf_dissem_info = {0};

	struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;
	const uint32_t kp_arr_sz = sizeof(g_specific_conf_id) / sizeof(g_specific_conf_id[0]);
	uint32_t tpcnt = 0, i = 0;
	uint8_t retryCnt;
		
	if((NULL == pt_sensor_op_ctr)||(NULL == pt_sensor_conf))
	{
		DBG_LOG_ERR("un");
		tpret = ST_ERR;
		goto EXIT;
	}

	// to allocate an waiting object
	pt_sensor_op_waiting_obj = (struct sensor_op_waiting*)l_malloc(sizeof(struct sensor_op_waiting));
	if(NULL == pt_sensor_op_waiting_obj)
	{
		DBG_LOG_ERR("malloc fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	memset( pt_sensor_op_waiting_obj, 0, sizeof(struct sensor_op_waiting));
	app_sch_init_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, 0, SENSOR_OP_CONF_DISSEM);
	
	// to fill the data structure for configuration dissemination
	snprintf( kp_conf_dissem_info.gw_macstr, MAX_ID_STR_LENGTH,"%s",app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()));
	app_pro_gen_get_active_client_id_str( kp_conf_dissem_info.sensor_macstr, MAX_ID_STR_LENGTH);
	kp_conf_dissem_info.pt_sensor_conf = pt_sensor_conf;

	tpcnt = 0;
	for( i = 0; i < kp_arr_sz; i++)
	{
		if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
			app_pro_gen_get_ctr()))
		{
			tpret = ST_ERR;
			goto EXIT;
		}
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
		if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
		if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
		{
			tpret = ST_ERR;
			goto EXIT;
		}
		
		//DBG_LOG_ERR("kp_arr_sz = %d, i = %d, tpcnt = %d, item_num = %d", kp_arr_sz, i, tpcnt, kp_conf_dissem_info.item_num);
		kp_conf_dissem_info.conf_item_array.specific[kp_conf_dissem_info.item_num % MAX_CONF_ITEM_DISSEM] = g_specific_conf_id[i];
		kp_conf_dissem_info.item_num++;
		tpcnt ++;
		if((kp_conf_dissem_info.item_num < MAX_CONF_ITEM_DISSEM)&&(tpcnt < kp_arr_sz))
		{
			continue;
		}

		retryCnt = 0;
		do{
			kp_conf_dissem_info.seq_no = app_pro_gen_allocate_msg_seq_no();
			
			kp_encoded_msg = util_froto_fill_msg_specific_config_dissem( &kp_conf_dissem_info);
			if(false == kp_encoded_msg.is_valid)
			{
				DBG_LOG_ERR("fail to get encoded msg");
				tpret = ST_ERR;
				break;
			}
			
			// to fill ipc msg
			kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
			memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA); 
			kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
			memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_encoded_msg.len);
			
			// to release the encoded msg
			util_froto_release_encoded_msg_v2( &kp_encoded_msg);

			// to clean the respone buffer
			memset( &pt_sensor_op_waiting_obj->sensor_op_resp, 0, sizeof(union sensor_op_response));
			//to put waiting object on waiting queue
			app_sch_set_seq_no_to_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, kp_conf_dissem_info.seq_no);
			app_sch_set_timval_to_sensor_op_waiting_obj(  pt_sensor_op_waiting_obj, time(NULL));
			app_sch_set_resp_event_to_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, EV_SENSOR_OP_DEFAULT);
			app_sch_put_sensor_op_waiting_obj_on_queue( pt_sensor_op_ctr,  pt_sensor_op_waiting_obj);

			#if(1)
			if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
			{
				app_sch_remove_sensor_op_waiting_obj_from_queue( pt_sensor_op_ctr, pt_sensor_op_waiting_obj);
				DBG_LOG_ERR("fail to put msg on the IPC queue");
				tpret = ST_ERR;
				goto EXIT;
			}
			#else
				DBG_LOG_ERR("this is only for debug, remeber to put message on queue");
			#endif
			retryCnt++;
			DBG_LOG_INFO("to wait for response from sensor");
			pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx);
			while(1)
			{
				if(EV_SENSOR_OP_DEFAULT == pt_sensor_op_waiting_obj->resp_event)
				{
					pthread_cond_wait( &pt_sensor_op_waiting_obj->cond_resp_received, &pt_sensor_op_waiting_obj->mtx);
				}
				else
				{
					break;
				}
			}
			pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);

			if(EV_SENSOR_OP_RESP_RECV != pt_sensor_op_waiting_obj->resp_event)
			{
				DBG_LOG_ERR("fail to get resp from sensor, when waiting  set conf to sensor");
				if(retryCnt >= 3)
				{
				  tpret = ST_ERR;
				  goto EXIT;
				}
				if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
					app_pro_gen_get_ctr()))
				{
				  tpret = ST_ERR;
				  goto EXIT;
				}
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
				if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
				if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
				{
				  tpret = ST_ERR;
				  goto EXIT;
				}

				DBG_LOG_WARN("Delay and retry ...");
				sleep(retryCnt);
			}else{
				// The sensor is slow , we need to wait for it
				DBG_LOG_DEBUG("Delay for a while to wait for sensor");
				usleep(150*1000);
				kp_conf_dissem_info.item_num = 0;
				memset( &kp_conf_dissem_info.conf_item_array.specific, 0, sizeof(SKFChina_Common_SpecificConfigItem)*MAX_CONF_ITEM_DISSEM);
				DBG_LOG_INFO("TOFix=======continue to check if the conf-hash match====");
				DBG_LOG_DEBUG("===resp-info===conf-hash=0x%X, edif-time=%d", pt_sensor_op_waiting_obj->sensor_op_resp.conf_hash.hash_val, pt_sensor_op_waiting_obj->sensor_op_resp.conf_hash.last_edit_time);

				// to reset the conf-id array
				kp_conf_dissem_info.item_num = 0;
				memset( &kp_conf_dissem_info.conf_item_array.specific, 0, sizeof(SKFChina_Common_SpecificConfigItem)*MAX_CONF_ITEM_DISSEM);

				break;
			}
		}while(1);
		
	}

	//DBG_LOG_ERR("BKP2098");

	EXIT:
		if(pt_sensor_op_waiting_obj)
		{
			app_sch_deinit_sensor_op_waiting_obj( pt_sensor_op_waiting_obj);
			l_free( pt_sensor_op_waiting_obj);
			pt_sensor_op_waiting_obj = NULL;
		}
	return tpret;
}





/*to sychronize the time with sensor

NOTE!? no way to now the operation is successful or failed, no response from the sensor

*/
app_state_t app_sch_synchronize_time( const uint8_t*pt_gwid_str, const uint8_t*pt_sensor_id)
{
	app_state_t tpret = ST_OK;
	struct encoded_froto_msg_pkt kp_encoded_msg = {0};
	uint32_t kp_seq_no = 0;
	struct ipc_msg_v2 kp_ipc_msg = {0};

	kp_seq_no = app_pro_gen_allocate_msg_seq_no();
	
	kp_encoded_msg = util_froto_fill_msg_config_dissem_set_time( pt_gwid_str, pt_sensor_id, kp_seq_no);
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get encoded msg");
		tpret = ST_ERR;
		goto EXIT;
	}
	// to fill the IPC msg 
	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);
	
	// to release the msg-buffer
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	// to put the msg on the queue, so it can be sent to sensor
	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	{
		DBG_LOG_ERR("fail to put msg on waiting queue");
		tpret = ST_ERR;
		goto EXIT;
	}
	EXIT:	
		return tpret;
}
#endif









