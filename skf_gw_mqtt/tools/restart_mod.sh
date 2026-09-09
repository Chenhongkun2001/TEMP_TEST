#!/bin/bash
APP_PATH=/home/root/skf_gw_mqtt/v1/skf_gw_mod
log=/home/root/skf_gw_mqtt/log
pgrep skf_gw_mod | xargs kill -s 9
if [ -f "$APP_PATH" ];then
    chmod 777 $APP_PATH
else
    echo "No file: $APP_PATH" 
    exit 1
fi
while true; do
    counter=$(pgrep -xc "skf_gw_mod")
    if [ $counter -ne 1 ]; then
        pgrep skf_gw_mod | xargs kill -s 9
        #$APP_PATH >$log/sql_$(date +%Y%m%d)_$(date +%H%M%S).log 2>&1 &
        $APP_PATH >>$log/modbus.log 2>&1 &
        sleep 9
    fi
    sleep 1
done