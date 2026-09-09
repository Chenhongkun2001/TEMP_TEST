#!/bin/bash

APP_PATH=/home/root/skf_gw_mqtt/v1/skf_gw_1
log=/home/root/skf_gw_mqtt/log

# Kill any running skf_gw_1 process first to avoid system reboot
pgrep skf_gw_1 | xargs kill -s 9

# Prepare for the lib upgrade
if [ -f "/home/root/down/gateway/libs.tar.gz" ]; then
if [ -f "/home/root/down/gateway/libell-private.a" ]; then
  mv /home/root/down/gateway/libell-private.a /usr/lib/libell-private.a
fi
if [ -f "/home/root/down/gateway/libell-private.la" ]; then
  mv /home/root/down/gateway/libell-private.la /usr/lib/libell-private.la
fi
if [ -f "/home/root/down/gateway/libell.la" ]; then
  mv /home/root/down/gateway/libell.la /usr/lib/libell.la
fi
if [ -f "/home/root/down/gateway/libell.lai" ]; then
  mv /home/root/down/gateway/libell.lai /usr/lib/libell.lai
fi
if [ -f "/home/root/down/gateway/libell.so" ]; then
  mv /home/root/down/gateway/libell.so /usr/lib/libell.so
fi
if [ -f "/home/root/down/gateway/libell.so.0" ]; then
  mv /home/root/down/gateway/libell.so.0 /usr/lib/libell.so.0
fi
if [ -f "/home/root/down/gateway/libell.so.0.0.2" ]; then
  mv /home/root/down/gateway/libell.so.0.0.2 /usr/lib/libell.so.0.0.2
fi
fi

# Update the autorun.sh and firmware if nrf52840-NIC is used
if [ -f "/home/root/skf_gw_mqtt/nrf52840nic" ]; then
# Disable the autorun.sh
sudo systemctl disable autorun.service
# Update autorun.sh 
SCRIPT="/etc/autorun.sh"
if [ -f "$SCRIPT" ]; then
  # Check whether patched (in an ugly but simple way)
  if grep -q '^[[:space:]]*sleep[[:space:]]\+23' "$SCRIPT" &&
  [ $(grep -c '^[[:space:]]*rfkill unblock bluetooth' "$SCRIPT") -ge 2 ]; then
echo "The patch has been applied and nothing to do ..."
  else
# 1. sleep 10 -> sleep 23 (before "#Start Bluetooth")
TMP=$(mktemp)
awk '
/^[[:space:]]*#[[:space:]]*Start Bluetooth/ {
 in_bluetooth = 1
}
!in_bluetooth && /^[[:space:]]*sleep[[:space:]]+10[[:space:]]*$/ {
 print "sleep 23"
 next
}
{ print }
' "$SCRIPT" > "$TMP" && mv "$TMP" "$SCRIPT"
chmod 777 "$SCRIPT"

# 2. Add "rfkill unblock bluetooth" after "/etc/init.d/bluetooth start"
line_nums=$(grep -n '/etc/init.d/bluetooth start' "$SCRIPT" | cut -d: -f1)
last_two=$(echo "$line_nums" | tail -2)

for ln in $(echo "$last_two" | sort -nr); do
 next_line=$((ln + 1))
 next_content=$(sed -n "${next_line}p" "$SCRIPT" 2>/dev/null)
 clean_next=$(printf '%s' "$next_content" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')
 if [ "$clean_next" != "rfkill unblock bluetooth" ]; then
 sed -i "${ln}a rfkill unblock bluetooth" "$SCRIPT"
 fi
done
  fi
fi
# Update firmware of nrf52840-NIC 
if [ -f "/home/root/skf_gw_mqtt/hci_uart.signed.bin" ]; then
  # Prepare restarting dbus used by bluez
  /etc/init.d/dbus-1 reload
  sleep 1s
  rfkill block bluetooth
  sudo service bluetooth stop
  /etc/init.d/bluetooth stop

  echo 82 > /sys/class/gpio/export
  echo out > /sys/class/gpio/gpio82/direction
  echo 1 > /sys/class/gpio/gpio82/value
  echo 81 > /sys/class/gpio/export
  echo out > /sys/class/gpio/gpio81/direction
  echo 1 > /sys/class/gpio/gpio81/value
  sleep 1
  echo 82 > /sys/class/gpio/export
  echo 0 > /sys/class/gpio/gpio82/value
  echo 81 > /sys/class/gpio/export
  echo 0 > /sys/class/gpio/gpio81/value
  sleep 1
  echo 82 > /sys/class/gpio/export
  echo 1 > /sys/class/gpio/gpio82/value
  echo 81 > /sys/class/gpio/export
  echo 1 > /sys/class/gpio/gpio81/value
  sleep 1
  
  cd /home/root/skf_gw_mqtt
  # Should read the image for multiple times before uploading the .bin file 
  # (Uploading would be timeout if "image list" is not running and not really 
  # know the root cause. So please keep them)
  ./mcumgr -s=dev=/dev/ttyS8,baud=115200 image list
  sleep 1
  ./mcumgr -s=dev=/dev/ttyS8,baud=115200 image list
  sleep 1
  ./mcumgr -s=dev=/dev/ttyS8,baud=115200 image upload hci_uart.signed.bin
  # Rename the .bin file after uploading completes (for debugging)
  mv hci_uart.signed.bin hci_uart.signed.bin.bk
  # Reset the nrf52840-nic by instruct (which is same as the reset by pin but faster)
  ./mcumgr -s=dev=/dev/ttyS8,baud=115200 reset
  
  # Run autorun.sh to set MAC address
  /etc/autorun.sh 
else
  # Prepare restarting dbus used by bluez
  /etc/init.d/dbus-1 reload
  sleep 1s
  rfkill block bluetooth
  sudo service bluetooth stop
  /etc/init.d/bluetooth stop
  
  # Run autorun.sh to set MAC address
  /etc/autorun.sh
fi
fi

# Update start.sh
echo "" > /home/root/skf_gw_mqtt/start.sh
echo "#!/bin/bash
cd /home/root/skf_gw_mqtt
./restart_ble.sh &
sleep 1
./restart_mqtt.sh &
sleep 1
./restart_sql.sh &
sleep 1
./restart_mod.sh &
exit 0" > /home/root/skf_gw_mqtt/start.sh

# Update bluetooth
echo "" > /etc/init.d/bluetooth
if [ -f "/home/root/skf_gw_mqtt/nrf52840nic" ]; then
echo "#!/bin/sh

[ -f /usr/libexec/bluetooth/bluetoothd ] || exit 0
[ -f /usr/libexec/bluetooth/obexd ] || exit 0

case \"\$1\" in
  start)
export DBUS_SESSION_BUS_ADDRESS=unix:path=/var/run/dbus/system_bus_socket
hciattach /dev/ttyS8 any -s 115200 115200 flow > /dev/null
hciconfig hci0 up
/usr/libexec/bluetooth/bluetoothd -P battery -E &
/usr/libexec/bluetooth/obexd -a -r /home/root &
;;
  stop)
killall bluetoothd
killall obexd
hciconfig hci0 down
killall hciattach
;;
  restart|reload)
\"\$0\" stop
\"\$0\" start
;;
  *)
echo \"Usage: \$0 {start|stop|restart}\"
exit 1
esac

exit 0" > /etc/init.d/bluetooth
else
echo "#!/bin/sh

[ -f /usr/libexec/bluetooth/bluetoothd ] || exit 0
[ -f /usr/libexec/bluetooth/obexd ] || exit 0

case \"\$1\" in
  start)
export DBUS_SESSION_BUS_ADDRESS=unix:path=/var/run/dbus/system_bus_socket
hciattach /dev/ttyS7 any -s 115200 115200 flow > /dev/null
hciconfig hci0 up
/usr/libexec/bluetooth/bluetoothd -P battery -E &
/usr/libexec/bluetooth/obexd -a -r /home/root &
;;
  stop)
killall bluetoothd
killall obexd
hciconfig hci0 down
killall hciattach
;;
  restart|reload)
\"\$0\" stop
\"\$0\" start
;;
  *)
echo \"Usage: \$0 {start|stop|restart}\"
exit 1
esac

exit 0" > /etc/init.d/bluetooth
fi

# Update logrotate
sudo tee /etc/systemd/system/gateway-logrotate.service > /dev/null <<'EOF' 
[Unit]
Description=Logrotate for Gateway Application
Documentation=man:logrotate(8)

[Service] 
Type=oneshot
ExecStart=/usr/sbin/logrotate /etc/logrotate.d/gateway  
EOF

sudo tee /etc/systemd/system/gateway-logrotate.timer > /dev/null <<'EOF'  
[Unit]
Description=Run gateway logrotate every 30 mins
Requires=gateway-logrotate.service   
  
[Timer] 
OnCalendar=*:0/30 
AccuracySec=1min 
Persistent=true
  
[Install]
WantedBy=timers.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable gateway-logrotate.timer 
sudo systemctl restart gateway-logrotate.timer 

# Check libraries in /home/root/skf_gw_mqtt
if [ -s "/home/root/skf_gw_mqtt/libs.tar.gz" ];then
tar -zxf /home/root/skf_gw_mqtt/libs.tar.gz --directory /usr/lib
else
echo "Do nothing ..."
fi

# Kill the running process and start daemon
pgrep skf_gw_1 | xargs kill -s 9
chmod 777 $APP_PATH
while true; do
#counter=$(pgrep -xc "skf_gw_ble_0613")
counter=$(pgrep -xc "skf_gw_1")
if [ $counter -eq 3 ]; then
  sleep 1s
else
  pgrep skf_gw_1 | xargs kill -s 9
  # echo "And kill the mqtt_op (just a workaround)"
  # pgrep mqtt_op | xargs kill -s 9

  echo "stop and start bluetooth"
  /etc/init.d/dbus-1 reload
  sleep 1s
  rfkill block bluetooth
  sudo service bluetooth stop
  /etc/init.d/bluetooth stop
  rfkill unblock bluetooth
  sleep 1s
  /etc/init.d/bluetooth start
  sleep 1s
  rfkill unblock bluetooth

  echo "to restart ble application"
  # Just for the ble application which does not support database
  cp /home/root/skf_gw_mqtt/gwConfig.json.backup /home/root/skf_gw_mqtt/v1/gwConfig.json
  #$APP_PATH >>$log/ble_$(date +%Y%m%d)_$(date +%H%M%S).log 2>&1 &
  $APP_PATH >>$log/ble.log 2>&1 &
  rfkill unblock bluetooth

  sleep 30s
fi
done
