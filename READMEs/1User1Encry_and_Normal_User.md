# 一机一密与普通用户

---

# 一、原代码修改部分

## 1. 最终账号与权限模型

| 账号 | 用途 | 最终权限 |
|---|---|---|
| `root` | 首次凭证生成、GPIO/Bluetooth 初始化、systemd 管理、Reverse SSH 维护 | 特权账号；不运行业务进程 |
| `skfgw` | `skf_gw`、SQL、MQTT、Modbus 四个业务进程 | 普通用户；无 sudo；只获得必要设备/数据权限 |

同时做到：

- `root` 和 `skfgw` **每台设备分别生成不同随机密码**；
- 同一设备上 `root` 密码与 `skfgw` 密码也不同；
- 不在 rootfs 镜像中写死密码；
- 不在 rootfs 中预置 Reverse SSH 私钥；
- 旧 Forward SSH/`frpc` 最终淘汰并 mask；
- Reverse SSH 仅由 `root` 控制，正常开启窗口为 **72 小时**。

---

## 2. 源码修改点

### 2.1 数据、配置和 IPC 从 `/home/root`、`/tmp` 迁出

| 文件 | 具体修改 |
|---|---|
| `skf_gw_mqtt/middleware/common/global.h` | 新增统一目录：`/var/lib/skf-gateway`、`/var/lib/skf-gateway/config`、`/run/skf-gateway`；数据库改为 `/var/lib/skf-gateway/skf.db`；配置文件从 `/home/root/...` 迁到 `/var/lib/skf-gateway/config/...`；消息队列权限由 `0777` 收紧为 `0600`。 |
| `modbus_rtu/modbus_slave/app_mslave.c` | `PATH_MFILE`、`PATH_MFILE_USER` 等从 `/home/root/...` 改到 `/var/lib/skf-gateway/config/...`。 |
| `skf_gw/source/include/app_general_def.h` | `ftok` key 文件从 `/tmp/key*` 改到 `/run/skf-gateway/ipc/key*`；IPC 权限改为 `0600`。 |
| `skf_gw/source/src/main.c` | IPC key 文件创建权限改为仅 owner 可读写，不再创建 world-writable 文件。 |
| `skf_gw/source/src/sys_def.c` | 同上，创建运行时 key 文件使用 owner-only 权限。 |
| `skf_gw_mqtt/middleware/sh_mem/sh_mem.h` | shared-memory/key 文件路径统一迁到 `/run/skf-gateway/ipc`；权限收紧。 |
| `skf_gw_mqtt/middleware/sh_mem/sh_mem.c` | 去掉不安全的 `system("touch ...")`/宽权限创建方式，改为 `open(..., O_CREAT|O_RDWR, 0600)`；处理已有 key/IPC 对象，避免旧 root IPC 残留影响普通用户启动。 |
| `modbus_rtu/modbus_slave/app_mslave_ipc.c` | 与主程序使用同一套 `/run/skf-gateway/ipc` 路径及 `0600` 权限。 |

最终目录权限：

```text
/var/lib/skf-gateway            0700 skfgw:skfgw
/var/lib/skf-gateway/config     0700 skfgw:skfgw
/var/lib/skf-gateway/data       0700 skfgw:skfgw
/run/skf-gateway                0700 skfgw:skfgw
/run/skf-gateway/ipc            0700 skfgw:skfgw
System V IPC / shared memory    0600 skfgw
```

### 2.2 普通业务程序不再直接做 root 初始化

| 文件 | 具体修改 |
|---|---|
| `skf_gw/source/src/general/app_io.c` | `init_gpio_output()` 不再由业务程序执行 GPIO `export/direction`；改成只使用 root 预先准备好的 `gpioXX/value`。 |
| `skf_gw/source/src/app_common.c` | 删除/停止业务进程对 GPIO86 的强制 export/direction 操作；LTE/GPIO 初始化改为检查节点是否已准备，而不是普通进程自行做 root 操作。 |
| `scripts/gpio-init.sh` | 新增 root 启动初始化：仅处理 GPIO `61/65/83/84/128`；`value` 设为 `root:skfgw 0620`。**GPIO86 明确不强制配置**。 |

> 网络/NTP/LTE 等少量历史特权逻辑在本轮没有全部重构成统一 privileged daemon；如后续重新启用相关路径，仍应单独复核，不能因为四个主进程已非 root 就默认全部安全。

### 2.3 串口、按键只给最小设备权限

`udev/99-skf-gateway.rules`：

```udev
SUBSYSTEM=="tty", KERNEL=="ttyS3", GROUP="skfgw", MODE="0660"
SUBSYSTEM=="input", KERNEL=="event*", ENV{ID_PATH}=="platform-gpio-keys1", GROUP="skfgw", MODE="0640"
```

效果：

```text
/dev/ttyS3       root:skfgw 0660
GPIO key event   root:skfgw 0640
GPIO value       root:skfgw 0620
```

不再通过 `chmod 777` 解决设备访问问题。

---

## 3. Bluetooth 修改点

原设计中 `restart_ble.sh`、`/etc/init.d/bluetooth` 同时操作：

- `dbus`
- `rfkill`
- `hciattach`
- `hciconfig`
- `bluetoothd`
- 应用启动

容易出现重复 `bluetoothd`、HCI busy、D-Bus name 冲突。

最终拆分：

| 文件 | 最终职责 |
|---|---|
| `scripts/bluetooth-hci-main.sh` | root 初始化 HCI 主流程。 |
| `scripts/bluetooth-hci-up.sh` | root 拉起/检查 HCI。 |
| `scripts/bluetooth-provision.sh` | root 完成 Bluetooth provisioning。 |
| `systemd/skf-gw-bluetooth-init.service` | 以 root 运行 HCI 初始化。 |
| `systemd/skf-gw-bluetooth-provision.service` | 以 root 运行 provisioning。 |
| `systemd/bluetooth.service.d/10-skf-hci.conf` | 约束 BlueZ 与 HCI 初始化顺序。 |
| `compat/bluetooth` | `/etc/init.d/bluetooth` 兼容入口，只委托 systemd，不再自己启动 `hciattach/bluetoothd`。 |
| `restart_ble.sh` | 不再作为正式生产启动链，不再同时管理 dbus/rfkill/bluetoothd/业务程序。 |

正常系统中应只有一套 HCI 初始化链和一个 `bluetoothd`。

---

## 4. systemd 服务模型修改点

原来：

```text
skf_gw.service
  └─ start.sh
      ├─ restart_ble
      ├─ mqtt
      ├─ sql
      └─ modbus
```

且缺少明确 `User=`，实际容易以 root 运行。

最终改为四个独立业务 service：

```text
skf-gw.service
skf-gw-sql.service
skf-gw-mqtt.service
skf-gw-modbus.service
```

核心配置统一为：

```ini
User=skfgw
Group=skfgw
WorkingDirectory=/var/lib/skf-gateway
Environment=HOME=/var/lib/skf-gateway
Restart=always
RestartSec=5
UMask=0077
NoNewPrivileges=true
```

四个二进制统一：

```text
/opt/skf-gateway/bin/*
root:root 0755
```

这样 `skfgw` 可以执行，但不能覆盖自己的程序文件。

同时新增/保留：

```text
skf-gateway.target
skf-gw-gpio-init.service
skf-gw-bluetooth-init.service
skf-gw-bluetooth-provision.service
skf-gateway-firstboot-credential.service
```

---

## 5. tmpfiles 修改点

`tmpfiles/skf-gateway.conf`：

```text
d /run/skf-gateway     0700 skfgw skfgw -
d /run/skf-gateway/ipc 0700 skfgw skfgw -
```

解决 `/run` 每次重启被清空的问题。

---

## 6. 编译系统修改点

| 文件 | 具体修改 |
|---|---|
| `skf_gw/CMakeLists.txt` | 编译器设置放到正确位置；protobuf/nanopb/ELL 路径切换为当前仓库；加入依赖存在检查；udev/usb/dl/rt 等库从 ARM64 target rootfs 查找，避免误链宿主机库。 |
| `skf_gw/source/include/sys_timer.h` | 在 ELL 使用前补 `#include <stdbool.h>`，解决 `bool` 编译问题。 |
| MQTT/SQL build 配置 | 修正 `libusb.h`、middleware、protobuf/nanopb 路径。 |
| Modbus Makefile/build | 原 `AppProtocols` 老路径切换到当前 `skf_gw_mqtt/middleware/...`；生成/使用 libmodbus `config.h`；确保使用 AArch64 compiler 而不是宿主机 `cc`。 |

最终四个二进制均应为：

```text
ELF 64-bit ... ARM aarch64
```

---

## 7. 一机一密修改点

### `scripts/provision-credentials.sh`

最终脚本只处理：

```text
root
skfgw
```

主要逻辑：

1. 从 `/dev/urandom` 分别生成两个随机密码；
2. 分别执行 `chpasswd`；
3. 写入仅 root 可读的首次凭证文件；
4. 创建 marker，后续启动不重复生成。

典型状态：

```text
/var/lib/skf-gateway/provision/
├── credentials.initialized
├── initial-root-password
└── initial-skfgw-password
```

权限：

```text
root:root
0600
```

**严禁在制作 rootfs 时执行此脚本。**

否则同一镜像烧录出的设备会共享密码，失去“一机一密”。

---

## 8. `install.sh` 修改点

最终 `install.sh` 的职责：

- 创建/修正 `skfgw`：
  - home：`/var/lib/skf-gateway`
  - shell：`/bin/bash`
  - 不再锁密码；
- 不再创建 `gwadmin`；
- 允许保留“删除旧 `gwadmin` 残留”的迁移清理代码；
- 安装四个 root-owned binary；
- 安装 systemd、udev、tmpfiles、Bluetooth、SSH、Reverse SSH 文件；
- 清理旧 root IPC / stale semaphore；
- 安装/调用首次凭证 provisioning；
- 老网关迁移时**不能在 Reverse SSH 尚未 online 前先切断正在使用的 `frpc`**。

---

## 9. SSH / Reverse SSH 修改点

### SSH：`ssh/60-skf-gateway.conf`

最终策略：

```sshconfig
PasswordAuthentication no
PermitRootLogin no
AllowUsers root

Match Address 127.0.0.1,::1
    PasswordAuthentication yes
    PermitRootLogin yes
Match all
```

效果：

- `skfgw`：不允许 SSH；
- `root`：不允许从 LAN/WAN 直接密码 SSH；
- Reverse SSH 最终从 Gateway 本机 `127.0.0.1` 回到 sshd，因此允许 root 登录；
- `skfgw` 仍可用于本地 console。

### `gateway-reverse-ssh-setup-light.sh`

最终规则：

- Gateway 端 Reverse SSH 用户为 `root`；
- 每台 Gateway 自己生成独立 Ed25519 key；
- 私钥不进入公共 rootfs；
- `reverse-ssh-control` 为 `root:root 0700`；
- `skfgw` 无 sudo、不能调用控制器；
- `open` = 开启 **72 小时**；
- `close` = 正确关闭方式；
- `always` 仅保留给 root 工程调试，不作为正常产品流程；
- 老网关只有在 `Reverse SSH: online` 后，才备份并淘汰旧 `frpc.service`；
- `frpc.service` 最终：
  ```text
  masked
  inactive
  /etc/systemd/system/frpc.service -> /dev/null
  ```

### `build_skf_package.sh`

最终职责仅为：

```text
收集最新产物
→ 组装 skf_package
→ 基础架构/文件检查
→ 生成 manifest.sha256
→ 生成 skf_package.tar.gz
```

不再把复杂账号审计硬塞进 build；并且：

- 不再要求 `sudoers/70-gwadmin-reverse-ssh`；
- 不再把旧 `sudoers/` 打包进去；
- `gwadmin` 的历史迁移清理允许存在于 `install.sh`，但最终不得创建/使用该用户。

---

# 二、如何把最终内容写入 rootfs

## 1. 先重新生成最终 package

开发机：

```bash
cd /home/forlinx/GW/GW

./build_skf_package.sh
```

确认最终目录：

```bash
export PKG=/home/forlinx/GW/GW/skf_gw_mqtt/deployment/skf_package
```

以及 tar 包：

```text
/home/forlinx/GW/GW/skf_gateway_install/skf_package.tar.gz
```

建议先检查：

```bash
find "$PKG" -maxdepth 3 -type f | sort
sha256sum "$PKG"/bin/*
```

---

## 2. 设置 rootfs

```bash
export ROOTFS=/home/forlinx/work/OK62xx-linux-sdk/OK62xx-linux-fs/rootfs
export PKG=/home/forlinx/GW/GW/skf_gw_mqtt/deployment/skf_package
```

建立目录：

```bash
sudo mkdir -p \
"$ROOTFS/opt/skf-gateway/bin" \
"$ROOTFS/usr/libexec/skf-gateway" \
"$ROOTFS/etc/skf-gateway" \
"$ROOTFS/etc/systemd/system/bluetooth.service.d" \
"$ROOTFS/etc/udev/rules.d" \
"$ROOTFS/etc/tmpfiles.d" \
"$ROOTFS/etc/ssh/sshd_config.d" \
"$ROOTFS/etc/init.d" \
"$ROOTFS/var/lib/skf-gateway/config" \
"$ROOTFS/var/lib/skf-gateway/data" \
"$ROOTFS/var/lib/skf-gateway/update" \
"$ROOTFS/var/lib/skf-gateway/provision" \
"$ROOTFS/var/lib/reverse-ssh-control"
```

---

## 3. 写入四个业务二进制

```bash
for f in skf_gw sql_op mqtt_op skf_gw_modbus_rtu; do
    sudo install -o root -g root -m 0755 \
    "$PKG/bin/$f" \
    "$ROOTFS/opt/skf-gateway/bin/$f"
done
```

确认：

```bash
file "$ROOTFS"/opt/skf-gateway/bin/*
```

必须是 AArch64。

---

## 4. 写入 root helper

```bash
for f in \
gpio-init.sh \
bluetooth-hci-main.sh \
bluetooth-hci-up.sh \
bluetooth-provision.sh \
provision-credentials.sh
do
    sudo install -o root -g root -m 0755 \
    "$PKG/scripts/$f" \
    "$ROOTFS/usr/libexec/skf-gateway/$f"
done
```

Reverse SSH：

```bash
sudo install -o root -g root -m 0755 \
"$PKG/tools/reverse-ssh-light-tools/gateway-reverse-ssh-setup-light.sh" \
"$ROOTFS/usr/libexec/skf-gateway/gateway-reverse-ssh-setup-light.sh"
```

**这里只复制脚本，不执行。**

---

## 5. 写入 systemd / udev / tmpfiles / SSH / Bluetooth

```bash
sudo install -o root -g root -m 0644 \
"$PKG/systemd/skf-gateway.target" \
"$ROOTFS/etc/systemd/system/skf-gateway.target"

for f in \
skf-gw.service \
skf-gw-sql.service \
skf-gw-mqtt.service \
skf-gw-modbus.service \
skf-gw-gpio-init.service \
skf-gw-bluetooth-init.service \
skf-gw-bluetooth-provision.service \
skf-gateway-firstboot-credential.service
do
    sudo install -o root -g root -m 0644 \
    "$PKG/systemd/$f" \
    "$ROOTFS/etc/systemd/system/$f"
done

sudo install -o root -g root -m 0644 \
"$PKG/systemd/bluetooth.service.d/10-skf-hci.conf" \
"$ROOTFS/etc/systemd/system/bluetooth.service.d/10-skf-hci.conf"

sudo install -o root -g root -m 0644 \
"$PKG/udev/99-skf-gateway.rules" \
"$ROOTFS/etc/udev/rules.d/99-skf-gateway.rules"

sudo install -o root -g root -m 0644 \
"$PKG/tmpfiles/skf-gateway.conf" \
"$ROOTFS/etc/tmpfiles.d/skf-gateway.conf"

sudo install -o root -g root -m 0644 \
"$PKG/ssh/60-skf-gateway.conf" \
"$ROOTFS/etc/ssh/sshd_config.d/60-skf-gateway.conf"

sudo install -o root -g root -m 0755 \
"$PKG/compat/bluetooth" \
"$ROOTFS/etc/init.d/bluetooth"
```

---

## 6. rootfs 中只保留 `root + skfgw`

删除旧 `gwadmin`：

```bash
for f in passwd shadow group gshadow passwd- shadow- group- gshadow- subuid subgid; do
    [ -f "$ROOTFS/etc/$f" ] && \
    sudo sed -i '/^gwadmin:/d' "$ROOTFS/etc/$f"
done

sudo rm -rf "$ROOTFS/home/gwadmin"
sudo rm -f "$ROOTFS/etc/sudoers.d/70-gwadmin-reverse-ssh"
```

确保 `skfgw` 存在；若已有，只修改 shell：

```bash
sudo usermod \
-R "$ROOTFS" \
-d /var/lib/skf-gateway \
-s /bin/bash \
skfgw
```

检查：

```bash
grep '^root:' "$ROOTFS/etc/passwd"
grep '^skfgw:' "$ROOTFS/etc/passwd"
grep '^gwadmin:' "$ROOTFS/etc/passwd" || true
```

目标：

```text
root    存在
skfgw   存在，home=/var/lib/skf-gateway，shell=/bin/bash
gwadmin 不存在
```

---

## 7. 修正 rootfs 数据目录 owner

不要硬编码 UID/GID：

```bash
SKFGW_UID=$(awk -F: '$1=="skfgw"{print $3}' "$ROOTFS/etc/passwd")
SKFGW_GID=$(awk -F: '$1=="skfgw"{print $3}' "$ROOTFS/etc/group")

echo "$SKFGW_UID:$SKFGW_GID"
```

执行：

```bash
sudo chown "$SKFGW_UID:$SKFGW_GID" \
"$ROOTFS/var/lib/skf-gateway"

sudo chown -R "$SKFGW_UID:$SKFGW_GID" \
"$ROOTFS/var/lib/skf-gateway/config" \
"$ROOTFS/var/lib/skf-gateway/data" \
"$ROOTFS/var/lib/skf-gateway/update"

sudo chmod 0700 \
"$ROOTFS/var/lib/skf-gateway" \
"$ROOTFS/var/lib/skf-gateway/config" \
"$ROOTFS/var/lib/skf-gateway/data" \
"$ROOTFS/var/lib/skf-gateway/update"

sudo chown root:root "$ROOTFS/var/lib/skf-gateway/provision"
sudo chmod 0700 "$ROOTFS/var/lib/skf-gateway/provision"
```

配置文件如存在：

```bash
sudo install -o "$SKFGW_UID" -g "$SKFGW_GID" -m 0600 \
"$PKG/config/magicFile.json" \
"$ROOTFS/var/lib/skf-gateway/config/magicFile.json"
```

`gwConfig.json` 同样按 `skfgw:skfgw 0600` 处理。

---

## 8. rootfs 中默认关闭旧 frpc

全新设备不需要旧 Forward SSH：

```bash
sudo systemctl --root="$ROOTFS" disable frpc.service 2>/dev/null || true

sudo rm -f "$ROOTFS/etc/systemd/system/frpc.service"
sudo ln -s /dev/null "$ROOTFS/etc/systemd/system/frpc.service"
```

清理旧迁移 timer：

```bash
sudo rm -f \
"$ROOTFS/etc/systemd/system/legacy-frpc-disable.timer" \
"$ROOTFS/etc/systemd/system/legacy-frpc-disable.service"
```

---

## 9. 全新设备 Reverse SSH 默认 `closed`

```bash
sudo install -d -o root -g root -m 0700 \
"$ROOTFS/var/lib/reverse-ssh-control"

printf 'MODE="closed"\nEXPIRES_AT="0"\n' | \
sudo tee "$ROOTFS/var/lib/reverse-ssh-control/state" >/dev/null

sudo chown root:root \
"$ROOTFS/var/lib/reverse-ssh-control/state"

sudo chmod 0600 \
"$ROOTFS/var/lib/reverse-ssh-control/state"
```

---

## 10. enable 正式 target 与 firstboot

```bash
sudo systemctl --root="$ROOTFS" enable skf-gateway.target
sudo systemctl --root="$ROOTFS" enable skf-gateway-firstboot-credential.service
```

四个业务 service 由 `skf-gateway.target` 统一管理，不需要再各自单独 enable。

---

## 11. rootfs 构建阶段严禁事项

不要执行：

```text
provision-credentials.sh
gateway-reverse-ssh-setup-light.sh
随意生成 root/skfgw 固定密码
随意生成 Reverse SSH 私钥
```

正确关系是：

```text
公共 rootfs
  ├─ 只有程序、service、规则、脚本
  └─ 无设备秘密

Gateway A 首次启动
  ├─ root 密码 A
  ├─ skfgw 密码 A
  └─ Reverse SSH key A

Gateway B 首次启动
  ├─ root 密码 B
  ├─ skfgw 密码 B
  └─ Reverse SSH key B
```

最后沿用已经验证过的 **OK62xx SDK 原有 rootfs/system image 制作流程**生成镜像。


同时roofts绝对不能有以下三个文件：

```text
credentials.initialized
initial-root-password
initial-skfgw-password
```
制作镜像前请强制：

```text
rm -f \
"$ROOTFS/var/lib/skf-gateway/provision/credentials.initialized" \
"$ROOTFS/var/lib/skf-gateway/provision/initial-root-password" \
"$ROOTFS/var/lib/skf-gateway/provision/initial-skfgw-password"
```

并确保以下命令行无输出：

```text
find \
"$ROOTFS/var/lib/skf-gateway/provision" \
-maxdepth 1 \
-type f \
-print
```
---

# 三、如何在老网关验证最终效果与安全性

## 1. 上传安装包并校验

建议不要放在容易被清理的 `/tmp`，使用：

```text
/home/root/skf-validation/package/
```

开发机先记录：

```bash
sha256sum skf_gateway_install/skf_package.tar.gz
```

老网关：

```bash
mkdir -p \
/home/root/skf-validation/package \
/home/root/skf-validation/extracted
```

上传后：

```bash
cd /home/root/skf-validation/package
sha256sum skf_package.tar.gz
```

两边 hash 必须一致。

解包：

```bash
tar -xzf skf_package.tar.gz \
-C /home/root/skf-validation/extracted
```

如 package 内有 `manifest.sha256`：

```bash
cd /home/root/skf-validation/extracted/skf_package
sha256sum -c manifest.sha256
```

必须全部 `OK`。

---

## 2. 安装前记录当前远程入口

老网关如果仍依赖 `frpc.service` 远程维护，**安装过程中不能先手工 stop/mask 它**。

先记录：

```bash
systemctl is-active frpc.service || true
systemctl is-enabled frpc.service || true
```

必须遵循：

```text
先建立并验证 Reverse SSH
→ 再备份旧 frpc
→ 再 disable/remove/mask frpc
```

否则可能把设备直接变成远程失联。

---

## 3. 执行最终安装

```bash
cd /home/root/skf-validation/extracted/skf_package

./install.sh --mac <设备真实MAC>
```

PRO 型按原有参数使用 `--pro`。

查找<设备真实MAC>：ifconfig -a，寻找UP对应的地址

安装完成后，先检查首次凭证：

```bash
sudo cat /var/lib/skf-gateway/provision/initial-root-password
sudo cat /var/lib/skf-gateway/provision/initial-skfgw-password

sudo stat -c '%a %U:%G %n' \
/var/lib/skf-gateway/provision/*
```

密码文件必须：

```text
root:root 0600
```

并且：

```text
root password != skfgw password
```

完成工厂记录后，生产流程应尽量删除明文初始密码文件，只保留 `credentials.initialized` marker；否则 root 失陷后仍可直接读取初始密码。

---

## 4. 重启前静态检查

### 用户

```bash
getent passwd root
getent passwd skfgw
getent passwd gwadmin || true
```

目标：

```text
root    存在
skfgw   存在，/bin/bash
gwadmin 无输出
```

### 四个业务 service

```bash
for s in \
skf-gw.service \
skf-gw-sql.service \
skf-gw-mqtt.service \
skf-gw-modbus.service
do
    systemctl cat "$s" | \
    grep -E 'User=|Group=|ExecStart=|NoNewPrivileges|UMask'
done
```

目标：

```text
User=skfgw
Group=skfgw
ExecStart=/opt/skf-gateway/bin/...
NoNewPrivileges=true
UMask=0077
```

### 二进制

```bash
stat -c '%a %U:%G %n' /opt/skf-gateway/bin/*
```

目标：

```text
755 root:root
```

### SSH 配置

```bash
sshd -t
```

必须无报错。

---

## 5. 建立 Reverse SSH，再淘汰老 frpc

运行最终 Gateway 脚本：

```bash
/usr/libexec/skf-gateway/gateway-reverse-ssh-setup-light.sh
```

先确认：

```text
Reverse SSH: online
reverse-ssh.service: active
```

之后脚本才允许处理旧 `frpc.service`。

检查：

```bash
systemctl is-enabled frpc.service
systemctl is-active frpc.service
ls -l /etc/systemd/system/frpc.service
```

最终目标：

```text
masked
inactive
frpc.service -> /dev/null
```

老 `frpc` unit 的备份应位于类似：

```text
/var/lib/reverse-ssh-control/legacy-forward-backup/<timestamp>/
```

然后把 Reverse SSH 收敛到正常 72 小时窗口：

```bash
/usr/bin/reverse-ssh-control open
/usr/bin/reverse-ssh-control status
```

目标：

```text
Mode: timed
Reverse SSH: online
Expires: 当前时间 + 72h
```

正常关闭必须使用：

```bash
/usr/bin/reverse-ssh-control close
```

不要只：

```bash
systemctl stop reverse-ssh.service
```

因为 health timer 会根据 state 再次拉起。

---

## 6. 重启后验收

```bash
reboot
```

重连后执行。

### 6.1 服务身份

```bash
systemctl is-active skf-gateway.target

systemctl is-active \
skf-gw.service \
skf-gw-sql.service \
skf-gw-mqtt.service \
skf-gw-modbus.service

ps -eo user,pid,cmd | \
grep -E '/opt/skf-gateway/bin/(skf_gw|sql_op|mqtt_op|skf_gw_modbus_rtu)' \
| grep -v grep
```

四个业务进程必须全部显示：

```text
skfgw
```

不能是 root。

### 6.2 数据与 IPC

```bash
stat -c '%a %U:%G %n' \
/var/lib/skf-gateway \
/var/lib/skf-gateway/config \
/var/lib/skf-gateway/data \
/run/skf-gateway \
/run/skf-gateway/ipc

ipcs -q
ipcs -m
ls -l /dev/shm
```

目标：

```text
数据/运行目录        0700 skfgw
消息队列/共享内存    owner=skfgw, perms=600
Gateway semaphore    skfgw，非 world-writable
```

### 6.3 验证业务用户不能改程序/系统

```bash
runuser -u skfgw -- sh -c \
'touch /var/lib/skf-gateway/data/write-test && echo DATA_OK'

runuser -u skfgw -- sh -c \
'touch /opt/skf-gateway/bin/should-fail' || echo OPT_DENIED_OK

runuser -u skfgw -- sh -c \
'touch /etc/should-fail' || echo ETC_DENIED_OK

runuser -u skfgw -- sh -c \
'touch /root/should-fail' || echo ROOT_DENIED_OK
```

预期：

```text
DATA_OK
OPT_DENIED_OK
ETC_DENIED_OK
ROOT_DENIED_OK
```

### 6.4 串口/GPIO

```bash
stat -c '%a %U:%G %n' /dev/ttyS3
```

目标：

```text
660 root:skfgw
```

GPIO 输出检查：

```bash
for n in 61 65 83 84 128; do
    stat -c '%a %U:%G %n' /sys/class/gpio/gpio$n/value 2>/dev/null
done
```

目标：

```text
620 root:skfgw
```

**GPIO86 不应被本方案强制 export/configure。**

### 6.5 Bluetooth

```bash
systemctl status \
skf-gw-bluetooth-init.service \
skf-gw-bluetooth-provision.service \
bluetooth.service \
--no-pager -l

hciconfig -a
ps -ef | grep -E 'bluetoothd|hciattach' | grep -v grep
```

目标：

- `hci0` 为 `UP RUNNING`；
- 只出现预期的一套 `hciattach`；
- 只有一个 `bluetoothd`；
- 无 `restart_ble/start.sh` 老启动链反复拉进程。

### 6.6 Reverse SSH root-only

```bash
stat -c '%a %U:%G %n' /usr/bin/reverse-ssh-control

runuser -u skfgw -- \
/usr/bin/reverse-ssh-control open
```

目标：

```text
/usr/bin/reverse-ssh-control = 700 root:root
skfgw 调用失败
```

### 6.7 SSH 边界

验收应同时满足：

```text
LAN/WAN 直接 root SSH      失败
LAN/WAN 直接 skfgw SSH     失败
Reverse SSH 登录 root       成功
本地 console 登录 skfgw     成功
```

Reverse SSH 登录 root 使用该设备自己的 `root` 一机一密密码。

---

## 7. 真正验证“一机一密”

至少用两台设备 A/B 比较：

```text
root(A)   != root(B)
skfgw(A)  != skfgw(B)
root(A)   != skfgw(A)
root(B)   != skfgw(B)
```

同时：

```bash
stat -c '%a %U:%G %n' \
/var/lib/skf-gateway/provision/credentials.initialized
```

marker 存在后，再重启设备，密码不应再次变化。

Reverse SSH 私钥也必须每台设备不同，不能从公共 rootfs 复制同一份私钥。
