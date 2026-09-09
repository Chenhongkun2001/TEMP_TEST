/*
*/
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>


#include "app_mslave_ipc.h"
#include "app_mslave.h"
#include "util_dbg.h"
#include "app_froto.h"

#include "sh_mem.h"
#include "app_mslave.h"

DBG_LOCAL_LOG_DEBUG



static void app_mslave_ipc_set_force_gwconf_update(struct modbus_slave_controller*pt_mslave_ctr, bool newstate);
static bool app_mslave_ipc_get_force_gwconf_update( struct modbus_slave_controller*pt_mslave_ctr);
static void app_mslave_ipc_set_force_dev_update(struct modbus_slave_controller*pt_mslave_ctr, bool newstate);
static bool app_mslave_ipc_get_force_dev_update(struct modbus_slave_controller*pt_mslave_ctr);
static void app_mslave_ipc_set_force_sdata_update(struct modbus_slave_controller*pt_mslave_ctr, bool newstate);
static bool app_mslave_ipc_get_force_sdata_update(struct modbus_slave_controller*pt_mslave_ctr);







//MK23U_____________________________________________________________________________

/*the layout of Predict Sensor data in Modbus Register table

...........
_______________________
000___name_id(2)
001____________________
xxx___sensor_name(35)
035____________________
xxx___sensor_mac(9)
043____________________
xxx___sensor_type(35)
077____________________
xxx___manufacture(35)
111____________________
<BOARD_TEMPERATURE_CURRENT>
....
xxx____________________
<VIBRATION_ACC_WAVE>
....
xxx____________________
<VIBTATION_ENV3_WAVE>
....
xxx____________________
<VIBRATION_VELOCITY_WAVE>
....
xxx____________________
<REMAINING_VOLUMN>
....
xxx____________________
<ADVANCED_ALGO_MACHINE_RUN_CODE>
....
xxx____________________
<ADVANCED_ALGO_MACHINE_CODICTION_CODE>
....
xxx____________________
<ADVANCED_ALGO_ALARM_CODE>
....
xxx____________________
<ADVANCED_ALGO_RMS_MAG_PRE>
....
xxx____________________
<ADVNACED_ALGO_RMS_VIB_PRE>
....
xxx____________________
<ENVIRONMENTAL_TEMPERATURE_CURRENT>
....
xxx____________________
<ENVIRONMENTAL_TEMPERATURE_MAX>
....
xxx____________________
<ENVIRONMENTAL_TEMPERATURE_MIN>
....
xxx____________________
<ADVANCED_ALGO_EST_RPM_START>
....
xxx____________________
<ADVANCED_ALGO_EST_RPM_END>
....
xxx____________________
<ADVANCED_ALGO_VEL_OV>
....
xxx____________________
<ADVANCED_ALGO_VEL_RSH>
....
xxx____________________
<ADVNACED_ALGO_VEL_MTR>
....
xxx____________________
<ADVANCED_ALGO_VEL_FAN>
....
xxx____________________
<ADVANCED_ALGO_VEL_PUMP>
....
xxx____________________
<ADVANCED_ALGO_ENV_OV>
....
xxx____________________
<ADVANCED_ALGO_ENV_BPFI>
....
xxx____________________
<ADVANCED_ALGO_ENV_BPFO>
....
xxx____________________
<ADVANCED_ALGO_ENV_BSF>
....
xxx____________________
<ADVNACED_ALGO_ENV_FTF>
....
xxx____________________
<ADVANCED_ALGO_ENV_RSH>
....
xxx____________________
<ADVNACED_ALGO_ACC_OV>
....
xxx____________________
<ADVANCED_ALGO_ACC_GEAR>
....
xxx____________________
<BLE_RSSI_CURRENT>
....
xxx____________________
_______________________
...........

*/


/* the layout of Insight-T data in Modbus Register table
...........
_______________________
000___name_id(2)
001____________________
xxx___sensor_name(35)
035____________________
xxx___sensor_mac(9)
043____________________
xxx___sensor_type(35)
077____________________
xxx___manufacture(35)
111____________________
<VOLTAGE_CURRENT>
....
xxx____________________
<BOARD_TEMPERATURE_CURRENT>
....
xxx____________________
<ENVIRONMENTAL_TEMPERATURE_CURRENT>
....
xxx____________________
<BLE_RSSI_CURRENT>
....
xxx____________________
_______________________
...........

*/




//MK23D_____________________________________________________________________________


//MK183U__________________________________

/*val@  the float value to be set 
ptdes @ point to buffer where the value is set to
*/
static void to_set_float(const float val, uint16_t *ptdes)
{
	enum mbus_byte_oder kp_border = app_mslave_get_mbus_border( app_mslave_get_ctr());
	float *ptfloat = &val;
	uint32_t *ptu32 = (uint32_t*)ptfloat;
	uint32_t kpu32 = *ptu32;
	uint8_t a, b, c, d;
	a = (kpu32 >> 24) & 0xFF;
	b = (kpu32 >> 16) & 0xFF;
	c = (kpu32 >> 8) & 0xFF;
	d = (kpu32 >> 0) & 0xFF;


	switch(kp_border)
	{
		
		case MBUS_BORDER_BADC:
		{
			ptdes[0] = (b << 8) | a;
			ptdes[1] = (d << 8) | c;
			break;
		}
		case MBUS_BORDER_CDAB:
		{
			ptdes[0] = (c << 8) | d;
			ptdes[1] = (a << 8) | b;
			break;
		}
		case MBUS_BORDER_DCBA:
		{
			ptdes[0] = (d << 8) | c;
			ptdes[1] = (b << 8) | a;
			break;
		}
		case MBUS_BORDER_ABCD:
		{// ABCD
			//break;
		}	
		default:
		{
			ptdes[0] = (a << 8) | b;
			ptdes[1] = (c << 8) | d;
			break;
		}
	}
	return;
}

/*
*/
static void to_set_uint32(const uint32_t val, uint16_t*ptdes)
{
	enum mbus_byte_oder kp_border = app_mslave_get_mbus_border( app_mslave_get_ctr());
	uint8_t a, b, c ,d;
	a = (val >> 24) & 0xFF;
	b = (val >> 16) & 0xFF;
	c = (val >>  8) & 0xFF;
	d = (val >>  0) & 0xFF;

	switch(kp_border)
	{
		case MBUS_BORDER_BADC:
		{
			ptdes[0] = (b << 8) | a;
			ptdes[1] = (d << 8) | c;
			break;
		}
		case MBUS_BORDER_CDAB:
		{
			ptdes[0] = (c << 8) | d;
			ptdes[1] = (a << 8) | b;
			break;
		}
		case MBUS_BORDER_DCBA:
		{
			ptdes[0] = (d << 8) | c;
			ptdes[1] = (b << 8) | a;
			break;
		}
		case MBUS_BORDER_ABCD:
		{
			//break;
		}
		default:
		{
			ptdes[0] = (a << 8) | b;
			ptdes[1] = (c << 8) | d;
			break;
		}
	}
	
	return;
} 

/*
*/
//static void to_set_uint16(const uint16_t val, uint16_t*ptdes)
void to_set_uint16(const uint16_t val, uint16_t*ptdes)
{
	enum mbus_byte_oder kp_border = app_mslave_get_mbus_border( app_mslave_get_ctr());
	uint8_t a, b;

	a = (val >> 8) & 0xFF;
	b = (val >> 0) & 0xFF;
	
	switch(kp_border)
	{
		case MBUS_BORDER_BADC:
		{
			//break;
		}
		case MBUS_BORDER_DCBA:
		{
			ptdes[0] = (b << 8) | a;
			break;
		}
		case MBUS_BORDER_CDAB:
		{
			//break;
		}
		case MBUS_BORDER_ABCD:
		{
			//break;
		}
		default:
		{	
			ptdes[0] = (a << 8) | b;
			break;
		}
	}
	return;
}


static void to_update_mbus_mapping_for_dbg(void)
{
	modbus_mapping_t * pt_mapping = NULL;
	uint16_t *ptu16 = NULL;
	pt_mapping = app_mslave_locate_mapping( app_mslave_get_ctr(), 0, ADDR_GP_HOLDING_REGISTER);
	if(NULL == pt_mapping)
	{
		DBG_LOG_ERR("fail to find the mapping");
		return;
	}

	ptu16 = pt_mapping->tab_registers;
	if(NULL == ptu16)
	{
		return;
	}
	to_set_float( 66.66f, ptu16);
	ptu16 = ptu16 + 2;
	to_set_uint32( 666666, ptu16);
	ptu16 = ptu16 + 2;
	to_set_uint16( 6666, ptu16);
	ptu16 = ptu16 + 1;
	to_set_uint32( 0x0A0B0C0D, ptu16);
	
}


//MK183D__________________________________




#if(1)
/*to write sensor data into register mapping table
pt_mapping @ the mapping block where the sensor data is updated to
pt_dtitem @ the sensor data to be updated to the mapping block 
addr @ offset where we write the sensor data to
ret@error_code

NOTE!!
1. 
*/
/*offset : the offset of of register index
*/
static enum error_code to_write_sensor_data_to_register_table( modbus_mapping_t *pt_mapping, const appShmDataItem_t_original *pt_shmdt, const uint16_t offset)
{
	enum error_code tpret = ERR_NONE;
	uint16_t *ptu16 = NULL;
	const uint32_t hld_sz_reg_tab = pt_mapping->nb_registers * sizeof(uint16_t); // in bytes
	uint32_t kpidx = 0; // the index of register

	const uint16_t*hld_ending_addr = (pt_mapping->tab_registers + pt_mapping->nb_registers);
	
	if((NULL == pt_mapping)||(NULL == pt_shmdt))
	{
		DBG_LOG_ERR("invalid parameters");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;	
	}

	//if(app_shm_data_type_sensordata != pt_shmdt->appShmDataType)	
	if(GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE != pt_shmdt->appShmDataType)
	{
		DBG_LOG_ERR("a sensordata item is expected here");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	if(pt_mapping->nb_registers <= offset)
	{
		tpret = ERR_UNKNOWN;//modbus_strerror(int errnum)
		DBG_LOG_ERR("invalid register offset");
		goto EXIT;
	}

	//if(( MAX_PRS_REGNUM > (pt_mapping->nb_registers - offset ))||( MAX_IST_REGNUM > (pt_mapping->nb_registers - offset)))	
	#if(0)
		if(( MODBUS_REG_SENSOR_LENGTH > (pt_mapping->nb_registers - offset )))
		{
			tpret = EMBMDATA;//modbus_strerror(int errnum)
			DBG_LOG_ERR("no enough space for data writting");
			goto EXIT;
		}
	#endif

	if(hld_sz_reg_tab <= 0)
	{
		DBG_LOG_ERR("unexpected register table size");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}


	

	#if(0)
	DBG_LOG_WARN("datatype = %d, offset = 0x%x, val = %f, 0x%x", pt_shmdt->sensorDataTableData.dataType, offset,\
	pt_shmdt->sensorDataTableData.value.data_float, *((uint32_t*)(&pt_shmdt->sensorDataTableData.value.data_float)));
	#else
		
	#endif
	
	// to write data into register table
	switch(pt_shmdt->sensorDataTableData.dataType)
	{
			
		//MK260U____Predict Sensor_________________________
		//ADVANCED_ALGO_VEL_OV [0,1]
		//#define DT_ADVANCED_ALGO_VEL_OV_OFFSET	(0x0)
		//#define DT_ADVANCED_ALGO_VEL_OV_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV:
		{ // float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_VEL_OV_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			
			
			break;
		}
		
		//ADVANCED_ALGO_ENV_OV [2,3]
		//#define DT_ADVANCED_ALGO_ENV_OV_OFFSET	(DT_ADVANCED_ALGO_VEL_OV_OFFSET + DT_ADVANCED_ALGO_VEL_OV_REGNUM)
		//#define DT_ADVANCED_ALGO_ENV_OV_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_OV_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			
			break;
		}	
		//ADVANCED_ALGO_ACC_OV	[4,5]
		//#define DT_ADVANCED_ALGO_ACC_OV_OFFSET	(DT_ADVANCED_ALGO_ENV_OV_OFFSET + DT_ADVANCED_ALGO_ENV_OV_REGNUM)
		//#define DT_ADVANCED_ALGO_ACC_OV_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ACC_OV_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			
			break;
		}	
		//ADVANCED_ALGO_VEL_RSH [6,7]
		//#define DT_ADVANCED_ALGO_VEL_RSH_OFFSET	(DT_ADVANCED_ALGO_ACC_OV_OFFSET + DT_ADVANCED_ALGO_ACC_OV_REGNUM)
		//#define DT_ADVANCED_ALGO_VEL_RSH_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_VEL_RSH_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);		
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_VEL_MTR [8,9]
		//#define DT_ADVANCED_ALGO_VEL_MTR_OFFSET	(DT_ADVANCED_ALGO_VEL_RSH_OFFSET + DT_ADVANCED_ALGO_VEL_RSH_REGNUM)
		//#define DT_ADVANCED_ALGO_VEL_MTR_ERGNUM (0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_VEL_MTR_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}		
		//ADVANCED_ALGO_VEL_FAN [10,11] 										   DT_ADVANCED_ALGO_VEL_MTR_ERGNUM 
		//#define DT_ADVANCED_ALGO_VEL_FAN_OFFSET	(DT_ADVANCED_ALGO_VEL_MTR_OFFSET + DT_ADVANCED_ALGO_VEL_MTR_ERGNUM)
		//#define DT_ADVANCED_ALGO_VEL_FAN_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_VEL_FAN_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float,  ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_VEL_PUMP [12,13]
		//#define DT_ADVANCED_ALGO_VEL_PUMP_OFFSET (DT_ADVANCED_ALGO_VEL_FAN_OFFSET + DT_ADVANCED_ALGO_VEL_FAN_REGNUM)
		//#define DT_ADVANCED_ALGO_VEL_PUMP_REGNUM (0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_VEL_PUMP_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);			
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif	
			break;
		}	
		//ADVANCED_ALGO_ENV_BPFI [14,15]
		//#define DT_ADVANCED_ALGO_ENV_BPFI_OFFSET (DT_ADVANCED_ALGO_VEL_PUMP_OFFSET + DT_ADVANCED_ALGO_VEL_PUMP_REGNUM)
		//#define DT_ADVANCED_ALGO_ENV_BPFI_REGNUM (0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_BPFI_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_ENV_BPFO	[16,17]
		//#define DT_ADVANCED_ALGO_ENV_BPFO_OFFSET (DT_ADVANCED_ALGO_ENV_BPFI_OFFSET + DT_ADVANCED_ALGO_ENV_BPFI_REGNUM)
		//#define DT_ADVANCED_ALGO_ENV_BPFO_REGNUM (0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_BPFO_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_ENV_BSF [18,19]
		//#define DT_ADVANCED_ALGO_ENV_BSF_OFFSET (DT_ADVANCED_ALGO_ENV_BPFO_OFFSET + DT_ADVANCED_ALGO_ENV_BPFO_REGNUM)
		//#define DT_ADVANCED_ALGO_ENV_BSF_REGNUM (0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_BSF_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_ENV_FTF [20,21]
		//#define DT_ADVANCED_ALGO_ENV_FTF_OFFSET	(DT_ADVANCED_ALGO_ENV_BSF_OFFSET + DT_ADVANCED_ALGO_ENV_BSF_REGNUM)
		//#define DT_ADVANCED_ALGO_ENV_FTF_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_FTF_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			//DBG_LOG_WARN("ADVANCED_ALGO_ENV_FTF is set to %f", pt_shmdt->sensorDataTableData.value.data_float);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_ENV_RSH [22,23]		
		//#define DT_ADVANCED_ALGO_ENV_RSH_OFFSET	(DT_ADVANCED_ALGO_ENV_FTF_OFFSET + DT_ADVANCED_ALGO_ENV_FTF_REGNUM)
		//#define DT_ADVANCED_ALGO_ENV_RSH_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH:
		{ // float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_RSH_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_ACC_GEAR [24,25]
		//#define DT_ADVANCED_ALGO_ACC_GEAR_OFFSET	(DT_ADVANCED_ALGO_ENV_RSH_OFFSET + DT_ADVANCED_ALGO_ENV_RSH_REGNUM)
		//#define DT_ADVANCED_ALGO_ACC_GEAR_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ACC_GEAR_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_EST_RPM_START [26,27]
		//#define DT_ADVANCED_ALGO_EST_RPM_START_OFFSET	(DT_ADVANCED_ALGO_ACC_GEAR_OFFSET + DT_ADVANCED_ALGO_ACC_GEAR_REGNUM)
		//#define DT_ADVANCED_ALGO_EST_RPM_START_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_START:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_EST_RPM_START_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_EST_RPM_END [28,29]
		//#define DT_ADVANCED_ALGO_EST_RPM_END_OFFSET	(DT_ADVANCED_ALGO_EST_RPM_START_OFFSET + DT_ADVANCED_ALGO_EST_RPM_START_REGNUM)
		//#define DT_ADVANCED_ALGO_EST_RPM_END_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_END:
		{ // float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_EST_RPM_END_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_RMS_MAG_PRE [30,31]
		//#define DT_ADVANCED_ALGO_RMS_MAG_PRE_OFFSET	(DT_ADVANCED_ALGO_EST_RPM_END_OFFSET + DT_ADVANCED_ALGO_EST_RPM_END_REGNUM)
		//#define DT_ADVANCED_ALGO_RMS_MAG_PRE_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_MAG_PRE:
		{ //float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_RMS_MAG_PRE_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);	
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		
		//ADVANCED_ALGO_RMS_VIB_PRE [32,33]
		//#define DT_ADVANCED_ALGO_RMS_VIB_PRE_OFFSET (DT_ADVANCED_ALGO_RMS_MAG_PRE_OFFSET + DT_ADVANCED_ALGO_RMS_MAG_PRE_REGNUM)
		//#define DT_ADVANCED_ALGO_RMS_VIB_PRE_REGNUM (0x02)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_VIB_PRE:
		{ // float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_RMS_VIB_PRE_OFFSET;
			#if(0)
				//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}	
		//ENVIRONMENTAL_TEMPERATURE_CURRENT [34]
		//#define DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET	(DT_ADVANCED_ALGO_RMS_VIB_PRE_OFFSET + DT_ADVANCED_ALGO_RMS_VIB_PRE_REGNUM)
		//#define DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM (0x01)
		case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT:
		{ // INT8   NOTE!!!
			int16_t tpi16 = 0;
			//memcpy( &tpu32, &pt_shmdt->sensorDataTableData.value.data_float, sizeof(uint32_t));
			#if(0)
			tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
			tpi16 = BYTE_REVERSE_U16(tpi16);
			
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET;
			MODBUS_SET_INT16_TO_INT8( ptu16, 0, tpi16);
			#else
				tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
				//tpi16 = BYTE_REVERSE_U16(tpi16);
				//kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET;
				//MODBUS_SET_INT16_TO_INT8( ptu16, 0, tpi16);
				if((ptu16 + 1) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_uint16( tpi16, ptu16);
			#endif

			
			#if(1)
			//last temperature sensing time [48,49] 	
			//#define DT_LAST_TEMP_SENSING_TIME (DT_LAST_VIB_SENSING_TIME_OFFSET + DT_LAST_VIB_SENSING_TIME_REGNUM)
			//#define DT_LAST_TEMP_SENSING_TIME_REGNUM	(0x02)
			uint32_t tpu32 = 0;
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_LAST_TEMP_SENSING_TIME_OFFSET;
			tpu32 = pt_shmdt->sensorDataTableData.sampleTime;
			if((ptu16 + 2) > hld_ending_addr)
			{
				DBG_LOG_ERR("mapping overflow");
				break;
			}
			to_set_uint32( tpu32, ptu16);
			#endif

			
			//DBG_LOG_WARN("NOTE!!! double check when things go wrong");
			break;
		}	
		//ENVIRONMENTAL_TEMPERATURE_MAX [35]
		//#define DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM)
		//#define DT_ENVRIONMENTAL_TEMPERATURE_MAX_REGNUM (0x01)
		case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MAX:
		{ // INT8
			int16_t tpi16 = 0;
			#if(0)
			//memcpy( &tpu32, &pt_shmdt->sensorDataTableData.value.data_float, sizeof(uint32_t));
			tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
			tpi16 = BYTE_REVERSE_U16(tpi16);
			
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET;
			MODBUS_SET_INT16_TO_INT8( ptu16, 0, tpi16);
			
			DBG_LOG_WARN("NOTE!!! double check when things go wrong");
			#else
				tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
				//tpi16 = BYTE_REVERSE_U16(tpi16);
				//kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET;
				//MODBUS_SET_INT16_TO_INT8( ptu16, 0, tpi16);
				if((ptu16 + 1) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_uint16( tpi16, ptu16);
			#endif
			break;
		}	
		//ENVIRONMENTAL_TEMPERATURE_MIN [36]
		//#define DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET (DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET + DT_ENVRIONMENTAL_TEMPERATURE_MAX_REGNUM)	
		//#define DT_ENVIRONMENTAL_TEMPERATURE_MIN_REGNUM (0x01)
		case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN:
		{ // INT8
			int16_t tpi16 = 0;
			#if(0)
			//memcpy( &tpu32, &pt_shmdt->sensorDataTableData.value.data_float, sizeof(uint32_t));
			tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
			tpi16 = BYTE_REVERSE_U16(tpi16);
			
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET;
			MODBUS_SET_INT16_TO_INT8( ptu16, 0, tpi16);
			
			DBG_LOG_WARN("NOTE!!! double check when things go wrong");
			#else
				tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
				//tpi16 = BYTE_REVERSE_U16(tpi16);
				//kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET;
				//MODBUS_SET_INT16_TO_INT8( ptu16, 0, tpi16);
				if((ptu16 + 1) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_uint16( tpi16, ptu16);
			#endif
			break;
		}	
		//ADVANCED_ALGO_ALARM_CODE [37,41]
		//#define DT_ADVANCED_ALGO_ALARM_CODE_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_MIN_REGNUM)
		//#define DT_ADVANCED_ALGO_ALARM_CODE_REGNUM	(0x5)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE:
		{ //  UINT8_array  the order !?
			uint16_t tpu16 = 0;
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ALARM_CODE_OFFSET;
			#if(0)
			for(uint32_t i = 0;i < DT_ADVANCED_ALGO_ALARM_CODE_REGNUM; i++)
			{
				tpu16 = pt_shmdt->sensorDataTableData.value.data_char[(i*SZ_MBUS_REG) % GW_PRO_MAX_STRING_LEN_BYTE] * 0x100 + pt_shmdt->sensorDataTableData.value.data_char[(i*SZ_MBUS_REG + 1) % GW_PRO_MAX_STRING_LEN_BYTE];
				tpu16 = BYTE_REVERSE_U16(tpu16);
				MODBUS_SET_INT16_TO_INT8( ptu16, i*SZ_MBUS_REG, tpu16);
			}
			#else
			// the ALARMING CODE is in the file
				uint32_t tpidx = 0;
				int32_t tpfd = 0;
				uint8_t tp_fpath[GW_PRO_MAX_STRING_LEN_BYTE] = {0};
				uint8_t tp_alarm_code[SZ_FROTO_ALARM_CODE] = {0};
				snprintf( tp_fpath, sizeof(tp_fpath), "%s", pt_shmdt->sensorDataTableData.value.data_char);
				if(0 != access(tp_fpath, R_OK))
				{
					DBG_LOG_ERR("file %s does not exist or no reading permission", tp_fpath);
					tpret = ERR_NO_PERMISSION;
					break;
				}
				tpfd = open( tp_fpath, O_RDONLY);
				if(tpfd < 0)
				{
					DBG_LOG_ERR("fail to open file %s, for %s", tp_fpath, strerror(errno));
					tpret = ERR_API_FAIL;
					break;
				}
				if(read(tpfd,(void*)tp_alarm_code, SZ_FROTO_ALARM_CODE) != SZ_FROTO_ALARM_CODE)
				{
					DBG_LOG_ERR("alarming code reading fail");
					tpret = ERR_API_FAIL;
					close(tpfd);
					break;
				}
				close(tpfd);

				if((ptu16 + DT_ADVANCED_ALGO_ALARM_CODE_REGNUM) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				
				tpidx = 0;
				while(1)
				{
					tpu16 = tp_alarm_code[tpidx % SZ_FROTO_ALARM_CODE] << 8;					
					tpidx = tpidx + 1;
					if(tpidx >= SZ_FROTO_ALARM_CODE)
					{
						#if(0)
							tpu16 = BYTE_REVERSE_U16( tpu16);						
							MODBUS_SET_INT16_TO_INT8( ptu16, (((tpidx - 1) / 2) % DT_ADVANCED_ALGO_ALARM_CODE_REGNUM )*SZ_MBUS_REG, tpu16);
						#else
							to_set_uint16( tpu16, ptu16 + (((tpidx-1) / 2) % DT_ADVANCED_ALGO_ALARM_CODE_REGNUM ));
						#endif
						break;
					}
					
					tpu16 = tpu16 + tp_alarm_code[tpidx % SZ_FROTO_ALARM_CODE];					
					tpidx = tpidx + 1;
					if(tpidx >= SZ_FROTO_ALARM_CODE)
					{	
						#if(0)
							tpu16 = BYTE_REVERSE_U16( tpu16);						
							MODBUS_SET_INT16_TO_INT8( ptu16, (((tpidx - 1) / 2) % DT_ADVANCED_ALGO_ALARM_CODE_REGNUM )*SZ_MBUS_REG, tpu16);
						#else
							to_set_uint16( tpu16, ptu16 + (((tpidx-1) / 2) % DT_ADVANCED_ALGO_ALARM_CODE_REGNUM ));
						#endif
						break;
					}
					#if(0)
					tpu16 = BYTE_REVERSE_U16( tpu16);						
					MODBUS_SET_INT16_TO_INT8( ptu16, (((tpidx-1) / 2) % DT_ADVANCED_ALGO_ALARM_CODE_REGNUM )*SZ_MBUS_REG, tpu16);
					#else
						//MODBUS_SET_INT16_TO_INT8
						to_set_uint16( tpu16, ptu16 + (((tpidx-1) / 2) % DT_ADVANCED_ALGO_ALARM_CODE_REGNUM ));
					#endif
				}
			#endif
			//DBG_LOG_WARN("NOTE!!! double check when things go wrong");
			break;
		}	
		//BLE_RSSI_CURRENT	[42]
		//#define DT_BLE_RSSI_CURRENT_OFFSET	(DT_ADVANCED_ALGO_ALARM_CODE_OFFSET + DT_ADVANCED_ALGO_ALARM_CODE_REGNUM)
		//#define DT_BLE_RSSI_CURRENT_REGNUM	(0x01)
		case SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT:
		{ //INT8
			int16_t tpi16 = 0;
			#if(0)	
			//uint16_t tpu16 = 0;
			//memcpy( &tpu32, &pt_shmdt->sensorDataTableData.value.data_float, sizeof(uint32_t));
			tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;			
			//DBG_LOG_WARN("NOTE!!! double check when things go wrong 0x%x", tpi16);
			tpi16 = BYTE_REVERSE_U16(tpi16);
			
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_BLE_RSSI_CURRENT_OFFSET;
			MODBUS_SET_INT16_TO_INT8( ptu16, 0, tpi16);
			#else
				tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;			
				ptu16 = pt_mapping->tab_registers + offset + DT_BLE_RSSI_CURRENT_OFFSET;

				if((ptu16 + 1) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_uint16( tpi16, ptu16);
			#endif
			//DBG_LOG_WARN("NOTE!!! double check when things go wrong 0x%x, 0x%x", *ptu16, tpi16);
			break;
		}	
		//REMAINING_VOLUME [43]
		//#define DT_REMAINING_VOLUME_OFFSET	(DT_BLE_RSSI_CURRENT_OFFSET + DT_BLE_RSSI_CURRENT_REGNUM)
		//#define DT_REMAINING_VOLUME_REGNUM	(0x01)
		case SKFChina_Common_MeasurementType_REMAINING_VOLUME:
		{ //UINT8
			uint16_t tpu16 = 0;
			#if(0)
			//memcpy( &tpu32, &pt_shmdt->sensorDataTableData.value.data_float, sizeof(uint32_t));
			tpu16 = pt_shmdt->sensorDataTableData.value.data_uint8;
			tpu16 = BYTE_REVERSE_U16(tpu16);
			
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_REMAINING_VOLUME_OFFSET;
			MODBUS_SET_INT16_TO_INT8( ptu16, 0, tpu16);
			#else
				//memcpy( &tpu32, &pt_shmdt->sensorDataTableData.value.data_float, sizeof(uint32_t));
				tpu16 = pt_shmdt->sensorDataTableData.value.data_uint8;
				ptu16 = pt_mapping->tab_registers + offset + DT_REMAINING_VOLUME_OFFSET;

				if((ptu16 + 1) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_uint16( tpu16, ptu16);
			#endif
			//DBG_LOG_WARN("NOTE!!! double check when things go wrong");
			break;
		}	
		//ADVANCED_ALGO_MACHINE_RUN_CODE	[44]
		//#define DT_ADVANCED_ALGO_MACHINE_RUN_CODE_OFFSET	(DT_REMAINING_VOLUME_OFFSET + DT_REMAINING_VOLUME_REGNUM)
		//#define DT_ADVANCED_ALGO_MACHINE_RUN_CODE_REGNUM	(0x01)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_RUN_CODE:
		{ // UINT8
			uint16_t tpu16 = 0;
			#if(0)
			//memcpy( &tpu32, &pt_shmdt->sensorDataTableData.value.data_float, sizeof(uint32_t));
			tpu16 = pt_shmdt->sensorDataTableData.value.data_uint8;
			tpu16 = BYTE_REVERSE_U16(tpu16);
			
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_MACHINE_RUN_CODE_OFFSET;
			MODBUS_SET_INT16_TO_INT8( ptu16, 0, tpu16);
			#else
				tpu16 = pt_shmdt->sensorDataTableData.value.data_uint8;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_MACHINE_RUN_CODE_OFFSET;

				if((ptu16 + 1) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_uint16( tpu16, ptu16);
			#endif
			
			//DBG_LOG_WARN("NOTE!!! double check when things go wrong");
			break;
		}	
		// ADVANCED_ALGO_MACHINE_CONDITION_CODE [45]
		//#define DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_OFFSET	(DT_ADVANCED_ALGO_MACHINE_RUN_CODE_OFFSET + DT_ADVANCED_ALGO_MACHINE_RUN_CODE_REGNUM)
		//#define DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_REGNUM (0x01)
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_CONDITON_CODE:
		{ // UINT8
			uint16_t tpu16 = 0;
			//memcpy( &tpu32, &pt_shmdt->sensorDataTableData.value.data_float, sizeof(uint32_t));
			#if(0)
			tpu16 = pt_shmdt->sensorDataTableData.value.data_uint8;
			tpu16 = BYTE_REVERSE_U16(tpu16);
			
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_OFFSET;
			MODBUS_SET_INT16_TO_INT8( ptu16, 0, tpu16);
			#else
				tpu16 = pt_shmdt->sensorDataTableData.value.data_uint8;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_OFFSET;

				if((ptu16 + 1) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_uint16( tpu16, ptu16);
			#endif
			
			//DBG_LOG_WARN("NOTE!!! double check when things go wrong");
			break;
		}
		// last vibration sensing time [46,47]
		//#define DT_LAST_VIB_SENSING_TIME_OFFSET	(DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_OFFSET + DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_REGNUM)
		//#define DT_LAST_VIB_SENSING_TIME_REGNUM	(0x02)

	

		#if(0)
		case ???:
		{ // uint32_t
			uint32_t tpu32 = 0;
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_LAST_VIB_SENSING_TIME_OFFSET;
			tpu32 = pt_shmdt->sensorDataTableData.value.....
			MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, 0, tpu32);
			break;
		}
		#else
			//#warning "Todo=========to complete"			
			//ref @ case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV:
		#endif
		default:
		{
			DBG_LOG_ERR("unknown data type %d", pt_shmdt->sensorDataTableData.dataType);
			tpret = EMBBADDATA;//modbus_strerror(int errnum)
			break;
		}
		//MK260D________________________________________
	}

	//last vibration sensing time
	switch(pt_shmdt->sensorDataTableData.dataType)
	{
		//ADVANCED_ALGO_VEL_OV,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV:
		{
			;
		}
		//ADVANCED_ALGO_VEL_RSH,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH:
		{
			;
		}
		//ADVANCED_ALGO_VEL_MTR,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR:
		{
			;
		}
		//ADVANCED_ALGO_VEL_FAN,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN:
		{
			;
		}
		//ADVANCED_ALGO_VEL_PUMP,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP:
		{
			;
		}
		//ADVANCED_ALGO_ENV_OV,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV:
		{
			;
		}
		//ADVANCED_ALGO_ENV_BPFI,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI:
		{
			;
		}
		//ADVANCED_ALGO_ENV_BPFO,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO:
		{
			;
		}
		//ADVANCED_ALGO_ENV_BSF,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF:
		{
			;
		}
		//ADVANCED_ALGO_ENV_FTF,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF:
		{
			;
		}
		//ADVANCED_ALGO_ENV_RSH,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH:
		{
			;
		}
		//ADVANCED_ALGO_ACC_OV,
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV:
		{
			;
		}
		//ADVANCED_ALGO_ACC_GEAR
		case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR:
		{
			
		#if(1)
				// last vibration sensing time [46,47]
				//#define DT_LAST_VIB_SENSING_TIME_OFFSET	(DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_OFFSET + DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_REGNUM)
				//#define DT_LAST_VIB_SENSING_TIME_REGNUM	(0x02)
				uint32_t tpu32 = 0;
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_LAST_VIB_SENSING_TIME_OFFSET;
				tpu32 = pt_shmdt->sensorDataTableData.sampleTime;
			#if(0)
			#else
					if((ptu16 + 2) > hld_ending_addr)
					{
						DBG_LOG_ERR("mapping overflow");
						break;
					}
					to_set_uint32( tpu32, ptu16);
			#endif
		#endif
			break;
		}
		default:
		{
			break;
		}
	}


	EXIT:
		return tpret;
}



static enum error_code to_write_sensor_bitmap_to_register_table( modbus_mapping_t *pt_mapping, uint64_t val, const uint16_t offset)
{
	enum error_code tpret = ERR_NONE;
	uint16_t *ptu16 = NULL;
	uint16_t tpu16 = 0;
	const uint32_t hld_sz_reg_tab = pt_mapping->nb_registers * sizeof(uint16_t);
	uint32_t kpidx = 0;

	const uint16_t *hld_ending_addr = (pt_mapping->tab_registers + pt_mapping->nb_registers);

	if(pt_mapping->nb_registers <= offset)
	{
		DBG_LOG_ERR("invalid parameters");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	if(hld_sz_reg_tab <= 0)
	{
		DBG_LOG_ERR("invalid parameters");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}
	

	//Update I	[50]
	//#define DT_UPDATE_I_OFFSET		(DT_LAST_TEMP_SENSING_TIME_OFFSET + DT_LAST_TEMP_SENSING_TIME_REGNUM)
	//#define DT_UPDATE_I_REGNUM	(0x01)
	tpu16 = (val) & 0xFF;
	ptu16 = pt_mapping->tab_registers + offset + DT_UPDATE_I_OFFSET;
	if((ptu16 + 1) > hld_ending_addr)
	{
		DBG_LOG_ERR("mapping overflow");
		goto EXIT;
	}
	to_set_uint16( tpu16, ptu16);
	
	//update II [51]
	//#define DT_UPDATE_II_OFFSET	(DT_UPDATE_I_OFFSET + DT_UPDATE_I_REGNUM)
	//#define DT_UPDATE_II_REGNUM	 (0x01)
	tpu16 = (val >> 16) & 0xFF;
	ptu16 = pt_mapping->tab_registers + offset + DT_UPDATE_II_OFFSET;
	if((ptu16 + 1) > hld_ending_addr)
	{
		DBG_LOG_ERR("mapping overflow");
		goto EXIT;
	}
	to_set_uint16( tpu16, ptu16);

	EXIT:
		return tpret;
}




#else
/*offset : the offset of of register index
*/
static enum error_code to_write_sensor_data_to_register_table( modbus_mapping_t *pt_mapping, const appShmDataItem_t *pt_shmdt, const uint16_t offset)
{
	enum error_code tpret = ERR_NONE;
	uint16_t *ptu16 = NULL;
	const uint32_t hld_sz_reg_tab = pt_mapping->nb_registers * sizeof(uint16_t); // in bytes
	uint32_t kpidx = 0; // the index of register
	
	if((NULL == pt_mapping)||(NULL == pt_shmdt))
	{
		DBG_LOG_ERR("invalid parameters");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;	
	}

	//if(app_shm_data_type_sensordata != pt_shmdt->appShmDataType)	
	if(GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE != pt_shmdt->appShmDataType)
	{
		DBG_LOG_ERR("a sensordata item is expected here");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	if(pt_mapping->nb_registers <= offset)
	{
		tpret = ERR_UNKNOWN;//modbus_strerror(int errnum)
		DBG_LOG_ERR("invalid register offset");
		goto EXIT;
	}

	//if(( MAX_PRS_REGNUM > (pt_mapping->nb_registers - offset ))||( MAX_IST_REGNUM > (pt_mapping->nb_registers - offset)))	
	if(( MAX_PRS_REGNUM > (pt_mapping->nb_registers - offset )))
	{
		tpret = EMBMDATA;//modbus_strerror(int errnum)
		DBG_LOG_ERR("no enough space for data writting");
		goto EXIT;
	}

	if(hld_sz_reg_tab <= 0)
	{
		DBG_LOG_ERR("unexpected register table size");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	
	// to write data into register table
		#if(1)
		//name_id
		//#define DT_NAME_ID_OFFSET	(0x0) // the offeset of snesor Data name_id
		//#define DT_NAME_ID_REGNUM	(0x2) // the number of registers occupied by name_id
		kpidx = 0;
		ptu16 = pt_mapping->tab_registers + offset + DT_NAME_ID_OFFSET;
		MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*sizeof(uint16_t), pt_shmdt->sensorDataTableData.nameId);		
		
		//sensor_name
		//#define DT_SENSOR_NAME_OFFSET	(DT_NAME_ID_OFFSET + DT_NAME_ID_REGNUM)
		//#define DT_SENSOR_NAME_REGNUM	(GW_PRO_MAX_STRING_LEN_BYTE / 2 + GW_PRO_MAX_STRING_LEN_BYTE % 2)
		ptu16 = pt_mapping->tab_registers + offset + DT_SENSOR_NAME_OFFSET;
		snprintf((uint8_t*)ptu16, DT_SENSOR_NAME_REGNUM*SZ_MBUS_REG, "%s", pt_shmdt->sensorDataTableData.BLESensorName);

		//sensor_mac
		//#define DT_SENSOR_MAC_OFFSET	(DT_SENSOR_NAME_OFFSET + DT_SENSOR_NAME_REGNUM)
		//#define DT_SENSOR_MAC_REGNUM	(0x09) //
		ptu16 = pt_mapping->tab_registers + offset + DT_SENSOR_MAC_OFFSET;
		snprintf((uint8_t*)ptu16, DT_SENSOR_MAC_REGNUM*SZ_MBUS_REG,"%s",pt_shmdt->sensorDataTableData.macAddr);

		//sensor_type
		//#define DT_SENSOR_TYPE_OFFSET	(DT_SENSOR_MAC_OFFSET + DT_SENSOR_MAC_REGNUM)
		//#define DT_SENSOR_TYPE_REGNUM	(GW_PRO_MAX_STRING_LEN_BYTE / 2 + GW_PRO_MAX_STRING_LEN_BYTE % 2)
		ptu16 = pt_mapping->tab_registers + offset + DT_SENSOR_TYPE_OFFSET;
		snprintf((uint8_t*)ptu16, DT_SENSOR_TYPE_REGNUM*SZ_MBUS_REG,"%s",pt_shmdt->sensorDataTableData.type);

		//manufacturer
		//#define DT_MANUFACTURER_OFFSET	(DT_SENSOR_TYPE_OFFSET + DT_SENSOR_TYPE_REGNUM)
		//#define DT_MANUFACTURER_REGNUM	(GW_PRO_MAX_STRING_LEN_BYTE / 2 + GW_PRO_MAX_STRING_LEN_BYTE % 2)
		ptu16 = pt_mapping->tab_registers + offset + DT_MANUFACTURER_OFFSET;
		snprintf((uint8_t*)ptu16, DT_MANUFACTURER_REGNUM*SZ_MBUS_REG,"%s",pt_shmdt->sensorDataTableData.manufacturer);

		switch(pt_shmdt->sensorDataTableData.dataType)
		{
			/*...........*/
			//VIBRATION_ACC_WAVE
			//#define DT_ACC_WAVE_OFFSET	(DT_MANUFACTURER_OFFSET + DT_MANUFACTURER_REGNUM)
			//#define DT_ACC_WAVE_REGNUM	(0x01) // NOTE!! assign a number to the file, for future reading operation
			case SKFChina_Common_MeasurementType_VIBRATION_ACC_WAVE:
			{
				DBG_LOG_WARN("Todo=====what to do for files?!");
				break;
			}
			/*...*/
			// VIBRATION_ENV3_WAVE
			//#define DT_ENV3_WAVE_OFFSET		(DT_ACC_WAVE_OFFSET + DT_ACC_WAVE_REGNUM)
			//#define DT_ENV3_WAVE_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_VIBRATION_ENV3_WAVE:
			{				
				DBG_LOG_WARN("Todo=====what to do for files?!");
				break;
			}
			/*...*/
			//VIBTATION_VELOCITY_WAVE
			//#define DT_VELOCITY_WAVE_OFFSET	(DT_ENV3_WAVE_OFFSET + DT_ENV3_WAVE_REGNUM)
			//#define DT_VELOCITY_WAVE_REGNUM	(0x01)
			/*...*/
			case SKFChina_Common_MeasurementType_VIBRATION_VELOCITY_WAVE:
			{				
				DBG_LOG_WARN("Todo=====what to do for files?!");
				break;
			}
			//ADVANCED_ALGO_MACHINE_RUN_CODE
			//#define DT_ALGO_MACHINE_RUN_CODE_OFFSET	(DT_VELOCITY_WAVE_OFFSET + DT_VELOCITY_WAVE_REGNUM)
			//#define DT_ALGO_MACHINE_RUN_CODE_REGNUM	(0x01)
			/*...*/
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_RUN_CODE:
			{	
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ALGO_MACHINE_RUN_CODE_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx * SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			//ADVNACED_ALGO_MACHINE_CONDITION_CODE
			//#define DT_ALGO_MACHINE_CODITION_CODE_OFFSET (DT_ALGO_MACHINE_RUN_CODE_OFFSET + DT_ALGO_MACHINE_RUN_CODE_REGNUM)
			//#define DT_ALGO_MACHINE_CODITION_CODE_REGNUM (0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_CONDITON_CODE:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ALGO_MACHINE_CODITION_CODE_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}

			//ADVNACED_ALGO_ALARM_CODE
			//#define DT_ALGO_ALARM_CODE_OFFSET (DT_ALGO_MACHINE_CODITION_CODE_OFFSET + DT_ALGO_MACHINE_CODITION_CODE_REGNUM)
			//#define DT_ALGO_ALARM_CODE_REGNUM (0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE:
			{				
				DBG_LOG_WARN("Todo=====what to do for files?!");
				break;
			}
			// ADVANCED_ALGO_RMS_MAG_PRE
			//#define DT_ALGO_RMS_MAG_PRE_OFFSET	(DT_ALGO_ALARM_CODE_OFFSET + DT_ALGO_ALARM_CODE_REGNUM)
			//#define DT_ALGO_RMS_MAG_PRE_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_MAG_PRE:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ALGO_RMS_MAG_PRE_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			//ADVANCED_ALGO_RMS_VIB_PRE
			//#define DT_ALGO_RMS_VIB_PRE_OFFSET	(DT_ALGO_RMS_MAG_PRE_OFFSET + DT_ALGO_RMS_MAG_PRE_REGNUM)
			//#define DT_ALGO_RMS_VIB_PRE_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_VIB_PRE:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ALGO_RMS_VIB_PRE_REGNUM;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			//ENVIRONMENTAL_TEMPERATURE_CURRENT
			//#define DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET	(DT_ALGO_RMS_VIB_PRE_OFFSET + DT_ALGO_RMS_VIB_PRE_REGNUM)
			//#define DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			// ENVIRONMENTAL_TEMPERATURE_MAX
			//#define DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM)
			//#define DT_ENVIRONMENTAL_TEMPERATURE_MAX_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MAX:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			//environmental_temperature_min
			//#define DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_MAX_REGNUM)
			//#define DT_ENVIRONMENTAL_TEMPERATURE_MIN_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}	
			//ADVANCED_ALGO_EST_RPM_START
			//#define DT_ADVANCED_ALGO_EST_RPM_START_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_MIN_REGNUM)
			//#define DT_ADVANCED_ALGO_EST_RPM_START_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_START:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_EST_RPM_START_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			//advanced_algo_est_rpm_end
			//#define DT_ADVANCED_ALGO_EST_RPM_END_OFFSET (DT_ADVANCED_ALGO_EST_RPM_START_OFFSET + DT_ADVANCED_ALGO_EST_RPM_START_REGNUM)
			//#define DT_ADVANCED_ALGO_EST_RPM_END_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_END:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_EST_RPM_END_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}

			//advanced_algo_vel_ov
			//#define DT_ADVANCED_ALGO_VEL_OV_OFFSET	(DT_ADVANCED_ALGO_EST_RPM_END_OFFSET + DT_ADVANCED_ALGO_EST_RPM_END_REGNUM)
			//#define DT_ADVANCED_ALGO_VEL_OV_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_VEL_OV_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}	
			//advanced_algo_vel_rsh
			//#define DT_ADVANCED_ALGO_VEL_RSH_OFFSET (DT_ADVANCED_ALGO_VEL_OV_OFFSET + DT_ADVANCED_ALGO_VEL_OV_REGNUM)
			//#define DT_ADVANCED_ALGO_VEL_RSH_REGNUM (0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_VEL_RSH_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			//advanced_algo_vel_mtr
			//#define DT_ADVANCED_ALGO_VEL_MTR_OFFSET	(DT_ADVANCED_ALGO_VEL_RSH_OFFSET + DT_ADVANCED_ALGO_VEL_RSH_REGNUM)
			//#define DT_ADVANCED_ALGO_VEL_MTR_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_VEL_MTR_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			// advanced_algo_vel_fan
			//#define DT_ADVANCED_ALGO_VEL_FAN_OFFSET	(DT_ADVANCED_ALGO_VEL_MTR_OFFSET + DT_ADVANCED_ALGO_VEL_MTR_REGNUM)
			//#define DT_ADVANCED_ALGO_VEL_FAN_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_VEL_FAN_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			// advanced_algo_vel_pump
			//#define DT_ADVANCED_ALGO_VEL_PUMP_OFFSET	(DT_ADVANCED_ALGO_VEL_FAN_OFFSET + DT_ADVANCED_ALGO_VEL_FAN_REGNUM)
			//#define DT_ADVANCED_ALGO_VEL_PUMP_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_VEL_PUMP_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			// advanced_algo_env_ov
			//#define DT_ADVANCED_ALGO_ENV_OV_OFFSET	(DT_ADVANCED_ALGO_VEL_PUMP_OFFSET + DT_ADVANCED_ALGO_VEL_PUMP_REGNUM)
			//#define DT_ADVANCED_ALGO_ENV_OV_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_OV_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			// advanced_algo_env_bpfi
			//#define DT_ADVANCED_ALGO_ENV_BPFI_OFFSET	(DT_ADVANCED_ALGO_ENV_OV_OFFSET + DT_ADVANCED_ALGO_ENV_OV_REGNUM)
			//#define DT_ADVANCED_ALGO_ENV_BPFI_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_BPFI_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			//advanced_algo_env_bpfo
			//#define DT_ADVANCED_ALGO_ENV_BPFO_OFFSET	(DT_ADVANCED_ALGO_ENV_BPFI_OFFSET + DT_ADVANCED_ALGO_ENV_BPFI_REGNUM)
			//#define DT_ADVANCED_ALGO_ENV_BPFO_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_BPFO_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			// advanced_algo_env_bsf
			//#define DT_ADVANCED_ALGO_ENV_BSF_OFFSET	(DT_ADVANCED_ALGO_ENV_BPFO_OFFSET + DT_ADVANCED_ALGO_ENV_BPFO_REGNUM)
			//#define DT_ADVANCED_ALGO_ENV_BSF_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_BSF_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
	
			// advanced_algo_env_ftf
			//#define DT_ADVANCED_ALGO_ENV_FTF_OFFSET	(DT_ADVANCED_ALGO_ENV_BSF_OFFSET + DT_ADVANCED_ALGO_ENV_BSF_REGNUM)
			//#define DT_ADVANCED_ALGO_ENV_FTF_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_FTF_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			// advanced_algo_env_rsh
			//#define DT_ADVANCED_ALGO_ENV_RSH_OFFSET	(DT_ADVANCED_ALGO_ENV_FTF_OFFSET + DT_ADVANCED_ALGO_ENV_FTF_REGNUM)
			//#define DT_ADVANCED_ALGO_ENV_RSH_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ENV_RSH_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			// advanced_algo_acc_ov
			//#define DT_ADVANCED_ALGO_ACC_OV_OFFSET	(DT_ADVANCED_ALGO_ENV_RSH_OFFSET + DT_ADVANCED_ALGO_ENV_RSH_REGNUM)
			//#define DT_ADVANCED_ALGO_ACC_OV_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ACC_OV_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}

			// advanced_algo_acc_gear
			//#define DT_ADVANCED_ALGO_ACC_GEAR_OFFSET	(DT_ADVANCED_ALGO_ACC_OV_OFFSET + DT_ADVANCED_ALGO_ACC_OV_REGNUM)
			//#define DT_ADVANCED_ALGO_ACC_GEAR_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_ADVANCED_ALGO_ACC_GEAR_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}

			// BLE_RSSI_CURRENT
			//#define DT_BLE_RSSI_CURRENT_OFFSET	(DT_ADVANCED_ALGO_ACC_GEAR_OFFSET + DT_ADVANCED_ALGO_ACC_GEAR_REGNUM)
			//#define DT_BLE_RSSI_CURRENT_REGNUM	(0x01)
			case SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT:
			{
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_BLE_RSSI_CURRENT_OFFSET;
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx *SZ_MBUS_REG,  (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
				break;
			}
			default:
			{
				DBG_LOG_ERR(" unknown measurement-type");
				tpret = EMBXILVAL; //modbus_strerror(int errnum)
				goto EXIT;
				break;
			}
		}
		#endif
	
	EXIT:
		return tpret;
}

#endif

/*offset : the offset of of register index
for Insight-T
*/
static enum error_code to_write_ist_data_to_register_table( modbus_mapping_t *pt_mapping, const appShmDataItem_t_original *pt_shmdt, const uint16_t offset)
{
	enum error_code tpret = ERR_NONE;
	uint16_t *ptu16 = NULL;
	const uint32_t hld_sz_reg_tab = pt_mapping->nb_registers * sizeof(uint16_t); // in bytes
	uint32_t kpidx = 0; // the index of register

	uint16_t* hld_ending_addr = (pt_mapping->tab_registers + pt_mapping->nb_registers);
	
	if((NULL == pt_mapping)||(NULL == pt_shmdt))
	{
		DBG_LOG_ERR("invalid parameters");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;	
	}

	//if(app_shm_data_type_sensordata != pt_shmdt->appShmDataType)	
	if(GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE != pt_shmdt->appShmDataType)
	{
		DBG_LOG_ERR("a sensordata item is expected here");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	if(pt_mapping->nb_registers <= offset)
	{
		tpret = ERR_UNKNOWN;//modbus_strerror(int errnum)
		DBG_LOG_ERR("invalid register offset");
		goto EXIT;
	}

	//if(( MAX_PRS_REGNUM > (pt_mapping->nb_registers - offset ))||( MAX_IST_REGNUM > (pt_mapping->nb_registers - offset)))	
	#if(0)
		if(( MODBUS_REG_SENSOR_LENGTH > (pt_mapping->nb_registers - offset )))
		{
			tpret = EMBMDATA;//modbus_strerror(int errnum)
			DBG_LOG_ERR("no enough space for data writting");
			goto EXIT;
		}
	#endif

	if(hld_sz_reg_tab <= 0)
	{
		DBG_LOG_ERR("unexpected register table size");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	
	// to write data into register table
	switch(pt_shmdt->sensorDataTableData.dataType)
	{
		//MK372U____Insight_T_____
		// BOARD_TEMPERATURE_CURRENT [0,1]
		//#define DT_IST_BOARD_TEMPERATURE_CURRENT_OFFSET	(0x0)
		//#define DT_IST_BOARD_TEMPERATURE_CURRENT_REGNUM	(0x02)
		case SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_CURRENT:
		{//float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_IST_BOARD_TEMPERATURE_CURRENT_OFFSET;
			#if(0)
			//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
			modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif
			break;
		}
		//VOLTAGE_CURRENT [2,3]
		//#define DT_VOLTAGE_CUTTENT_OFFSET (DT_IST_BOARD_TEMPERATURE_CURRENT_OFFSET + DT_IST_BOARD_TEMPERATURE_CURRENT_REGNUM)
		//#define DT_VOLTAGE_CURRENT_REGNUM (0x02)
		case SKFChina_Common_MeasurementType_VOLTAGE_CURRENT:
		{ // float
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_VOLTAGE_CUTTENT_OFFSET;
			#if(0)
			//MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx*SZ_MBUS_REG, (uint32_t)pt_shmdt->sensorDataTableData.value.data_float);
			modbus_set_float_abcd( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#else
				if((ptu16 + 2) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_float( pt_shmdt->sensorDataTableData.value.data_float, ptu16);
			#endif	
			break;			
		}
		//BLE_RSSI_CURRENT [4]
		//#define DT_IST_BLE_RSSI_CURRENT_OFFSET	(DT_VOLTAGE_CUTTENT_OFFSET + DT_VOLTAGE_CURRENT_REGNUM)
		//#define DT_IST_BLE_RSSI_CURRENT_REGNUM (0x01)
		case SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT:
		{//INT8
			int16_t tpi16 = 0;
			#if(0)	
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_IST_BLE_RSSI_CURRENT_OFFSET;
			tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
			tpi16 = BYTE_REVERSE_U16(tpi16);
			MODBUS_SET_INT16_TO_INT8( ptu16, kpidx, tpi16);
			#else
				ptu16 = pt_mapping->tab_registers + offset + DT_IST_BLE_RSSI_CURRENT_OFFSET;
				tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
				if((ptu16 + 1) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_uint16( tpi16, ptu16);
			#endif
			break;
		}
		//ENVIRONMENTAL_TEMPERATURE_CURRENT [5]
		//#define DT_IST_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET (DT_IST_BLE_RSSI_CURRENT_OFFSET + DT_IST_BLE_RSSI_CURRENT_REGNUM)
		//#define DT_IST_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM (0x01)
		case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT:
		{//INT8
			int16_t tpi16 = 0;
			#if(0)
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_IST_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET;
			tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
			tpi16 = BYTE_REVERSE_U16(tpi16);
			MODBUS_SET_INT16_TO_INT8( ptu16, kpidx, tpi16);
			#else
				ptu16 = pt_mapping->tab_registers + offset + DT_IST_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET;
				tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
				if((ptu16 + 1) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_uint16( tpi16, ptu16);
			#endif

			#if(1)
				// last temperature sensing time [46, 47]
				//#define DT_IST_TEMP_SENSING_TIME_OFFSET	(46)
				//#define DT_IST_TEMP_SENSING_TIME_REGNUM	(0x02)
				uint32_t tpu32 = 0;
				#if(0)
				//uint32_t val = 0;
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_IST_TEMP_SENSING_TIME_OFFSET;
				tpu32 = (uint32_t)pt_shmdt->sensorDataTableData.sampleTime;				
				//DBG_LOG_ERR("A the sample time is 0x%X , org 0x%x", tpu32, pt_shmdt->sensorDataTableData.sampleTime);
				//val = tpu32;
				//DBG_LOG_ERR("0x%x - 0x%x - 0x%x - 0x%x", (((uint32_t)val)&0xFF)<<24 , (((((uint32_t)val) >> 8)&0xFF)<<16) , (((((uint32_t)val) >> 16)&0xFF)<<8) , ((((uint32_t)val) >> 24)&0xFF));
				tpu32 = BYTE_REVERSE_IN_HWORD_U32(tpu32);
				//DBG_LOG_ERR("D----0x%x -- 0x%x", BYTE_REVERSE_U32(tpu32), (((((uint32_t)val)&0xFF)<<24) + (((((uint32_t)val) >> 8)&0xFF)<<16) + (((((uint32_t)val) >> 16)&0xFF)<<8) + ((((uint32_t)val) >> 24)&0xFF)));
				//DBG_LOG_ERR("B the sample time is 0x%X , org 0x%x", tpu32, pt_shmdt->sensorDataTableData.sampleTime);
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx, tpu32);
				//DBG_LOG_ERR("C the sample time is 0x%X , org 0x%x", tpu32, pt_shmdt->sensorDataTableData.sampleTime);
				#else
					ptu16 = pt_mapping->tab_registers + offset + DT_IST_TEMP_SENSING_TIME_OFFSET;
					tpu32 = (uint32_t)pt_shmdt->sensorDataTableData.sampleTime;		
					if((ptu16 + 2) > hld_ending_addr)
					{
						DBG_LOG_ERR("mapping overflow");
						break;
					}
					to_set_uint32( tpu32, ptu16);
				#endif
			#endif
			
			break;
		}

		//Update I 	[8]
		//#define DT_IST_UPDATE_I	(DT_IST_TEMP_SENSING_TIME_OFFSET + DT_IST_TEMP_SENSING_TIME_REGNUM)
		//#define DT_IST_UPDATE_I (0x01)


		
		#if(0)
		case ???:
		{
			uint32_t tpu32 = 0;
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_IST_TEMP_SENSING_TIME_OFFSET;
			tpu32 = pt_shmdt->sensorDataTableData.value....
			MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx, tpu32);
			break;
		}
		#else 
			//#warning "Todo==========to complete"
			//ref @ SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT
		#endif
		//MK372D__________________
		default:
		{
			DBG_LOG_ERR("unknown measurement-type");
			tpret = EMBBADDATA; //modbus_strerror(int errnum)
			break;
		}
	}


	EXIT:
		return tpret;
}



static enum error_code to_write_ist_bitmap_to_register_table( modbus_mapping_t *pt_mapping, uint64_t val, const uint16_t offset)
{
	enum error_code tpret = ERR_NONE;
	uint16_t *ptu16 = NULL;
	uint16_t tpu16 = 0;
	const uint32_t hld_sz_reg_tab = pt_mapping->nb_registers * sizeof(uint16_t);
	uint32_t kpidx = 0;

	const uint16_t *hld_ending_addr = (pt_mapping->tab_registers + pt_mapping->nb_registers);

	if(pt_mapping->nb_registers <= offset)
	{
		tpret = ERR_UNKNOWN;
		DBG_LOG_ERR("invalid register offset");
		goto EXIT;
	}

	if(hld_sz_reg_tab <= 0)
	{
		DBG_LOG_ERR("unexpected register table size");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	//Update I 	[8]
	//#define DT_IST_UPDATE_I_OFFSET	(DT_IST_TEMP_SENSING_TIME_OFFSET + DT_IST_TEMP_SENSING_TIME_REGNUM)
	//#define DT_IST_UPDATE_I_REGNUM (0x01)
	tpu16 = val & 0xFF;
	ptu16 = pt_mapping->tab_registers + offset + DT_IST_UPDATE_I_OFFSET;
	if((ptu16 + 1) > hld_ending_addr)
	{
		DBG_LOG_ERR("mapping overflow");
		goto EXIT;
	}
	to_set_uint16( tpu16, ptu16);

	EXIT:
		return tpret;
	
}




/*offset : the offset of of register index
for gateway
*/
static enum error_code to_write_gw_data_to_register_table( modbus_mapping_t *pt_mapping, const appShmDataItem_t_original *pt_shmdt, const uint16_t offset)
{
	enum error_code tpret = ERR_NONE;
	uint16_t *ptu16 = NULL;
	const uint32_t hld_sz_reg_tab = pt_mapping->nb_registers * sizeof(uint16_t); // in bytes
	uint32_t kpidx = 0; // the index of register

	uint16_t *hld_ending_addr = (pt_mapping->tab_registers + pt_mapping->nb_registers);
	
	if((NULL == pt_mapping)||(NULL == pt_shmdt))
	{
		DBG_LOG_ERR("invalid parameters");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;	
	}

	//if(app_shm_data_type_sensordata != pt_shmdt->appShmDataType)	
	if(GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE != pt_shmdt->appShmDataType)
	{
		DBG_LOG_ERR("a sensordata item is expected here");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	if(pt_mapping->nb_registers <= offset)
	{
		tpret = ERR_UNKNOWN;//modbus_strerror(int errnum)
		DBG_LOG_ERR("invalid register offset");
		goto EXIT;
	}

	//if(( MAX_PRS_REGNUM > (pt_mapping->nb_registers - offset ))||( MAX_IST_REGNUM > (pt_mapping->nb_registers - offset)))	
	if(( MODBUS_REG_SENSOR_LENGTH > (pt_mapping->nb_registers - offset )))
	{
		tpret = EMBMDATA;//modbus_strerror(int errnum)
		DBG_LOG_ERR("no enough space for data writting");
		goto EXIT;
	}

	if(hld_sz_reg_tab <= 0)
	{
		DBG_LOG_ERR("unexpected register table size");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	
	// to write data into register table
	switch(pt_shmdt->sensorDataTableData.dataType)
	{
		//MK258U___GW parameter______
		//BOARD_TEMPERATURE_CURRENT
		//#define DT_GW_BOARD_TEMPERATURE_CURRENT_OFFSET	(0x00)
		//#define DT_GW_BOARD_TEMPERATURE_CURRENT_REGNUM	(0x01)
		//MK258D_____________________
		case SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_CURRENT:
		{//INT8
			#if(1)
			int16_t tpi16 = 0;
			#if(0)
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_GW_BOARD_TEMPERATURE_CURRENT_OFFSET;
			tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;
			tpi16 = BYTE_REVERSE_U16(tpi16);
			MODBUS_SET_INT16_TO_INT8( ptu16, kpidx, tpi16);
			#else
				ptu16 = pt_mapping->tab_registers + offset + DT_GW_BOARD_TEMPERATURE_CURRENT_OFFSET;
				tpi16 = pt_shmdt->sensorDataTableData.value.data_int8;

				if((ptu16 + 1) > hld_ending_addr)
				{
					DBG_LOG_ERR("mapping overflow");
					break;
				}
				to_set_uint16( tpi16, ptu16);
			#endif
			
			#if(1)
				//last temperature sensing time [1,2]
				//#define DT_GW_LAST_TEMP_SENSING_TIME_OFFSET (0x01)	
				//#define DT_GW_LAST_TEMP_SENSING_TIME_REGNUM	(0x02)	
				uint32_t tpu32 = 0;
				#if(0)
				kpidx = 0;
				ptu16 = pt_mapping->tab_registers + offset + DT_GW_LAST_TEMP_SENSING_TIME_OFFSET;
				tpu32 = pt_shmdt->sensorDataTableData.sampleTime;
				tpu32 = BYTE_REVERSE_IN_HWORD_U32(tpu32);
				MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx, tpu32);
				#else
					ptu16 = pt_mapping->tab_registers + offset + DT_GW_LAST_TEMP_SENSING_TIME_OFFSET;
					tpu32 = pt_shmdt->sensorDataTableData.sampleTime;

					if((ptu16 + 2) > hld_ending_addr)
					{
						DBG_LOG_ERR("mapping overflow");
						break;
					}
					to_set_uint32( tpu32, ptu16);
				#endif
			#endif
			#else		
			#endif
			
			break;
		}
		#if(0)
		//last temperature sensing time [1,2]
		//#define DT_GW_LAST_TEMP_SENSING_TIME_OFFSET (0x01)	
		//#define DT_GW_LAST_TEMP_SENSING_TIME_REGNUM	(0x02)
		case ???:
		{
			uint32_t tpu32 = 0;
			kpidx = 0;
			ptu16 = pt_mapping->tab_registers + offset + DT_GW_LAST_TEMP_SENSING_TIME_OFFSET;
			tpu32 = pt_shmdt->sensorDataTableData.value....;
			MODBUS_SET_UINT32_TO_UINT8( ptu16, hld_sz_reg_tab, kpidx, tpu32);
			break;
		}
		#else
			//#warning "Todo==========to complete"
			//ref @  SKFChina_Common_MeasurementType_BOARD_TEMPERATURE_CURRENT
		#endif
		//MK372D__________________
		default:
		{
			DBG_LOG_ERR("unknown measurement-type");
			tpret = EMBBADDATA; //modbus_strerror(int errnum)
			break;
		}
	}


	EXIT:
		return tpret;
}






/* to check if the gateway-conf info read from shared-memory is valid or not
ret@true is returned when the info is valid
*/
static bool is_gateway_conf_valid(const gatewayTableData_t*pt_gwconf)
{
	#if(0) // the original
	bool tpret = true;

	//DBG_LOG_ERR("Todo==============to check if the gw-conf info valid");
	if((0 == pt_gwconf->lastEditTimeS)&&(0 == pt_gwconf->gwConfig.nameId))
	{
		tpret = false;
	}
	return tpret;

	#else //_____________________
		bool tpret = true;

		if((0 == pt_gwconf->lastEditTimeS)||(0 == pt_gwconf->gwConfig.nameId))
		{
			DBG_LOG_ERR("invalid lastEditTimes %d or nameId %d", pt_gwconf->lastEditTimeS, pt_gwconf->gwConfig.nameId);
			tpret = false;
			goto EXIT;
		}
		// to check parity Mode
		switch(pt_gwconf->gwConfig.modbusConfig.paritySlave)
		{
			case MODBUS_PARITY_MODE_NONE://MODBUS_PARITY_MODE_NONE = 0,
			{
				;
			}
			case MODBUS_PARITY_MODE_ODD://MODBUS_PARITY_MODE_ODD,
			{
				;
			}
			case MODBUS_PARITY_MODE_EVEN://MODBUS_PARITY_MODE_EVEN,
			{
				break;
			}
			default:
			{
				DBG_LOG_ERR("invalid parity mode");
				tpret = false;
				goto EXIT;
				break;
			}
		}
		// to check stop-bit
		switch(pt_gwconf->gwConfig.modbusConfig.stopSlave)
		{
			case MODBUS_STOP_1_BIT://MODBUS_STOP_1_BIT = 0,
			{
				;
			}
			case MODBUS_STOP_15_BIT://MODBUS_STOP_15_BIT,
			{
				;
			}
			case MODBUS_STOP_2_BIT://MODBUS_STOP_2_BIT,
			{
				break;
			}
			default:
			{
				DBG_LOG_ERR("invlaid stop bit");
				tpret = false;
				goto EXIT;
				break;
			}
		}
		// to check the baudrate
		if((pt_gwconf->gwConfig.modbusConfig.baudrateSlave < MIN_MBUS_BAUDRATE)||(pt_gwconf->gwConfig.modbusConfig.baudrateSlave > MAX_MBUS_BAUDRATE))
		{
			DBG_LOG_ERR("the baudrate is out of range[ %d, %d]", MIN_MBUS_BAUDRATE, MAX_MBUS_BAUDRATE);
			tpret = false;	
			goto EXIT;
		}

		// to check the Slave ID
		if((pt_gwconf->gwConfig.modbusConfig.slaveAddrSlave < MIN_MBUS_SADDR)||(pt_gwconf->gwConfig.modbusConfig.slaveAddrSlave > MAX_MBUS_SADDR))
		{
			DBG_LOG_ERR("the slave ADDR should be in the range of [ %d, %d]", MIN_MBUS_SADDR, MAX_MBUS_SADDR);
			tpret = false;
			goto EXIT;
		}
		// the register mode
		if((REG_MD_EXTENSIBLE != app_mslave_get_reg_mode(pt_gwconf->gwConfig.modbusConfig.registerMode)) && \
			(REG_MD_EFFICIENT != app_mslave_get_reg_mode(pt_gwconf->gwConfig.modbusConfig.registerMode)))
		{
			DBG_LOG_ERR("invalid register mode");
			tpret = false;
			goto EXIT;
		}
		// well done
		EXIT:
			return tpret;
	#endif
}

/* to check if the dev-info read from shared-memory is valid or not
ret@true is returned when the info is valid
*/
static bool is_dev_info_valid(const deviceTableData_t*pt_devinfo)
{
	bool tpret = true;

	//DBG_LOG_ERR("TODO===========to check if the dev-info is valid");
	if(0 == pt_devinfo->nameId)
	{
		tpret = false;
	}
	
	return tpret;
}

/*to check if the sensordata read from shared-memory is valid or not
ret@true is returned when sensordata is valid
*/
static bool is_sensor_data_valid(const sensorDataTableData_t *pt_sdata)
{
	bool tpret = true;

	//DBG_LOG_ERR("TODO============to check if the sensordata is valid");
	if(0 == pt_sdata->nameId)
	{
		tpret = false;
	}
	
	return tpret;
}

#if(0)// for test
#define NM_ID_BASE (80000000U)
/*
*/
void test_fill_mem_buf(struct modbus_slave_controller*pt_mslave_ctr)
{
	appShmDataItem_t kp_shm_dtitem = {0};
	uint8_t kp_nmstr[GW_PRO_MAX_STRING_LEN_BYTE] = {0};
	uint32_t kp_nmid = NM_ID_BASE;
	uint32_t kp_mem_addr = 0;
	uint16_t kp_addr = 0, kp_len = MODBUS_REG_SENSOR_LENGTH;
	static uint32_t kp_cnt = 0;

	static uint32_t kp_baudrate = 115200U;
	static uint32_t kp_reg_mode = REG_MD_EXTENSIBLE;

	if(0 != kp_cnt % 3)
	{		
		kp_cnt ++;
		return;
	}
	kp_cnt ++;

	#if(0)
		if(115200U == kp_baudrate)
		{
			kp_baudrate = 19200;
		}
		else
		{
			kp_baudrate = 115200;
		}
		if(REG_MD_EXTENSIBLE == kp_reg_mode)
		{
			kp_reg_mode = REG_MD_EFFICIENT;
		}
		else
		{
			kp_reg_mode = REG_MD_EXTENSIBLE;
		}		
		DBG_LOG_ERR("the baudrate is updated to %d, reg_mode = %d", kp_baudrate, kp_reg_mode);
		//sleep(3);
	#else
		kp_baudrate = 115200;
		kp_reg_mode = REG_MD_EXTENSIBLE;
	#endif
	
	
	if(NULL == pt_mslave_ctr)
	{
		return;
	}
	DBG_LOG_INFO("BKP");
	
	memset( pt_mslave_ctr->pt_shmem_buf, 0, APP_SHM_TOTAL_SIZE);

	if(sizeof(kp_shm_dtitem) > APP_SHM_GATEWAY_TABLE_ITEM_SIZE)\
	{
		DBG_LOG_ERR("no enough mem-space for gw-info writting");
		return;
	}
	// gateway-info
	kp_shm_dtitem.appShmDataType = GW_PRO_TABLE_IDX_GW_CONFIG_TABLE;
	kp_shm_dtitem.gwTableData.lastEditTimeS = 123456789;//time(NULL);
	kp_shm_dtitem.gwTableData.version = 666;
	kp_shm_dtitem.gwTableData.gwConfig.nameId = 666;
	kp_shm_dtitem.gwTableData.gwConfig.modbusConfig.baudrateSlave = kp_baudrate;//115200;
	kp_shm_dtitem.gwTableData.gwConfig.modbusConfig.paritySlave= PARITY_NONE;
	kp_shm_dtitem.gwTableData.gwConfig.modbusConfig.registerMode = kp_reg_mode;//REG_MD_EXTENSIBLE;
	kp_shm_dtitem.gwTableData.gwConfig.modbusConfig.slaveAddrSlave = 1;
	kp_shm_dtitem.gwTableData.gwConfig.modbusConfig.stopSlave = SBIT_1;
	if(sizeof(kp_shm_dtitem) < APP_SHM_GATEWAY_TABLE_ITEM_MAX)
	{
		DBG_LOG_ERR("no enough memory space for data writting-op");
		return;
	}
	memcpy( &pt_mslave_ctr->pt_shmem_buf[APP_SHM_GATEWAY_TABLE_ADDR_OFFSET], &kp_shm_dtitem, sizeof(kp_shm_dtitem));


	
	//DBG_LOG_INFO("BKP");
	//the dev-list
	if(sizeof(kp_shm_dtitem) > APP_SHM_DEV_TABLE_ITEM_SIZE)
	{
		DBG_LOG_ERR("no enought memory space for dev-info writting");
		return;
	}
	memset( &kp_shm_dtitem, 0, sizeof(appShmDataItem_t));

	//DBG_LOG_INFO("BKP");
		
	kp_shm_dtitem.appShmDataType = GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE;
	//kp_shm_dtitem.devTableData.nameId = kp_nmid;
	//kp_shm_dtitem.devTableData.name
	snprintf( kp_shm_dtitem.devTableData.type, GW_PRO_MAX_STRING_LEN_BYTE, "%s", TYPE_STR_INSIGHT_P);
	snprintf( kp_shm_dtitem.devTableData.manufacturer, GW_PRO_MAX_STRING_LEN_BYTE, "%s", "SKF");
	snprintf( kp_shm_dtitem.devTableData.macAddr, GW_PRO_MAX_STRING_LEN_BYTE, "%s", "C4:BD:6A:10:00:20");
	snprintf( kp_shm_dtitem.devTableData.BLESensorName, GW_PRO_MAX_STRING_LEN_BYTE, "%s", "PRS100020");
	
	//DBG_LOG_INFO("BKP");
	
	kp_addr = MODBUS_REG_SENSOR_START_ADDR_OFFSET;
	kp_len = MODBUS_REG_SENSOR_LENGTH;
		
	for(uint32_t i = 0; i < BULLET_GW_SENSOR_NUM_MAX; i++)
	{
		snprintf( kp_nmstr, GW_PRO_MAX_STRING_LEN_BYTE, SENSOR_NAME_BUILD_STRING, 0, kp_addr, kp_len, MODBUS_PRODUCT_TYPE_PREDICT_SENSOR);
		kp_addr = kp_addr + kp_len;
		
		memset( kp_shm_dtitem.devTableData.name, 0, GW_PRO_MAX_STRING_LEN_BYTE);
		snprintf( kp_shm_dtitem.devTableData.name, GW_PRO_MAX_STRING_LEN_BYTE, "%s", kp_nmstr);
		kp_shm_dtitem.devTableData.nameId = kp_nmid;

		//DBG_LOG_ERR("BKP");
		
		kp_mem_addr = APP_SHM_DEV_TABLE_ADDR_OFFSET + i * APP_SHM_DEV_TABLE_ITEM_SIZE ;
		memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
		
		kp_nmid = kp_nmid + 1;
	}

	
	//DBG_LOG_INFO("BKP");
	// the sensor data
	if(sizeof(appShmDataItem_t) > APP_SHM_SENSORDATA_TABLE_PIECE_SIZE)
	{
		DBG_LOG_ERR("no enough mem-space for sensor-data writting");
		return;
	}
	memset( &kp_shm_dtitem, 0, sizeof(appShmDataItem_t));

	kp_shm_dtitem.appShmDataType = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
	//kp_shm_dtitem.sensorDataTableData.nameId =
	kp_shm_dtitem.sensorDataTableData.dataType =  SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV;//SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT; 
	kp_shm_dtitem.sensorDataTableData.product = SKFChina_Common_ProductType_BULLET_NODE;
	kp_shm_dtitem.sensorDataTableData.value.data_float = 0xAABBCCDD;//666;
	kp_shm_dtitem.sensorDataTableData.sampleTime = 123456789;

	kp_nmid = NM_ID_BASE;
	for(uint32_t i = 0; i < BULLET_GW_SENSOR_NUM_MAX; i++)
	//for(uint32_t i = 0; i < 1; i++)		
	{
		kp_shm_dtitem.sensorDataTableData.nameId = kp_nmid;
		#if(0)
		for(uint32_t j = 0; j < APP_SHM_SENSORDATA_TABLE_PIECE_MAX; j ++)
		{
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * j + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
		}
		#else
			uint32_t j = 0;
			//K260U____Predict Sensor_________________________
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV:
			// float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 123.123;//66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_OV;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;						
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV:
			// float			
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_OV;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;	
				
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV:
			//float			
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_OV;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH:
			//float			
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_RSH;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR:
			//float			
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_MTR;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN:
			//float			
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_FAN;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP:
			//float			
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_VEL_PUMP;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI:
			// float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFI;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO:
			//float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BPFO;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF:
			//float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_BSF;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF:
			//float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_FTF;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH:
			// float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ENV_RSH;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR:
			// float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ACC_GEAR;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_START:
			//float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_START;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_END:
			//float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_EST_RPM_END;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_MAG_PRE:
			//float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_MAG_PRE;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_VIB_PRE:
			//float
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_float = 66.0f;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_RMS_VIB_PRE;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT:
			// INT8
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_int8 = 66;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MAX:
			//INT8
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_int8 = 66;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MAX;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN:
			//INT8
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_int8 = 66;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_MIN;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE:
			//INT8-array
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			#if(0)
			kp_shm_dtitem.sensorDataTableData.value.data_char[0 % GW_PRO_MAX_STRING_LEN_BYTE] = 66;
			kp_shm_dtitem.sensorDataTableData.value.data_char[1 % GW_PRO_MAX_STRING_LEN_BYTE] = 66;
			kp_shm_dtitem.sensorDataTableData.value.data_char[2 % GW_PRO_MAX_STRING_LEN_BYTE] = 66;
			kp_shm_dtitem.sensorDataTableData.value.data_char[3 % GW_PRO_MAX_STRING_LEN_BYTE] = 66;
			kp_shm_dtitem.sensorDataTableData.value.data_char[4 % GW_PRO_MAX_STRING_LEN_BYTE] = 66;
			kp_shm_dtitem.sensorDataTableData.value.data_char[5 % GW_PRO_MAX_STRING_LEN_BYTE] = 66;
			kp_shm_dtitem.sensorDataTableData.value.data_char[6 % GW_PRO_MAX_STRING_LEN_BYTE] = 66;
			kp_shm_dtitem.sensorDataTableData.value.data_char[7 % GW_PRO_MAX_STRING_LEN_BYTE] = 66;
			kp_shm_dtitem.sensorDataTableData.value.data_char[8 % GW_PRO_MAX_STRING_LEN_BYTE] = 66;
			kp_shm_dtitem.sensorDataTableData.value.data_char[9 % GW_PRO_MAX_STRING_LEN_BYTE] = 66;
			#else
				snprintf( kp_shm_dtitem.sensorDataTableData.value.data_char, GW_PRO_MAX_STRING_LEN_BYTE,"%s","./alarm_code");
			#endif
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_ALARM_CODE;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//case SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT:
			//INT8
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_int8 = 66;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_BLE_RSSI_CURRENT;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;

			//case SKFChina_Common_MeasurementType_REMAINING_VOLUME:
			//UINT8
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_uint8 = 66;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_REMAINING_VOLUME;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_RUN_CODE:
			//UINT8
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_uint8 = 66;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_RUN_CODE;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			//case SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_CONDITON_CODE:
			//UINT8
			memset( &kp_shm_dtitem.sensorDataTableData.value, 0, sizeof(measure_data_t));
			kp_shm_dtitem.sensorDataTableData.value.data_uint8 = 66;	
			kp_shm_dtitem.sensorDataTableData.dataType = SKFChina_Common_MeasurementType_ADVANCED_ALGO_MACHINE_CONDITON_CODE;
			kp_mem_addr = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX  + j) * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			memcpy( &pt_mslave_ctr->pt_shmem_buf[kp_mem_addr % APP_SHM_TOTAL_SIZE], &kp_shm_dtitem, sizeof(appShmDataItem_t));
			j = j + 1;
			
			//MK260D________________________________________
		#endif
		kp_nmid = kp_nmid + 1;
	}	
	//DBG_LOG_INFO("BKP");
}
#endif


#if(0)
/*to load data from shared-memory
ret@error_code
*/
static enum error_code to_load_data_from_shmem(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t tpu32 = 0;
	appShmDataItem_t *pt_shmem_dtitem = {0};

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	DBG_LOG_WARN("to wait for sh-memory be ready %d", time(NULL));
	#if(1)
		sem_wait( pt_mslave_ctr->pt_sem_for_shmem_dtblock);
		memcpy( pt_mslave_ctr->pt_shmem_buf, pt_mslave_ctr->pt_dtblock, APP_SHM_TOTAL_SIZE);
		sem_post( pt_mslave_ctr->pt_sem_for_shmem_dtblock);
	#else
		/*only for test*/
		DBG_LOG_WARN("data for test is loaded, go the other branch for the real thing");
		test_fill_mem_buf( pt_mslave_ctr);
	#endif
	DBG_LOG_WARN("BKP  %d ", time(NULL));
	
	// to check the gateway-conf
	if(sizeof(appShmDataItem_t) >= APP_SHM_GATEWAY_TABLE_ITEM_SIZE)
	{
		DBG_LOG_ERR("buffer might overflow");
		tpret = EMBBADDATA;
		goto EXIT;
	}
	//tpu16 = util_tool_cal_crc16( &pt_mslave_ctr->pt_shmem_buf[APP_SHM_GATEWAY_TABLE_ADDR_OFFSET % APP_SHM_TOTAL_SIZE], APP_SHM_GATEWAY_TABLE_ITEM_SIZE);
	tpu32 = util_tool_cal_elfhash((const uint8_t *) &pt_mslave_ctr->pt_shmem_buf[APP_SHM_GATEWAY_TABLE_ADDR_OFFSET % APP_SHM_TOTAL_SIZE],  APP_SHM_GATEWAY_TABLE_ITEM_SIZE);
	if(tpu32 != pt_mslave_ctr->gwconf_hash)
	{
		pt_shmem_dtitem = (appShmDataItem_t*)&pt_mslave_ctr->pt_shmem_buf[APP_SHM_GATEWAY_TABLE_ADDR_OFFSET % APP_SHM_TOTAL_SIZE];
		//if(app_shm_data_type_gateway != pt_shmem_dtitem->appShmDataType)
		if(GW_PRO_TABLE_IDX_GW_CONFIG_TABLE != pt_shmem_dtitem->appShmDataType)			
		{
			DBG_LOG_ERR("no valid gateway-conf info available");
			tpret = EMBXILVAL;//modbus_strerror(int errnum)
			goto EXIT;
		}

		if(true != is_gateway_conf_valid( &pt_shmem_dtitem->gwTableData))
		{
			DBG_LOG_ERR("Illegal gateway-conf info");
			tpret = EMBXILVAL; //modbus_strerror(int errnum)
			goto EXIT;
		}
		
		//Todo==========we probably need to reinitialize the serial-port when the conf-info is modified
		memcpy( &pt_mslave_ctr->gwconf, &pt_shmem_dtitem->gwTableData, sizeof(gatewayTableData_t));
		pt_mslave_ctr->gwconf_hash = tpu32; // update the CRC16
	}
	

	// Dev list
	if(sizeof(appShmDataItem_t) >= APP_SHM_DEV_TABLE_ITEM_SIZE)
	{
		DBG_LOG_ERR("buffer might overflow");
		tpret = EMBBADDATA;
		goto EXIT;
	}
	tpu32 = util_tool_cal_elfhash( (const uint8_t *)&pt_mslave_ctr->pt_shmem_buf[APP_SHM_DEV_TABLE_ADDR_OFFSET % APP_SHM_TOTAL_SIZE], APP_SHM_DEV_TABLE_ITEM_SIZE*APP_SHM_DEV_TABLE_ITEM_MAX);
	if(tpu32 != pt_mslave_ctr->dev_hash)
	{
		for(uint32_t i = 0; i < APP_SHM_DEV_TABLE_ITEM_MAX; i++)
		{
			pt_shmem_dtitem = (appShmDataItem_t *)&pt_mslave_ctr->pt_shmem_buf[APP_SHM_DEV_TABLE_ADDR_OFFSET + i*APP_SHM_DEV_TABLE_ITEM_SIZE];
			//if( app_shm_data_type_dev != pt_shmem_dtitem->appShmDataType)			
			if( GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE != pt_shmem_dtitem->appShmDataType)
			{
				DBG_LOG_ERR("invalid DEV data_type");
				#if(0)
					tpret = EMBXILVAL;//modbus_strerror(int errnum)
					goto EXIT;
				#else
					memset( &pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX], 0, sizeof(deviceTableData_t));
					continue;
				#endif
			}

			if(true != is_dev_info_valid( &pt_shmem_dtitem->devTableData))
			{
				DBG_LOG_ERR("the DEV-info is invalid");
				#if(0)
					tpret =  EMBXILVAL;
					goto EXIT;
				#else						
					memset( &pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX], 0, sizeof(deviceTableData_t));
					continue;
				#endif
			}

			memcpy( &pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX], &pt_shmem_dtitem->devTableData, sizeof(deviceTableData_t));
		}
		pt_mslave_ctr->dev_hash = tpu32;
	}

	#if(1) // commented for test	
	//DBG_LOG_INFO("BKP");
	
	// sensor-data....
	if(sizeof(appShmDataItem_t) >= APP_SHM_SENSORDATA_TABLE_PIECE_SIZE)
	{
		DBG_LOG_ERR("buffer might overflow, sizeof(xx)=%d, unit-sz=%d", sizeof(appShmDataItem_t), APP_SHM_SENSORDATA_TABLE_PIECE_SIZE);
		tpret = EMBBADDATA;	
		goto EXIT;
	}

	//DBG_LOG_WARN("APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET= %d, APP_SHM_TOTAL_SIZE = %d", APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET, APP_SHM_TOTAL_SIZE);
	//DBG_LOG_WARN("APP_SHM_SENSORDATA_TABLE_ITEM_SIZE*APP_SHM_SENSORDATA_TABLE_ITEM_MAX = %d", APP_SHM_SENSORDATA_TABLE_ITEM_SIZE*APP_SHM_SENSORDATA_TABLE_ITEM_MAX);
	
	tpu32 = util_tool_cal_elfhash( (const uint8_t *)&pt_mslave_ctr->pt_shmem_buf[APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET % APP_SHM_TOTAL_SIZE], APP_SHM_SENSORDATA_TABLE_ITEM_SIZE*APP_SHM_SENSORDATA_TABLE_ITEM_MAX);
	if(tpu32 != pt_mslave_ctr->sdata_hash)
	{		
		for(uint32_t i = 0;i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
		{			
			for(uint32_t j = 0; j < APP_SHM_SENSORDATA_TABLE_PIECE_MAX; j++)
			{				
				//DBG_LOG_INFO("BKP");
				
				pt_shmem_dtitem = (appShmDataItem_t*)&pt_mslave_ctr->pt_shmem_buf[(APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + i * APP_SHM_SENSORDATA_TABLE_ITEM_SIZE + j * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE) % APP_SHM_TOTAL_SIZE];
				//if(app_shm_data_type_sensordata != pt_shmem_dtitem->appShmDataType)				
				if(GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE != pt_shmem_dtitem->appShmDataType)
				{
					//DBG_LOG_ERR("invalid SensorData type");
					#if(0)
						tpret = EMBXILVAL;
						goto EXIT;
					#else
						continue;
					#endif
				}

				if(true != is_sensor_data_valid( &pt_shmem_dtitem->sensorDataTableData))
				{
					DBG_LOG_ERR("the sensor data is invalid");
					#if(0)
						tpret = EMBXILVAL;
						goto EXIT;
					#else
						continue;
					#endif
				}
			}
		}
		/* ... */
		pt_mslave_ctr->sdata_hash = tpu32;
	}	
	DBG_LOG_INFO("BKP");
	#endif
	
	EXIT:
		return tpret;
}

#endif


#if(1)
/*to load GW configuration from shared memory
ret@error_code
*/
static enum error_code to_load_gwcfg_from_shmem(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t tpu32 = 0;
	appShmDataItem_t *pt_shmem_dtitem = NULL;

	// to read GW configuration info from shared-memory
	DBG_LOG_WARN("to wait for sh-memory being ready %d", time(NULL));

	if(APP_SHM_GATEWAY_TABLE_ITEM_SIZE > SZ_IPC_BUF)
	{
		DBG_LOG_ERR("Buffer might overflow");
		tpret = EMBBADDATA;
		goto EXIT;
	}


	sem_wait(pt_mslave_ctr->pt_sem_for_shmem_dtblock);
	memcpy( pt_mslave_ctr->ipc_dtitem_buf, ((uint8_t*)pt_mslave_ctr->pt_dtblock) + (APP_SHM_GATEWAY_TABLE_ADDR_OFFSET % APP_SHM_TOTAL_SIZE), APP_SHM_GATEWAY_TABLE_ITEM_SIZE % SZ_IPC_BUF);
	sem_post(pt_mslave_ctr->pt_sem_for_shmem_dtblock);

	DBG_LOG_WARN("BKP %d", time(NULL));

	if(sizeof(appShmDataItem_t) >= APP_SHM_GATEWAY_TABLE_ITEM_SIZE)
	{
		DBG_LOG_ERR("buffer might overflow");
		tpret = EMBBADDATA;
		goto EXIT;
	}

	//
	tpu32 = util_tool_cal_elfhash( (const uint8_t*)&pt_mslave_ctr->ipc_dtitem_buf, APP_SHM_GATEWAY_TABLE_ITEM_SIZE % SZ_IPC_BUF);
	if(tpu32 != pt_mslave_ctr->gwconf_hash)
	{
		pt_shmem_dtitem = (appShmDataItem_t*)pt_mslave_ctr->ipc_dtitem_buf;
		if(GW_PRO_TABLE_IDX_GW_CONFIG_TABLE != pt_shmem_dtitem->appShmDataType)
		{
			DBG_LOG_ERR("no valid gateway-conf info available");
			tpret = EMBXILVAL;
			goto EXIT;
		}

		if(true != is_gateway_conf_valid(&pt_shmem_dtitem->gwTableData))
		{
			DBG_LOG_ERR("illegal gateway-conf info");
			tpret = EMBXILVAL;
			goto EXIT;
		}

		// the GW configuration is updated and it is valid
		memcpy( &pt_mslave_ctr->gwconf, &pt_shmem_dtitem->gwTableData, sizeof(gatewayTableData_t));
		pt_mslave_ctr->gwconf_hash = tpu32;

		//MK2591U__to set the endianness_____
		switch(pt_shmem_dtitem->gwTableData.gwConfig.modbusConfig.registerMode)
		{
			case MODBUS_REG_ADDR_EXT_BW_BB_MODE:  //= 0, // 0, extensible mode(big word, big byte)
			{//0xABCD
				pt_mslave_ctr->mbus_border = MBUS_BORDER_ABCD;
				break;
			}
			case MODBUS_REG_ADDR_EFF_BW_BB_MODE://,     // 1, efficient mode(big word, big byte)
			{//0xABCD
				pt_mslave_ctr->mbus_border = MBUS_BORDER_ABCD;
				break;
			}
			case MODBUS_REG_ADDR_EXT_BW_LB_MODE://,     // 2, extensible mode(big word, little byte)
			{//0xBADC
				pt_mslave_ctr->mbus_border = MBUS_BORDER_BADC;
				break;
			}
			case MODBUS_REG_ADDR_EFF_BW_LB_MODE://,     // 3, efficient mode(big word, little byte)
			{//0xBADC
				pt_mslave_ctr->mbus_border = MBUS_BORDER_BADC;
				break;
			}
			case MODBUS_REG_ADDR_EXT_LW_BB_MODE://,     // 4, extensible mode(little word, big byte)
			{//0xCDAB
				pt_mslave_ctr->mbus_border = MBUS_BORDER_CDAB;
				break;
			}
			case MODBUS_REG_ADDR_EFF_LW_BB_MODE://,     // 5, efficient mode(little word, big byte)
			{//0xCDAB
				pt_mslave_ctr->mbus_border = MBUS_BORDER_CDAB;
				break;
			}
			case MODBUS_REG_ADDR_EXT_LW_LB_MODE://,     // 6, extensible mode(little word, little byte)
			{//0xDCBA
				pt_mslave_ctr->mbus_border = MBUS_BORDER_DCBA;
				break;
			}
			case MODBUS_REG_ADDR_EFF_LW_LB_MODE://,     // 7, efficient mode(little word, little byte)
			{//0xDCBA
				pt_mslave_ctr->mbus_border = MBUS_BORDER_DCBA;
				break;
			}
			default:
			{
				DBG_LOG_ERR("not supposed to happen");
				break;
			}
		}
		//MK2591D__________________________
	



		
	}
	
	EXIT:
		return tpret;
}

/*to load DEV list from shared memory
ret@error_code
*/
static enum error_code to_load_devlist_from_shmem(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t tpu32 = 0;
	appShmDataItem_t *pt_shmem_dtitem = NULL;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	if(sizeof(appShmDataItem_t) >= APP_SHM_DEV_TABLE_ITEM_SIZE)
	{
		DBG_LOG_ERR("buffer might overflow");
		tpret = EMBBADDATA;
		goto EXIT;
	}

	if(APP_SHM_DEV_TABLE_ITEM_SIZE > SZ_IPC_BUF)
	{
		DBG_LOG_ERR("buffer might overflow");
		tpret = EMBBADDATA;
		goto EXIT;
	}

	// to check the DEV hash
	//MK2623U__________________________________________________________
	sem_wait(pt_mslave_ctr->pt_sem_for_shmem_dtblock);

	do{
		tpu32 = util_tool_cal_elfhash(((const uint8_t *)pt_mslave_ctr->pt_dtblock) + APP_SHM_DEV_TABLE_ADDR_OFFSET % APP_SHM_TOTAL_SIZE, APP_SHM_DEV_TABLE_ITEM_SIZE*APP_SHM_DEV_TABLE_ITEM_MAX);
		if(tpu32 == pt_mslave_ctr->dev_hash)
		{// the DEV list is not updated
			break;
		}

		// to read the DEV item one by one
		for(uint32_t i = 0; i < APP_SHM_DEV_TABLE_ITEM_MAX; i++)
		{
			memset( pt_mslave_ctr->ipc_dtitem_buf, 0, SZ_IPC_BUF);
			memcpy( pt_mslave_ctr->ipc_dtitem_buf, ((uint8_t*)pt_mslave_ctr->pt_dtblock) + APP_SHM_DEV_TABLE_ADDR_OFFSET + i*APP_SHM_DEV_TABLE_ITEM_SIZE, APP_SHM_DEV_TABLE_ITEM_SIZE % SZ_IPC_BUF);	
			pt_shmem_dtitem = (appShmDataItem_t*)pt_mslave_ctr->ipc_dtitem_buf;
			if(GW_PRO_TABLE_IDX_MNGED_DEVICE_LIST_TABLE != pt_shmem_dtitem->appShmDataType)
			{
				DBG_LOG_ERR("invalid DEV data type");
				memset( &pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX], 0, sizeof(deviceTableData_t));
				continue;
			}

			if(true != is_dev_info_valid(&pt_shmem_dtitem->devTableData))
			{
				DBG_LOG_ERR("the DEV-info is invalid");
				memset( &pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX], 0, sizeof(deviceTableData_t));
				continue;
			}
	
			memcpy( &pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX], &pt_shmem_dtitem->devTableData, sizeof(deviceTableData_t));
		}

		pt_mslave_ctr->dev_hash = tpu32;
	}while(0);
	
	sem_post(pt_mslave_ctr->pt_sem_for_shmem_dtblock);
	//MK2623D__________________________________________________________

	

	EXIT:
		return tpret;
}


/*to load sensor data from shared memory
ret@error_code
NOTE: this API will check the whole sensor-data block in shared-memory , the hash value will be updated
*/
static enum error_code to_check_sdata_hash_value(struct modbus_slave_controller *pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t tpu32 = 0;
	//appShmDataItem_t *pt_shmem_dtitem = NULL;

	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	if(sizeof(appShmDataItem_t) >=  APP_SHM_SENSORDATA_TABLE_PIECE_SIZE)
	{
		DBG_LOG_ERR("buffer might overflow");
		tpret = EMBBADDATA;
		goto EXIT;
	}
	
	//MK2690U_______________________________
	sem_wait(pt_mslave_ctr->pt_sem_for_shmem_dtblock);

	tpu32 = util_tool_cal_elfhash(((const uint8_t *)pt_mslave_ctr->pt_dtblock) + APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET % APP_SHM_TOTAL_SIZE, APP_SHM_SENSORDATA_TABLE_ITEM_SIZE*APP_SHM_SENSORDATA_TABLE_ITEM_MAX);
	if(tpu32 != pt_mslave_ctr->sdata_hash)
	{
		pt_mslave_ctr->sdata_hash = tpu32;
	}
	sem_post(pt_mslave_ctr->pt_sem_for_shmem_dtblock);
	//MK2690D_______________________________

	EXIT:
		return tpret;
	
}



/*to load sensor data from shared memory
ret@error_code
Note: this API just read the sensor-data from shared-memory by dtidx, the hash value will not be updated
*/
static enum error_code to_load_sdata_from_shmem_v2(struct modbus_slave_controller *pt_mslave_ctr, uint32_t didx)
{
	enum error_code tpret = ERR_NONE;
	appShmDataItem_t_original *pt_shmem_dtitem = NULL;
	uint32_t kpidx = 0;
	
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}


	if(sizeof(appShmDataItem_t_original) >= SZ_IPC_BUF)
	{
		DBG_LOG_ERR("buffer might overflow");
		tpret = EMBBADDATA;
		goto EXIT;
	}

	if(sizeof(sensorDataTableData_t) > APP_SHM_SENSORDATA_TABLE_PIECE_SIZE)
	{
		DBG_LOG_ERR("Buffer might overflow");
		tpret = EMBBADDATA;
		goto EXIT;
	}
	

	#if(0) // the original
	//MK2733U_______________________
	kpidx = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + didx * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
	sem_wait(pt_mslave_ctr->pt_sem_for_shmem_dtblock);

	memset( pt_mslave_ctr->ipc_dtitem_buf, 0, APP_SHM_SENSORDATA_TABLE_PIECE_SIZE % SZ_IPC_BUF);
	memcpy( pt_mslave_ctr->ipc_dtitem_buf, ((uint8_t*)pt_mslave_ctr->pt_dtblock) + kpidx % APP_SHM_TOTAL_SIZE, APP_SHM_SENSORDATA_TABLE_PIECE_SIZE);
	
	sem_post(pt_mslave_ctr->pt_sem_for_shmem_dtblock);
	//MK2733D_______________________
	#else

	//
	memset( pt_mslave_ctr->ipc_dtitem_buf, 0, SZ_IPC_BUF);
	pt_shmem_dtitem = (appShmDataItem_t_original*)pt_mslave_ctr->ipc_dtitem_buf;
	//MK2733U_______________________
	kpidx = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + didx * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
	sem_wait(pt_mslave_ctr->pt_sem_for_shmem_dtblock);
	
	memcpy( &pt_shmem_dtitem->sensorDataTableData, ((uint8_t*)pt_mslave_ctr->pt_dtblock) + kpidx % APP_SHM_TOTAL_SIZE, sizeof(sensorDataTableData_t));
	
	sem_post(pt_mslave_ctr->pt_sem_for_shmem_dtblock);

	if(pt_shmem_dtitem->sensorDataTableData.nameId)
	{
		pt_shmem_dtitem->appShmDataType = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
	}
	//MK2733D_______________________
	
	#endif
	//
	
	EXIT:
		return tpret;
}



/*to load the sensor data bitmap from the shared memory
ret@
NOTE!!! idx is the index of sensor data item in the shared memory , not the memory size
*/
static enum error_code to_load_sdata_bitmap_from_shmem(struct modbus_slave_controller*pt_mslave_ctr, uint32_t idx)
{
	enum error_code tpret = ERR_NONE;
	uint32_t kpidx = 0;

	if(NULL == pt_mslave_ctr)
	{
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	kpidx = (idx / APP_SHM_SENSORDATA_TABLE_PIECE_MAX + 1) * APP_SHM_SENSORDATA_TABLE_PIECE_MAX - 1; // find the bitmap block
	kpidx = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + kpidx * APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;

	//MK2945U________________________

	sem_wait(pt_mslave_ctr->pt_sem_for_shmem_dtblock);

	pt_mslave_ctr->bitmapval = *( (uint64_t*) (((uint8_t*)pt_mslave_ctr->pt_dtblock) + kpidx % APP_SHM_TOTAL_SIZE) );

	sem_post(pt_mslave_ctr->pt_sem_for_shmem_dtblock);

	//MK2945D________________________

	EXIT:
		return tpret;
}


#endif

/*to get the sensor data layout info
ret@pointer points to the info when found, NULL when nothing is found
*/
#if(0)
//nameId
static struct sdata_layout to_get_sdata_layout_info(struct modbus_slave_controller *pt_mslave_ctr, const uint32_t nameid)
{
	struct sdata_layout kp_sdata_layout_info = { .is_valid = false, 0};
	deviceTableData_t *pt_devinfo = NULL;
		
	//DBG_LOG_ERR("Todo=====================to complete");

	
	for(uint32_t i = 0; i < BULLET_GW_SENSOR_NUM_MAX; i++)
	{
		// output for debug only
		//DBG_LOG_WARN("nameId %d, name %s", pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].nameId, pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].name);

		if(nameid == pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].nameId)
		{			
			//DBG_LOG_WARN("nameId %d, name %s", pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].nameId, pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].name);
			pt_devinfo = &pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX];
			break;
		}
	}

	if(NULL == pt_devinfo)
	{
		DBG_LOG_WARN("fail to find dev-info with nameId %d", nameid);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	// the channel
	if((false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_CHN_OFFSET]))||(false == util_tool_is_hex_char( pt_devinfo->name[SENSOR_NAME_CHN_OFFSET + 1])))
	{
		DBG_LOG_ERR("invalid HEX char, %s", pt_devinfo->name);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.ch = util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_CHN_OFFSET])*16 + util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_CHN_OFFSET + 1]);

	//the address
	if((false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET]))||
		(false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET + 1]))||
		(false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET + 2]))||
		(false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET + 3])))
	{
		DBG_LOG_ERR("invalid HEX char, %s, 0x%x, 0x%x ", pt_devinfo->name, pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET], pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET + 1]);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.addr = util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET])*16*16*16;
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET+1])*16*16;	
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET+2])*16;	
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET+3]);	

	//the reg number
	if((false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_REG_LEN_OFFSET]))||
		(false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_REG_LEN_LENGTH + 1])))
	{
		DBG_LOG_ERR("invalid HEX char, %s", pt_devinfo->name);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}
	kp_sdata_layout_info.regnum = util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_REG_LEN_OFFSET])*16 + util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_REG_LEN_OFFSET + 1]);

	// the sensor type
	if(0 == memcmp( &pt_devinfo->name[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_PREDICT_SENSOR, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_PREDICT;
	}
	else if(0 == memcmp( &pt_devinfo->name[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_INSIGHT_T;
	}
	else
	{
		DBG_LOG_ERR("unknown sensor type");
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.is_valid = true;
	
	EXIT:
		return kp_sdata_layout_info;
}
#else
//description
static struct sdata_layout to_get_sdata_layout_info(struct modbus_slave_controller *pt_mslave_ctr, const uint32_t nameid)
{
	struct sdata_layout kp_sdata_layout_info = { .is_valid = false, 0};
	deviceTableData_t *pt_devinfo = NULL;

	uint16_t kpu16 = 0;
	
	//DBG_LOG_ERR("Todo=====================to complete");

	
	for(uint32_t i = 0; i < BULLET_GW_SENSOR_NUM_MAX; i++)
	{
		// output for debug only
		//DBG_LOG_WARN("nameId %d, name %s", pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].nameId, pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].name);

		if(nameid == pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].nameId)
		{			
			//DBG_LOG_WARN("nameId %d, name %s", pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].nameId, pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX].name);
			pt_devinfo = &pt_mslave_ctr->dev_array[i % BULLET_GW_SENSOR_NUM_MAX];
			break;
		}
	}

	if(NULL == pt_devinfo)
	{
		DBG_LOG_WARN("fail to find dev-info with nameId %d", nameid);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	// the channel
	if((false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_CHN_OFFSET]))||(false == util_tool_is_hex_char( pt_devinfo->description[SENSOR_NAME_CHN_OFFSET + 1])))
	{
		DBG_LOG_ERR("invalid HEX char, %s", pt_devinfo->description);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.ch = util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_CHN_OFFSET])*16 + util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_CHN_OFFSET + 1]);

	//the address
	if((false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET]))||
		(false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET + 1]))||
		(false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET + 2]))||
		(false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET + 3])))
	{
		DBG_LOG_ERR("invalid HEX char, %s, 0x%x, 0x%x ", pt_devinfo->description, pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET], pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET + 1]);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.addr = util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET])*16*16*16;
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET+1])*16*16;	
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET+2])*16;	
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET+3]);	

	//the reg number
	if((false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_REG_LEN_OFFSET]))||
		(false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_REG_LEN_LENGTH + 1])))
	{
		DBG_LOG_ERR("invalid HEX char, %s", pt_devinfo->description);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}
	kp_sdata_layout_info.regnum = util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_REG_LEN_OFFSET])*16 + util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_REG_LEN_OFFSET + 1]);

	// the sensor type
	if(0 == memcmp( &pt_devinfo->description[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_PREDICT_SENSOR, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_PREDICT;
	}
	else if(0 == memcmp( &pt_devinfo->description[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_INSIGHT_T;
	}
	else
	{
		DBG_LOG_ERR("unknown sensor type");
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	// to add the offset info
	kpu16 = app_mslave_get_reg_mapping_offset();
	if(kp_sdata_layout_info.addr + kpu16 <= 0xFFFF)
	{
		kp_sdata_layout_info.addr = kp_sdata_layout_info.addr + kpu16; 
	}
	
	kp_sdata_layout_info.is_valid = true;
	
	EXIT:
		return kp_sdata_layout_info;
}

#endif






/*to get the sensor data layout info
ret@pointer points to the info when found, NULL when nothing is found
*/
#if(0)
struct sdata_layout app_mslave_ipc_get_sdata_layout_info(const deviceTableData_t *pt_devinfo)
{
	struct sdata_layout kp_sdata_layout_info = { .is_valid = false, 0};
	
		
	DBG_LOG_ERR("Todo=====================to complete");


	if(NULL == pt_devinfo)
	{
		DBG_LOG_WARN("unexpected NULL");
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	// the channel
	if((false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_CHN_OFFSET]))||(false == util_tool_is_hex_char( pt_devinfo->name[SENSOR_NAME_CHN_OFFSET + 1])))
	{
		DBG_LOG_ERR("invalid HEX char %s|", pt_devinfo->name);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.ch = util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_CHN_OFFSET])*16 + util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_CHN_OFFSET + 1]);

	//the address
	if((false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET]))||
		(false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET + 1]))||
		(false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET + 2]))||
		(false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET + 3] )))
	{
		DBG_LOG_ERR("invalid HEX char, %s|", pt_devinfo->name);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.addr = util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET])*16*16*16;
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET+1])*16*16;	
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET+2])*16;	
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_START_ADDR_OFFSET+3]);	

	//the reg number
	if((false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_REG_LEN_OFFSET]))||
		(false == util_tool_is_hex_char(pt_devinfo->name[SENSOR_NAME_REG_LEN_LENGTH + 1])))
	{
		DBG_LOG_ERR("invalid HEX char, %s|", pt_devinfo->name);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}
	kp_sdata_layout_info.regnum = util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_REG_LEN_OFFSET])*16 + util_tool_hex_char_to_uint8(pt_devinfo->name[SENSOR_NAME_REG_LEN_OFFSET + 1]);

	// the sensor type
	if(0 == memcmp( &pt_devinfo->name[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_PREDICT_SENSOR, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_PREDICT;
	}
	else if(0 == memcmp( &pt_devinfo->name[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_INSIGHT_T;
	}
	else
	{
		DBG_LOG_ERR("unknown sensor type , %s, %s", pt_devinfo->name, &pt_devinfo->name[SENSOR_NAME_TYPE_OFFSET]);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.nameid = pt_devinfo->nameId; 
	
	kp_sdata_layout_info.is_valid = true;
	
	EXIT:
		return kp_sdata_layout_info;
}

#else
// description
struct sdata_layout app_mslave_ipc_get_sdata_layout_info(const deviceTableData_t *pt_devinfo)
{
	struct sdata_layout kp_sdata_layout_info = { .is_valid = false, 0};

	uint16_t kpu16 = 0;
		
	DBG_LOG_ERR("Todo=====================to complete");


	if(NULL == pt_devinfo)
	{
		DBG_LOG_WARN("unexpected NULL");
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	// the channel
	if((false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_CHN_OFFSET]))||(false == util_tool_is_hex_char( pt_devinfo->description[SENSOR_NAME_CHN_OFFSET + 1])))
	{
		DBG_LOG_ERR("invalid HEX char %s|", pt_devinfo->description);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.ch = util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_CHN_OFFSET])*16 + util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_CHN_OFFSET + 1]);

	//the address
	if((false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET]))||
		(false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET + 1]))||
		(false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET + 2]))||
		(false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET + 3] )))
	{
		DBG_LOG_ERR("invalid HEX char, %s|", pt_devinfo->description);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.addr = util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET])*16*16*16;
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET+1])*16*16;	
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET+2])*16;	
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_START_ADDR_OFFSET+3]);	

	//the reg number
	if((false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_REG_LEN_OFFSET]))||
		(false == util_tool_is_hex_char(pt_devinfo->description[SENSOR_NAME_REG_LEN_LENGTH + 1])))
	{
		DBG_LOG_ERR("invalid HEX char, %s|", pt_devinfo->description);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}
	kp_sdata_layout_info.regnum = util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_REG_LEN_OFFSET])*16 + util_tool_hex_char_to_uint8(pt_devinfo->description[SENSOR_NAME_REG_LEN_OFFSET + 1]);

	// the sensor type
	if(0 == memcmp( &pt_devinfo->description[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_PREDICT_SENSOR, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_PREDICT;
	}
	else if(0 == memcmp( &pt_devinfo->description[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_INSIGHT_T;
	}
	else
	{
		DBG_LOG_ERR("unknown sensor type , %s, %s", pt_devinfo->description, &pt_devinfo->description[SENSOR_NAME_TYPE_OFFSET]);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.nameid = pt_devinfo->nameId; 

	//
	kpu16 = app_mslave_get_reg_mapping_offset();
	if(kp_sdata_layout_info.addr + kpu16 <= 0xFFFF)
	{
		kp_sdata_layout_info.addr = kp_sdata_layout_info.addr + kpu16; 
	}
	
	kp_sdata_layout_info.is_valid = true;
	
	EXIT:
		return kp_sdata_layout_info;
}

#endif



#if(1)
/*app_mslave_get_ctr()
*/
struct sdata_layout app_mslave_ipc_get_gwconf_layout_info(const gatewayTableData_t *pt_gwconf)
{
	struct sdata_layout kp_sdata_layout_info = { .is_valid = false, 0};

	uint16_t kpu16 = 0;
		
	DBG_LOG_ERR("Todo=====================to complete");


	if(NULL == pt_gwconf)
	{
		DBG_LOG_WARN("unexpected NULL");
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	// the channel
	if((false == util_tool_is_hex_char(pt_gwconf->gwConfig.description[SENSOR_NAME_CHN_OFFSET]))||(false == util_tool_is_hex_char( pt_gwconf->gwConfig.description[SENSOR_NAME_CHN_OFFSET + 1])))
	{
		DBG_LOG_ERR("invalid HEX char %s|", pt_gwconf->gwConfig.description);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.ch = util_tool_hex_char_to_uint8(pt_gwconf->gwConfig.description[SENSOR_NAME_CHN_OFFSET])*16 + util_tool_hex_char_to_uint8(pt_gwconf->gwConfig.description[SENSOR_NAME_CHN_OFFSET + 1]);

	//the address
	if((false == util_tool_is_hex_char(pt_gwconf->gwConfig.description[SENSOR_NAME_START_ADDR_OFFSET]))||
		(false == util_tool_is_hex_char(pt_gwconf->gwConfig.description[SENSOR_NAME_START_ADDR_OFFSET + 1]))||
		(false == util_tool_is_hex_char(pt_gwconf->gwConfig.description[SENSOR_NAME_START_ADDR_OFFSET + 2]))||
		(false == util_tool_is_hex_char(pt_gwconf->gwConfig.description[SENSOR_NAME_START_ADDR_OFFSET + 3] )))
	{
		DBG_LOG_ERR("invalid HEX char, %s|", pt_gwconf->gwConfig.description);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.addr = util_tool_hex_char_to_uint8(pt_gwconf->gwConfig.description[SENSOR_NAME_START_ADDR_OFFSET])*16*16*16;
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_gwconf->gwConfig.description[SENSOR_NAME_START_ADDR_OFFSET+1])*16*16;	
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_gwconf->gwConfig.description[SENSOR_NAME_START_ADDR_OFFSET+2])*16;	
	kp_sdata_layout_info.addr = kp_sdata_layout_info.addr +  util_tool_hex_char_to_uint8(pt_gwconf->gwConfig.description[SENSOR_NAME_START_ADDR_OFFSET+3]);	

	//the reg number
	if((false == util_tool_is_hex_char(pt_gwconf->gwConfig.description[SENSOR_NAME_REG_LEN_OFFSET]))||
		(false == util_tool_is_hex_char(pt_gwconf->gwConfig.description[SENSOR_NAME_REG_LEN_LENGTH + 1])))
	{
		DBG_LOG_ERR("invalid HEX char, %s|", pt_gwconf->gwConfig.description);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}
	kp_sdata_layout_info.regnum = util_tool_hex_char_to_uint8(pt_gwconf->gwConfig.description[SENSOR_NAME_REG_LEN_OFFSET])*16 + util_tool_hex_char_to_uint8(pt_gwconf->gwConfig.description[SENSOR_NAME_REG_LEN_OFFSET + 1]);

	// the sensor type
	if(0 == memcmp( &pt_gwconf->gwConfig.description[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_PREDICT_SENSOR, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_PREDICT;
	}
	else if(0 == memcmp( &pt_gwconf->gwConfig.description[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_INSIGHT_T_SENSOR, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_INSIGHT_T;
	}
	else if(0 == memcmp( &pt_gwconf->gwConfig.description[SENSOR_NAME_TYPE_OFFSET], MODBUS_PRODUCT_TYPE_GATEWAY, SENSOR_NAME_TYPE_LENGTH))
	{
		kp_sdata_layout_info.stype = STYPE_GW;
	}
	else
	{
		DBG_LOG_ERR("unknown sensor type , %s, %s", pt_gwconf->gwConfig.description, &pt_gwconf->gwConfig.description[SENSOR_NAME_TYPE_OFFSET]);
		kp_sdata_layout_info.is_valid = false;
		goto EXIT;
	}

	kp_sdata_layout_info.nameid = pt_gwconf->gwConfig.nameId; 


	//to add an offset when register remapping is set
	kpu16 = app_mslave_get_reg_mapping_offset();
	if(kp_sdata_layout_info.addr + kpu16 <= 0xFFFF)
	{
		kp_sdata_layout_info.addr = kp_sdata_layout_info.addr + kpu16;
	}
	
	kp_sdata_layout_info.is_valid = true;
	
	EXIT:
		return kp_sdata_layout_info;
}

#endif


/*to update the remapping block
ret@error_code
*/
static enum error_code to_update_remapping_block(modbus_mapping_t*pt_des_blk, modbus_mapping_t *pt_src_blk, const struct register_mapping_info*pt_remap_info)
{
	enum error_code tpret = ERR_NONE;
	uint16_t kp_src_addr = 0, kp_des_addr = 0, kp_regval = 0;

	if((NULL == pt_des_blk)||(NULL == pt_src_blk)||(NULL == pt_remap_info))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	for(uint16_t i = 0; i < pt_remap_info->mapping_num; i++)
	{
		kp_src_addr = pt_remap_info->src[i % MAX_REG_MAPPING];
		kp_des_addr = pt_remap_info->des[i % MAX_REG_MAPPING];
		if((kp_src_addr < pt_src_blk->start_registers)||(kp_src_addr >= (pt_src_blk->start_registers + pt_src_blk->nb_registers)))
		{ // the source register is not in the current source-block
			continue;
		}
		if((kp_des_addr < pt_des_blk->start_registers)||(kp_des_addr >= (pt_des_blk->start_registers + pt_des_blk->nb_registers)))
		{
			DBG_LOG_ERR("You are not supposed to see this!");
			continue;
		}

		if((NULL == pt_src_blk->tab_registers)||(NULL == pt_des_blk->tab_registers))
		{
			DBG_LOG_ERR("This is not supposed to happen");
			continue;
		}

		if((pt_src_blk->nb_registers <= 0)||(pt_des_blk->nb_registers <= 0))
		{
			DBG_LOG_ERR("This is not supposed to happen");
			continue;
		}

		kp_regval = pt_src_blk->tab_registers[(kp_src_addr - pt_src_blk->start_registers) % pt_src_blk->nb_registers];
		pt_des_blk->tab_registers[(kp_des_addr - pt_des_blk->start_registers) % pt_des_blk->nb_registers] = kp_regval;		
	}

	EXIT:
		return tpret;
}


#if(0)

/*to update the sensor-data register table
ret@error_code
*/
static enum error_code to_update_sdata_register(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	modbus_mapping_t *pt_mapping = NULL;
	appShmDataItem_t *pt_shmdt = NULL;
	struct sdata_layout kp_sdata_layout = {0};
	uint32_t kp_offset = 0;
	uint32_t kpidx = 0;

	struct register_mapping_info *pt_remap_info = app_mslave_get_reg_mapping_info();
	modbus_mapping_t *pt_remap_blk = NULL;

	// only for debug
	app_mslave_dump_sdata_info((const uint8_t *)pt_mslave_ctr->pt_shmem_buf);

	#if(1) //
	if(true == pt_remap_info->flg)
	{// to find the remapping block
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
		pt_remap_blk = app_mslave_locate_mapping( pt_mslave_ctr, pt_remap_info->des[0], ADDR_GP_HOLDING_REGISTER);
		pthread_mutex_unlock(&pt_mslave_ctr->mtx_for_mapping_queue);		
	}
	#endif


	for(uint32_t i = 0; i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
	{
		for(uint32_t j = 0; j < APP_SHM_SENSORDATA_TABLE_PIECE_MAX; j++)
		{
			kpidx = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i*APP_SHM_SENSORDATA_TABLE_PIECE_MAX + j)*APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			pt_shmdt = (appShmDataItem_t*)&pt_mslave_ctr->pt_shmem_buf[kpidx % APP_SHM_TOTAL_SIZE];
			//if(app_shm_data_type_sensordata == pt_shmdt->appShmDataType)			
			if(GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE == pt_shmdt->appShmDataType)
			{
				//DBG_LOG_ERR("to write sensor data into register table");

				kp_sdata_layout = to_get_sdata_layout_info( pt_mslave_ctr, pt_shmdt->sensorDataTableData.nameId);
				if(true != kp_sdata_layout.is_valid)
				{
					DBG_LOG_WARN("fail to get valid data layout info");
					break;
				}

				//MK953U_____________________________________
				pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);

				pt_mapping = app_mslave_locate_mapping( pt_mslave_ctr, kp_sdata_layout.addr, ADDR_GP_HOLDING_REGISTER);
				if(NULL == pt_mapping)
				{
					DBG_LOG_ERR("fail to find reg mapping @0x%x", kp_sdata_layout.addr);

					
					pthread_mutex_unlock(&pt_mslave_ctr->mtx_for_mapping_queue);
					break;
				}

				// to make sure the mapping block has enough space for the sensor-data writting
				if((kp_sdata_layout.addr + kp_sdata_layout.regnum) > (pt_mapping->start_registers + pt_mapping->nb_registers))
				{
					DBG_LOG_ERR(" no enough memory for sensor data writting nameid %d , 0x%x - 0x%x, 0x%x - 0x%x",\
						kp_sdata_layout.nameid, kp_sdata_layout.addr, kp_sdata_layout.regnum, pt_mapping->start_registers, pt_mapping->nb_registers);
					
					pthread_mutex_unlock(&pt_mslave_ctr->mtx_for_mapping_queue);
					break;
				}
				// to write sensor data info register table
				kp_offset = kp_sdata_layout.addr - pt_mapping->start_registers;

				switch(pt_shmdt->sensorDataTableData.product)
				{
					case SKFChina_Common_ProductType_BULLET_NODE:
					{
						to_write_sensor_data_to_register_table( pt_mapping, pt_shmdt, kp_offset);
						break;
					}
					case SKFChina_Common_ProductType_INSIGHT_T:
					{
						to_write_ist_data_to_register_table( pt_mapping, pt_shmdt, kp_offset);
						break;
					}
					case SKFChina_Common_ProductType_BULLET_GATEWAY:
					{
						to_write_gw_data_to_register_table( pt_mapping, pt_shmdt, kp_offset);
						break;
					}
					default:
					{
						DBG_LOG_ERR("unknown product type %d", pt_shmdt->sensorDataTableData.product);
						break;
					}
				}

				// do the remapping
				if(NULL != pt_remap_blk)
				{ 
					to_update_remapping_block( pt_remap_blk, pt_mapping, (const struct register_mapping_info *) pt_remap_info);
				}
				
				pthread_mutex_unlock(&pt_mslave_ctr->mtx_for_mapping_queue);
				//MK953D_______________________________________

			}
			else
			{
				//DBG_LOG_WARN("no valid sensor-data");
			}
		}
	}
	EXIT:
		return tpret;	
}

#else

/*to update the sensor-data register table
ret@error_code
*/
static enum error_code to_update_sdata_register(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	modbus_mapping_t *pt_mapping = NULL;
	appShmDataItem_t_original *pt_shmdt = NULL;
	struct sdata_layout kp_sdata_layout = {0};
	uint32_t kp_offset = 0;
	uint32_t kpidx = 0;

	struct register_mapping_info *pt_remap_info = app_mslave_get_reg_mapping_info();
	modbus_mapping_t *pt_remap_blk = NULL;

	// only for debug
	//app_mslave_dump_sdata_info((const uint8_t *)pt_mslave_ctr->pt_shmem_buf);

	//
	if(sizeof(appShmDataItem_t_original) > sizeof(appShmDataItem_t))
	{ // this is a long story
		DBG_LOG_ERR("Risk to overflow");
		tpret = ERR_API_FAIL;
		goto EXIT;
	}

	#if(1) //
	if(true == pt_remap_info->flg)
	{// to find the remapping block
		pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);
		pt_remap_blk = app_mslave_locate_mapping( pt_mslave_ctr, pt_remap_info->des[0], ADDR_GP_HOLDING_REGISTER);
		pthread_mutex_unlock(&pt_mslave_ctr->mtx_for_mapping_queue);		
	}
	#endif


	for(uint32_t i = 0; i < APP_SHM_SENSORDATA_TABLE_ITEM_MAX; i++)
	{
		for(uint32_t j = 0; j < APP_SHM_SENSORDATA_TABLE_PIECE_MAX; j++)
		{
				#if(0)
			kpidx = APP_SHM_SENSORDATA_TABLE_ADDR_OFFSET + (i*APP_SHM_SENSORDATA_TABLE_PIECE_MAX + j)*APP_SHM_SENSORDATA_TABLE_PIECE_SIZE;
			pt_shmdt = (appShmDataItem_t*)&pt_mslave_ctr->pt_shmem_buf[kpidx % APP_SHM_TOTAL_SIZE];	
				#else
			memset( pt_mslave_ctr->ipc_dtitem_buf, 0, SZ_IPC_BUF);
				
			to_load_sdata_from_shmem_v2( pt_mslave_ctr, (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX) + j);
			to_load_sdata_bitmap_from_shmem( pt_mslave_ctr, (i * APP_SHM_SENSORDATA_TABLE_PIECE_MAX) + j);
				
			pt_shmdt = (appShmDataItem_t_original *)pt_mslave_ctr->ipc_dtitem_buf;
				#endif


			
			if(GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE == pt_shmdt->appShmDataType)
			{
				//DBG_LOG_ERR("to write sensor data into register table");

				kp_sdata_layout = to_get_sdata_layout_info( pt_mslave_ctr, pt_shmdt->sensorDataTableData.nameId);
				if(true != kp_sdata_layout.is_valid)
				{
					DBG_LOG_WARN("fail to get valid data layout info");
					break;
				}

				//MK953U_____________________________________
				pthread_mutex_lock( &pt_mslave_ctr->mtx_for_mapping_queue);

				pt_mapping = app_mslave_locate_mapping( pt_mslave_ctr, kp_sdata_layout.addr, ADDR_GP_HOLDING_REGISTER);
				if(NULL == pt_mapping)
				{
					DBG_LOG_ERR("fail to find reg mapping @0x%x", kp_sdata_layout.addr);

					
					pthread_mutex_unlock(&pt_mslave_ctr->mtx_for_mapping_queue);
					break;
				}

				// to make sure the mapping block has enough space for the sensor-data writting
				if((kp_sdata_layout.addr + kp_sdata_layout.regnum) > (pt_mapping->start_registers + pt_mapping->nb_registers))
				{
					DBG_LOG_ERR(" no enough memory for sensor data writting nameid %d , 0x%x - 0x%x, 0x%x - 0x%x",\
						kp_sdata_layout.nameid, kp_sdata_layout.addr, kp_sdata_layout.regnum, pt_mapping->start_registers, pt_mapping->nb_registers);
					
					pthread_mutex_unlock(&pt_mslave_ctr->mtx_for_mapping_queue);
					break;
				}
				// to write sensor data info register table
				kp_offset = kp_sdata_layout.addr - pt_mapping->start_registers;

				switch(pt_shmdt->sensorDataTableData.product)
				{
					case SKFChina_Common_ProductType_BULLET_NODE:
					{
						to_write_sensor_data_to_register_table( pt_mapping, pt_shmdt, kp_offset);
						
						to_write_sensor_bitmap_to_register_table( pt_mapping, pt_mslave_ctr->bitmapval, kp_offset);

						break;
					}
					case SKFChina_Common_ProductType_INSIGHT_T:
					{
						to_write_ist_data_to_register_table( pt_mapping, pt_shmdt, kp_offset);

						to_write_ist_bitmap_to_register_table( pt_mapping, pt_mslave_ctr->bitmapval, kp_offset);
						break;
					}
					case SKFChina_Common_ProductType_BULLET_GATEWAY:
					{
						to_write_gw_data_to_register_table( pt_mapping, pt_shmdt, kp_offset);
						break;
					}
					default:
					{
						DBG_LOG_ERR("unknown product type %d", pt_shmdt->sensorDataTableData.product);
						break;
					}
				}

				// do the remapping
				if(NULL != pt_remap_blk)
				{ 
					to_update_remapping_block( pt_remap_blk, pt_mapping, (const struct register_mapping_info *) pt_remap_info);
				}
				
				pthread_mutex_unlock(&pt_mslave_ctr->mtx_for_mapping_queue);
				//MK953D_______________________________________

			}
			else
			{
				//DBG_LOG_WARN("no valid sensor-data");
			}
		}
	}
	EXIT:
		return tpret;	
}



#endif


/* to check if the port conf is modified; when it is , continue to reinitialized the PORT
ret@bool
*/
static bool is_port_conf_modified( struct port_conf_info *pt_port_conf, const gatewayTableData_t *pt_gwconf)
{
	bool tpret = false;
	struct port_conf_info tp_port_cfg = {0};


	if((NULL == pt_port_conf)||(NULL == pt_gwconf))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		goto EXIT;
	}

	#if(0)
	if(pt_port_conf->baudrate != pt_gwconf->gwConfig.modbusConfig.baudrateSlave)
	{
		tpret = true;
	}
	else if(pt_port_conf->parity != pt_gwconf->gwConfig.modbusConfig.paritySlave)
	{
		tpret = true;
	}
	else if(pt_port_conf->stopbit != pt_gwconf->gwConfig.modbusConfig.stopSlave)
	{
		tpret = true;
	}
	else if(pt_port_conf->slaveID != pt_gwconf->gwConfig.modbusConfig.slaveAddrSlave)
	{
		tpret = true;
	}
	else
	{
		tpret = false;
	}
	#else
		memset( &tp_port_cfg, 0, sizeof(struct port_conf_info));
		if(ERR_NONE != app_mslave_ipc_set_port_conf( &tp_port_cfg, &pt_gwconf->gwConfig.modbusConfig))
		{
			tpret = false;
			goto EXIT;
		}
		//
		if(pt_port_conf->baudrate != tp_port_cfg.baudrate)
		{
			tpret = true;
		}
		else if(pt_port_conf->parity != tp_port_cfg.parity)
		{
			tpret = true;
		}
		else if(pt_port_conf->stopbit != tp_port_cfg.stopbit)
		{
			tpret = true;
		}
		else if(pt_port_conf->slaveID != tp_port_cfg.slaveID)
		{
			tpret = true;
		}
		else
		{
			tpret = false;
		}
	#endif
	EXIT:
		return tpret;

}


#if(0)

/*to update the modbus register mapping
ret@error code
*/
static enum error_code to_update_mslave_mapping( struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t kp_gwinfo_hash = 0, kp_dev_hash = 0, kp_sdata_hash = 0;
	modbus_t *pt_modbus_port = NULL;
	bool is_reg_mode_modified = false;
	int32_t tpi32 = 0;
	
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	kp_gwinfo_hash = pt_mslave_ctr->gwconf_hash;
	kp_dev_hash = pt_mslave_ctr->dev_hash;
	kp_sdata_hash = pt_mslave_ctr->sdata_hash;

	DBG_LOG_WARN("gwinfo_hash = 0x%X, dev_hash = 0x%X, sdata_hash = 0x%X", pt_mslave_ctr->gwconf_hash, pt_mslave_ctr->dev_hash, pt_mslave_ctr->sdata_hash);
	
	tpret = to_load_data_from_shmem( pt_mslave_ctr);
	if(ERR_NONE != tpret)
	{
		DBG_LOG_ERR("fail to load data from shared-memory");

		DBG_LOG_ERR("Todo=============go to exit directly");
		//goto EXIT; // commented for test
	}	
	DBG_LOG_WARN("the reg-mode info %d - %d", pt_mslave_ctr->port_conf.reg_md, pt_mslave_ctr->gwconf.gwConfig.modbusConfig.registerMode);
	
	if((kp_gwinfo_hash != pt_mslave_ctr->gwconf_hash)||(true == app_mslave_ipc_get_force_gwconf_update( pt_mslave_ctr)))
	{
		DBG_LOG_ERR("TODO==6===the port configuration is modfied, to reinitialized the PORT");
		//MK1775U____________________________
		tpi32 = pthread_mutex_trylock( &pt_mslave_ctr->mtx_for_port_conf);
		if(0 != tpi32)
		{// keep running, when we fail to acquire the mutex, instead of blocking and waiting.
			//DBG_LOG_ERR("fail to acquire the mutex, is_waiting_for_recv %d", pt_mslave_ctr->is_waiting_for_recv);
			app_mslave_set_to_update_mslave(  pt_mslave_ctr, true);
			DBG_LOG_ERR("fail to acquire the mutex, to_update_mslave %d", app_mslave_get_to_update_mslave( pt_mslave_ctr));
			
			app_mslave_ipc_set_force_gwconf_update( pt_mslave_ctr, true); // set flag, to try latter
			tpret = ERR_RES_UNAVAILABLE;
			goto EXIT;
		}

		app_mslave_set_to_update_mslave( pt_mslave_ctr, false);		
		app_mslave_ipc_set_force_gwconf_update( pt_mslave_ctr, false); // to reset the flag
		do{
			//if(true == pt_mslave_ctr->is_waiting_for_recv)
			//{
			//	DBG_LOG_WARN("the Slave is waiting for receivation, Modbus-ctx is locked. Try latter");
			//	break;
			//}
			
			
			if(true != is_gateway_conf_valid( &pt_mslave_ctr->gwconf))
			{
				DBG_LOG_ERR(" the gateway configuration is invalid");
				break;
			}

			if(pt_mslave_ctr->port_conf.reg_md != pt_mslave_ctr->gwconf.gwConfig.modbusConfig.registerMode)
			{
				is_reg_mode_modified = true;
			}
			
			// to check if the PORT configuration is modified
			if(true != is_port_conf_modified( &pt_mslave_ctr->port_conf, &pt_mslave_ctr->gwconf))
			{
				DBG_LOG_WARN("the PORT conf is not modified");
				break;
			}
			// to REINITIALIZE the PORT with new configuration
			modbus_close( app_mslave_get_mctx( pt_mslave_ctr));
			modbus_free( app_mslave_get_mctx( pt_mslave_ctr));
			app_mslave_set_mctx( pt_mslave_ctr, NULL);

			pt_modbus_port = modbus_new_rtu( DEV_NM_FOR_MBUS_SLAVE, pt_mslave_ctr->gwconf.gwConfig.modbusConfig.baudrateSlave,\
				pt_mslave_ctr->gwconf.gwConfig.modbusConfig.paritySlave,\
				8, pt_mslave_ctr->gwconf.gwConfig.modbusConfig.stopSlave);
			if(NULL == pt_modbus_port)
			{
				DBG_LOG_ERR("fail to create modbus context");
				break;
			}
	
			modbus_set_slave( pt_modbus_port, pt_mslave_ctr->port_conf.slaveID);
			modbus_set_debug( pt_modbus_port, TRUE);
			modbus_set_response_timeout( pt_modbus_port, 3, 0);

			if(-1 == modbus_connect( pt_modbus_port))
			{
				modbus_close( pt_modbus_port);
				modbus_free( pt_modbus_port);
				DBG_LOG_ERR("fail to open PORT");
				break;
			}
			
			app_mslave_set_mctx( pt_mslave_ctr, pt_modbus_port);
			// to update the PORT configuration info
			#if(0)
			pt_mslave_ctr->slave_id = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.slaveAddrSlave;
			pt_mslave_ctr->port_conf.slaveID = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.slaveAddrSlave;
			pt_mslave_ctr->port_conf.baudrate = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.baudrateSlave;
			pt_mslave_ctr->port_conf.parity = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.paritySlave;
			pt_mslave_ctr->port_conf.stopbit = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.stopSlave;
			pt_mslave_ctr->port_conf.reg_md = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.registerMode;
			#else
				app_mslave_ipc_set_port_conf( &pt_mslave_ctr->port_conf, &pt_mslave_ctr->gwconf.gwConfig.modbusConfig);
				pt_mslave_ctr->slave_id = pt_mslave_ctr->port_conf.slaveID;
			#endif
			
			DBG_LOG_WARN("Double check ! 1. communication broken;2.be seen at high frequency");
		}while(0);
		pthread_mutex_unlock(&pt_mslave_ctr->mtx_for_port_conf);
		//MK1775D____________________________
		app_mslave_dump_port_conf( &pt_mslave_ctr->port_conf);
	}

	#if(1)
	// for debug only
		//app_mslave_ipc_set_force_dev_update( pt_mslave_ctr, true);
	#endif
	DBG_LOG_ERR("is_reg_md_modified = %d, dev_hash = 0x%x bs 0x%x, force_flag = %d", is_reg_mode_modified, kp_dev_hash, pt_mslave_ctr->dev_hash, app_mslave_ipc_get_force_dev_update( pt_mslave_ctr));

	if((kp_dev_hash != pt_mslave_ctr->dev_hash)||(true == is_reg_mode_modified)||(true == app_mslave_ipc_get_force_dev_update( pt_mslave_ctr)))
	{	/*1. the dev-list is updated, then we just release the old mapping block queue, create a new one.
		  2. no need when register mode is REG_MD_EFFICIENT
		*/

		 app_mslave_ipc_set_force_dev_update( pt_mslave_ctr, false);
		  
		DBG_LOG_WARN("NOTE!!! when you see this output a lot. you probably need to do some modification below");
		/*it's not a good idea when the blocking queue is created and released at high frequency*/
		//app_mslave_release_mapping( pt_mslave_ctr);
		if(ERR_NONE != app_mslave_create_mapping( pt_mslave_ctr))
		{
			app_mslave_ipc_set_force_dev_update( pt_mslave_ctr, true);
			DBG_LOG_ERR("app_mslave_create_mapping fail, set flag to try latter");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
		//to make sure the sensor-data register is updated
		//app_mslave_ipc_trig_mapping_block_update( pt_mslave_ctr);
		app_mslave_ipc_set_force_sdata_update( pt_mslave_ctr, true);
	}

	#if(1) 
		// for test only
		app_mslave_ipc_set_force_sdata_update( pt_mslave_ctr, true);
		//app_mslave_dump_port_conf( &pt_mslave_ctr->port_conf);
	#endif
	DBG_LOG_ERR("info===============kp_sdata_hash=0x%X vs 0x%X", kp_sdata_hash, pt_mslave_ctr->sdata_hash);
	if((kp_sdata_hash != pt_mslave_ctr->sdata_hash)||(true == app_mslave_ipc_get_force_sdata_update( pt_mslave_ctr)))
	{
		DBG_LOG_ERR("TODO========================================to update the register mapping");

		app_mslave_ipc_set_force_sdata_update( pt_mslave_ctr, false); // to reset the flag
		
		tpret = to_update_sdata_register( pt_mslave_ctr);
		if(ERR_NONE != tpret)
		{
			DBG_LOG_ERR(" fail to update sensor-data");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
	}

	EXIT:
		return tpret;
}


#else // to use smaller IPC buffer


/*to update the modbus register mapping
ret@error code
*/
static enum error_code to_update_mslave_mapping( struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t kp_gwinfo_hash = 0, kp_dev_hash = 0, kp_sdata_hash = 0;
	modbus_t *pt_modbus_port = NULL;
	bool is_reg_mode_modified = false;
	int32_t tpi32 = 0;
	
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}

	kp_gwinfo_hash = pt_mslave_ctr->gwconf_hash;
	kp_dev_hash = pt_mslave_ctr->dev_hash;
	kp_sdata_hash = pt_mslave_ctr->sdata_hash;

	DBG_LOG_WARN("gwinfo_hash = 0x%X, dev_hash = 0x%X, sdata_hash = 0x%X", pt_mslave_ctr->gwconf_hash, pt_mslave_ctr->dev_hash, pt_mslave_ctr->sdata_hash);

		#if(0)
	tpret = to_load_data_from_shmem( pt_mslave_ctr);
	if(ERR_NONE != tpret)
	{
		DBG_LOG_ERR("fail to load data from shared-memory");

		DBG_LOG_ERR("Todo=============go to exit directly");
		//goto EXIT; // commented for test
	}	
		#else
			tpret = to_load_gwcfg_from_shmem( pt_mslave_ctr);
			if(ERR_NONE != tpret)
			{
				DBG_LOG_ERR("fail to load GW configuration from shared-memory");
				goto EXIT;
			}
			tpret = to_load_devlist_from_shmem( pt_mslave_ctr);
			if(ERR_NONE != tpret)
			{
				DBG_LOG_ERR("fail to load DEV list from shared-memory");
				goto EXIT;
			}
			to_check_sdata_hash_value( pt_mslave_ctr);
		#endif


	
	DBG_LOG_WARN("the reg-mode info %d - %d", pt_mslave_ctr->port_conf.reg_md, pt_mslave_ctr->gwconf.gwConfig.modbusConfig.registerMode);
	
	if((kp_gwinfo_hash != pt_mslave_ctr->gwconf_hash)||(true == app_mslave_ipc_get_force_gwconf_update( pt_mslave_ctr)))
	{
		DBG_LOG_ERR("TODO==6===the port configuration is modfied, to reinitialized the PORT");
		//MK1775U____________________________
		tpi32 = pthread_mutex_trylock( &pt_mslave_ctr->mtx_for_port_conf);
		if(0 != tpi32)
		{// keep running, when we fail to acquire the mutex, instead of blocking and waiting.
			//DBG_LOG_ERR("fail to acquire the mutex, is_waiting_for_recv %d", pt_mslave_ctr->is_waiting_for_recv);
			app_mslave_set_to_update_mslave(  pt_mslave_ctr, true);
			DBG_LOG_ERR("fail to acquire the mutex, to_update_mslave %d", app_mslave_get_to_update_mslave( pt_mslave_ctr));
			
			app_mslave_ipc_set_force_gwconf_update( pt_mslave_ctr, true); // set flag, to try latter
			tpret = ERR_RES_UNAVAILABLE;
			goto EXIT;
		}

		app_mslave_set_to_update_mslave( pt_mslave_ctr, false);		
		app_mslave_ipc_set_force_gwconf_update( pt_mslave_ctr, false); // to reset the flag
		do{
			//if(true == pt_mslave_ctr->is_waiting_for_recv)
			//{
			//	DBG_LOG_WARN("the Slave is waiting for receivation, Modbus-ctx is locked. Try latter");
			//	break;
			//}
			
			
			if(true != is_gateway_conf_valid( &pt_mslave_ctr->gwconf))
			{
				DBG_LOG_ERR(" the gateway configuration is invalid");
				break;
			}

			if(pt_mslave_ctr->port_conf.reg_md != app_mslave_get_reg_mode(pt_mslave_ctr->gwconf.gwConfig.modbusConfig.registerMode))
			{
				is_reg_mode_modified = true;
			}
			
			// to check if the PORT configuration is modified
			if(true != is_port_conf_modified( &pt_mslave_ctr->port_conf, &pt_mslave_ctr->gwconf))
			{
				DBG_LOG_WARN("the PORT conf is not modified");
				break;
			}
			// to REINITIALIZE the PORT with new configuration
			modbus_close( app_mslave_get_mctx( pt_mslave_ctr));
			modbus_free( app_mslave_get_mctx( pt_mslave_ctr));
			app_mslave_set_mctx( pt_mslave_ctr, NULL);

			pt_modbus_port = modbus_new_rtu( DEV_NM_FOR_MBUS_SLAVE, pt_mslave_ctr->gwconf.gwConfig.modbusConfig.baudrateSlave,\
				pt_mslave_ctr->gwconf.gwConfig.modbusConfig.paritySlave,\
				8, pt_mslave_ctr->gwconf.gwConfig.modbusConfig.stopSlave);
			if(NULL == pt_modbus_port)
			{
				DBG_LOG_ERR("fail to create modbus context");
				break;
			}
	
			modbus_set_slave( pt_modbus_port, pt_mslave_ctr->port_conf.slaveID);
			modbus_set_debug( pt_modbus_port, TRUE);
			modbus_set_response_timeout( pt_modbus_port, 3, 0);

			if(-1 == modbus_connect( pt_modbus_port))
			{
				modbus_close( pt_modbus_port);
				modbus_free( pt_modbus_port);
				DBG_LOG_ERR("fail to open PORT");
				break;
			}
			
			app_mslave_set_mctx( pt_mslave_ctr, pt_modbus_port);
			// to update the PORT configuration info
			#if(0)
			pt_mslave_ctr->slave_id = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.slaveAddrSlave;
			pt_mslave_ctr->port_conf.slaveID = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.slaveAddrSlave;
			pt_mslave_ctr->port_conf.baudrate = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.baudrateSlave;
			pt_mslave_ctr->port_conf.parity = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.paritySlave;
			pt_mslave_ctr->port_conf.stopbit = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.stopSlave;
			pt_mslave_ctr->port_conf.reg_md = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.registerMode;
			#else
				app_mslave_ipc_set_port_conf( &pt_mslave_ctr->port_conf, &pt_mslave_ctr->gwconf.gwConfig.modbusConfig);
				pt_mslave_ctr->slave_id = pt_mslave_ctr->port_conf.slaveID;
			#endif
			
			DBG_LOG_WARN("Double check ! 1. communication broken;2.be seen at high frequency");
		}while(0);
		pthread_mutex_unlock(&pt_mslave_ctr->mtx_for_port_conf);
		//MK1775D____________________________
		app_mslave_dump_port_conf( &pt_mslave_ctr->port_conf);
	}

	#if(1)
	// for debug only
		//app_mslave_ipc_set_force_dev_update( pt_mslave_ctr, true);
	#endif
	DBG_LOG_ERR("is_reg_md_modified = %d, dev_hash = 0x%x bs 0x%x, force_flag = %d", is_reg_mode_modified, kp_dev_hash, pt_mslave_ctr->dev_hash, app_mslave_ipc_get_force_dev_update( pt_mslave_ctr));

	if((kp_dev_hash != pt_mslave_ctr->dev_hash)||(true == is_reg_mode_modified)||(true == app_mslave_ipc_get_force_dev_update( pt_mslave_ctr)))
	{	/*1. the dev-list is updated, then we just release the old mapping block queue, create a new one.
		  2. no need when register mode is REG_MD_EFFICIENT
		*/

		 app_mslave_ipc_set_force_dev_update( pt_mslave_ctr, false);
		  
		DBG_LOG_WARN("NOTE!!! when you see this output a lot. you probably need to do some modification below");
		/*it's not a good idea when the blocking queue is created and released at high frequency*/
		//app_mslave_release_mapping( pt_mslave_ctr);
		if(ERR_NONE != app_mslave_create_mapping( pt_mslave_ctr))
		{
			app_mslave_ipc_set_force_dev_update( pt_mslave_ctr, true);
			DBG_LOG_ERR("app_mslave_create_mapping fail, set flag to try latter");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
		//to make sure the sensor-data register is updated
		//app_mslave_ipc_trig_mapping_block_update( pt_mslave_ctr);
		app_mslave_ipc_set_force_sdata_update( pt_mslave_ctr, true);
	}

	#if(1) 
		// for test only
		app_mslave_ipc_set_force_sdata_update( pt_mslave_ctr, true);
		//app_mslave_dump_port_conf( &pt_mslave_ctr->port_conf);
	#endif
	DBG_LOG_ERR("info===============kp_sdata_hash=0x%X vs 0x%X", kp_sdata_hash, pt_mslave_ctr->sdata_hash);
	if((kp_sdata_hash != pt_mslave_ctr->sdata_hash)||(true == app_mslave_ipc_get_force_sdata_update( pt_mslave_ctr)))
	{
		DBG_LOG_ERR("TODO========================================to update the register mapping");

		app_mslave_ipc_set_force_sdata_update( pt_mslave_ctr, false); // to reset the flag
		
		tpret = to_update_sdata_register( pt_mslave_ctr);
		if(ERR_NONE != tpret)
		{
			DBG_LOG_ERR(" fail to update sensor-data");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
	}

	EXIT:
		return tpret;
}


#endif


/*thread handler for IPC service
void *(* start_routine)(void *)

*/
void *pthandler_mslave_ipc_service(void *pt_param)
{
	float kp_fval = 0;
	uint8_t *pt_u8 = NULL;
	
	struct modbus_slave_controller *hld_mslave_ctr = app_mslave_get_ctr();


	app_mslave_ipc_set_force_sdata_update( hld_mslave_ctr, true);	
	while(1)
	{
		DBG_LOG_WARN("to update mslave-mapping");
		if(ERR_NONE == to_update_mslave_mapping( hld_mslave_ctr))
		{
			DBG_LOG_WARN("to_update_mslave_maping goes well, to_update_mslave=%d", app_mslave_get_to_update_mslave( hld_mslave_ctr));

			fflush(stdout); // flush the IO buffer before we sleep
			sleep(MBUS_UPDATE_PERIOD); 
			
			#if(1) // for test only
				#if(0) // for test only
				pthread_mutex_lock(&hld_mslave_ctr->mtx_for_mapping_queue);
				to_update_mbus_mapping_for_dbg();
				pthread_mutex_unlock( &hld_mslave_ctr->mtx_for_mapping_queue);
				#endif
				
				app_mslave_dump_mbus_mapping_info( hld_mslave_ctr);
				app_mslave_dump_port_conf( &hld_mslave_ctr->port_conf);
				app_mslave_dump_dev_info( hld_mslave_ctr);
				app_mslave_dump_reg_mapping_info(app_mslave_get_reg_mapping_info());

				app_mslave_dump_sdata_info( hld_mslave_ctr);
			#endif
		}
		else
		{
			if((app_mslave_ipc_get_force_gwconf_update( hld_mslave_ctr))||(app_mslave_ipc_get_force_dev_update( hld_mslave_ctr)))
			{ /* gwconf or dev-mapping need to be updated immediately ! the possibility is that to_update_mlsave_mapping
			fails to acquire the mutex when it return the wrong code*/
				usleep(1000);
			}
			else
			{
				sleep(MBUS_UPDATE_PERIOD);
			}
		}
	}
}




/*to create a shared-memory
ret@error_code
*/
enum error_code app_mslave_ipc_create_shmem(struct modbus_slave_controller*pt_mslave_ctr, const uint32_t memsize)
{
	enum error_code tpret = ERR_NONE;
	int tpi32 = -1;
	key_t kp_shm_key = 0;

	if((NULL == pt_mslave_ctr)||(memsize <= 0))
	{
		DBG_LOG_ERR("invalid parameters");
		tpret = ERR_INVALID_PARAM;
		goto EXIT;
	}
	#if(1) // the original
	
	//to create a file
	if(0 != access( FPATH_FOR_SHM, F_OK))
	{// to create the file when it does not exist
		tpi32 = creat( FPATH_FOR_SHM, S_IRUSR | S_IWUSR);
		if(tpi32 < 0)
		{
			DBG_LOG_ERR("fail to create file %s, for %s", FPATH_FOR_SHM, strerror(errno));
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
		close(tpi32);
	}

	//to get the shm-key
	kp_shm_key = ftok( FPATH_FOR_SHM, PROJECT_ID_FOR_SHM);
	if(kp_shm_key < 0)
	{
		DBG_LOG_ERR("ftok fail, for %s", strerror(errno));
		tpret = ERR_API_FAIL;
		goto EXIT;
	}
	
	DBG_LOG_WARN("the shared-memory key is 0x%x", kp_shm_key);
	
	//
	pt_mslave_ctr->shmid = shmget( kp_shm_key, memsize, IPC_CREAT | PERMISSION_FLAG_FOR_SHM);
	if(pt_mslave_ctr->shmid < 0)
	{
		DBG_LOG_ERR("shmget fail, for %s", strerror(errno));
		tpret = ERR_API_FAIL;
		goto EXIT;
	}
	pt_mslave_ctr->pt_dtblock = shmat(pt_mslave_ctr->shmid, NULL, 0);
	if(pt_mslave_ctr->pt_dtblock < 0)
	{
		DBG_LOG_ERR("shmat fail, for %s", strerror(errno));
		tpret = ERR_API_FAIL;
		goto EXIT;
	}

	DBG_LOG_WARN("the shared-memory ID %d", pt_mslave_ctr->shmid);
	
	// 
	#if(0)
	if(shmctl( pt_mslave_ctr->shmid, IPC_RMID, 0) < 0)
	{
		DBG_LOG_ERR("shmat fail, for %s", strerror(errno));
		tpret = ERR_API_FAIL;
		goto EXIT;	
	} 
	#endif

	#else	
	#endif
	
	EXIT:
		return tpret;
}


/*to destroy the shared-memory
ret@error_code
*/
enum error_code app_mslave_ipc_destroy_shmem(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	if(pt_mslave_ctr->pt_dtblock >= 0)
	{
		shmdt(pt_mslave_ctr->pt_dtblock);
		pt_mslave_ctr->pt_dtblock = -1;
	}

	if(pt_mslave_ctr->shmid >= 0)
	{
		shmctl( pt_mslave_ctr->shmid, IPC_RMID, NULL);
		pt_mslave_ctr->shmid = -1;
	}


	return tpret;
}


/*to create a named semaphore for shared-memory access sychronization
ret@error_code
*/
enum error_code app_mslave_ipc_create_sem(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint8_t kp_strbuf[0x100] = {0};
	int32_t tpi32 = 0;
	
	snprintf( kp_strbuf, sizeof(kp_strbuf), "/dev/shm%s", SHMEM_NAMED_SEMAPHORE_FILENAME);
	DBG_LOG_WARN("file path for sem %s", kp_strbuf);

	//sem_unlink( SHMEM_NAMED_SEMAPHORE_FILENAME);	
	sem_unlink( kp_strbuf);
	//
	#if(1)
	if(0 != access( kp_strbuf, F_OK))
	{ // to create the file when it does not exist
		tpi32 = creat(kp_strbuf, S_IRWXU|S_IRWXG|S_IRWXO);	
		if(tpi32 < 0)
		{
			tpret = ERR_API_FAIL;
			return tpret;
		}
		close(tpi32);
	}
	#endif
	
	//pt_mslave_ctr->pt_sem_for_shmem_dtblock = sem_open( IPC_SEM_NAME, O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO, 1);	
	//pt_mslave_ctr->pt_sem_for_shmem_dtblock = sem_open( SHMEM_NAMED_SEMAPHORE_FILENAME, O_CREAT, PERMISSION_FLAG_FOR_SHM, 1);
	pt_mslave_ctr->pt_sem_for_shmem_dtblock = sem_open(  SHMEM_NAMED_SEMAPHORE_FILENAME, O_CREAT, PERMISSION_FLAG_FOR_SHM, 0);		
	if(SEM_FAILED == pt_mslave_ctr->pt_sem_for_shmem_dtblock)
	{
		tpret = ERR_API_FAIL;
		DBG_LOG_ERR("sem_open fail, for %s", strerror(errno));
	}
	return tpret;
}


/*to release the named semaphore
ret@error_code
*/
enum error_code app_mslave_ipc_release_sem(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;

	if(pt_mslave_ctr->pt_sem_for_shmem_dtblock)
	{
		sem_close(pt_mslave_ctr->pt_sem_for_shmem_dtblock);
		//sem_unlink(IPC_SEM_NAME);
	}
	
	return tpret;
}

/*to trig the update of mapping block
ret@error_code
NOTE!!! NOT THREAD safe
*/
enum error_code app_mslave_ipc_trig_mapping_block_update(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	
	pt_mslave_ctr->sdata_hash = pt_mslave_ctr->sdata_hash + 1;
	DBG_LOG_WARN("sdata_hash is updated to 0x%x", pt_mslave_ctr->sdata_hash);
	
	return tpret;
}

/*
NOTE!!! not thread-safe
*/
static void app_mslave_ipc_set_force_gwconf_update(struct modbus_slave_controller*pt_mslave_ctr, bool newstate)
{
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		return;
	}

	if(newstate)
	{
		pt_mslave_ctr->to_force_gwconf_update = true;
	}
	else
	{
		pt_mslave_ctr->to_force_gwconf_update = false;
	}
	return;
}


/*
NOTE!!! not thread-safe
*/
static bool app_mslave_ipc_get_force_gwconf_update( struct modbus_slave_controller*pt_mslave_ctr)
{
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	return pt_mslave_ctr->to_force_gwconf_update;
}

/*
NOTE!!! not thread-safe
*/
static void app_mslave_ipc_set_force_dev_update(struct modbus_slave_controller*pt_mslave_ctr, bool newstate)
{
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		return ;
	}
	if(newstate)
	{
		pt_mslave_ctr->to_force_dev_update = true;
	}
	else
	{
		pt_mslave_ctr->to_force_dev_update = false;
	}
	return;
}

/*
NOTE!!! not thread-safe
*/
static bool app_mslave_ipc_get_force_dev_update(struct modbus_slave_controller*pt_mslave_ctr)
{
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	return pt_mslave_ctr->to_force_dev_update;
}

/*NOTE!!! not thread-safe
*/
static void app_mslave_ipc_set_force_sdata_update(struct modbus_slave_controller*pt_mslave_ctr, bool newstate)
{
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		return;
	}
	if(newstate)
	{
		pt_mslave_ctr->to_force_sdata_update = true;
	}
	else
	{
		pt_mslave_ctr->to_force_sdata_update = false;
	}
	return;
}

/*
*/
static bool app_mslave_ipc_get_force_sdata_update(struct modbus_slave_controller*pt_mslave_ctr)
{
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		return false;
	}
	return pt_mslave_ctr->to_force_sdata_update;
}


#if(0)

/* to load configuration from shared memory
ret@error_code
NOTE!!!
1. this API will wait for valid configuration be available
2. this API is not thread safe.
*/
enum error_code app_mslave_ipc_load_conf(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t kp_gwconf_hash = 0, kp_dev_hash = 0;	
	//DBG_LOG_ERR("Todo============================to complete");

	while(1)
	{		
		DBG_LOG_WARN("To wait for gwconf and dev-list be ready");
		sleep(1);
		kp_gwconf_hash = pt_mslave_ctr->gwconf_hash;
		kp_dev_hash = pt_mslave_ctr->dev_hash;
		tpret = to_load_data_from_shmem( pt_mslave_ctr);
		if(ERR_NONE != tpret)
		{
			DBG_LOG_ERR("fail to load gwconf and dev-list");
			continue;
		}

		//if(pt_mslave_ctr->gwconf.lastEditTimeS)
		if(true == is_gateway_conf_valid( &pt_mslave_ctr->gwconf))
		{
			//DBG_LOG_ERR("To-fix=====modify this if-branch");

			//MK2140U____________________________
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			#if(0)
			pt_mslave_ctr->slave_id = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.slaveAddrSlave;
			pt_mslave_ctr->port_conf.slaveID = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.slaveAddrSlave;
			pt_mslave_ctr->port_conf.baudrate = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.baudrateSlave;
			pt_mslave_ctr->port_conf.parity = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.paritySlave;
			pt_mslave_ctr->port_conf.stopbit = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.stopSlave;
			pt_mslave_ctr->port_conf.reg_md = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.registerMode;
			#else
				tpret = app_mslave_ipc_set_port_conf( &pt_mslave_ctr->port_conf, &pt_mslave_ctr->gwconf.gwConfig.modbusConfig);
				pt_mslave_ctr->slave_id = pt_mslave_ctr->port_conf.slaveID;
			#endif
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2140D____________________________

			
			//app_mslave_ipc_trig_mapping_block_update( pt_mslave_ctr);			
			//app_mslave_ipc_set_force_sdata_update( pt_mslave_ctr, true);
			if(ERR_NONE == tpret)
			{
				break;
			}
			else
			{
				DBG_LOG_ERR("This is not suppose to happen");
			}
		}
	}
	
	return tpret;
}

#else // to use smaller IPC buffer

/* to load configuration from shared memory
ret@error_code
NOTE!!!
1. this API will wait for valid configuration be available
2. this API is not thread safe.
*/

enum error_code app_mslave_ipc_load_conf(struct modbus_slave_controller*pt_mslave_ctr)
{
	enum error_code tpret = ERR_NONE;
	uint32_t kp_gwconf_hash = 0, kp_dev_hash = 0;	
	//DBG_LOG_ERR("Todo============================to complete");

	while(1)
	{		
		DBG_LOG_WARN("To wait for gwconf and dev-list be ready");
		sleep(1);
		kp_gwconf_hash = pt_mslave_ctr->gwconf_hash;
		kp_dev_hash = pt_mslave_ctr->dev_hash;
		
		//tpret = to_load_data_from_shmem( pt_mslave_ctr);
		tpret = to_load_gwcfg_from_shmem( pt_mslave_ctr);
		if(ERR_NONE != tpret)
		{
			DBG_LOG_ERR("fail to load GW configuration");
			continue;
		}

		tpret = to_load_devlist_from_shmem( pt_mslave_ctr);
		if(ERR_NONE != tpret)
		{
			DBG_LOG_ERR("fail to load gwconf and dev-list");
			continue;
		}

		//if(pt_mslave_ctr->gwconf.lastEditTimeS)
		if(true == is_gateway_conf_valid( &pt_mslave_ctr->gwconf))
		{
			//DBG_LOG_ERR("To-fix=====modify this if-branch");

			//MK2140U____________________________
			pthread_mutex_lock( &pt_mslave_ctr->mtx_for_port_conf);
			#if(0)
			pt_mslave_ctr->slave_id = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.slaveAddrSlave;
			pt_mslave_ctr->port_conf.slaveID = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.slaveAddrSlave;
			pt_mslave_ctr->port_conf.baudrate = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.baudrateSlave;
			pt_mslave_ctr->port_conf.parity = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.paritySlave;
			pt_mslave_ctr->port_conf.stopbit = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.stopSlave;
			pt_mslave_ctr->port_conf.reg_md = pt_mslave_ctr->gwconf.gwConfig.modbusConfig.registerMode;
			#else
				tpret = app_mslave_ipc_set_port_conf( &pt_mslave_ctr->port_conf, &pt_mslave_ctr->gwconf.gwConfig.modbusConfig);
				pt_mslave_ctr->slave_id = pt_mslave_ctr->port_conf.slaveID;
			#endif
			pthread_mutex_unlock( &pt_mslave_ctr->mtx_for_port_conf);
			//MK2140D____________________________

			
			//app_mslave_ipc_trig_mapping_block_update( pt_mslave_ctr);			
			//app_mslave_ipc_set_force_sdata_update( pt_mslave_ctr, true);
			if(ERR_NONE == tpret)
			{
				break;
			}
			else
			{
				DBG_LOG_ERR("This is not suppose to happen");
			}
		}
	}
	
	return tpret;
}


#endif


/*to check and wait for Modbus mapping/configuration update
NOTE!!! this function will sleep , and wait for the update operation to be done
*/
enum error_code app_mslave_ipc_wait_for_update(const struct modbus_slave_controller * pt_mslave_ctr)
{
	enum error_code tpret = ST_OK;
	uint32_t tpcnt = 0;
	
	if(NULL == pt_mslave_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	for(tpcnt = 0; tpcnt < 32; tpcnt ++)
	{
		if((true != app_mslave_ipc_get_force_gwconf_update( pt_mslave_ctr))&&(true != app_mslave_ipc_get_force_dev_update( pt_mslave_ctr)))
		{
			break;
		}
		
		sched_yield();
		usleep(1000);//1mS
	}
}



/*to set the port configuration
ret@error_code
*/
enum error_code app_mslave_ipc_set_port_conf(struct port_conf_info *pt_port_conf, const gwConfig_gwConfig_modbusConfig_t*pt_mbus_cfg)
{
	enum error_code tpret = ERR_NONE;
	if((NULL == pt_port_conf)||(NULL == pt_mbus_cfg))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ERR_INVALID_PARAM;
		//return tpret;
		goto EXIT;
	}

	//slave ID
	pt_port_conf->slaveID = pt_mbus_cfg->slaveAddrSlave;
	//baudrate
	pt_port_conf->baudrate = pt_mbus_cfg->baudrateSlave;
	//register-mode
	pt_port_conf->reg_md = app_mslave_get_reg_mode(pt_mbus_cfg->registerMode);
	
	//parity
	switch(pt_mbus_cfg->paritySlave)
	{
		case MODBUS_PARITY_MODE_NONE://MODBUS_PARITY_MODE_NONE = 0,
		{
			pt_port_conf->parity = PARITY_NONE;
			break;
		}
		case MODBUS_PARITY_MODE_ODD: //MODBUS_PARITY_MODE_ODD,
		{
			pt_port_conf->parity = PARITY_ODD;
			break;
		}
		case MODBUS_PARITY_MODE_EVEN://MODBUS_PARITY_MODE_EVEN,
		{
			pt_port_conf->parity = PARITY_EVEN;
			break;
		}
		default:
		{
			DBG_LOG_ERR("invalid port conf-info");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
	}
	//stop-bit
	switch(pt_mbus_cfg->stopSlave)
	{
	 /*NOTE!!! when it comes to UART stop bit, set 1 in Database for 1 stop bit; set 2 in database for 2 stop
	 bits. we do not care about  the definition stopBitMode_t in sh_mem.sh anymore
	 */	
		case MODBUS_STOP_1_BIT://MODBUS_STOP_1_BIT = 0,
		{
			pt_port_conf->stopbit = SBIT_1;
			break;
		}
		case MODBUS_STOP_15_BIT://MODBUS_STOP_15_BIT,
		{
			pt_port_conf->stopbit = SBIT_1_5;
			break;
		}
		case MODBUS_STOP_2_BIT://MODBUS_STOP_2_BIT,
		{
			pt_port_conf->stopbit = SBIT_2;
			break;
		}
		
		default:
		{
			DBG_LOG_ERR("invalid port conf-info");
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
	}
	

	EXIT:
		return tpret;
}


