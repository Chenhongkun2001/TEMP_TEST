#ifndef AUXILIAR_H
#define AUXILIAR_H
#include "global.h"
#include "stdint.h"
#include "stdio.h"
typedef struct configure {
  char *type;
  char *filedname;
} configure_content_t;
struct datatable {
  char *tablename;
  struct configure *filedinfo;
  int filed_size;
};

#if (ENABLE_MODBUS_FEATURE == 1)
extern struct configure GatewayConfig[33];  // update to '33' if we need to add 'description' attr for gateway table too to be confirmed.
extern struct configure DeviceList[7];
#else
extern struct configure GatewayConfig[27];
extern struct configure DeviceList[6];
#endif
#if (STATUS_SENSING_FEATURE_ENABLE == 1)
// Add 'sensorModeParameter' into sensor
extern struct configure SensorConfig[98]; 
#else /* STATUS_SENSING_FEATURE_ENABLE == 1 */
extern struct configure SensorConfig[97]; 
#endif /* STATUS_SENSING_FEATURE_ENABLE != 1 */
extern struct configure SensorData[25];

uint8_t send_msg(int qid, struct msgbuf *msg, size_t __msgsz);
uint8_t recv_msg(int qid, struct msgbuf *msg, uint8_t msgtyp);
uint8_t recv_msg1(int qid, struct msgbuf *msg, uint8_t msgtyp);
uint8_t MYULT_EncodeVersion(char *string, uint32_t *version);
void Calculate_query_data_size(char *data, uint32_t *size);
void gain_tablename(uint32_t tb, char *table_name, uint16_t len);
uint8_t Gain_clientID(char *macaddr, char *clientID);
uint8_t Gain_passwd(char *clientID, char *passwd, uint8_t len);
uint32_t revert_tablename(char *table_name);
int findMax(int arr[], int low, int high);
void get_curr_time(char *buf, uint8_t buf_len);
#endif // AUXILIAR_H
