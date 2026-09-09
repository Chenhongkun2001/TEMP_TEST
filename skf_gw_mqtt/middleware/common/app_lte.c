#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "global.h"
// #include <sys/stat.h>
// #include <sys/statfs.h>
// #include <fcntl.h>
// #include <sys/ipc.h>
// #include <sys/msg.h>
#include "app_io.h"
#include "app_lte.h"
#include "libusb.h"
DBG_LOCAL_LOG_DEBUG

bool isDhcp = false;
char gwIp[20] = { 0 };
static enum net_id currentNet = NET_ETH1;

/**
 * @brief Initialize the GPIO for LTE module control
 * @return @p ST_OK if things go well
 */
app_state_t
app_com_init_gpio_for_lte_ctr(void)
{
  app_state_t tpret = ST_OK;
  const char *str_pin_export =
    "echo 86 > /sys/class/gpio/export";
  const char *str_pin_dir =
    "echo \"out\" > /sys/class/gpio/gpio86/direction";
  const char *str_pin_val =
    "echo 0 > /sys/class/gpio/gpio86/value";

  // To export the PIN
  if(0 != system(str_pin_export))
  {
    tpret = ST_ERR;
    LOG_ERR(OUTPOINT, "Failed to execute %s", str_pin_export);
    goto EXIT;
  }
  // To set the PIN as output
  if(0 != system(str_pin_dir))
  {
    tpret = ST_ERR;
    LOG_ERR(OUTPOINT, "Failed to execute %s", str_pin_dir);
    goto EXIT;
  }
  // To set the default output as 0
  if(0 != system(str_pin_val))
  {
    tpret = ST_ERR;
    LOG_ERR(OUTPOINT, "Failed to execute %d", str_pin_val);
    goto EXIT;
  }
EXIT:
  return tpret;
}
/**
 * @brief Power on/off the LTE module
 * @param nstate: true to power on the LTE module; false to power off the
 * LTE module.
 * @return @p ST_OK if things go well
 */
app_state_t
app_com_set_lte_power_supply(const bool nstate)
{
  app_state_t tpret = ST_OK;
  char *cmdstr = "echo %d > /sys/class/gpio/gpio86/value";
  char strbuf[128] = { 0 };

  if(nstate)
  {
    snprintf(strbuf, sizeof(strbuf), cmdstr, 1);
    LOG_DEBUG(OUTPOINT, "%s", strbuf);
  }
  else
  {
    snprintf(strbuf, sizeof(strbuf), cmdstr, 0);
    LOG_DEBUG(OUTPOINT, "%s", strbuf);
  }

  if(0 != system(strbuf))
  {
    tpret = ST_ERR;
    LOG_ERR(OUTPOINT, "Failed to execute: %s", strbuf);
  }
  return tpret;
}
/**
 * @brief To check whether the LTE module is attached to the usb bus
 * @return true if the LTE module has been attached
 */
bool
app_com_is_lte_module_attached(void)
{
  bool tpret = false;
  libusb_context *pt_context = NULL;
  libusb_device **ptlist;
  ssize_t kpcnt = 0;
  size_t kpidx = 0;

  struct libusb_device *ptdev;
  struct libusb_device_descriptor kpdes;
  char chbuf[0x100] = { 0 };
  struct libusb_device_handle *pthandle;

  // Initialize the usb and get all the devices on the usb bus
  if(0 != libusb_init(&pt_context))
  {
    LOG_ERR(OUTPOINT, "Failed to initialize libusb");
    tpret = false;
    goto ERR;
  }
  kpcnt = libusb_get_device_list(pt_context, &ptlist);
  for(kpidx = 0; kpidx < kpcnt; kpidx++)
  {
    ptdev = ptlist[kpidx];
    if(0 != libusb_get_device_descriptor(ptdev, &kpdes))
    {
      LOG_ERR(OUTPOINT, "Failed to get usb desciptor");
      continue;
    }
    if(0 != libusb_open(ptdev, &pthandle))
    {
      LOG_ERR(OUTPOINT, "Failed to open the device");
      continue;
    }

    // the USB INFO
    LOG_DEBUG(OUTPOINT, "idx %d device: 0x%04X:0x%04X",
              kpidx,
              kpdes.idVendor,
              kpdes.idProduct);

    memset(chbuf, 0, sizeof(chbuf));
    if(libusb_get_string_descriptor_ascii(pthandle,
                                          kpdes.iManufacturer,
                                          (unsigned char *)chbuf,
                                          sizeof(chbuf)) < 0)
    {
      LOG_ERR(OUTPOINT, "Failed to get manufacturer of the usb device");
    }
    else
    {
      LOG_DEBUG(OUTPOINT, "manufacturer: %s", chbuf);
    }

    memset(chbuf, 0, sizeof(chbuf));
    if(libusb_get_string_descriptor_ascii(pthandle, kpdes.iProduct,
                                          (unsigned char *)chbuf,
                                          sizeof(chbuf)) < 0)
    {
      LOG_ERR(OUTPOINT, "Failed to get product info of the usb device");
    }
    else
    {
      LOG_DEBUG(OUTPOINT, "product: %s", chbuf);
    }
    libusb_close(pthandle);

    LOG_DEBUG(OUTPOINT,
              "ivendor: 0x%x (expected 0x%x), iproduct: 0x%x (expected 0x%x)",
              kpdes.idVendor, LTE_USB_IVENDOR,
              kpdes.idProduct, LTE_USB_IPRODUCT);

    if((LTE_USB_IVENDOR == kpdes.idVendor) &&
       (LTE_USB_IPRODUCT == kpdes.idProduct))
    {
      tpret = true;
      LOG_INFO(OUTPOINT, "the LTE Module is attached to the system");
      break;
    }
  }


	// to deinitialized the libusb
	libusb_exit(pt_context);

  return tpret;
ERR:
	if(pt_context)
	{
		libusb_exit(pt_context);				
	}
  return tpret;
}
/**
 * @brief Connect to/disconnect from internet
 * @param nstate: True to connect to internet; false to disconnect from
 * internet
 * @return true if things go well
 */
app_state_t
app_com_set_lte_con_state(const bool nstate)
{
  app_state_t tpret = ST_OK;

  if(true != app_com_is_lte_module_attached())
  {
    LOG_ERR(OUTPOINT, "The LTE module has not been attached yet");
    tpret = ST_ERR;
    return tpret;
  }

  if(nstate)
  {
    LOG_INFO(OUTPOINT, "To execute: %s", CMD_STR_LAUNCH_QCM);
    if(0 != system(CMD_STR_LAUNCH_QCM))
    {
      LOG_ERR(OUTPOINT, "Failed to connect internet");
      tpret = ST_ERR;
    }
  }
  else
  {
    LOG_INFO(OUTPOINT, "To execute: %s", CMD_STR_KILL_QCM);
    if(0 != system(CMD_STR_KILL_QCM))
    {
      LOG_ERR(OUTPOINT, "Failed to disconnect internet");
      tpret = ST_ERR;
    }
  }
  return tpret;
}
/**
 * @brief Setup the route for eth0/eth1/wwan/wifi
 * @param net: To assign which network requiring to get/set the gateway IP
 * automatically
 * @return @p ST_OK if things go well
 */
app_state_t
app_com_set_up_route_for_dhcp(const enum net_id net)
{
  app_state_t tpret = ST_OK;
  FILE *ptfile = NULL;
  uint8_t strbuf[0x100] = { 0 };
  uint8_t *pt_cmdstr = NULL;

  switch(net)
  {
    case NET_ETH0:
    {
      pt_cmdstr = CMD_STR_GET_ETH0_GWIP;
      break;
    }
    case NET_ETH1:
    {
      pt_cmdstr = CMD_STR_GET_ETH1_GWIP;
      break;
    }
    case NET_LTE:
    {
      pt_cmdstr = CMD_STR_GET_WWAN_GWIP;
      break;
    }
    case NET_WIFI:
    {
      pt_cmdstr = CMD_STR_GET_WIFI_GWIP;
      break;
    }
    default:
    {
      LOG_ERR(OUTPOINT, "Unknown network interface");
      tpret = ST_ERR;
      goto EXIT;
    }
  }

  LOG_INFO(OUTPOINT, "To execute: %s", pt_cmdstr);
  ptfile = popen(pt_cmdstr, "r");
  if(NULL == ptfile)
  {
    LOG_ERR(OUTPOINT, "Failed to popen");
    tpret = ST_ERR;
    goto EXIT;
  }

  if(fgets((void *)strbuf, sizeof(strbuf), ptfile) <= 0)
  {
    LOG_ERR(OUTPOINT, "Failed to read the return, for %s",
            strerror(errno));
    tpret = ST_ERR;
    goto EXIT;
  }

  // LOG_DEBUG(OUTPOINT, "gwip : %s, len %d", strbuf, strlen(strbuf));
  // for(uint32_t i = (strlen(strbuf) - 1); i > 0; i--)
  // {
  //   if((strbuf[i] != '\n') && (strbuf[i] != '\n'))
  //   {
  //     break;
  //   }
  //   strbuf[i] = 0;
  // }

  // // to set up the route
  // if(ST_OK != app_com_set_up_default_route(net, strbuf))
  // {
  //   LOG_ERR(OUTPOINT, "Failed to add the route");
  //   tpret = ST_ERR;
  //   goto EXIT;
  // }

EXIT:
  if(ptfile)
  {
    pclose(ptfile);
    ptfile = NULL;
  }
  return tpret;
}
/**
 * @brief Setup the default route to the given IP address
 * @param net: To assign which network requiring the gateway IP address
 * @param pt_gwip: The gw IP (in string)
 * @return @p ST_OK if things go well
 */
app_state_t
app_com_set_up_default_route(
  const enum net_id net,
  const char *pt_gwip)
{
  app_state_t tpret = ST_OK;
  const char *pt_net_iname = NULL;
  const char *hld_cmdstr_add_defroute = "route add default gw %s %s";
  const char *hld_cmdstr_del_defroute = "route del default";
  char strbuf[0x100] = { 0 };

  if(NULL == pt_gwip)
  {
    LOG_ERR(OUTPOINT, "unexpected NULL");
    tpret = ST_ERR;
    goto EXIT;
  }

  if(strlen(pt_gwip) < 7)
  {
    // 0.0.0.0 -> 7
    // Just quickly check the validity of gw IP
    LOG_ERR(OUTPOINT, "invalid GW IP str");
    tpret = ST_ERR;
    goto EXIT;
  }

  switch(net)
  {
    case NET_ETH0:
    {
      pt_net_iname = NET_ETH0_INAME;
      break;
    }
    case NET_ETH1:
    {
      pt_net_iname = NET_ETH1_INAME;
      break;
    }
    case NET_LTE:
    {
      pt_net_iname = NET_LTE_INAME;
      break;
    }
    case NET_WIFI:
    {
      pt_net_iname = NET_WIFI_INAME;
      break;
    }
    default:
    {
      LOG_ERR(OUTPOINT, "Unknown network");
      tpret = ST_ERR;
      goto EXIT;
    }
  }

  // Delete the default first
  LOG_DEBUG(OUTPOINT, "To execute : %s", hld_cmdstr_del_defroute);
  if(0 != system(hld_cmdstr_del_defroute))
  {
    LOG_WARN(OUTPOINT, "Failed to delete the default route, which may be led by there is no default route");
  }

  // Add the default route
  snprintf(strbuf, sizeof(strbuf), hld_cmdstr_add_defroute, pt_gwip,
           pt_net_iname);
  LOG_DEBUG(OUTPOINT, "To execute:%s", strbuf);
  if(0 != system(strbuf))
  {
    LOG_ERR(OUTPOINT, "Failed to add default route");
    tpret = ST_ERR;
    goto EXIT;
  }

  LOG_INFO(OUTPOINT, "The default route is modified successfully");
EXIT:
  return tpret;
}
/**
 * @brief Toggle network between ethernet1 and cellular (e.g., to switch
 * cellular network if currently the network is over ethernet)
 */
void
toggleNetwork(void)
{
  uint32_t retry = 0;

  // Just for debug ...
  if(isDhcp == true)
  {
    LOG_INFO(OUTPOINT, "DHCP is enabled");
  }
  else
  {
    LOG_INFO(OUTPOINT, "DHCP is disabled");
  }
  LOG_DEBUG(OUTPOINT, "gwIp from database %s", gwIp);

  if(currentNet == NET_ETH1)
  {
    bool _errorExist = false; // No error existed ...

    // Switch to LTE
    app_com_init_gpio_for_lte_ctr();

    app_com_set_lte_power_supply(true);

	// to add wwan0 under the management of service systemd-networkd
	app_com_set_wwan0_managed();  

    LOG_DEBUG(OUTPOINT, "Waiting 60 s ...");
    sleep(60);

    do{
      if(app_com_is_lte_module_attached())
      {
        LOG_INFO(OUTPOINT, "LTE module is attached successfully");
        _errorExist = false;
        retry = 0;
        break;
      }
      else
      {
        _errorExist = true;
        LOG_WARN(OUTPOINT,
                 "Retry to find LTE module, and have a try after %d s",
                 2 * (retry + 1));
        sleep(2 * (retry + 1));
        retry++;
      }
    }while(retry < 3);
    if(_errorExist == false)
    {
      // Attach to the network
      if(ST_OK != app_com_set_lte_con_state(true))
      {
        LOG_WARN(OUTPOINT, "Failed to attach to internet");
        _errorExist = true;
      }
      else
      {
        LOG_DEBUG(OUTPOINT, "Waiting 90 s ...");
        sleep(90);
      }
    }
    if(_errorExist == false)
    {
      // Attach to the network
      if(ST_OK != app_com_set_up_route_for_dhcp(NET_LTE))
      {
        LOG_WARN(OUTPOINT, "Failed to set the route");
        _errorExist = true;
      }
    }
    if(_errorExist == false)
    {
      LOG_INFO(OUTPOINT, "Switch to LTE successfully");
      currentNet = NET_LTE;
    }
    else
    {
      LOG_WARN(OUTPOINT, "Failed to switch to LTE");
      // Disconnect LTE module from internet
      app_com_set_lte_con_state(false);
      // Power off LTE module
      app_com_set_lte_power_supply(false);
      if(isDhcp == true)
      {
        LOG_INFO(OUTPOINT, "DHCP is enabled");
        app_com_set_up_route_for_dhcp(NET_ETH1);
      }
      else
      {
        LOG_INFO(OUTPOINT, "DHCP is disabled");
        LOG_DEBUG(OUTPOINT, "gwIp %s", gwIp);
        app_com_set_up_default_route(NET_ETH1, gwIp);
      }
      currentNet = NET_ETH1;
    }
  }
  else
  {
    LOG_INFO(OUTPOINT, "Switch to ETH1");
    // Disconnect LTE module from internet
    app_com_set_lte_con_state(false);
    // Power off LTE module
    app_com_set_lte_power_supply(false);
    if(isDhcp == true)
    {
      LOG_INFO(OUTPOINT, "DHCP is enabled");
      app_com_set_up_route_for_dhcp(NET_ETH1);
    }
    else
    {
      LOG_INFO(OUTPOINT, "DHCP is disabled");
      LOG_DEBUG(OUTPOINT, "gwIp %s", gwIp);
      app_com_set_up_default_route(NET_ETH1, gwIp);
    }
    currentNet = NET_ETH1;
  }
}



/*to add interface wwan0 to the management of service systemd-networkd
ret@app_state_t
*/
app_state_t app_com_set_wwan0_managed(void)
{
	app_state_t tpret = ST_OK;
	const uint8_t *pt_fpath = "/etc/systemd/network/20-wwan0.network";
	const uint8_t *pt_confinfo = "[Match]\nName=wwan*\n[Network]\nDHCP=no\nIgnoreCarrierLoss=yes\n[Link]\nRequiredForOnline=no\n";
	const uint8_t *pt_cmdstr = "chmod 0644 /etc/systemd/network/20-wwan0.network ;  systemctl restart systemd-networkd";
	int32_t kpfd = -1;

	// to write the configuration file
	kpfd = open( pt_fpath, O_CREAT|O_RDWR, 0644);
	if(kpfd < 0)
	{
		LOG_ERR(OUTPOINT,"fail to open %s, for %s", pt_fpath, strerror(errno));
		tpret = ST_ERR;
		return tpret;
	}
	write(kpfd, pt_confinfo, strlen(pt_confinfo));
	close(kpfd);
	// to restart the service systemd-networkd
	LOG_WARN(OUTPOINT,"to execute: %s", pt_cmdstr);
	if(0 != system(pt_cmdstr))
	{
		LOG_ERR(OUTPOINT,"system fail");
		tpret = ST_ERR;
		return tpret;
	}
	LOG_WARN(OUTPOINT,"wwan0 should be under the management of service systemd-networkd now");
	return tpret;
}



