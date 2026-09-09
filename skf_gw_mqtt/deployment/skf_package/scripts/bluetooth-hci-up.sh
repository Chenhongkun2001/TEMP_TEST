#!/bin/sh
set -eu
 
retry=0
 
while [ ! -d /sys/class/bluetooth/hci0 ]; do
    retry=$((retry + 1))
 
    if [ "${retry}" -ge 50 ]; then
        echo "hci0 creation timeout"
        exit 1
    fi
 
    sleep 0.2
done
 
command -v rfkill >/dev/null 2>&1 \
&& rfkill unblock bluetooth 2>/dev/null \
    || true
 
retry=0
 
while ! hciconfig hci0 up; do
    retry=$((retry + 1))
 
    if [ "${retry}" -ge 10 ]; then
        echo "failed to bring hci0 UP"
        hciconfig -a || true
        exit 1
    fi
 
    sleep 1
done
 
hciconfig -a
 
exit 0
