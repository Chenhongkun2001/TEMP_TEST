#!/bin/bash
#
# SKF Gateway secure installer
#
# Usage:
#   ./skf_gateway_install.sh C4:BD:6A:12:00:00
#   ./skf_gateway_install.sh C4:BD:6A:13:00:00 pro
#
# Optional for engineering/debug:
#   SKF_NO_REBOOT=1 ./skf_gateway_install.sh C4:BD:6A:12:00:00
#
# Final architecture:
#   - skfgw: service-only account, no interactive login
#   - gwadmin: normal Linux maintenance account, SSH login
#   - root: hardware/provisioning/recovery only; direct SSH disabled by package
#   - skf-gw/sql/mqtt/modbus: systemd, User=skfgw
#   - GPIO/Bluetooth HCI init: dedicated root systemd services
#   - Forward SSH (frpc) remains available until Reverse SSH provisioning succeeds
#   - Reverse/Forward SSH switching is handled later by reverse-ssh-light-tools
#

set -Eeuo pipefail
 
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}"
 
BOOT_MOUNT="/run/media/Boot-mmcblk0p1"
INSTALL_STAGE="/opt/skf-installer"
PKG_ARCHIVE="${ROOT_DIR}/skf_package.tar.gz"
PKG_STAGE="${INSTALL_STAGE}/skf_package"
 
GW_USER="skfgw"
GW_GROUP="skfgw"
GW_DATA_DIR="/var/lib/skf-gateway"
GW_CONFIG_DIR="${GW_DATA_DIR}/config"
GW_RUNTIME_DIR="/run/skf-gateway"
 
LEGACY_SERVICE="skf_gw.service"
NEW_TARGET="skf-gateway.target"
 
DB_MQTT_PID=""
DB_SQL_PID=""
DB_JSON_PID=""
 
log()
{
    printf '[SKF-INSTALL] %s\n' "$*"
}
 
die()
{
    printf '[SKF-INSTALL][ERROR] %s\n' "$*" >&2
    exit 1
}
 
on_error()
{
    local rc=$?
    printf '[SKF-INSTALL][ERROR] failed at line %s (rc=%s)\n' \
        "${BASH_LINENO[0]:-unknown}" "${rc}" >&2
    exit "${rc}"
}
 
trap on_error ERR
 
usage()
{
    cat <<'EOF'
Usage:
  ./skf_gateway_install.sh <MAC-address> [pro]
 
Examples:
  ./skf_gateway_install.sh C4:BD:6A:12:00:00
  ./skf_gateway_install.sh C4:BD:6A:13:00:00 pro
 
Environment:
  SKF_NO_REBOOT=1   Do not reboot automatically after installation.
EOF
}
 
require_root()
{
    [ "$(id -u)" -eq 0 ] || die "This installer must run as root."
}
 
validate_mac()
{
    echo "${MACADDR}" | grep -Eq \
        '^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$' \
        || die "Invalid MAC address: ${MACADDR}"
}
 
require_file()
{
    [ -f "$1" ] || die "Required file missing: $1"
}
 
stop_legacy_and_new_gateway_processes()
{
    log "Stopping legacy/new gateway services before installation"
 
    systemctl stop "${NEW_TARGET}" 2>/dev/null || true
 
    for unit in \
        skf-gw-modbus.service \
        skf-gw-mqtt.service \
        skf-gw-sql.service \
        skf-gw.service \
        "${LEGACY_SERVICE}"
    do
        systemctl stop "${unit}" 2>/dev/null || true
    done
 
    systemctl disable "${LEGACY_SERVICE}" 2>/dev/null || true
 
    # Legacy script supervisors must not survive the migration.
    pkill -f '/restart_ble\.sh' 2>/dev/null || true
    pkill -f '/restart_mqtt\.sh' 2>/dev/null || true
    pkill -f '/restart_sql\.sh' 2>/dev/null || true
    pkill -f '/restart_mod\.sh' 2>/dev/null || true
 
    pkill -x skf_gw_1 2>/dev/null || true
    pkill -x mqtt_op 2>/dev/null || true
    pkill -x sql_op 2>/dev/null || true
    pkill -x skf_gw_mod 2>/dev/null || true
    pkill -x skf_gw_modbus_rtu 2>/dev/null || true
 
    # The old autorun path manipulated BlueZ/HCI directly.
    systemctl disable autorun.service 2>/dev/null || true
    systemctl stop autorun.service 2>/dev/null || true
}
 
prepare_periodic_reboot()
{
    log "Installing periodic reboot job (preserving existing 17:58 schedule)"
 
    install -d -o root -g root -m 0755 /usr/libexec/skf-gateway
 
    cat > /usr/libexec/skf-gateway/daily-reboot.sh <<'EOF'
#!/bin/sh
sync
sync
sync
exec /bin/systemctl reboot
EOF
 
    chmod 0700 /usr/libexec/skf-gateway/daily-reboot.sh
    chown root:root /usr/libexec/skf-gateway/daily-reboot.sh
 
    # Replace only the old SKF reboot entry; preserve unrelated root crontab jobs.
    {
        crontab -l 2>/dev/null \
            | grep -v -E '/home/root/cron/restart\.sh|/usr/libexec/skf-gateway/daily-reboot\.sh' \
            || true
        echo '58 17 * * * /usr/libexec/skf-gateway/daily-reboot.sh'
    } | crontab -
}
 
remove_obsolete_logrotate_supervisor()
{
    # New services log to journald.  Do not keep the old
    # /home/root/skf_gw_mqtt/log supervisor alive.
    systemctl disable --now gateway-logrotate.timer 2>/dev/null || true
 
    rm -f \
        /etc/systemd/system/gateway-logrotate.service \
        /etc/systemd/system/gateway-logrotate.timer \
        /etc/logrotate.d/gateway
 
    systemctl daemon-reload
}
 
install_boot_files()
{
    log "Installing bootloader files"
 
    [ -d "${BOOT_MOUNT}" ] \
        || die "Boot partition is not mounted at ${BOOT_MOUNT}"
 
    require_file "${ROOT_DIR}/tiboot3.bin"
    require_file "${ROOT_DIR}/tispl.bin"
    require_file "${ROOT_DIR}/u-boot.img"
 
    # Copy, do not mv: keep the installer bundle intact/repeatable.
    install -m 0750 "${ROOT_DIR}/tiboot3.bin" "${BOOT_MOUNT}/tiboot3.bin"
    install -m 0750 "${ROOT_DIR}/tispl.bin"  "${BOOT_MOUNT}/tispl.bin"
    install -m 0750 "${ROOT_DIR}/u-boot.img" "${BOOT_MOUNT}/u-boot.img"
 
    sync
}
 
install_forward_frp_fallback()
{
    log "Installing FRP forward-SSH fallback"
 
    require_file "${ROOT_DIR}/frp.tar"
 
    rm -rf /home/root/frp
    tar -xf "${ROOT_DIR}/frp.tar" -C /home/root
 
    require_file /home/root/frp/frpc
    require_file /home/root/frp/frpc.ini
    require_file /home/root/frp/frpc.service
 
    chmod 0755 /home/root/frp/frpc
    chown -R root:root /home/root/frp
 
    # Gateway identity is the final three MAC octets, e.g. 12:00:33 -> 120033.
    sed -i \
        "s/120043/${ADDR}/g" \
        /home/root/frp/frpc.ini
 
    install -o root -g root -m 0644 \
        /home/root/frp/frpc.service \
        /etc/systemd/system/frpc.service
 
    printf '%s\n' "${GATEWAY_HOSTNAME}" > /etc/hostname
 
    if command -v hostnamectl >/dev/null 2>&1; then
        hostnamectl set-hostname "${GATEWAY_HOSTNAME}" || true
    fi
 
    systemctl daemon-reload
 
    # IMPORTANT:
    # Keep forward FRP alive until reverse-SSH provisioning is proven online.
    # reverse-ssh-control will later perform the safe mutual-exclusion switch.
    systemctl enable frpc.service
    systemctl restart frpc.service || \
        log "WARNING: frpc.service did not start now; inspect before reboot."
}
 
trim_legacy_system_payload()
{
    log "Applying existing system-trim policy"
 
    require_file "${ROOT_DIR}/qt.tar"
 
    tar -xf "${ROOT_DIR}/qt.tar" -C /usr/lib
 
    rm -f /usr/bin/seva-launcher-aarch64
    rm -rf /usr/include/tensorflow
    rm -rf /usr/include/opencv*
    rm -rf /usr/include/onnxruntime
    rm -rf /opt/edgeai-*
    rm -f /opt/seva-browser.tar.gz
}
 
extract_and_verify_package()
{
    log "Extracting and verifying skf_package.tar.gz"
 
    require_file "${PKG_ARCHIVE}"
 
    rm -rf "${INSTALL_STAGE}"
    install -d -o root -g root -m 0700 "${INSTALL_STAGE}"
 
    tar -xzf "${PKG_ARCHIVE}" -C "${INSTALL_STAGE}"
 
    [ -d "${PKG_STAGE}" ] \
        || die "Package archive does not contain top-level skf_package/"
 
    require_file "${PKG_STAGE}/install.sh"
    chmod 0755 "${PKG_STAGE}/install.sh"
 
    if [ -f "${PKG_STAGE}/manifest.sha256" ]; then
        (
            cd "${PKG_STAGE}"
            sha256sum -c manifest.sha256
        ) || die "Package manifest verification failed."
    else
        log "WARNING: package has no manifest.sha256"
    fi
 
    # Reject the legacy start.sh launcher if it was accidentally re-packed.
    if grep -RInE \
        'ExecStart=.*/start\.sh' \
        "${PKG_STAGE}" \
        --exclude='*.bak' \
        >/dev/null 2>&1
    then
        die "Package contains prohibited legacy start.sh configuration."
    fi
}
 
install_secure_package()
{
    log "Installing secure skf_package"
 
    cd "${PKG_STAGE}"
 
    if [ "${PRO}" -eq 1 ]; then
        ./install.sh --mac "${MACADDR}" --pro
    else
        ./install.sh --mac "${MACADDR}"
    fi
 
    cd "${ROOT_DIR}"
 
    # Validate that the formal runtime layout was actually installed.
    require_file /opt/skf-gateway/bin/skf_gw
    require_file /opt/skf-gateway/bin/sql_op
    require_file /opt/skf-gateway/bin/mqtt_op
    require_file /opt/skf-gateway/bin/skf_gw_modbus_rtu
 
    require_file /etc/systemd/system/skf-gateway.target
    require_file /etc/udev/rules.d/99-skf-gateway.rules
    require_file /etc/tmpfiles.d/skf-gateway.conf
    require_file /etc/ssh/sshd_config.d/60-skf-gateway.conf
    require_file /etc/sudoers.d/70-gwadmin-reverse-ssh
 
    id "${GW_USER}" >/dev/null 2>&1 \
        || die "skfgw account was not created."
 
    id gwadmin >/dev/null 2>&1 \
        || die "gwadmin account was not created."
 
    systemd-tmpfiles --create /etc/tmpfiles.d/skf-gateway.conf
}
 
neutralize_legacy_bluetooth_autorun()
{
    log "Neutralizing legacy Bluetooth autorun path"
 
    systemctl disable autorun.service 2>/dev/null || true
    systemctl stop autorun.service 2>/dev/null || true
 
    # Old autorun.sh repeatedly stopped/started Bluetooth and rewrote MAC state.
    # Keep a harmless compatibility file in case another old component calls it.
    cat > /etc/autorun.sh <<'EOF'
#!/bin/sh
# Bluetooth/HCI is now managed by SKF systemd hardware-init services.
exit 0
EOF
 
    chmod 0755 /etc/autorun.sh
    chown root:root /etc/autorun.sh
}
 
cleanup_db_bootstrap_processes()
{
    pkill -u "${GW_USER}" -x json_test 2>/dev/null || true
    pkill -u "${GW_USER}" -x mqtt_op 2>/dev/null || true
    pkill -u "${GW_USER}" -x sql_op 2>/dev/null || true
 
    DB_MQTT_PID=""
    DB_SQL_PID=""
    DB_JSON_PID=""
}
 
bootstrap_database_if_needed()
{
    local existing_db=""
    local db_file=""
    local elapsed=0
    local max_wait=120
 
    existing_db="$(find "${GW_DATA_DIR}" -maxdepth 2 -type f -name 'skf.db' -print -quit 2>/dev/null || true)"
 
    if [ -n "${existing_db}" ]; then
        log "Database already exists: ${existing_db}"
        chown "${GW_USER}:${GW_GROUP}" "${existing_db}"
        chmod 0600 "${existing_db}"
        return 0
    fi
 
    if [ ! -x /opt/skf-gateway/bin/json_test ]; then
        log "WARNING: json_test is not installed; database bootstrap is deferred to runtime."
        return 0
    fi
 
    log "Bootstrapping gateway database as ${GW_USER}"
 
    systemctl stop skf-gw-modbus.service 2>/dev/null || true
    systemctl stop skf-gw-mqtt.service 2>/dev/null || true
    systemctl stop skf-gw-sql.service 2>/dev/null || true
    systemctl stop skf-gw.service 2>/dev/null || true
 
    systemd-tmpfiles --create /etc/tmpfiles.d/skf-gateway.conf
 
    # One-time migration cleanup of stale root-created POSIX semaphores.
    rm -f /dev/shm/sem.namedsemafile
    rm -f /dev/shm/namedsemafile
 
    runuser -u "${GW_USER}" -- /bin/sh -c \
        'cd /var/lib/skf-gateway && exec /opt/skf-gateway/bin/mqtt_op' \
        >/var/log/skf-db-bootstrap-mqtt.log 2>&1 &
    DB_MQTT_PID=$!
 
    sleep 1
 
    runuser -u "${GW_USER}" -- /bin/sh -c \
        'cd /var/lib/skf-gateway && exec /opt/skf-gateway/bin/sql_op' \
        >/var/log/skf-db-bootstrap-sql.log 2>&1 &
    DB_SQL_PID=$!
 
    sleep 1
 
    runuser -u "${GW_USER}" -- /bin/sh -c \
        'cd /var/lib/skf-gateway && exec /opt/skf-gateway/bin/json_test' \
        >/var/log/skf-db-bootstrap-json.log 2>&1 &
    DB_JSON_PID=$!
 
    while kill -0 "${DB_JSON_PID}" 2>/dev/null; do
        sleep 1
        elapsed=$((elapsed + 1))
 
        if [ "${elapsed}" -ge "${max_wait}" ]; then
            cleanup_db_bootstrap_processes
            die "json_test did not finish within ${max_wait}s; see /var/log/skf-db-bootstrap-*.log"
        fi
    done
 
    wait "${DB_JSON_PID}" || true
    DB_JSON_PID=""
 
    cleanup_db_bootstrap_processes
 
    db_file="$(find "${GW_DATA_DIR}" -maxdepth 2 -type f -name 'skf.db' -print -quit 2>/dev/null || true)"
 
    if [ -z "${db_file}" ]; then
        log "WARNING: json_test finished but no skf.db was found under ${GW_DATA_DIR}."
        log "Runtime SQL service may create it later; review /var/log/skf-db-bootstrap-*.log if needed."
        return 0
    fi
 
    chown "${GW_USER}:${GW_GROUP}" "${db_file}"
    chmod 0600 "${db_file}"
 
    log "Database initialized: ${db_file}"
}
 
validate_installed_security_model()
{
    log "Validating installed security/service configuration"
 
    systemctl daemon-reload
 
    # Do not restart sshd in the middle of a remote installation.
    # Validate the config now; reboot will apply it cleanly.
    if command -v sshd >/dev/null 2>&1; then
        sshd -t || die "sshd configuration validation failed."
    fi
 
    if command -v visudo >/dev/null 2>&1; then
        visudo -cf /etc/sudoers.d/70-gwadmin-reverse-ssh \
            || die "gwadmin reverse-SSH sudoers validation failed."
    fi
 
    # The legacy root launcher must not be enabled.
    if systemctl is-enabled "${LEGACY_SERVICE}" >/dev/null 2>&1; then
        die "Legacy ${LEGACY_SERVICE} is still enabled."
    fi
 
    # New application entry point must be the target.
    systemctl is-enabled "${NEW_TARGET}" >/dev/null 2>&1 \
        || die "${NEW_TARGET} is not enabled."
 
    # Forward SSH is deliberately kept as the provisioning fallback.
    systemctl is-enabled frpc.service >/dev/null 2>&1 \
        || log "WARNING: frpc.service is not enabled."
 
    log "Security/service configuration validation passed."
}
 
show_credentials_and_next_steps()
{
    local gwadmin_pass_file="${GW_DATA_DIR}/provision/initial-gwadmin-password"
    local root_pass_file="${GW_DATA_DIR}/provision/initial-root-recovery-password"
 
    echo
    echo "============================================================"
    echo " SKF Gateway installation complete"
    echo "============================================================"
    echo "Gateway hostname : ${GATEWAY_HOSTNAME}"
    echo "Gateway MAC      : ${MACADDR}"
    echo "PRO model        : ${PRO}"
    echo
 
    if [ -f "${gwadmin_pass_file}" ]; then
        echo "GWADMIN_INITIAL_PASSWORD=$(cat "${gwadmin_pass_file}")"
    else
        echo "GWADMIN_INITIAL_PASSWORD=<generated on first boot>"
    fi
 
    if [ -f "${root_pass_file}" ]; then
        echo "ROOT_RECOVERY_PASSWORD=$(cat "${root_pass_file}")"
    else
        echo "ROOT_RECOVERY_PASSWORD=<generated on first boot>"
    fi
 
    echo
    echo "IMPORTANT:"
    echo "  1. Record the per-device passwords in the official factory record."
    echo "  2. Direct root SSH is disabled by the new SSH policy."
    echo "  3. gwadmin is the human maintenance SSH account."
    echo "  4. skfgw is service-only and must not be used for SSH."
    echo
    echo "Forward SSH fallback is intentionally left enabled until"
    echo "Reverse SSH provisioning has been completed and verified."
    echo
    echo "After reboot, provision Reverse SSH from local/root maintenance:"
    echo "  /usr/libexec/skf-gateway/gateway-reverse-ssh-setup-light.sh"
    echo "  /usr/bin/reverse-ssh-control always"
    echo
    echo "Only after reverse-SSH is confirmed online will the new"
    echo "reverse-ssh-control logic disable frpc.service."
    echo "============================================================"
    echo
}
 
main()
{
    require_root
 
    if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
        usage
        exit 1
    fi
 
    MACADDR="$1"
 
    if [ "${2:-}" = "pro" ]; then
        PRO=1
    elif [ -n "${2:-}" ]; then
        usage
        die "Unknown second argument: ${2}"
    else
        PRO=0
    fi
 
    validate_mac
 
    IFS=':' read -r MAC1 MAC2 MAC3 MAC4 MAC5 MAC6 <<< "${MACADDR}"
 
    ADDR="$(printf '%s%s%s' "${MAC4}" "${MAC5}" "${MAC6}" | tr '[:lower:]' '[:upper:]')"
    GATEWAY_HOSTNAME="PGW${ADDR}"
 
    log "ROOT_DIR=${ROOT_DIR}"
    log "MAC=${MACADDR}"
    log "ADDR=${ADDR}"
    log "HOSTNAME=${GATEWAY_HOSTNAME}"
    log "PRO=${PRO}"
 
    require_file "${PKG_ARCHIVE}"
 
    stop_legacy_and_new_gateway_processes
    remove_obsolete_logrotate_supervisor
    prepare_periodic_reboot
 
    install_boot_files
    install_forward_frp_fallback
    trim_legacy_system_payload
 
    extract_and_verify_package
    install_secure_package
    neutralize_legacy_bluetooth_autorun
 
    bootstrap_database_if_needed
    validate_installed_security_model
 
    sync
    sync
 
    show_credentials_and_next_steps
 
    if [ "${SKF_NO_REBOOT:-0}" = "1" ]; then
        log "SKF_NO_REBOOT=1: automatic reboot skipped."
        log "Reboot manually after completing any local checks."
        exit 0
    fi
 
    log "Rebooting gateway..."
    sleep 3
    systemctl reboot
}
 
main "$@"
 


