#include "app_local_service.h"
#include "util_dbg.h"
#include "bt_gatt.h"

DBG_LOCAL_LOG_DEBUG


/*to register the local BLE service
ret@ST_OK is returned when things go well
*/
app_state_t app_register_local_service( struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy)
{
	app_state_t tpret = ST_ERR;

	if(pt_dbus == NULL)
	{
		DBG_LOG_ERR("unexpected parameter");
		tpret = ST_ERR;
		return tpret;
	}

	if(!l_dbus_object_manager_enable( pt_dbus, "/"))
	{
		DBG_LOG_ERR("fail to enable object manager enable");
		tpret = ST_ERR;
		return tpret;
	}
	
	// to register the service
	if(ST_OK != bt_gatt_register_service( pt_dbus, NORDIC_UART_SERVICE_UART, 0, true))
	{
		DBG_LOG_ERR("fail to register service %s", NORDIC_UART_SERVICE_UART);
		tpret = ST_ERR;
		return tpret;
	}
	DBG_LOG_INFO("service %s is register successfully\n", NORDIC_UART_SERVICE_UART);

	// to rgetiste the characteristic
	if(ST_OK != bt_gatt_register_characteristic( pt_dbus, NORDIC_UART_CHARAC_TX_UUID, "notify", 0))
	{
		DBG_LOG_ERR("fail to register characteristic %s\n", NORDIC_UART_CHARAC_TX_UUID);
		tpret = ST_ERR;
		return tpret;
	}
	else
	{
		DBG_LOG_INFO("characteristic %s is registered successfully\n", NORDIC_UART_CHARAC_TX_UUID);
		// to continue to register the descriptor
		#if(0)	
		if(ST_OK != bt_gatt_register_descriptor( pt_dbus, NORDIC_UART_DESC_TX_UUID, "read,write", 0))
		{
			DBG_LOG_ERR("fail to register descriptor");
			tpret = ST_ERR;
			return tpret;
		}
		#else 
			#warning "What the hell!!!===to figure this out, why no need to call bt_gatt_register_descriptor"
		#endif
	}

	#if(1)
	if(ST_OK != bt_gatt_register_characteristic( pt_dbus, NORDIC_UART_CHARAC_RX_UUID, "write", 0))
	{
		DBG_LOG_ERR("fail to register characteristic %s", NORDIC_UART_CHARAC_RX_UUID);
		tpret = ST_ERR;
		//return tpret;
	}
	#endif

	// to register the application
	if(ST_OK != bt_gatt_register_application( pt_dbus,  pt_proxy) )
	{
		DBG_LOG_ERR("fail to register application");
		tpret = ST_ERR;
		return tpret;
	}
	DBG_LOG_INFO("the BLE application is registered successfully\n");
	
	return tpret;
}







