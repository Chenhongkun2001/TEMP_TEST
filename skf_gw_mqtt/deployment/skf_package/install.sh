#!/bin/bash
 
set -euo pipefail
 
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
 
MACADDR=""
PRO=0
 
usage()
{
    echo "Usage:"
    echo "  $0 --mac C4:BD:6A:12:00:00 [--pro]"
}
 
while [ $# -gt 0 ]; do
    case "$1" in
        --mac)
            MACADDR="${2:-}"
            shift 2
            ;;
        --pro)
            PRO=1
            shift
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done
 
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: install.sh must run as root"
    exit 1
fi
 
if [ -z "${MACADDR}" ]; then
    echo "ERROR: --mac is required"
    usage
    exit 1
fi
 
if ! echo "${MACADDR}" | grep -Eq \
'^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$'; then
    echo "ERROR: invalid MAC address: ${MACADDR}"
    exit 1
fi
 
echo "=================================================="
echo " SKF Gateway secure installation"
echo " MAC=${MACADDR}"
echo " PRO=${PRO}"
echo "=================================================="

#
# Package-model consistency checks
#

for required in \
    "${PKG_DIR}/scripts/provision-credentials.sh" \
    "${PKG_DIR}/ssh/60-skf-gateway.conf" \
    "${PKG_DIR}/tools/reverse-ssh-light-tools/gateway-reverse-ssh-setup-light.sh"
do
    if [ ! -f "${required}" ]; then
        echo "ERROR: required package file missing: ${required}"
        exit 1
    fi
done

if grep -q 'gwadmin' "${PKG_DIR}/scripts/provision-credentials.sh"; then
    echo "ERROR: provision-credentials.sh still contains obsolete gwadmin logic."
    exit 1
fi

if grep -q 'gwadmin' "${PKG_DIR}/ssh/60-skf-gateway.conf"; then
    echo "ERROR: SSH policy still contains obsolete gwadmin logic."
    exit 1
fi

if grep -q 'gwadmin' \
    "${PKG_DIR}/tools/reverse-ssh-light-tools/gateway-reverse-ssh-setup-light.sh"; then
    echo "ERROR: Gateway Reverse SSH setup still contains obsolete gwadmin logic."
    exit 1
fi

 
 
#
# 1. Stop legacy application management chain
#
 
#
# The Gateway target is the single lifecycle owner.
# Stop it first so an installation never leaves an active
# target with individually stopped member services.
#
systemctl stop skf-gateway.target 2>/dev/null || true
 
#
# Compatibility with installations that predate PartOf=.
#
systemctl stop skf-gw-mqtt.service 2>/dev/null || true
systemctl stop skf-gw.service      2>/dev/null || true
systemctl stop skf-gw-sql.service  2>/dev/null || true
systemctl stop skf-gw-modbus.service 2>/dev/null || true
 
systemctl disable skf_gw.service 2>/dev/null || true
systemctl stop skf_gw.service    2>/dev/null || true
 
pkill -f '/restart_ble.sh' 2>/dev/null || true
pkill -f '/restart_mqtt.sh' 2>/dev/null || true
pkill -f '/restart_sql.sh' 2>/dev/null || true
pkill -f '/restart_mod.sh' 2>/dev/null || true
 
pkill -x skf_gw_1 2>/dev/null || true
pkill -x mqtt_op 2>/dev/null || true
pkill -x sql_op 2>/dev/null || true
pkill -x skf_gw_mod 2>/dev/null || true
pkill -x skf_gw_modbus_rtu 2>/dev/null || true
 
# Old autorun manipulates Bluetooth and must not control new systemd chain.
systemctl disable autorun.service 2>/dev/null || true

systemctl stop skf-gw-mqtt.service 2>/dev/null || true
systemctl stop skf-gw.service      2>/dev/null || true
systemctl stop skf-gw-sql.service  2>/dev/null || true
 
#
# 2. Create dedicated Gateway application account
#
 
if ! getent group skfgw >/dev/null; then
    groupadd --system skfgw
fi
 
if ! id skfgw >/dev/null 2>&1; then
    useradd \
        --system \
        --gid skfgw \
        --home-dir /var/lib/skf-gateway \
        --shell /bin/bash \
        skfgw
else
    usermod \
        --home /var/lib/skf-gateway \
        --shell /bin/bash \
        skfgw
fi
 
# skfgw keeps /bin/bash and receives its own per-device password from
# provision-credentials.sh. SSH policy, not account locking, controls remote login.
 
 

#
# Remove obsolete gwadmin artifacts from the previous three-account design
#

rm -f /etc/sudoers.d/70-gwadmin-reverse-ssh

if id gwadmin >/dev/null 2>&1; then
    usermod -L -s /usr/sbin/nologin gwadmin 2>/dev/null || true

    if command -v pgrep >/dev/null 2>&1 &&
       pgrep -u gwadmin >/dev/null 2>&1; then
        echo "WARNING: gwadmin still has running processes."
        echo "         The account is locked; delete it after that old session exits."
    else
        userdel -r gwadmin 2>/dev/null || true
    fi
fi

if ! id gwadmin >/dev/null 2>&1 &&
   getent group gwadmin >/dev/null 2>&1; then
    groupdel gwadmin 2>/dev/null || true
fi

#
# 3. Permanent directory layout
#
 
install -d -o root  -g root  -m 0755 /opt/skf-gateway
install -d -o root  -g root  -m 0755 /opt/skf-gateway/bin
 
install -d -o root  -g root  -m 0755 /usr/libexec/skf-gateway
install -d -o root  -g root  -m 0755 /etc/skf-gateway
 
install -d -o skfgw -g skfgw -m 0700 /var/lib/skf-gateway
 
NEW_DB="/var/lib/skf-gateway/skf.db"
OLD_DB="/home/root/skf_gw_mqtt/skf.db"
 
db_usable()
{
    local db="$1"
    local check=""
    local required_tables=""
 
    [ -s "${db}" ] || return 1
 
    check="$(
        sqlite3 "${db}" \
            'PRAGMA quick_check;' \
            2>/dev/null || true
    )"
 
    [ "${check}" = "ok" ] || return 1
 
    required_tables="$(
        sqlite3 "${db}" "
SELECT COUNT(*)
FROM sqlite_master
WHERE type='table'
  AND name IN (
      'Gateway',
      'Dev',
      'Sensor',
      'SensorData'
  );
" 2>/dev/null || true
    )"
 
    [ "${required_tables}" = "4" ]
}
 
 
if db_usable "${NEW_DB}"; then
 
    echo "Existing SKF Gateway database is healthy."
 
elif db_usable "${OLD_DB}"; then
 
    echo "Current Gateway DB is missing or unhealthy."
    echo "Restoring verified legacy Gateway database..."
 
    if [ -e "${NEW_DB}" ]; then
        BAD_DB="${NEW_DB}.bad.$(date +%Y%m%d-%H%M%S)"
 
        cp -a \
            "${NEW_DB}" \
            "${BAD_DB}"
 
        echo "Preserved unhealthy DB as:"
        echo "  ${BAD_DB}"
    fi
 
    rm -f \
        "${NEW_DB}-wal" \
        "${NEW_DB}-shm"
 
    install \
        -o skfgw \
        -g skfgw \
        -m 0600 \
        "${OLD_DB}" \
        "${NEW_DB}"
 
    if ! db_usable "${NEW_DB}"; then
        echo "ERROR: restored Gateway database failed validation."
        exit 1
    fi
 
else
 
    echo "ERROR: no healthy SKF Gateway database is available."
    echo "Current DB: ${NEW_DB}"
    echo "Legacy DB:  ${OLD_DB}"
    exit 1
 
fi
install -d -o skfgw -g skfgw -m 0700 /var/lib/skf-gateway/config
install -d -o skfgw -g skfgw -m 0700 /var/lib/skf-gateway/data
install -d -o skfgw -g skfgw -m 0700 /var/lib/skf-gateway/update
install -d -o root  -g root  -m 0700 /var/lib/skf-gateway/provision
 
 
#
# 4. Install root-owned binaries
#
 
install -o root -g root -m 0755 \
    "${PKG_DIR}/bin/skf_gw" \
    /opt/skf-gateway/bin/skf_gw
 
install -o root -g root -m 0755 \
    "${PKG_DIR}/bin/mqtt_op" \
    /opt/skf-gateway/bin/mqtt_op
 
install -o root -g root -m 0755 \
    "${PKG_DIR}/bin/sql_op" \
    /opt/skf-gateway/bin/sql_op
 
install -o root -g root -m 0755 \
    "${PKG_DIR}/bin/skf_gw_modbus_rtu" \
    /opt/skf-gateway/bin/skf_gw_modbus_rtu
 
if [ -f "${PKG_DIR}/bin/json_test" ]; then
    install -o root -g root -m 0755 \
        "${PKG_DIR}/bin/json_test" \
        /opt/skf-gateway/bin/json_test
fi
 
 
#
# 5. Install application configuration
#
 
install -o skfgw -g skfgw -m 0600 \
    "${PKG_DIR}/config/magicFile.json" \
    /var/lib/skf-gateway/config/magicFile.json
 
MACADDRDASH="${MACADDR//:/-}"
 
sed \
    "s/C4-BD-6A-12-34-56/${MACADDRDASH}/g" \
    "${PKG_DIR}/config/gwConfig.json.backup" \
> /var/lib/skf-gateway/config/gwConfig.json
 
chown skfgw:skfgw \
    /var/lib/skf-gateway/config/gwConfig.json
 
chmod 0600 \
    /var/lib/skf-gateway/config/gwConfig.json
 
install -o skfgw -g skfgw -m 0600 \
    "${PKG_DIR}/config/gwConfig.json.backup" \
    /var/lib/skf-gateway/config/gwConfig.json.backup
 
#

# Update 2026/09/04 5.1 Validate production MQTT broker

#

# Production package MUST use production MQTT broker.

#

EXPECTED_MQTT_HOST="mqtt-dp.skf4u.com"

EXPECTED_MQTT_PORT="1883"
 
GW_CONFIG="/var/lib/skf-gateway/config/gwConfig.json"

GW_DB="/var/lib/skf-gateway/skf.db"
 
echo

echo "Checking MQTT broker configuration..."
 
PROD_MQTT_HOST="mqtt-dp.skf4u.com"
GW_CONFIG="/var/lib/skf-gateway/config/gwConfig.json"
 
echo "Checking MQTT broker configuration..."
 
if [ ! -r "${GW_CONFIG}" ]; then
    echo "ERROR: cannot read ${GW_CONFIG}"
    exit 1
fi
 
CONFIG_MQTT_HOST="$(
    sed -n 's/.*"MQTTHost"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' \
        "${GW_CONFIG}" |
    head -n 1
)"
 
echo "Config MQTT host: ${CONFIG_MQTT_HOST}"
 
if [ "${CONFIG_MQTT_HOST}" != "${PROD_MQTT_HOST}" ]; then
    echo "ERROR: invalid MQTT broker in gwConfig.json"
    echo "Expected: ${PROD_MQTT_HOST}"
    echo "Actual:   ${CONFIG_MQTT_HOST}"
    exit 1
fi
 
echo "MQTT config broker OK."

#
# 6. Bluetooth per-device configuration
#
 
if [ "${PRO}" -eq 1 ]; then
    BLUETOOTH_UART=/dev/ttyS8
else
    BLUETOOTH_UART=/dev/ttyS7
fi
 
cat > /etc/skf-gateway/bluetooth.env <<EOF
BLUETOOTH_UART=${BLUETOOTH_UART}
EOF
 
chmod 0600 /etc/skf-gateway/bluetooth.env
chown root:root /etc/skf-gateway/bluetooth.env
 
cat > /etc/skf-gateway/device.env <<EOF
GATEWAY_MAC=${MACADDR}
GATEWAY_PRO=${PRO}
EOF
 
chmod 0600 /etc/skf-gateway/device.env
chown root:root /etc/skf-gateway/device.env
 
 
#
# 7. Install hardware/helper scripts
#
 
for f in \
    gpio-init.sh \
    bluetooth-hci-main.sh \
    bluetooth-hci-up.sh \
    bluetooth-provision.sh \
    bluetooth-recover.sh \
    provision-credentials.sh \
    hci-conn-update-dispatch.sh \
    network-recovery-dispatch.sh
do
    install -o root -g root -m 0755 \
        "${PKG_DIR}/scripts/${f}" \
        "/usr/libexec/skf-gateway/${f}"
done
 
 
#
# 8. Install systemd units
#
 
for f in \
    skf-gateway.target \
    skf-gw.service \
    skf-gw-sql.service \
    skf-gw-mqtt.service \
    skf-gw-modbus.service \
    skf-gw-gpio-init.service \
    skf-gw-bluetooth-init.service \
    skf-gw-bluetooth-provision.service \
    skf-gw-bluetooth-recover.service \
    skf-gw-bluetooth-recover.path \
    skf-gateway-firstboot-credential.service \
    skf-gw-hci-conn-update.service \
    skf-gw-hci-conn-update.path \
    skf-gw-network-recovery.service \
    skf-gw-network-recovery.path \
    skf-gw-quectel.service
do
    install -o root -g root -m 0644 \
        "${PKG_DIR}/systemd/${f}" \
        "/etc/systemd/system/${f}"
done
 
install -d -o root -g root -m 0755 \
    /etc/systemd/system/bluetooth.service.d
 
install -o root -g root -m 0644 \
    "${PKG_DIR}/systemd/bluetooth.service.d/10-skf-hci.conf" \
    /etc/systemd/system/bluetooth.service.d/10-skf-hci.conf
 
 
#
# 9. Install tmpfiles
#
 
install -d -o root -g root -m 0755 /etc/tmpfiles.d
 
install -o root -g root -m 0644 \
    "${PKG_DIR}/tmpfiles/skf-gateway.conf" \
    /etc/tmpfiles.d/skf-gateway.conf
 
systemd-tmpfiles --create \
    /etc/tmpfiles.d/skf-gateway.conf
 
 
#
# 10. Install udev rules
#
# Important remembered requirement:
# /dev/ttyS3 must automatically become root:skfgw 0660.
#
 
install -d -o root -g root -m 0755 /etc/udev/rules.d
 
install -o root -g root -m 0644 \
    "${PKG_DIR}/udev/99-skf-gateway.rules" \
    /etc/udev/rules.d/99-skf-gateway.rules
 
udevadm control --reload-rules 2>/dev/null || true
 
udevadm trigger \
    --action=add \
    --subsystem-match=input \
    2>/dev/null || true
 
udevadm trigger \
    --action=add \
    --subsystem-match=tty \
    2>/dev/null || true

udevadm trigger \
    --action=add \
    --subsystem-match=usb \
    2>/dev/null || true

udevadm settle 2>/dev/null || true
 
 
#
# 11. Install formal compatibility /etc/init.d/bluetooth
#
# It no longer runs hciattach/bluetoothd itself.
#
 
install -o root -g root -m 0755 \
    "${PKG_DIR}/compat/bluetooth" \
    /etc/init.d/bluetooth
 
 
#
# 12. SSH policy
#
 
install -d -o root -g root -m 0755 /etc/ssh/sshd_config.d
 
install -o root -g root -m 0644 \
    "${PKG_DIR}/ssh/60-skf-gateway.conf" \
    /etc/ssh/sshd_config.d/60-skf-gateway.conf
 

#
# 13. Reverse SSH tools
#
 
install -o root -g root -m 0755 \
    "${PKG_DIR}/tools/reverse-ssh-light-tools/gateway-reverse-ssh-setup-light.sh" \
    /usr/libexec/skf-gateway/gateway-reverse-ssh-setup-light.sh

#
# Remove old dual-mode/fallback timer artifacts.
#
# Do not stop an active frpc session here unless Reverse SSH is already
# verified online. This prevents an in-place migration from cutting off
# the connection that is currently running this installer.
#

systemctl disable --now legacy-frpc-disable.timer >/dev/null 2>&1 || true
systemctl stop legacy-frpc-disable.service >/dev/null 2>&1 || true

rm -f \
    /etc/systemd/system/legacy-frpc-disable.timer \
    /etc/systemd/system/legacy-frpc-disable.service \
    /usr/bin/disable-legacy-frpc.sh \
    /var/lib/reverse-ssh-control/legacy-frpc-disable-deadline

systemctl daemon-reload
systemctl start skf-gw-hci-conn-update.path
systemctl start skf-gw-network-recovery.path

if [ -x /usr/bin/reverse-ssh-control ]; then
    REVERSE_STATUS="$(/usr/bin/reverse-ssh-control status 2>/dev/null || true)"

    if printf '%s\n' "${REVERSE_STATUS}" | grep -qx 'Reverse SSH: online'; then
        systemctl disable frpc.service >/dev/null 2>&1 || true

        if systemctl mask frpc.service >/dev/null 2>&1; then
            systemctl stop frpc.service >/dev/null 2>&1 || true
        else
            echo "WARNING: Reverse SSH is online, but frpc.service could not be masked."
        fi
    else
        echo "INFO: Reverse SSH is not online yet; current frpc is left untouched"
        echo "      to avoid cutting off an in-place migration session."
    fi
fi

 

#
# 15. Optional library package
#
 
if [ -f "${PKG_DIR}/libs.tar.gz" ]; then
    LIB_TMP="$(mktemp -d)"
 
    tar -xzf "${PKG_DIR}/libs.tar.gz" \
        -C "${LIB_TMP}"
 
    if [ -d "${LIB_TMP}/libs" ]; then
        cp -a "${LIB_TMP}/libs/." /usr/lib/
    else
        cp -a "${LIB_TMP}/." /usr/lib/
    fi
 
    rm -rf "${LIB_TMP}"
 
    command -v ldconfig >/dev/null 2>&1 && ldconfig || true
fi
 
 
#
# 16. One-time migration cleanup of old root-owned IPC objects
#
 
rm -f /dev/shm/sem.namedsemafile
rm -f /dev/shm/namedsemafile
 
ipcrm -Q 0x12 2>/dev/null || true
ipcrm -Q 0x13 2>/dev/null || true
ipcrm -Q 0x14 2>/dev/null || true
 
 
#
# 2026/09/04 update Final production MQTT broker check
#
 
PROD_MQTT_HOST="mqtt-dp.skf4u.com"
PROD_MQTT_PORT="1883"
GW_DB="/var/lib/skf-gateway/skf.db"
 
echo "Checking final MQTT broker configuration..."
 
if [ ! -f "${GW_DB}" ]; then
    echo "ERROR: Gateway database not found: ${GW_DB}"
    exit 1
fi
 
DB_MQTT_HOST="$(
    sqlite3 "${GW_DB}" \
        'SELECT MQTTHost FROM Gateway LIMIT 1;' \
        2>/dev/null || true
)"
 
DB_MQTT_PORT="$(
    sqlite3 "${GW_DB}" \
        'SELECT MQTTPort FROM Gateway LIMIT 1;' \
        2>/dev/null || true
)"
 
echo "MQTT host: ${DB_MQTT_HOST}"
echo "MQTT port: ${DB_MQTT_PORT}"
 
if [ "${DB_MQTT_HOST}" != "${PROD_MQTT_HOST}" ] ||
   [ "${DB_MQTT_PORT}" != "${PROD_MQTT_PORT}" ]; then
 
    echo "Fixing MQTT broker to production..."
 
    sqlite3 "${GW_DB}" "
UPDATE Gateway
SET MQTTHost='mqtt-dp.skf4u.com',
    MQTTPort=1883;
"
fi
 
DB_MQTT_HOST="$(
    sqlite3 "${GW_DB}" \
        'SELECT MQTTHost FROM Gateway LIMIT 1;'
)"
 
DB_MQTT_PORT="$(
    sqlite3 "${GW_DB}" \
        'SELECT MQTTPort FROM Gateway LIMIT 1;'
)"
 
if [ "${DB_MQTT_HOST}" != "${PROD_MQTT_HOST}" ] ||
   [ "${DB_MQTT_PORT}" != "${PROD_MQTT_PORT}" ]; then
 
    echo "ERROR: final MQTT broker validation failed."
    exit 1
fi
 
chown skfgw:skfgw "${GW_DB}"
chmod 0600 "${GW_DB}"
 
echo "Final MQTT broker OK: ${DB_MQTT_HOST}:${DB_MQTT_PORT}"
 
 
#
# 17. One-device-one-password
#
 
/usr/libexec/skf-gateway/provision-credentials.sh --show
 
 
#
# 18. Remove direct legacy enablement
#
 
systemctl disable skf_gw.service 2>/dev/null || true
 
systemctl disable skf-gw.service 2>/dev/null || true
systemctl disable skf-gw-sql.service 2>/dev/null || true
systemctl disable skf-gw-mqtt.service 2>/dev/null || true
systemctl disable skf-gw-modbus.service 2>/dev/null || true
 
 
#
# 19. Enable and start the new Gateway stack
#
 
systemctl daemon-reload
 
systemctl enable skf-gateway.target
systemctl enable skf-gateway-firstboot-credential.service
 
#
# Bluetooth must be clean before the new skf_gw starts.
# This preserves the BLE lifecycle conclusion we already
# obtained from the Sensor Offline investigation.
#
systemctl restart bluetooth.service
 
BT_WAIT=0
while ! systemctl is-active --quiet bluetooth.service; do
    BT_WAIT=$((BT_WAIT + 1))
 
    if [ "$BT_WAIT" -ge 10 ]; then
        echo "ERROR: bluetooth.service failed to become active"
        exit 1
    fi
 
    sleep 1
done
 
sleep 3
 
#
# Starting the target starts:
#   sql
#   gw
#   mqtt
#   modbus
#   privileged path watchers
#
systemctl start skf-gateway.target
 
#
# Installation is NOT successful unless the three critical
# application services are actually running.
#
for unit in \
    skf-gw-sql.service \
    skf-gw.service \
    skf-gw-mqtt.service
do
    if ! systemctl is-active --quiet "$unit"; then
        echo "ERROR: $unit is not active after installation"
        systemctl status "$unit" --no-pager -l || true
        exit 1
    fi
done

echo
echo "=================================================="
echo " Congratulations! Installation successfully completed."
echo
echo " Record ROOT_INITIAL_PASSWORD and SKFGW_INITIAL_PASSWORD."
echo " skfgw runs Gateway applications with least privilege."
echo
echo " Per-device Reverse SSH provisioning:"
echo "   /usr/libexec/skf-gateway/gateway-reverse-ssh-setup-light.sh"
echo
echo " Ubuntu-side gwssh/gwctl must use Gateway root."
echo " reverse-ssh-control close does NOT restore Forward SSH."
echo "=================================================="
