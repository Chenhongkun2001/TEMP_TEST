#!/bin/bash
usage() {
    printf "Usage:
    ./skf_gateway_install_check.sh [MAC address],  e.g.,
    ./skf_gateway_install_check.sh C4:BD:6A:12:00:00\n"
}

if [ $# -ne 1 ];
then
    usage
    exit
fi

MACADDR=$1
MACADDRWOCOLON=$(echo ${MACADDR//:/})

cd /home/root/skf_gw_mqtt/
sqlite3 skf.db 'select * from gateway' |grep "mqtt-dp.skf4u.com" | grep $MACADDRWOCOLON
   if [ $? -eq 0 ]; then
       echo “烧写成功”
   else
       echo “烧写不成功”
   fi
cd /home/root/skf_gw_mqtt/v1/
md5sum mcumgr hci_uart.signed.bin mqtt_op sql_op skf_gw_1 skf_gw_mod ../restart_ble.sh ../restart_mqtt.sh ../restart_sql.sh ../restart_mod.sh > ../chksum.md5
md5sum ../chksum.md5