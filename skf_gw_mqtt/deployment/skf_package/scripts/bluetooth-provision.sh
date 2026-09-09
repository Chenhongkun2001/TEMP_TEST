#!/bin/sh
set -eu
 
. /etc/skf-gateway/device.env
 
STATE_DIR=/var/lib/skf-gateway/provision
STAMP="${STATE_DIR}/bluetooth-provisioned"
 
[ -f "${STAMP}" ] && exit 0
 
install -d -o root -g root -m 0700 "${STATE_DIR}"
 
retry=0
while [ ! -d /sys/class/bluetooth/hci0 ]; do
    retry=$((retry + 1))
 
    [ "${retry}" -ge 30 ] && exit 1
    sleep 0.2
done
 
hciconfig hci0 up
 
if [ "${GATEWAY_PRO}" = "1" ]; then
 
    MAC_HEX="$(
        echo "${GATEWAY_MAC}" |
        awk -F: '{
            for(i=6;i>=1;i--) {
                printf "0x%s", $i
                if(i>1) printf " "
            }
        }'
    )"
 
    btmgmt --index 0 static-addr FF:02:03:04:05:FF || true
    sleep 2
 
    btmgmt --index 0 auto-power || true
    sleep 2
 
    hcitool cmd 0x3f 0x005 0x01
    sleep 2
 
    # shellcheck disable=SC2086
    hcitool cmd 0x3f 0x006 ${MAC_HEX}
    sleep 2
 
else
 
    bdaddr -i hci0 "${GATEWAY_MAC}"
    sleep 1
 
fi
 
hciconfig hci0 down || true
sleep 1
hciconfig hci0 up
 
touch "${STAMP}"
chmod 0600 "${STAMP}"
chown root:root "${STAMP}"
 
exit 0
