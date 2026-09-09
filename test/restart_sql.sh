#!/bin/bash

APP_PATH=/home/root/skf_gw_mqtt/sql_op

while true; do
    if pgrep -x "sql_op" > /dev/null; then
        sleep 1s
    else
	echo "to restart sql application"
    	pgrep sql_op | xargs kill -s 9
        $APP_PATH >log_sql_$(date +%Y%m%d)_$(date +%H%M%S).log 2>&1 &
    fi
done
