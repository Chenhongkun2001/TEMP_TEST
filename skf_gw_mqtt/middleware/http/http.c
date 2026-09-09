#include "http.h"
#include "common.h"
#include <DeviceAppBulletGateway.pb.h>
#include <assert.h>
#include <dirent.h>
#include <global.h>
#include <ipcmbs.h>
#include <jsonparse.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
DBG_LOCAL_LOG_DEBUG

#define CONFIG_START_MQTT_SHELL "/home/root/skf_gw_mqtt/restart_mqtt.sh"
#define CONFIG_START_SQL_SHELL "/home/root/skf_gw_mqtt/restart_sql.sh"
#define CONFIG_START_BLE_SHELL "/home/root/skf_gw_mqtt/restart_ble.sh"
#define CONFIG_START_MODBUS_SHELL "/home/root/skf_gw_mqtt/restart_mod.sh"
#define CONFIG_ROOT_TMP_DOWNLOAD "/var/lib/skf-gateway/update"
#define CONFIG_GATEWAY_FOLDER "/var/lib/skf-gateway/update/gateway"
#define CONFIG_SENSOR_FOLDER "/var/lib/skf-gateway/update/sensor"
#define CONFIG_OTHERS_FOLDER "/var/lib/skf-gateway/update/others"
//
extern uint8_t downloadtype;
static uint8_t arg_num = 0;
static char tmpshell[512] = {0};
extern sem_t otasem;
extern sem_t enablehttpsem;
static char command[2048] = {0};
static char filename[100] = {0};
static char folder_dir[128] = {0};
extern char firmname[100];
static char tempfile[CONFIG_JSON_MAX_LEN] = {0};
uint32_t sensor_firmware_version = 0;
static bool update_flag = false;
static char local_topc[50] = {0};
extern SKFChina_App_AppMessage SKF_BullGateway;
int getfilename(char *basePath, char *jsonfile, uint32_t len) {
  DIR *dir;
  struct dirent *ptr;
  if ((dir = opendir(basePath)) == NULL) {
    LOG_WARN(OUTPOINT, "Open dir error...\r\n");
    return 1;
  }

  while ((ptr = readdir(dir)) != NULL) {
    if (strcmp(ptr->d_name, ".") == 0 ||
        strcmp(ptr->d_name, "..") == 0) /// current dir OR parrent dir
      continue;
    else if ((ptr->d_type == 8) &&
             (NULL != strstr(ptr->d_name, ".json"))) /// is json file
    {
      snprintf(jsonfile, len, "%s", ptr->d_name);
      LOG_INFO(OUTPOINT, "file_name:%s/%s\n", basePath, ptr->d_name);
    }
  }
  closedir(dir);
  return 0;
}

static void handle_files(uint8_t argno, uint32_t sensor_version,
                         uint32_t downloadtype, char *topic, char *folder_dir,
                         char *firmname) {
  char tempstr[100] = {0};
  int status = 0;
  gw_pro_sqlite_cond_element_t
      http_filter[GW_PRO_MAX_DATA_FIELD_PER_OPERATION] = {0};
  gw_pro_command_sqlite_filter_response_t http_filter_rx = {0};
  struct msgbuf msgbufp = {0};
  gw_pro_message_header_t *pdata = (gw_pro_message_header_t *)msgbufp.mtext;
  gw_pro_data_t update_data[GW_PRO_MAX_DATA_FIELD_PER_OPERATION] = {0};
  if (argno == MAX_DOWNLOAD_ARGS) {
    switch (downloadtype) {
    case 1: {
      if (!strcmp(topic, clientID)) {
        snprintf(tempstr, sizeof(tempstr) - 1, "%s/%s", CONFIG_GATEWAY_FOLDER,
                 GATEWAY_FIRM_FILE_NAME);
        if ((access(tempstr, 0)) == -1) {
          LOG_WARN(OUTPOINT, "No such file:%s!\r\n", GATEWAY_FIRM_FILE_NAME);
          return;
        }
        // fuota gateway
        FILE *fp = NULL;
        fp = fopen(CONFIG_START_MQTT_SHELL, "rb");
        if (fp == NULL) {
          LOG_WARN(OUTPOINT, "Failed to ota gateway!\r\n");
          return;
        }
        fread(tmpshell, 1, sizeof(tmpshell), fp);
        fclose(fp);
        snprintf(command, sizeof(command) - 1, "cd %s &&  ",
                 CONFIG_GATEWAY_FOLDER);
        if (strstr(tmpshell, "v1")) {
          update_flag = false;
          strcat(command,
                 "tar -zxf *.tar.gz && chmod 777 mqtt_op sql_op "
                 "skf_gw_1 && mv mqtt_op sql_op skf_gw_1 ~/skf_gw_mqtt/v2");
        } else {
          update_flag = true;
          strcat(command,
                 "tar -zxf *.tar.gz && chmod 777 mqtt_op sql_op "
                 "skf_gw_1 && mv mqtt_op sql_op skf_gw_1 ~/skf_gw_mqtt/v1");
        }
        status = system(command);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
          LOG_WARN(OUTPOINT, "gateway: Failed to ota!\r\n");
          return;
        }

        // Update skf_gw_mod if there existed
        bool hasModbusRtu = false;
        snprintf(command, sizeof(command) - 1, "%s/skf_gw_mod",
                 CONFIG_GATEWAY_FOLDER);
        LOG_DEBUG(OUTPOINT, "Check skf_gw_mod: %s!\r\n", command);
        if ((access(command, 0)) == 0) {
          hasModbusRtu = true;
          snprintf(command, sizeof(command) - 1, "cd %s &&  ",
                 CONFIG_GATEWAY_FOLDER);
          if (update_flag == false) {
            strcat(command,
                  "chmod 777 skf_gw_mod "
                  "&& mv skf_gw_mod ~/skf_gw_mqtt/v2");
          } else {
            strcat(command,
                  "chmod 777 skf_gw_mod "
                  "&& mv skf_gw_mod ~/skf_gw_mqtt/v1");
          }
          status = system(command);
          if (!WIFEXITED(status) || WEXITSTATUS(status)) {
            LOG_WARN(OUTPOINT, "gateway: Failed to ota!\r\n");
            return;
          }
        }

        snprintf(command, sizeof(command) - 1,
                 "pkill restart");
        status = system(command);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
          LOG_WARN(OUTPOINT, "gateway: Failed to ota!\r\n");
          return;
        }
        sleep(1);
        snprintf(command, sizeof(command) - 1,
                 "cd %s && chmod 777 restart*.sh && mv * "
                 "~/skf_gw_mqtt",
                 CONFIG_GATEWAY_FOLDER);
        status = system(command);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
          LOG_WARN(OUTPOINT, "gateway: Failed to ota!\r\n");
          return;
        }

        if(hasModbusRtu)
        {
          if (update_flag) {
            snprintf(command, sizeof(command) - 1,
                    "sed -i 's/v2/v1/g' %s && sed -i 's/v2/v1/g' %s && sed -i "
                    "'s/v2/v1/g' %s && sed -i 's/v2/v1/g' %s",
                    CONFIG_START_MQTT_SHELL, CONFIG_START_SQL_SHELL,
                    CONFIG_START_BLE_SHELL, CONFIG_START_MODBUS_SHELL);
          } else {
            snprintf(command, sizeof(command) - 1,
                    "sed -i 's/v1/v2/g' %s && sed -i 's/v1/v2/g' %s && sed -i "
                    "'s/v1/v2/g' %s && sed -i 's/v1/v2/g' %s",
                    CONFIG_START_MQTT_SHELL, CONFIG_START_SQL_SHELL,
                    CONFIG_START_BLE_SHELL, CONFIG_START_MODBUS_SHELL);
          }
        }else{
          if (update_flag) {
            snprintf(command, sizeof(command) - 1,
                    "sed -i 's/v2/v1/g' %s && sed -i 's/v2/v1/g' %s && sed -i "
                    "'s/v2/v1/g' %s",
                    CONFIG_START_MQTT_SHELL, CONFIG_START_SQL_SHELL,
                    CONFIG_START_BLE_SHELL);
          } else {
            snprintf(command, sizeof(command) - 1,
                    "sed -i 's/v1/v2/g' %s && sed -i 's/v1/v2/g' %s && sed -i "
                    "'s/v1/v2/g' %s",
                    CONFIG_START_MQTT_SHELL, CONFIG_START_SQL_SHELL,
                    CONFIG_START_BLE_SHELL);
          }
        }
        status = system(command);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
          LOG_WARN(OUTPOINT, "gateway: Failed to ota!\r\n");
          return;
        }
        sleep(1);
        snprintf(command, sizeof(command) - 1, "cd %s &&  rm -rf *",
                 CONFIG_GATEWAY_FOLDER);
        status = system(command);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
          LOG_WARN(OUTPOINT, "gateway: Failed to ota!\r\n");
          return;
        }
        snprintf(command, sizeof(command) - 1, "%s &", CONFIG_START_BLE_SHELL);
        status = system(command);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
          LOG_WARN(OUTPOINT, "gateway: Failed to ota!\r\n");
          return;
        }
        snprintf(command, sizeof(command) - 1, "%s &", CONFIG_START_MQTT_SHELL);
        status = system(command);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
          LOG_WARN(OUTPOINT, "gateway: Failed to ota!\r\n");
          return;
        }
        snprintf(command, sizeof(command) - 1, "%s &", CONFIG_START_SQL_SHELL);
        status = system(command);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
          LOG_WARN(OUTPOINT, "gateway: Failed to ota!\r\n");
          return;
        }
        if(hasModbusRtu)
        {
          snprintf(command, sizeof(command) - 1, "%s &", CONFIG_START_MODBUS_SHELL);
          status = system(command);
          if (!WIFEXITED(status) || WEXITSTATUS(status)) {
            LOG_WARN(OUTPOINT, "gateway: Failed to ota!\r\n");
            return;
          }
        }
      } else {
        // fuota sensor update the sensorconfig
        http_filter[0].condition = gw_pro_sqlite_cond_str_equal;
        http_filter[0].field = 4;
        snprintf(http_filter[0].condParam.string_value.s,
                 GW_PRO_MAX_STRING_LEN_BYTE - 1, "%s", topic);
        if (ipc_sql_filter_item(DEVICE_LIST_TABLE, 1, http_filter, &msgbufp) ||
            pdata->comRespFg == GW_PRO_COMRESPFG_COMMAND ||
            pdata->command_or_response.responseMsg.response ==
                gw_pro_response_failed) {
          LOG_WARN(OUTPOINT, "Failed to get datas from Dev table\r\n");
          return;
        }
        memcpy(&http_filter_rx,
               &pdata->command_or_response.responseMsg.responseInfo
                    .sqlite_filter_item_reponse,
               sizeof(gw_pro_command_sqlite_filter_response_t));
        update_data[0].field = 96;
        update_data[0].data.data_uint32 = sensor_version;
        update_data[1].field = 97;
        snprintf(update_data[1].data.data_char, GW_PRO_MAX_STRING_LEN_BYTE - 1,
                 "%s/%s", folder_dir, firmname);
        if (ipc_sql_update_item(SENSOR_CONFIGURE_TABLE,
                                http_filter_rx.filteredItemIdx[0], 2,
                                update_data, NULL)) {
          LOG_WARN(OUTPOINT, "Failed to update %d into sensor table\r\n",
                   http_filter_rx.filteredItemIdx[0]);
        }
        LOG_INFO(OUTPOINT, "update %d into sensor table\r\n",
                 http_filter_rx.filteredItemIdx[0]);
      }
    } break;
    case 2: {
      // parse json and update the database
      getfilename(CONFIG_OTHERS_FOLDER, filename, sizeof(filename) - 1);
      snprintf(command, sizeof(command) - 1, "%s/%s", CONFIG_OTHERS_FOLDER,
               filename);
      parse_config(command);
      snprintf(command, sizeof(command) - 1, "cd %s && rm -rf *",
               CONFIG_OTHERS_FOLDER);
      status = system(command);
      if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        LOG_WARN(OUTPOINT, "parse: Failed to update config!\r\n");
        return;
      }
    } break;
    default:
      break;
    }
  }
}

void *wait_handle_heep_task(void *arg) {
  downloadparameter_t downloadparameter = {0};
  downloadparameter.sensor_firmware_version = sensor_firmware_version;
  downloadparameter.downloadtype = downloadtype;
  downloadparameter.argno = arg_num;
  snprintf(downloadparameter.local_topic,
           sizeof(downloadparameter.local_topic) - 1, "%s", local_topc);
  snprintf(downloadparameter.folder_dir,
           sizeof(downloadparameter.folder_dir) - 1, "%s", folder_dir);
  char local_ota_url[OTA_URL_ADDR_LEN] = {0};
  char local_up_url[OTA_URL_ADDR_LEN] = {0};
  char local_firmname[100] = {0};
  char local_upfilename[100] = {0};
  char removefile[100] = {0};
  char *downargs[MAX_DOWNLOAD_ARGS] = {"curl", "-X GET", (char *)local_ota_url,
                                       "-o", local_firmname};
  char *upargs[MAX_UPLOAD_ARGS] = {"curl",
                                   "-H",
                                   "\"Authorization:token\"",
                                   "-F",
                                   local_upfilename,
                                   (char *)local_up_url};
  snprintf(local_up_url, sizeof(local_up_url) - 1, "%s", up_url);
  snprintf(local_ota_url, sizeof(local_ota_url) - 1, "%s", ota_url);
  snprintf(local_firmname, sizeof(local_firmname) - 1, "%s", firmname);
  snprintf(local_upfilename, sizeof(local_upfilename) - 1, "%s", upfilename);
  sem_post(&enablehttpsem);
  sem_wait(&otasem);
  int8_t runno = 3;
  int status = 0;
  memset(command, 0, sizeof(command));
  if (downloadparameter.argno == MAX_DOWNLOAD_ARGS) {
    if (downloadparameter.downloadtype == 1) {
      if (!strcmp(downloadparameter.local_topic, clientID))
      {
        // Empty down/gateway before downloading ...
        snprintf(command, sizeof(command) - 1,
                 "cd %s && rm -r *",
                 CONFIG_GATEWAY_FOLDER);
        status = system(command);
        
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
          LOG_WARN(OUTPOINT, "Failed to empty /home/root/down/gateway but do nothing\r\n");
          // DO NOT CHECK STATUS SINCE IF THERE IS NO FILE ERROR WOULD BE RETURNED
          // goto EXIT;
        }
        
        memset(command, 0, sizeof(command));
        snprintf(command, sizeof(command) - 1, "cd %s &&  ",
                 CONFIG_GATEWAY_FOLDER);
      }
      else
        snprintf(command, sizeof(command) - 1, "cd %s &&  ",
                 downloadparameter.folder_dir);
    } else if (downloadparameter.downloadtype == 2) {
      snprintf(command, sizeof(command) - 1, "cd %s &&  ",
               CONFIG_OTHERS_FOLDER);
    }
  }
  for (uint8_t i = 0; i < downloadparameter.argno; i++) {
    if (downloadparameter.argno == MAX_DOWNLOAD_ARGS)
      strcat(command, downargs[i]);
    else
      strcat(command, upargs[i]);
    strcat(command, " ");
  }
  command[strlen(command) - 1] = 0;
  LOG_INFO(OUTPOINT, "cmd:%s\r\n", command);
  do {
    status = system(command);
  } while ((!WIFEXITED(status) || WEXITSTATUS(status)) && (--runno));
  LOG_INFO(OUTPOINT, "argno:%d;d_type:%d\r\n", downloadparameter.argno,
           downloadparameter.downloadtype);
  if (runno <= 0) {
    LOG_WARN(OUTPOINT, "%s action is executed unsuccessfully!\r\n",
             (downloadparameter.argno == MAX_DOWNLOAD_ARGS) ? "download "
                                                            : "upload");
  } else {
    LOG_INFO(OUTPOINT, "%s action is executed successfully!\r\n",
             (downloadparameter.argno == MAX_DOWNLOAD_ARGS) ? "download "
                                                            : "upload");
    handle_files(downloadparameter.argno,
                 downloadparameter.sensor_firmware_version,
                 downloadparameter.downloadtype, downloadparameter.local_topic,
                 downloadparameter.folder_dir, local_firmname);
  }
#if 1
  if (downloadparameter.argno != MAX_DOWNLOAD_ARGS) {
    if (strstr(local_upfilename, SENSOR_CONFIG_FILE_NAME))
      snprintf(removefile, sizeof(removefile) - 1, "%s_%s",
               downloadparameter.local_topic, SENSOR_CONFIG_FILE_NAME);
    else
      snprintf(removefile, sizeof(removefile) - 1, "%s_%s",
               downloadparameter.local_topic, GATEWAY_CONFIG_FILE_NAME);
    usleep(100000);

#ifndef TEST_MAC
    if (remove(removefile))
      LOG_WARN(OUTPOINT, "Del file:%s failed!\r\n", removefile);
#else
    FILE *fp = NULL;
    fp = fopen(removefile, "rb");
    if (fp != NULL) {
      memset(tempfile, 0, sizeof(tempfile));
      fread(tempfile, 1, sizeof(tempfile), fp);
      fclose(fp);
      LOG_DEBUG(OUTPOINT, "json content(%ld):%s\r\n", strlen(tempfile),
                tempfile);
    }

#endif
  }
#endif
EXIT:
  sem_post(&otasem);
  pthread_exit(NULL);
}
int enable_http_task(uint8_t argno) {
  pthread_t tid = 0;
  arg_num = argno;
  snprintf(local_topc, sizeof(local_topc) - 1, "%s", topic_clientID);
  if ((access(CONFIG_ROOT_TMP_DOWNLOAD, 0)) == -1) {
    mkdir(CONFIG_ROOT_TMP_DOWNLOAD, S_IRWXU);
  }
  if (argno == MAX_DOWNLOAD_ARGS) {
    if (downloadtype == 1 && !strcmp(local_topc, clientID)) {
      if ((access(CONFIG_GATEWAY_FOLDER, 0)) == -1) {
        mkdir(CONFIG_GATEWAY_FOLDER, S_IRWXU);
      }
      snprintf(firmname, sizeof(firmname) - 1, "%s", GATEWAY_FIRM_FILE_NAME);
    } else if (downloadtype == 2) {
      if ((access(CONFIG_OTHERS_FOLDER, 0)) == -1) {
        mkdir(CONFIG_OTHERS_FOLDER, S_IRWXU);
      }
      if (strstr((char *)ota_url, "sensorConfig")) {
        snprintf(firmname, sizeof(firmname) - 1, "%s", SENSOR_CONFIG_FILE_NAME);
      } else if (strstr((char *)ota_url, "gwConfig")) {
        snprintf(firmname, sizeof(firmname) - 1, "%s",
                 GATEWAY_CONFIG_FILE_NAME);
      } else {
        snprintf(firmname, sizeof(firmname) - 1, "%s", OTHERS_FILE_NAME);
      }
    } else if (downloadtype == 1 && strcmp(local_topc, clientID)) {
      if ((access(CONFIG_SENSOR_FOLDER, 0)) == -1) {
        mkdir(CONFIG_SENSOR_FOLDER, S_IRWXU);
      }
      snprintf(folder_dir, sizeof(folder_dir) - 1, "%s/%s",
               CONFIG_SENSOR_FOLDER, local_topc);
      if ((access(folder_dir, 0)) == -1) {
        mkdir(folder_dir, S_IRWXU);
      }
      snprintf(firmname, sizeof(firmname) - 1, "%d_%s", (int)time(NULL),
               SENSOR_FIRM_FILE_NAME);
    } else {
      LOG_WARN(OUTPOINT, "Invalid action for http!\r\n");
      return 1;
    }
  }
  int res = pthread_create(&tid, NULL, wait_handle_heep_task, NULL);
  if (res) {
    LOG_INFO(OUTPOINT, "Failed to creat a new http thread!\r\n");
  } else {
    sem_wait(&enablehttpsem);
  }
  LOG_INFO(OUTPOINT, "wait next http request!\r\n");
  return 0;
}
