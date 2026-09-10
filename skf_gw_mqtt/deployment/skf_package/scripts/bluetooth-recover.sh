#!/bin/sh
 
REQUEST=/run/skf-gateway/requests/bluetooth-recover
RESULT=/run/skf-gateway/state/bluetooth-recover.result
 
if [ "$(id -u)" -ne 0 ]; then
    exit 126
fi
 
[ -r "$REQUEST" ] || exit 0
IFS= read -r TOKEN < "$REQUEST" || TOKEN=""
rm -f "$REQUEST"
[ -n "$TOKEN" ] || exit 0
logger -t skf-bt-priv \
    "full BLE stack recovery requested token=$TOKEN"
 
#
# IMPORTANT:
# Reproduce the recovery sequence proven on the Gateway.
#
# Do not leave the existing skf_gw process alive while BlueZ
# Device1 objects are destroyed/recreated.
#

systemctl stop skf-gw.service
BT_RC=0
systemctl restart bluetooth.service || BT_RC=$?
sleep 5
GW_RC=0
systemctl start skf-gw.service || GW_RC=$?
 
RC=0
if [ "$BT_RC" -ne 0 ] ||
   [ "$GW_RC" -ne 0 ]; then
    RC=1
fi
 
TMP="${RESULT}.tmp.$$" 
umask 027
printf '%s %d\n' \
    "$TOKEN" \
    "$RC" \
> "$TMP"
 
chown root:skfgw "$TMP"
chmod 0640 "$TMP"
mv -f "$TMP" "$RESULT"
logger -t skf-bt-priv \
    "full BLE stack recovery finished token=$TOKEN rc=$RC"
#
# Do not make the path unit fail repeatedly.
#
exit 0

 