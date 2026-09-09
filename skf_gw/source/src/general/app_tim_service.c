/**
 * @file    app_tim_service.c
 * @author  Tongde (Victor) Yan
 * @date    2025-11-04
 * @brief   Periodic services (e.g., periodical disk clean up) are
 *          implemented in this file.
 * @details
 */
#include <pthread.h>
#include <time.h>
#include "app_tim_service.h"
#include "app_common.h"
#include "app_io.h"
#if (SKF_GW_NEW == 1)
#include "app_pro_general_new.h"
#else /* SKF_GW_NEW == 1 */
#include "app_db.h"
#include "app_gw_scheduler.h"
#include "app_pro_general.h"
#endif /* SKF_GW_NEW != 1 */
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG

#ifndef TIMEOUT_SENSOR_OP_SECOND
#define TIMEOUT_SENSOR_OP_SECOND 1// at least 1 second
#endif /* TIMEOUT_SENSOR_OP_SECOND */

#ifndef TIMEOUT_DB_OP_SECOND
#define TIMEOUT_DB_OP_SECOND 10
#endif

#ifndef TIMEOUT_BLE_OP_SECOND
#define TIMEOUT_BLE_OP_SECOND 10
#endif

#if (SKF_GW_NEW == 1)
#ifdef UNITTEST
#if (UNITTEST == UNITTEST_UNIVERSE)
#define APP_TIM_CLEANUP_REQUIRED (1) // Enable clean up test case
#define APP_TIM_CLEANUP_AVAILABLE_SIZE_MB (800) // 800 Mbytes
#else /* UNITTEST == UNITTEST_UNIVERSE */
#define APP_TIM_CLEANUP_REQUIRED (0) // Disable clean up test case
#define APP_TIM_CLEANUP_AVAILABLE_SIZE_MB (200) // 200 Mbytes
#endif
#else /* UNITTEST */
#define APP_TIM_CLEANUP_REQUIRED (0) // Disable clean up test case
#define APP_TIM_CLEANUP_AVAILABLE_SIZE_MB (200) // 200 Mbytes
#endif /* UNITTEST */
#else /* SKF_GW_NEW == 1 */
#define APP_TIM_CLEANUP_REQUIRED (0) // Disable clean up test case
#define APP_TIM_CLEANUP_AVAILABLE_SIZE_MB (200) // 200 Mbytes
#endif /* SKF_GW_NEW != 1 */

// Only for old version. To be deprecated soon ...
#if (SKF_GW_NEW != 1)
bool ble_reading_timeout_fg = false;

bool
app_tim_get_ble_reading_timeout_fg(void)
{
  return ble_reading_timeout_fg;
}
void
app_tim_set_ble_reading_timeout_fg(bool fg)
{
  ble_reading_timeout_fg = fg;
}
// Callback for l_queue_remove_if
// typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
static bool
queue_match_to_check_sensor_op_waiting_obj(
  const void *a,
  const void *b)
{
  bool tpret = false;
  struct sensor_op_waiting *pt_sensor_op_waiting_obj =
    (struct sensor_op_waiting *)a;
  time_t kp_timval = (time_t)b;

  if(NULL == pt_sensor_op_waiting_obj)
  {
    DBG_LOG_WARN("The waiting queue is empty");
    return false;
  }
  if((kp_timval - pt_sensor_op_waiting_obj->timval) >
     TIMEOUT_SENSOR_OP_SECOND)
  {
    tpret = true;
  }
  return tpret;
}
// Callback for l_queue_remove_if
// typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
static bool
queue_match_to_check_dbop_waiting_obj(
  const void *a,
  const void *b)
{
  bool tpret = false;
  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj =
    (struct dbop_resp_waiting *)a;
  time_t kp_timval = (time_t)b;

  if(NULL == pt_dbop_resp_waiting_obj)
  {
    DBG_LOG_WARN("The queue is empty");
    return false;
  }
  if((kp_timval - pt_dbop_resp_waiting_obj->tim_val) >
     TIMEOUT_DB_OP_SECOND)
  {
    tpret = true;
  }
  return tpret;
}
// Callback for l_queue_remove_if
// typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
static bool
queue_match_to_check_resp_waiting_obj(
  const void *a,
  const void *b)
{
  bool tpret = false;
  struct resp_waiting *pt_resp_waiting_obj = (struct resp_waiting *)a;
  time_t kp_timval = (time_t)b;

  if(NULL == pt_resp_waiting_obj)
  {
    DBG_LOG_WARN("The queue is empty");
    return false;
  }
  if((kp_timval - pt_resp_waiting_obj->tim_val) > TIMEOUT_BLE_OP_SECOND)
  {
    tpret = true;
  }
  return tpret;
}
#endif /* SKF_GW_NEW != 1 */

/**
 * @brief The thread monitoring something periodically (in a coarse
 *        granularity).
 */
void *
thandler_tim_service(void *pt_para)
{
#if (SKF_GW_NEW == 1)
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  uint8_t _connect_cnt = 0;

#endif /* SKF_GW_CONFIG_MULTI_CONNECTION == 1 */
  uint32_t _periodic_dev_list_update_cnt = 0;

#endif /* SKF_GW_NEW == 1 */
  struct tm *formattedTimePtr = NULL;
  time_t kp_timval = 0;

  struct dbop_controller *pt_dbop_ctr = app_db_get_dbop_controller();
  bool database_clear_fg = false; // True indicates gabage has been cleaned
                                  // up during a given period and the free
                                  // disk is sufficient after the gabage
                                  // clean up
  uint64_t availSpaceMB = 0;
  time_t reservedTime = SYS_DATA_RESERVED_TIME_S;

  // Only for old version. To be deprecated soon ...
#if (SKF_GW_NEW != 1)
  struct sensor_op_controller *pt_sensor_op_ctr =
    app_sch_get_sensor_op_controller();
  struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;

  struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;

  struct resp_waiting_cotnroller *pt_resp_waiting_ctr =
    app_sch_get_resp_waiting_ctr();
  struct resp_waiting *pt_resp_waiting_obj = NULL;

  uint32_t kp_cnt = 0;
#endif /* SKF_GW_NEW != 1 */

  while(1)
  {
    sleep(1);

    /*******************************************************
     * 1. Periodically check connection status to update LED display
     *******************************************************/
#if (SKF_GW_NEW == 1)
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    if(app_pro_gen_get_ctr()->bt_role == BT_ROLE_CLIENT)
    {
      pthread_mutex_lock(&app_pro_gen_get_ctr()->mtx_for_bt_connection);
      _connect_cnt = 0;
      for(uint8_t _i = 0;
          _i < CONFIG_BLE_CONNECTION_MAX_NUM;
          _i++)
      {
        if(app_pro_gen_get_ctr()->bt_connection_flg[_i]
           == true)
        {
          _connect_cnt++;
          break;
        }
      }
      if(app_pro_gen_get_ctr()->bt_connection_server_flg == true)
      {
        _connect_cnt++;
      }
      pthread_mutex_unlock(&app_pro_gen_get_ctr()->mtx_for_bt_connection);
      // LED status update only when all the sensors are disconnected
      if(0 == _connect_cnt)
      {
        app_io_post_led_event(LED_EV_BLE_CON_RL);
      }
    }
#endif /* SKF_GW_CONFIG_MULTI_CONNECTION == 1 */
#endif /* SKF_GW_NEW == 1 */
    // To post an tick event to LED-controller
    app_io_post_led_event(LED_EV_TIM_TICK);

    /*******************************************************
     * 2. To check the gabage cleanup job (per hour)
     * Remove data generated several days ago (defined by
     * SYS_DATA_RESERVED_TIME_S) per hours. Remove more data if the free
     * disk space is still less than the given value (defined by
     * APP_TIM_CLEANUP_AVAILABLE_SIZE_MB).
     *******************************************************/
    kp_timval = time(NULL);
    formattedTimePtr = gmtime(&kp_timval);
    DBG_LOG_DEBUG("%04d-%02d-%02d %02d:%02d:%02d",
                  1900 + formattedTimePtr->tm_year,
                  1 + formattedTimePtr->tm_mon,
                  formattedTimePtr->tm_mday,
                  formattedTimePtr->tm_hour,
                  formattedTimePtr->tm_min,
                  formattedTimePtr->tm_sec);
#if (1)
    // skip the peak-time (the first 2 mins per hour may be the peak time
    // for db to store the data from sensor)
    // if((formattedTimePtr->tm_min >= 7) && (formattedTimePtr->tm_min <=
    // 9))
    if((formattedTimePtr->tm_min >= 5) && (formattedTimePtr->tm_min <= 8))
#else
    if((formattedTimePtr->tm_min >= 0) && (formattedTimePtr->tm_min <= 3))
#endif
    {
      if(ST_OK == DH_GetDiskAvailSpaceMB("/", &availSpaceMB))
      {
        if(availSpaceMB <= APP_TIM_CLEANUP_AVAILABLE_SIZE_MB)
        {
          // If available space is still less than the given value
          // (APP_TIM_CLEANUP_AVAILABLE_SIZE_MB), then continue cleaning up
          // gabage and shorten the reserved time (i.e., more aggresive)
          database_clear_fg = false;
          reservedTime = reservedTime / 2;

#if (APP_TIM_CLEANUP_REQUIRED == 1)
          // Clean up the faked files created by UNITTEST_UNIVERSE test
          // case
          char command[100];
          pid_t status;

          memset(command, 0, sizeof(command));
          snprintf(command,
                   sizeof(command),
                   "find /var/lib/skf-gateway/update/others -type f -name \"fake_*\" -exec rm -rf {} \\;");
          DBG_LOG_INFO("cmd: %s\r\n", command);
          status = system(command);
          if(WIFEXITED(status))
          {
            if(0 == WEXITSTATUS(status))
            {
              DBG_LOG_INFO("cleanup faked files successfully\n");
            }
            else
            {
              DBG_LOG_INFO(
                "Failed to cleanup faked files(maybe there's nothing to clean), %d\n",
                WEXITSTATUS(status));
            }
          }
          else
          {
            DBG_LOG_ERR("Failed to do faked file cleanup, %d\n",
                        WEXITSTATUS(status));
          }
#endif
        }
        else
        {
          // Otherwise, recover the reserved time
          reservedTime = SYS_DATA_RESERVED_TIME_S;
        }
      }
      if(database_clear_fg == false)
      {
        database_clear_fg = true;
        DBG_LOG_DEBUG("Clear the database, reservedTime %lld",
                      reservedTime);
        if(ST_OK != app_db_gabage_cleanup(pt_dbop_ctr,
                                          kp_timval - reservedTime,
                                          reservedTime))
        {
          DBG_LOG_ERR("Failed to do database cleanup!");
        }
        else
        {
          DBG_LOG_INFO("Database cleanup successfully!");
        }
      }
    }
    else
    {
      database_clear_fg = false;
    }

#if (SKF_GW_NEW == 1)
    /*******************************************************
     * 3. To update the device list (to the process BLE) in
     * case that the device list has been modified. The period
     * is 10 minutes.
     *******************************************************/
    _periodic_dev_list_update_cnt++;
    if(_periodic_dev_list_update_cnt >= 600)
    {
      _periodic_dev_list_update_cnt = 0;
      app_pro_update_dev_list();
    }
#endif /* SKF_GW_NEW */

    /*******************************************************
     * 3. Check the child process to avoid defuncted process
     *******************************************************/
    monitor_child_process();

    // Only for old version. To be deprecated soon ...
#if (SKF_GW_NEW != 1)
    DBG_LOG_INFO("To check and release waiting obj, kp_cnt = %u\n",
                 kp_cnt++);

    if(app_tim_get_ble_reading_timeout_fg() == true)
    {
      pthread_mutex_lock(&pt_sensor_op_ctr->mtx);
      while(1)
      {
        pt_sensor_op_waiting_obj = l_queue_remove_if(
          pt_sensor_op_ctr->pt_sensor_op_waiting_queue,
          queue_match_to_check_sensor_op_waiting_obj,
          (const void *)kp_timval);
        if(NULL == pt_sensor_op_waiting_obj)
        {
          // No object is timeout
          break;
        }
        DBG_LOG_WARN("sensor-conf reading seq-no %u is timeout",
                     pt_sensor_op_waiting_obj->msg_seq_no);
        // To signal the timeout event
        pthread_mutex_lock(&pt_sensor_op_waiting_obj->mtx);
        pt_sensor_op_waiting_obj->resp_event = EV_SENSOR_OP_TIMEOUT;
        pthread_cond_signal(&pt_sensor_op_waiting_obj->cond_resp_received);
        pthread_mutex_unlock(&pt_sensor_op_waiting_obj->mtx);
      }
      pthread_mutex_unlock(&pt_sensor_op_ctr->mtx);
    }

    pthread_mutex_lock(&pt_dbop_ctr->mtx);
    while(1)
    {
      pt_dbop_resp_waiting_obj = l_queue_remove_if(
        pt_dbop_ctr->pt_dbop_resp_waiting_queue,
        queue_match_to_check_dbop_waiting_obj, (const void *)kp_timval);
      if(NULL == pt_dbop_resp_waiting_obj)
      {
        // No object is timeout
        break;
      }
      DBG_LOG_WARN("dbop waiting obj %u is timeout",
                   pt_dbop_resp_waiting_obj->command_id);
      // To signal the timeout event
      pthread_mutex_lock(&pt_dbop_resp_waiting_obj->mtx);
      pt_dbop_resp_waiting_obj->db_resp_event = EV_DB_OP_TIMEOUT;
      pthread_cond_signal(&pt_dbop_resp_waiting_obj->cond_resp_received);
      pthread_mutex_unlock(&pt_dbop_resp_waiting_obj->mtx);
    }
    pthread_mutex_unlock(&pt_dbop_ctr->mtx);

    pthread_mutex_lock(&pt_resp_waiting_ctr->mtx);
    while(1)
    {
      pt_resp_waiting_obj = l_queue_remove_if(
        pt_resp_waiting_ctr->pt_resp_waiting_queue,
        queue_match_to_check_resp_waiting_obj, (const void *)kp_timval);
      if(NULL == pt_resp_waiting_obj)
      {
        // No object is timeout
        break;
      }
      DBG_LOG_WARN("ble waiting obj %u is timeout",
                   pt_resp_waiting_obj->msg_seq_no);
      // To signal the timeout event
      pthread_mutex_lock(&pt_resp_waiting_obj->mtx);
      pt_resp_waiting_obj->cmd_resp_event = EV_CMD_RESP_TIMEOUT;
      pthread_cond_signal(&pt_resp_waiting_obj->cond_resp_received);
      pthread_mutex_unlock(&pt_resp_waiting_obj->mtx);
    }
    pthread_mutex_unlock(&pt_resp_waiting_ctr->mtx);
#endif /* SKF_GW_NEW != 1 */
  }

  return NULL;
}

#if 0 // Deprecated
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>



#if(TIM_SERVICE_SOLUTION == TIM_SERVICE_BY_TIMER)

#warning "TODO=================to create a timer, to trig all thread waitting. in case of timeout"

/*typedef bool (*l_queue_match_func_t) (const void *a, const void *b);
to check the sensor-op-waiting object is time out or not
*/
static bool queue_match_to_check_sensor_op_waiting_obj(const void *a, const void *b)
{
	bool tpret = false;
	struct sensor_op_waiting *pt_sensor_op_waiting_obj = (struct sensor_op_waiting*)a;
	time_t kp_timval = (time_t)b;
	if(NULL == pt_sensor_op_waiting_obj)
	{
		DBG_LOG_ERR("the waiting queue is empty");
		return false;
	}
	if((kp_timval - pt_sensor_op_waiting_obj->timval) > TIMEOUT_SENSOR_OP_SECOND)
	{
		tpret = true;
	}
	return tpret;
}


/*
*/
static bool queue_match_to_check_dbop_waiting_obj(const void *a, const void *b)
{
	bool tpret = false;
	struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = (struct dbop_resp_waiting*)a;
	time_t kp_timval = (time_t)b;
	if(NULL == pt_dbop_resp_waiting_obj)
	{
		DBG_LOG_WARN("the queue is probably empty");
		return false;
	}
	if((kp_timval - pt_dbop_resp_waiting_obj->tim_val) > TIMEOUT_DB_OP_SECOND)
	{
		tpret = true;
	}
	return tpret;
}

/*we check and release the all waiting object on different queue
*/
static void sig_handler_for_timeout_service(int sig, siginfo_t *pt_sig, void*uc)
{
	struct sensor_op_controller *pt_sensor_op_ctr = NULL;
	struct sensor_op_waiting *pt_sensor_op_waiting_obj = NULL;
	const time_t kp_timval = time(NULL);

	struct dbop_controller *pt_dbop_ctr = NULL;
	struct dbop_resp_waiting *pt_dbop_resp_waiting_obj = NULL;
	//DBG_LOG_INFO("**************to check and release the timeout waiting object here************");

	// 
	DBG_LOG_INFO("BKP74");
 	pt_sensor_op_ctr = app_sch_get_sensor_op_controller();
	pthread_mutex_lock( &pt_sensor_op_ctr->mtx);
	while(1)
	{
		pt_sensor_op_waiting_obj = l_queue_remove_if( pt_sensor_op_ctr->pt_sensor_op_waiting_queue, queue_match_to_check_sensor_op_waiting_obj, (const void *)kp_timval);
		if(NULL == pt_sensor_op_waiting_obj)
		{ // no objecj on queue is timeout
			break;
		}
		DBG_LOG_WARN("sensor-conf reading seq-no %d is timeout", pt_sensor_op_waiting_obj->msg_seq_no);
		// continue to signal the timeout event
		pthread_mutex_lock( &pt_sensor_op_waiting_obj->mtx);
		pt_sensor_op_waiting_obj->resp_event = EV_SENSOR_OP_TIMEOUT;
		pthread_cond_signal( &pt_sensor_op_waiting_obj->cond_resp_received);
		pthread_mutex_unlock( &pt_sensor_op_waiting_obj->mtx);
	}
	pthread_mutex_unlock( &pt_sensor_op_ctr->mtx);	
	DBG_LOG_INFO("BKP75");
	
	// to check and release the dbop-waiting obj when it is timeout
	pt_dbop_ctr = app_db_get_dbop_controller();
	pthread_mutex_lock( &pt_dbop_ctr->mtx);
	while(1)
	{
		pt_dbop_resp_waiting_obj = l_queue_remove_if( pt_dbop_ctr->pt_dbop_resp_waiting_queue, queue_match_to_check_dbop_waiting_obj, (const void *)kp_timval);
		if(NULL == pt_dbop_resp_waiting_obj)
		{
			break;
		}
		DBG_LOG_WARN("dbop waiting obj is timeout %d", pt_dbop_resp_waiting_obj->command_id);
		// to signal the timeout event
		pthread_mutex_lock( &pt_dbop_resp_waiting_obj->mtx);
		pt_dbop_resp_waiting_obj->db_resp_event = EV_DB_OP_TIMEOUT;
		pthread_cond_signal( &pt_dbop_resp_waiting_obj->cond_resp_received);
		pthread_mutex_unlock( &pt_dbop_resp_waiting_obj->mtx);
	}
	pthread_mutex_unlock( &pt_dbop_ctr->mtx);
	return;
}


timer_t g_timerid_for_timeout_service;

/*create a timer, to release the waiting objects when they are timeout 
*/
app_state_t app_tim_create_timer_for_timeout_service(void)
{
	app_state_t tpret = ST_OK;
	timer_t kp_timeid = 0;
	struct sigaction kp_sa = {0};
	struct sigevent kp_sev = {0};
	struct itimerspec kp_its = {0};
	sigset_t kp_msk = {0};

	//to setup the handler for timer signal
	kp_sa.sa_flags = SA_SIGINFO;
	kp_sa.sa_sigaction = sig_handler_for_timeout_service;
	sigemptyset(&kp_sa.sa_mask);
	if(sigaction( SIG_TIMEOUT_SERVICE, &kp_sa, NULL) < 0)
	{
		DBG_LOG_ERR(" sigaction fail, for %s", strerror(errno));
		tpret = ST_ERR;
		return tpret;
	}

	// to block the signal temporarily
	sigemptyset(&kp_msk);
	sigaddset(&kp_msk, SIG_TIMEOUT_SERVICE);
	if(sigprocmask(SIG_SETMASK, &kp_msk, NULL) < 0)
	{
		DBG_LOG_ERR("sigpromask fail , for %s", strerror(errno));
		tpret = ST_ERR;
		return tpret;
	}

	// create the timer
	kp_sev.sigev_notify = SIGEV_SIGNAL;
	kp_sev.sigev_signo = SIG_TIMEOUT_SERVICE;
	kp_sev.sigev_value.sival_ptr = &kp_timeid;
	if(timer_create(CLOCK_REALTIME, &kp_sev, &kp_timeid) < 0)
	{
		DBG_LOG_ERR("fail to create timer , for %s", strerror(errno));
		tpret = ST_ERR;
		return tpret;
	}

	//to unblock the signal
	if(sigprocmask(SIG_UNBLOCK, &kp_msk, NULL) < 0)
	{
		DBG_LOG_ERR("fail to unblock the signal");
		tpret = ST_ERR;
		return tpret; 
	}

	// now to start the timer
	kp_its.it_value.tv_sec = 3;
	kp_its.it_value.tv_nsec = 0;
	kp_its.it_interval.tv_sec = 1; // the timer will keep running till the terminal of the application
	kp_its.it_interval.tv_nsec = 0;

	if(timer_settime(kp_timeid, 0, &kp_its, NULL) < 0)
	{
		DBG_LOG_ERR("fail to launch the timer, you are in trouble now");
		tpret = ST_ERR;
		return tpret;
	}

	g_timerid_for_timeout_service = kp_timeid;
	
	return tpret;
}



/*to destroy the 
*/
void app_tim_destroy_timer_for_timer_service(void)
{
	DBG_LOG_WARN(" to release the timer here");
	return;
}


#elif(TIM_SERVICE_SOLUTION == TIM_SERVICE_BY_THREAD)

#else
	#error " .... "	
#endif
#endif

