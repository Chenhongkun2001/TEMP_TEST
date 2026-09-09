/* to evaluate the performance of SQLite3
to compile this file :

cp ~/skf_gw_x/sqlite3_test.c .;rm sqlite3_test ; aarch64-none-linux-gnu-gcc sqlite3_test.c -I/home/forlinx/work/OK62xx-linux-sdk/OK62xx-linux-fs/rootfs/usr/include -I../../AppProtocols/nanopb_protocol/ -I../../AppProtocols/tools/nanopb/ -I./libell_for_OK62xx/src/ell/ -I../skf_gw_mqtt/middleware/common/ -I./cjson/	--sysroot=/home/forlinx/work/OK62xx-linux-sdk/OK62xx-linux-fs/rootfs -L/usr/lib/ -L./monitor/lib_bluez -lsqlite3 -lglib-2.0 -ldl -lrt -lshared-mainloop -lbluetooth-internal -o sqlite3_test ; scp sqlite3_test root@192.168.1.66:~/skf_gw_mqtt/


*/
#include <stddef.h>
#include "sqlite3.h"
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
//#include "util_dbg.h"
#include "app_hci_evt_mgmt.h"
#include "Common.pb.h"
#include "app_db.h"
#include "app_froto.h"

#include <sys/ipc.h>
#include <sys/msg.h>

#define PATH_DB	"./skf.db"

#define LOG_INFO(OUTPOINT, ...) \
  if(INFO_OPEN) { \
    printf("\033[0;36m[Info]file(%s),line(%d):\033[0;0m ", __FILE__, \
           __LINE__); \
    printf(__VA_ARGS__); \
    printf("\r\n\033[0;0m"); \
  }


#define DBG_LOG_ERR(__fmt, ...)	 LOG_INFO(OUTPOINT, __fmt,##__VA_ARGS__)
#define DBG_LOG_INFO(__fmt, ...) LOG_INFO(OUTPOINT, __fmt,##__VA_ARGS__)	 
#define DBG_LOG_WARN(__fmt, ...) LOG_INFO(OUTPOINT, __fmt,##__VA_ARGS__)	 




#define SQL_STR_FOR_INSERTION "INSERT INTO SensorData ( nameId,BLESensorName,macAddr,type,manufacturer,measureseqno,sampletime,ReceivedTimestamp,measurement,DataType,Format,range,unit,measurelengthsample,totaldatalengthsample,dimension,dataformat,sampleperiods,encryp,value,sample_rate,product,sensor)  VALUES (0,'SIT666666','C4BD6A100117','Insight-T','SKF',215,1725849420,1725849420,0,29,'Digit',0,5,1,1,0,7,0,0,'2.978906',0.000000,7,11)"

static void sql_insert(char op_mode)
{
	int tpint;		
	sqlite3 *pt_db = NULL;	
	unsigned int tpcnt = 0;	
	char *pt_errmsg = NULL;
	
	if(op_mode)
	{
		tpint = sqlite3_open( PATH_DB, &pt_db);
		if(SQLITE_OK != tpint)
		{
			DBG_LOG_ERR("fail to open DB %s", PATH_DB);
			pt_db = NULL;
			goto ERR;
		}
		DBG_LOG_INFO("database %s is opened successfully", PATH_DB);
		#if(1) // to insert
			tpcnt = 0;
			while(1)
			{
				tpcnt ++;
				DBG_LOG_INFO(" cnt val = %d", tpcnt);
				tpint = sqlite3_exec( pt_db, SQL_STR_FOR_INSERTION, NULL, NULL, &pt_errmsg);
				if(SQLITE_OK != tpint)
				{
					DBG_LOG_ERR("fail to insert data to table SensorData");
					goto ERR;
				}
			}
		#endif
		ERR:
			if(pt_db)
			{
				sqlite3_close( pt_db);
				pt_db = NULL;
			}
	}
	else //____________________________________________________
	{
		#if(1) // to insert
				tpcnt = 0;
				while(1)
				{
					tpint = sqlite3_open( PATH_DB, &pt_db);
					if(SQLITE_OK != tpint)
					{
						DBG_LOG_ERR("fail to open DB %s", PATH_DB);
						pt_db = NULL;
						goto ERR2;
						break;
					}
					//DBG_LOG_INFO("database %s is opened successfully", PATH_DB);
					tpcnt ++;
					DBG_LOG_INFO(" cnt val = %d", tpcnt);
					tpint = sqlite3_exec( pt_db, SQL_STR_FOR_INSERTION, NULL, NULL, &pt_errmsg);
					if(SQLITE_OK != tpint)
					{
						DBG_LOG_ERR("fail to insert data to table SensorData");
						goto ERR2;
					}
					
					if(pt_db)
					{	
						sqlite3_close( pt_db);
						pt_db = NULL;
					}
				}
			ERR2:
					
		#endif
	}
}




static int cb_for_tbl_sensordata_query(void *NotUsed, int argc, char **argv, char **azColName);
#define SQL_STR_FOR_QUERY	"select * from sensordata where BLESensorName=\"SIT666666\""
static void sql_query(char op_mode)
{
	int tpint;		
	sqlite3 *pt_db = NULL;	
	unsigned int tpcnt = 0;	
	char *pt_errmsg = NULL;
	
	if(1)
	{
		tpint = sqlite3_open( PATH_DB, &pt_db);
		if(SQLITE_OK != tpint)
		{
			DBG_LOG_ERR("fail to open DB %s", PATH_DB);
			pt_db = NULL;
			goto ERR;
		}
		DBG_LOG_INFO("database %s is opened successfully", PATH_DB);
		#if(1) // to insert
			tpcnt = 0;
			while(1)
			{
				tpcnt ++;
				DBG_LOG_INFO(" ========================================cnt val = %d", tpcnt);
				tpint = sqlite3_exec( pt_db, SQL_STR_FOR_QUERY, cb_for_tbl_sensordata_query, NULL, &pt_errmsg);
				if(SQLITE_OK != tpint)
				{
					DBG_LOG_ERR("fail to insert data to table SensorData");
					goto ERR;
				}
			}
		#endif
		ERR:
			if(pt_db)
			{
				sqlite3_close( pt_db);
				pt_db = NULL;
			}
	}
}

static int cb_for_tbl_sensordata_query(void *NotUsed, int argc, char **argv, char **azColName)
{
	for(int i = 0; i < argc; i++)
	{
		DBG_LOG_INFO(" %s = %s ", azColName[i], argv[i]);
	}
	return 0;
}


#define SQL_STR_FOR_DELETE	"delete from sensordata where BLESensorName=\"SIT666666\""
static void sql_delete(char op_mode)
{
	int tpint;		
	sqlite3 *pt_db = NULL;	
	unsigned int tpcnt = 0;	
	char *pt_errmsg = NULL;
	
	if(op_mode)
	{
		tpint = sqlite3_open( PATH_DB, &pt_db);
		if(SQLITE_OK != tpint)
		{
			DBG_LOG_ERR("fail to open DB %s", PATH_DB);
			pt_db = NULL;
			goto ERR;
		}
		DBG_LOG_INFO("database %s is opened successfully", PATH_DB);
		#if(1) // to insert
			tpcnt = 0;
			while(1)
			{
				tpcnt ++;
				DBG_LOG_INFO(" cnt val = %d", tpcnt);
				tpint = sqlite3_exec( pt_db, SQL_STR_FOR_DELETE, NULL, NULL, &pt_errmsg);
				if(SQLITE_OK != tpint)
				{
					DBG_LOG_ERR("fail to delete data from table SensorData");
					goto ERR;
				}
			}
		#endif
		ERR:
			if(pt_db)
			{
				sqlite3_close( pt_db);
				pt_db = NULL;
			}
	}
	else //____________________________________________________
	{
		#if(1) // to insert
				tpcnt = 0;
				while(1)
				{
					tpint = sqlite3_open( PATH_DB, &pt_db);
					if(SQLITE_OK != tpint)
					{
						DBG_LOG_ERR("fail to open DB %s", PATH_DB);
						pt_db = NULL;
						goto ERR2;
						break;
					}
					//DBG_LOG_INFO("database %s is opened successfully", PATH_DB);
					tpcnt ++;
					DBG_LOG_INFO(" cnt val = %d", tpcnt);
					tpint = sqlite3_exec( pt_db, SQL_STR_FOR_DELETE, NULL, NULL, &pt_errmsg);
					if(SQLITE_OK != tpint)
					{
						DBG_LOG_ERR("fail to delete data from table SensorData");
						goto ERR2;
					}
					
					if(pt_db)
					{	
						sqlite3_close( pt_db);
						pt_db = NULL;
					}
				}
			ERR2:
					
		#endif
	}
}



#define SQL_STR_FOR_UPDATE	"update sensordata set nameId=888 where BLESensorName=\"SIT666666\""
static void sql_update(char op_mode)
{
	int tpint;		
	sqlite3 *pt_db = NULL;	
	unsigned int tpcnt = 0;	
	char *pt_errmsg = NULL;
	
	if(op_mode)
	{
		tpint = sqlite3_open( PATH_DB, &pt_db);
		if(SQLITE_OK != tpint)
		{
			DBG_LOG_ERR("fail to open DB %s", PATH_DB);
			pt_db = NULL;
			goto ERR;
		}
		DBG_LOG_INFO("database %s is opened successfully", PATH_DB);
		#if(1) // to insert
			tpcnt = 0;
			while(1)
			{
				tpcnt ++;
				DBG_LOG_INFO(" cnt val = %d", tpcnt);
				tpint = sqlite3_exec( pt_db, SQL_STR_FOR_UPDATE, NULL, NULL, &pt_errmsg);
				if(SQLITE_OK != tpint)
				{
					DBG_LOG_ERR("fail to delete data from table SensorData");
					goto ERR;
				}
			}
		#endif
		ERR:
			if(pt_db)
			{
				sqlite3_close( pt_db);
				pt_db = NULL;
			}
	}
	else //____________________________________________________
	{
		#if(1) // to insert
				tpcnt = 0;
				while(1)
				{
					tpint = sqlite3_open( PATH_DB, &pt_db);
					if(SQLITE_OK != tpint)
					{
						DBG_LOG_ERR("fail to open DB %s", PATH_DB);
						pt_db = NULL;
						goto ERR2;
						break;
					}
					//DBG_LOG_INFO("database %s is opened successfully", PATH_DB);
					tpcnt ++;
					DBG_LOG_INFO(" cnt val = %d", tpcnt);
					tpint = sqlite3_exec( pt_db, SQL_STR_FOR_UPDATE, NULL, NULL, &pt_errmsg);
					if(SQLITE_OK != tpint)
					{
						DBG_LOG_ERR("fail to delete data from table SensorData");
						goto ERR2;
					}
					
					if(pt_db)
					{	
						sqlite3_close( pt_db);
						pt_db = NULL;
					}
				}
			ERR2:
					
		#endif
	}
}



#if(1)
static app_state_t to_fill_dtunit_array_for_sit_data_writing( struct msg_format_controller*pt_msg_format_ctr, const struct sit_dtpacket*pt_sit_dtpkt)
{
	app_state_t tpret = ST_OK;
	/**/
#if(1)// for debug only 0830
				//g_sensor_data_idx++;
				//kp_dtnum = 23; // the number of table column
				// to fill the data unit ref@configure_content_t SendorData[] 
				
				//sequence number
				//pt_msg_format_ctr->dtunit[0].field = 1; // NOTE!!! the index start from 1
				//pt_msg_format_ctr->dtunit[0].data.data_uint32 = g_sensor_data_idx;//pt_dtitem->idx;
			
				//nameID
				pt_msg_format_ctr->dtunit[0].field = 2; // NOTE!!! the index start from 1
				pt_msg_format_ctr->dtunit[0].data.data_uint32 = 0;//*((uint32_t*)&pt_sit_dtpkt->addr[2]);//pt_dtitem->name_id;
				//SensorName
				pt_msg_format_ctr->dtunit[1].field = 3;
				//snprintf( pt_msg_format_ctr->dtunit[1].data.data_char, GW_PRO_MAX_STRING_LEN_BYTE, "%s",pt_dtitem->sensor_name);				
				snprintf( pt_msg_format_ctr->dtunit[1].data.data_char, GW_PRO_MAX_STRING_LEN_BYTE, "%s", pt_sit_dtpkt->ad_data.local_name);
				
				//macAddr
				pt_msg_format_ctr->dtunit[2].field = 4;
				snprintf(pt_msg_format_ctr->dtunit[2].data.data_char, GW_PRO_MAX_STRING_LEN_BYTE, "%.2X%.2X%.2X%.2X%.2X%.2X", pt_sit_dtpkt->addr[0],pt_sit_dtpkt->addr[1],pt_sit_dtpkt->addr[2],pt_sit_dtpkt->addr[3],pt_sit_dtpkt->addr[4],pt_sit_dtpkt->addr[5]);//pt_dtitem->sensor_mac
				//DBG_LOG_ERR("MAC-add string %s", pt_msg_format_ctr->dtunit[2].data.data_char);
				
				//type
				pt_msg_format_ctr->dtunit[3].field = 5;
				//pt_msg_format_ctr->dtunit[3].data.data_uint32 = pt_dtitem->sensor_type;
				snprintf( pt_msg_format_ctr->dtunit[3].data.data_char, GW_PRO_MAX_STRING_LEN_BYTE, "%s", "Insight-T");				
				//snprintf( pt_msg_format_ctr->dtunit[3].data.data_char, GW_PRO_MAX_STRING_LEN_BYTE, "%s", "BulletSensor");
				
				//manufacture
				pt_msg_format_ctr->dtunit[4].field = 6;
				snprintf(pt_msg_format_ctr->dtunit[4].data.data_char, GW_PRO_MAX_STRING_LEN_BYTE, "%s", "SKF");
				//measure-seq-no
				pt_msg_format_ctr->dtunit[5].field = 7;
				pt_msg_format_ctr->dtunit[5].data.data_uint32 = 0;
				//sample time
				pt_msg_format_ctr->dtunit[6].field = 8;
				pt_msg_format_ctr->dtunit[6].data.data_uint32 =  pt_sit_dtpkt->tstamp;
				//Recicved TS
				pt_msg_format_ctr->dtunit[7].field = 9;
				pt_msg_format_ctr->dtunit[7].data.data_uint32 = pt_sit_dtpkt->tstamp;
				//measurement
				pt_msg_format_ctr->dtunit[8].field = 10;
				pt_msg_format_ctr->dtunit[8].data.data_uint32 = 0;
				//data-type
				pt_msg_format_ctr->dtunit[9].field = 11;
				pt_msg_format_ctr->dtunit[9].data.data_uint32 = SKFChina_Common_MeasurementType_ENVIROMENTAL_TEMPERATURE_CURRENT;
				//format
				pt_msg_format_ctr->dtunit[10].field = 12; 
				snprintf( pt_msg_format_ctr->dtunit[10].data.data_char, GW_PRO_MAX_STRING_LEN_BYTE, "%s", DT_STORAGE_FORM_DIGIT);
				//range
				pt_msg_format_ctr->dtunit[11].field = 13;
				pt_msg_format_ctr->dtunit[11].data.data_uint32 = 0;
				//unit
				pt_msg_format_ctr->dtunit[12].field = 14;
				pt_msg_format_ctr->dtunit[12].data.data_uint32 = SKFChina_Common_Unit_CDEGREE; // c
				//measurement length in Pts
				pt_msg_format_ctr->dtunit[13].field = 15;
				pt_msg_format_ctr->dtunit[13].data.data_uint32 = 0;
				//total data length
				pt_msg_format_ctr->dtunit[14].field = 16;
				pt_msg_format_ctr->dtunit[14].data.data_uint32 = 0;
				//dimension of data
				pt_msg_format_ctr->dtunit[15].field = 17; 
				pt_msg_format_ctr->dtunit[15].data.data_uint32 = 0;
				//data format
				pt_msg_format_ctr->dtunit[16].field = 18;
				pt_msg_format_ctr->dtunit[16].data.data_uint32 = SKFChina_Common_Format_FORMAT_FLOAT32;//SKFChina_Common_Format_FORMAT_INT8;//pt_dtitem->data_format;
				//sample period
				pt_msg_format_ctr->dtunit[17].field = 19;
				pt_msg_format_ctr->dtunit[17].data.data_uint32 = 0;
				//encryption
				pt_msg_format_ctr->dtunit[18].field = 20;
				pt_msg_format_ctr->dtunit[18].data.data_uint32 = 0;

				//value
				pt_msg_format_ctr->dtunit[19].field = 21;
				pt_msg_format_ctr->dtunit[19].data.data_float = pt_sit_dtpkt->ad_data.m_tempval;
				
				// sample-rate
				pt_msg_format_ctr->dtunit[20].field = 22;
				pt_msg_format_ctr->dtunit[20].data.data_float = 0;
				//Product type
				pt_msg_format_ctr->dtunit[21].field = 23;
				pt_msg_format_ctr->dtunit[21].data.data_uint32 = SKFChina_Common_ProductType_INSIGHT_T;
				// Sensor type
				pt_msg_format_ctr->dtunit[22].field = 24;
				pt_msg_format_ctr->dtunit[22].data.data_uint32 = SKFChina_Common_SensorType_NTC_TEMPERATURE;
#else
#endif
		
	return tpret;
}

/* to be called to write Insight-T data to database

NOTE!!! we just try to write, return imediately when fail to write to DB. no waiting, no retry
*/
//app_state_t app_db_write_sensor_conf_to_db(struct dbop_controller * pt_dbop_ctr, const sensorConfig_t * pt_sensor_conf)
static app_state_t to_write_sit_data_to_db( struct msg_format_controller*pt_msg_format_ctr, const int kp_qid)
{
	app_state_t tpret = ST_OK;
	uint32_t kp_dtunit_sz = 0;

	if((NULL == pt_msg_format_ctr))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		goto EXIT;
	}

	// to clean the formating buffer
	memset( pt_msg_format_ctr, 0, sizeof(struct msg_format_controller));

	#if(1)
		struct sit_dtpacket kp_sit_dtpkt = {0};
		snprintf( kp_sit_dtpkt.ad_data.local_name, MAX_PKT_INFO, "%s", "SIT666666"),
		to_fill_dtunit_array_for_sit_data_writing( pt_msg_format_ctr,  &kp_sit_dtpkt);
	#endif
	
	// 
	pt_msg_format_ctr->msginfo.header.from = gw_pro_process_ble;
	pt_msg_format_ctr->msginfo.header.msgTimestamp_s = time(NULL);
	pt_msg_format_ctr->msginfo.header.seqNo = 0;//app_db_allocate_command_id( pt_dbop_ctr);
	pt_msg_format_ctr->msginfo.header.current_package = 1;
	pt_msg_format_ctr->msginfo.header.total_package = 1;
	pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.command = gw_pro_command_sqlite_update_item;
	pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.commandId = gw_pro_command_sqlite_update_item;
	pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_update_item_param.itemIdx = pt_msg_format_ctr->dtunit[0].data.data_uint32;
	pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_update_item_param.table = GW_PRO_TABLE_IDX_SENSOR_DATA_TABLE;
	
	kp_dtunit_sz = sizeof(SensorData)/sizeof(SensorData[0]) - 1; //

	pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_update_item_param.dataNum = kp_dtunit_sz;
	pt_msg_format_ctr->msginfo.header.comRespFg = GW_PRO_COMRESPFG_COMMAND;

	for(uint32_t i = 0; i < kp_dtunit_sz; i++)
	{
		pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].field = \
			pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].field;
		memcpy(&pt_msg_format_ctr->msginfo.header.command_or_response.commandMsg.param.sqlite_update_item_param.updateData[i % GW_PRO_MAX_DATA_FIELD_PER_OPERATION].data,\
			&pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data, sizeof(pt_msg_format_ctr->dtunit[i % MAX_MSG_DTUNIT].data));
	}

	// to fill the msg
	pt_msg_format_ctr->ipcmsg.mtype = gw_pro_command;
	memcpy( &pt_msg_format_ctr->ipcmsg.mtext, &pt_msg_format_ctr->msginfo, sizeof(gw_pro_message_header_t));

	// to send the msg
	//if(msgsnd( kp_qid, &pt_msg_format_ctr->ipcmsg, sizeof(gw_pro_message_header_t), IPC_NOWAIT) != 0)	
	if(msgsnd( kp_qid, &pt_msg_format_ctr->ipcmsg, sizeof(gw_pro_message_header_t), 0) != 0)
	{
		//DBG_LOG_ERR("msgsnd fail, for %s", strerror(errno));
		tpret = ST_ERR;
	}

	EXIT:
			return tpret;
}

#endif


//void *(* start_routine)(void *)
void *proutine_db_msg_recv(void *arg)
{
	const int hld_msgid_from_db = (int)arg;
	struct msgbuf kp_msg = {0};
	while(1)
	{
		if(msgrcv(hld_msgid_from_db, &kp_msg, MSG_BUF_LEN, 0, 0) < 0)
		{
			DBG_LOG_ERR("msgrcv fail , for %s", strerror(errno));
			continue;
		}
		DBG_LOG_INFO("msg is received from DB");
	}
}



static void sql_ipc_test(const char kp_op_mode)
{
	int tpcnt = 0;
	int tpint = 0;
	pthread_t kp_pthread;
	
	struct msg_format_controller kp_msg_format_ctr = {0};
	int kp_msgid_to_db = -1;
	int kp_msgid_from_db = -1;
	
	kp_msgid_to_db = msgget(MSG_SQL_QUEUE_KEY, IPC_CREAT | MSG_QUEUE_FLAG);
	if(kp_msgid_to_db < 0)
	{
		DBG_LOG_ERR("fail to get Queue ID");
		return;
	}

	kp_msgid_from_db = msgget( MSG_BLE_QUEUE_KEY, IPC_CREAT|MSG_QUEUE_FLAG);
	if(kp_msgid_from_db < 0)
	{
		DBG_LOG_ERR("fail to get Queue ID from msg receivation from DB");
		return;
	}

	
	tpint = pthread_create( &kp_pthread, NULL, proutine_db_msg_recv, (void *)kp_msgid_from_db);
	if(tpint != 0)
	{
		DBG_LOG_ERR("fail to create thread, for %s", strerror(tpint));
		return;
	}
	// write IPC msg to queue
	while(1)
	{
		tpcnt ++;
		DBG_LOG_INFO("=======================tpcnt = %d", tpcnt);
		memset( &kp_msg_format_ctr, 0, sizeof(struct msg_format_controller));
		to_write_sit_data_to_db( &kp_msg_format_ctr, kp_msgid_to_db);
	}
}


void main(int argc, char* argv[])
{
	int tpint = 0;
	pid_t kp_pid = 0;

	char kp_op_code = 0;
	int kp_op_mode = 0; // zero -> open-op-close...open-op-close; none-zero -> open-op....op->close

	if(argc != 3)
	{
		DBG_LOG_ERR("./sqlite3_test  <i/d/u/q>   <0/1>    //i ~ insert, d ~ delect , u ~ update , q ~ query");
		return;
	}

	for(int i = 0; i < argc; i++)
	{
		DBG_LOG_INFO("argc %d argv %s", i , argv[i]);
		if(i == 1)
		{
			kp_op_code = argv[i][0];
		}
		else if(i == 2)
		{
			kp_op_mode = atoi(argv[i]);
		}
	}
	if(('i' != kp_op_code)&&('d' != kp_op_code)&&('u' != kp_op_code)&&('q' != kp_op_code)&&('x' != kp_op_code))
	{
		DBG_LOG_ERR("./sqlite3_test  <i/d/u/q>   <0/1>    //i ~ insert, d ~ delect , u ~ update , q ~ query");				
		return;
	}
	DBG_LOG_WARN("op_code = %c , op_mode = %d", kp_op_code, kp_op_mode);

	kp_pid = fork();	
	if(kp_pid)
	{// the parent-process
		DBG_LOG_WARN("to sleep %d @ %d", getpid(), time(NULL));				
		sleep(3);
		DBG_LOG_WARN("to killl process %d @ %d", kp_pid, time(NULL));
		kill( kp_pid, SIGKILL);
		
	}
	else if(0 == kp_pid)
	{ // the child
		if('i' == kp_op_code)
		{
			DBG_LOG_WARN("test SQL INSERT");
			sql_insert(kp_op_mode);
		}
		else if('d' == kp_op_code)
		{			
			DBG_LOG_WARN("test SQL DELETE");
			sql_delete( kp_op_mode);
		}
		else if('u' == kp_op_code)
		{			
			DBG_LOG_WARN("test SQL UPDATE");
			sql_update( kp_op_code);
		}
		else if('q' == kp_op_code)
		{			
			DBG_LOG_WARN("test SQL QUERY");
			sql_query( kp_op_mode);
		}
		else if('x' == kp_op_code)
		{
			sql_ipc_test( kp_op_mode);
		}
		else
		{
			DBG_LOG_ERR("This is not suppose to happen");
		}
	}
	
	else
	{
		DBG_LOG_ERR("this is not supposed to happen");
	}

	DBG_LOG_WARN(" to EXIT the app");	
	
	return;
}


