#!/bin/bash

APP_PATH=/home/root/skf_gw_mqtt/mqtt_op

while true; do
    if pgrep -x "mqtt_op" > /dev/null; then
        sleep 1s
    else
    	echo "to restart mqtt application"
	pgrep mqtt_op | xargs kill -s 9
        $APP_PATH >log_mqtt_$(date +%Y%m%d)_$(date +%H%M%S).log 2>&1 &
    fi
done
