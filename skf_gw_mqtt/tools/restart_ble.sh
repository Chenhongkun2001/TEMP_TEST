#!/bin/bash
 
APP_PATH=/home/root/skf_gw_mqtt/v1/skf_gw_1
log=/home/root/skf_gw_mqtt/log
pgrep skf_gw_1 | xargs kill -s 9
if [ -f "$APP_PATH" ];then
    chmod 777 $APP_PATH
else
    echo "No file: $APP_PATH" 
    exit 1
fi
while true; do
    counter=$(pgrep -xc "skf_gw_1")
    if [ $counter -ne 1 ]; then
        pgrep skf_gw_1 | xargs kill -s 9
        echo "stop and start bluetooth"
        /etc/init.d/bluetooth stop
        /etc/init.d/bluetooth start
        $APP_PATH > $log/ble_$(date +%Y%m%d)_$(date +%H%M%S).log 2>&1 &
        sleep 9
    fi
    sleep 1
done
