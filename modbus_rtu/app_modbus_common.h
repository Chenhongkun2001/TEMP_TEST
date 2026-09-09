/*
*/
#ifndef __APP_MODBUS_COMMON_H
#define __APP_MODBUS_COMMON_H

#include "stdint.h"
#include "modbus-private.h"
#include "modbus.h"

#ifndef MODBUS_RTU_CHECKSUM_LENGTH 
	#define MODBUS_RTU_CHECKSUM_LENGTH 2
#endif

/*Modbus Functoion codes definition , ref@modbus.h for more*/
#ifndef MODBUS_FC_READ_FIFO_QUEUE	
	#define MODBUS_FC_READ_FIFO_QUEUE	0x18
#endif
#ifndef MODBUS_FC_READ_FILE_RECORD	
	#define MODBUS_FC_READ_FILE_RECORD	0x14
#endif
#ifndef MODBUS_FC_WRITE_FILE_RECORD
	#define MODBUS_FC_WRITE_FILE_RECORD	0x15
#endif
#ifndef MODBUS_FC_DIAGNOSTIC	
	#define MODBUS_FC_DIAGNOSTIC	0x08 /*sub-code 00 ~ 18, 20*/
#endif
#ifndef MODBUS_FC_GET_COM_EVENT_COUNTER 
	#define MODBUS_FC_GET_COM_EVENT_COUNTER 0x0B
#endif
#ifndef MODBUS_FC_GET_COM_EVENT_LOG
	#define MODBUS_FC_GET_COM_EVENT_LOG	0x0C
#endif
#ifndef MODBUS_FC_READ_DEV_ID
	#define MODBUS_FC_READ_DEV_ID	0x2B /*sub-code 13,14*/
#endif


/*sub-function definition for diagnostic*/
#define MODBUS_SFC_DIAG_RETURN_QUERY_DATA	0x0000
#define MODBUS_SFC_DIAG_RESTART_COM_OPT		0x0001
#define MODBUS_SFC_DIAG_RET_DIAG_REG		0x0002
#define MODBUS_SFC_DIAG_CHANGE_ASCII_INPUT_DELIMITER	0x0003
#define MODBUS_SFC_FORCE_LISTEN_MODE	0x0004
/***reserved***/
#define MODBUS_SFC_CLEAR_COUNTER_REGISTER	(0x000A)
#define MODBUS_SFC_RET_BUS_MSG_CNT	(0x000B)
#define MODBUS_SFC_RET_BUS_COM_ERR_CNT	(0x000C)
#define MODBUS_SFC_RET_BUS_EXCEPT_ERR_CNT	(0x000D)
#define MODBUS_SFC_RET_BUS_SERVER_MSG_CNT	(0x000E)
#define MODBUS_SFC_RET_SERVER_NO_RESP_CNT	(0x000F)
#define MODBUS_SFC_RET_SERVER_NAK_CNT	(0x0010)
#define MODBUS_SFC_RET_SERVER_BUSY_CNT	(0x0011)
#define MODBUS_SFC_RET_RET_BUS_CHAR_OVERRUN_CNT	(0x0012)
/*reserved*/
#define MODBUS_SFC_CLEAR_OVERRUN_CNT_FLG	(0x0014)
/*reserved*/
#define MODBUS_SFC_UNDEFINED        (0xFFFF);



#ifndef FILE_READ_REF_TYPE
	#define FILE_READ_REF_TYPE	(0x06)
#endif

#ifndef MAX_RECORD_NUMBER	
	#define MAX_RECORD_NUMBER (0x270F)
#endif


int app_modbus_com_send_msg(modbus_t *ctx, uint8_t *msg, int msg_length);



#endif

