/*for Modbus Slave
*/

#ifndef __APP_MSLAVE_H
#define __APP_MSLAVE_H

#include "ell/queue.h"
#include "modbus.h"
#include "pthread.h"
#include <time.h>
#include <semaphore.h>

#include "sh_mem.h"

#ifndef DEV_NM_FOR_MBUS_SLAVE
	#define DEV_NM_FOR_MBUS_SLAVE "/dev/ttyS3"
#endif


#ifndef SZ_RTU_MSGBUF
	//#define SZ_RTU_MSGBUF	(1024U)
	#define SZ_RTU_MSGBUF	(256U)
#endif

#ifndef MAX_MODBUS_SERIAL_ADU
	#define MAX_MODBUS_SERIAL_ADU (256U)
#endif

#ifndef MIN_RTU_PKT
	#define MIN_RTU_PKT	(4U)
#endif

#ifndef MAX_COMM_EVENT_LOG
	#define MAX_COMM_EVENT_LOG	(64U)
#endif

#ifndef MAX_DIAG_CNT
	#define MAX_DIAG_CNT	(0xFFFF)
#endif

#ifndef MAX_REG_NUMBER
	#define MAX_REG_NUMBER	(0xFFFF)
#endif

#ifndef MAX_REG_MAPPING_MEM
	#define MAX_REG_MAPPING_MEM	(MAX_REG_NUMBER * 2) // 2Bytes for each register
#endif

enum slave_mode{
	SMODE_UNKNOWN = 0,
	SMODE_ACTIVE = 1,
	SMODE_LISTEN_ONLY = 2,
	SMODE_MAX = 3
};

#ifndef MIN_MBUS_BAUDRATE
	#define MIN_MBUS_BAUDRATE	(1200U)
#endif

#ifndef MAX_MBUS_BAUDRATE	
	#define MAX_MBUS_BAUDRATE	(1000000U)
#endif

// slave individual address -> [1, 247]
#ifndef MIN_MBUS_SADDR
	#define MIN_MBUS_SADDR	(1U)
#endif

#ifndef MAX_MBUS_SADDR
	#define MAX_MBUS_SADDR	(247U)
#endif



enum port_parity{
	PARITY_NONE = 'N',
	PARITY_EVEN = 'E',
	PARITY_ODD = 'O',
};

enum stop_bit{
	SBIT_1 = 1,
	SBIT_2 = 2,
	SBIT_1_5 = 3,  // NO support yet.
};


enum register_mode{
	//REG_MD_UNKNOWN	= 0,
	REG_MD_EXTENSIBLE = 0,//MODBUS_REG_ADDR_EXTENSIBLE_MODE,
	REG_MD_EFFICIENT = 1,//MODBUS_REG_ADDR_EFFICIENT_MODE,
	REG_MD_UNKNOWN = 0xFF,  
};



/*Modbus Byte oder definition
*/
enum mbus_byte_oder{
	MBUS_BORDER_ABCD = 0,
	MBUS_BORDER_DCBA,
	MBUS_BORDER_BADC,
	MBUS_BORDER_CDAB,
};


//#define MBUS_BORDER_DEFAULT MBUS_BORDER_ABCD
#define MBUS_BORDER_DEFAULT MBUS_BORDER_CDAB


struct port_conf_info{
	uint32_t baudrate;
	enum port_parity parity;
	enum stop_bit stopbit;
	
	uint8_t slaveID;
	enum register_mode reg_md;
	
	/*...*/
};


/* counters, event log, register.etc */
struct counter_log_register_set{
	// definition for diagnostic counters
	
	/* "the quantity of messages that the remote device has detectedon the communications system since
	its last restart, clear counters operation, or power–up" */
	uint16_t diag_cnt_bus_message;

	/* " the quantity of CRC errors encountered by the remote devicesince its last restart, clear
	counters operation, or power–up." */
	uint16_t diag_cnt_bus_comm_error;

	/* "the quantity of MODBUS exception responses returned by the remote device since its last
	restart, clear counters operation, or power–up." */
	uint16_t diag_cnt_slave_exception_error;

	/* "the quantity of messages addressed to the remote device, or broadcast, that the remote device
	has processed since its last restart, clear counters operation, or power–up." */
	uint16_t diag_cnt_slave_message;

	/* " the quantity of messages addressed to the remote device for which it has returned no response
	(neither a normal response nor an exception response), since its last restart, clear counters operation, or 
	power–up." */
	uint16_t diag_cnt_slave_no_response;
	
	/* "the quantity of messages addressed to the remote device for which it returned a Negative Acknowledge (NAK) 
	exception response, since its last restart, clear counters operation, or power–up."*/
	uint16_t diag_cnt_slave_nak;

	/* " the quantity of messages addressed to the remote device for which it returned a Server Device Busy exception
	response, since its last restart, clear counters operation, or power–up." */
	uint16_t diag_cnt_slave_busy;

	/* the quantity of messages addressed to the remote device that it could not handle due to a character overrun 
	condition, since its last restart, clear counters operation, or power–up*/
	uint16_t diag_cnt_bus_charac_overrun;
	
	// diagnostic register
	uint16_t diag_regisger;

	//comm event counter, for function-code 0x0B
	uint16_t cnt_comm_event;

	// comm event log, for function-code 0x0C
	uint8_t log_comm_event[MAX_COMM_EVENT_LOG];
	uint8_t log_event_cnt;
	uint8_t log_event_idx; // (log_event_idx -1)  always points to the latest comm event 

	// exception status, for fcode00x07
	uint8_t exception_status;
};


/*the file-reading buffer
*/
#ifndef SZ_FREADING_BUFFER
	#define SZ_FREADING_BUFFER	(2028U*2U) //NOTE!! always set it as an even number
#endif
#ifndef VAL_BUF_NODE_LIFE	
	#define VAL_BUF_NODE_LIFE	(15U) // 15 seconds, with no file-operation, the buffer-node will be released
#endif
struct freading_buffer{
	uint32_t file_len; // the file length

	time_t tstamp; // the timestamp when the reading buffer is created 
	uint16_t file_no; // 
	uint16_t record_no; // the starting of record number in buffer below
	uint16_t record_len; // the number of records in buffer below
	uint8_t dtbuf[SZ_FREADING_BUFFER];
};


#define MAX_ABC(a,b,c) ( ((a) > (b) ? (a) : (b)) > (c) ? ((a) > (b) ? (a) : (b)) : (c) ) 
#define SZ_IPC_BUF (0x1000)

#if(MAX_ABC( APP_SHM_GATEWAY_TABLE_ITEM_SIZE, APP_SHM_DEV_TABLE_ITEM_SIZE, APP_SHM_SENSORDATA_TABLE_PIECE_SIZE) >= SZ_IPC_BUF)
	#error "no enough buffer space, chance to overflow"
#endif



struct modbus_slave_controller{
	
	
		
	// slave_mode
	enum slave_mode smode;

	//depend on configuration from user
	uint8_t slave_id; // the slave-ID
	modbus_t *mctx;
	struct port_conf_info port_conf;
	/* the thread for data receivation will be blocked for waiting when no incomming data, set the follosing flag to true when in waiting state*/
	bool to_update_mslave_req; 
	pthread_mutex_t mtx_for_port_conf;

	enum mbus_byte_oder mbus_border; // the byte-order for Modbus data communication


	/*the local register mapping*/
	pthread_mutex_t mtx_for_mapping_queue;
	struct l_queue *mapping_queue; // the queue node ref@ modbus_mapping_t

	/*ref@ function-code like 0x08*/
	pthread_mutex_t mtx_for_state_set;
	struct counter_log_register_set state_set;

	// input delimiter
	uint8_t ch_input_delimiter;

	//the file-record reading buffer-queue
	/*Rule-1 : only one buffer node is allowed for each file*/
	struct l_queue *freading_buf_queue; // the queue node ref @ struct freading_buffer; R
	pthread_mutex_t mtx_for_freading_buf_queue;

	// definition for IPC;
	//NOTE!!!add a mutex when you try to modify the following variable-group in different threads.
	int32_t shmid;// shared-memory ID
	struct mslave_ipc_dtblock *pt_dtblock; // points to the shared-memory where the sensordata, configuration info is

	#if(0)
		uint8_t pt_shmem_buf[APP_SHM_TOTAL_SIZE]; //  memory to buffer data from DB  !!! Space for time 	
	#else
		uint8_t ipc_dtitem_buf[ SZ_IPC_BUF]; //appShmDataItem_t  define an item buffer to copy data item from shared memory one by one, to replace pt_shmem_buf in the future
		uint64_t bitmapval;
	#endif
	
	sem_t *pt_sem_for_shmem_dtblock; // for shared-memory access sychronization
	gatewayTableData_t gwconf; // the gateway configuration info from table Gateway
	uint32_t gwconf_hash;
	bool to_force_gwconf_update;
	deviceTableData_t dev_array[ BULLET_GW_SENSOR_NUM_MAX];
	uint32_t dev_hash;
	bool to_force_dev_update;
	#if(0)
	//sensorDataTableData_t sdata_array[BULLET_GW_SENSOR_NUM_MAX];
	#else
		/*it takes too much memory to buffer the sensordata, just keep it in buffer pointed to by pt_shmem_buf*/
	#endif
	uint32_t sdata_hash;
	bool to_force_sdata_update;

	
	/*...*/
};

#ifndef MSLAVE_APP_ENOBASE
	#define MSLAVE_APP_ENOBASE	(MODBUS_ENOBASE + MODBUS_EXCEPTION_MAX + 0x10) // DO NOT overwrite /* Native libmodbus error codes */ ref@ definition like EMBBADSLAVE.etc
#endif

enum error_code{
	ERR_NONE = 0,
	/* reserved for errno or libmodbus errno */
	ERR_UNKNOWN = MSLAVE_APP_ENOBASE + 1, /**/
	ERR_API_FAIL = MSLAVE_APP_ENOBASE + 2, /**/
	ERR_INVALID_PARAM = MSLAVE_APP_ENOBASE + 3,
	ERR_RES_UNAVAILABLE = MSLAVE_APP_ENOBASE + 4, 
	ERR_NO_PERMISSION = MSLAVE_APP_ENOBASE + 5,
	/*...*/
};


/*
*/
enum addr_group{
	ADDR_GP_UNKNOWN = 0,
	ADDR_GP_COIL = 1, // ref@MODBUS_FC_READ_COILS
	ADDR_GP_DISCRETE_INPUT = 2, //ref@MODBUS_FC_READ_DISCRETE_INPUTS
	ADDR_GP_INPUT_REGISTER = 3, //ref@MODBUS_FC_INPUTS_REGISTERS
	ADDR_GP_HOLDING_REGISTER = 4, // ref@MODBUS_FC_HOLDING_REGISTERS
	/*_____________________________*/
	ADDR_GP_REPORT_SLAVE_ID = 5, //ref@MODBUS_FC_REPORT_SLAVE_ID
	ADDR_GP_FIFO_QUEUE = 6, // ref@MODBUS_FC_READ_FIFO_QUEUE
	ADDR_GP_FILE_RECORD = 7, // ref@MODBUS_FC_READ_FILE_RECORD
	ADDR_GP_DIAGNOSTICS = 8, //ref@MODBUS_FC_DIAGNOSTIC
	ADDR_GP_COM_EVENT_COUNTER = 9, // ref@MODBUS_FC_GET_COM_EVENT_COUNTER
	ADDR_GP_COM_EVENT_LOG = 10, //ref@MODBUS_FC_GET_COM_EVENT_LOG
	ADDR_GP_DEV_ID = 11, // ref@MODBUS_FC_READ_DEV_ID
};


/*info-packet for file-reading
*/
struct file_reading_info_pkt{
	uint8_t ref_type; 
	uint16_t file_no; // range ->[0x1, 0xFFFF]
	uint16_t record_no; // range->[0x0, 0x270F] , NOTE!!! 0x270F is the max according to the specific
	uint16_t record_len; // NOTE!! the response might overflow when record_len to be read is too big
	//__________________________________________
	uint32_t file_len;
	uint8_t dtlen;
	uint8_t dtbuf[SZ_RTU_MSGBUF];
};



/*modbus register mapping info
*/
#ifndef MAX_REG_MAPPING
	#define MAX_REG_MAPPING	(1024U)
#endif
struct register_mapping_info
{
	bool flg;
	uint16_t offset;
	uint16_t mapping_num;
	uint16_t src[MAX_REG_MAPPING];
	uint16_t src_cnt;
	uint16_t des[MAX_REG_MAPPING];
	uint16_t des_cnt;

	uint16_t max_reg_addr;
	uint16_t min_reg_addr;
	
};

extern struct modbus_slave_controller  g_mslave_ctr;


struct modbus_slave_controller *app_mslave_get_ctr(void);

enum error_code app_mslave_init_ctr(struct modbus_slave_controller * pt_mslave_ctr);
enum error_code app_mslave_deinit_ctr(struct modbus_slave_controller * pt_mslave_ctr);

enum error_code app_mslave_set_mctx(struct modbus_slave_controller * pt_mslave_ctr, modbus_t * pt_mctx);
modbus_t* app_mslave_get_mctx(struct modbus_slave_controller * pt_mslave_ctr);

const char* app_mslave_strerror(enum error_code err_code);


enum error_code app_mslave_append_mapping(struct modbus_slave_controller*pt_mslave_ctr, modbus_mapping_t*pt_mapping);
modbus_mapping_t *app_mslave_locate_mapping(struct modbus_slave_controller*pt_mslave_ctr, const uint16_t kp_addr, enum addr_group);

uint16_t app_mslave_pick_starting_address(const uint8_t*pt_msg, const uint32_t len);
uint8_t app_mslave_pick_fun_code(const uint8_t*pt_msg, const uint32_t len);

enum error_code app_mslave_release_mapping_none_thread_safe(struct modbus_slave_controller*pt_mslave_ctr);



void app_mslave_mloop(struct modbus_slave_controller *pt_mslave_ctr);

enum error_code app_mslave_load_conf(struct modbus_slave_controller*pt_mslave_ctr);
enum error_code app_mslave_init_port( struct modbus_slave_controller *pt_mslave_ctr);

void app_mslave_dump_port_conf(const struct port_conf_info*pt_port_conf);

void app_mslave_set_to_update_mslave(struct modbus_slave_controller*pt_mslave_ctr, bool newstate);
bool app_mslave_get_to_update_mslave(struct modbus_slave_controller*pt_mslave_ctr);


enum mbus_byte_oder app_mslave_get_mbus_border(struct modbus_slave_controller *pt_mslave_ctr);
enum register_mode app_mslave_get_reg_mode(uint8_t regmdval);


void app_mslave_dump_mbus_mapping_info(const struct modbus_slave_controller*pt_mslave_ctr);
void app_mslave_dump_dev_info(const struct modbus_slave_controller*pt_mslave_ctr);

	
enum error_code app_mslave_load_reg_mapping_info( struct register_mapping_info *pt_mapping_info);
struct register_mapping_info *app_mslave_get_reg_mapping_info(void);
uint16_t app_mslave_get_reg_mapping_offset(void);
void app_mslave_dump_reg_mapping_info(struct register_mapping_info *pt_remap_info);


#endif

