#!/bin/sh

# going to 

echo "===Going to set the network==="

ifconfig mlan0 up

sleep 1

wpa_supplicant -B -c/etc/wpa_supplicant.conf -imlan0

sleep 1

ifconfig mlan0 192.168.66.66




