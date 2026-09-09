/**
 * @file    mobile_server_interface.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Implementation of service interfaces for mobile phone.
 * @details
 */
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <fcntl.h>
#include "ConfigurationAndCommand.pb.h"
#include "Common.pb.h"
#include "util_dbg.h"
#include "app_pro_general_new.h"
#include "app_general_def.h"
#include "serviceInterface/service_interface_mobile_phone.h"

DBG_LOCAL_LOG_DEBUG

#define MAX_CONCURRENT_FILE_HANDLE_NUM (10)

bool isBLEMobileServerRunning;

// Summary of the group variables:
// 1. @p ble_mobile_server_ctr_mtx is initialized if
// @p ble_mobile_server_ctr_mtx_initialized set to true
// 2. @p ble_mobile_server_ctr_mtx is the locker of
// @p cond_val_ble_mobile_server_ctr_initialized,
// @p cond_val_ble_mobile_server_ctr,
// @p cond_val_ble_mobile_server_ctr_attr,
// @p cond_val_ble_mobile_server_ctr_tv, and @p ble_mobile_server_ctr_queue
// 3. @p cond_val_ble_mobile_server_ctr,
// @p cond_val_ble_mobile_server_ctr_attr,
// and @p cond_val_ble_mobile_server_ctr_tv are available only if
// @p cond_val_ble_mobile_server_ctr_initialized is set to true
// 4. @p ble_mobile_server_ctr_mtx_initialized would be set to true when
// the service thread started and would never be set to false
// 5. @p cond_val_ble_mobile_server_ctr_initialized would be set to true
// when the service thread started and would be set to false when the
// service thread is going to be finished.
static bool ble_mobile_server_ctr_mtx_initialized = false;
static pthread_mutex_t ble_mobile_server_ctr_mtx;
static bool cond_val_ble_mobile_server_ctr_initialized = false;
static pthread_cond_t cond_val_ble_mobile_server_ctr;
static pthread_condattr_t cond_val_ble_mobile_server_ctr_attr;
static struct timespec cond_val_ble_mobile_server_ctr_tv;
static struct l_queue *ble_mobile_server_ctr_queue = NULL;

// Data structure used by @p ble_mobile_server_ctr_queue
enum mobile_server_event
{
  MS_EVENT_UNDEFINED = 0,
  MS_EVENT_GWCONFIG_FILE_RECEIVE_NOTIFY = 1,
  MS_EVENT_FILE_RECEIVE = 2,
  MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY = 3,
  MS_EVENT_FILE_SEND = 4,
  MS_EVENT_TIMESETTING = 5,
};
struct mobile_server_queue_ele
{
  enum mobile_server_event event;
  void *content;
};

// Summary of the group variables:
// 1. @p file_id_type_mtx is initialized if @p file_id_type_mtx_initialized
// set to true
// 2. @p file_id_type_mtx is the locker of @p file_id_type_array and
// @p file_id_type_size
// 3. @p file_id_type_mtx_initialized would be set to true when the service
// thread started and would never be set to false
static bool file_id_type_mtx_initialized = false;
static pthread_mutex_t file_id_type_mtx;
struct file_id_type
{
  time_t create_time;
  uint32_t file_id;
  uint8_t file_type;
  int fd;
  bool isFUOTA;
  bool isReceive;
  // The followings are only valid when isReceive is set to false
  uint32_t longPktId;
  uint32_t totalBlk;
  uint32_t currentBlk;
  uint32_t offset;
  uint32_t offsetLast;
};
static struct file_id_type file_id_type_array[
  MAX_CONCURRENT_FILE_HANDLE_NUM];
static uint32_t file_id_type_size;

static struct service_interface intf;

static void mobileServiceRegister(struct service_interface *intf);
static void mobileServiceUnregister(struct service_interface *intf);
static void clean_file_id_array(void);
static bool mobileServerSend(
  void *ctx,
  void *data_to_be_sent,
  uint32_t dataLen,
  uint8_t *err_code);
// Callback
static void q_destroy_mobile_server_queue_ele(void *data);
static bool handleRequestConfigDissem(
  void *parsedData,
  uint8_t *err_code);
static bool handleRequestFileNotifyDissem(
  void *parsedData,
  uint8_t *err_code);
static bool handleRequestImageBlockDissem(
  void *parsedData,
  uint8_t *err_code);
static bool handleRequestConfigRetrieve(
  void *parsedData,
  uint8_t *err_code);
static bool handleRequestImageBlockRetrieve(
  void *parsedData,
  uint8_t *err_code);

/**
 * @brief Get the interface structure
 */
struct service_interface *
getMobileServiceIntf(void)
{
  return &intf;
}
/**
 * @brief Register the interface (called in the service thread when the
 * service launched)
 */
static void
mobileServiceRegister(struct service_interface *intf)
{
  intf->data_upload_processor = NULL;
  intf->config_hash_upload_processor = NULL;
  intf->specific_config_upload_processor = NULL;
  intf->current_version_upload_processor = NULL;
  intf->fuota_status_processor = NULL;
  intf->image_block_request_processor = NULL;
  intf->config_dissem_processor = handleRequestConfigDissem;
  intf->file_notify_dissem_processor = handleRequestFileNotifyDissem;
  intf->image_block_dissem_processor = handleRequestImageBlockDissem;
  intf->config_retrieve_processor = handleRequestConfigRetrieve;
  intf->image_block_retrieve_processor = handleRequestImageBlockRetrieve;
}
/**
 * @brief Unregister the interface (called in the service thread when the
 * service is about to be finished)
 */
static void
mobileServiceUnregister(struct service_interface *intf)
{
  intf->data_upload_processor = NULL;
  intf->config_hash_upload_processor = NULL;
  intf->specific_config_upload_processor = NULL;
  intf->current_version_upload_processor = NULL;
  intf->fuota_status_processor = NULL;
  intf->image_block_request_processor = NULL;
  intf->config_dissem_processor = NULL;
  intf->file_notify_dissem_processor = NULL;
  intf->image_block_dissem_processor = NULL;
  intf->config_retrieve_processor = NULL;
  intf->image_block_retrieve_processor = NULL;
}
/**
 * @brief Go through the array and clean the timeout elements (100 s
 * expired since created). Notice please lock @p file_id_type_mtx before
 * calling this function.
 */
static void
clean_file_id_array(void)
{
  for(uint32_t i = 0; i < file_id_type_size;)
  {
    // Clean the array
    DBG_LOG_DEBUG("file_id_type_array[%d].create_time = %llu", i,
                  file_id_type_array[i].create_time);
    if(((time(NULL) - file_id_type_array[i].create_time) < 0) ||
       ((time(NULL) - file_id_type_array[i].create_time) > 100))
    {
      DBG_LOG_DEBUG("Element #%d has been cleaned", i);
      // If the file task is created with an invalid time or the
      // file task is timeout, then remove element from the array
      // Close the file (if opened) and remove the element
      // Close the file (if opened)
      if(file_id_type_array[i].fd != -1)
      {
        close(file_id_type_array[i].fd);
        file_id_type_array[i].fd = -1;
      }
      // Remove the element
      for(uint32_t ii = i; ii < file_id_type_size - 1;
          ii++)
      {
        file_id_type_array[ii].create_time =
          file_id_type_array[ii + 1].create_time;
        file_id_type_array[ii].file_id =
          file_id_type_array[ii + 1].file_id;
        file_id_type_array[ii].file_type =
          file_id_type_array[ii + 1].file_type;
        file_id_type_array[ii].isFUOTA =
          file_id_type_array[ii + 1].isFUOTA;
        file_id_type_array[ii].isReceive =
          file_id_type_array[ii + 1].isReceive;
        file_id_type_array[ii].fd =
          file_id_type_array[ii + 1].fd;
        file_id_type_array[ii].longPktId =
          file_id_type_array[ii + 1].longPktId;
        file_id_type_array[ii].totalBlk =
          file_id_type_array[ii + 1].totalBlk;
        file_id_type_array[ii].currentBlk =
          file_id_type_array[ii + 1].currentBlk;
        file_id_type_array[ii].offset =
          file_id_type_array[ii + 1].offset;
        file_id_type_array[ii].offsetLast =
          file_id_type_array[ii + 1].offsetLast;
      }
      file_id_type_size--;
    }
    else
    {
      i++;
    }
  }
}
/**
 * @brief Message sending implementation
 */
static bool
mobileServerSend(
  void *ctx,
  void *data_to_be_sent,
  uint32_t dataLen,
  uint8_t *err_code)
{
#if (SKF_GW_NEW == 1)
  struct ipc_msg_v3 _ipc_msg = { 0 };

  _ipc_msg.type = M_TYPE_PROTO_DATA;
  _ipc_msg.payload.proto_pkt.len = dataLen;
  memcpy(_ipc_msg.payload.proto_pkt.dtbuf, data_to_be_sent, dataLen);

  // To send the msg
  if(0 != msgsnd(app_pro_gen_get_ctr()->hld_msg_to_pro_ble,
                 (const void *)&_ipc_msg,
                 sizeof(_ipc_msg.payload), 0))
  {
    DBG_LOG_ERR("msgsnd fail, for %s", strerror(errno));
    return false;
  }
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  if(app_pro_gen_get_ctr()->bt_connection_server_flg == false)
#else
  if(app_pro_gen_get_ctr()->bt_connection_flg == false)
#endif
  {
    DBG_LOG_ERR("The connection is broken\n");
    return false;
  }
  DBG_LOG_INFO("SEND MSG [FOR DEBUG]");
  util_dbg_buf_dump(
    (uint8_t const *)data_to_be_sent,
    dataLen);
#endif /* SKF_GW_NEW == 1 */
  return true;
}
// typedef
// void (*l_queue_destroy_func_t) (void *data);
/**
 * @brief: Free the allocated memory according to different event. The
 * callback would be called by l_queue_destroy.
 */
static void
q_destroy_mobile_server_queue_ele(void *data)
{
  struct mobile_server_queue_ele *pt =
    (struct mobile_server_queue_ele *)data;

  if(pt != NULL)
  {
    switch(pt->event)
    {
      case MS_EVENT_TIMESETTING:
        break;
      case MS_EVENT_GWCONFIG_FILE_RECEIVE_NOTIFY:
      {
        protobuf_codec_file_notify_dissem_t *_pt =
          (protobuf_codec_file_notify_dissem_t *)pt->content;

        l_free(_pt);
        break;
      }
      case MS_EVENT_FILE_RECEIVE:
      {
        protobuf_codec_image_block_dissem_t *_pt =
          (protobuf_codec_image_block_dissem_t *)pt->content;

        if(_pt->sensorIDLen != 0)
        {
          l_free(_pt->sensorID);
        }
        if(_pt->contentLen != 0)
        {
          l_free(_pt->content);
        }
        l_free(_pt);
      }
      case MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY:
      {
        protobuf_codec_config_retrieve_t *_pt =
          (protobuf_codec_config_retrieve_t *)pt->content;

        if(_pt->sensorIDLen != 0)
        {
          l_free(_pt->sensorID);
        }
        l_free(_pt);
      }
      case MS_EVENT_FILE_SEND:
      {
        protobuf_codec_image_block_retrieve_status_t *_pt =
          (protobuf_codec_image_block_retrieve_status_t *)pt->content;

        if(_pt->sensorIDLen != 0)
        {
          l_free(_pt->sensorID);
        }
        l_free(_pt);
      }
      default:
        break;
    }
    l_free(pt);
    pt = NULL;
  }
}
/**
 * @brief Thread of BLE Mobile server (the thread is not allowed to be
 * created for multiple times, i.e., only one thread is allowed
 * simultaneously).
 */
void *
thandler_BLEMobileServer(void *pt_para)
{
  struct mobile_server_queue_ele *_ptr_ele = NULL;
  isBLEMobileServerRunning = true;

  DBG_LOG_INFO("Enter the BLE mobile server\n");

  // Initialize something ...
  if(ble_mobile_server_ctr_mtx_initialized == false)
  {
    if(pthread_mutex_init(&ble_mobile_server_ctr_mtx, NULL))
    {
      DBG_LOG_ERR("Failed to initialize mutex");
      isBLEMobileServerRunning = false;
      pthread_exit(NULL);
    }
    ble_mobile_server_ctr_mtx_initialized = true;
  }

  pthread_mutex_lock(&ble_mobile_server_ctr_mtx);
  if(pthread_condattr_init(&cond_val_ble_mobile_server_ctr_attr))
  {
    DBG_LOG_ERR(
      "Failed to initialize condition attribute for mobile server controller");
    isBLEMobileServerRunning = false;
    pthread_exit(NULL);
  }
  if(pthread_condattr_setclock(&cond_val_ble_mobile_server_ctr_attr,
                               CLOCK_MONOTONIC))
  {
    DBG_LOG_ERR(
      "Failed to initialize condition attribute for mobile server controller");
    isBLEMobileServerRunning = false;
    pthread_exit(NULL);
  }
  if(pthread_cond_init(&cond_val_ble_mobile_server_ctr,
                       &cond_val_ble_mobile_server_ctr_attr))
  {
    DBG_LOG_ERR(
      "Failed to init condition variable for mobile server controller");
    isBLEMobileServerRunning = false;
    pthread_exit(NULL);
  }
  cond_val_ble_mobile_server_ctr_initialized = true;
  ble_mobile_server_ctr_queue = l_queue_new();
  pthread_mutex_unlock(&ble_mobile_server_ctr_mtx);

  if(file_id_type_mtx_initialized == false)
  {
    if(pthread_mutex_init(&file_id_type_mtx, NULL))
    {
      DBG_LOG_ERR("Failed to initialize mutex");
      isBLEMobileServerRunning = false;
      pthread_exit(NULL);
    }
    file_id_type_size = 0;
    file_id_type_mtx_initialized = true;
  }

  // Register the interface ...
  mobileServiceRegister(&intf);

  while(1)
  {
    // Enter the loop ...
    do{
      pthread_mutex_lock(&ble_mobile_server_ctr_mtx);
      clock_gettime(CLOCK_MONOTONIC,
                    &cond_val_ble_mobile_server_ctr_tv);
      cond_val_ble_mobile_server_ctr_tv.tv_sec += 120;
      if(0 == l_queue_length(ble_mobile_server_ctr_queue))
      {
        // If there is nothing in the server event queue, then wait ...
        if(pthread_cond_timedwait(&cond_val_ble_mobile_server_ctr,
                                  &ble_mobile_server_ctr_mtx,
                                  &cond_val_ble_mobile_server_ctr_tv)
           == ETIMEDOUT)
        {
          // If no operation timeout expired, queue the service thread
          // would be killed
          l_queue_destroy(ble_mobile_server_ctr_queue,
                          q_destroy_mobile_server_queue_ele);
          ble_mobile_server_ctr_queue = NULL;
          pthread_cond_destroy(&cond_val_ble_mobile_server_ctr);
          cond_val_ble_mobile_server_ctr_initialized = false;
          pthread_mutex_unlock(&ble_mobile_server_ctr_mtx);
          pthread_mutex_lock(&file_id_type_mtx);
          for(uint32_t i = 0; i < file_id_type_size; i++)
          {
            if(file_id_type_array[i].fd != -1)
            {
              close(file_id_type_array[i].fd);
              file_id_type_array[i].fd = -1;
            }
          }
          file_id_type_size = 0;
          pthread_mutex_unlock(&file_id_type_mtx);

          // Disconnect ...
#if (SKF_GW_NEW == 1)
          struct ipc_msg_v3 _ipc_msg = { 0 };

          _ipc_msg.type = M_TYPE_COMMAND;
          _ipc_msg.payload.cmd_info.cmd = CMD_DISCONNECTION;
          _ipc_msg.payload.cmd_info.msgid =
            app_pro_gen_allocate_msg_seq_no();
          if(0 != msgsnd(app_pro_gen_get_ctr()->hld_msg_to_pro_ble,
                         (const void *)&_ipc_msg,
                         sizeof(_ipc_msg.payload), 0))
          {
            DBG_LOG_ERR("Failed to send msg to the process BLE");
          }
#endif
          // wait for some times to make sure disconnect GW with the APP, to be confirmed
          sleep(2);
          // TODO: Before unregister, disconnect from the mobile phone is a
          // MUST. Otherwise, a function pointer to NULL would be called.
          mobileServiceUnregister(&intf);
          DBG_LOG_WARN("thandler_BLEMobileServer timeout!\n");
          DBG_LOG_WARN("exit ...\n");
          isBLEMobileServerRunning = false;
          pthread_exit(NULL);
        }
        pthread_mutex_unlock(&ble_mobile_server_ctr_mtx);
      }
      else
      {
        // If there is something in the server event queue, pop it up to @p
        // _ptr_ele
        _ptr_ele = l_queue_pop_head(ble_mobile_server_ctr_queue);
        DBG_LOG_INFO("Triggered by event %d\n", _ptr_ele->event);
        DBG_LOG_DEBUG("BLE mobile server length %d\n",
                      l_queue_length(ble_mobile_server_ctr_queue));
        pthread_mutex_unlock(&ble_mobile_server_ctr_mtx);
        break;
      }
    }while(1);
    switch(_ptr_ele->event)
    {
      case MS_EVENT_TIMESETTING:
      {
        DBG_LOG_INFO("Do nothing with time setting\n");
        l_free(_ptr_ele);
        break;
      }
      case MS_EVENT_GWCONFIG_FILE_RECEIVE_NOTIFY:
      {
        DBG_LOG_INFO("evt GWCONFIG_FILE_RECEIVE_NOTIFY\n");
        // There would be a file to receive ...
        // In this case, just put the task to @p file_id_type_array
        protobuf_codec_file_notify_dissem_t *pt =
          (protobuf_codec_file_notify_dissem_t *)_ptr_ele->content;

        pthread_mutex_lock(&file_id_type_mtx);
        clean_file_id_array();
        if(file_id_type_size >= MAX_CONCURRENT_FILE_HANDLE_NUM)
        {
          pthread_mutex_unlock(&file_id_type_mtx);
          DBG_LOG_WARN(
            "The concurrent handle file reaches the max. level %d",
            MAX_CONCURRENT_FILE_HANDLE_NUM);
        }
        else
        {
          file_id_type_array[file_id_type_size].create_time = time(NULL);
          file_id_type_array[file_id_type_size].file_id = pt->taskId;
          file_id_type_array[file_id_type_size].file_type = pt->fileType;
          file_id_type_array[file_id_type_size].isFUOTA = false;
          file_id_type_array[file_id_type_size].isReceive = true;
          file_id_type_array[file_id_type_size].fd = -1;
          file_id_type_array[file_id_type_size].longPktId = 0;
          file_id_type_array[file_id_type_size].totalBlk = 0;
          file_id_type_array[file_id_type_size].currentBlk = 0;
          file_id_type_array[file_id_type_size].offset = 0;
          file_id_type_array[file_id_type_size].offsetLast = 0;
          file_id_type_size++;
        }
        pthread_mutex_unlock(&file_id_type_mtx);
        l_free(_ptr_ele->content);
        l_free(_ptr_ele);
        break;
      }
      case MS_EVENT_FILE_RECEIVE:
      {
        DBG_LOG_INFO("evt FILE_RECEIVE\n");
        // To receive a block of a file ...
        protobuf_codec_image_block_dissem_t *pt =
          (protobuf_codec_image_block_dissem_t *)_ptr_ele->content;

        pthread_mutex_lock(&file_id_type_mtx);
        // Clean the array
        clean_file_id_array();
        // Find task ID in the array
        for(uint32_t i = 0; i < file_id_type_size; i++)
        {
          if((file_id_type_array[i].file_id == pt->taskId) &&
             (file_id_type_array[i].isFUOTA == pt->isFuota) &&
             (file_id_type_array[i].isReceive == true))
          {
            switch(file_id_type_array[i].file_type)
            {
              case
                SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG
                : // Just for keeping compatible with existed mobile phone
              // implementation
              case
                SKFChina_ConfigurationAndCommand_RetrievePayload_GATEWAY_CONFIG_FILE_CONFIG
                :
              {
                // Send back image block request message first
                uint8_t encodedBuf[256];
                uint32_t encodedPacketLen;
                uint32_t usedSeqNo;
                uint32_t seqNo = pt->seqNo + 1;
                SKFChina_Froto_FrotoMsgType msgType;

                app_froto_n_encode_msg_image_block_request(
                  SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
                  pt->isFuota,
                  pt->taskId,
                  pt->offset,
                  0, // 0 indicates that acknowledge the current image
                     // block has been received
                  pt->sensorID,
                  pt->sensorIDLen,
                  pt->sensorID, // Since the peer is mobile phone, so gw id
                                // and sensor id are identical
                  pt->sensorIDLen,
                  &seqNo,
                  encodedBuf,
                  sizeof(encodedBuf),
                  &encodedPacketLen,
                  &usedSeqNo,
                  &msgType,
                  NULL);

                if(false == app_froto_n_simple_send(encodedBuf,
                                                    encodedPacketLen,
                                                    NULL,
                                                    mobileServerSend,
                                                    NULL))
                {
                  // If failed to send message, then jump out the
                  // "switch-case"
                  DBG_LOG_ERR("Failed to send message");
                  break;
                }

                // Save the message payload to a file
                if(file_id_type_array[i].fd == -1)
                {
                  char fileName[40];

                  snprintf(fileName, sizeof(fileName), "%sgwCfg_%lu.json",
                           GW_CONF_FILE_PATH, pt->taskId);
                  file_id_type_array[i].fd = open(fileName,
                                                  O_CREAT | O_RDWR,
                                                  S_IRUSR | S_IWUSR);
                  if(file_id_type_array[i].fd < 0)
                  {
                    // If failed to open a file to save the received block,
                    // then jump out the "switch-case"
                    DBG_LOG_ERR("Failed to open file %s, for %s",
                                fileName,
                                strerror(errno));
                    break;
                  }
                }
                if(file_id_type_array[i].fd >= 0)
                {
                  off_t offset = lseek(file_id_type_array[i].fd,
                                       pt->offset,
                                       SEEK_SET);

                  if(offset < 0)
                  {
                    // If failed to seek the offset, then close the file
                    // and jump out the "switch-case"
                    close(file_id_type_array[i].fd);
                    file_id_type_array[i].fd = -1;
                    DBG_LOG_ERR("Failed to lseek, for %s",
                                strerror(errno));
                    break;
                  }
                  write(file_id_type_array[i].fd, pt->content,
                        pt->contentLen);
                  if((pt->totalBlock <= 1) ||
                     ((pt->totalBlock > 1) &&
                      ((pt->currentBlock + 1) >= pt->totalBlock)))
                  {
                    char buf[1024] = { '\0' };
                    char file_path[1024] = { '\0' };

                    // Fetch the file name
                    snprintf(buf, sizeof(buf), "/proc/self/fd/%d",
                             file_id_type_array[i].fd);
                    readlink(buf, file_path, sizeof(file_path) - 1);

                    close(file_id_type_array[i].fd);
                    file_id_type_array[i].fd = -1;

                    // Write to the database from json
                    app_db_deliver_gwconf_and_dev_to_db_from_json(
                      app_db_get_dbop_controller(), file_path);

                    // Then clean the array
                    file_id_type_array[i].create_time = 0;
                    clean_file_id_array();
                  }
                }
                break;
              }
              default:
                DBG_LOG_ERR("The file type is not supported so far");
                break;
            }
            break;
          }
        }
        if(pt->sensorIDLen != 0)
        {
          l_free(pt->sensorID);
        }
        if(pt->contentLen != 0)
        {
          l_free(pt->content);
        }
        l_free(_ptr_ele->content);
        l_free(_ptr_ele);
        pthread_mutex_unlock(&file_id_type_mtx);
        break;
      }
      case MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY:
      {
        DBG_LOG_INFO("evt GWCONFIG_FILE_SEND_NOTIFY\n");
        // There would be a file to send ...
        // In this case, send the notify message and the first block
        protobuf_codec_config_retrieve_t *pt =
          (protobuf_codec_config_retrieve_t *)_ptr_ele->content;
        char fileName[40];
        uint32_t task_id = 0;
        uint32_t idx = 0;
        time_t create_time = 0;
        bool fileIsReady = false;

        pthread_mutex_lock(&file_id_type_mtx);
        clean_file_id_array();
        if(file_id_type_size >= MAX_CONCURRENT_FILE_HANDLE_NUM)
        {
          pthread_mutex_unlock(&file_id_type_mtx);
          DBG_LOG_WARN(
            "The concurrent handle file reaches the max. level %d",
            MAX_CONCURRENT_FILE_HANDLE_NUM);
        }
        else
        {
          file_id_type_array[file_id_type_size].create_time = time(NULL);
          create_time =
            file_id_type_array[file_id_type_size].create_time;
          file_id_type_array[file_id_type_size].file_id =
            (uint32_t)file_id_type_array[file_id_type_size].create_time;
          task_id = file_id_type_array[file_id_type_size].file_id;
          idx = file_id_type_size;
          file_id_type_array[file_id_type_size].file_type =
            SKFChina_Common_FileType_GATEWAY_CONFIG_FILE;
          file_id_type_array[file_id_type_size].isFUOTA = false;
          file_id_type_array[file_id_type_size].isReceive = false;
          file_id_type_array[file_id_type_size].fd = -1;
          file_id_type_array[file_id_type_size].longPktId = 0;
          file_id_type_array[file_id_type_size].totalBlk = 0;
          file_id_type_array[file_id_type_size].currentBlk = 0;
          file_id_type_array[file_id_type_size].offset = 0;
          file_id_type_array[file_id_type_size].offsetLast = 0;
          snprintf(fileName, sizeof(fileName), "%sgwCfgSend_%lu.json",
                   GW_CONF_FILE_PATH,
                   file_id_type_array[file_id_type_size].file_id);

          DBG_LOG_INFO("file_id_type_size %d, fileName %s",
                        file_id_type_size, fileName);

          if(ST_OK !=
             app_db_fetch_gwconf_and_dev_from_db_to_file(
               app_db_get_dbop_controller(), fileName))
          {
            DBG_LOG_ERR("Failed to generate gwCfgSend file");
            file_id_type_array[file_id_type_size].create_time = 0;
            clean_file_id_array();
          }
          else
          {
            fileIsReady = true;
          }
          file_id_type_size++;
        }
        pthread_mutex_unlock(&file_id_type_mtx);
        DBG_LOG_DEBUG("task_id %d, create_time %d, fileIsReady %d",
                      task_id, create_time, fileIsReady);

        if((task_id == 0) || (create_time == 0) || (fileIsReady == false))
        {
          if(pt->sensorIDLen != 0)
          {
            l_free(pt->sensorID);
          }
          l_free(_ptr_ele->content);
          l_free(_ptr_ele);
          // Failed to put the task to @p file_id_type_array or failed to
          // read gwconf to a file
          // Jump out the "switch-case"
          break;
        }

        pthread_mutex_lock(&file_id_type_mtx);
        // task_id != 0: there is still room in file_id_type_array
        // fileIsReady == true: the config has been fetched from database
        // successfully
        if((task_id != 0) && (fileIsReady == true))
        {
          // Send file notify upload message first
          uint8_t encodedBuf[412];
          uint32_t encodedPacketLen;
          uint32_t usedSeqNo;
          SKFChina_Froto_FrotoMsgType msgType;
          uint32_t hash = 0;
          uint8_t blockContent[FILE_PKT_SIZE];

          app_froto_n_encode_msg_file_notify_upload(
            SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
            task_id,
            SKFChina_Common_CommunicationType_BLECONNECTION_COMM,
            SKFChina_Common_FileType_GATEWAY_CONFIG_FILE,
            SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_fuota_with_froto_over_ble_ots_tag,
            NULL,
            0,
            NULL,
            0,
            true,
            SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_elf_hash_value_tag,
            &hash,
            sizeof(hash),
            SKFChina_Common_Encryption_NO_ENCRYPTION,
            pt->sensorID,
            pt->sensorIDLen,
            pt->sensorID,
            pt->sensorIDLen,
            NULL,
            encodedBuf,
            sizeof(encodedBuf),
            &encodedPacketLen,
            &usedSeqNo,
            &msgType,
            NULL);

#if 0 // Just for sanity check
          const uint8_t _buffer1[] = {
            0x08, 0x01, 0x9a, 0x01, 0x40, 0x0a, 0x1f, 0x08, 0x01, 0x12,
            0x05, 0x47, 0x57, 0x30, 0x30, 0x37, 0x22, 0x05, 0x47, 0x57,
            0x30, 0x30, 0x37, 0x30, 0x01, 0x40, 0x01, 0x48, 0x01, 0x50,
            0x01, 0x58, 0x0b, 0x70, 0x01, 0x80, 0x01, 0x00, 0x10, 0x01,
            0x1a, 0x05, 0x47, 0x57, 0x30, 0x30, 0x37, 0x20, 0x02, 0x28,
            0x01, 0x30, 0x01, 0x4a, 0x09, 0x0a, 0x05, 0x47, 0x57, 0x30,
            0x30, 0x37, 0x10, 0x01, 0x68, 0xb0, 0x82, 0xad, 0x05
          };

          memcpy(encodedBuf, _buffer1, sizeof(_buffer1));
          encodedPacketLen = sizeof(_buffer1);
          if(false == app_froto_n_simple_send(encodedBuf,
                                              encodedPacketLen,
                                              NULL,
                                              mobileServerSend,
                                              NULL))
          {
            DBG_LOG_ERR("Failed to send message");
            goto MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY_EXIT;
          }
#else
          if(false == app_froto_n_simple_send(encodedBuf,
                                              encodedPacketLen,
                                              NULL,
                                              mobileServerSend,
                                              NULL))
          {
            DBG_LOG_ERR("Failed to send message");
            goto MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY_EXIT;
          }
#endif
          // Open the image
          file_id_type_array[idx].fd = open(fileName, O_RDONLY);

          if(file_id_type_array[idx].fd < 0)
          {
            DBG_LOG_ERR("Failed to open file %s, for %s",
                        fileName,
                        strerror(errno));
            goto MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY_EXIT;
          }

          off_t fsize = lseek(file_id_type_array[idx].fd, 0, SEEK_END);

          if(fsize < 0)
          {
            DBG_LOG_ERR("Failed to lseek, for %s", strerror(errno));
            goto MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY_EXIT;
          }
          lseek(file_id_type_array[idx].fd, 0, SEEK_SET);  // Reset the

          uint32_t totalBlock = fsize / FILE_PKT_SIZE;

          if(fsize % FILE_PKT_SIZE)
          {
            totalBlock++;
          }

          uint32_t blockContentLen = read(file_id_type_array[idx].fd,
                                          blockContent, FILE_PKT_SIZE);

          hash = util_froto_bullet_elfhash(blockContent, blockContentLen);

          // Send the image
          app_froto_n_encode_msg_image_block_upload(
            SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
            0,
            false,
            task_id,
            blockContent,
            blockContentLen,
            hash,
            totalBlock,
            0,
            file_id_type_array[idx].longPktId,
            pt->sensorID,
            pt->sensorIDLen,
            pt->sensorID,
            pt->sensorIDLen,
            NULL,
            encodedBuf,
            sizeof(encodedBuf),
            &encodedPacketLen,
            &usedSeqNo,
            &msgType,
            NULL);

          file_id_type_array[idx].longPktId = usedSeqNo;
          file_id_type_array[idx].currentBlk = 0;
          file_id_type_array[idx].totalBlk = totalBlock;
          file_id_type_array[idx].offset = blockContentLen;
          file_id_type_array[idx].offsetLast = 0;

#if 0 // Just for sanity check
          const uint8_t _buffer2[] = {
            0x08, 0x01, 0xa2, 0x01, 0x92, 0x02, 0x0a, 0x21, 0x08, 0x01,
            0x12, 0x05, 0x47, 0x57, 0x30, 0x30, 0x37, 0x22, 0x05, 0x47,
            0x57, 0x30, 0x30, 0x37, 0x30, 0x01, 0x38, 0x42, 0x40, 0x01,
            0x48, 0x01, 0x50, 0x01, 0x58, 0x0b, 0x70, 0x01, 0x80, 0x01,
            0x00, 0x10, 0x01, 0x1a, 0x05, 0x47, 0x57, 0x30, 0x30, 0x37,
            0x32, 0xdc, 0x01, 0x7b, 0x22, 0x76, 0x65, 0x72, 0x73, 0x69,
            0x6f, 0x6e, 0x22, 0x3a, 0x31, 0x2c, 0x22, 0x6c, 0x61, 0x73,
            0x74, 0x45, 0x64, 0x69, 0x74, 0x54, 0x69, 0x6d, 0x65, 0x22,
            0x3a, 0x31, 0x37, 0x34, 0x34, 0x33, 0x33, 0x36, 0x37, 0x36,
            0x39, 0x2c, 0x22, 0x67, 0x77, 0x43, 0x6f, 0x6e, 0x66, 0x69,
            0x67, 0x22, 0x3a, 0x7b, 0x22, 0x74, 0x79, 0x70, 0x65, 0x22,
            0x3a, 0x22, 0x47, 0x41, 0x54, 0x45, 0x57, 0x41, 0x59, 0x22,
            0x2c, 0x22, 0x6d, 0x61, 0x6e, 0x75, 0x66, 0x61, 0x63, 0x74,
            0x75, 0x72, 0x65, 0x72, 0x22, 0x3a, 0x22, 0x53, 0x4b, 0x46,
            0x22, 0x2c, 0x22, 0x6e, 0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22,
            0x42, 0x75, 0x6c, 0x6c, 0x65, 0x74, 0x47, 0x57, 0x31, 0x32,
            0x30, 0x30, 0x33, 0x34, 0x22, 0x2c, 0x22, 0x6e, 0x61, 0x6d,
            0x65, 0x49, 0x64, 0x22, 0x3a, 0x31, 0x37, 0x37, 0x39, 0x35,
            0x36, 0x34, 0x35, 0x39, 0x36, 0x2c, 0x22, 0x6d, 0x61, 0x63,
            0x41, 0x64, 0x64, 0x72, 0x22, 0x3a, 0x22, 0x63, 0x34, 0x2d,
            0x62, 0x64, 0x2d, 0x36, 0x61, 0x2d, 0x31, 0x32, 0x2d, 0x30,
            0x30, 0x2d, 0x33, 0x34, 0x22, 0x2c, 0x22, 0x67, 0x61, 0x74,
            0x65, 0x77, 0x61, 0x79, 0x4d, 0x6f, 0x64, 0x65, 0x22, 0x3a,
            0x31, 0x2c, 0x22, 0x42, 0x4c, 0x45, 0x43, 0x6f, 0x6e, 0x66,
            0x69, 0x67, 0x22, 0x3a, 0x7b, 0x22, 0x42, 0x4c, 0x45, 0x31,
            0x54, 0x78, 0x50, 0x6f, 0x77, 0x65, 0x72, 0x22, 0x3a, 0x30,
            0x2c, 0x22, 0x42, 0x4c, 0x45, 0x32, 0x54, 0x78, 0x50, 0x6f,
            0x77, 0x65, 0x72, 0x38, 0xd2, 0xa0, 0xc7, 0x0b, 0x48, 0x01
          };

          memcpy(encodedBuf, _buffer2, sizeof(_buffer2));
          encodedPacketLen = sizeof(_buffer2);
          if(false == app_froto_n_simple_send(encodedBuf,
                                              encodedPacketLen,
                                              NULL,
                                              mobileServerSend,
                                              NULL))
          {
            DBG_LOG_ERR("Failed to send message");
            goto MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY_EXIT;
          }
#else
          if(false == app_froto_n_simple_send(encodedBuf,
                                              encodedPacketLen,
                                              NULL,
                                              mobileServerSend,
                                              NULL))
          {
            DBG_LOG_ERR("Failed to send message");
            goto MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY_EXIT;
          }
#endif
        }
MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY_EXIT:
        pthread_mutex_unlock(&file_id_type_mtx);
        if(pt->sensorIDLen != 0)
        {
          l_free(pt->sensorID);
        }
        l_free(_ptr_ele->content);
        l_free(_ptr_ele);
        break;
      }
      case MS_EVENT_FILE_SEND:
      {
        DBG_LOG_INFO("evt FILE_SEND\n");
        uint8_t encodedBuf[412];
        uint32_t encodedPacketLen;
        uint32_t usedSeqNo;
        SKFChina_Froto_FrotoMsgType msgType;
        uint32_t hash = 0;
        uint8_t blockContent[FILE_PKT_SIZE];
        bool foundFg = false;

        protobuf_codec_image_block_retrieve_status_t *pt =
          (protobuf_codec_image_block_retrieve_status_t *)_ptr_ele->content;

        pthread_mutex_lock(&file_id_type_mtx);
        // Clean the array
        clean_file_id_array();
        // Find ID in the array
        for(uint32_t i = 0; i < file_id_type_size; i++)
        {
          if((file_id_type_array[i].file_id == pt->task_id) &&
             (file_id_type_array[i].isFUOTA == pt->is_fuota_task) &&
             (file_id_type_array[i].isReceive == false))
          {
            foundFg = true;
            if(pt->size == 0)
            {
              if(file_id_type_array[i].currentBlk + 1 ==
                 file_id_type_array[i].totalBlk)
              {
                file_id_type_array[i].create_time = 0;
                close(file_id_type_array[i].fd);
                file_id_type_array[i].fd = -1;
                break;
              }
              // Last packet has been received
              if(-1 == lseek(file_id_type_array[i].fd,
                             file_id_type_array[i].offset, SEEK_SET))
              {
                DBG_LOG_ERR("Failed to lseek, for %s", strerror(errno));
                goto MS_EVENT_FILE_SEND_EXIT;
              }
              file_id_type_array[i].offsetLast =
                file_id_type_array[i].offset;
            }
            else
            {
              // Last packet has not been received ...
              if(-1 == lseek(file_id_type_array[i].fd,
                             file_id_type_array[i].offsetLast, SEEK_SET))
              {
                DBG_LOG_ERR("Failed to lseek, for %s", strerror(errno));
                goto MS_EVENT_FILE_SEND_EXIT;
              }
            }

            uint32_t blockContentLen = read(file_id_type_array[i].fd,
                                            blockContent, FILE_PKT_SIZE);

            hash = util_froto_bullet_elfhash(blockContent,
                                             blockContentLen);

            app_froto_n_encode_msg_image_block_upload(
              SKFChina_Froto_FrotoPmtType_SIMPLE_UPLOAD,
              file_id_type_array[i].offset,
              false,
              file_id_type_array[i].file_id,
              blockContent,
              blockContentLen,
              hash,
              file_id_type_array[i].totalBlk,
              file_id_type_array[i].currentBlk + 1,
              file_id_type_array[i].longPktId,
              pt->sensorID,
              pt->sensorIDLen,
              pt->sensorID,
              pt->sensorIDLen,
              NULL,
              encodedBuf,
              sizeof(encodedBuf),
              &encodedPacketLen,
              &usedSeqNo,
              &msgType,
              NULL);

            file_id_type_array[i].currentBlk++;
            file_id_type_array[i].offset =
              file_id_type_array[i].offsetLast + blockContentLen;

            if(false == app_froto_n_simple_send(encodedBuf,
                                                encodedPacketLen,
                                                NULL,
                                                mobileServerSend,
                                                NULL))
            {
              DBG_LOG_ERR("Failed to send message");
              goto MS_EVENT_FILE_SEND_EXIT;
            }
          }
        }
MS_EVENT_FILE_SEND_EXIT:
        pthread_mutex_unlock(&file_id_type_mtx);
        if(foundFg == false)
        {
          if(pt->sensorIDLen != 0)
          {
            l_free(pt->sensorID);
          }
          l_free(_ptr_ele->content);
          l_free(_ptr_ele);
        }
        break;
      }
      default:
      {
        DBG_LOG_WARN("Unknown event in the BLE mobile server queue");
        l_free(_ptr_ele);
        break;
      }
    }
  }
}
/**
 * @brief Deal with request message via configuration dissemination
 * @param parsedData The parsed configuration dissemination data (i.e., the
 * configuration pair)
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
static bool
handleRequestConfigDissem(
  void *parsedData,
  uint8_t *err_code)
{
  protobuf_codec_specific_config_t *pt =
    (protobuf_codec_specific_config_t *)parsedData;

  // TODO: To check sensor ID

  // Deal with time set
  for(uint32_t cnt = 0; cnt < pt->config_pair.size; cnt++)
  {
    if(pt->config_pair.buffer[cnt].which_config_item ==
       SKFChina_ConfigurationAndCommand_ConfigPair_specific_config_item_tag)
    {
      if(pt->config_pair.buffer[cnt].config_item.specific_config_item ==
         SKFChina_Common_SpecificConfigItem_CURRENT_TIME)
      {
        if(pt->config_pair.buffer[cnt].which_config_content ==
           SKFChina_ConfigurationAndCommand_ConfigPair_time_config_content_tag)
        {
          if(pt->config_pair.buffer[cnt].content.time.size > 0)
          {
            struct timeval tv;

            tv.tv_sec = pt->config_pair.buffer[cnt].content.time.time[0];
            tv.tv_usec = 0;
            // Set the system time
            settimeofday(&tv, NULL);
            // And set the system to the hardware RTC
            system("hwclock -w");
            DBG_LOG_INFO("Current time is set to %lu",
                         pt->config_pair.buffer[cnt].content.time.time[0]);

            // Notify the server
            if(ble_mobile_server_ctr_mtx_initialized == true)
            {
              struct mobile_server_queue_ele *_ele =
                l_malloc(sizeof(struct mobile_server_queue_ele));

              _ele->event = MS_EVENT_TIMESETTING;
              pthread_mutex_lock(&ble_mobile_server_ctr_mtx);
              if(cond_val_ble_mobile_server_ctr_initialized)
              {
                pthread_cond_signal(&cond_val_ble_mobile_server_ctr);
              }
              else
              {
                l_free(_ele);
                _ele = NULL;
              }
              if((ble_mobile_server_ctr_queue != NULL) && (_ele != NULL))
              {
                l_queue_push_tail(ble_mobile_server_ctr_queue, _ele);
              }
              pthread_mutex_unlock(&ble_mobile_server_ctr_mtx);
            }
          }
        }
      }
    }
  }

  return true;
}
/**
 * @brief Deal with request message via file notify dissemination
 * @param parsedData The parsed file notify dissemination data
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
static bool
handleRequestFileNotifyDissem(
  void *parsedData,
  uint8_t *err_code)
{
  protobuf_codec_file_notify_dissem_t *pt =
    (protobuf_codec_file_notify_dissem_t *)parsedData;

  // TODO: To check sensor ID

  DBG_LOG_INFO("File notify dissem received\n");
  DBG_LOG_INFO("File type is %d", pt->fileType);
  DBG_LOG_INFO("task id is %d", pt->taskId);
  DBG_LOG_DEBUG("overBleUART is %d", pt->overBleUART);

  // Notify the server
  if(ble_mobile_server_ctr_mtx_initialized == true)
  {
    if((pt->fileType ==
        SKFChina_ConfigurationAndCommand_RetrievePayload_GATEWAY_CONFIG_FILE_CONFIG)
       ||
       (pt->fileType ==
        SKFChina_ConfigurationAndCommand_RetrievePayload_SPECIFIC_CURRENT_CONFIG))
    {
      struct mobile_server_queue_ele *_ele =
        l_malloc(sizeof(struct mobile_server_queue_ele));

      _ele->event = MS_EVENT_GWCONFIG_FILE_RECEIVE_NOTIFY;
      _ele->content =
        l_malloc(sizeof(protobuf_codec_file_notify_dissem_t));
      ((protobuf_codec_file_notify_dissem_t *)(_ele->content))->fileType =
        pt->fileType;
      ((protobuf_codec_file_notify_dissem_t *)(_ele->content))->taskId =
        pt->taskId;

      pthread_mutex_lock(&ble_mobile_server_ctr_mtx);
      if(cond_val_ble_mobile_server_ctr_initialized)
      {
        pthread_cond_signal(&cond_val_ble_mobile_server_ctr);
      }
      else
      {
        l_free(_ele->content);
        l_free(_ele);
        _ele = NULL;
      }
      if((ble_mobile_server_ctr_queue != NULL) && (_ele != NULL))
      {
        l_queue_push_tail(ble_mobile_server_ctr_queue, _ele);
      }
      pthread_mutex_unlock(&ble_mobile_server_ctr_mtx);
    }
  }

  return true;
}
/**
 * @brief Deal with request message via image block dissemination
 * @param parsedData The parsed file notify dissemination data
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
static bool
handleRequestImageBlockDissem(
  void *parsedData,
  uint8_t *err_code)
{
  protobuf_codec_image_block_dissem_t *pt =
    (protobuf_codec_image_block_dissem_t *)parsedData;

  // TODO: To check sensor ID

  DBG_LOG_INFO("Image block dissem received\n");

  // Notify the server
  if(ble_mobile_server_ctr_mtx_initialized == true)
  {
    struct mobile_server_queue_ele *_ele =
      l_malloc(sizeof(struct mobile_server_queue_ele));

    _ele->event = MS_EVENT_FILE_RECEIVE;
    _ele->content = l_malloc(sizeof(protobuf_codec_image_block_dissem_t));
    ((protobuf_codec_image_block_dissem_t *)(_ele->content))->taskId =
      pt->taskId;
    ((protobuf_codec_image_block_dissem_t *)(_ele->content))->isFuota =
      pt->isFuota;
    ((protobuf_codec_image_block_dissem_t *)(_ele->content))->offset =
      pt->offset;
    ((protobuf_codec_image_block_dissem_t *)(_ele->content))->totalBlock =
      pt->totalBlock;
    ((protobuf_codec_image_block_dissem_t *)(_ele->content))->currentBlock
      = pt->currentBlock;
    ((protobuf_codec_image_block_dissem_t *)(_ele->content))->longPktId
      = pt->longPktId;
    ((protobuf_codec_image_block_dissem_t *)(_ele->content))->seqNo
      = pt->seqNo;

    ((protobuf_codec_image_block_dissem_t *)(_ele->content))->contentLen =
      pt->contentLen;
    if(pt->contentLen != 0)
    {
      ((protobuf_codec_image_block_dissem_t *)(_ele->content))->content =
        l_malloc(pt->contentLen);
      memcpy(
        ((protobuf_codec_image_block_dissem_t *)(_ele->content))->content,
        pt->content, pt->contentLen);
    }
    else
    {
      ((protobuf_codec_image_block_dissem_t *)(_ele->content))->content =
        NULL;
    }

    ((protobuf_codec_image_block_dissem_t
      *)(_ele->content))->sensorIDLen =
      pt->sensorIDLen;
    if(pt->sensorIDLen != 0)
    {
      ((protobuf_codec_image_block_dissem_t *)(_ele->content))->sensorID =
        l_malloc(pt->sensorIDLen);
      memcpy(
        ((protobuf_codec_image_block_dissem_t *)(_ele->content))->sensorID,
        pt->sensorID, pt->sensorIDLen);
    }
    else
    {
      ((protobuf_codec_image_block_dissem_t *)(_ele->content))->sensorID =
        NULL;
    }

    pthread_mutex_lock(&ble_mobile_server_ctr_mtx);
    if(cond_val_ble_mobile_server_ctr_initialized)
    {
      pthread_cond_signal(&cond_val_ble_mobile_server_ctr);
    }
    else
    {
      if(((protobuf_codec_image_block_dissem_t *)(_ele->content))->content
         != NULL)
      {
        l_free(
          ((protobuf_codec_image_block_dissem_t *)(_ele->content))->content);
      }
      if(((protobuf_codec_image_block_dissem_t *)(_ele->content))->sensorID
         != NULL)
      {
        l_free(
          ((protobuf_codec_image_block_dissem_t *)(_ele->content))->
          sensorID);
      }
      l_free(_ele->content);
      l_free(_ele);
      _ele = NULL;
    }
    if((ble_mobile_server_ctr_queue != NULL) && (_ele != NULL))
    {
      l_queue_push_tail(ble_mobile_server_ctr_queue, _ele);
    }
    pthread_mutex_unlock(&ble_mobile_server_ctr_mtx);
  }

  return true;
}
/**
 * @brief Deal with request message via config retrieve
 * @param parsedData The parsed file notify dissemination data
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
static bool
handleRequestConfigRetrieve(
  void *parsedData,
  uint8_t *err_code)
{
  protobuf_codec_config_retrieve_t *pt =
    (protobuf_codec_config_retrieve_t *)parsedData;

  // TODO: To check sensor ID

  DBG_LOG_INFO("Config retrieve received\n");

  // Notify the server
  if(ble_mobile_server_ctr_mtx_initialized == true)
  {
    if(pt->isGWConfigFile == true)
    {
      struct mobile_server_queue_ele *_ele =
        l_malloc(sizeof(struct mobile_server_queue_ele));

      _ele->event = MS_EVENT_GWCONFIG_FILE_SEND_NOTIFY;
      _ele->content = l_malloc(sizeof(protobuf_codec_config_retrieve_t));
      ((protobuf_codec_config_retrieve_t *)(_ele->content))->isGWConfigFile
        = pt->isGWConfigFile;

      ((protobuf_codec_config_retrieve_t *)(_ele->content))->sensorIDLen =
        pt->sensorIDLen;
      if(pt->sensorIDLen != 0)
      {
        ((protobuf_codec_config_retrieve_t *)(_ele->content))->sensorID =
          l_malloc(pt->sensorIDLen);
        memcpy(
          ((protobuf_codec_config_retrieve_t *)(_ele->content))->sensorID,
          pt->sensorID, pt->sensorIDLen);
      }
      else
      {
        ((protobuf_codec_config_retrieve_t *)(_ele->content))->sensorID =
          NULL;
      }

      pthread_mutex_lock(&ble_mobile_server_ctr_mtx);
      if(cond_val_ble_mobile_server_ctr_initialized)
      {
        pthread_cond_signal(&cond_val_ble_mobile_server_ctr);
      }
      else
      {
        if(((protobuf_codec_config_retrieve_t *)(_ele->content))->sensorID
           != NULL)
        {
          l_free(
            ((protobuf_codec_config_retrieve_t *)(_ele->content))->sensorID);
        }
        l_free(_ele->content);
        l_free(_ele);
        _ele = NULL;
      }
      if((ble_mobile_server_ctr_queue != NULL) && (_ele != NULL))
      {
        l_queue_push_tail(ble_mobile_server_ctr_queue, _ele);
      }
      pthread_mutex_unlock(&ble_mobile_server_ctr_mtx);
    }
  }

  return true;
}
/**
 * @brief Deal with request message via image block retrieve
 * @param parsedData The parsed file notify dissemination data
 * @param errCode Returned error code (this pointer can be NULL if no
 * errCode is required).
 * @return True if everything is ok.
 */
static bool
handleRequestImageBlockRetrieve(
  void *parsedData,
  uint8_t *err_code)
{
  protobuf_codec_image_block_retrieve_status_t *pt =
    (protobuf_codec_image_block_retrieve_status_t *)parsedData;

  // TODO: To check sensor ID

  DBG_LOG_INFO("Image block received\n");

  // Notify the server
  if(ble_mobile_server_ctr_mtx_initialized == true)
  {
    if(pt->is_fuota_task == false)
    {
      struct mobile_server_queue_ele *_ele =
        l_malloc(sizeof(struct mobile_server_queue_ele));

      _ele->event = MS_EVENT_FILE_SEND;
      _ele->content =
        l_malloc(sizeof(protobuf_codec_image_block_retrieve_status_t));
      ((protobuf_codec_image_block_retrieve_status_t *)(_ele->content))->
      offset = pt->offset;
      ((protobuf_codec_image_block_retrieve_status_t *)(_ele->content))->
      size = pt->size;
      ((protobuf_codec_image_block_retrieve_status_t *)(_ele->content))->
      task_id = pt->task_id;
      ((protobuf_codec_image_block_retrieve_status_t *)(_ele->content))->
      is_fuota_task = pt->is_fuota_task;

      ((protobuf_codec_image_block_retrieve_status_t *)(_ele->content))->
      sensorIDLen = pt->sensorIDLen;
      if(pt->sensorIDLen != 0)
      {
        ((protobuf_codec_image_block_retrieve_status_t *)(_ele->content))->
        sensorID =
          l_malloc(pt->sensorIDLen);
        memcpy(
          ((protobuf_codec_image_block_retrieve_status_t *)(_ele->content))
          ->sensorID,
          pt->sensorID, pt->sensorIDLen);
      }
      else
      {
        ((protobuf_codec_image_block_retrieve_status_t *)(_ele->content))->
        sensorID = NULL;
      }

      pthread_mutex_lock(&ble_mobile_server_ctr_mtx);
      if(cond_val_ble_mobile_server_ctr_initialized)
      {
        pthread_cond_signal(&cond_val_ble_mobile_server_ctr);
      }
      else
      {
        if(((protobuf_codec_image_block_retrieve_status_t *)(_ele->content))
           ->sensorID
           != NULL)
        {
          l_free(
            ((protobuf_codec_image_block_retrieve_status_t *)(_ele->content))
            ->sensorID);
        }
        l_free(_ele->content);
        l_free(_ele);
        _ele = NULL;
      }
      if((ble_mobile_server_ctr_queue != NULL) && (_ele != NULL))
      {
        l_queue_push_tail(ble_mobile_server_ctr_queue, _ele);
      }
      pthread_mutex_unlock(&ble_mobile_server_ctr_mtx);
    }
  }

  return true;
}
