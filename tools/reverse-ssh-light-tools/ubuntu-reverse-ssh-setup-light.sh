#!/usr/bin/env bash
set -Eeuo pipefail
 
# Author: hongkun.chen@skf.com
 
# ============================================================
# Ubuntu 调试服务器：全局配置区
# ============================================================
 
FRP_VERSION="0.58.1"
FRP_SERVER_ADDR="47.117.161.234"
FRP_SERVER_PORT="31700"
 
# 通常先正向 SSH 登录到 Gateway 执行：
# grep 'auth.token' /home/root/frp/frpc.ini
FRP_AUTH_TOKEN="ixohjeeb5useikusohLev6Keig7quoov"
 
UBUNTU_SSH_USER="forlinx"
GATEWAY_SSH_USER="root"
SOCKET_DIR_NAME=".rssh"
 
FRPC_BIN="/usr/local/bin/frpc"
FRPC_CONFIG="/etc/frp/frpc.toml"
FRPC_SERVICE="/etc/systemd/system/frpc-forlinx.service"
SERVER_ID_FILE="/etc/reverse-ssh-server-id"
 
# ============================================================
# 以下通常无需修改
# ============================================================
 
SERVER_ID=""
FRP_CUSTOM_DOMAIN=""
FRP_PROXY_NAME=""
 
log(){ printf '[Ubuntu] %s\n' "$*"; }
die(){ printf '[Ubuntu][ERROR] %s\n' "$*" >&2; exit 1; }
 
on_err(){
  local rc=$?
  printf '[Ubuntu][ERROR] line=%s command=%s rc=%s\n' \
    "${BASH_LINENO[0]:-?}" "${BASH_COMMAND:-?}" "$rc" >&2
  exit "$rc"
}
trap on_err ERR
 
require_root(){
  if [[ "$EUID" -ne 0 ]]; then
    die "请使用 sudo 执行：sudo $0"
  fi
  return 0
}
 
home_of(){
  getent passwd "$UBUNTU_SSH_USER" | cut -d: -f6
}
 
generate_server_id(){
  if [[ -s "$SERVER_ID_FILE" ]]; then
    SERVER_ID="$(tr -d '[:space:]' <"$SERVER_ID_FILE")"
  else
    local seed hash
    if [[ -r /proc/sys/kernel/random/uuid ]]; then
      seed="$(cat /proc/sys/kernel/random/uuid)"
    elif command -v uuidgen >/dev/null 2>&1; then
      seed="$(uuidgen)"
    else
      seed="$(date +%s%N)-$$-$(hostname)-$RANDOM"
    fi
 
    if command -v sha256sum >/dev/null 2>&1; then
      hash="$(printf '%s' "$seed" | sha256sum | awk '{print $1}')"
    elif command -v openssl >/dev/null 2>&1; then
      hash="$(printf '%s' "$seed" | openssl dgst -sha256 | awk '{print $NF}')"
    else
      die "缺少 sha256sum/openssl，无法生成持久唯一 SERVER_ID。"
    fi
 
    SERVER_ID="debug-ubuntu-${hash:0:12}"
    printf '%s\n' "$SERVER_ID" >"$SERVER_ID_FILE"
    chmod 0644 "$SERVER_ID_FILE"
  fi
 
  if [[ ! "$SERVER_ID" =~ ^[A-Za-z0-9._-]+$ ]]; then
    die "SERVER_ID_FILE 内容非法：$SERVER_ID"
  fi
 
  FRP_CUSTOM_DOMAIN="rss-${SERVER_ID}"
  FRP_PROXY_NAME="ssh-${SERVER_ID}"
 
  log "SERVER_ID: $SERVER_ID"
  log "FRP target: $FRP_CUSTOM_DOMAIN"
  return 0
}
 
validate(){
  if [[ "$FRP_AUTH_TOKEN" == "REPLACE_WITH_REAL_FRP_TOKEN" ]]; then
    die "请先填写 FRP_AUTH_TOKEN。"
  fi
 
  if ! id "$UBUNTU_SSH_USER" >/dev/null 2>&1; then
    die "用户不存在：$UBUNTU_SSH_USER"
  fi
 
  if [[ ! "$GATEWAY_SSH_USER" =~ ^[A-Za-z_][A-Za-z0-9_-]*$ ]]; then
    die "GATEWAY_SSH_USER 非法：$GATEWAY_SSH_USER"
  fi
 
  local c
  for c in systemctl ss python3 tar; do
    if ! command -v "$c" >/dev/null 2>&1; then
      die "缺少 $c"
    fi
  done
  return 0
}
 
configure_sshd_streamlocal(){
  local sshd_bin
  sshd_bin="$(command -v sshd || true)"
  [[ -n "$sshd_bin" ]] || sshd_bin="/usr/sbin/sshd"
  [[ -x "$sshd_bin" ]] || die "未找到 sshd。"
 
  install -d -m 0755 /etc/ssh/sshd_config.d
  cat >/etc/ssh/sshd_config.d/90-reverse-ssh.conf <<EOF
Match User $UBUNTU_SSH_USER
    AllowStreamLocalForwarding remote
    StreamLocalBindUnlink yes
EOF
 
  "$sshd_bin" -t || {
    rm -f /etc/ssh/sshd_config.d/90-reverse-ssh.conf
    die "sshd 配置校验失败。"
  }
 
  if systemctl cat ssh.service >/dev/null 2>&1; then
      systemctl restart ssh.service
  elif systemctl cat sshd.service >/dev/null 2>&1; then
      systemctl restart sshd.service
  else
      die "找不到 ssh.service/sshd.service。"
  fi
 
  log "sshd StreamLocal remote forwarding 已配置。"
  return 0
}
 
check_sshd(){
  local sshd_bin effective
  sshd_bin="$(command -v sshd || true)"
  [[ -n "$sshd_bin" ]] || sshd_bin="/usr/sbin/sshd"
  [[ -x "$sshd_bin" ]] || die "未找到 sshd。"
 
  if ! ss -lntH | awk '{print $4}' | grep -Eq '(^|:|\])22$'; then
    die "Ubuntu SSH :22 未监听。"
  fi
 
  effective="$("$sshd_bin" -T \
    -C user="$UBUNTU_SSH_USER",addr=127.0.0.1,host=localhost \
    2>/dev/null || true)"
 
  grep -q '^allowstreamlocalforwarding remote$' <<<"$effective" ||
    die "AllowStreamLocalForwarding remote 未生效。"
 
  grep -q '^streamlocalbindunlink yes$' <<<"$effective" ||
    die "StreamLocalBindUnlink yes 未生效。"
 
  if grep -q '^disableforwarding yes$' <<<"$effective"; then
    die "sshd DisableForwarding=yes。"
  fi
 
  log "Ubuntu sshd 检查通过。"
  return 0
}
 
arch(){
  case "$(uname -m)" in
    x86_64) echo amd64 ;;
    aarch64|arm64) echo arm64 ;;
    armv7l|armv7*) echo arm ;;
    *) die "不支持架构：$(uname -m)" ;;
  esac
}
 
download(){
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 --connect-timeout 15 "$1" -o "$2"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$2" "$1"
  else
    die "缺少 curl/wget"
  fi
}
 
install_frpc(){
  if [[ -x "$FRPC_BIN" ]] &&
     [[ "$("$FRPC_BIN" -v 2>/dev/null || true)" == "$FRP_VERSION" ]]; then
    log "frpc $FRP_VERSION 已安装。"
    return 0
  fi
 
  local a pkg tmp url
  a="$(arch)"
  pkg="frp_${FRP_VERSION}_linux_${a}.tar.gz"
  url="https://github.com/fatedier/frp/releases/download/v${FRP_VERSION}/${pkg}"
  tmp="$(mktemp -d)"
 
  log "安装 frpc $FRP_VERSION ..."
  download "$url" "$tmp/$pkg"
  tar -xzf "$tmp/$pkg" -C "$tmp"
  install -m 0755 "$tmp/frp_${FRP_VERSION}_linux_${a}/frpc" "$FRPC_BIN"
  rm -rf "$tmp"
 
  if [[ "$("$FRPC_BIN" -v)" != "$FRP_VERSION" ]]; then
    die "frpc 版本校验失败。"
  fi
  return 0
}
 
configure_dirs(){
  local home
  home="$(home_of)"
  [[ -n "$home" ]] || die "无法确定 $UBUNTU_SSH_USER HOME。"
 
  install -d -m 0700 -o "$UBUNTU_SSH_USER" -g "$UBUNTU_SSH_USER" \
    "$home/$SOCKET_DIR_NAME"
  return 0
}
 
configure_frpc(){
  install -d -m 0755 "$(dirname "$FRPC_CONFIG")"
 
  cat >"$FRPC_CONFIG" <<EOF
serverAddr = "$FRP_SERVER_ADDR"
serverPort = $FRP_SERVER_PORT
 
auth.token = "$FRP_AUTH_TOKEN"
 
[[proxies]]
name = "$FRP_PROXY_NAME"
type = "tcpmux"
multiplexer = "httpconnect"
customDomains = ["$FRP_CUSTOM_DOMAIN"]
localIP = "127.0.0.1"
localPort = 22
EOF
 
  chmod 0600 "$FRPC_CONFIG"
 
  cat >"$FRPC_SERVICE" <<EOF
[Unit]
Description=FRP Client for Reverse SSH Ubuntu
Wants=network-online.target
After=network-online.target
 
[Service]
Type=simple
ExecStart=$FRPC_BIN -c $FRPC_CONFIG
Restart=always
RestartSec=5
 
[Install]
WantedBy=multi-user.target
EOF
 
  systemctl daemon-reload
  systemctl enable frpc-forlinx.service >/dev/null 2>&1
  systemctl restart frpc-forlinx.service
  log "frpc-forlinx.service 已启动。"
  return 0
}
 
wait_frp(){
  local i pid logs
  log "等待 FRP 路由注册..."
 
  for i in $(seq 1 40); do
    if systemctl is-active --quiet frpc-forlinx.service; then
      pid="$(systemctl show frpc-forlinx.service -p MainPID --value 2>/dev/null || true)"
      if [[ -n "$pid" && "$pid" != "0" ]]; then
        logs="$(journalctl _PID="$pid" -n 80 --no-pager 2>/dev/null || true)"
        if grep -q 'router config conflict' <<<"$logs"; then
          die "FRP 路由冲突：$FRP_CUSTOM_DOMAIN 已被占用。"
        fi
        if grep -qE 'start proxy success|proxy added' <<<"$logs"; then
          log "FRP 注册成功：$FRP_CUSTOM_DOMAIN"
          return 0
        fi
      fi
    fi
    sleep 1
  done
 
  journalctl -u frpc-forlinx.service -n 100 --no-pager || true
  die "FRP 注册失败。"
}
 
 
install_tools(){
  local home socket_dir auth_file
  home="$(home_of)"
  socket_dir="$home/$SOCKET_DIR_NAME"
  install -d -m 0700 -o "$UBUNTU_SSH_USER" -g "$UBUNTU_SSH_USER" \
    "$socket_dir/.gwlist-state"
 
  # 迁移兼容：
  # 每个 Gateway 的脚本会单独删除自己的 <DEVICE_ID>-control key/state。
  # Ubuntu 这里不粗暴删除整个 .rssh-control，避免误伤尚未升级的其他 Gateway。
  #
  # 只有 authorized_keys 中已经没有任何旧 *-control key 时，
  # 才删除旧 rssh-control-server 和空的 .rssh-control 目录。
  auth_file="$home/.ssh/authorized_keys"
 
  if [[ ! -f "$auth_file" ]] ||
     ! grep -Eq ' [A-Za-z0-9._-]+-control([[:space:]]|$)' "$auth_file"; then
    rm -f /usr/local/bin/rssh-control-server
 
    if [[ -d "$home/.rssh-control" ]]; then
      find "$home/.rssh-control" \
        -maxdepth 1 -type f -name '*.state' -delete 2>/dev/null || true
      rmdir "$home/.rssh-control" 2>/dev/null || true
    fi
  fi
 
  # gwctl:
  cat >/usr/local/bin/gwctl <<EOF
#!/usr/bin/env bash
set -euo pipefail

ID="\${1:-}"
ACT="\${2:-}"

[[ "\$ID" =~ ^[A-Za-z0-9._-]+\$ ]] || {
  echo "Usage: gwctl DEVICE_ID {open|always|close|status}" >&2
  exit 2
}

case "\$ACT" in
  open|always|close|status) ;;
  *)
    echo "Usage: gwctl DEVICE_ID {open|always|close|status}" >&2
    exit 2
    ;;
esac

S="$socket_dir/\${ID}.sock"
CACHE="/usr/local/bin/rssh-state-cache"

if [[ ! -S "\$S" ]]; then
  echo "Gateway \$ID is offline on Reverse SSH." >&2
  echo "If it is in Forward SSH mode, connect through the normal/FRP SSH path." >&2
  exit 1
fi

if "\$CACHE" current "\$ID" "\$S"; then
  exec ssh \
    -T \
    -o "ProxyCommand=/usr/local/bin/rssh-unix-proxy.py \$S" \
    -o "HostKeyAlias=\${ID}-reverse" \
    root@"\$ID" \
    "/usr/bin/reverse-ssh-control \$ACT"
fi

CONTROL="$socket_dir/.gwlist-control-\${ID}-\$\$"

if ssh \
  -T \
  -o ControlMaster=auto \
  -o ControlPersist=15s \
  -o "ControlPath=\$CONTROL" \
  -o "ProxyCommand=/usr/local/bin/rssh-unix-proxy.py \$S" \
  -o "HostKeyAlias=\${ID}-reverse" \
  root@"\$ID" \
  "/usr/bin/reverse-ssh-control \$ACT"
then
  rc=0
else
  rc=\$?
fi

"\$CACHE" sync "\$ID" "\$S" "\$CONTROL" >/dev/null 2>&1 || true
ssh -S "\$CONTROL" -O exit root@"\$ID" >/dev/null 2>&1 || true
rm -f "\$CONTROL"

exit "\$rc"
EOF
 
  chmod 0755 /usr/local/bin/gwctl
 
  cat >/usr/local/bin/rssh-unix-proxy.py <<'PY'
#!/usr/bin/env python3
 
import os
import select
import socket
import sys
 
if len(sys.argv) != 2:
    raise SystemExit(2)
 
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sys.argv[1])
 
stdin_fd = sys.stdin.fileno()
stdout_fd = sys.stdout.fileno()
stdin_open = True
 
while True:
    readable, _, _ = select.select(
        [s] + ([stdin_fd] if stdin_open else []),
        [],
        [],
    )
 
    if s in readable:
        data = s.recv(65536)
        if not data:
            raise SystemExit(0)
        os.write(stdout_fd, data)
 
    if stdin_open and stdin_fd in readable:
        data = os.read(stdin_fd, 65536)
        if not data:
            stdin_open = False
            try:
                s.shutdown(socket.SHUT_WR)
            except OSError:
                pass
        else:
            s.sendall(data)
PY
 
  chmod 0755 /usr/local/bin/rssh-unix-proxy.py
  python3 -m py_compile /usr/local/bin/rssh-unix-proxy.py

  cat >/usr/local/bin/rssh-state-cache <<EOF
#!/usr/bin/env bash
set -euo pipefail

SOCKET_DIR="$socket_dir"
CACHE_DIR="\$SOCKET_DIR/.gwlist-state"
CACHE_OWNER="$UBUNTU_SSH_USER"

valid_id() {
  [[ "\${1:-}" =~ ^[A-Za-z0-9._-]+\$ ]]
}

session_key() {
  local socket="\$1" token

  token="\$(
    ss -xlH | awk -v p="\$socket" '
      {
        for (i = 1; i <= NF; i++) {
          if (\$i == p) {
            print \$(i + 1)
            exit
          }
        }
      }
    '
  )"

  [[ -n "\$token" ]] || return 1
  printf '%s\n' "\$token"
}

cache_file() {
  printf '%s/%s.state\n' "\$CACHE_DIR" "\$1"
}

cache_is_current() {
  local id="\$1" socket="\$2" key file stored

  [[ -S "\$socket" ]] || return 1
  key="\$(session_key "\$socket")" || return 1
  file="\$(cache_file "\$id")"
  [[ -f "\$file" ]] || return 1

  stored="\$(sed -n 's/^SESSION_KEY="\([^"]*\)"\$/\1/p' "\$file" | tail -n1)"
  [[ -n "\$stored" && "\$stored" == "\$key" ]]
}

sync_from_master() {
  local id="\$1" socket="\$2" control
  local lock got_lock i key_before key_after state mode expires file tmp

  control="\${3:-}"

  cache_is_current "\$id" "\$socket" && return 0
  [[ -S "\$socket" ]] || return 1
  [[ -n "\$control" ]] || return 1

  if [[ "\$(id -u)" -eq 0 ]]; then
    install -d -m 0700 -o "\$CACHE_OWNER" -g "\$CACHE_OWNER" "\$CACHE_DIR"
  else
    install -d -m 0700 "\$CACHE_DIR"
  fi

  lock="\$CACHE_DIR/.\${id}.lock"
  got_lock=0
  i=0

  while (( i < 50 )); do
    if cache_is_current "\$id" "\$socket"; then
      return 0
    fi

    if mkdir "\$lock" 2>/dev/null; then
      got_lock=1
      break
    fi

    sleep 0.1
    i=\$((i + 1))
  done

  (( got_lock == 1 )) || return 0

  cleanup_lock() {
    rmdir "\$lock" 2>/dev/null || true
  }
  trap cleanup_lock EXIT HUP INT TERM

  key_before="\$(session_key "\$socket")" || return 1

  ssh \
    -S "\$control" \
    -O check \
    root@"\$id" >/dev/null 2>&1 || return 1

  state="\$(
    ssh \
      -T \
      -S "\$control" \
      -o BatchMode=yes \
      -o ControlMaster=no \
      root@"\$id" \
      'cat /var/lib/reverse-ssh-control/state 2>/dev/null'
  )" || return 1

  mode="\$(sed -n 's/^MODE="\([^"]*\)"\$/\1/p' <<<"\$state" | tail -n1)"
  expires="\$(sed -n 's/^EXPIRES_AT="\([0-9][0-9]*\)"\$/\1/p' <<<"\$state" | tail -n1)"

  case "\$mode" in
    timed|always|closed) ;;
    *) return 1 ;;
  esac

  [[ "\$expires" =~ ^[0-9]+\$ ]] || return 1

  key_after="\$(session_key "\$socket")" || return 1
  [[ "\$key_after" == "\$key_before" ]] || return 1

  file="\$(cache_file "\$id")"
  tmp="\${file}.tmp.\$\$"
  umask 077

  printf 'SESSION_KEY="%s"\nMODE="%s"\nEXPIRES_AT="%s"\n' \
    "\$key_after" \
    "\$mode" \
    "\$expires" >"\$tmp"

  chmod 0600 "\$tmp"

  if [[ "\$(id -u)" -eq 0 ]]; then
    chown "\$CACHE_OWNER:\$CACHE_OWNER" "\$tmp" 2>/dev/null || true
  fi

  mv -f "\$tmp" "\$file"

  cleanup_lock
  trap - EXIT HUP INT TERM
  return 0
}

cmd="\${1:-}"
id="\${2:-}"
socket="\${3:-}"

valid_id "\$id" || exit 2

case "\$cmd" in
  current)
    [[ \$# -eq 3 ]] || exit 2
    cache_is_current "\$id" "\$socket"
    ;;
  sync)
    [[ \$# -eq 4 ]] || exit 2
    sync_from_master "\$id" "\$socket" "\$4"
    ;;
  *)
    exit 2
    ;;
esac
EOF

  chmod 0755 /usr/local/bin/rssh-state-cache
 
  # gwssh：Reverse SSH 调试 shell 使用普通维护用户 root。
  cat >/usr/local/bin/gwssh <<EOF
#!/usr/bin/env bash
set -euo pipefail

ID="\${1:-}"

[[ "\$ID" =~ ^[A-Za-z0-9._-]+\$ ]] || {
  echo "Usage: gwssh DEVICE_ID" >&2
  exit 2
}

S="$socket_dir/\${ID}.sock"
CACHE="/usr/local/bin/rssh-state-cache"

if [[ ! -S "\$S" ]]; then
  echo "Gateway \$ID is offline on Reverse SSH." >&2
  echo "If it is in Forward SSH mode, connect through the normal/FRP SSH path." >&2
  exit 1
fi

if "\$CACHE" current "\$ID" "\$S"; then
  exec ssh \
    -o "ProxyCommand=/usr/local/bin/rssh-unix-proxy.py \$S" \
    -o "HostKeyAlias=\${ID}-reverse" \
    root@"\$ID"
fi

CONTROL="$socket_dir/.gwlist-control-\${ID}-\$\$"

if ssh \
  -o ControlMaster=auto \
  -o ControlPersist=15s \
  -o "ControlPath=\$CONTROL" \
  -o "ProxyCommand=/usr/local/bin/rssh-unix-proxy.py \$S" \
  -o "HostKeyAlias=\${ID}-reverse" \
  root@"\$ID"
then
  rc=0
else
  rc=\$?
fi

"\$CACHE" sync "\$ID" "\$S" "\$CONTROL" >/dev/null 2>&1 || true
ssh -S "\$CONTROL" -O exit root@"\$ID" >/dev/null 2>&1 || true
rm -f "\$CONTROL"

exit "\$rc"
EOF
 
  chmod 0755 /usr/local/bin/gwssh
  
  cat >/usr/local/bin/gwscp <<EOF
#!/usr/bin/env bash
set -Eeuo pipefail

SOCKET_DIR="$socket_dir"
PROXY="/usr/local/bin/rssh-unix-proxy.py"
CACHE="/usr/local/bin/rssh-state-cache"

usage() {
    cat >&2 <<'USAGE'
Usage:
  gwscp DEVICE_ID LOCAL_FILE REMOTE_PATH
  gwscp DEVICE_ID REMOTE_PATH LOCAL_FILE --download

Upload example:
  gwscp bulletgw120120 \
    /home/forlinx/GW/GW/skf_gateway_install/skf_package.tar.gz \
    /home/root/skf-validation/package/

Download example:
  gwscp bulletgw120120 \
    /home/root/example.log \
    /home/forlinx/ \
    --download
USAGE
    exit 2
}

ID="\${1:-}"
SOURCE="\${2:-}"
DESTINATION="\${3:-}"
MODE="\${4:-upload}"

[[ -n "\$ID" && -n "\$SOURCE" && -n "\$DESTINATION" ]] || usage

if [[ ! "\$ID" =~ ^[A-Za-z0-9._-]+\$ ]]; then
    echo "Invalid DEVICE_ID: \$ID" >&2
    exit 2
fi

SOCKET="\$SOCKET_DIR/\${ID}.sock"

if [[ ! -x "\$PROXY" ]]; then
    echo "Reverse SSH proxy is missing or not executable: \$PROXY" >&2
    exit 1
fi

if [[ ! -S "\$SOCKET" ]]; then
    echo "Gateway \$ID is not currently available." >&2
    echo "Unix socket does not exist: \$SOCKET" >&2
    echo "Check with: gwlist" >&2
    exit 1
fi

SSH_OPTIONS=(
    -o "ProxyCommand=\$PROXY \$SOCKET"
    -o "HostKeyAlias=\${ID}-reverse"
)

run_scp() {
    case "\$MODE" in
        upload)
            if [[ ! -e "\$SOURCE" ]]; then
                echo "Local source does not exist: \$SOURCE" >&2
                exit 1
            fi

            scp -O \
                "\${SSH_OPTIONS[@]}" \
                "\$@" \
                -- \
                "\$SOURCE" \
                "root@\${ID}:\$DESTINATION"
            ;;

        --download|download)
            scp -O \
                "\${SSH_OPTIONS[@]}" \
                "\$@" \
                -- \
                "root@\${ID}:\$SOURCE" \
                "\$DESTINATION"
            ;;

        *)
            usage
            ;;
    esac
}

if "\$CACHE" current "\$ID" "\$SOCKET"; then
    case "\$MODE" in
        upload)
            if [[ ! -e "\$SOURCE" ]]; then
                echo "Local source does not exist: \$SOURCE" >&2
                exit 1
            fi

            exec scp -O \
                "\${SSH_OPTIONS[@]}" \
                -- \
                "\$SOURCE" \
                "root@\${ID}:\$DESTINATION"
            ;;

        --download|download)
            exec scp -O \
                "\${SSH_OPTIONS[@]}" \
                -- \
                "root@\${ID}:\$SOURCE" \
                "\$DESTINATION"
            ;;

        *)
            usage
            ;;
    esac
fi

CONTROL="\$SOCKET_DIR/.gwlist-control-\${ID}-\$\$"
CONTROL_OPTIONS=(
    -o ControlMaster=auto
    -o ControlPersist=15s
    -o "ControlPath=\$CONTROL"
)

if run_scp "\${CONTROL_OPTIONS[@]}"; then
    rc=0
else
    rc=\$?
fi

"\$CACHE" sync "\$ID" "\$SOCKET" "\$CONTROL" >/dev/null 2>&1 || true
ssh -S "\$CONTROL" -O exit root@"\$ID" >/dev/null 2>&1 || true
rm -f "\$CONTROL"

exit "\$rc"
EOF

  chmod 0755 /usr/local/bin/gwscp

  cat >/usr/local/bin/gwlist <<EOF
#!/usr/bin/env bash
set -euo pipefail

socket_dir="$socket_dir"
cache_dir="\$socket_dir/.gwlist-state"
cache_helper="/usr/local/bin/rssh-state-cache"

format_remaining() {
  local total="\$1" days hours minutes seconds

  (( total < 0 )) && total=0

  days=\$((total / 86400))
  hours=\$(((total % 86400) / 3600))
  minutes=\$(((total % 3600) / 60))
  seconds=\$((total % 60))

  if (( days > 0 )); then
    printf '%dd %02dh %02dm %02ds' "\$days" "\$hours" "\$minutes" "\$seconds"
  elif (( hours > 0 )); then
    printf '%dh %02dm %02ds' "\$hours" "\$minutes" "\$seconds"
  elif (( minutes > 0 )); then
    printf '%dm %02ds' "\$minutes" "\$seconds"
  else
    printf '%ds' "\$seconds"
  fi
}

printf "%-32s %-8s %s\n" DEVICE_ID STATE AUTO_CLOSE_IN

found=0

while IFS= read -r line; do
  p="\$(grep -o "\$socket_dir/[A-Za-z0-9._-]*\.sock" <<<"\$line" | head -n1 || true)"
  [[ -n "\$p" ]] || continue

  id="\$(basename "\$p" .sock)"
  auto_close_in="unknown"

  if "\$cache_helper" current "\$id" "\$p"; then
    state="\$(cat "\$cache_dir/\${id}.state" 2>/dev/null || true)"
    mode="\$(sed -n 's/^MODE="\([^"]*\)"\$/\1/p' <<<"\$state" | tail -n1)"
    expires_at="\$(sed -n 's/^EXPIRES_AT="\([0-9][0-9]*\)"\$/\1/p' <<<"\$state" | tail -n1)"

    case "\$mode" in
      timed)
        if [[ "\$expires_at" =~ ^[0-9]+\$ ]]; then
          remaining=\$((expires_at - \$(date +%s)))
          auto_close_in="\$(format_remaining "\$remaining")"
        fi
        ;;
      always)
        auto_close_in="never"
        ;;
      closed)
        auto_close_in="0s"
        ;;
    esac
  fi

  printf "%-32s %-8s %s\n" "\$id" online "\$auto_close_in"
  found=1
done < <(ss -xlH)

if [[ "\$found" -eq 0 ]]; then
  echo "(no Gateway currently online; AUTO_CLOSE_IN=N/A)"
fi
EOF
 
  chmod 0755 /usr/local/bin/gwlist
 
  log "Ubuntu Reverse SSH 访问/在线控制工具已安装。"
  log "Gateway 登录用户：$GATEWAY_SSH_USER"
  return 0
}
 
 
main(){
  require_root
 
  log "[1/9] 生成/读取持久 SERVER_ID"
  generate_server_id
 
  log "[2/9] 检查配置"
  validate
 
  log "[3/9] 配置 sshd StreamLocal forwarding"
  configure_sshd_streamlocal
 
  log "[4/9] 检查 sshd"
  check_sshd
 
  log "[5/9] 安装/检查 frpc"
  install_frpc
 
  log "[6/9] 准备运行目录"
  configure_dirs
 
  log "[7/9] 配置并启动 frpc"
  configure_frpc
 
  log "[8/9] 验证 FRP 注册"
  wait_frp
 
  log "[9/9] 安装 Reverse SSH 访问/在线控制工具"
  install_tools
 
  echo
  echo "================ Ubuntu reverse-ssh is ready ================"
  echo "SERVER_ID : $SERVER_ID"
  echo "FRP target: $FRP_CUSTOM_DOMAIN"
  echo "Gateway SSH user: $GATEWAY_SSH_USER"
  echo
  echo "请把下面这一行写入所有对应 Gateway 的脚本："
  echo "UBUNTU_SERVER_ID=\"$SERVER_ID\""
  echo
  echo "Reverse SSH 在线时可在 Ubuntu 使用："
  echo "  gwctl DEVICE_ID open"
  echo "  gwctl DEVICE_ID always"
  echo "  gwctl DEVICE_ID status"
  echo "  gwctl DEVICE_ID close"
  echo "  gwlist"
  echo "  gwssh DEVICE_ID"
  echo "  gwscp DEVICE_ID LOCAL_FILE REMOTE_PATH"
  echo "  gwscp DEVICE_ID REMOTE_PATH LOCAL_FILE --download"
  echo
  echo "说明："
  echo "  gwssh/gwctl 均通过 Reverse SSH 以 Gateway root 登录"
  echo "  gwctl 直接执行 root-only 的 /usr/bin/reverse-ssh-control。"
  echo "  close 或 timed 到期后 Reverse SSH socket 会消失。"
  echo "  若 Gateway 已按双模式方案配置，close 会先恢复 Forward/FRP SSH，"
  echo "  然后再关闭 Reverse SSH；此时请通过 Forward SSH 登录 $GATEWAY_SSH_USER。"
  echo "  gwscp用于上传/下载文件，上传时 LOCAL_FILE 必须存在，下载时 REMOTE_PATH 必须存在。"
  echo "==============================================================="
}
 
main "$@"
