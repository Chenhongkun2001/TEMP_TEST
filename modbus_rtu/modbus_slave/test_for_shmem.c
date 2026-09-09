/*for shared-memory test
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h> 	
#include <unistd.h>
#include <errno.h>
#include <sys/shm.h>
#include <string.h>
#include <semaphore.h>

#include "app_mslave.h"
#include "app_mslave_ipc.h"
#include "../../skf_gw/source/include/common/util_dbg.h"
#include "sh_mem.h"
DBG_LOCAL_LOG_DEBUG

//weak
void get_curr_time(char *buf, uint8_t buf_len)
{
	;
}
// for logging
uint8_t g_log_level = CONFIG_LOG_LEVEL_SET;

void main(void)
{
	enum error_code tpret = ERR_NONE;
	
	
	int32_t kp_shmid = -1, tpi32 = -1;
	struct mslave_ipc_dtblock *pt_ipc_dtblock = -1;
	uint8_t *pt_mem = NULL;
	
	key_t kp_shm_key = 0;
	sem_t *pt_sem = NULL;
	
	DBG_LOG_INFO("for shared-memory test");
	
	
	//to create a file
	if(0 != access( FPATH_FOR_SHM, F_OK))
	{// to create the file when it does not exist
		tpi32 = creat( FPATH_FOR_SHM, S_IRWXU|S_IRWXG|S_IRWXO);
		if(tpi32 < 0)
		{
			DBG_LOG_ERR("fail to create file %s, for %s", FPATH_FOR_SHM, strerror(errno));
			tpret = ERR_API_FAIL;
			goto EXIT;
		}
		close(tpi32);
	}

	//to get the shm-key
	kp_shm_key = ftok( FPATH_FOR_SHM, PROJECT_ID_FOR_SHM);
	if(kp_shm_key < 0)
	{
		DBG_LOG_ERR("ftok fail, for %s", strerror(errno));
		tpret = ERR_API_FAIL;
		goto EXIT;
	}
	
	DBG_LOG_WARN("the shared-memory key is 0x%x", kp_shm_key);
	
	//
	kp_shmid = shmget( kp_shm_key, APP_SHM_TOTAL_SIZE, IPC_CREAT|S_IRWXU|S_IRWXG|S_IRWXO);	
	//kp_shmid = shmget( kp_shm_key, getpagesize(), IPC_CREAT|S_IRUSR|S_IWUSR);
	if(kp_shmid < 0)
	{
		DBG_LOG_ERR("shmget fail,pagesize=%d FK ,for %s", getpagesize(),strerror(errno));
		tpret = ERR_API_FAIL;
		goto EXIT;
	}


	DBG_LOG_WARN("shm-ID %d", kp_shmid);
	
	pt_mem = (uint8_t*)shmat( kp_shmid, NULL, 0);
	if( pt_mem < 0)
	{
		DBG_LOG_ERR("shmat fail, for %s", strerror(errno));
		tpret = ERR_API_FAIL;
		goto EXIT;
	}

	/*
	if(shmctl( kp_shmid, IPC_RMID, 0) < 0)
	{
		DBG_LOG_ERR("shmat fail, for %s", strerror(errno));
		tpret = ERR_API_FAIL;
		goto EXIT;	
	}*/

	// to create a semaphore for sychronization
	//pt_sem = sem_open( IPC_SEM_NAME, O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO,  1);	
	if(0 != sem_unlink( SHMEM_NAMED_SEMAPHORE_FILENAME))
	{
		DBG_LOG_ERR("fail to remove the named semaphore");
	}
	pt_sem = sem_open( SHMEM_NAMED_SEMAPHORE_FILENAME, O_CREAT, PERMISSION_FLAG_FOR_SHM,  1);
	if(pt_sem == SEM_FAILED)
	{
		DBG_LOG_ERR("sem_open fail, for %s", strerror(errno));
		tpret = ERR_API_FAIL;
		goto EXIT;
	}
	else
	{
		tpi32 = 0;
		if(0 != sem_getvalue( pt_sem, &tpi32))
		{
			DBG_LOG_ERR("sem_getvalue fail, for %s", strerror(errno));
		}
		else
		{
			DBG_LOG_INFO("the sem-value is %d", tpi32);
		}
	}
	
	DBG_LOG_WARN("the shared-memory@0x%X, sem @0x%x", pt_mem, pt_sem);
	while(1)
	{
		//sleep(3);
		tpi32 = 0;
		if(0 != sem_getvalue( pt_sem, &tpi32))
		{
			DBG_LOG_ERR("sem_getvalue fail, for %s", strerror(errno));
		}
		else
		{
			DBG_LOG_INFO("the sem-value is %d", tpi32);
		}
		
		DBG_LOG_INFO("to update the shared-memory %d\n", *pt_mem);
		sem_wait( pt_sem);
		sleep(3);
		*pt_mem = *pt_mem + 1;	
		sem_post( pt_sem);
	}

	EXIT:
		DBG_LOG_ERR("exit APP");
		return;
}







