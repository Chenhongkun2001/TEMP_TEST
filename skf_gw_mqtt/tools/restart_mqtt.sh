#!/bin/bash
APP_PATH=/home/root/skf_gw_mqtt/v1/mqtt_op
log=/home/root/skf_gw_mqtt/log
pgrep mqtt_op | xargs kill -s 9
if [ -f "$APP_PATH" ];then
    chmod 777 $APP_PATH
else
    echo "No file: $APP_PATH"
    exit 1
fi
while true; do
    counter=$(pgrep -xc "mqtt_op")
    if [ $counter -ne 1 ]; then
        pgrep mqtt_op | xargs kill -s 9
        pgrep sql_op  | xargs kill -s 9
        sleep 30
        #$APP_PATH >$log/mqtt_$(date +%Y%m%d)_$(date +%H%M%S).log 2>&1 &
        $APP_PATH >>$log/mqtt.log 2>&1 &
        sleep 9
    fi
    sleep 1
done
