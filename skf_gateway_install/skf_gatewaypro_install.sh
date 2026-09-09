#!/bin/bash
ROOT_DIR=$(pwd)
CRON_DIR="/home/root/cron"

usage() {
    printf "Usage:
    ./skf_gateway_install.sh [MAC address] [pro-optional], e.g.,
    ./skf_gateway_install.sh C4:BD:6A:13:00:00 pro\n"
}

if [ $# -lt 1 ];
then
    usage
    exit
fi

if [ "$2" == "pro" ];then
    PRO=1
    echo ${PRO}
else
    PRO=0
    echo ${PRO}
fi

MACADDR=$1 # C4:BD:6A:13:00:00
MACADDRDASH=$(echo ${MACADDR//:/-}) # C4-BD-6A-13-00-00
MACADDRHEX=$(echo ${MACADDR//:/ 0x}) # C4 0xBD 0x6A 0x13 0x00 0x00
MACADDRHEX=$(echo $MACADDRHEX | sed 's/^/0x/g') # 0xC4 0xBD 0x6A 0x13 0x00 0x00
MACADDRHEX=$(echo "$MACADDRHEX" | tr ' ' '\n' | tac | xargs) # 0x00 0x00 0x13 0x6A 0xBD 0xC4
#Prepare the default gwConfig.json
cp gwConfig.json.backup gwConfig.json
sed -i "s/C4-BD-6A-12-34-56/$MACADDRDASH/g" gwConfig.json

#Prepare folders
mkdir /home/root/
mkdir /home/root/skf_gw_mqtt
if [ ${PRO} == 1 ];then
    #Create a file to indicate the gateway adopts nrf52840 as the BLE NIC
    :>/home/root/skf_gw_mqtt/nrf52840nic
else
    #Otherwise, remove the file
    rm /home/root/skf_gw_mqtt/nrf52840nic
fi

#Prepare logrotate (limit log size)
echo -e "/home/root/skf_gw_mqtt/log/*.log {\n  copytruncate\n size 100M\n  compress\n  rotate 1\n  missingok\n  notifempty\n}" > /etc/logrotate.d/gateway
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
#Prepare crontab service (periodic restart at 1.58 Beijing Time)
if [ ! -d "$CRON_DIR" ]; then
    mkdir -p "$CRON_DIR" && chmod 755 "$CRON_DIR"
fi
echo -e "#!/bin/bash
/usr/bin/killall restart_ble.sh
/usr/bin/killall restart_mqtt.sh
/usr/bin/killall restart_sql.sh
/usr/bin/killall restart_mod.sh
/usr/bin/killall mqtt_op
/usr/bin/killall sql_op
/usr/bin/killall skf_gw_1
/usr/bin/killall skf_gw_mod
sync
sync
sync
/sbin/reboot" > /home/root/cron/restart.sh
# echo -e "#!/bin/sh\n\nsync\nsync\nsync\n/sbin/reboot" > /home/root/cron/restart.sh
chmod 700 /home/root/cron/restart.sh
echo -e "58 17 * * * /home/root/cron/restart.sh" > /home/root/cron/cron_restart && crontab /home/root/cron/cron_restart
#Change the password of root
echo root:skfcm456|chpasswd

#Prepare BLE MAC address
echo -n > /etc/autorun.sh
if [ -f "/home/root/skf_gw_mqtt/nrf52840nic" ]; then
    echo -e "#!/bin/sh\n/etc/init.d/bluetooth stop\n#Enable GPIO\necho 82 > /sys/class/gpio/export\necho out > /sys/class/gpio/gpio82/direction\necho 1 > /sys/class/gpio/gpio82/value\necho 81 > /sys/class/gpio/export\necho out > /sys/class/gpio/gpio81/direction\necho 1 > /sys/class/gpio/gpio81/value\nsleep 1\necho 82 > /sys/class/gpio/export\necho 0 > /sys/class/gpio/gpio82/value\necho 81 > /sys/class/gpio/export\necho 0 > /sys/class/gpio/gpio81/value\nsleep 1\necho 82 > /sys/class/gpio/export\necho 1 > /sys/class/gpio/gpio82/value\necho 81 > /sys/class/gpio/export\necho 1 > /sys/class/gpio/gpio81/value\nsleep 23\n\n#Start Bluetooth\n/etc/init.d/bluetooth start\n# bdaddr is used to set the MAC address of azurewave module\n# bdaddr -i hci0 C4:BD:6A:12:01:29\n# The following commands are used to set the MAC address of 52840 module\n# For the all-zero address, we need following bgmgmt commands\n# FF:02:03:04:05:FF is a dummy address (would not affect finally, just for avoiding the all-zero address)\n/etc/init.d/bluetooth stop\n/etc/init.d/bluetooth start\nrfkill unblock bluetooth\nsleep 1\nbtmgmt --index 0 static-addr FF:02:03:04:05:FF\nsleep 2\nbtmgmt --index 0 auto-power\nsleep 2\n# Configure the real MAC address\nhcitool cmd 0x3f 0x005 0x01\nsleep 2\nhcitool cmd 0x3f 0x006 $MACADDRHEX\nsleep 2\n/etc/init.d/bluetooth stop\n/etc/init.d/bluetooth start\nrfkill unblock bluetooth\necho 100000 > /sys/kernel/debug/mmc2/clock\n" >> /etc/autorun.sh
    sudo systemctl disable autorun.service
else
    echo -e "#!/bin/sh\n#Start Bluetooth\n/etc/init.d/bluetooth stop\n/etc/init.d/bluetooth start\nbdaddr -i hci0 $MACADDR\n/etc/init.d/bluetooth stop\n/etc/init.d/bluetooth start\necho 100000 > /sys/kernel/debug/mmc2/clock\n" >> /etc/autorun.sh
fi
chmod 700 /etc/autorun.sh
#Prepare BLE service: to disable the battery info retrieve
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
            /usr/libexec/bluetooth/bluetoothd -P battery &
            /usr/libexec/bluetooth/obexd -a -r /home/root &
            ;;
        stop)
            killall bluetoothd
            killall obexd
            hciconfig hci0 down
            killall hciattach
            ;;
        restart|reload)
            \$0 stop
            \$0 start
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
            /usr/libexec/bluetooth/bluetoothd -P battery &
            /usr/libexec/bluetooth/obexd -a -r /home/root &
            ;;
        stop)
            killall bluetoothd
            killall obexd
            hciconfig hci0 down
            killall hciattach
            ;;
        restart|reload)
            \$0 stop
            \$0 start
            ;;
        *)
            echo \"Usage: \$0 {start|stop|restart}\"
            exit 1
    esac

    exit 0" > /etc/init.d/bluetooth
fi
chmod 700 /etc/init.d/bluetooth

#Prepare Boot file for reboot
cd /home/root/skf_gateway_install
mv tiboot3.bin /run/media/Boot-mmcblk0p1
mv tispl.bin /run/media/Boot-mmcblk0p1
mv u-boot.img /run/media/Boot-mmcblk0p1
chmod 750 /run/media/Boot-mmcblk0p1/*

#Prepare the install package
cd $ROOT_DIR
cp skf_package.tar.gz /home/root/
cd /home/root/
tar -zxvf skf_package.tar.gz
chmod 700 skf_package/mqtt_op
chmod 700 skf_package/sql_op
chmod 700 skf_package/skf_gw_1
chmod 700 skf_package/skf_gw_mod
chmod 700 skf_package/mcumgr
chmod 700 skf_package/hci_uart.signed.bin
chmod 700 skf_package/restart_mqtt.sh
chmod 700 skf_package/restart_sql.sh
chmod 700 skf_package/restart_ble.sh
chmod 700 skf_package/restart_mod.sh
chmod 700 skf_package/start.sh
chmod 700 skf_package/magicFile.json
#install database
mkdir skf_gw_mqtt/v1/
cp skf_package/mqtt_op skf_gw_mqtt/v1/
cp skf_package/sql_op skf_gw_mqtt/v1/
cp skf_package/hci_uart.signed.bin skf_gw_mqtt/
cp skf_package/mcumgr skf_gw_mqtt/
cp skf_package/restart_mqtt.sh skf_gw_mqtt/
cp skf_package/restart_sql.sh skf_gw_mqtt/
cp skf_package/magicFile.json /home/root/
cp skf_package/json_test skf_gw_mqtt/v1/ 
cp $ROOT_DIR/gwConfig.json skf_gw_mqtt/v1/
tar -zxf skf_package/libs.tar.gz -C skf_package/
cp -arf  skf_package/libs/*  /usr/lib
cd skf_gw_mqtt/v1/
killall restart_ble.sh
killall restart_mqtt.sh
killall restart_sql.sh
killall restart_mod.sh
killall mqtt_op
killall sql_op
killall skf_gw_1
killall skf_gw_mod
cd ..
rm skf.db
cd v1/
./mqtt_op &
sleep 1
./sql_op &
sleep 1
./json_test &
while true; do
    counter=$(pgrep -xc "json_test")                                             
    if [ $counter -eq 0 ]; then                                   
        killall mqtt_op                         
        killall sql_op                                
        break                                                                                                                                          
    fi                                                                           
done
chmod 700 ../skf.db
#Prepare Frpc and hostname
cd /home/root/skf_gateway_install
tar -xvf frp.tar
mv frp /home/root
cp /home/root/frp/frpc.service /etc/systemd/system
cd /home/root/frp
ADD=$1
ADD=$(echo ${MACADDR:8:9})
ADDR=$1
ADDR=$(echo ${ADD//:/})
sed -i "s/120043/$ADDR/" frpc.ini
sed -i "s/OK62xx/PGW$ADDR/" /etc/hostname
sudo systemctl enable frpc

#Trim the system
cd /home/root/skf_gateway_install
cp qt.tar /usr/lib
cd /usr/lib
tar -xvf qt.tar
rm qt.tar
rm /usr/bin/seva-launcher-aarch64
rm -r /usr/include/tensorflow/
rm -r /usr/include/opencv*
rm -r /usr/include/onnxruntime/
rm -r /opt/edgeai-*
rm /opt/seva-browser.tar.gz

#Install the package
cd /home/root/
cd skf_package/
sh install.sh

#Reboot
reboot