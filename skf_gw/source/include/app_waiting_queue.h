#ifndef APP_WAITING_QUEUE_H
#define APP_WAITING_QUEUE_H

#include "pthread.h"
#include "semaphore.h"
#include "app_general_def.h"
#include "sys_def.h"

/*data structure for waiting queue
*/
struct waiting_queue_controller{
	struct l_queue *pt_data_queue; // you can decide the data waitting on the queue
	sem_t sem_data_num; // the number of the data item on the queue above
	pthread_mutex_t mtx; // for atomic operation 
};

app_state_t app_wq_init_ctr(struct waiting_queue_controller *pt_wq_ctr);

struct data_unit *app_wq_wait_for_data_trans_queue(struct waiting_queue_controller*pt_wq_ctr);
app_state_t app_wq_post_to_waiting_queue(struct waiting_queue_controller  *pt_wq_ctr, uint8_t*pt_dtbuf, const uint32_t kplen);
void app_wq_release_data_unit(struct data_unit*pt_dtunit);
void app_wq_destroy_ctr(struct waiting_queue_controller *pt_wq_ctr);


#endif

