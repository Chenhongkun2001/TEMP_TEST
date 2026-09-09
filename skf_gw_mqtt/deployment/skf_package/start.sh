#!/bin/bash
cd /home/root/skf_gw_mqtt
./restart_mqtt.sh &
sleep 1
./restart_sql.sh &
sleep 1
./restart_ble.sh &
exit 0
