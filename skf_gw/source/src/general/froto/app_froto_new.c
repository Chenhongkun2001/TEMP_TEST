/**
 * @file    app_froto_new.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-03-24
 * @brief   Implementation of Froto protocol.
 * @details
 */
#include "sys_def.h"
#include "froto/app_froto_common.h"
#include "froto/app_froto_new.h"
#include "froto/decoder/app_froto_data_upload_decoder.h"
#include "froto/decoder/app_froto_config_decoder.h"
#include "froto/decoder/app_froto_fuota_decoder.h"
#include "util_dbg.h"
#include "app_common.h"

DBG_LOCAL_LOG_DEBUG

struct msg_buf_t
{
  uint8_t *replyMsgBuf;
  uint32_t replyMsgBufLen;
};
struct reply_ack_t
{
  struct pthread_cond_var *pt_cond_var;
  const uint8_t *pt_peer_str;
  uint8_t pt_peer_str_len;
  SKFChina_Froto_FrotoPmtType ExpPrimitive;
  SKFChina_Froto_FrotoMsgType ExpMsgType;
  uint32_t ExpLongPktId;
  uint32_t totalBlock;
  uint32_t currentBlock;
  bool checkSeqNo;
  uint32_t ExpSeqNo;
  uint32_t SeqNo;
  uint32_t *replyAck;
  uint32_t replyAckLen;
  pthread_mutex_t replyMsgQueueMtx;
  struct l_queue *replyMsgQueue;
};
static pthread_mutex_t reply_ack_queue_mtx;
static struct l_queue *reply_ack_queue;
static uint32_t singleChannelConcurrentCnt; // This variable is also

// controlled by mtx

/**
 * @brief Initialize Froto stack
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_n_init(uint8_t *errCode)
{
  if(pthread_mutex_init(&reply_ack_queue_mtx, NULL))
  {
    DBG_LOG_ERR("Failed to initialize mutex");
    return false;
  }

  reply_ack_queue = l_queue_new();
  return true;
}
// typedef
// void (*l_queue_foreach_func_t) (void *data, void *user_data);
/**
 * @brief: The callback called by l_queue_foreach (to count the matched
 * elements in the queue)
 */
static void
q_match_addr(
  void *data,
  void *user_data)
{
  struct reply_ack_t *pta = (struct reply_ack_t *)data;
  struct reply_ack_t *ptb = (struct reply_ack_t *)user_data;

  if(pta->pt_peer_str_len == ptb->pt_peer_str_len)
  {
    if(memcmp(pta->pt_peer_str, ptb->pt_peer_str,
              pta->pt_peer_str_len) == 0)
    {
      // Equal
      singleChannelConcurrentCnt++;
    }
  }
}
/**
 * @brief: Register the reply
 */
static bool
registerReplyAck(
  struct pthread_cond_var *pt_cond_var,
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  SKFChina_Froto_FrotoPmtType expPrimitive,
  SKFChina_Froto_FrotoMsgType expMsgType,
  bool checkSeqNo,
  uint32_t expSeqNo,
  uint32_t seqNo,
  struct reply_ack_t **reply_ack,
  uint8_t *errCode)
{
  struct reply_ack_t *_reply_ack = NULL;

  *reply_ack = NULL;

  // Check pt_cond_var
  if(NULL == pt_cond_var)
  {
    // The condition variable should be given
    DBG_LOG_ERR("Invalid cond var");
    return false;
  }
  // Check pt_sensorid_str
  if((NULL == pt_sensorid_str) || (0 == pt_sensorid_str_len))
  {
    // The peer address should be given
    DBG_LOG_ERR("Invalid sensor id");
    return false;
  }
  // Check primitive and msgType
  _reply_ack = l_malloc(sizeof(struct reply_ack_t));
  if(_reply_ack == NULL)
  {
    // TODO: SHOULD NOT BE HERE (BUT WHAT SHOULD WE DO IF REALLY BE HERE)
    DBG_LOG_ERR("Failed to malloc");
    return false;
  }
  _reply_ack->pt_cond_var = pt_cond_var;
  _reply_ack->pt_peer_str = l_malloc(pt_sensorid_str_len);
  memcpy(_reply_ack->pt_peer_str, pt_sensorid_str, pt_sensorid_str_len);
  _reply_ack->pt_peer_str_len = pt_sensorid_str_len;
  _reply_ack->ExpPrimitive = expPrimitive;
  _reply_ack->ExpMsgType = expMsgType;
  _reply_ack->checkSeqNo = checkSeqNo;
  _reply_ack->ExpSeqNo = expSeqNo;
  _reply_ack->SeqNo = seqNo;
  _reply_ack->ExpLongPktId = 0;
  _reply_ack->totalBlock = 0;
  _reply_ack->currentBlock = 0;
  _reply_ack->replyAck = NULL;
  _reply_ack->replyAckLen = 0;
  _reply_ack->replyMsgQueue = NULL;
  if(pthread_mutex_init(&(_reply_ack->replyMsgQueueMtx), NULL))
  {
    // TODO: SHOULD NOT BE HERE (BUT WHAT SHOULD WE DO IF REALLY BE HERE)
    DBG_LOG_ERR("Failed to initialize mutex");
    return false;
  }

  pthread_mutex_lock(&reply_ack_queue_mtx);
  // Check waiting number in one sensor and check the total waiting number
  // Only when both are not overflowed, the _reply_ack would be added to
  // the queue
  singleChannelConcurrentCnt = 0;
  l_queue_foreach(reply_ack_queue, q_match_addr, (void *)_reply_ack);
  if(singleChannelConcurrentCnt >=
     FROTO_MAX_SINGLE_CHANNEL_CONCURRENT_NUM_REPLY_ACK)
  {
    // If >= the max concurrent number in a single (connection) channel,
    // then free the memory and return false (i.e., failed to register)
    l_free(_reply_ack->pt_peer_str);
    l_free(_reply_ack);
    pthread_mutex_unlock(&reply_ack_queue_mtx);
    DBG_LOG_ERR("singleChannelConcurrentCnt (%d) >= %d", 
      singleChannelConcurrentCnt, 
      FROTO_MAX_SINGLE_CHANNEL_CONCURRENT_NUM_REPLY_ACK);
    return false;
  }
  else
  {
    uint32_t _tmp_queue_len = l_queue_length(reply_ack_queue);
    if(_tmp_queue_len >=
       FROTO_MAX_CONCURRENT_NUM_REPLY_ACK)
    {
      // If >= the max total concurrent number, then free the memory and
      // return false (i.e., failed to register)
      l_free(_reply_ack->pt_peer_str);
      l_free(_reply_ack);
      pthread_mutex_unlock(&reply_ack_queue_mtx);
      DBG_LOG_ERR("length of reply_ack_queue (%d) >= %d", 
        _tmp_queue_len, 
        FROTO_MAX_CONCURRENT_NUM_REPLY_ACK);
      return false;
    }

    DBG_LOG_DEBUG("length of reply_ack_queue (%d)", 
        _tmp_queue_len);
    // If < the max allowed concurrent number, then add the expected reply
    // to the queue and complete the registration
    l_queue_push_tail(reply_ack_queue, (void *)_reply_ack);
  }
  pthread_mutex_unlock(&reply_ack_queue_mtx);

  *reply_ack = _reply_ack;
  return true;
}
// typedef
// void (*l_queue_destroy_func_t) (void *data);
/**
 * @brief: The callback called by l_queue_destroy.
 */
static void
q_destroy_reply_msg_buf(void *data)
{
  struct msg_buf_t *pt = (struct msg_buf_t *)data;

  if(pt != NULL)
  {
    if(pt->replyMsgBuf != NULL)
    {
      l_free(pt->replyMsgBuf);
    }
    pt->replyMsgBufLen = 0;
    l_free(pt);
    pt = NULL;
  }
}
// typedef
// bool (*l_queue_remove_func_t) (void *data, void *user_data);
/**
 * @brief: The callback called by l_queue_foreach_remove.
 * @return true if matched
 */
static bool
q_match_reply_ack(
  void *data,
  void *user_data)
{
  struct reply_ack_t *pta = (struct reply_ack_t *)data;
  struct reply_ack_t *ptb = (struct reply_ack_t *)user_data;

  if((pta->ExpMsgType == ptb->ExpMsgType) &&
     (pta->ExpPrimitive == ptb->ExpPrimitive) &&
     (pta->ExpSeqNo == ptb->ExpSeqNo) &&
     (pta->SeqNo == ptb->SeqNo) &&
     (pta->checkSeqNo == ptb->checkSeqNo) &&
     (pta->pt_peer_str_len == ptb->pt_peer_str_len) &&
     (pta->pt_cond_var == ptb->pt_cond_var))
  {
    if(memcmp(pta->pt_peer_str,
              ptb->pt_peer_str,
              pta->pt_peer_str_len) == 0)
    {
      pthread_mutex_lock(&(pta->replyMsgQueueMtx));
      l_queue_destroy(pta->replyMsgQueue, q_destroy_reply_msg_buf);
      pthread_mutex_unlock(&(pta->replyMsgQueueMtx));
      pthread_mutex_destroy(&(pta->replyMsgQueueMtx));
      l_free((void *)(pta->pt_peer_str));
      pta->pt_peer_str_len = 0;
      l_free((void *)(pta->replyAck));
      pta->replyAckLen = 0;
      l_free(data);
      return true;
    }
  }

  return false;
}
// typedef
// bool (*l_queue_remove_func_t) (void *data, void *user_data);
/**
 * @brief: The callback called by l_queue_foreach_remove for gabage clean.
 * @return true if matched
 */
static bool
q_match_reply_ack_gabage_clean(
  void *data,
  void *user_data)
{
  struct reply_ack_t *pta = (struct reply_ack_t *)data;
  struct reply_ack_t *ptb = (struct reply_ack_t *)user_data;

  if(pta->pt_peer_str_len == ptb->pt_peer_str_len)
  {
    if(memcmp(pta->pt_peer_str,
              ptb->pt_peer_str,
              pta->pt_peer_str_len) == 0)
    {
      pthread_mutex_lock(&(pta->replyMsgQueueMtx));
      l_queue_destroy(pta->replyMsgQueue, q_destroy_reply_msg_buf);
      pthread_mutex_unlock(&(pta->replyMsgQueueMtx));
      pthread_mutex_destroy(&(pta->replyMsgQueueMtx));
      l_free((void *)(pta->pt_peer_str));
      pta->pt_peer_str_len = 0;
      l_free((void *)(pta->replyAck));
      pta->replyAckLen = 0;
      l_free(data);
      return true;
    }
  }

  return false;
}
// typedef
// bool (*l_queue_match_func_t) (const void *a, const void *b);
/**
 * @brief: The callback called by l_queue_find.
 * @return true if matched
 */
static bool
q_match_reply_ack_except_cond_var(
  const void *a,
  const void *b)
{
  struct reply_ack_t *pta = (struct reply_ack_t *)a;
  struct reply_ack_t *ptb = (struct reply_ack_t *)b;

  if((pta->ExpMsgType == ptb->ExpMsgType) &&
     (pta->ExpPrimitive == ptb->ExpPrimitive) &&
     (pta->pt_peer_str_len == ptb->pt_peer_str_len))
  {
    if(memcmp(pta->pt_peer_str, ptb->pt_peer_str,
              pta->pt_peer_str_len) == 0)
    {
      if(pta->checkSeqNo)
      {
        if(pta->ExpSeqNo == ptb->ExpSeqNo)
        {
          if(ptb->totalBlock > 1)
          {
            // For a splitted packet, update the packet split information
            pta->currentBlock = ptb->currentBlock;
            pta->totalBlock = ptb->totalBlock;
            pta->ExpLongPktId = ptb->ExpLongPktId;
            return true;
          }
          else
          {
            // For a non-splitted packet, return true directly
            return true;
          }
        }
      }
      else
      {
        for(uint32_t i = 0; i < ptb->replyAckLen; i++)
        {
          if(ptb->replyAck[i] == pta->SeqNo)
          {
            // Remove the matched one from the replyAck
            if(i == (ptb->replyAckLen - 1))
            {
              // If it is the last one, then just shorten the length
              ptb->replyAckLen = ptb->replyAckLen - 1;
            }
            else
            {
              memmove(&(ptb->replyAck[i]), &(ptb->replyAck[i + 1]),
                      ptb->replyAckLen - i - 1);
              ptb->replyAckLen = ptb->replyAckLen - 1;
            }
            if(ptb->totalBlock > 1)
            {
              // For a splitted packet, update the packet split
              // information
              pta->currentBlock = ptb->currentBlock;
              pta->totalBlock = ptb->totalBlock;
              pta->ExpLongPktId = ptb->ExpLongPktId;
              return true;
            }
            else
            {
              // For a non-splitted packet, return true directly
              return true;
            }
          }
        }
      }
      if(pta->ExpLongPktId == ptb->ExpLongPktId)
      {
        pta->currentBlock = ptb->currentBlock;
        pta->totalBlock = ptb->totalBlock;
      }
    }
  }

  return false;
}
bool app_froto_gabage_clean(
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  uint8_t *errCode)
{
  struct reply_ack_t *_reply_ack = NULL;
  DBG_LOG_DEBUG("Froto gabage clean is called");
  
  // Check pt_sensorid_str
  if((NULL == pt_sensorid_str) || (0 == pt_sensorid_str_len))
  {
    // The peer address should be given
    DBG_LOG_ERR("Invalid sensor id");
    return false;
  }

  _reply_ack = l_malloc(sizeof(struct reply_ack_t));
  if(_reply_ack == NULL)
  {
    DBG_LOG_ERR("Failed to malloc");
    return false;
  }
  _reply_ack->pt_peer_str = l_malloc(pt_sensorid_str_len);
  memcpy(_reply_ack->pt_peer_str, pt_sensorid_str, pt_sensorid_str_len);
  _reply_ack->pt_peer_str_len = pt_sensorid_str_len;

  pthread_mutex_lock(&reply_ack_queue_mtx);
  singleChannelConcurrentCnt = 0;
  l_queue_foreach_remove(reply_ack_queue, q_match_reply_ack_gabage_clean,
                         (void *)_reply_ack);
  uint32_t _tmp_queue_len = l_queue_length(reply_ack_queue);
  DBG_LOG_DEBUG("length of reply_ack_queue (%d)", 
        _tmp_queue_len);
  pthread_mutex_unlock(&reply_ack_queue_mtx);

  l_free(_reply_ack->pt_peer_str);
  l_free(_reply_ack);

  return true;
  
}

static bool
unregisterReplyAck(
  struct pthread_cond_var *pt_cond_var,
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  SKFChina_Froto_FrotoPmtType expPrimitive,
  SKFChina_Froto_FrotoMsgType expMsgType,
  bool checkSeqNo,
  uint32_t expSeqNo,
  uint32_t seqNo,
  uint8_t *errCode)
{
  struct reply_ack_t *_reply_ack = NULL;

  // Check pt_cond_var
  if(NULL == pt_cond_var)
  {
    // The condition variable should be given
    return false;
  }
  // Check pt_sensorid_str
  if((NULL == pt_sensorid_str) || (0 == pt_sensorid_str_len))
  {
    // The peer address should be given
    return false;
  }

  _reply_ack = l_malloc(sizeof(struct reply_ack_t));
  if(_reply_ack == NULL)
  {
    return false;
  }
  _reply_ack->pt_cond_var = pt_cond_var;
  _reply_ack->pt_peer_str = l_malloc(pt_sensorid_str_len);
  memcpy(_reply_ack->pt_peer_str, pt_sensorid_str, pt_sensorid_str_len);
  _reply_ack->pt_peer_str_len = pt_sensorid_str_len;
  _reply_ack->ExpPrimitive = expPrimitive;
  _reply_ack->ExpMsgType = expMsgType;
  _reply_ack->checkSeqNo = checkSeqNo;
  _reply_ack->ExpSeqNo = expSeqNo;
  _reply_ack->SeqNo = seqNo;

  pthread_mutex_lock(&reply_ack_queue_mtx);
  singleChannelConcurrentCnt = 0;
  l_queue_foreach_remove(reply_ack_queue, q_match_reply_ack,
                         (void *)_reply_ack);
  pthread_mutex_unlock(&reply_ack_queue_mtx);

  l_free(_reply_ack->pt_peer_str);
  l_free(_reply_ack);

  return true;
}
bool
app_froto_n_parser(
  uint8_t *readBuf,
  uint32_t readBufLen,
  froto_after_parse_t data_upload_processor,
  froto_after_parse_t config_hash_upload_processor,
  froto_after_parse_t specific_config_upload_processor,
  froto_after_parse_t current_version_upload_processor,
  froto_after_parse_t fuota_status_processor,
  froto_after_parse_t image_block_request_processor,
  froto_after_parse_t config_dissem_processor,
  froto_after_parse_t file_notify_dissem_processor,
  froto_after_parse_t image_block_dissem_processor,
  froto_after_parse_t config_retrieve_processor,
  froto_after_parse_t image_block_retrieve_processor,
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
  SKFChina_Froto_FrotoHeader *_Froto_header;

  pb_wire_type_t _wire_type;
  pb_istream_t _istream = pb_istream_from_buffer(readBuf, readBufLen);

  if(true == pb_decode(&_istream, SKFChina_App_AppMessage_fields,
                       &_SKF_Froto_App))
  {
    DBG_LOG_DEBUG("Froto APP version: %d", _SKF_Froto_App.appVer);
    DBG_LOG_INFO("Froto APP which__messages: %d",
                  _SKF_Froto_App.which__messages);
    switch(_SKF_Froto_App.which__messages)
    {
      case SKFChina_App_AppMessage_data_upload_tag:
        app_froto_data_upload_parser(readBuf, readBufLen, &_istream,
                                     &_SKF_Froto_App, &_Froto_header,
                                     data_upload_processor, NULL);
        break;
      case SKFChina_App_AppMessage_config_hash_upload_tag:
        app_froto_config_hash_upload_parser(readBuf, readBufLen, &_istream,
                                            &_SKF_Froto_App,
                                            &_Froto_header,
                                            config_hash_upload_processor,
                                            NULL);
        break;
      case SKFChina_App_AppMessage_specific_config_upload_tag:
        app_froto_specific_config_upload_parser(readBuf, readBufLen,
                                                &_istream,
                                                &_SKF_Froto_App,
                                                &_Froto_header,
                                                specific_config_upload_processor,
                                                NULL);
        break;
      case SKFChina_App_AppMessage_current_version_upload_tag:
        app_froto_current_version_upload_parser(readBuf, readBufLen,
                                                &_istream,
                                                &_SKF_Froto_App,
                                                &_Froto_header,
                                                current_version_upload_processor,
                                                NULL);
        break;
      case SKFChina_App_AppMessage_fuota_status_tag:
        app_froto_fuota_status_parser(readBuf, readBufLen,
                                      &_istream,
                                      &_SKF_Froto_App,
                                      &_Froto_header,
                                      fuota_status_processor,
                                      NULL);
        break;
      case SKFChina_App_AppMessage_image_block_request_tag:
        app_froto_image_block_request_parser(readBuf, readBufLen,
                                             &_istream,
                                             &_SKF_Froto_App,
                                             &_Froto_header,
                                             image_block_request_processor,
                                             NULL);
        break;
      case SKFChina_App_AppMessage_config_dissem_tag:
        app_froto_config_dissem_parser(readBuf, readBufLen,
                                       &_istream,
                                       &_SKF_Froto_App,
                                       &_Froto_header,
                                       config_dissem_processor,
                                       NULL);
        break;
      case SKFChina_App_AppMessage_file_notify_dissem_tag:
        app_froto_file_notify_dissem_parser(readBuf, readBufLen,
                                            &_istream,
                                            &_SKF_Froto_App,
                                            &_Froto_header,
                                            file_notify_dissem_processor,
                                            NULL);
        break;
      case SKFChina_App_AppMessage_image_block_dissem_tag:
        app_froto_image_block_dissem_parser(readBuf, readBufLen,
                                            &_istream,
                                            &_SKF_Froto_App,
                                            &_Froto_header,
                                            image_block_dissem_processor,
                                            NULL);
        break;
      case SKFChina_App_AppMessage_config_retrieve_tag:
        app_froto_config_retrieve_parser(readBuf, readBufLen,
                                         &_istream,
                                         &_SKF_Froto_App,
                                         &_Froto_header,
                                         config_retrieve_processor,
                                         NULL);
        break;
      case SKFChina_App_AppMessage_image_block_retrieve_tag:
        app_froto_image_block_retrieve_parser(readBuf, readBufLen,
                                              &_istream,
                                              &_SKF_Froto_App,
                                              &_Froto_header,
                                              image_block_retrieve_processor,
                                              NULL);
        break;
      default:
        if(false == pb_skip_field(&_istream, _wire_type))
        {
          DBG_LOG_WARN(
            "Should not reach here: unknown oneof _message, just skip it");
          return false;
        }
        break;
    }
  }
  DBG_LOG_DEBUG("bytes_left: %d", _istream.bytes_left);
}
bool
app_froto_n_get_message_type(
  uint8_t *readBuf,
  uint32_t readBufLen,
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
  SKFChina_Froto_FrotoHeader *_Froto_header;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];
  uint32_t _froto_app_root_cnt = 0;
  uint32_t _froto_msg_start_cnt = 0;
  uint32_t _froto_msg_end_cnt = 0;
  uint8_t *_tmp_buf = NULL;
  pb_istream_t _istream = pb_istream_from_buffer(readBuf, readBufLen);
  pb_istream_t _substream;
  pb_istream_t _headerstream;

  pb_wire_type_t _wire_type;
  uint32_t _tag;
  bool _eof;
  pb_field_iter_t _iter;
  pb_field_iter_t _header_iter;

  while(pb_decode_tag(&_istream, &_wire_type, &_tag, &_eof))
  {
    DBG_LOG_DEBUG("_tag: %d", _tag);

    if(_tag == SKFChina_App_AppMessage_appVer_tag)
    {
      // Version
      _froto_app_root_cnt++;
      if(true == pb_decode_varint32(&_istream, &_SKF_Froto_App.appVer))
      {
        DBG_LOG_DEBUG("Froto APP version: %d", _SKF_Froto_App.appVer);
      }
    }
    else
    {
      if(_wire_type == PB_WT_STRING)
      {
        // The oneof _message
        // SKFChina_App_AppMessage_data_upload_tag
        // SKFChina_App_AppMessage_data_selection_tag
        // SKFChina_App_AppMessage_config_retrieve_tag
        // SKFChina_App_AppMessage_config_hash_upload_tag
        // SKFChina_App_AppMessage_specific_config_upload_tag
        // SKFChina_App_AppMessage_config_dissem_tag
        // SKFChina_App_AppMessage_command_dissem_tag
        // SKFChina_App_AppMessage_version_retrieve_tag
        // SKFChina_App_AppMessage_current_version_upload_tag
        // SKFChina_App_AppMessage_last_fuota_upload_tag
        // SKFChina_App_AppMessage_fuota_notify_dissem_tag
        // SKFChina_App_AppMessage_fuota_status_tag
        // SKFChina_App_AppMessage_image_block_dissem_tag
        // SKFChina_App_AppMessage_image_block_request_tag
        // SKFChina_App_AppMessage_ack_tag
        // SKFChina_App_AppMessage_debug_message_tag
        // SKFChina_App_AppMessage_file_notify_dissem_tag
        // SKFChina_App_AppMessage_file_notify_upload_tag
        // SKFChina_App_AppMessage_image_block_upload_tag
        // SKFChina_App_AppMessage_image_block_retrieve_tag

        pb_wire_type_t _sub_wire_type;
        uint32_t _sub_tag;
        uint32_t _expected_sub_tag;
        bool _sub_eof;
        protobuf_codec_general_bytes_t _sensor_id;
        protobuf_codec_acked_seq_number_t _acked_seq_no_array;

        switch(_tag)
        {
          case SKFChina_App_AppMessage_data_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.data_upload.header);
            _expected_sub_tag =
              SKFChina_SensingDataUpload_DataUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_data_selection_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.data_selection.header);
            _expected_sub_tag =
              SKFChina_SensingDataUpload_DataSelectionDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_config_retrieve_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.config_retrieve.header);
            _expected_sub_tag =
              SKFChina_ConfigurationAndCommand_ConfigRetrieve_header_tag;
            break;
          case SKFChina_App_AppMessage_config_hash_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.config_hash_upload.header);
            _expected_sub_tag =
              SKFChina_ConfigurationAndCommand_ConfigHashUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_specific_config_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.specific_config_upload.
                header);
            _expected_sub_tag =
              SKFChina_ConfigurationAndCommand_SpecificConfigUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_config_dissem_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.config_dissem.header);
            _expected_sub_tag =
              SKFChina_ConfigurationAndCommand_ConfigDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_command_dissem_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.command_dissem.header);
            _expected_sub_tag =
              SKFChina_ConfigurationAndCommand_CommandDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_version_retrieve_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.version_retrieve.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_VersionRetrieve_header_tag;
            break;
          case SKFChina_App_AppMessage_current_version_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.current_version_upload.
                header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_CurrentVersionUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_last_fuota_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.last_fuota_upload.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_LastFUOTAUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_fuota_notify_dissem_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.fuota_notify_dissem.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_fuota_status_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.fuota_status.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_UpdateStatusUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_image_block_dissem_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.image_block_dissem.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_image_block_request_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.image_block_request.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_header_tag;
            break;
          case SKFChina_App_AppMessage_ack_tag:
            _Froto_header = &(_SKF_Froto_App._messages.ack.header);
            _expected_sub_tag =
              SKFChina_Froto_FrotoAckMsg_header_tag;
            break;
          case SKFChina_App_AppMessage_debug_message_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.debug_message.header);
            _expected_sub_tag =
              SKFChina_Debug_DebugMessage_header_tag;
            break;
          case SKFChina_App_AppMessage_file_notify_dissem_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.file_notify_dissem.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_file_notify_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.file_notify_upload.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_image_block_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.image_block_upload.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_image_block_retrieve_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.image_block_retrieve.
                header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_header_tag;
            break;
          default:
            if(false == pb_skip_field(&_istream, _wire_type))
            {
              DBG_LOG_WARN(
                "Should not reach here: unknown oneof _message, just skip it");
              return false;
            }
            break;
        }

        pb_make_string_substream(&_istream, &_substream);
        while(pb_decode_tag(&_substream, &_sub_wire_type, &_sub_tag,
                            &_sub_eof))
        {
          if(_sub_wire_type == PB_WT_STRING)
          {
            if(_expected_sub_tag == _sub_tag)
            {
              DBG_LOG_DEBUG("Header is found");

              if(pb_field_iter_begin(&_iter,
                                     SKFChina_App_AppMessage_fields,
                                     NULL) &&
                 pb_field_iter_find(&_iter, _tag))
              {
                DBG_LOG_DEBUG("Iter (%d) has been found!", _tag);
                if(pb_field_iter_begin(&_header_iter, _iter.submsg_desc,
                                       NULL) &&
                   pb_field_iter_find(&_header_iter, _sub_tag))
                {
                  DBG_LOG_DEBUG("Iter (%d) has been found!", _sub_tag);

                  pb_make_string_substream(&_substream, &_headerstream);

                  _sensor_id.buffer = _peer_id;
                  _sensor_id.size = sizeof(_peer_id);
                  _sensor_id.length = 0;
                  _Froto_header->sensor_id.arg = &_sensor_id;
                  _Froto_header->sensor_id.funcs.decode =
                    &protobuf_dec_bytes_callback;

                  _acked_seq_no_array.buffer = _acked_seq_no;
                  _acked_seq_no_array.size = sizeof(_acked_seq_no) /
                                             sizeof(_acked_seq_no[0]);
                  _acked_seq_no_array.cnt = 0;
                  _Froto_header->acked_message_seq_number.arg =
                    &_acked_seq_no_array;
                  _Froto_header->acked_message_seq_number.funcs.decode =
                    &protobuf_dec_acked_seq_number_callback;

                  if(true == pb_decode(&_headerstream,
                                       _header_iter.submsg_desc,
                                       _Froto_header))
                  {
                    DBG_LOG_DEBUG("Get Froto header done!");
                  }
                  else
                  {
                    DBG_LOG_WARN("Failed to get Froto header, %s",
                                 _istream.errmsg);
                    return false;
                  }
                  pb_close_string_substream(&_substream, &_headerstream);
                }
              }
              break;
            }
          }
        }
        pb_close_string_substream(&_istream, &_substream);

        // A completed Froto message has been read
        _froto_msg_end_cnt = readBufLen - _istream.bytes_left - 1;
        _tmp_buf = l_malloc(_froto_msg_end_cnt - _froto_msg_start_cnt + 1);
        if(_tmp_buf == NULL)
        {
          DBG_LOG_ERR("Failed to malloc");
          return false;
        }
        memcpy(_tmp_buf, &readBuf[_froto_msg_start_cnt],
               _froto_msg_end_cnt - _froto_msg_start_cnt + 1);

        struct reply_ack_t *_reply_ack =
          l_malloc(sizeof(struct reply_ack_t));

        if(_reply_ack == NULL)
        {
          l_free(_tmp_buf);
          _tmp_buf = NULL;
          DBG_LOG_ERR("Failed to malloc");
          return false;
        }
        _reply_ack->ExpPrimitive = _Froto_header->primitive_type;
        _reply_ack->ExpMsgType = _Froto_header->message_type;
        _reply_ack->ExpSeqNo = _Froto_header->message_seq_no;
        _reply_ack->ExpLongPktId = _Froto_header->long_packet_id;
        _reply_ack->totalBlock = _Froto_header->total_block;
        if(_Froto_header->has_current_block)
        {
          _reply_ack->currentBlock = _Froto_header->current_block;
        }
        else
        {
          _reply_ack->currentBlock = 0;
        }
        _reply_ack->replyAck =
          ((protobuf_codec_acked_seq_number_t *)(_Froto_header->
                                                 acked_message_seq_number.
                                                 arg))
          ->buffer;
        _reply_ack->replyAckLen =
          ((protobuf_codec_acked_seq_number_t *)(_Froto_header->
                                                 acked_message_seq_number.
                                                 arg))
          ->cnt;
        _reply_ack->pt_peer_str =
          ((protobuf_codec_general_bytes_t *)(_Froto_header->sensor_id.arg))
          ->buffer;
        _reply_ack->pt_peer_str_len =
          ((protobuf_codec_general_bytes_t *)(_Froto_header->sensor_id.arg))
          ->length;

        DBG_LOG_INFO("Froto message length: %d bytes",
                      _froto_msg_end_cnt - _froto_msg_start_cnt + 1);
        DBG_LOG_INFO("_Froto_header->message_seq_no: %d",
                      _Froto_header->message_seq_no);
        DBG_LOG_DEBUG("_Froto_header->primitive_type: %d",
                      _Froto_header->primitive_type);
        DBG_LOG_INFO("_Froto_header->message_type: %d",
                      _Froto_header->message_type);
        DBG_LOG_DEBUG("_Froto_header->acked_message_seq_number: ");
        for(uint32_t i = 0;
            i < ((protobuf_codec_acked_seq_number_t *)(_Froto_header->
                                                       acked_message_seq_number
                                                       .arg))->cnt;
            i++)
        {
          DBG_LOG_DEBUG("%d",
                        ((protobuf_codec_acked_seq_number_t *)(
                           _Froto_header->acked_message_seq_number.arg))->
                        buffer[i]);
        }
        DBG_LOG_DEBUG("_Froto_header->sensor_id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(_Froto_header->sensor_id.arg))
          ->buffer,
          ((protobuf_codec_general_bytes_t *)(_Froto_header->sensor_id.arg))
          ->length);

        // Find in the queue
        pthread_mutex_lock(&reply_ack_queue_mtx);
        singleChannelConcurrentCnt = 0;

        struct reply_ack_t *_found_reply_ack = NULL;

        _found_reply_ack = l_queue_find(reply_ack_queue,
                                        q_match_reply_ack_except_cond_var,
                                        (void *)_reply_ack);
        if(_found_reply_ack != NULL)
        {
          struct msg_buf_t *_msg_buf = l_malloc(sizeof(struct msg_buf_t));

          _msg_buf->replyMsgBuf = _tmp_buf;
          _msg_buf->replyMsgBufLen = _froto_msg_end_cnt -
                                     _froto_msg_start_cnt + 1;

          pthread_mutex_lock(&(_found_reply_ack->replyMsgQueueMtx));
          if(_found_reply_ack->replyMsgQueue == NULL)
          {
            _found_reply_ack->replyMsgQueue = l_queue_new();
          }
          l_queue_push_tail(_found_reply_ack->replyMsgQueue,
                            (void *)_msg_buf);
          pthread_mutex_unlock(&(_found_reply_ack->replyMsgQueueMtx));

          // Trigger the signal if found
          if(pthread_mutex_trylock(&_found_reply_ack->pt_cond_var->mtx))
          {
            DBG_LOG_ERR(
              "Failed to acquire the lock (perhaps timeout has happened)");
          }
          else
          {
            _found_reply_ack->pt_cond_var->is_op_done = true;
            pthread_cond_signal(&_found_reply_ack->pt_cond_var->cond_var);
            pthread_mutex_unlock(&_found_reply_ack->pt_cond_var->mtx);
          }
          pthread_mutex_unlock(&reply_ack_queue_mtx);

          // Continue finding till all the elements in the queue has been
          // checked
          do{
            // Similarly, find first
            _found_reply_ack = NULL;
            pthread_mutex_lock(&reply_ack_queue_mtx);
            singleChannelConcurrentCnt = 0;
            _found_reply_ack = l_queue_find(reply_ack_queue,
                                            q_match_reply_ack_except_cond_var,
                                            (void *)_reply_ack);

            // Trigger the signal once found
            // Note: the replyMsgBuf only copied once. So here, no
            // replyMsgBuf needs to be copied
            if(_found_reply_ack != NULL)
            {
              if(pthread_mutex_trylock(
                   &_found_reply_ack->pt_cond_var->mtx))
              {
                DBG_LOG_ERR(
                  "Failed to acquire the lock (perhaps timeout has happened)");
              }
              else
              {
                _found_reply_ack->pt_cond_var->is_op_done = true;
                pthread_cond_signal(
                  &_found_reply_ack->pt_cond_var->cond_var);
                pthread_mutex_unlock(&_found_reply_ack->pt_cond_var->mtx);
              }
            }
            pthread_mutex_unlock(&reply_ack_queue_mtx);
          }while(_found_reply_ack);

          l_free(_reply_ack);
          _reply_ack = NULL;
        }
        else
        {
          // Not found
          // TODO ...
          app_froto_n_parser(_tmp_buf,
                             _froto_msg_end_cnt - _froto_msg_start_cnt + 1,
                             NULL, // The routine to process parsed data
                                   // upload messages
                             NULL, // The routine to process parsed config
                                   // hash upload messages
                             NULL, // The routine to process parsed
                                   // specific config upload messages
                             NULL, // The routine to process parsed
                                   // current version upload messages
                             NULL, // The routine to process parsed
                                   // fuota status messages
                             NULL, // The routine to process parsed
                                   // image block request messages
                             NULL, // The routine to process parsed
                                   // config dissem messages
                             NULL, // The routine to process parsed
                                   // file notify dissem messages
                             NULL, // The routine to process parsed
                                   // image block dissem messages
                             NULL, // The routine to process parsed
                                   // config retrieve messages
                             NULL, // The routine to process parsed
                                   // image block retrieve messages
                             NULL);
          l_free(_tmp_buf);
          _tmp_buf = NULL;

          pthread_mutex_unlock(&reply_ack_queue_mtx);
          l_free(_reply_ack);
          _reply_ack = NULL;
        }

        _froto_msg_start_cnt = readBufLen - _istream.bytes_left;

        memset((void *)&_SKF_Froto_App, 0, sizeof(_SKF_Froto_App));
      }
      else
      {
        DBG_LOG_DEBUG("Unknown field. Just skip it ...");
        if(false == pb_skip_field(&_istream, _wire_type))
        {
          break;
        }
      }
    }

    DBG_LOG_DEBUG("bytes_left: %d", _istream.bytes_left);
  }
}
/**
 * @brief Send a message (and wait for a reply). The reply message can be
 * assigned by the parameters (e.g., the primitive, message type, seqNo,
 * etc.) of the routine.
 * @param encodedBuf Pointer to the encoded message.
 * @param encodedBufLen The length (in byte) of the returned (encoded)
 * message.
 * @param userData This can be used to pass some parameters used by send
 * routine.
 * @param timeout_s The timeout in second. If nothing has been received
 * till the timeout timer expired, the routine would return false.
 * @param send An implementation of sending the encoded message pointed by
 * @p encodedBuf
 * @param readBuf Pointed to the read (undecoded) message
 * @param maxReadBufLen The max length (in byte) of @p readBuf
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * If read message is longer than @p maxReadBufLen , then the routine would
 * return false.
 * @param pt_sensorid_str The peer's ID
 * @param pt_sensorid_str_len The length of @p pt_sensorid_str
 * @param expPrimitive The expected primitive
 * @param expMsgType Expected message type
 * @param checkSeqNo Need to check the seqNo or not. Check @p expSeqNo if
 * true
 * @param expSeqNo Only valid when @p checkSeqNo is true
 * @param seqNo Seq number of the sending message
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_n_send_with_reply(
  uint8_t *encodedBuf,
  uint32_t encodedBufLen,
  void *userData,
  uint32_t timeout_s,
  froto_send_msg_t send,
  uint8_t *readBuf,
  uint32_t maxReadBufLen,
  uint32_t *readBufLen,
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  SKFChina_Froto_FrotoPmtType expPrimitive,
  SKFChina_Froto_FrotoMsgType expMsgType,
  bool checkSeqNo,
  uint32_t expSeqNo,
  uint32_t seqNo,
  uint8_t *errCode)
{
  uint8_t _errCode;
  app_state_t _ret = ST_OK;
  bool _rt = true;
  struct pthread_cond_var _cond_var;
  struct reply_ack_t *_reply_ack = NULL;

  if((encodedBuf == NULL) || (encodedBufLen == 0))
  {
    // There seems nothing to send
    return false;
  }
  if(send == NULL)
  {
    // No sending method is provided
    return false;
  }
  if((pt_sensorid_str_len == 0) || (pt_sensorid_str == NULL))
  {
    // Sensor ID should be provided
    return false;
  }
  if(readBufLen != NULL)
  {
    *readBufLen = 0;
  }

  _ret = app_com_init_pthread_cond_var_ctr(&_cond_var);
  if(ST_OK != _ret)
  {
    DBG_LOG_ERR(
      "Failed to initialize the condition variable with attributes");
    return false;
  }

  // Setup the timeout timer
  _cond_var.is_op_done = false;
  clock_gettime(CLOCK_MONOTONIC, &_cond_var.tv);
  _cond_var.tv.tv_sec = _cond_var.tv.tv_sec + timeout_s;
  if(false == registerReplyAck(&_cond_var,
                               pt_sensorid_str,
                               pt_sensorid_str_len,
                               expPrimitive,
                               expMsgType,
                               checkSeqNo,
                               expSeqNo,
                               seqNo,
                               &_reply_ack,
                               _errCode))
  {
    _rt = false;
    DBG_LOG_ERR("Failed to register");
    goto EXIT;
  }

  if(false == send(userData, encodedBuf, encodedBufLen, &_errCode))
  {
    unregisterReplyAck(&_cond_var,
                       pt_sensorid_str,
                       pt_sensorid_str_len,
                       expPrimitive,
                       expMsgType, checkSeqNo,
                       expSeqNo,
                       seqNo,
                       _errCode);
    _rt = false;
    DBG_LOG_ERR("Failed to send");
    goto EXIT;
  }

  pthread_mutex_lock(&_cond_var.mtx);
  while(false == _cond_var.is_op_done)
  {
    if(pthread_cond_timedwait(&_cond_var.cond_var, &_cond_var.mtx,
                              &_cond_var.tv) == ETIMEDOUT)
    {
      DBG_LOG_ERR("Wait for reply timeout");

      _rt = false;
      break;
    }
  }
  if(_cond_var.is_op_done == true)
  {
    DBG_LOG_INFO("app_froto_n_send_with_reply is successful");
    pthread_mutex_lock(&(_reply_ack->replyMsgQueueMtx));
    if(l_queue_length(_reply_ack->replyMsgQueue) == 1)
    {
      DBG_LOG_DEBUG("The message is available.");

      struct msg_buf_t *msg_buf =
        l_queue_pop_head(_reply_ack->replyMsgQueue);

      if((readBuf != NULL) &&
         (maxReadBufLen >= msg_buf->replyMsgBufLen) &&
         (readBufLen != NULL))
      {
        memcpy(readBuf, msg_buf->replyMsgBuf,
               msg_buf->replyMsgBufLen);
        *readBufLen = msg_buf->replyMsgBufLen;
        l_free(msg_buf->replyMsgBuf);
        msg_buf->replyMsgBuf = NULL;
        msg_buf->replyMsgBufLen = 0;
        l_free(msg_buf);
        msg_buf = NULL;
      }
      else
      {
        DBG_LOG_DEBUG("The space is unavailable.");
      }
    }
    else
    {
      DBG_LOG_DEBUG("There seems no message or multiple messages (%d).",
                    l_queue_length(_reply_ack->replyMsgQueue));
    }
    pthread_mutex_unlock(&(_reply_ack->replyMsgQueueMtx));
  }
  pthread_mutex_unlock(&_cond_var.mtx);
  unregisterReplyAck(&_cond_var,
                     pt_sensorid_str,
                     pt_sensorid_str_len,
                     expPrimitive,
                     expMsgType, checkSeqNo,
                     expSeqNo,
                     seqNo,
                     _errCode);

EXIT:
  app_com_deinit_pthread_cond_var_ctr(&_cond_var);
  return _rt;
}
/**
 * @brief Send a message (and wait for splitted replies) (first half: just
 * send message and wait for the first splitted replies). The reply message
 * can be assigned by the parameters (e.g., the primitive, message type,
 * seqNo, etc.) of the routine. The routine would be returned till the
 * first
 * splitted packet received or timeout happens.
 * @param encodedBuf Pointer to the encoded message.
 * @param encodedBufLen The length (in byte) of the returned (encoded)
 * message.
 * @param userData This can be used to pass some parameters used by send
 * routine.
 * @param timeout_s The timeout in second. If nothing has been received
 * till the timeout timer expired, the routine would return false.
 * @param send An implementation of sending the encoded message pointed by
 * @p encodedBuf
 * @param readBuf Pointed to the read (undecoded) message
 * @param maxReadBufLen The max length (in byte) of @p readBuf
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * If read message is longer than @p maxReadBufLen , then the routine would
 * return false.
 * @param pt_sensorid_str The peer's ID
 * @param pt_sensorid_str_len The length of @p pt_sensorid_str
 * @param expPrimitive The expected primitive
 * @param expMsgType Expected message type
 * @param checkSeqNo Need to check the seqNo or not. Check @p expSeqNo if
 * true
 * @param expSeqNo Only valid when @p checkSeqNo is true
 * @param seqNo Seq number of the sending message
 * @param cond_var Condition variable which would be used by
 * @p app_froto_n_send_with_multiple_replies_second_half
 * @param state A pointer to a state which would be used by
 * @p app_froto_n_send_with_multiple_replies_second_half
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_n_send_with_multiple_replies_first_half(
  uint8_t *encodedBuf,
  uint32_t encodedBufLen,
  void *userData,
  uint32_t timeout_s,
  froto_send_msg_t send,
  uint8_t *readBuf,
  uint32_t maxReadBufLen,
  uint32_t *readBufLen,
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  SKFChina_Froto_FrotoPmtType expPrimitive,
  SKFChina_Froto_FrotoMsgType expMsgType,
  bool checkSeqNo,
  uint32_t expSeqNo,
  uint32_t seqNo,
  struct pthread_cond_var *cond_var,
  void **state,
  uint8_t *errCode)
{
  uint8_t _errCode;
  app_state_t _ret = ST_OK;
  bool _rt = true;
  struct reply_ack_t *_reply_ack = NULL;

  if(cond_var == NULL)
  {
    // The condition variable must be provided
    return false;
  }
  if(state == NULL)
  {
    // A state pointer should be given
    return false;
  }
  if((encodedBuf == NULL) || (encodedBufLen == 0))
  {
    // There seems nothing to send
    return false;
  }
  if(send == NULL)
  {
    // No sending method is provided
    return false;
  }
  if((pt_sensorid_str_len == 0) || (pt_sensorid_str == NULL))
  {
    // Sensor ID should be provided
    return false;
  }

  _ret = app_com_init_pthread_cond_var_ctr(cond_var);
  if(ST_OK != _ret)
  {
    DBG_LOG_ERR(
      "Failed to initialize the condition variable with attributes");
    return false;
  }

  // Setup the timeout timer
  cond_var->is_op_done = false;
  clock_gettime(CLOCK_MONOTONIC, &cond_var->tv);
  cond_var->tv.tv_sec = cond_var->tv.tv_sec + timeout_s;
  if(false == registerReplyAck(cond_var,
                               pt_sensorid_str,
                               pt_sensorid_str_len,
                               expPrimitive,
                               expMsgType,
                               checkSeqNo,
                               expSeqNo,
                               seqNo,
                               &_reply_ack,
                               _errCode))
  {
    _rt = false;
    DBG_LOG_ERR("Failed to register");
    goto EXIT;
  }

  if(false == send(userData, encodedBuf, encodedBufLen, &_errCode))
  {
    unregisterReplyAck(cond_var,
                       pt_sensorid_str,
                       pt_sensorid_str_len,
                       expPrimitive,
                       expMsgType, checkSeqNo,
                       expSeqNo,
                       seqNo,
                       _errCode);
    _rt = false;
    DBG_LOG_ERR("Failed to send");
    goto EXIT;
  }

  pthread_mutex_lock(&cond_var->mtx);
  while(false == cond_var->is_op_done)
  {
    if(pthread_cond_timedwait(&cond_var->cond_var, &cond_var->mtx,
                              &cond_var->tv) == ETIMEDOUT)
    {
      DBG_LOG_ERR("Wait for reply timeout");
      _rt = false;
      break;
    }
  }

  if(cond_var->is_op_done == true)
  {
    DBG_LOG_INFO(
      "app_froto_n_send_with_multiple_replies_first_half is successful");
    *state = _reply_ack;
    pthread_mutex_lock(&(_reply_ack->replyMsgQueueMtx));
    if(l_queue_length(_reply_ack->replyMsgQueue) > 0)
    {
      DBG_LOG_DEBUG("The message is available.");

      struct msg_buf_t *msg_buf =
        l_queue_pop_head(_reply_ack->replyMsgQueue);

      if((readBuf != NULL) &&
         (maxReadBufLen >= msg_buf->replyMsgBufLen) &&
         (readBufLen != NULL))
      {
        memcpy(readBuf, msg_buf->replyMsgBuf,
               msg_buf->replyMsgBufLen);
        *readBufLen = msg_buf->replyMsgBufLen;
        l_free(msg_buf->replyMsgBuf);
        msg_buf->replyMsgBuf = NULL;
        msg_buf->replyMsgBufLen = 0;
        l_free(msg_buf);
        msg_buf = NULL;
      }
      else
      {
        DBG_LOG_DEBUG("The space is unavailable.");
      }
    }
    else
    {
      DBG_LOG_DEBUG("There seems no message.");
    }
    pthread_mutex_unlock(&(_reply_ack->replyMsgQueueMtx));
  }
  pthread_mutex_unlock(&cond_var->mtx);
  if(cond_var->is_op_done == false)
  {
    unregisterReplyAck(cond_var,
                       pt_sensorid_str,
                       pt_sensorid_str_len,
                       expPrimitive,
                       expMsgType, checkSeqNo,
                       expSeqNo,
                       seqNo,
                       _errCode);
  }

EXIT:
  if(cond_var->is_op_done == false)
  {
    app_com_deinit_pthread_cond_var_ctr(cond_var);
  }
  return _rt;
}
/**
 * @brief Wait for a splitted replies. This routine is generally called
 * after @p app_froto_n_send_with_multiple_replies_first_half . The routine
 * would be returned till a splitted packet received or timeout happens.
 * @param timeout_s The timeout in second. If nothing has been received
 * till the timeout timer expired, the routine would return false.
 * @param readBuf Pointed to the read (undecoded) message
 * @param maxReadBufLen The max length (in byte) of @p readBuf
 * @param readBufLen The length (in byte) of the read (undecoded) message.
 * If read message is longer than @p maxReadBufLen , then the routine would
 * return false.
 * @param pt_sensorid_str The peer's ID
 * @param pt_sensorid_str_len The length of @p pt_sensorid_str
 * @param expPrimitive The expected primitive
 * @param expMsgType Expected message type
 * @param checkSeqNo Need to check the seqNo or not. Check @p expSeqNo if
 * true
 * @param expSeqNo Only valid when @p checkSeqNo is true
 * @param seqNo Seq number of the sending message
 * @param cond_var Condition variable
 * @param state A pointer to a state
 * @param lastOne The condition variable and state would be uninitialized
 * if true.
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_n_send_with_multiple_replies_second_half(
  uint32_t timeout_s,
  uint8_t *readBuf,
  uint32_t maxReadBufLen,
  uint32_t *readBufLen,
  const uint8_t *pt_sensorid_str,
  uint8_t pt_sensorid_str_len,
  SKFChina_Froto_FrotoPmtType expPrimitive,
  SKFChina_Froto_FrotoMsgType expMsgType,
  bool checkSeqNo,
  uint32_t expSeqNo,
  uint32_t seqNo,
  struct pthread_cond_var *cond_var,
  void **state,
  bool lastOne,
  uint8_t *errCode)
{
  uint8_t _errCode;
  bool _rt = true;
  struct reply_ack_t *_reply_ack = NULL;

  if(cond_var == NULL)
  {
    // The condition variable must be provided
    return false;
  }
  if(state == NULL)
  {
    // A state pointer should be given
    return false;
  }
  if((pt_sensorid_str_len == 0) || (pt_sensorid_str == NULL))
  {
    // Sensor ID should be provided
    return false;
  }

  _reply_ack = (struct reply_ack_t *)(*state);

  // Check the queue first
  pthread_mutex_lock(&(_reply_ack->replyMsgQueueMtx));
  if(l_queue_length(_reply_ack->replyMsgQueue) > 0)
  {
    DBG_LOG_DEBUG("The message is available.");

    struct msg_buf_t *msg_buf =
      l_queue_pop_head(_reply_ack->replyMsgQueue);

    if((readBuf != NULL) &&
       (maxReadBufLen >= msg_buf->replyMsgBufLen) &&
       (readBufLen != NULL))
    {
      memcpy(readBuf, msg_buf->replyMsgBuf,
             msg_buf->replyMsgBufLen);
      *readBufLen = msg_buf->replyMsgBufLen;
      l_free(msg_buf->replyMsgBuf);
      msg_buf->replyMsgBuf = NULL;
      msg_buf->replyMsgBufLen = 0;
      l_free(msg_buf);
      msg_buf = NULL;
      pthread_mutex_unlock(&(_reply_ack->replyMsgQueueMtx));
      return _rt;
    }
    else
    {
      DBG_LOG_DEBUG("The space is unavailable.");
    }
  }
  else
  {
    DBG_LOG_DEBUG("There seems no message.");
  }
  pthread_mutex_unlock(&(_reply_ack->replyMsgQueueMtx));

  // If nothing in the queue, then setup the timeout timer and wait
  cond_var->is_op_done = false;
  clock_gettime(CLOCK_MONOTONIC, &cond_var->tv);
  cond_var->tv.tv_sec = cond_var->tv.tv_sec + timeout_s;

  pthread_mutex_lock(&cond_var->mtx);
  while(false == cond_var->is_op_done)
  {
    if(pthread_cond_timedwait(&cond_var->cond_var, &cond_var->mtx,
                              &cond_var->tv) == ETIMEDOUT)
    {
      DBG_LOG_ERR("Wait for reply timeout");

      _rt = false;
      break;
    }
  }
  if(cond_var->is_op_done == true)
  {
    DBG_LOG_INFO(
      "app_froto_n_send_with_multiple_replies_second_half is successful");
    pthread_mutex_lock(&(_reply_ack->replyMsgQueueMtx));
    if(l_queue_length(_reply_ack->replyMsgQueue) > 0)
    {
      DBG_LOG_DEBUG("The message is available.");

      struct msg_buf_t *msg_buf =
        l_queue_pop_head(_reply_ack->replyMsgQueue);

      if((readBuf != NULL) &&
         (maxReadBufLen >= msg_buf->replyMsgBufLen) &&
         (readBufLen != NULL))
      {
        memcpy(readBuf, msg_buf->replyMsgBuf,
               msg_buf->replyMsgBufLen);
        *readBufLen = msg_buf->replyMsgBufLen;
        l_free(msg_buf->replyMsgBuf);
        msg_buf->replyMsgBuf = NULL;
        msg_buf->replyMsgBufLen = 0;
        l_free(msg_buf);
        msg_buf = NULL;
      }
      else
      {
        DBG_LOG_DEBUG("The space is unavailable.");
      }
    }
    else
    {
      DBG_LOG_DEBUG("There seems no message.");
    }
    pthread_mutex_unlock(&(_reply_ack->replyMsgQueueMtx));
  }
  pthread_mutex_unlock(&cond_var->mtx);

  if((lastOne == true) || (cond_var->is_op_done == false))
  {
    unregisterReplyAck(cond_var,
                       pt_sensorid_str,
                       pt_sensorid_str_len,
                       expPrimitive,
                       expMsgType, checkSeqNo,
                       expSeqNo,
                       seqNo,
                       _errCode);
  }

EXIT:
  if((lastOne == true) || (cond_var->is_op_done == false))
  {
    app_com_deinit_pthread_cond_var_ctr(cond_var);
  }
  return _rt;
}
/**
 * @brief Simple send a message.
 * @param encodedBuf Pointer to the encoded message.
 * @param encodedBufLen The length (in byte) of the encoded message.
 * @param userData This can be used to pass some parameters used by send
 * routine.
 * @param send An implementation of sending the encoded message pointed by
 * @p encodedBuf
 * @param errCode Returned error code if the routine return false. Null is
 * allowed.
 * @return true if everything is okay.
 */
bool
app_froto_n_simple_send(
  uint8_t *encodedBuf,
  uint32_t encodedBufLen,
  void *userData,
  froto_send_msg_t send,
  uint8_t *errCode)
{
  uint8_t _errCode;
  app_state_t _ret = ST_OK;
  bool _rt = true;

  if((encodedBuf == NULL) || (encodedBufLen == 0))
  {
    // There seems nothing to send
    return false;
  }
  if(send == NULL)
  {
    // No sending method is provided
    return false;
  }
  if(false == send(userData, encodedBuf, encodedBufLen, &_errCode))
  {
    _rt = false;
    DBG_LOG_ERR("Failed to send");
    goto EXIT;
  }

EXIT:
  return _rt;
}
bool
app_froto_n_get_message_type_server(
  uint8_t *readBuf,
  uint32_t readBufLen,
  froto_after_parse_t data_upload_processor,
  froto_after_parse_t config_hash_upload_processor,
  froto_after_parse_t specific_config_upload_processor,
  froto_after_parse_t current_version_upload_processor,
  froto_after_parse_t fuota_status_processor,
  froto_after_parse_t image_block_request_processor,
  froto_after_parse_t config_dissem_processor,
  froto_after_parse_t file_notify_dissem_processor,
  froto_after_parse_t image_block_dissem_processor,
  froto_after_parse_t config_retrieve_processor,
  froto_after_parse_t image_block_retrieve_processor,
  uint8_t *errCode)
{
  bool rt = true;
  SKFChina_App_AppMessage _SKF_Froto_App = { 0 };
  SKFChina_Froto_FrotoHeader *_Froto_header;
  uint8_t _peer_id[MAX_ID_STR_LENGTH];
  uint32_t _acked_seq_no[FROTO_ACKED_SEQ_NUMBER_MAX_NUMBER];
  uint32_t _froto_app_root_cnt = 0;
  uint32_t _froto_msg_start_cnt = 0;
  uint32_t _froto_msg_end_cnt = 0;
  uint8_t *_tmp_buf = NULL;
  pb_istream_t _istream = pb_istream_from_buffer(readBuf, readBufLen);
  pb_istream_t _substream;
  pb_istream_t _headerstream;

  pb_wire_type_t _wire_type;
  uint32_t _tag;
  bool _eof;
  pb_field_iter_t _iter;
  pb_field_iter_t _header_iter;

  while(pb_decode_tag(&_istream, &_wire_type, &_tag, &_eof))
  {
    DBG_LOG_INFO("_tag: %d", _tag);

    if(_tag == SKFChina_App_AppMessage_appVer_tag)
    {
      // Version
      _froto_app_root_cnt++;
      if(true == pb_decode_varint32(&_istream, &_SKF_Froto_App.appVer))
      {
        DBG_LOG_DEBUG("Froto APP version: %d", _SKF_Froto_App.appVer);
      }
    }
    else
    {
      if(_wire_type == PB_WT_STRING)
      {
        // The oneof _message
        // SKFChina_App_AppMessage_data_upload_tag
        // SKFChina_App_AppMessage_data_selection_tag
        // SKFChina_App_AppMessage_config_retrieve_tag
        // SKFChina_App_AppMessage_config_hash_upload_tag
        // SKFChina_App_AppMessage_specific_config_upload_tag
        // SKFChina_App_AppMessage_config_dissem_tag
        // SKFChina_App_AppMessage_command_dissem_tag
        // SKFChina_App_AppMessage_version_retrieve_tag
        // SKFChina_App_AppMessage_current_version_upload_tag
        // SKFChina_App_AppMessage_last_fuota_upload_tag
        // SKFChina_App_AppMessage_fuota_notify_dissem_tag
        // SKFChina_App_AppMessage_fuota_status_tag
        // SKFChina_App_AppMessage_image_block_dissem_tag
        // SKFChina_App_AppMessage_image_block_request_tag
        // SKFChina_App_AppMessage_ack_tag
        // SKFChina_App_AppMessage_debug_message_tag
        // SKFChina_App_AppMessage_file_notify_dissem_tag
        // SKFChina_App_AppMessage_file_notify_upload_tag
        // SKFChina_App_AppMessage_image_block_upload_tag
        // SKFChina_App_AppMessage_image_block_retrieve_tag

        pb_wire_type_t _sub_wire_type;
        uint32_t _sub_tag;
        uint32_t _expected_sub_tag;
        bool _sub_eof;
        protobuf_codec_general_bytes_t _sensor_id;
        protobuf_codec_acked_seq_number_t _acked_seq_no_array;

        switch(_tag)
        {
          case SKFChina_App_AppMessage_data_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.data_upload.header);
            _expected_sub_tag =
              SKFChina_SensingDataUpload_DataUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_data_selection_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.data_selection.header);
            _expected_sub_tag =
              SKFChina_SensingDataUpload_DataSelectionDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_config_retrieve_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.config_retrieve.header);
            _expected_sub_tag =
              SKFChina_ConfigurationAndCommand_ConfigRetrieve_header_tag;
            break;
          case SKFChina_App_AppMessage_config_hash_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.config_hash_upload.header);
            _expected_sub_tag =
              SKFChina_ConfigurationAndCommand_ConfigHashUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_specific_config_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.specific_config_upload.
                header);
            _expected_sub_tag =
              SKFChina_ConfigurationAndCommand_SpecificConfigUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_config_dissem_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.config_dissem.header);
            _expected_sub_tag =
              SKFChina_ConfigurationAndCommand_ConfigDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_command_dissem_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.command_dissem.header);
            _expected_sub_tag =
              SKFChina_ConfigurationAndCommand_CommandDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_version_retrieve_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.version_retrieve.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_VersionRetrieve_header_tag;
            break;
          case SKFChina_App_AppMessage_current_version_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.current_version_upload.
                header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_CurrentVersionUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_last_fuota_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.last_fuota_upload.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_LastFUOTAUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_fuota_notify_dissem_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.fuota_notify_dissem.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_fuota_status_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.fuota_status.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_UpdateStatusUpload_header_tag;
            break;
          case SKFChina_App_AppMessage_image_block_dissem_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.image_block_dissem.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_image_block_request_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.image_block_request.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_header_tag;
            break;
          case SKFChina_App_AppMessage_ack_tag:
            _Froto_header = &(_SKF_Froto_App._messages.ack.header);
            _expected_sub_tag =
              SKFChina_Froto_FrotoAckMsg_header_tag;
            break;
          case SKFChina_App_AppMessage_debug_message_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.debug_message.header);
            _expected_sub_tag =
              SKFChina_Debug_DebugMessage_header_tag;
            break;
          case SKFChina_App_AppMessage_file_notify_dissem_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.file_notify_dissem.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_FileNotifiyDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_file_notify_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.file_notify_upload.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_FUOTANotifiyDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_image_block_upload_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.image_block_upload.header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_ImageBlockDisseminate_header_tag;
            break;
          case SKFChina_App_AppMessage_image_block_retrieve_tag:
            _Froto_header =
              &(_SKF_Froto_App._messages.image_block_retrieve.
                header);
            _expected_sub_tag =
              SKFChina_FirmwareUpdateOverTheAir_ImageBlockRequest_header_tag;
            break;
          default:
            if(false == pb_skip_field(&_istream, _wire_type))
            {
              DBG_LOG_WARN(
                "Should not reach here: unknown oneof _message, just skip it");
              return false;
            }
            break;
        }

        pb_make_string_substream(&_istream, &_substream);
        while(pb_decode_tag(&_substream, &_sub_wire_type, &_sub_tag,
                            &_sub_eof))
        {
          if(_sub_wire_type == PB_WT_STRING)
          {
            if(_expected_sub_tag == _sub_tag)
            {
              DBG_LOG_DEBUG("Header is found");

              if(pb_field_iter_begin(&_iter,
                                     SKFChina_App_AppMessage_fields,
                                     NULL) &&
                 pb_field_iter_find(&_iter, _tag))
              {
                DBG_LOG_INFO("Iter (%d) has been found!", _tag);
                if(pb_field_iter_begin(&_header_iter, _iter.submsg_desc,
                                       NULL) &&
                   pb_field_iter_find(&_header_iter, _sub_tag))
                {
                  DBG_LOG_INFO("Iter (%d) has been found!", _sub_tag);

                  pb_make_string_substream(&_substream, &_headerstream);

                  _sensor_id.buffer = _peer_id;
                  _sensor_id.size = sizeof(_peer_id);
                  _sensor_id.length = 0;
                  _Froto_header->sensor_id.arg = &_sensor_id;
                  _Froto_header->sensor_id.funcs.decode =
                    &protobuf_dec_bytes_callback;

                  _acked_seq_no_array.buffer = _acked_seq_no;
                  _acked_seq_no_array.size = sizeof(_acked_seq_no) /
                                             sizeof(_acked_seq_no[0]);
                  _acked_seq_no_array.cnt = 0;
                  _Froto_header->acked_message_seq_number.arg =
                    &_acked_seq_no_array;
                  _Froto_header->acked_message_seq_number.funcs.decode =
                    &protobuf_dec_acked_seq_number_callback;

                  if(true == pb_decode(&_headerstream,
                                       _header_iter.submsg_desc,
                                       _Froto_header))
                  {
                    DBG_LOG_DEBUG("Get Froto header done!");
                  }
                  else
                  {
                    DBG_LOG_WARN("Failed to get Froto header, %s",
                                 _istream.errmsg);
                    return false;
                  }
                  pb_close_string_substream(&_substream, &_headerstream);
                }
              }
              break;
            }
          }
        }
        pb_close_string_substream(&_istream, &_substream);

        // A completed Froto message has been read
        _froto_msg_end_cnt = readBufLen - _istream.bytes_left - 1;
        _tmp_buf = l_malloc(_froto_msg_end_cnt - _froto_msg_start_cnt + 1);
        if(_tmp_buf == NULL)
        {
          DBG_LOG_ERR("Failed to malloc");
          return false;
        }
        memcpy(_tmp_buf, &readBuf[_froto_msg_start_cnt],
               _froto_msg_end_cnt - _froto_msg_start_cnt + 1);

        DBG_LOG_DEBUG("Froto message length: %d bytes",
                      _froto_msg_end_cnt - _froto_msg_start_cnt + 1);
        DBG_LOG_INFO("_Froto_header->message_seq_no: %d",
                      _Froto_header->message_seq_no);
        DBG_LOG_DEBUG("_Froto_header->primitive_type: %d",
                      _Froto_header->primitive_type);
        DBG_LOG_INFO("_Froto_header->message_type: %d",
                      _Froto_header->message_type);
        DBG_LOG_DEBUG("_Froto_header->acked_message_seq_number: ");
        for(uint32_t i = 0;
            i < ((protobuf_codec_acked_seq_number_t *)(_Froto_header->
                                                       acked_message_seq_number
                                                       .arg))->cnt;
            i++)
        {
          DBG_LOG_DEBUG("%d",
                        ((protobuf_codec_acked_seq_number_t *)(
                           _Froto_header->acked_message_seq_number.arg))->
                        buffer[i]);
        }
        DBG_LOG_DEBUG("_Froto_header->sensor_id: ");
        util_dbg_buf_dump(
          ((protobuf_codec_general_bytes_t *)(_Froto_header->sensor_id.arg))
          ->buffer,
          ((protobuf_codec_general_bytes_t *)(_Froto_header->sensor_id.arg))
          ->length);

        app_froto_n_parser(_tmp_buf,
                           _froto_msg_end_cnt - _froto_msg_start_cnt + 1,
                           data_upload_processor,
                           config_hash_upload_processor,
                           specific_config_upload_processor,
                           current_version_upload_processor,
                           fuota_status_processor,
                           image_block_request_processor,
                           config_dissem_processor,
                           file_notify_dissem_processor,
                           image_block_dissem_processor,
                           config_retrieve_processor,
                           image_block_retrieve_processor,
                           NULL);
        l_free(_tmp_buf);
        _tmp_buf = NULL;

        _froto_msg_start_cnt = readBufLen - _istream.bytes_left;

        memset((void *)&_SKF_Froto_App, 0, sizeof(_SKF_Froto_App));
      }
      else
      {
        DBG_LOG_DEBUG("Unknown field. Just skip it ...");
        if(false == pb_skip_field(&_istream, _wire_type))
        {
          break;
        }
      }
    }

    DBG_LOG_DEBUG("bytes_left: %d", _istream.bytes_left);
  }
}
