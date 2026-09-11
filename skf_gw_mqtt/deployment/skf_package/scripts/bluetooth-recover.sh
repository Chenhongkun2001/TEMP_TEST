#!/bin/sh
 
REQUEST=/run/skf-gateway/requests/bluetooth-recover
RESULT=/run/skf-gateway/state/bluetooth-recover.result
 
if [ "$(id -u)" -ne 0 ]; then
    echo "bluetooth-recover.sh must run as root" >&2
    exit 126
fi
 
[ -r "$REQUEST" ] || exit 0
 
IFS= read -r TOKEN < "$REQUEST" || TOKEN=""
 
#
# Remove first. Otherwise the .path unit can retrigger
# while the recovery service is still running.
#
rm -f "$REQUEST"
 
[ -n "$TOKEN" ] || exit 1
 
logger -t skf-bt-recover \
    "FULL BLE recovery begin token=$TOKEN"
 
MODBUS_WAS_ACTIVE=0
if systemctl is-active --quiet skf-gw-modbus.service; then
    MODBUS_WAS_ACTIVE=1
fi
 
STOP_RC=0
BT_RC=0
GW_RC=0
MODBUS_RC=0
 
systemctl stop skf-gw.service || STOP_RC=$?
 
systemctl restart bluetooth.service || BT_RC=$?
 
if [ "$BT_RC" -eq 0 ]; then
    i=0
 
    while [ "$i" -lt 10 ]; do
        systemctl is-active --quiet bluetooth.service && break
        sleep 1
        i=$((i + 1))
    done
 
    systemctl is-active --quiet bluetooth.service || BT_RC=1
fi
 
if [ "$BT_RC" -eq 0 ]; then
    sleep 3
fi
 
#
# Always restore GW even when Bluetooth recovery failed.
#
systemctl start skf-gw.service || GW_RC=$?
 
if [ "$GW_RC" -eq 0 ]; then
    i=0
 
    while [ "$i" -lt 10 ]; do
        systemctl is-active --quiet skf-gw.service && break
        sleep 1
        i=$((i + 1))
    done
 
    systemctl is-active --quiet skf-gw.service || GW_RC=1
fi
 
if [ "$MODBUS_WAS_ACTIVE" -eq 1 ]; then
    systemctl start skf-gw-modbus.service || MODBUS_RC=$?
fi
 
RC=0
 
[ "$BT_RC" -eq 0 ] || RC=1
[ "$GW_RC" -eq 0 ] || RC=1
[ "$MODBUS_RC" -eq 0 ] || RC=1
 
TMP="${RESULT}.tmp.$$"
 
umask 027
 
printf '%s %d\n' \
    "$TOKEN" \
    "$RC" \
> "$TMP"
 
chown root:skfgw "$TMP"
chmod 0640 "$TMP"
 
mv -f "$TMP" "$RESULT"
 
logger -t skf-bt-recover \
    "FULL BLE recovery finished token=$TOKEN stop_rc=$STOP_RC bluetooth_rc=$BT_RC gw_rc=$GW_RC modbus_rc=$MODBUS_RC rc=$RC"
 
exit "$RC"