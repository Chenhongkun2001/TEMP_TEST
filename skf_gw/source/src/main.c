#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
 
#include "bt_hcitool.h"
#include "app_pro_ble.h"
#include "app_pro_general.h"
#include "util_dbg.h"
#include "sys_def.h"
#include "gwConfig.h"
#include "app_froto.h"
#include "app_io.h"
#include "app_hci_evt_mgmt.h"

#include "global.h"
#include "ppGW.h"
#include "list.h"

#if (SKF_GW_NEW == 1)
#include <fcntl.h>
#include "app_pro_ble_new.h"
#include "app_pro_general_new.h"
#include "unittest.h"
#endif /* SKF_GW_NEW == 1 */
#include "app_common.h"

// for logging
DBG_LOCAL_LOG_DEBUG
uint8_t g_log_level = CONFIG_LOG_LEVEL_SET;
#define LOG_LEVEL_PARAMETER_IDX (1)


//typedef void (*l_timeout_notify_cb_t) (struct l_timeout *timeout,void *user_data);
 void time_cbk(struct l_timeout *timeout,void *user_data)
{
 	DBG_LOG_INFO("hello");
}

int test_for_cjson(void);
int btmon_main(int argc, char *argv[]);

static bool
parse_u16_arg(const char *str, uint16_t *out)
{
  char *end = NULL;
  unsigned long val;
 
  if((str == NULL) || (out == NULL))
  {
    return false;
  }
 
  errno = 0;
  val = strtoul(str, &end, 10);
 
  if((errno != 0) ||
     (end == str) ||
     (*end != '\0') ||
     (val > 0xFFFF))
  {
    return false;
  }
 
  *out = (uint16_t)val;
  return true;
}
 
 
static bool
parse_mac12(const char *str, uint8_t mac[6])
{
  unsigned int b[6];
 
  if((str == NULL) ||
     (mac == NULL) ||
     (strlen(str) != 12))
  {
    return false;
  }
 
  if(sscanf(str,
            "%2x%2x%2x%2x%2x%2x",
&b[0], &b[1], &b[2],
&b[3], &b[4], &b[5]) != 6)
  {
    return false;
  }
 
  for(int i = 0; i < 6; i++)
  {
    if(b[i] > 0xFF)
    {
      return false;
    }
 
    mac[i] = (uint8_t)b[i];
  }
 
  return true;
}
 
 
static int
run_privileged_hci_conn_update(
  int argc,
  char **argv)
{
  struct bluetooth_connection_param con_param = { 0 };
  uint8_t con_id[6] = { 0 };
 
  /*
   * argv:
   * 0 skf_gw
   * 1 --priv-hci-conn-update
   * 2 MAC
   * 3 min interval
   * 4 max interval
   * 5 latency
   * 6 supervision timeout
   * 7 min ce len
   * 8 max ce len
   */
  if(argc != 9)
  {
    fprintf(stderr,
            "Invalid privileged HCI helper arguments\n");
    return 2;
  }
 
  /*
   * This mode must never be usable by the normal skfgw process.
   */
  if(geteuid() != 0)
  {
    fprintf(stderr,
            "Privileged HCI helper requires root\n");
    return 126;
  }
 
  if(!parse_mac12(argv[2], con_id) ||
     !parse_u16_arg(argv[3], &con_param.min_con_interval) ||
     !parse_u16_arg(argv[4], &con_param.max_con_interval) ||
     !parse_u16_arg(argv[5], &con_param.con_latency) ||
     !parse_u16_arg(argv[6], &con_param.supervision_timeout) ||
     !parse_u16_arg(argv[7], &con_param.min_ce_len) ||
     !parse_u16_arg(argv[8], &con_param.max_ce_len))
  {
    fprintf(stderr,
            "Invalid privileged HCI helper parameters\n");
    return 2;
  }
 
  con_param.phy = LE_PHY_1M;
 
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
 
  if(ST_OK !=
     bt_hcitool_set_bluetooth_connection_param(
&con_param,
       con_id))
  {
    fprintf(stderr,
            "Privileged HCI connection update failed\n");
    return 1;
  }
 
#else
 
  if(ST_OK !=
     bt_hcitool_set_bluetooth_connection_param(
&con_param))
  {
    fprintf(stderr,
            "Privileged HCI connection update failed\n");
    return 1;
  }
 
#endif
 
  return 0;
}

int
main(
  int argc,
  char **argv)
{
  int _status = 0;
  pid_t _pid = 0, _child_pid = 0;

  if((argc >= 2) &&
   (strcmp(argv[1],
           "--priv-hci-conn-update") == 0))
{
  return run_privileged_hci_conn_update(
    argc,
    argv);
}

  DBG_LOG_INFO("skf_gw (including multiple processes) started ...\n\r");
  DBG_LOG_DEBUG("Number of arguments (#) = %d\n\r", argc);
  for(uint32_t i = 0; i < argc; i++)
  {
    DBG_LOG_DEBUG("argv[%d]: %s\n\r", i, argv[i]);
  }
  if(argc >= LOG_LEVEL_PARAMETER_IDX + 1)
  {
    uint8_t tmpu8 = (uint8_t)strtol(argv[LOG_LEVEL_PARAMETER_IDX], NULL, 10);
    if(tmpu8 < CONFIG_LOG_LEVEL_ALL)
    {
      g_log_level = tmpu8;
    } else
    {
      g_log_level = CONFIG_LOG_LEVEL_SET;
    }
  } else  // set to default 
  {
    g_log_level = CONFIG_LOG_LEVEL_SET;
  }
  srand(time(NULL));

#if (SKF_GW_NEW == 1)
  // Initialize GPIOs for LED
  app_io_init_gpio_for_led();

  // Create files for IPC
  int _fd = 0;

  _fd = open(FPATH_FOR_SKF_GW_NEW_IPC0, O_CREAT | O_RDWR,
             S_IRUSR | S_IWUSR);
  if(_fd < 0)
  {
    DBG_LOG_ERR("Failed to create file %s, for %s",
                FPATH_FOR_SKF_GW_NEW_IPC0, strerror(errno));
    exit(EXIT_FAILURE);
  }
  close(_fd);

  _fd = open(FPATH_FOR_SKF_GW_NEW_IPC1, O_CREAT | O_RDWR,
             S_IRUSR | S_IWUSR);
  if(_fd < 0)
  {
    DBG_LOG_ERR("Failed to create file %s, for %s",
                FPATH_FOR_SKF_GW_NEW_IPC1, strerror(errno));
    exit(EXIT_FAILURE);
  }
  close(_fd);

  // Create child process
  _pid = fork();

  switch(_pid)
  {
    case -1:
    {
      DBG_LOG_ERR("Failed to create a process\n\r");
      break;
    }
    case 0:
    {
      // The child process
      DBG_LOG_DEBUG("The child-PID is %d\n\r", getpid());

#if ((UNITTEST == UNITTEST_SCAN_AND_PARSE_INSIGHT_T) || \
      (UNITTEST == UNITTEST_MULTI_CON_SCAN_AND_PARSE_INSIGHT_T) || \
      (UNITTEST == UNITTEST_UNIVERSE))
      // Create an IPC pipe between process scanning and
      // the process general
      if(ST_OK != app_hci_create_pipe_fds())
      {
        DBG_LOG_ERR("Failed to create pipe");
        exit(EXIT_FAILURE);
      }

      // Create one more child process
      _child_pid = fork();

      if(_child_pid > 0)
      {
        // The parent process
        DBG_LOG_DEBUG("The parent-PID is %d\n\r", getpid());
        DBG_LOG_DEBUG("The child-PID is %d\n\r", _pid);

        close(g_pipefd_hci_event_report[1]);
        app_pro_general_new(_child_pid);

        // SHOULD NOT BE HERE FOREVER!
        // waitpid has been already implemented in app_pro_general_new
        exit(EXIT_FAILURE);
      }
      else if(0 == _child_pid)
      {
        // The child process
        DBG_LOG_DEBUG("The child-PID is %d\n\r", getpid());
        close(g_pipefd_hci_event_report[0]);
        btmon_main(0, NULL);
        exit(EXIT_FAILURE);
      }
      else
      {
        DBG_LOG_ERR("Failed to create a process\n\r");
        exit(EXIT_FAILURE);
      }
#else \
      /* UNITTEST == UNITTEST_SCAN_AND_PARSE_INSIGHT_T ||
         UNITTEST_MULTI_CON_SCAN_AND_PARSE_INSIGHT_T */
      app_pro_general_new(0);
#endif \
      /* UNITTEST != UNITTEST_SCAN_AND_PARSE_INSIGHT_T ||
         UNITTEST_MULTI_CON_SCAN_AND_PARSE_INSIGHT_T*/
      exit(EXIT_SUCCESS);
      break;
    }
    default:
    {
      // The parent process
      DBG_LOG_DEBUG("The parent-PID is %d\n\r", getpid());
      DBG_LOG_DEBUG("The child-PID is %d\n\r", _pid);
      app_pro_ble_new(_pid);
      // SHOULD NOT BE HERE FOREVER!
      // waitpid has been already implemented in app_pro_ble_new
      exit(EXIT_FAILURE);
      break;
    }
  }
#else /* SKF_GW_NEW == 1 */
	int kp_status = 0;
	pid_t kp_pid = 0, tp_pid = 0;
	#if(0)
	// for test only
		gwConfig_t kp_gwcfg = {0};
		kp_gwcfg.gwConfig.modbusConfig.baudrateSlave = 115200;
		kp_gwcfg.gwConfig.modbusConfig.paritySlave = 1;
		kp_gwcfg.gwConfig.modbusConfig.registerMode = 1;
		kp_gwcfg.gwConfig.modbusConfig.slaveAddrSlave = 1;
		kp_gwcfg.gwConfig.modbusConfig.stopSlave = 1;
		snprintf( kp_gwcfg.gwConfig.description, sizeof(kp_gwcfg.gwConfig.description),"%s","AAA_BBB_CCC");
		
		INIT_LIST_HEAD( &kp_gwcfg.gwConfig.childrenList);
		
		DBG_LOG_INFO("BKP");
		if(true != app_cjson_gwconfig_to_jsonstr( &kp_gwcfg, "/tmp/val_to_jstr"))
		{
			DBG_LOG_ERR("fail to write gwconf to json-string");
		}		
		DBG_LOG_INFO("BKP");
		
		memset( &kp_gwcfg, 0, sizeof(kp_gwcfg));
		if(true != app_cjson_jsonstr_to_gwconfig( &kp_gwcfg, "/tmp/val_to_jstr"))
		{
			DBG_LOG_ERR("fail to read gwconf from json-string");
		}
		else
		{
			DBG_LOG_WARN("baudrate = %d", kp_gwcfg.gwConfig.modbusConfig.baudrateSlave);
			DBG_LOG_WARN("parity = %d", kp_gwcfg.gwConfig.modbusConfig.paritySlave);			
			DBG_LOG_WARN("registerMode = %d", kp_gwcfg.gwConfig.modbusConfig.registerMode);		
			DBG_LOG_WARN("slaveAddr = %d", kp_gwcfg.gwConfig.modbusConfig.slaveAddrSlave);
			DBG_LOG_WARN("stop = %d", kp_gwcfg.gwConfig.modbusConfig.stopSlave);
			DBG_LOG_WARN("gw-description = %s\n", kp_gwcfg.gwConfig.description);
		}
		
		sleep(1);
		return 0;
	#endif
		
#if(0)// only for debug
	for(uint32_t i = 0; i < 1; i++)
	{
		// APIs for encoding test
		//util_froto_fill_msg_config_dissem_set_time();
		//util_froto_fill_msg_data_selection_retrieve_battery();			
		//util_froto_fill_msg_data_selection_retrieve_data();
		//util_froto_fill_msg_command_dissem();
		//util_froto_fill_msg_version_retrieve();
		//util_froto_fill_msg_fuota_notify_dissem();
		//util_froto_fill_msg_img_block_dissem();

		//util_froto_fill_msg_up_data_upload();
		//util_froto_fill_msg_up_image_block_request();
		//util_froto_fill_msg_up_file_notify_upload();
		//util_froto_fill_msg_up_current_version_upload();
		//util_froto_fill_msg_up_image_block_upload();
	}
	DBG_LOG_INFO("Going to exti!");
	return;
#endif
	// to create files for IPC
	sys_create_file_for_ipc();
	// init GPIO for LED-control
	app_io_init_gpio_for_led();

	kp_pid = fork();
	switch(kp_pid)
	{
		case -1:
			{
				printf("Oops! Fail to create a process");
				break;
			}
			case 0:
			{ // the child-process				
				printf("the child-PID is %d\n", getpid());
				if(ST_OK != app_hci_create_pipe_fds())
				{
					DBG_LOG_ERR("fail to create pipe");
					exit(-1);
				}				
				tp_pid = fork();
				if(tp_pid)
				{
					close(g_pipefd_hci_event_report[1]); // close the terminal for writting
					
					app_pro_general();
				}
				else if(0 == tp_pid)
				{
					close(g_pipefd_hci_event_report[0]); // close the terminal for reading
					btmon_main( 0, NULL);
				}
				else
				{
					DBG_LOG_ERR("fail to fork");
					exit(-1);
				}
				break;
			}
			default:
			{ // the parent process
				// the parent-process
				app_pro_ble();				
				printf("the parent-PID is %d\n", getpid());
				// tp_pid = wait(&kp_status);
				printf("child-process(%d) exit with status %d\n", tp_pid, kp_status);
				break;
			}
	}
#endif /* SKF_GW_NEW != 1 */
  DBG_LOG_INFO("Exit (%d) ... \n", getpid());
  return;
}


