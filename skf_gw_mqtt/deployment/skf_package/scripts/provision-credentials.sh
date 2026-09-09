#!/bin/sh
set -eu
 
STATE_DIR=/var/lib/skf-gateway/provision
STAMP="${STATE_DIR}/credentials.initialized"
 
SKFGW_FILE="${STATE_DIR}/initial-skfgw-password"
ROOT_FILE="${STATE_DIR}/initial-root-password"
 
SHOW=0

FORCE=0
 
while [ $# -gt 0 ]; do
    case "$1" in
        --show)
            SHOW=1
            ;;
        --force)
            FORCE=1
            ;;
        *)
            echo "Usage: $0 [--show] [--force]"
            exit 1
            ;;
    esac
    shift
done

 
if [ "${1:-}" = "--show" ]; then
    SHOW=1
fi
 
install -d \
    -o root \
    -g root \
    -m 0700 \
    "${STATE_DIR}"
 
if [ -f "${STAMP}" ] && [ "${FORCE}" -eq 0 ]; then
 
    if [ "${SHOW}" -eq 1 ]; then
 
        echo "Credentials already provisioned."
 
        if [ -f "${SKFGW_FILE}" ]; then
            echo "SKFGW_INITIAL_PASSWORD=$(cat "${SKFGW_FILE}")"
        fi
 
        if [ -f "${ROOT_FILE}" ]; then
            echo "ROOT_INITIAL_PASSWORD=$(cat "${ROOT_FILE}")"
        fi
    fi
 
    exit 0
fi
 
 
generate_password()
{
    od -An -N18 -tx1 /dev/urandom |
        tr -d ' \n'
}
 
 
SKFGW_PASSWORD="$(generate_password)"
ROOT_PASSWORD="$(generate_password)"
 
 
printf 'skfgw:%s\n' "${SKFGW_PASSWORD}" |
    chpasswd
 
printf 'root:%s\n' "${ROOT_PASSWORD}" |
    chpasswd
 
 
umask 077
 
printf '%s\n' "${SKFGW_PASSWORD}" \
> "${SKFGW_FILE}"
 
printf '%s\n' "${ROOT_PASSWORD}" \
> "${ROOT_FILE}"
 
 
chown root:root \
    "${SKFGW_FILE}" \
    "${ROOT_FILE}"
 
chmod 0600 \
    "${SKFGW_FILE}" \
    "${ROOT_FILE}"
 
 
touch "${STAMP}"
 
chown root:root "${STAMP}"
chmod 0600 "${STAMP}"
 
 
if [ "${SHOW}" -eq 1 ]; then
 
    echo
    echo "=================================================="
    echo " SKF Gateway per-device credentials"
    echo
    echo " SKFGW_INITIAL_PASSWORD=${SKFGW_PASSWORD}"
    echo " ROOT_INITIAL_PASSWORD=${ROOT_PASSWORD}"
    echo
    echo " Record these passwords in the factory database."
    echo "=================================================="
 
else
 
    {
        echo
        echo "SKF Gateway per-device credentials generated."
        echo "SKFGW_INITIAL_PASSWORD=${SKFGW_PASSWORD}"
        echo "ROOT_INITIAL_PASSWORD=${ROOT_PASSWORD}"
        echo
    } > /dev/console 2>/dev/null || true
 
fi
 
 
unset SKFGW_PASSWORD
unset ROOT_PASSWORD
 
exit 0