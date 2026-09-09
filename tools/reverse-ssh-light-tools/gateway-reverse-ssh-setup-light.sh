#!/bin/sh
set -eu
 
# Author: hongkun.chen@skf.com
#
# SKF Gateway Reverse SSH lightweight setup
#
# Access model:
#   Reverse SSH is the only supported remote-maintenance channel.
#   Ubuntu-side gwssh/gwctl connect to Gateway root through the reverse socket.
#   reverse-ssh-control close only closes Reverse SSH; it never starts or
#   enables a Forward-SSH/frpc fallback.
#   skfgw remains the least-privilege application account and is not used by
#   this root-maintenance tunnel.
 
# ============================================================
# Gateway: global configuration
# ============================================================
 
DEVICE_ID="$(hostname)"
 
# Persistent SERVER_ID printed by the Ubuntu-side setup script.
UBUNTU_SERVER_ID="debug-ubuntu-3405c7681085"
 
UBUNTU_FRP_DOMAIN="rss-${UBUNTU_SERVER_ID}"
UBUNTU_SSH_USER="forlinx"
 
FRP_HTTP_CONNECT_HOST="47.117.161.234"
FRP_HTTP_CONNECT_PORT="31702"
 
GATEWAY_SSH_PORT="22"
ROOT_HOME="/home/root"
 
# Timed Reverse SSH opening duration: 3 days.
DEFAULT_OPEN_SECONDS=259200
 
# Reverse SSH health-check period.
HEALTH_CHECK_SECONDS=60
 
# Legacy positive/forward transport. This setup retires it only after the
# Reverse SSH socket is verified online. reverse-ssh-control never restores it.
LEGACY_FORWARD_SERVICE="frpc.service"
 
# ============================================================
# Usually no changes are required below
# ============================================================
 
SSH_DIR="$ROOT_HOME/.ssh"
REVERSE_KEY="$SSH_DIR/id_ed25519_reverse"
 
PROXY="/usr/bin/http-connect-proxy.py"
START="/usr/bin/start-reverse-ssh.sh"
CTL="/usr/bin/reverse-ssh-control"
 
STATE_DIR="/var/lib/reverse-ssh-control"
SOCKET="/home/${UBUNTU_SSH_USER}/.rssh/${DEVICE_ID}.sock"
 
REVERSE_SERVICE="/etc/systemd/system/reverse-ssh.service"
HEALTH_SERVICE="/etc/systemd/system/reverse-ssh-health.service"
HEALTH_TIMER="/etc/systemd/system/reverse-ssh-health.timer"
DEBUG_OPEN_SERVICE="/etc/systemd/system/reverse-ssh-debug-open.service"
 
die() {
  echo "[Gateway:$DEVICE_ID][ERROR] $*" >&2
  exit 1
}
 
log() {
  echo "[Gateway:$DEVICE_ID] $*"
}
 
require_root() {
  if [ "$(id -u)" -ne 0 ]; then
    die "请以 root 执行。"
  fi
}
 
validate() {
  case "$DEVICE_ID" in
    *[!A-Za-z0-9._-]*|'') die "DEVICE_ID 非法：$DEVICE_ID" ;;
  esac
 
  if [ "$UBUNTU_SERVER_ID" = "REPLACE_WITH_UBUNTU_SERVER_ID" ]; then
    die "请先填写 UBUNTU_SERVER_ID。"
  fi
 
  case "$DEFAULT_OPEN_SECONDS" in
    ''|*[!0-9]*) die "DEFAULT_OPEN_SECONDS 必须是非负整数秒。" ;;
  esac
 
  case "$HEALTH_CHECK_SECONDS" in
    ''|*[!0-9]*) die "HEALTH_CHECK_SECONDS 必须是非负整数秒。" ;;
  esac
 
  for c in ssh ssh-keygen python3 systemctl sed grep hostname date cp; do
    command -v "$c" >/dev/null 2>&1 || die "缺少 $c"
  done
 
}
 
# ------------------------------------------------------------
# HTTP CONNECT ProxyCommand
# ------------------------------------------------------------
 
install_proxy() {
cat >"$PROXY" <<EOF
#!/usr/bin/env python3
import os
import select
import socket
import sys
 
PROXY_HOST = "${FRP_HTTP_CONNECT_HOST}"
PROXY_PORT = ${FRP_HTTP_CONNECT_PORT}
 
if len(sys.argv) != 3:
    print("Usage: http-connect-proxy.py HOST PORT", file=sys.stderr)
    raise SystemExit(2)
 
host = sys.argv[1]
port = int(sys.argv[2])
 
sock = socket.create_connection((PROXY_HOST, PROXY_PORT), timeout=15)
sock.sendall(("CONNECT %s:%d HTTP/1.0\\r\\n\\r\\n" % (host, port)).encode("ascii"))
 
response = b""
while b"\\r\\n\\r\\n" not in response:
    chunk = sock.recv(4096)
    if not chunk:
        print("proxy closed before response", file=sys.stderr)
        raise SystemExit(1)
    response += chunk
    if len(response) > 65536:
        print("proxy response too large", file=sys.stderr)
        raise SystemExit(1)
 
headers, remaining = response.split(b"\\r\\n\\r\\n", 1)
status = headers.split(b"\\r\\n", 1)[0]
parts = status.split()
 
if len(parts) < 2 or parts[1] != b"200":
    print("HTTP CONNECT failed: " + status.decode(errors="replace"), file=sys.stderr)
    raise SystemExit(1)
 
if remaining:
    os.write(sys.stdout.fileno(), remaining)
 
sock.settimeout(None)
stdin_fd = sys.stdin.fileno()
stdout_fd = sys.stdout.fileno()
stdin_open = True
 
while True:
    inputs = [sock] + ([stdin_fd] if stdin_open else [])
    readable, _, _ = select.select(inputs, [], [])
 
    if sock in readable:
        data = sock.recv(65536)
        if not data:
            raise SystemExit(0)
        os.write(stdout_fd, data)
 
    if stdin_open and stdin_fd in readable:
        data = os.read(stdin_fd, 65536)
        if not data:
            stdin_open = False
            try:
                sock.shutdown(socket.SHUT_WR)
            except OSError:
                pass
        else:
            sock.sendall(data)
EOF
 
  chmod 0755 "$PROXY"
  python3 -m py_compile "$PROXY" || die "HTTP CONNECT 代理脚本语法错误。"
  log "HTTP CONNECT 代理已安装。"
}
 
# ------------------------------------------------------------
# SSH keys
# ------------------------------------------------------------
 
generate_keys() {
  mkdir -p "$SSH_DIR"
  chmod 0700 "$SSH_DIR"
 
  if [ ! -f "$REVERSE_KEY" ]; then
    ssh-keygen -t ed25519 -f "$REVERSE_KEY" -N '' -C "${DEVICE_ID}-reverse"
  fi
 
  chmod 0600 "$REVERSE_KEY"
  chmod 0644 "$REVERSE_KEY.pub"
  log "SSH keys 已准备。"
}
 
reverse_ssh() {
  /usr/bin/ssh \
    -T \
    -i "$REVERSE_KEY" \
    -o IdentitiesOnly=yes \
    -o BatchMode=yes \
    -o PasswordAuthentication=no \
    -o KbdInteractiveAuthentication=no \
    -o PreferredAuthentications=publickey \
    -o "ProxyCommand=$PROXY %h %p" \
    -o StrictHostKeyChecking=accept-new \
    "$UBUNTU_SSH_USER@$UBUNTU_FRP_DOMAIN" "$@"
}
 
reverse_key_works() {
  reverse_ssh 'printf REVERSE_AUTH_OK' 2>/dev/null | grep -qx REVERSE_AUTH_OK
}
 
install_reverse_key_if_needed() {
  if reverse_key_works; then
    log "Reverse SSH 公钥认证已可用。"
    return 0
  fi
 
  log "首次部署：请输入一次 Ubuntu 用户 $UBUNTU_SSH_USER 的密码。"
 
  cat "$REVERSE_KEY.pub" |
    /usr/bin/ssh \
      -T \
      -o "ProxyCommand=$PROXY %h %p" \
      -o StrictHostKeyChecking=accept-new \
      "$UBUNTU_SSH_USER@$UBUNTU_FRP_DOMAIN" \
      'umask 077
       mkdir -p "$HOME/.ssh"
       touch "$HOME/.ssh/authorized_keys"
       key="$(cat)"
       grep -qxF "$key" "$HOME/.ssh/authorized_keys" ||
         printf "%s\n" "$key" >>"$HOME/.ssh/authorized_keys"'
 
  reverse_key_works || die "Reverse SSH 公钥安装失败。"
  log "Reverse SSH 公钥认证安装完成。"
}
 
verify_ubuntu_instance() {
  remote_id="$(reverse_ssh 'cat /etc/reverse-ssh-server-id 2>/dev/null || true')"
 
  if [ "$remote_id" != "$UBUNTU_SERVER_ID" ]; then
    die "连接到错误 Ubuntu：期望 $UBUNTU_SERVER_ID，实际 ${remote_id:-unknown}"
  fi
 
  log "Ubuntu 实例核对通过：$remote_id"
}
 
remove_old_lightweight_control_channel() {
  systemctl disable --now reverse-ssh-command-poll.timer >/dev/null 2>&1 || true
  systemctl stop reverse-ssh-command-poll.service >/dev/null 2>&1 || true
 
  rm -f \
    /etc/systemd/system/reverse-ssh-command-poll.timer \
    /etc/systemd/system/reverse-ssh-command-poll.service \
    /usr/bin/reverse-ssh-command-poll
 
  reverse_ssh \
    "if [ -f \"\$HOME/.ssh/authorized_keys\" ]; then
       tmp=\"\$(mktemp)\"
       grep -vF ' ${DEVICE_ID}-control' \"\$HOME/.ssh/authorized_keys\" >\"\$tmp\" || true
       cat \"\$tmp\" >\"\$HOME/.ssh/authorized_keys\"
       rm -f \"\$tmp\"
       chmod 600 \"\$HOME/.ssh/authorized_keys\"
     fi
     rm -f \"\$HOME/.rssh-control/${DEVICE_ID}.state\"" || true
 
  rm -f "$SSH_DIR/id_ed25519_control" "$SSH_DIR/id_ed25519_control.pub"
  systemctl daemon-reload
  log "本 Gateway 的旧轻量控制通道组件已清理。"
}
 

# ------------------------------------------------------------
# Reverse SSH runtime
# ------------------------------------------------------------

install_runtime() {

cat >"$START" <<EOF
#!/bin/sh

exec /usr/bin/ssh \
  -N \
  -T \
  -i "$REVERSE_KEY" \
  -o IdentitiesOnly=yes \
  -o 'ProxyCommand=$PROXY %h %p' \
  -o BatchMode=yes \
  -o PasswordAuthentication=no \
  -o KbdInteractiveAuthentication=no \
  -o PreferredAuthentications=publickey \
  -o StrictHostKeyChecking=yes \
  -o ExitOnForwardFailure=yes \
  -o ServerAliveInterval=30 \
  -o ServerAliveCountMax=3 \
  -o ConnectTimeout=15 \
  -o StreamLocalBindUnlink=yes \
  -R '$SOCKET:127.0.0.1:$GATEWAY_SSH_PORT' \
  '$UBUNTU_SSH_USER@$UBUNTU_FRP_DOMAIN'
EOF

  chmod 0755 "$START"

cat >"$CTL" <<EOF
#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
  echo "reverse-ssh-control must be run as root." >&2
  exit 1
fi

STATE_DIR="$STATE_DIR"
STATE_FILE="\$STATE_DIR/state"

SERVICE="reverse-ssh.service"
DEFAULT_OPEN_SECONDS="$DEFAULT_OPEN_SECONDS"

mkdir -p "\$STATE_DIR"
chmod 0700 "\$STATE_DIR"

read_state() {
  MODE="closed"
  EXPIRES_AT="0"

  [ -f "\$STATE_FILE" ] && . "\$STATE_FILE"

  case "\$MODE" in
    closed|always)
      EXPIRES_AT="0"
      ;;
    timed)
      echo "\$EXPIRES_AT" | grep -Eq '^[0-9]+$' || {
        MODE="closed"
        EXPIRES_AT="0"
      }
      ;;
    *)
      MODE="closed"
      EXPIRES_AT="0"
      ;;
  esac
}

write_state() {
  tmp="\${STATE_FILE}.tmp.\$\$"
  umask 077

  printf 'MODE="%s"\nEXPIRES_AT="%s"\n' \
    "\$MODE" \
    "\$EXPIRES_AT" >"\$tmp"

  chmod 0600 "\$tmp"
  mv "\$tmp" "\$STATE_FILE"
}

cleanup_socket() {
  /usr/bin/ssh \
    -T \
    -i "$REVERSE_KEY" \
    -o IdentitiesOnly=yes \
    -o 'ProxyCommand=$PROXY %h %p' \
    -o BatchMode=yes \
    -o PasswordAuthentication=no \
    -o KbdInteractiveAuthentication=no \
    -o PreferredAuthentications=publickey \
    -o StrictHostKeyChecking=yes \
    '$UBUNTU_SSH_USER@$UBUNTU_FRP_DOMAIN' \
    "rm -f '$SOCKET'" >/dev/null 2>&1 || true
}

socket_online() {
  /usr/bin/ssh \
    -T \
    -i "$REVERSE_KEY" \
    -o IdentitiesOnly=yes \
    -o 'ProxyCommand=$PROXY %h %p' \
    -o BatchMode=yes \
    -o PasswordAuthentication=no \
    -o KbdInteractiveAuthentication=no \
    -o PreferredAuthentications=publickey \
    -o StrictHostKeyChecking=yes \
    '$UBUNTU_SSH_USER@$UBUNTU_FRP_DOMAIN' \
    "ss -xlH | grep -Fq '$SOCKET'" >/dev/null 2>&1
}

stop_tunnel() {
  cleanup_socket
  systemctl stop "\$SERVICE" >/dev/null 2>&1 || true
}

start_tunnel() {
  if ! systemctl is-active --quiet "\$SERVICE"; then
    systemctl start "\$SERVICE"
  fi

  retry=0

  while ! socket_online; do
    retry=\$((retry + 1))

    if [ "\$retry" -ge 20 ]; then
      echo "Reverse SSH failed to become online." >&2
      return 1
    fi

    sleep 1
  done
}

apply_state() {
  read_state
  now="\$(date +%s)"

  case "\$MODE" in
    timed)
      if [ "\$now" -lt "\$EXPIRES_AT" ]; then
        start_tunnel
      else
        MODE="closed"
        EXPIRES_AT="0"
        write_state
        stop_tunnel
      fi
      ;;

    always)
      start_tunnel
      ;;

    closed)
      stop_tunnel
      ;;
  esac
}

case "\${1:-status}" in
  open)
    MODE="timed"
    EXPIRES_AT="\$((\$(date +%s)+DEFAULT_OPEN_SECONDS))"
    write_state
    apply_state
    ;;

  always)
    MODE="always"
    EXPIRES_AT="0"
    write_state
    apply_state
    ;;

  close)
    MODE="closed"
    EXPIRES_AT="0"
    write_state

    # If this command arrived through Reverse SSH, the session may disappear
    # immediately. No Forward-SSH fallback is started.
    stop_tunnel

    echo "Reverse SSH: offline"
    echo "Mode: closed"
    echo "Expires epoch: 0"
    echo "Forward SSH fallback: disabled/unsupported"
    exit 0
    ;;

  health)
    apply_state
    read_state

    case "\$MODE" in
      timed|always)
        if ! socket_online; then
          systemctl restart "\$SERVICE"
          sleep 2

          if ! socket_online; then
            echo "Reverse SSH health recovery failed." >&2
            exit 1
          fi
        fi
        ;;
    esac

    exit 0
    ;;

  status)
    apply_state
    ;;

  *)
    echo "Usage: reverse-ssh-control {open|always|close|status}" >&2
    exit 2
    ;;
esac

read_state

if socket_online; then
  ONLINE="online"
else
  ONLINE="offline"
fi

echo "Reverse SSH: \$ONLINE"
echo "Mode: \$MODE"
echo "Expires epoch: \$EXPIRES_AT"
EOF

  chown root:root "$CTL"
  chmod 0700 "$CTL"
  log "Reverse SSH 运行组件已安装。"
}

# ------------------------------------------------------------
# systemd: reverse SSH + local state + health
# ------------------------------------------------------------

install_runtime_units() {

cat >"$REVERSE_SERVICE" <<EOF
[Unit]
Description=Reverse SSH for $DEVICE_ID
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
ExecStart=$START
Restart=always
RestartSec=10
EOF

cat >"$DEBUG_OPEN_SERVICE" <<EOF
[Unit]
Description=Open SKF Reverse SSH debug channel for 3 days
Wants=network-online.target
After=network-online.target
 
[Service]
Type=oneshot
User=root
Group=root
ExecStart=$CTL open
EOF

cat >"$HEALTH_SERVICE" <<EOF
[Unit]
Description=Reverse SSH health check for $DEVICE_ID

[Service]
Type=oneshot
ExecStart=$CTL health
EOF

cat >"$HEALTH_TIMER" <<EOF
[Unit]
Description=Reverse SSH health timer for $DEVICE_ID

[Timer]
OnBootSec=45
OnUnitActiveSec=${HEALTH_CHECK_SECONDS}
Unit=reverse-ssh-health.service

[Install]
WantedBy=timers.target
EOF

  systemctl daemon-reload

  systemctl disable reverse-ssh.service >/dev/null 2>&1 || true

  systemctl enable reverse-ssh-health.timer >/dev/null 2>&1
  systemctl restart reverse-ssh-health.timer

  #
  # Initial mode policy:
  #
  #   Legacy Gateway:
  #     frpc is not masked yet. Open Reverse SSH for one timed 3-day migration
  #     window so the reverse socket can be verified before frpc is retired.
  #
  #   Fresh/already-migrated Gateway:
  #     frpc is already masked. Reverse SSH stays closed until root explicitly
  #     requests a timed debug window with "reverse-ssh-control open".
  #
  # Existing explicit state is always preserved.
  #
  if [ ! -f "$STATE_DIR/state" ]; then
    mkdir -p "$STATE_DIR"
    chmod 0700 "$STATE_DIR"
  
    legacy_state="$(
      systemctl is-enabled "$LEGACY_FORWARD_SERVICE" 2>/dev/null || true
    )"
  
    if [ "$legacy_state" = "masked" ]; then
      #
      # Fresh rootfs or an already-migrated Gateway.
      #
      printf 'MODE="closed"\nEXPIRES_AT="0"\n' \
  >"$STATE_DIR/state"
  
      log "frpc 已 mask；Reverse SSH 初始状态设为 closed。"
    else
      #
      # Legacy Gateway migration.
      #
      initial_expires="$(( $(date +%s) + DEFAULT_OPEN_SECONDS ))"
  
      printf 'MODE="timed"\nEXPIRES_AT="%s"\n' \
        "$initial_expires" \
  >"$STATE_DIR/state"
  
      log "检测到旧 frpc；为安全迁移开启一次 ${DEFAULT_OPEN_SECONDS}s Reverse SSH 窗口。"
    fi
  
    chmod 0600 "$STATE_DIR/state"
  fi

  "$CTL" status >/dev/null

  log "Reverse SSH health timer 已启用；本地模式已收敛。"
}

verify_runtime() {
  systemctl is-active --quiet reverse-ssh-health.timer ||
    die "reverse-ssh-health.timer 未运行。"

  status="$("$CTL" status)"
  mode="$(echo "$status" | sed -n 's/^Mode: //p')"
  online="$(echo "$status" | sed -n 's/^Reverse SSH: //p')"

  case "$mode" in
    closed)
      [ "$online" = "offline" ] ||
        die "Mode=closed 但 Reverse SSH 仍在线：$status"
      ;;

    timed|always)
      i=0

      while [ "$i" -lt 20 ]; do
        status="$("$CTL" status)"
        online="$(echo "$status" | sed -n 's/^Reverse SSH: //p')"

        if [ "$online" = "online" ]; then
          log "Reverse SSH 本地状态/数据通道验证通过。"
          return 0
        fi

        i=$((i + 1))
        sleep 1
      done

      journalctl -u reverse-ssh.service -n 50 --no-pager || true
      die "Mode=$mode，但 Reverse SSH 未正确收敛：$status"
      ;;

    *)
      die "未知本地模式：$mode"
      ;;
  esac

  log "Reverse SSH closed 状态验证通过。"
}

# One-time retirement only; this is NOT a fallback path.
#
# Safety rule:
#   Never touch the legacy Forward-SSH transport until Reverse SSH has been
#   verified online. When retiring frpc, mask it BEFORE stopping the running
#   service so an installer that is itself using frpc cannot be left in a
#   half-retired state if that connection disappears.
retire_legacy_forward_transport() {
  systemctl disable --now legacy-frpc-disable.timer >/dev/null 2>&1 || true
  systemctl stop legacy-frpc-disable.service >/dev/null 2>&1 || true

  rm -f \
    /etc/systemd/system/legacy-frpc-disable.timer \
    /etc/systemd/system/legacy-frpc-disable.service \
    /usr/bin/disable-legacy-frpc.sh \
    /var/lib/reverse-ssh-control/legacy-frpc-disable-deadline

  systemctl daemon-reload

  #
  # Gate 1: the Reverse SSH socket must really be online.
  #
  status="$("$CTL" status 2>/dev/null || true)"
  online="$(echo "$status" | sed -n 's/^Reverse SSH: //p')"

  if [ "$online" != "online" ]; then
    log "Reverse SSH 尚未 online；为避免迁移失联，本次不关闭旧 frpc。"
    log "执行 reverse-ssh-control always 后再次运行本脚本即可完成淘汰。"
    return 0
  fi

  #
  # Gate 2: the local Reverse SSH service must also be running.
  #
  if ! systemctl is-active --quiet reverse-ssh.service; then
    log "Reverse SSH socket 显示 online，但 reverse-ssh.service 未运行。"
    log "为避免误切断当前远程连接，本次不关闭旧 frpc。"
    return 0
  fi

  legacy_enabled="$(systemctl is-enabled "$LEGACY_FORWARD_SERVICE" 2>/dev/null || true)"
  legacy_active="$(systemctl is-active "$LEGACY_FORWARD_SERVICE" 2>/dev/null || true)"
  legacy_fragment="$(
    systemctl show "$LEGACY_FORWARD_SERVICE" -p FragmentPath 2>/dev/null |
      sed -n 's/^FragmentPath=//p'
  )"

  #
  # Idempotency: if the unit is already persistently masked, only make sure a
  # leftover running instance is asked to stop.
  #
  if [ "$legacy_enabled" = "masked" ]; then
    if [ "$legacy_active" = "active" ] || [ "$legacy_active" = "activating" ]; then
      log "$LEGACY_FORWARD_SERVICE 已 mask，但仍在运行；提交 stop 任务。"
      systemctl --no-block stop "$LEGACY_FORWARD_SERVICE" >/dev/null 2>&1 ||
        die "无法停止已 mask 的 $LEGACY_FORWARD_SERVICE。"
    else
      log "$LEGACY_FORWARD_SERVICE 已是 masked + ${legacy_active:-unknown}；无需重复淘汰。"
    fi
    return 0
  fi

  #
  # Persistently back up the old unit before changing it.  /var/lib is used
  # deliberately so the backup is not placed in a temporary/cleanup-prone
  # directory.
  #
  backup_root="$STATE_DIR/legacy-forward-backup"
  backup_stamp="$(date '+%Y%m%d-%H%M%S')-$$"
  backup_dir="$backup_root/$backup_stamp"
  etc_unit="/etc/systemd/system/$LEGACY_FORWARD_SERVICE"
  etc_dropin="/etc/systemd/system/${LEGACY_FORWARD_SERVICE}.d"

  mkdir -p "$backup_dir"
  chmod 0700 "$backup_root" "$backup_dir"

  {
    echo "service=$LEGACY_FORWARD_SERVICE"
    echo "fragment=${legacy_fragment:-not-found}"
    echo "enabled_state=${legacy_enabled:-unknown}"
    echo "active_state=${legacy_active:-unknown}"
    echo "backup_time=$(date '+%Y-%m-%dT%H:%M:%S%z')"
  } >"$backup_dir/state.txt"
  chmod 0600 "$backup_dir/state.txt"

  systemctl cat "$LEGACY_FORWARD_SERVICE" \
    >"$backup_dir/systemctl-cat.txt" 2>/dev/null || true
  chmod 0600 "$backup_dir/systemctl-cat.txt"

  had_etc_unit=0
  had_etc_dropin=0

  if [ -e "$etc_unit" ] || [ -L "$etc_unit" ]; then
    cp -a "$etc_unit" "$backup_dir/frpc.service"
    had_etc_unit=1
  fi

  if [ -d "$etc_dropin" ]; then
    cp -a "$etc_dropin" "$backup_dir/frpc.service.d"
    had_etc_dropin=1
  fi

  log "旧 $LEGACY_FORWARD_SERVICE 已备份到 $backup_dir"

  #
  # Disable first so no boot target keeps a wants/ symlink to the old unit.
  # Do NOT stop the running frpc yet: this setup may itself be running through
  # that connection.
  #
  systemctl disable "$LEGACY_FORWARD_SERVICE" >/dev/null 2>&1 || true
  rm -f "/etc/systemd/system/multi-user.target.wants/$LEGACY_FORWARD_SERVICE"

  #
  # A legacy Gateway can have /etc/systemd/system/frpc.service as a normal
  # file.  systemctl mask cannot replace that file, so back it up above and
  # remove the /etc copy (and its local drop-ins) before creating the mask.
  # Vendor units under /lib or /usr/lib are intentionally left untouched:
  # the /etc mask overrides them.
  #
  rm -f "$etc_unit"
  rm -rf "$etc_dropin"

  if ! systemctl mask "$LEGACY_FORWARD_SERVICE" >/dev/null 2>&1; then
    #
    # Masking failed while frpc is still running. Roll back the /etc unit and
    # enablement state so the current Forward SSH path is not stranded.
    #
    rm -f "$etc_unit"

    if [ "$had_etc_unit" -eq 1 ]; then
      cp -a "$backup_dir/frpc.service" "$etc_unit"
    fi

    if [ "$had_etc_dropin" -eq 1 ]; then
      rm -rf "$etc_dropin"
      cp -a "$backup_dir/frpc.service.d" "$etc_dropin"
    fi

    systemctl daemon-reload

    case "$legacy_enabled" in
      enabled|enabled-runtime|linked|linked-runtime)
        systemctl enable "$LEGACY_FORWARD_SERVICE" >/dev/null 2>&1 || true
        ;;
    esac

    die "无法 mask $LEGACY_FORWARD_SERVICE；旧 frpc unit 已回滚，当前通道未主动停止。"
  fi

  systemctl daemon-reload

  masked_state="$(systemctl is-enabled "$LEGACY_FORWARD_SERVICE" 2>/dev/null || true)"
  if [ "$masked_state" != "masked" ]; then
    #
    # This should be extremely rare. Since frpc has not been stopped yet,
    # restore the previous /etc unit before failing.
    #
    systemctl unmask "$LEGACY_FORWARD_SERVICE" >/dev/null 2>&1 || true
    rm -f "$etc_unit"

    if [ "$had_etc_unit" -eq 1 ]; then
      cp -a "$backup_dir/frpc.service" "$etc_unit"
    fi

    if [ "$had_etc_dropin" -eq 1 ]; then
      rm -rf "$etc_dropin"
      cp -a "$backup_dir/frpc.service.d" "$etc_dropin"
    fi

    systemctl daemon-reload

    case "$legacy_enabled" in
      enabled|enabled-runtime|linked|linked-runtime)
        systemctl enable "$LEGACY_FORWARD_SERVICE" >/dev/null 2>&1 || true
        ;;
    esac

    die "$LEGACY_FORWARD_SERVICE mask 状态验证失败；旧配置已回滚，当前通道未主动停止。"
  fi

  #
  # The irreversible remote-connectivity step is deliberately LAST.
  # --no-block hands the stop job to systemd before this shell can disappear.
  #
  if [ "$legacy_active" = "active" ] || [ "$legacy_active" = "activating" ]; then
    log "Reverse SSH 已确认 online；$LEGACY_FORWARD_SERVICE 已 disable + mask。"
    log "现在提交旧 frpc stop 任务；如果当前 shell 正通过 Forward SSH，连接可能立即断开。"

    systemctl --no-block stop "$LEGACY_FORWARD_SERVICE" >/dev/null 2>&1 ||
      die "$LEGACY_FORWARD_SERVICE 已 mask，但 stop 任务提交失败；请通过 Reverse SSH 检查。"
  else
    log "旧 $LEGACY_FORWARD_SERVICE 已 disable + mask，且当前状态为 ${legacy_active:-unknown}。"
  fi

  log "旧正向 frpc 已完成永久淘汰；reverse-ssh-control 不会恢复它。"
}

summary() {
  echo
  echo "================ Gateway reverse-ssh is ready ================"
  echo "Device ID       : $DEVICE_ID"
  echo "Ubuntu ID       : $UBUNTU_SERVER_ID"
  echo "Health checking : ${HEALTH_CHECK_SECONDS}s"
  echo
  echo "访问模式："
  echo "  closed  = Reverse SSH OFF; no automatic Forward-SSH fallback"
  echo "  timed   = Reverse SSH ON until expiry"
  echo "  always  = Reverse SSH ON persistently"
  echo
  echo "Gateway root 本地控制："
  echo "  reverse-ssh-control open"
  echo "  reverse-ssh-control always"
  echo "  reverse-ssh-control close"
  echo "  reverse-ssh-control status"
  echo
  echo "Reverse SSH 在线时，Ubuntu 可执行："
  echo "  gwctl $DEVICE_ID open"
  echo "  gwctl $DEVICE_ID always"
  echo "  gwctl $DEVICE_ID status"
  echo "  gwctl $DEVICE_ID close"
  echo "  gwlist"
  echo "  gwssh $DEVICE_ID"
  echo
  echo "注意：Ubuntu 侧 gwssh/gwctl 应登录 Gateway root。"
  echo "      skfgw 只运行 Gateway 应用，不参与 Reverse SSH。"
  echo "      close 会切断 Reverse SSH，且不会自动恢复正向 SSH。"
  echo "==============================================================="
}

main() {
  require_root

  log "[1/9] 检查配置"
  validate

  log "[2/9] 部署 Reverse SSH"

  log "[3/9] 安装 HTTP CONNECT ProxyCommand"
  install_proxy

  log "[4/9] 生成/检查 SSH keys"
  generate_keys

  log "[5/9] 配置 Reverse SSH 公钥认证"
  install_reverse_key_if_needed

  log "[6/9] 核对 Ubuntu 实例并清理旧轻量控制通道"
  verify_ubuntu_instance
  remove_old_lightweight_control_channel

  log "[7/9] 安装 Reverse SSH 本地状态运行组件"
  install_runtime
  install_runtime_units

  log "[8/9] 验证 Reverse SSH 状态"
  verify_runtime

  log "[9/9] Reverse SSH online 后淘汰旧正向 frpc 通道"
  retire_legacy_forward_transport

  summary
}

main "$@"
