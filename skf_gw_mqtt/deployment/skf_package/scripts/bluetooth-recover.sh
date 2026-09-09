#!/bin/sh
 
REQUEST=/run/skf-gateway/requests/bluetooth-recover
RESULT=/run/skf-gateway/state/bluetooth-recover.result
 
if [ "$(id -u)" -ne 0 ]; then
    echo "bluetooth-recover.sh must run as root" >&2
    exit 126
fi
 
if [ ! -r "$REQUEST" ]; then
    exit 0
fi
 
IFS= read -r TOKEN < "$REQUEST" || TOKEN=""
 
#
# Remove request immediately so the .path unit does not
# continuously retrigger while this recovery is running.
#
rm -f "$REQUEST"
 
if [ -z "$TOKEN" ]; then
    exit 1
fi
 
echo "SKF Bluetooth recovery requested: $TOKEN"
 
#
# Keep the original business recovery sequence.
#
/etc/init.d/dbus-1 reload
 
sleep 1
 
/usr/sbin/rfkill block bluetooth
 
/usr/sbin/service bluetooth stop
 
/etc/init.d/bluetooth stop
 
/usr/sbin/rfkill unblock bluetooth
 
sleep 1
 
/etc/init.d/bluetooth start
 
sleep 2
 
/usr/sbin/rfkill unblock bluetooth
 
#
# Preserve the original shell semantics:
# the original system() checked the exit status of the
# whole shell script, which is the status of its final command.
#
RC=$?
 
TMP="${RESULT}.tmp.$$"
 
umask 027
 
printf '%s %d\n' "$TOKEN" "$RC" > "$TMP"
 
chown root:skfgw "$TMP"
chmod 0640 "$TMP"
 
mv -f "$TMP" "$RESULT"
 
echo "SKF Bluetooth recovery finished: token=$TOKEN rc=$RC"
 
exit "$RC"