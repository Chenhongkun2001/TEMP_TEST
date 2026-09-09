#include "bt_hcitool.h"
#include "util_dbg.h"
#include "bluetooth.h"
#include "hci.h"
#include "hci_lib.h"
#include <sys/ioctl.h>
#include <stddef.h>

DBG_LOCAL_LOG_DEBUG


struct con_handle_info
{
  bool is_valid;
  uint16_t handle;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
  uint8_t con_id[3];
#endif
};

static struct con_handle_info g_con_handle = { 0 };
// make sure only 1 'instance' is using g_con_handle at the same time.
static bool g_con_handle_flag = false;
#define MAX_HCI_CMD_STATUS_BUF (64U)

struct hci_cmd_status
{
  uint8_t len;
  uint8_t buf[MAX_HCI_CMD_STATUS_BUF];
};

static app_state_t to_send_hci_cmd_v2(
  int dev_id,
  int argc,
  char **argv,
  struct hci_cmd_status *pt_cmd_status);


/**
 * @brief Callback called by @p hci_for_each_dev . This function is
 * borrowed from conn_list in
 * "skf_gw/source/monitor/lib_bluez/bluez/tools/hcitool.c".
 * Once a connection is found, @p g_con_handle.is_valid would be set true,
 * which means we assume that there is only ONE device can be connected at
 * ONE moment.
 * Note that: Only 1 connection is expected (say, gw in server role) when 
 *      con_id is set to [0xff, 0xff, 0xff], 
 *      and API's parameter 'con_id' should be NULL for this case
 */
static int
conn_list(
  int s,
  int dev_id,
  long arg)
{
  struct hci_conn_list_req *cl;
  struct hci_conn_info *ci;
  int id = arg;
  int i;

  if(id != -1 && dev_id != id)
  {
    return 0;
  }

  if(!(cl = malloc(10 * sizeof(*ci) + sizeof(*cl))))
  {
    DBG_LOG_ERR("Can't allocate memory");
    return -1;
  }
  cl->dev_id = dev_id;
  cl->conn_num = 10;
  ci = cl->conn_info;

  if(ioctl(s, HCIGETCONNLIST, (void *)cl))
  {
    DBG_LOG_ERR("Can't get connection list");
    return -1;
  }

  for(i = 0; i < cl->conn_num; i++, ci++)
  {
    char addr[18];

    ba2str(&ci->bdaddr, addr);
    DBG_LOG_DEBUG("The connection handle is %d (%s)", ci->handle, addr);
    // assumes that there is only ONE device can be connected at ONE
    // moment. 
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
    DBG_LOG_DEBUG("id is %02x %02x %02x\n", ci->bdaddr.b[2], ci->bdaddr.b[1], ci->bdaddr.b[0]);
    DBG_LOG_DEBUG("con id is %02x %02x %02x\n", g_con_handle.con_id[0], g_con_handle.con_id[1], g_con_handle.con_id[2]);
	if((g_con_handle.con_id[0] == ci->bdaddr.b[2]) &&
		(g_con_handle.con_id[1] == ci->bdaddr.b[1]) &&
		(g_con_handle.con_id[2] == ci->bdaddr.b[0]))
	{
		DBG_LOG_INFO("The connection handle is found! handle:%d\n", ci->handle);
		g_con_handle.is_valid = true;
		g_con_handle.handle = ci->handle;
		break;
	}
	if((g_con_handle.con_id[0] == 0xff) &&
	   (g_con_handle.con_id[1] == 0xff) &&
	   (g_con_handle.con_id[2] == 0xff))
	{
		DBG_LOG_INFO("The connection handle is found(svr)! handle:%d\n", ci->handle);
		g_con_handle.is_valid = true;
		g_con_handle.handle = ci->handle;
		break;
	}
#else
    g_con_handle.is_valid = true;
    g_con_handle.handle = ci->handle;
#endif
  }

  free(cl);
  return 0;
}
/**
 * @brief Routine for BLE Tx-power modification. For Zephyr-based BLE NIC,
 * please refer to
 * https://github.com/zephyrproject-rtos/zephyr/blob/v3.7.1/doc/connectivity/bluetooth/api/hci.txt.
 * @param htype: which handle needs to be modified (advertising, scanning,
 * or connection).
 * @param plevel: Tx Power (in dBm) to be set. For Bluez, the supported
 * range is from 128 dBm to (-127) dBm. However this range might be
 * hardware specific (e.g., nrf52840 supports from -40 dBm to 8 dBm)
 * @return ST_OK if everything is OK
 */
app_state_t
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
bt_hcitool_set_tx_power(
  const enum bt_vspecific_handle_type htype,
  const int8_t plevel,
  uint8_t *con_id)
#else
bt_hcitool_set_tx_power(
  const enum bt_vspecific_handle_type htype,
  const int8_t plevel)
#endif
{
  app_state_t _ret = ST_OK;
  int32_t _devid = -1;
  char *_hcicmd_str[] = { "0x3F", "0x000E", NULL, NULL, NULL, NULL };
  const int _argc = sizeof(_hcicmd_str) / sizeof(_hcicmd_str[0]);
  struct hci_cmd_status _cmd_status = { 0 };

  char _p_htype[] = "0x00";
  char _h_str_l[] = "0x00";
  char _h_str_m[] = "0x00";
  char _p_level_str[] = "0x00";

  _hcicmd_str[2] = _p_htype;
  _hcicmd_str[3] = _h_str_l;
  _hcicmd_str[4] = _h_str_m;
  _hcicmd_str[5] = _p_level_str; // the power-level string

  switch(htype)
  {
    case BT_HCI_VS_LL_HANDLE_TYPE_ADV:
    {
      snprintf(_p_htype, sizeof(_p_htype), "0x%.02x", htype);
      snprintf(_h_str_l, sizeof(_h_str_l), "0x%.02x", 0);
      snprintf(_h_str_m, sizeof(_h_str_m), "0x%.02x", 0);
      if(plevel >= 0)
      {
        snprintf(_p_level_str, sizeof(_p_level_str), "0x%.02X", plevel);
      }
      else
      {
        snprintf(_p_level_str, sizeof(_p_level_str), "0x%.02X",
                 256 + plevel);
      }
      break;
    }
    case BT_HCI_VS_LL_HANDLE_TYPE_SCAN:
    {
      snprintf(_p_htype, sizeof(_p_htype), "0x%.02x", htype);
      snprintf(_h_str_l, sizeof(_h_str_l), "0x%.02x", 0);
      snprintf(_h_str_m, sizeof(_h_str_m), "0x%.02x", 0);
      if(plevel >= 0)
      {
        snprintf(_p_level_str, sizeof(_p_level_str), "0x%.02X", plevel);
      }
      else
      {
        snprintf(_p_level_str, sizeof(_p_level_str), "0x%.02X",
                 256 + plevel);
      }
      break;
    }
    case BT_HCI_VS_LL_HANDLE_TYPE_CONN:
    {
      snprintf(_p_htype, sizeof(_p_htype), "0x%.02x", htype);
      // To get the connection handle
      _devid = hci_devid("hci0");
      DBG_LOG_DEBUG("Device ID of \"hci0\" is %d", _devid);

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      if(g_con_handle_flag)
      {
        DBG_LOG_WARN("hci handle onging!\n");
        _ret = ST_ERR;
        return _ret;
      }
      else
      {
        g_con_handle_flag = true;
      }
      memset(&g_con_handle, 0, sizeof(struct con_handle_info));
      g_con_handle.is_valid = false;
      // set con_id to NULL for:
      // i: server role;  ii: 'random' con_id
      if(NULL == con_id)
      {
        // manually set con_id to 0xff here
        g_con_handle.con_id[0] = 0xff;
        g_con_handle.con_id[1] = 0xff;
        g_con_handle.con_id[2] = 0xff;
      }
      else if((con_id[0] != 0xc4) ||
              (con_id[1] != 0xbd) ||
              (con_id[2] != 0x6a))
      {
        DBG_LOG_ERR("invalid con_id!\n");
        _ret = ST_ERR;
        g_con_handle_flag = false;
        return _ret;
      }
      else
      {
        // Notes: use the last 3-byte only here
        // eg.: for "c4 bd 6a 11 01 94",
        //     con_id[0] = 0x11
        //     con_id[1] = 0x01
        //     con_id[2] = 0x94
        g_con_handle.con_id[0] = con_id[3];
        g_con_handle.con_id[1] = con_id[4];
        g_con_handle.con_id[2] = con_id[5];
      }
      hci_for_each_dev(HCI_UP, conn_list, _devid);
      if(false == g_con_handle.is_valid)
      {
        DBG_LOG_ERR("Failed to get the connection handle");
        g_con_handle_flag = false;
        _ret = ST_ERR;
        return _ret;
      }
      snprintf(_h_str_l, sizeof(_h_str_l), "0x%.02x",
               g_con_handle.handle & 0xFF);
      snprintf(_h_str_m, sizeof(_h_str_m), "0x%.02x",
               (g_con_handle.handle >> 8) & 0xFF);
      g_con_handle_flag = false;
#else
      memset(&g_con_handle, 0, sizeof(struct con_handle_info));
      g_con_handle.is_valid = false;
      hci_for_each_dev(HCI_UP, conn_list, _devid);
      if(false == g_con_handle.is_valid)
      {
        DBG_LOG_ERR("Failed to get the connection handle");
        _ret = ST_ERR;
        return _ret;
      }
      snprintf(_h_str_l, sizeof(_h_str_l), "0x%.02x",
               g_con_handle.handle & 0xFF);
      snprintf(_h_str_m, sizeof(_h_str_m), "0x%.02x",
               (g_con_handle.handle >> 8) & 0xFF);
#endif
      if(plevel >= 0)
      {
        snprintf(_p_level_str, sizeof(_p_level_str), "0x%.02X", plevel);
      }
      else
      {
        snprintf(_p_level_str, sizeof(_p_level_str), "0x%.02X",
                 256 + plevel);
      }
      break;
    }
    default:
    {
      DBG_LOG_WARN("Unknown handle type");
      _ret = ST_ERR;
      return _ret;
    }
  }

  DBG_LOG_DEBUG("%s %s %s %s %s %s",
                _hcicmd_str[0 % _argc],
                _hcicmd_str[1 % _argc],
                _hcicmd_str[2 % _argc],
                _hcicmd_str[3 % _argc],
                _hcicmd_str[4 % _argc],
                _hcicmd_str[5 % _argc]);

  _ret = to_send_hci_cmd_v2(_devid, _argc, _hcicmd_str,
                            &_cmd_status);
  if(ST_OK != _ret)
  {
    DBG_LOG_ERR("Failed to send HCI cmd");
    _ret = ST_ERR;
    return _ret;
  }
#if 0
  // For bluetoothd v576
  if((0x01 != _cmd_status.buf[0 % MAX_HCI_CMD_STATUS_BUF]) ||
     (0x01 != _cmd_status.buf[1 % MAX_HCI_CMD_STATUS_BUF]) ||
     (0x0E != _cmd_status.buf[2 % MAX_HCI_CMD_STATUS_BUF]) ||
     (0xFC != _cmd_status.buf[3 % MAX_HCI_CMD_STATUS_BUF]))
  {
    DBG_LOG_ERR("Unexpected HCI execution status");
    _ret = ST_ERR;
    return _ret;
  }
#else
  // For bluetoothd v565
  if((0x01 != _cmd_status.buf[0 % MAX_HCI_CMD_STATUS_BUF]) ||
     (0x0E != _cmd_status.buf[1 % MAX_HCI_CMD_STATUS_BUF]) ||
     (0xFC != _cmd_status.buf[2 % MAX_HCI_CMD_STATUS_BUF]))
  {
    DBG_LOG_ERR("Unexpected HCI execution status. stat: 0x%02x 0x%02x 0x%02x \n", _cmd_status.buf[0 % MAX_HCI_CMD_STATUS_BUF], _cmd_status.buf[1 % MAX_HCI_CMD_STATUS_BUF], _cmd_status.buf[2 % MAX_HCI_CMD_STATUS_BUF]);
    _ret = ST_ERR;
    return _ret;
  }
#endif
  return _ret;
}
/**
 * @brief Routine to get current BLE Tx-power. For Zephyr-based BLE NIC,
 * please refer to
 * https://github.com/zephyrproject-rtos/zephyr/blob/v3.7.1/doc/connectivity/bluetooth/api/hci.txt.
 * @param htype: which handle needs to be modified (advertising, scanning,
 * or connection).
 * @param plevel: returned Tx Power (in dBm) to be set.
 * @return ST_OK if everything is OK. The returned Tx Power is only valid
 * when @p ST_OK is returned
 */
app_state_t
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
bt_hcitool_get_tx_power(
  const enum bt_vspecific_handle_type htype,
  int8_t *ptlevel,
  uint8_t *con_id)
#else
bt_hcitool_get_tx_power(
  const enum bt_vspecific_handle_type htype,
  int8_t * ptlevel)
#endif
{
  app_state_t _ret = ST_OK;
  int32_t _devid = -1;
  char *_hcicmd_str[] = { "0x3F", "0x000F", NULL, NULL, NULL };
  const int _argc = sizeof(_hcicmd_str) / sizeof(_hcicmd_str[0]);
  struct hci_cmd_status _cmd_status = { 0 };

  char _p_htype[] = "0x00";
  char _h_str_l[] = "0x00";
  char _h_str_m[] = "0x00";

  _hcicmd_str[2] = _p_htype;
  _hcicmd_str[3] = _h_str_l;
  _hcicmd_str[4] = _h_str_m;

  if(NULL == ptlevel)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  switch(htype)
  {
    case BT_HCI_VS_LL_HANDLE_TYPE_ADV:
    {
      snprintf(_p_htype, sizeof(_p_htype), "0x%.02x", htype);
      snprintf(_h_str_l, sizeof(_h_str_l), "0x%.02x", 0);
      snprintf(_h_str_m, sizeof(_h_str_m), "0x%.02x", 0);
      break;
    }
    case BT_HCI_VS_LL_HANDLE_TYPE_SCAN:
    {
      snprintf(_p_htype, sizeof(_p_htype), "0x%.02x", htype);
      snprintf(_h_str_l, sizeof(_h_str_l), "0x%.02x", 0);
      snprintf(_h_str_m, sizeof(_h_str_m), "0x%.02x", 0);
      break;
    }
    case BT_HCI_VS_LL_HANDLE_TYPE_CONN:
    {
      snprintf(_p_htype, sizeof(_p_htype), "0x%.02x", htype);
      // To get the connection handle
      _devid = hci_devid("hci0");
      DBG_LOG_DEBUG("Device ID of \"hci0\" is %d", _devid);

#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
      if(g_con_handle_flag)
      {
        DBG_LOG_WARN("hci handle onging!\n");
        _ret = ST_ERR;
        return _ret;
      }
      else
      {
        g_con_handle_flag = true;
      }
      memset(&g_con_handle, 0, sizeof(struct con_handle_info));
      g_con_handle.is_valid = false;
      // set con_id to NULL for:
      // i: server role;  ii: 'random' con_id
      if(NULL == con_id)
      {
        // manually set con_id to 0xff here
        g_con_handle.con_id[0] = 0xff;
        g_con_handle.con_id[1] = 0xff;
        g_con_handle.con_id[2] = 0xff;
      }
      else if((con_id[0] != 0xc4) ||
              (con_id[1] != 0xbd) ||
              (con_id[2] != 0x6a))
      {
        DBG_LOG_ERR("invalid con_id!\n");
        _ret = ST_ERR;
        g_con_handle_flag = false;
        return _ret;
      }
      else
      {
        // Notes: use the last 3-byte only here
        // eg.: for "c4 bd 6a 11 01 94",
        //     con_id[0] = 0x11
        //     con_id[1] = 0x01
        //     con_id[2] = 0x94
        g_con_handle.con_id[0] = con_id[3];
        g_con_handle.con_id[1] = con_id[4];
        g_con_handle.con_id[2] = con_id[5];
      }
      hci_for_each_dev(HCI_UP, conn_list, _devid);
      if(false == g_con_handle.is_valid)
      {
        DBG_LOG_ERR("Failed to get the connection handle");
        _ret = ST_ERR;
        g_con_handle_flag = false;
        return _ret;
      }
      snprintf(_h_str_l, sizeof(_h_str_l), "0x%.02x",
               g_con_handle.handle & 0xFF);
      snprintf(_h_str_m, sizeof(_h_str_m), "0x%.02x",
               (g_con_handle.handle >> 8) & 0xFF);
      g_con_handle_flag = false;
#else
      memset(&g_con_handle, 0, sizeof(struct con_handle_info));
      g_con_handle.is_valid = false;
      hci_for_each_dev(HCI_UP, conn_list, _devid);
      if(false == g_con_handle.is_valid)
      {
        DBG_LOG_ERR("Failed to get the connection handle");
        _ret = ST_ERR;
        return _ret;
      }
      snprintf(_h_str_l, sizeof(_h_str_l), "0x%.02x",
               g_con_handle.handle & 0xFF);
      snprintf(_h_str_m, sizeof(_h_str_m), "0x%.02x",
               (g_con_handle.handle >> 8) & 0xFF);
#endif
      break;
    }
    default:
    {
      DBG_LOG_WARN("Unknown handle type");
      _ret = ST_ERR;
      return _ret;
    }
  }

  DBG_LOG_DEBUG("%s %s %s %s %s",
                _hcicmd_str[0 % _argc],
                _hcicmd_str[1 % _argc],
                _hcicmd_str[2 % _argc],
                _hcicmd_str[3 % _argc],
                _hcicmd_str[4 % _argc]);

  _ret = to_send_hci_cmd_v2(_devid, _argc, _hcicmd_str,
                            &_cmd_status);
  if(ST_OK != _ret)
  {
    DBG_LOG_ERR("Failed to send HCI cmd");
    _ret = ST_ERR;
    return _ret;
  }
#if 0
  // For bluetoothd v576
  if((0x01 != _cmd_status.buf[0 % MAX_HCI_CMD_STATUS_BUF]) ||
     (0x01 != _cmd_status.buf[1 % MAX_HCI_CMD_STATUS_BUF]) ||
     (0x0F != _cmd_status.buf[2 % MAX_HCI_CMD_STATUS_BUF]) ||
     (0xFC != _cmd_status.buf[3 % MAX_HCI_CMD_STATUS_BUF]))
  {
    DBG_LOG_ERR("Unexpected HCI execution status");
    _ret = ST_ERR;
    return _ret;
  }
  *ptlevel = _cmd_status.buf[7 % MAX_HCI_CMD_STATUS_BUF];
#else
  // For bluetoothd v565
  if((0x01 != _cmd_status.buf[0 % MAX_HCI_CMD_STATUS_BUF]) ||
     (0x0F != _cmd_status.buf[1 % MAX_HCI_CMD_STATUS_BUF]) ||
     (0xFC != _cmd_status.buf[2 % MAX_HCI_CMD_STATUS_BUF]))
  {
    DBG_LOG_ERR("Unexpected HCI execution status");
    _ret = ST_ERR;
    return _ret;
  }
  *ptlevel = _cmd_status.buf[6 % MAX_HCI_CMD_STATUS_BUF];
#endif

  return _ret;
}
/**
 * @brief Send HCI command to BLE NIC. This function is borrowed from
 * cmd_cmd in source/monitor/lib_bluez/bluez/tools/hcitool.c.
 * @param dev_id Device ID of hcixx (e.g., hci0)
 * @param argc Parameter number of the HIC command
 * @param argv Pointer to the parameters
 * @param pt_cmd_status The status of HCI cmd execution will be returned by
 * this pointer
 * @return ST_OK if everthing is OK. The returned HCI cmd execution status
 * is only valid when @p ST_OK is returned
 */
static app_state_t
to_send_hci_cmd_v2(
  int dev_id,
  int argc,
  char **argv,
  struct hci_cmd_status *pt_cmd_status)
{
  app_state_t _ret = ST_OK;

  unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr = buf;
  struct hci_filter flt;
  hci_event_hdr *hdr;
  int i, opt, len, dd;
  uint16_t ocf;
  uint8_t ogf;

  if(NULL == pt_cmd_status)
  {
    DBG_LOG_ERR("Unexpected NULL");
    _ret = ST_ERR;
    return _ret;
  }

  if(dev_id < 0)
  {
    dev_id = hci_get_route(NULL);
    if(dev_id < 0)
    {
      DBG_LOG_ERR("Device is not available");
      _ret = ST_ERR;
      return _ret;
    }
  }

  errno = 0;
  ogf = strtol(argv[0], NULL, 16);
  ocf = strtol(argv[1], NULL, 16);
  if(errno == ERANGE || (ogf > 0x3f) || (ocf > 0x3ff))
  {
    DBG_LOG_ERR("Invalid CMD string");
    _ret = ST_ERR;
    return _ret;
  }

  for(i = 2, len = 0; i < argc && len < (int)sizeof(buf); i++, len++)
  {
    *ptr++ = (uint8_t)strtol(argv[i], NULL, 16);
  }

  dd = hci_open_dev(dev_id);
  if(dd < 0)
  {
    DBG_LOG_ERR("Failed to open device");
    _ret = ST_ERR;
    goto EXIT;
  }

  /* Setup filter */
  hci_filter_clear(&flt);
  hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
  hci_filter_all_events(&flt);
  if(setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0)
  {
    DBG_LOG_ERR("Failed to setup HCI filter");
    _ret = ST_ERR;
    goto EXIT;
  }

  DBG_LOG_DEBUG("< HCI Command: ogf 0x%02x, ocf 0x%04x, plen %d\n", ogf,
                ocf, len);
  util_dbg_buf_dump(buf, len);

  if(hci_send_cmd(dd, ogf, ocf, len, buf) < 0)
  {
    DBG_LOG_ERR("Failed to send hci cmd");
    _ret = ST_ERR;
    goto EXIT;
  }

  len = read(dd, buf, sizeof(buf));
  if(len < 0)
  {
    DBG_LOG_ERR("Failed to read HCI cmd's response");
    _ret = ST_ERR;
    goto EXIT;
  }

  hdr = (void *)(buf + 1);
  ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
  len -= (1 + HCI_EVENT_HDR_SIZE);

  DBG_LOG_DEBUG("> HCI Event: 0x%02x plen %d\n", hdr->evt, hdr->plen);
  util_dbg_buf_dump(ptr, len);

  if(len > MAX_HCI_CMD_STATUS_BUF)
  {
    pt_cmd_status->len = MAX_HCI_CMD_STATUS_BUF;
    memcpy(pt_cmd_status->buf, ptr, MAX_HCI_CMD_STATUS_BUF);
  }
  else
  {
    pt_cmd_status->len = len;
    memcpy(pt_cmd_status->buf, ptr, len);
  }

  util_dbg_buf_dump(pt_cmd_status->buf, pt_cmd_status->len);

EXIT:
  hci_close_dev(dd);

  return _ret;
}



// TODO: uncleaned

/*to send HCI command
*/
static app_state_t to_send_hci_cmd(int dev_id, int argc, char **argv)
{
	app_state_t tpret = ST_OK;
	
	unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr = buf;
	struct hci_filter flt;
	hci_event_hdr *hdr;
	int i, opt, len, dd;
	uint16_t ocf;
	uint8_t ogf;

	#if(0)
	for_each_opt(opt, cmd_options, NULL) {
		switch (opt) {
		default:
			printf("%s", cmd_help);
			return;
		}
	}
	helper_arg(2, -1, &argc, &argv, cmd_help);
	#endif

	//DBG_LOG_ERR("BKP93");

	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	DBG_LOG_INFO("%s %s", argv[0], argv[1]);

	errno = 0;
	ogf = strtol(argv[0], NULL, 16);
	ocf = strtol(argv[1], NULL, 16);
	if (errno == ERANGE || (ogf > 0x3f) || (ocf > 0x3ff)) {
		//printf("%s", cmd_help);
		DBG_LOG_ERR("invalid CMD string");
		tpret = ST_ERR;
		return tpret;
	}

	//DBG_LOG_ERR("BKP107");
	
	for (i = 2, len = 0; i < argc && len < (int) sizeof(buf); i++, len++)
		*ptr++ = (uint8_t) strtol(argv[i], NULL, 16);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		//perror("Device open failed");
		DBG_LOG_ERR("Device open failed");
		//exit(EXIT_FAILURE);
		tpret = ST_ERR;
		return tpret;
	}

	//DBG_LOG_ERR("BKP121");
	
	/* Setup filter */
	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_all_events(&flt);
	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		//perror("HCI filter setup failed");
		DBG_LOG_ERR("HCI filter setup failed");
		//exit(EXIT_FAILURE);
		tpret = ST_ERR;
		goto EXIT;
	}

	//printf("< HCI Command: ogf 0x%02x, ocf 0x%04x, plen %d\n", ogf, ocf, len);	
	DBG_LOG_INFO("< HCI Command: ogf 0x%02x, ocf 0x%04x, plen %d\n", ogf, ocf, len);
	//hex_dump("  ", 20, buf, len); fflush(stdout);
	util_dbg_buf_dump( buf, len);
	
	if (hci_send_cmd(dd, ogf, ocf, len, buf) < 0) {
		//perror("Send failed");
		DBG_LOG_ERR("hci_send_cmd failed");
		//exit(EXIT_FAILURE);
		tpret = ST_ERR;
		goto EXIT;
	}

	len = read(dd, buf, sizeof(buf));
	if (len < 0) {
		//perror("Read failed");
		DBG_LOG_ERR("Read Failed");
		//exit(EXIT_FAILURE);
		tpret = ST_ERR;
		goto EXIT;
	}

	hdr = (void *)(buf + 1);
	ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
	len -= (1 + HCI_EVENT_HDR_SIZE);

	//printf("> HCI Event: 0x%02x plen %d\n", hdr->evt, hdr->plen);
	DBG_LOG_INFO("> HCI Event: 0x%02x plen %d\n", hdr->evt, hdr->plen);
	//hex_dump("  ", 20, ptr, len); fflush(stdout);
	util_dbg_buf_dump( ptr, len);
	
	EXIT:
		hci_close_dev(dd);
		
		return tpret;
}





/*to set the 
*/
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
app_state_t bt_hcitool_set_bluetooth_phy(uint8_t *con_id)
#else
app_state_t bt_hcitool_set_bluetooth_phy(void)
#endif
{
	app_state_t tpret = ST_OK;
	int argc = 0;
	// command string to set Bluetooth PHY to 1M
	char *kp_hcicmd_str[] = {"0x08", "0x0032", NULL, NULL,"0x0","0x01", "0x01", "0x00", "0x00"};
	argc = sizeof(kp_hcicmd_str)/ sizeof(char*);
	uint8_t h_str_l[0x10] = {0};
	uint8_t h_str_m[0x10] = {0};
	uint8_t tpu8 = 0;

	kp_hcicmd_str[2] = h_str_l; // little endian
	kp_hcicmd_str[3] = h_str_m;
	
	int32_t kp_devid = -1;
	kp_devid = hci_devid("hci0");
	
	DBG_LOG_INFO("the DEV-ID is %d", kp_devid);
	//sleep(1);
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	  if(g_con_handle_flag)
	  {
		DBG_LOG_WARN("hci handle onging!\n");
		tpret = ST_ERR;
		return tpret;
	  } else {
		g_con_handle_flag = true;
	  }
      memset(&g_con_handle, 0, sizeof(struct con_handle_info));
      g_con_handle.is_valid = false;
	  // set con_id to NULL for:
	  // i: server role;  ii: 'random' con_id
	  if(NULL == con_id)
	  {
		// manually set con_id to 0xff here
		g_con_handle.con_id[0] = 0xff;
		g_con_handle.con_id[1] = 0xff;
		g_con_handle.con_id[2] = 0xff;
	  } else if ((con_id[0] != 0xc4) ||
				 (con_id[1] != 0xbd) ||
				 (con_id[2] != 0x6a))
	  {
		DBG_LOG_ERR("invalid con_id!\n");
		tpret = ST_ERR;
		g_con_handle_flag = false;
		return tpret;
	  } else
	  {
		// Notes: use the last 3-byte only here
		// eg.: for "c4 bd 6a 11 01 94",
		//     con_id[0] = 0x11 
		//     con_id[1] = 0x01 
		//     con_id[2] = 0x94 
		g_con_handle.con_id[0] = con_id[3];
		g_con_handle.con_id[1] = con_id[4];
		g_con_handle.con_id[2] = con_id[5];
	  }
	hci_for_each_dev(HCI_UP, conn_list, kp_devid);
	if(false == g_con_handle.is_valid)
	{
		DBG_LOG_ERR("Failed to get the connection handle");
		tpret = ST_ERR;
		g_con_handle_flag = false;
		return tpret;
	}
	snprintf(h_str_l, sizeof(h_str_l), "0x%.02x",
			g_con_handle.handle & 0xFF);
	snprintf(h_str_m, sizeof(h_str_m), "0x%.02x",
			(g_con_handle.handle >> 8) & 0xFF);
	g_con_handle_flag = false;
#else
	memset( &g_con_handle, 0, sizeof(struct con_handle_info));
	g_con_handle.is_valid = false;
	hci_for_each_dev( HCI_UP, conn_list, kp_devid);

	if(false == g_con_handle.is_valid)
	{
		tpret = ST_ERR;
		DBG_LOG_ERR("fail to get the connection handle");
		return tpret;
	}
	tpu8 = g_con_handle.handle & 0xFF;
	snprintf( h_str_l, sizeof(h_str_l),"0x%.02X", tpu8);
	tpu8 = (g_con_handle.handle >> 8) & 0xFF;
	snprintf(h_str_m, sizeof(h_str_m), "0x%.02X", tpu8);
#endif
	DBG_LOG_DEBUG("the HCI command string to set BT PHY");
	for(uint8_t i = 0; i < argc; i++)
	{
		DBG_LOG_DEBUG(" %s", kp_hcicmd_str[i]);
	}
	
	tpret = to_send_hci_cmd( kp_devid, argc, kp_hcicmd_str);

	return tpret;
}


#define MAX_HCICMD_STRBUF_ROW 0x20
#define MAX_HCICMD_STRBUF_COL 0x10
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
/* to update the BT connection parameter
1.modify PHY
2.connection parameter
*/
app_state_t bt_hcitool_set_bluetooth_connection_param(const struct bluetooth_connection_param *con_param, uint8_t *con_id)
{
	app_state_t tpret = ST_OK;
	int argc = 0;

	uint32_t tpu32 = 0;
	float tpfval = 0;

	int32_t kp_devid = -1;
	uint8_t tpu8 = 0;
	// uint8_t _con_id[3] = {0};
	// uint8_t _idx = CONFIG_BLE_CONNECTION_MAX_NUM;
	
	char kp_hcicmd_strbuf[MAX_HCICMD_STRBUF_ROW][MAX_HCICMD_STRBUF_COL] = {0};
	char *pt_hcicmd_str[MAX_HCICMD_STRBUF_ROW] = {0};

	struct bluetooth_connection_param kp_con_param_default = {0};
	struct bluetooth_connection_param *pt_con_param = NULL;

	if(NULL == con_param)
	{
		kp_con_param_default.phy = LE_PHY_1M;

		kp_con_param_default.min_con_interval = 12; ///
		kp_con_param_default.max_con_interval = 12; // 15 mS
		kp_con_param_default.con_latency = 0; // 29;
		kp_con_param_default.supervision_timeout = (1 + 0)*(12*1.25)*2+1;//(1 + 29)*(12*1.25)*2+1;
		kp_con_param_default.min_ce_len = 12 * 2;
		kp_con_param_default.max_ce_len = 0xffff;

		kp_con_param_default.tx_octets = 0x00FB;
		kp_con_param_default.tx_time = 0x4290;

	
		pt_con_param = &kp_con_param_default;
	}
	else
	{
		pt_con_param = con_param;
	}

	// to check if the parameters are valid or not
	if((pt_con_param->min_con_interval < 0x0006)||(pt_con_param->min_con_interval > 0x0C80))
	{
		DBG_LOG_ERR(" invalid min_con_interval value 0x%x", pt_con_param->min_con_interval);
		tpret = ST_ERR;
		return tpret;
	}
	if((pt_con_param->max_con_interval < 0x0006)||(pt_con_param->max_con_interval > 0x0C80))
	{
		DBG_LOG_ERR("invalid max_con_interval value 0x%x", pt_con_param->max_con_interval);
		tpret = ST_ERR;
		return tpret;
	}
	if(pt_con_param->min_con_interval > pt_con_param->max_con_interval)
	{
		DBG_LOG_ERR("invalid con_intervals");
		tpret = ST_ERR;
		return tpret;
	}

	if((pt_con_param->con_latency < 0x0000)||(pt_con_param->con_latency > 0x01F3))
	{
		DBG_LOG_ERR("invalid con_latency value");
		tpret = ST_ERR;
		return tpret;
	}


	tpfval = (1.25f*pt_con_param->max_con_interval);
	tpu32 = (uint32_t)tpfval;
	if((tpfval - tpu32) > 0)
	{
		tpu32 = tpu32 + 1;
	}
	tpu32 = (1 + pt_con_param->con_latency) * tpu32 * 2;
	if((pt_con_param->supervision_timeout < 0x000A)||(pt_con_param->supervision_timeout > 0x0C80))
	{
		DBG_LOG_ERR("invalid supervision_timeout");
		tpret = ST_ERR;
		return tpret;
	}
	if(pt_con_param->supervision_timeout < tpu32)
	{
		DBG_LOG_ERR("supervision_timeout shall be larger than 0x%x", tpu32);
		tpret = ST_ERR;
		return tpret;
	}
	

	if((pt_con_param->min_ce_len < 0x0000)||( pt_con_param->min_ce_len > 0xFFFF))
	{
		DBG_LOG_ERR("invalid min_ce_len");
		tpret = ST_ERR;
		return tpret;
	}
	if((pt_con_param->max_ce_len < 0x0000)||(pt_con_param->max_con_interval > 0xFFFF))
	{
		DBG_LOG_ERR("invalid max_ce_len");
		tpret = ST_ERR;
		return tpret;
	}
	if(pt_con_param->max_ce_len < pt_con_param->min_ce_len)
	{
		DBG_LOG_ERR("invalid CE_len conf");
		tpret = ST_ERR;
		return tpret;
	}
	//to update connection parameters
	
	kp_devid = hci_devid("hci0");
	DBG_LOG_INFO("the DEV-ID is %d", kp_devid);
	//
	if(g_con_handle_flag)
	{
		DBG_LOG_WARN("hci handle onging!\n");
		tpret = ST_ERR;
		return tpret;
	} else {
		g_con_handle_flag = true;
	}
	memset(&g_con_handle, 0, sizeof(struct con_handle_info));
	g_con_handle.is_valid = false;
	// set con_id to NULL for:
	// i: server role;  ii: 'random' con_id
	if(NULL == con_id)
	{
		// manually set con_id to 0xff here
		g_con_handle.con_id[0] = 0xff;
		g_con_handle.con_id[1] = 0xff;
		g_con_handle.con_id[2] = 0xff;
	} else if ((con_id[0] != 0xc4) ||
				(con_id[1] != 0xbd) ||
				(con_id[2] != 0x6a))
	{
		DBG_LOG_ERR("invalid con_id!\n");
		tpret = ST_ERR;
		g_con_handle_flag = false;
		return tpret;
	} else
	{
		// Notes: use the last 3-byte only here
		// eg.: for "c4 bd 6a 11 01 94",
		//     con_id[0] = 0x11 
		//     con_id[1] = 0x01 
		//     con_id[2] = 0x94 
		g_con_handle.con_id[0] = con_id[3];
		g_con_handle.con_id[1] = con_id[4];
		g_con_handle.con_id[2] = con_id[5];
	}
	// g_con_handle.con_id[0] = _con_id[0];
	// g_con_handle.con_id[1] = _con_id[1];
	// g_con_handle.con_id[2] = _con_id[2];
	hci_for_each_dev( HCI_UP, conn_list, kp_devid);

	if(false == g_con_handle.is_valid)
	{
		DBG_LOG_ERR("fail to get the connection handle.\n");
		tpret = ST_ERR;
		g_con_handle_flag = false;
		return tpret;
	}

	// to update connection parameter like con_interval, le_len, con_lentancy....
	memset( kp_hcicmd_strbuf, 0, sizeof(kp_hcicmd_strbuf));
	memset( pt_hcicmd_str, 0, sizeof(pt_hcicmd_str));
	argc = 0;

	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x08");
	argc ++;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x0013");
	argc ++;
	//handle
	tpu8 = g_con_handle.handle & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (g_con_handle.handle >> 8)&0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	// make sure set this flag to false once we don't use g_con_handle any more
	g_con_handle_flag = false;
	//min_con_interval
	tpu8 = pt_con_param->min_con_interval & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->min_con_interval >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	//DBG_LOG_ERR("BKP495");
	//max_con_interval
	tpu8 = pt_con_param->max_con_interval & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->max_con_interval >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	// con_latency
	tpu8 = pt_con_param->con_latency & 0xFF;
	snprintf(kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->con_latency >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	//DBG_LOG_ERR("BKP510");
	//supervision_timeout
	tpu8 = pt_con_param->supervision_timeout & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->supervision_timeout >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	// min_ce_len
	tpu8 = pt_con_param->min_ce_len & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->min_ce_len >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	// max_ce_len
	tpu8 = pt_con_param->max_ce_len & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->max_ce_len >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;	

	DBG_LOG_DEBUG("the HCI cmd string to set BT PHY");
	for(uint32_t i = 0; i < argc; i++)
	{
		DBG_LOG_DEBUG("%s", kp_hcicmd_strbuf[i % MAX_HCICMD_STRBUF_ROW]);
		pt_hcicmd_str[i % MAX_HCICMD_STRBUF_ROW] = kp_hcicmd_strbuf[i % MAX_HCICMD_STRBUF_ROW];
	}
	tpret = to_send_hci_cmd( kp_devid, argc, pt_hcicmd_str);
	if(ST_OK != tpret)
	{
		DBG_LOG_ERR("fail to update connection parameters");
		tpret = ST_ERR;
		return tpret;
	}
	return tpret;
}
#else
/* to update the BT connection parameter
1.modify PHY
2.connection parameter
*/
app_state_t bt_hcitool_set_bluetooth_connection_param(const struct bluetooth_connection_param *con_param)
{
	app_state_t tpret = ST_OK;
	int argc = 0;

	uint32_t tpu32 = 0;
	float tpfval = 0;

	int32_t kp_devid = -1;
	uint8_t tpu8 = 0;
	
	char kp_hcicmd_strbuf[MAX_HCICMD_STRBUF_ROW][MAX_HCICMD_STRBUF_COL] = {0};
	char *pt_hcicmd_str[MAX_HCICMD_STRBUF_ROW] = {0};

	struct bluetooth_connection_param kp_con_param_default = {0};
	struct bluetooth_connection_param *pt_con_param = NULL;

	if(NULL == con_param)
	{
		kp_con_param_default.phy = LE_PHY_1M;

		kp_con_param_default.min_con_interval = 12; ///
		kp_con_param_default.max_con_interval = 12; // 15 mS
		kp_con_param_default.con_latency = 0; // 29;
		kp_con_param_default.supervision_timeout = (1 + 0)*(12*1.25)*2+1;//(1 + 29)*(12*1.25)*2+1;
		kp_con_param_default.min_ce_len = 12 * 2;
		kp_con_param_default.max_ce_len = 0xffff;

		kp_con_param_default.tx_octets = 0x00FB;
		kp_con_param_default.tx_time = 0x4290;

	
		pt_con_param = &kp_con_param_default;
	}
	else
	{
		pt_con_param = con_param;
	}


	// to check if the parameters are valid or not
	if((pt_con_param->min_con_interval < 0x0006)||(pt_con_param->min_con_interval > 0x0C80))
	{
		DBG_LOG_ERR(" invalid min_con_interval value 0x%x", pt_con_param->min_con_interval);
		tpret = ST_ERR;
		return tpret;
	}
	if((pt_con_param->max_con_interval < 0x0006)||(pt_con_param->max_con_interval > 0x0C80))
	{
		DBG_LOG_ERR("invalid max_con_interval value 0x%x", pt_con_param->max_con_interval);
		tpret = ST_ERR;
		return tpret;
	}
	if(pt_con_param->min_con_interval > pt_con_param->max_con_interval)
	{
		DBG_LOG_ERR("invalid con_intervals");
		tpret = ST_ERR;
		return tpret;
	}

	if((pt_con_param->con_latency < 0x0000)||(pt_con_param->con_latency > 0x01F3))
	{
		DBG_LOG_ERR("invalid con_latency value");
		tpret = ST_ERR;
		return tpret;
	}


	tpfval = (1.25f*pt_con_param->max_con_interval);
	tpu32 = (uint32_t)tpfval;
	if((tpfval - tpu32) > 0)
	{
		tpu32 = tpu32 + 1;
	}
	tpu32 = (1 + pt_con_param->con_latency) * tpu32 * 2;
	if((pt_con_param->supervision_timeout < 0x000A)||(pt_con_param->supervision_timeout > 0x0C80))
	{
		DBG_LOG_ERR("invalid supervision_timeout");
		tpret = ST_ERR;
		return tpret;
	}
	if(pt_con_param->supervision_timeout < tpu32)
	{
		DBG_LOG_ERR("supervision_timeout shall be larger than 0x%x", tpu32);
		tpret = ST_ERR;
		return tpret;
	}
	

	if((pt_con_param->min_ce_len < 0x0000)||( pt_con_param->min_ce_len > 0xFFFF))
	{
		DBG_LOG_ERR("invalid min_ce_len");
		tpret = ST_ERR;
		return tpret;
	}
	if((pt_con_param->max_ce_len < 0x0000)||(pt_con_param->max_con_interval > 0xFFFF))
	{
		DBG_LOG_ERR("invalid max_ce_len");
		tpret = ST_ERR;
		return tpret;
	}
	if(pt_con_param->max_ce_len < pt_con_param->min_ce_len)
	{
		DBG_LOG_ERR("invalid CE_len conf");
		tpret = ST_ERR;
		return tpret;
	}

	// data length
#if 0
	if((pt_con_param->tx_octets < 0x001B)||(pt_con_param->tx_octets > 0x00FB))
	{
		DBG_LOG_ERR("invalid tx_octets");
		tpret = ST_ERR;
		return tpret;
	}
	if((pt_con_param->tx_time < 0x0148)||(pt_con_param->tx_time > 0x4290))
	{
		DBG_LOG_ERR("invalid tx_time");
		tpret = ST_ERR;
		return tpret;
	}
#endif

	#if(0)
		// command string to set Bluetooth PHY to 1M
		char *kp_hcicmd_str[] = {"0x08", "0x0032", NULL, NULL,"0x0","0x01", "0x01", "0x00", "0x00"};
		argc = sizeof(kp_hcicmd_str)/ sizeof(char*);
		uint8_t h_str_l[0x10] = {0};
		uint8_t h_str_m[0x10] = {0};
		uint8_t tpu8 = 0;

		kp_hcicmd_str[2] = h_str_l; // little endian
		kp_hcicmd_str[3] = h_str_m;
		
		int32_t kp_devid = -1;
		kp_devid = hci_devid("hci0");
		
		DBG_LOG_WARN("the DEV-ID is %d", kp_devid);
		//sleep(1);
		memset( &g_con_handle, 0, sizeof(struct con_handle_info));
		g_con_handle.is_valid = false;
		hci_for_each_dev( HCI_UP, conn_list, kp_devid);

		if(false == g_con_handle.is_valid)
		{
			tpret = ST_ERR;
			DBG_LOG_ERR("fail to get the connection handle");
			return tpret;
		}
		tpu8 = g_con_handle.handle & 0xFF;
		snprintf( h_str_l, sizeof(h_str_l),"0x%.02X", tpu8);
		tpu8 = (g_con_handle.handle >> 8) & 0xFF;
		snprintf(h_str_m, sizeof(h_str_m), "0x%.02X", tpu8);

		DBG_LOG_ERR("the HCI command string to set BT PHY");
		for(uint8_t i = 0; i < argc; i++)
		{
			DBG_LOG_WARN(" %s", kp_hcicmd_str[i]);
		}
		
		tpret = to_send_hci_cmd( kp_devid, argc, kp_hcicmd_str);
		if(tpret != ST_OK)
		{
			return tpret;
		}
	#endif
	//to update connection parameters
	
	kp_devid = hci_devid("hci0");
	DBG_LOG_INFO("the DEV-ID is %d", kp_devid);
	//
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	memset( &g_con_handle[0], 0, sizeof(struct con_handle_info));
	g_con_handle[0].is_valid = false;
	hci_for_each_dev( HCI_UP, conn_list, kp_devid);

	if(false == g_con_handle[0].is_valid)
	{
		DBG_LOG_ERR("fail to get the connection handle");
		tpret = ST_ERR;
		return tpret;
	}
#else
	memset( &g_con_handle, 0, sizeof(struct con_handle_info));
	g_con_handle.is_valid = false;
	hci_for_each_dev( HCI_UP, conn_list, kp_devid);

	if(false == g_con_handle.is_valid)
	{
		DBG_LOG_ERR("fail to get the connection handle");
		tpret = ST_ERR;
		return tpret;
	}
#endif
// 	// now to full fill the the CMD string
// 	memset( kp_hcicmd_strbuf, 0, sizeof(kp_hcicmd_strbuf));
// 	memset( pt_hcicmd_str, 0, sizeof(pt_hcicmd_str));
// 	argc = 0;

// 	snprintf(kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x08");
// 	argc++;

// 	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x0032");
// 	argc++;

// 	tpu8 = g_con_handle.handle & 0xFF;
// 	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
// 	argc ++;
// 	tpu8 = (g_con_handle.handle >> 8) & 0xFF;
// 	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
// 	argc ++;
	
// 	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x00");
// 	argc ++;

// 	if(LE_PHY_1M == pt_con_param->phy)
// 	{
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x01");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x01");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x00");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x00");
// 		argc ++;
// 	}
// 	else if(LE_PHY_2M == pt_con_param->phy)
// 	{
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x02");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x02");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x00");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x00");
// 		argc ++;
// 	}
// #if 0 // Not supported for the current BLE module
// 	else if(LE_PHY_CODED_S2 == pt_con_param->phy)
// 	{
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x04");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x04");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x00");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x01");
// 		argc ++;
// 	}
// 	else if(LE_PHY_CODED_S8 == pt_con_param->phy)
// 	{
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x04");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x04");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x00");
// 		argc ++;
// 		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x02");
// 		argc ++;
// 	}
// #endif
// 	else
// 	{
// 		DBG_LOG_ERR("unknown PHY info");
// 		tpret = ST_ERR;
// 		return tpret;
// 	}

// 	DBG_LOG_WARN("the HCI cmd string to set BT PHY");
// 	for(uint32_t i = 0; i < argc; i++)
// 	{
// 		DBG_LOG_WARN("%s", kp_hcicmd_strbuf[i % MAX_HCICMD_STRBUF_ROW]);
// 		pt_hcicmd_str[i % MAX_HCICMD_STRBUF_ROW] = kp_hcicmd_strbuf[i % MAX_HCICMD_STRBUF_ROW];
// 	}
// 	tpret = to_send_hci_cmd( kp_devid,  argc, pt_hcicmd_str);
// 	if(ST_OK != tpret)
// 	{
// 		DBG_LOG_ERR("fail to set LE PHY");
// 		tpret = ST_ERR;
// 		return tpret;
// 	}

	//DBG_LOG_ERR("BKP471");

	// to update connection parameter like con_interval, le_len, con_lentancy....
	memset( kp_hcicmd_strbuf, 0, sizeof(kp_hcicmd_strbuf));
	memset( pt_hcicmd_str, 0, sizeof(pt_hcicmd_str));
	argc = 0;

	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x08");
	argc ++;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x0013");
	argc ++;
#if (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
	//handle
	tpu8 = g_con_handle[0].handle & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (g_con_handle[0].handle >> 8)&0xFF;
#else
	//handle
	tpu8 = g_con_handle.handle & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (g_con_handle.handle >> 8)&0xFF;
#endif
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	//min_con_interval
	tpu8 = pt_con_param->min_con_interval & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->min_con_interval >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	//DBG_LOG_ERR("BKP495");
	//max_con_interval
	tpu8 = pt_con_param->max_con_interval & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->max_con_interval >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	// con_latency
	tpu8 = pt_con_param->con_latency & 0xFF;
	snprintf(kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->con_latency >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	//DBG_LOG_ERR("BKP510");
	//supervision_timeout
	tpu8 = pt_con_param->supervision_timeout & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->supervision_timeout >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	// min_ce_len
	tpu8 = pt_con_param->min_ce_len & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->min_ce_len >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	// max_ce_len
	tpu8 = pt_con_param->max_ce_len & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;
	tpu8 = (pt_con_param->max_ce_len >> 8) & 0xFF;
	snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
	argc ++;	

	DBG_LOG_DEBUG("the HCI cmd string to set BT PHY");
	for(uint32_t i = 0; i < argc; i++)
	{
		DBG_LOG_DEBUG("%s", kp_hcicmd_strbuf[i % MAX_HCICMD_STRBUF_ROW]);
		pt_hcicmd_str[i % MAX_HCICMD_STRBUF_ROW] = kp_hcicmd_strbuf[i % MAX_HCICMD_STRBUF_ROW];
	}
	tpret = to_send_hci_cmd( kp_devid, argc, pt_hcicmd_str);
	if(ST_OK != tpret)
	{
		DBG_LOG_ERR("fail to update connection parameters");
		tpret = ST_ERR;
		return tpret;
	}

	#if(0)
		// to set DATA length
		memset( kp_hcicmd_strbuf, 0, sizeof(kp_hcicmd_strbuf));
		memset( pt_hcicmd_str, 0, sizeof(pt_hcicmd_str));
		argc = 0;

		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x08");
		argc ++;

		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "%s", "0x0022");
		argc ++;

		// connection handle
		tpu8 = g_con_handle.handle & 0xFF;
		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
		argc ++;
		tpu8 = (g_con_handle.handle >> 8 ) & 0xFF;
		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
		argc ++;
		//tx_octets
		tpu8 = pt_con_param->tx_octets & 0xFF;
		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
		argc++;
		tpu8 = (pt_con_param->tx_octets >> 8) & 0xFF;
		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_COL, "0x%.02X", tpu8);
		argc ++;
		// tx_time
		tpu8 = pt_con_param->tx_time & 0xFF;
		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_ROW, "0x%.02X", tpu8);
		argc ++;
		tpu8 = (pt_con_param->tx_time >> 8) & 0xFF;
		snprintf( kp_hcicmd_strbuf[argc % MAX_HCICMD_STRBUF_ROW], MAX_HCICMD_STRBUF_ROW, "0x%.02X", tpu8);
		argc ++;

		DBG_LOG_WARN("the HCI command str to set DATA len");
		for(uint32_t i = 0; i < argc; i++)
		{
			DBG_LOG_WARN("%s", kp_hcicmd_strbuf[i % MAX_HCICMD_STRBUF_ROW]);
			pt_hcicmd_str[i % MAX_HCICMD_STRBUF_ROW] = kp_hcicmd_strbuf[i % MAX_HCICMD_STRBUF_ROW];
		}
		tpret = to_send_hci_cmd( kp_devid, argc, pt_hcicmd_str);
		if(tpret != ST_OK)
		{
			DBG_LOG_ERR("fail to set DATA length");
		}
	#endif

	
	return tpret;
}
#endif	//end of (SKF_GW_CONFIG_MULTI_CONNECTION == 1)
