#include "bt_gatt.h"

#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>


#include "bt_common.h"
#include "util_dbg.h"
#include "app_general_def.h"
#include "app_common.h"

DBG_LOCAL_LOG_DEBUG

#define DELAY_AND_RETRY(func, time_us, retry_cnt, retry_msg, fail_msg, \
                        success_msg) do { \
    uint32_t cnt = retry_cnt; \
    do { \
      if(0 != func) \
      { \
        if(cnt != 0) { \
          cnt--; \
          retry_msg; \
          usleep(time_us); \
        } else { \
          fail_msg; \
          break; \
        } \
      } else { \
        success_msg; \
        break; \
      } \
    } while(cnt > 0); \
} while(0);

struct desc {
	struct chrc *chrc;
	char *path;
	uint16_t handle;
	char *uuid;
	char **flags;
	size_t value_len;
	unsigned int max_val_len;
	uint8_t *value;
};

struct chrc {
	struct service *service;
	struct l_dbus_proxy *proxy;
	char *path;
	uint16_t handle;
	char *uuid;
	char **flags;
	bool notifying;
	struct l_queue *descs;
	size_t value_len;
	unsigned int max_val_len;
	uint8_t *value;
	uint16_t mtu;
	struct l_io *write_io;
	struct l_io *notify_io;
	bool authorization_req;
};

struct service {
	struct l_dbus *conn;
	struct l_dbus_proxy *proxy;
	char *path;
	uint16_t handle;
	char *uuid;
	bool primary;
	struct l_queue *chrcs;
	struct l_queue *inc;
};


static struct l_queue *g_pt_local_services = NULL;
static struct l_queue *g_pt_local_uuids = NULL;

static struct sock_io g_write_io;
static struct sock_io g_notify_io;

struct sock_io * get_g_notify_io(void)
{
	return &g_notify_io;
}

static bool is_chrc_proxy_active(struct l_dbus_proxy *pt_chrc_proxy);

/*
typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
to check if the proxy-UUID match the UUID given
ret@true is returned when they match
*/
static bool q_match_for_proxy_uuid(const void*pt_data, const const void*pt_uuid)
{
	bool tpret = false;

	struct l_dbus_proxy*pt_proxy = NULL;//
	char*pt_uuid_buf = NULL;
	const char*pt_interface = NULL;

	//DBG_LOG_INFO("BKP");
	
	if(pt_data == NULL)
	{
		DBG_LOG_ERR("the queue-item data is NULL");
		tpret = false;
		return tpret;
	}

	pt_proxy = *((struct l_dbus_proxy**)pt_data);

	//DBG_LOG_INFO("BKP");
	
	if((pt_uuid == NULL)||(pt_proxy == NULL))
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = false;
		return tpret;
	}
	//DBG_LOG_INFO("BKP proxy@0x%x", pt_proxy);
	
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if((pt_interface == NULL)||(strcmp( pt_interface, BT_IF_NM_GATTCHARACTERISTIC)))
	{
		DBG_LOG_ERR("the proxy interface validate fail");
		tpret = false;
		return tpret;
	}
	//DBG_LOG_INFO("BKP");
	if(false == l_dbus_proxy_get_property( pt_proxy, "UUID", "s", &pt_uuid_buf))
	{
		DBG_LOG_ERR("fail to get property UUID");
		tpret = false;
		return tpret ;
	}
	//DBG_LOG_INFO("BKP , tp_uuid_buf = 0x%x, %s", pt_uuid_buf, pt_uuid_buf);
	if(strcmp( pt_uuid_buf, (const char*)pt_uuid))
	{	
		//DBG_LOG_ERR("%s vs %s does not match", (const char*)pt_uuid, pt_uuid_buf);
		tpret = false;
		return tpret;
	}
	
	// >>> @20240530 Added by Sean (to fix the "write-wrong-proxy-when-multi-slaves-connected bug")
	DBG_LOG_DEBUG("UUID exists");
	if(false == is_chrc_proxy_active(pt_proxy))
	{
		DBG_LOG_DEBUG("But it is not active");
		tpret = false;
		return tpret;
	}
	DBG_LOG_DEBUG("And it is active");
	// <<<

	//DBG_LOG_INFO("BKP");
	
	tpret = true;
	return tpret;
}

/*to find the characteristic by the UUID given
pt_queue : the queue where the characteristic is kept 
pt_uuid : the characteristic going to find
ret@pointer points to the char-proxy, NULL is returned when fail to find the characteristic
*/

struct l_dbus_proxy*bt_gatt_find_characteristic( struct l_queue*pt_queue, const char*pt_uuid)
{
	struct l_dbus_proxy**pt_ret = NULL;
	if(pt_queue == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		return NULL;
	}

	if(0 == l_queue_length( pt_queue))
	{
		DBG_LOG_WARN("the queue length is 0 , nothing to find");
		return NULL;
	}
	
	pt_ret = l_queue_find( pt_queue, q_match_for_proxy_uuid, (const void *)pt_uuid);

	if(NULL == pt_ret)
	{
		return NULL;
	}

	return (*pt_ret);
}


/*to check if all BT characteristic is registered
ret@true is returned if it does
*/
bool bt_gatt_is_btchr_registration_done(struct l_queue *pt_queue)
{
	bool tpret = false;
	uint32_t tpu32 = 0;
	if(NULL == pt_queue)
	{
		tpret = false;
	}
	tpu32 = l_queue_length( pt_queue);
	if(tpu32 >= NUM_BTCHR_EXPECTED)
	{
		tpret = true;
	}
	DBG_LOG_INFO(" the BT-chr queue length %d, len expected %d", tpu32, NUM_BTCHR_EXPECTED);
	return tpret;
}

/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);
*/
static void msg_setup_for_charac_write_value(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder*pt_builder = NULL;
	struct attr_value*pt_attr_val = NULL;
	uint16_t i = 0;
	
	pt_builder = l_dbus_message_builder_new( message);
	if(pt_builder == NULL)
	{
		DBG_LOG_ERR("Oops!");
		return;
	}

	if(user_data == NULL)
	{
		DBG_LOG_ERR("this is not suppose to happen");
		return;
	}
	pt_attr_val = (struct attr_value*)user_data;

	l_dbus_message_builder_enter_array( pt_builder, "y");
	// the data to write
	//uint8_t tpu8 = 'B';
	for(i = 0;i < pt_attr_val->len;  i++ )
	{
		l_dbus_message_builder_append_basic( pt_builder, 'y', (const void *)&pt_attr_val->ptbuf[i]);
	}
	
	l_dbus_message_builder_leave_array( pt_builder);

	l_dbus_message_builder_enter_array( pt_builder, "{sv}");
	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	// type
	#if(0)
	l_dbus_message_builder_append_basic( pt_builder, 's', (const void *)"type");
	l_dbus_message_builder_enter_variant( pt_builder, "s");
	l_dbus_message_builder_append_basic( pt_builder, 's', (const void *)"");
	l_dbus_message_builder_leave_variant( pt_builder);
	#endif
	
	// offset
	l_dbus_message_builder_append_basic( pt_builder, 's', (const void *)"offset");
	l_dbus_message_builder_enter_variant( pt_builder, "q");
	uint16_t tpu16 = 0;
	l_dbus_message_builder_append_basic( pt_builder, 'q', (const void *)&tpu16);
	l_dbus_message_builder_leave_variant( pt_builder);
	
	l_dbus_message_builder_leave_dict( pt_builder);
	l_dbus_message_builder_leave_array( pt_builder);


	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);
	
	return;
}

/*typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);
*/
static void msg_reply_for_charac_write_value(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	struct attr_value *pt_attr_value = (struct attr_value*)user_data;
	
	const char*pt_name = NULL, *pt_text = NULL;
	if(l_dbus_message_is_error(result))
	{
		if(l_dbus_message_get_error( result, &pt_name, &pt_text))
		{
			DBG_LOG_ERR("charac WriteValue fail , err_info:%s, %s", pt_name, pt_text);
		}
		else
		{
			DBG_LOG_ERR("Charac WriteValue fail, unknown reasosn");
		}
	}
	else
	{
		DBG_LOG_INFO("charac WriteValue goes well");
	}

	// just signal the cond-variables , even error happens
	DBG_LOG_INFO("to signal the chrc-writting is done");
	if(pt_attr_value->pt_cond_var)
	{
		pthread_mutex_lock(&pt_attr_value->pt_cond_var->mtx);
		pt_attr_value->pt_cond_var->is_op_done = true;
		pthread_cond_signal(&pt_attr_value->pt_cond_var->cond_var);
		pthread_mutex_unlock( &pt_attr_value->pt_cond_var->mtx);
	}
	else
	{
		DBG_LOG_ERR("Oops! This is not supposed to happen");
	}
	
	return;
}

/*typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
static void destroy_for_charac_write_value(void *user_data)
{
	struct attr_value*pt_attr_val = NULL;
	if(user_data)
	{
		pt_attr_val = (struct attr_value*)user_data;
		l_free( pt_attr_val->ptbuf);
		l_free(pt_attr_val);
	}
	else
	{
		DBG_LOG_ERR("this is not suppose to happen");
	}
	return;
}

/*to check if the given chrc-proxy is still active
ret@true is returned
NOTE!! we check the device-proxy which the chrc-proxy belongs to is connected or not
*/
static bool is_chrc_proxy_active(struct l_dbus_proxy *pt_chrc_proxy)
{
	bool tpret = true;

	bool tp_connected = false;
	
	const char *pt_dev_path = NULL;
	const char *pt_chrc_path = NULL;
	const struct l_dbus_proxy *pt_dev_proxy = NULL;
	if(pt_chrc_proxy == NULL)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}
	pt_dev_proxy = sys_get_connected_proxy( sys_get_mgt());
	if(NULL == pt_dev_proxy)
	{
		DBG_LOG_WARN("***no connected proxy!");
		tpret = false;
		return tpret;
	}
	
	pt_dev_path = l_dbus_proxy_get_path(pt_dev_proxy);
	if(NULL == pt_dev_path)
	{
		DBG_LOG_ERR("fail to get dev-proxy path");
		tpret = false;
		return tpret;
	}
	pt_chrc_path = l_dbus_proxy_get_path( pt_chrc_proxy);
	if(NULL == pt_chrc_path)
	{
		DBG_LOG_ERR("fail to get chrc-proxy path");
		tpret = false;
		return tpret;
	}
	DBG_LOG_INFO("dev-proxy path :%s , chrc-proxy path : %s", pt_dev_path, pt_chrc_path);
	if(strncmp( pt_dev_path, pt_chrc_path, strlen(pt_dev_path)))
	{
		DBG_LOG_ERR("the chrc-proxy does not belong to the connected proxy");
		tpret = false;
		return tpret;
	}
	// they match, to check if the device-proxy still is connected
	if(false == l_dbus_proxy_get_property( pt_dev_proxy, "Connected", "b", &tp_connected))
	{
		DBG_LOG_ERR("fail to get property Connected");
		tpret = false;
		return tpret;
	}
	DBG_LOG_INFO("the property Connected is :%d", tp_connected);
	tpret = tp_connected;
	return tpret;
}


/*to write characteristic 
ret@ST_OK is returned when things go well
*/
app_state_t bt_gatt_charac_write_value(struct l_dbus_proxy*pt_proxy, const uint8_t *ptdata, const uint16_t kplen, struct pthread_cond_var*pt_cond_var)
{

	app_state_t tpret = ST_OK;
	const char*pt_interface = NULL;
	
	struct attr_value *pt_attr_val = NULL;
	
	if((pt_proxy == NULL)||(ptdata == NULL)||(pt_cond_var == NULL))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		return tpret;
	}


	if(kplen >= MAX_ATTR_VAL_LEN)
	{
		DBG_LOG_ERR("too much data %d, the limitation is %s", kplen, MAX_ATTR_VAL_LEN);
		tpret = ST_ERR;
		return tpret;
	}
	if(kplen <= 0)
	{
		DBG_LOG_ERR("No data to write");
		tpret = ST_ERR;
		return tpret;
	}
	
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if(pt_interface == NULL)
	{
		DBG_LOG_ERR("fail to get proxy interface");
		tpret = ST_ERR;
		return tpret;
	}

	if(strcmp(pt_interface, BT_IF_NM_GATTCHARACTERISTIC))
	{
		DBG_LOG_ERR("a proxy with interface %s is expected");
		tpret = ST_ERR;
		return tpret;
	}
	
	if(false == is_chrc_proxy_active( pt_proxy))	
	{
		DBG_LOG_ERR("the chrc-proxy is not active, writting-op is not supported");
		tpret = ST_ERR;
		return tpret;
	}
	else
	{
		DBG_LOG_INFO("the chrc-proxy is active");
	}

	#warning "TODO=== to figure out where do you release memory allocated here!!!"
	pt_attr_val = (struct attr_value*)l_malloc(sizeof(struct attr_value));
	if(pt_attr_val == NULL)
	{
		DBG_LOG_ERR("l_malloc fail");
		tpret = ST_ERR;
		return tpret;
	}

	pt_attr_val->ptbuf = (uint8_t*)l_malloc(kplen);
	if(pt_attr_val->ptbuf == NULL)
	{
		if(pt_attr_val)
		{
			l_free(pt_attr_val);
			pt_attr_val = NULL;
		}
		
		DBG_LOG_ERR("l_malloc fail");
		tpret = ST_ERR;
		return tpret;
	}
	memset( pt_attr_val->ptbuf, 0, kplen);
	memcpy( pt_attr_val->ptbuf, ptdata, kplen);
	pt_attr_val->len = kplen;
	// to set up the cond-var
	pt_attr_val->pt_cond_var = pt_cond_var;
	
	if(0 == l_dbus_proxy_method_call( pt_proxy, "WriteValue", msg_setup_for_charac_write_value, msg_reply_for_charac_write_value, (void*)pt_attr_val, destroy_for_charac_write_value))
	{
		DBG_LOG_ERR("fail to call method WriteValue");
		tpret = ST_ERR;
		return tpret;
	}
	return tpret;
}


#if(1)

/*
typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
to check if the proxy-UUID match the UUID given
ret@true is returned when they match
*/
static bool q_match_for_charac_proxy_uart_rx_v2(const void*pt_data, const const void*pt_dev_path)
{
	bool tpret = false;

	struct l_dbus_proxy*pt_proxy = NULL;//
	char*pt_uuid_buf = NULL;
	const char*pt_interface = NULL;
	const char*pt_chrc_path = NULL;

	//DBG_LOG_INFO("BKP");
	
	if(pt_data == NULL)
	{
		DBG_LOG_ERR("the queue-item data is NULL");
		tpret = false;
		return tpret;
	}

	pt_proxy = *((struct l_dbus_proxy**)pt_data);

	//DBG_LOG_INFO("BKP");
	
	if((pt_dev_path == NULL)||(pt_proxy == NULL))
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = false;
		return tpret;
	}
	//DBG_LOG_INFO("BKP proxy@0x%x", pt_proxy);
	
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if((pt_interface == NULL)||(strcmp( pt_interface, BT_IF_NM_GATTCHARACTERISTIC)))
	{
		DBG_LOG_ERR("the proxy interface validate fail");
		tpret = false;
		return tpret;
	}
	//DBG_LOG_INFO("BKP");
	if(false == l_dbus_proxy_get_property( pt_proxy, "UUID", "s", &pt_uuid_buf))
	{
		DBG_LOG_ERR("fail to get property UUID");
		tpret = false;
		return tpret ;
	}
	//DBG_LOG_INFO("BKP , tp_uuid_buf = 0x%x, %s", pt_uuid_buf, pt_uuid_buf);
	if(strcmp( pt_uuid_buf, (const char*)NORDIC_UART_CHARAC_RX_UUID))
	{	
		//DBG_LOG_ERR("%s vs %s does not match", (const char*)pt_uuid, pt_uuid_buf);
		tpret = false;
		return tpret;
	}

	pt_chrc_path = l_dbus_proxy_get_path( pt_proxy);
	if(NULL == pt_chrc_path)
	{
		DBG_LOG_ERR("fail to chrc-proxy path");
		tpret = false;
		return tpret;
	}

	DBG_LOG_WARN("dev-proxy path %s, chrc-proxy path %s", pt_dev_path, pt_chrc_path);
	if(strncmp( pt_dev_path,  pt_chrc_path, strlen(pt_dev_path)))
	{
		DBG_LOG_ERR("the chrc-proxy does not beloing to the connected-proxy");
		tpret = false;
		return tpret;
	}
	//DBG_LOG_INFO("BKP");
	
	tpret = true;
	return tpret;
}


/*
function to send data to peer-device
*/
app_state_t bt_gatt_charac_write_value_v2( app_management *pt_app_mgt, const uint8_t *ptdata, const uint16_t kplen, struct pthread_cond_var*pt_cond_var)
{
	//#warning "TODO=================to make this function thread-safe!!"
	/*........*/
	app_state_t kpret = ST_OK;
	struct l_dbus_proxy *pt_proxy_for_writting = NULL;
	struct l_dbus_proxy **pt_proxy_buf = NULL;
	struct l_dbus_proxy *pt_dev_proxy = NULL;
	
	uint8_t dev_path_buf[0x100] = {0};
	const uint8_t *pt_dev_path = NULL;
	bool tp_connected = false;

	struct attr_value *pt_attr_val = NULL;
	
	if((NULL == pt_app_mgt)||(NULL == ptdata)||(0 == kplen))
	{
		DBG_LOG_ERR("invalid parameter");
		kpret = ST_ERR;
		goto EXIT;
	}

	if((kplen <= 0)||(kplen >= MAX_ATTR_VAL_LEN))
	{
		DBG_LOG_ERR("invalid dtlength to write");
		kpret = ST_ERR;
		goto EXIT;
	}
	
	// to get the connected-proxy path
	pthread_mutex_lock( &pt_app_mgt->mtx_for_connected_proxy);
	pt_dev_proxy = sys_get_connected_proxy( pt_app_mgt);
	if(NULL == pt_dev_proxy)
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_for_connected_proxy);
		DBG_LOG_ERR("no connected proxy");
		kpret = ST_ERR;
		goto EXIT;			
	}

	if(false == l_dbus_proxy_get_property( pt_dev_proxy, "Connected", "b", &tp_connected))
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_for_connected_proxy);
		DBG_LOG_ERR("fail to get property-Connected");
		kpret = ST_ERR;
		goto EXIT;
	}

	if(false == tp_connected)
	{	
		pthread_mutex_unlock( &pt_app_mgt->mtx_for_connected_proxy);
		DBG_LOG_ERR("no active device-porxy"); // which means no BT-connection now
		kpret = ST_ERR;
		goto EXIT;
	}
	
	pt_dev_path	= l_dbus_proxy_get_path( pt_dev_proxy);
	if(NULL == pt_dev_path)
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_for_connected_proxy);
		DBG_LOG_ERR("invalid dev-path");
		kpret = ST_ERR;
		goto EXIT;
	}
	snprintf( dev_path_buf, sizeof(dev_path_buf), "%s", pt_dev_path);
	pthread_mutex_unlock( &pt_app_mgt->mtx_for_connected_proxy);


	DBG_LOG_WARN("the path of connected proxy %s", dev_path_buf);
	//__________________________________________________________________________________

	// try to find the chrc-proxy for writting
	//MK617U________________________________________________________________
	pthread_mutex_lock( &pt_app_mgt->mtx_q_characteristic);
	if(0 == l_queue_length( pt_app_mgt->pt_q_characteristics))
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		DBG_LOG_ERR("unexpected queue length");
		kpret = ST_ERR;
		goto EXIT;			
	}

	
	pt_proxy_buf = (struct l_dbus_proxy**)l_queue_find( pt_app_mgt->pt_q_characteristics, q_match_for_charac_proxy_uart_rx_v2, (const void *)dev_path_buf);	
	if(NULL == pt_proxy_buf)
	{	
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		DBG_LOG_ERR("fail to find the proxy");
		kpret = ST_ERR;
		goto EXIT;
	}

	pt_proxy_for_writting = *pt_proxy_buf;
	if(NULL == pt_proxy_for_writting)
	{	
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		DBG_LOG_ERR("fail to find the proxy for writting");
		kpret = ST_ERR;
		goto EXIT;
	}

	// now the  chrc-proxy for writing is found , try to write
	#warning "To make sure the memory allocated below is free!"
	pt_attr_val = (struct attr_value*)l_malloc(sizeof(struct attr_value));
	if(NULL == pt_attr_val)
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		DBG_LOG_ERR("l_malloc fail");
		kpret = ST_ERR;
		goto EXIT;
	}
	pt_attr_val->ptbuf = (uint8_t*)l_malloc( kplen);
	if(NULL == pt_attr_val->ptbuf)
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		if(pt_attr_val)
		{
			l_free( pt_attr_val);
			pt_attr_val = NULL;
		}
		
		DBG_LOG_ERR("l_malloc fail");
		kpret = ST_ERR;
		goto EXIT;
	}
	
	memset( pt_attr_val->ptbuf, 0, kplen);
	memcpy( pt_attr_val->ptbuf, ptdata, kplen);
	pt_attr_val->len = kplen;

	// to set the cond_var
	pt_attr_val->pt_cond_var = pt_cond_var; 
	if(0 == l_dbus_proxy_method_call( pt_proxy_for_writting, "WriteValue", msg_setup_for_charac_write_value, msg_reply_for_charac_write_value, (void*)pt_attr_val, destroy_for_charac_write_value))
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);				
		DBG_LOG_ERR("fail to call method-WriteValue");
		#if(0)
		// to release the memory
		l_free(pt_attr_val->ptbuf);
		pt_attr_val->ptbuf = NULL;
		l_free( pt_attr_val);
		pt_attr_val = NULL;
		#endif

		kpret = ST_ERR;
		goto EXIT;
	}
	pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
	//MK617D____________________________________________________________________

	EXIT:
		return kpret;
}
#endif

//
static bool sock_read(struct l_io *io, void *user_data)
{
	struct chrc *chrc = user_data;
	struct msghdr msg;
	struct iovec iov;
	uint8_t buf[MAX_ATTR_VAL_LEN];
	int fd = l_io_get_fd(io);
	ssize_t bytes_read;
msg.msg_iovlen = 1;
			

	if (io != g_notify_io.io && !chrc)
	{
		return true;
	}

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	bytes_read = recvmsg(fd, &msg, MSG_DONTWAIT);
	if (bytes_read < 0) {
		//bt_shell_printf("recvmsg: %s", strerror(errno));
		DBG_LOG_ERR("recvmsg : %s", strerror(errno));
		return false;
	}

	if (!bytes_read)
		return false;

	if (chrc)
	{
		/*
		bt_shell_printf("[" COLORED_CHG "] Attribute %s (%s) "
				"written:\n", chrc->path,
				bt_uuidstr_to_str(chrc->uuid));*/
		DBG_LOG_INFO(" Attribute %s (%s) written:", chrc->path,(chrc->uuid));
	}
	else
	{
		/*bt_shell_printf("[" COLORED_CHG "] %s Notification:\n",
				g_dbus_proxy_get_path(notify_io.proxy));*/
		DBG_LOG_INFO("%s Notiication:", l_dbus_proxy_get_path(g_notify_io.proxy));
	}
	#if(SKF_GW_NEW != 1)
	#if(1) // for debug only
		uint16_t tpu16 = 0;
		DBG_LOG_INFO(" %d read", bytes_read);
		for(tpu16 = 0; tpu16 < bytes_read; tpu16++)
		{
			//DBG_LOG_INFO("buf[%d] = 0x%x", tpu16, buf[tpu16]);
		}
		
		// send the data received to process-general
		int kp_msgid = sys_get_msgid_to_pro_general( sys_get_mgt());
		struct ipc_msg_v2 kp_msg = {0};
		if((kp_msgid >= 0)&&(bytes_read <= MAX_IPC_MSG_PROTO_DATA))
		{
			kp_msg.mtype = M_TYPE_PROTO_DATA;
			memcpy( kp_msg.mtext.proto_pkt.dtbuf, buf, bytes_read);
			kp_msg.mtext.proto_pkt.len = bytes_read; 

			#if(1)
				DELAY_AND_RETRY(msgsnd( kp_msgid, &kp_msg,sizeof(union msg_load), IPC_NOWAIT), 
				500000, 
				100,
				if(EAGAIN == errno)
				{
					DBG_LOG_WARN("Failed to send data to process-general, for %s, and retry", strerror(errno));
				}
				else
				{
					DBG_LOG_ERR("Failed to send data to process-general, for %s, and give it up", strerror(errno));
					return false;
				},
				DBG_LOG_ERR("Failed to send data to process-general, for %s, and give it up", strerror(errno));return false;,
				);
			#else
				app_com_send_ipc_msg( app_com_get_ipc_msg_sender_for_ble_to_gen(), (void *)&kp_msg, sizeof(union msg_load));
			#endif
		}
		else
		{
			DBG_LOG_ERR("TODO===to fix this");
		}
	#endif
	
	#else//_____________________
		// Nothing different, we just use ipc_msg_v3
		#if(1) // for debug only
			uint16_t tpu16 = 0;
			DBG_LOG_INFO(" %d read", bytes_read);
			for(tpu16 = 0; tpu16 < bytes_read; tpu16++)
			{
				//DBG_LOG_INFO("buf[%d] = 0x%x", tpu16, buf[tpu16]);
			}
			
			// send the data received to process-general
			int kp_msgid = sys_get_msgid_to_pro_general( sys_get_mgt());
			struct ipc_msg_v3 kp_msg = {0};
			if((kp_msgid >= 0)&&(bytes_read <= MAX_IPC_MSG_PROTO_DATA))
			{
				kp_msg.type = M_TYPE_PROTO_DATA;
				memcpy( kp_msg.payload.proto_pkt.dtbuf, buf, bytes_read);
				kp_msg.payload.proto_pkt.len = bytes_read; 
				pthread_mutex_lock(&(sys_get_mgt()->mtx_adv_scanning_ctr));
  				l_queue_push_tail(sys_get_mgt()->pt_adv_scanning_event_queue,
                   (void *)BT_EVENT_DO_NOTHING);
  				pthread_cond_signal(&(sys_get_mgt()->cond_val_adv_scanning_ctr));
  				pthread_mutex_unlock(&(sys_get_mgt()->mtx_adv_scanning_ctr));

			#if(1)
					DELAY_AND_RETRY(msgsnd( kp_msgid, &kp_msg,sizeof(union msg_payload), IPC_NOWAIT), 
					500000, 
					100,
					if(EAGAIN == errno)
					{
						DBG_LOG_WARN("Failed to send data to process-general, for %s, and retry", strerror(errno));
					}
					else
					{
						DBG_LOG_ERR("Failed to send data to process-general, for %s, and give it up", strerror(errno));
						return false;
					},
					DBG_LOG_ERR("Failed to send data to process-general, for %s, and give it up", strerror(errno));return false;,
					);
			#else
					app_com_send_ipc_msg( app_com_get_ipc_msg_sender_for_ble_to_gen(), (void *)&kp_msg, sizeof(union msg_load));
			#endif
			}
			else
			{
				DBG_LOG_ERR("TODO===to fix this");
			}
		#endif
	
	#endif

	return true;
}

static void notify_io_destroy(void)
{
	l_io_destroy(g_notify_io.io);
	memset(&g_notify_io, 0, sizeof(g_notify_io));
}

static void write_io_destroy(void)
{
	l_io_destroy(g_write_io.io);
	memset(&g_write_io, 0, sizeof(g_write_io));
}


/*typedef void (*l_io_disconnect_cb_t) (struct l_io *io, void *user_data);
*/
static void sock_hup(struct l_io *io, void *user_data)
{
	struct chrc *chrc = user_data;

	if (chrc) {
		#if(0)
		bt_shell_printf("Attribute %s %s sock closed\n", chrc->path,
				io == chrc->write_io ? "Write" : "Notify");
		#else
			DBG_LOG_INFO("Attribute %s %s sock closed fd %d, io@0x%x\n", chrc->path,io == chrc->write_io ? "Write" : "Notify", l_io_get_fd(io), io);
		#endif
		
		if (io == chrc->write_io) {
			l_io_destroy(chrc->write_io);
			chrc->write_io = NULL;
		} else {
			l_io_destroy(chrc->notify_io);
			chrc->notify_io = NULL;
		}

		return ;
	}

	//bt_shell_printf("%s closed\n", io == notify_io.io ? "Notify" : "Write");
	DBG_LOG_INFO("%s closed io=0x%x, notify_io.io =0x%x\n", io == g_notify_io.io ? "Notify" : "Write", io, g_notify_io.io);

	if (io == g_notify_io.io)
	{
		//notify_io_destroy();
		#warning "TODO=====Double check when things go wrong"
	}
	else
		write_io_destroy();

	return ;
}


//
struct l_io *sock_io_new(int fd, void *user_data)
{
	struct l_io *io;

	io = l_io_new(fd);

	l_io_set_close_on_destroy(io, true);
	
	l_io_set_read_handler(io, sock_read, user_data, NULL);

	l_io_set_disconnect_handler(io, sock_hup, user_data, NULL);

	DBG_LOG_INFO("g_notify_io @ %p", &g_notify_io);
	DBG_LOG_INFO("io @ %p", io);
	return io;
}

/*
*/
#if(1)

/*
typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
to check if the proxy-UUID match the UUID given
ret@true is returned when they match
*/
static bool q_match_for_charac_proxy_uart_tx_v2(const void*pt_data, const const void*pt_dev_path)
{
	bool tpret = false;

	struct l_dbus_proxy*pt_proxy = NULL;//
	char*pt_uuid_buf = NULL;
	const char*pt_interface = NULL;
	const char*pt_chrc_path = NULL;

	//DBG_LOG_INFO("BKP");
	
	if(pt_data == NULL)
	{
		DBG_LOG_ERR("the queue-item data is NULL");
		tpret = false;
		return tpret;
	}

	pt_proxy = *((struct l_dbus_proxy**)pt_data);

	//DBG_LOG_INFO("BKP");
	
	if((pt_dev_path == NULL)||(pt_proxy == NULL))
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = false;
		return tpret;
	}
	//DBG_LOG_INFO("BKP proxy@0x%x", pt_proxy);
	
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if((pt_interface == NULL)||(strcmp( pt_interface, BT_IF_NM_GATTCHARACTERISTIC)))
	{
		DBG_LOG_ERR("the proxy interface validate fail");
		tpret = false;
		return tpret;
	}
	//DBG_LOG_INFO("BKP");
	if(false == l_dbus_proxy_get_property( pt_proxy, "UUID", "s", &pt_uuid_buf))
	{
		DBG_LOG_ERR("fail to get property UUID");
		tpret = false;
		return tpret ;
	}
	//DBG_LOG_INFO("BKP , tp_uuid_buf = 0x%x, %s", pt_uuid_buf, pt_uuid_buf);
	if(strcmp( pt_uuid_buf, (const char*)NORDIC_UART_CHARAC_TX_UUID))
	{	
		//DBG_LOG_ERR("%s vs %s does not match", (const char*)pt_uuid, pt_uuid_buf);
		tpret = false;
		return tpret;
	}

	pt_chrc_path = l_dbus_proxy_get_path( pt_proxy);
	if(NULL == pt_chrc_path)
	{
		DBG_LOG_ERR("fail to chrc-proxy path");
		tpret = false;
		return tpret;
	}

	DBG_LOG_WARN("dev-proxy path %s, chrc-proxy path %s", pt_dev_path, pt_chrc_path);
	if(strncmp( pt_dev_path,  pt_chrc_path, strlen(pt_dev_path)))
	{
		DBG_LOG_ERR("the chrc-proxy does not beloing to the connected-proxy");
		tpret = false;
		return tpret;
	}
	//DBG_LOG_INFO("BKP");
	
	tpret = true;
	return tpret;
}

/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);
*/
static void msg_setup_for_charac_acquire_notify_v2(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder*pt_builder = NULL;
	pt_builder = l_dbus_message_builder_new( message);

	l_dbus_message_builder_enter_array( pt_builder, "{sv}");

	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	
	l_dbus_message_builder_leave_dict( pt_builder);
	
	l_dbus_message_builder_leave_array( pt_builder);

	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);
	return;
}

/*typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);
*/
static void msg_reply_for_charac_acquire_notify_v2(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	int kp_fd = -1;
	uint16_t kp_mtu = 0;

	struct pthread_cond_var *pt_cond_var = (struct pthread_cond_var*)user_data;
	
	if(l_dbus_message_is_error(result))
	{
		const char*pt_name = NULL, *pt_text = NULL;
		if(l_dbus_message_get_error( result, &pt_name, &pt_text))
		{
			DBG_LOG_ERR("Acquire Notify fail, err_info %s, %s", pt_name, pt_text);
		}
		else
		{
			DBG_LOG_ERR("Acquire Notify fail, unknown reason");
		}
		//return;
		goto EXIT;
	}
	if(g_notify_io.io)
	{
		l_io_destroy(g_notify_io.io);
		g_notify_io.io = NULL;
	}
	g_notify_io.mtu = 0;

	if(l_dbus_message_get_arguments( result, "hq",&kp_fd, &kp_mtu) == false)
	{
		DBG_LOG_ERR("Invalid AcquireNotify response");
		return;
	}

	g_notify_io.mtu = kp_mtu;
	g_notify_io.io = sock_io_new( kp_fd, NULL);

	DBG_LOG_INFO("AcquireNotify goes well fd %d, mtu %d", kp_fd, kp_mtu);

	EXIT:
		if(pt_cond_var)
		{// to signal the receivation of reply
			pthread_mutex_lock( &pt_cond_var->mtx);
			pt_cond_var->is_op_done = true;
			pthread_cond_signal( &pt_cond_var->cond_var);
			pthread_mutex_unlock( &pt_cond_var->mtx);
		}
		return;
}


/*to acquite notification for the specific characteristic
ret@ST_OK is returned when things go well

to make characteristic-queue operation thread-safe
NOTE !! this funciton will be blocked to wait for OP to be done
*/
app_state_t bt_gatt_acquire_notify_v2(app_management*pt_app_mgt)
{
	app_state_t tpret = ST_OK;

	struct l_dbus_proxy*pt_dev_proxy = NULL;

	uint8_t dev_path_buf[0x100] = {0};
	const char*pt_dev_path = NULL;
	bool flg_connected = false;

	struct l_dbus_proxy *pt_proxy_for_notification = NULL;
	struct l_dbus_proxy **pt_proxy_buf = NULL;
	bool flg_is_notifyacquired = false;

	struct pthread_cond_var kp_cond_var;
	struct timespec tp_tspec;
	int32_t tpint = 0;

	if((NULL == pt_app_mgt))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		goto EXIT;
	}

	pthread_mutex_lock( &pt_app_mgt->mtx_for_connected_proxy);
	pt_dev_proxy = sys_get_connected_proxy( pt_app_mgt);
	if(NULL == pt_dev_proxy)
	{		
		pthread_mutex_unlock( &pt_app_mgt->mtx_for_connected_proxy);
		DBG_LOG_ERR("no valid dev-proxy is connected");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(false == l_dbus_proxy_get_property( pt_dev_proxy, "Connected", "b", &flg_connected))
	{	
		pthread_mutex_unlock( &pt_app_mgt->mtx_for_connected_proxy);
		DBG_LOG_ERR("fail to get property-Connected");
		tpret = ST_ERR;
		goto EXIT;
	}
	if(false == flg_connected)
	{	
		pthread_mutex_unlock( &pt_app_mgt->mtx_for_connected_proxy);
		DBG_LOG_ERR("the dev-proxy is inactive");
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_dev_path = l_dbus_proxy_get_path( pt_dev_proxy);
	if(NULL == pt_dev_path)
	{	
		pthread_mutex_unlock( &pt_app_mgt->mtx_for_connected_proxy);
		DBG_LOG_ERR("invalid proxy path");
		tpret = ST_ERR;
		goto EXIT;
	}
	snprintf( dev_path_buf, sizeof(dev_path_buf), "%s", pt_dev_path);
	pthread_mutex_unlock( &pt_app_mgt->mtx_for_connected_proxy);

	DBG_LOG_ERR("the path of connected proxy is %s", dev_path_buf);

	//_______________________________________________________________________
	// to find the proxy then enable notification
	//MK1028U==================================
	pthread_mutex_lock( &pt_app_mgt->mtx_q_characteristic);
	if(0 == l_queue_length( pt_app_mgt->pt_q_characteristics))
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		DBG_LOG_ERR("unexpected queue length");
		tpret = ST_ERR;
		goto EXIT;
	}

	pt_proxy_buf = (struct l_dbus_proxy**)l_queue_find( pt_app_mgt->pt_q_characteristics, q_match_for_charac_proxy_uart_tx_v2, (const void *)dev_path_buf);
	if(NULL == pt_proxy_buf)
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		DBG_LOG_ERR("fail to find the proxy %s", NORDIC_UART_CHARAC_TX_UUID);
		tpret = ST_ERR;
		goto EXIT;
	}
	pt_proxy_for_notification = *pt_proxy_buf;

	if(NULL == pt_proxy_for_notification)
	{		
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		DBG_LOG_ERR("invalid charac-proxy");
		tpret = ST_ERR;
		goto EXIT;
	}

	if(false == l_dbus_proxy_get_property( pt_proxy_for_notification, "NotifyAcquired", "b", &flg_is_notifyacquired))
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		DBG_LOG_ERR("fail to get property NotifyAcquired");
		tpret = ST_ERR;
		goto EXIT;
	}

	if(flg_is_notifyacquired)
	{	
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		DBG_LOG_WARN("the property NotifyAcquired is set");
		tpret = ST_OK;
		goto EXIT;
	}

	//__continue_to_acquire notification________________________________	
	app_com_init_pthread_cond_var_ctr( &kp_cond_var);
	kp_cond_var.is_op_done = false;
	if(0 == l_dbus_proxy_method_call( pt_proxy_for_notification, "AcquireNotify", msg_setup_for_charac_acquire_notify_v2, msg_reply_for_charac_acquire_notify_v2, (void *)&kp_cond_var, NULL))
	{
		pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);
		tpret = ST_ERR;
		
		app_com_deinit_pthread_cond_var_ctr( &kp_cond_var);
		goto EXIT;
	}
	pthread_mutex_unlock( &pt_app_mgt->mtx_q_characteristic);


	DBG_LOG_INFO("to wait for reply for AcquireNotify");

	// to wait for the acquiring-notification to be done
	pthread_mutex_lock( &kp_cond_var.mtx);
	clock_gettime(CLOCK_REALTIME, &tp_tspec);
	tp_tspec.tv_sec = tp_tspec.tv_sec + 3;
	while(1)
	{
		if(false == kp_cond_var.is_op_done)
		{
			tpint = pthread_cond_timedwait( &kp_cond_var.cond_var, &kp_cond_var.mtx, &tp_tspec);
			if((0 != tpint)&&(ETIMEDOUT == tpint))
			{
				DBG_LOG_ERR("===fail to acquire-notification, for timeout");
				break;
			}
		}
		else
		{
			DBG_LOG_WARN("acquirint-notification is done successfully");
			g_notify_io.proxy = pt_proxy_for_notification;
			
			break;
		}
	}
	pthread_mutex_unlock( &kp_cond_var.mtx);
	
	app_com_deinit_pthread_cond_var_ctr( &kp_cond_var);
	//MK1028D==================================

	EXIT:
		return tpret;
}

#endif



/*to release notification
ret@ST_OK is returned when things go well
*/
app_state_t bt_gatt_release_notify(struct l_dbus_proxy*pt_proxy)
{
	app_state_t tpret = ST_OK;
	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = ST_ERR;
		return tpret;
	}
	if((pt_proxy != g_notify_io.proxy)||(!g_notify_io.io))
	{
		DBG_LOG_ERR("Notification not required for the proxy %s", l_dbus_proxy_get_path( pt_proxy));
		tpret = ST_ERR;
		return tpret;
	}
	notify_io_destroy();

	return tpret;
}

static bool attr_authorization_flag_exists(char **flags)
{
	int i;

	for (i = 0; flags[i]; i++) {
		if (!strcmp("authorize", flags[i]))
			return true;
	}

	return false;
}


static void chrc_free(void *data)
{
	struct chrc *chrc = data;
	#if(0)
	g_list_free_full(chrc->descs, desc_unregister);
	g_free(chrc->path);
	g_free(chrc->uuid);
	g_strfreev(chrc->flags);
	g_free(chrc->value);
	g_free(chrc);
	#
	#else 
	// TODO
		l_queue_destroy( chrc->descs, l_free);
		l_free(chrc->path);
		l_free(chrc->uuid);
		l_strfreev(chrc->flags);
		l_free(chrc->value);
		l_free(chrc);
	#endif
}


static void
print_chrc(
  struct chrc *chrc,
  void *user_data)
{
  DBG_LOG_DEBUG("Characteristic path (handle 0x%04x) %s, %s", 
  				chrc->handle,
                chrc->path, 
				chrc->uuid);
}




#if(0)
static int parse_options(DBusMessageIter *iter, uint16_t *offset, uint16_t *mtu,
						char **device, char **link,
						bool *prep_authorize)
{
	DBusMessageIter dict;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return -EINVAL;

	dbus_message_iter_recurse(iter, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		const char *key;
		DBusMessageIter value, entry;
		int var;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		var = dbus_message_iter_get_arg_type(&value);
		if (strcasecmp(key, "offset") == 0) {
			if (var != DBUS_TYPE_UINT16)
				return -EINVAL;
			if (offset)
				dbus_message_iter_get_basic(&value, offset);
		} else if (strcasecmp(key, "MTU") == 0) {
			if (var != DBUS_TYPE_UINT16)
				return -EINVAL;
			if (mtu)
				dbus_message_iter_get_basic(&value, mtu);
		} else if (strcasecmp(key, "device") == 0) {
			if (var != DBUS_TYPE_OBJECT_PATH)
				return -EINVAL;
			if (device)
				dbus_message_iter_get_basic(&value, device);
		} else if (strcasecmp(key, "link") == 0) {
			if (var != DBUS_TYPE_STRING)
				return -EINVAL;
			if (link)
				dbus_message_iter_get_basic(&value, link);
		} else if (strcasecmp(key, "prepare-authorize") == 0) {
			if (var != DBUS_TYPE_BOOLEAN)
				return -EINVAL;
			if (prep_authorize) {
				int tmp;

				dbus_message_iter_get_basic(&value, &tmp);
				*prep_authorize = !!tmp;
			}
		}

		dbus_message_iter_next(&dict);
	}

	return 0;
}

#else
static int parse_options(struct l_dbus_message *message, uint16_t *offset, uint16_t *mtu,char **device, char **link,bool *prep_authorize)
{
	struct l_dbus_message_iter kpiter = {0}, tpiter = {0};
	char*pt_str = NULL;
	if(false == l_dbus_message_get_arguments( message, "a{sv}", &kpiter))	
	{
		DBG_LOG_ERR("fail to get arguments with type a{sv}");
		return -1;
	}
	while(l_dbus_message_iter_next_entry( &kpiter, &pt_str, &tpiter))
	{
		DBG_LOG_INFO("the key-str is : %s", pt_str);
		if(0 == strcmp( pt_str, "offset"))
		{
			if(offset)
			{
				l_dbus_message_iter_get_variant( &tpiter, "q", offset);
				DBG_LOG_INFO("offset %d", *offset);
			}
		}
		else if(0 == strcmp( pt_str, "mtu"))
		{
			if(mtu)
			{
				l_dbus_message_iter_get_variant( &tpiter, "q", mtu);
				DBG_LOG_INFO("mtu %d", *mtu);
			}
		}
		else if(0 == strcmp(pt_str, "device"))
		{
			if(device)
			{
				l_dbus_message_iter_get_variant( &tpiter, "o", device);
				DBG_LOG_INFO("device %s", *device);
			}
		}
		else if(0 == strcmp(pt_str, "link"))
		{
			if(link)
			{
				l_dbus_message_iter_get_variant( &tpiter, "s", link);
				DBG_LOG_INFO("link %s", *link);
			}
		}
		else if(0 == strcmp( pt_str, "prepare-authorize"))
		{
			if(prep_authorize)
			{
				l_dbus_message_iter_get_variant( &tpiter, "b", prep_authorize);
				DBG_LOG_INFO("prepare-authorize %d", *prep_authorize);
			}
		}
		else
		{
			DBG_LOG_WARN("unknown key-str %s", pt_str);
		}
	}
	
	return 0;
}


static int parse_options_for_chrc_write_value(struct l_dbus_message *message, uint16_t *offset, uint16_t *mtu,char **device, char **link,bool *prep_authorize)
{
	struct l_dbus_message_iter kpiter = {0}, tpiter = {0};
	char*pt_str = NULL;
	if(false == l_dbus_message_get_arguments( message, "aya{sv}", &kpiter))	
	{
		DBG_LOG_ERR("fail to get arguments with type a{sv}");
		return -1;
	}
	while(l_dbus_message_iter_next_entry( &kpiter, &pt_str, &tpiter))
	{
		DBG_LOG_INFO("the key-str is : %s", pt_str);
		if(0 == strcmp( pt_str, "offset"))
		{
			if(offset)
			{
				l_dbus_message_iter_get_variant( &tpiter, "q", offset);
				DBG_LOG_INFO("offset %d", *offset);
			}
		}
		else if(0 == strcmp( pt_str, "mtu"))
		{
			if(mtu)
			{
				l_dbus_message_iter_get_variant( &tpiter, "q", mtu);
				DBG_LOG_INFO("mtu %d", *mtu);
			}
		}
		else if(0 == strcmp(pt_str, "device"))
		{
			if(device)
			{
				l_dbus_message_iter_get_variant( &tpiter, "o", device);
				DBG_LOG_INFO("device %s", *device);
			}
		}
		else if(0 == strcmp(pt_str, "link"))
		{
			if(link)
			{
				l_dbus_message_iter_get_variant( &tpiter, "s", link);
				DBG_LOG_INFO("link %s", *link);
			}
		}
		else if(0 == strcmp( pt_str, "prepare-authorize"))
		{
			if(prep_authorize)
			{
				l_dbus_message_iter_get_variant( &tpiter, "b", prep_authorize);
				DBG_LOG_INFO("prepare-authorize %d", *prep_authorize);
			}
		}
		else
		{
			DBG_LOG_WARN("unknown key-str %s", pt_str);
		}
	}
	
	return 0;
}



#endif



#if(0) // the original
static DBusMessage *create_sock(struct chrc *chrc, DBusMessage *msg)
{
	int fds[2];
	struct io *io;
	bool dir;
	DBusMessage *reply;

	if (socketpair(AF_LOCAL, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC,
								0, fds) < 0)
		return g_dbus_create_error(msg, "org.bluez.Error.Failed", "%s",
							strerror(errno));

	dir = dbus_message_has_member(msg, "AcquireWrite");

	io = sock_io_new(fds[!dir], chrc);
	if (!io) {
		close(fds[0]);
		close(fds[1]);
		return g_dbus_create_error(msg, "org.bluez.Error.Failed", "%s",
							strerror(errno));
	}

	reply = g_dbus_create_reply(msg, DBUS_TYPE_UNIX_FD, &fds[dir],
					DBUS_TYPE_UINT16, &chrc->mtu,
					DBUS_TYPE_INVALID);

	close(fds[dir]);

	if (dir)
		chrc->write_io = io;
	else
		chrc->notify_io = io;

	bt_shell_printf("[" COLORED_CHG "] Attribute %s %s sock acquired\n",
					chrc->path, dir ? "Write" : "Notify");

	return reply;
}

#else
/*
*/
static struct l_dbus_message *create_sock(struct chrc *chrc, struct l_dbus_message *msg)
{
	int fds[2];
	struct l_io *io;
	bool dir;
	char*pt_member = NULL;
	
	struct l_dbus_message *reply;

	if (socketpair(AF_LOCAL, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC,0, fds) < 0)
	{
		//return g_dbus_create_error(msg, "org.bluez.Error.Failed", "%s",strerror(errno));
		DBG_LOG_ERR("socketpair fail : %s", strerror(errno));
		return l_dbus_message_new_error( msg, "org.freedesktop.DBus.Error",  "SocketPair", "Fail");						
	}

	
	//dir = dbus_message_has_member(msg, "AcquireWrite");
	pt_member = l_dbus_message_get_member(msg);
	DBG_LOG_INFO("the member str : %s", pt_member);
	if(0 == strcmp( pt_member, "AcquireWrite"))
	{
		dir = true;
	}
	else
	{
		dir = false;
	}

	
	io = sock_io_new(fds[!dir], chrc);
	if (!io) {
		close(fds[0]);
		close(fds[1]);
		//return g_dbus_create_error(msg, "org.bluez.Error.Failed", "%s",strerror(errno));	
		return l_dbus_message_new_error( msg, "org.bluez.Error.Failed",  "sock_io_new", "Fail"); 					
		
	}

	DBG_LOG_INFO("======BKP, the dir %d, !dir %d, fd[x] %d, fd[x] %d", dir, !dir,fds[dir], fds[!dir]);
	
	//reply = g_dbus_create_reply(msg, DBUS_TYPE_UNIX_FD, &fds[dir],DBUS_TYPE_UINT16, &chrc->mtu,DBUS_TYPE_INVALID);
	reply = l_dbus_message_new_method_return(msg);
	struct l_dbus_message_builder *pt_builder = l_dbus_message_builder_new(reply);	
	l_dbus_message_builder_append_basic( pt_builder, 'h', &fds[dir]);
	l_dbus_message_builder_append_basic( pt_builder, 'q', &chrc->mtu);
	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);

	DBG_LOG_INFO("=======BKP");

	#if(1)
		close(fds[dir]);
	#endif

	if (dir)
	{
		chrc->write_io = io;
	}
	else
	{
		chrc->notify_io = io;
	}
	//bt_shell_printf("[" COLORED_CHG "] Attribute %s %s sock acquired\n",chrc->path, dir ? "Write" : "Notify");
	DBG_LOG_INFO("Attribue %s  %s sock acquired fd %d , io@0x%x", chrc->path, dir ? "Write":"Notify", l_io_get_fd( io), io);
	
	return reply;
}

#endif


#if (SKF_GW_NEW == 1)
#include "bt_gatt_new.h"
#include "global.h"

// typedef
// struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
// struct l_dbus_message *message, void *user_data);
// Callback to be called when reading value (used in BLE server)
static struct l_dbus_message *
chrc_method_cb_read_value(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  void *user_data)
{
  struct l_dbus_message *_pt_reply = NULL;
  struct l_dbus_message_iter _kp_obj = { 0 };
  struct l_dbus_message_builder *_pt_builder = NULL;

  struct chrc *_pt_chrc = NULL;
  uint32_t _kplen = 0;
  uint8_t *_ptu8 = NULL;

  if((pt_dbus == NULL) || (message == NULL) || (user_data == NULL))
  {
    return l_dbus_message_new_error(message,
                                    "org.freedesktop.DBus.Error",
                                    "InvalidArgs",
                                    "NULL pointer");
  }
  if(!l_dbus_message_get_arguments(message, "a{sv}", &_kp_obj))
  {
    DBG_LOG_ERR("Invalid arguments");
    return l_dbus_message_new_error(message,
                                    "org.freedesktop.DBus.Error",
                                    "InvalidArgs",
                                    "Invalid arguments");
  }

  _pt_reply = l_dbus_message_new_method_return(message);
  _pt_builder = l_dbus_message_builder_new(_pt_reply);

  l_dbus_message_builder_enter_array(_pt_builder, "y");

  // Read data
  _pt_chrc = (struct chrc *)user_data;
  if(0 == strcmp(_pt_chrc->uuid, BLE_MANUFACTURER_NM_CHR_UUID))
  {
    // Manufacturer
    _kplen = strlen("SKF");
    _ptu8 = "SKF";
    for(uint32_t i = 0; i < _kplen; i++)
    {
      l_dbus_message_builder_append_basic(_pt_builder, 'y',
                                          (const void *)(_ptu8 + i));
    }
  }
  else if(0 == strcmp(_pt_chrc->uuid, BLE_HARDWARE_VERSION_CHR_UUID))
  {
    // Hardware version
    _kplen = strlen(CONFIG_HARDWARE_DEFAULT_VERSION);
    _ptu8 = CONFIG_HARDWARE_DEFAULT_VERSION;
    for(uint32_t i = 0; i < _kplen; i++)
    {
      l_dbus_message_builder_append_basic(_pt_builder, 'y',
                                          (const void *)(_ptu8 + i));
    }
  }
  else if(0 == strcmp(_pt_chrc->uuid, BLE_FIRMWARE_VERSION_CHR_UUID))
  {
    // Firmware version
    _kplen = strlen(CONFIG_APPLICATION_VERSION);
    _ptu8 = CONFIG_APPLICATION_VERSION;
    for(uint32_t i = 0; i < _kplen; i++)
    {
      l_dbus_message_builder_append_basic(_pt_builder, 'y',
                                          (const void *)(_ptu8 + i));
    }
  }
  else
  {
    _ptu8 = "Unknown";
    _kplen = strlen("Unknown");

    for(uint32_t i = 0; i < _kplen; i++)
    {
      l_dbus_message_builder_append_basic(_pt_builder, 'y',
                                          (const void *)(_ptu8 + i));
    }
  }

  l_dbus_message_builder_leave_array(_pt_builder);

  l_dbus_message_builder_finalize(_pt_builder);
  l_dbus_message_builder_destroy(_pt_builder);

  return _pt_reply;
}
#else // the original

/*typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data);
*/
static struct l_dbus_message *chrc_method_cb_read_value(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message * pt_reply = NULL ;
	struct l_dbus_message_iter kp_obj = {0};
	struct l_dbus_message_builder *pt_builder = NULL;
	
	if((pt_dbus == NULL)||(message == NULL)||(user_data == NULL))
	{
		
		return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "NULL pointer");
	}

	if(!l_dbus_message_get_arguments( message, "a{sv}", &kp_obj))
	{
		DBG_LOG_ERR("Invalid arguments");
		return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "Invalid arguments");
	}

	pt_reply = l_dbus_message_new_method_return( message);
	pt_builder = l_dbus_message_builder_new(pt_reply);
	l_dbus_message_builder_enter_array( pt_builder, "y");

	#warning "TODO========to read the real data"
	uint8_t tpu8 = 'A';
	l_dbus_message_builder_append_basic( pt_builder, 'y', (const void *)&tpu8);
	l_dbus_message_builder_leave_array(pt_builder);

	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);

	return pt_reply;
}
#endif





#if(0)
static int parse_value_arg(DBusMessageIter *iter, uint8_t **value, int *len)
{
	DBusMessageIter array;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return -EINVAL;

	dbus_message_iter_recurse(iter, &array);
	dbus_message_iter_get_fixed_array(&array, value, len);

	return 0;
}

#else
static int parse_value_arg(struct l_dbus_message*message, uint8_t **value, int *len)
{
	struct l_dbus_message_iter kpiter = {0};
	struct l_dbus_message_iter kp_iter_options = {0};
	if(false == l_dbus_message_get_arguments( message, "aya{sv}", &kpiter, &kp_iter_options))
	{
		DBG_LOG_ERR("fail to get arguments type-a");
		return -1;
	}

	if(false == l_dbus_message_iter_get_fixed_array( &kpiter, value, len))
	{
		DBG_LOG_ERR("fail to get fixed-array");
		return -1;
	}
	DBG_LOG_INFO("len %d", *len);
	
	return 0;
}

#endif


// typedef
// struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
// struct l_dbus_message *message, void *user_data);
// Callback to be called when writing value (used in BLE server)
static struct l_dbus_message *
chrc_method_cb_write_value(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  void *user_data)
{
  struct chrc *_pt_chrc = (struct chrc *)user_data;
  uint16_t _kp_offset = 0;
  bool _kp_pre_authorize = false;
  char *_pt_dev = NULL, *_pt_link = NULL;

  int _kp_len = 0;

  uint8_t *_pt_value = NULL;
  uint8_t *_pt_dtbuf = NULL;

  if(parse_value_arg(message, &_pt_value, &_kp_len))
  {
    DBG_LOG_ERR("Failed to parse the written value");
    return l_dbus_message_new_error(message,
                                    "org.bluez.Error.InvalidArguments",
                                    "ToFix");
  }

#if (0)
  if(parse_options_for_chrc_write_value(message, &_kp_offset, NULL,
                                        &_pt_dev,
                                        &_pt_link, &_kp_pre_authorize))
  {
    DBG_LOG_INFO("fail to parse options");
    return l_dbus_message_new_error(message,
                                    "org.bluez.Error.InvalidArguments",
                                    "ToFixs");
  }
  DBG_LOG_DEBUG("%s WriteValue offset %d link %s",
                _pt_chrc->path,
                _kp_offset,
                _pt_link);
#else
  DBG_LOG_WARN(
    "Here parse_options_for_chrc_write_value is commented since there exists a segment fault if the function is called (this would be fixed later)!");
#endif

  // Dump the data for debug
  util_dbg_buf_dump(_pt_value, _kp_len);

  if(_kp_len)
  {
    _pt_dtbuf = (uint8_t *)l_malloc(_kp_len);
    if(NULL == _pt_dtbuf)
    {
      return l_dbus_message_new_error(message,
                                      "org.bluez.Error",
                                      "l_malloc fail");
    }
    memset(_pt_dtbuf, 0, _kp_len);
    memcpy(_pt_dtbuf, _pt_value, _kp_len);
    if(_pt_chrc->value)
    {
      l_free(_pt_chrc->value);
    }
    _pt_chrc->value = _pt_dtbuf;
    _pt_chrc->value_len = _kp_len;
    _pt_chrc->max_val_len = _kp_len;
  }

  DBG_LOG_DEBUG(
    "chrc-info @ 0x%p, path %s, uuid %s, value_buf @ 0x%p, value_len %d",
    _pt_chrc, _pt_chrc->path, _pt_chrc->uuid, _pt_chrc->value,
    _pt_chrc->value_len);

#if (SKF_GW_NEW == 1)
  int32_t _kp_msgid = sys_get_msgid_to_pro_general(sys_get_mgt());
  struct ipc_msg_v3 _kp_msg = { 0 };

  if(0 == strcmp(_pt_chrc->uuid, NORDIC_UART_CHARAC_RX_UUID))
  {
    if((_pt_chrc->value_len && (_pt_chrc->value_len < MAX_IPC_MSG_BUF)))
    {
      _kp_msg.type = M_TYPE_PROTO_DATA;
      memcpy(_kp_msg.payload.proto_pkt.dtbuf, _pt_chrc->value,
             _pt_chrc->value_len);
      _kp_msg.payload.proto_pkt.len = _pt_chrc->value_len;
      if(0 != msgsnd(_kp_msgid, &_kp_msg, sizeof(union msg_payload),
                     IPC_NOWAIT))
      {
        DBG_LOG_ERR("Failed to send msg to the process general");
      }
    }
  }
#else
  // send the data received to process-general
  int _kp_msgid = sys_get_msgid_to_pro_general(sys_get_mgt());
  struct ipc_msg_v2 _kp_msg = { 0 };

  if(0 == strcmp(_pt_chrc->uuid, NORDIC_UART_CHARAC_RX_UUID))
  {
    if(_pt_chrc->value_len && (_pt_chrc->value_len < MAX_IPC_MSG_BUF))
    {
      _kp_msg.mtype = M_TYPE_PROTO_DATA;
      memcpy(_kp_msg.mtext.proto_pkt.dtbuf, _pt_chrc->value,
             _pt_chrc->value_len);
      _kp_msg.mtext.proto_pkt.len = _pt_chrc->value_len;

      DELAY_AND_RETRY(
        msgsnd(_kp_msgid, &_kp_msg, sizeof(union msg_load),
               IPC_NOWAIT),
        500000,
        100,
        if(EAGAIN == errno)
		{
			DBG_LOG_WARN("Failed to send message to %d, for %s, and retry",
						_kp_msgid, strerror(errno));
		}
			else
		{
			perror("msgsnd-err");
			DBG_LOG_ERR("Failed to send message to %d", _kp_msgid);
			break;
		},
        perror("msgsnd-err");
        DBG_LOG_ERR("Failed to send message to %d",
                    _kp_msgid);
        ,
        );
    }
    else
    {
      DBG_LOG_WARN("Invalid value length (%d)", _pt_chrc->value_len);
    }
  }
#endif /* SKF_GW_NEW == 1 */

  l_dbus_property_changed(pt_dbus, 
  						  _pt_chrc->path,
                          "org.bluez.GattCharacteristic1",
						  "Value");
  return l_dbus_message_new_method_return(message);
}





/*typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data);
*/
static struct l_dbus_message *chrc_method_cb_acquire_write (struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	#if(0)	
	DBG_LOG_ERR("TODO===to complete this function");	
	return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "TODO");
	#else
		struct chrc*pt_chrc = (struct chrc*)user_data;
		struct l_dbus_message_iter kp_iter = {0};
		struct l_dbus_message *reply = NULL;

		char* pt_dev = NULL, *pt_link = NULL;

		if(pt_chrc->write_io)
		{
			DBG_LOG_ERR("NotPermitted");
			return l_dbus_message_new_error( message, "org.bluez.Error", "NotPermitted");
		}

		if(parse_options( message, NULL, &pt_chrc->mtu, &pt_dev, &pt_link, NULL))
		{
			DBG_LOG_ERR("parse_options fail");
			return l_dbus_message_new_error( message, "org.bluez.Error", "InvalidArguments");
		}

		DBG_LOG_INFO("AcquireWrite %s link %s mtu %d", pt_dev, pt_link, pt_chrc->mtu);

		reply = create_sock( pt_chrc, message);

		if(pt_chrc->write_io)
		{
			l_dbus_property_changed( pt_dbus, pt_chrc->path, BT_IF_NM_GATTCHARACTERISTIC, "WriteAcquired");
		}
		DBG_LOG_INFO("BKP");
		return reply;
	#endif
}




/*typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data);
*/
static struct l_dbus_message *chrc_method_cb_acquire_notify(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	#if(0) // the original
	DBG_LOG_WARN("TODO");	
	return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "TODO");
	#else
		struct chrc*pt_chrc = (struct chrc*)user_data;

		struct l_dbus_message *pt_reply = NULL;
		
		struct l_dbus_message_iter kpiter = {0};
		struct l_dbus_message_iter tpiter = {0};
		uint8_t *ptstr = NULL;

		uint16_t kp_offset = 0;
		uint16_t kp_mtu = 0;
		uint8_t*pt_link = NULL;
		uint8_t*pt_dev = NULL;
		
		DBG_LOG_INFO("*********************************hello there");
		if(NULL == pt_chrc)
		{
			DBG_LOG_INFO("unexpected NULL");
			return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "TODO");						
		}
		if(pt_chrc->notify_io)
		{
			DBG_LOG_INFO("NotPermitted");
			return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "NotPerimitted", "TOFIX");												
		}

		if(false == l_dbus_message_get_arguments( message, "a{sv}", &kpiter))
		{
			DBG_LOG_ERR("fail to pick the arguments");		
			return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "Unknown error", "TOFIX"); 											
		}
		while(l_dbus_message_iter_next_entry( &kpiter, &ptstr, &tpiter))
		{
			DBG_LOG_INFO("the str %s", ptstr);	
			if(0 == strcmp( ptstr,"offset"))
			{
				l_dbus_message_iter_get_variant( &tpiter, "q", &kp_offset);
			}
			else if(0 == strcmp( ptstr, "mtu"))
			{
				l_dbus_message_iter_get_variant( &tpiter, "q", &kp_mtu);
			}
			else if(0 == strcmp( ptstr, "device"))
			{
				l_dbus_message_iter_get_variant( &tpiter, "o", &pt_dev);
			}
			else if(0 == strcmp(ptstr, "link"))
			{
				l_dbus_message_iter_get_variant( &tpiter, "s", &pt_link);
			}
			else
			{
				DBG_LOG_WARN("unknown parameter");
			}
		}
		DBG_LOG_INFO("pt_dev:%s , pt_link : %s, mtu : %d, offset : %d", pt_dev, pt_link, kp_mtu, kp_offset);

		// update the MTU
		DBG_LOG_INFO("chrc (%s) MTU is set to %d", pt_chrc->path, kp_mtu);
		pt_chrc->mtu = kp_mtu;

		pt_reply = create_sock( pt_chrc, message);

		if(pt_chrc->notify_io)
		{
			l_dbus_property_changed( pt_dbus, pt_chrc, BT_IF_NM_GATTCHARACTERISTIC, "NotifyAcquired");
		}
		
		return pt_reply;//l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "Unknown error", "TOFIX"); 															
	#endif
}

#if(0) // the original
static void proxy_notify_reply(DBusMessage *message, void *user_data)
{
	struct notify_attribute_data *data = user_data;
	DBusConnection *conn = bt_shell_get_env("DBUS_CONNECTION");
	DBusError error;

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, message) == TRUE) {
		bt_shell_printf("Failed to %s: %s\n",
				data->enable ? "StartNotify" : "StopNotify",
				error.name);
		dbus_error_free(&error);
		g_dbus_send_error(conn, data->msg, error.name, "%s",
							error.message);
		goto done;
	}

	g_dbus_send_reply(conn, data->msg, DBUS_TYPE_INVALID);

	data->chrc->notifying = data->enable;
	bt_shell_printf("[" COLORED_CHG "] Attribute %s (%s) "
				"notifications %s\n",
				data->chrc->path,
				bt_uuidstr_to_str(data->chrc->uuid),
				data->enable ? "enabled" : "disabled");
	g_dbus_emit_property_changed(conn, data->chrc->path, CHRC_INTERFACE,
							"Notifying");

done:
	dbus_message_unref(data->msg);
	free(data);
}
#else
struct notify_attribute_data {
	struct chrc *chrc;
	//DBusMessage *msg;
	struct l_dbus_message *msg;
	bool enable;
};

static void proxy_notify_reply(struct l_dbus_message *message, void *user_data)
{
	struct notify_attribute_data *data = (struct notify_attribute_data*)user_data;
	#if(0)
	DBusConnection *conn = bt_shell_get_env("DBUS_CONNECTION");
	DBusError error;
	dbus_error_init(&error);
	if (dbus_set_error_from_message(&error, message) == TRUE) {
		bt_shell_printf("Failed to %s: %s\n",
				data->enable ? "StartNotify" : "StopNotify",
				error.name);
		dbus_error_free(&error);
		g_dbus_send_error(conn, data->msg, error.name, "%s",
							error.message);
		goto done;
	}
	#else
		if(l_dbus_message_is_error( message))
		{
			char*pt_name = NULL, *pt_text = NULL;
			l_dbus_message_get_error( message, &pt_name, &pt_text);			
			DBG_LOG_ERR("fail to %s, errinfo: %s - %s", data->enable ?"StartNotify":"StopNotify", pt_name, pt_text);
			goto done;
		}
	#endif
	
	#if(1)
		//g_dbus_send_reply(conn, data->msg, DBUS_TYPE_INVALID);
		//l_dbus_send_with_reply(struct l_dbus * dbus, struct l_dbus_message * message, l_dbus_message_func_t function, void * user_data, l_dbus_destroy_func_t destroy)
		l_dbus_send( sys_init_get_dbus(sys_get_mgt()), data->msg);
		DBG_LOG_WARN("dboule check when things go wrong!");
	#endif
	
	data->chrc->notifying = data->enable;

	#if(0)
	bt_shell_printf("[" COLORED_CHG "] Attribute %s (%s) ""notifications %s\n",data->chrc->path,
				bt_uuidstr_to_str(data->chrc->uuid),
				data->enable ? "enabled" : "disabled");			
	g_dbus_emit_property_changed(conn, data->chrc->path, CHRC_INTERFACE,
							"Notifying");
	#else
		DBG_LOG_INFO("Attribue(%s), notification %s", data->chrc->path, data->enable);
		l_dbus_property_changed(sys_init_get_dbus(sys_get_mgt()), data->chrc->path, BT_IF_NM_GATTCHARACTERISTIC, "Notifying");
	#endif
done:
	//dbus_message_unref(data->msg);
	l_dbus_message_unref( data->msg);
	
	l_free(data);
}


#endif


#if(0)
static DBusMessage *proxy_notify(struct chrc *chrc, DBusMessage *msg,
							bool enable)
{
	struct notify_attribute_data *data;
	const char *method;

	if (enable == TRUE)
		method = "StartNotify";
	else
		method = "StopNotify";

	data = new0(struct notify_attribute_data, 1);
	data->chrc = chrc;
	data->msg = dbus_message_ref(msg);
	data->enable = enable;

	if (g_dbus_proxy_method_call(chrc->proxy, method, NULL,
					proxy_notify_reply, data, NULL))
		return NULL;

	return g_dbus_create_error(msg, "org.bluez.Error.InvalidArguments",
								NULL);
}

#else


static struct l_dbus_message *proxy_notify(struct chrc *chrc, struct l_dbus_message *msg,bool enable)
{
	struct notify_attribute_data *data;
	const char *method;

	if (enable == true)
		method = "StartNotify";
	else
		method = "StopNotify";

	
	//data = new0(struct notify_attribute_data, 1);
	data = (struct notify_attribute_data*)l_malloc(sizeof(struct notify_attribute_data));
	memset( data, 0, sizeof(struct notify_attribute_data));
	
	data->chrc = chrc;
	//data->msg = dbus_message_ref(msg);
	data->msg = l_dbus_message_ref(msg);
	
	data->enable = enable;

	#if(0)
	if (g_dbus_proxy_method_call(chrc->proxy, method, NULL,proxy_notify_reply, data, NULL))
		return NULL;
	#else
		if(l_dbus_proxy_method_call( chrc->proxy, method, NULL, proxy_notify_reply, data, NULL))
		{
			DBG_LOG_ERR("fail to call method %s", method);
			return NULL;
		}
	#endif
	//return g_dbus_create_error(msg, "org.bluez.Error.InvalidArguments",NULL);
	return l_dbus_message_new_error( msg, "org.bluez.Error","InvalidArguments","TOFIX");
}

#endif


/*typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data);
*/
static struct l_dbus_message *chrc_method_cb_start_notify(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	#if(0)
	DBG_LOG_WARN("TODO");	
	return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "TODO");
	#else
		struct chrc*pt_chrc = (struct chrc*)user_data;
		if(pt_chrc == NULL)
		{
			DBG_LOG_ERR("Critical error");
			return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "TOFFIX");		 
		}
		if(pt_chrc->notifying)
		{
			DBG_LOG_INFO("chrc->notifying %d", pt_chrc->notifying);
			return l_dbus_message_new_method_return(message);
		}
		if(pt_chrc->proxy)
		{
			return proxy_notify( pt_chrc, message, true);
		}
		pt_chrc->notifying = true;
		DBG_LOG_INFO("Attribue(%s) notifications enabled", pt_chrc->path);

		l_dbus_property_changed( pt_dbus, pt_chrc->path, BT_IF_NM_GATTCHARACTERISTIC, "Notifying");

		return l_dbus_message_new_method_return(message);
		
	#endif
}


/*typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data);
*/
static struct l_dbus_message *chrc_method_cb_stop_notify(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	#if(0) 	
		DBG_LOG_ERR("TODO");	
		return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "TODO");
	#else
		struct chrc*pt_chrc = (struct chrc*)user_data;
		if(pt_chrc == NULL)
		{
			DBG_LOG_ERR("Critical error");
			return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "TOFFIX");		 
		}
		if(!pt_chrc->notifying)
		{
			DBG_LOG_INFO("chrc->notifying %d", pt_chrc->notifying);
			return l_dbus_message_new_method_return(message);
		}
		if(pt_chrc->proxy)
		{
			return proxy_notify( pt_chrc, message, false);
		}

		pt_chrc->notifying = false;
		DBG_LOG_INFO("Attribue(%s), notifications disabled", pt_chrc->path);

		l_dbus_property_changed( pt_dbus, pt_chrc->path, BT_IF_NM_GATTCHARACTERISTIC, "Notifying");
		
		return l_dbus_message_new_method_return(message);
	#endif
}


/*typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data);
*/
static struct l_dbus_message *chrc_method_cb_confirm(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_ERR("TODO==============to complete");	
	//return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "TODO");
	return l_dbus_message_new_method_return( message);
}

/*
typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);*/
static bool chrc_pro_cb_get_handle(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct chrc*pt_chrc = (struct chrc*)user_data;
	DBG_LOG_INFO("CB chrc to get handle");
	return l_dbus_message_builder_append_basic( builder, 'q', (const void *)&pt_chrc->handle);
}
/*
typedef struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_iter *new_value,
					l_dbus_property_complete_cb_t complete,
					void *user_data);*/

static struct l_dbus_message *chrc_pro_cb_set_handle(struct l_dbus *ptdbus,struct l_dbus_message *message,struct l_dbus_message_iter *new_value,l_dbus_property_complete_cb_t complete,void *user_data)
{
	struct l_dbus_message*pt_reply = NULL;
	struct chrc*pt_chrc = (struct chrc*)user_data;
	uint16_t tpu16 = 0;
	DBG_LOG_INFO("CB to set chrc handle");
	if(!l_dbus_message_iter_get_variant( new_value, "q", &tpu16))
	{
		DBG_LOG_ERR("fail to get the new handle value");
		return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "Invalid arguments");
	}
	DBG_LOG_INFO("CHRC@0x%x handle is set to %d", pt_chrc, tpu16);
	pt_chrc->handle = tpu16;

	pt_reply = l_dbus_message_new_method_return(message);
	return pt_reply;
}

/*
typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);*/
static bool chrc_pro_cb_get_uuid(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct chrc*pt_chrc = (struct chrc*)user_data;
	DBG_LOG_INFO("CB to get UUID");
	return l_dbus_message_builder_append_basic( builder, 's', (const void *)pt_chrc->uuid);
}
/*
typedef struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_iter *new_value,
					l_dbus_property_complete_cb_t complete,
					void *user_data);*/

static struct l_dbus_message *chrc_pro_cb_set_uuid(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_iter *new_value,l_dbus_property_complete_cb_t complete,void *user_data)
{
	DBG_LOG_ERR("nothing todo ,readonly property");
	return NULL;
}

/*
typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);*/
static bool chrc_pro_cb_get_service(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct chrc*pt_chrc = (struct chrc*)user_data;
	DBG_LOG_INFO("CB to get service");
	return l_dbus_message_builder_append_basic( builder, 'o', pt_chrc->service->path);
}

/*
typedef struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_iter *new_value,
					l_dbus_property_complete_cb_t complete,
					void *user_data);*/

static struct l_dbus_message *chrc_pro_cb_set_service(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_iter *new_value,l_dbus_property_complete_cb_t complete,void *user_data)
{
	DBG_LOG_ERR("readonly property, nothing to do");
	return NULL;
}

/*
typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);*/
static bool chrc_pro_cb_get_value(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct chrc*pt_chrc = (struct chrc*)user_data;
	uint16_t tpval = 0, i = 0;

	#if(0) // the original
	DBG_LOG_INFO("notify======CB to get value");
	l_dbus_message_builder_enter_array( builder, "y");
	if(pt_chrc->value_len)
	{
		tpval = pt_chrc->value_len;
		for(i = 0;i < tpval; i++)
		{
			l_dbus_message_builder_append_basic( builder, 'y', (const void *)&pt_chrc->value[i]);
		}
	}
	l_dbus_message_builder_leave_array( builder);
	#else
	DBG_LOG_INFO("chrc@0x%x , value_buf@0x%x,value_len = %d", pt_chrc, pt_chrc->value, pt_chrc->value_len);
	
	l_dbus_message_builder_enter_array( builder, "y");
	if(1)
	{
		for(i = 0;i < pt_chrc->value_len; i++)
		{
			l_dbus_message_builder_append_basic( builder, 'y', (const void *)&pt_chrc->value[i]);
		}
	}
	l_dbus_message_builder_leave_array( builder);
	#endif
	return true;
}


/*
typedef struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_iter *new_value,
					l_dbus_property_complete_cb_t complete,
					void *user_data);*/

static struct l_dbus_message *chrc_pro_cb_set_value(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_iter *new_value,l_dbus_property_complete_cb_t complete,void *user_data)
{
	DBG_LOG_ERR("NOthing to do");
	return NULL;
}

/*
typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);*/
static bool chrc_pro_cb_get_notifying(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct chrc*pt_chrc = (struct chrc*)user_data;
	bool kp_bval = false;
	DBG_LOG_INFO("CB to get notifying %d", pt_chrc->notifying);
	kp_bval = pt_chrc->notifying?true:false;
	return l_dbus_message_builder_append_basic( builder, 'b', (const void *)&kp_bval);
}


/*
typedef struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_iter *new_value,
					l_dbus_property_complete_cb_t complete,
					void *user_data);*/

static struct l_dbus_message *chrc_pro_cb_set_notifying(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_iter *new_value,l_dbus_property_complete_cb_t complete,void *user_data)
{
	DBG_LOG_ERR("Nothing  to do");
	return NULL;
}

/*
typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);*/
static bool chrc_pro_cb_get_flags(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct chrc*pt_chrc = (struct chrc*)user_data;
	uint16_t i = 0;
	DBG_LOG_INFO("CB to get flags");
	l_dbus_message_builder_enter_array( builder, "s");
	for(i = 0; pt_chrc->flags[i]; i++)
	{
		l_dbus_message_builder_append_basic( builder, 's', (const void *)pt_chrc->flags[i]);
	}
	l_dbus_message_builder_leave_array( builder);
	return true;
}

/*
typedef struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_iter *new_value,
					l_dbus_property_complete_cb_t complete,
					void *user_data);*/

static struct l_dbus_message *chrc_pro_cb_set_flags(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_iter *new_value,l_dbus_property_complete_cb_t complete,void *user_data)
{
	DBG_LOG_ERR("Nothing to do");
	return NULL;
}

/*
typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);*/
static bool chrc_pro_cb_get_write_acquired(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct chrc*pt_chrc = (struct chrc*)user_data;
	bool kp_bval = false;
	DBG_LOG_INFO("CB to get write qcquired\n");
	kp_bval = pt_chrc->write_io ? true : false;
	return l_dbus_message_builder_append_basic( builder, 'b', (const void *)&kp_bval);
}

/*
typedef struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_iter *new_value,
					l_dbus_property_complete_cb_t complete,
					void *user_data);*/

static struct l_dbus_message *chrc_pro_cb_set_write_acquired(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_iter *new_value,l_dbus_property_complete_cb_t complete,void *user_data)
{
	DBG_LOG_ERR("Nothing to do");
	return NULL;
}

/*
typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);*/
static bool chrc_pro_cb_get_notify_acquired(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct chrc*pt_chrc = (struct chrc*)user_data;
	bool kp_bval = false;
	DBG_LOG_INFO("CB to get notify acquired");
	kp_bval = pt_chrc->notify_io ? true : false;
	return l_dbus_message_builder_append_basic( builder, 'b', (const void *)&kp_bval);
}

/*
typedef struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_iter *new_value,
					l_dbus_property_complete_cb_t complete,
					void *user_data);*/

static struct l_dbus_message *chrc_pro_cb_set_notify_acquired(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_iter *new_value,l_dbus_property_complete_cb_t complete,void *user_data)
{
	DBG_LOG_ERR("Nothing to do");
	return NULL;
}



// typedef
// void (*l_dbus_interface_setup_func_t) (struct l_dbus_interface);
/**
 * @brief: The callback called when the characteristic interface is
 * defined.
 * @return: NULL
 */
static void
setup_func_for_register_chrc(struct l_dbus_interface *pt_interface)
{
  bool _ret = false;

  // Set ReadValue method
  _ret = l_dbus_interface_method(pt_interface, "ReadValue", 0,
                                 chrc_method_cb_read_value, "ay", "a{sv}",
                                 "value", "options");
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add ReadValue method");
  }

  // Set WriteValue method
  _ret = l_dbus_interface_method(pt_interface, "WriteValue", 0,
                                 chrc_method_cb_write_value, "",
                                 "aya{sv}", "value", "options");
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add WriteValue method");
  }

  // Set AcquireWrite method
  _ret = l_dbus_interface_method(pt_interface, "AcquireWrite", 0,
                                 chrc_method_cb_acquire_write, "",
                                 "a{sv}", "options");
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add AcquireWrite method");
  }

  // Set AcquireNotify method
  _ret = l_dbus_interface_method(pt_interface, "AcquireNotify", 0,
                                 chrc_method_cb_acquire_notify, "",
                                 "a{sv}", "options");
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add AcquireNotify method");
  }

  // Set StartNotify method
  _ret = l_dbus_interface_method(pt_interface, "StartNotify", 0,
                                 chrc_method_cb_start_notify, "", "");
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add StartNotify method");
  }

  // Set StopNotify method
  _ret = l_dbus_interface_method(pt_interface, "StopNotify", 0,
                                 chrc_method_cb_stop_notify, "", "");
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add StopNotify method");
  }

  // Set Confirm method
  _ret = l_dbus_interface_method(pt_interface, "Confirm", 0,
                                 chrc_method_cb_confirm, "", "");
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add Confirm method");
  }

  // Set handle property
  _ret = l_dbus_interface_property(pt_interface, "Handle", 0, "q",
                                   chrc_pro_cb_get_handle,
                                   chrc_pro_cb_set_handle);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add handle property");
  }

  // Set uuid property
  _ret = l_dbus_interface_property(pt_interface, "UUID", 0, "s",
                                   chrc_pro_cb_get_uuid, NULL);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add uuid property");
  }

  // Set service property
  _ret = l_dbus_interface_property(pt_interface, "Service", 0, "o",
                                   chrc_pro_cb_get_service, NULL);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add service property");
  }

  // Set value property
  _ret = l_dbus_interface_property(pt_interface, "Value", 0, "ay",
                                   chrc_pro_cb_get_value, NULL);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add value property");
  }

  // Set notifying property
  _ret = l_dbus_interface_property(pt_interface, "Notifying", 0, "b",
                                   chrc_pro_cb_get_notifying, NULL);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add notifying property");
  }

  // Set flags property
  _ret = l_dbus_interface_property(pt_interface, "Flags", 0, "as",
                                   chrc_pro_cb_get_flags, NULL);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add flags property");
  }

  // Set writeacquired property
  _ret = l_dbus_interface_property(pt_interface, "WriteAcquired", 0, "b",
                                   chrc_pro_cb_get_write_acquired, NULL);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add writeacquired property");
  }

  // Set notifyacquired property
  _ret = l_dbus_interface_property(pt_interface, "NotifyAcquired", 0, "b",
                                   chrc_pro_cb_get_notify_acquired, NULL);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add notifyacquired property");
  }
  return;
}
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
/**
 * @brief: The callback called by EVERY TIME an instance of characteristic
 * is being removed from an object on this bus.
 * @return: NULL
 */
static void
destroy_for_register_chrc(void *user_data)
{
  DBG_LOG_DEBUG("Going to free characteristic %p", user_data);
  if(user_data)
  {
    chrc_free(user_data);
  }
  return;
}
/**
 * @brief Register local characteristic in a service (with the given UUID)
 * @param pt_dbus The dbus
 * @param pt_uuid UUID of the service to register
 * @param kp_handle The handle
 * @param pt_flagstr The flag string for some options
 * @return @p ST_OK if things go well
 */
app_state_t
bt_gatt_register_characteristic(
  struct l_dbus *pt_dbus,
  const char *pt_uuid,
  const char *pt_flagstr,
  const uint16_t kp_handle)
{
  app_state_t _ret = ST_OK;
  struct service *_pt_service = NULL;
  struct chrc *_pt_chrc = NULL;

  if(g_pt_local_services == NULL)
  {
    DBG_LOG_ERR("No available service");
    _ret = ST_ERR;
    return;
  }

  if((pt_dbus == NULL) || (pt_uuid == NULL) ||
     (pt_flagstr == NULL))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  // The characteristic is assumed to attach to the last service in the
  // service queue
  _pt_service = l_queue_peek_tail(g_pt_local_services);
  _pt_chrc = (struct chrc *)l_malloc(sizeof(struct chrc));

  memset(_pt_chrc, 0, sizeof(struct chrc));
  _pt_chrc->service = _pt_service;
  _pt_chrc->uuid = l_strdup(pt_uuid);
  _pt_chrc->path = l_strdup_printf("%s/chrc%u",
                                   _pt_service->path,
                                   l_queue_length(_pt_service->chrcs));
  _pt_chrc->flags = l_strsplit(pt_flagstr, ',');
  _pt_chrc->authorization_req =
    attr_authorization_flag_exists(_pt_chrc->flags);
  _pt_chrc->handle = kp_handle;

  // Register the interface
  if(false == l_dbus_register_interface(pt_dbus,
                                        (const char *)
                                        BT_IF_NM_GATTCHARACTERISTIC,
                                        setup_func_for_register_chrc,
                                        destroy_for_register_chrc,
                                        true))
  {
    DBG_LOG_WARN("Failed to register %s (uuid %s)",
                 BT_IF_NM_GATTCHARACTERISTIC,
                 pt_uuid);
    DBG_LOG_WARN(
      "This may be caused since the interface has been registered");
  }
  else
  {
    DBG_LOG_INFO("%s has been registered successfully",
                 BT_IF_NM_GATTCHARACTERISTIC);
  }

  // Add instance of characteristic interface to object
  if(false == l_dbus_object_add_interface(pt_dbus,
                                          (const char *)_pt_chrc->path,
                                          (const char *)
                                          BT_IF_NM_GATTCHARACTERISTIC,
                                          _pt_chrc))
  {
    DBG_LOG_ERR(
      "Failed to add an instance %s to object %s",
      BT_IF_NM_GATTCHARACTERISTIC,
      _pt_chrc->path);
    l_dbus_unregister_interface(pt_dbus,
                                (const char *)BT_IF_NM_GATTCHARACTERISTIC);
    chrc_free(_pt_chrc);
    _ret = ST_ERR;
    return _ret;
  }
  else
  {
    DBG_LOG_DEBUG("Interface %s has been registered successfully",
                  BT_IF_NM_GATTCHARACTERISTIC);
  }

  // Add instance of property interface to object
  if(false == l_dbus_object_add_interface(pt_dbus,
                                          (const char *)_pt_chrc->path,
                                          (const char *)
                                          DBUS_INTERFACE_PROPERTIES,
                                          NULL))
  {
    DBG_LOG_ERR(
      "Failed to add an instance %s to object %s",
      DBUS_INTERFACE_PROPERTIES,
      _pt_chrc->path);
    l_dbus_object_remove_interface(pt_dbus, _pt_chrc->path,
                                   BT_IF_NM_GATTCHARACTERISTIC);
    l_dbus_unregister_interface(pt_dbus,
                                (const char *)BT_IF_NM_GATTCHARACTERISTIC);
    chrc_free(_pt_chrc);
    _ret = ST_ERR;
    return _ret;
  }
  else
  {
    DBG_LOG_DEBUG("Interface %s has been registered successfully",
                  DBUS_INTERFACE_PROPERTIES);
  }
#if (0)
  // Add instance of introspectable interface to object
  if(false == l_dbus_object_add_interface(pt_dbus,
                                          (const char *)_pt_chrc->path,
                                          (const char *)
                                          DBUS_INTERFACE_INTROSPECTABLE,
                                          NULL))
  {
    DBG_LOG_ERR(
      "Failed to add an instance %s to object %s",
      DBUS_INTERFACE_INTROSPECTABLE,
      _pt_chrc->path);
    l_dbus_object_remove_interface(pt_dbus, _pt_chrc->path,
                                   DBUS_INTERFACE_PROPERTIES);
    l_dbus_object_remove_interface(pt_dbus, _pt_chrc->path,
                                   BT_IF_NM_GATTCHARACTERISTIC);
    l_dbus_unregister_interface(pt_dbus, BT_IF_NM_GATTCHARACTERISTIC);
    chrc_free(_pt_chrc);
    _ret = ST_ERR;
    return _ret;
  }
  else
  {
    DBG_LOG_DEBUG("Interface %s has been registered successfully",
                  DBUS_INTERFACE_INTROSPECTABLE);
  }
#endif
  print_chrc(_pt_chrc, NULL);

  if(_pt_service->chrcs)
  {
    l_queue_push_tail(_pt_service->chrcs, (void *)_pt_chrc);
  }
  else
  {
    DBG_LOG_DEBUG("Local characteristic queue needs to be created");
    _pt_service->chrcs = l_queue_new();
    l_queue_push_tail(_pt_service->chrcs, (void *)_pt_chrc);
  }

  // To set the default value for the registered characteristic
  _pt_chrc->value = l_malloc(1);
  *(_pt_chrc->value) = 0;
  _pt_chrc->value_len = 1;
  _pt_chrc->max_val_len = 1;

  return _ret;
}


/*
typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
*/
static bool chrc_find_queue_match_uuid(const void *a, const void *b)
{
	bool tpret = true;
	struct chrc*pt_chrc = NULL;
	if((a == NULL)||( b == NULL))
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = false;
		return tpret;
	}
	pt_chrc = (struct chrc*)a;
	DBG_LOG_DEBUG("%p", pt_chrc);
	if(strcmp( pt_chrc->uuid, (const char*)b))
	{ // they do not match
		tpret = false;
	}
	else
	{
		tpret = true;
	}
	return tpret;
}

/*pt_uuid the UUID string trying to find
*/
static struct chrc *chrc_find(const char *pt_uuid)
{
	struct l_queue *l, *lc;
	struct service *service;
	struct chrc *chrc;

	struct l_queue_entry*pt_service_entries = NULL;

	#if(0)
	for (l = local_service; l; l = g_list_next(l)) {
		service = l->data;

		for (lc = service->chrcs; lc; lc =  g_list_next(lc)) {
			chrc = lc->data;

			/* match object path */
			if (!strcmp(chrc->path, pattern))
				return chrc;

			/* match UUID */
			if (!strcmp(chrc->uuid, pattern))
				return chrc;
		}
	}
	#else
		pt_service_entries = l_queue_get_entries(g_pt_local_services);
		if(pt_service_entries == NULL)
		{
			DBG_LOG_WARN("The local service queue is empty!");
			return NULL;
		}
		while(pt_service_entries)
		{
			DBG_LOG_DEBUG("The service %s, %s", ((struct service*)(pt_service_entries->data))->path, ((struct service*)(pt_service_entries->data))->uuid);
			DBG_LOG_DEBUG("%p", ((struct service*)(pt_service_entries->data))->chrcs);
			chrc = l_queue_find( ((struct service*)(pt_service_entries->data))->chrcs, chrc_find_queue_match_uuid, pt_uuid);
		 	if(chrc)
		 	{ // we found the specific characteristic
		 		return chrc;
		 	}
		 	pt_service_entries = pt_service_entries->next;
		}
	#endif

	return NULL;
}

/*
*/
static void chrc_unregister(void*data)
{
	struct chrc *pt_chrc = (struct chrc*)data;
	
	DBG_LOG_DEBUG("pt_chrc %p", pt_chrc);
	fflush(stdout);
	if(pt_chrc->service != NULL)
	{
		DBG_LOG_DEBUG("pt_chrc->service %p", pt_chrc->service);
		fflush(stdout);
		if(pt_chrc->service->conn != NULL)
		{
	  		DBG_LOG_DEBUG("pt_chrc->service->conn %p", pt_chrc->service->conn);
			fflush(stdout);
		}else{
			DBG_LOG_DEBUG("pt_chrc->service->conn NULL");
			fflush(stdout);
		}
		if(pt_chrc->path != NULL)
		{
  		  DBG_LOG_DEBUG("pt_chrc->path %p", pt_chrc->path);
		  fflush(stdout);
		}else{
		  DBG_LOG_DEBUG("pt_chrc->path NULL");
		  fflush(stdout);
		}
		if(pt_chrc->service->conn != NULL)
		{
		  if(pt_chrc->path != NULL)
		  {
			if(false == l_dbus_object_remove_interface( pt_chrc->service->conn, pt_chrc->path, BT_IF_NM_GATTCHARACTERISTIC))
			{
				DBG_LOG_ERR("Fail to remove  %s", pt_chrc->path);
				fflush(stdout);

			}
		  }
		}
	}
	else
	{
		DBG_LOG_DEBUG("pt_chrc->service->conn NULL");
		fflush(stdout);
	}
}

/**
 * @brief Unregister local characteristic
 * @param pt_dbus the dbus
 * @param pt_uuid uuid of the characteristic to be unregistered
 * @return @p ST_OK if things go well
 */
app_state_t
bt_gatt_unregister_characteristic(
  struct l_dbus *pt_dbus,
  const char *pt_uuid)
{
  app_state_t _ret = ST_OK;
  struct chrc *pt_chrc = NULL;

  if((pt_dbus == NULL) || (pt_uuid == NULL))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  pt_chrc = chrc_find(pt_uuid);
  if(pt_chrc == NULL)
  {
    DBG_LOG_ERR("Failed to find chrc %s", pt_uuid);
    _ret = ST_ERR;
    return _ret;
  }

  if(false == l_queue_remove(pt_chrc->service->chrcs, (void *)pt_chrc))
  {
    DBG_LOG_ERR("This should not happen.");
    _ret = ST_ERR;
    return _ret;
  }

  // Just for observing/debugging ...
  DBG_LOG_DEBUG("The characteristic of the service (@ %p) length is %d",
                pt_chrc->service,
                l_queue_length(pt_chrc->service->chrcs));

  // Unregister the interface
  chrc_unregister(pt_chrc);

  return _ret;
}




static void desc_free(void *data)
{
	struct desc *desc = data;

	l_free(desc->path);
	l_free(desc->uuid);
	l_strfreev(desc->flags);
	l_free(desc->value);
	l_free(desc);
}

static void print_desc(struct desc *desc)
{

		DBG_LOG_INFO("desc@(Handle 0x%04x), %s, %s",desc->handle, desc->path, desc->uuid);
}



/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data);
*/
static struct l_dbus_message *desc_method_cb_read_value(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message *pt_reply = NULL;
	struct l_dbus_message_builder *pt_builder = NULL;
	
	DBG_LOG_ERR("TO complete this function later");

	pt_reply = l_dbus_message_new_method_return( message);
	pt_builder = l_dbus_message_builder_new( pt_reply);
	l_dbus_message_builder_enter_array( pt_builder, "y");
	uint8_t tpval = 'B';
	l_dbus_message_builder_append_basic( pt_builder, 'y', (const void *)&tpval);
	l_dbus_message_builder_leave_array( pt_builder);

	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);
	
	return pt_reply;
}

/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data);
*/
static struct l_dbus_message *desc_method_cb_write_value(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_WARN("to complete this function later");
	return l_dbus_message_new_method_return( message);
}

/*typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);
*/
static bool desc_pro_cb_get_handle(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct desc*pt_desc = (struct desc*)user_data;
	DBG_LOG_INFO("CB to get Handle");
	return l_dbus_message_builder_append_basic( builder, 'q', (const void *)&pt_desc->handle);
}

/*typedef struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_iter *new_value,
					l_dbus_property_complete_cb_t complete,
					void *user_data);
*/

static struct l_dbus_message *desc_pro_cb_set_handle(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_iter *new_value,l_dbus_property_complete_cb_t complete,void *user_data)
{
	struct desc*pt_desc = (struct desc*)user_data;
	uint16_t tpu16 = 0;
	
	DBG_LOG_INFO("CB to set Handle");
	if(false == l_dbus_message_iter_get_variant( new_value, "q", &tpu16))
	{	
		DBG_LOG_ERR("fail to get the Handle value");
		return l_dbus_message_new_error( message, "org.freedesktop.DBus.Error",  "InvalidArgs", "Invalid arguments");
	}
	DBG_LOG_INFO("CB desc@0x%x handle is set to %d", pt_desc, tpu16);
	pt_desc->handle = tpu16;

	return l_dbus_message_new_method_return(message);
}


/*typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);
*/
static bool desc_pro_cb_get_uuid(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct desc*pt_desc = (struct desc*)user_data;
	DBG_LOG_INFO("CB to get UUID");
	return l_dbus_message_builder_append_basic( builder, 's', (const void *)pt_desc->uuid);
}

/*typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);
*/
static bool desc_pro_cb_get_characteristic(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct desc*pt_desc = (struct desc*)user_data;
	DBG_LOG_INFO("CB to get characteristic");
	return l_dbus_message_builder_append_basic( builder, 'o', (const void *)pt_desc->chrc->path);
}

/*typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);
*/
static bool desc_pro_cb_get_value(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct desc*pt_desc = (struct desc*)user_data;
	uint16_t i = 0;
	DBG_LOG_INFO("CB to get value");

	l_dbus_message_builder_enter_array( builder, "y");
	for(i = 0; i < pt_desc->value_len; i++)
	{
		l_dbus_message_builder_append_basic( builder, 'y', (const void *)&pt_desc->value[i]);
	}
	l_dbus_message_builder_leave_array( builder);
	
	return true;
}


/*typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);
*/
static bool desc_pro_cb_get_flags(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	struct desc*pt_desc = (struct desc*)user_data;
	int i = 0;
	DBG_LOG_INFO(" CB to get flags");
	l_dbus_message_builder_enter_array( builder, "s");
	for(i = 0; pt_desc->flags[i]; i++)
	{
		l_dbus_message_builder_append_basic( builder, 's', (const void *)pt_desc->flags[i]);
	}
	l_dbus_message_builder_leave_array( builder);
	return true;
}


/*
typedef void (*l_dbus_interface_setup_func_t) (struct l_dbus_interface *);
*/
static void msg_setup_for_register_desc(struct l_dbus_interface *pt_interface)
{
	bool tp_bval = false;
	DBG_LOG_INFO("CB for register desc");
	tp_bval = l_dbus_interface_method( pt_interface, "ReadValue", 0, desc_method_cb_read_value, "ay", "a{sv}", "value", "options");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to register method ReadValue");
	}
	tp_bval = l_dbus_interface_method( pt_interface, "WriteValue", 0, desc_method_cb_write_value, "a{sv}", "ay", "options", "value");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to register method WriteValue");
	}

	// to register propertyies
	tp_bval = l_dbus_interface_property( pt_interface, "Handle", 0, "q", desc_pro_cb_get_handle, desc_pro_cb_set_handle);
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to register Handle");
	}
	tp_bval = l_dbus_interface_property( pt_interface, "UUID", 0, "s", desc_pro_cb_get_uuid, NULL);
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to register UUID");
	}
	tp_bval = l_dbus_interface_property( pt_interface, "Characteristic", 0, "o", desc_pro_cb_get_characteristic, NULL);
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to register characteristic");
	}
	tp_bval = l_dbus_interface_property( pt_interface, "Value", 0, "ay", desc_pro_cb_get_value, NULL);
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to register value");
	}
	tp_bval = l_dbus_interface_property( pt_interface, "Flags", 0, "as", desc_pro_cb_get_flags, NULL);
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to register flags");
	}
}

/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
static void destroy_for_register_desc(void *user_data)
{
	struct desc*pt_desc = (struct desc*)user_data;
	DBG_LOG_WARN(" double check when things goes wrong");
	if(pt_desc)
	{
		desc_free( pt_desc);
	}
}


/*register local descriptor
ret@ST_OK is returned when things go well
*/
app_state_t bt_gatt_register_descriptor(struct l_dbus *pt_dbus, const char*pt_uuid, const char*pt_flags, uint16_t kp_handle)
{
	app_state_t tpret = ST_OK;

	struct service*pt_service = NULL;
	struct desc*pt_desc = NULL;

	if((pt_dbus == NULL)||(pt_uuid == NULL)||(pt_flags == NULL))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}
	if(g_pt_local_services == NULL)
	{
		DBG_LOG_ERR("No local service registered");
		tpret = ST_ERR;
		return tpret;
	}
	
	pt_service = (struct service*)l_queue_peek_tail(g_pt_local_services);
	if(NULL == pt_service->chrcs)
	{
		DBG_LOG_ERR("No local characteristic registered");
		tpret = ST_ERR;
		return tpret;
	}
	
	pt_desc = (struct desc*)l_malloc(sizeof(struct desc));
	if(pt_desc == NULL)
	{
		DBG_LOG_ERR("fail to allocate memroy");
		tpret = ST_ERR;
		return tpret;
	}
	memset( pt_desc, 0, sizeof(struct desc));
	pt_desc->chrc = (struct chrc*)l_queue_peek_tail( pt_service->chrcs);
	pt_desc->uuid = l_strdup( pt_uuid);
	pt_desc->path = l_strdup_printf( "%s/desc%u", pt_desc->chrc->path, l_queue_length(pt_desc->chrc->descs));
	pt_desc->flags = l_strsplit( pt_flags, ',');
	pt_desc->handle = kp_handle;

	// to register the descriptor
	if(false == l_dbus_register_interface( pt_dbus, BT_IF_NM_GATTDESCRIPTOR, msg_setup_for_register_desc, destroy_for_register_desc, true))
	{
		
		DBG_LOG_ERR("fail to register interface %s", BT_IF_NM_GATTDESCRIPTOR);
		#if(0)
		desc_free( pt_desc);
		tpret = ST_ERR;
		return tpret;
		#else
			DBG_LOG_WARN("this happens when the interface is registered before.");
		#endif
	}
	if(false == l_dbus_object_add_interface( pt_dbus, pt_desc->path, BT_IF_NM_GATTDESCRIPTOR, pt_desc))
	{
		l_dbus_unregister_interface( pt_dbus, BT_IF_NM_GATTDESCRIPTOR);
		desc_free(pt_desc);
		
		DBG_LOG_ERR("fail to add %s @ %s", pt_desc->path, BT_IF_NM_GATTDESCRIPTOR);
		tpret = ST_ERR;
		return tpret;
	}

	if(false == l_dbus_object_add_interface( pt_dbus, pt_desc->path, DBUS_INTERFACE_PROPERTIES, NULL))
	{
		l_dbus_object_remove_interface( pt_dbus,  pt_desc->path, BT_IF_NM_GATTDESCRIPTOR);
		l_dbus_unregister_interface( pt_dbus,  BT_IF_NM_GATTDESCRIPTOR);
		desc_free( pt_desc);
		
		DBG_LOG_ERR("fail to register interface %s", DBUS_INTERFACE_PROPERTIES);
		tpret = ST_ERR;
		return tpret;
	}
	
	print_desc(pt_desc);
	
	return tpret;
}


// typedef
// void (*l_queue_destroy_func_t) (void *data);
// Callback called to destroy "includes" queue
static void
inc_unregister(void *data)
{
  // Nothing to do since nothing in "includes"
}
static void
service_free(void *data)
{
  struct service *service = data;

//   l_queue_destroy(service->chrcs, chrc_unregister);
//   l_queue_destroy(service->inc, inc_unregister);
  l_free(service->path);
  l_free(service->uuid);
  l_free(service);
}
// typedef
// bool (*l_dbus_property_get_cb_t) (struct l_dbus *, struct l_dbus_message
// *message, struct l_dbus_message_builder *builder, void *user_data);
/**
 * @brief: The getter callback of the property of "handle".
 * @return: NULL
 */
static bool
service_pro_cb_get_handle(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_builder *builder,
  void *user_data)
{
  struct service *pt_service = (struct service *)user_data;

  DBG_LOG_DEBUG("Service (@ %p) get handle %d", pt_service,
                pt_service->handle);
  return l_dbus_message_builder_append_basic(builder, 'q',
                                             &pt_service->handle);
}
// typedef
// struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
// struct l_dbus_message *message, struct l_dbus_message_iter *new_value,
// l_dbus_property_complete_cb_t complete, void *user_data);
/**
 * @brief: The setter callback of the property of "handle".
 * @return: NULL
 */
static struct l_dbus_message *
service_pro_cb_set_handle(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_iter *new_value,
  l_dbus_property_complete_cb_t complete,
  void *user_data)
{
  struct service *pt_service = (struct service *)user_data;
  uint16_t tpu16 = 0;

  if(!l_dbus_message_iter_get_variant(new_value, "q", &tpu16))
  {
    return l_dbus_message_new_error(message,
                                    "org.freedesktop.DBus.Error"
                                    "InvalidArgs",
                                    "fail to set handle value");
  }

  DBG_LOG_DEBUG("Service (@ %p) set handle to %d", pt_service, tpu16);
  pt_service->handle = tpu16;

  return l_dbus_message_new_method_return(message);
}
// typedef
// bool (*l_dbus_property_get_cb_t) (struct l_dbus *, struct l_dbus_message
// *message, struct l_dbus_message_builder *builder, void *user_data);
/**
 * @brief: The getter callback of the property of "uuid".
 * @return: NULL
 */
static bool
service_pro_cb_get_uuid(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_builder *builder,
  void *user_data)
{
  struct service *pt_service = (struct service *)user_data;

  DBG_LOG_DEBUG("Service (@ %p) get uuid %s", pt_service,
                pt_service->uuid);
  return l_dbus_message_builder_append_basic(builder, 's',
                                             (const void *)pt_service->uuid);
}
// typedef
// struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
// struct l_dbus_message *message, struct l_dbus_message_iter *new_value,
// l_dbus_property_complete_cb_t complete, void *user_data);
/**
 * @brief: The setter callback of the property of "uuid".
 * @return: NULL
 */
static struct l_dbus_message *
service_pro_cb_set_uuid(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_iter *new_value,
  l_dbus_property_complete_cb_t complete,
  void *user_data)
{
  struct service *pt_service = (struct service *)user_data;
  char *pt_uuid = NULL;

  if(!l_dbus_message_get_arguments(message, "s", &pt_uuid))
  {
    return l_dbus_message_new_error(message,
                                    "org.freedesktop.DBus.Error"
                                    "InvalidArgs",
                                    "fail to set uuid");
  }

  DBG_LOG_DEBUG("Service (@ %p) set uuid to %s", pt_service, pt_uuid);

  if(pt_service->uuid)
  {
    l_free(pt_service->uuid);
    pt_service->uuid = NULL;
  }
  pt_service->uuid = l_strdup(pt_uuid);

  return l_dbus_message_new_method_return(message);
}
// typedef
// bool (*l_dbus_property_get_cb_t) (struct l_dbus *, struct l_dbus_message
// *message, struct l_dbus_message_builder *builder, void *user_data);
/**
 * @brief: The getter callback of the property of "primary".
 * @return: NULL
 */
static bool
service_pro_cb_get_primary(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_builder *builder,
  void *user_data)
{
  struct service *pt_service = (struct service *)user_data;

  DBG_LOG_DEBUG("Service (@ %p) get primary %d", pt_service,
                pt_service->primary);
  return l_dbus_message_builder_append_basic(builder, 'b',
                                             &pt_service->primary);
}
// typedef
// struct l_dbus_message *(*l_dbus_property_set_cb_t) (struct l_dbus *,
// struct l_dbus_message *message, struct l_dbus_message_iter *new_value,
// l_dbus_property_complete_cb_t complete, void *user_data);
/**
 * @brief: The setter callback of the property of "primary".
 * @return: NULL
 */
static struct l_dbus_message *
service_pro_cb_set_primary(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_iter *new_value,
  l_dbus_property_complete_cb_t complete,
  void *user_data)
{
  struct service *pt_service = (struct service *)user_data;
  bool tp_bval = false;

  if(!l_dbus_message_get_arguments(message, "b", &tp_bval))
  {
    return l_dbus_message_new_error(message,
                                    "org.freedesktop.DBus.Error"
                                    "InvalidArgs",
                                    "fail to set primary");
  }

  DBG_LOG_DEBUG("Service (@ %p) set primary to %d", pt_service, tp_bval);
  pt_service->primary = tp_bval;

  return l_dbus_message_new_method_return(message);
}
// typedef
// bool (*l_dbus_property_get_cb_t) (struct l_dbus *, struct l_dbus_message
// *message, struct l_dbus_message_builder *builder, void *user_data);
/**
 * @brief: The getter callback of the property of "includes". This function
 * needs to be modified when there is something to be included.
 * @return: NULL
 */
static bool
service_pro_cb_get_includes(
  struct l_dbus *pt_dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_builder *builder,
  void *user_data)
{
  struct service *pt_service = (struct service *)user_data;
  struct l_queue_entry *pt_queue = NULL;

  DBG_LOG_DEBUG("Service (@ %p) get includes", pt_service);

  l_dbus_message_builder_enter_array(builder, "o");

  pt_queue = l_queue_get_entries(pt_service->inc);
  while(pt_queue)
  {
    l_dbus_message_builder_append_basic(builder, 'o',
                                        (const void *)pt_queue->data);
    pt_queue = pt_queue->next;
  }
  l_dbus_message_builder_leave_array(builder);

  return true;
}
// typedef
// void (*l_dbus_interface_setup_func_t) (struct l_dbus_interface);
/**
 * @brief: The callback called when the gatt service interface is defined.
 * @return: NULL
 */
static void
setup_func_for_register_service(struct l_dbus_interface *pt_interface)
{

  bool _ret = false;

  // Set handle property
  _ret = l_dbus_interface_property(pt_interface, "Handle", 0, "q",
                                   service_pro_cb_get_handle,
                                   service_pro_cb_set_handle);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add handle property");
  }

  // Set uuid property
  _ret = l_dbus_interface_property(pt_interface, "UUID", 0, "s",
                                   service_pro_cb_get_uuid, NULL);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add UUID property");
  }

  // Set primary property
  _ret = l_dbus_interface_property(pt_interface, "Primary", 0, "b",
                                   service_pro_cb_get_primary, NULL);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add primary property");
  }

  // Set includes property
  _ret = l_dbus_interface_property(pt_interface, "Includes", 0, "ao",
                                   service_pro_cb_get_includes, NULL);
  if(!_ret)
  {
    DBG_LOG_ERR("Failed to add includes property");
  }
  return;
}
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
/**
 * @brief: The callback called by EVERY TIME an instance of gatt service is
 * being removed from an object on this bus.
 * @return: NULL
 */
static void
destroy_for_register_service(void *user_data)
{
  struct service *pt_service = (struct service *)user_data;

  DBG_LOG_DEBUG("Free a service %s", pt_service->path);
  service_free((void *)pt_service);
  return;
}
static void
print_service(
  struct service *pt_service,
  void *user_data)
{
  DBG_LOG_DEBUG("Service path %s, uuid %s",
                pt_service->path,
                pt_service->uuid);
}
/**
 * @brief Register local service (with the given UUID)
 * @param pt_dbus The dbus
 * @param pt_uuid UUID of the service to register
 * @param kp_handle The handle (0 is given for local services)
 * @param is_primary True to indicate the service is a primary service
 * @return @p ST_OK if things go well
 */
app_state_t
bt_gatt_register_service(
  struct l_dbus *pt_dbus,
  const char *pt_uuid,
  const uint16_t kp_handle,
  bool is_primary)
{
  app_state_t _ret = ST_OK;
  struct service *_pt_service = NULL;

  if((pt_dbus == NULL) || (pt_uuid == NULL))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  _pt_service = (struct service *)l_malloc(sizeof(struct service));
  memset(_pt_service, 0, sizeof(struct service));
  _pt_service->conn = pt_dbus;
  _pt_service->uuid = l_strdup(pt_uuid);
  _pt_service->path = l_strdup_printf("%s/service%u",
                                      BT_OBJ_PATH_APP,
                                      l_queue_length(g_pt_local_services));
  _pt_service->primary = is_primary;
  _pt_service->handle = kp_handle;

  // Register the interface
  if(false == l_dbus_register_interface(pt_dbus,
                                        (const char *)BT_IF_NM_GATTSERVICE,
                                        setup_func_for_register_service,
                                        destroy_for_register_service,
                                        true))
  {
    DBG_LOG_WARN("Failed to register %s (uuid %s)",
                 BT_IF_NM_GATTSERVICE,
                 pt_uuid);
    DBG_LOG_WARN(
      "This may be caused since the interface has been registered");
  }
  else
  {
    DBG_LOG_INFO("%s has been registered successfully",
                 BT_IF_NM_GATTSERVICE);
  }

  // Add instance of gatt service interface to object
  if(false == l_dbus_object_add_interface(pt_dbus,
                                          (const char *)_pt_service->path,
                                          (const char *)
                                          BT_IF_NM_GATTSERVICE,
                                          _pt_service))
  {
    DBG_LOG_ERR(
      "Failed to add an instance %s to object %s",
      BT_IF_NM_GATTSERVICE,
      _pt_service->path);
    l_dbus_unregister_interface(pt_dbus,
                                (const char *)BT_IF_NM_GATTSERVICE);
    service_free(_pt_service);
    _ret = ST_ERR;
    return _ret;
  }
  else
  {
    DBG_LOG_DEBUG("Interface %s has been registered successfully",
                  BT_IF_NM_GATTSERVICE);
  }

  // Add instance of property interface to object
  if(false == l_dbus_object_add_interface(pt_dbus,
                                          (const char *)_pt_service->path,
                                          (const char *)
                                          DBUS_INTERFACE_PROPERTIES,
                                          NULL))
  {
    DBG_LOG_ERR(
      "Failed to add an instance %s to object %s",
      DBUS_INTERFACE_PROPERTIES,
      _pt_service->path);
    l_dbus_object_remove_interface(pt_dbus, _pt_service->path,
                                   BT_IF_NM_GATTSERVICE);
    l_dbus_unregister_interface(pt_dbus,
                                (const char *)BT_IF_NM_GATTSERVICE);
    service_free(_pt_service);
    _ret = ST_ERR;
    return _ret;
  }
  else
  {
    DBG_LOG_DEBUG("Interface %s has been registered successfully",
                  DBUS_INTERFACE_PROPERTIES);
  }
#if (0)
  // Add instance of introspectable interface to object
  if(false == l_dbus_object_add_interface(pt_dbus,
                                          (const char *)_pt_service->path,
                                          (const char *)
                                          DBUS_INTERFACE_INTROSPECTABLE,
                                          NULL))
  {
    DBG_LOG_ERR(
      "Failed to add an instance %s to object %s",
      DBUS_INTERFACE_INTROSPECTABLE,
      _pt_service->path);
    l_dbus_object_remove_interface(pt_dbus, _pt_service->path,
                                   DBUS_INTERFACE_PROPERTIES);
    l_dbus_object_remove_interface(pt_dbus, _pt_service->path,
                                   BT_IF_NM_GATTSERVICE);
    l_dbus_unregister_interface(pt_dbus, BT_IF_NM_GATTSERVICE);
    service_free(_pt_service);
    _ret = ST_ERR;
    return _ret;
  }
  else
  {
    DBG_LOG_DEBUG("Interface %s has been registered successfully",
                  DBUS_INTERFACE_INTROSPECTABLE);
  }
#endif
  print_service(_pt_service, NULL);

  if(g_pt_local_services)
  {
    l_queue_push_tail(g_pt_local_services, (void *)_pt_service);
  }
  else
  {
    DBG_LOG_DEBUG("Local service queue needs to be created");
    g_pt_local_services = l_queue_new();
    l_queue_push_tail(g_pt_local_services, (void *)_pt_service);
  }

  return _ret;
}
// typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
// A callback called by l_queue_find to find element in the queue @p
// g_pt_local_services
static bool
queue_match_service_uuid(
  const void *a,
  const void *b)
{
  struct service *pt_service = NULL;

  if((a == NULL) || (b == NULL))
  {
    DBG_LOG_ERR("Unexpected NULL");
    return false;
  }

  pt_service = (struct service *)a;
  if(strcmp(pt_service->uuid, (const char *)b))
  {
    return false;
  }
  else
  {
    // The service is found
    return true;
  }
}
/**
 * @brief Find the corresponding element in @p g_pt_local_services based on
 * the given uuid ( @p pt_uuid )
 * @param pt_uuid The UUID
 * @return The element of the queue if found; otherwise NULL.
 */
static struct service *
service_find(const char *pt_uuid)
{
  struct service *pt_service = NULL;

  pt_service = (struct service *)l_queue_find(g_pt_local_services,
                                              queue_match_service_uuid,
                                              (const void *)pt_uuid);

  return pt_service;
}
/**
 * @brief Unregister local service
 * @param pt_dbus the dbus
 * @param pt_uuid uuid of the service to be unregistered
 * @return @p ST_OK if things go well
 */
app_state_t
bt_gatt_unregister_service(
  struct l_dbus *pt_dbus,
  const char *pt_uuid)
{
  app_state_t _ret = ST_OK;
  struct service *pt_service = NULL;

  if((pt_dbus == NULL) || (pt_uuid == NULL))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  pt_service = service_find(pt_uuid);
  if(pt_service == NULL)
  {
    DBG_LOG_ERR("Failed to find service %s", pt_uuid);
    _ret = ST_ERR;
    return _ret;
  }

  if(false == l_queue_remove(g_pt_local_services, pt_service))
  {
    DBG_LOG_ERR("This should not happen.");
    _ret = ST_ERR;
    return _ret;
  }

  // Just for observing/debugging ...
  DBG_LOG_DEBUG("The service queue (@ %p) length is %d",
                g_pt_local_services,
                l_queue_length(g_pt_local_services));

  // Unregister the interface
  if(false ==
     l_dbus_object_remove_interface(pt_dbus, pt_service->path,
                                    BT_IF_NM_GATTSERVICE))
  {
    DBG_LOG_ERR("Failed to remove %s from the service queue",
                pt_service->path);
    _ret = ST_ERR;
    return _ret;
  }

  return _ret;
}




/*typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);
*/
static bool app_register_cb_get_uuids(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	l_dbus_message_builder_enter_array( builder, "s");
	l_dbus_message_builder_append_basic( builder, 's', "register-application");
	l_dbus_message_builder_leave_array( builder);
	return true;
}

/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data);
*/
static struct l_dbus_message *app_register_cb_method_release(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	//l_dbus_unregister_interface(struct l_dbus * dbus, const char * interface)

	if(false == l_dbus_object_remove_interface( pt_dbus, BT_OBJ_PATH_APP, BT_IF_NM_GATTPROFILE))
	{
		DBG_LOG_ERR("fail to remove %s from %s", BT_IF_NM_GATTPROFILE, BT_OBJ_PATH_APP);
	}
	return l_dbus_message_new_method_return(message);
}


/*
typedef void (*l_dbus_interface_setup_func_t) (struct l_dbus_interface *);
*/
static void setup_func_for_register_application(struct l_dbus_interface *pt_interface)
{
	DBG_LOG_INFO("to register method/property for BLE application");
	//method
	l_dbus_interface_method( pt_interface, "Release", 0, app_register_cb_method_release, NULL, NULL);
	// the property
	l_dbus_interface_property( pt_interface, "UUIDs", 0, "as", app_register_cb_get_uuids, NULL);

	
	return ;
}

/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
static void destroy_for_register_application(void *user_data)
{
	DBG_LOG_ERR("Make sure you have nothing to do here");
	return;
}

/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);
*/
static void msg_setup_for_register_application(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder *pt_builder = NULL;
	DBG_LOG_WARN("TO setup message for application register");
	pt_builder = l_dbus_message_builder_new( message);
	l_dbus_message_builder_append_basic( pt_builder, 'o', "/");
	l_dbus_message_builder_enter_array( pt_builder, "{sv}");
	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	l_dbus_message_builder_leave_dict( pt_builder);
	l_dbus_message_builder_leave_array( pt_builder);

	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);
	return;
}

/*typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);
*/
static void msg_reply_for_register_application(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	const char*pt_name = NULL, *pt_text = NULL; 
	if(l_dbus_message_is_error(result))
	{
		l_dbus_message_get_error( result, &pt_name, &pt_text);
		DBG_LOG_ERR("fail to register BT application, err_info %s , %s", pt_name, pt_text);
	}
	else
	{
		 DBG_LOG_INFO("BT application is registered successfully");
	}
	return ;
}



/*register local BT application
ret@ST_OK is returned when things go well
*/
app_state_t bt_gatt_register_application( struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy)
{
	app_state_t tpret = ST_OK;
	if((pt_dbus == NULL)||(pt_proxy == NULL))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	// to register APP interface
	if(false == l_dbus_register_interface( pt_dbus, BT_IF_NM_GATTPROFILE, setup_func_for_register_application, destroy_for_register_application, true))
	{
		DBG_LOG_ERR("fail to register %s", BT_IF_NM_GATTPROFILE);
		tpret = ST_ERR;
		return tpret;
	}
	if(false == l_dbus_object_add_interface( pt_dbus, BT_OBJ_PATH_APP, BT_IF_NM_GATTPROFILE, NULL))
	{
		DBG_LOG_ERR("fail to add object %s", BT_OBJ_PATH_APP);
		l_dbus_unregister_interface( pt_dbus, BT_IF_NM_GATTPROFILE);
		
		tpret = ST_ERR;
		return tpret;
	}
	if(false == l_dbus_object_add_interface( pt_dbus, BT_OBJ_PATH_APP, DBUS_INTERFACE_PROPERTIES, NULL))
	{
		l_dbus_object_remove_interface( pt_dbus, BT_OBJ_PATH_APP, BT_IF_NM_GATTPROFILE);
		l_dbus_unregister_interface( pt_dbus, BT_IF_NM_GATTPROFILE);
		
		DBG_LOG_ERR("fail to register %s", DBUS_INTERFACE_PROPERTIES);
		tpret = ST_ERR;
		return tpret;
	}

	// to register the application
	if(0 == l_dbus_proxy_method_call( pt_proxy, "RegisterApplication", msg_setup_for_register_application, msg_reply_for_register_application, NULL, destroy_for_register_application))
	{
		DBG_LOG_ERR("fail to register application %s", BT_OBJ_PATH_APP);
		
		l_dbus_object_remove_interface( pt_dbus, BT_OBJ_PATH_APP, DBUS_INTERFACE_PROPERTIES);
		l_dbus_object_remove_interface( pt_dbus, BT_OBJ_PATH_APP, BT_IF_NM_GATTPROFILE);
		l_dbus_unregister_interface( pt_dbus, BT_IF_NM_GATTPROFILE);
		
		tpret = ST_ERR;
		return tpret;
	}
	return tpret;
}

/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);
*/
static void msg_setup_for_unregister_application(struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_ERR("TO COMPLETE");
	return;
}

/*typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);
*/
static void msg_reply_for_unregister_application(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	DBG_LOG_ERR("TO COMPLETE");
	return;
}

/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
static void destory_for_unregister_application(void *user_data)
{
	DBG_LOG_ERR("TO COMPLETE");
	return;
}

/*unregister local BT application
ret@ST_OK is returned when things go well
*/
app_state_t bt_gatt_unregister_application(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy)
{
	app_state_t tpret = ST_OK;
	if((pt_dbus == NULL)||(pt_proxy == NULL))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}

	if(l_dbus_proxy_method_call( pt_proxy, "UnregisterApplication", msg_setup_for_unregister_application, msg_reply_for_unregister_application, NULL, destory_for_unregister_application))
	{
		DBG_LOG_ERR("fail to call method UnreigisterApplication");
		tpret = ST_ERR;
		return tpret;
	}
	return ST_OK;
}



/*local service send notification
for debug only
*/
void bt_gatt_local_service_send_notification(struct app_management*pt_app_mgt)
{
	struct service *pt_service = NULL;
	struct chrc *pt_chrc = NULL;
	struct l_dbus*pt_dbus = NULL;
	struct l_dbus_proxy*pt_connected_proxy = NULL;
	if(pt_app_mgt == NULL)
	{
		DBG_LOG_INFO("unexpected NULL");
		return;
	}
	pt_dbus = sys_init_get_dbus( pt_app_mgt);
	if(NULL == pt_dbus)
	{
		DBG_LOG_ERR("no valid dbus connection");
		return;
	}
	pt_connected_proxy = sys_get_connected_proxy( pt_app_mgt);
	if(NULL == pt_connected_proxy)
	{
		DBG_LOG_ERR("no connected proxy");
		return;
	}
	pt_chrc = chrc_find(NORDIC_UART_CHARAC_TX_UUID);
	if(NULL == pt_chrc)
	{
		DBG_LOG_ERR("fail to find chrc %s", NORDIC_UART_CHARAC_TX_UUID);
		return;
	}
	DBG_LOG_INFO("Going to signal property changed for %s - %s", pt_chrc->path, pt_chrc->uuid);
	l_dbus_property_changed( pt_dbus, pt_chrc->path, BT_IF_NM_GATTCHARACTERISTIC, "Value");
	return;
}


#if(1) // for debug only
/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);
*/
static void msg_setup_for_write_value (struct l_dbus_message *message,void *user_data)
{// a{sv}
	uint16_t tpu16 = 0;
	uint8_t tpu8 = 'A';
	struct l_dbus_message_builder *pt_builder = l_dbus_message_builder_new( message);

	const char*pt_path = l_dbus_proxy_get_path(sys_get_connected_proxy(sys_get_mgt()));		
	DBG_LOG_INFO("the connected proxy path is %s", pt_path);

	l_dbus_message_builder_enter_array( pt_builder, "y");
	l_dbus_message_builder_append_basic( pt_builder, 'y', &tpu8);
	l_dbus_message_builder_leave_array( pt_builder);

	
	l_dbus_message_builder_enter_array( pt_builder, "{sv}");
	// offset
	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	l_dbus_message_builder_append_basic( pt_builder, 's', "offset");
	l_dbus_message_builder_enter_variant( pt_builder, "q");
	tpu16 = 0;
	l_dbus_message_builder_append_basic( pt_builder, 'q', &tpu16);
	l_dbus_message_builder_leave_variant( pt_builder);
	l_dbus_message_builder_leave_dict( pt_builder);
	//mtu
	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	l_dbus_message_builder_append_basic( pt_builder, 's', "mtu");
	l_dbus_message_builder_enter_variant( pt_builder, "q");
	tpu16 = 517;
	l_dbus_message_builder_append_basic( pt_builder, 'q', &tpu16);
	l_dbus_message_builder_leave_variant( pt_builder);
	l_dbus_message_builder_leave_dict( pt_builder);
	//device
	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	l_dbus_message_builder_append_basic( pt_builder, 's', "device");
	l_dbus_message_builder_enter_variant( pt_builder, "o");
	l_dbus_message_builder_append_basic( pt_builder, 'o', pt_path);
	l_dbus_message_builder_leave_variant( pt_builder);
	l_dbus_message_builder_leave_dict( pt_builder);
	//link
	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	l_dbus_message_builder_append_basic( pt_builder, 's', "link");
	l_dbus_message_builder_enter_variant( pt_builder, "s");
	l_dbus_message_builder_append_basic( pt_builder, 's', "LE");
	l_dbus_message_builder_leave_variant( pt_builder);
	l_dbus_message_builder_leave_dict( pt_builder);	
	l_dbus_message_builder_leave_array( pt_builder);

	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);

	DBG_LOG_INFO("for acquire-notify");
	return;
}

/*
typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);

*/
static void msg_reply_from_write_value (struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_INFO("from acquire-notify");

	if(l_dbus_message_is_error( message))
	{
		const char*pt_name = NULL, *pt_text = NULL;
		l_dbus_message_get_error( message, &pt_name, &pt_text);
		DBG_LOG_ERR("acquire notify  fail with err-info %s-%s", pt_name, pt_text);
	}
	else
	{
		DBG_LOG_INFO("Acquire notify goes well");
	}
	return ;
}

/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
 static void destroy_for_write_value(void *user_data)
 {
 	DBG_LOG_INFO(" to make sure you have nothing do here");
 	return;
 }

 void bt_gatt_local_service_send_notification_v2(struct app_management*pt_app_mgt)
{
	struct service *pt_service = NULL;
	struct chrc *pt_chrc = NULL;
	struct l_dbus*pt_dbus = NULL;
	struct l_dbus_proxy*pt_connected_proxy = NULL;
	if(pt_app_mgt == NULL)
	{
		DBG_LOG_INFO("unexpected NULL");
		return;
	}
	pt_dbus = sys_init_get_dbus( pt_app_mgt);
	if(NULL == pt_dbus)
	{
		DBG_LOG_ERR("no valid dbus connection");
		return;
	}
	pt_connected_proxy = sys_get_connected_proxy( pt_app_mgt);
	if(NULL == pt_connected_proxy)
	{
		DBG_LOG_ERR("no connected proxy");
		return;
	}
	pt_chrc = chrc_find(NORDIC_UART_CHARAC_TX_UUID);
	if(NULL == pt_chrc)
	{
		DBG_LOG_ERR("fail to find chrc %s", NORDIC_UART_CHARAC_TX_UUID);
		return;
	}
	DBG_LOG_INFO("Going to signal property changed for %s - %s", pt_chrc->path, pt_chrc->uuid);
	#if(0)
		l_dbus_property_changed( pt_dbus, pt_chrc->path, BT_IF_NM_GATTCHARACTERISTIC, "Value");
	#else
		//l_dbus_proxy_method_call( proxy, const char * method, l_dbus_message_func_t setup, l_dbus_client_proxy_result_func_t reply, void * user_data, l_dbus_destroy_func_t destroy)
		l_dbus_method_call( pt_dbus, "org.bluez.LEAdvertisement1", pt_chrc->path, BT_IF_NM_GATTCHARACTERISTIC, "WriteValue", msg_setup_for_write_value, msg_reply_from_write_value, NULL,  destroy_for_write_value);
	#endif
	return;
}

#endif

/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);
*/
static void msg_setup_for_notify_value (struct l_dbus_message *message,void *user_data)
{// a{sv}
	uint16_t tpu16 = 0;
	//uint8_t tpu8 = 'A';
	struct attr_value *pt_attr = (struct attr_value*)user_data;
	struct l_dbus_message_builder *pt_builder = l_dbus_message_builder_new( message);
	const char*pt_path = l_dbus_proxy_get_path(sys_get_connected_proxy(sys_get_mgt()));		
	DBG_LOG_INFO("the connected proxy path is %s", pt_path);

	if((NULL == pt_attr)||(pt_path == NULL))
	{
		DBG_LOG_ERR("unexpected NULL");
		return;
	}
	
	l_dbus_message_builder_enter_array( pt_builder, "y");
	for(uint32_t i = 0; i < pt_attr->len; i++)
	{
		l_dbus_message_builder_append_basic( pt_builder, 'y', &pt_attr->ptbuf[i]);
	}
	l_dbus_message_builder_leave_array( pt_builder);

	
	l_dbus_message_builder_enter_array( pt_builder, "{sv}");
	// offset
	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	l_dbus_message_builder_append_basic( pt_builder, 's', "offset");
	l_dbus_message_builder_enter_variant( pt_builder, "q");
	tpu16 = 0;
	l_dbus_message_builder_append_basic( pt_builder, 'q', &tpu16);
	l_dbus_message_builder_leave_variant( pt_builder);
	l_dbus_message_builder_leave_dict( pt_builder);
	//mtu
	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	l_dbus_message_builder_append_basic( pt_builder, 's', "mtu");
	l_dbus_message_builder_enter_variant( pt_builder, "q");
	tpu16 = 517;
	l_dbus_message_builder_append_basic( pt_builder, 'q', &tpu16);
	l_dbus_message_builder_leave_variant( pt_builder);
	l_dbus_message_builder_leave_dict( pt_builder);
	//device
	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	l_dbus_message_builder_append_basic( pt_builder, 's', "device");
	l_dbus_message_builder_enter_variant( pt_builder, "o");
	l_dbus_message_builder_append_basic( pt_builder, 'o', pt_path);
	l_dbus_message_builder_leave_variant( pt_builder);
	l_dbus_message_builder_leave_dict( pt_builder);
	//link
	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	l_dbus_message_builder_append_basic( pt_builder, 's', "link");
	l_dbus_message_builder_enter_variant( pt_builder, "s");
	l_dbus_message_builder_append_basic( pt_builder, 's', "LE");
	l_dbus_message_builder_leave_variant( pt_builder);
	l_dbus_message_builder_leave_dict( pt_builder);	
	l_dbus_message_builder_leave_array( pt_builder);

	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);
	
	//DBG_LOG_INFO("for acquire-notify");
	return;
}

/*
typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);

*/
static void msg_reply_from_notify_value (struct l_dbus_message *message,void *user_data)
{
	struct attr_value *pt_attr = (struct attr_value*)user_data; 
	DBG_LOG_INFO("from acquire-notify");

	if(l_dbus_message_is_error( message))
	{
		const char*pt_name = NULL, *pt_text = NULL;
		l_dbus_message_get_error( message, &pt_name, &pt_text);
		DBG_LOG_ERR("acquire notify  fail with err-info %s-%s", pt_name, pt_text);
	}
	else
	{
		DBG_LOG_INFO("Acquire notify goes well");
	}

	// to signal the value-notification is done
	if(pt_attr && pt_attr->pt_cond_var)	
	{
		pthread_mutex_lock( &pt_attr->pt_cond_var->mtx);
		pt_attr->pt_cond_var->is_op_done = true;
		pthread_cond_signal( &pt_attr->pt_cond_var->cond_var);
		pthread_mutex_unlock( &pt_attr->pt_cond_var->mtx);
	}
	else
	{
		DBG_LOG_ERR("Oops! unexpected situation");
	}
	return ;
}

/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
 static void destroy_for_notify_value(void *user_data)
 {
 	struct attr_value *pt_attr_val = NULL;
 	DBG_LOG_INFO("to release the data buf");
	if(user_data)
	{
		pt_attr_val = (struct attr_value*)user_data;
		if(pt_attr_val->ptbuf)
		{
			l_free( pt_attr_val->ptbuf);
			pt_attr_val->ptbuf = NULL;
			l_free( pt_attr_val);
			pt_attr_val = NULL;
		}
	}
	else
	{
		DBG_LOG_WARN("Double check to make sure this is expected");
	}
 	return;
 }


/**
 * @brief Write to the local Tx characteristic by notifying
 * @param pt_dtbuf The pointer to the data to send
 * @param kplen The data length
 * @return @p ST_OK if things go well
 */
app_state_t
bt_gatt_charac_send_notification(
  app_management *pt_app_mgt,
  uint8_t *pt_dtbuf,
  uint32_t kplen,
  struct pthread_cond_var *pt_cond_var)
{
  app_state_t _tpret = ST_OK;
  struct l_dbus *_pt_dbus = NULL;
  struct chrc *_pt_chrc = NULL;

  struct attr_value *_pt_attr = { 0 };

  if((NULL == pt_dtbuf) || (NULL == pt_cond_var) || (pt_app_mgt == NULL))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _tpret = ST_ERR;
    return _tpret;
  }

  if(kplen == 0)
  {
    DBG_LOG_ERR("Invalid data length");
    _tpret = ST_ERR;
    return _tpret;
  }

  _pt_dbus = sys_init_get_dbus(pt_app_mgt);
  if(NULL == _pt_dbus)
  {
    DBG_LOG_ERR("Failed to get dbus");
    _tpret = ST_ERR;
    return _tpret;
  }

  _pt_chrc = chrc_find(NORDIC_UART_CHARAC_TX_UUID);
  if(NULL == _pt_chrc)
  {
    DBG_LOG_ERR("Failed to find chrc %s", NORDIC_UART_CHARAC_TX_UUID);
    _tpret = ST_ERR;
    return _tpret;
  }

  // Malloc @p _pt_attr
  _pt_attr = (struct attr_value *)l_malloc(sizeof(struct attr_value));
  if(NULL == _pt_attr)
  {
    DBG_LOG_ERR("Failed to malloc");
    _tpret = ST_ERR;
    return _tpret;
  }
  memset(_pt_attr, 0, sizeof(struct attr_value));

  _pt_attr->ptbuf = (uint8_t *)l_malloc(kplen);
  if(NULL == _pt_attr->ptbuf)
  {
    DBG_LOG_ERR("Failed to malloc");
    if(_pt_attr)
    {
      l_free(_pt_attr);
      _pt_attr = NULL;
    }
    _tpret = ST_ERR;
    return _tpret;
  }
  memset(_pt_attr->ptbuf, 0, kplen);

  // Copy data
  memcpy(_pt_attr->ptbuf, pt_dtbuf, kplen);
  _pt_attr->len = kplen;
  _pt_attr->pt_cond_var = pt_cond_var;

  DBG_LOG_INFO("To update value of the characterisc %s (%s) in server role\n",
               _pt_chrc->uuid, _pt_chrc->path);

  if(0 == l_dbus_method_call(_pt_dbus, BT_DBUS_DES_BLUEZ_LEADV,
                             _pt_chrc->path, BT_IF_NM_GATTCHARACTERISTIC,
                             "WriteValue", msg_setup_for_notify_value,
                             msg_reply_from_notify_value, _pt_attr,
                             destroy_for_notify_value))
  {
    DBG_LOG_ERR("Failed to call WriteValue method");
    _tpret = ST_ERR;
  }

  return _tpret;
}









/*to check if the proxy given is Acquired Notification
ret@true when the Notification is Acquired
*/
bool bt_gatt_is_notification_acquired(struct l_dbus_proxy*pt_proxy)
{
	#if(0)

	bool tpret = true;
	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = false;
		return tpret;
	}
	if(pt_proxy != g_notify_io.proxy)
	{
		tpret = false;
	}
	return tpret;

	#else //________________________________
	
	bool tpret = true;
	if(pt_proxy == NULL)	
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = false;
		return tpret;
	}
	
	if(!l_dbus_proxy_get_property( pt_proxy, "NotifyAcquired", "b", &tpret))
	{
		DBG_LOG_ERR("fail to get property NotifyAcquired");
		tpret = false;
		return tpret;
	}

	DBG_LOG_INFO("%s , property NotifyAcquired %d", l_dbus_proxy_get_path(pt_proxy), tpret);
	return tpret;
	#endif
}
/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,
							void *user_data);
*/
static void msg_setup_for_charac_acquire_notify(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder*pt_builder = NULL;
	pt_builder = l_dbus_message_builder_new( message);

	l_dbus_message_builder_enter_array( pt_builder, "{sv}");

	l_dbus_message_builder_enter_dict( pt_builder, "sv");
	
	l_dbus_message_builder_leave_dict( pt_builder);
	
	l_dbus_message_builder_leave_array( pt_builder);

	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);
	return;
}
/*typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);
*/
static void msg_reply_for_charac_acquire_notify(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	int kp_fd = -1;
	uint16_t kp_mtu = 0;
	
	if(l_dbus_message_is_error(result))
	{
		const char*pt_name = NULL, *pt_text = NULL;
		if(l_dbus_message_get_error( result, &pt_name, &pt_text))
		{
			DBG_LOG_ERR("Acquire Notify fail, err_info %s, %s", pt_name, pt_text);
		}
		else
		{
			DBG_LOG_ERR("Acquire Notify fail, unknown reason");
		}
		return;
	}
	if(g_notify_io.io)
	{
		l_io_destroy(g_notify_io.io);
		g_notify_io.io = NULL;
	}
	g_notify_io.mtu = 0;

	if(l_dbus_message_get_arguments( result, "hq",&kp_fd, &kp_mtu) == false)
	{
		DBG_LOG_ERR("Invalid AcquireNotify response");
		return;
	}

	g_notify_io.mtu = kp_mtu;
	g_notify_io.io = sock_io_new( kp_fd, NULL);

	DBG_LOG_INFO("AcquireNotify goes well fd %d, mtu %d", kp_fd, kp_mtu);
	return;
}
/*to acquite notification for the specific characteristic
ret@ST_OK is returned when things go well
*/
app_state_t bt_gatt_acquire_notify(struct l_dbus_proxy*pt_proxy)
{
	app_state_t tpret = ST_OK;
	const char*pt_interface = NULL;
	
	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = ST_ERR;
		return tpret;
	}

	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if(NULL == pt_interface)
	{
		DBG_LOG_ERR("fail to get proxy interface");
		tpret = ST_ERR;
		return tpret;
	}

	if(strcmp( pt_interface, BT_IF_NM_GATTCHARACTERISTIC))
	{
		DBG_LOG_ERR("proxy with interface %s is expected here");
		tpret = ST_ERR;
		return tpret;
	}

	if(0 == l_dbus_proxy_method_call( pt_proxy, "AcquireNotify", msg_setup_for_charac_acquire_notify, msg_reply_for_charac_acquire_notify, NULL, NULL))
	{
		DBG_LOG_ERR("fail to call method AcquireNotify");
		tpret = ST_ERR;
		return tpret;
	}

	g_notify_io.proxy = pt_proxy;
	
	return tpret;
}
