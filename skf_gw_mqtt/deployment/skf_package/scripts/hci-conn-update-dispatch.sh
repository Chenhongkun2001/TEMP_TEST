#!/bin/sh
 
set -u
 
REQ_DIR="/run/skf-gateway/hci-conn-update/requests"
RES_DIR="/run/skf-gateway/hci-conn-update/results"
 
#
# There is NO separate skf_hci_conn_update binary.
# The existing skf_gw binary exposes one privileged helper mode.
#
HELPER="/opt/skf-gateway/bin/skf_gw"
 
if [ "$(id -u)" -ne 0 ]; then
    echo "HCI dispatcher must run as root" >&2
    exit 126
fi
 
mkdir -p "$RES_DIR"
 
for REQ in "$REQ_DIR"/*.req
do
    [ -f "$REQ" ] || exit 0
    [ ! -L "$REQ" ] || {
        rm -f -- "$REQ"
        continue
    }
 
    NAME="${REQ##*/}"
    TOKEN="${NAME%.req}"
 
    case "$TOKEN" in
        ""|*[!A-Za-z0-9._-]*)
            rm -f -- "$REQ"
            continue
            ;;
    esac
 
    WORK="${REQ}.processing.$$"
 
    if ! mv -- "$REQ" "$WORK"; then
        continue
    fi
 
    MAC=""
    PHY=""
    MIN=""
    MAX=""
    LAT=""
    SUP=""
    MINCE=""
    MAXCE=""
    EXTRA=""
 
    IFS=' ' read -r \
        MAC PHY MIN MAX LAT SUP MINCE MAXCE EXTRA \
< "$WORK" || true
 
    rm -f -- "$WORK"
 
    RC=0
 
    #
    # Validate MAC.
    #
    if [ "${#MAC}" -ne 12 ]; then
        RC=2
    else
        case "$MAC" in
            *[!0-9A-Fa-f]*)
                RC=2
                ;;
        esac
    fi
 
    #
    # Exactly seven integer parameters are required.
    #
    if [ -n "$EXTRA" ]; then
        RC=2
    fi
 
    if [ "$RC" -eq 0 ]; then
        for VALUE in \
            "$PHY" \
            "$MIN" \
            "$MAX" \
            "$LAT" \
            "$SUP" \
            "$MINCE" \
            "$MAXCE"
        do
            case "$VALUE" in
                ""|*[!0-9]*)
                    RC=2
                    break
                    ;;
            esac
 
            if [ "$VALUE" -gt 65535 ] 2>/dev/null; then
                RC=2
                break
            fi
        done
    fi
 
    if [ "$RC" -eq 0 ]; then
        if "$HELPER" \
            --priv-hci-conn-update \
            "$MAC" \
            # "$PHY" \
            "$MIN" \
            "$MAX" \
            "$LAT" \
            "$SUP" \
            "$MINCE" \
            "$MAXCE"
        then
            RC=0
        else
            RC=$?
        fi
    fi
 
    TMP="${RES_DIR}/${TOKEN}.result.tmp.$$"
    RESULT="${RES_DIR}/${TOKEN}.result"
 
    umask 027
 
    printf '%d\n' "$RC" > "$TMP"
 
    chown root:skfgw "$TMP"
    chmod 0640 "$TMP"
 
    mv -f "$TMP" "$RESULT"
 
    echo "SKF HCI connection update: token=$TOKEN rc=$RC"
done
 
exit 0