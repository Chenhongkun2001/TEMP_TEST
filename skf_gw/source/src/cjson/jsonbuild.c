#include "jsonBuild.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DBG_LOCAL_LOG_DEBUG


/**
 * @brief Generate a json string according to the given gwConfig (Thread
 * safe)
 * @param gwConfig: The input structure
 * @param outputJson: The output string pointer
 * @param ret: successful or not (can be NULL).
 */
void
generateGwConfigJson(
  gwConfig_t *gwConfig,
  char *outputJson,
  int8_t *ret)
{
  // The size of mac address (represented as a string and seperated by
  // dash) is fixed, e.g., "c4-bd-6a-01-02-03" (do not forget the "\0" at
  // the end)
  char _macAddrStr[18];
  // The max size of ip address (represented as a string and seperated by
  // dot) is 16 (i.e., "255.255.255.255", do not forget the "\0" at the
  // end)
  char _ipAddrStr[16];
  char _numChar[30];  // To store the number in char to guarantee the
                      // accuracy
  double _tmpDouble;
  uint16_t _cnt = 0;
  cJSON *_root = NULL;
  cJSON *_gwcfg = NULL;
  cJSON *_blecfg = NULL;
  cJSON *_timecfg = NULL;
  cJSON *_mqttcfg = NULL;
  cJSON *_ipcfg = NULL;
  cJSON *_child = NULL;

  #if(1 == ENABLE_MODBUS_FEATURE)
  	cJSON *_mbus_cfg = NULL;
  #endif

#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
#else
  uint16_t _childrenNumber = 0;
#endif
  cJSON *_temp = NULL;
  char *_tempjson = NULL;
  int8_t _ret = 0;

  if(gwConfig == NULL)
  {
    _ret = -1;
    goto exit;
  }
  if(outputJson == NULL)
  {
    _ret = -1;
    goto exit;
  }

  _root = cJSON_CreateObject();
  _gwcfg = cJSON_CreateObject();
  _blecfg = cJSON_CreateObject();
  _timecfg = cJSON_CreateObject();
  _mqttcfg = cJSON_CreateObject();
  _ipcfg = cJSON_CreateObject();

  #if(1 == ENABLE_MODBUS_FEATURE)
  	_mbus_cfg = cJSON_CreateObject();
  #endif
  	
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
#else
  _childrenNumber = gwConfig->gwConfig.childrenNumber;
#endif
  _child = cJSON_CreateArray();

  /***********************************************************/
  /************************ 1. Root **************************/
  /***********************************************************/
  // version
  snprintf(_numChar, sizeof(_numChar), "%d", gwConfig->version);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_root, GWCONFIGJSON_NAME_VERSION,
                          _tmpDouble);
  // lastedittime
  snprintf(_numChar, sizeof(_numChar), "%ld", gwConfig->lastEditTimeS);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_root, GWCONFIGJSON_NAME_LASTEDITTIME,
                          _tmpDouble);
  /***********************************************************/
  /******************* 2. Root->gwConfig *********************/
  /***********************************************************/
  // gwConfig->type
  cJSON_AddStringToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_TYPE,
                          gwConfig->gwConfig.type);
  // gwConfig->manufacturer
  cJSON_AddStringToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_MANUFACTURER,
                          gwConfig->gwConfig.manufacturer);
  // gwConfig->name
  cJSON_AddStringToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_NAME,
                          gwConfig->gwConfig.name);
  // gwConfig->nameId
  snprintf(_numChar, sizeof(_numChar), "%d", gwConfig->gwConfig.nameId);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_NAMEID,
                          _tmpDouble);
  // gwConfig->macAddr
  // TODO: to check the MAC address' endianness
  snprintf(_macAddrStr, sizeof(_macAddrStr),
           "%02x-%02x-%02x-%02x-%02x-%02x",
           gwConfig->gwConfig.macAddr.addr.addrArray[0],
           gwConfig->gwConfig.macAddr.addr.addrArray[1],
           gwConfig->gwConfig.macAddr.addr.addrArray[2],
           gwConfig->gwConfig.macAddr.addr.addrArray[3],
           gwConfig->gwConfig.macAddr.addr.addrArray[4],
           gwConfig->gwConfig.macAddr.addr.addrArray[5]);
  cJSON_AddStringToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_MACADDR,
                          _macAddrStr);

  // gwConfig->gatewayMode
  snprintf(_numChar, sizeof(_numChar), "%d",
           gwConfig->gwConfig.gatewayMode);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_GATEWAYMODE,
                          _tmpDouble);

  /***********************************************************/
  /************* 3. Root->gwConfig->BLEConfig ****************/
  /***********************************************************/
  // gwConfig->BLEConfig->BLE1TxPower
  snprintf(_numChar, sizeof(_numChar), "%d",
           gwConfig->gwConfig.bleConfig.ble1TxPower);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_blecfg,
                          GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG_BLE1TXPOWER,
                          _tmpDouble);
  // gwConfig->BLEConfig->BLE2TxPower
  snprintf(_numChar, sizeof(_numChar), "%d",
           gwConfig->gwConfig.bleConfig.ble2TxPower);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_blecfg,
                          GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG_BLE2TXPOWER,
                          _tmpDouble);
  // gwConfig->BLEConfig->BLE1Antenna
  snprintf(_numChar, sizeof(_numChar), "%d",
           gwConfig->gwConfig.bleConfig.ble1Antenna);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_blecfg,
                          GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG_BLE1ANTENNA,
                          _tmpDouble);
  // gwConfig->BLEConfig->BLE2Antenna
  snprintf(_numChar, sizeof(_numChar), "%d",
           gwConfig->gwConfig.bleConfig.ble2Antenna);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_blecfg,
                          GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG_BLE2ANTENNA,
                          _tmpDouble);
  // gwConfig->BLEConfig->BLEDuplicatedDrop
  snprintf(_numChar, sizeof(_numChar), "%d",
           gwConfig->gwConfig.bleConfig.bleDuplicatedDrop_ms);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_blecfg,
                          GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG_BLEDUPLICATEDDROP,
                          _tmpDouble);
  cJSON_AddItemToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_BLECONFIG,
                        _blecfg);

  /***********************************************************/
  /************* 4. Root->gwConfig->timeConfig ***************/
  /***********************************************************/
  // gwConfig->timeConfig->timeStamp
  cJSON_AddBoolToObject(_timecfg,
                        GWCONFIGJSON_NAME_GWCONFIG_TIMECONFIG_TIMESTAMP,
                        (cJSON_bool)gwConfig->gwConfig.timeConfig.needTimestampForRecBeacon);
  // gwConfig->timeConfig->timeSetting
  snprintf(_numChar, sizeof(_numChar), "%d",
           gwConfig->gwConfig.timeConfig.timeSetting);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_timecfg,
                          GWCONFIGJSON_NAME_GWCONFIG_TIMECONFIG_TIMESETTING,
                          _tmpDouble);
  // gwConfig->timeConfig->timeNTPUrl
  cJSON_AddStringToObject(_timecfg,
                          GWCONFIGJSON_NAME_GWCONFIG_TIMECONFIG_TIMENTPURL,
                          gwConfig->gwConfig.timeConfig.timeNtpUrl);
  cJSON_AddItemToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_TIMECONFIG,
                        _timecfg);

  /***********************************************************/
  /************* 5. Root->gwConfig->MQTTConfig ***************/
  /***********************************************************/
  // gwConfig->mqttConfig->MQTTSecurityType
  snprintf(_numChar, sizeof(_numChar), "%d",
           gwConfig->gwConfig.mqttConfig.mqttSecurityType);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_mqttcfg,
                          GWCONFIGJSON_NAME_GWCONFIG_MQTTCONFIG_MQTTSECURITYTYPE,
                          _tmpDouble);
  // gwConfig->mqttConfig->MQTTHost
  cJSON_AddStringToObject(_mqttcfg,
                          GWCONFIGJSON_NAME_GWCONFIG_MQTTCONFIG_MQTTHOST,
                          gwConfig->gwConfig.mqttConfig.MQTTHost);
  // gwConfig->mqttConfig->MQTTPort
  snprintf(_numChar, sizeof(_numChar), "%d",
           gwConfig->gwConfig.mqttConfig.MQTTPort);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_mqttcfg,
                          GWCONFIGJSON_NAME_GWCONFIG_MQTTCONFIG_MQTTPORT,
                          _tmpDouble);
  cJSON_AddItemToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_MQTTCONFIG,
                        _mqttcfg);

  /***********************************************************/
  /************* 6. Root->gwConfig->ipConfig *****************/
  /***********************************************************/
  // gwConfig->ipConfig->DHCPEnable
  cJSON_AddBoolToObject(_ipcfg,
                        GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG_DHCPENABLE,
                        (cJSON_bool)gwConfig->gwConfig.ipConfig.DHCPEnable);
  // gwConfig->ipConfig->staticIPAddr
  // TODO: to check the IP address' endianness
  snprintf(_ipAddrStr, sizeof(_ipAddrStr), "%d.%d.%d.%d",
           gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[0],
           gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[1],
           gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[2],
           gwConfig->gwConfig.ipConfig.staticIPAddr.addr.addrArray[3]);
  cJSON_AddStringToObject(_ipcfg,
                          GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG_STATICIPADDR,
                          _ipAddrStr);
  // gwConfig->ipConfig->netMask
  // TODO: to check the IP address' endianness
  snprintf(_ipAddrStr, sizeof(_ipAddrStr), "%d.%d.%d.%d",
           gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[0],
           gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[1],
           gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[2],
           gwConfig->gwConfig.ipConfig.netMask.addr.addrArray[3]);
  cJSON_AddStringToObject(_ipcfg,
                          GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG_NETMASK,
                          _ipAddrStr);
  // gwConfig->ipConfig->DNSServer
  // TODO: to check the IP address' endianness
  snprintf(_ipAddrStr, sizeof(_ipAddrStr), "%d.%d.%d.%d",
           gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[0],
           gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[1],
           gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[2],
           gwConfig->gwConfig.ipConfig.dnsServer.addr.addrArray[3]);
  cJSON_AddStringToObject(_ipcfg,
                          GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG_DNSSERVER,
                          _ipAddrStr);
  // gwConfig->ipConfig->gatewayAddr
  // TODO: to check the IP address' endianness
  snprintf(_ipAddrStr, sizeof(_ipAddrStr), "%d.%d.%d.%d",
           gwConfig->gwConfig.ipConfig.gatewayAddr.addr.addrArray[0],
           gwConfig->gwConfig.ipConfig.gatewayAddr.addr.addrArray[1],
           gwConfig->gwConfig.ipConfig.gatewayAddr.addr.addrArray[2],
           gwConfig->gwConfig.ipConfig.gatewayAddr.addr.addrArray[3]);
  cJSON_AddStringToObject(_ipcfg,
                          GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG_GATEWAYADDR,
                          _ipAddrStr);
  cJSON_AddItemToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_IPCONFIG,
                        _ipcfg);


	#if(1 == ENABLE_MODBUS_FEATURE)
	/*********************************************************
	******************Modbus Conf Info************************
	**********************************************************/
	//printf("\nBKP280\n");
	//baudrate
	snprintf( _numChar, sizeof(_numChar),"%d", gwConfig->gwConfig.modbusConfig.baudrateSlave);
	_tmpDouble = atof(_numChar);
	cJSON_AddNumberToObject( _mbus_cfg, GWCONFIGJSON_NAME_GWCONFIG_MBUS_BAUDRATE, _tmpDouble);
	//uint32_t slaveAddrSlave;  // salve address
	snprintf( _numChar, sizeof(_numChar),"%d", gwConfig->gwConfig.modbusConfig.slaveAddrSlave);
	_tmpDouble = atof(_numChar);
	cJSON_AddNumberToObject(_mbus_cfg, GWCONFIGJSON_NAME_GWCONFIG_MBUS_SLAVEADDR, _tmpDouble);
 	//uint8_t paritySlave;  // parity of serial port
	snprintf(_numChar, sizeof(_numChar), "%d", gwConfig->gwConfig.modbusConfig.paritySlave);
	_tmpDouble = atof(_numChar);
	cJSON_AddNumberToObject(_mbus_cfg, GWCONFIGJSON_NAME_GWCONFIG_MBUS_PARITY, _tmpDouble);
	//uint8_t stopSlave;  // stop bit of serial port
	snprintf( _numChar, sizeof(_numChar),"%d", gwConfig->gwConfig.modbusConfig.stopSlave);
	_tmpDouble = atof(_numChar);
	cJSON_AddNumberToObject( _mbus_cfg, GWCONFIGJSON_NAME_GWCONFIG_MBUS_STOP, _tmpDouble);
  	//uint8_t registerMode;  // mode of registers in gateway, extensible or efficient
	snprintf(_numChar, sizeof(_numChar),"%d", gwConfig->gwConfig.modbusConfig.registerMode);
	_tmpDouble = atof(_numChar);
	cJSON_AddNumberToObject(_mbus_cfg, GWCONFIGJSON_NAME_GWCONFIG_MBUS_REGMODE, _tmpDouble);
	// add Item to Object
	cJSON_AddItemToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_MODBUSCONFIG, _mbus_cfg);	
	//printf("\nBKP303\n");
	/*****************************************************************************************
	*******************************description************************************************
	******************************************************************************************/
	cJSON_AddStringToObject( _gwcfg, GWCONFIGJSON_NAME_GWCONFIG_DESCRIPTION, gwConfig->gwConfig.description);
  	//printf("\nBKP308\n");		
	#endif
	
	
  //printf("\n %d BKP308\n", __LINE__);	   
  /***********************************************************/
  /************* 7. Root->gwConfig->Chirldren ****************/
  /***********************************************************/
  // Create the first child
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
  gwConfig_gwConfig_children_t *_childrenListNodeTmp = NULL;
  gwConfig_gwConfig_children_t *_currentChildrenListNode = NULL;
  _cnt = 0;
  list_for_each_entry_safe(_currentChildrenListNode, _childrenListNodeTmp,
                           &(gwConfig->gwConfig.childrenList),
                           childrenNode)
  {
  	
 	 //printf("\n %d BKP308\n", __LINE__); 	  
#else
  for(_cnt = 0; _cnt < _childrenNumber; _cnt++)
  {
#endif
    cJSON_AddItemToArray(_child, cJSON_CreateObject());
    if(_cnt == 0)
    {
      // For the first element of the linked list
      _temp = _child->child;
    }
    else
    {
      // For the non-first element of the linked list
      _temp = _temp->next;
    }
#if (GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
    cJSON_AddStringToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_TYPE,
                            _currentChildrenListNode->type);
    cJSON_AddStringToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_MANUFACTURER,
                            _currentChildrenListNode->manufacturer);
    cJSON_AddStringToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_NAME,
                            _currentChildrenListNode->name);
    snprintf(_numChar, sizeof(_numChar), "%d",
             _currentChildrenListNode->nameId);
    _tmpDouble = atof(_numChar);
    cJSON_AddNumberToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_NAMEID,
                            _tmpDouble);
    cJSON_AddStringToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_BLESENSORNAME,
                            _currentChildrenListNode->bleName);
    // gwConfig->gwConfig->children(element)->addrArray
    // TODO: to check the MAC address' endianness
    snprintf(_macAddrStr, sizeof(_macAddrStr),
             "%02x-%02x-%02x-%02x-%02x-%02x",
             _currentChildrenListNode->macAddr.addr.addrArray[0],
             _currentChildrenListNode->macAddr.addr.addrArray[1],
             _currentChildrenListNode->macAddr.addr.addrArray[2],
             _currentChildrenListNode->macAddr.addr.addrArray[3],
             _currentChildrenListNode->macAddr.addr.addrArray[4],
             _currentChildrenListNode->macAddr.addr.addrArray[5]);
    cJSON_AddStringToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_MACADDR,
                            _macAddrStr);
	
	#if(1 == ENABLE_MODBUS_FEATURE)
		cJSON_AddStringToObject(_temp, GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_DESC, _currentChildrenListNode->description);
	#endif
	
    _cnt++;
#else
    cJSON_AddStringToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_TYPE,
                            gwConfig->gwConfig.children[_cnt].type);
    cJSON_AddStringToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_MANUFACTURER,
                            gwConfig->gwConfig.children[_cnt].manufacturer);
    cJSON_AddStringToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_NAME,
                            gwConfig->gwConfig.children[_cnt].name);
    snprintf(_numChar, sizeof(_numChar), "%d",
             gwConfig->gwConfig.children[_cnt].nameId);
    _tmpDouble = atof(_numChar);
    cJSON_AddNumberToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_NAMEID,
                            _tmpDouble);
    cJSON_AddStringToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_BLESENSORNAME,
                            gwConfig->gwConfig.children[_cnt].bleName);
    // gwConfig->gwConfig->children(element)->addrArray
    // TODO: to check the MAC address' endianness
    snprintf(_macAddrStr, sizeof(_macAddrStr),
             "%02x-%02x-%02x-%02x-%02x-%02x",
             gwConfig->gwConfig.children[_cnt].macAddr.addr.addrArray[0],
             gwConfig->gwConfig.children[_cnt].macAddr.addr.addrArray[1],
             gwConfig->gwConfig.children[_cnt].macAddr.addr.addrArray[2],
             gwConfig->gwConfig.children[_cnt].macAddr.addr.addrArray[3],
             gwConfig->gwConfig.children[_cnt].macAddr.addr.addrArray[4],
             gwConfig->gwConfig.children[_cnt].macAddr.addr.addrArray[5]);
    cJSON_AddStringToObject(_temp,
                            GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_MACADDR,
                            _macAddrStr);
	// description
	#if(1 == ENABLE_MODBUS_FEATURE)
		cJSON_AddStringToObject( _temp, GWCONFIGJSON_NAME_GWCONFIG_CHILDREN_ELEMENT_DESC, gwConfig->gwConfig.children[_cnt].description);
	#endif
#endif
  }

	
  //printf("\n %d BKP308\n", __LINE__); 	  
  
  snprintf(_numChar, sizeof(_numChar), "%d",
           gwConfig->gwConfig.childrenNumber);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_gwcfg,
                          GWCONFIGJSON_NAME_GWCONFIG_CHILDRENNUMBER,
                          _tmpDouble);
  if(gwConfig->gwConfig.childrenNumber > 0)
  {
    cJSON_AddItemToObject(_gwcfg, GWCONFIGJSON_NAME_GWCONFIG_CHILDREN,
                          _child);
  }

  // After construct gwConfig, then add it to the root finnally
  cJSON_AddItemToObject(_root, GWCONFIGJSON_NAME_GWCONFIG, _gwcfg);

  _tempjson = cJSON_Print(_root);
  cJSON_Minify(_tempjson);
  sprintf(outputJson, "%s", _tempjson);
  free(_tempjson);
  _tempjson = NULL;

  // Delete all the created nodes recursively
  cJSON_Delete(_root);  
	
	printf("\n %d BKP308\n", __LINE__); 	 
exit:
  if(ret != NULL)
  {
    *ret = _ret;
  }
}
/**
 * @brief Generate a json string according to the given sensorConfig
 *(Thread safe)
 * @param sensorConfig: The input structure
 * @param outputJson: The output string pointer
 * @param ret: successful or not (can be NULL).
 */
void
generateSensorConfigJson(
  sensorConfig_t *sensorConfig,
  char *outputJson,
  int8_t *ret)
{
  // The size of mac address (represented as a string and seperated by
  // dash) is fixed, e.g., "c4-bd-6a-01-02-03" (do not forget the "\0"
  // at the end)
  char _macAddrStr[18];
  char _temp[50];  // To store the work mode of sensor
  char _numChar[30];  // To store the number in char to guarantee the
                      // accuracy
  double _tmpDouble;
  cJSON *_root = NULL;
  cJSON *_bulletsensorcfg = NULL;
  cJSON *_syscfg = NULL;
  cJSON *_schedulecfg = NULL;
  cJSON *_sensingcfg = NULL;
  cJSON *_algocfg = NULL;
  char *_tempjson = NULL;
  int8_t _ret = 0;

  if(sensorConfig == NULL)
  {
    _ret = -1;
    goto exit;
  }
  if(outputJson == NULL)
  {
    _ret = -1;
    goto exit;
  }

  _root = cJSON_CreateObject();
  _bulletsensorcfg = cJSON_CreateObject();
  _syscfg = cJSON_CreateObject();
  _schedulecfg = cJSON_CreateObject();
  _sensingcfg = cJSON_CreateObject();
  _algocfg = cJSON_CreateObject();

  /***********************************************************/
  /************************** 1. Root ************************/
  /***********************************************************/
  // version
  snprintf(_numChar, sizeof(_numChar), "%d", sensorConfig->version);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_root, SENSORCONFIGJSON_NAME_VERSION,
                          _tmpDouble);
  // lastEditTime
  snprintf(_numChar, sizeof(_numChar), "%ld", sensorConfig->lastEditTimeS);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_root, SENSORCONFIGJSON_NAME_LASTEDITTIME,
                          _tmpDouble);
  // haveConfiguredToSensor
  cJSON_AddBoolToObject(_root,
                        SENSORCONFIGJSON_NAME_HAVECONFIGUREDTOSENSOR,
                        (cJSON_bool)sensorConfig->haveConfiguredToSensor);
  if(sensorConfig->haveConfiguredToSensor == true)
  {
    // whenConfiguredToSensor
    cJSON_AddNumberToObject(_root,
                            SENSORCONFIGJSON_NAME_WHENCONFIGUREDSENSOR,
                            (double)sensorConfig->whenConfiguredToSensor);
  }
  // type
  cJSON_AddStringToObject(_root, SENSORCONFIGJSON_NAME_TYPE,
                          sensorConfig->type);
  // manufacturer
  cJSON_AddStringToObject(_root, SENSORCONFIGJSON_NAME_MANUFACTURER,
                          sensorConfig->manufacturer);
  snprintf(_numChar, sizeof(_numChar), "%d", sensorConfig->nameId);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_root, SENSORCONFIGJSON_NAME_NAMEID,
                          _tmpDouble);
  // macAddr
  // TODO: to check the MAC address' endianness
  snprintf(_macAddrStr, sizeof(_macAddrStr),
           "%02x-%02x-%02x-%02x-%02x-%02x",
           sensorConfig->macAddr.addr.addrArray[0],
           sensorConfig->macAddr.addr.addrArray[1],
           sensorConfig->macAddr.addr.addrArray[2],
           sensorConfig->macAddr.addr.addrArray[3],
           sensorConfig->macAddr.addr.addrArray[4],
           sensorConfig->macAddr.addr.addrArray[5]);
  cJSON_AddStringToObject(_root, SENSORCONFIGJSON_NAME_MACADDR,
                          _macAddrStr);

  /***********************************************************/
  /********** 2. Root->bulletSensorConfig->sysConfig *********/
  /***********************************************************/
  // bulletSensorConfig->sysConfig->sensorMode
  switch(sensorConfig->bulletSensorConfig.sysConfig.sensorMode)
  {
    case BULLET_SENSOR_MODE_FLIGHT:
      strncpy(_temp, BULLET_SENSOR_MODE_FLIGHT_NAME, sizeof(_temp));
      break;
    case BULLET_SENSOR_MODE_STANDBY:
      strncpy(_temp, BULLET_SENSOR_MODE_STANDBY_NAME, sizeof(_temp));
      break;
    case BULLET_SENSOR_MODE_NORMAL:
      strncpy(_temp, BULLET_SENSOR_MODE_NORMAL_NAME, sizeof(_temp));
      break;
    case BULLET_SENSOR_MODE_FCT:
      strncpy(_temp, BULLET_SENSOR_MODE_FCT_NAME, sizeof(_temp));
      break;
    default:
      _temp[0] = '\0';
  }
  cJSON_AddStringToObject(_syscfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_SENSORMODE,
                          _temp);
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
  // bulletSensorConfig->sysConfig->sensorModeParameter
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.sysConfig.sensorModeParameter);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_syscfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_SENSORMODEPARAM,
                          _tmpDouble);
#endif /* STATUS_SENSING_FEATURE_ENABLE == 1 */
  // bulletSensorConfig->sysConfig->batteryAlarmThreshold
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.sysConfig.batteryAlarmThrePercent);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_syscfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_BATTERYALARMTHRE,
                          _tmpDouble);
  // bulletSensorConfig->sysConfig->currentTime
  snprintf(_numChar, sizeof(_numChar), "%ld",
           sensorConfig->bulletSensorConfig.sysConfig.currentTimeS);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_syscfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_CURRENTTIME,
                          _tmpDouble);
  // bulletSensorConfig->sysConfig->txPower_adv
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.sysConfig.txPowerAdv);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_syscfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_TXPOWERADV,
                          _tmpDouble);
  // bulletSensorConfig->sysConfig->dataRate_auxAdv
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.sysConfig.dataRateAuxAdv);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_syscfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_DATARATEAUXADV,
                          _tmpDouble);
  // bulletSensorConfig->sysConfig->txPower_auxAdv
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.sysConfig.txPowerAuxAdv);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_syscfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG_TXPOWERAUXADV,
                          _tmpDouble);
  cJSON_AddItemToObject(_bulletsensorcfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SYSCFG,
                        _syscfg);

  /***********************************************************/
  /******* 3. Root->bulletSensorConfig->scheduleConfig *******/
  /***********************************************************/
  // bulletSensorConfig->scheduleConfig->period_quickPolling
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.schConfig.period_quickPolling_s);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_schedulecfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_PERIODQUICKPOLL,
                          _tmpDouble);
  // bulletSensorConfig->scheduleConfig->referenceTime_quickPolling
  // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
	// but locally use a uint32_t to store
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.schConfig.refTime_quickPolling_s);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_schedulecfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_REFTIMEQUICKPOLL,
                          _tmpDouble);
  // bulletSensorConfig->scheduleConfig->period_regularSensing
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.schConfig.period_regularSensing_s);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_schedulecfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_PERIODREGULARSEN,
                          _tmpDouble);
  // bulletSensorConfig->scheduleConfig->referenceTime_regularSensing
  // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
	// but locally use a uint32_t to store  
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.schConfig.refTime_regularSensing_s);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_schedulecfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_REFTIMEREGULARSEN,
                          _tmpDouble);
  // bulletSensorConfig->scheduleConfig->period_comm
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.schConfig.period_comm_s);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_schedulecfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_PERIODCOMM,
                          _tmpDouble);
  // bulletSensorConfig->scheduleConfig->referenceTime_period_comm
  // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
	// but locally use a uint32_t to store  
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.schConfig.refTime_period_comm_s);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_schedulecfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_REFTIMEPERIODCOMM,
                          _tmpDouble);
  cJSON_AddItemToObject(_bulletsensorcfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG,
                        _schedulecfg);
  // bulletSensorConfig->scheduleConfig->waveDataAcqPeriod
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.schConfig.period_wave_s);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_schedulecfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_PERIODWAVEDATAACQ,
                          _tmpDouble);
  // bulletSensorConfig->scheduleConfig->waveDataAcqReference
  // Note: this is a SENSOR_CONF_CONTENT_TIMEARRAY variable, 
	// but locally use a uint32_t to store  
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.schConfig.refTime_wave_s);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_schedulecfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG_REFTIMEWAVEDATAACQ,
                          _tmpDouble);
  cJSON_AddItemToObject(_bulletsensorcfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SCHCFG,
                        _schedulecfg);

  /***********************************************************/
  /******* 4. Root->bulletSensorConfig->sensingConfig ********/
  /***********************************************************/
  // bulletSensorConfig->sensingConfig->PRE_ACQ_VIB_FS_HZ
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_fs_hz);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQVIBFSHZ,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->PRE_ACQ_VIB_N
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_n);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQVIBN,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->PRE_ACQ_VIB_AXIS_ACQ_EVAL
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_axis_acq_eval);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQVIBAXISACQEVAL,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->PRE_ACQ_VIB_RANGE
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.pre_acq_vib_range);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQVIBRANGE,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->PRE_ACQ_MAG_FS_HZ
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.pre_acq_mag_fs_hz);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQMAGFSHZ,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->PRE_ACQ_MAG_N
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.pre_acq_mag_n);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQMAGN,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->PRE_ACQ_MAG_AXIS_ACQ_EVAL
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.pre_acq_mag_axis_acq_eval);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_PREACQMAGAXISACQEVAL,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->Facc
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.fAccHz);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_FACC,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->Nacc
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.nAcc);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_NACC,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->ACQ_VIB_AXIS_ACQ_EVAL
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.acq_vib_axis_acq_eval);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_ACQVIBAXISACQEVAL,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->ACQ_VIB_RANGE
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.acq_vib_range);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_ACQVIBRANGE,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->ACQ_MAG_FS_HZ
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.acq_mag_fs_hz);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_ACQMAGFSHZ,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->ACQ_MAG_N
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.acq_mag_n);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_ACQMAGN,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->ACQ_MAG_AXIS_ACQ_EVAL
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.senConfig.acq_mag_axis_acq_eval);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_ACQMAGAXISACQEVAL,
                          _tmpDouble);
  // bulletSensorConfig->sensingConfig->FS_COEF
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.senConfig.fs_coef);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_sensingcfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG_FSCOEF,
                          _tmpDouble);
  cJSON_AddItemToObject(_bulletsensorcfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_SENCFG,
                        _sensingcfg);

  /***********************************************************/
  /****** 5. Root->bulletSensorConfig->algorithmConfig *******/
  /***********************************************************/
  // bulletSensorConfig->algorithmConfig->GEE_COEF
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.GEE_COEF);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_GEECOEF,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->V_COEF
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.V_COEF);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VCOEF,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->MEAS_POSITION
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.MEAS_POSITION);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MEASPOSITION,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->MEAS_LOAD
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.MEAS_LOAD);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MEASLOAD,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->MEAS_AXIS_VIB
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.MEAS_AXIS_VIB);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MEASAXISVIB,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->MEAS_AXIS_MAG
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.MEAS_AXIS_MAG);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MEASAXISMAG,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->VIB_START_FG
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VIBSTARTFG,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.VIB_START_FG);
  // bulletSensorConfig->algorithmConfig->VIB_RMS_START_TL
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.VIB_RMS_START_TL);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VIBRMSSTARTTL,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->MAG_START_FG
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MAGSTARTFG,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.MAG_START_FG);
  // bulletSensorConfig->algorithmConfig->MAG_RMS_START_TL
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.MAG_RMS_START_TL);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MAGRMSSTARTTL,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->MAG_STABLE_FG
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MAGSTABLEFG,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.MAG_STABLE_FG);
  // bulletSensorConfig->algorithmConfig->MAG_RMS_VAR_TH
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.MAG_RMS_VAR_TH);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MAGRMSVARTH,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->RPM_VAR_RANGE
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.RPM_VAR_RANGE);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_RPMVARRANGE,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->DEC_LOGIC_TEMP_M
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_M);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICTEMPM,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->DEC_LOGIC_TEMP_N
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_TEMP_N);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICTEMPN,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->DEC_LOGIC_LEARN_NUM_TEMP
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_TEMP);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICLEARNNUMTEMP,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->DEC_LOGIC_VIB_M
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_M);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICVIBM,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->DEC_LOGIC_VIB_N
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_VIB_N);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICVIBN,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->DEC_LOGIC_LEARN_NUM_VIB
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_VIB);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICLEARNNUMVIB,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->DEC_LOGIC_LEARN_NUM_MAG
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.DEC_LOGIC_LEARN_NUM_MAG);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_DECLOGICLEARNNUMMAG,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->FUNC_ANOM_TEMP
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMTEMP,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_TEMP);
  // bulletSensorConfig->algorithmConfig->FUNC_ANOM_OV
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMOV,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_OV);
  // bulletSensorConfig->algorithmConfig->FUNC_ANOM_MECH
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMMECH,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_MECH);
  // bulletSensorConfig->algorithmConfig->FUNC_ANOM_BRG
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMBRG,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_BRG);
  // bulletSensorConfig->algorithmConfig->FUNC_ANOM_LUB
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMLUB,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_LUB);
  // bulletSensorConfig->algorithmConfig->FUNC_ANOM_MTR
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMMTR,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_MTR);
  // bulletSensorConfig->algorithmConfig->FUNC_ANOM_GEAR
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMGEAR,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_GEAR);
  // bulletSensorConfig->algorithmConfig->FUNC_ANOM_FAN
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMFAN,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_FAN);
  // bulletSensorConfig->algorithmConfig->FUNC_ANOM_PUMP
  cJSON_AddBoolToObject(_algocfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FUNCANOMPUMP,
                        (cJSON_bool)sensorConfig->bulletSensorConfig.algoConfig.FUNC_ANOM_PUMP);
  // bulletSensorConfig->algorithmConfig->ASSET_LEVEL
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.ASSET_LEVEL);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ASSETLEVEL,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->FLEX_TYPE
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.FLEX_TYPE);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FLEXTYPE,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->BORE_DIAMETER_MM
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.BORE_DIAMETER_MM);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_BOREDIAMETERMM,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->RUN_SPEED_RPM
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.RUN_SPEED_RPM);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_RUNSPEEDRPM,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->BRG_INFO_BPFO
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_BPFO);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_BRGINFOBPFO,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->BRG_INFO_BPFI
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_BPFI);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_BRGINFOBPFI,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->BRG_INFO_BSF
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_BSF);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_BRGINFOBSF,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->BRG_INFO_FTF
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.BRG_INFO_FTF);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_BRGINFOFTF,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->MTR_INFO_FL
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.MTR_INFO_FL);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MTRINFOFL,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->MTR_INFO_BAR
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.MTR_INFO_BAR);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_MTRINFOBAR,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->GEAR_INFO_TOOTH
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.GEAR_INFO_TOOTH);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_GEARINFOTOOTH,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->FAN_INFO_BLADE
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.FAN_INFO_BLADE);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_FANINFOBLADE,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->PUMP_INFO_VANE
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.PUMP_INFO_VANE);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_PUMPINFOVANE,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->TEMP_OV_ALERT_CDEGREE
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.TEMP_OV_ALERT_CDEGREE);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_TEMPOVALERTCDEGREE,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->AlarmThrestemp
  snprintf(_numChar, sizeof(_numChar), "%d",
           sensorConfig->bulletSensorConfig.algoConfig.AlarmThrestemp);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ALARMTHRESHTEMP,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->ACC_OV_ALERT
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.ACC_OV_ALERT);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ACCOVALERT,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->ACC_OV_ALARM
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.ACC_OV_ALARM);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ACCOVALARM,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->ACC_HAL_ALERT
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.ACC_HAL_ALERT);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ACCHALALERT,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->ACC_HAL_ALARM
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.ACC_HAL_ALARM);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ACCHALALARM,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->VEL_OV_ALERT
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.VEL_OV_ALERT);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VELOVALERT,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->VEL_OV_ALARM
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.VEL_OV_ALARM);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VELOVALARM,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->VEL_HAL_ALERT
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.VEL_HAL_ALERT);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VELHALALERT,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->VEL_HAL_ALARM
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.VEL_HAL_ALARM);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_VELHALALARM,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->ENV_OV_ALERT
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.ENV_OV_ALERT);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ENVOVALERT,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->ENV_OV_ALARM
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.ENV_OV_ALARM);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ENVOVALARM,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->ENV_HAL_ALERT
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.ENV_HAL_ALERT);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ENVHALALERT,
                          _tmpDouble);
  // bulletSensorConfig->algorithmConfig->ENV_HAL_ALARM
  snprintf(_numChar, sizeof(_numChar), "%f",
           sensorConfig->bulletSensorConfig.algoConfig.ENV_HAL_ALARM);
  _tmpDouble = atof(_numChar);
  cJSON_AddNumberToObject(_algocfg,
                          SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG_ENVHALALARM,
                          _tmpDouble);
  cJSON_AddItemToObject(_bulletsensorcfg,
                        SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG_ALGOCFG,
                        _algocfg);

  // After construct bulletSensorConfig, then add it to the root finnally
  cJSON_AddItemToObject(_root, SENSORCONFIGJSON_NAME_BULLETSENSORCONFIG,
                        _bulletsensorcfg);

  _tempjson = cJSON_Print(_root);
  sprintf(outputJson, "%s", _tempjson);
  free(_tempjson);
  _tempjson = NULL;
  cJSON_Delete(_root);  // Delete all the created nodes recursively

exit:
  if(ret != NULL)
  {
    *ret = _ret;
  }
}
