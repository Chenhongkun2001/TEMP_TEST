/*
*/
#include "app_mslave.h"
#include "util_dbg.h"
#include "errno.h"
#include "ell/util.h"
#include "util_tool.h"
#include "app_modbus_common.h"
#include "app_mslave_ipc.h"

#include "sh_mem.h"
#include "pthread.h"
#include <sched.h>
#include "cJSON.h"

DBG_LOCAL_LOG_DEBUG


struct modbus_slave_controller  g_mslave_ctr;


/*
*/
struct modbus_slave_controller *app_mslave_get_ctr(void)
{
	return &g_mslave_ctr;
}


/*to set port-configuration to default
ret@void
*/
static void to_set_port_conf_default(struct port_conf_info *pt_port_conf)
{
	if(NULL == pt_port_conf)
	{
		DBG_LOG_ERR("unexpected NULL pointer");
		return;
	}

	pt_port_conf->baudrate = 115200U;
	pt_port_conf->parity = PARITY_NONE;
	pt_port_conf->stopbit = SBIT_1;
	
	pt_port_conf->slaveID = 1; // the default Slave ID
	pt_port_conf->reg_md = REG_MD_EXTENSIBLE;
}

/*fun@to initialize the modbus slave controller
ret@ error code
*/
enum error_code app_mslave_init_ctr(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	int tpint = 0;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		goto ERR;
	}

	// slave mode
	pt_mslave_ctr->smode = SMODE_ACTIVE;

	// init the PORT-conf
	memset( &pt_mslave_ctr->port_conf, 0, sizeof(struct port_conf_info));
	to_set_port_conf_default( &pt_mslave_ctr->port_conf);
	
	tpint = pthread_mutex_init(&pt_mslave_ctr->mtx_for_port_conf, NULL);
	if(0 != tpint)
	{
		DBG_LOG_ERR("fail to init mtx, for %s", strerror(tpint));
		tpret = ERR_API_FAIL;
		goto ERR;
	}

	//
	pt_mslave_ctr->mctx = NULL; 

	//to set the default byte-order
	pt_mslave_ctr->mbus_border = MBUS_BORDER_DEFAULT;

	// to cteate the mapping queue
	pt_mslave_ctr->mapping_queue = l_queue_new();
	tpint = pthread_mutex_init( &pt_mslave_ctr->mtx_for_mapping_queue, NULL);
	if(0 != tpint)
	{
		DBG_LOG_ERR("fail to init mutex, for %s", strerror(tpint));
		tpret = ERR_API_FAIL;
		goto ERR;
	}
	
	// to initialize the port configuration information
	//pt_mslave_ctr->port_info;


	// to initialize the comm counters/log/registers
	memset( &pt_mslave_ctr->state_set, 0, sizeof(struct counter_log_register_set));
	tpint = pthread_mutex_init( &pt_mslave_ctr->mtx_for_state_set, NULL);
	if(0 != tpint)
	{
		DBG_LOG_ERR("fail to init mutex, for %s", strerror(tpint));
		tpret = ERR_API_FAIL;
		goto ERR;
	}

	//input delimiter
	pt_mslave_ctr->ch_input_delimiter = 0;

	// the file-reading buffer queue
	pt_mslave_ctr->freading_buf_queue = l_queue_new();
	tpint = pthread_mutex_init( &pt_mslave_ctr->mtx_for_freading_buf_queue, NULL);
	if(0 != tpint)
	{
		DBG_LOG_ERR("fail to init mtx, for %s", strerror(tpint));
		tpret = ERR_API_FAIL;
		goto ERR;
	}


	//
	pt_mslave_ctr->shmid = -1;
	pt_mslave_ctr->pt_dtblock = NULL;
	pt_mslave_ctr->pt_sem_for_shmem_dtblock = NULL;
	
	//memset( pt_mslave_ctr->pt_shmem_buf, 0, APP_SHM_TOTAL_SIZE);
	memset( pt_mslave_ctr->ipc_dtitem_buf, 0, SZ_IPC_BUF);

	memset( &pt_mslave_ctr->gwconf, 0, sizeof(gatewayTableData_t));
	memset( pt_mslave_ctr->dev_array, 0, sizeof(deviceTableData_t)*BULLET_GW_SENSOR_NUM_MAX);
	//memset( pt_mslave_ctr->sdata_array, 0, sizeof(sensorDataTableData_t)*BULLET_GW_SENSOR_NUM_MAX);
	//tpret = app_mslave_ipc_create_shmem( pt_mslave_ctr, sizeof(struct mslave_ipc_dtblock));	
	tpret = app_mslave_ipc_create_shmem( pt_mslave_ctr, APP_SHM_TOTAL_SIZE);
	if(ERR_NONE != tpret)
	{
		DBG_LOG_ERR("fail to create shared-memory");
		tpret = ERR_API_FAIL;
		goto ERR;
	}
	tpret = app_mslave_ipc_create_sem( pt_mslave_ctr);
	if(ERR_NONE != tpret)
	{
		DBG_LOG_ERR("fail to create semaphore");
		tpret = ERR_API_FAIL;
		goto ERR;
	}
	// the flags
	//app_mslave_ipc_set_force_dev_update( pt_mslave_ctr, false);
	pt_mslave_ctr->to_force_dev_update = false;
	//app_mslave_ipc_set_force_gwconf_update( pt_mslave_ctr, false);
	pt_mslave_ctr->to_force_gwconf_update;
	//app_mslave_ipc_set_force_sdata_update( pt_mslave_ctr, false);
	pt_mslave_ctr->to_force_sdata_update = false;

	
	return tpret;
	ERR:
		if(pt_mslave_ctr->mapping_queue)
		{
			l_queue_destroy( pt_mslave_ctr->mapping_queue, l_free);
			pt_mslave_ctr->mapping_queue = NULL;
		}
		return tpret;
}

/*to deinitialize the modbus slave controller
ret@ error code
*/
enum error_code app_mslave_deinit_ctr(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("invlaid parameter");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	//
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
	l_queue_destroy( pt_mslave_ctr->mapping_queue, l_free);
	pt_mslave_ctr->mapping_queue = NULL;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	pthread_mutex_destroy( &pt_mslave_ctr->mtx_for_mapping_queue);

	//
	pthread_mutex_destroy( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}

/*
ret @ the error code is returned ref@ errno, modbus_strerror
*/
enum error_code app_mslave_set_mctx( struct modbus_slave_controller*pt_mslave_ctr, modbus_t *pt_mctx)
{
	enum error_code tpret = ERR_NONE;

	//if((NULL == pt_mslave_ctr)||(NULL == pt_mctx))
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pt_mslave_ctr->mctx = pt_mctx;
	
	return tpret;
}

/*
ret@ pointer points to the modbus context
NOTE!!! NULL might be returned when no valid modbus context available
*/
modbus_t *app_mslave_get_mctx(struct modbus_slave_controller*pt_mslave_ctr)
{
	modbus_t *ptret = NULL;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		ptret = NULL;
		return ptret;
	}

	ptret = pt_mslave_ctr->mctx;
	return ptret;
}


/*to get the Modbus Byte-oder
return@ 
*/
enum mbus_byte_oder app_mslave_get_mbus_border(struct modbus_slave_controller *pt_mslave_ctr)
{
	enum mbus_byte_oder tpret = MBUS_BORDER_DEFAULT;
	if(pt_mslave_ctr)
	{
		tpret = pt_mslave_ctr->mbus_border;
	}
	return tpret;
}



/*to get the register mode
ret@
*/
enum register_mode app_mslave_get_reg_mode(uint8_t regmdval)
{
	enum register_mode tpret = REG_MD_UNKNOWN;
	switch(regmdval)
	{
		case MODBUS_REG_ADDR_EXT_BW_BB_MODE:// = 0, // 0, extensible mode(big word, big byte)
		{
			tpret = REG_MD_EXTENSIBLE;
			break;
		}
		case MODBUS_REG_ADDR_EFF_BW_BB_MODE://,     // 1, efficient mode(big word, big byte)
		{
			tpret = REG_MD_EFFICIENT;
			break;
		}
		case MODBUS_REG_ADDR_EXT_BW_LB_MODE://,     // 2, extensible mode(big word, little byte)
		{
			tpret = REG_MD_EXTENSIBLE;
			break;
		}
		case MODBUS_REG_ADDR_EFF_BW_LB_MODE://,     // 3, efficient mode(big word, little byte)
		{
			tpret = REG_MD_EFFICIENT;
			break;
		}
		case MODBUS_REG_ADDR_EXT_LW_BB_MODE://,     // 4, extensible mode(little word, big byte)
		{
			tpret = REG_MD_EXTENSIBLE;
			break;
		}
		case MODBUS_REG_ADDR_EFF_LW_BB_MODE://,     // 5, efficient mode(little word, big byte)
		{
			tpret = REG_MD_EFFICIENT;
			break;
		}
		case MODBUS_REG_ADDR_EXT_LW_LB_MODE://,     // 6, extensible mode(little word, little byte)
		{
			tpret = REG_MD_EXTENSIBLE;
			break;
		}
		case MODBUS_REG_ADDR_EFF_LW_LB_MODE://,     // 7, efficient mode(little word, little byte)
		{
			tpret = REG_MD_EFFICIENT;
			break;
		}
		default:
		{
			DBG_LOG_ERR("unexpected error");
			break;
		}
	}
	
	return tpret;
}




/*
none thread-safe
*/
void app_mslave_set_to_update_mslave(struct modbus_slave_controller*pt_mslave_ctr, bool newstate)
{
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		return;
	}
	if(newstate)
	{
		pt_mslave_ctr->to_update_mslave_req = true;
	}
	else
	{
		pt_mslave_ctr->to_update_mslave_req = false;
	}
}


/*
none thread-safe
*/
bool app_mslave_get_to_update_mslave(struct modbus_slave_controller*pt_mslave_ctr)
{
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	return pt_mslave_ctr->to_update_mslave_req;
}

/*
*/
const char * app_mslave_strerror(enum error_code err_code)
{
	const char * ptret = NULL;
	switch(err_code)
	{
		case ERR_UNKNOWN:
		{
			ptret = "Unknown error";
			break;
		}
		case ERR_API_FAIL:
		{
			ptret = "API fail";
			break;
		}
		case ERR_INVALID_PARAM:
		{
			ptret = "Invalid parameter";
			break;
		}
		
		/*..........*/

		default:
		{
			ptret = modbus_strerror( err_code);
			if(NULL == ptret)
			{
				ptret = "Unknown error code";
			}
			break;
		}
	}
	return ptret;
}





/* append modbus mapping to queue
ret@error code
*/
enum error_code app_mslave_append_mapping(struct modbus_slave_controller*pt_mslave_ctr, modbus_mapping_t*pt_mapping)
{
	enum error_code tpret = ERR_NONE;

	if((NULL == pt_mslave_ctr)||(NULL == pt_mapping))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
	l_queue_push_tail( pt_mslave_ctr->mapping_queue, pt_mapping);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);

	return tpret;
}

/*to find the the mapping
ret@modbus_mapping_t
*/
modbus_mapping_t *app_mslave_locate_mapping(struct modbus_slave_controller*pt_mslave_ctr, const uint16_t kp_addr, enum addr_group addr_gp)
{
	modbus_mapping_t *ptret = NULL;

	bool is_mapping_located = false;

	uint16_t tp_addr_min = 0, tp_addr_max = 0;
	
	struct l_queue_entry *pt_queue_entry = NULL;
	
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		ptret = NULL;
		return ptret;
	}

	if(l_queue_length( pt_mslave_ctr->mapping_queue) <= 0)
	{
		DBG_LOG_ERR("no mapping available");
		ptret = NULL;
		return ptret;
	}

	pt_queue_entry = l_queue_get_entries( pt_mslave_ctr->mapping_queue);
	while(1)
	{
		if(NULL == pt_queue_entry)
		{
			break;
		}
		ptret = (modbus_mapping_t*)pt_queue_entry->data;

		
		switch(addr_gp)
		{
			case ADDR_GP_COIL:
			{
				tp_addr_min = ptret->start_bits;
				tp_addr_max = ptret->start_bits + ptret->nb_bits;
				//DBG_LOG_WARN("coil mapping, range[0x%x, 0x%x) is located", tp_addr_min, tp_addr_max);
				if((kp_addr >= tp_addr_min)&&(kp_addr < tp_addr_max))
				{
					DBG_LOG_WARN("coil mapping, range[0x%x, 0x%x) is located", tp_addr_min, tp_addr_max);
					is_mapping_located = true;
				}
				break;
			}
			case ADDR_GP_DISCRETE_INPUT:
			{
				tp_addr_min = ptret->start_input_bits;
				tp_addr_max = ptret->start_input_bits + ptret->nb_input_bits;
				//DBG_LOG_WARN(" discrete input, range[ 0x%x, 0x%x) is located", tp_addr_min, tp_addr_max);
				if((kp_addr >= tp_addr_min)||(kp_addr < tp_addr_max))
				{
					DBG_LOG_WARN(" discrete input, range[ 0x%x, 0x%x) is located", tp_addr_min, tp_addr_max);
					is_mapping_located = true;
				}
				break;
			}
			case ADDR_GP_HOLDING_REGISTER:
			{
				tp_addr_min = ptret->start_registers;
				tp_addr_max = ptret->start_registers + ptret->nb_registers;
				//DBG_LOG_WARN("register mapping, range [0x%x, 0x%x) is located", tp_addr_min, tp_addr_max);
				if((kp_addr >= tp_addr_min)&&(kp_addr < tp_addr_max))
				{// when the address fall into the mapping
					//DBG_LOG_WARN("register mapping, range [0x%x, 0x%x) is located", tp_addr_min, tp_addr_max);
					is_mapping_located = true;
				}
				break;
			}
			case ADDR_GP_INPUT_REGISTER:
			{
				tp_addr_min = ptret->start_input_registers;
				tp_addr_max = ptret->start_input_registers + ptret->nb_input_registers;
				//DBG_LOG_WARN(" input register, range[ 0x%x, 0x%x) is located", tp_addr_min, tp_addr_max);
				if((kp_addr >= tp_addr_min)&&(kp_addr < tp_addr_max))
				{
					DBG_LOG_WARN(" input register, range[ 0x%x, 0x%x) is located", tp_addr_min, tp_addr_max);
					is_mapping_located = true;
				}
				break;
			}
			default:
			{
				ptret = NULL;
				break;
			}
		}


		if(true == is_mapping_located)
		{ // we found the mapping
			break;
		}
		
		pt_queue_entry = pt_queue_entry->next;
	}

	if(false == is_mapping_located)
	{
		ptret = NULL;
	}
	
	return ptret;
}


/*to check if the function-code is for reading
ret@ TRUE is returned when the fun-code is for reading
*/
static bool is_fun_code_for_reading(const uint8_t fcode) 
{
	bool tpret = false;

	switch(fcode)
	{
		case MODBUS_FC_READ_COILS:
		{	;}
		case MODBUS_FC_READ_DISCRETE_INPUTS:
		{	;}
		case MODBUS_FC_READ_HOLDING_REGISTERS:
		{	;}
		case MODBUS_FC_READ_INPUT_REGISTERS:
		{	;}
		case MODBUS_FC_READ_EXCEPTION_STATUS:
		{
			tpret = true;
			break;
		}
		case  MODBUS_FC_REPORT_SLAVE_ID :
		{
			tpret = true;
			break;
		}
		case MODBUS_FC_READ_FIFO_QUEUE:
		{
			tpret = true;
			break;
		}
		case MODBUS_FC_READ_FILE_RECORD:
		{
			tpret = true;
			break;
		}
		case MODBUS_FC_DIAGNOSTIC:
		{
			tpret = true;
			break;
		}
		case MODBUS_FC_GET_COM_EVENT_COUNTER:
		{
			tpret = true;
			break;
		}
		case MODBUS_FC_GET_COM_EVENT_LOG:
		{
			tpret = true;
			break;
		}
		case MODBUS_FC_READ_DEV_ID:
		{
			tpret = true;
			break;
		}
		/*....*/
		
		default:
		{
			tpret = false;
			break;
		}
	}
	
	return tpret;
}


/*to get the starting address of reading operation
ret@ starting address for reading

NOTE!!! we make the assumption that the msg pkt given is valid.

*/
uint16_t app_mslave_pick_starting_address(const uint8_t*pt_msg, const uint32_t len)
{
	uint16_t tpret = 0x0;
	if((NULL != pt_msg)&&(len >= MIN_RTU_PKT))
	{
		tpret = (pt_msg[2] << 8) | pt_msg[3];
	}
	return tpret;
}

/*to get the quantity of registers/coils/discret to be read
ref@the quantity to be read

NOTE!!! a valid ADU is expected
*/
uint16_t app_mslave_pick_quantity(const uint8_t *pt_msg, const uint32_t len)
{
	uint16_t tpret = 0x0;

	if((NULL != pt_msg)||(len >= MIN_RTU_PKT))
	{
		tpret = (pt_msg[4] << 8) | pt_msg[5];
	}
	
	return tpret;
}


/*to get the function code from the msg pkt
ret@ the function code
NOTE !!! make sure the valid msg pkg is given
*/

uint8_t app_mslave_pick_fun_code(const uint8_t*pt_msg, const uint32_t len)
{
	uint8_t tpret = 0x0;

	if((NULL != pt_msg)&&(len >= MIN_RTU_PKT))
	{
		tpret = pt_msg[1];
	}
	
	return tpret;
}

/*to get the Slave ID from msg pkt

*/
uint8_t app_mslave_pick_slave_id(const uint8_t *pt_msg, const uint32_t len)
{
	uint8_t tpret = 0x0;

	if((NULL != pt_msg)&&(len >= MIN_RTU_PKT))
	{
		tpret = pt_msg[0];
	}
	
	return tpret;
}

/*to get diagnostic register
ret# the register value
*/
uint16_t app_mslave_get_diagnostic_register(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint16_t tpret = 0;

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.diag_regisger;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}


/*to modify the diagnostic register
ret@error_code
*/
enum error_code app_mslave_set_diagnostic_register(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_API_FAIL;

	DBG_LOG_ERR("TODO===========to complete");

	return tpret;
}


/* to set the slave mode
ret@error_code
*/
enum error_code app_mslave_set_mode(struct modbus_slave_controller*pt_mslave_ctr, enum slave_mode new_mode)
{
	enum error_code tpret = ERR_NONE;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	if((new_mode <= SMODE_UNKNOWN)||(new_mode >= SMODE_MAX))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pt_mslave_ctr->smode = new_mode;
	
	return tpret;
}

/* to get the slave mode
ret@error_code
*/
enum slave_mode app_mslave_get_mode(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum slave_mode tpret = SMODE_UNKNOWN;

	tpret = pt_mslave_ctr->smode;

	return tpret;
	
}

/*to clear state-set
ret@error_code
*/
enum error_code app_mslave_clear_state_set(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	memset( &pt_mslave_ctr->state_set, 0, sizeof(struct counter_log_register_set));
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}

//

/*to increase Bus Message Count
ret@enum error_code
*/
enum error_code app_mslave_inc_bus_msg_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	if(pt_mslave_ctr->state_set.diag_cnt_bus_message < MAX_DIAG_CNT)
	{
		pt_mslave_ctr->state_set.diag_cnt_bus_message++;
	}
	else
	{
		;//DBG_LOG_WARN(" diagnostic bus message counter overflow");
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);
	
	return tpret;
}

/*to get the bus message counter
ret@ val
*/
uint16_t app_mslave_get_bus_msg_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint16_t tpret = 0;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = 0;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.diag_cnt_bus_message;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);
	
	return tpret;
}

/* to increase bus communication error count
ret@error code
*/
enum error_code app_mslave_inc_bus_comm_error_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}	

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	if(pt_mslave_ctr->state_set.diag_cnt_bus_comm_error < MAX_DIAG_CNT)
	{
		pt_mslave_ctr->state_set.diag_cnt_bus_comm_error ++;
	}
	else
	{
		DBG_LOG_WARN("BUS communication error overflow");
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}

/*to get bus communication error count
ret@val
*/
uint16_t app_mslave_get_bus_comm_error_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint16_t tpret = 0;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = 0;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.diag_cnt_bus_comm_error;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);
	
	return tpret;
}

/*to increase bus exception error count
ret@error code
*/
enum error_code app_mslave_inc_slave_exception_error_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	if(pt_mslave_ctr->state_set.diag_cnt_slave_exception_error < MAX_DIAG_CNT)
	{
		pt_mslave_ctr->state_set.diag_cnt_slave_exception_error++;
	}
	else
	{
		DBG_LOG_WARN("slave exception error overflow");
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);
	
	return tpret;
}

/*to get...
ret@val
*/
uint16_t app_mslave_get_slave_exception_error_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint16_t tpret = 0;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = 0;
		return tpret;
	}
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.diag_cnt_slave_exception_error;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}

/*to increase
ret@error code
*/
enum error_code app_mslave_inc_slave_msg_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}
	
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	if(pt_mslave_ctr->state_set.diag_cnt_slave_message < MAX_DIAG_CNT)
	{
		pt_mslave_ctr->state_set.diag_cnt_slave_message++;
	}
	else
	{
		;//DBG_LOG_WARN("slave msg overflow");
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}

/* to get ...
ret@val
*/
uint16_t app_mslave_get_slave_msg_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint16_t tpret = 0;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.diag_cnt_slave_message;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);
	
	return tpret;
}

/*
*/
enum error_code app_mslave_inc_slave_no_resp_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	if(pt_mslave_ctr->state_set.diag_cnt_slave_no_response < MAX_DIAG_CNT)
	{
		pt_mslave_ctr->state_set.diag_cnt_slave_no_response ++;
	}
	else
	{
		;//DBG_LOG_WARN("slave no resp cnt overflow");
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}

/*
*/
uint16_t app_mslave_get_slave_no_resp_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint16_t tpret = 0;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("invalid parameter");
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.diag_cnt_slave_no_response;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}


/*
*/
enum error_code app_mslave_inc_slave_nak_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	if(pt_mslave_ctr->state_set.diag_cnt_slave_nak < MAX_DIAG_CNT)
	{
		pt_mslave_ctr->state_set.diag_cnt_slave_nak ++;
	}
	else
	{
		DBG_LOG_WARN("slave NAK cnt overflow");
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);
	
	return tpret;
}


/*
*/
uint16_t app_mslave_get_slave_nak_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint16_t tpret = 0;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("invalid parameter");
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.diag_cnt_slave_nak;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}



/*
*/
enum error_code app_mslave_inc_slave_busy_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	if(pt_mslave_ctr->state_set.diag_cnt_slave_busy < MAX_DIAG_CNT)
	{
		pt_mslave_ctr->state_set.diag_cnt_slave_busy ++;
	}
	else
	{
		DBG_LOG_WARN("slave busy cnt overflow");
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);
	
	return tpret;
}


/*
*/
uint16_t app_mslave_get_slave_busy_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint16_t tpret = 0;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("invalid parameter");
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.diag_cnt_slave_busy;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}


/*
*/
enum error_code app_mslave_inc_bus_charac_overrun_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	if(pt_mslave_ctr->state_set.diag_cnt_bus_charac_overrun < MAX_DIAG_CNT)
	{
		pt_mslave_ctr->state_set.diag_cnt_bus_charac_overrun ++;
	}
	else
	{
		DBG_LOG_WARN("bus character overrun cnt overflow");
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);
	
	return tpret;
}



/*
*/
uint16_t app_mslave_get_slave_bus_charac_overrun_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint16_t tpret = 0;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("invalid parameter");
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.diag_cnt_bus_charac_overrun;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}


/*
*/
enum error_code app_mslave_inc_comm_event_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	if( pt_mslave_ctr->state_set.cnt_comm_event < MAX_DIAG_CNT)
	{
		pt_mslave_ctr->state_set.cnt_comm_event ++;
	}
	else
	{
		DBG_LOG_WARN("comm event cnt overflow");
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}

/*
*/
uint16_t app_mslave_get_comm_event_cnt(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint16_t tpret = 0;
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("invalid parameter");
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.cnt_comm_event;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}


/*to set the comm-event log
*/
enum error_code app_mslave_set_comm_event_log(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t event_byte)
{
	enum error_code tpret = ERR_NONE;

	//DBG_LOG_ERR("Todo===================to complete");

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);

	pt_mslave_ctr->state_set.log_comm_event[pt_mslave_ctr->state_set.log_event_idx % MAX_COMM_EVENT_LOG] = event_byte;
	pt_mslave_ctr->state_set.log_event_idx = (pt_mslave_ctr->state_set.log_event_idx + 1) % MAX_COMM_EVENT_LOG;
	if(pt_mslave_ctr->state_set.log_event_cnt < MAX_COMM_EVENT_LOG)
	{
		pt_mslave_ctr->state_set.log_event_cnt ++;	
	}
	
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}

/* ptresbuf @ the event-log bytes is returned by buffer pointed by this pointer
ret@ the number of event-log byte is returned

NOTE!!! make sure enough memory(MAX_COMM_EVENT_LOG) is given for event-log bytes to be returned
		pt_resbuf[0] is the latest event log
*/
uint16_t app_mslave_get_comm_event_log(struct modbus_slave_controller*pt_mslave_ctr, uint8_t *pt_resbuf)
{
	uint16_t tpret = 0;

	if((NULL == pt_mslave_ctr)||(NULL == pt_resbuf))
	{
		DBG_LOG_ERR(" unexpected NULL");
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.log_event_cnt > MAX_COMM_EVENT_LOG ? MAX_COMM_EVENT_LOG : pt_mslave_ctr->state_set.log_event_cnt;
	if(tpret)
	{
		for(uint32_t i = 0;i < tpret; i++)
		{
			pt_resbuf[i % MAX_COMM_EVENT_LOG] = pt_mslave_ctr->state_set.log_comm_event[(0xFF + pt_mslave_ctr->state_set.log_event_idx - 1 - i) % MAX_COMM_EVENT_LOG];
		}
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}

/*
*/
enum error_code app_mslave_set_exception_status(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t except_status)
{
	enum error_code tpret = ERR_NONE;

	if(NULL == tpret)
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	pt_mslave_ctr->state_set.exception_status = except_status;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);

	return tpret;
}

/*
*/
uint8_t app_mslave_get_exception_status(struct modbus_slave_controller*pt_mslave_ctr)
{
	uint8_t tpret = 0;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR(" unexpected NULL");
		return tpret;
	}

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_state_set);
	tpret = pt_mslave_ctr->state_set.exception_status;
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_state_set);
	
	return tpret;
}
/* to validate the the modbus packet received 
ret@error code
*/
enum error_code app_mslave_validate_pkt(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t*pt_msg, const uint8_t len)
{
	enum error_code tpret = ERR_NONE;
	uint16_t tp_crc_calculated = 0;
	uint16_t tp_crc_received = 0;
	
	if((NULL == pt_mslave_ctr) || (NULL == pt_msg)||(len < MIN_RTU_PKT)) // <data>(>= 1B) + CRC(2B)
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	//MK1155U________________________________	
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	//to check if Slave-ID match
	if(pt_msg[0] != modbus_get_slave( app_mslave_get_mctx( pt_mslave_ctr)))
	{		
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		DBG_LOG_ERR(" the Salve ID does not match");			
		tpret = EMBXSFAIL;
		goto EXIT;
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK1155D__________________________________
	
	// to check the CRC
	tp_crc_calculated = util_tool_cal_crc16( pt_msg, len - 2);
	tp_crc_received = (pt_msg[len - 1] << 8) | pt_msg[len -2];
	DBG_LOG_INFO("CRC-value  caculated:0x%.4X, received:0x%.4X", tp_crc_calculated, tp_crc_received);
	if(tp_crc_calculated != tp_crc_received)
	{
		DBG_LOG_ERR("invalid msg pkt, for CRC mismatch");

		app_mslave_inc_bus_comm_error_cnt( pt_mslave_ctr);
		
		tpret = EMBBADCRC;
		goto EXIT;
	}

	//to check the funciton-code; for Modbus Slave, only function-code for reading is available
	if(is_fun_code_for_reading( pt_msg[1]) != true)
	{
		DBG_LOG_ERR(" only F-CODE for reading is avilable");
		tpret = EMBXILFUN;
		goto EXIT;
	}

	/*..to_complete........*/
	
	EXIT:
		return tpret;
}


#if(0)
/*function: to send the Modbus packet
ret@ the number of bytes sendt
*/
int app_mslave_msg_send(const modbus_t*pt_mctx, const uint8_t *pt_pkt, const uint32_t len)
{
	int tpret = -1;

	/*to complete===================*/
	DBG_LOG_ERR("TODO========to complete");
	
	return tpret;
}
#endif


/* to get the modbus ADDRESS GROUP depending on the message packet
ret@ address group info
*/
enum addr_group app_mslave_get_op_addr_group(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t*pt_msg, const uint8_t len)
{
	enum addr_group tpret = ADDR_GP_UNKNOWN;

	if((NULL == pt_mslave_ctr)||(NULL == pt_msg)||(len < MIN_RTU_PKT))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ADDR_GP_UNKNOWN;
		goto EXIT;
	}

	switch(pt_msg[1])
	{
		case MODBUS_FC_READ_COILS:
		{
			tpret = ADDR_GP_COIL;
			break;
		}
		case MODBUS_FC_READ_DISCRETE_INPUTS:
		{
			tpret = ADDR_GP_DISCRETE_INPUT;
			break;
		}
		case MODBUS_FC_READ_HOLDING_REGISTERS:
		{
			tpret = ADDR_GP_HOLDING_REGISTER;
			break;
		}
		case MODBUS_FC_READ_INPUT_REGISTERS:
		{
			tpret = ADDR_GP_INPUT_REGISTER;
			break;
		}
		//__________________________
		case MODBUS_FC_REPORT_SLAVE_ID:
		{
			tpret = ADDR_GP_REPORT_SLAVE_ID;
			break;
		}
		case MODBUS_FC_READ_FIFO_QUEUE:
		{
			tpret = ADDR_GP_FIFO_QUEUE;
			break;
		}
		case MODBUS_FC_READ_FILE_RECORD:
		{
			tpret = ADDR_GP_FILE_RECORD;
			break;
		}
		case MODBUS_FC_DIAGNOSTIC:
		{
			tpret = ADDR_GP_DIAGNOSTICS;
			break;
		}
		case MODBUS_FC_GET_COM_EVENT_COUNTER:
		{
			tpret = ADDR_GP_COM_EVENT_COUNTER;
			break;
		}
		case MODBUS_FC_GET_COM_EVENT_LOG:
		{
			tpret = ADDR_GP_COM_EVENT_LOG;
			break;
		}
		case MODBUS_FC_READ_DEV_ID:
		{
			tpret = MODBUS_FC_READ_DEV_ID;
			break;
		}
		/*....*/

		default:
		{
			tpret = ADDR_GP_UNKNOWN;
			break;
		}
	}
	
	EXIT:
		return tpret;
}


/*to create EFFICIENT register mapping
ret@error_code
*/
static enum error_code to_create_efficient_reg_mapping(const struct modbus_slave_controller*pt_mslave_ctr)
{
	
	enum error_code tpret = ERR_API_FAIL;
	uint32_t tpu32 = 0;
	modbus_mapping_t *pt_mapping = NULL;

	struct register_mapping_info *pt_reg_mapping_info = NULL;
	struct sdata_layout kp_sdata_layout = {0};
	

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
	tpu32 = l_queue_length( pt_mslave_ctr->mapping_queue);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	if(tpu32)
	{
		DBG_LOG_WARN("!!! the mapping queue is not empty, len = %d", tpu32);
	}

	/*NOTE!! all register is on the same mapping-block, which means (0xFFFF*2) Bytes will be allocated from heap.
	Is there any chance that we fail to request for so much memory as one block ?
	*/

	//MK1327U___________________________
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
	app_mslave_release_mapping_none_thread_safe( pt_mslave_ctr);
	
	pt_mapping = modbus_mapping_new_start_address( 0, 0, 0, 0, 0, MAX_REG_NUMBER, 0, 0);
	if(NULL == pt_mapping)
	{		
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
		DBG_LOG_ERR("fail  to create Reg mapping");
		tpret = ERR_API_FAIL;
		goto EXIT;
	}
	//app_mslave_append_mapping( pt_mslave_ctr, pt_mapping);		
	l_queue_push_tail( pt_mslave_ctr->mapping_queue, pt_mapping);

	//MK1398U__to check if the remapping configuration is valid____
	pt_reg_mapping_info = app_mslave_get_reg_mapping_info();
	if(true != pt_reg_mapping_info->flg)
	{
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
		DBG_LOG_INFO("no valid mapping info");
		goto EXIT;			
	}
	// to check if the mapping info overwrite the sensor-data block
	for(uint32_t i = 0; i < BULLET_GW_SENSOR_NUM_MAX; i++)
	{
		kp_sdata_layout = app_mslave_ipc_get_sdata_layout_info(&pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX]);	
		if(true != kp_sdata_layout.is_valid)
		{
			continue;
		}
		if((pt_reg_mapping_info->min_reg_addr > (kp_sdata_layout.addr + kp_sdata_layout.regnum)) || (pt_reg_mapping_info->max_reg_addr < kp_sdata_layout.addr ))
		{
			continue;
		}
		else
		{ // the modbus reg-block is over-written
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
			pt_reg_mapping_info->flg = false; // reset the flag when the remapping info is invalid
			goto EXIT;							
		}
	}
	//MK1398D______________________________________________________

	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	//MK1327D____________________________

	EXIT:
		return tpret;
}


/*to create EXTENSIBLE register mapping
ret#error code
*/
static enum error_code to_create_extensible_reg_mapping(const struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t tpu32 = 0;
	uint16_t kp_addr = 0, kp_sz_mblock = 0;
	
	modbus_mapping_t *pt_mapping = NULL;

	
	//MK1308U____________________________
	pthread_mutex_lock(&pt_mslave_ctr->mtx_for_mapping_queue);
	tpu32 = l_queue_length( pt_mslave_ctr->mapping_queue);
	if(tpu32)
	{ // you probably need to release the queue before trying to create a new one
		DBG_LOG_WARN("!!!! the mapping queue is not empty, len = %d", tpu32);
	}	
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	
	// for Gateway
	pt_mapping = modbus_mapping_new_start_address( 0, 0, 0, 0, MODBUS_REG_GATEWAY_START_ADDR_OFFSET, MODBUS_REG_GATEWAY_LENGTH, 0, 0);
	if(NULL == pt_mapping)
	{		
		DBG_LOG_ERR("fail to create Reg mapping for gateway");
		tpret = ERR_API_FAIL;
		goto EXIT;
	}	
	
	app_mslave_append_mapping( pt_mslave_ctr, pt_mapping);
	
	
	// for each sensor
	kp_sz_mblock = MODBUS_REG_SENSOR_LENGTH;
	kp_addr =  MODBUS_REG_SENSOR_START_ADDR_OFFSET;
	for(uint32_t i = 0; i < BULLET_GW_SENSOR_NUM_MAX; i++)
	{
		if((kp_addr + kp_sz_mblock) > 0xFFFF)
		{
			DBG_LOG_ERR("Oops!! the Reg address overflow.");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
		
		pt_mapping = modbus_mapping_new_start_address( 0, 0, 0, 0, kp_addr, kp_sz_mblock, 0, 0);
		if(NULL == pt_mapping)
		{
			DBG_LOG_ERR("fail to create Reg mapping for sensor");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
		app_mslave_append_mapping( pt_mslave_ctr, pt_mapping);
		kp_addr = kp_addr + kp_sz_mblock;
	}
	
	//MK1308D____________________________
	
	EXIT:
		return tpret;
}


/*to create EXTENSIBLE register mapping
ret#error code
NOTE!!!
1. we don't check the length and the offset of each mapping block. so make sure the configuration info given is valid.

*/
static enum error_code to_create_extensible_reg_mapping_v2(const struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t tpu32 = 0;
	uint16_t kp_addr = 0, kp_sz_mblock = 0;
	struct sdata_layout kp_sdata_layout = {0};	
	modbus_mapping_t *pt_mapping = NULL;
	
	struct register_mapping_info *pt_reg_remap_info = NULL;
	
	//MK1308U____________________________
	pthread_mutex_lock(&pt_mslave_ctr->mtx_for_mapping_queue);
	tpu32 = l_queue_length( pt_mslave_ctr->mapping_queue);
	if(tpu32)
	{ // you probably need to release the queue before trying to create a new one
		DBG_LOG_WARN("!!!! the mapping queue is not empty, len = %d", tpu32);
	}	
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);


	
	pthread_mutex_lock(&pt_mslave_ctr->mtx_for_mapping_queue);
	app_mslave_release_mapping_none_thread_safe( pt_mslave_ctr); // to release the old mapping queue
	do
	{
		// for Gateway
		kp_sdata_layout = app_mslave_ipc_get_gwconf_layout_info( &pt_mslave_ctr->gwconf);
		if((true != kp_sdata_layout.is_valid)||((kp_sdata_layout.addr + kp_sdata_layout.regnum) > 0xFFFF))
		{
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
			DBG_LOG_ERR("fail to get the mapping info for gateway 0x%x - 0x%x", kp_sdata_layout.addr, kp_sdata_layout.regnum);
			tpret = ERR_API_FAIL;
			goto EXIT;
		} 
		//__________________
		pt_mapping = modbus_mapping_new_start_address( 0, 0, 0, 0, kp_sdata_layout.addr, kp_sdata_layout.regnum, 0, 0);
		if(NULL == pt_mapping)
		{			
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
			DBG_LOG_ERR("fail to create Reg mapping for gateway");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}		
		//app_mslave_append_mapping( pt_mslave_ctr, pt_mapping);		
		l_queue_push_tail( pt_mslave_ctr->mapping_queue, pt_mapping);



		
		// for each sensor_________________________
		kp_sz_mblock = 0;
		kp_addr =  0;
		for(uint32_t i = 0; i < BULLET_GW_SENSOR_NUM_MAX; i++)
		{
			kp_sdata_layout = app_mslave_ipc_get_sdata_layout_info( &pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX]);	

			if(true != kp_sdata_layout.is_valid)
			{
				DBG_LOG_WARN(" no valid sdata layout info is gotten, to double check dev-name");
				continue;
			}
			DBG_LOG_INFO("name-str:%s, addr:0x%04X, len:0x%02X", pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].name, kp_sdata_layout.addr, kp_sdata_layout.regnum);
			
			kp_addr = kp_sdata_layout.addr;
			kp_sz_mblock = kp_sdata_layout.regnum;
			
			if((kp_addr + kp_sz_mblock) > 0xFFFF)
			{
				DBG_LOG_ERR("Oops!! the Reg address overflow.");
				#if(0)
					//tpret = ERR_API_FAIL;
					//goto EXIT;
				#else
					continue;
				#endif
			}
			
			pt_mapping = modbus_mapping_new_start_address( 0, 0, 0, 0, kp_addr, kp_sz_mblock, 0, 0);
			if(NULL == pt_mapping)
			{				
				pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
				DBG_LOG_ERR("fail to create Reg mapping for sensor");
				tpret = ERR_API_FAIL;
				goto EXIT;
			}
			//app_mslave_append_mapping( pt_mslave_ctr, pt_mapping);			
			l_queue_push_tail( pt_mslave_ctr->mapping_queue, pt_mapping);

			DBG_LOG_WARN("Modbus mapping block for %d @0x%x, reglen %d is created", kp_sdata_layout.nameid, kp_addr, kp_sz_mblock);
		}


		//MK1568U____add the remapping block__________
		pt_reg_remap_info = app_mslave_get_reg_mapping_info();
		if(true != pt_reg_remap_info->flg)
		{
			#if(0)
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
			DBG_LOG_ERR("the remapping info is invalid");
			tpret = ERR_API_FAIL;
			goto EXIT;
			#else
				DBG_LOG_WARN("the remapping info is invalid");
				break;
			#endif
		}
		// to make sure the remapping block will not overwrite the sensor-data mapping block
		for(uint32_t i = 0;i < BULLET_GW_SENSOR_NUM_MAX; i++)
		{
			kp_sdata_layout = app_mslave_ipc_get_sdata_layout_info( &pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX]);
			if(true != kp_sdata_layout.is_valid)
			{
				continue;
			}
			if((pt_reg_remap_info->min_reg_addr > (kp_sdata_layout.addr + kp_sdata_layout.regnum))\
				||(pt_reg_remap_info->max_reg_addr < kp_sdata_layout.addr))
			{
				continue;
			}
			else
			{
				pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
				DBG_LOG_ERR("the reg mapping is over-written");
				tpret = ERR_API_FAIL;
				goto EXIT;
			}
		}
		//continue to create the remapping block
		kp_addr = pt_reg_remap_info->min_reg_addr;
		kp_sz_mblock = pt_reg_remap_info->max_reg_addr - pt_reg_remap_info->min_reg_addr + 1; // +1 for the reason that the max_reg_addr is included
		pt_mapping = modbus_mapping_new_start_address( 0, 0, 0, 0, kp_addr, kp_sz_mblock, 0, 0);
		if(NULL == pt_mapping)
		{
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
			DBG_LOG_ERR("fail to create Reg mapping for remapping");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
		l_queue_push_tail( pt_mslave_ctr->mapping_queue, pt_mapping);
		DBG_LOG_WARN("Modbus mapping block 0x%x-reglen 0x%x is created", kp_addr, kp_sz_mblock);
		//MK1568D_____________________________________		
	}while(0);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
		
	
	//MK1308D____________________________
	
	EXIT:
		return tpret;
}



/*create Modbus slave mapping
ret@error code
*/
enum error_code app_mslave_create_mapping(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	#if(0)
		modbus_mapping_t *pt_mapping = NULL;
		DBG_LOG_WARN("the following is only for test");

		// addressing [0, 0x10) , coils, register, input register and input bit
		pt_mapping = modbus_mapping_new( 0x10, 0x10, 0x10, 0x10);
		if(NULL == pt_mapping)
		{
			DBG_LOG_ERR("fail to cteate new mapping, for %s", modbus_strerror( errno));
			tpret = ERR_API_FAIL;
			goto EXIT;
		}

		app_mslave_append_mapping( pt_mslave_ctr, pt_mapping);

		//addressing [ 0xFF00, 0xFF20)
		pt_mapping = modbus_mapping_new_start_address( 0xFF00, 0x20,0xFF00, 0x20,0xFF00, 0x20,0xFF00, 0x20);
		if(NULL == pt_mapping)
		{
			DBG_LOG_ERR("fail to create new mapping for %s", modbus_strerror( errno));
			tpret = ERR_API_FAIL;
			goto EXIT;
		}

		app_mslave_append_mapping( pt_mslave_ctr,  pt_mapping);
	#else
		// the layout of Modbus Slave Register Map ref@sh_mem.h
		modbus_mapping_t *pt_mapping = NULL;
		int32_t tpint = 0;

		//
		enum register_mode kp_reg_md = REG_MD_UNKNOWN;
		//MK1549U_____________
		tpint = pthread_mutex_trylock( &pt_mslave_ctr->mtx_for_port_conf);
		if(0 != tpint)
		{
			//DBG_LOG_ERR("fail to acquire the mutex, is_waiting_for_recv %d", pt_mslave_ctr->is_waiting_for_recv);
			app_mslave_set_to_update_mslave( pt_mslave_ctr, true); // to set the flag to request the lock
			DBG_LOG_ERR("fail to acquire the mutex, to_update_mslave %d", app_mslave_get_to_update_mslave( pt_mslave_ctr));
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
		app_mslave_set_to_update_mslave( pt_mslave_ctr, false);
		
		/*
		//if(true == pt_mslave_ctr->is_waiting_for_recv)
		//{
		//	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//	DBG_LOG_ERR(" the PORT conf is locked, for data receivation");						
		//	tpret = ERR_API_FAIL;
		//	goto EXIT;
		}*/
		kp_reg_md = pt_mslave_ctr->port_conf.reg_md;
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1549D______________
		
		if(REG_MD_EXTENSIBLE == kp_reg_md)	
		{
			DBG_LOG_WARN(" to create Register mapping in  EXTENSIBLE_MODE");
			//tpret = to_create_extensible_reg_mapping( pt_mslave_ctr);
			tpret = to_create_extensible_reg_mapping_v2( pt_mslave_ctr);
			if(ERR_NONE != tpret)
			{
				DBG_LOG_ERR("fail to create EXTENSIBLE Reg Mapping");
				goto EXIT;
			}
		}
		else if(REG_MD_EFFICIENT == kp_reg_md)
		{
			DBG_LOG_WARN("to create Register mapping in EFFICIENT_MODE");
			tpret = to_create_efficient_reg_mapping( pt_mslave_ctr);
		}
		else
		{
			DBG_LOG_ERR(" unknown Reg Mode");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
	#endif

	EXIT:
		return tpret;
}

/*
typedef bool (*l_queue_remove_func_t) (void *data, void *user_data);
*/
static bool cbk_q_rm_mapping(void *data, void *user_data)
{
	if(data)
	{
		modbus_mapping_free((modbus_mapping_t *)data);
	}
	else
	{
		DBG_LOG_ERR("unexpected NULL");
	}
	return true;
}

/*to release the register mapping queue
ret@error_code
*/
enum error_code app_mslave_release_mapping(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);

	//l_queue_destroy( pt_mslave_ctr->mapping_queue, modbus_mapping_free);
	l_queue_foreach_remove( pt_mslave_ctr->mapping_queue, cbk_q_rm_mapping, NULL);

	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	
	return tpret;
}





/*to release the register mapping queue
ret@error_code
NOTE!!! this API is not thread safe
*/
enum error_code app_mslave_release_mapping_none_thread_safe(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	//pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);

	//l_queue_destroy( pt_mslave_ctr->mapping_queue, modbus_mapping_free);
	l_queue_foreach_remove( pt_mslave_ctr->mapping_queue, cbk_q_rm_mapping, NULL);

	//pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	
	return tpret;
}





/*to handle function-code READ_COILS
ret@ error code
*/
static enum error_code handler_for_fcode_read_coils(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t*pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_UNKNOWN;
	const modbus_t * pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
	modbus_mapping_t *pt_modbus_mapping = NULL;
	
uint16_t kp_starting_addr = 0;
	enum addr_group kp_addr_gp  = ADDR_GP_COIL;
	
	uint16_t kp_quantity = 0;
	
	bool flg_exception = false;

	kp_starting_addr = app_mslave_pick_starting_address( pt_pktbuf, len);

	// to check the Quantity
	kp_quantity = app_mslave_pick_quantity( pt_pktbuf, len);
	if((kp_quantity <= 0)||(kp_quantity > 0x07D0))
	{
		DBG_LOG_ERR("invalid quantity to be read %d", kp_quantity);

		//MK1607U____________________________
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1607D____________________________
		
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);

		tpret = EMBXILVAL;
		goto EXIT;
	}

	// to check the address range; kp_starting_addr is always in the range
	if((kp_starting_addr + kp_quantity) > 0xFFFF)
	{
		DBG_LOG_ERR(" the coil address is out of range");

		//MK1624__________________________________
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1624___________________________________
		
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);

		tpret = EMBXILADD;
		goto EXIT;
	}
	

	//MK579U_____________________
	DBG_LOG_WARN("Double check when this thread run into dead-lock");
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
	// to locate the Modbus mapping
	pt_modbus_mapping = app_mslave_locate_mapping( pt_mslave_ctr, kp_starting_addr, kp_addr_gp);
	if(NULL == pt_modbus_mapping)
	{
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);

		DBG_LOG_ERR(" fail to find the mapping for starting_addr@0x%x", kp_starting_addr);

		//MK1649U__________________
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1649D__________________
	
		//app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
		flg_exception = true; /*NOTE!!! DO NOT take another mtx while one is taken, in case of dead-lock*/
		
		tpret = EMBXILADD;
		goto EXIT;			
	}

	int32_t kp_saved_errno = errno;
	errno = 0;
	//MK1666U_____________________
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
	modbus_reply( pt_mctx, pt_pktbuf, len, pt_modbus_mapping);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK1666D_____________________
	if(0 != errno)
	{
		//app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
		flg_exception = true;
	}
	else
	{
		errno = kp_saved_errno;
	}
	
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	//MK579D_____________________

	if(true == flg_exception)
	{
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
	}
	
	EXIT:
		return tpret;
}

/*to handle function-code READ discrete inputs
ret@ error code
*/
static enum error_code handler_for_fcode_read_discrete_inputs(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t*pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_UNKNOWN;

	const modbus_t *pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
	modbus_mapping_t *pt_modbus_mapping = NULL;
	uint16_t kp_starting_addr = 0;
	enum addr_group kp_addr_gp = ADDR_GP_DISCRETE_INPUT;

	uint16_t kp_quantity = 0;
	bool flg_exception = false;

	kp_starting_addr = app_mslave_pick_starting_address( pt_pktbuf,  len);

	// to check the quantity
	kp_quantity = app_mslave_pick_quantity( pt_pktbuf, len);
	if((kp_quantity <= 0)||(kp_quantity > 0x07D0))
	{
		DBG_LOG_ERR("invalid quantity to be read %d", kp_quantity);

		//MK1716U__________________
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1716D__________________
		
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
		
		tpret = EMBXILVAL;

		goto EXIT;
	}

	// to check the address range
	if((kp_starting_addr + kp_quantity) > 0xFFFF)
	{
		DBG_LOG_ERR("the discrete inputs is out of range");
		//MK1735U______________________
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1735D______________________
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
		
		tpret = EMBXILADD;
		goto EXIT;
	}

	//MK619U________________________
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
	pt_modbus_mapping = app_mslave_locate_mapping( pt_mslave_ctr, kp_starting_addr, kp_addr_gp);
	if(NULL == pt_modbus_mapping)
	{
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);

		DBG_LOG_ERR("fail to find the mapping for starting_addr@0x%x", kp_starting_addr);

		//MK1755U___________
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1755D___________

		flg_exception = true;
		tpret = EMBXILADD;
		goto EXIT;
	}

	int32_t kp_saved_errno = errno;
	errno = 0;
	//MK1770U_____
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
	modbus_reply( pt_mctx, pt_pktbuf, len, pt_modbus_mapping);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK1770D_____
	if(0 != errno)
	{
		flg_exception = true;
	}
	else
	{
		errno = kp_saved_errno;
	}
	
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	//MK619D________________________

	if(true == flg_exception)
	{
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
	}

	EXIT:
		return tpret;
}

/*to handle function-code read inputs registers
ret@error code
*/
static enum error_code handler_for_fcode_read_inputs_registers(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t*pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_UNKNOWN;

	const modbus_t *pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
	modbus_mapping_t *pt_modbus_mapping = NULL;
	uint16_t kp_starting_addr = 0;
	enum addr_group kp_addr_gp = ADDR_GP_INPUT_REGISTER;

	uint16_t kp_quantity = 0;
	bool flg_exception = false;

	kp_starting_addr = app_mslave_pick_starting_address( pt_pktbuf,  len);

	//to check the quantity
	kp_quantity = app_mslave_pick_quantity( pt_pktbuf,  len);
	if((kp_quantity <= 0)||(kp_quantity > 0x007D))
	{
		DBG_LOG_ERR("illegal quantity of registers to be read %d", kp_quantity);
		//MK1819U
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1819D
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
		
		tpret = EMBXILVAL;
		goto EXIT;
	}

	// to check the address range
	if((kp_starting_addr + kp_quantity) > 0xFFFF)
	{
		DBG_LOG_ERR("the reading operation is out of range");
		//MK1835U
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1835D
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
		
		tpret = EMBXILADD;
		goto EXIT;
	}
	
	//MK655U_____________________
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
	pt_modbus_mapping = app_mslave_locate_mapping( pt_mslave_ctr, kp_starting_addr, kp_addr_gp);
	if(NULL == pt_modbus_mapping)
	{
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);

		DBG_LOG_ERR("fail to find the mapping for starting_addr@0x%x", kp_starting_addr);
		//MK1855U
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1855D
		flg_exception = true;

		tpret = EMBXILADD;
		goto EXIT;
	}

	int32_t kp_saved_errno = errno;
	errno = 0;
	//MK1869U
	pthread_mutex_lock(&pt_mslave_ctr->mtx_for_port_conf);
	pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
	modbus_reply( pt_mctx, pt_pktbuf, len, pt_modbus_mapping);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK1869D
	if(0 != 0)
	{
		flg_exception = true;
	}
	else
	{
		errno = kp_saved_errno;
	}
	
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	//MK655D_____________________

	if(true == flg_exception)
	{
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
	}
	
	EXIT:	
		return tpret;
}

/*to handle function code read holding registers
ret@
*/
static enum error_code handler_for_fcode_read_holding_registers(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t*pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_UNKNOWN;
	const modbus_t *pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
	modbus_mapping_t *pt_modbus_mapping = NULL;
	uint16_t kp_starting_addr = 0;
	enum addr_group kp_addr_gp = ADDR_GP_HOLDING_REGISTER;

	uint16_t kp_quantity = 0;
	bool flg_exception = false;

	kp_starting_addr = app_mslave_pick_starting_address( pt_pktbuf,  len);

	// to check the quantity
	kp_quantity = app_mslave_pick_quantity( pt_pktbuf, len);
	if((kp_quantity <= 0x0)||(kp_quantity > 0x007D))
	{
		DBG_LOG_ERR("invalid quantity to be read %d", kp_quantity);
		//MK1917U
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1917D
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
		
		tpret = EMBXILVAL;
		goto EXIT;
	}

	// to check the address range
	if((kp_starting_addr + kp_quantity) > 0xFFFF)
	{
		DBG_LOG_ERR("Reading operation is out of range");
		//MK1933U
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf,  MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1933D
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);

		tpret = EMBXILADD;
		goto EXIT;
	}

	//MK690U____________________
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
	pt_modbus_mapping = app_mslave_locate_mapping( pt_mslave_ctr, kp_starting_addr, kp_addr_gp);
	if(NULL == pt_modbus_mapping)
	{
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);

		DBG_LOG_ERR("fail to find the mapping for starting_addr@0x%x", kp_starting_addr);
		//MK1953U___
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK1953D___
		flg_exception = true;
		
		tpret = EMBXILADD;
		goto EXIT;
		
	}

	int32_t kp_saved_errno = errno;
	errno = 0;
	//MK1968U___
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
	modbus_reply( pt_mctx, pt_pktbuf, len, pt_modbus_mapping);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK1968D___
	if(0 != errno)
	{
		flg_exception = true;
	}
	else
	{
		errno = kp_saved_errno;
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	//MK690D____________________

	if(true == flg_exception)
	{
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
	}
	
	EXIT:
		return tpret;
}



/*to pick the sub-fucntion code for function-code diagnostics(0x08)
ret@ the sub-function code
*/
static uint16_t to_pick_sub_fcode_for_disagnostic(const uint8_t *pt_pkt, const uint32_t len)
{
	uint16_t tpret = MODBUS_SFC_UNDEFINED;

	if((pt_pkt)&&(len >= MIN_RTU_PKT))
	{
		tpret = (pt_pkt[2] << 8) | pt_pkt[3];
	}
	
	return tpret;
}

/*to handle function-code Diagnostics (0x08)
ret@ error code
*/
static enum error_code  handler_for_fcode_diagnostics(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t *pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_NONE;

	const modbus_t *pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
	
	uint16_t kp_sfcode = MODBUS_SFC_UNDEFINED;
	
	uint8_t pt_respbuf[SZ_RTU_MSGBUF] = {0};
	uint32_t kp_dtlen = 0;
	uint16_t tpu16 = 0;

	util_dbg_buf_dump( pt_pktbuf, len);

	kp_sfcode = to_pick_sub_fcode_for_disagnostic( pt_pktbuf, len);

	
	switch(kp_sfcode)
	{
		case MODBUS_SFC_DIAG_RETURN_QUERY_DATA:
		{ //0x0000 "The entire response message should be identical to the request"
			kp_dtlen = len - MODBUS_RTU_CHECKSUM_LENGTH;
			memcpy( pt_respbuf, pt_pktbuf, kp_dtlen);
			//MK2037U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2027D
			break;
		}
		case MODBUS_SFC_DIAG_RESTART_COM_OPT:
		{ // 0x0001
			//DBG_LOG_ERR("TODO==================to complete");
			/*
			The remote device serial line port must be initialized and restarted, and all of its
			communications event counters are cleared. If the port is currently in Listen Only Mode, no
			response is returned. This function is the only one that brings the port out of Listen Only
			Mode. If the port is not currently in Listen Only Mode, a normal response is returned. This
			occurs before the restart is executed.
			When the remote device receives the request, it attempts a restart and executes its power–up
			confidence tests. Successful completion of the tests will bring the port online.
			A request data field contents of FF 00 hex causes the port’s Communications Event Log to be
			cleared also. Contents of 00 00 leave the log as it was prior to the restart.
			*/			


			/* TODO=======to reset/clear the counters */			
			app_mslave_clear_state_set( pt_mslave_ctr);
			#if(0)
			if(SMODE_LISTEN_ONLY ==  app_mslave_get_mode( pt_mslave_ctr))
			{// to bring the Port out ot Listen only mode	; no reponse is expected here		
				app_mslave_set_mode( pt_mslave_ctr, SMODE_ACTIVE);
			}
			else
			{	
				//MK2070U
				pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
				modbus_reply_exception( app_mslave_get_mctx( pt_mslave_ctr), pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_FUNCTION );
				pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
				//MK2070D
			}
			#else			
				app_mslave_set_mode( pt_mslave_ctr, SMODE_ACTIVE);
				// send the response
				pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
				pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
				kp_dtlen = len - MODBUS_RTU_CHECKSUM_LENGTH;//2;
				memcpy( pt_respbuf, pt_pktbuf, kp_dtlen);
				app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen );
				pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			#endif

			break;
		}
		case MODBUS_SFC_DIAG_RET_DIAG_REG:
		{ //0x0002 "device’s 16–bit diagnostic register are returned in the response"

			tpu16 = app_mslave_get_diagnostic_register( pt_mslave_ctr);
		
			// slave ID
			pt_respbuf[0] = pt_pktbuf[0];
			kp_dtlen++;
			//function-code 
			pt_respbuf[1] = pt_pktbuf[1];
			kp_dtlen++;
			//sub-function code
			pt_respbuf[2] = pt_pktbuf[2];
			kp_dtlen++;
			pt_respbuf[3] = pt_pktbuf[3];
			kp_dtlen++;
			#if(0)
			// diagnostic regise
			pt_respbuf[4] = (tpu16 >> 8) & 0xFF; // H-Byte
			kp_dtlen++;
			pt_respbuf[5] = tpu16 & 0xFF; // L-Byte
			kp_dtlen++;
			#else
				//to_set_uint16(const uint16_t val, uint16_t * ptdes)
				to_set_uint16( tpu16, &pt_respbuf[4]);
				kp_dtlen++;
				kp_dtlen++;
			#endif
			//CRC
			//tpu16 = util_tool_cal_crc16( pt_respbuf, kp_dtlen);
			//pt_respbuf[6] = (tpu16 >> 8) & 0xFF; // H-Byte
			//kp_dtlen++;
			//pt_respbuf[7] = tpu16 & 0xFF;
			//kp_dtlen++;
			
			//MK2106U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);	
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2106D
			break;
		}
		case MODBUS_SFC_DIAG_CHANGE_ASCII_INPUT_DELIMITER:
		{ // 0x0003
			// to update the input delimiter
			pt_mslave_ctr->ch_input_delimiter = pt_pktbuf[4];

			kp_dtlen = len - MODBUS_RTU_CHECKSUM_LENGTH;
			memcpy( pt_respbuf, pt_pktbuf, kp_dtlen);

			//MK2123U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2123D
			
			break;
		}
		case MODBUS_SFC_FORCE_LISTEN_MODE:
		{ /*0x0004 to enable listen-mode; No response is returned */
			app_mslave_set_mode( pt_mslave_ctr, SMODE_LISTEN_ONLY);
			DBG_LOG_WARN(" the slave is set to listen-only mode");
			break;
		}
		case MODBUS_SFC_CLEAR_COUNTER_REGISTER:
		{ // 0x000A
			app_mslave_clear_state_set( pt_mslave_ctr);
			DBG_LOG_WARN(" counters and diagnostic resgiter cleared");
			kp_dtlen = len - MODBUS_RTU_CHECKSUM_LENGTH;
			memcpy( pt_respbuf, pt_pktbuf, kp_dtlen);
			//MK2144
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2144
			break;	
		}
		case MODBUS_SFC_RET_BUS_MSG_CNT:
		{// 0x00B
			// slave ID
			pt_respbuf[0] = pt_pktbuf[0];
			kp_dtlen ++;
			//fun-code
			pt_respbuf[1] = pt_pktbuf[1];
			kp_dtlen ++;
			//sub-fun code
			pt_respbuf[2] = pt_pktbuf[2];
			kp_dtlen ++;
			pt_respbuf[3] = pt_pktbuf[3];
			kp_dtlen ++;
			//msg count			
			tpu16 = app_mslave_get_bus_msg_cnt( pt_mslave_ctr);
			#if(0)
			pt_respbuf[4] = (tpu16 >> 8) & 0xFF;
			kp_dtlen ++;
			pt_respbuf[5] = tpu16 & 0xFF;
			kp_dtlen ++;
			#else
				//to_set_uint16(const uint16_t val, uint16_t * ptdes)
				to_set_uint16( tpu16, &pt_respbuf[4]);
				kp_dtlen ++;
				kp_dtlen ++;
			#endif
			
			// CRC
			//tpu16 = util_tool_cal_crc16( pt_respbuf, kp_dtlen);
			//pt_respbuf[6] = (tpu16 >> 8) & 0xFF;
			//kp_dtlen ++;
			//pt_respbuf[7] = tpu16 & 0xFF;
			//kp_dtlen ++;
			//MK2177U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2177D
			break;
		}
		case MODBUS_SFC_RET_BUS_COM_ERR_CNT:
		{ //0x000C
			//slave-ID
			pt_respbuf[0] = pt_pktbuf[0];
			kp_dtlen ++;
			// fun-code
			pt_respbuf[1] = pt_pktbuf[1];
			kp_dtlen ++;
			// sub-fun code
			pt_respbuf[2] = pt_pktbuf[2];
			kp_dtlen ++;
			pt_respbuf[3] = pt_pktbuf[3];
			kp_dtlen ++;
			// bus communication error count
			tpu16 = app_mslave_get_bus_comm_error_cnt( pt_mslave_ctr);
			#if(0)
			pt_respbuf[4] = (tpu16 >> 8) & 0xFF;
			kp_dtlen ++;
			pt_respbuf[5] = tpu16 & 0xFF;
			kp_dtlen ++;
			#else
				//to_set_uint16(const uint16_t val, uint16_t * ptdes)
				to_set_uint16( tpu16, &pt_respbuf[4]);
				kp_dtlen ++;
				kp_dtlen ++;
			#endif
			// CRC
			//tpu16 = util_tool_cal_crc16( pt_respbuf, kp_dtlen);
			//pt_respbuf[6] = (tpu16 >> 8) & 0xFF;
			//kp_dtlen ++;
			//pt_respbuf[7] = tpu16 & 0xFF;
			//kp_dtlen ++;
			//MK2210U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2210D
			break;
		}
		case MODBUS_SFC_RET_BUS_EXCEPT_ERR_CNT://	(0x000DU)
		{ //0x000D
			//slave-ID
			pt_respbuf[0] = pt_pktbuf[0];
			kp_dtlen ++;
			// fun-code
			pt_respbuf[1] = pt_pktbuf[1];
			kp_dtlen ++;
			// sub-fun code
			pt_respbuf[2] = pt_pktbuf[2];
			kp_dtlen ++;
			pt_respbuf[3] = pt_pktbuf[3];
			kp_dtlen ++;
			//bus exception error count
			tpu16 = app_mslave_get_slave_exception_error_cnt( pt_mslave_ctr);
			#if(0)
			pt_respbuf[4] = (tpu16 >> 8) & 0xFF;
			kp_dtlen ++;
			pt_respbuf[5] = tpu16 & 0xFF;
			kp_dtlen ++;
			#else
				//to_set_uint16(const uint16_t val, uint16_t * ptdes)
				to_set_uint16( tpu16, &pt_respbuf[4]);
				kp_dtlen ++;
				kp_dtlen ++;
			#endif
			// CRC 
			//tpu16 = util_tool_cal_crc16( pt_respbuf, kp_dtlen);
			//pt_respbuf[6] = (tpu16 >> 8) & 0xFF;
			//kp_dtlen ++;
			//pt_respbuf[7] = tpu16 & 0xFF;
			//kp_dtlen ++;
			//MK2243U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2243D
			break;
		}
		case MODBUS_SFC_RET_BUS_SERVER_MSG_CNT://	(0x000EU)
		{// 0x000E
			// slave-ID
			pt_respbuf[0] = pt_pktbuf[0];
			kp_dtlen++;
			// fun-code
			pt_respbuf[1] = pt_pktbuf[1];
			kp_dtlen ++;
			// sub-fun code
			pt_respbuf[2] = pt_pktbuf[2];
			kp_dtlen ++;
			pt_respbuf[3] = pt_pktbuf[3];
			kp_dtlen ++;
			// server/slave msg count
			tpu16 = app_mslave_get_slave_msg_cnt( pt_mslave_ctr);
			#if(0)
			pt_respbuf[4] = (tpu16 >> 8) & 0xFF;
			kp_dtlen ++;
			pt_respbuf[5] = tpu16 & 0xFF;
			kp_dtlen ++;
			#else
				//to_set_uint16(const uint16_t val, uint16_t * ptdes)
				to_set_uint16( tpu16, &pt_respbuf[4]);
				kp_dtlen ++;
				kp_dtlen ++;
			#endif
			// CRC
			//tpu16 = util_tool_cal_crc16( pt_respbuf, kp_dtlen);
			//pt_respbuf[6] = (tpu16 >> 8) & 0xFF;
			//kp_dtlen ++;
			//pt_respbuf[7] = tpu16 & 0xFF;
			//kp_dtlen ++;
			//MK2276U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2276D
			break;
		}
		case MODBUS_SFC_RET_SERVER_NO_RESP_CNT://	(0x000FU)
		{// 0x000F			
			kp_dtlen = 0;
			//slave ID
			pt_respbuf[ kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
			kp_dtlen ++;
			//function code
			pt_respbuf[ kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[1];
			kp_dtlen ++;
			//sub-func code
			pt_respbuf[ kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[2];
			kp_dtlen ++;
			pt_respbuf[ kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[3];
			kp_dtlen ++;
			// server/slave no response cnt
			tpu16 = app_mslave_get_slave_no_resp_cnt( pt_mslave_ctr);
			#if(0)
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = ( tpu16 >> 8 ) & 0xFF;
			kp_dtlen ++;
			pt_respbuf[ kp_dtlen % SZ_RTU_MSGBUF] = tpu16 & 0xFF;
			kp_dtlen ++;
			#else
				//to_set_uint16(const uint16_t val, uint16_t * ptdes)
				to_set_uint16( tpu16, &pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF]);
				kp_dtlen ++;
				kp_dtlen ++;
			#endif
			
			//CRC
			//tpu16 = util_tool_cal_crc16( pt_respbuf, kp_dtlen);
			//pt_respbuf[ kp_dtlen % SZ_RTU_MSGBUF] = (tpu16 >> 8) & 0xFF;
			//kp_dtlen ++;
			//pt_respbuf[ kp_dtlen % SZ_RTU_MSGBUF] = tpu16 & 0xFF;
			//kp_dtlen ++;
			//MK2309U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2309D
			break;
		}
		case MODBUS_SFC_RET_SERVER_NAK_CNT://	(0x0010U)
		{ // 0x0010
			kp_dtlen = 0;
			// slave-ID
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
			kp_dtlen ++;
			// function code
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[1];
			kp_dtlen ++;
			// sub-func code
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[2];
			kp_dtlen ++;
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[3];
			kp_dtlen ++;
			//server/salve NAK cnt
			tpu16 = app_mslave_get_slave_nak_cnt( pt_mslave_ctr);
			#if(0)
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = (tpu16 >> 8) & 0xFF;
			kp_dtlen ++;
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = tpu16 & 0xFF;
			kp_dtlen ++;
			#else
				//to_set_uint16(const uint16_t val, uint16_t * ptdes)
				to_set_uint16( tpu16, &pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF]);
				kp_dtlen ++;
				kp_dtlen ++;
			#endif
			// CRC
			//tpu16 = util_tool_cal_crc16( pt_respbuf, kp_dtlen);
			//pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
			//kp_dtlen ++;
			//pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
			//kp_dtlen ++;
			//MK2343U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2343
			break;
		}
		case MODBUS_SFC_RET_SERVER_BUSY_CNT://	(0x0011U)
		{ // 0x0011
			kp_dtlen = 0;
			// slave-ID
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
			kp_dtlen ++;
			// function code
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[1];
			kp_dtlen ++;
			// sub-func code
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[2];
			kp_dtlen ++;
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[3];
			kp_dtlen ++;
			//server/salve NAK cnt
			
			tpu16 = app_mslave_get_slave_busy_cnt( pt_mslave_ctr);
			#if(0)
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = (tpu16 >> 8) & 0xFF;
			kp_dtlen ++;
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = tpu16 & 0xFF;
			kp_dtlen ++;
			#else	
				//to_set_uint16(const uint16_t val, uint16_t * ptdes)
				to_set_uint16( tpu16, &pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF]);
				kp_dtlen ++;
				kp_dtlen ++;
			#endif
			// CRC
			//tpu16 = util_tool_cal_crc16( pt_respbuf, kp_dtlen);
			//pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
			//kp_dtlen ++;
			//pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
			//kp_dtlen ++;
			//MK2377U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2377D
			break;
		}
		case MODBUS_SFC_RET_RET_BUS_CHAR_OVERRUN_CNT://	(0x0012U)
		{ // 0x0012
			kp_dtlen = 0;
			// slave-ID
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
			kp_dtlen ++;
			// function code
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[1];
			kp_dtlen ++;
			// sub-func code
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[2];
			kp_dtlen ++;
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[3];
			kp_dtlen ++;
			//server/salve NAK cnt
			tpu16 = app_mslave_get_slave_bus_charac_overrun_cnt( pt_mslave_ctr);
			#if(0)
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = (tpu16 >> 8) & 0xFF;
			kp_dtlen ++;
			pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = tpu16 & 0xFF;
			kp_dtlen ++;
			#else
				//to_set_uint16(const uint16_t val, uint16_t * ptdes)
				to_set_uint16( tpu16, &pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF]);
				kp_dtlen ++;
				kp_dtlen ++;
			#endif
			// CRC
			//tpu16 = util_tool_cal_crc16( pt_respbuf, kp_dtlen);
			//pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
			//kp_dtlen ++;
			//pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
			//kp_dtlen ++;
			//MK2411U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2411
			break;
		}
		/*reserved*/
		case MODBUS_SFC_CLEAR_OVERRUN_CNT_FLG://	(0x0014U)
		{ // 0x0014
			DBG_LOG_ERR("Todo=====to clear the overrun error cnt and reset the error flag");
			
			kp_dtlen = len - MODBUS_RTU_CHECKSUM_LENGTH;
			memcpy( pt_respbuf, pt_pktbuf, kp_dtlen);
			//MK2426U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			app_modbus_com_send_msg( pt_mctx, pt_respbuf, kp_dtlen);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2426D
			break;
		}
		default:
		{
			//MK2436U
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
			modbus_reply_exception( pt_mctx, pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2436
			app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
			
			tpret =  EMBXILFUN;
			break;
		}
	}
	
	return tpret;
}

/*0x08-0x0001, restart communication option
ret@error_code

NOTE!! 0x08-0x0001 (restart communication option) is the only request that can be answered in listen-only mode

*/
static enum error_code handle_for_restart_com_option_in_listen_mode(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t*pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_NONE;
	
	uint8_t kp_fcode = 0;
	uint16_t kp_sfcode = MODBUS_SFC_UNDEFINED;
	
	kp_fcode = app_mslave_pick_fun_code( pt_pktbuf, len);

	if(MODBUS_FC_DIAGNOSTIC != kp_fcode)
	{ // 
		goto EXIT;
	}
	DBG_LOG_WARN("DIAGNOSTIC function code is received");

	kp_sfcode = to_pick_sub_fcode_for_disagnostic( pt_pktbuf, len);
	if(MODBUS_SFC_DIAG_RESTART_COM_OPT != kp_sfcode)
	{
		goto EXIT;
	}

	DBG_LOG_WARN("diagnostic sub-function 0x%x is received", kp_sfcode);

	// to clear the counter and bring the slave out of listen-only mode
	app_mslave_clear_state_set( pt_mslave_ctr);
	app_mslave_set_mode( pt_mslave_ctr, SMODE_ACTIVE);		
	
	EXIT:
		return tpret;
}

/* 0x11
ret@error_code
*/
static enum error_code  handler_for_fcode_report_slave_id(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t *pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_API_FAIL;
	
	uint8_t pt_respbuf[SZ_RTU_MSGBUF] = {0};
	uint32_t kp_dtlen = 0;
	
	DBG_LOG_ERR("Todo============================to complete");
	/*.
	" A MODBUS request packed with function code 17 Report Slave ID will generate a response that can include
	information about the MODBUS server device in addition to the ID. The information provided is device-specific. ""

	ref@https://www.sommer.at/support-center-IDS-20s/en-us/Content/Product/Communication/Modbus/Functions/Report-slave-ID/cnpt-report-slave-ID.htm?TocPath=Manual%7CCommunication%7CModbus%7CModbus%20commands%20and%20registers%7C_____3

	*/
	//MK2441U
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	modbus_reply_exception( app_mslave_get_mctx( pt_mslave_ctr), pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_FUNCTION );
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK2441D
	app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
	
	return tpret;
}

/* 0x07
ref@error_code
*/
static enum error_code handler_for_fcode_read_exception_status(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t*pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_NONE;

	uint8_t pt_respbuf[SZ_RTU_MSGBUF] = {0};
	uint32_t kp_dtlen = 0;
	uint8_t tpu8 = 0;

	kp_dtlen = 0;
	//slave-ID
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
	kp_dtlen ++;

	//function code 
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[1];
	kp_dtlen ++;

	//status
	tpu8 = app_mslave_get_exception_status( pt_mslave_ctr);
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = tpu8;
	kp_dtlen ++;

	//MK2476U
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	app_modbus_com_send_msg( app_mslave_get_mctx( pt_mslave_ctr), pt_respbuf, kp_dtlen);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK2476D
	return tpret;
}

/* 0x
ret@error_code
*/
static enum error_code  handler_for_fcode_read_fifo_queue(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t *pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_API_FAIL;

	DBG_LOG_ERR("Todo============================to complete");
	//MK2492U
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	modbus_reply_exception( app_mslave_get_mctx( pt_mslave_ctr), pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_FUNCTION );
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK2492D
	app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
	
	return tpret;
}

/*to release the buf-node; remove it from the queue then release the memory
ret @ error_code
*/
static enum error_code to_release_buf_node(struct modbus_slave_controller*pt_mslave_ctr, struct freading_buffer*pt_freading_buf)
{
	enum error_code tpret  = ERR_NONE;

	if(l_queue_remove( pt_mslave_ctr->freading_buf_queue, (void *)pt_freading_buf))
	{
		l_free(pt_freading_buf);
	}
	else
	{
		DBG_LOG_ERR("fail to remove queue-node");
		tpret = ERR_API_FAIL;
	}
	return tpret;
	
}

/*to create new file-reading buffer node; allocate the memory, read the content from the file , then add it to the queue
ret@ the new buf-node is returned when it is created successfully
*/
static struct freading_buffer *to_create_buf_node(struct modbus_slave_controller*pt_mslave_ctr, struct file_reading_info_pkt*pt_freading_info_pkt)
{
	struct freading_buffer *ptret = NULL;

	ptret = l_malloc(sizeof(struct freading_buffer));
	if(NULL == ptret)
	{
		DBG_LOG_ERR(" fail to allocate memory");
		goto ERR;
	}
	memset( ptret, 0, sizeof(struct freading_buffer));

	#if(1)
		// for debug only
		uint16_t tpcnt = 0, tpidx = 0;
		uint8_t tpu8 = 0;
		
		DBG_LOG_WARN("only for debug");
		ptret->file_len = 0xFF00; // the file length
		ptret->file_no = pt_freading_info_pkt->file_no;
		ptret->record_no = pt_freading_info_pkt->record_no;
		//ptret->record_len = ?;
		ptret->tstamp = time(NULL);

		tpu8 = (pt_freading_info_pkt->record_no*2) % 0xFF;
		tpcnt = pt_freading_info_pkt->record_no * 2;
		if(tpcnt >= ptret->file_len)
		{
			DBG_LOG_ERR("file-reading is out of range");
			goto ERR;
		}
		
		while(1)
		{
			ptret->dtbuf[tpidx % SZ_FREADING_BUFFER] = tpu8;
			tpu8 ++;
			tpcnt ++;
			tpidx ++;
			if(tpidx >= SZ_FREADING_BUFFER)
			{
				DBG_LOG_WARN("the file reading buffer is full");
				break;
			}
			if(tpcnt >= ptret->file_len )
			{
				DBG_LOG_WARN("the ending of file is read");
				break;
			}
		}
		ptret->record_len = tpidx / 2 + tpidx % 2;
	#endif


	// to put the buffer-node on queue
	if(true != l_queue_push_tail( pt_mslave_ctr->freading_buf_queue, (void *)ptret))
	{
		DBG_LOG_ERR("fail to put freading-buffer on queue");
		goto ERR;
	}
	DBG_LOG_INFO(" a new file-reading buffer is created");
	
	return ptret;
	ERR:
		if(ptret)
		{
			l_free( ptret);
			ptret = NULL;
		}
		return ptret;
}


/*
typedef bool (*l_queue_match_func_t) (const void *data, const void *user_data);
*/
static bool q_match_freading_buffer(const void *data, const void *user_data)
{
	bool tpret = false;
	const struct freading_buffer *pt_freading_buf = (const struct freading_buffer*)data;
	uint16_t kp_file_no = (uint16_t)user_data;

	if(kp_file_no == pt_freading_buf->file_no)
	{
		tpret = true;
	}
	
	return tpret;
}

/* to find the file-reading buffer node
ret@ the pointer points to the buffer node is returned, NULL if nothing is found
*/
static struct freading_buffer *to_find_buf_node( struct modbus_slave_controller*pt_mslave_ctr, const uint16_t file_no)
{
	struct freading_buffer *ptret = NULL;

	ptret = l_queue_find( pt_mslave_ctr->freading_buf_queue, q_match_freading_buffer, (const void * )file_no);
		
	return ptret;
}

/* to check if the file-record to be read is in the buffer-node
ret@ true is returned when it is
*/
static bool is_records_in_buffer_node(struct file_reading_info_pkt *pt_freading_info_pkt, struct freading_buffer *pt_freading_buf)
{
	bool tpret = false;
	uint16_t tpu16a = 0, tpu16b = 0;

	tpu16a = pt_freading_buf->record_no;
	tpu16b = pt_freading_buf->record_no + pt_freading_buf->record_len;
	
	if(pt_freading_buf->record_len < (SZ_FREADING_BUFFER / 2 ))
	{ // the last block file is in the buffer-node; any file-block will full-fill the buffer except the last one.
		if((pt_freading_info_pkt->record_no >= tpu16a) && (pt_freading_info_pkt->record_no < tpu16b))
		{
			tpret = true;
		}
	}
	else if(pt_freading_buf->record_len == (SZ_FREADING_BUFFER / 2))
	{
		if((pt_freading_info_pkt->record_no >= tpu16a) && ( (pt_freading_info_pkt->record_no + pt_freading_info_pkt->record_len) < tpu16b))
		{
			tpret = true;
		}
	}
	else
	{
		DBG_LOG_ERR("Oops! this is not expected");
	}

	return tpret;
}

/*
typedef bool (*l_queue_remove_func_t) (void *data, void *user_data);
ret@true

NOTE !!! the buffer-node is released here

*/
static bool q_foreach_remove_buf_node(void *data, void *user_data)
{
	bool tpret = false;
	struct freading_buffer *pt_freading_buf = (struct freading_buffer*)data;
	time_t kp_tval = (time_t)user_data;
	if(NULL == pt_freading_buf)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}

	if((kp_tval - pt_freading_buf->tstamp) > VAL_BUF_NODE_LIFE)
	{
		l_free( pt_freading_buf);
		pt_freading_buf = NULL;
		
		tpret = true;
	}
	return tpret;
}


/*to release aging freading-buffer node
ret@error_code
*/
static enum error_code to_release_aging_buf_node(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t tpu32 = 0;
	time_t kp_tval = time(NULL);
	
	tpu32 = l_queue_foreach_remove( pt_mslave_ctr->freading_buf_queue, q_foreach_remove_buf_node, (void *)kp_tval);
	DBG_LOG_WARN("%d freaing-buffer node is released for aging", tpu32);

	return tpret;
}


/*to pick the data from a file for response
ret@
*/
static enum error_code to_read_file_content(struct modbus_slave_controller*pt_mslave_ctr, struct file_reading_info_pkt *pt_freading_info_pkt)
{
	enum error_code tpret = ERR_NONE;

	struct freading_buffer *pt_freading_buf = NULL;
	uint16_t tpidx = 0, tplen = 0;
	
	/*to continue.....
	1. create a buffer-queue for file reading; release the node when it is too old
	2. read the file content , and fill pt_freading_info_pkt.dtbuf with the data read
	3. to check the buffer-queue first for any reading-request; then read a block of data from the file
	   being read if no buffer for it;
	*/
	//__to_continue____________

	//MK1856U_____________________________
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_freading_buf_queue);

	pt_freading_buf = to_find_buf_node( pt_mslave_ctr, pt_freading_info_pkt->file_no);
	if(NULL == pt_freading_buf)
	{// no file-reading buffer is found
		pt_freading_buf = to_create_buf_node( pt_mslave_ctr, pt_freading_info_pkt);
		if(NULL == pt_freading_buf)
		{
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_freading_buf_queue);
			DBG_LOG_ERR("fail to create a new buffer node");
			tpret = ERR_API_FAIL;
			goto EXIT;	
		}
		// to make sure the data  to be read is in the new buffer-node
		if(true != is_records_in_buffer_node( pt_freading_info_pkt,  pt_freading_buf))
		{
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_freading_buf_queue);
			DBG_LOG_ERR(" the data to read is out of range, records_no %d, record_len %d", pt_freading_info_pkt->record_no, pt_freading_info_pkt->record_len);
			tpret = ERR_API_FAIL;
			goto EXIT;	
		}
	}
	else
	{ // the file-reading is found
	
		if(true != is_records_in_buffer_node( pt_freading_info_pkt,  pt_freading_buf))
		{ // the file-records to be read is out the range of the buffer node, a new one is expected to be created

			tpret = to_release_buf_node( pt_mslave_ctr,  pt_freading_buf);
			if(ERR_NONE != tpret)
			{
				pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_freading_buf_queue);
				DBG_LOG_ERR("fail to release buf-node, this is not suppose to happen");
				goto EXIT;							
			}
			else
			{
				pt_freading_buf = NULL;
				pt_freading_buf = to_create_buf_node( pt_mslave_ctr, pt_freading_info_pkt);
				if(NULL == pt_freading_buf)
				{
					pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_freading_buf_queue);
					DBG_LOG_ERR("fail to create new buf-node");
					tpret = ERR_API_FAIL;
					goto EXIT;							
				}
				
			}
			//to make sure the data to be read is in the new buf-node
			if(true != is_records_in_buffer_node( pt_freading_info_pkt, pt_freading_buf))
			{ // out of range again; too much data to read or some unexpected reason		
				pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_freading_buf_queue);
				DBG_LOG_ERR("some unexpected error");
				tpret = ERR_API_FAIL;
				goto EXIT;
			}
		}	
	}

	DBG_LOG_WARN("to fill the buffer");
	// to fill the buffer
	tpidx = (pt_freading_info_pkt->record_no - pt_freading_buf->record_no) * 2;
	if(pt_freading_buf->record_len < (SZ_FREADING_BUFFER / 2))
	{
		if((pt_freading_info_pkt->record_no + pt_freading_info_pkt->record_len) > (pt_freading_buf->record_no + pt_freading_buf->record_len))
		{
			tplen = ((pt_freading_buf->record_no + pt_freading_buf->record_len) - pt_freading_info_pkt->record_no) * 2;
		}
		else
		{
			tplen = pt_freading_info_pkt->record_len * 2;
		}
	}
	else if(pt_freading_buf->record_len == (SZ_FREADING_BUFFER / 2))
	{
		tplen = pt_freading_info_pkt->record_len * 2;
	}
	else
	{
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_freading_buf_queue);
		DBG_LOG_ERR("you are not supposed to see this output");
		tpret = ERR_API_FAIL;
		goto EXIT;
	}
	if((tplen > 0)&&(tplen <= SZ_RTU_MSGBUF))
	{
		DBG_LOG_WARN("memcpy tpidx = %d, tplen = %d", tpidx, tplen);
			
		memcpy( pt_freading_info_pkt->dtbuf, &pt_freading_buf->dtbuf[tpidx], tplen);
		pt_freading_info_pkt->dtlen = tplen;
	}
	else
	{
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_freading_buf_queue);
		DBG_LOG_ERR("unexpected situation");
		tpret = ERR_API_FAIL;
		goto EXIT;
	}

	// to update the timestamp
	pt_freading_buf->tstamp = time(NULL);
	
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_freading_buf_queue);
	//MK1856D_____________________________

	EXIT:
		DBG_LOG_WARN("to release the aging buffer node");
		
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_freading_buf_queue);
		to_release_aging_buf_node( pt_mslave_ctr);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_freading_buf_queue);

		return tpret;
}

/* 0x14 to handle file-reading request
ret@error_code

NOTE!!
1. the end of file is reached when the number of record returned is smaller than the number of record requested;
2. we use several file-number for a file reading operation when the file is too big;
3. an exception code is returned when the record-number is out the range of the file being read;
4.arrange of file content;  <file-length 4B> | <file-content....> ; which means record-0 and record-1 is the file length

....

*/
static enum error_code  handler_for_fcode_read_file_record(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t *pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_NONE;
	uint8_t pt_respbuf[SZ_RTU_MSGBUF] = {0};
	uint32_t kp_dtlen = 0;
	
	uint32_t kp_cnt = 0;

	const uint32_t kp_req_pattern_base = 3;
	int32_t kp_exception_code = 0; 
	
	uint8_t kp_req_bcount = 0;
		
	struct file_reading_info_pkt kp_freading_info_pkt = {0};	
	//DBG_LOG_ERR("Todo============================to complete");

	if((NULL == pt_mslave_ctr)||(NULL == pt_pktbuf))
	{
		DBG_LOG_ERR("invalid parameter");
		kp_exception_code = MODBUS_EXCEPTION_NOT_DEFINED;
		tpret = ERR_INVALID_PARAM;
		goto ERR;
	}

	kp_dtlen = 0;
	//slave-ID
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
	kp_dtlen ++;
	// function-code
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[1];
	kp_dtlen++;
	//resp data-length
	//pt_respbuf[] = ????
	kp_dtlen ++; // NOTE!! we set this value when all data is read
	
	//to fill the file-record
	kp_req_bcount = pt_pktbuf[2];
	kp_cnt = 0;
	if(( kp_req_bcount < 0x07)||(kp_req_bcount > 0xF5))
	{
		DBG_LOG_ERR("invalid request byte-count");
		kp_exception_code =  MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
		tpret = ERR_INVALID_PARAM;
		goto ERR;
	}
	if(0 != (kp_req_bcount % 0x07)) // 0x07 = 1 + 2 _ 2 + 2
	{
		DBG_LOG_ERR("invalid request byte-count");
		kp_exception_code = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
		tpret = ERR_INVALID_PARAM;
		goto ERR;
	}
	
	while(1)
	{
		if(kp_cnt >= kp_req_bcount)
		{// all request-pattern is handled
			break;
		}
		memset( &kp_freading_info_pkt, 0, sizeof(struct file_reading_info_pkt));
		kp_freading_info_pkt.ref_type = pt_pktbuf[kp_req_pattern_base + kp_cnt];
		kp_cnt ++;
		kp_freading_info_pkt.file_no =  (pt_pktbuf[kp_req_pattern_base + kp_cnt] << 8) + pt_pktbuf[kp_req_pattern_base + kp_cnt + 1];
		kp_cnt = kp_cnt + 2;
		kp_freading_info_pkt.record_no = (pt_pktbuf[kp_req_pattern_base + kp_cnt] << 8) + pt_pktbuf[kp_req_pattern_base + kp_cnt + 1];
		kp_cnt = kp_cnt + 2;
		kp_freading_info_pkt.record_len = (pt_pktbuf[kp_req_pattern_base + kp_cnt] << 8) + pt_pktbuf[kp_req_pattern_base + kp_cnt + 1];
		kp_cnt = kp_cnt + 2;
		if(FILE_READ_REF_TYPE != kp_freading_info_pkt.ref_type)
		{
			DBG_LOG_ERR("invalid ref-type");
			kp_exception_code = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
			tpret = ERR_INVALID_PARAM;
			goto ERR;
		}
		if(kp_freading_info_pkt.record_no > MAX_RECORD_NUMBER)
		{
			DBG_LOG_ERR("invlaid record-number");
			kp_exception_code = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
			tpret = ERR_INVALID_PARAM;
			goto ERR;
		}
		
		// to pick the data content
		tpret = to_read_file_content( pt_mslave_ctr, &kp_freading_info_pkt);
		if(ERR_NONE != tpret)
		{
			DBG_LOG_ERR("fail to file content");
			kp_exception_code = MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE;
			tpret = ERR_INVALID_PARAM;
			goto ERR;
		}
		
		// to put file-data info resp-buf
		pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = kp_freading_info_pkt.dtlen; // file resp-length
		kp_dtlen ++;
		pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = FILE_READ_REF_TYPE; // Ref.Type
		kp_dtlen ++;

		if((kp_dtlen + kp_freading_info_pkt.dtlen) <= (MAX_MODBUS_SERIAL_ADU - 2))
		{
			memcpy( &pt_respbuf[kp_dtlen], kp_freading_info_pkt.dtbuf, kp_freading_info_pkt.dtlen);
			kp_dtlen = kp_dtlen + kp_freading_info_pkt.dtlen;
		}
		else
		{
			DBG_LOG_ERR("too much data is requested");			
			kp_exception_code = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
			tpret = ERR_INVALID_PARAM;
			goto ERR;
		}
	}
	// to fill the resp data-length
	pt_respbuf[2] = kp_dtlen - 1 - 1 -1; // -1 for slave-ID, -1 for function-code, -1 for resp data-length

	//MK2966U
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	if(app_modbus_com_send_msg( app_mslave_get_mctx( pt_mslave_ctr), pt_respbuf, kp_dtlen) < 0)
	{		
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);

		DBG_LOG_ERR(" msg-sending fail");		
		kp_exception_code = MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE;
		tpret = ERR_API_FAIL;
		goto ERR;
	}
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK2966D

	return tpret;
	ERR:	
		//MK2982
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		modbus_reply_exception( app_mslave_get_mctx( pt_mslave_ctr), pt_pktbuf, kp_exception_code);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK2982
		app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
		
		return tpret;
}


/* 0x0B
ret@error_code
*/
static enum error_code  handler_for_fcode_get_comm_event_cnt(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t *pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_NONE;

	uint8_t pt_respbuf[SZ_RTU_MSGBUF] = {0};
	uint32_t kp_dtlen = 0;

	uint16_t tpu16 = 0;
	
	//DBG_LOG_ERR("Todo============================to complete");
	if((NULL == pt_mslave_ctr)||(NULL == pt_pktbuf))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	kp_dtlen = 0;
	//slave-ID
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
	kp_dtlen ++;
	// fun-code
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[1];
	kp_dtlen ++;
	// status 
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = 0x0;
	kp_dtlen ++;
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = 0x0;
	kp_dtlen ++;
	// COMM-evnet counter
	tpu16 = app_mslave_get_comm_event_cnt( pt_mslave_ctr);
	#if(0)
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = (tpu16 >> 8) & 0xFF;
	kp_dtlen ++;
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = tpu16 & 0xFF;
	kp_dtlen ++;
	#else
		//to_set_uint16(const uint16_t val, uint16_t * ptdes)
		to_set_uint16( tpu16, &pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF]);
		kp_dtlen ++;
		kp_dtlen ++;
	#endif
	
	//MK3097U
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	app_modbus_com_send_msg( app_mslave_get_mctx( pt_mslave_ctr), pt_respbuf, kp_dtlen);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK3097D
	return tpret;
}

/* 0x0C
ret@error_code
*/
static enum error_code  handler_for_fcode_get_comm_event_log(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t *pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_NONE;

	uint8_t pt_respbuf[SZ_RTU_MSGBUF] = {0};
	uint32_t kp_dtlen = 0;

	uint16_t tpu16 = 0;
	uint16_t kp_eventlog_cnt = 0;
	uint8_t pt_eventlog[MAX_COMM_EVENT_LOG] = {0};
	
	if((NULL == pt_mslave_ctr)||(NULL == pt_pktbuf))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	kp_dtlen = 0;
	//slave-ID
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[0];
	kp_dtlen ++;
	//function-code
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_pktbuf[1];
	kp_dtlen ++;
	//byte-count
	kp_eventlog_cnt = app_mslave_get_comm_event_log( pt_mslave_ctr, pt_eventlog);
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = kp_eventlog_cnt + 3 * 2;
	kp_dtlen ++;
	//status
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = 0x0;
	kp_dtlen ++;
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = 0x0;
	kp_dtlen ++;
	//event counter
	tpu16 = app_mslave_get_comm_event_cnt( pt_mslave_ctr);
	#if(0)
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = (tpu16 >> 8) & 0xFF;
	kp_dtlen ++;
	pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = tpu16 & 0xFF;
	kp_dtlen ++;
	#else
		//to_set_uint16(const uint16_t val, uint16_t * ptdes)
		to_set_uint16( tpu16, &pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF]);
		kp_dtlen ++;
		kp_dtlen ++;
	#endif
	
	//the events
	for(uint32_t i = 0;i < kp_eventlog_cnt; i++)
	{
		pt_respbuf[kp_dtlen % SZ_RTU_MSGBUF] = pt_eventlog[i];
		kp_dtlen ++;
	}
	//MK3155
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	app_modbus_com_send_msg( app_mslave_get_mctx( pt_mslave_ctr), pt_respbuf, kp_dtlen);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK3155
	return tpret;
}


/* 0x2B_0x0E
ret@error_code
*/
static enum error_code  handler_for_fcode_read_dev_id(struct modbus_slave_controller*pt_mslave_ctr, const uint8_t *pt_pktbuf, const uint32_t len)
{
	enum error_code tpret = ERR_API_FAIL;

	DBG_LOG_ERR("Todo============================to complete");
	//MK3172
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	modbus_reply_exception( app_mslave_get_mctx( pt_mslave_ctr), pt_pktbuf, MODBUS_EXCEPTION_ILLEGAL_FUNCTION );
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK3172
	return tpret;
}



/* the modbus main loop
*/
void app_mslave_mloop(struct modbus_slave_controller *pt_mslave_ctr)
{
	enum error_code kp_errcode = ERR_NONE;
	enum addr_group kp_addr_gp = ADDR_GP_UNKNOWN;
	uint8_t kp_fcode = 0;
	
	struct modbus_slave_controller *hld_mslave_ctr = app_mslave_get_ctr();
	
	uint8_t kp_msg_buf[SZ_RTU_MSGBUF] = {0};
	modbus_t *pt_mctx = NULL;
	modbus_mapping_t *hld_mslave_mapping = NULL;
	int32_t tpint = 0;
	uint32_t tpu32 = 0;

	//app_mslave_release_mapping( pt_mslave_ctr); // to release the old mapping before a new is created
	//app_mslave_create_mapping( hld_mslave_ctr); // create Modbus slave mapping

	/*
	hld_mctx = app_mslave_get_mctx( app_mslave_get_ctr());
	if(NULL == hld_mctx)
	{
		DBG_LOG_ERR("no valid modbus context available");
		goto ERR;
	}*/
	
	while(1)
	{		
		app_mslave_ipc_wait_for_update(hld_mslave_ctr);
		
		//modbus_receive(modbus_t * ctx, uint8_t * req)
		//DBG_LOG_INFO("the Modbus Slave main-loop is running!");		
		//sleep(1);
	#if(0)
	// the original
		//MK3325U__________________________________
		// to check and wait for Modbus-slave being updated
		tpu32 = 0;
		while(1)
		{
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			if(false == app_mslave_get_to_update_mslave( pt_mslave_ctr))
			{					
				pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);																
				break;
			}
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			tpu32 ++;
			if(tpu32 > 3000U)
			{
				DBG_LOG_WARN(" waiting is timeout!?");
				break;
			}				
			sched_yield();
			usleep(1000U); //1mS
		}
		//MK3325D__________________________________
	
		memset( kp_msg_buf, 0, sizeof(kp_msg_buf));
		//MK3036U_____________________
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
		//pt_mslave_ctr->is_waiting_for_recv = true;
		pt_mctx = app_mslave_get_mctx( app_mslave_get_ctr());
		tpint = modbus_receive( pt_mctx, kp_msg_buf);
		//pt_mslave_ctr->is_waiting_for_recv = false;
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		//MK3036D_____________________
	#else		
		memset( kp_msg_buf, 0, sizeof(kp_msg_buf));
		if(0 != pthread_mutex_trylock( &pt_mslave_ctr->mtx_for_port_conf))
		{
			DBG_LOG_WARN("fail to lock the mutex");			
			usleep(5000U);
			continue;
		}
		pt_mctx = app_mslave_get_mctx( pt_mslave_ctr);
		tpint = modbus_receive( pt_mctx, kp_msg_buf);
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);				
	#endif

		//DBG_LOG_WARN("number of bytes received is %d", tpint);

		// to update Bus Message Count
		app_mslave_inc_bus_msg_cnt( hld_mslave_ctr);

		//
		if(tpint <= 0)
		{
			//DBG_LOG_ERR("invlaid pkt len");
			
			//to update the no response count
			app_mslave_inc_slave_no_resp_cnt( pt_mslave_ctr);

			sched_yield();
			continue;
		}
		
		// to validate the msg pkt
		kp_errcode = app_mslave_validate_pkt( app_mslave_get_ctr(), kp_msg_buf, tpint);
		if(kp_errcode != ERR_NONE)
		{
			//DBG_LOG_ERR("invalid msg pkt is received");

			//DBG_LOG_ERR("TODO=======some exception-reply is expected here");

			//to update the no response count
			app_mslave_inc_slave_no_resp_cnt( pt_mslave_ctr);

			sched_yield();
			continue;
		}
	
		// to update the Server Message count
		app_mslave_inc_slave_msg_cnt( pt_mslave_ctr);

		// to check the Slave-Mode
		if(SMODE_LISTEN_ONLY == app_mslave_get_mode(hld_mslave_ctr))
		{// Listening-Mode, no response is expected from this slave
			DBG_LOG_WARN("In listening mode, no response");
			handle_for_restart_com_option_in_listen_mode( hld_mslave_ctr, kp_msg_buf, tpint);

			sched_yield();
			continue;
		}


		#if(1)
			kp_fcode = app_mslave_pick_fun_code( kp_msg_buf, tpint);
			switch(kp_fcode)
			{
				case MODBUS_FC_READ_COILS:
				{//0x01
					handler_for_fcode_read_coils( hld_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_READ_DISCRETE_INPUTS:
				{// 0x02
					handler_for_fcode_read_discrete_inputs( hld_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_READ_INPUT_REGISTERS:
				{//0x04
					handler_for_fcode_read_inputs_registers( hld_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_READ_HOLDING_REGISTERS:
				{//0x03
					handler_for_fcode_read_holding_registers( hld_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_REPORT_SLAVE_ID:
				{// 0x11
					handler_for_fcode_report_slave_id( pt_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_READ_EXCEPTION_STATUS:
				{ // 0x07
					handler_for_fcode_read_exception_status( pt_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_READ_FIFO_QUEUE:
				{//0x18
					handler_for_fcode_read_fifo_queue( pt_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_READ_FILE_RECORD:
				{//0x14
					handler_for_fcode_read_file_record( pt_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_DIAGNOSTIC:
				{// 0x08
					handler_for_fcode_diagnostics( hld_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_GET_COM_EVENT_COUNTER:
				{//0x0B
					handler_for_fcode_get_comm_event_cnt( pt_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_GET_COM_EVENT_LOG:
				{//0x0C
					handler_for_fcode_get_comm_event_log( pt_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case MODBUS_FC_READ_DEV_ID:
				{//0x2B
					handler_for_fcode_read_dev_id( pt_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				default:
				{
					//DBG_LOG_ERR("TODO======to reply an exception");
					DBG_LOG_ERR(" illegal function code");
					//MK3318
					pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);					
					pt_mctx = app_mslave_get_mctx( app_mslave_get_ctr());
					modbus_reply_exception( pt_mctx, kp_msg_buf, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
					pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
					//MK3318
					app_mslave_inc_slave_exception_error_cnt( pt_mslave_ctr);
					
					break;
				}
			}
		#else
			// try to locate Modbus mapping for reading operation
			kp_addr_gp = app_mslave_get_op_addr_group( hld_mslave_ctr, kp_msg_buf, tpint);
			switch(kp_addr_gp)
			{
				case ADDR_GP_COIL:
				{	;}
				case ADDR_GP_DISCRETE_INPUT:
				{	;}
				case ADDR_GP_HOLDING_REGISTER:
				{	;}
				case ADDR_GP_INPUT_REGISTER:
				{
					modbus_mapping_t *pt_modbus_mapping = NULL;
					uint16_t tp_starting_addr = app_mslave_pick_starting_address( kp_msg_buf, tpint);

					//MK454U___________________
					pthread_mutex_lock( &hld_mslave_ctr->mtx_for_mapping_queue);
					
					//to locate the Modbus Mapping for Reading operation			
					pt_modbus_mapping = app_mslave_locate_mapping( hld_mslave_ctr, tp_starting_addr, kp_addr_gp);
					if(NULL == pt_modbus_mapping)
					{					
						pthread_mutex_unlock( &hld_mslave_ctr->mtx_for_mapping_queue);
						DBG_LOG_ERR("fail to find the mapping for starting_addr 0x%x", tp_starting_addr);
						break;
					}
					
					//modbus_reply( hld_mctx, kp_msg_buf, tpint, hld_mslave_mapping);
					modbus_reply( hld_mctx, kp_msg_buf, tpint, pt_modbus_mapping);

					pthread_mutex_unlock( &hld_mslave_ctr->mtx_for_mapping_queue);
					//MK454D___________________
					
					break;
				}
				case ADDR_GP_REPORT_SLAVE_ID:
				{
					DBG_LOG_ERR("TODO==================to complete");
					break;
				}
				case ADDR_GP_FIFO_QUEUE:
				{
					DBG_LOG_ERR("TODO=================to complete");
					break;
				}
				case ADDR_GP_FILE_RECORD:
				{
					DBG_LOG_ERR("TODO=================to complete");
					break;
				}
				case ADDR_GP_DIAGNOSTICS:
				{
					//DBG_LOG_ERR("TODO==================to complete");
					handler_for_fcode_diagnostics( hld_mslave_ctr, kp_msg_buf, tpint);
					break;
				}
				case ADDR_GP_COM_EVENT_COUNTER:
				{
					DBG_LOG_ERR("TODO==================to complete");
					break;
				}
				case ADDR_GP_COM_EVENT_LOG:
				{
					DBG_LOG_ERR("TODO==================to complete");
					break;
				}
				case ADDR_GP_DEV_ID:
				{
					DBG_LOG_ERR("TODO==================to complete");
					break;
				}
				default:
				{
					DBG_LOG_ERR(" this is not suppose to happen ADDR_GP %d", kp_addr_gp);
					break;
				}
			}
		#endif

		
		//app_mslave_ipc_wait_for_update(hld_mslave_ctr);

		sched_yield();
	}

	ERR:
		DBG_LOG_ERR("Critical Error");
		while(1)
		{
			sleep(1);
		}
}


/*
*/
void app_mslave_dump_port_conf(const struct port_conf_info*pt_port_conf)
{
	if(NULL == pt_port_conf)
	{
		return;
	}
	DBG_LOG_WARN("slave_ID = %d", pt_port_conf->slaveID);
	DBG_LOG_WARN("baudrate = %d", pt_port_conf->baudrate);
	DBG_LOG_WARN("parity = %d", pt_port_conf->parity);
	DBG_LOG_WARN("stopbit = %d", pt_port_conf->stopbit);
	DBG_LOG_WARN("reg_mod = %d", pt_port_conf->reg_md);
}

/*
*/
void app_mslave_dump_dev_info(const struct modbus_slave_controller*pt_mslave_ctr)
{
	deviceTableData_t *pt_dev_cfg = NULL;
	if(NULL == pt_mslave_ctr)
	{
		return;
	}
	DBG_LOG_INFO("To dump dev-array :");
	for(uint32_t i = 0; i < APP_SHM_DEV_TABLE_ITEM_MAX; i++)
	{
		pt_dev_cfg = &pt_mslave_ctr->dev_array[i % APP_SHM_DEV_TABLE_ITEM_MAX];
		if(0 == pt_dev_cfg->nameId) //is_dev_info_valid(const deviceTableData_t * pt_devinfo)
		{
			continue;
		}
		pt_dev_cfg->description[(GW_PRO_MAX_STRING_LEN_BYTE - 1 -1) % GW_PRO_MAX_STRING_LEN_BYTE ] = 0; // in case 
		DBG_LOG_INFO("%d -  %s", pt_dev_cfg->nameId, pt_dev_cfg->description);
	}
}


/*
the index is returned
*/
static int32_t to_find_nameid(const uint32_t *pt_idbuf, const uint32_t nameid)
{
	int32_t kpidx = -1;
	if(nameid <= 0)
	{
		DBG_LOG_ERR("invalid nameId is given");
		return kpidx;
	}
	for(uint32_t i = 0; i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
	{
		if(pt_idbuf[i] == nameid)
		{
			kpidx = i;
			break;
		}
	}
	
	return kpidx;
}

/*
NOTE!!! we take the assumption that 0 is an invalid nameId!
*/
static bool to_insert_nameid(uint32_t *pt_idbuf, const uint32_t nameid)
{
	//DBG_LOG_WARN("to insert nameId %d", nameid);
	// to check if the nameId is in the buffer already
	for(uint32_t i = 0; i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
	{ 
		if(pt_idbuf[i] == nameid)
		{
			//DBG_LOG_ERR("%d is in the array", nameid);
			return false;
		}
	}
	
	//DBG_LOG_WARN("to insert nameId %d", nameid);		
	for(uint32_t i = 0; i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
	{
		if(pt_idbuf[i] == 0)
		{
			//DBG_LOG_WARN("put nameId %d in array idx = %d", nameid, i);
			pt_idbuf[i] = nameid;
			break;
		}
	}
	
	return true;
}

#if(0)
/*
*/
void app_mslave_dump_sdata_info(const uint8_t *pt_sdata_buf)
{
	uint32_t kpidx = 0;
	appShmDataItem_t *pt_dtitem = {0};
	SKFChina_Common_MeasurementType dtype_array[APP_SHM_SENSORDATA_TABLE_ITEM_MAX][APP_SHM_SENSORDATA_TABLE_PIECE_MAX] = {0};
	uint32_t nameid_array[APP_SHM_SENSORDATA_TABLE_ITEM_MAX] = {0};
	int32_t tpi32 = 0;
	
	DBG_LOG_WARN("========to dump sensor-data info=========");
	for(uint32_t i = 0; i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
	{
		for(uint32_t j = 0; j < APP_SHM_SENSORDATA_TABLE_PIECE_MAX; j++)
		{
			kpidx = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i*APP_SHM_SENSORDATA_TABLE_PIECE_MAX + j)*APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			pt_dtitem = (appShmDataItem_t*)&pt_sdata_buf[kpidx % APP_SHM_TOTAL_SIZE];
			if(GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE != pt_dtitem->appShmDataType)
			{// invalid data-item
				continue;
			}
			// to insert the nameId
			to_insert_nameid( nameid_array, pt_dtitem->sensorDataTableData.nameId);
			// to get the idx of nameId in the array
			tpi32 = to_find_nameid( nameid_array, pt_dtitem->sensorDataTableData.nameId);
			if(tpi32 < 0)
			{
				DBG_LOG_ERR(" the data-itme might be ruined when you see this output");
				continue;
			}
			// to insert measurement-type into the array
			for(uint32_t k = 0; k < APP_SHM_SENSORDATA_TABLE_PIECE_MAX; k++)
			{
				if(dtype_array[tpi32 % APP_SHM_SENSORDATA_TABLE_ITEM_MAX][k] == SKFChina_Common_MeasurementType_UNKNOWN_MEASUREMENT_TYPE)
				{
					dtype_array[tpi32 % APP_SHM_SENSORDATA_TABLE_ITEM_MAX][k] = pt_dtitem->sensorDataTableData.dataType;
					break;
				}
			}
			
		}
	}
	// to dump the data-type
	for(uint32_t i = 0; i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
	{
		if(0 == nameid_array[i])
		{
			continue;
		}
		DBG_LOG_WARN("the data-type info for idx = %d nameId %d, max_piece_num = %d", i, nameid_array[i], APP_SHM_SENSORDATA_TABLE_PIECE_MAX);
		tpi32 = APP_SHM_SENSORDATA_TABLE_PIECE_MAX % 6 ? 1 : 0;
		for(uint32_t j = 0; j < (APP_SHM_SENSORDATA_TABLE_PIECE_MAX / 6 + tpi32); j ++)
		{
			DBG_LOG_WARN("%d - %d - %d - %d - %d - %d",\
				dtype_array[i][(j*6) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX],\
				dtype_array[i][(j*6 + 1) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX],\
				dtype_array[i][(j*6 + 2) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX],\
				dtype_array[i][(j*6 + 3) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX],\
				dtype_array[i][(j*6 + 4) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX],\
				dtype_array[i][(j*6 + 5) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX]);
		}
	}
	
}
#else
/*to dump the the sensor data in the shared-memory for debug

NOTE!!! (1) this funciton will lock the shared memory in the process of sensor data dumping
		(2) the IPC buffer is ruined by this function
*/
void app_mslave_dump_sdata_info(const struct modbus_slave_controller*pt_mslave_ctr)
{
	uint32_t kpidx = 0;
	appShmDataItem_t_original *pt_dtitem = NULL;
	SKFChina_Common_MeasurementType dtype_array[APP_SHM_SENSORDATA_TABLE_ITEM_MAX][APP_SHM_SENSORDATA_TABLE_PIECE_MAX] = {0};
	uint32_t nameid_array[APP_SHM_SENSORDATA_TABLE_ITEM_MAX] = {0};
	uint64_t bitmap_array[APP_SHM_SENSORDATA_TABLE_ITEM_MAX] = {0};
	int32_t tpi32 = 0;

	if(NULL == pt_mslave_ctr)
	{
		return;
	}

	if(sizeof(appShmDataItem_t_original) > SZ_IPC_BUF)
	{
		DBG_LOG_ERR("no enough buffer space");
		return;
	}

	//MK4042U_______________________________________
	pt_dtitem = (appShmDataItem_t_original*)pt_mslave_ctr->ipc_dtitem_buf;
	
	sem_wait(pt_mslave_ctr->pt_sem_for_shmem_dtblock);

	for(uint32_t i = 0;i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
	{
		for(uint32_t j = 0; j < APP_SHM_SENSORDATA_TABLE_PIECE_MAX; j++ )
		{
			kpidx = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX + j)*APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memset( pt_dtitem, 0, sizeof(appShmDataItem_t_original));
			memcpy( &pt_dtitem->sensorDataTableData, ((uint8_t*)pt_mslave_ctr->pt_dtblock) + kpidx % APP_SHM_TOTAL_SIZE,  sizeof(sensorDataTableData_t));

            #if(0) // to dump data of the first 4 channels
 
                if(0 == pt_dtitem->sensorDataTableData.nameId)
 
                {
 
                    DBG_LOG_WARN("sensor-data channel %d piece %d is invalid", i, j);
 
                }
 
                else
 
                {
 
                    DBG_LOG_WARN("B_sensor-data ch-%d-p-%d____________", i, j);
 
                    DBG_LOG_WARN("seqNo %d", pt_dtitem->sensorDataTableData.sequenceNumber);
 
                    DBG_LOG_WARN("nameId %d", pt_dtitem->sensorDataTableData.nameId);
 
                    pt_dtitem->sensorDataTableData.BLESensorName[GW_PRO_MAX_STRING_LEN_BYTE-1] = 0;
 
                    DBG_LOG_WARN("BLESensorName %s", pt_dtitem->sensorDataTableData.BLESensorName);
 
                    pt_dtitem->sensorDataTableData.macAddr[GW_PRO_MAX_STRING_LEN_BYTE - 1] = 0;
 
                    DBG_LOG_WARN("macAddr %s", pt_dtitem->sensorDataTableData.macAddr);
 
                    pt_dtitem->sensorDataTableData.type[GW_PRO_MAX_STRING_LEN_BYTE - 1] = 0;
 
                    DBG_LOG_WARN("type %s", pt_dtitem->sensorDataTableData.type);
 
                    pt_dtitem->sensorDataTableData.manufacturer[GW_PRO_MAX_STRING_LEN_BYTE - 1] = 0;
 
                    DBG_LOG_WARN("manufacturer %s", pt_dtitem->sensorDataTableData.manufacturer);
 
                    DBG_LOG_WARN("measureSeqno %d", pt_dtitem->sensorDataTableData.measureSeqno);
 
                    DBG_LOG_WARN("sampelTime %d", pt_dtitem->sensorDataTableData.sampleTime);
 
                    DBG_LOG_WARN("measurement %d", pt_dtitem->sensorDataTableData.measurement);
 
                    DBG_LOG_WARN("receivedTimestamp %d", pt_dtitem->sensorDataTableData.receivedTimestamp);
 
                    DBG_LOG_WARN("dataType %d", pt_dtitem->sensorDataTableData.dataType);
 
                    pt_dtitem->sensorDataTableData.format[MAX_DT_STORATE_FORM_BUF-1] = 0;
 
                    DBG_LOG_WARN("range %d", pt_dtitem->sensorDataTableData.range);
 
                    DBG_LOG_WARN("unit %d", pt_dtitem->sensorDataTableData.unit);
 
                    DBG_LOG_WARN("measureLengthSample %d", pt_dtitem->sensorDataTableData.measureLengthSample);
 
                    DBG_LOG_WARN("totalDataLengthSamle %d", pt_dtitem->sensorDataTableData.totalDataLengthSample);
 
                    DBG_LOG_WARN("dimension %d", pt_dtitem->sensorDataTableData.dimension);
 
                    DBG_LOG_WARN("dataFormat %d", pt_dtitem->sensorDataTableData.dataFormat);
 
                    DBG_LOG_WARN("samplePeriods %d", pt_dtitem->sensorDataTableData.samplePeriods);
 
                    DBG_LOG_WARN("encryption %d", pt_dtitem->sensorDataTableData.encryption);
 
                    DBG_LOG_WARN("value i %d", pt_dtitem->sensorDataTableData.value.data_int32);
 
                    DBG_LOG_WARN("value f %f", pt_dtitem->sensorDataTableData.value.data_float);
 
                    DBG_LOG_WARN("sampleRate %d", pt_dtitem->sensorDataTableData.sampleRate);
 
                    DBG_LOG_WARN("product %d", pt_dtitem->sensorDataTableData.product);
 
                    DBG_LOG_WARN("sensor %d", pt_dtitem->sensorDataTableData.sensor);
 
                    DBG_LOG_WARN("sent %d", pt_dtitem->sensorDataTableData.sent);
 
                    DBG_LOG_WARN("E_sensor-data ch-%d-p-%d____________", i, j);
 
                }
 
            #endif
			if(0 == pt_dtitem->sensorDataTableData.nameId)
			{
				continue;
			}
			
			pt_dtitem->appShmDataType = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;

			if(GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE != pt_dtitem->appShmDataType)
			{ // invalid sensor-data item
				continue;
			}

			// to insert the nameId
			to_insert_nameid( nameid_array, pt_dtitem->sensorDataTableData.nameId);
			//to get the index of nameId in the array
			tpi32 = to_find_nameid( nameid_array, pt_dtitem->sensorDataTableData.nameId);
			if(tpi32 < 0)
			{
				DBG_LOG_ERR("the data-item might be ruined when you see this output");
				continue;
			}
			//to insert measurement-type into the array
			for(uint32_t k = 0; k < APP_SHM_SENSORDATA_TABLE_PIECE_MAX; k++)
			{
				if(dtype_array[tpi32 % APP_SHM_SENSORDATA_TABLE_ITEM_MAX][k] == SKFChina_Common_MeasurementType_UNKNOWN_MEASUREMENT_TYPE)
				{
					dtype_array[tpi32 % APP_SHM_SENSORDATA_TABLE_ITEM_MAX][k] = pt_dtitem->sensorDataTableData.dataType;
					break;
				}
			}
		}
	}

	//to get the bitmap
	for(uint32_t i = 0; i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
	{
		kpidx = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + ((i + 1)*APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1)*APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
		 bitmap_array[i] = *((uint64_t*)(((uint8_t*)pt_mslave_ctr->pt_dtblock) + kpidx % APP_SHM_TOTAL_SIZE));
	}
	

	sem_post(pt_mslave_ctr->pt_sem_for_shmem_dtblock);
	//MK4042D_______________________________________

	// to dump the data-type
	for(uint32_t i = 0; i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
	{
		if(0 == nameid_array[i])
		{
			continue;
		}
		DBG_LOG_WARN("the data-type info for idx = %d nameId %d, max_piece_num = %d", i, nameid_array[i], APP_SHM_SENSORDATA_TABLE_PIECE_MAX);
		tpi32 = APP_SHM_SENSORDATA_TABLE_PIECE_MAX % 6 ? 1 : 0;
		for(uint32_t j = 0; j < (APP_SHM_SENSORDATA_TABLE_PIECE_MAX / 6 + tpi32); j ++)
		{
			DBG_LOG_WARN("%d - %d - %d - %d - %d - %d",\
				dtype_array[i][(j*6) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX],\
				dtype_array[i][(j*6 + 1) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX],\
				dtype_array[i][(j*6 + 2) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX],\
				dtype_array[i][(j*6 + 3) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX],\
				dtype_array[i][(j*6 + 4) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX],\
				dtype_array[i][(j*6 + 5) % APP_SHM_SENSORDATA_TABLE_PIECE_MAX]);
		}
	}
	// to dump the bitmap
	DBG_LOG_WARN("===============the bitmap==================%d", APP_SHM_SENSORDATA_TABLE_ITEM_MAX);
	tpi32 = APP_SHM_SENSORDATA_TABLE_ITEM_MAX % 6 ? 1 : 0;		
	for(uint32_t i = 0; i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX / 6 + tpi32; i++)
	{
		DBG_LOG_WARN("0x%.8x - 0x%.8x - 0x%.8x  - 0x%.8x - 0x%.8x - 0x%.8x",\
			bitmap_array[(i*6 + 0) % APP_SHM_SENSORDATA_TABLE_ITEM_MAX ], bitmap_array[(i*6 + 1) % APP_SHM_SENSORDATA_TABLE_ITEM_MAX ],\
			bitmap_array[(i*6 + 2) % APP_SHM_SENSORDATA_TABLE_ITEM_MAX ], bitmap_array[(i*6 + 3) % APP_SHM_SENSORDATA_TABLE_ITEM_MAX ],\
			bitmap_array[(i*6 + 4) % APP_SHM_SENSORDATA_TABLE_ITEM_MAX ], bitmap_array[(i*6 + 5) % APP_SHM_SENSORDATA_TABLE_ITEM_MAX ]);
	}

	//the endianness
	DBG_LOG_WARN("the endianess %d", pt_mslave_ctr->mbus_border);

}
#endif


/*modbus_mapping_t
typedef void (*l_queue_foreach_func_t) (void *data, void *user_data);
*/
void q_cbk_foreach_print_mapping_info(void *data, void *user_data)
{
	modbus_mapping_t *pt_mapping_info = (modbus_mapping_t*)data;
	if(NULL == pt_mapping_info)
	{
		DBG_LOG_ERR("unexpected NULL");
		return;
	}
	DBG_LOG_WARN("Addr@0x%x - L0x%x", pt_mapping_info->start_registers, pt_mapping_info->nb_registers);
}

/*
*/
void app_mslave_dump_mbus_mapping_info(const struct modbus_slave_controller*pt_mslave_ctr)
{
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
	l_queue_foreach( pt_mslave_ctr->mapping_queue, q_cbk_foreach_print_mapping_info, NULL);
	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_mapping_queue);
	
	return;
}


/*to initialize the PORT
ret@error_code
NOTE!!
*/
enum error_code app_mslave_init_port( struct modbus_slave_controller *pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	modbus_t *pt_modbus_port = NULL;
	struct port_conf_info*pt_port_conf = NULL;

	if(NULL == pt_mslave_ctr)
	{
		tpret = ERR_INVALID_PARAM;
		DBG_LOG_ERR("unexpected NULL");
		goto EXIT;
	}

	//MK3251U______________________________________
	pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
	
	pt_port_conf = &pt_mslave_ctr->port_conf;

	app_mslave_dump_port_conf(pt_port_conf);	

	#if(1)
	pt_modbus_port = modbus_new_rtu( 
		DEV_NM_FOR_MBUS_SLAVE, pt_port_conf->baudrate, pt_port_conf->parity, 8, pt_port_conf->stopbit);

	#else
		// only for test
		pt_modbus_port = modbus_new_rtu( 
DEV_NM_FOR_MBUS_SLAVE, 115200, 'N', 8, 1);		
		//pt_port_conf->reg_md = REG_MD_EXTENSIBLE;  
		pt_port_conf->baudrate = 115200; 
		pt_port_conf->parity = PARITY_NONE;
		pt_port_conf->slaveID = 1;
		pt_port_conf->stopbit = SBIT_1;
		pt_mslave_ctr->slave_id = 1;
	#endif
	if(NULL == pt_modbus_port)
	{		
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		DBG_LOG_ERR("fail to create modbus")
		tpret = ERR_API_FAIL;
		goto EXIT;
	}

	modbus_set_slave( pt_modbus_port, pt_port_conf->slaveID);
	//modbus_set_debug( pt_modbus_port, TRUE);	
	modbus_set_debug( pt_modbus_port, false); // to disable the library log output
	modbus_set_response_timeout( pt_modbus_port, 3, 0);

	if(modbus_connect( pt_modbus_port))
	{		
		pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
		modbus_close( pt_modbus_port);
		modbus_free( pt_modbus_port);
		DBG_LOG_ERR("fail to open PORT");
		tpret = ERR_API_FAIL;
		goto EXIT;
	}
	app_mslave_set_mctx( pt_mslave_ctr, pt_modbus_port);

	pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
	//MK3251_________________________________


	EXIT:
		return tpret;
}




#if(1)
	#ifndef PATH_MFILE
 		#define PATH_MFILE "/var/lib/skf-gateway/config/magicFile.json"
	#endif
	
	#ifndef PATH_MFILE_USER
		#define PATH_MFILE_USER	"/var/lib/skf-gateway/config/magicFile.json.user"
	#endif

static char parentstr[100] = {0};



static void parse_magicfileitem(cJSON *item, struct register_mapping_info*pt_mapping_info) {
  cJSON *tmp = NULL;
  int rootsize = cJSON_GetArraySize(item);
  for (int i = 0; i < rootsize; i++) {
    if (NULL == (tmp = cJSON_GetArrayItem(item, i)))
      continue;
	
	//DBG_LOG_WARN("json type %d", tmp->type);

	switch (tmp->type) {
    case cJSON_Number: {
      if (0 == strcmp(parentstr, "disabledGatewayData")) {
        //disabledData.GW[disabledData.gwCnt % (sizeof(disabledData.GW)/sizeof(disabledData.GW[0]))] = tmp->valueint;
		//DBG_LOG_WARN("disabledGatewayData %d", tmp->valueint);
	  	//disabledData.gwCnt++;
      } else if (0 == strcmp(parentstr, "disabledPredictSensorData")){
        //disabledData.PredictSensor[disabledData.predictSensorCnt % (sizeof(disabledData.PredictSensor)/sizeof(disabledData.PredictSensor[0]))] = tmp->valueint;
        //disabledData.predictSensorCnt++;
      } else if (0 == strcmp(parentstr, "disabledInsightTData")){
        //disabledData.InsightT[disabledData.insightTCnt % (sizeof(disabledData.InsightT)/sizeof(disabledData.InsightT[0]))] = tmp->valueint;
        //disabledData.insightTCnt++;
      } else if (0 == strcmp(parentstr, "disabledPredictSensorProData")){
        //disabledData.PredictSensorPro[disabledData.predictSensorProCnt % (sizeof(disabledData.PredictSensorPro)/sizeof(disabledData.PredictSensorPro[0]))] = tmp->valueint;
        //disabledData.predictSensorProCnt++;
      } else if (0 == strcmp(parentstr, "disabledGatewayConfig")){
        //disabledConfig.GW[disabledConfig.gwCnt % (sizeof(disabledConfig.GW)/sizeof(disabledConfig.GW[0]))] = tmp->valueint;
        //disabledConfig.gwCnt++;
      } else if (0 == strcmp(parentstr, "disabledPredictSensorConfig")){
        //disabledConfig.PredictSensor[disabledConfig.predictSensorCnt % (sizeof(disabledConfig.PredictSensor)/sizeof(disabledConfig.PredictSensor[0]))] = tmp->valueint;
        //disabledConfig.predictSensorCnt++;
      } else if (0 == strcmp(parentstr, "disabledInsightTConfig")){
        //disabledConfig.InsightT[disabledConfig.insightTCnt % (sizeof(disabledConfig.InsightT)/sizeof(disabledConfig.InsightT[0]))] = tmp->valueint;
        //disabledConfig.insightTCnt++;
      } else if (0 == strcmp(parentstr, "disabledPredictSensorProConfig")){
        //disabledConfig.PredictSensorPro[disabledConfig.predictSensorProCnt % (sizeof(disabledConfig.PredictSensorPro)/sizeof(disabledConfig.PredictSensorPro[0]))] = tmp->valueint;
        //disabledConfig.predictSensorProCnt++;
      }
	  #if(1)// for the Modbus Register remapping
	  	else if(0 == strcmp(parentstr, "offset"))
	  	{
	  		pt_mapping_info->offset = tmp->valueint;
			//DBG_LOG_WARN("offset %d", tmp->valueint);
	  	}
		else if(0 == strcmp(parentstr, "mappingNum"))
		{
			pt_mapping_info->mapping_num = tmp->valueint;
			//DBG_LOG_WARN("mappingNum %d", tmp->valueint);
		}
		else if(0 == strcmp(parentstr, "src"))
		{
			pt_mapping_info->src[pt_mapping_info->src_cnt % MAX_REG_MAPPING] = tmp->valueint;
			pt_mapping_info->src_cnt++;
			//DBG_LOG_WARN("src[%d] %d", pt_mapping_info->src_cnt -1, tmp->valueint);
		}
		else if(0 == strcmp(parentstr, "dest"))
		{
			pt_mapping_info->des[pt_mapping_info->des_cnt % MAX_REG_MAPPING] = tmp->valueint;
			pt_mapping_info->des_cnt++;
			//DBG_LOG_WARN("des[%d] %d", pt_mapping_info->des_cnt -1, tmp->valueint);
			// to set the initialization value
			if(1 == pt_mapping_info->des_cnt)
			{
				pt_mapping_info->max_reg_addr = tmp->valueint;
				pt_mapping_info->min_reg_addr = tmp->valueint;
			}
			// the MAX
			if(tmp->valueint > pt_mapping_info->max_reg_addr)
			{
				pt_mapping_info->max_reg_addr = tmp->valueint;
			}
			//the MIN
			if(tmp->valueint < pt_mapping_info->min_reg_addr)
			{
				pt_mapping_info->min_reg_addr = tmp->valueint;
			}
		}
	  #endif
    } break;
    case cJSON_Array:
    case cJSON_Object: {
      if (tmp->string != NULL) {
        snprintf(parentstr, sizeof(parentstr) - 1, "%s", tmp->string);
		//DBG_LOG_WARN("parentstr %s", tmp->string);
      }
      parse_magicfileitem(tmp, pt_mapping_info);
      if (i + 1 == rootsize) {
        parentstr[0] = 0;
      }
    } break;
    default:
      DBG_LOG_ERR( "Invalid type!\r\n");
      break;
    }
  }
}

static void magicfile_parse(char *input_json, struct register_mapping_info*pt_mapping_info)
{
  cJSON *_root = NULL;
  if ((input_json == NULL)||(NULL == pt_mapping_info)) 
  {
    DBG_LOG_WARN( "Invalid json string\r\n");
    return;
  }
  // LOG_INFO(OUTPOINT, "file:%s\r\n",input_json);
  _root = cJSON_Parse(input_json);
  if (_root == NULL) {
    // the json string seems invalid
    DBG_LOG_WARN( "Failed to  parse json\r\n");
    return;
  }
  parse_magicfileitem(_root, pt_mapping_info);
  cJSON_Delete(_root);
}


static int config_file_parse(const char *filename, char **jsonStr) {
  FILE *fd = NULL;
  int fileSize = 0;
  int rd_size = 0;
  struct stat statbuf = {0};
  stat(filename, &statbuf);
  fileSize = statbuf.st_size;
  *jsonStr = (char *)malloc(sizeof(char) * fileSize + 1);
  memset(*jsonStr, 0, fileSize + 1);
  DBG_LOG_INFO( ":%d\n", fileSize);
  fd = fopen(filename, "r");
  if (fd == NULL) {
    DBG_LOG_WARN("Open file fail!\r\n");
    return -1;
  }
  rd_size = fread(*jsonStr, sizeof(char), fileSize, fd);
  if (rd_size != fileSize) {
    DBG_LOG_WARN( "failed to read json file!%d,%d\r\n", fileSize, rd_size);
    fclose(fd);
    return -1;
  }
  fclose(fd);
  return 0;
}


/*
to be called to load the remapping info from magic file
*/
enum error_code app_mslave_load_reg_mapping_info( struct register_mapping_info *pt_mapping_info)
{
	uint8_t *pt_jsonstr = NULL;
	enum error_code tpret = ERR_NONE;
	const char*pt_fname = NULL;
	
	if(NULL == pt_mapping_info)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		return tpret;
	}

	memset( pt_mapping_info, 0, sizeof(struct register_mapping_info));

	if(access( PATH_MFILE_USER, F_OK ) == 0)
	{
		pt_fname = PATH_MFILE_USER;
	}
	else if(access(PATH_MFILE, F_OK) == 0)
	{
		pt_fname = PATH_MFILE;
	}
	else
	{
		DBG_LOG_ERR("the MAGIC file does not exist");
		tpret = ERR_UNKNOWN;
		return tpret;
	}


	if(config_file_parse( pt_fname, &pt_jsonstr) != 0)
	{
		if(NULL != pt_jsonstr)
		{
			free(pt_jsonstr);
			pt_jsonstr = NULL;
		}
		tpret = ERR_UNKNOWN;
		return tpret;
	}

	magicfile_parse( pt_jsonstr, pt_mapping_info);

	free(pt_jsonstr);
	pt_jsonstr = NULL;


	//MK4417U___the source register is shifted by "offset" in magic file_____
	// set the "src" register address to the real address in Modbus mapping block
	if(pt_mapping_info->src_cnt > 0)
	{
		for(uint32_t i = 0; i < pt_mapping_info->src_cnt; i++)
		{
			pt_mapping_info->src[i % MAX_REG_MAPPING] = pt_mapping_info->src[i % MAX_REG_MAPPING] + pt_mapping_info->offset; 
		}
	}   
	//MK4417D_________________________________________________________________

	
	// to validate the mapping info	
	pt_mapping_info->flg = false;
	do{
		if(pt_mapping_info->offset > 0xFFFF)
		{
			break;
		}
		if((pt_mapping_info->offset + pt_mapping_info->mapping_num) > 0xFFFF)
		{
			break;
		}

		if((pt_mapping_info->mapping_num != pt_mapping_info->src_cnt)||(pt_mapping_info->mapping_num != pt_mapping_info->des_cnt))
		{
			break;
		}

		if(pt_mapping_info->max_reg_addr <= pt_mapping_info->min_reg_addr)
		{// when the des-addr is overflow(two-byte), this might happen
			break;
		}

		if(pt_mapping_info->mapping_num > MAX_REG_MAPPING)
		{
			DBG_LOG_ERR("too many registers for mapping");
			break;
		}

		if(0 == pt_mapping_info->mapping_num)
		{
			break;
		}
		
		pt_mapping_info->flg = true;
	}while(0);

	return tpret;
}



static struct register_mapping_info g_reg_mapping_info = {0}; 

struct register_mapping_info *app_mslave_get_reg_mapping_info(void)
{
	return &g_reg_mapping_info;
}


/*to get the offset info from the magic file
0 is returned when the magic file does not set or no valid remapping info
*/
uint16_t app_mslave_get_reg_mapping_offset(void)
{
	uint16_t tpret = 0;

	if(true == g_reg_mapping_info.flg)
	{
		tpret = g_reg_mapping_info.offset;
	}
	return tpret;
}



void app_mslave_dump_reg_mapping_info(struct register_mapping_info *pt_remap_info)
{
	if(NULL == pt_remap_info)
	{
		return;
	}
	DBG_LOG_WARN("flg %d,offset %d ,mapping_num %d,src_cnt %d,des_cnt %d, max_addr %d, min_addr %d",\
	pt_remap_info->flg ,pt_remap_info->offset, pt_remap_info->mapping_num, pt_remap_info->src_cnt, pt_remap_info->des_cnt,\
	pt_remap_info->max_reg_addr, pt_remap_info->min_reg_addr);
}

#endif




