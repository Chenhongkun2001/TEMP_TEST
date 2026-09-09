/**
 * @file    app_getConfigure.c
 * @author  Xiaoyuan (Sean) Ma (zz2134)
 * @date    2024-05-31
 * @brief   Implementations for APIs to get GW configurations
 * @details
 */

#include "app_getConfigure.h"
#include "app_cjson.h"
#include "cjson/gwConfig.h"
#include "cjson/list.h"
#include "sys_def.h"
#include "util_dbg.h"
#define APP_GETCONFIGURE_GW_CONF_PATH "gwConfig.json"

DBG_LOCAL_LOG_DEBUG


bool
getChildMACList(
  struct bt_addr *list,
  uint32_t list_sz,
  uint32_t *childNumber)
{
  gwConfig_t kp_gw_cfg;
  bool rt = true;

  if(list == NULL)
  {
    DBG_LOG_ERR("list is NULL");
    return false;
  }

  DBG_LOG_DEBUG("get child list");

  INIT_LIST_HEAD(&(kp_gw_cfg.gwConfig.childrenList));
  // Then get gwConfig
  rt = app_cjson_jsonstr_to_gwconfig(&kp_gw_cfg,
                                     APP_GETCONFIGURE_GW_CONF_PATH);
  if(rt != true)
  {
    DBG_LOG_WARN("rt_state = %d", rt);
    rt = false;
    goto fun_exit;
  }

  DBG_LOG_DEBUG("Set the child list");
  // Then set the child list
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  gwConfig_gwConfig_children_t *_childrenListNodeTmp = NULL;
  gwConfig_gwConfig_children_t *_currentChildrenListNode = NULL;
  uint16_t _cnt = 0;
  list_for_each_entry_safe(_currentChildrenListNode, _childrenListNodeTmp,
                           &(kp_gw_cfg.gwConfig.childrenList),
                           childrenNode)
  {
#else
  for(_cnt = 0;
      _cnt <
      (kp_gw_cfg.gwConfig.childrenNumber <
       list_sz ? kp_gw_cfg.gwConfig.childrenNumber : list_sz);
      _cnt++)
  {
#endif
    if(_cnt >= list_sz)
    {
      break;
    }
    if(_cnt >= kp_gw_cfg.gwConfig.childrenNumber)
    {
      break;
    }

#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
    list[_cnt].nap =
      (_currentChildrenListNode->macAddr.addr.addrArray[0] << 8) +
      _currentChildrenListNode->macAddr.addr.addrArray[1];
    list[_cnt].uap = _currentChildrenListNode->macAddr.addr.addrArray[2];
    list[_cnt].lap =
      (_currentChildrenListNode->macAddr.addr.addrArray[3] << 16) +
      (_currentChildrenListNode->macAddr.addr.addrArray[4]
        << 8) +
      _currentChildrenListNode->macAddr.addr.addrArray[5];
    // TODO: to check the MAC address' endianness
    DBG_LOG_DEBUG("%02x-%02x-%02x-%02x-%02x-%02x",
                  _currentChildrenListNode->macAddr.addr.addrArray[0],
                  _currentChildrenListNode->macAddr.addr.addrArray[1],
                  _currentChildrenListNode->macAddr.addr.addrArray[2],
                  _currentChildrenListNode->macAddr.addr.addrArray[3],
                  _currentChildrenListNode->macAddr.addr.addrArray[4],
                  _currentChildrenListNode->macAddr.addr.addrArray[5]);
    _cnt++;
#else
    list[_cnt].nap =
      (kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[0] << 8) +
      kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[1];
    list[_cnt].uap =
      kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[2];
    list[_cnt].lap =
      (kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[3] <<
    16) +
      (kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[4]
        << 8) +
      kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[5];
    // TODO: to check the MAC address' endianness
    DBG_LOG_DEBUG("%02x-%02x-%02x-%02x-%02x-%02x",
                  kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[
                    0],
                  kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[
                    1],
                  kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[
                    2],
                  kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[
                    3],
                  kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[
                    4],
                  kp_gw_cfg->gwConfig.children[_cnt].macAddr.addr.addrArray[
                    5]);
#endif
  }
  DBG_LOG_INFO("childNumber: %d", _cnt);

  if(childNumber != NULL)
  {
    *childNumber = _cnt;
  }

fun_exit:
  return rt;
}
