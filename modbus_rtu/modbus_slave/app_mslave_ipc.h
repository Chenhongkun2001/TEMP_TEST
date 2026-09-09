/*
*/
#ifndef __APP_MSLAVE_IPC_H
#define __APP_MSLAVE_IPC_H

#include <stdint.h>
#include "app_mslave.h"


#ifndef MBUS_UPDATE_PERIOD
	#define MBUS_UPDATE_PERIOD	(5U)
#endif


#ifndef IPC_PATH_SHMEM
	#define IPC_PATH_SHMEM "/tmp/ipc_path_shmem"
#endif

#ifndef IPC_ID_SHMEM
	#define IPC_ID_SHMEM	0x123456
#endif

#ifndef IPC_SEM_NAME	
	#define IPC_SEM_NAME	"/sem_for_shmem"
#endif

#ifndef MAX_SENSOR_MAPPING_BLOCK
	#define MAX_SENSOR_MAPPING_BLOCK	(512U)
#endif

#define MODBUS_SET_INT32_TO_UINT8( tab_uint8, tab_sz, index, value)	\
	do {													\
		((uint8_t*)(tab_uint8))[(index) % (tab_sz)] = (uint8_t)((value) >> 24);\
		((uint8_t*)(tab_uint8))[((index) + 1) % (tab_sz)] = (uint8_t)((value) >> 16);\
		((uint8_t*)(tab_uint8))[((index) + 2) % (tab_sz)] = (uint8_t)((value) >> 8);\
		((uint8_t*)(tab_uint8))[((index) + 3) % (tab_sz)] = (uint8_t)((value));\
		}while(0)
	

#define MODBUS_SET_UINT32_TO_UINT8( tab_uint8, tab_sz, index, value)	\
	do {													\
		((uint8_t*)(tab_uint8))[(index) % (tab_sz)] = (uint8_t)((value) >> 24);\
		((uint8_t*)(tab_uint8))[((index) + 1) % (tab_sz)] = (uint8_t)((value) >> 16);\
		((uint8_t*)(tab_uint8))[((index) + 2) % (tab_sz)] = (uint8_t)((value) >> 8);\
		((uint8_t*)(tab_uint8))[((index) + 3) % (tab_sz)] = (uint8_t)((value));\
		}while(0)



#define MODBUS_SET_FLOAT_TO_UINT8( tab_uint8, tab_sz, index, value)	\
	do {													\
		((uint8_t*)(tab_uint8))[(index) % (tab_sz)] = (uint8_t)(((uint32_t)value) >> 24);\
		((uint8_t*)(tab_uint8))[((index) + 1) % (tab_sz)] = (uint8_t)(((uint32_t)value) >> 16);\
		((uint8_t*)(tab_uint8))[((index) + 2) % (tab_sz)] = (uint8_t)(((uint32_t)value) >> 8);\
		((uint8_t*)(tab_uint8))[((index) + 3) % (tab_sz)] = (uint8_t)(((uint32_t)value));\
		}while(0)


#define MODBUS_SET_UINT64_TO_UINT8( tab_uint8, tab_sz, index, value)	\
	do {													\
		((uint8_t*)(tab_uint8))[(index) % (tab_sz)] = (uint8_t)((value) >> 56);\
		((uint8_t*)(tab_uint8))[((index) + 1) % (tab_sz)] = (uint8_t)((value) >> 48);\
		((uint8_t*)(tab_uint8))[((index) + 2) % (tab_sz)] = (uint8_t)((value) >> 40);\
		((uint8_t*)(tab_uint8))[((index) + 3) % (tab_sz)] = (uint8_t)((value) >> 32);\
		((uint8_t*)(tab_uint8))[((index) + 4) % (tab_sz)] = (uint8_t)((value) >> 24);\
		((uint8_t*)(tab_uint8))[((index) + 5) % (tab_sz)] = (uint8_t)((value) >> 16);\
		((uint8_t*)(tab_uint8))[((index) + 6) % (tab_sz)] = (uint8_t)((value) >> 8);\
		((uint8_t*)(tab_uint8))[((index) + 7) % (tab_sz)] = (uint8_t)((value));\
		}while(0)


#ifndef BYTE_REVERSE_U32
	#define BYTE_REVERSE_U32(val) (((((uint32_t)val)&0xFF)<<24) + (((((uint32_t)val) >> 8)&0xFF)<<16) + (((((uint32_t)val) >> 16)&0xFF)<<8) + ((((uint32_t)val) >> 24)&0xFF))
#endif

#ifndef BYTE_REVERSE_U16
	#define BYTE_REVERSE_U16(val)	(((((uint16_t)val)&0xFF)<< 8) + (((((uint16_t)val) >> 8)&0xFF)))
#endif

#ifndef BYTE_REVERSE_IN_HWORD_U32
	#define BYTE_REVERSE_IN_HWORD_U32(val) (((((uint32_t)val)&0xFF)<< 8) + (((((uint32_t)val) >> 8)&0xFF)) + (((((uint32_t)val) >> 16)&0xFF)<<24) + (((((uint32_t)val) >> 24)&0xFF) << 16))
#endif


#ifndef SZ_MBUS_REG
	#define SZ_MBUS_REG	(sizeof(uint16_t))
#endif

#if(0)
//MK67U_____for Predict sensor___________________________
//NOTE!!! for string, the string terminal character is included

//name_id
#define DT_NAME_ID_OFFSET	(0x0) // the offeset of snesor Data name_id
#define DT_NAME_ID_REGNUM	(0x2) // the number of registers occupied by name_id
//sensor_name
#define DT_SENSOR_NAME_OFFSET	(DT_NAME_ID_OFFSET + DT_NAME_ID_REGNUM)
#define DT_SENSOR_NAME_REGNUM	(GW_PRO_MAX_STRING_LEN_BYTE / 2 + GW_PRO_MAX_STRING_LEN_BYTE % 2)
//sensor_mac
#define DT_SENSOR_MAC_OFFSET	(DT_SENSOR_NAME_OFFSET + DT_SENSOR_NAME_REGNUM)
#define DT_SENSOR_MAC_REGNUM	(0x09) //
//sensor_type
#define DT_SENSOR_TYPE_OFFSET	(DT_SENSOR_MAC_OFFSET + DT_SENSOR_MAC_REGNUM)
#define DT_SENSOR_TYPE_REGNUM	(GW_PRO_MAX_STRING_LEN_BYTE / 2 + GW_PRO_MAX_STRING_LEN_BYTE % 2)
//manufacturer
#define DT_MANUFACTURER_OFFSET	(DT_SENSOR_TYPE_OFFSET + DT_SENSOR_TYPE_REGNUM)
#define DT_MANUFACTURER_REGNUM	(GW_PRO_MAX_STRING_LEN_BYTE / 2 + GW_PRO_MAX_STRING_LEN_BYTE % 2)

/*...........*/
//VIBRATION_ACC_WAVE
#define DT_ACC_WAVE_OFFSET	(DT_MANUFACTURER_OFFSET + DT_MANUFACTURER_REGNUM)
#define DT_ACC_WAVE_REGNUM	(0x01) // NOTE!! assign a number to the file, for future reading operation
/*...*/
// VIBRATION_ENV3_WAVE
#define DT_ENV3_WAVE_OFFSET		(DT_ACC_WAVE_OFFSET + DT_ACC_WAVE_REGNUM)
#define DT_ENV3_WAVE_REGNUM	(0x01)
/*...*/
//VIBTATION_VELOCITY_WAVE
#define DT_VELOCITY_WAVE_OFFSET	(DT_ENV3_WAVE_OFFSET + DT_ENV3_WAVE_REGNUM)
#define DT_VELOCITY_WAVE_REGNUM	(0x01)
/*...*/
//ADVANCED_ALGO_MACHINE_RUN_CODE
#define DT_ALGO_MACHINE_RUN_CODE_OFFSET	(DT_VELOCITY_WAVE_OFFSET + DT_VELOCITY_WAVE_REGNUM)
#define DT_ALGO_MACHINE_RUN_CODE_REGNUM	(0x02)
/*...*/

//ADVNACED_ALGO_MACHINE_CONDITION_CODE
#define DT_ALGO_MACHINE_CODITION_CODE_OFFSET (DT_ALGO_MACHINE_RUN_CODE_OFFSET + DT_ALGO_MACHINE_RUN_CODE_REGNUM)
#define DT_ALGO_MACHINE_CODITION_CODE_REGNUM (0x02)


//ADVNACED_ALGO_ALARM_CODE
#define DT_ALGO_ALARM_CODE_OFFSET (DT_ALGO_MACHINE_CODITION_CODE_OFFSET + DT_ALGO_MACHINE_CODITION_CODE_REGNUM)
#define DT_ALGO_ALARM_CODE_REGNUM (0x01)

// ADVANCED_ALGO_RMS_MAG_PRE
#define DT_ALGO_RMS_MAG_PRE_OFFSET	(DT_ALGO_ALARM_CODE_OFFSET + DT_ALGO_ALARM_CODE_REGNUM)
#define DT_ALGO_RMS_MAG_PRE_REGNUM	(0x02)

//ADVANCED_ALGO_RMS_VIB_PRE
#define DT_ALGO_RMS_VIB_PRE_OFFSET	(DT_ALGO_RMS_MAG_PRE_OFFSET + DT_ALGO_RMS_MAG_PRE_REGNUM)
#define DT_ALGO_RMS_VIB_PRE_REGNUM	(0x02)

//ENVIRONMENTAL_TEMPERATURE_CURRENT
#define DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET	(DT_ALGO_RMS_VIB_PRE_OFFSET + DT_ALGO_RMS_VIB_PRE_REGNUM)
#define DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM	(0x02)

// ENVIRONMENTAL_TEMPERATURE_MAX
#define DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM)
#define DT_ENVIRONMENTAL_TEMPERATURE_MAX_REGNUM	(0x02)

//environmental_temperature_min
#define DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_MAX_REGNUM)
#define DT_ENVIRONMENTAL_TEMPERATURE_MIN_REGNUM	(0x02)

//ADVANCED_ALGO_EST_RPM_START
#define DT_ADVANCED_ALGO_EST_RPM_START_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_MIN_REGNUM)
#define DT_ADVANCED_ALGO_EST_RPM_START_REGNUM	(0x02)

//advanced_algo_est_rpm_end
#define DT_ADVANCED_ALGO_EST_RPM_END_OFFSET (DT_ADVANCED_ALGO_EST_RPM_START_OFFSET + DT_ADVANCED_ALGO_EST_RPM_START_REGNUM)
#define DT_ADVANCED_ALGO_EST_RPM_END_REGNUM	(0x02)


//advanced_algo_vel_ov
#define DT_ADVANCED_ALGO_VEL_OV_OFFSET	(DT_ADVANCED_ALGO_EST_RPM_END_OFFSET + DT_ADVANCED_ALGO_EST_RPM_END_REGNUM)
#define DT_ADVANCED_ALGO_VEL_OV_REGNUM	(0x02)

//advanced_algo_vel_rsh
#define DT_ADVANCED_ALGO_VEL_RSH_OFFSET (DT_ADVANCED_ALGO_VEL_OV_OFFSET + DT_ADVANCED_ALGO_VEL_OV_REGNUM)
#define DT_ADVANCED_ALGO_VEL_RSH_REGNUM (0x02)

//advanced_algo_vel_mtr
#define DT_ADVANCED_ALGO_VEL_MTR_OFFSET	(DT_ADVANCED_ALGO_VEL_RSH_OFFSET + DT_ADVANCED_ALGO_VEL_RSH_REGNUM)
#define DT_ADVANCED_ALGO_VEL_MTR_REGNUM	(0x02)

// advanced_algo_vel_fan
#define DT_ADVANCED_ALGO_VEL_FAN_OFFSET	(DT_ADVANCED_ALGO_VEL_MTR_OFFSET + DT_ADVANCED_ALGO_VEL_MTR_REGNUM)
#define DT_ADVANCED_ALGO_VEL_FAN_REGNUM	(0x02)

// advanced_algo_vel_pump
#define DT_ADVANCED_ALGO_VEL_PUMP_OFFSET	(DT_ADVANCED_ALGO_VEL_FAN_OFFSET + DT_ADVANCED_ALGO_VEL_FAN_REGNUM)
#define DT_ADVANCED_ALGO_VEL_PUMP_REGNUM	(0x02)

// advanced_algo_env_ov
#define DT_ADVANCED_ALGO_ENV_OV_OFFSET	(DT_ADVANCED_ALGO_VEL_PUMP_OFFSET + DT_ADVANCED_ALGO_VEL_PUMP_REGNUM)
#define DT_ADVANCED_ALGO_ENV_OV_REGNUM	(0x02)

// advanced_algo_env_bpfi
#define DT_ADVANCED_ALGO_ENV_BPFI_OFFSET	(DT_ADVANCED_ALGO_ENV_OV_OFFSET + DT_ADVANCED_ALGO_ENV_OV_REGNUM)
#define DT_ADVANCED_ALGO_ENV_BPFI_REGNUM	(0x02)

//advanced_algo_env_bpfo
#define DT_ADVANCED_ALGO_ENV_BPFO_OFFSET	(DT_ADVANCED_ALGO_ENV_BPFI_OFFSET + DT_ADVANCED_ALGO_ENV_BPFI_REGNUM)
#define DT_ADVANCED_ALGO_ENV_BPFO_REGNUM	(0x02)

// advanced_algo_env_bsf
#define DT_ADVANCED_ALGO_ENV_BSF_OFFSET	(DT_ADVANCED_ALGO_ENV_BPFO_OFFSET + DT_ADVANCED_ALGO_ENV_BPFO_REGNUM)
#define DT_ADVANCED_ALGO_ENV_BSF_REGNUM	(0x02)

// advanced_algo_env_ftf
#define DT_ADVANCED_ALGO_ENV_FTF_OFFSET	(DT_ADVANCED_ALGO_ENV_BSF_OFFSET + DT_ADVANCED_ALGO_ENV_BSF_REGNUM)
#define DT_ADVANCED_ALGO_ENV_FTF_REGNUM	(0x02)

// advanced_algo_env_rsh
#define DT_ADVANCED_ALGO_ENV_RSH_OFFSET	(DT_ADVANCED_ALGO_ENV_FTF_OFFSET + DT_ADVANCED_ALGO_ENV_FTF_REGNUM)
#define DT_ADVANCED_ALGO_ENV_RSH_REGNUM	(0x02)

// advanced_algo_acc_ov
#define DT_ADVANCED_ALGO_ACC_OV_OFFSET	(DT_ADVANCED_ALGO_ENV_RSH_OFFSET + DT_ADVANCED_ALGO_ENV_RSH_REGNUM)
#define DT_ADVANCED_ALGO_ACC_OV_REGNUM	(0x02)


// advanced_algo_acc_gear
#define DT_ADVANCED_ALGO_ACC_GEAR_OFFSET	(DT_ADVANCED_ALGO_ACC_OV_OFFSET + DT_ADVANCED_ALGO_ACC_OV_REGNUM)
#define DT_ADVANCED_ALGO_ACC_GEAR_REGNUM	(0x02)


// BLE_RSSI_CURRENT
#define DT_BLE_RSSI_CURRENT_OFFSET	(DT_ADVANCED_ALGO_ACC_GEAR_OFFSET + DT_ADVANCED_ALGO_ACC_GEAR_REGNUM)
#define DT_BLE_RSSI_CURRENT_REGNUM	(0x02)

// the max register number for predict-sensor
#define MAX_PRS_REGNUM	(DT_BLE_RSSI_CURRENT_OFFSET + DT_BLE_RSSI_CURRENT_REGNUM - DT_NAME_ID_OFFSET + 1)

//MK67D________________________________

//MK201U_________for Insight-T______________________
#if(0)
//name_id
#define DT_NAME_ID_OFFSET	(0x0) // the offeset of snesor Data name_id
#define DT_NAME_ID_REGNUM	(0x2) // the number of registers occupied by name_id
//sensor_name
#define DT_SENSOR_NAME_OFFSET	(DT_NAME_ID_OFFSET + DT_NAME_ID_REGNUM)
#define DT_SENSOR_NAME_REGNUM	(GW_PRO_MAX_STRING_LEN_BYTE / 2 + GW_PRO_MAX_STRING_LEN_BYTE % 2)
//sensor_mac
#define DT_SENSOR_MAC_OFFSET	(DT_SENSOR_NAME_OFFSET + DT_SENSOR_NAME_REGNUM)
#define DT_SENSOR_MAC_REGNUM	(0x09) //
//sensor_type
#define DT_SENSOR_TYPE_OFFSET	(DT_SENSOR_MAC_OFFSET + DT_SENSOR_MAC_REGNUM)
#define DT_SENSOR_TYPE_REGNUM	(GW_PRO_MAX_STRING_LEN_BYTE / 2 + GW_PRO_MAX_STRING_LEN_BYTE % 2)
//manufacturer
#define DT_MANUFACTURER_OFFSET	(DT_SENSOR_TYPE_OFFSET + DT_SENSOR_TYPE_REGNUM)
#define DT_MANUFACTURER_REGNUM	(GW_PRO_MAX_STRING_LEN_BYTE / 2 + GW_PRO_MAX_STRING_LEN_BYTE % 2)
/*...........*/


// voltge current
#define DT_VOLTAGE_CURRENT_OFFSET	(DT_MANUFACTURER_OFFSET + DT_MANUFACTURER_REGNUM)
#define DT_VOLTAGE_CURRENT_REGNUM	(0x02)

//board_temperature_current
#define DT_BOARD_TEMPERATURE_CURRENT_OFFSET	(DT_VOLTAGE_CURRENT_OFFSET + DT_VOLTAGE_CURRENT_REGNUM)
#define DT_BOARD_TEMPERATURE_CURRENT_REGNUM	(0x02)

//environmental_temperature_current
#define DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET	(DT_BOARD_TEMPERATURE_CURRENT_OFFSET + DT_BOARD_TEMPERATURE_CURRENT_REGNUM)
#define DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM (0x02)

// BLE_RSSI_CURRENT
#define DT_IST_BLE_RSSI_CURRENT_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM)
#define DT_IST_BLE_RSSI_CURRENT_REGNUM (0x02)

// the max register number for insight-T
#define MAX_IST_REGNUM (DT_IST_BLE_RSSI_CURRENT_OFFSET + DT_IST_BLE_RSSI_CURRENT_REGNUM - DT_NAME_ID_OFFSET)
#else

// the max register number for insight-T
#define MAX_IST_REGNUM (DT_IST_BLE_RSSI_CURRENT_OFFSET + DT_IST_BLE_RSSI_CURRENT_REGNUM - DT_NAME_ID_OFFSET)

#endif
//MK201D_______________________________


#else
// ref@Parameter of bullet gateway.xlsx(Data Uploading)

//MK258U___GW parameter______
//BOARD_TEMPERATURE_CURRENT
#define DT_GW_BOARD_TEMPERATURE_CURRENT_OFFSET	(0x00)
#define DT_GW_BOARD_TEMPERATURE_CURRENT_REGNUM	(0x01)
//last temperature sensing time [1,2]
#define DT_GW_LAST_TEMP_SENSING_TIME_OFFSET (0x01)	
#define DT_GW_LAST_TEMP_SENSING_TIME_REGNUM	(0x02)

//MK258D_____________________


//MK260U____Predict Sensor_________________________
//ADVANCED_ALGO_VEL_OV [0,1]
#define DT_ADVANCED_ALGO_VEL_OV_OFFSET	(0x0)
#define DT_ADVANCED_ALGO_VEL_OV_REGNUM	(0x02)

//ADVANCED_ALGO_ENV_OV [2,3]
#define DT_ADVANCED_ALGO_ENV_OV_OFFSET	(DT_ADVANCED_ALGO_VEL_OV_OFFSET + DT_ADVANCED_ALGO_VEL_OV_REGNUM)
#define DT_ADVANCED_ALGO_ENV_OV_REGNUM	(0x02)

//ADVANCED_ALGO_ACC_OV	[4,5]
#define DT_ADVANCED_ALGO_ACC_OV_OFFSET	(DT_ADVANCED_ALGO_ENV_OV_OFFSET + DT_ADVANCED_ALGO_ENV_OV_REGNUM)
#define DT_ADVANCED_ALGO_ACC_OV_REGNUM	(0x02)

//ADVANCED_ALGO_VEL_RSH [6,7]
#define DT_ADVANCED_ALGO_VEL_RSH_OFFSET	(DT_ADVANCED_ALGO_ACC_OV_OFFSET + DT_ADVANCED_ALGO_ACC_OV_REGNUM)
#define DT_ADVANCED_ALGO_VEL_RSH_REGNUM	(0x02)

//ADVANCED_ALGO_VEL_MTR [8,9]
#define DT_ADVANCED_ALGO_VEL_MTR_OFFSET	(DT_ADVANCED_ALGO_VEL_RSH_OFFSET + DT_ADVANCED_ALGO_VEL_RSH_REGNUM)
#define DT_ADVANCED_ALGO_VEL_MTR_ERGNUM (0x02)

//ADVANCED_ALGO_VEL_FAN [10,11]                                            DT_ADVANCED_ALGO_VEL_MTR_ERGNUM 
#define DT_ADVANCED_ALGO_VEL_FAN_OFFSET	(DT_ADVANCED_ALGO_VEL_MTR_OFFSET + DT_ADVANCED_ALGO_VEL_MTR_ERGNUM)
#define DT_ADVANCED_ALGO_VEL_FAN_REGNUM	(0x02)

//ADVANCED_ALGO_VEL_PUMP [12,13]
#define DT_ADVANCED_ALGO_VEL_PUMP_OFFSET (DT_ADVANCED_ALGO_VEL_FAN_OFFSET + DT_ADVANCED_ALGO_VEL_FAN_REGNUM)
#define DT_ADVANCED_ALGO_VEL_PUMP_REGNUM (0x02)

//ADVANCED_ALGO_ENV_BPFI [14,15]
#define DT_ADVANCED_ALGO_ENV_BPFI_OFFSET (DT_ADVANCED_ALGO_VEL_PUMP_OFFSET + DT_ADVANCED_ALGO_VEL_PUMP_REGNUM)
#define DT_ADVANCED_ALGO_ENV_BPFI_REGNUM (0x02)

//ADVANCED_ALGO_ENV_BPFO	[16,17]
#define DT_ADVANCED_ALGO_ENV_BPFO_OFFSET (DT_ADVANCED_ALGO_ENV_BPFI_OFFSET + DT_ADVANCED_ALGO_ENV_BPFI_REGNUM)
#define DT_ADVANCED_ALGO_ENV_BPFO_REGNUM (0x02)

//ADVANCED_ALGO_ENV_BSF [18,19]
#define DT_ADVANCED_ALGO_ENV_BSF_OFFSET (DT_ADVANCED_ALGO_ENV_BPFO_OFFSET + DT_ADVANCED_ALGO_ENV_BPFO_REGNUM)
#define DT_ADVANCED_ALGO_ENV_BSF_REGNUM (0x02)

//ADVANCED_ALGO_ENV_FTF [20,21]
#define DT_ADVANCED_ALGO_ENV_FTF_OFFSET	(DT_ADVANCED_ALGO_ENV_BSF_OFFSET + DT_ADVANCED_ALGO_ENV_BSF_REGNUM)
#define DT_ADVANCED_ALGO_ENV_FTF_REGNUM	(0x02)

//ADVANCED_ALGO_ENV_RSH [22,23]
#define DT_ADVANCED_ALGO_ENV_RSH_OFFSET	(DT_ADVANCED_ALGO_ENV_FTF_OFFSET + DT_ADVANCED_ALGO_ENV_FTF_REGNUM)
#define DT_ADVANCED_ALGO_ENV_RSH_REGNUM	(0x02)

//ADVANCED_ALGO_ACC_GEAR [24,25]
#define DT_ADVANCED_ALGO_ACC_GEAR_OFFSET	(DT_ADVANCED_ALGO_ENV_RSH_OFFSET + DT_ADVANCED_ALGO_ENV_RSH_REGNUM)
#define DT_ADVANCED_ALGO_ACC_GEAR_REGNUM	(0x02)

//ADVANCED_ALGO_EST_RPM_START [26,27]
#define DT_ADVANCED_ALGO_EST_RPM_START_OFFSET	(DT_ADVANCED_ALGO_ACC_GEAR_OFFSET + DT_ADVANCED_ALGO_ACC_GEAR_REGNUM)
#define DT_ADVANCED_ALGO_EST_RPM_START_REGNUM	(0x02)

//ADVANCED_ALGO_EST_RPM_END [28,29]
#define DT_ADVANCED_ALGO_EST_RPM_END_OFFSET	(DT_ADVANCED_ALGO_EST_RPM_START_OFFSET + DT_ADVANCED_ALGO_EST_RPM_START_REGNUM)
#define DT_ADVANCED_ALGO_EST_RPM_END_REGNUM	(0x02)

//ADVANCED_ALGO_RMS_MAG_PRE [30,31]
#define DT_ADVANCED_ALGO_RMS_MAG_PRE_OFFSET	(DT_ADVANCED_ALGO_EST_RPM_END_OFFSET + DT_ADVANCED_ALGO_EST_RPM_END_REGNUM)
#define DT_ADVANCED_ALGO_RMS_MAG_PRE_REGNUM	(0x02)


//ADVANCED_ALGO_RMS_VIB_PRE [32,33]
#define DT_ADVANCED_ALGO_RMS_VIB_PRE_OFFSET (DT_ADVANCED_ALGO_RMS_MAG_PRE_OFFSET + DT_ADVANCED_ALGO_RMS_MAG_PRE_REGNUM)
#define DT_ADVANCED_ALGO_RMS_VIB_PRE_REGNUM (0x02)

//ENVIRONMENTAL_TEMPERATURE_CURRENT [34]
#define DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET	(DT_ADVANCED_ALGO_RMS_VIB_PRE_OFFSET + DT_ADVANCED_ALGO_RMS_VIB_PRE_REGNUM)
#define DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM (0x01)

//ENVIRONMENTAL_TEMPERATURE_MAX [35]
#define DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM)
#define DT_ENVRIONMENTAL_TEMPERATURE_MAX_REGNUM (0x01)

//ENVIRONMENTAL_TEMPERATURE_MIN [36]
#define DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET (DT_ENVIRONMENTAL_TEMPERATURE_MAX_OFFSET + DT_ENVRIONMENTAL_TEMPERATURE_MAX_REGNUM)	
#define DT_ENVIRONMENTAL_TEMPERATURE_MIN_REGNUM (0x01)

//ADVANCED_ALGO_ALARM_CODE [37,41]
#define DT_ADVANCED_ALGO_ALARM_CODE_OFFSET	(DT_ENVIRONMENTAL_TEMPERATURE_MIN_OFFSET + DT_ENVIRONMENTAL_TEMPERATURE_MIN_REGNUM)
#define DT_ADVANCED_ALGO_ALARM_CODE_REGNUM	(0x5)

//BLE_RSSI_CURRENT	[42]
#define DT_BLE_RSSI_CURRENT_OFFSET	(DT_ADVANCED_ALGO_ALARM_CODE_OFFSET + DT_ADVANCED_ALGO_ALARM_CODE_REGNUM)
#define DT_BLE_RSSI_CURRENT_REGNUM	(0x01)

//REMAINING_VOLUME [43]
#define DT_REMAINING_VOLUME_OFFSET	(DT_BLE_RSSI_CURRENT_OFFSET + DT_BLE_RSSI_CURRENT_REGNUM)
#define DT_REMAINING_VOLUME_REGNUM	(0x01)

//ADVANCED_ALGO_MACHINE_RUN_CODE	[44]
#define DT_ADVANCED_ALGO_MACHINE_RUN_CODE_OFFSET	(DT_REMAINING_VOLUME_OFFSET + DT_REMAINING_VOLUME_REGNUM)
#define DT_ADVANCED_ALGO_MACHINE_RUN_CODE_REGNUM	(0x01)

// ADVANCED_ALGO_MACHINE_CONDITION_CODE [45]
#define DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_OFFSET	(DT_ADVANCED_ALGO_MACHINE_RUN_CODE_OFFSET + DT_ADVANCED_ALGO_MACHINE_RUN_CODE_REGNUM)
#define DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_REGNUM (0x01)

// last vibration sensing time [46,47]
#define DT_LAST_VIB_SENSING_TIME_OFFSET	(DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_OFFSET + DT_ADVANCED_ALGO_MACHINE_CONDITION_CODE_REGNUM)
#define DT_LAST_VIB_SENSING_TIME_REGNUM	(0x02)

//last temperature sensing time [48,49]
#define DT_LAST_TEMP_SENSING_TIME_OFFSET	(DT_LAST_VIB_SENSING_TIME_OFFSET + DT_LAST_VIB_SENSING_TIME_REGNUM)
#define DT_LAST_TEMP_SENSING_TIME_REGNUM	(0x02)

//Update I	[50]
#define DT_UPDATE_I_OFFSET		(DT_LAST_TEMP_SENSING_TIME_OFFSET + DT_LAST_TEMP_SENSING_TIME_REGNUM)
#define DT_UPDATE_I_REGNUM	(0x01)

//update II [51]
#define DT_UPDATE_II_OFFSET	(DT_UPDATE_I_OFFSET + DT_UPDATE_I_REGNUM)
#define DT_UPDATE_II_REGNUM	 (0x01)




//MK260D________________________________________


//MK372U____Insight_T_____
// BOARD_TEMPERATURE_CURRENT [0,1]
#define DT_IST_BOARD_TEMPERATURE_CURRENT_OFFSET	(0x0)
#define DT_IST_BOARD_TEMPERATURE_CURRENT_REGNUM	(0x02)
//VOLTAGE_CURRENT [2,3]
#define DT_VOLTAGE_CUTTENT_OFFSET (DT_IST_BOARD_TEMPERATURE_CURRENT_OFFSET + DT_IST_BOARD_TEMPERATURE_CURRENT_REGNUM)
#define DT_VOLTAGE_CURRENT_REGNUM (0x02)
//BLE_RSSI_CURRENT [4]
#define DT_IST_BLE_RSSI_CURRENT_OFFSET	(DT_VOLTAGE_CUTTENT_OFFSET + DT_VOLTAGE_CURRENT_REGNUM)
#define DT_IST_BLE_RSSI_CURRENT_REGNUM (0x01)
//ENVIRONMENTAL_TEMPERATURE_CURRENT [5]
#define DT_IST_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET (DT_IST_BLE_RSSI_CURRENT_OFFSET + DT_IST_BLE_RSSI_CURRENT_REGNUM)
#define DT_IST_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM (0x01)

/*...*/

// last temperature sensing time [6, 7]
#define DT_IST_TEMP_SENSING_TIME_OFFSET	(DT_IST_ENVIRONMENTAL_TEMPERATURE_CURRENT_OFFSET +  DT_IST_ENVIRONMENTAL_TEMPERATURE_CURRENT_REGNUM)
#define DT_IST_TEMP_SENSING_TIME_REGNUM	(0x02)

//Update I 	[8]
#define DT_IST_UPDATE_I_OFFSET	(DT_IST_TEMP_SENSING_TIME_OFFSET + DT_IST_TEMP_SENSING_TIME_REGNUM)
#define DT_IST_UPDATE_I_REGNUM (0x01)




//MK372D__________________




#endif






enum sensor_type{
	STYPE_UNKNOWN = 0,
	STYPE_PREDICT = 1,
	STYPE_INSIGHT_T = 2,
	STYPE_GW = 3,
};


/*
*/
struct sdata_layout{
	bool is_valid; // set to true when the layout-info is valid
	uint32_t nameid;
	uint16_t ch;
	uint16_t addr; // the reg-address where we put the sensor-data
	uint16_t regnum; // the number of registers
	enum sensor_type stype;
};



/*
*/
struct mslave_ipc_dtblock{
	uint32_t val;
	uint8_t strbuf[0x100];
};



#if(1)
#include "sh_mem.h"
/*
 * data structure for items in share memory.
 */
typedef struct _appShmDataItem_original {   /* 0-2 used */
  //appShmDataType_t appShmDataType;
  uint32_t appShmDataType;
  union {
    deviceTableData_t devTableData;
    gatewayTableData_t gwTableData;
    sensorDataTableData_t sensorDataTableData;
  };
} appShmDataItem_t_original;
#endif




void *pthandler_mslave_ipc_service(void *pt_param);

enum error_code app_mslave_ipc_create_shmem(struct modbus_slave_controller*pt_mslave_ctr, const uint32_t memsize);
enum error_code app_mslave_ipc_destroy_shmem(struct modbus_slave_controller*pt_mslave_ctr);

enum error_code app_mslave_ipc_create_sem(struct modbus_slave_controller*pt_mslave_ctr);
enum error_code app_mslave_ipc_release_sem(struct modbus_slave_controller*pt_mslave_ctr);

enum error_code app_mslave_ipc_load_conf(struct modbus_slave_controller*pt_mslave_ctr);

struct sdata_layout app_mslave_ipc_get_sdata_layout_info(const deviceTableData_t *pt_devinfo);
enum error_code app_mslave_ipc_trig_mapping_block_update(struct modbus_slave_controller*pt_mslave_ctr);

//void app_mslave_dump_sdata_info(const uint8_t *pt_sdata_buf);
void app_mslave_dump_sdata_info(const struct modbus_slave_controller*pt_mslave_ctr);

enum error_code app_mslave_ipc_wait_for_update(const struct modbus_slave_controller * pt_mslave_ctr);

struct sdata_layout app_mslave_ipc_get_gwconf_layout_info(const gatewayTableData_t *pt_gwconf);

void to_set_uint16(const uint16_t val, uint16_t*ptdes);

enum error_code app_mslave_ipc_set_port_conf(struct port_conf_info *pt_port_conf, const gwConfig_gwConfig_modbusConfig_t*pt_mbus_cfg);



#endif

