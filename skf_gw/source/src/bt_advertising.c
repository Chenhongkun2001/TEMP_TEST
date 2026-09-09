#include "bt_advertising.h"
#include "unittest.h"
#include <ell/ell.h>
#include "ell/dbus-client.h"
#include "stdbool.h"
#include "stdint.h"

#include "util_dbg.h"

#include "bt_common.h"

DBG_LOCAL_LOG_DEBUG


static struct ad g_bt_advertising = {0};


//#define IDX_AD_ON			(0x0)
//#define IDX_AD_PERIPHERAL	(0x02)
//#define IDX_AD_BROADCAST	(0x03)
static const char *g_ad_arguments[] = {
	"on",
	"off",
	"peripheral",
	"broadcast",
	NULL
};

// A callback called by properties_set (advertising data)
// typedef struct l_dbus_message *( *l_dbus_property_set_cb_t )(
//   struct l_dbus *,
//   struct l_dbus_message *message,
//   struct l_dbus_message_iter *new_value,
//   l_dbus_property_complete_cb_t complete,
//   void *user_data);
static struct l_dbus_message *bt_ad_property_data_setter(
  struct l_dbus *dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_iter *new_value,
  l_dbus_property_complete_cb_t complete,
  void *user_data);
// A callback called by properties_set (local name)
// typedef struct l_dbus_message *( *l_dbus_property_set_cb_t )(
//   struct l_dbus *,
//   struct l_dbus_message *message,
//   struct l_dbus_message_iter *new_value,
//   l_dbus_property_complete_cb_t complete,
//   void *user_data);
static struct l_dbus_message *bt_ad_property_localname_setter(
  struct l_dbus *dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_iter *new_value,
  l_dbus_property_complete_cb_t complete,
  void *user_data);
// A callback called by properties_set (type: peripheral / broadcast)
// typedef struct l_dbus_message *( *l_dbus_property_set_cb_t )(
//   struct l_dbus *,
//   struct l_dbus_message *message,
//   struct l_dbus_message_iter *new_value,
//   l_dbus_property_complete_cb_t complete,
//   void *user_data);
static struct l_dbus_message *bt_ad_property_type_setter(
  struct l_dbus *dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_iter *new_value,
  l_dbus_property_complete_cb_t complete,
  void *user_data);

/*to get the advertisement arguments ref@g_ad_arguments
kpidx ref@IDX_AD_xxx
ret@pointer points to the AD arguments string
*/
const char* bt_advertising_get_arguments_str(uint8_t kpidx)
{
	return g_ad_arguments[kpidx % 4];
}

/*to get the BLE advertising controller
ret@ always return the pointer points to the controller
*/
static struct ad *bt_advertising_get_adv_ctrl(void)
{
	return &g_bt_advertising;
}

/*be called to get the registeration of BLE advertising
ret@true is returned when the Advertising is registered
*/
static bool bt_ad_get_registration_state(struct ad *pt_adv)
{
	return pt_adv->registered;
}

/*to release the ad-type
*/
static void bt_ad_release_ad_type(struct ad*pt_ad)
{
	if(pt_ad == NULL)
	{
		DBG_LOG_ERR("invalid parameter");
		return;
	}
	if(pt_ad->type)
	{
		l_free(pt_ad->type);
	}
	return;
}


/*be called to set up the advertisement type
ret@true is returned when things go well
*/
static bool bt_ad_set_ad_type(struct ad*pt_adv, const char*pt_type)
{
	bool tpret = true;
	uint32_t tpval = 0;
	uint8_t *pt_strbuf = NULL;
	
	if((pt_adv == NULL)||(pt_type == NULL))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = false;
		goto ERR;
	}

	if( (strcmp( pt_type, g_ad_arguments[IDX_AD_PERIPHERAL])) && (strcmp(pt_type, g_ad_arguments[IDX_AD_BROADCAST])) )
	{ // no need to set the type
		tpret = true;
		goto ERR;
	}

	tpval = strlen(pt_type);
	tpval = tpval + 1;// +1 for the string terminal character
	pt_strbuf = l_malloc(tpval); 
	if(pt_strbuf == NULL)
	{
		DBG_LOG_ERR("fail to allocate memory");
		tpret = false;
		goto ERR;
	}
	memset( pt_strbuf, 0, tpval);
	strcpy( pt_strbuf, pt_type);

	if(pt_adv->type != NULL)
	{
		DBG_LOG_WARN("the ad-type is released before , set to a new one");
		bt_ad_release_ad_type( pt_adv);
	}
	
	pt_adv->type = pt_strbuf; 
	DBG_LOG_INFO("the ad type is:%s", pt_strbuf);
	
	return tpret;
	ERR:
		return tpret;
}


/*to get the BT-name string for advertising
ret@ bool
*/
bool bt_advertising_get_nmstr(uint8_t *pt_strbuf,const uint32_t bufsz)
{
	uint8_t kp_idstr[MAX_ID_STR_LENGTH] = {0};
	size_t tp_strlen = 0;
	snprintf( kp_idstr, sizeof(kp_idstr),"%s",sys_get_gw_id_string());
	tp_strlen = strlen(kp_idstr);
	if(tp_strlen >= 6)
	{
		snprintf(pt_strbuf, bufsz,"%s%s", NM_SKF_GW_BT_SERVER, &kp_idstr[tp_strlen - 6]);
	}
	else
	{
		DBG_LOG_WARN("to make sure the gw-idstring is set successfully");
		snprintf(pt_strbuf, bufsz,"%s", NM_SKF_GW_BT_SERVER);
	}
	return true;
}

/*to set up the BT name to broadcast
pt_name: the name string
ret@true when things go well
*/
bool bt_advertising_set_local_name(const char*pt_name)
{
	size_t kp_len = 0;
	bool tpret = true;
	struct ad*pt_ad = NULL;
	if(pt_name == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		tpret = false;
		goto ERR;
	}
	pt_ad = bt_advertising_get_adv_ctrl();
	if((pt_ad->local_name) && (!strcmp( pt_name, pt_ad->local_name)))
	{
		DBG_LOG_WARN("the same local-name is set");
		return tpret;
	}
	// to release the old name-string
	if(pt_ad->local_name)
	{
		l_free( pt_ad->local_name);
		pt_ad->local_name = NULL;
	}
	
	kp_len = strlen(pt_name) + 1 ; // +1 for the terminal character
	pt_ad->local_name = (char*)l_malloc( kp_len);
	if(pt_ad->local_name == NULL)
	{
		DBG_LOG_ERR("fail to allocate memory");
		tpret = false;
		goto ERR;
	}

	memcpy( pt_ad->local_name, pt_name, kp_len);

	//remove local-name from Includes 
	#if(1)
	if(pt_ad->name)
	{
		pt_ad->name = false;
	}
	#else
		//l_dbus_property_changed(struct l_dbus * dbus, const char * path, const char * interface, const char * property)
	#endif
	
	return tpret;	
	ERR:
		return tpret;
}


/*to set up Includes tx-power
ret@true is returned when things go well
*/
bool bt_advertising_set_includes_tx_power(bool kp_newval)
{
	struct ad*pt_ad_info = bt_advertising_get_adv_ctrl();
	pt_ad_info->tx_power = kp_newval;
	return true;
}


/*to set the advertising interval
return@true
*/
bool bt_advertising_set_adv_interval(uint32_t minval, uint32_t maxval)
{
	bool tpret = true;
	struct ad *pt_ad_info = bt_advertising_get_adv_ctrl();

	if(minval < MIN_BT_ADV_INTERVAL)
	{
		DBG_LOG_WARN("invalid min interval\n");
		tpret = false;
		goto EXIT;
	}
	if(maxval > MAX_BT_ADV_INTERVAL)
	{
		DBG_LOG_WARN("invalid max interval\n");
		tpret = false;
		goto EXIT;
	}
	if(minval > maxval)
	{
		DBG_LOG_WARN("invalid configuration");
		tpret = false;
		goto EXIT;
	}

	pt_ad_info->min_interval = minval;
	pt_ad_info->max_interval = maxval;

	DBG_LOG_INFO("the advertising interval is set to %d ~ %d", minval, maxval);

	EXIT:
		return tpret;
}

/*to set Includes name
ret@true is returned when things go well
*/
bool bt_advertising_set_includes_name(bool kp_newval)
{
	struct ad*pt_ad_info = bt_advertising_get_adv_ctrl();
	pt_ad_info->name = kp_newval;

	if(kp_newval == false)
	{
		if(pt_ad_info->local_name)
		{
			l_free( pt_ad_info->local_name);
			pt_ad_info->local_name = NULL;
		}
	}
	return true;
}

bool bt_advertising_set_includes_appearance(bool kp_newval)
{
	struct ad*pt_ad_info = bt_advertising_get_adv_ctrl();
	pt_ad_info->appearance = kp_newval;
	if(!kp_newval)
	{
		pt_ad_info->local_appearance = UINT16_MAX;
	}
	return true;
}

/*to clear AD info, set to default value
*/
void bt_advertising_clear_ad_info(void)
{
	struct ad*pt_ad_info = bt_advertising_get_adv_ctrl();
	if(pt_ad_info == NULL)
	{
		DBG_LOG_ERR("unexpected pointer NULL");
		return;
	}

	// set to default value
	pt_ad_info->discoverable = true;
	pt_ad_info->local_appearance = UINT16_MAX;


	// to clear UUIDs
	char**pt_uuids = pt_ad_info->uuids;
	for(pt_uuids; pt_uuids && *pt_uuids; pt_uuids++)
	{
		l_free(*pt_uuids);
	}
	l_free(pt_ad_info->uuids);
	pt_ad_info->uuids = NULL;
	pt_ad_info->uuids_len = 0;

	// to clear service
	if(pt_ad_info->service.uuid)
	{
		l_free( pt_ad_info->service.uuid);
		pt_ad_info->service.uuid = NULL;
	}
	memset(&pt_ad_info->service, 0, sizeof(pt_ad_info->service));

	// to clear manufacturer
	memset( &pt_ad_info->manufacturer, 0, sizeof(pt_ad_info->manufacturer));

	// to clear data
	memset( &pt_ad_info->data, 0, sizeof(pt_ad_info->data));
	
	// to clear tx-power
	pt_ad_info->tx_power = false;
	
	// to clear name
	pt_ad_info->name = false;
	if(pt_ad_info->local_name)
	{
		l_free(pt_ad_info->local_name);
		pt_ad_info->local_name = NULL;
	}
	
	// to clear appearance
	pt_ad_info->appearance = false;
	pt_ad_info->local_appearance = UINT16_MAX;

	// to clear duration
	pt_ad_info->duration = 0;

	// to clear timeout
	pt_ad_info->timeout = 0;

	// to clear secondary
	if(pt_ad_info->secondary)
	{
		l_free(pt_ad_info->secondary);	
		pt_ad_info->secondary = NULL;
	}

	// to clear interval
	pt_ad_info->min_interval = MIN_BT_ADV_INTERVAL;//0;
	pt_ad_info->max_interval = MAX_BT_ADV_INTERVAL;//0;
	
	
}

static bool bt_ad_get_discoverable(struct ad*pt_ad, bool *pt_retbuf)
{
	bool tpret = false;
	if((pt_ad == NULL)||(pt_retbuf == NULL))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = false;
		goto ERR;
	}

	*pt_retbuf = pt_ad->discoverable;
	return tpret;
	ERR:
		return tpret;
}

static void bt_ad_set_discoverable(struct ad*pt_ad, bool newstate)
{
	if(pt_ad == NULL)
	{
		DBG_LOG_ERR("unexpected null pointer");
		return;
	}
	pt_ad->discoverable = newstate;
	return;
}


/*
typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);

*/
static bool bt_ad_property_data_getter(struct l_dbus *pt_dbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	uint8_t tpu8 = 0;
	
	DBG_LOG_WARN("conitnue to have the job done");
	#if(1) // for debug  to be confirmed.
		return false;
	#endif
	
	l_dbus_message_builder_enter_dict( builder, "a{yv}");
	l_dbus_message_builder_enter_array( builder, "{yv}");


	l_dbus_message_builder_enter_dict(builder, "yv");
	tpu8 = 0x0;
	l_dbus_message_builder_append_basic( builder, 'y', (const void *)&tpu8);
	
	l_dbus_message_builder_enter_variant( builder, "y");
	tpu8 = 0x0;
	l_dbus_message_builder_append_basic( builder, 'y', (const void *)&tpu8);
	l_dbus_message_builder_leave_variant( builder);
	
	l_dbus_message_builder_leave_dict(builder);



	l_dbus_message_builder_enter_dict( builder, "yv");
	tpu8 = 0x1;
	l_dbus_message_builder_append_basic( builder, 'y', (const void *)&tpu8);
	l_dbus_message_builder_enter_variant( builder, "y");
	tpu8 = 0x77;
	l_dbus_message_builder_append_basic(  builder, 'y', (const void *)&tpu8);
	l_dbus_message_builder_leave_variant( builder);
	
	l_dbus_message_builder_leave_dict( builder);
	

	
	l_dbus_message_builder_leave_array( builder);
	l_dbus_message_builder_leave_dict( builder);
	return true;
}

/**
 * @brief A callback called by properties_set (advertising data)
 */
// typedef struct l_dbus_message *( *l_dbus_property_set_cb_t )(
//   struct l_dbus *,
//   struct l_dbus_message *message,
//   struct l_dbus_message_iter *new_value,
//   l_dbus_property_complete_cb_t complete,
//   void *user_data);
static struct l_dbus_message *
bt_ad_property_data_setter(
  struct l_dbus *dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_iter *new_value,
  l_dbus_property_complete_cb_t complete,
  void *user_data)
{
  // TODO: DO NOT support data setting so far
  DBG_LOG_DEBUG("TODO: DO NOT support data setting so far");
  return NULL;
}

/* be called to get property "LocalName"
*/
static bool bt_ad_property_localname_getter(struct l_dbus *ptdbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	bool tpret = true;
	struct ad*pt_ad_info = bt_advertising_get_adv_ctrl();
	if(pt_ad_info->local_name)
	{
		l_dbus_message_builder_append_basic( builder, 's', (const void*)pt_ad_info->local_name);
		DBG_LOG_INFO("local name is set to : %s", pt_ad_info->local_name);
	}
	else
	{
		DBG_LOG_WARN("the local-name is not set");
		tpret = false;
	}
	return tpret;
}

/**
 * @brief A callback called by properties_set (local name)
 */
// typedef struct l_dbus_message *( *l_dbus_property_set_cb_t )(
//   struct l_dbus *,
//   struct l_dbus_message *message,
//   struct l_dbus_message_iter *new_value,
//   l_dbus_property_complete_cb_t complete,
//   void *user_data);
static struct l_dbus_message *
bt_ad_property_localname_setter(
  struct l_dbus *dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_iter *new_value,
  l_dbus_property_complete_cb_t complete,
  void *user_data)
{
  // TODO: DO NOT support local setting so far
  DBG_LOG_DEBUG("TODO: DO NOT support local name setting so far");
  return NULL;
}

/*for property "Type"
*/
static bool bt_ad_property_type_getter(struct l_dbus *ptdbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	bool tpret = false;
	struct ad*pt_ad_info = bt_advertising_get_adv_ctrl();
	const char*pt_type = g_ad_arguments[IDX_AD_PERIPHERAL];
	if(pt_ad_info->type)
	{
		pt_type = pt_ad_info->type;
	}
	l_dbus_message_builder_append_basic( builder, 's', pt_type);

	return true;
}

/**
 * @brief A callback called by properties_set (type: peripheral / broadcast)
 */
// typedef struct l_dbus_message *( *l_dbus_property_set_cb_t )(
//   struct l_dbus *,
//   struct l_dbus_message *message,
//   struct l_dbus_message_iter *new_value,
//   l_dbus_property_complete_cb_t complete,
//   void *user_data);
static struct l_dbus_message *
bt_ad_property_type_setter(
  struct l_dbus *dbus,
  struct l_dbus_message *message,
  struct l_dbus_message_iter *new_value,
  l_dbus_property_complete_cb_t complete,
  void *user_data)
{
  // TODO: DO NOT support property type setting so far
  DBG_LOG_DEBUG("TODO: DO NOT support property type setting so far");
  return NULL;
}

/*for property Discoverable*/
static bool bt_ad_property_discoverable_getter(struct l_dbus *ptdbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	bool tp_bval = true;
	struct ad*pt_ad_info = bt_advertising_get_adv_ctrl();
	tp_bval = pt_ad_info->discoverable;

	l_dbus_message_builder_append_basic( builder, 'b', (const void *)&tp_bval);

	return true;
}

/*for property IncludeTxPower
*/
static bool bt_ad_property_include_txpower_getter(struct l_dbus *ptdbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	bool tp_vbal = true;
	DBG_LOG_INFO("to get property include-txpower");
	l_dbus_message_builder_append_basic( builder, 'b', (const void *)&tp_vbal);
}

/*for property Includes
*/
static bool bt_ad_property_includes_getter(struct l_dbus *ptdbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	bool tpret = false;
	struct ad*pt_ad_info = bt_advertising_get_adv_ctrl();
		
	l_dbus_message_builder_enter_dict( builder, "as");
	l_dbus_message_builder_enter_array( builder, "s");

	if(pt_ad_info->tx_power)
	{
		l_dbus_message_builder_append_basic(  builder, 's', "tx-power");
		tpret = true;
	}
	if(pt_ad_info->name)
	{
		l_dbus_message_builder_append_basic( builder, 's', "local-name");
		tpret = true;
	}
	if(pt_ad_info->appearance)
	{
		l_dbus_message_builder_append_basic( builder, 's', "appearance");	
		tpret = true;
	}
	
	l_dbus_message_builder_leave_array( builder);
	l_dbus_message_builder_leave_dict( builder);

	return tpret;
}


/*typedef bool (*l_dbus_property_get_cb_t) (struct l_dbus *,
					struct l_dbus_message *message,
					struct l_dbus_message_builder *builder,
					void *user_data);
*/
static bool bt_ad_property_min_interval_getter (struct l_dbus * pt_interface,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	bool tpret = true;
	uint32_t kp_u32_min = 0, kp_u32_max = 0;
	struct ad*pt_ad_info = bt_advertising_get_adv_ctrl();

	kp_u32_min = pt_ad_info->min_interval;
	kp_u32_max = pt_ad_info->max_interval;
	
	if(kp_u32_min > kp_u32_max)
	{
		DBG_LOG_ERR("invalid interval min %d, max %d", kp_u32_min, kp_u32_max);
		tpret = false;
		return tpret;
	}

	if(kp_u32_min < MIN_BT_ADV_INTERVAL)
	{
		DBG_LOG_ERR("invalid interval value %d", kp_u32_min);
		tpret = false;
		return tpret;
	}

	if(kp_u32_max > MAX_BT_ADV_INTERVAL)
	{
		DBG_LOG_ERR("invalid inteval value %d", kp_u32_max);
		tpret = false;
		return tpret;
	}
	
	l_dbus_message_builder_append_basic( builder, 'u', (const void *)&kp_u32_min);

	return tpret;
}

static bool bt_ad_property_max_interval_getter (struct l_dbus * pt_interface,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
{
	bool tpret = true;
	uint32_t kp_u32_min = 0, kp_u32_max = 0;
	struct ad *pt_ad_info = bt_advertising_get_adv_ctrl();

	kp_u32_min = pt_ad_info->min_interval;
	kp_u32_max = pt_ad_info->max_interval;

	if(kp_u32_min > kp_u32_max)
	{
		DBG_LOG_ERR("invalid interval min %d, max %d", kp_u32_min, kp_u32_max);
		tpret = false;
		return tpret;
	}

	if(kp_u32_min < MIN_BT_ADV_INTERVAL)
	{
		DBG_LOG_ERR("invalid interval value %d", kp_u32_min);
		tpret = false;
		return tpret;
	}

	if(kp_u32_max > MAX_BT_ADV_INTERVAL)
	{
		DBG_LOG_ERR("invalid interval value %d", kp_u32_max);
		tpret = false;
		return tpret;
	}	
	l_dbus_message_builder_append_basic( builder, 'u', (const void *)&kp_u32_max);
	
	return tpret;
}


/*static struct l_dbus_message *get_managed_objects(struct l_dbus *dbus,
						struct l_dbus_message *message,
						void *user_data)
interface method-Release
*/
static struct l_dbus_message* bt_ad_advertisement_method_release(struct l_dbus *dbus,struct l_dbus_message *message,void *user_data)
{
	DBG_LOG_INFO("to complete");
}

/*
*/
static void bt_ad_setup_interface(struct l_dbus_interface*pt_interface)
{
	bool tp_bval = false;
	DBG_LOG_INFO("Going to setup interface");

	//Method
	tp_bval = l_dbus_interface_method( pt_interface, "Release", 0, bt_ad_advertisement_method_release, "", "");
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to add method Release");
	}
	// Property
	//property-Data
	#if(1)
	tp_bval = l_dbus_interface_property( pt_interface, "Data", 0, "a{yv}", bt_ad_property_data_getter, bt_ad_property_data_setter);
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to reg property Data");
	}
	#endif
	//property-LocalName
	tp_bval = l_dbus_interface_property( pt_interface, "LocalName", 0, "s", bt_ad_property_localname_getter, bt_ad_property_localname_setter);
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to reg property LocalName");
	}
	//property-Type
	tp_bval = l_dbus_interface_property( pt_interface, "Type", 0, "s", bt_ad_property_type_getter, bt_ad_property_type_setter);	
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to reg property Type");
	}
	//property-Discoverable
	tp_bval = l_dbus_interface_property( pt_interface, "Discoverable", L_DBUS_PROPERTY_FLAG_AUTO_EMIT, "b", bt_ad_property_discoverable_getter, NULL);
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to reg property Discoverable");
	}
	// conntinue to add more properties.............
	//property-Includes
	tp_bval = l_dbus_interface_property( pt_interface, "Includes", 0, "as", bt_ad_property_includes_getter, NULL);
	if(tp_bval == false)
	{
		DBG_LOG_ERR("fail to reg property Includes");
	}
	
	// property-MinInterval
	tp_bval = l_dbus_interface_property( pt_interface, "MinInterval", 0, "u", bt_ad_property_min_interval_getter, NULL);
	if(false == tp_bval)
	{
		DBG_LOG_ERR("fail to reg property MinInterval");
	}
	//property-MaxInterval	
	tp_bval = l_dbus_interface_property( pt_interface, "MaxInterval", 0, "u", bt_ad_property_max_interval_getter, NULL);
	if(false == tp_bval)
	{
		DBG_LOG_ERR("fail to reg property MaxInterval");
	}
	#warning "TODO===============continue to add more properties here!";
	//__continue_from_here
}

/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);

*/
void bt_ad_destroy_interface(void*user_data)
{
	DBG_LOG_INFO("Remember to release the resource");
}


/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data);
*/
void bt_ad_msg_setup_for_reg_advertisement(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder*ptbuilder = NULL;
	DBG_LOG_INFO("msg setup");
	ptbuilder = l_dbus_message_builder_new( message);
	l_dbus_message_builder_append_basic( ptbuilder, 'o', BT_OBJ_PATH_HCI);

	l_dbus_message_builder_enter_array( ptbuilder, "{sv}");
	#if(0)
		l_dbus_message_builder_enter_dict( ptbuilder, "sv");

		l_dbus_message_builder_append_basic( ptbuilder, 's', "LocalName");

		l_dbus_message_builder_enter_variant( ptbuilder, "s");
		l_dbus_message_builder_append_basic( ptbuilder, 's', "RPI666");
		l_dbus_message_builder_leave_variant( ptbuilder);

		l_dbus_message_builder_append_basic(  ptbuilder, 's', "Type");
		l_dbus_message_builder_enter_variant( ptbuilder, "s");
		l_dbus_message_builder_append_basic( ptbuilder, 's', "broadcast");
		l_dbus_message_builder_leave_variant( ptbuilder);
		
		l_dbus_message_builder_leave_dict( ptbuilder);
	#endif
	l_dbus_message_builder_leave_array( ptbuilder);

	l_dbus_message_builder_finalize( ptbuilder);
	l_dbus_message_builder_destroy( ptbuilder);
}


/*
/*typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data);
*/
void bt_ad_msg_setup_for_getll(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder*ptbuilder = NULL;
	DBG_LOG_INFO("msg setup for method GetAll");
	ptbuilder = l_dbus_message_builder_new( message);

	l_dbus_message_builder_append_basic( ptbuilder, 's', BT_IF_NM_LEADVERTISEMENT);

	l_dbus_message_builder_finalize( ptbuilder);
	l_dbus_message_builder_destroy( ptbuilder);
}



/*
typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);

*/
void bt_ad_result_from_reg_advertisement(struct l_dbus_proxy *proxy, struct l_dbus_message *result,void *user_data)
{
	const char*pt_name = NULL;
	const char*pt_text = NULL;

#if (SKF_GW_NEW == 1)
		struct pthread_cond_var *pt_cond_var = NULL;
		if(user_data)
		{
			pt_cond_var = (struct pthread_cond_var*)user_data;
			pthread_mutex_lock( &pt_cond_var->mtx);
			pt_cond_var->is_op_done = true;
			pthread_cond_signal( &pt_cond_var->cond_var);
			pthread_mutex_unlock( &pt_cond_var->mtx);
		}
#endif

	if(l_dbus_message_is_error(result))
	{
		if(l_dbus_message_get_error( result, &pt_name, &pt_text))
		{
			DBG_LOG_INFO("err-info:%s, %s", pt_name, pt_text);
			goto EXIT;
		}
		else
		{
			DBG_LOG_ERR("fail to get err-info");
		}
	}
	DBG_LOG_INFO("ADV registration goes well");
	
	EXIT:
		return;
}

/*
typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
						struct l_dbus_message *result,
						void *user_data);

*/
void bt_ad_result_from_getall( struct l_dbus_message *result,void *user_data)
{
	const char*pt_name = NULL;
	const char*pt_text = NULL;
	if(l_dbus_message_is_error(result))
	{
		if(l_dbus_message_get_error( result, (const char * * )&pt_name, (const char * *)&pt_text))
		{
			DBG_LOG_INFO("err-info:%s , %s", pt_name, pt_text);
		}
		else
		{
			DBG_LOG_ERR("Fail to get err-info");
		}
		goto EXIT;
	}
	DBG_LOG_INFO("method GetAll goes well");
	EXIT:
		return;
}
/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);

*/
void bt_ad_destroy_for_reg_advertisement(void*user_data)
{
	DBG_LOG_INFO("TODO======================");
}

/*
typedef void (*l_dbus_name_acquire_func_t) (struct l_dbus *dbus, bool success,bool queued, void *user_data);

*/
static void   bt_ad_name_advertisement1_require (struct l_dbus *dbus, bool success,bool queued, void *user_data)
{
	DBG_LOG_INFO("TO complete success %d, queued %d", success, queued);
}


/*be called to start the BLE advertising
pt_porxy : the LEAdvertisement proxy
type : must be one in {on/ off/peripheral/broadcast}
ret@true is returned when things go well
*/
bool bt_advertising_register_v2(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy, const char *pt_type, struct pthread_cond_var*pt_cond_var)
{
		bool tpret = true;

	struct ad *pt_ad = NULL;
	const char*pt_interface = NULL;

	static bool is_hci_manager_enable = false;

	if((pt_dbus == NULL)||(pt_proxy == NULL)||(pt_type == NULL))
	{
		DBG_LOG_ERR("unexpected pointer null");
		tpret = false;
		goto ERR;
	}

	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if((pt_interface == NULL)||(strcmp(pt_interface, BT_IF_NM_LEADVERTISING_MANAGER)))
	{
		DBG_LOG_ERR("proxy with interface(%s) is expected", BT_IF_NM_LEADVERTISING_MANAGER);
		tpret = false;
		goto ERR;
	}
	
	if(bt_ad_get_registration_state(bt_advertising_get_adv_ctrl()))
	{
		DBG_LOG_ERR("the Advertisement is registered already");
		tpret = false;
		goto ERR;
	}

	pt_ad = bt_advertising_get_adv_ctrl();
	if(bt_ad_set_ad_type( pt_ad, pt_type) == false)
	{
		DBG_LOG_ERR("fail to set up the ad-type");
		tpret = false;
		goto ERR;
	}

	if(!strcasecmp(pt_type, g_ad_arguments[IDX_AD_BROADCAST]))
	{
		bt_ad_set_discoverable( pt_ad, false);
	}

	// to require an well-known name
	l_dbus_name_acquire( pt_dbus, BT_IF_NM_LEADVERTISEMENT, true, true, true, bt_ad_name_advertisement1_require, NULL);

	#if(0) // the original
	// to enable object manager
	if(!l_dbus_object_manager_enable( pt_dbus, BT_OBJ_PATH_HCI))
	{
		DBG_LOG_ERR("unable to enable object manager");
		tpret = false;
		//goto ERR;
	}
	#else // for test
		// to enable object manager
		if(false == is_hci_manager_enable)
		{
			if(!l_dbus_object_manager_enable( pt_dbus, BT_OBJ_PATH_HCI))
			{
				DBG_LOG_ERR("unable to enable object manager");
				tpret = false;
				//goto ERR;
			}
			else
			{
				DBG_LOG_WARN("obj manager %s is enabled successfully", BT_OBJ_PATH_HCI);
				is_hci_manager_enable = true;
			}
		}
		else
		{
			DBG_LOG_WARN("%s is enable before", BT_OBJ_PATH_HCI);
		}
	#endif
	
	#if(0)
	// add interface 
	if(!l_dbus_object_add_interface( pt_dbus, "/org/bluez/advertising", "org.freedesktop.DBus.Introspectable", NULL))
	{
		DBG_LOG_ERR("fail to add interface %s", L_DBUS_INTERFACE_INTROSPECTABLE);
		tpret = false;
		goto ERR;
	}
	#endif

	
	//to register interface
	//if(false == l_dbus_register_interface( pt_dbus, "org.bluez.LEAdvertisement1", bt_ad_setup_interface, bt_ad_destroy_interface, true))
	if(false == l_dbus_register_interface( pt_dbus, BT_IF_NM_LEADVERTISEMENT, bt_ad_setup_interface, bt_ad_destroy_interface, true))
	{
		DBG_LOG_ERR("fail to register interface %s", BT_IF_NM_LEADVERTISEMENT);
		tpret = false;
		goto ERR;
	}
	// "/org/bluez/advertising"   "org.bluez.LEAdvertisement1"	
	//if(!l_dbus_object_add_interface( pt_dbus, BT_OBJ_PATH_HCI, "org.bluez.LEAdvertisement1", NULL))
	if(!l_dbus_object_add_interface( pt_dbus, BT_OBJ_PATH_HCI, BT_IF_NM_LEADVERTISEMENT, NULL))
	{
		DBG_LOG_WARN("Fail to add interface : %s", BT_IF_NM_LEADVERTISEMENT);
		tpret = false;
		goto ERR;
	}
	else
	{
		DBG_LOG_INFO("interface %s is added", "/org/bluez/advertising");
	}
	
	#if(0)
	// add interface 
	if(!l_dbus_object_add_interface( pt_dbus, "/org/bluez/advertising", "org.freedesktop.DBus.Properties", NULL))
	{
		DBG_LOG_ERR("fail to add interface xxx.DBus.Properties");
		tpret = false;
		goto ERR;
	}
	#endif
	// to get the 
	#if(0)
	if(0 == l_dbus_proxy_method_call( pt_proxy, "GetAll", bt_ad_msg_setup_for_getll, bt_ad_result_from_getall, NULL, NULL))
	{
		DBG_LOG_ERR("fail to call method GetAll");
		tpret = false;
		goto ERR;
	}
	#else
		/*
		if(0 == l_dbus_method_call( pt_dbus, L_DBUS_INTERFACE_PROPERTIES, BT_OBJ_PATH_HCI, L_DBUS_INTERFACE_PROPERTIES, "GetAll", bt_ad_msg_setup_for_getll, bt_ad_result_from_getall, NULL, NULL))
		{
			DBG_LOG_ERR("fail to call method GetAll");
			tpret = false;
			goto ERR;	
		}*/
	#endif
	
	// to call start advertising
	if(0 == l_dbus_proxy_method_call( pt_proxy, "RegisterAdvertisement", bt_ad_msg_setup_for_reg_advertisement, bt_ad_result_from_reg_advertisement, pt_cond_var, bt_ad_destroy_for_reg_advertisement))
	{
		DBG_LOG_ERR("fail to call method RegisterAdvertisement");
		tpret = false;
		goto ERR;
	}
	
	return tpret;
	ERR:
		return tpret;
}

bool bt_advertising_register(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy, const char *pt_type)
{
	bool tpret = true;

	struct ad *pt_ad = NULL;
	const char*pt_interface = NULL;

	if((pt_dbus == NULL)||(pt_proxy == NULL)||(pt_type == NULL))
	{
		DBG_LOG_ERR("unexpected pointer null");
		tpret = false;
		goto ERR;
	}

	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if((pt_interface == NULL)||(strcmp(pt_interface, BT_IF_NM_LEADVERTISING_MANAGER)))
	{
		DBG_LOG_ERR("proxy with interface(%s) is expected", BT_IF_NM_LEADVERTISING_MANAGER);
		tpret = false;
		goto ERR;
	}
	
	if(bt_ad_get_registration_state(bt_advertising_get_adv_ctrl()))
	{
		DBG_LOG_ERR("the Advertisement is registered already");
		tpret = false;
		goto ERR;
	}

	pt_ad = bt_advertising_get_adv_ctrl();
	if(bt_ad_set_ad_type( pt_ad, pt_type) == false)
	{
		DBG_LOG_ERR("fail to set up the ad-type");
		tpret = false;
		goto ERR;
	}

	if(!strcasecmp(pt_type, g_ad_arguments[IDX_AD_BROADCAST]))
	{
		bt_ad_set_discoverable( pt_ad, false);
	}

	// to require an well-known name
	l_dbus_name_acquire( pt_dbus, BT_IF_NM_LEADVERTISEMENT, true, true, true, bt_ad_name_advertisement1_require, NULL);

	// to enable object manager
	if(!l_dbus_object_manager_enable( pt_dbus, BT_OBJ_PATH_HCI))
	{
		DBG_LOG_ERR("unable to enable object manager");
		tpret = false;
		//goto ERR;
	}
	
	#if(0)
	// add interface 
	if(!l_dbus_object_add_interface( pt_dbus, "/org/bluez/advertising", "org.freedesktop.DBus.Introspectable", NULL))
	{
		DBG_LOG_ERR("fail to add interface %s", L_DBUS_INTERFACE_INTROSPECTABLE);
		tpret = false;
		goto ERR;
	}
	#endif

	
	//to register interface
	//if(false == l_dbus_register_interface( pt_dbus, "org.bluez.LEAdvertisement1", bt_ad_setup_interface, bt_ad_destroy_interface, true))
	if(false == l_dbus_register_interface( pt_dbus, BT_IF_NM_LEADVERTISEMENT, bt_ad_setup_interface, bt_ad_destroy_interface, true))
	{
		DBG_LOG_ERR("fail to register interface %s", BT_IF_NM_LEADVERTISEMENT);
		tpret = false;
		goto ERR;
	}
	// "/org/bluez/advertising"   "org.bluez.LEAdvertisement1"	
	//if(!l_dbus_object_add_interface( pt_dbus, BT_OBJ_PATH_HCI, "org.bluez.LEAdvertisement1", NULL))
	if(!l_dbus_object_add_interface( pt_dbus, BT_OBJ_PATH_HCI, BT_IF_NM_LEADVERTISEMENT, NULL))
	{
		DBG_LOG_INFO("Fail to add interface : %s", BT_IF_NM_LEADVERTISEMENT);
		tpret = false;
		goto ERR;
	}
	else
	{
		DBG_LOG_INFO("interface %s is added", "/org/bluez/advertising");
	}
	
	#if(0)
	// add interface 
	if(!l_dbus_object_add_interface( pt_dbus, "/org/bluez/advertising", "org.freedesktop.DBus.Properties", NULL))
	{
		DBG_LOG_ERR("fail to add interface xxx.DBus.Properties");
		tpret = false;
		goto ERR;
	}
	#endif
	// to get the 
	#if(0)
	if(0 == l_dbus_proxy_method_call( pt_proxy, "GetAll", bt_ad_msg_setup_for_getll, bt_ad_result_from_getall, NULL, NULL))
	{
		DBG_LOG_ERR("fail to call method GetAll");
		tpret = false;
		goto ERR;
	}
	#else
		/*
		if(0 == l_dbus_method_call( pt_dbus, L_DBUS_INTERFACE_PROPERTIES, BT_OBJ_PATH_HCI, L_DBUS_INTERFACE_PROPERTIES, "GetAll", bt_ad_msg_setup_for_getll, bt_ad_result_from_getall, NULL, NULL))
		{
			DBG_LOG_ERR("fail to call method GetAll");
			tpret = false;
			goto ERR;	
		}*/
	#endif
	
	// to call start advertising
	if(0 == l_dbus_proxy_method_call( pt_proxy, "RegisterAdvertisement", bt_ad_msg_setup_for_reg_advertisement, bt_ad_result_from_reg_advertisement, NULL, bt_ad_destroy_for_reg_advertisement))
	{
		DBG_LOG_ERR("fail to call method RegisterAdvertisement");
		tpret = false;
		goto ERR;
	}

	return tpret;
	ERR:
		return tpret;
}

/*
typedef void (*l_dbus_message_func_t) (struct l_dbus_message *message,void *user_data);
*/
static void bt_ad_msg_setup_for_unreg_advertisement(struct l_dbus_message *message,void *user_data)
{
	struct l_dbus_message_builder *pt_builder = l_dbus_message_builder_new( message);
	if(pt_builder == NULL)
	{
		DBG_LOG_ERR("fail to create message builder");
		goto EXIT;
	}
	l_dbus_message_builder_append_basic( pt_builder, 'o', (const void *)BT_OBJ_PATH_HCI);

	l_dbus_message_builder_finalize( pt_builder);
	l_dbus_message_builder_destroy( pt_builder);
	EXIT:
		return;
}


/*
typedef void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
*/
static void bt_ad_msg_result_from_unreg_advertisement(struct l_dbus_proxy *proxy,struct l_dbus_message *result,void *user_data)
{
	struct l_dbus* pt_dbus = NULL;
	const char*pt_name = NULL, *pt_text = NULL;

	#if(1)
		struct pthread_cond_var *pt_cond_var = NULL;
		if(user_data)
		{
			pt_cond_var = (struct pthread_cond_var*)user_data;
			pthread_mutex_lock( &pt_cond_var->mtx);
			pt_cond_var->is_op_done = true;
			pthread_cond_signal( &pt_cond_var->cond_var);
			pthread_mutex_unlock( &pt_cond_var->mtx);
		}
	#endif

	if(l_dbus_message_is_error(result))
	{ // unregiste advertisement fail
		l_dbus_message_get_error( result, (const char * *)&pt_name, (const char * *)&pt_text);
		DBG_LOG_ERR("err_info:%s , %s", pt_name,pt_text);
		goto EXIT;
	}

	pt_dbus = sys_init_get_dbus( sys_get_mgt());
	if(pt_dbus == NULL)
	{
		DBG_LOG_ERR("Oops! This is not supposed to happen");
		goto EXIT;
	}
	// going to remove the interface Advertising1
	if(false == l_dbus_object_remove_interface( pt_dbus, BT_OBJ_PATH_HCI, BT_IF_NM_LEADVERTISEMENT))
	{
		DBG_LOG_ERR("fail to remove interface %s from %s", BT_IF_NM_LEADVERTISEMENT, BT_OBJ_PATH_HCI);
		goto EXIT;
	}
	// going to unregiste interface
	if(false == l_dbus_unregister_interface( pt_dbus, BT_IF_NM_LEADVERTISEMENT))
	{
		DBG_LOG_ERR("fail to unregister interface %s", BT_IF_NM_LEADVERTISEMENT);
		goto EXIT;
	}
	EXIT:
		return;
}

/*
typedef void (*l_dbus_destroy_func_t) (void *user_data);
*/
void bt_ad_destroy_for_unreg_advertisement(void *user_data)
{
	return;
}


/*function: to unregister advertisement
*/
void bt_advertising_unregister(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy, struct pthread_cond_var*pt_cond_var)
{
	const char*pt_interface = NULL;
	if((pt_dbus == NULL)||(pt_proxy == NULL))
	{
		DBG_LOG_ERR("unexpected pointer null");
		goto EXIT;
	}
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if((pt_interface == NULL)||(strcmp( pt_interface, BT_IF_NM_LEADVERTISING_MANAGER)))
	{
		DBG_LOG_ERR("proxy with interface %s is expected", BT_IF_NM_LEADVERTISING_MANAGER);
		goto EXIT;
	}

	if(0 == l_dbus_proxy_method_call( pt_proxy, "UnregisterAdvertisement", bt_ad_msg_setup_for_unreg_advertisement, bt_ad_msg_result_from_unreg_advertisement,  pt_cond_var,  bt_ad_destroy_for_unreg_advertisement))
	{
		DBG_LOG_ERR("fail to call method: unregisterAdvertisement");
	}
	DBG_LOG_INFO("LE-Advertisement is unregistered successfully");
	EXIT:
		return;
}

/*
typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
*/
static bool q_match_for_proxy_leadvertisement(const void*a, const void*b)
{
	bool tpret = true;
	struct l_dbus_proxy*pt_proxy = NULL;
	const char*pt_interface = NULL;
	
	if((a == NULL)||(b == NULL))
	{
		DBG_LOG_ERR("unexpected pointer null");
		tpret = false;
		goto ERR;
	}
	pt_proxy = *((struct l_dbus_proxy**)a);
	if(pt_proxy == NULL)
	{
		DBG_LOG_ERR("Oops! This is unexpected");
		tpret = false;
		goto ERR;
	}
	pt_interface = l_dbus_proxy_get_interface( pt_proxy);
	if(strcmp( pt_interface, (const char*)b))
	{ // not the node we want
		//DBG_LOG_ERR("");
		tpret = false;
		goto ERR;
	}
	return tpret;
	ERR:
		return tpret;
}

/*to find the proxy for LE-advertisement on the queue
ret@NULL is returned when fail to find the proxy
*/
struct l_dbus_proxy*bt_ad_find_proxy_for_leadvertisement(struct l_queue*pt_queue)
{
	struct l_dbus_proxy*pt_ret = NULL;
	if(pt_queue == NULL)
	{
		DBG_LOG_ERR("unexpected null");
		pt_ret = NULL;
		return pt_ret;
	}
	pt_ret = l_queue_find( pt_queue, q_match_for_proxy_leadvertisement, BT_IF_NM_LEADVERTISING_MANAGER);
	return pt_ret;	
}



#if 1//(UNITTEST ==  UNITTEST_ADVERTISING)
#include "bt_advertising_new.h"

static bool to_register_advertising_object(
  struct l_dbus *pt_dbus,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_type);
static bool method_call_register_advertisement(
  struct l_dbus_proxy *pt_proxy,
  struct pthread_cond_var *pt_cond_var);
static void bt_ad_msg_setup_for_reg_advertisement_beta(
  struct l_dbus_message *message,
  void *user_data);
static void bt_ad_result_from_reg_advertisement_beta(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data);
static void bt_ad_destroy_for_reg_advertisement_beta(void *user_data);
static bool method_call_unregister_advertisement(
  struct l_dbus_proxy *pt_proxy,
  struct pthread_cond_var *pt_cond_var);
static void bt_ad_msg_setup_for_unreg_advertisement_beta(
  struct l_dbus_message *message,
  void *user_data);
static void bt_ad_msg_result_from_unreg_advertisement_beta(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data);
static void bt_ad_destroy_for_unreg_advertisement_beta(void *user_data);

/**
 * @brief Prepare adv parameters and register the interface
 * @param pt_adv_ctr: The advertising controller
 * @return True if things go well
 */
bool
bt_ad_register_object(struct bluetooth_adv_controller *pt_adv_ctr)
{
  bool _ret = true;
  uint8_t _strbuf[MAX_ID_STR_LENGTH] = { 0 };
  size_t _sz = 0;

  if(NULL == pt_adv_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = false;
    return _ret;
  }

  // Initialize @p g_bt_advertising
  bt_advertising_clear_ad_info();

  // Local name
  _sz = strlen(pt_adv_ctr->gw_id_buf);
  if(_sz >= 6)
  {
    snprintf(_strbuf, sizeof(_strbuf),
             "%s%s", NM_SKF_GW_BT_SERVER, &pt_adv_ctr->gw_id_buf[_sz - 6]);
  }
  else
  {
    snprintf(_strbuf, sizeof(_strbuf), "%s", NM_SKF_GW_BT_SERVER);
  }
  bt_advertising_set_local_name(_strbuf);

  // Advertising interval
  bt_advertising_set_adv_interval(MIN_BT_ADV_INTERVAL + 10U,
                                  MIN_BT_ADV_INTERVAL + 100U);

  // Register the interface to the object
  _ret = to_register_advertising_object(pt_adv_ctr->dbus,
                                        pt_adv_ctr->proxy_le_adv_manager,
                                        bt_advertising_get_arguments_str(
                                          IDX_AD_PERIPHERAL));
  return _ret;
}
/**
 * @brief Register the advertising interface to the object
 * @param pt_dbus: Pointer to the D-BUS
 * @param pt_porxy: the LEAdvertisement proxy
 * @param pt_type: Type must be one of {on/off/peripheral/broadcast}
 * @return True if things go well
 */
static bool
to_register_advertising_object(
  struct l_dbus *pt_dbus,
  struct l_dbus_proxy *pt_proxy,
  const char *pt_type)
{
  bool _ret = true;

  struct ad *_pt_ad = NULL;
  const char *_pt_interface = NULL;

  if((pt_dbus == NULL) || (pt_proxy == NULL) || (pt_type == NULL))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = false;
    goto ERR;
  }

  _pt_interface = l_dbus_proxy_get_interface(pt_proxy);
  if((_pt_interface == NULL) || (strcmp(_pt_interface,
                                        "org.bluez.LEAdvertisingManager1")))
  {
    DBG_LOG_ERR("Unexpected proxy with interface (%s)", _pt_interface);
    _ret = false;
    goto ERR;
  }

  if(bt_ad_get_registration_state(bt_advertising_get_adv_ctrl()))
  {
    DBG_LOG_ERR("Adv has been registered");
    _ret = false;
    goto ERR;
  }

  _pt_ad = bt_advertising_get_adv_ctrl();
  if(bt_ad_set_ad_type(_pt_ad, pt_type) == false)
  {
    DBG_LOG_ERR("Failed to set up the adv type");
    _ret = false;
    goto ERR;
  }

  if(!strcasecmp(pt_type, g_ad_arguments[IDX_AD_BROADCAST]))
  {
    bt_ad_set_discoverable(_pt_ad, true);
  }

  // To require an well-known name
  l_dbus_name_acquire(pt_dbus, "org.bluez.LEAdvertisement1", true, true,
                      true, bt_ad_name_advertisement1_require, NULL);

  // To enable object manager
  if(!l_dbus_object_manager_enable(pt_dbus, "/org/bluez/advertising"))
  {
    DBG_LOG_ERR("Failed to enable adv manager");
    _ret = false;
    goto ERR;
  }

  // To register interface
  if(false == l_dbus_register_interface(pt_dbus,
                                        "org.bluez.LEAdvertisement1",
                                        bt_ad_setup_interface,
                                        bt_ad_destroy_interface,
                                        true))
  {
    DBG_LOG_ERR("Failed to register adv interface");
    _ret = false;
    goto ERR;
  }
  // To add the interface to the object
  if(false == l_dbus_object_add_interface(pt_dbus,
                                          "/org/bluez/advertising",
                                          "org.bluez.LEAdvertisement1",
                                          NULL))
  {
    DBG_LOG_ERR("Failed to add adv interface");
    _ret = false;
    goto ERR;
  }
  else
  {
    DBG_LOG_INFO("Adv interface has been added");
  }

  return _ret;
ERR:
  return _ret;
}
// Setup callback called by RegisterAdvertisement method call
// typedef
// void (*l_dbus_message_func_t) (struct l_dbus_message *message, void
// *user_data);
static void
bt_ad_msg_setup_for_reg_advertisement_beta(
  struct l_dbus_message *message,
  void *user_data)
{
  struct l_dbus_message_builder *ptbuilder = NULL;

  DBG_LOG_DEBUG("Msg setup");

  ptbuilder = l_dbus_message_builder_new(message);
  if(ptbuilder == NULL)
  {
    DBG_LOG_ERR("Failed to create message builder");
    goto EXIT;
  }

  l_dbus_message_builder_append_basic(ptbuilder, 'o',
                                      "/org/bluez/advertising");

  l_dbus_message_builder_enter_array(ptbuilder, "{sv}");
#if (0) // Just for debug
  l_dbus_message_builder_enter_dict(ptbuilder, "sv");

  l_dbus_message_builder_append_basic(ptbuilder, 's', "LocalName");

  l_dbus_message_builder_enter_variant(ptbuilder, "s");
  l_dbus_message_builder_append_basic(ptbuilder, 's', "RPI666");
  l_dbus_message_builder_leave_variant(ptbuilder);

  l_dbus_message_builder_append_basic(ptbuilder, 's', "Type");
  l_dbus_message_builder_enter_variant(ptbuilder, "s");
  l_dbus_message_builder_append_basic(ptbuilder, 's', "broadcast");
  l_dbus_message_builder_leave_variant(ptbuilder);

  l_dbus_message_builder_leave_dict(ptbuilder);
#endif
  l_dbus_message_builder_leave_array(ptbuilder);

  l_dbus_message_builder_finalize(ptbuilder);
  l_dbus_message_builder_destroy(ptbuilder);
EXIT:
  return;
}
// Reply callback called by RegisterAdvertisement method call
// typedef
// void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
// struct l_dbus_message *result, void *user_data);
static void
bt_ad_result_from_reg_advertisement_beta(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data)
{
#if (SKF_GW_NEW == 1)
  struct pthread_cond_var *pt_cond_var = NULL;

  if(user_data)
  {
    pt_cond_var = (struct pthread_cond_var *)user_data;
    pthread_mutex_lock(&pt_cond_var->mtx);
    pt_cond_var->is_op_done = true;
    pthread_cond_signal(&pt_cond_var->cond_var);
    pthread_mutex_unlock(&pt_cond_var->mtx);
  }
#endif /* SKF_GW_NEW == 1 */

  if(l_dbus_message_is_error(result))
  {
    const char *_pt_name = NULL, *_pt_text = NULL;

    // DBUS error happens
    l_dbus_message_get_error(result, &_pt_name, &_pt_text);
    DBG_LOG_ERR("Failed to register adv, err_info: %s-%s", _pt_name,
                _pt_text);
    return;
  }

  DBG_LOG_INFO("Adv registration is successful!");

EXIT:
  return;
}
// Destroy callback called by RegisterAdvertisement method call
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
static void
bt_ad_destroy_for_reg_advertisement_beta(void *user_data)
{
  DBG_LOG_DEBUG("Destroy the adv registeration but do nothing ...");
}
/**
 * @brief Call D-Bus method to start BLE advertising
 */
static bool
method_call_register_advertisement(
  struct l_dbus_proxy *pt_proxy,
  struct pthread_cond_var *pt_cond_var)
{
  bool _ret = true;
  const char *_pt_interface = NULL;

  if((NULL == pt_proxy) || (NULL == pt_cond_var))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = false;
    goto ERR;
  }

  _pt_interface = l_dbus_proxy_get_interface(pt_proxy);
  if((NULL == _pt_interface) || (strcmp(_pt_interface,
                                        "org.bluez.LEAdvertisingManager1")))
  {
    DBG_LOG_ERR("Unexpected proxy with interface (%s)", _pt_interface);
    _ret = false;
    goto ERR;
  }

  // Call method to start advertising
  if(0 == l_dbus_proxy_method_call(pt_proxy, "RegisterAdvertisement",
                                   bt_ad_msg_setup_for_reg_advertisement_beta,
                                   bt_ad_result_from_reg_advertisement_beta,
                                   pt_cond_var,
                                   bt_ad_destroy_for_reg_advertisement_beta))
  {
    DBG_LOG_ERR("Failed to call method RegisterAdvertisement");
    _ret = false;
    goto ERR;
  }
  return _ret;
ERR:
  return _ret;
}
/**
 * @brief Start BLE advertising
 * @param pt_adv_ctr: The BLE advertising controller
 * @return true if things go well
 */
bool
bt_ad_register_advertisement(struct bluetooth_adv_controller *pt_adv_ctr)
{
  bool _ret = true;

  if(NULL == pt_adv_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = false;
    goto EXIT;
  }

  pt_adv_ctr->cond_var.is_op_done = false;
  clock_gettime(CLOCK_MONOTONIC, &pt_adv_ctr->cond_var.tv);
  pt_adv_ctr->cond_var.tv.tv_sec = pt_adv_ctr->cond_var.tv.tv_sec +
                                   CONFIG_BT_TURN_ON_LE_ADVERTISEMENT_TIMEOUT_S;

  _ret =
    method_call_register_advertisement(pt_adv_ctr->proxy_le_adv_manager,
                                       &pt_adv_ctr->cond_var);
  if(true != _ret)
  {
    DBG_LOG_ERR("Failed to start BLE adv");
    goto EXIT;
  }

  pthread_mutex_lock(&pt_adv_ctr->cond_var.mtx);
  while(false == pt_adv_ctr->cond_var.is_op_done)
  {
    if(ETIMEDOUT == pthread_cond_timedwait(&pt_adv_ctr->cond_var.cond_var,
                                           &pt_adv_ctr->cond_var.mtx,
                                           &pt_adv_ctr->cond_var.tv))
    {
      DBG_LOG_ERR("Starting BLE adv timeout happened ...");
      _ret = false;
      break;
    }
  }
  pthread_mutex_unlock(&pt_adv_ctr->cond_var.mtx);

EXIT:
  return _ret;
}
// Setup callback called by UnregisterAdvertisement method call
// typedef
// void (*l_dbus_message_func_t) (struct l_dbus_message *message, void
// *user_data);
static void
bt_ad_msg_setup_for_unreg_advertisement_beta(
  struct l_dbus_message *message,
  void *user_data)
{
  struct l_dbus_message_builder *pt_builder =
    l_dbus_message_builder_new(message);

  if(pt_builder == NULL)
  {
    DBG_LOG_ERR("Failed to create message builder");
    goto EXIT;
  }

  l_dbus_message_builder_append_basic(pt_builder, 'o',
                                      "/org/bluez/advertising");

  l_dbus_message_builder_finalize(pt_builder);
  l_dbus_message_builder_destroy(pt_builder);
EXIT:
  return;
}
// Reply callback called by UnregisterAdvertisement method call
// typedef
// void (*l_dbus_client_proxy_result_func_t) (struct l_dbus_proxy *proxy,
// struct l_dbus_message *result, void *user_data);
static void
bt_ad_msg_result_from_unreg_advertisement_beta(
  struct l_dbus_proxy *proxy,
  struct l_dbus_message *result,
  void *user_data)
{
#if (1)
  struct pthread_cond_var *pt_cond_var = NULL;

  if(user_data)
  {
    pt_cond_var = (struct pthread_cond_var *)user_data;
    pthread_mutex_lock(&pt_cond_var->mtx);
    pt_cond_var->is_op_done = true;
    pthread_cond_signal(&pt_cond_var->cond_var);
    pthread_mutex_unlock(&pt_cond_var->mtx);
  }
#endif

  if(l_dbus_message_is_error(result))
  {
    const char *_pt_name = NULL, *_pt_text = NULL;

    // DBUS error happens
    l_dbus_message_get_error(result, &_pt_name, &_pt_text);
    DBG_LOG_ERR("Failed to unregister adv, err_info: %s-%s", _pt_name,
                _pt_text);
    return;
  }

  DBG_LOG_INFO("Adv unregistration is successful!");
EXIT:
  return;
}
// Destroy callback called by UnregisterAdvertisement method call
// typedef
// void (*l_dbus_destroy_func_t) (void *user_data);
static void
bt_ad_destroy_for_unreg_advertisement_beta(void *user_data)
{
  DBG_LOG_DEBUG("Destroy the adv unregisteration but do nothing ...");
}
/**
 * @brief Call D-Bus method to stop BLE advertising
 */
static bool
method_call_unregister_advertisement(
  struct l_dbus_proxy *pt_proxy,
  struct pthread_cond_var *pt_cond_var)
{
  bool _ret = true;
  const char *_pt_interface = NULL;

  if((NULL == pt_proxy) || (NULL == pt_cond_var))
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = false;
    goto EXIT;
  }

  _pt_interface = l_dbus_proxy_get_interface(pt_proxy);
  if((NULL == _pt_interface) || (strcmp(_pt_interface,
                                        "org.bluez.LEAdvertisingManager1")))
  {
    DBG_LOG_ERR("Unexpected proxy with interface (%s)", _pt_interface);
    _ret = false;
    goto EXIT;
  }

  // Call method to stop advertising
  if(0 == l_dbus_proxy_method_call(pt_proxy, "UnregisterAdvertisement",
                                   bt_ad_msg_setup_for_unreg_advertisement_beta,
                                   bt_ad_msg_result_from_unreg_advertisement_beta,
                                   pt_cond_var,
                                   bt_ad_destroy_for_unreg_advertisement_beta))
  {
    DBG_LOG_ERR("Failed to call method unregisterAdvertisement");
    _ret = false;
    goto EXIT;
  }
  return _ret;
EXIT:
  return _ret;
}
/**
 * @brief Stop BLE advertising
 * @param pt_adv_ctr: The BLE advertising controller
 * @return true if things go well
 */
bool
bt_ad_unregister_advertisement(struct bluetooth_adv_controller *pt_adv_ctr)
{
  bool _ret = true;

  if(NULL == pt_adv_ctr)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = false;
    goto EXIT;
  }

  pt_adv_ctr->cond_var.is_op_done = false;
  clock_gettime(CLOCK_MONOTONIC, &pt_adv_ctr->cond_var.tv);
  pt_adv_ctr->cond_var.tv.tv_sec = pt_adv_ctr->cond_var.tv.tv_sec +
                                   CONFIG_BT_TURN_OFF_LE_ADVERTISEMENT_TIMEOUT_S;

  _ret = method_call_unregister_advertisement(pt_adv_ctr->proxy_le_adv_manager, &pt_adv_ctr->cond_var);
  if(true != _ret)
  {
    DBG_LOG_ERR("Failed to stop BLE adv");
    goto EXIT;
  }

  pthread_mutex_lock(&pt_adv_ctr->cond_var.mtx);
  while(false == pt_adv_ctr->cond_var.is_op_done)
  {
    if(ETIMEDOUT == pthread_cond_timedwait(&pt_adv_ctr->cond_var.cond_var,
                                           &pt_adv_ctr->cond_var.mtx,
                                           &pt_adv_ctr->cond_var.tv))
    {
      DBG_LOG_ERR("Stopping BLE adv timeout happened ...");
      _ret = false;
      break;
    }
  }
  pthread_mutex_unlock(&pt_adv_ctr->cond_var.mtx);
  
EXIT:
  return _ret;
}

#endif




