#!/bin/sh
 
set -u
 
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH
 
REQ_DIR="/run/skf-gateway/network-recovery/requests"
RES_DIR="/run/skf-gateway/network-recovery/results"
 
if [ "$(id -u)" -ne 0 ]; then
    echo "Network recovery dispatcher must run as root" >&2
    exit 126
fi
 
 
iface_from_id()
{
    case "$1" in
        1) echo "eth0" ;;
        2) echo "eth1" ;;
        3) echo "wwan0" ;;
        4) echo "mlan0" ;;
        *) return 1 ;;
    esac
}
 
 
write_result()
{
    TOKEN="$1"
    RC="$2"
 
    TMP="${RES_DIR}/${TOKEN}.result.tmp.$$"
    RESULT="${RES_DIR}/${TOKEN}.result"
 
    umask 027
 
    printf '%d\n' "${RC}" > "${TMP}"
    chown root:skfgw "${TMP}"
    chmod 0640 "${TMP}"
 
    mv -f "${TMP}" "${RESULT}"
}
 
 
mkdir -p "${RES_DIR}"
 
 
for REQ in "${REQ_DIR}"/*.req
do
    [ -f "${REQ}" ] || exit 0
 
    [ ! -L "${REQ}" ] || {
        rm -f -- "${REQ}"
        continue
    }
 
    OWNER="$(
        stat -c %U "${REQ}" \
        2>/dev/null || true
    )"
 
    [ "${OWNER}" = "skfgw" ] || {
        rm -f -- "${REQ}"
        continue
    }
 
    NAME="${REQ##*/}"
    TOKEN="${NAME%.req}"
 
    case "${TOKEN}" in
        ""|*[!A-Za-z0-9._-]*)
            rm -f -- "${REQ}"
            continue
            ;;
    esac
 
    WORK="${REQ}.processing.$$"
 
    if ! mv -- "${REQ}" "${WORK}"; then
        continue
    fi
 
    OP=""
    ARG1=""
    ARG2=""
    EXTRA=""
 
    IFS=' ' read -r \
        OP ARG1 ARG2 EXTRA \
< "${WORK}" || true
 
    rm -f -- "${WORK}"
 
    RC=1
 
 
    case "${OP}" in
 
        probe)
            RC=0
            ;;
 
 
        gpio_init)
            if [ ! -d /sys/class/gpio/gpio86 ]; then
                echo 86 \
> /sys/class/gpio/export \
                    2>/dev/null || true
            fi
 
            if [ -d /sys/class/gpio/gpio86 ]; then
                echo out \
> /sys/class/gpio/gpio86/direction &&
 
                echo 0 \
> /sys/class/gpio/gpio86/value
 
                RC=$?
            fi
            ;;
 
 
        lte_power)
            case "${ARG1}" in
                0|1)
                    echo "${ARG1}" \
> /sys/class/gpio/gpio86/value
                    RC=$?
                    ;;
 
                *)
                    RC=2
                    ;;
            esac
            ;;
 
 
        wwan_managed)
            TMP_NET="$(
                mktemp \
                /etc/systemd/network/.20-wwan0.network.XXXXXX
            )"
 
            cat > "${TMP_NET}" <<'NETEOF'
[Match]
Name=wwan*
 
[Network]
DHCP=no
IgnoreCarrierLoss=yes
 
[Link]
RequiredForOnline=no
NETEOF
 
            if chmod 0644 "${TMP_NET}" &&
               chown root:root "${TMP_NET}" &&
               mv -f \
                   "${TMP_NET}" \
                   /etc/systemd/network/20-wwan0.network &&
               systemctl restart systemd-networkd
            then
                RC=0
            else
                rm -f "${TMP_NET}"
                RC=1
            fi
            ;;
 
 
        lte_connect)
            case "${ARG1}" in
                1)
                    systemctl start \
                        skf-gw-quectel.service
 
                    RC=$?
                    ;;
 
                0)
                    systemctl stop \
                        skf-gw-quectel.service
 
                    RC=$?
                    ;;
 
                *)
                    RC=2
                    ;;
            esac
            ;;
 
 
        dhcp)
            IFACE="$(
                iface_from_id "${ARG1}" \
                2>/dev/null
            )" || {
                RC=2
                IFACE=""
            }
 
            if [ -n "${IFACE}" ]; then
 
                OUTPUT="$(
                    udhcpc \
                        -f \
                        -n \
                        -q \
                        -t 5 \
                        -i "${IFACE}" \
                        2>&1
                )"
 
                if printf '%s\n' "${OUTPUT}" |
                   grep -q 'server'
                then
                    RC=0
                else
                    RC=1
                fi
            fi
            ;;
 
 
        default_route)
            IFACE="$(
                iface_from_id "${ARG1}" \
                2>/dev/null
            )" || {
                RC=2
                IFACE=""
            }
 
            case "${ARG2}" in
                ""|"-"|*[!0-9.]*)
                    RC=2
                    ;;
 
                *)
                    if [ -n "${IFACE}" ]; then
 
                        route del default \
>/dev/null 2>&1 || true
 
                        route add default \
                            gw "${ARG2}" "${IFACE}"
 
                        RC=$?
                    fi
                    ;;
            esac
            ;;
 
 
        *)
            RC=127
            ;;
    esac
 
 
    logger -t skf-net-priv \
        "op=${OP} arg1=${ARG1} rc=${RC}"
 
    write_result \
        "${TOKEN}" \
        "${RC}"
done
 
#
# Individual operation failure is returned to mqtt_op
# through the result file.  The dispatcher itself remains
# successful so a path unit cannot enter start-limit-hit.
#
exit 0