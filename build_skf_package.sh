#!/bin/bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

PKG="${ROOT}/skf_gw_mqtt/deployment/skf_package"
DEPLOY_DIR="${ROOT}/skf_gw_mqtt/deployment"

OUT_DIR="${ROOT}/skf_gateway_install"
OUT="${OUT_DIR}/skf_package.tar.gz"

SKFGW="${ROOT}/skf_gw/build/skf_gw"
MQTT="${ROOT}/skf_gw_mqtt/mqtt/mqtt_op"
SQL="${ROOT}/skf_gw_mqtt/sqlite/sql_op"
MODBUS="${ROOT}/modbus_rtu/modbus_slave/skf_gw_modbus_rtu"

MAGIC="${ROOT}/modbus_rtu/magicFile.json"
GWCONFIG="${ROOT}/skf_gateway_install/gwConfig.json.backup"
PROD_MQTT_HOST="mqtt-dp.skf4u.com"


if ! grep -q \
    "\"MQTTHost\"[[:space:]]*:[[:space:]]*\"${PROD_MQTT_HOST}\"" \
    "${GWCONFIG}"
then
    echo "ERROR: production gwConfig contains wrong MQTT host:"
    grep -n 'MQTTHost' "${GWCONFIG}" || true
    exit 1
fi
 
if grep -q 'mqtt-dpuat\.skf4u\.com' "${GWCONFIG}"; then
    echo "ERROR: UAT MQTT host must not enter production package."
    exit 1
fi

REVERSE_GW="${ROOT}/tools/reverse-ssh-light-tools/gateway-reverse-ssh-setup-light.sh"
REVERSE_UBUNTU="${ROOT}/tools/reverse-ssh-light-tools/ubuntu-reverse-ssh-setup-light.sh"

echo "=== Rebuilding SKF package ==="

# 1. Remove only generated package areas.
rm -rf "${PKG}/bin"
rm -rf "${PKG}/config"
rm -rf "${PKG}/tools/reverse-ssh-light-tools"

mkdir -p "${PKG}/bin"
mkdir -p "${PKG}/config"
mkdir -p "${PKG}/tools/reverse-ssh-light-tools"

# 2. Remove obsolete gwadmin-era package artifacts.
# install.sh may still contain migration-cleanup code mentioning gwadmin.
rm -f "${PKG}/sudoers/70-gwadmin-reverse-ssh"
rmdir "${PKG}/sudoers" 2>/dev/null || true

# 3. Runtime binaries.
install -m 0755 "${SKFGW}" "${PKG}/bin/skf_gw"
install -m 0755 "${MQTT}" "${PKG}/bin/mqtt_op"
install -m 0755 "${SQL}" "${PKG}/bin/sql_op"
install -m 0755 "${MODBUS}" "${PKG}/bin/skf_gw_modbus_rtu"

# 4. Optional json_test.
JSON_TEST="${ROOT}/skf_gw_mqtt/middleware/json/json_test"

if [ -f "${JSON_TEST}" ]; then
    install -m 0755 "${JSON_TEST}" "${PKG}/bin/json_test"
fi

# 5. Runtime configuration templates.
install -m 0644 "${MAGIC}" "${PKG}/config/magicFile.json"
install -m 0644 "${GWCONFIG}" "${PKG}/config/gwConfig.json.backup"

# 6. Reverse SSH tools: always copy the current source versions.
install -m 0755 \
    "${REVERSE_GW}" \
    "${PKG}/tools/reverse-ssh-light-tools/gateway-reverse-ssh-setup-light.sh"

install -m 0755 \
    "${REVERSE_UBUNTU}" \
    "${PKG}/tools/reverse-ssh-light-tools/ubuntu-reverse-ssh-setup-light.sh"

# 7. Package file permissions.
chmod 0755 "${PKG}/install.sh"
chmod 0755 "${PKG}/scripts/"*.sh
chmod 0755 "${PKG}/compat/bluetooth"

# 8. Build manifest.
cd "${PKG}"

rm -f manifest.sha256

find . \
    -type f \
    ! -name manifest.sha256 \
    -print0 | \
sort -z | \
xargs -0 sha256sum \
    > manifest.sha256

# 9. Build package atomically.
mkdir -p "${OUT_DIR}"

cd "${DEPLOY_DIR}"

rm -f "${OUT}.new"

tar -czf "${OUT}.new" skf_package
mv "${OUT}.new" "${OUT}"

echo
echo "=========================================="
echo "SKF package successfully created"
echo
echo "${OUT}"
echo
sha256sum "${OUT}"
echo "=========================================="
