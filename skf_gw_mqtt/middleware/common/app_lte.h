#ifndef __APP_LTE_H__
#define __APP_LTE_H__

// The LTE module USB infos
#ifndef LTE_USB_IVENDOR
#define LTE_USB_IVENDOR (0x2C7C)
#endif
#ifndef LTE_USB_IPRODUCT
#define LTE_USB_IPRODUCT (0x0125)
#endif
// The commands of the LTE module
#define CMD_STR_LAUNCH_QCM \
        "tpid=`pidof quectel-CM` \n echo \"${tpid}\" \n if [ \"${tpid}\" != \"\" ] \n then \n echo \"quectel-CM is running\" \n else \n /usr/bin/quectel-CM & \n fi"
#define CMD_STR_KILL_QCM \
        "tpid=`pidof quectel-CM` \n echo \"${tpid}\" \n if [ \"${tpid}\" != \"\" ] \n then \n killall -s 9 quectel-CM \n else \n echo \"no quectel-CM to be killed\" \n fi"

// The commands to get gateway IP
#define CMD_STR_GET_WWAN_GWIP \
        "tpoutput=/tmp/udhcpc_output; tpscript=\"/tmp/udhcpc.sh\"; echo \" udhcpc -f -n -q -t 5 -i wwan0 2>&1\" > $tpscript; sh $tpscript > $tpoutput; sed -nr \'/server/ s/.*server([^\"]+).*/\\1/p\' $tpoutput"
#define CMD_STR_GET_ETH0_GWIP \
        "tpoutput=/tmp/udhcpc_output; tpscript=\"/tmp/udhcpc.sh\"; echo \" udhcpc -f -n -q -t 5 -i eth0 2>&1\" > $tpscript; sh $tpscript > $tpoutput; sed -nr \'/server/ s/.*server([^\"]+).*/\\1/p\' $tpoutput"
#define CMD_STR_GET_ETH1_GWIP \
        "tpoutput=/tmp/udhcpc_output; tpscript=\"/tmp/udhcpc.sh\"; echo \" udhcpc -f -n -q -t 5 -i eth1 2>&1\" > $tpscript; sh $tpscript > $tpoutput; sed -nr \'/server/ s/.*server([^\"]+).*/\\1/p\' $tpoutput"
#define CMD_STR_GET_WIFI_GWIP \
        "tpoutput=/tmp/udhcpc_output; tpscript=\"/tmp/udhcpc.sh\"; echo \" udhcpc -f -n -q -t 5 -i mlan0 2>&1\" > $tpscript; sh $tpscript > $tpoutput; sed -nr \'/server/ s/.*server([^\"]+).*/\\1/p\' $tpoutput"

// Netowrk interface names
#ifndef NET_ETH0_INAME
#define NET_ETH0_INAME "eth0"
#endif
#ifndef NET_ETH1_INAME
#define NET_ETH1_INAME "eth1"
#endif
#ifndef NET_LTE_INAME
#define NET_LTE_INAME "wwan0"
#endif
#ifndef NET_WIFI_INAME
#define NET_WIFI_INAME "mlan0"
#endif

extern bool isDhcp;
extern char gwIp[20];

enum net_id
{
  NET_UNKNOWN = 0,
  NET_ETH0 = 1,
  NET_ETH1 = 2,
  NET_LTE = 3,
  NET_WIFI = 4,
};

app_state_t app_com_init_gpio_for_lte_ctr(void);
app_state_t app_com_set_lte_power_supply(const bool nstate);
bool app_com_is_lte_module_attached(void);
app_state_t app_com_set_up_route_for_dhcp(const enum net_id net);
app_state_t app_com_set_up_default_route(
  const enum net_id net,
  const char *pt_gwip);
void toggleNetwork(void);


app_state_t app_com_set_wwan0_managed(void);



#endif /* __APP_LTE_H__ */
