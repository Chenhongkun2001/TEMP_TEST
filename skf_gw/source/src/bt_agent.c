#include "bt_agent.h"

#include <ell/ell.h>

#include "util_dbg.h"
#include "bt_common.h"


DBG_LOCAL_LOG_DEBUG


static const char*g_io_capabiliry[] = {
	"DisplayOnly",
	"DisplayYesNo",
	"KeyboardDisplay",
	"KeyboardOnly",
	"NoInputNoOutput",
};


/*to get the IO-capability string
*/
const char* bt_agent_get_io_capability_string(enum io_capability_idx kpidx)
{
	return g_io_capabiliry[kpidx % 5];
}


/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data)
*/
static struct l_dbus_message *agent_cb_release (struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_INFO("You can do some clean-up here");
	return NULL;
}

/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data)
*/
struct l_dbus_message *agent_cb_request_pincode(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	#warning "TODO===to complete this function"
	return NULL;
}

/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data)
*/
static struct l_dbus_message *agent_cb_display_pincode (struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	#warning "TODO===to complete this function"
	return NULL;
}

/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data)
*/

static struct l_dbus_message *agent_cb_request_passkey(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_INFO("TODO====to complete this function");
	return NULL;
}

/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data)
*/
static struct l_dbus_message *agent_cb_display_passkey(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_ERR("TODO===to complete this function");
	return NULL;
}

/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data)
*/
static struct l_dbus_message *agent_cb_request_confirmation(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_ERR("TODO===to complete this function");
	return NULL;
}


/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data)
*/
static struct l_dbus_message *agent_cb_request_authorization (struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_ERR("TODO===to complete this function");
	return NULL;
}


/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data)
*/
static struct l_dbus_message *agent_cb_authorize_service(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_ERR("TODO===to complete this function");
	return NULL;
}

/*
typedef struct l_dbus_message *(*l_dbus_interface_method_cb_t) (struct l_dbus *,
						struct l_dbus_message *message,
						void *user_data)
*/
static struct l_dbus_message *agent_cb_cancel(struct l_dbus *pt_dbus,struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_ERR("TODO===to complete this function");
	return NULL;
}




/*typedef void (*l_dbus_interface_setup_func_t) (struct l_dbus_interface *);
*/
static void setup_for_interface_agent(struct l_dbus_interface * pt_interface)
{
	bool tp_bval = false;
	DBG_LOG_INFO("TODO===add method and property");
	//method
	tp_bval = l_dbus_interface_method( pt_interface, "Release", 0, agent_cb_release, "", "");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to add method Release");
	}
	tp_bval = l_dbus_interface_method( pt_interface, "RequestPinCode", 0, agent_cb_request_pincode, "s", "o");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to add method RequestPinCode");
	}
	tp_bval = l_dbus_interface_method( pt_interface, "DisplayPinCode", 0, agent_cb_display_pincode, "", "os");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to add method DisplayCode");		
	}
	tp_bval = l_dbus_interface_method( pt_interface, "RequestPasskey", 0, agent_cb_request_passkey, "u", "o");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to add method RequestPasskey");
	}
	tp_bval = l_dbus_interface_method( pt_interface, "DisplayPasskey", 0, agent_cb_display_passkey, "", "ou");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to add method DisplayPasskey");
	}
	tp_bval = l_dbus_interface_method( pt_interface, "RequestConfirmation", 0, agent_cb_request_confirmation, "", "ou");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to add method RequestConfirmation");
	}
	tp_bval = l_dbus_interface_method( pt_interface, "RequestAuthorization", 0, agent_cb_request_authorization, "", "o");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to add method RequestAuthorization");
	}
	tp_bval = l_dbus_interface_method( pt_interface, "AuthorizeService", 0, agent_cb_authorize_service, "", "os");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to add method AuthorizeService");
	}
	tp_bval = l_dbus_interface_method( pt_interface, "Cancel", 0, agent_cb_cancel, "", "");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to add method Cancel");
	}
	return;
}

/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
static void destroy_for_interface_agent(void *user_data)
{
	DBG_LOG_WARN("TODO===to complete this function");
	return;
}

/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data);
*/
static void msg_setup_for_agent_register(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder *pt_builder = NULL;
	
	DBG_LOG_INFO("TODO===continue to complete this function,io-capability:%s", (char*)user_data);
	pt_builder = l_dbus_message_builder_new( message);
	l_dbus_message_builder_append_basic( pt_builder, 'o', BT_OBJ_PATH_HCI);
	l_dbus_message_builder_append_basic( pt_builder, 's', (const void *)user_data);
	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);
	return;
}

/*typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);
*/
static void reply_for_agent_register(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	//DBG_LOG_WARN("TODO====continue to complete this function");
	if(l_dbus_message_is_error(result))
	{
		const char*pt_name = NULL, *pt_text = NULL;
		if(l_dbus_message_get_error( result, &pt_name, &pt_text))
		{
			DBG_LOG_ERR("fail to register %s, err_info:%s, %s", BT_IF_NM_AGENT, pt_name, pt_text);
		}
		else
		{
			DBG_LOG_ERR("fail  to register %s", BT_IF_NM_AGENT);
		}
	}
	else
	{
		DBG_LOG_INFO("proxy %s is registered  successfully, io-cap:%s", BT_IF_NM_AGENT,(char*)user_data);
	}
	return;
}


/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
void destroy_for_agent_register(void *user_data)
{
	DBG_LOG_WARN("TODO===continue to complete this function to free %s", ((char*)user_data));
	// to free the memory
	if(user_data)
	{
		//l_free(user_data);
	}
	return;
}


/*to register an agent
*/
app_state_t bt_agent_register(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy, const char*pt_io_str)
{
	app_state_t tpret = ST_ERR;
	char*pt_str = NULL;
	size_t kp_sz = 0;
	
	if((pt_dbus == NULL)||(pt_io_str == NULL) || (pt_proxy == NULL))
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = ST_ERR;
		goto EXIT;
	}

	
	if(false == l_dbus_register_interface( pt_dbus, BT_IF_NM_AGENT, setup_for_interface_agent, destroy_for_interface_agent, true))
	{
		DBG_LOG_ERR("fail to register interface %s", BT_IF_NM_AGENT);
		tpret = ST_ERR;
		goto EXIT;
	}

	
	if(false == l_dbus_object_add_interface( pt_dbus, BT_OBJ_PATH_HCI, BT_IF_NM_AGENT, NULL))
	{
		DBG_LOG_ERR("fail to add interface %s to  %s", BT_IF_NM_AGENT, BT_OBJ_PATH_HCI);
		tpret = ST_ERR;
		goto EXIT;
	}

	kp_sz = strlen(pt_io_str) + 1; // +1 for the terminal character
	pt_str = (char*)l_malloc(kp_sz);
	if(pt_str == NULL)
	{
		DBG_LOG_ERR("fail to allocate memory");
		tpret = ST_ERR;
		goto EXIT;
	}
	memset( pt_str, 0, kp_sz);
	memcpy (pt_str,  pt_io_str, kp_sz);
	// continue to register the Agent
	if(0 == l_dbus_proxy_method_call( pt_proxy, "RegisterAgent", msg_setup_for_agent_register, reply_for_agent_register, (void *)pt_str, destroy_for_agent_register))
	{
		DBG_LOG_ERR("fail to call method RegisterAgent");
		tpret = ST_ERR;

		//to free the memory
		//l_free(pt_str);
		//NOTE!!! the reply_xx and destroy_xx call-back will be called even fail to call method, so free the memory in the callback will be more elegant 
		
		goto EXIT;
	}
	EXIT:
		return tpret;
}


app_state_t bt_agent_unregister(void)
{
	#warning "TODO ==== to complete this function"
}




