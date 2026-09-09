/*for MODBUS-RTU
20241028
*/
#include "util_dbg.h"
#include "modbus.h"
#include "errno.h"

int main(int argc, char**argv)
{
	modbus_t *pt_modbus_port = NULL;
	int tpint = 0;
	
	DBG_LOG_INFO("===Applicaiton for MODBUS-RTU is running===");

	pt_modbus_port = modbus_new_rtu( "/dev/ttyS3", 115200, 'N', 8, 1);

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

	for(uint32_t i = 0; i < 65536; i++)
	{
		tpint = modbus_write_register( pt_modbus_port, 0x1 + i, 0x0 + i);
		if( tpint < 0)
		{
			DBG_LOG_ERR(" fail to write register, for %d", errno);
		}
	}



	modbus_close( pt_modbus_port);
	modbus_free( pt_modbus_port);
	
	return 0;
}
















