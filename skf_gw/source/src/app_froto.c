/* definition for application based on Froto message*/

#include "app_froto.h"
#include "util_froto.h"
#include "util_dbg.h"
#include "DeviceAppBulletGateway.pb.h"
#include "ConfigurationAndCommand.pb.h"
#include "Froto.pb.h"
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "app_general_def.h"
#include "pb_common.h"

#include "util_froto.h"

#include "app_pro_general.h"
#include "app_gw_scheduler.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "app_db.h"
#include "util_froto_datawrap.h"
#include "app_common.h"
#include "app_waiting_queue.h"
#include "app_pro_general_new.h"
#include "app_gw_scheduler.h"
#include "app_tim_service.h"

DBG_LOCAL_LOG_DEBUG


uint8_t app_froto_shortDataReveivedFg = 0;
uint8_t app_froto_shortDataReveivedRetryCnt = 0;
static int32_t pick_froto_submsg_tag(const struct msg_tree_node *pt_root);

//static bool is_mtype_to_file(const SKFChina_Common_MeasurementType kp_mtype);

/*to compare the task id of the ftcb with the given id-value
*/
static bool to_compare_task_id( void* pt_data, const uint32_t tsk_id)
{
	bool tpret = false;
	if(NULL == pt_data)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}
	struct file_trans_control_block*pt_ftcb = (struct file_trans_control_block*)pt_data;

	if(pt_ftcb->tskid == tsk_id)
	{//found it
		tpret = true;
	}
	return tpret;
}




/*function to pick the time info for time sychronization
*/
static app_state_t locate_time_array(const struct msg_tree_node* pt_root)
{
	app_state_t tpret = ST_OK;
	struct node_path kp_nd_path = {.path= {7,5,3,1,1},.kpdepth = 5};
	struct msg_tree_node *pt_node = NULL;
	
	struct l_queue *pt_node_queue = l_queue_new();
	struct l_queue_entry *pt_entry = NULL;

	uint32_t kp_len = 0;
		
	if(NULL == pt_root)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(true == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_node_queue))
	{
		kp_len = l_queue_length(pt_node_queue);
		pt_entry = l_queue_get_entries( pt_node_queue);
		DBG_LOG_INFO("the length of time array is %d", kp_len);
		for(uint32_t i = 0; i < kp_len; i++)
		{
			pt_node = (struct msg_tree_node*)pt_entry->data;
			DBG_LOG_DEBUG("msg node info: role = %d, tag = %d, value = %d", pt_node->role, pt_node->tag, *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf));
			//.....
			pt_entry = pt_entry->next;
		}
	}
	else
	{
		tpret = ST_ERR;
		DBG_LOG_ERR("fail to find the time array");
	}

	EXIT:
		/*NOTE!!!DO NOT try to free the node on the queue; 
		util_froto_release_msg_tree will have the job done
		*/
		l_queue_destroy( pt_node_queue, NULL); 
		return tpret;
}


/*function to give response to data-selection msg  for battery volume
ret@ST_OK is expected if all go well
NOTE!!! we just put the responsing message on the waiting-queue, and thread thandler_post_ipc_msg_to_queue will pick the message and send
it to process-BLE, so the message can be passed on to the BT peer-device
*/
static app_state_t data_upload_remaining_battery_volume(const  uint32_t kpbat)
{
	app_state_t  tpret = ST_OK;
	
	struct ipc_msg_v2 kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {.is_valid = false,0};

	//util_froto_fill_msg_data_selection_retrieve_battery(void)
	kp_encoded_msg = util_froto_fill_msg_up_data_upload();

	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded message");
		tpret = ST_ERR;
		return tpret;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to release the encoded msg pkt
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	// to put message on the waiting-queue
	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(kp_ipc_msg)))
	{
		DBG_LOG_ERR("fail to put message on the waiting-queue");
		tpret = ST_ERR;
	}
	
	return tpret;
}

/*function to handle with Froto message data selection
pt_root ~ 
ret@ST_OK, when every thing goes as you expect
*/
static app_state_t msg_handler_for_data_selection(const struct msg_tree_node*pt_root)
{
	app_state_t tpret = ST_OK;
	struct node_path kp_nd_path = {.path = { 3, 4},.kpdepth = 2};
	struct msg_tree_node *pt_node = NULL;

	struct l_queue *pt_node_queue = l_queue_new(); // put all the node found on the queue
	struct l_queue_entry *pt_entry = NULL;

	uint32_t kp_m_type = 0;
	uint32_t kp_q_len = 0;


	if((NULL == pt_root)||(pt_root->role != ND_ROOT))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		goto EXIT;
	}

	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_node_queue))
	{
		DBG_LOG_ERR("fail to locate the info-node");
		goto EXIT;
	}

	kp_q_len = l_queue_length( pt_node_queue);
	pt_entry = l_queue_get_entries( pt_node_queue);
	DBG_LOG_INFO(" the length of measurement-type queue is %d", kp_q_len);
	for(uint32_t i = 0;i < kp_q_len; i++)
	{
		pt_node = (struct msg_tree_node*)pt_entry->data;
		kp_m_type = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);
		switch(kp_m_type)
		{
			case SKFChina_Common_MeasurementType_REMAINING_VOLUME:
			{
				//DBG_LOG_INFO("You need to Report the Battery Volume");
				data_upload_remaining_battery_volume(666);
				break;
			}
			//...
			default:
			{
				DBG_LOG_INFO("unknown M-type %d", kp_m_type);
				break;
			}
		}
		pt_entry = pt_entry->next;
	}
	EXIT:
		l_queue_destroy( pt_node_queue, NULL);
		return tpret;
}

/*to give response to each image block-dissem
ret@ST_OK when things go well
*/
static app_state_t image_block_rquest_give_feedback(const uint32_t kp_tskid, const uint32_t kp_msg_seq_no)
{
	app_state_t tpret = ST_OK;

	struct ipc_msg_v2 kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {.is_valid = false, 0};

	kp_encoded_msg = util_froto_fill_msg_up_image_block_request(kp_tskid, kp_msg_seq_no);

	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded msg");
		tpret = ST_ERR;
		return tpret;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to release the encoded msg pkt
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(kp_ipc_msg)))
	{
		tpret = ST_ERR;
		DBG_LOG_ERR("fail to put messsage on the waiting-queue");
	}
	
	return tpret;
}


/*to handle in-coming message image_block_dissem
ret@ST_OK when things go well
*/
static app_state_t msg_handler_for_image_block_dissem(const struct msg_tree_node*pt_root)
{
	app_state_t tpret = ST_OK;
	bool is_blk_dissem_done = false;
	uint32_t tp_tskid = 0, tp_msg_seq_no = 0, tp_total_blk = 0, tp_cur_blk_idx = 0;

	struct node_path kp_nd_path = {0};
	struct msg_tree_node *pt_node = NULL;

	struct l_queue *pt_node_queue = l_queue_new();

	struct file_trans_controller *pt_ftcr = NULL;
	struct file_trans_control_block *pt_ftcb = NULL;

	if((NULL == pt_root)||(pt_root->role != ND_ROOT))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		goto EXIT;
	}

	// to pick the and check the data received
	/*.........................*/
	// to get the total block
	kp_nd_path.path[0] = SKFChina_App_AppMessage_image_block_dissem_tag;
	kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_header_tag;
	kp_nd_path.path[2] = SKFChina_Froto_FrotoHeader_total_block_tag;
	kp_nd_path.kpdepth = 3;
	if(false == app_froto_get_node( pt_root, kp_nd_path, &pt_node))
	{
		DBG_LOG_ERR("fail to get node total-block");
		tpret = ST_ERR;
		goto EXIT;
	}
	tp_total_blk = *((uint32_t*)(pt_node->nd_data.dtinfo.dtbuf));
	// to get current block index
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = SKFChina_App_AppMessage_image_block_dissem_tag;
	kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_header_tag;
	kp_nd_path.path[2] = SKFChina_Froto_FrotoHeader_current_block_tag;
	kp_nd_path.kpdepth = 3;
	if(false == app_froto_get_node( pt_root, kp_nd_path, &pt_node))
	{
		DBG_LOG_ERR("fail to get node current-blk idx");
		tpret = ST_ERR;
		goto EXIT;
	}
	tp_cur_blk_idx = *((uint32_t*)(pt_node->nd_data.dtinfo.dtbuf));
	//to get the ID info
	do
	{
		memset( &kp_nd_path, 0, sizeof(struct node_path));
		kp_nd_path.path[0] = SKFChina_App_AppMessage_image_block_dissem_tag;
		kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_file_task_id_tag;
		kp_nd_path.kpdepth = 2;
		if(true == app_froto_get_node( pt_root, kp_nd_path, &pt_node))
		{	
			tp_tskid = *((uint32_t*)(pt_node->nd_data.dtinfo.dtbuf));
			break;
		}
		memset( &kp_nd_path, 0, sizeof(struct node_path));
		kp_nd_path.path[0] = SKFChina_App_AppMessage_image_block_dissem_tag;
		kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_fuota_task_id_tag;
		kp_nd_path.kpdepth = 2;
		if(true == app_froto_get_node( pt_root, kp_nd_path, &pt_node))
		{	
			tp_tskid = *((uint32_t*)(pt_node->nd_data.dtinfo.dtbuf));
			break;
		}
		DBG_LOG_ERR("fail to get the file-trans task-id");
		goto EXIT;
	}while(0);
	
	DBG_LOG_INFO("file-recv , tskid=%d, total_blk = %d, cur_blk = %d" , tp_tskid, tp_total_blk, tp_cur_blk_idx);	

	// to get the image-content node
	memset(&kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = SKFChina_App_AppMessage_image_block_dissem_tag;
	kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_image_content_tag;
	kp_nd_path.kpdepth = 2;
	if(false == app_froto_get_node( pt_root, kp_nd_path, &pt_node))
	{
		DBG_LOG_ERR("fail to get the image content node");
		tpret = ST_ERR;
		goto EXIT;
	}

	/**/
	DBG_LOG_INFO( "img_blk_dissem  tskid = %d, total_blk = %d, cur_blk = %d", tp_tskid, tp_total_blk, tp_cur_blk_idx);


	// to get the message-seq-no
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = SKFChina_App_AppMessage_image_block_dissem_tag;
	kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_header_tag;
	kp_nd_path.path[2] = SKFChina_Froto_FrotoHeader_message_seq_no_tag;
	kp_nd_path.kpdepth = 3;
	if(false == app_froto_get_node( pt_root, kp_nd_path, &pt_node))
	{
		DBG_LOG_ERR("fail to find the message-seq-no");
		tpret = ST_ERR;
		goto EXIT;
	}
	tp_msg_seq_no = *((uint32_t*)(pt_node->nd_data.dtinfo.dtbuf));
	tp_msg_seq_no ++;
	DBG_LOG_INFO("the message-seq-no is %d", tp_msg_seq_no);

	// to find the image-content
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = SKFChina_App_AppMessage_image_block_dissem_tag;
	kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_image_content_tag;
	kp_nd_path.kpdepth = 2;
	if(false == app_froto_get_node( pt_root, kp_nd_path, &pt_node))
	{
		DBG_LOG_ERR("fail to find the image content");
		tpret = ST_ERR;
		goto EXIT;
	}

	// to find the file-trans-controller block
	pt_ftcr = app_froto_get_file_recv_ctr();
	
	pthread_mutex_lock( &pt_ftcr->mtx);//______________________

	pt_ftcb = l_queue_find( pt_ftcr->pt_queue_ftcb, to_compare_task_id, (const void *)tp_tskid);
	if(NULL != pt_ftcb)
	{
		DBG_LOG_DEBUG("to write data to file here , fd %d, size to write %d", pt_ftcb->fd, pt_node->nd_data.dtinfo.size);
		if(write( pt_ftcb->fd, pt_node->nd_data.dtinfo.dtbuf, pt_node->nd_data.dtinfo.size) < 0)
		{
			DBG_LOG_ERR("fail to write data to file %s", pt_ftcb->des_fpath);			
		}
		if(tp_total_blk <= (tp_cur_blk_idx + 1))
		{ // the file receivation is done
			DBG_LOG_INFO("the file receivation is done, clean all the stuff that is not needed any more");
			close(pt_ftcb->fd);

			if(FTYPE_CONF == pt_ftcb->ftype)
			{// when the file received is conf-file, we set flag 			
				is_blk_dissem_done = true;
			}
			
			pt_ftcb = NULL;
			pt_ftcb = l_queue_remove_if( pt_ftcr->pt_queue_ftcb, to_compare_task_id, (const void *)tp_tskid);
			if(NULL != pt_ftcb)
			{
				l_free(pt_ftcb);
			}
			else
			{
				DBG_LOG_INFO("fail to remove the FTCB with ID %d", tp_tskid);
			}
		}
	}
	else
	{
		DBG_LOG_ERR("fail to find the file-trans block with taskid=%d", tp_tskid);
	}

	pthread_mutex_unlock( &pt_ftcr->mtx); //____________________________


	// give the response
	image_block_rquest_give_feedback(tp_tskid, tp_msg_seq_no);

	if(is_blk_dissem_done)
	{// to update the gw-conf to DB
		#if(0)
		 	app_db_clear_table( app_db_get_dbop_controller(), GW_PRO_TABLE_IDX_GW_CONFIG_TABLE);
			 app_db_clear_table( app_db_get_dbop_controller(), GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE);
		#else 
			#warning " DO NOT clear the table here, sometimes the gwConfig.json we get from APP is incomplete. Well , there is a reason!"
		#endif
		util_dbg_copy_file( FPATH_CONF, FPATH_CONF_FROM_APP); // just for debug
		app_db_deliver_gwconf_and_dev_to_db_from_json( app_db_get_dbop_controller(), FPATH_CONF);	 
	}
	
	EXIT:
		l_queue_destroy( pt_node_queue, NULL);
		return tpret;
}


/*to handle incomming msg config-retrieve
pt_tskid ~ buffer where the ID for file transporation will be returned
ret@ST_OK
*/
static app_state_t msg_handler_for_config_retrieve(const struct msg_tree_node*pt_root, uint32_t *pt_tskid)
{
	app_state_t tpret = ST_OK;	

	struct file_trans_control_block*pt_ftcb = NULL;

	struct ipc_msg_v2 kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {.is_valid = false, 0};

	uint32_t tp_retrieve_payload =  SKFChina_ConfigurationAndCommand_RetrievePayload_UNKNOWN_RETRIEVE;
	struct msg_tree_node *pt_node = {0};
	struct node_path kp_nd_path = {0};
	uint32_t kp_tskid = 0;

	#if(0)
	// going to check the config-retrieve message and create a file-trans controlling block to take care of all the file sending operation
	// to get the retrieve payload
	kp_nd_path.path[0] = SKFChina_App_AppMessage_config_retrieve_tag;
	kp_nd_path.path[1] = SKFChina_ConfigurationAndCommand_ConfigRetrieve_payload_tag;
	kp_nd_path.kpdepth = 2;
	if(false == app_froto_get_node( &pt_root,  kp_nd_path, &pt_node))
	{
		DBG_LOG_ERR("fail to find the retrieving payload");
		goto EXIT;
	}
	tp_retrieve_payload = *((uint32_t*)(pt_node->nd_data.dtinfo.dtbuf));
	#else // for debug only
		tp_retrieve_payload = SKFChina_ConfigurationAndCommand_RetrievePayload_GATEWAY_CONFIG_FILE_CONFIG;
	#endif


	if(SKFChina_ConfigurationAndCommand_RetrievePayload_GATEWAY_CONFIG_FILE_CONFIG != tp_retrieve_payload)
	{
		DBG_LOG_ERR("unexpected retrieving payload %d", tp_retrieve_payload);
		tpret = ST_ERR;
		goto EXIT;	
	}

	// to write gateway-conf to file
	//if(ST_OK != app_db_write_gw_conf_to_file( app_db_get_dbop_controller(), FPATH_CONF))	
	//if(ST_OK != app_db_write_gw_conf_to_file( app_db_get_dbop_controller(), FPATH_CONF_FROM_DB))				
	if(ST_OK != app_db_fetch_gwconf_and_dev_from_db_to_file( app_db_get_dbop_controller(), FPATH_CONF))	
	{
		DBG_LOG_ERR("fail to write gw-conf to file");
		tpret = ST_ERR;
		goto EXIT;
	}

	util_dbg_copy_file( FPATH_CONF, FPATH_CONF_FROM_DB); // only for debug

	// now to create a file-trans controlling block
	kp_tskid = app_froto_allocate_file_send_id(app_froto_get_file_send_ctr());
	#if(0) // for debug
		kp_tskid = 555;
	#else	
		DBG_LOG_INFO("the tsk-ID for file transportation is %d", kp_tskid);
	#endif
	if(ST_OK != app_froto_create_file_trans_task( app_froto_get_file_send_ctr(), 5, kp_tskid, FTYPE_CONF, FOP_SEND))
	{
		DBG_LOG_ERR("fail to create file-trans block");
		goto EXIT;	
	}
	// return the tsk-ID
	*pt_tskid = kp_tskid;

	//to find the FTCB
	pt_ftcb = l_queue_find( app_froto_get_file_send_ctr()->pt_queue_ftcb, to_compare_task_id, (const void *)kp_tskid);
	if(NULL == pt_ftcb)
	{
		DBG_LOG_ERR("not suppose to happen, fail to find FTCB with ID %d", kp_tskid);
		tpret = ST_ERR;
		goto EXIT;
	}

	// to give the feed back 
	kp_encoded_msg = util_froto_fill_msg_up_file_notify_upload(kp_tskid, pt_ftcb->total_blks);
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded msg");
		tpret = ST_ERR;
		goto EXIT;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf,  kp_ipc_msg.mtext.proto_pkt.len);

	// to release the encoded msg pkt
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	// to put the message on the waiting queue
	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(kp_ipc_msg)))
	{
		DBG_LOG_ERR("fail to put message on the waiting queue");
		tpret = ST_ERR;
		goto EXIT;
	}

	EXIT:
		return tpret;
}

/**
 * @brief To pick the submessage of AppMessage (e.g., data_upload or
 *        data_selection). Therefore, the input @p pt_root should be
 *        pointed to the root.
 * @param pt_root A pointer to the received proto message (which has been
 *                transformed to @p msg_tree_node )
 * @return A tag of "SKFChina_App_AppMessage" (e.g.,
 *         SKFChina_App_AppMessage_appVer_tag) when no error happens;
 *         otherwise, return -1
 */
static int32_t
pick_froto_submsg_tag(const struct msg_tree_node *pt_root)
{
  int32_t tpret = -1;
  struct msg_tree_node *pt_node = NULL;

  if(NULL == pt_root)
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = -1;
    return tpret;
  }
  if(pt_root->role != ND_ROOT)
  {
    DBG_LOG_ERR("A root node is expected here");
    tpret = -1;
    return tpret;
  }

  // To pick the submessage of AppMessage
  // (e.g., data_upload or data_selection)
  pt_node = pt_root->nd_data.branch;
  while(pt_node)
  {
    if(pt_node->role == ND_ROOT)
    {
      tpret = pt_node->tag;
      break;
    }
    pt_node = pt_node->next;
  }

  return tpret;
}



protobuf_encode_general_bytes_t g_dbg_file_content = {0}; // for debug only

/*kp_tskid ~ valid only where try to send the first block packet
*/
static app_state_t msg_handler_for_file_retrieve(const struct msg_tree_node*pt_root, const uint32_t kp_tskid)
{
	app_state_t tpret = ST_OK;

	struct node_path kp_nd_path = {0};
	struct msg_tree_node *pt_node = {0};
	int32_t kp_msg_tag = -1;
	uint32_t tp_tskid = 0;
	uint32_t tp_request_size = 0;
	struct file_trans_control_block *pt_ftcb = NULL;
	struct file_trans_controller *pt_ftcr = NULL;

	struct ipc_msg_v2 kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {.is_valid = false, 0};

	if((NULL == pt_root)||(pt_root->role != ND_ROOT))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		goto EXIT;
	}

	kp_msg_tag = pick_froto_submsg_tag(pt_root);
	if(SKFChina_App_AppMessage_config_retrieve_tag == kp_msg_tag)
	{
		tp_tskid = kp_tskid;
	}
	else if( SKFChina_App_AppMessage_image_block_retrieve_tag == kp_msg_tag)
	{
		// to pick the tsk-ID
		do{
			kp_nd_path.path[0] = SKFChina_App_AppMessage_image_block_retrieve_tag;
			kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_file_task_id_tag;
			kp_nd_path.kpdepth = 2;
			if(false == app_froto_get_node( pt_root,  kp_nd_path, &pt_node))
			{
				DBG_LOG_INFO("found the node ");
				break;
			}
			memset( &kp_nd_path, 0, sizeof(struct node_path));
			kp_nd_path.path[0] = SKFChina_App_AppMessage_image_block_retrieve_tag;
			kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_fuota_task_id_tag;
			kp_nd_path.kpdepth = 2;
			if(false == app_froto_get_node( pt_root,  kp_nd_path, &pt_node))
			{
				DBG_LOG_INFO("found the node ");
				break;
			}
			DBG_LOG_ERR("fail to found the node with the tsk-ID info");
			goto EXIT;
		}while(0);
		tp_tskid = *((uint32_t*)(pt_node->nd_data.dtinfo.dtbuf));

		// to check if the last packet is sent successfully
		memset( &kp_nd_path, 0, sizeof(struct node_path));
		kp_nd_path.path[0] = SKFChina_App_AppMessage_image_block_retrieve_tag;
		kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_size_tag;
		kp_nd_path.kpdepth = 2;
		if(false == app_froto_get_node( pt_root,  kp_nd_path, &pt_node))
		{
			DBG_LOG_INFO("fail to find the node size");
			//goto EXIT;
		}
		tp_request_size = *((uint32_t*)(pt_node->nd_data.dtinfo.dtbuf)); // we need to try to send the last pkt for one more time, when the request-size is not zero
	}
	else
	{
		DBG_LOG_ERR("unexpected message");
		tpret = ST_ERR;
		goto EXIT;
	}

	#if(0) // for debug
		tp_tskid = kp_tskid;
	#endif

	// to find the controlling block with the given ID
	pt_ftcr = app_froto_get_file_send_ctr();
	pt_ftcb = l_queue_find( pt_ftcr->pt_queue_ftcb, to_compare_task_id, tp_tskid);
	if(NULL == pt_ftcb)
	{
		DBG_LOG_ERR("fail to find the FTCB with ID %d", tp_tskid);
		tpret = ST_ERR;
		goto EXIT;
	}
	//... continue to send the data
	DBG_LOG_INFO("the tsk-ID =%d, the request_size = %d", tp_tskid, tp_request_size);
	if(g_dbg_file_content.buffer)
	{
		l_free( g_dbg_file_content.buffer);
		g_dbg_file_content.buffer = NULL;
	}
	g_dbg_file_content.buffer = (uint8_t*)l_malloc(FILE_PKT_SIZE);
	memset(g_dbg_file_content.buffer, 0, FILE_PKT_SIZE);
	g_dbg_file_content.size = read( pt_ftcb->fd, g_dbg_file_content.buffer, FILE_PKT_SIZE);
	DBG_LOG_DEBUG(" %d bytes is read from file %s", g_dbg_file_content.size, pt_ftcb->des_fpath);

	if(0 == g_dbg_file_content.size)
	{
		// DBG_LOG_WARN("end of file");
		//tpret = ST_ERR;
		//goto EXIT;

		DBG_LOG_INFO("Going to release th FTCB , tsk-ID %d", tp_tskid);
		pt_ftcb = l_queue_remove_if( pt_ftcr->pt_queue_ftcb, to_compare_task_id, (const void *)tp_tskid);
		close(pt_ftcb->fd);
		l_free(pt_ftcb);
		
		goto EXIT;
	}
	#warning "TODO===remember to remove the FTCB"
	//_________________________________________	
	DBG_LOG_INFO("total block %d, current block %d", pt_ftcb->total_blks, pt_ftcb->blk_idx);
	
	kp_encoded_msg = util_froto_fill_msg_up_image_block_upload(tp_tskid, pt_ftcb->total_blks, pt_ftcb->blk_idx++);
	if(false == kp_encoded_msg.is_valid)	
	{
		DBG_LOG_ERR("fail to get the encoded message");
		tpret = ST_ERR;
		goto EXIT;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf,  kp_ipc_msg.mtext.proto_pkt.len);

	// to release the encoded msg
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);


	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(kp_ipc_msg)))
	{
		DBG_LOG_ERR("fail to put msg on the waiting-queue");
		tpret = ST_ERR;
		goto EXIT;
	}

	EXIT:
		return tpret;
}


/*handler for msg version-retrieve
ret@ST_OK
*/
static app_state_t msg_handler_for_version_retrieve(const struct msg_tree_node *pt_root)
{
	app_state_t tpret = ST_OK;

	struct ipc_msg_v2 kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {.is_valid = false, 0};


	/* **you probably have sth to do here*** */


	// now we just send back the verions info
	kp_encoded_msg = util_froto_fill_msg_up_current_version_upload();
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded msg");
		tpret = ST_ERR;
		return tpret;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to release the encoded message
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	// to put message on the queue
	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(kp_ipc_msg)))			
	{
		DBG_LOG_ERR("fail to put message on the queue");
		tpret = ST_ERR;
	}
	
	return tpret;
}


/* fuota-notify-dissem
*/
static app_state_t fuota_notify_dissem(const struct msg_tree_node *pt_root, struct longdata_xfer_ctr *pt_img_dissem_ctr)
{
	app_state_t tpret = ST_OK;

	struct ipc_msg_v2 kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {.is_valid = false, 0};


	if((NULL == pt_root)||(pt_root->role != ND_ROOT))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		return tpret;
	}
	
	kp_encoded_msg = util_froto_fill_msg_fuota_notify_dissem( app_pro_gen_get_gateway_idstr(app_pro_gen_get_ctr()), app_pro_gen_allocate_msg_seq_no(), pt_img_dissem_ctr);
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded msg");
		tpret = ST_ERR;
		return tpret;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to release the encoded msg pkt
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(kp_ipc_msg)))
	{
		DBG_LOG_ERR("fail to put msg on the waiting queue");
		tpret = ST_ERR;
	}
	return tpret;
}


/* fuota-notify-dissem
*/
static app_state_t fuota_notify_dissem_v2(struct longdata_xfer_ctr *pt_img_dissem_ctr)
{
	app_state_t tpret = ST_OK;

	struct ipc_msg_v2 kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {.is_valid = false, 0};
	
	kp_encoded_msg = util_froto_fill_msg_fuota_notify_dissem( app_pro_gen_get_gateway_idstr(app_pro_gen_get_ctr()), app_pro_gen_allocate_msg_seq_no(), pt_img_dissem_ctr);
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded msg");
		tpret = ST_ERR;
		return tpret;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to release the encoded msg pkt
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(kp_ipc_msg)))
	{
		DBG_LOG_ERR("fail to put msg on the waiting queue");
		tpret = ST_ERR;
	}
	return tpret;
}



/*image block dissem, to be called to disseminate the first image packet
*/
static app_state_t image_block_dissem(const struct msg_tree_node*pt_root, struct longdata_xfer_ctr *pt_img_dissem_ctr)
{
	app_state_t tpret = ST_OK;

	struct ipc_msg_v2 kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {.is_valid = false, 0};
	uint32_t kp_msg_seq_no = app_pro_gen_allocate_msg_seq_no();

	if((NULL == pt_root)||(ND_ROOT != pt_root->role))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		return tpret;
	}

	// to check if the controller is still valid
	pthread_mutex_lock( &pt_img_dissem_ctr->mtx);
	if(false == pt_img_dissem_ctr->is_in_process)
	{
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);			
		DBG_LOG_ERR("the img dissemination controller is in active");
		tpret = ST_ERR;
		return tpret;
	}
	
	// to set the msg seq-no
	pt_img_dissem_ctr->last_seq_no = kp_msg_seq_no;
	pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);

	kp_encoded_msg = util_froto_fill_msg_img_block_dissem( pt_img_dissem_ctr, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), kp_msg_seq_no);
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded msg");
		tpret = ST_ERR;
		return tpret;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to release the msg
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(kp_ipc_msg)))
	{
		DBG_LOG_ERR("fail to put msg on the queue");
		tpret = ST_ERR;
	}

	
	
	return tpret;
}

/*in the process of image dissemination , we need to update the time-value for waiting object
*/
app_state_t app_froto_update_image_dissem_waiting_obj(struct sensor_op_controller*pt_sensor_op_ctr, time_t timval)
{
	app_state_t tpret = ST_OK;

	struct l_queue_entry *pt_entry = NULL;
	struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;
	if(NULL == pt_sensor_op_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	pthread_mutex_lock( &pt_sensor_op_ctr->mtx);
	pt_entry = l_queue_get_entries(pt_sensor_op_ctr->pt_sensor_op_waiting_queue);
	
	//DBG_LOG_WARN(" to check and update waiting obj entry@0x%x", pt_entry);
	while(pt_entry)
	{
		pt_sensor_op_waiting_obj = (struct sensor_op_waiting*)(pt_entry->data);
		
		//DBG_LOG_WARN("BKP884 op_code %d", pt_sensor_op_waiting_obj->op_code);			

		pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx);
		if(SENSOR_OP_IMG_DISSEM == pt_sensor_op_waiting_obj->op_code)
		{// only for image dissemination
			DBG_LOG_WARN(" to update timval for waiting obj for image dissemination");
			pt_sensor_op_waiting_obj->timval = timval;
		}
		pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);
	
		pt_entry = pt_entry->next;
	}
	pthread_mutex_unlock( &pt_sensor_op_ctr->mtx);
}


/*image block dissem, to be called to disseminate the first image packet
*/
static app_state_t image_block_dissem_v2(struct longdata_xfer_ctr *pt_img_dissem_ctr)
{
	app_state_t tpret = ST_OK;

	struct ipc_msg_v2 kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {.is_valid = false, 0};
	uint32_t kp_msg_seq_no = app_pro_gen_allocate_msg_seq_no();

	// to check if the controller is still valid
	pthread_mutex_lock( &pt_img_dissem_ctr->mtx);
	if(false == pt_img_dissem_ctr->is_in_process)
	{
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);			
		DBG_LOG_ERR("the img dissemination controller is in active");
		tpret = ST_ERR;
		return tpret;
	}
	
	// to set the msg seq-no
	pt_img_dissem_ctr->last_seq_no = kp_msg_seq_no;
	pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);

	kp_encoded_msg = util_froto_fill_msg_img_block_dissem( pt_img_dissem_ctr, app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), kp_msg_seq_no);
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded msg");
		tpret = ST_ERR;
		return tpret;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to release the msg
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(kp_ipc_msg)))
	{
		DBG_LOG_ERR("fail to put msg on the queue");
		tpret = ST_ERR;
	}

	// to update the time-value for image-dissemination waiting object
	app_froto_update_image_dissem_waiting_obj( app_sch_get_sensor_op_controller(), time(NULL));
	
	return tpret;
}




static app_state_t msg_handler_for_just_sending_image(const struct msg_tree_node*pt_root, struct longdata_xfer_ctr*pt_img_dissem_ctr);

/* to disseminate the first 
case SKFChina_App_AppMessage_current_version_upload_tag:// 10
*/
static app_state_t msg_handler_for_version_upload(const struct msg_tree_node*pt_root, struct longdata_xfer_ctr*pt_img_dissem_ctr)
{
	app_state_t tpret = ST_OK;
	uint32_t kp_fw_ver = 0;

	#warning "================TO compare the version info" 

	pthread_mutex_lock( &pt_img_dissem_ctr->mtx);
	if(false == pt_img_dissem_ctr->is_in_process)
	{		
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
		DBG_LOG_ERR("the controller is inactive");
		tpret = ST_ERR;
		goto EXIT;
	}
	// to load the fist image packet
	if(pt_img_dissem_ctr->fd < 0)
	{		
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
		tpret = ST_ERR;	
		DBG_LOG_ERR("img file %s is probably not opened successfully", pt_img_dissem_ctr->img_fpath);
		goto EXIT;
	}
	pt_img_dissem_ctr->dtlen = read( pt_img_dissem_ctr->fd,pt_img_dissem_ctr->pkt_buf, FILE_PKT_SIZE);
	if(pt_img_dissem_ctr->dtlen < 0)
	{	
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
		
		DBG_LOG_ERR("fail to read img file, fd %d", pt_img_dissem_ctr->fd);
		tpret = ST_ERR;
		goto EXIT;
	}
	DBG_LOG_INFO(" %d bytes is read from fd %d", pt_img_dissem_ctr->dtlen, pt_img_dissem_ctr->fd);
	pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);

	if(ST_OK == fuota_notify_dissem( pt_root, app_froto_get_longdata_xfer_timer_ctr()))
	{
		image_block_dissem( pt_root, app_froto_get_longdata_xfer_timer_ctr());
		#if(1) // we just send the rest of the image
			msg_handler_for_just_sending_image( pt_root, app_froto_get_longdata_xfer_timer_ctr());
		#endif
	}
	else
	{
		DBG_LOG_ERR("fail to notify FUOTA dissemination");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	EXIT:
		return tpret;
}

/*just send the rest of the image
*/

static app_state_t msg_handler_for_just_sending_image(const struct msg_tree_node*pt_root, struct longdata_xfer_ctr*pt_img_dissem_ctr)
{
	app_state_t tpret = ST_OK;	
	struct itimerspec kp_its = {0};
	while(1)
	{
		pthread_mutex_lock( &pt_img_dissem_ctr->mtx);
		if(false == pt_img_dissem_ctr->is_in_process)
		{		
			pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
			DBG_LOG_ERR("the ctr is inactive");
			tpret = ST_ERR;
			goto EXIT;
		}

		if(pt_img_dissem_ctr->total_blk <= pt_img_dissem_ctr->cur_blk_idx)
		{//all pkt is sent, to release the ctr
			DBG_LOG_INFO("all pkt is sent, to release the controller");
			pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);						
			goto EXIT;
		}
		
		if(pt_img_dissem_ctr->fd < 0)
		{		
			pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
			DBG_LOG_ERR("invalid fd");
			tpret = ST_ERR;
			goto EXIT;
		}
		memset( pt_img_dissem_ctr->pkt_buf, 0, FILE_PKT_SIZE);
		pt_img_dissem_ctr->dtlen = read( pt_img_dissem_ctr->fd, pt_img_dissem_ctr->pkt_buf, FILE_PKT_SIZE);
		if(pt_img_dissem_ctr->dtlen < 0)
		{		
			pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
			DBG_LOG_ERR("read fail for %d", strerror(errno));
			tpret = ST_ERR;
			goto EXIT;
		}
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
		if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
		if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
		{
			DBG_LOG_INFO("no valid BT connection ");
			tpret = ST_ERR;
			goto EXIT;
		}


		if(ST_OK != image_block_dissem( pt_root, pt_img_dissem_ctr))
		{
			DBG_LOG_ERR("fail to disseminate image block");
			tpret = ST_ERR;
			goto EXIT;
		}
		usleep(1*1000);
	}
	EXIT:
		pthread_mutex_lock( &pt_img_dissem_ctr->mtx);
			pt_img_dissem_ctr->is_in_process = false;
			pt_img_dissem_ctr->img_ver = 0;
			if(pt_img_dissem_ctr->fd >= 0)
			{
				close(pt_img_dissem_ctr->fd);
				pt_img_dissem_ctr->fd = -1;
			}
			memset( pt_img_dissem_ctr->img_fpath, 0, MAX_FPATH);
			pt_img_dissem_ctr->fsize = 0;
			pt_img_dissem_ctr->total_blk = 0;
			pt_img_dissem_ctr->dtlen = 0;
			memset( pt_img_dissem_ctr->pkt_buf, 0, FILE_PKT_SIZE);
			pt_img_dissem_ctr->retry_cnt = 0;
			pt_img_dissem_ctr->last_seq_no = 0;
			pt_img_dissem_ctr->cur_blk_idx = 0;
			pt_img_dissem_ctr->f_tsk_id = 0;
			//disarm the timer
			timer_settime(pt_img_dissem_ctr->timer_id, 0, &kp_its, NULL);
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);			
		return tpret;
}

static app_state_t msg_handler_for_just_sending_file(
  struct longdata_xfer_ctr *pt_file_dissem_ctr);

void
app_froto_set_force_fuota(struct longdata_xfer_ctr *pt_img_dissem_ctr)
{
  if(NULL == pt_img_dissem_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }
  pt_img_dissem_ctr->force_fuota = true;
}
void
app_clear_force_fuota(
  struct longdata_xfer_ctr *pt_img_dissem_ctr)
{
  if(NULL == pt_img_dissem_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    return;
  }
  pt_img_dissem_ctr->force_fuota = false;
}
/**
 * @brief Send the file (e.g., the FUOTA image)
 * @param pt_file_dissem_ctr : a pointer to the file disseminate controller
 * @return ST_OK if disseminate the file successfully
 */
static app_state_t
msg_handler_for_just_sending_file(
  struct longdata_xfer_ctr *pt_file_dissem_ctr)
{
  app_state_t tpret = ST_OK;

#if 0  // The timer is not used actually
  struct itimerspec kp_its = { 0 };
#endif
  uint32_t timeout_s = 0;

  // To disable the timeout first because here gateway needs to send FUOTA
  // image to the sensor and no application ack would be received from the
  // sensor.
#if (SKF_GW_NEW != 1)
  app_tim_set_ble_reading_timeout_fg(false);
#endif
  while(1)
  {
    // If BLE role is not client (e.g., the gateway is connecting to a
    // mobile phone as a BLE server), then exit
    if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
         app_pro_gen_get_ctr()))
    {
      tpret = ST_ERR;

      // Recover the timeout timer
      // TODO: to optimize the code structure in the future
      app_froto_update_image_dissem_waiting_obj(
        app_sch_get_sensor_op_controller(), time(NULL));
#if (SKF_GW_NEW != 1)
      app_tim_set_ble_reading_timeout_fg(true);
#endif

      goto EXIT;
    }
    // If BLE is not being connected, then exit
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
	if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
    {
      tpret = ST_ERR;

      // Recover the timeout timer
      // TODO: to optimize the code structure in the future
      app_froto_update_image_dissem_waiting_obj(
        app_sch_get_sensor_op_controller(), time(NULL));
#if (SKF_GW_NEW != 1)
      app_tim_set_ble_reading_timeout_fg(true);
#endif

      goto EXIT;
    }
    // If the dissemination controller is not in_process, then exit
    pthread_mutex_lock(&pt_file_dissem_ctr->mtx);
    if(false == pt_file_dissem_ctr->is_in_process)
    {
      pthread_mutex_unlock(&pt_file_dissem_ctr->mtx);

      DBG_LOG_ERR("The dissemination controller is inactive");
      tpret = ST_ERR;

      // Recover the timeout timer
      // TODO: to optimize the code structure in the future
      app_froto_update_image_dissem_waiting_obj(
        app_sch_get_sensor_op_controller(), time(NULL));
#if (SKF_GW_NEW != 1)
      app_tim_set_ble_reading_timeout_fg(true);
#endif

      goto EXIT;
    }
    // If the dissemination current block is equal to the total block,
    // then exit
    if(pt_file_dissem_ctr->total_blk <= pt_file_dissem_ctr->cur_blk_idx)
    {
      // All pkt is sent, to release the ctr
      pthread_mutex_unlock(&pt_file_dissem_ctr->mtx);
      DBG_LOG_INFO("All the blocks have been disseminated");

      // Recover the timeout timer (wait for the FUOTA completed, at most
      // 240 s)
      // TODO: to optimize the code structure in the future
      app_froto_update_image_dissem_waiting_obj(
        app_sch_get_sensor_op_controller(), time(NULL) + 240);
#if (SKF_GW_NEW != 1)
      app_tim_set_ble_reading_timeout_fg(true);
#endif

      goto EXIT;
    }

    // Read the file to pt_file_dissem_ctr->pkt_buf
    if(pt_file_dissem_ctr->fd < 0)
    {
      pthread_mutex_unlock(&pt_file_dissem_ctr->mtx);
      DBG_LOG_ERR("Invalid fd of the file");
      tpret = ST_ERR;

      // Recover the timeout timer
      // TODO: to optimize the code structure in the future
      app_froto_update_image_dissem_waiting_obj(
        app_sch_get_sensor_op_controller(), time(NULL));
#if (SKF_GW_NEW != 1)
      app_tim_set_ble_reading_timeout_fg(true);
#endif

      goto EXIT;
    }
    memset(pt_file_dissem_ctr->pkt_buf, 0, FILE_PKT_SIZE);
    pt_file_dissem_ctr->dtlen = read(pt_file_dissem_ctr->fd,
                                     pt_file_dissem_ctr->pkt_buf,
                                     FILE_PKT_SIZE);
    if(pt_file_dissem_ctr->dtlen < 0)
    {
      pthread_mutex_unlock(&pt_file_dissem_ctr->mtx);
      DBG_LOG_ERR("Failed to read, for %d", strerror(errno));
      tpret = ST_ERR;

      // Recover the timeout timer
      // TODO: to optimize the code structure in the future
      app_froto_update_image_dissem_waiting_obj(
        app_sch_get_sensor_op_controller(), time(NULL));
#if (SKF_GW_NEW != 1)
      app_tim_set_ble_reading_timeout_fg(true);
#endif

      goto EXIT;
    }
    pthread_mutex_unlock(&pt_file_dissem_ctr->mtx);

    if(ST_OK != image_block_dissem_v2(pt_file_dissem_ctr))
    {
      DBG_LOG_ERR("Failed to disseminate block");
      tpret = ST_ERR;

      // Recover the timeout timer
      // TODO: to optimize the code structure in the future
      app_froto_update_image_dissem_waiting_obj(
        app_sch_get_sensor_op_controller(), time(NULL));
#if (SKF_GW_NEW != 1)
      app_tim_set_ble_reading_timeout_fg(true);
#endif

      goto EXIT;
    }
  }
EXIT:
  pthread_mutex_lock(&pt_file_dissem_ctr->mtx);
  // Close the file
  if(pt_file_dissem_ctr->fd >= 0)
  {
    close(pt_file_dissem_ctr->fd);
    pt_file_dissem_ctr->fd = -1;
  }
  // Clear the variables and flags
  memset(pt_file_dissem_ctr->img_fpath, 0, MAX_FPATH);
  pt_file_dissem_ctr->fsize = 0;
  pt_file_dissem_ctr->total_blk = 0;
  pt_file_dissem_ctr->cur_blk_idx = 0;
  pt_file_dissem_ctr->last_seq_no = 0;
  pt_file_dissem_ctr->f_tsk_id = 0;
  pt_file_dissem_ctr->img_ver = 0;
  pt_file_dissem_ctr->dtlen = 0;
  memset(pt_file_dissem_ctr->pkt_buf, 0, FILE_PKT_SIZE);
  pt_file_dissem_ctr->retry_cnt = 0;
  pt_file_dissem_ctr->is_in_process = false;

  // Disarm the timer
#if 0  // The timer is not used actually
  timer_settime(pt_file_dissem_ctr->timer_id, 0, &kp_its, NULL);
#endif
  pthread_mutex_unlock(&pt_file_dissem_ctr->mtx);
  return tpret;
}
/**
 * @brief To disseminate the FUOTA image in a block manner. The function
 *        would not return till the FUOTA image dissemination has been
 *        completed or an error happens.
 * @param pt_file_dissem_ctr : a pointer to the file disseminate controller
 * @param pt_sensor_op_ctr : a pointer to the sensor operation controller
 *                          (including an operation queue)
 * @param pt_file_fpath : The path of a file to be sent (i.e., the FUOTA
 *                        image)
 * @param kp_verinfo : FUOTA image version
 * @return ST_OK if disseminate the file successfully
 */
app_state_t
app_froto_img_dissemination_with_block(
  struct longdata_xfer_ctr *pt_file_dissem_ctr,
  struct sensor_op_controller *pt_sensor_op_ctr,
  const uint8_t *pt_file_fpath,
  const uint32_t kp_verinfo)
{
  app_state_t tpret = ST_OK;
  int hld_fd = -1;

#if 0  // The timer is not used actually
  struct itimerspec kp_its = { 0 };
#endif
  struct encoded_froto_msg_pkt kp_encoded_msg = { 0 };
  struct ipc_msg_v2 kp_ipc_msg = { 0 };
  uint32_t kp_msg_seq_no = app_pro_gen_allocate_msg_seq_no();

  struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;

  if((NULL == pt_file_dissem_ctr) || (NULL == pt_file_fpath) ||
     (NULL == pt_sensor_op_ctr))
  {
    DBG_LOG_ERR("Unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  hld_fd = open(pt_file_fpath, O_RDONLY);
  if(hld_fd < 0)
  {
    DBG_LOG_ERR("Failed to open file %s, for %s", pt_file_fpath,
                strerror(errno));
    tpret = ST_ERR;
    return tpret;
  }

#if 0  // The timer is not used actually
  kp_its.it_value.tv_sec = 420;
  kp_its.it_value.tv_nsec = 0;
  kp_its.it_interval.tv_sec = 0;
  kp_its.it_interval.tv_nsec = 0;
#endif

  pthread_mutex_lock(&pt_file_dissem_ctr->mtx);
  if(true == pt_file_dissem_ctr->is_in_process)
  {
    pthread_mutex_unlock(&pt_file_dissem_ctr->mtx);

    DBG_LOG_ERR("The file dissemination controller is in process");
    tpret = ST_BUSSY;
    goto ERR;
  }

  // Set up variables and flags
  pt_file_dissem_ctr->fd = hld_fd;
  memset(pt_file_dissem_ctr->img_fpath, 0, MAX_FPATH);
  snprintf(pt_file_dissem_ctr->img_fpath, MAX_FPATH, "%s", pt_file_fpath);
  pt_file_dissem_ctr->fsize = lseek(hld_fd, 0, SEEK_END);
  if(pt_file_dissem_ctr->fsize < 0)
  {
    pthread_mutex_unlock(&pt_file_dissem_ctr->mtx);
    DBG_LOG_ERR("Failed to lseek, for %s", strerror(errno));
    tpret = ST_ERR;
    goto ERR;
  }
  lseek(pt_file_dissem_ctr->fd, 0, SEEK_SET);  // reset the file pointer
  pt_file_dissem_ctr->total_blk = pt_file_dissem_ctr->fsize /
                                  FILE_PKT_SIZE;
  if(pt_file_dissem_ctr->fsize % FILE_PKT_SIZE)
  {
    pt_file_dissem_ctr->total_blk++;
  }
  pt_file_dissem_ctr->cur_blk_idx = 0;
  pt_file_dissem_ctr->last_seq_no = 0;
  pt_file_dissem_ctr->f_tsk_id = kp_msg_seq_no;
  pt_file_dissem_ctr->img_ver = kp_verinfo;
  pt_file_dissem_ctr->retry_cnt = 0;
  pt_file_dissem_ctr->is_in_process = true;  // set the controller to
                                             // active

  pthread_mutex_unlock(&pt_file_dissem_ctr->mtx);

  // If BLE role is not client (e.g., the gateway is connecting to a
  // mobile phone as a BLE server), then exit
  if(BT_ROLE_CLIENT != app_pro_gen_get_bluetooth_role(
       app_pro_gen_get_ctr()))
  {
    tpret = ST_ERR;
    goto ERR;
  }
  // If BLE is not being connected, then exit
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr(), 0))	// idx to be confirmed.
#else
  if(false == app_pro_gen_get_connection_state(app_pro_gen_get_ctr()))
#endif
  {
    tpret = ST_ERR;
    goto ERR;
  }
  // To notify the dissemination of image
  if(ST_OK != fuota_notify_dissem_v2(pt_file_dissem_ctr))
  {
    DBG_LOG_ERR("Failed to notify the dissemination of image");
    tpret = ST_ERR;
    goto ERR;
  }

	#if(1)
		// add a delay, the sensor need sometime to make some preparation for image receivation  
		//usleep(50*1000U); // 50mS
		sleep(2);
	#endif

  // Set up a timeout timer to wait the final ack message when file
  // dissemination completes
  // To create an waiting object
  pt_sensor_op_waiting_obj =
    (struct sensor_op_waiting *)l_malloc(sizeof(struct sensor_op_waiting));
  if(NULL == pt_sensor_op_waiting_obj)
  {
    DBG_LOG_ERR("Failed to malloc");
    tpret = ST_ERR;
    goto ERR;
  }
  memset(pt_sensor_op_waiting_obj, 0, sizeof(struct sensor_op_waiting));
  app_sch_init_sensor_op_waiting_obj(pt_sensor_op_waiting_obj,
                                     kp_msg_seq_no, SENSOR_OP_IMG_DISSEM);
  app_sch_set_timval_to_sensor_op_waiting_obj(pt_sensor_op_waiting_obj, time(
                                                NULL));
  // To put the pt_sensor_op_waiting_obj on waiting queue
  app_sch_put_sensor_op_waiting_obj_on_queue(pt_sensor_op_ctr,
                                             pt_sensor_op_waiting_obj);

  // To disseminate the image (encode the message and send the encoded
  // messages to the BLE process)
  msg_handler_for_just_sending_file(pt_file_dissem_ctr);
#if (1)
  struct timespec tp_tspec = {0};
#endif
  pthread_mutex_lock(&pt_sensor_op_waiting_obj->mtx);
#if (1)
  clock_gettime(CLOCK_REALTIME, &tp_tspec);
  tp_tspec.tv_sec = tp_tspec.tv_sec + CONFIG_SENSOR_OP_IMG_DISSEM_TIMEOUT; // 5 mins for OTA.
#endif

  // To wait for response (a signal sent to @p cond_resp_received)
  while(1)
  {
    if(EV_SENSOR_OP_DEFAULT == pt_sensor_op_waiting_obj->resp_event)
    {
#if (1)
	  int tpint = 0;
	  tpint = pthread_cond_timedwait( &pt_sensor_op_waiting_obj->cond_resp_received, &pt_sensor_op_waiting_obj->mtx, &tp_tspec);
	  if((0 != tpint)&&(ETIMEDOUT == tpint))
      {
		DBG_LOG_ERR("===wait sensor op img dissem timeout");
		break;						
	  }
#else
  	  pthread_cond_wait(&pt_sensor_op_waiting_obj->cond_resp_received,
                        &pt_sensor_op_waiting_obj->mtx);
#endif
    }
    else
    {
      break;
    }
  }
  pthread_mutex_unlock(&pt_sensor_op_waiting_obj->mtx);
  // The final ack received or timeout
  if(EV_SENSOR_OP_RESP_RECV != pt_sensor_op_waiting_obj->resp_event)
  {
    tpret = ST_ERR;
    DBG_LOG_ERR("Failed to disseminate the image");
  }
  else
  {
    if(pt_sensor_op_waiting_obj->sensor_op_resp.fw_dissem_resp.
       is_successful)
    {
      DBG_LOG_INFO("FUOTA image has been disseminated successfully");
    }
    else
    {
      DBG_LOG_ERR("Failed to disseminate the image");
      tpret = ST_ERR;
    }
  }
ERR:
  if(hld_fd >= 0)
  {
    // Close the opened file
    close(hld_fd);
  }
  if(pt_sensor_op_waiting_obj)
  {
    app_sch_deinit_sensor_op_waiting_obj(pt_sensor_op_waiting_obj);
    l_free(pt_sensor_op_waiting_obj);
    pt_sensor_op_waiting_obj = NULL;
  }
  return tpret;
}





/*function to pick firmware version info
pt_sensor_op@ the sensor operation controller
pt_msg_root@ the froto message root
pt_ver_buf @ where the version info is returned

*/
//app_state_t app_froto_pick_sensor_fw_version( struct sensor_op_controller*pt_sensor_op_ctr, const struct msg_tree_node*pt_msg_root, uint32_t *pt_ver_buf)
static app_state_t msg_handler_for_version_upload_v2(  struct sensor_op_controller*pt_sensor_op_ctr,const struct msg_tree_node*pt_msg_root)
{
	app_state_t tpret = ST_OK;

	uint32_t kp_acked_seq_no = 0;
	uint32_t kp_fw_ver = 0;

	struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;

	struct l_queue *pt_nd_queue = NULL;
	struct node_path kp_nd_path = {0};
	struct l_queue_entry *pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL;

	if((NULL == pt_sensor_op_ctr)||(NULL == pt_msg_root))	
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	pt_nd_queue = l_queue_new();
	if(NULL == pt_nd_queue)
	{
		tpret = ST_ERR;
		DBG_LOG_ERR("fail to create queue");
		goto EXIT;
	}

	// to check if the seq-no match
	// fill the node path
	kp_nd_path.path[0] = SKFChina_App_AppMessage_current_version_upload_tag;
	kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_CurrentVersionUpload_header_tag;
	kp_nd_path.path[2] = SKFChina_Froto_FrotoHeader_acked_message_seq_number_tag;
	kp_nd_path.path[3] = SKFChina_Froto_FrotoAckMsg_header_tag;
 	kp_nd_path.kpdepth = 4;
	if(false == util_froto_locate_msg_node_v2( pt_msg_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the acked-seq-no");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queue) != 1)
	{
		DBG_LOG_ERR("unexepcted queue length");
		tpret = ST_ERR;
		goto EXIT;
	}

	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)(pt_entry->data);

	kp_acked_seq_no = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	// to find the FW version info
	l_queue_clear( pt_nd_queue, NULL);
	//fill the path to FW version
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = SKFChina_App_AppMessage_current_version_upload_tag;
	kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_CurrentVersionUpload_firmware_version_tag;
	kp_nd_path.kpdepth = 2;
	if(false == util_froto_locate_msg_node_v2( pt_msg_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the FW version node");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queue) != 1)
	{
		DBG_LOG_ERR("unexpected queue length");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_fw_ver = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	DBG_LOG_INFO("the FW info 0x%x is reported by msg %d", kp_fw_ver, kp_acked_seq_no);

	// pass the FW info to the thread waiting on it
	pthread_mutex_lock(&pt_sensor_op_ctr->mtx);
	pt_sensor_op_waiting_obj = l_queue_remove_if( pt_sensor_op_ctr->pt_sensor_op_waiting_queue, queue_match_for_sensor_waiting, (const void *)kp_acked_seq_no);
	if(pt_sensor_op_waiting_obj)
	{
		DBG_LOG_INFO(" signal to object %d", kp_acked_seq_no);
		pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx);
		
		pt_sensor_op_waiting_obj->sensor_op_resp.ver_info.ver = kp_fw_ver; // set the version info

		pt_sensor_op_waiting_obj->resp_event = EV_SENSOR_OP_RESP_RECV;
		pthread_cond_signal( &pt_sensor_op_waiting_obj->cond_resp_received);
		pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);
	}
	else
	{
		DBG_LOG_WARN(" No object with seq-no %d is waiting on queue", kp_acked_seq_no);
	}
	pthread_mutex_unlock( &pt_sensor_op_ctr->mtx);

	EXIT:
		if(pt_nd_queue)
		{
			l_queue_destroy( pt_nd_queue, NULL);
		}
		return tpret;
}




/*pt_root@ the message tree
pt_resp_buf_array@the array where the value returned by sensor is stored
ret@ST_OK is epxected
*/
app_state_t app_froto_pick_conf_hash_value_and_edit_time( struct msg_tree_node *pt_root, struct sensor_conf_retrieve_resp *pt_resp_buf_array,const  uint32_t kp_arrsz )
{
	app_state_t tpret = ST_OK;
	struct l_queue *pt_nd_queue = NULL;
	struct node_path kp_nd_path = {0};
	struct l_queue_entry *pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL;
	
	if((NULL == pt_root)||(NULL == pt_resp_buf_array)||(0 == kp_arrsz))
	{
		tpret = ST_ERR;
		DBG_LOG_ERR("invalid parameter is given");
		goto EXIT;
	}
	pt_nd_queue = l_queue_new();
	if(NULL == pt_nd_queue)
	{
		DBG_LOG_ERR("fail to create new queue");
		tpret = ST_ERR;
		goto EXIT;
	}

	// fill the node path to hash value
	kp_nd_path.path[0] = SKFChina_App_AppMessage_config_hash_upload_tag;
	kp_nd_path.path[1] = SKFChina_ConfigurationAndCommand_ConfigHashUpload_config_hash_value_tag;
	kp_nd_path.kpdepth = 2;

	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the hash value node");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queue) <= 0)
	{
		DBG_LOG_ERR("no msg node is found");
		tpret = ST_ERR;
		goto EXIT;
	}

	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	pt_resp_buf_array[0].conf_val.hash_and_last_edit_time.hash_val = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	// to pick the last edit time
	l_queue_clear( pt_nd_queue, NULL);

	// fill the path to last-editing time
	kp_nd_path.path[0] = SKFChina_App_AppMessage_config_hash_upload_tag;
	kp_nd_path.path[1] = SKFChina_ConfigurationAndCommand_ConfigHashUpload_last_edit_time_tag;
	kp_nd_path.path[2] = SKFChina_ConfigurationAndCommand_TimeArray_time_tag;
	kp_nd_path.kpdepth = 3;
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the last-editting time");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queue) <= 0)
	{
		DBG_LOG_ERR("no msg node is found");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	pt_resp_buf_array[0].conf_val.hash_and_last_edit_time.last_edit_time = *((uint64_t*)pt_node->nd_data.dtinfo.dtbuf);
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
app_state_t app_froto_pick_sensor_conf_info( struct msg_tree_node *pt_root, struct sensor_conf_retrieve_resp *pt_resp_buf_array,const  uint32_t kp_arrsz )
{
	app_state_t tpret = ST_OK;
	struct l_queue*pt_nd_queue = NULL;
	struct node_path kp_nd_path = {0};
	struct l_queue_entry *pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL, *pt_leavef_nd = NULL;	
	struct msg_tree_node *pt_temp_node = NULL;
	uint32_t kpidx = 0;	
	
	//DBG_LOG_WARN("BKP===to pick conf-info");
	
	if((NULL == pt_root)||(NULL == pt_resp_buf_array)||(0 == kp_arrsz))
	{
		DBG_LOG_ERR("invalid parameter is given");
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
	//DBG_LOG_INFO("BKP==== to locate the conf-pair branch");
		
	// to find the config-pair branch
	kp_nd_path.path[0] = SKFChina_App_AppMessage_specific_config_upload_tag;
	kp_nd_path.path[1] = SKFChina_ConfigurationAndCommand_SpecificConfigUpload_config_pair_tag;
	kp_nd_path.kpdepth = 2;

	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("util_forot_locate_msg_node fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queue) <= 0)
	{
		DBG_LOG_ERR("no msg node is found");
		tpret = ST_ERR;
		goto EXIT;
	}
	//DBG_LOG_INFO("BKP==========to pick sensor conf-info");
	kpidx = 0;
	while(1)
	{
		pt_node = (struct msg_tree_node*)l_queue_pop_head( pt_nd_queue);
		if(NULL == pt_node)
		{
			break;
		}
		if(ND_ROOT != pt_node->role)
		{
			DBG_LOG_WARN("we expect a branch here, double ckeck to make sure things are fine");
			continue;
		}
		pt_leavef_nd = pt_node->nd_data.branch;
		while(pt_leavef_nd)
		{
			switch(pt_leavef_nd->tag)
			{
				case SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag:// 1
				{
					pt_resp_buf_array[kpidx % kp_arrsz].conf_item_id.specific_item_id = *((SKFChina_Common_SpecificConfigItem*)pt_leavef_nd->nd_data.dtinfo.dtbuf);	
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_fscheduler_config_item_tag:// 6
				{					
					pt_resp_buf_array[kpidx % kp_arrsz].conf_item_id.scheduler_item_id = *((SKFChina_Common_fSchedulerConfigItem*)pt_leavef_nd->nd_data.dtinfo.dtbuf);	
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_tag:// 2
				{//
					memcpy(pt_resp_buf_array[kpidx % kp_arrsz].conf_val.general_content, pt_leavef_nd->nd_data.dtinfo.dtbuf, pt_leavef_nd->nd_data.dtinfo.size > MAX_GENERAL_CONF_CONTENT ? MAX_GENERAL_CONF_CONTENT: pt_leavef_nd->nd_data.dtinfo.size);
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag:// 3
				{// TImeArray
					DBG_LOG_ERR("~~~To double check");
					if(ND_ROOT != pt_leavef_nd->role)
					{
						DBG_LOG_ERR("a branch is expected here");
						break;
					}
					pt_temp_node = pt_leavef_nd->nd_data.branch;
					if((NULL == pt_temp_node)||(pt_temp_node->tag != SKFChina_ConfigurationAndCommand_TimeArray_time_tag)||(ND_ROOT != pt_temp_node->role))
					{
						DBG_LOG_ERR("unexpected msg node");
						break;
					}
					pt_temp_node = pt_temp_node->nd_data.branch;
					if((NULL == pt_temp_node)||(SKFChina_ConfigurationAndCommand_TimeArrayElement_time_tag != pt_temp_node->tag)||(ND_INFO != pt_temp_node->role))
					{
						DBG_LOG_ERR("unepxected msg node");
						break;
					}
					pt_resp_buf_array[kpidx % kp_arrsz].conf_val.val_u32 = *((uint32_t*)pt_temp_node->nd_data.dtinfo.dtbuf);
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_measurement_description_tag:// 4
				{//WakeUpToSenseDesc					
					DBG_LOG_ERR("***to complete this when you see this output");
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_bool_config_content_tag:// 5
				{//Bool Array					
					DBG_LOG_WARN("***To-double check");
					
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_uint32_tag:// 7
				{
					pt_resp_buf_array[kpidx % kp_arrsz].conf_val.val_u32 = *((uint32_t*)pt_leavef_nd->nd_data.dtinfo.dtbuf);
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_int32_tag:// 8
				{			
					pt_resp_buf_array[kpidx % kp_arrsz].conf_val.val_i32 = *((int32_t*)pt_leavef_nd->nd_data.dtinfo.dtbuf);
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_float_tag:// 9
				{					
					pt_resp_buf_array[kpidx % kp_arrsz].conf_val.val_float = *((float*)pt_leavef_nd->nd_data.dtinfo.dtbuf);
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_work_mode_content_tag:// 11
				{// workmode pair						
					//NOTE!!! the work mode is a branch
					//pt_resp_buf_array[kpidx % kp_arrsz].conf_val.val_u32 = *((uint32_t*)pt_leavef_nd->nd_data.dtinfo.dtbuf);
					DBG_LOG_WARN("~~~~~~~~~To double check");
					if(ND_ROOT != pt_leavef_nd->role)
					{
						DBG_LOG_ERR("unexpected situation");
						break;
					}
					pt_temp_node = pt_leavef_nd->nd_data.branch;
					while(1)
					{
						if(NULL == pt_temp_node)
						{
							break;
						}
						
						if((ND_INFO == pt_temp_node->role)&&(SKFChina_ConfigurationAndCommand_WorkModePair_work_mode_tag == pt_temp_node->tag))
						{
							pt_resp_buf_array[kpidx % kp_arrsz].conf_val.val_u32 = *((uint32_t*)pt_temp_node->nd_data.dtinfo.dtbuf);
							break;
						}
						pt_temp_node = pt_temp_node->next;
					}
					
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_gateway_info_content_tag:// 12
				{// Gateway info pair					
					DBG_LOG_ERR("~~~To double check");
					if((ND_ROOT !=pt_leavef_nd->role))
					{
						DBG_LOG_ERR("a msg branch is expected here");
						break;
					}
					pt_temp_node = pt_leavef_nd->nd_data.branch;
					while(1)
					{
						if(NULL == pt_temp_node)
						{
							break;
						}

						if((ND_INFO == pt_temp_node->role)&&(SKFChina_ConfigurationAndCommand_GatewayInfoPair_mac_address_tag == pt_temp_node->tag))
						{
							memcpy( pt_resp_buf_array[kpidx % kp_arrsz].conf_val.general_content, pt_temp_node->nd_data.dtinfo.dtbuf, pt_temp_node->nd_data.dtinfo.size > MAX_GENERAL_CONF_CONTENT ? MAX_GENERAL_CONF_CONTENT : pt_temp_node->nd_data.dtinfo.size);
							break;
						}
						
						pt_temp_node = pt_temp_node->next;
					}
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_general_config_content_bool_tag:// 13
				{// bool
					pt_resp_buf_array[kpidx % kp_arrsz].conf_val.val_bool = *((bool*)pt_leavef_nd->nd_data.dtinfo.dtbuf);								
					break;
				}
				case SKFChina_ConfigurationAndCommand_ConfigPair_memory_id_tag:// 10
				{// memory ID
					break;
				}
				default:
				{
					DBG_LOG_WARN("unknown message tag");
					break;
				}
			}
			
			pt_leavef_nd = pt_leavef_nd->next;
		}

		kpidx++;
		
	}
	//DBG_LOG_INFO("BKP===pick sensor conf-info done");
	EXIT:
		if(NULL != pt_nd_queue)
		{
			l_queue_destroy( pt_nd_queue, NULL);
			pt_nd_queue = NULL;
		}		
		return tpret;
}	


/*
case SKFChina_App_AppMessage_config_hash_upload_tag:// 5
*/
static app_state_t msg_handler_for_config_hash_upload(struct sensor_op_controller*pt_sensor_op_ctr,const struct msg_tree_node*pt_root)
{
	struct node_path kp_ndpath = {0};
	// to fill the node path
	#if(0)
	kp_ndpath.path[0] = 5;
	kp_ndpath.path[1] = 1;		
	kp_ndpath.path[2] = 18;
	kp_ndpath.path[3] = 1;
	kp_ndpath.kpdepth = 4;
	#else
		kp_ndpath.path[0] = SKFChina_App_AppMessage_config_hash_upload_tag;
		kp_ndpath.path[1] = SKFChina_ConfigurationAndCommand_ConfigHashUpload_header_tag;
		kp_ndpath.path[2] = SKFChina_Froto_FrotoHeader_acked_message_seq_number_tag;
		kp_ndpath.path[3] = SKFChina_Froto_SeqNoElement_seqNo_tag;
		kp_ndpath.kpdepth = 4;
	#endif
	return app_sch_signal_and_pop_sensor_op_waiting_obj( pt_sensor_op_ctr, pt_root, kp_ndpath);
}

/* SKFChina_App_AppMessage_specific_config_upload_tag:
*/
static app_state_t msg_handler_for_specific_config_upload(struct sensor_op_controller*pt_sensor_op_ctr,const struct msg_tree_node*pt_root)
{	
	struct node_path kp_ndpath = {0};
	// to fill the node path
	#if(0)
	kp_ndpath.path[0] = 6;
	kp_ndpath.path[1] = 1;		
	kp_ndpath.path[2] = 18;
	kp_ndpath.path[3] = 1;
	kp_ndpath.kpdepth = 4;
	#else
		kp_ndpath.path[0] = SKFChina_App_AppMessage_specific_config_upload_tag;
		kp_ndpath.path[1] = SKFChina_ConfigurationAndCommand_SpecificConfigUpload_header_tag;		
		kp_ndpath.path[2] = SKFChina_Froto_FrotoHeader_acked_message_seq_number_tag;
		kp_ndpath.path[3] = SKFChina_Froto_SeqNoElement_seqNo_tag;
		kp_ndpath.kpdepth = 4;			
	#endif
	return app_sch_signal_and_pop_sensor_op_waiting_obj( pt_sensor_op_ctr, pt_root, kp_ndpath);
		
}

static app_state_t msg_handler_for_image_block_request(const struct msg_tree_node*pt_root, struct longdata_xfer_ctr*pt_img_dissem_ctr);
static app_state_t msg_handler_for_image_block_request_v2(const struct msg_tree_node*pt_root, struct sensor_op_controller *pt_sensor_op_ctr);

/*function to handle all in-comming Froto message
pt_dtbuf ~ the buffer where the encoded Froto message is
kplen ~ the length of message in the buffer 
ret@ST_OK when things go well
*/
app_state_t
app_froto_incomming_msg_handler(
  const uint8_t *pt_dtbuf,
  const uint32_t kplen)
{
  app_state_t tpret = ST_OK;
  pb_istream_t kp_istream = { 0 };
  struct msg_tree_node hld_root = { .role = ND_ROOT, 0 };
  int32_t kp_msg_tag = -1;

  if((NULL == pt_dtbuf) || (kplen <= 0))
  {
    DBG_LOG_ERR("Invalid parameter");
    tpret = ST_ERR;
    return tpret;
  }

  // Read the incoming Froto message to @p hld_root
  kp_istream = pb_istream_from_buffer(pt_dtbuf, kplen);
  if(false ==
     util_froto_format_msg_info(&hld_root, &kp_istream,
                                SKFChina_App_AppMessage_fields))
  {
    DBG_LOG_ERR("Failed to format Froto message");
    tpret = ST_ERR;
    goto EXIT;
  }

  // To get SKFChina_App_AppMessage
  kp_msg_tag = pick_froto_submsg_tag(&hld_root);
  if(kp_msg_tag < 0)
  {
    DBG_LOG_ERR("Failed to get the message tag");
    tpret = ST_ERR;
    goto EXIT;
  }
  DBG_LOG_DEBUG("The Froto message tag is %d", kp_msg_tag);

  switch(kp_msg_tag)
  {
    // Data upload
    case SKFChina_App_AppMessage_data_upload_tag:
    {
      if(ST_OK != app_froto_data_uploading_handler(&hld_root))
      {
        DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_data_upload_tag");
      }
      break;
    }
    // Data selection
    case SKFChina_App_AppMessage_data_selection_tag:
    {
      if(ST_OK != msg_handler_for_data_selection(&hld_root))
      {
        DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_data_selection_tag");
      }
      break;
    }
    // Config retrieve
    case SKFChina_App_AppMessage_config_retrieve_tag:
    {
      uint32_t tp_tskid = 0;
      if(ST_OK != msg_handler_for_config_retrieve(&hld_root, &tp_tskid))
      {
        DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_config_retrieve_tag");
        break;
      }
      // Continue to send the fist pkt of the configuration file
      msg_handler_for_file_retrieve(&hld_root, tp_tskid);
      break;
    }
    // Config hash upload
    case SKFChina_App_AppMessage_config_hash_upload_tag:
    {
      if(ST_OK !=
         msg_handler_for_config_hash_upload(
           app_sch_get_sensor_op_controller(),
           &hld_root))
      {
        DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_config_hash_upload_tag");
      }
      break;
    }
    // Specific config upload
    case SKFChina_App_AppMessage_specific_config_upload_tag:
    {
      if(ST_OK !=
         msg_handler_for_specific_config_upload(
           app_sch_get_sensor_op_controller(),
           &hld_root))
      {
        DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_specific_config_upload_tag");
      }
      break;
    }
    // Config dissem
    case SKFChina_App_AppMessage_config_dissem_tag:
    {
      if(ST_OK != locate_time_array(&hld_root))
      {
        DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_config_dissem_tag");
      }
      break;
    }
    // Command dissem
    case SKFChina_App_AppMessage_command_dissem_tag:
    {
      // TODO: To support in the future
      DBG_LOG_WARN(
        "So far, not support to handle SKFChina_App_AppMessage_command_dissem_tag");
      break;
    }
    // Version retrieve
    case SKFChina_App_AppMessage_version_retrieve_tag:
    {
      if(ST_OK != msg_handler_for_version_retrieve(&hld_root))
      {
        DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_version_retrieve_tag");
      }
      break;
    }
    // Current version upload
    case SKFChina_App_AppMessage_current_version_upload_tag:  // 10
    {
#if (0)  // Only for debug
      if(ST_OK ==
         fuota_notify_dissem(&hld_root,
                             app_froto_get_longdata_xfer_timer_ctr()))
      {
        image_block_dissem(&hld_root,
                           app_froto_get_longdata_xfer_timer_ctr());
      }
      else
      {
        DBG_LOG_ERR("Failed to notify FUOTA dissemination");
      }
#else
      if(ST_OK !=
         msg_handler_for_version_upload_v2(app_sch_get_sensor_op_controller(),
                                           &hld_root))
      {
        DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_current_version_upload_tag");
      }
#endif
      break;
    }
    // Last FUOTA upload
    case SKFChina_App_AppMessage_last_fuota_upload_tag:
    {
      // TODO: To support in the future
      DBG_LOG_WARN(
        "So far, not support to handle SKFChina_App_AppMessage_last_fuota_upload_tag");
      break;
    }
    case SKFChina_App_AppMessage_fuota_notify_dissem_tag:
    {  // the OTA image is about to come, create a file-trans controlling
       // block for the receivation
      struct msg_tree_node *pt_node = NULL;
      struct node_path kp_nd_path = { 0 };

      uint32_t tp_tskid = 0;
      uint32_t tp_blk_num = 0;
      // to get the tskid
      kp_nd_path.path[0] = SKFChina_App_AppMessage_fuota_notify_dissem_tag;
      kp_nd_path.path[1] =
        SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_fuota_task_id_tag;
      kp_nd_path.kpdepth = 2;
      if(false == app_froto_get_node(&hld_root, kp_nd_path, &pt_node))
      {
        DBG_LOG_ERR("fail to get task-id");
        break;
      }
      tp_tskid = *((uint32_t *)(pt_node->nd_data.dtinfo.dtbuf));
      // to get the total block
      memset(&kp_nd_path, 0, sizeof(struct node_path));
      kp_nd_path.path[0] = SKFChina_App_AppMessage_fuota_notify_dissem_tag;
      kp_nd_path.path[1] =
        SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_header_tag;
      kp_nd_path.path[2] = SKFChina_Froto_FrotoHeader_total_block_tag;
      kp_nd_path.kpdepth = 3;
      if(false == app_froto_get_node(&hld_root, kp_nd_path, &pt_node))
      {
        DBG_LOG_ERR("fail to get total block info");
        break;
      }
      tp_blk_num = *((uint32_t *)(pt_node->nd_data.dtinfo.dtbuf));

      DBG_LOG_INFO("the file trans , tsk-id = %d, total-block-num = %d",
                   tp_tskid, tp_blk_num);

      if(ST_OK !=
         app_froto_create_file_trans_task(app_froto_get_file_recv_ctr(),
                                          tp_blk_num, tp_tskid, FTYPE_IMG,
                                          FOP_RECV))
      {
        DBG_LOG_ERR("fail to cteate file-trans controller");
        break;
      }
      DBG_LOG_INFO(
        "file receivation task with ID %d is created successfully",
        tp_tskid);
      break;
    }
    // FUOTA status
    case SKFChina_App_AppMessage_fuota_status_tag:
    {
      // TODO: To support in the future
      DBG_LOG_WARN(
        "So far, not support to handle SKFChina_App_AppMessage_fuota_status_tag");
      break;
    }
	// Image block dissem
    case SKFChina_App_AppMessage_image_block_dissem_tag:
    {  
	  if(ST_OK != msg_handler_for_image_block_dissem(&hld_root))
	  {
		DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_image_block_dissem_tag");
	  }
      break;
    }
	// Image block request
    case SKFChina_App_AppMessage_image_block_request_tag:
    {

       // TODO: To support in the future
#if (0)  // Now wo DO NOT wait for response from the peer device, just send
         // it. you need this when you expect a response from peer device
      msg_handler_for_image_block_request(&hld_root,
                                          app_froto_get_longdata_xfer_timer_ctr());
#else
	if(ST_OK != msg_handler_for_image_block_request_v2(&hld_root,
                                             app_sch_get_sensor_op_controller()))
	{
												DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_image_block_request_tag");
	}
#endif
      break;
    }
    // Ack
    case SKFChina_App_AppMessage_ack_tag:
    {
      // TODO: To support in the future
      DBG_LOG_WARN(
        "So far, not support to handle SKFChina_App_AppMessage_ack_tag");
      break;
    }
    // Debug message
    case SKFChina_App_AppMessage_debug_message_tag:
    {
      // TODO: To support in the future
      DBG_LOG_WARN(
        "So far, not support to handle SKFChina_App_AppMessage_debug_message_tag");
      break;
    }
	// File notify dissem
    case SKFChina_App_AppMessage_file_notify_dissem_tag:
    {  // to do the initialize for data receivation
      struct msg_tree_node *pt_node = NULL;
      struct node_path kp_nd_path = { 0 };
      uint32_t tp_tskid = 0;
      uint32_t tp_blk_num = 0;
      // to get the tskid
      kp_nd_path.path[0] = SKFChina_App_AppMessage_file_notify_dissem_tag;
      kp_nd_path.path[1] =
        SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_task_id_tag;
      kp_nd_path.kpdepth = 2;
      if(false == app_froto_get_node(&hld_root, kp_nd_path, &pt_node))
      {
        DBG_LOG_ERR("fail to get task-id info");
        break;
      }
      tp_tskid = *((uint32_t *)(pt_node->nd_data.dtinfo.dtbuf));

      DBG_LOG_DEBUG("BKP tskid = %d", tp_tskid);

      // to get the total block
      memset(&kp_nd_path, 0, sizeof(struct node_path));
      kp_nd_path.path[0] = SKFChina_App_AppMessage_file_notify_dissem_tag;
      kp_nd_path.path[1] =
        SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_header_tag;
      kp_nd_path.path[2] = SKFChina_Froto_FrotoHeader_total_block_tag;
      kp_nd_path.kpdepth = 3;
#if (0)
      if(false == app_froto_get_node(&hld_root, kp_nd_path, &pt_node))
      {
        DBG_LOG_ERR("fail to get the total-blk info");
        break;
      }
      tp_blk_num = *((uint32_t *)(pt_node->nd_data.dtinfo.dtbuf));
#else  // this is only for debug
#warning \
      "TODO====this part is only for debug, remember to modify this part"
      tp_blk_num = 5;
      //tp_tskid = 666;
#endif

      DBG_LOG_DEBUG("the file trans tsk-id = %d, total-blk-num = %d",
                   tp_tskid, tp_blk_num);

      if(ST_OK !=
         app_froto_create_file_trans_task(app_froto_get_file_recv_ctr(),
                                          tp_blk_num, tp_tskid, FTYPE_CONF,
                                          FOP_RECV))
      {
        DBG_LOG_ERR("fail to create file trans controller");
        break;
      }

      DBG_LOG_INFO(
        "file receivation task with ID %d is created successfully",
        tp_tskid);

      break;
    }
    // File notify upload
    case SKFChina_App_AppMessage_file_notify_upload_tag:
    {
      // TODO: To support in the future
      DBG_LOG_WARN(
        "So far, not support to handle SKFChina_App_AppMessage_file_notify_upload_tag");
      break;
    }
    // Image block upload
    case SKFChina_App_AppMessage_image_block_upload_tag:
    {
      // TODO: To support in the future
      DBG_LOG_WARN(
        "So far, not support to handle SKFChina_App_AppMessage_image_block_upload_tag");
      break;
    }
	// Image block retrieve
    case SKFChina_App_AppMessage_image_block_retrieve_tag:
    {
	  // TODO: To support in the future
	  if(ST_OK != msg_handler_for_file_retrieve(&hld_root, 555))
      {
		DBG_LOG_ERR(
          "Failed to handle SKFChina_App_AppMessage_image_block_retrieve_tag");
	  }
      break;
    }
    default:
    {
      DBG_LOG_WARN("Unknown message tag");
      break;
    }
  }

EXIT:
  util_froto_release_msg_tree(&hld_root);
  return tpret;
}




#if(1) // for file transportation

static struct file_trans_controller g_file_send_ctr = {0};
static struct file_trans_controller g_file_recv_ctr = {0};


struct file_trans_controller *app_froto_get_file_send_ctr(void)
{
	return &g_file_send_ctr;
}

/*pt_ftctr ~ the file-trans controller for sending is expected
*/
uint32_t app_froto_allocate_file_send_id(struct file_trans_controller*pt_ftcr)
{
	if(NULL == pt_ftcr)
	{
		DBG_LOG_ERR("unexpected NULL");
		return 0;
	}
	pt_ftcr->ftrans_id_cnt++;
	return pt_ftcr->ftrans_id_cnt;
}

struct file_trans_controller *app_froto_get_file_recv_ctr(void)
{
	return &g_file_recv_ctr;
}


app_state_t app_froto_init_file_trans_ctr(struct file_trans_controller*pt_fctr)
{
	app_state_t tpret = ST_OK;
	if(NULL == pt_fctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	pt_fctr->ftrans_id_cnt = 0;

	pt_fctr->pt_queue_ftcb = l_queue_new();

	if(0 != pthread_mutex_init( &pt_fctr->mtx, NULL))
	{
		DBG_LOG_ERR("fail to init mtx");
		tpret = ST_ERR;
	}
	return tpret;
}


app_state_t app_froto_definit_file_trans_ctr(struct file_trans_controller*pt_fctr)
{
	app_state_t tpret = ST_OK;
	if(NULL == pt_fctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}


	l_queue_destroy( pt_fctr->pt_queue_ftcb, l_free);
	pthread_mutex_destroy( &pt_fctr->mtx);

	return tpret;
}




/*for eacn file transporation, we add a controlling-block to the queue
*/
app_state_t app_froto_create_file_trans_task(struct file_trans_controller*pt_fctr, const uint32_t blk_num, const uint32_t tsk_id, enum file_type ftype, enum file_trans_op fop)
{
	app_state_t tpret = ST_OK;

	int32_t kp_fsz = 0;
	struct file_trans_control_block *pt_ftcb = NULL;

	if(NULL == pt_fctr)	
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	if(blk_num <= 0)
	{
		DBG_LOG_ERR("invalid block number");
		tpret = ST_ERR;
		return tpret;
	}

	if(NULL != l_queue_find( pt_fctr->pt_queue_ftcb, to_compare_task_id, (const void *)tsk_id))
	{// there is FTCB with the given tsk_id exist, then we DO NOT create it;  this is not suppose to happen
		DBG_LOG_ERR("FTCB with task-ID %d exists", tsk_id);
		tpret = ST_ERR;
		return tpret;
	}

	
	pt_ftcb = (struct file_trans_control_block*)l_malloc(sizeof(struct file_trans_control_block));
	if(NULL == pt_ftcb)
	{
		DBG_LOG_ERR("l_malloc fail");
		tpret = ST_ERR;
		return tpret;
	}
	memset( pt_ftcb, 0, sizeof(struct file_trans_control_block));

	

	//.... to open the file and write the data to a file
	if(ftype == FTYPE_CONF)
	{
		snprintf( pt_ftcb->des_fpath, MAX_FPATH,"%s", FPATH_CONF);
	}
	else
	{
		snprintf( pt_ftcb->des_fpath, MAX_FPATH,"%s", FPATH_IMG);
	}


	if(FOP_RECV == fop)
	{// data-receivation
		pt_ftcb->fd = open( pt_ftcb->des_fpath, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR|S_IWUSR);
	}
	else
	{ // data sending
		pt_ftcb->fd = open( pt_ftcb->des_fpath, O_CREAT | O_RDWR, S_IRUSR|S_IWUSR);		
	}
	if(pt_ftcb->fd < 0)
	{
		DBG_LOG_ERR("fail to create/open file %s, err-info %s", pt_ftcb->des_fpath, strerror(errno));
		tpret = false;
		goto ERR;
	}


	
	if(FOP_SEND == fop)
	{
		// to get the size of the file	
		kp_fsz = lseek( pt_ftcb->fd, 0, SEEK_END);
		if(kp_fsz < 0)
		{
			DBG_LOG_ERR("lseek fail");
			close(pt_ftcb->fd);
		
			tpret = false;
			goto ERR;
		}
		lseek(pt_ftcb->fd, 0, SEEK_SET); // reset the file-pointer to the head

		pt_ftcb->tskid = tsk_id;
		pt_ftcb->ftype = ftype;

		pt_ftcb->total_blks = kp_fsz / FILE_PKT_SIZE;
		if(kp_fsz % FILE_PKT_SIZE)
		{
			pt_ftcb->total_blks++;
		}
		
		pt_ftcb->blk_idx = 0;
		pt_ftcb->fsz = kp_fsz;

	}
	else
	{
		pt_ftcb->tskid = tsk_id;
		pt_ftcb->ftype = ftype;
		pt_ftcb->total_blks = blk_num;
		pt_ftcb->blk_idx = 0;
		pt_ftcb->fsz = 0;
	}
	// now the controlling block is on the queue
	DBG_LOG_INFO("ftcb with id %d is created", tsk_id);
	
	pthread_mutex_lock( &pt_fctr->mtx);
	l_queue_push_tail( pt_fctr->pt_queue_ftcb, (void *)pt_ftcb);
	pthread_mutex_unlock( &pt_fctr->mtx);	
	
	return tpret;
	
	ERR:
		if(pt_ftcb)
		{
			l_free( pt_ftcb);
			pt_ftcb = NULL;
		}
		return tpret;
}


/*to remove the file transportation task from the queue
*/
app_state_t app_froto_remove_file_trans_task(struct file_trans_controller*pt_fctr, const uint32_t tsk_id)
{
	app_state_t tpret = ST_ERR;

	struct file_trans_control_block *pt_ftcb = NULL ;
	
	if(NULL == pt_fctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	pt_ftcb =(struct file_trans_control_block*) l_queue_remove_if( pt_fctr->pt_queue_ftcb, to_compare_task_id, (const void *)tsk_id);

	DBG_LOG_INFO("FTCB with task-id %d is removed", pt_ftcb->tskid);
	
	l_free(pt_ftcb);
	
	return tpret;
}

/*to pick the number of blocks for file transportation
*/
bool app_froto_pick_total_blk_num(const struct msg_tree_node*pt_root, uint32_t *pt_blk_num)
{
	bool tpret = false;
	struct node_path kp_nd_path = {.kpdepth = 0, .path = {0}};

	struct msg_tree_node *pt_node = NULL;


	struct l_queue*pt_queue = l_queue_new();
	struct l_queue_entry *pt_entry = NULL;

	//
	if((NULL == pt_root)||(pt_root  != ND_ROOT))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = false;
		goto EXIT;
	}

	// fill the node path
	kp_nd_path.path[0] = SKFChina_App_AppMessage_file_notify_upload_tag;
	kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_header_tag;
	kp_nd_path.path[2] = SKFChina_Froto_FrotoHeader_total_block_tag;
	kp_nd_path.kpdepth = 3;

	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_queue))
	{
		DBG_LOG_INFO("fail to locate the block number");
		tpret = false;
		goto EXIT;
	}

	if(1 != l_queue_length( pt_queue))
	{
		DBG_LOG_ERR("this is not not supposed to happen'");
		tpret = false;
		goto EXIT;
	}

	pt_entry = l_queue_get_entries( pt_queue);

	pt_node = (struct msg_tree_node*)pt_entry->data;

	*pt_blk_num = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf); 
	
	EXIT:
		l_queue_destroy( pt_queue, NULL);
		return tpret;
}

/*to pick the current block idx
*/
bool app_froto_pick_current_blk_idx(const struct msg_tree_node*pt_root, uint32_t *pt_cur_blk_idx)
{
	bool tpret = false;
	struct node_path kp_nd_path = {.kpdepth = 0, .path = {0}};

	struct msg_tree_node *pt_node = NULL;


	struct l_queue*pt_queue = l_queue_new();
	struct l_queue_entry *pt_entry = NULL;

	//
	if((NULL == pt_root)||(pt_root  != ND_ROOT))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = false;
		goto EXIT;
	}

	// fill the node path
	kp_nd_path.path[0] = SKFChina_App_AppMessage_file_notify_upload_tag;
	kp_nd_path.path[1] = SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_header_tag;
	kp_nd_path.path[2] = SKFChina_Froto_FrotoHeader_current_block_tag;
	kp_nd_path.kpdepth = 3;

	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_queue))
	{
		DBG_LOG_INFO("fail to locate the block number");
		tpret = false;
		goto EXIT;
	}

	if(1 != l_queue_length( pt_queue))
	{
		DBG_LOG_ERR("this is not not supposed to happen'");
		tpret = false;
		goto EXIT;
	}

	pt_entry = l_queue_get_entries( pt_queue);

	pt_node = (struct msg_tree_node*)pt_entry->data;

	*pt_cur_blk_idx = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf); 
	
	EXIT:
		l_queue_destroy( pt_queue, NULL);
		return tpret;
}


/*to get all the message node which match the node-path given
ret@
*/
bool app_froto_get_node_queue(const struct msg_tree_node*pt_root, const struct node_path kp_nd_path, struct l_queue*pt_res_queue)
{
	bool tpret = true;

	if((NULL == pt_root)||(NULL == pt_res_queue))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = false;
		return tpret;
	}
	return util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_res_queue);
}

/*
*/
bool app_froto_get_node(const struct msg_tree_node*pt_root, const struct node_path kp_nd_path, struct msg_tree_node**pt_node)
{
	bool tpret = true;
	struct l_queue*pt_queue = NULL;
	struct l_queue_entry *pt_entry = NULL;
	
	if((NULL == pt_root)||(NULL == pt_node))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = false;
		return tpret;
	}

	pt_queue = l_queue_new();
	
	tpret = util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_queue);
	if(false == tpret)
	{
		DBG_LOG_ERR("fail to locate the msg node");
		goto EXIT;
	}
	
	// DBG_LOG_DEBUG("BKP");
	
	if(1 != l_queue_length( pt_queue))
	{
		DBG_LOG_ERR("unexpected node is found");
		tpret = false;
		goto EXIT;
	}
	
	// DBG_LOG_DEBUG("BKP");
	
	pt_entry = l_queue_get_entries( pt_queue);

	// now the node is returned
	*pt_node = pt_entry->data;
	
	DBG_LOG_DEBUG("BKP===the node is found");
	EXIT:
		l_queue_destroy( pt_queue, NULL);
		return tpret;
}



#endif





#if(1) // for data-selection request
/* For short data transfer */
static struct shortdata_xfer_ctr g_shortdata_xfer_timer_ctr = { 0 };
static void shortdata_xfer_timer_handler(
  int sig,
  siginfo_t *pt_sig,
  void *uc);
/* For long data transfer */
static struct longdata_xfer_ctr g_longdata_xfer_timer_ctr =
{ 0 };
static void longdata_xfer_timer_handler(
  int sig,
  siginfo_t *pt_sig,
  void *uc);

/**
 * @brief Get the pointer of shortdata_xfer_timer_ctr
 */
struct shortdata_xfer_ctr *
app_froto_get_shortdata_xfer_timer_ctr(void)
{
  return &g_shortdata_xfer_timer_ctr;
}
/**
 * @brief Get the pointer of longdata_xfer_timer_ctr
 */
struct longdata_xfer_ctr *
app_froto_get_longdata_xfer_timer_ctr(void)
{
  return &g_longdata_xfer_timer_ctr;
}
/**
 * @brief Initialize the timeout timer for short data
 */
void
app_froto_init_shortdata_xfer_timer_ctr(
  struct shortdata_xfer_ctr *shortdata_xfer_timer_ctr)
{
  struct sigaction kp_sa = { 0 };
  timer_t kp_timeid = 0;
  struct sigevent kp_sev = { 0 };
  sigset_t kp_msk = { 0 };

  if(shortdata_xfer_timer_ctr == NULL)
  {
    DBG_LOG_ERR("shortdata_xfer_timer_ctr is NULL");
    return;
  }

  // Initialize members and mutex
  shortdata_xfer_timer_ctr->is_in_process = false;
  shortdata_xfer_timer_ctr->pt_dtrequest_queue = l_queue_new();
  shortdata_xfer_timer_ctr->msg_seq_no = 0;
  pthread_mutex_init(&shortdata_xfer_timer_ctr->mtx, NULL);

  // To setup the timer
  // 1. Set up the handler for timer signal
  kp_sa.sa_flags = SA_SIGINFO;
  kp_sa.sa_sigaction = shortdata_xfer_timer_handler;
  sigemptyset(&kp_sa.sa_mask);
  if(sigaction(SIG_SHORTDATA_XFER_TIMEOUT, &kp_sa, NULL) == -1)
  {
    DBG_LOG_ERR("Failed to set sigaction");
    return;
  }
  // 2. Block the timer signal temporarily
  sigemptyset(&kp_msk);
  sigaddset(&kp_msk, SIG_SHORTDATA_XFER_TIMEOUT);
  if(sigprocmask(SIG_SETMASK, &kp_msk, NULL) == -1)
  {
    DBG_LOG_ERR("Failed to set the signal mask");
    return;
  }
  // 3. Create the timer
  kp_sev.sigev_notify = SIGEV_SIGNAL;
  kp_sev.sigev_signo = SIG_SHORTDATA_XFER_TIMEOUT;
  kp_sev.sigev_value.sival_ptr = &kp_timeid;
  if(timer_create(CLOCKID, &kp_sev, &kp_timeid) == -1)
  {
    DBG_LOG_ERR("Failed to create timer, for %s", strerror(errno));
    return;
  }
  // 4. Unblock the signal
  if(sigprocmask(SIG_UNBLOCK, &kp_msk, NULL) == -1)
  {
    DBG_LOG_ERR("Failed to unblock signal\n");
    return;
  }

  // Restore the timer ID
  shortdata_xfer_timer_ctr->timer_id = kp_timeid;

  return;
}
/**
 * @brief De-initialize the timeout timer for short data
 */
void
app_froto_deinit_shortdata_xfer_timer_ctr(
  struct shortdata_xfer_ctr *shortdata_xfer_timer_ctr)
{
  if(shortdata_xfer_timer_ctr == NULL)
  {
    DBG_LOG_ERR("shortdata_xfer_timer_ctr is NULL");
    return;
  }
  // Destroy the queue
  l_queue_destroy(shortdata_xfer_timer_ctr->pt_dtrequest_queue, l_free);
  // Delete the timer
  timer_delete(shortdata_xfer_timer_ctr->timer_id);
  // Destroy the mutex
  pthread_mutex_destroy(&shortdata_xfer_timer_ctr->mtx);
}
/**
 * @brief The handler for short data transfer timer
 */
static void
shortdata_xfer_timer_handler(
  int sig,
  siginfo_t *pt_sig,
  void *uc)
{
  struct shortdata_xfer_ctr *shortdata_xfer_timer_ctr =
    app_froto_get_shortdata_xfer_timer_ctr();

  DBG_LOG_WARN("Short data transfer timeout timer, sig %d", sig);

  app_froto_shortDataReveivedFg = 0x02;
  app_froto_shortDataReveivedRetryCnt++;

  // Release the resource once timeout occurred
  pthread_mutex_lock(&shortdata_xfer_timer_ctr->mtx);
  shortdata_xfer_timer_ctr->is_in_process = false;
  l_queue_clear(shortdata_xfer_timer_ctr->pt_dtrequest_queue, l_free);
  shortdata_xfer_timer_ctr->msg_seq_no = 0;
  pthread_mutex_unlock(&shortdata_xfer_timer_ctr->mtx);
}
/**
 * @brief Initialize the timeout timer for long data
 */
void
app_froto_init_longdata_xfer_timer_ctr(
  struct longdata_xfer_ctr *longdata_xfer_timer_ctr)
{
  struct sigaction kp_sa = { 0 };
  timer_t kp_timeid = 0;
  struct sigevent kp_sev = { 0 };
  sigset_t kp_msk = { 0 };

  if(longdata_xfer_timer_ctr == NULL)
  {
    DBG_LOG_ERR("longdata_xfer_timer_ctr is NULL");
    return;
  }

  // Initialize members and mutex
  longdata_xfer_timer_ctr->is_in_process = false;
  longdata_xfer_timer_ctr->fd = -1;
  longdata_xfer_timer_ctr->img_ver = 0;
  app_clear_force_fuota(longdata_xfer_timer_ctr);
  pthread_mutex_init(&longdata_xfer_timer_ctr->mtx, NULL);

  // To setup the timer
  // 1. Set up the handler for timer signal
  kp_sa.sa_flags = SA_SIGINFO;
  kp_sa.sa_sigaction = longdata_xfer_timer_handler;
  sigemptyset(&kp_sa.sa_mask);
  if(sigaction(SIG_LONGDATA_XFER_TIMEOUT, &kp_sa, NULL) == -1)
  {
    DBG_LOG_ERR("Failed to set sigaction");
    return;
  }
  // 2. Block the timer signal temporarily
  sigemptyset(&kp_msk);
  sigaddset(&kp_msk, SIG_LONGDATA_XFER_TIMEOUT);
  if(sigprocmask(SIG_SETMASK, &kp_msk, NULL) == -1)
  {
    DBG_LOG_ERR("Failed to set the signal mask");
    return;
  }
  // 3. Create the timer
  kp_sev.sigev_notify = SIGEV_SIGNAL;
  kp_sev.sigev_signo = SIG_LONGDATA_XFER_TIMEOUT;
  kp_sev.sigev_value.sival_ptr = &kp_timeid;
  if(timer_create(CLOCKID, &kp_sev, &kp_timeid) == -1)
  {
    DBG_LOG_ERR("Failed to create timer, for %s", strerror(errno));
    return;
  }
  // 4. Unblock the signal
  if(sigprocmask(SIG_UNBLOCK, &kp_msk, NULL) == -1)
  {
    DBG_LOG_ERR("Failed to unblock signal\n");
    return;
  }

  // Restore the timer ID
  longdata_xfer_timer_ctr->timer_id = kp_timeid;

  return;
}
/**
 * @brief De-initialize the timeout timer for long data
 */
void
app_froto_deinit_longdata_xfer_timer_ctr(
  struct longdata_xfer_ctr *longdata_xfer_timer_ctr)
{
  if(longdata_xfer_timer_ctr == NULL)
  {
    DBG_LOG_ERR("longdata_xfer_timer_ctr is NULL");
    return;
  }
  // TODO
  DBG_LOG_WARN("TODO");
  return;
}
/**
 * @brief The handler for long data transfer timer
 */
static void
longdata_xfer_timer_handler(
  int sig,
  siginfo_t *pt_sig,
  void *uc)
{
  struct longdata_xfer_ctr *longdata_xfer_timer_ctr =
    app_froto_get_longdata_xfer_timer_ctr();

  DBG_LOG_WARN("Long data transfer timeout timer, sig %d", sig);

  // Release the resource once timeout occurred
  pthread_mutex_lock(&longdata_xfer_timer_ctr->mtx);
  longdata_xfer_timer_ctr->is_in_process = false;
  longdata_xfer_timer_ctr->img_ver = 0;
  if(longdata_xfer_timer_ctr->fd >= 0)
  {
    close(longdata_xfer_timer_ctr->fd);
    longdata_xfer_timer_ctr->fd = -1;
  }
  longdata_xfer_timer_ctr->fsize = 0;
  longdata_xfer_timer_ctr->total_blk = 0;
  longdata_xfer_timer_ctr->cur_blk_idx = 0;
  longdata_xfer_timer_ctr->retry_cnt = 0;
  longdata_xfer_timer_ctr->f_tsk_id = 0;
  memset(longdata_xfer_timer_ctr->img_fpath, 0, MAX_FPATH);
  memset(longdata_xfer_timer_ctr->pkt_buf, 0, FILE_PKT_SIZE);
  pthread_mutex_unlock(&longdata_xfer_timer_ctr->mtx);
}

/*
*/
static  bool froto_to_lock_dtselection_ctr(struct shortdata_xfer_ctr*pt_dtselect_ctr)
{
	bool tpret = false;
	pthread_mutex_lock( &pt_dtselect_ctr->mtx);
	if(false == pt_dtselect_ctr->is_in_process)
	{
		pt_dtselect_ctr->is_in_process = true;
		tpret = true;
	}
	pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
	return tpret;
}


/*
*/
static  void froto_to_unlock_dtselection_ctr(struct shortdata_xfer_ctr*pt_dtselect_ctr)
{
	struct itimerspec kp_its = {0};

	// set its to disarm the timer
	kp_its.it_value.tv_sec = 0;
	kp_its.it_value.tv_nsec = 0;
	kp_its.it_interval.tv_sec = 0;
	kp_its.it_interval.tv_nsec = 0;
	
	pthread_mutex_lock( &pt_dtselect_ctr->mtx);
	if(true == pt_dtselect_ctr->is_in_process)
	{
		pt_dtselect_ctr->is_in_process = false;
		// to disarm the timer
		timer_settime( pt_dtselect_ctr->timer_id, 0, &kp_its, NULL);
		//pt_dtselect_ctr->meas_type = 0;
		l_queue_clear( pt_dtselect_ctr->pt_dtrequest_queue, l_free);
		
		pt_dtselect_ctr->msg_seq_no = 0;
	}
	pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
}

//typedef void (*l_timeout_destroy_cb_t) (void *user_data);
/*
*/
static void froto_data_select_request_timeout_destroy(void*user_data)
{
	return;
}

/*pt_mtype_array @ the measurement type array
kp_arr_len @ array length
ret@true is returned when the measurement pattern is valid
*/
static bool to_validate_mtype_partten(const SKFChina_Common_MeasurementType *pt_mtype_array, const uint32_t kp_arr_len)
{
	bool tpret = true;
	
	if((0 == kp_arr_len )||(NULL == pt_mtype_array))
	{ // invalid parameter
		tpret = false;
	}
	else if((1 == kp_arr_len)&&(SKFChina_Common_MeasurementType_UNKNOWN_MEASUREMENT_TYPE != pt_mtype_array[0]))
	{
		tpret = true;
	}
	else if(kp_arr_len > 1)
	{
		for(uint32_t i = 0; i < kp_arr_len; i++)
		{
			if((SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE == pt_mtype_array[i])||
				(SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE) == pt_mtype_array[i] || 
				(SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE == pt_mtype_array[i]))
			{
				tpret = false;
				break;
			}
		}
	}
	else
	{
		tpret = false;
	}
	return tpret;
}
//static uint32_t to_get_waiting_period(SKFChina_Common_MeasurementType kp_mtype)
uint32_t to_get_waiting_period(SKFChina_Common_MeasurementType kp_mtype)

{
	uint32_t tpret = 3;

	#if(0) // the original	
	if(SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE == kp_mtype)
	{
		tpret = 180;
	}
	else if(SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE == kp_mtype)
	{
		tpret = 50;
	}
	else if(SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE == kp_mtype)
	{
		tpret = 30;
	}
	else
	{
		tpret = 2;
	}
	#else
		tpret = 3;
	#endif
	return tpret;
}



/*to check if the measurement type need to be written to file or not
ret@true is returned when we need to write the data to file
*/
bool is_mtype_to_file(const SKFChina_Common_MeasurementType kp_mtype)
{
	bool tpret = false;
	switch(kp_mtype)
	{
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE:
		case SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE:
		case SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE:
		case SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE:
		case SKFChina_Common_MeasurementType_VIBRATION_ACC_STATUS_WAVE:
		{	
			tpret = true;
			break;
		}
		/*...*/
		default:
		{	tpret = false;
		}
	}
	return tpret;
}



/* to send the request for data selection
pt_mtype_array@ the arrary where they measurement type is 
kp_arr_len @ the number of element in pt_mtype_array
pt_gwid_str@the gateway ID string
kp_seq_no@ the message sequence number

NOTE!!! there is some rules we need to follow when we request data from the sensor
1. for data types(like waveform which is splited in different data packet), you have to request for then one by one; and DO NOT request for it 
with some other data type. even though you can put them in a signle requesting packet ;

2. for the other data type except waveform, you can request at most 5 types by one single packet;

*/
#if(1)
app_state_t app_froto_send_data_selection_request(const SKFChina_Common_MeasurementType *pt_mtype_array, const uint32_t kp_arr_len,const uint8_t * pt_gwid_str, const uint32_t kp_seq_no)
{
	app_state_t tpret = ST_OK;
	struct ipc_msg_v2  kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {0};
	struct shortdata_xfer_ctr*pt_dtselect_ctr = NULL;
	struct itimerspec kp_its = {0};
	struct data_item *pt_dtitem = NULL;
	time_t kp_tval = 0;

	if((NULL == pt_mtype_array)||(0 == kp_arr_len)||(NULL == pt_gwid_str))
	{
		DBG_LOG_ERR("invalid parameters");
		tpret = ST_ERR;
		return tpret;
	}

	if(false == to_validate_mtype_partten( pt_mtype_array, kp_arr_len))
	{
		DBG_LOG_ERR("invalid measurement type pattern");
		tpret = ST_ERR;
		return tpret;
	}
	
	#if(0)
		kp_encoded_msg = util_froto_fill_msg_data_selection( kp_mtype, pt_gwid_str, kp_seq_no);
	#else
		//kp_encoded_msg = util_froto_fill_msg_data_selection_v2( 0, sys_get_gw_id_string(), 66);	
		kp_encoded_msg = util_froto_fill_msg_data_selection_v3( pt_mtype_array, kp_arr_len, pt_gwid_str,kp_seq_no);
	#endif
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded msg for data-selection");
		tpret = ST_ERR;
		return tpret;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to release the encoded message
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	// to put the message on the queue
	pt_dtselect_ctr = app_froto_get_shortdata_xfer_timer_ctr();

	// to set the timer interval
	//kp_its.it_value.tv_sec = 180;
	kp_its.it_value.tv_sec = to_get_waiting_period( pt_mtype_array[0]);
	kp_its.it_value.tv_nsec = 0;
	kp_its.it_interval.tv_sec = 0;
	kp_its.it_interval.tv_nsec = 0;
	
	
	kp_tval = time(NULL);
	
	// to setup the controller before we put the request message on the queue
	pthread_mutex_lock( &pt_dtselect_ctr->mtx);

	if(true == pt_dtselect_ctr->is_in_process)
	{
		DBG_LOG_ERR("the dt-selection ctr is in process, timer@0x%x", pt_dtselect_ctr->timer_id);
		tpret = ST_BUSSY;
		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
		
		goto EXIT;
	}

	//pt_dtselect_ctr->meas_type = kp_mtype;
	for(uint32_t i = 0; i < kp_arr_len; i++)
	{
		pt_dtitem = (struct data_item*)l_malloc( sizeof(struct data_item));
		if(NULL == pt_dtitem)
		{
			DBG_LOG_ERR("Critical error , l_malloc fail");
			break;
		}
		memset( pt_dtitem, 0, sizeof(struct data_item));
		// set the measurement type
		pt_dtitem->data_type = pt_mtype_array[i];
		if(true == is_mtype_to_file(pt_mtype_array[i]))
		{ 
			// we need to allocate an file name for the data receivation
			if(pt_mtype_array[i] == SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE)
			{
				snprintf( pt_dtitem->value.fpath.fpath, MAX_FPATH,"%s%d_%d",FPATH_ALARM_PREFIX,kp_tval,i);
			}else{
				snprintf( pt_dtitem->value.fpath.fpath, MAX_FPATH,"%s%d_%d",FPATH_SDATA_PREFIX,kp_tval,i);
			}
			pt_dtitem->value.fpath.fd = -1;
		}
		l_queue_push_head( pt_dtselect_ctr->pt_dtrequest_queue, pt_dtitem);
	}
	pt_dtselect_ctr->msg_seq_no = kp_seq_no;
	pt_dtselect_ctr->is_in_process = true;
	
	//pt_dtselect_ctr->pt_timer = l_timeout_create( 3, froto_data_select_request_timeout_cbk, (void *)pt_dtselect_ctr, froto_data_select_request_timeout_destroy);
	if(timer_settime( pt_dtselect_ctr->timer_id, 0, &kp_its, NULL) < 0)
	{//timer_settime 
		DBG_LOG_ERR("fail to set timer");
		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);	
		froto_to_unlock_dtselection_ctr( pt_dtselect_ctr);
		tpret = ST_ERR;
		goto EXIT;
	}

	pthread_mutex_unlock( &pt_dtselect_ctr->mtx);



	
	DBG_LOG_DEBUG(" timer @0x%x", pt_dtselect_ctr->timer_id);

	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	{
		DBG_LOG_ERR("fail to put msg on the queue");
		
		froto_to_unlock_dtselection_ctr( pt_dtselect_ctr);
		goto EXIT;
	}
	
	EXIT:
		return tpret;
}

#else
app_state_t app_froto_send_data_selection_request(SKFChina_Common_MeasurementType kp_mtype, const uint8_t * pt_gwid_str, uint32_t kp_seq_no)
{
	app_state_t tpret = ST_OK;
	struct ipc_msg_v2  kp_ipc_msg = {0};
	struct encoded_froto_msg_pkt kp_encoded_msg = {0};
	struct shortdata_xfer_ctr*pt_dtselect_ctr = NULL;
	struct itimerspec kp_its = {0};

	#if(0)
		kp_encoded_msg = util_froto_fill_msg_data_selection( kp_mtype, pt_gwid_str, kp_seq_no);
	#else
		//kp_encoded_msg = util_froto_fill_msg_data_selection_v2( 0, sys_get_gw_id_string(), 66);	
		kp_encoded_msg = util_froto_fill_msg_data_selection_v3( 0, sys_get_gw_id_string(), 66);
	#endif
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get the encoded msg for data-selection");
		tpret = ST_ERR;
		return tpret;
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to release the encoded message
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	// to put the message on the queue
	pt_dtselect_ctr = app_froto_get_shortdata_xfer_timer_ctr();

	// to set the timer interval
	kp_its.it_value.tv_sec = 10;
	kp_its.it_value.tv_nsec = 0;
	kp_its.it_interval.tv_sec = 0;
	kp_its.it_interval.tv_nsec = 0;
	
	// to setup the controller before we put the request message on the queue
	pthread_mutex_lock( &pt_dtselect_ctr->mtx);
	if(true == pt_dtselect_ctr->is_in_process)
	{
		DBG_LOG_ERR("the dt-selection ctr is in process, timer@0x%x", pt_dtselect_ctr->timer_id);
		tpret = ST_ERR;
		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
		
		goto EXIT;
	}
	pt_dtselect_ctr->meas_type = kp_mtype;
	pt_dtselect_ctr->msg_seq_no = kp_seq_no;
	pt_dtselect_ctr->is_in_process = true;
	
	//pt_dtselect_ctr->pt_timer = l_timeout_create( 3, froto_data_select_request_timeout_cbk, (void *)pt_dtselect_ctr, froto_data_select_request_timeout_destroy);
	if(timer_settime( pt_dtselect_ctr->timer_id, 0, &kp_its, NULL) < 0)
	{//timer_settime 
		DBG_LOG_ERR("fail to set timer");
		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);	
		froto_to_unlock_dtselection_ctr( pt_dtselect_ctr);
		tpret = ST_ERR;
		goto EXIT;
	}
	pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
	
	DBG_LOG_INFO(" timer @0x%x", pt_dtselect_ctr->timer_id);

	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	{
		DBG_LOG_ERR("fail to put msg on the queue");
		
		froto_to_unlock_dtselection_ctr( pt_dtselect_ctr);
		goto EXIT;
	}
	
	EXIT:
		return tpret;
}
#endif
/**
 * @brief Request for ACC data x/y/z
 */
app_state_t
app_froto_send_data_selection_request_by_mtype_msg(
  SKFChina_SensingDataUpload_MeasurementTypeMsg kp_mtype_msg,
  const uint8_t *pt_gwid_str,
  const uint32_t kp_seq_no)
{
  app_state_t tpret = ST_OK;
  struct ipc_msg_v2 kp_ipc_msg = { 0 };
  struct encoded_froto_msg_pkt kp_encoded_msg = { 0 };
  struct shortdata_xfer_ctr *pt_dtselect_ctr = NULL;
  struct itimerspec kp_its = { 0 };
  struct data_item *pt_dtitem = NULL;
  time_t kp_tval = 0;

  uint8_t kp_dimension = 'X'; // X as the default axis

  SKFChina_Common_MeasurementType pt_mtype_array[1] = { 0 };
  const uint32_t kp_arr_len = 1;

  pt_mtype_array[0] = kp_mtype_msg.measure_type;

  if((NULL == pt_gwid_str))
  {
    DBG_LOG_ERR("invalid parameters");
    tpret = ST_ERR;
    return tpret;
  }

  if((kp_mtype_msg.measure_type <= _SKFChina_Common_MeasurementType_MIN) ||
     (kp_mtype_msg.measure_type > _SKFChina_Common_MeasurementType_MAX))
  {
    DBG_LOG_ERR("invalid measurement type pattern");
    tpret = ST_ERR;
    return tpret;
  }

#if (0)
  kp_encoded_msg = util_froto_fill_msg_data_selection(kp_mtype,
                                                      pt_gwid_str,
                                                      kp_seq_no);
#else
  kp_encoded_msg = util_froto_fill_msg_data_selection_with_mtype_msg(
    kp_mtype_msg, pt_gwid_str, kp_seq_no);
#endif
  if(false == kp_encoded_msg.is_valid)
  {
    DBG_LOG_ERR("fail to get the encoded msg for data-selection");
    tpret = ST_ERR;
    return tpret;
  }

  kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
  memset(kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
  kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
  memcpy(kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf,
         kp_ipc_msg.mtext.proto_pkt.len);

  // to release the encoded message
  util_froto_release_encoded_msg_v2(&kp_encoded_msg);

  // to put the message on the queue
  pt_dtselect_ctr = app_froto_get_shortdata_xfer_timer_ctr();

  // to set the timer interval
  //kp_its.it_value.tv_sec = 180;
  kp_its.it_value.tv_sec = to_get_waiting_period(pt_mtype_array[0]);
  kp_its.it_value.tv_nsec = 0;
  kp_its.it_interval.tv_sec = 0;
  kp_its.it_interval.tv_nsec = 0;

  kp_tval = time(NULL);

  // to setup the controller before we put the request message on the queue
  pthread_mutex_lock(&pt_dtselect_ctr->mtx);

  if(true == pt_dtselect_ctr->is_in_process)
  {
    DBG_LOG_ERR("the dt-selection ctr is in process, timer@0x%x",
                pt_dtselect_ctr->timer_id);
    tpret = ST_BUSSY;
    pthread_mutex_unlock(&pt_dtselect_ctr->mtx);

    goto EXIT;
  }

  // to get the dimension
  if(kp_mtype_msg.has_dimension)
  {
    if(SENSINGCONFIG_SENSOR_AXIS_DIMENSION_X == kp_mtype_msg.dimension)
    {
      kp_dimension = 'X';
    }
    else if(SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Y ==
            kp_mtype_msg.dimension)
    {
      kp_dimension = 'Y';
    }
    else if(SENSINGCONFIG_SENSOR_AXIS_DIMENSION_Z ==
		kp_mtype_msg.dimension)
	{
	kp_dimension = 'Z';
	}
	else
    {
      kp_dimension = 'U'; // Unknown
    }
  }
  else
  {
    kp_dimension = 'X';
  }

  //pt_dtselect_ctr->meas_type = kp_mtype;
  for(uint32_t i = 0; i < kp_arr_len; i++)
  {
    pt_dtitem = (struct data_item *)l_malloc(sizeof(struct data_item));
    if(NULL == pt_dtitem)
    {
      DBG_LOG_ERR("Critical error , l_malloc fail");
      break;
    }
    memset(pt_dtitem, 0, sizeof(struct data_item));
    // set the measurement type
    pt_dtitem->data_type = pt_mtype_array[i];
    if(true == is_mtype_to_file(pt_mtype_array[i]))
    {
      // we need to allocate an file name for the data receivation
      if(pt_mtype_array[i] ==
         SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE)
      {
        snprintf(pt_dtitem->value.fpath.fpath, MAX_FPATH, "%s%d_%d",
                 FPATH_ALARM_PREFIX, kp_tval, i);
      }
      else
      {
        if(kp_mtype_msg.has_dimension)
        {
          snprintf(pt_dtitem->value.fpath.fpath, MAX_FPATH, "%s%d_%d_%c",
                   FPATH_SDATA_PREFIX, kp_tval, i, kp_dimension);
        }
        else
        {
          snprintf(pt_dtitem->value.fpath.fpath, MAX_FPATH, "%s%d_%d",
                   FPATH_SDATA_PREFIX, kp_tval, i);
        }
      }
      pt_dtitem->value.fpath.fd = -1;
    }
    l_queue_push_head(pt_dtselect_ctr->pt_dtrequest_queue, pt_dtitem);
  }
  pt_dtselect_ctr->msg_seq_no = kp_seq_no;
  pt_dtselect_ctr->is_in_process = true;

  //pt_dtselect_ctr->pt_timer = l_timeout_create( 3,
  // froto_data_select_request_timeout_cbk, (void *)pt_dtselect_ctr,
  // froto_data_select_request_timeout_destroy);
  if(timer_settime(pt_dtselect_ctr->timer_id, 0, &kp_its, NULL) < 0)
  {//timer_settime
    DBG_LOG_ERR("fail to set timer");
    pthread_mutex_unlock(&pt_dtselect_ctr->mtx);
    froto_to_unlock_dtselection_ctr(pt_dtselect_ctr);
    tpret = ST_ERR;
    goto EXIT;
  }

  pthread_mutex_unlock(&pt_dtselect_ctr->mtx);

  DBG_LOG_INFO(" timer @0x%x", pt_dtselect_ctr->timer_id);

  if(ST_OK !=
     app_wq_post_to_waiting_queue(app_pro_gen_get_wq_ipc_msg_ctr(),
                                  (uint8_t *)&kp_ipc_msg,
                                  sizeof(struct ipc_msg_v2)))
  {
    DBG_LOG_ERR("fail to put msg on the queue");

    froto_to_unlock_dtselection_ctr(pt_dtselect_ctr);
    goto EXIT;
  }

EXIT:
  return tpret;
}
/*to reset the  timer in the process of data receivation
*/
app_state_t app_froto_dtselect_reset_timer(struct shortdata_xfer_ctr *pt_dtselect_ctr)
{
	app_state_t tpret = ST_OK;
	struct itimerspec kp_its = {0};
	
	if(NULL == pt_dtselect_ctr)
	{
		tpret = ST_ERR;
		return tpret;
	}

	kp_its.it_value.tv_sec = to_get_waiting_period(0);
	kp_its.it_value.tv_nsec = 0;
	kp_its.it_interval.tv_sec = 0;
	kp_its.it_interval.tv_nsec = 0;

	pthread_mutex_lock( &pt_dtselect_ctr->mtx);
	if(true != pt_dtselect_ctr->is_in_process)
	{	
		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
		DBG_LOG_WARN("the controller is in active");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(timer_settime(pt_dtselect_ctr->timer_id, 0, &kp_its, NULL) < 0)
	{
		DBG_LOG_ERR("fail to reset timer , for %s", strerror(errno));
		tpret = ST_ERR;
	}
	DBG_LOG_INFO(" the timer is reset for data-selection operation");
	pthread_mutex_unlock( &pt_dtselect_ctr->mtx);

	EXIT:
		return tpret;
}

#if(1)
/*map measure type to data format
*/
static SKFChina_Common_Format to_map_mea_type_to_data_format(SKFChina_Common_MeasurementType kp_dttype)
{
	SKFChina_Common_Format tpret = SKFChina_Common_Format_UNKNOWN_FORMAT;
	switch(kp_dttype)
	{
		    /* 0 - 62 used */
		case SKFChina_Common_MeasurementType_UNKNOWN_MEASUREMENT_TYPE ://= 0,
		{
			break;
		}
		/* Vibration */
		case    SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE://= 1,
		{
			tpret = SKFChina_Common_Format_FORMAT_INT16;
			break;
		}
		case    SKFChina_Common_MeasurementType_VIBRATION_ENV2_WAVE:// = 2,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE:// = 3,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE:// = 4,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_VIBRATION_ACC_P2P:// = 5,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_VIBRATION_ACC_RMS: //= 6,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_VIBRATION_ENV2_P2P:// = 7,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_VIBRATION_ENV2_RMS:// = 8,
		{
			break;
		}
		case      SKFChina_Common_MeasurementType_VIBRATION_ENV3_P2P:// = 9,
		{
			break;
		}
		case      SKFChina_Common_MeasurementType_VIBRATION_ENV3_RMS:// = 10,
		{
			break;
		}
		case      SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_P2P:// = 11,
		{
			break;
		}	
		case      SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_RMS:// = 12,
		{
			break;
		}
		/* Rotation speed */
		case    SKFChina_Common_MeasurementType_ROTATION_SPEED:// = 31,
		{
			break;
		}
		/* Enviromental temperature */
		case    SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT:// = 13,
		{
			tpret = SKFChina_Common_Format_FORMAT_INT8;
			break;
		}
		case    SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MAX:// = 14,
		{
			tpret = SKFChina_Common_Format_FORMAT_INT8;
			break;
		}
		case    SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN:// = 15,
		{
			tpret = SKFChina_Common_Format_FORMAT_INT8;
			break;
		}
		case    SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_HISTORY:// = 16,
		{
			break;
		}
		/* Enviromental humidity */
		case    SKFChina_Common_MeasurementType_ENVIROMENTAL_HUMIDITY_CURRENT:// = 17,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_ENVIROMENTAL_HUMIDITY_MAX:// = 18,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_ENVIROMENTAL_HUMIDITY_MIN:// = 19,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_ENVIROMENTAL_HUMIDITY_HISTORY:// = 20,
		{
			break;
		}
		/* Board temperature */
		case    SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_CURRENT:// = 21,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_MAX:// = 22,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_MIN:// = 23,
		{
			break;
		}
		case   SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_HISTORY:// = 24,
		{
			break;
		}
		/* Board humidity */
		case    SKFChina_Common_MeasurementType_BOARD_HUMIDITY_CURRENT:// = 25,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_BOARD_HUMIDITY_MAX:// = 26,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_BOARD_HUMIDITY_MIN:// = 27,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_BOARD_HUMIDITY_HISTORY:// = 28,
		{
			break;
		}
		/* Battery */
		case    SKFChina_Common_MeasurementType_VOLTAGE_CURRENT:// = 29,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_REMAINING_VOLUME:// = 30, /* namely RSOC (relative state-of-charge) */
		{
			tpret = SKFChina_Common_Format_FORMAT_UINT8;
			break;
		}
		case    SKFChina_Common_MeasurementType_STATE_OF_HEALTH_CURRENT:// = 37,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_AVERAGE_TIME_TO_FULL:// = 38,
		{
			break;
		}	
		case    SKFChina_Common_MeasurementType_AVERAGE_TIME_TO_EMPTY:// = 39,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_BATTERY_SURFACE_TEMPERATURE_CURRENT:// = 40,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_BATTERY_MNGMENT_IC_TEMPERATURE_CURRENT:// = 41,
		{
			break;
		}
		/* Communication */
		case    SKFChina_Common_MeasurementType_CELLULAR_RSRP_CURRENT:// = 32,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_CELLULAR_RSRQ_CURRENT:// = 33,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_CELLULAR_SINR_CURRENT:// = 34,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_CELLULAR_RSSI_CURRENT:// = 35,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_NB_IOT_CELEVEL_CURRENT:// = 36,
		{
			break;
		}
		case    SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT:// = 42,
		{
			tpret = SKFChina_Common_Format_FORMAT_INT8;
			break;
		}
		/* Advanced algorithm */
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_RUN_CODE:// = 43,
		{
			tpret = SKFChina_Common_Format_FORMAT_UINT8;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_CONDITON_CODE:// = 44,
		{
			tpret = SKFChina_Common_Format_FORMAT_UINT8;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE:// = 45,
		{
			tpret = SKFChina_Common_Format_FORMAT_UINT8;
			break;
		}	
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_MAG_PRE:// = 46,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_VIB_PRE:// = 47,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_START:// = 48,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_END:// = 49,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV:// = 50,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH:// = 51,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR:// = 52,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN:// = 53,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP:// = 54,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV:// = 55,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}	
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI://  = 56,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO://  = 57,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}	
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF://  = 58,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF://  = 59,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}	
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH://  = 60,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV://  = 61,
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		case    SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR://  = 62
		{
			tpret = SKFChina_Common_Format_FORMAT_FLOAT32;
			break;
		}
		default:
		{	
			tpret = SKFChina_Common_Format_UNKNOWN_FORMAT;
			break;
		}
		
	}
}

#endif


#if(1)

/*
#ifndef DT_STORAGE_FORM_DIGIT	
	#define DT_STORAGE_FORM_DIGIT "Digit"
#endif

#ifndef DT_STORAGE_FORM_FILENAME
	#define DT_STORAGE_FORM_FILENAME "Filename"
#endif
*/


/*to fill the data item
pt_branch@ the msg branch must be SKFChina_SensingDataUpload_DataPair_fields
NOTE!? DO NOT do anything might block the running of the function!?
SKFChina_SensingDataUpload_DataPair_fields
*/
static app_state_t to_fill_data_item(struct data_item*pt_dtitem, struct msg_tree_node*pt_branch)
{
	app_state_t tpret = ST_OK;
	struct msg_tree_node*pt_node = NULL;
	struct msg_tree_node*pt_leaf_node = NULL;
	
	if((NULL == pt_dtitem)||(NULL == pt_branch))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	//app_froto_to_dump_dtitem( pt_dtitem);
	
	pt_node = (struct msg_tree_node*)pt_branch->nd_data.branch;
	while(pt_node)
	{		
		pt_leaf_node = pt_node->nd_data.branch;
		if(NULL == pt_leaf_node)
		{
			DBG_LOG_ERR("end of msg branch");
			tpret = ST_ERR;
			goto EXIT;
		}
		if(SKFChina_SensingDataUpload_DataPair_measurement_data_tag == pt_node->tag)
		{//ref@message DataPair
			while(pt_leaf_node)
			{
				switch(pt_leaf_node->tag)
				{
					case SKFChina_SensingDataUpload_Data_start_point_tag:
					{
					
						break;
					}
					case SKFChina_SensingDataUpload_Data_data_tag:
					{
						uint8_t tp_alarm_code[SZ_FROTO_ALARM_CODE] = { 0 };
						DBG_LOG_INFO("The data type is %d\n", pt_dtitem->data_type);
						// util_dbg_buf_dump(pt_leaf_node->nd_data.dtinfo.dtbuf,
						//                   pt_leaf_node->nd_data.dtinfo.size); // only for debug
						if(true == is_mtype_to_file(pt_dtitem->data_type))
						{
						DBG_LOG_INFO("Write data to %s by fd %d\n",
									pt_dtitem->value.fpath.fpath, pt_dtitem->value.fpath.fd);

						if(pt_dtitem->value.fpath.fd < 0)
						{// to open the file
							pt_dtitem->value.fpath.fd = open(pt_dtitem->value.fpath.fpath,
															O_CREAT | O_RDWR,
															S_IRUSR | S_IWUSR);
						}
						if(pt_dtitem->value.fpath.fd >= 0)
						{
							if(SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE ==
							pt_dtitem->data_type)
							{
							memcpy(tp_alarm_code, pt_leaf_node->nd_data.dtinfo.dtbuf,
									pt_leaf_node->nd_data.dtinfo.size >
									SZ_FROTO_ALARM_CODE ? SZ_FROTO_ALARM_CODE : pt_leaf_node->nd_data.dtinfo.size);
							write(pt_dtitem->value.fpath.fd, tp_alarm_code, SZ_FROTO_ALARM_CODE);
							DBG_LOG_DEBUG(
								"The alarm code read from sensor is");
							util_dbg_buf_dump(pt_leaf_node->nd_data.dtinfo.dtbuf,
												pt_leaf_node->nd_data.dtinfo.size);
							DBG_LOG_DEBUG(
								"The alarm code has been written to file %s successfully\n",
								pt_dtitem->value.fpath.fpath);
							close(pt_dtitem->value.fpath.fd);
							}
							else
							{
							write(pt_dtitem->value.fpath.fd, pt_leaf_node->nd_data.dtinfo.dtbuf,
									pt_leaf_node->nd_data.dtinfo.size);
							DBG_LOG_DEBUG(
								"Waveform has been written to file %s successfully\n",
								pt_dtitem->value.fpath.fpath);
							}
						}

						// Set the storage form
						snprintf(pt_dtitem->format, MAX_DT_STORATE_FORM_BUF, "%s",
								DT_STORAGE_FORM_FILENAME);
						}
						else
						{
						memcpy(&pt_dtitem->value.valu64,
								pt_leaf_node->nd_data.dtinfo.dtbuf,
								pt_leaf_node->nd_data.dtinfo.size >
								sizeof(uint64_t) ? sizeof(uint64_t) : pt_leaf_node->nd_data.dtinfo.size);

						// Set the storage form
						snprintf(pt_dtitem->format, MAX_DT_STORATE_FORM_BUF, "%s",
								DT_STORAGE_FORM_DIGIT);

						DBG_LOG_DEBUG("The data read from sensor is");
						util_dbg_buf_dump(pt_leaf_node->nd_data.dtinfo.dtbuf,
											pt_leaf_node->nd_data.dtinfo.size);
						app_froto_shortDataReveivedFg |= 0x01;
						app_froto_shortDataReveivedRetryCnt++;
						}					
						
						// to set the receivation -time
						pt_dtitem->received_ts = time(NULL);
						
						break;
					}
					case SKFChina_SensingDataUpload_Data_crc32_value_tag:
					{
						pt_dtitem->crc32 = *((uint32_t*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					default:
					{
						DBG_LOG_ERR("unknown node tag %d", pt_leaf_node->tag);
						tpret = ST_ERR;
						goto EXIT;
						break;
					}
				}

				pt_leaf_node = pt_leaf_node->next;
			}
		}
		else if(SKFChina_SensingDataUpload_DataPair_measurement_tag == pt_node->tag)
		{//ref@message DataPair
			while(pt_leaf_node)
			{
				switch(pt_leaf_node->tag)
				{
					
					case SKFChina_SensingDataUpload_Measurement_measure_seq_no_tag:
					{ // Measurement ID
						pt_dtitem->meas_id = *((uint32_t*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_sample_time_tag:
					{//Measurement Timestamp
						pt_dtitem->meas_ts = *((uint64_t*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_alarm_or_notify_tag:
					{// Alarm ; NOTE!!! this is a root-node
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_measure_type_tag:
					{// Data Type
						// data type is set when the request is sent, no need to pick and set again
						//pt_dtitem->data_type

						//map to the data_format
						//pt_dtitem->data_format = to_map_mea_type_to_data_format( pt_dtitem->data_type);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_range_tag:
					{//Range
						pt_dtitem->range = *((SKFChina_Common_Range*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_unit_tag:
					{// unit
						pt_dtitem->unit = *((SKFChina_Common_Unit*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						//DBG_LOG_INFO("the unit is %d", pt_dtitem->unit);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_measure_length_sample_tag:
					{//Measurement Length in Pts 
						pt_dtitem->measure_length_sample = *((uint32_t*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						//DBG_LOG_INFO("measure_length_sample is %d", pt_dtitem->measure_length_sample);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_total_data_length_sample_tag:
					{// total data length in Pts 
						pt_dtitem->total_data_length_sample = *((uint32_t*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_dimension_tag:
					{//Dimension of Data 
						pt_dtitem->dimension = *((uint32_t*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_data_format_tag:
					{//Data Format 
						pt_dtitem->data_format = *((SKFChina_Common_Format*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_sample_period_s_tag:
					{// Sample Period
						pt_dtitem->sample_period_s = *((uint32_t*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_encryp_tag:
					{//Encryption 
						pt_dtitem->encryp = *(( SKFChina_Common_Encryption*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_sample_rate_hz_tag:
					{//Sample-rate
						pt_dtitem->sample_rate_hz = *((float*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_product_tag:
					{// Product
						pt_dtitem->product_type = *((SKFChina_Common_ProductType*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					case SKFChina_SensingDataUpload_Measurement_sensor_tag:
					{// Sensor
						pt_dtitem->froto_sensor_type = *((SKFChina_Common_SensorType*)pt_leaf_node->nd_data.dtinfo.dtbuf);
						break;
					}
					//.....
					default:
					{
						DBG_LOG_ERR("unhandled tag %d", pt_leaf_node->tag);
						//tpret = ST_ERR;
						//goto EXIT;
						break;
					}
				}
				
				pt_leaf_node = pt_leaf_node->next;
			}
		}
		else
		{
			DBG_LOG_ERR("unexpected message tag");
			tpret = ST_ERR;
			break;
		}
		pt_node = pt_node->next;
	}
	
	EXIT:
		return tpret;
	
}

//typedef bool (*l_queue_match_func_t) (const void *data, const void *user_data);
static bool cbk_q_match_meas_type(const void *data, const void *user_data)
{
	bool tpret = false;
	 SKFChina_Common_MeasurementType kp_mtype = ( SKFChina_Common_MeasurementType)user_data;
	 struct data_item *pt_dtitem = (struct data_item*)data;
	 if(NULL == data)
	 {
	 	tpret = false;
		return tpret;
	 }
	if(pt_dtitem->data_type == kp_mtype)
	{ // they match
		tpret = true;
	}
	return tpret; 
}

/*to pick the measurement data from the msg node;
pt_dtselect_ctr@points to the controller
pt_branch*points to the msg info branch
ret@ST_OK is expected
*/
static app_state_t to_pick_the_data(struct shortdata_xfer_ctr*pt_dtselect_ctr, const struct msg_tree_node*pt_branch)
{
	app_state_t tpret = ST_OK;

	union dtitem_value kp_dtitem_value = {0};
	struct node_path kp_nd_path = {0};
	struct l_queue *pt_nd_queue = NULL;
	struct l_queue_entry *pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL;

	struct data_item *pt_dtitem = NULL;
	
	SKFChina_Common_MeasurementType kp_m_type = 0;
	uint32_t kp_q_len = 0;
	uint32_t kp_crc32 = 0;
	

	if((NULL == pt_dtselect_ctr)||(NULL == pt_branch))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}
	if(pt_branch->tag != SKFChina_SensingDataUpload_DataUpload_data_pair_tag)
	{
		DBG_LOG_ERR("well ! a branch with tag %s is expected", SKFChina_SensingDataUpload_DataUpload_data_pair_tag);
		tpret = ST_ERR;
		return tpret;
	}

	// to locate the measurement type
	pt_nd_queue = l_queue_new();
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 1;
	kp_nd_path.path[1] = 1;
	kp_nd_path.kpdepth = 2;
	if(false == util_froto_locate_msg_node_v2( pt_branch,  kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the measurement type info");
		tpret = ST_ERR;
		goto EXIT;
	}
	kp_q_len = l_queue_length(pt_nd_queue);
	if(kp_q_len == 1)
	{
		pt_entry = l_queue_get_entries( pt_nd_queue);
		pt_node = (struct msg_tree_node*)pt_entry->data;
		kp_m_type = *(( SKFChina_Common_MeasurementType*)pt_node->nd_data.dtinfo.dtbuf);
	}
	else
	{ // when the data transportation is splited in different the measuremen-type is not always in the packet
		kp_m_type = SKFChina_Common_MeasurementType_UNKNOWN_MEASUREMENT_TYPE;
	}
	

	#if(1)//MK1993U______________________________________________
	pthread_mutex_lock( &pt_dtselect_ctr->mtx);

	if(false == pt_dtselect_ctr->is_in_process)
	{
		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
		DBG_LOG_ERR("well ! the controller is invalid now");
		tpret = ST_ERR;
		goto EXIT;
	}

	if(SKFChina_Common_MeasurementType_UNKNOWN_MEASUREMENT_TYPE == kp_m_type)
	{ // only for data types that is splited in diffent packet, ref@app_froto_send_data_selection_request
		kp_q_len = l_queue_length( pt_dtselect_ctr->pt_dtrequest_queue);
		if(kp_q_len != 1)
		{ // unacceptable situation 
			pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
			DBG_LOG_ERR("unexpected queue length %d", kp_q_len);
			tpret = ST_ERR;
			goto EXIT;
		}
		pt_entry = l_queue_get_entries( pt_dtselect_ctr->pt_dtrequest_queue);
		pt_dtitem = (struct data_item*)pt_entry->data;
		kp_m_type = pt_dtitem->data_type;
		
		if((SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE != kp_m_type)\
			&&(SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE != kp_m_type)\
			&&(SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE != kp_m_type))
		{//unacceptable situation
			pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
			DBG_LOG_ERR("unexpected measurement type here");
			tpret = ST_ERR;
			goto EXIT;
		}
	}
	else
	{
		pt_dtitem = l_queue_find( pt_dtselect_ctr->pt_dtrequest_queue, cbk_q_match_meas_type, (const void *)kp_m_type);
		if(NULL == pt_dtitem)
		{		
			pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
			DBG_LOG_ERR("fail to find data-item with mtype %d", kp_m_type);
			tpret = ST_ERR;
			goto EXIT;
		}
	}

	tpret = to_fill_data_item(pt_dtitem, pt_branch);
	
	pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
	#endif //MK1993D_____________________________________________

	DBG_LOG_INFO("tp_fill_data_item ret %d", tpret);
	
	EXIT:
		if(pt_nd_queue)
		{
			l_queue_destroy( pt_nd_queue, NULL);
			pt_nd_queue = NULL;
		}
		return tpret;
	
}

/**
 * @brief Dump the data item
 */
void
app_froto_to_dump_dtitem(struct data_item *pt_dtitem)
{
  if(NULL == pt_dtitem)
  {
    return;
  }
  DBG_LOG_DEBUG("===the following is the content of dtitem @ 0x%lx===",
               pt_dtitem);
  DBG_LOG_DEBUG("idx: %d name_id: %d sensor_name: %s", pt_dtitem->idx,
               pt_dtitem->name_id, pt_dtitem->sensor_name);
  DBG_LOG_DEBUG("sensor_mac: %s sensor_type: %d manufacturer: %s",
               pt_dtitem->sensor_mac, pt_dtitem->sensor_type,
               pt_dtitem->manufacturer);
  DBG_LOG_DEBUG("meas_id: %d meas_ts: %d received_ts: %d",
               pt_dtitem->meas_id, pt_dtitem->meas_ts,
               pt_dtitem->received_ts);
  DBG_LOG_DEBUG("alarm: %d data_type: %d format: %s ", pt_dtitem->alarm,
               pt_dtitem->data_type, pt_dtitem->format);
  DBG_LOG_DEBUG(
    "range: %d unit: %d meas_len_sample: %d total_dt_len_sample: %d",
    pt_dtitem->range, pt_dtitem->unit, pt_dtitem->measure_length_sample,
    pt_dtitem->total_data_length_sample);
  DBG_LOG_DEBUG("dimension: %d dt_format: %d samp_period: %d",
               pt_dtitem->dimension, pt_dtitem->data_format,
               pt_dtitem->sample_period_s);
  DBG_LOG_DEBUG("encrypt: %d", pt_dtitem->encryp);
  if(is_mtype_to_file(pt_dtitem->data_type))
  {  // data in file
    DBG_LOG_DEBUG("data_file: %s", pt_dtitem->value.fpath.fpath);
  }
  else
  {
    DBG_LOG_DEBUG("u64: %d u32: %d u16: %d u8: %d", pt_dtitem->value.valu64,
                 pt_dtitem->value.valu32, pt_dtitem->value.valu16,
                 pt_dtitem->value.valu8);
    util_dbg_buf_dump((uint8_t *)&pt_dtitem->value.byte_array,
                      sizeof(pt_dtitem->value.byte_array));
  }
  DBG_LOG_DEBUG("sample_rate: %f product: %d sensor: %d",
               pt_dtitem->sample_rate_hz, 
			   pt_dtitem->product_type,
               pt_dtitem->froto_sensor_type);
  return;
}


/*to pick data from data_uploading message
tp_root@pointer points to msg tree
ret@ST_OK is returned when things go well
*/

app_state_t app_froto_data_uploading_handler(const struct msg_tree_node*pt_root)
{
	app_state_t tpret = ST_OK;
	struct node_path kp_nd_path = {.path = {0}, .kpdepth = 0};
	struct l_queue *pt_nd_queue = l_queue_new();
	
	struct l_queue *pt_dtitem_queue = NULL;
	struct database_operation_controller *pt_db_op_ctr = NULL;
	
	struct l_queue_entry *pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL;
	struct itimerspec kp_its = {0};

	struct shortdata_xfer_ctr *pt_dtselect_ctr = app_froto_get_shortdata_xfer_timer_ctr();
	
	uint32_t kp_q_len = 0;
	
	uint32_t kp_acked_seq_no = 0;
	uint32_t kp_m_type = 0;
	uint32_t kp_total_blk = 0, kp_cur_blk = 0;
	uint32_t kp_crc32 = 0;

	struct data_item *pt_dtitem = NULL;
	struct db_op_msg *pt_op_msg = NULL;

	// to reset the data-selection timer, in case of timeout in the prcocess of data receivation
	app_froto_dtselect_reset_timer( pt_dtselect_ctr);
	
	
	if((NULL == pt_root)||(ND_ROOT != pt_root->role))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		goto EXIT;
	}

#if(1)
	// fill the node-path, to get the acked_message_seq_number
	kp_nd_path.path[0] = 2;
	kp_nd_path.path[1] = 1;
	kp_nd_path.path[2] = 18;
	kp_nd_path.path[3] = 1;
	kp_nd_path.kpdepth = 4;

	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to locate the info-node");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	kp_q_len = l_queue_length(pt_nd_queue);
	if(kp_q_len != 1)
	{
		DBG_LOG_ERR("unexpected queue length %d", kp_q_len);
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_acked_seq_no = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);
	DBG_LOG_INFO("the sequence-no is %d", kp_acked_seq_no);

	pthread_mutex_lock( &pt_dtselect_ctr->mtx);
	if((false == pt_dtselect_ctr->is_in_process)||(kp_acked_seq_no != pt_dtselect_ctr->msg_seq_no ))
	{
		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
		DBG_LOG_ERR("the seq-no does not match( %d vs %d) or the ctr is inactive(%d)", pt_dtselect_ctr->msg_seq_no, kp_acked_seq_no, pt_dtselect_ctr->is_in_process);
		tpret = ST_ERR;
		goto EXIT;		
	}
	pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
#endif	
	
	// to get the total blk info
	l_queue_destroy( pt_nd_queue, NULL);
	pt_nd_queue = l_queue_new();
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 2;
	kp_nd_path.path[1] = 1;
	kp_nd_path.path[2] = 11;
	kp_nd_path.kpdepth = 3;
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the total block node");
		tpret = ST_ERR;
		goto EXIT;
	}
	kp_q_len = l_queue_length( pt_nd_queue);
	if(kp_q_len != 1)
	{
		DBG_LOG_ERR("unexpected queue length %d", kp_q_len);
		tpret  = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_total_blk = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	// to get the current blk info
	l_queue_destroy( pt_nd_queue, NULL);
	pt_nd_queue = l_queue_new();
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 2;
	kp_nd_path.path[1] = 1;
	kp_nd_path.path[2] = 16;
	kp_nd_path.kpdepth = 3;
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_WARN("fail to find the current blk info");
		kp_cur_blk = 0;
	}
	else
	{
		kp_q_len = l_queue_length( pt_nd_queue);
		if(kp_q_len != 1)
		{
			DBG_LOG_WARN("unexpected queue length %d", kp_q_len);
			kp_cur_blk = 0;
		}
		else
		{
			pt_entry = l_queue_get_entries( pt_nd_queue);
			pt_node = (struct msg_tree_node*)pt_entry->data;
			kp_cur_blk = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);
		}
	}
	
	if(kp_cur_blk >= kp_total_blk)
	{
		DBG_LOG_ERR("invalid block info total_blk %d, cur_blk %d", kp_total_blk, kp_cur_blk);
		tpret = ST_ERR;
		goto EXIT;
	}

	//to find the data-pair
	l_queue_destroy( pt_nd_queue, NULL);
	pt_nd_queue = l_queue_new();
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 2;
	kp_nd_path.path[1] = 4;
	kp_nd_path.kpdepth = 2;
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the msg-node");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	kp_q_len = l_queue_length( pt_nd_queue);
	for(uint32_t i = 0; i < kp_q_len; i++)
	{
		pt_node = (struct msg_tree_node*)pt_entry->data;		
		pt_entry = pt_entry->next;
		tpret = to_pick_the_data(  pt_dtselect_ctr, pt_node);
		if(tpret != ST_OK)
		{
			DBG_LOG_ERR("Fail to pick data from the msg branch");
			break;
		}
	}

	DBG_LOG_INFO("total block %d , cur_blk %d", kp_total_blk, kp_cur_blk);
	
	if(kp_total_blk <= (kp_cur_blk + 1))
	{ // all data from peer-device is received, to release the controller
		DBG_LOG_INFO("All data requested is received successfully");
		pthread_mutex_lock( &pt_dtselect_ctr->mtx);
		if(false == pt_dtselect_ctr->is_in_process)
		{
			pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
			tpret = ST_ERR;
			goto EXIT;
		}
		if(timer_settime(pt_dtselect_ctr->timer_id, 0, &kp_its, NULL) < 0)
		{
			DBG_LOG_ERR("fail to disarm the timer");
		}

		pt_dtselect_ctr->is_in_process = false;
		pt_dtselect_ctr->msg_seq_no = 0;
		// to close the data file if the data is written to file
		if(1 == l_queue_length( pt_dtselect_ctr->pt_dtrequest_queue))
		{
			pt_entry = l_queue_get_entries( pt_dtselect_ctr->pt_dtrequest_queue);
			pt_dtitem = (struct data_item*)pt_entry->data;	
			DBG_LOG_INFO("data_item info dt_type %d, fd %d, fpath %s", pt_dtitem->data_type, pt_dtitem->value.fpath.fd, pt_dtitem->value.fpath.fpath);	
			if(is_mtype_to_file( pt_dtitem->data_type) && (pt_dtitem->value.fpath.fd >=0))
			{
				DBG_LOG_INFO("the sensor data file is closed successfully");
				close( pt_dtitem->value.fpath.fd);
			}
		}

		#if(1)
			/*
			pt_entry = l_queue_get_entries(pt_dtselect_ctr->pt_dtrequest_queue);
			while(pt_entry)
			{
				to_dump_dtitem((struct data_item *)pt_entry->data);
				pt_entry = pt_entry->next;
			}*/
			pt_dtitem_queue = l_queue_new();
			while(1)
			{
				pt_dtitem = (struct data_item*)l_queue_pop_head( pt_dtselect_ctr->pt_dtrequest_queue);
				if(NULL == pt_dtitem)
				{
					break;
				}
				l_queue_push_tail( pt_dtitem_queue, (void *)pt_dtitem);
			}
		#endif
		/* you have to save data to database before this line */
		//l_queue_clear( pt_dtselect_ctr->pt_dtrequest_queue, l_free); // to release the data item

		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);



		//WE DO NOT try to lock another mutex when a mutex is loced;to sign the thread to update data to database
		DBG_LOG_INFO("to trig the thread to update sensor data to database");
		pt_db_op_ctr = app_db_get_database_op_ctr();
		pthread_mutex_lock( &pt_db_op_ctr->mtx);
		while(1)
		{
			pt_dtitem = (struct data_item*)l_queue_pop_head( pt_dtitem_queue);
			if(NULL == pt_dtitem)
			{
				break;
			}
			//app_froto_to_dump_dtitem( pt_dtitem); // for debug only
			pt_op_msg = (struct db_op_msg*)l_malloc(sizeof(struct db_op_msg));
			if(pt_op_msg)
			{
				memset( pt_op_msg, 0, sizeof(struct db_op_msg));
				pt_op_msg->cmd = gw_pro_command_sqlite_update_item;
				pt_op_msg->table_idx = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE; // to update the sensor-data table
				pt_op_msg->cmd_para.pt_dtitem = pt_dtitem;
				l_queue_push_tail( pt_db_op_ctr->pt_dtitem_queue, (void *)pt_op_msg);
			}
			else
			{//when this happens, the data item is abandoned
				DBG_LOG_ERR("Critical error! l_malloc fail.");
				l_free( pt_dtitem);	
			}
		}
		DBG_LOG_INFO("to signal data available");
		pthread_cond_signal( &pt_db_op_ctr->cond_dt_available);
		
		pthread_mutex_unlock( &pt_db_op_ctr->mtx);
		
	}
	
	EXIT:
		if(pt_nd_queue)
		{
			l_queue_destroy( pt_nd_queue, NULL);
		}
		if(pt_dtitem)
		{
			l_queue_destroy( pt_dtitem_queue, NULL);
		}
		return tpret;
}

#else
/*to pick data from data_uploading message
tp_root@pointer points to msg tree
ret@ST_OK is returned when things go well
*/

app_state_t app_froto_data_uploading_handler(const struct msg_tree_node*pt_root)
{
	app_state_t tpret = ST_OK;
	struct node_path kp_nd_path = {.path = {0}, .kpdepth = 0};
	struct l_queue *pt_nd_queue = l_queue_new();
	struct l_queue_entry *pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL;
	struct itimerspec kp_its = {0};

	struct shortdata_xfer_ctr *pt_dtselect_ctr = app_froto_get_shortdata_xfer_timer_ctr();
	
	uint32_t kp_q_len = 0;
	
	uint32_t kp_seq_no = 0;
	uint32_t kp_m_type = 0;
	uint32_t kp_total_blk = 0, kp_cur_blk = 0;
	uint32_t kp_crc32 = 0;
	
	if((NULL == pt_root)||(ND_ROOT != pt_root->role))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		goto EXIT;
	}

	// fill the node-path, get the sequence-no
	kp_nd_path.path[0] = 2;
	kp_nd_path.path[1] = 1;
	kp_nd_path.path[2] = 7;
	kp_nd_path.kpdepth = 3;

	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to locate the info-node");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	kp_q_len = l_queue_length(pt_nd_queue);
	if(kp_q_len != 1)
	{
		DBG_LOG_ERR("unexpected queue length %d", kp_q_len);
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_seq_no = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);
	DBG_LOG_INFO("the sequence-no is %d", kp_seq_no);

	// to get the total blk info
	l_queue_destroy( pt_nd_queue, NULL);
	pt_nd_queue = l_queue_new();
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 2;
	kp_nd_path.path[1] = 1;
	kp_nd_path.path[2] = 11;
	kp_nd_path.kpdepth = 3;
	if(false == util_froto_locate_msg_node_v2( pt_node, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the total block node");
		tpret = ST_ERR;
		goto EXIT;
	}
	kp_q_len = l_queue_length( pt_nd_queue);
	if(kp_q_len != 1)
	{
		DBG_LOG_ERR("unexpected queue length %d", kp_q_len);
		tpret  = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_total_blk = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	// to get the current blk info
	l_queue_destroy( pt_nd_queue, NULL);
	pt_nd_queue = l_queue_new();
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 2;
	kp_nd_path.path[1] = 1;
	kp_nd_path.path[2] = 16;
	kp_nd_path.kpdepth = 3;
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the current blk info");
		tpret = ST_ERR;
		goto EXIT;
	}
	kp_q_len = l_queue_length( pt_nd_queue);
	if(kp_q_len != 1)
	{
		DBG_LOG_ERR("unexpected queue length %d", kp_q_len);
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_cur_blk = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	if(kp_cur_blk >= kp_total_blk)
	{
		DBG_LOG_ERR("invalid block info total_blk %d, cur_blk %d", kp_total_blk, kp_cur_blk);
		tpret = ST_ERR;
		goto EXIT;
	}
	
	// to check if the measurement-type match
	if(kp_cur_blk == 0)
	{// only the fist pkt has the measurement-info
		l_queue_destroy( pt_nd_queue, NULL);
		pt_nd_queue = l_queue_new();
		// fill the node-path, get the measurement type
		kp_nd_path.path[0] = 2;
		kp_nd_path.path[1] = 4;
		kp_nd_path.path[2] = 1;
		kp_nd_path.path[3] = 1;
		kp_nd_path.kpdepth = 4;
		if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
		{
			DBG_LOG_ERR("fail to find the mea-type info");
			tpret = ST_ERR;
			goto EXIT;
		}
		kp_q_len = l_queue_length( pt_nd_queue);
		if(kp_q_len != 1)
		{
			DBG_LOG_ERR("invalid queue length %d", kp_q_len);
			tpret = ST_ERR;
			goto EXIT;
		}
		pt_entry = l_queue_get_entries( pt_nd_queue);
		pt_node = (struct msg_tree_node*)pt_entry->data;
		kp_m_type = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);
	}

	// to check if the data is valid
	pthread_mutex_lock( &pt_dtselect_ctr->mtx);
	if(false == pt_dtselect_ctr->is_in_process)
	{
		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
		DBG_LOG_ERR("the data-selection controller is inactive");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(kp_seq_no != (pt_dtselect_ctr->msg_seq_no + 1))
	{
		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
		DBG_LOG_ERR("the message sequence no does not match!");
		tpret = ST_ERR;
		goto EXIT;
	}
	if((kp_cur_blk == 0) && (kp_m_type != pt_dtselect_ctr->meas_type))
	{		
		pthread_mutex_unlock( &pt_dtselect_ctr->mtx);
		DBG_LOG_ERR("the measurement type does not match!");
		tpret = ST_ERR;
		goto EXIT;
	}
	// get the correct  response
	if(kp_total_blk == (kp_cur_blk + 1))
	{// all response pkt is received, to releae the controller
		if(timer_settime( pt_dtselect_ctr->timer_id, 0, &kp_its, NULL) < 0)
		{
			DBG_LOG_ERR("fail to disarm the timer");
		}

		pt_dtselect_ctr->is_in_process = false;
		pt_dtselect_ctr->meas_type = 0;
		pt_dtselect_ctr->msg_seq_no = 0;
	}
	pthread_mutex_unlock( &pt_dtselect_ctr->mtx);

	// to pick the CRC32
	l_queue_destroy( pt_nd_queue, NULL);
	pt_nd_queue = l_queue_new();
	memset(&kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 2;
	kp_nd_path.path[1]= 4;
	kp_nd_path.path[2]= 2;
	kp_nd_path.path[3] = 3;
	kp_nd_path.kpdepth = 4;
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the crc32_node");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_crc32 = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);


	// to pick the data node
	l_queue_destroy( pt_nd_queue, NULL);
	pt_nd_queue = l_queue_new();
	memset(&kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 2;
	kp_nd_path.path[1]= 4;
	kp_nd_path.path[2]= 2;
	kp_nd_path.path[3] = 2;
	kp_nd_path.kpdepth = 4;
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("fail to find the data-node");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;

	//to save the sensor data
	to_save_sensor_data( kp_m_type,  pt_node, kp_crc32);

	EXIT:
		if(pt_nd_queue)
		{
			l_queue_destroy( pt_nd_queue, NULL);
		}
		return tpret;
}

#endif

#endif



#if(1)// for FUOTA; to disseminate IMG to sensors

#if(0)//ref@util_froto.h

/*data structure definition for image disseminate control
*/
struct longdata_xfer_ctr{
	pthread_mutex_t mtx;
	bool is_in_process;
	uint32_t img_ver; // the image version info
	int fd; // the file descriptor of image file when it is opened for transportation
	uint8_t img_fpath[MAX_FPATH]; // the file path of image file
	uint32_t fsize; // the file size
	uint32_t total_blk; // ref@FILE_PKT_SIZE
	int32_t dtlen; // the length of data in buffer bellow
	uint8_t pkt_buf[FILE_PKT_SIZE]; // we put the last data into this buffer, in case of re-transportation when error happens
	uint32_t retry_cnt; // ref@MAX_FTRANS_RETRY
	
	
	uint32_t last_seq_no;
	uint32_t cur_blk_idx;

	uint32_t f_tsk_id;// the task ID of the file(configuration or image) transportation
	
	timer_t timer_id; //timer , in case of timerout
};

#endif


/* to retrive the sensor firmeare version
pt_sensor_op_ctr@the sensor operation controller
pt_ver_buf @ the version info is returned by this buffer
*/
app_state_t app_froto_retrieve_sensor_fw_version_info(struct sensor_op_controller*pt_sensor_op_ctr, uint32_t *pt_ver_buf)
{
	app_state_t tpret = ST_OK;
	int hld_fd = -1;	
	
	struct encoded_froto_msg_pkt kp_encoded_msg = {0};
	struct ipc_msg_v2 kp_ipc_msg = {0};
	uint32_t kp_msg_seq_no = app_pro_gen_allocate_msg_seq_no();

	struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;
	
	if((NULL == pt_sensor_op_ctr)||(NULL == pt_ver_buf))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	// send msg to retrieve the IMG version info
	kp_encoded_msg = util_froto_fill_msg_version_retrieve( app_pro_gen_get_gateway_idstr( app_pro_gen_get_ctr()), kp_msg_seq_no);
	if(false == kp_encoded_msg.is_valid)
	{
		DBG_LOG_ERR("fail to get encoded msg for version retrieving");
		tpret = ST_ERR;
		goto ERR;	
	}

	kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
	memset( kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
	kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
	memcpy( kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf, kp_ipc_msg.mtext.proto_pkt.len);

	// to releae the encoded msg
	util_froto_release_encoded_msg_v2( &kp_encoded_msg);

	//to create an waiting object
	pt_sensor_op_waiting_obj = (struct sensor_op_waiting*)l_malloc(sizeof(struct sensor_op_waiting));
	if(NULL == pt_sensor_op_waiting_obj)
	{		
		DBG_LOG_ERR("malloc fail");
		tpret = ST_ERR;
		goto ERR;
	}
	memset( pt_sensor_op_waiting_obj, 0, sizeof(struct sensor_op_waiting));
	app_sch_init_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, kp_msg_seq_no, SENSOR_OP_FW_VERSION_RETRIEVE);
	app_sch_set_timval_to_sensor_op_waiting_obj( pt_sensor_op_waiting_obj, time(NULL));
	// to put the waiting obj on waiting queue
	app_sch_put_sensor_op_waiting_obj_on_queue( pt_sensor_op_ctr, pt_sensor_op_waiting_obj);
	
	// to put the message on the waiting queue
	if(ST_OK != app_wq_post_to_waiting_queue( app_pro_gen_get_wq_ipc_msg_ctr(), (uint8_t *)&kp_ipc_msg, sizeof(struct ipc_msg_v2)))
	{
		tpret = ST_ERR;
		goto ERR;
	}

#if (1)
	struct timespec tp_tspec = {0};
#endif
	//to wait for response from sensor
	pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx);
#if (1)
	clock_gettime(CLOCK_REALTIME, &tp_tspec);
	tp_tspec.tv_sec = tp_tspec.tv_sec + CONFIG_SENSOR_OP_FW_VERSION_RETRIEVE_TIMEOUT; // to be confirmed.
#endif
	while(1)
	{
		if(EV_SENSOR_OP_DEFAULT == pt_sensor_op_waiting_obj->resp_event)
		{
#if (1)
			int tpint = 0;
			tpint = pthread_cond_timedwait( &pt_sensor_op_waiting_obj->cond_resp_received, &pt_sensor_op_waiting_obj->mtx, &tp_tspec);
			if((0 != tpint)&&(ETIMEDOUT == tpint))
			{
				DBG_LOG_ERR("===wait sensor op timeout");
				break;						
			}
#else
			pthread_cond_wait( &pt_sensor_op_waiting_obj->cond_resp_received, &pt_sensor_op_waiting_obj->mtx);
#endif
		}
		else
		{
			break;
		}
	}
	pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);

	if(EV_SENSOR_OP_RESP_RECV != pt_sensor_op_waiting_obj->resp_event)
	{
		tpret = ST_ERR;
		DBG_LOG_ERR("retrieve senor fw version failed");
		goto ERR;
	}

	*pt_ver_buf = pt_sensor_op_waiting_obj->sensor_op_resp.ver_info.ver;
	
	// to release the waiting obj
	app_sch_deinit_sensor_op_waiting_obj( pt_sensor_op_waiting_obj);
	l_free( pt_sensor_op_waiting_obj);
	pt_sensor_op_waiting_obj = NULL;
	
	return tpret;
	ERR:
		if(hld_fd >= 0)
		{ // close the file opened
			close(hld_fd);
		}
		if(pt_sensor_op_waiting_obj)			
		{
			app_sch_deinit_sensor_op_waiting_obj( pt_sensor_op_waiting_obj);
			l_free( pt_sensor_op_waiting_obj);
			pt_sensor_op_waiting_obj = NULL;
		}
		return tpret;
}




/*SKFChina_App_AppMessage_image_block_request_tag
*/
static app_state_t msg_handler_for_image_block_request(const struct msg_tree_node*pt_root, struct longdata_xfer_ctr*pt_img_dissem_ctr)
{
	app_state_t tpret = ST_ERR;
	struct node_path kp_nd_path = {0};
	uint32_t kp_acked_seq_no = 0, kp_fuota_tsk_id = 0, kp_request_size = 0;
	struct l_queue *pt_nd_queue = l_queue_new();
	struct l_queue_entry*pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL;	
	struct itimerspec kp_its = {0};

	// to find the acked_seq_no
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 15;
	kp_nd_path.path[1] = 1;
	kp_nd_path.path[2] = 18;
	kp_nd_path.path[3] = 1;
	kp_nd_path.kpdepth = 4;
	l_queue_clear( pt_nd_queue, NULL);
	if((false ==  util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue)))
	{
		DBG_LOG_ERR("fail to locate the acked_seq_number");
		tpret = ST_ERR;
		goto EXIT;
	}
	if((l_queue_length( pt_nd_queue) <= 0))
	{
		DBG_LOG_ERR("no acked_seq_number is found");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_acked_seq_no = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	// to get the task_id
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 15;
	kp_nd_path.path[1] = 7;
	kp_nd_path.kpdepth = 2;
	l_queue_clear( pt_nd_queue, NULL);
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("locate_msg_node_v2 fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queue) <= 0)
	{
		DBG_LOG_ERR("no node is found");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_fuota_tsk_id = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	// to get the request size
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 15;
	kp_nd_path.path[1] = 5;
	kp_nd_path.kpdepth = 2;
	l_queue_clear( pt_nd_queue, NULL);
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("locate_msg_node_v2 fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queue) <= 0)
	{
		DBG_LOG_ERR("no node is found");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_request_size = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);


	// to validate the msg
	pthread_mutex_lock( &pt_img_dissem_ctr->mtx);
	
	if(false == pt_img_dissem_ctr->is_in_process)
	{		
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
		DBG_LOG_ERR("the controller is inactive now, probably for timeout");
		tpret = ST_ERR;
		goto EXIT;
	}
	
	if(pt_img_dissem_ctr->f_tsk_id != kp_fuota_tsk_id)
	{	
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
		DBG_LOG_ERR("Well the FUOTA task ID does not match, %d is expected", pt_img_dissem_ctr->f_tsk_id);
		tpret = ST_ERR;
		goto EXIT;
	}
	if(pt_img_dissem_ctr->last_seq_no != kp_acked_seq_no)
	{	
		pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
		DBG_LOG_ERR("the msg seq_no does not match, %d is expected", pt_img_dissem_ctr->last_seq_no);
		tpret = ST_ERR;
		goto EXIT;
	}
	if(0 == kp_request_size)
	{
		// to check if all file is sent
		if(pt_img_dissem_ctr->total_blk <= (pt_img_dissem_ctr->cur_blk_idx))
		{// all blocks are sent successfully, to release the controller
			pt_img_dissem_ctr->is_in_process = false;
			pt_img_dissem_ctr->img_ver = 0;
			if(pt_img_dissem_ctr->fd >= 0)
			{
				close(pt_img_dissem_ctr->fd);
				pt_img_dissem_ctr->fd = -1;
			}
			memset( pt_img_dissem_ctr->img_fpath, 0, MAX_FPATH);
			pt_img_dissem_ctr->fsize = 0;
			pt_img_dissem_ctr->total_blk = 0;
			pt_img_dissem_ctr->dtlen = 0;
			memset( pt_img_dissem_ctr->pkt_buf, 0, FILE_PKT_SIZE);
			pt_img_dissem_ctr->retry_cnt = 0;
			pt_img_dissem_ctr->last_seq_no = 0;
			pt_img_dissem_ctr->cur_blk_idx = 0;
			pt_img_dissem_ctr->f_tsk_id = 0;
			// to disarm the timer
			timer_settime(pt_img_dissem_ctr->timer_id, 0, &kp_its, NULL);

			// to unlock		
			pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
			DBG_LOG_ERR("retrying-counter reach the limitation");
			tpret = ST_ERR;
			goto EXIT;
		}
		
		// continue to read the next packet from the file
		if(pt_img_dissem_ctr->fd < 0)
		{		
			pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
			DBG_LOG_ERR("well , invalid fd");
			tpret = ST_ERR;
			goto EXIT;
		}
		//to read the file
		memset( pt_img_dissem_ctr->pkt_buf, 0, FILE_PKT_SIZE);
		pt_img_dissem_ctr->dtlen = read( pt_img_dissem_ctr->fd, pt_img_dissem_ctr->pkt_buf, FILE_PKT_SIZE);
		if(pt_img_dissem_ctr->dtlen < 0)
		{		
			pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
			DBG_LOG_ERR("fail to read from file, for %s", strerror(errno));
			tpret = ST_ERR;
			goto EXIT;
		}

		pt_img_dissem_ctr->retry_cnt = 0; // reset the retrying-counter
	}
	else
	{
		pt_img_dissem_ctr->retry_cnt++;
		if(pt_img_dissem_ctr->retry_cnt > MAX_FTRANS_RETRY)
		{ // well we hit the point, to release the controller
			pt_img_dissem_ctr->is_in_process = false;
			pt_img_dissem_ctr->img_ver = 0;
			if(pt_img_dissem_ctr->fd >= 0)
			{
				close(pt_img_dissem_ctr->fd);
				pt_img_dissem_ctr->fd = -1;
			}
			memset( pt_img_dissem_ctr->img_fpath, 0, MAX_FPATH);
			pt_img_dissem_ctr->fsize = 0;
			pt_img_dissem_ctr->total_blk = 0;
			pt_img_dissem_ctr->dtlen = 0;
			memset( pt_img_dissem_ctr->pkt_buf, 0, FILE_PKT_SIZE);
			pt_img_dissem_ctr->retry_cnt = 0;
			pt_img_dissem_ctr->last_seq_no = 0;
			pt_img_dissem_ctr->cur_blk_idx = 0;
			pt_img_dissem_ctr->f_tsk_id = 0;
			// to disarm the timer
			timer_settime(pt_img_dissem_ctr->timer_id, 0, &kp_its, NULL);

			// to unlock		
			pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);
			DBG_LOG_ERR("retrying-counter reach the limitation");
			tpret = ST_ERR;
			goto EXIT;
		}
	}
	pthread_mutex_unlock( &pt_img_dissem_ctr->mtx);

	tpret = image_block_dissem(pt_root, pt_img_dissem_ctr);
	DBG_LOG_INFO("the ret of image_block_dissem %d", tpret);

	
	EXIT:
		if(pt_nd_queue)
		{
			l_queue_destroy( pt_nd_queue, NULL);
		}
		return tpret;
}


#if(1)
/*
typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
*/ 
static bool queue_match_to_check_sensor_op_code(const void *a, const void *b)
{
	bool tpret = false;
	struct sensor_op_waiting *pt_sensor_op_waiting_obj = (struct sensor_op_waiting*)a;
	enum sensor_op_code kp_op_code = (enum sensor_op_code)b;
	if(NULL == pt_sensor_op_waiting_obj)
	{
		tpret = false;
		return tpret;
	}
	if(pt_sensor_op_waiting_obj->op_code == kp_op_code)
	{
		tpret = true;
	}
	return tpret;
}


/*SKFChina_App_AppMessage_image_block_request_tag
to check if the image dissemination is done successfullt
*/
static app_state_t msg_handler_for_image_block_request_v2(const struct msg_tree_node*pt_root, struct sensor_op_controller *pt_sensor_op_ctr)
{
	app_state_t tpret = ST_ERR;
	struct node_path kp_nd_path = {0};
	uint32_t kp_acked_seq_no = 0, kp_fuota_tsk_id = 0, kp_request_size = 0;
	struct l_queue *pt_nd_queue = l_queue_new();
	struct l_queue_entry*pt_entry = NULL;
	struct msg_tree_node *pt_node = NULL;	

	//struct l_queue_entry *pt_entry = NULL;
	struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;


	// to find the acked_seq_no
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 15;
	kp_nd_path.path[1] = 1;
	kp_nd_path.path[2] = 18;
	kp_nd_path.path[3] = 1;
	kp_nd_path.kpdepth = 4;
	l_queue_clear( pt_nd_queue, NULL);
	if((false ==  util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue)))
	{
		DBG_LOG_ERR("fail to locate the acked_seq_number");
		tpret = ST_ERR;
		goto EXIT;
	}
	if((l_queue_length( pt_nd_queue) <= 0))
	{
		DBG_LOG_ERR("no acked_seq_number is found");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_acked_seq_no = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	// to get the task_id
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 15;
	kp_nd_path.path[1] = 7;
	kp_nd_path.kpdepth = 2;
	l_queue_clear( pt_nd_queue, NULL);
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("locate_msg_node_v2 fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queue) <= 0)
	{
		DBG_LOG_ERR("no node is found");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_entry = l_queue_get_entries( pt_nd_queue);
	pt_node = (struct msg_tree_node*)pt_entry->data;
	kp_fuota_tsk_id = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);

	// to get the request size
	memset( &kp_nd_path, 0, sizeof(struct node_path));
	kp_nd_path.path[0] = 15;
	kp_nd_path.path[1] = 5;
	kp_nd_path.kpdepth = 2;
	l_queue_clear( pt_nd_queue, NULL);
	if(false == util_froto_locate_msg_node_v2( pt_root, kp_nd_path, pt_nd_queue))
	{
		DBG_LOG_ERR("locate_msg_node_v2 fail");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(l_queue_length( pt_nd_queue) >= 1)
	{
		pt_entry = l_queue_get_entries( pt_nd_queue);
		pt_node = (struct msg_tree_node*)pt_entry->data;
		kp_request_size = *((uint32_t*)pt_node->nd_data.dtinfo.dtbuf);
	}
	else
	{
		kp_request_size = 0;
	}
	


	#if(1) 
	/*ref@app_sch_signal_and_pop_sensor_op_waiting_obj ; 
	*/
	// to remove the waiting object on queue
		pthread_mutex_lock( &pt_sensor_op_ctr->mtx);
		pt_sensor_op_waiting_obj = l_queue_remove_if( pt_sensor_op_ctr->pt_sensor_op_waiting_queue, queue_match_to_check_sensor_op_code, (const void *)SENSOR_OP_IMG_DISSEM);
		if(pt_sensor_op_waiting_obj)
		{
			pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx);
			if(kp_request_size)
			{ // image dissemination fail
				pt_sensor_op_waiting_obj->sensor_op_resp.fw_dissem_resp.is_successful = false;
			}
			else
			{
				pt_sensor_op_waiting_obj->sensor_op_resp.fw_dissem_resp.is_successful = true;
			}
			
			pt_sensor_op_waiting_obj->resp_event = EV_SENSOR_OP_RESP_RECV;
			pthread_cond_signal( &pt_sensor_op_waiting_obj->cond_resp_received);
			pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);
		}
		else
		{
			DBG_LOG_WARN("no waiting obj for image dissemination on queue");
		}
		pthread_mutex_unlock( &pt_sensor_op_ctr->mtx);
	#endif

	EXIT:
		if(pt_nd_queue)
		{
			l_queue_destroy( pt_nd_queue, NULL);
		}
		return tpret;
}

#endif	

#endif



#if(0)

// to 


//for configuration dissemination
enum sensor_conf_sync_event{
	SYNC_EV_UNDEFINED = 0,// undefined
	SYNC_EV_DBREAD_SUCCESS = 1, //event database is read
	SYNC_EV_DB_READ_FAIL= 2, // database read fail
	SYNC_EV_SYNC_SUCCESS = 3, //  synchronize successfully
	SYNC_EV_SYNC_FAIL = 4, // synchronize fail
	SYNC_EV_TIMEOUT = 5, // operation time out, no response from DB or no response from sensor
};

/*data struct definition to control the sychronization of sensor configuration
*/
struct sensor_conf_sync_controller{
	pthread_mutex_t mtx;
	pthread_cond_t cond;
	bool is_active; // true when the controller is active
	enum sensor_conf_sync_event sync_event;
	sensorConfig_t sensor_conf; // the sensor configuration 
};

static struct sensor_conf_sync_controller g_sensor_conf_sync_ctr = {0};

/*to init controller
*/
app_state_t app_froto_init_sensor_conf_sync_ctr(struct sensor_conf_sync_controller*pt_sensor_conf_sync_ctr)
{
	return ST_ERR;
}

/*deinit controller*/
app_state_t app_froto_deinit_sensor_conf_sync_ctr(struct sensor_conf_sync_controller*pt_sensor_conf_sync_ctr)
{
	return ST_ERR;
}


/*to load sensor configuration
NOTE!!! to be called to load the sensor configuration when no record in DB related to the sensor
*/
app_state_t app_froto_load_sensor_conf(struct sensor_conf_sync_controller*pt_sensor_conf_sync_ctr, )
{

}

/*try to sychronize the sensor configuration
*/
app_state_t app_froto_sensor_conf_sync(struct sensor_conf_sync_controller*pt_sensor_conf_sync_ctr)
{
	app_state_t tpret = ST_ERR;

	// to read 

}
#endif

#if 0 // Deprecated
/**
 * To trigger an image dissemination sequence
 */
app_state_t
app_froto_trig_img_dissemination(
  struct longdata_xfer_ctr *pt_img_dissem_ctr,
  const uint8_t *pt_img_fpath,
  const uint32_t kp_verinfo)
{
  app_state_t tpret = ST_OK;
  int hld_fd = -1;
  struct itimerspec kp_its = { 0 };
  struct encoded_froto_msg_pkt kp_encoded_msg = { 0 };
  struct ipc_msg_v2 kp_ipc_msg = { 0 };
  uint32_t kp_msg_seq_no = app_pro_gen_allocate_msg_seq_no();

  if((NULL == pt_img_dissem_ctr) || (NULL == pt_img_fpath))
  {
    DBG_LOG_ERR("unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

  hld_fd = open(pt_img_fpath, O_RDONLY);
  if(hld_fd < 0)
  {
    DBG_LOG_ERR(
      "fail to open file %s, make sure you have the permission or the file exist",
      pt_img_fpath);
    tpret = ST_ERR;
    return tpret;
  }

  kp_its.it_value.tv_sec = 420;
  kp_its.it_value.tv_nsec = 0;
  kp_its.it_interval.tv_sec = 0;
  kp_its.it_interval.tv_nsec = 0;

  pthread_mutex_lock(&pt_img_dissem_ctr->mtx);
  if(true == pt_img_dissem_ctr->is_in_process)
  {
    pthread_mutex_unlock(&pt_img_dissem_ctr->mtx);

    DBG_LOG_ERR("the img dissemination is in process");
    tpret = ST_BUSSY;
    goto ERR;
  }

  pt_img_dissem_ctr->fd = hld_fd;
  memset(pt_img_dissem_ctr->img_fpath, 0, MAX_FPATH);
  snprintf(pt_img_dissem_ctr->img_fpath, MAX_FPATH, "%s", pt_img_fpath);
  pt_img_dissem_ctr->fsize = lseek(hld_fd, 0, SEEK_END);
  if(pt_img_dissem_ctr->fsize < 0)
  {
    pthread_mutex_unlock(&pt_img_dissem_ctr->mtx);
    DBG_LOG_ERR("lseek fail, for %s", strerror(errno));
    tpret = ST_ERR;
    goto ERR;
  }
  lseek(pt_img_dissem_ctr->fd, 0, SEEK_SET);  // reset the file pointer
  pt_img_dissem_ctr->total_blk = pt_img_dissem_ctr->fsize / FILE_PKT_SIZE;
  if(pt_img_dissem_ctr->fsize % FILE_PKT_SIZE)
  {
    pt_img_dissem_ctr->total_blk++;
  }
  pt_img_dissem_ctr->cur_blk_idx = 0;
  pt_img_dissem_ctr->last_seq_no = 0;
  pt_img_dissem_ctr->f_tsk_id = kp_msg_seq_no;

  // to set up the timer
  if(timer_settime(pt_img_dissem_ctr->timer_id, 0, &kp_its, NULL) < 0)
  {
    pthread_mutex_unlock(&pt_img_dissem_ctr->mtx);
    DBG_LOG_ERR("fail to set up timer");
    tpret = ST_ERR;
    goto ERR;
  }
  pt_img_dissem_ctr->is_in_process = true;  // set the controller to active
  pthread_mutex_unlock(&pt_img_dissem_ctr->mtx);

  // send msg to retrieve the IMG version info
  kp_encoded_msg = util_froto_fill_msg_version_retrieve(app_pro_gen_get_gateway_idstr(
                                                          app_pro_gen_get_ctr()),
                                                        kp_msg_seq_no);
  if(false == kp_encoded_msg.is_valid)
  {
    DBG_LOG_ERR("fail to get encoded msg for version retrieving");
    tpret = ST_ERR;
    goto ERR;
  }

  kp_ipc_msg.mtype = M_TYPE_PROTO_DATA;
  memset(kp_ipc_msg.mtext.proto_pkt.dtbuf, 0, MAX_IPC_MSG_PROTO_DATA);
  kp_ipc_msg.mtext.proto_pkt.len = kp_encoded_msg.len;
  memcpy(kp_ipc_msg.mtext.proto_pkt.dtbuf, kp_encoded_msg.ptbuf,
         kp_ipc_msg.mtext.proto_pkt.len);

  // to releae the encoded msg
  util_froto_release_encoded_msg_v2(&kp_encoded_msg);

  // to put the message on the waiting queue
  if(ST_OK !=
     app_wq_post_to_waiting_queue(app_pro_gen_get_wq_ipc_msg_ctr(),
                                  (uint8_t *)&kp_ipc_msg,
                                  sizeof(struct ipc_msg_v2)))
  {
    tpret = ST_ERR;
    goto ERR;
  }

  return tpret;
ERR:
  if(hld_fd >= 0)
  {  // close the file opened
    close(hld_fd);
  }
  return tpret;
}
/*to save sensor data
   pt_node@the data node
   kp_crc32@the CRC32 value of data
   ret@app_state_t
 */
static app_state_t
to_save_sensor_data(
  const SKFChina_Common_MeasurementType kp_mtype,
  const struct msg_tree_node *pt_node,
  const uint32_t kp_crc32)
{
  app_state_t tpret = ST_OK;

  if(NULL == pt_node)
  {
    DBG_LOG_ERR("unexpected NULL");
    tpret = ST_ERR;
    return tpret;
  }

#warning "TODO ==== to check the CRC32 here"

  util_dbg_buf_dump(pt_node->nd_data.dtinfo.dtbuf,
                    pt_node->nd_data.dtinfo.size);  // only for debug

  switch(kp_mtype)
  {
    case SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_REMAINING_VOLUME:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_RUN_CODE:
    {
      break;
    }
    case
      SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_CONDITON_CODE:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_MAG_PRE:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MAX:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_START:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_END:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR:
    {
      break;
    }
    case SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT:
    {
      break;
    }
    default:
    {
      DBG_LOG_ERR("unexpected measurement type %d", kp_mtype);
      tpret = ST_ERR;
      break;
    }
  }

  return tpret;
}
#endif
