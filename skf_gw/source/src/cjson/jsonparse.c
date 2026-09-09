#include "jsonParse.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void parseSenCfg(
  cJSON *item,
  sensorConfig_t *sensorConfig);
static void parseGwCfg(
  cJSON *item,
  gwConfig_t *gwConfig);

/**
 * @brief Parse a json string and fill to gwConfig (Thread
 * safe)
 * @param inputJson: The input json string pointer
 * @param gwConfig: The parsed output
 * @param ret: successful or not (can be NULL).
 */
void
parseGwConfigJson(
  char *inputJson,
  gwConfig_t *gwConfig,
  int8_t *ret)
{
  cJSON *_root = NULL;
  int8_t _ret = 0;

  if(inputJson == NULL)
  {
    _ret = -1;
    goto exit;
  }
  if(gwConfig == NULL)
  {
    _ret = -1;
    goto exit;
  }

  _root = cJSON_Parse(inputJson);
  if(_root == NULL)
  {
    // the json string seems invalid
    _ret = -2;
    goto exit;
  }
  parseGwCfg(_root, gwConfig);

  cJSON_Delete(_root);

exit:
  if(ret != NULL)
  {
    *ret = _ret;
  }
}
/**
 * @brief Implementation of parsing a json string and fill to gwConfig
 *(Thread safe)
 * @param inputJson: The pointer to the cJSON root
 * @param sensorConfig: The parsed output
 */
static void
parseGwCfg(
  cJSON *item,
  gwConfig_t *gwConfig)
{
  cJSON *_tmp = NULL;
  cJSON *_tmpSub = NULL;
  cJSON *_tmpSubSub = NULL;
  cJSON *_tmpSubSubSub = NULL;
  uint16_t _childrenSize = 0;
  uint16_t _i = 0;
  unsigned int _tmpMacAddr[6];  // To store mac address by using sscanf
  int _tmpIpAddr[4];  // To store mac address by using sscanf

  _tmp = cJSON_GetObjectItem(item, GWCONFIGJSON_NAME_VERSION);
  if(_tmp != NULL)
  {
    gwConfig->version = _tmp->valueint;
  }
  _tmp = cJSON_GetObjectItem(item, GWCONFIGJSON_NAME_LASTEDITTIME);
  if(_tmp != NULL)
  {
    gwConfig->lastEditTimeS = _tmp->valueint;
  }
  _tmp = cJSON_GetObjectItem(item, GWCONFIGJSON_NAME_GWCONFIG);
  if(_tmp != NULL)
  {
    _tmpSub = cJSON_GetObjectItem(_tmp, GWCONFIGJSON_NAME_GWCONFIG_TYPE);
    if(_tmpSub != NULL)
    {
      strncpy(gwConfig->gwConfig.type, _tmpSub->valuestring,
              sizeof(gwConfig->gwConfig.type));
    }
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  GWCONFIGJSON_NAME_GWCONFIG_MANUFACTURER);
    if(_tmpSub != NULL)
    {
      strncpy(gwConfig->gwConfig.manufacturer, _tmpSub->valuestring,
              sizeof(gwConfig->gwConfig.manufacturer));
    }
    _tmpSub = cJSON_GetObjectItem(_tmp, GWCONFIGJSON_NAME_GWCONFIG_NAME);
    if(_tmpSub != NULL)
    {
      strncpy(gwConfig->gwConfig.name, _tmpSub->valuestring,
              sizeof(gwConfig->gwConfig.name));
    }
    _tmpSub = cJSON_GetObjectItem(_tmp, GWCONFIGJSON_NAME_GWCONFIG_NAMEID);
    if(_tmpSub != NULL)
    {
      gwConfig->gwConfig.nameId = _tmpSub->valueint;
    }
    // TODO: to check the MAC address' endianness
    _tmpSub =
      cJSON_GetObjectItem(_tmp, GWCONFIGJSON_NAME_GWCONFIG_MACADDR);
    if(_tmpSub != NULL)
    {
      sscanf(_tmpSub->valuestring, "%02x-%02x-%02x-%02x-%02x-%02x",
             &_tmpMacAddr[0], &_tmpMacAddr[1], &_tmpMacAddr[2],
             &_tmpMacAddr[3], &_tmpMacAddr[4], &_tmpMacAddr[5]);
      gwConfig->gwConfig.macAddr.addr.addrArray[0] = _tmpMacAddr[0] & 0xff;
      gwConfig->gwConfig.macAddr.addr.addrArray[1] = _tmpMacAddr[1] & 0xff;
      gwConfig->gwConfig.macAddr.addr.addrArray[2] = _tmpMacAddr[2] & 0xff;
      gwConfig->gwConfig.macAddr.addr.addrArray[3] = _tmpMacAddr[3] & 0xff;
      gwConfig->gwConfig.macAddr.addr.addrArray[4] = _tmpMacAddr[4] & 0xff;
      gwConfig->gwConfig.macAddr.addr.addrArray[5] = _tmpMacAddr[5] & 0xff;
    }
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  GWCONFIGJSON_NAME_GWCONFIG_GATEWAYMODE);
    if(_tmpSub != NULL)
    {
      gwConfig->gwConfig.gatewayMode = _tmpSub->valueint;
    }
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG);
    if(_tmpSub != NULL)
    {
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG_BLE1TXPOWER);
      if(_tmpSubSub != NULL)
      {
        gwConfig->gwConfig.bleConfig.ble1TxPower = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG_BLE2TXPOWER);
      if(_tmpSubSub != NULL)
      {
        gwConfig->gwConfig.bleConfig.ble2TxPower = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG_BLE1ANTENNA);
      if(_tmpSubSub != NULL)
      {
        gwConfig->gwConfig.bleConfig.ble1Antenna = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG_BLE2ANTENNA);
      if(_tmpSubSub != NULL)
      {
        gwConfig->gwConfig.bleConfig.ble2Antenna = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG_BLEDUPLICATEDDROP);
      if(_tmpSubSub != NULL)
      {
        gwConfig->gwConfig.bleConfig.bleDuplicatedDrop_ms =
          _tmpSubSub->valueint;
      }
    }
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  GWCONFIGJSON_NAME_GWCONFIG_TIMECONFIG);
    if(_tmpSub != NULL)
    {
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_TIMECONFIG_TIMESTAMP);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          gwConfig->gwConfig.timeConfig.needTimestampForRecBeacon = true;
        }
        else
        {
          gwConfig->gwConfig.timeConfig.needTimestampForRecBeacon = false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_TIMECONFIG_TIMESETTING);
      if(_tmpSubSub != NULL)
      {
        gwConfig->gwConfig.timeConfig.timeSetting = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_TIMECONFIG_TIMENTPURL);
      if(_tmpSubSub != NULL)
      {
        strncpy(gwConfig->gwConfig.timeConfig.timeNtpUrl,
                _tmpSubSub->valuestring,
                sizeof(gwConfig->gwConfig.timeConfig.timeNtpUrl));
      }
    }
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  GWCONFIGJSON_NAME_GWCONFIG_MQTTCONFIG);
    if(_tmpSub != NULL)
    {
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_MQTTCONFIG_MQTTSECURITYTYPE);
      if(_tmpSubSub != NULL)
      {
        gwConfig->gwConfig.mqttConfig.mqttSecurityType =
          _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_MQTTCONFIG_MQTTHOST);
      if(_tmpSubSub != NULL)
      {
        strncpy(gwConfig->gwConfig.mqttConfig.MQTTHost,
                _tmpSubSub->valuestring,
                sizeof(gwConfig->gwConfig.mqttConfig.MQTTHost));
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_MQTTCONFIG_MQTTPORT);
      if(_tmpSubSub != NULL)
      {
        gwConfig->gwConfig.mqttConfig.MQTTPort = _tmpSubSub->valueint;
      }
    }
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG);
    if(_tmpSub != NULL)
    {
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG_DHCPENABLE);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          gwConfig->gwConfig.ipConfig.DHCPEnable = true;
        }
        else
        {
          gwConfig->gwConfig.ipConfig.DHCPEnable = false;
        }
      }
      // TODO: to check the IP address' endianness
      _tmpSubSub =
        cJSON_GetObjectItem(_tmpSub,
                            GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG_STATICIPADDR);
      if(_tmpSubSub != NULL)
      {
        sscanf(_tmpSubSub->valuestring, "%d.%d.%d.%d", &_tmpIpAddr[0],
               &_tmpIpAddr[1], &_tmpIpAddr[2], &_tmpIpAddr[3]);
        gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[0] =
          _tmpIpAddr[0] & 0xff;
        gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[1] =
          _tmpIpAddr[1] & 0xff;
        gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[2] =
          _tmpIpAddr[2] & 0xff;
        gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[3] =
          _tmpIpAddr[3] & 0xff;
      }
      // TODO: to check the IP address' endianness
      _tmpSubSub =
        cJSON_GetObjectItem(_tmpSub,
                            GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG_NETMASK);
      if(_tmpSubSub != NULL)
      {
        sscanf(_tmpSubSub->valuestring, "%d.%d.%d.%d", &_tmpIpAddr[0],
               &_tmpIpAddr[1], &_tmpIpAddr[2], &_tmpIpAddr[3]);
        gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[0] =
          _tmpIpAddr[0] & 0xff;
        gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[1] =
          _tmpIpAddr[1] & 0xff;
        gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[2] =
          _tmpIpAddr[2] & 0xff;
        gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[3] =
          _tmpIpAddr[3] & 0xff;
      }
      // TODO: to check the IP address' endianness
      _tmpSubSub =
        cJSON_GetObjectItem(_tmpSub,
                            GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG_DNSSERVER);
      if(_tmpSubSub != NULL)
      {
        sscanf(_tmpSubSub->valuestring, "%d.%d.%d.%d", &_tmpIpAddr[0],
               &_tmpIpAddr[1], &_tmpIpAddr[2], &_tmpIpAddr[3]);
        gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[0] =
          _tmpIpAddr[0] & 0xff;
        gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[1] =
          _tmpIpAddr[1] & 0xff;
        gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[2] =
          _tmpIpAddr[2] & 0xff;
        gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[3] =
          _tmpIpAddr[3] & 0xff;
      }
      // TODO: to check the IP address' endianness
      _tmpSubSub =
        cJSON_GetObjectItem(_tmpSub,
                            GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG_GATEWAYADDR);
      if(_tmpSubSub != NULL)
      {
        sscanf(_tmpSubSub->valuestring, "%d.%d.%d.%d", &_tmpIpAddr[0],
               &_tmpIpAddr[1], &_tmpIpAddr[2], &_tmpIpAddr[3]);
        gwConfig->gwConfig.ipConfig.gatewayAddr.addr.addrArray[0] =
          _tmpIpAddr[0] & 0xff;
        gwConfig->gwConfig.ipConfig.gatewayAddr.addr.addrArray[1] =
          _tmpIpAddr[1] & 0xff;
        gwConfig->gwConfig.ipConfig.gatewayAddr.addr.addrArray[2] =
          _tmpIpAddr[2] & 0xff;
        gwConfig->gwConfig.ipConfig.gatewayAddr.addr.addrArray[3] =
          _tmpIpAddr[3] & 0xff;
      }
    }

	#if(1 == ENABLE_MODBUS_FEATURE)
		// to get the Modbus Config Info
		_tmpSub = cJSON_GetObjectItem(_tmp, GWCONFIGJSON_NAME_GWCONFIG_MODBUSCONFIG);
		if(NULL != _tmpSub)
		{
			//uint32_t baudrateSlave;  // baudrate of slave(gateway)
			_tmpSubSub = cJSON_GetObjectItem(_tmpSub, GWCONFIGJSON_NAME_GWCONFIG_MBUS_BAUDRATE);
			if(NULL != _tmpSubSub)
			{
				gwConfig->gwConfig.modbusConfig.baudrateSlave = _tmpSubSub->valueint;
			}
			//uint32_t slaveAddrSlave;  // salve address
			_tmpSubSub = cJSON_GetObjectItem(_tmpSub, GWCONFIGJSON_NAME_GWCONFIG_MBUS_SLAVEADDR);
			if(NULL != _tmpSubSub)
			{
				gwConfig->gwConfig.modbusConfig.slaveAddrSlave = _tmpSubSub->valueint;
			}
  			//uint8_t paritySlave;  // parity of serial port
  			_tmpSubSub = cJSON_GetObjectItem(_tmpSub, GWCONFIGJSON_NAME_GWCONFIG_MBUS_PARITY);
			if(NULL != _tmpSubSub)
			{
				gwConfig->gwConfig.modbusConfig.paritySlave = _tmpSubSub->valueint;
			}
  			//uint8_t stopSlave;  // stop bit of serial port
  			_tmpSubSub = cJSON_GetObjectItem(_tmpSub, GWCONFIGJSON_NAME_GWCONFIG_MBUS_STOP);
			if(NULL != _tmpSubSub)
			{
				gwConfig->gwConfig.modbusConfig.stopSlave = _tmpSubSub->valueint;
			}
  			//uint8_t registerMode;  // mode of registers in gateway, extensible or efficient
  			_tmpSubSub = cJSON_GetObjectItem(_tmpSub, GWCONFIGJSON_NAME_GWCONFIG_MBUS_REGMODE);
			if(NULL != _tmpSubSub)
			{
				gwConfig->gwConfig.modbusConfig.registerMode = _tmpSubSub->valueint;
			}
		}
		// to get the description info
		_tmpSub = cJSON_GetObjectItem(_tmp, GWCONFIGJSON_NAME_GWCONFIG_DESCRIPTION);
		if(NULL != _tmpSub)
		{
			snprintf( gwConfig->gwConfig.description,sizeof(gwConfig->gwConfig.description),"%s", _tmpSub->valuestring);
		}
	#endif
	
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  GWCONFIGJSON_NAME_GWCONFIG_CHILDRENNUMBER);
    if(_tmpSub != NULL)
    {
      gwConfig->gwConfig.childrenNumber = _tmpSub->valueint;
    }
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  GWCONFIGJSON_NAME_GWCONFIG_CHILDREN);
    if(_tmpSub != NULL)
    {
      _childrenSize = cJSON_GetArraySize(_tmpSub);
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
      gwConfig_gwConfig_children_t *_childrenListNodeTmp = NULL;
      gwConfig_gwConfig_children_t *_currentChildrenListNode = NULL;
      if(list_empty(&(gwConfig->gwConfig.childrenList)))
      {
        printf("parseGwCfg\n");

        // If the list is empty, then initialize the link directly.
        INIT_LIST_HEAD(&(gwConfig->gwConfig.childrenList));
      }
      else
      {
        printf("parseGwCfg123\n");

        // Otherwise, delete the list.
        list_for_each_entry_safe(_currentChildrenListNode,
                                 _childrenListNodeTmp,
                                 &gwConfig->gwConfig.childrenList,
                                 childrenNode)
        {
          list_del(&_currentChildrenListNode->childrenNode);
          free(_currentChildrenListNode);
        }
        // Then, initialize the list.
        INIT_LIST_HEAD(&(gwConfig->gwConfig.childrenList));
      }
#endif
      for(_i = 0; _i < _childrenSize; _i++)
      {
        _tmpSubSub = cJSON_GetArrayItem(_tmpSub, _i);
        if(_tmpSubSub != NULL)
        {
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
          gwConfig_gwConfig_children_t *_childrenListNode = NULL;
          _childrenListNode = malloc(sizeof(gwConfig_gwConfig_children_t));
          if(_childrenListNode == NULL)
          {
            // Failed to make a node
            return;
          }
          _tmpSubSubSub = cJSON_GetObjectItem(_tmpSubSub,
                                              GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_TYPE);
          if(_tmpSubSubSub != NULL)
          {
            strncpy(_childrenListNode->type,
                    _tmpSubSubSub->valuestring,
                    sizeof(_childrenListNode->type));
          }
          _tmpSubSubSub = cJSON_GetObjectItem(_tmpSubSub,
                                              GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_MANUFACTURER);
          if(_tmpSubSubSub != NULL)
          {
            strncpy(_childrenListNode->manufacturer,
                    _tmpSubSubSub->valuestring,
                    sizeof(_childrenListNode->manufacturer));
          }
          _tmpSubSubSub = cJSON_GetObjectItem(_tmpSubSub,
                                              GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_NAME);
          if(_tmpSubSubSub != NULL)
          {
            strncpy(_childrenListNode->name,
                    _tmpSubSubSub->valuestring,
                    sizeof(_childrenListNode->name));
          }
          _tmpSubSubSub = cJSON_GetObjectItem(_tmpSubSub,
                                              GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_NAMEID);
          if(_tmpSubSubSub != NULL)
          {
            _childrenListNode->nameId = _tmpSubSubSub->valueint;
          }
          _tmpSubSubSub = cJSON_GetObjectItem(_tmpSubSub,
                                              GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_BLESENSORNAME);
          if(_tmpSubSubSub != NULL)
          {
            strncpy(_childrenListNode->bleName,
                    _tmpSubSubSub->valuestring,
                    sizeof(_childrenListNode->bleName));
          }

		  #if(1 == ENABLE_MODBUS_FEATURE)
		  	_tmpSubSubSub= cJSON_GetObjectItem( _tmpSubSub, GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_DESC);
		  	if(NULL != _tmpSubSubSub)
		  	{
		  		strncpy( _childrenListNode->description, _tmpSubSubSub->valuestring, sizeof(_childrenListNode->description));
		  	} 
		  #endif
		  
          // TODO: to check the MAC address' endianness
          _tmpSubSubSub =
            cJSON_GetObjectItem(_tmpSubSub,
                                GWCONFIGJSON_NAME_GWCONFIG_MACADDR);
          if(_tmpSubSubSub != NULL)
          {
            sscanf(_tmpSubSubSub->valuestring,
                   "%02x-%02x-%02x-%02x-%02x-%02x",
                   &_tmpMacAddr[0], &_tmpMacAddr[1], &_tmpMacAddr[2],
                   &_tmpMacAddr[3], &_tmpMacAddr[4], &_tmpMacAddr[5]);
            _childrenListNode->macAddr.addr.addrArray[0] =
              _tmpMacAddr[0] & 0xff;
            _childrenListNode->macAddr.addr.addrArray[1] =
              _tmpMacAddr[1] & 0xff;
            _childrenListNode->macAddr.addr.addrArray[2] =
              _tmpMacAddr[2] & 0xff;
            _childrenListNode->macAddr.addr.addrArray[3] =
              _tmpMacAddr[3] & 0xff;
            _childrenListNode->macAddr.addr.addrArray[4] =
              _tmpMacAddr[4] & 0xff;
            _childrenListNode->macAddr.addr.addrArray[5] =
              _tmpMacAddr[5] & 0xff;
          }
          list_add_tail(&(_childrenListNode->childrenNode),
                        &(gwConfig->gwConfig.childrenList));
#else
          _tmpSubSubSub = cJSON_GetObjectItem(_tmpSubSub,
                                              GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_TYPE);
          if(_tmpSubSubSub != NULL)
          {
            strncpy(gwConfig->gwConfig.children[_i].type,
                    _tmpSubSubSub->valuestring,
                    sizeof(gwConfig->gwConfig.children[_i].type));
          }
          _tmpSubSubSub = cJSON_GetObjectItem(_tmpSubSub,
                                              GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_MANUFACTURER);
          if(_tmpSubSubSub != NULL)
          {
            strncpy(gwConfig->gwConfig.children[_i].manufacturer,
                    _tmpSubSubSub->valuestring,
                    sizeof(gwConfig->gwConfig.children[_i].manufacturer));
          }
          _tmpSubSubSub = cJSON_GetObjectItem(_tmpSubSub,
                                              GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_NAME);
          if(_tmpSubSubSub != NULL)
          {
            strncpy(gwConfig->gwConfig.children[_i].name,
                    _tmpSubSubSub->valuestring,
                    sizeof(gwConfig->gwConfig.children[_i].name));
          }
          _tmpSubSubSub = cJSON_GetObjectItem(_tmpSubSub,
                                              GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_NAMEID);
          if(_tmpSubSubSub != NULL)
          {
            gwConfig->gwConfig.children[_i].nameId =
              _tmpSubSubSub->valueint;
          }
          _tmpSubSubSub = cJSON_GetObjectItem(_tmpSubSub,
                                              GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_BLESENSORNAME);
          if(_tmpSubSubSub != NULL)
          {
            strncpy(gwConfig->gwConfig.children[_i].bleName,
                    _tmpSubSubSub->valuestring,
                    sizeof(gwConfig->gwConfig.children[_i].bleName));
          }
		
		  	#if(1 == ENABLE_MODBUS_FEATURE)
				_tmpSubSubSub = cJSON_GetObjectItem( _tmpSubSub, GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_DESC);
				if(NULL != _tmpSubSubSub)
				{
					strncpy( gwConfig->gwConfig.children[_i].description, _tmpSubSubSub->valuestring, sizeof(gwConfig->gwConfig.children[_i].description));
				} 
			#endif
		  
          // TODO: to check the MAC address' endianness
          _tmpSubSubSub =
            cJSON_GetObjectItem(_tmpSubSub,
                                GWCONFIGJSON_NAME_GWCONFIG_MACADDR);
          if(_tmpSubSubSub != NULL)
          {
            sscanf(_tmpSubSubSub->valuestring,
                   "%02x-%02x-%02x-%02x-%02x-%02x",
                   &_tmpMacAddr[0], &_tmpMacAddr[1], &_tmpMacAddr[2],
                   &_tmpMacAddr[3], &_tmpMacAddr[4], &_tmpMacAddr[5]);
            gwConfig->gwConfig.children[_i].macAddr.addr.addrArray[0] =
              _tmpMacAddr[0] & 0xff;
            gwConfig->gwConfig.children[_i].macAddr.addr.addrArray[1] =
              _tmpMacAddr[1] & 0xff;
            gwConfig->gwConfig.children[_i].macAddr.addr.addrArray[2] =
              _tmpMacAddr[2] & 0xff;
            gwConfig->gwConfig.children[_i].macAddr.addr.addrArray[3] =
              _tmpMacAddr[3] & 0xff;
            gwConfig->gwConfig.children[_i].macAddr.addr.addrArray[4] =
              _tmpMacAddr[4] & 0xff;
            gwConfig->gwConfig.children[_i].macAddr.addr.addrArray[5] =
              _tmpMacAddr[5] & 0xff;
          }
#endif
        }
      }
    }
  }
}
/**
 * @brief Parse a json string and fill to sensorConfig (Thread
 * safe)
 * @param inputJson: The input json string pointer
 * @param sensorConfig: The parsed output
 * @param ret: successful or not (can be NULL).
 */
void
parseSensorConfigJson(
  char *inputJson,
  sensorConfig_t *sensorConfig,
  int8_t *ret)
{
  cJSON *_root = NULL;
  int8_t _ret = 0;

  if(inputJson == NULL)
  {
    _ret = -1;
    goto exit;
  }
  if(sensorConfig == NULL)
  {
    _ret = -1;
    goto exit;
  }

  _root = cJSON_Parse(inputJson);
  if(_root == NULL)
  {
    // the json string seems invalid
    _ret = -2;
    goto exit;
  }
  parseSenCfg(_root, sensorConfig);

  cJSON_Delete(_root);

exit:
  if(ret != NULL)
  {
    *ret = _ret;
  }
}
/**
 * @brief Implementation of parsing a json string and fill to sensorConfig
 *(Thread safe)
 * @param inputJson: The pointer to the cJSON root
 * @param sensorConfig: The parsed output
 */
static void
parseSenCfg(
  cJSON *item,
  sensorConfig_t *sensorConfig)
{
  cJSON *_tmp = NULL;
  cJSON *_tmpSub = NULL;
  cJSON *_tmpSubSub = NULL;
  unsigned int _tmpMacAddr[6];  // To store mac address by using sscanf

  _tmp = cJSON_GetObjectItem(item, SENSORCONFIGJSON_NAME_VERSION);
  if(_tmp != NULL)
  {
    sensorConfig->version = _tmp->valueint;
  }
  _tmp = cJSON_GetObjectItem(item, SENSORCONFIGJSON_NAME_LASTEDITTIME);
  if(_tmp != NULL)
  {
    sensorConfig->lastEditTimeS = _tmp->valueint;
  }
  _tmp = cJSON_GetObjectItem(item,
                             SENSORCONFIGJSON_NAME_HAVECONFIGUREDTOSENSOR);
  if(_tmp != NULL)
  {
    if(_tmp->valueint)
    {
      sensorConfig->haveConfiguredToSensor = true;
    }
    else
    {
      sensorConfig->haveConfiguredToSensor = false;
    }
  }
  _tmp = cJSON_GetObjectItem(item,
                             SENSORCONFIGJSON_NAME_WHENCONFIGUREDSENSOR);
  if(_tmp != NULL)
  {
    sensorConfig->whenConfiguredToSensor = _tmp->valueint;
  }
  _tmp = cJSON_GetObjectItem(item, SENSORCONFIGJSON_NAME_TYPE);
  if(_tmp != NULL)
  {
    strncpy(sensorConfig->type, _tmp->valuestring,
            sizeof(sensorConfig->type));
  }
  _tmp = cJSON_GetObjectItem(item, SENSORCONFIGJSON_NAME_MANUFACTURER);
  if(_tmp != NULL)
  {
    strncpy(sensorConfig->manufacturer, _tmp->valuestring,
            sizeof(sensorConfig->manufacturer));
  }
  _tmp = cJSON_GetObjectItem(item, SENSORCONFIGJSON_NAME_NAMEID);
  if(_tmp != NULL)
  {
    sensorConfig->nameId = _tmp->valueint;
  }
  // TODO: to check the MAC address' endianness
  _tmp = cJSON_GetObjectItem(item, SENSORCONFIGJSON_NAME_MACADDR);
  if(_tmp != NULL)
  {
    sscanf(_tmp->valuestring, "%02x-%02x-%02x-%02x-%02x-%02x",
           &_tmpMacAddr[0], &_tmpMacAddr[1],
           &_tmpMacAddr[2], &_tmpMacAddr[3],
           &_tmpMacAddr[4], &_tmpMacAddr[5]);
    sensorConfig->macAddr.addr.addrArray[0] = _tmpMacAddr[0] & 0xff;
    sensorConfig->macAddr.addr.addrArray[1] = _tmpMacAddr[1] & 0xff;
    sensorConfig->macAddr.addr.addrArray[2] = _tmpMacAddr[2] & 0xff;
    sensorConfig->macAddr.addr.addrArray[3] = _tmpMacAddr[3] & 0xff;
    sensorConfig->macAddr.addr.addrArray[4] = _tmpMacAddr[4] & 0xff;
    sensorConfig->macAddr.addr.addrArray[5] = _tmpMacAddr[5] & 0xff;
  }
  _tmp =
    cJSON_GetObjectItem(item, SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG);
  if(_tmp != NULL)
  {
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG);
    if(_tmpSub != NULL)
    {
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_SENSORMODE);
      if(_tmpSubSub != NULL)
      {
        if(strcmp(_tmpSubSub->valuestring,
                  BULLET_SENSOR_MODE_FLIGHT_NAME) == 0)
        {
          sensorConfig->bulletSensorConfig.sysConfig.sensorMode =
            BULLET_SENSOR_MODE_FLIGHT;
        }
        else if(strcmp(_tmpSubSub->valuestring,
                       BULLET_SENSOR_MODE_STANDBY_NAME) == 0)
        {
          sensorConfig->bulletSensorConfig.sysConfig.sensorMode =
            BULLET_SENSOR_MODE_STANDBY;
        }
        else if(strcmp(_tmpSubSub->valuestring,
                       BULLET_SENSOR_MODE_NORMAL_NAME) == 0)
        {
          sensorConfig->bulletSensorConfig.sysConfig.sensorMode =
            BULLET_SENSOR_MODE_NORMAL;
        }
        else if(strcmp(_tmpSubSub->valuestring,
                       BULLET_SENSOR_MODE_FCT_NAME) == 0)
        {
          sensorConfig->bulletSensorConfig.sysConfig.sensorMode =
            BULLET_SENSOR_MODE_FCT;
        }
      }
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_SENSORMODEPARAM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.sysConfig.sensorModeParameter
          = _tmpSubSub->valueint;
      }
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_BATTERYALARMTHRE);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.sysConfig.batteryAlarmThrePercent
          = _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_CURRENTTIME);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.sysConfig.currentTimeS
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_TXPOWERADV);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.sysConfig.txPowerAdv
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_DATARATEAUXADV);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.sysConfig.dataRateAuxAdv
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_TXPOWERAUXADV);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.sysConfig.txPowerAuxAdv
          = _tmpSubSub->valueint;
      }
    }
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG);
    if(_tmpSub != NULL)
    {
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_PERIODQUICKPOLL);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.schConfig.period_quickPolling_s
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_REFTIMEQUICKPOLL);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.schConfig.refTime_quickPolling_s
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_PERIODREGULARSEN);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_REFTIMEREGULARSEN);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.schConfig.refTime_regularSensing_s
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_PERIODCOMM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.schConfig.period_comm_s
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_REFTIMEPERIODCOMM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.schConfig.refTime_period_comm_s
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_PERIODWAVEDATAACQ);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.schConfig.period_wave_s
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_REFTIMEWAVEDATAACQ);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s
          = _tmpSubSub->valueint;
      }
    }
    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG);
    if(_tmpSub != NULL)
    {
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQVIBFSHZ);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQVIBN);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_n
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQVIBAXISACQEVAL);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.
        pre_acq_vib_axis_acq_eval
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQVIBRANGE);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_range
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQMAGFSHZ);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQMAGN);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.pre_acq_mag_n
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQMAGAXISACQEVAL);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.
        pre_acq_mag_axis_acq_eval
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_FACC);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.fAccHz
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_NACC);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.nAcc
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_ACQVIBAXISACQEVAL);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_ACQVIBRANGE);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.acq_vib_range
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_ACQMAGFSHZ);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.acq_mag_fs_hz
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_ACQMAGN);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.acq_mag_n
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_ACQMAGAXISACQEVAL);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_FSCOEF);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.senConfig.fs_coef
          = _tmpSubSub->valuedouble;
      }
    }

    _tmpSub = cJSON_GetObjectItem(_tmp,
                                  SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG);
    if(_tmpSub != NULL)
    {
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_GEECOEF);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.GEE_COEF
          = _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VCOEF);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.V_COEF
          = _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MEASPOSITION);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.MEAS_POSITION
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MEASLOAD);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.MEAS_LOAD
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MEASAXISVIB);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MEASAXISMAG);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VIBSTARTFG);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.VIB_START_FG = true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.VIB_START_FG = false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VIBRMSSTARTTL);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.VIB_RMS_START_TL
          = _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MAGSTARTFG);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.MAG_START_FG = true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.MAG_START_FG = false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MAGRMSSTARTTL);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.MAG_RMS_START_TL
          = _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MAGSTABLEFG);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.MAG_STABLE_FG = true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.MAG_STABLE_FG =
            false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MAGRMSVARTH);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH
          = _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_RPMVARRANGE);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.RPM_VAR_RANGE
          = _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICTEMPM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICTEMPN);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICLEARNNUMTEMP);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.
        DEC_LOGIC_LEARN_NUM_TEMP = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICVIBM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICVIBN);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICLEARNNUMVIB);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICLEARNNUMMAG);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG
          = _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMTEMP);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP =
            true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP =
            false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMOV);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_OV =
            true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_OV =
            false;
        }
      }

      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMMECH);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH =
            true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH =
            false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMBRG);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG =
            true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG =
            false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMLUB);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB =
            true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB =
            false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMMTR);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR =
            true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR =
            false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMGEAR);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR =
            true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR =
            false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMFAN);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN =
            true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN =
            false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMPUMP);
      if(_tmpSubSub != NULL)
      {
        if(_tmpSubSub->valueint)
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP =
            true;
        }
        else
        {
          sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP =
            false;
        }
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ASSETLEVEL);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.ASSET_LEVEL =
          _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FLEXTYPE);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.FLEX_TYPE =
          _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_BOREDIAMETERMM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_RUNSPEEDRPM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.RUN_SPEED_RPM =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_BRGINFOBPFO);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_BPFO =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_BRGINFOBPFI);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_BPFI =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_BRGINFOBSF);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_BSF =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_BRGINFOFTF);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_FTF =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MTRINFOFL);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.MTR_INFO_FL =
          _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MTRINFOBAR);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.MTR_INFO_BAR =
          _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_GEARINFOTOOTH);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH =
          _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FANINFOBLADE);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.FAN_INFO_BLADE =
          _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_PUMPINFOVANE);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.PUMP_INFO_VANE =
          _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_TEMPOVALERTCDEGREE);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ALARMTHRESHTEMP);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.AlarmThrestemp =
          _tmpSubSub->valueint;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ACCOVALERT);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.ACC_OV_ALERT =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ACCOVALARM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.ACC_OV_ALARM =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ACCHALALERT);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.ACC_HAL_ALERT =
          _tmpSubSub->valuedouble;
      }

      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ACCHALALARM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.ACC_HAL_ALARM =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VELOVALERT);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.VEL_OV_ALERT =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VELOVALARM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.VEL_OV_ALARM =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VELHALALERT);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.VEL_HAL_ALERT =
          _tmpSubSub->valuedouble;
      }

      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VELHALALARM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.VEL_HAL_ALARM =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ENVOVALERT);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.ENV_OV_ALERT =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ENVOVALARM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.ENV_OV_ALARM =
          _tmpSubSub->valuedouble;
      }
      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ENVHALALERT);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.ENV_HAL_ALERT =
          _tmpSubSub->valuedouble;
      }

      _tmpSubSub = cJSON_GetObjectItem(_tmpSub,
                                       SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ENVHALALARM);
      if(_tmpSubSub != NULL)
      {
        sensorConfig->bulletSensorConfig.algoConfig.ENV_HAL_ALARM =
          _tmpSubSub->valuedouble;
      }
    }
  }
}
