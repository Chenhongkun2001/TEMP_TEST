#ifndef MQTT_H
#define MQTT_H
// #include "global.h"
// #include "stdint.h"
// #include "stdio.h"
#include "MQTTClient.h" /* MQTT头文件的位置  */
#if 0
// #define ADDRESS "mqtt://192.168.0.128:1883"
#define ADDRESS "mqtt://mqtt-dpuat.skf4u.com:1883"
#define CLIENTID "mqttx_560519f0"
#define MQTT_USERNAME "insight"
#define MQTT_PASSWORD "insight123456"
#else
#define ADDRESS "mqtt://mqtt-dpuat.skf4u.com:1883" // 1885
#define CLIENTID "A841F48BB274"
#define MQTT_USERNAME "bullet"
#define MQTT_PASSWORD "A841F48B74B2"
#define GATAWAY_NAME "gateway"
#define SENSOR_NAME "sensor"
#define SENSOR_T_NAME "sensort"
#endif
/**
 * @brief
 * type includes gateway and sensor;clientID is the mac address of gateway(+
 * /mac of sensor when type is sensor)
 * @version 0.1
 * @author EVIDEN
 * @date 2024-04-17
 * @copyright Copyright (c) 2024
 */
/***/
#define COINFIG_MQTT_PUBLISH_DATA_TOPIC(buffer, type, clientID)                \
  do {                                                                         \
    snprintf(buffer, sizeof(buffer) - 1, "bullet/%s/data/up/%s", type,         \
             clientID);                                                        \
  } while (0);
#define COINFIG_MQTT_PUBLISH_CONFIG_TOPIC(buffer, type, clientID)              \
  do {                                                                         \
    snprintf(buffer, sizeof(buffer) - 1, "bullet/%s/config/up/%s", type,       \
             clientID);                                                        \
  } while (0);
#define COINFIG_MQTT_PUBLISH_FUOTA_TOPIC(buffer, type, clientID)               \
  do {                                                                         \
    snprintf(buffer, sizeof(buffer) - 1, "bullet/%s/fuota/up/%s", type,        \
             clientID);                                                        \
  } while (0);
#define COINFIG_MQTT_PUBLISH_DEBUG_TOPIC(buffer, type, clientID)               \
  do {                                                                         \
    snprintf(buffer, sizeof(buffer) - 1, "bullet/%s/debug/up/%s", type,        \
             clientID);                                                        \
  } while (0);
// down
#define COINFIG_MQTT_SBUSCRIBE_DATA_TOPIC(buffer, type, clientID)              \
  do {                                                                         \
    snprintf(buffer, sizeof(buffer) - 1, "bullet/%s/data/down/%s", type,       \
             clientID);                                                        \
  } while (0);
#define COINFIG_MQTT_SBUSCRIBE_CONFIG_TOPIC(buffer, type, clientID)            \
  do {                                                                         \
    snprintf(buffer, sizeof(buffer) - 1, "bullet/%s/config/down/%s", type,     \
             clientID);                                                        \
  } while (0);
#define COINFIG_MQTT_SBUSCRIBE_FUOTA_TOPIC(buffer, type, clientID)             \
  do {                                                                         \
    snprintf(buffer, sizeof(buffer) - 1, "bullet/%s/fuota/down/%s", type,      \
             clientID);                                                        \
  } while (0);
#define COINFIG_MQTT_SBUSCRIBE_DEBUG_TOPIC(buffer, type, clientID)             \
  do {                                                                         \
    snprintf(buffer, sizeof(buffer) - 1, "bullet/%s/debug/down/%s", type,      \
             clientID);                                                        \
  } while (0);
#define PUBLISH_TOPIC "bullet/sensor/data/up/C4BD6A020401/C4BD6A123459"
#define SUBSCRIBE_TOPIC "bullet/sensor/config/down/C4BD6A020401/C4BD6A123459"
// #define SUBSCRIBE_TOPIC "bullet/sensor/data/down/C4BD6A020401/C4BD6A123459"
#define REV_TIMEOUT 2000L
#define SEND_TIMEOUT 1000L
#define CONFIG_DATA_BUFFER_LEN 4096

enum _mqtt_qos {
  /* 0 - 7 used */
  mqtt_qos_zero = 0, // at most once
  mqtt_qos_one,      // at least once
  mqtt_qos_two,      // only once
};

int mqtt_task();
int mqtt_send(MQTTClient_message *pubmsg, char *send_topic);
void up_edit_time();
#endif
