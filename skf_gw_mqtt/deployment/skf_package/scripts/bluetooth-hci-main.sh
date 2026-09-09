#!/bin/sh
set -eu
 
. /etc/skf-gateway/bluetooth.env
 
exec hciattach \
    -n \
    "${BLUETOOTH_UART}" \
    any \
    -s 115200 \
    115200 \
    flow
