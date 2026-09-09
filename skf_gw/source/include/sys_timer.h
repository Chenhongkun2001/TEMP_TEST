#ifndef __SYS_TIMER_H
#define __SYS_TIMER_H
#include <stdbool.h>
#include <ell/timeout.h>

	void sys_timer_callback(struct l_timeout *timeout,void *user_data);
	void sys_timer_destroy_callback(void *user_data);


#endif



