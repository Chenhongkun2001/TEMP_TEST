#include "sh_mem.h"
#include "global.h"
#include "stdio.h"
#include "unistd.h"
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
DBG_LOCAL_LOG_DEBUG

sem_t *g_shm_sem = SEM_FAILED;
int g_shm_id = -1;
static char tmpstring[100] = {0};

/**
 * @brief generate key based on key file & project id(ftok())
 * @return @p kp_key : on success, the generated key_t is returned
 *                     on failure -1 is returned with <errno> set  
 */
key_t
app_shmem_ftok(void)
{
    key_t kp_key = -1;
    int key_fd = -1;
 
    key_fd = open(FPATH_FOR_SHM,
                  O_CREAT | O_RDWR,
                  S_IRUSR | S_IWUSR);
 
    if(key_fd < 0)
    {
        LOG_ERR(OUTPOINT,
                "Fail to create IPC key file-%s:%s\n",
                FPATH_FOR_SHM,
                strerror(errno));
        return (-1);
    }
 
    close(key_fd);
 
    kp_key = ftok(FPATH_FOR_SHM, PROJECT_ID_FOR_SHM);
    if(-1 == kp_key)
    {
        LOG_ERR(OUTPOINT, "Fail to create key-%d!\n", errno);
        return (-1);
    }
 
    LOG_DEBUG(OUTPOINT, "shm create key:%d!\n", kp_key);
    return kp_key;
}

/**
 * @brief init and open a named semaphore(sem_open())
 * @return @p g_shm_sem : on success, return 0
 *                        on failure -1 is returned with <errno> set  
 */
int
app_shmem_sem_open(void)
{
  g_shm_sem = sem_open(SHMEM_NAMED_SEMAPHORE_FILENAME, O_CREAT, PERMISSION_FLAG_FOR_SHM, 0);
  if(SEM_FAILED == g_shm_sem)
  {
    LOG_ERR(OUTPOINT, "Fail to create named sem-%d!\n", errno);
    return (-1);
  }
  LOG_INFO(OUTPOINT, "create named sem pass!\n");
  return 0;
}

/**
 * @brief unlock a semaphore(sem_post())
 */
int
app_shmem_sem_post(void)
{
  return sem_post(g_shm_sem);
}

/**
 * @brief lock a semaphore(sem_wait())
 */
int
app_shmem_sem_wait(void)
{
  return sem_wait(g_shm_sem);
}

/**
 * @brief lock a semaphore in trywait way(sem_trywait())
 */
int
app_shmem_sem_trywait(void)
{
  return sem_trywait(g_shm_sem);
}

/**
 * @brief close a semaphore(sem_close())
 */
int
app_shmem_sem_close(void)
{
  return sem_close(g_shm_sem);
}

/**
 * @brief allocates a System V shared memory segment(shmget())
 * @param @p shm_key : key_t type value returned by app_shmem_ftok()
 * @return @p g_shm_id : on success, a valid shm id is returned
 *                       on failure -1 is returned with <errno> set  
 */
int
app_shmem_shmget(key_t shm_key)
{
  if(-1 == shm_key)
  {
    LOG_WARN(OUTPOINT, "wrong key, create it first!\n");
    return shm_key;
  }
  g_shm_id = shmget(shm_key, APP_SHM_TOTAL_SIZE, IPC_CREAT | IPC_EXCL | PERMISSION_FLAG_FOR_SHM);
  if(-1 == g_shm_id)
  {
    if(EEXIST == errno) // try to fetch exist shmid (if it's already created before)
    {
      g_shm_id = shmget(shm_key, 0, 0);
    }
    if(-1 == g_shm_id)
    {
      LOG_ERR(OUTPOINT, "Fail to allocate shmem-%d!\n", errno);
      // exit(EXIT_FAILURE);
    }
  } else
  {
    LOG_DEBUG(OUTPOINT, "shm allocated:%d!\n", g_shm_id);
  }
  return g_shm_id;
}

/**
 * @brief    attach shm segment(by id) to the address space of the calling process(shmat())
 * @shmflag: SHM_RDONLY -- Attach the segment for read-only access. If this flag is not specified,
 *                         the segment is attached for read and write access, and the process must 
 *                         have read and write permission for the segment.
 *           for consumer(modbus_op): app_shmem_shmat(address, SHM_RDONLY);
 *           for producer(sql_op):    app_shmem_shmat(address, NULL);
 * @return : on success, return the address of attached shm segment
 *           on failure (void *)-1 is returned with <errno> set  
 */
void
*app_shmem_shmat(void *addr, int shmflag)
{
  if(-1 == g_shm_id)
  {
    LOG_WARN(OUTPOINT, "wrong shm id, create it first!\n");
    return (void *)(-1);
  }
  return shmat(g_shm_id, addr, shmflag);
}

/* 
 * @brief    attach shm segment(by id) to the address apace of the calling process(shmat())
 * @param @p cmd: IPC_RMID -- The caller must ensure that a segment is eventually destroyed;  other‐
 *                            wise its pages that were faulted in will remain in memory or swap.
 *                            refer to /proc/sys/kernel/shm_rmid_forced in proc(5) later if necessary
 *           for caller: app_shmem_shmctl(shmid, IPC_RMID, NULL);
 * @return : on success, return 0 for shm use case
 *           on failure -1 is returned with <errno> set  
 */
int
app_shmem_shmctl(int cmd, struct shmid_ds *buf)
{
  if(-1 == g_shm_id)
  {
    LOG_WARN(OUTPOINT, "wrong shm id, create it first!%d\n", g_shm_id);
    return (-1);
  }
  return shmctl(g_shm_id, cmd, buf);
}

/* 
 * @brief    detach shm segment from the address apace of the calling process(shmat())
 * @param @p addr : The to-be-detached segment must be currently attached with shmaddr equal to 
 *                  the value returned by the attaching shmat() call
 * @return : on success, return 0 for shm use case
 *           on failure -1 is returned with <errno> set  
 */
int
app_shmem_shmdt(void *addr)
{
  return shmdt(addr);
}
