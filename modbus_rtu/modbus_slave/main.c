/*for MODBUS-RTU  Slave
20241028
*/
#include "util_dbg.h"
#include "modbus.h"
#include "errno.h"
#include "app_mslave.h"
#include "app_mslave_ipc.h"
#include <string.h>

DBG_LOCAL_LOG_DEBUG


#if(0)
/*
To-do list

2. load configuration info before run into the main loop; -> done
3. add support to modify the register mapping when the Dev-list is modified; -> done
4. add support to reinitialize when port configuration is modified; 
1. add support for efficient mode;
5. the layout data in registers(Bullet, Insight-T and gateway) -> done; 

....

*/
#endif

// for logging
uint8_t g_log_level = CONFIG_LOG_LEVEL_SET;
#define LOG_LEVEL_PARAMETER_IDX (1)


int main(int argc, char**argv)
{
	pthread_t hld_pthread_ipc_service;
	
	modbus_t *pt_modbus_port = NULL;
	int tpint = 0;
	DBG_LOG_INFO("modbus process started ...\n\r");
	DBG_LOG_DEBUG("Number of arguments (#) = %d\n\r", argc);
	for(uint32_t i = 0; i < argc; i++)
	{
		DBG_LOG_DEBUG("argv[%d]: %s\n\r", i, argv[i]);
	}
	if(argc >= LOG_LEVEL_PARAMETER_IDX + 1)
	{
		uint8_t tmpu8 = (uint8_t)strtol(argv[LOG_LEVEL_PARAMETER_IDX], NULL, 10);
		if(tmpu8 < CONFIG_LOG_LEVEL_ALL)
		{
		g_log_level = tmpu8;
		} else
		{
		g_log_level = CONFIG_LOG_LEVEL_SET;
		}
	} else  // set to default 
	{
		g_log_level = CONFIG_LOG_LEVEL_SET;
	}
	
	DBG_LOG_INFO("===Applicaiton for MODBUS-RTU is running===");


	#if(0)
		// for API test only
		struct register_mapping_info kp_reg_mapping_info = {0};
		app_mslave_load_reg_mapping_info( &kp_reg_mapping_info);
		DBG_LOG_WARN("offset = %d ,mapping_num = %d,src_cnt = %d,des_cnt = %d,  flg = %d",\
			kp_reg_mapping_info.offset, kp_reg_mapping_info.mapping_num,\
			kp_reg_mapping_info.src_cnt, kp_reg_mapping_info.des_cnt, kp_reg_mapping_info.flg);
		DBG_LOG_WARN("max_reg_addr %d, min_reg_addr %d", kp_reg_mapping_info.max_reg_addr, kp_reg_mapping_info.min_reg_addr);
		return 0;
	#endif

	// to load the register remapping info
	app_mslave_load_reg_mapping_info(app_mslave_get_reg_mapping_info());
	//app_mslave_dump_reg_mapping_info(app_mslave_get_reg_mapping_info());
	
	
	#if(1)
		// to initialize the Modbus slave controller	
		app_mslave_init_ctr( app_mslave_get_ctr());
		app_mslave_ipc_load_conf( app_mslave_get_ctr());
	#endif

	#if(1)
		app_mslave_init_port( app_mslave_get_ctr());
	#else
		pt_modbus_port = modbus_new_rtu( "/dev/ttyS3", 115200, 'N', 8, 1);
		//pt_modbus_port = modbus_new_rtu( "/dev/ttyS3", 19200, 'N', 8, 1);

		if(NULL == pt_modbus_port)
		{
			DBG_LOG_ERR("fail to open modbus-port");
			return -1;
		}

		modbus_set_slave( pt_modbus_port, 0x01);
		modbus_set_debug( pt_modbus_port, TRUE);
		modbus_set_response_timeout( pt_modbus_port, 3, 0);
		
		if(modbus_connect( pt_modbus_port) == -1)
		{
			DBG_LOG_ERR("fail to connect to modbus-port");
			return -1;
		}
		//app_mslave_init_ctr( app_mslave_get_ctr());
		app_mslave_set_mctx( app_mslave_get_ctr(), pt_modbus_port);
	#endif


	#if(1)
	

		//to create mapping block
		app_mslave_release_mapping( app_mslave_get_ctr()); // to release the old mapping before a new is created
		app_mslave_create_mapping( app_mslave_get_ctr()); // create Modbus slave mapping
		
		//_____________________________________________________________________________________________

		/*to create thread for IPC service*/
	 	tpint = pthread_create( &hld_pthread_ipc_service, NULL, pthandler_mslave_ipc_service, NULL);
		if(0 != tpint)
		{
			DBG_LOG_ERR("pthread_create fail for %s", strerror(tpint));
		}
		app_mslave_mloop( &g_mslave_ctr);
	#else
		for(uint32_t i = 0; i < 65536; i++)
		{
			tpint = modbus_write_register( pt_modbus_port, 0x1 + i, 0x0 + i);
			if( tpint < 0)
			{
				DBG_LOG_ERR(" fail to write register, for %d", modbus_strerror( errno));
			}
		}
	#endif
	modbus_close( app_mslave_get_mctx( app_mslave_get_ctr()));
	modbus_free( app_mslave_get_mctx( app_mslave_get_ctr()));
	
	
	return 0;
}
















