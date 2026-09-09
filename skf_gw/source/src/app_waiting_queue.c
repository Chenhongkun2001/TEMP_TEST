/*NOTE!!! Make sure APIs defined here is thread-safe, they might be called in different threads/process
*/

#include "app_waiting_queue.h"
#include "sys_def.h"
#include "app_general_def.h"
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG


/*to initialize the BT data-trans controller
*/
app_state_t app_wq_init_ctr(struct waiting_queue_controller *pt_wq_ctr)
{
	app_state_t tpret = ST_OK;
	if(NULL == pt_wq_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = ST_ERR;
		return tpret;
	}
	
	pt_wq_ctr->pt_data_queue = l_queue_new();

	if(pthread_mutex_init( &pt_wq_ctr->mtx, NULL))
	{
		DBG_LOG_ERR("fail to init mtx");
		tpret = ST_ERR;
		goto ERR;
	}
	if(sem_init( &pt_wq_ctr->sem_data_num, 0, 0))
	{
		DBG_LOG_ERR("fail to init semaphore");		
		tpret = ST_ERR;
		goto ERR;
	}
	
	return tpret;
	ERR:
		if(pt_wq_ctr->pt_data_queue)
		{
			l_queue_destroy( pt_wq_ctr->pt_data_queue, NULL);
		}
		sem_destroy( &pt_wq_ctr->sem_data_num);	
		pthread_mutex_destroy( &pt_wq_ctr->mtx);

		return tpret;
}


/*to post a data-unit to the queue
ret@ST_OK when things go well
*/
app_state_t app_wq_post_to_waiting_queue(struct waiting_queue_controller  *pt_wq_ctr, uint8_t*pt_dtbuf, const uint32_t kplen)
{
	app_state_t tpret = ST_OK;
	struct data_unit *pt_dtunit = NULL;
	if((NULL == pt_wq_ctr)||(NULL == pt_dtbuf)||(0 == kplen))
	{
		DBG_LOG_ERR("invalid parameter");
		tpret = ST_ERR;
		return tpret;	
	}

	pt_dtunit = (struct data_unit*)l_malloc(sizeof(struct data_unit));
	if(NULL == pt_dtbuf)
	{
		// DBG_LOG_ERR("fail to malloc data-unit");
		goto ERR;
	}
	memset( pt_dtunit, 0, sizeof(struct data_unit));

	pt_dtunit->pt_buf = (uint8_t*)l_malloc(kplen);
	if(NULL == pt_dtunit->pt_buf)
	{
		// DBG_LOG_ERR("fail to malloc buffer for data");
		goto ERR;
	}
	memset( pt_dtunit->pt_buf, 0, kplen);
	memcpy( pt_dtunit->pt_buf, pt_dtbuf, kplen);
	pt_dtunit->len = kplen;

	
	pthread_mutex_lock( &pt_wq_ctr->mtx);	
	l_queue_push_tail( pt_wq_ctr->pt_data_queue, pt_dtunit);	

#if (0)
	DBG_LOG_INFO("queue-len %d", l_queue_length( pt_wq_ctr->pt_data_queue));	
	DBG_LOG_INFO("data-buf@0x%x is put on the queue, len = %d", pt_dtunit, pt_dtunit->len);
#else
	DBG_LOG_INFO("q-len %d, d-buf@0x%x, d-len = %d", l_queue_length( pt_wq_ctr->pt_data_queue), pt_dtunit, pt_dtunit->len);	
#endif
	sem_post( &pt_wq_ctr->sem_data_num);
	pthread_mutex_unlock( &pt_wq_ctr->mtx);
		
	return tpret;
	ERR:
		DBG_LOG_WARN("to release the memory");
		if(pt_dtunit)
		{
			if(pt_dtunit->pt_buf)
			{
				l_free( pt_dtunit->pt_buf);
				pt_dtunit->pt_buf = NULL;
			}
			l_free( pt_dtunit);
			pt_dtunit = NULL;
		}
	
		tpret = ST_ERR;
		return tpret;
	
}



/*to wait for a data-unit, this function will return a data-unit if there is any on the queue, otherwise it will be blocked
ret@pointer points to a data-unit, NULL might be returnd

NOTE!! you have to release the data-unit memory for the reason that it is allocated from heap.ref@app_com_post_to_data_trans_queue
*/
struct data_unit *app_wq_wait_for_data_trans_queue(struct waiting_queue_controller*pt_wq_ctr)
{
	struct data_unit*pt_dtunit = NULL;
	if(NULL == pt_wq_ctr)
	{	
		DBG_LOG_ERR("unexpected NULL");
		return NULL;
	}

	
	sem_wait( &pt_wq_ctr->sem_data_num);
	pthread_mutex_lock( &pt_wq_ctr->mtx);
	pt_dtunit = l_queue_pop_head( pt_wq_ctr->pt_data_queue);
	pthread_mutex_unlock( &pt_wq_ctr->mtx);
	return pt_dtunit;
}	

/*to release the data-unit memory
*/
void app_wq_release_data_unit(struct data_unit*pt_dtunit)
{
	if(NULL == pt_dtunit)
	{
		DBG_LOG_ERR("unexpected NULL");
		return;
	}
	if(pt_dtunit->pt_buf)
	{
		l_free(pt_dtunit->pt_buf);
		pt_dtunit->pt_buf = NULL;
	}
	l_free( pt_dtunit);
}



/*to destroy the data-trans controller
*/
void app_wq_destroy_ctr(struct waiting_queue_controller *pt_wq_ctr)
{
	if(NULL == pt_wq_ctr)
	{
		DBG_LOG_ERR("unexpected NULL");
		return ;
	}
	l_queue_destroy( pt_wq_ctr->pt_data_queue, app_wq_release_data_unit);
	sem_destroy( &pt_wq_ctr->sem_data_num);
	pthread_mutex_destroy( &pt_wq_ctr->mtx);
	return;
}



