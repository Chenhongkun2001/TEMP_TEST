#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
 
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

#define SKF_NET_REQ_DIR \
  "/run/skf-gateway/network-recovery/requests"
 
#define SKF_NET_RES_DIR \
  "/run/skf-gateway/network-recovery/results"
 
 
static app_state_t
app_net_priv_request(
  const char *op,
  int arg1,
  const char *arg2)
{
  static uint32_t seq = 0;
 
  struct timespec ts;
 
  char token[96] = { 0 };
  char req_tmp[256] = { 0 };
  char req_path[256] = { 0 };
  char res_path[256] = { 0 };
 
  int fd = -1;
 
  if(op == NULL)
  {
    return ST_ERR;
  }
 
  clock_gettime(
    CLOCK_MONOTONIC,
&ts);
 
  snprintf(
    token,
    sizeof(token),
    "%ld-%ld-%ld-%u",
    (long)getpid(),
    (long)ts.tv_sec,
    (long)ts.tv_nsec,
    ++seq);
 
  snprintf(
    req_tmp,
    sizeof(req_tmp),
    "%s/%s.tmp",
    SKF_NET_REQ_DIR,
    token);
 
  snprintf(
    req_path,
    sizeof(req_path),
    "%s/%s.req",
    SKF_NET_REQ_DIR,
    token);
 
  snprintf(
    res_path,
    sizeof(res_path),
    "%s/%s.result",
    SKF_NET_RES_DIR,
    token);
 
  fd = open(
    req_tmp,
    O_WRONLY |
    O_CREAT |
    O_EXCL |
    O_CLOEXEC,
    0600);
 
  if(fd < 0)
  {
    LOG_ERR(
      OUTPOINT,
      "NET_PRIV request open failed: %s",
      strerror(errno));
 
    return ST_ERR;
  }
 
  if(dprintf(
       fd,
       "%s %d %s\n",
       op,
       arg1,
       (arg2 != NULL) ? arg2 : "-") <= 0)
  {
    close(fd);
    unlink(req_tmp);
 
    LOG_ERR(
      OUTPOINT,
      "NET_PRIV request write failed");
 
    return ST_ERR;
  }
 
  fsync(fd);
  close(fd);
 
  /*
   * Publish atomically so the path unit never sees
   * an incomplete request.
   */
  if(rename(req_tmp, req_path) != 0)
  {
    unlink(req_tmp);
 
    LOG_ERR(
      OUTPOINT,
      "NET_PRIV request rename failed: %s",
      strerror(errno));
 
    return ST_ERR;
  }
 
  /*
   * Preserve the old synchronous call semantics.
   * This does not alter toggleNetwork() retry/sleep logic.
   */
  for(int i = 0; i < 1200; i++)
  {
    FILE *fp = fopen(
      res_path,
      "r");
 
    if(fp != NULL)
    {
      int rc = -1;
 
      if(fscanf(
           fp,
           "%d",
&rc) == 1)
      {
        fclose(fp);
        unlink(res_path);
 
        if(rc == 0)
        {
          return ST_OK;
        }
 
        LOG_ERR(
          OUTPOINT,
          "NET_PRIV op %s failed rc=%d",
          op,
          rc);
 
        return ST_ERR;
      }
 
      fclose(fp);
    }
 
    usleep(100000);
  }
 
  unlink(req_path);
 
  LOG_ERR(
    OUTPOINT,
    "NET_PRIV op %s timeout",
    op);
 
  return ST_ERR;
}

/**
 * @brief Initialize the GPIO for LTE module control
 * @return @p ST_OK if things go well
 */
app_state_t
app_com_init_gpio_for_lte_ctr(void)
{
  return app_net_priv_request(
    "gpio_init",
    0,
    NULL);
}

/**
* @brief Power on/off the LTE module.
*
* The original synchronous call semantics are preserved.
* Only the privileged GPIO operation is delegated to the
* root network-recovery helper.
*
* @param nstate true to power on; false to power off.
* @return ST_OK on success, ST_ERR on failure.
*/

app_state_t
app_com_set_lte_power_supply(
  const bool nstate)
{
  return app_net_priv_request(
    "lte_power",
    nstate ? 1 : 0,
    NULL);
}
 
/**
 * @brief Power on/off the LTE module
 * @param nstate: true to power on the LTE module; false to power off the
 * LTE module.
 * @return @p ST_OK if things go well
 */
app_state_t
app_com_set_lte_con_state(
  const bool nstate)
{
  if(true !=
     app_com_is_lte_module_attached())
  {
    LOG_ERR(
      OUTPOINT,
      "The LTE module has not been attached yet");
 
    return ST_ERR;
  }
 
  return app_net_priv_request(
    "lte_connect",
    nstate ? 1 : 0,
    NULL);
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
 * @brief Setup the route for eth0/eth1/wwan/wifi
 * @param net: To assign which network requiring to get/set the gateway IP
 * automatically
 * @return @p ST_OK if things go well
 */
app_state_t
app_com_set_up_route_for_dhcp(
  const enum net_id net)
{
  return app_net_priv_request(
    "dhcp",
    (int)net,
    NULL);
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
  if(pt_gwip == NULL)
  {
    LOG_ERR(
      OUTPOINT,
      "unexpected NULL");
 
    return ST_ERR;
  }
 
  if(strlen(pt_gwip) < 7)
  {
    LOG_ERR(
      OUTPOINT,
      "invalid GW IP str");
 
    return ST_ERR;
  }
 
  return app_net_priv_request(
    "default_route",
    (int)net,
    pt_gwip);
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
app_state_t
app_com_set_wwan0_managed(void)
{
  return app_net_priv_request(
    "wwan_managed",
    0,
    NULL);
}



