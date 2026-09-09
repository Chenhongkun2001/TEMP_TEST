#!/bin/bash
cd /home/root/skf_gw_mqtt
./restart_ble.sh &
sleep 1
./restart_mqtt.sh &
sleep 1
./restart_sql.sh &
sleep 1
./restart_mod.sh &
exit 0