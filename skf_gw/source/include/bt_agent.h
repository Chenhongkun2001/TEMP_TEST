#ifndef  __BT_AGENT_H
#define __BT_AGENT_H

#include "sys_def.h"

/*ref@static const char*g_io_capabiliry[]*/
enum io_capability_idx{
	IDX_IO_DISPLAYONLY = 0,
	IDX_IO_DISPLAY_YES_NO = 1,
	IDX_IO_BORADDISPLAY = 2,
	IDX_IO_KEYBOARDONLY = 3,
	IDX_IO_NOINPUTNOOUTPUT = 4,
};


app_state_t bt_agent_register(struct l_dbus*pt_dbus, struct l_dbus_proxy*pt_proxy, const char*pt_io_str);
const char* bt_agent_get_io_capability_string(enum io_capability_idx kpidx);


#endif

