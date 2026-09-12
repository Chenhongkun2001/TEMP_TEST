# 一机一密与普通用户

---

> 本文记录当前最终实现状态。重点说明账号/权限边界、特权操作拆分、systemd 生命周期、Bluetooth/HCI/Network helper、安装与 rootfs 写入方式，以及最终验收方法。  
> 排错过程、临时实验和已被后续方案替代的中间实现不写入本文。

# 一、原代码修改部分

## 1. 最终账号与权限模型

| 账号 | 用途 | 最终权限 |
|---|---|---|
| `root` | 首次凭证生成、GPIO/HCI/Bluetooth 初始化、窄范围 HCI/Network/Bluetooth recovery helper、systemd 管理、Reverse SSH 维护 | 特权账号；不长期运行业务进程 |
| `skfgw` | `skf_gw`、SQL、MQTT、Modbus 四个业务进程 | 普通用户；无 sudo；仅获得必要目录、设备和最小 capability |

最终要求：

- `root` 和 `skfgw` **每台设备分别生成不同随机密码**；
- 同一设备上 `root` 密码与 `skfgw` 密码不同；
- 不在公共 rootfs 中写死密码；
- 不在公共 rootfs 中预置 Reverse SSH 私钥；
- `skfgw` 不拥有 sudo 权限；
- `skf_gw` 仅保留业务所需的 `CAP_NET_RAW`；
- 不给 `skf_gw`、`mqtt_op`、`sql_op` 增加 `CAP_NET_ADMIN`；
- HCI Connection Update、网络恢复、Bluetooth daemon recovery 通过受控 root helper 完成；
- 旧 `start.sh/restart_*.sh/autorun/skf_gw.service` 不再作为正式生命周期管理器；
- 正式应用生命周期统一由 `skf-gateway.target` 管理；
- 旧 Forward SSH/`frpc` 最终淘汰并 mask；
- Reverse SSH 仅由 `root` 控制，正常开启窗口为 **72 小时**。

---

## 2. 源码修改点

### 2.1 数据、配置和 IPC 从 `/home/root`、`/tmp` 迁出

| 文件 | 最终修改 |
|---|---|
| `skf_gw_mqtt/middleware/common/global.h` | 数据库统一为 `/var/lib/skf-gateway/skf.db`；配置统一放到 `/var/lib/skf-gateway/config`；运行时目录统一使用 `/run/skf-gateway`；SysV IPC 权限收紧。 |
| `modbus_rtu/modbus_slave/app_mslave.c` | Modbus 配置路径从 `/home/root/...` 迁到 `/var/lib/skf-gateway/config/...`。 |
| `skf_gw/source/include/app_general_def.h` | `ftok` key 文件迁到 `/run/skf-gateway/ipc`，IPC 权限使用 owner-only 模型。 |
| `skf_gw/source/src/main.c` | 普通启动路径使用新 IPC 目录；同时增加受控的 `--priv-hci-conn-update` root helper mode。 |
| `skf_gw/source/src/sys_def.c` | 运行时 key 文件按普通用户可安全使用的 owner-only 权限创建。 |
| `skf_gw_mqtt/middleware/sh_mem/sh_mem.h` | shared-memory/key 路径统一迁到 `/run/skf-gateway/ipc`。 |
| `skf_gw_mqtt/middleware/sh_mem/sh_mem.c` | 不再使用宽权限 `touch`；改为 `open(..., O_CREAT|O_RDWR, 0600)`；兼容清理旧 root IPC 对象。 |
| `modbus_rtu/modbus_slave/app_mslave_ipc.c` | 与主程序统一使用 `/run/skf-gateway/ipc` 和 `0600` IPC 权限。 |

最终主要目录：

```text
/var/lib/skf-gateway                 skfgw:skfgw 0700
/var/lib/skf-gateway/config          skfgw:skfgw 0700
/var/lib/skf-gateway/data            skfgw:skfgw 0700
/var/lib/skf-gateway/update          skfgw:skfgw 0700

/run/skf-gateway                     root:root   0755
/run/skf-gateway/ipc                 skfgw:skfgw 0700
/run/skf-gateway/requests            skfgw:skfgw 0700
/run/skf-gateway/state               root:skfgw  0750

System V IPC / shared memory         owner=skfgw, perms=0600
```

---

### 2.2 普通业务程序不再直接执行 root 初始化

| 文件 | 最终修改 |
|---|---|
| `skf_gw/source/src/general/app_io.c` | 业务进程不再负责 GPIO export/direction；只访问 root 预先准备好的 GPIO `value`。 |
| `skf_gw/source/src/app_common.c` | 普通业务路径不再直接承担 root GPIO 初始化；硬件准备交给 root helper/service。 |
| `skf_gw_mqtt/deployment/skf_package/scripts/gpio-init.sh` | root 启动初始化固定 GPIO，并把应用真正需要访问的 `value` 节点设置成 `root:skfgw` 的最小写权限。 |

基础 GPIO 初始化仍负责：

```text
GPIO 61
GPIO 65
GPIO 83
GPIO 84
GPIO 128
```

LTE 使用的 GPIO86 不再由主业务进程直接 export/configure，而由 Network Recovery root helper 中的白名单操作负责。

---

### 2.3 HCI Connection Update 从业务进程拆成 root helper

#### `skf_gw/source/src/main.c`

增加受控 helper mode：

```text
/opt/skf-gateway/bin/skf_gw
    --priv-hci-conn-update
    MAC
    MIN
    MAX
    LAT
    SUP
    MINCE
    MAXCE
```

该模式：

- 只由 root systemd helper 调用；
- 复用原有 `bt_hcitool_set_bluetooth_connection_param()`；
- `phy` 固定使用 `LE_PHY_1M`；
- **不再维护第二个 `skf_hci_conn_update` binary**。

#### `skf_gw/source/src/general/app_pro_general_new.c`

新增普通用户侧同步 request helper：

```text
skfgw
  ↓ write request
/run/skf-gateway/hci-conn-update/requests
  ↓
root dispatcher
  ↓ write result
/run/skf-gateway/hci-conn-update/results
```

正常 Sensor 流程和 FUOTA 流程中原来直接执行 HCI Connection Update 的位置都改为请求 root helper；原有业务等待时序保留。

请求内容统一为：

```text
MAC + MIN + MAX + LAT + SUP + MINCE + MAXCE
```

不再额外传递 PHY。

---

### 2.4 LTE / Network 特权 primitive 从 `mqtt_op` 拆出

文件：

```text
skf_gw_mqtt/middleware/common/app_lte.c
```

新增同步 request/result helper，并把下面几个需要 root 的 primitive 改为请求 root dispatcher：

```text
app_com_init_gpio_for_lte_ctr()
app_com_set_lte_power_supply()
app_com_set_lte_con_state()
app_com_set_up_route_for_dhcp()
app_com_set_up_default_route()
app_com_set_wwan0_managed()
```

普通 `mqtt_op/skfgw` 不再直接：

```text
export/write GPIO86
启动/停止 quectel-CM
写 /etc/systemd/network/20-wwan0.network
restart systemd-networkd
执行 udhcpc
修改 default route
```

`toggleNetwork()` 的判断、重试、等待和网络切换业务逻辑保持原样，只把其内部 root primitive 搬到 helper。

---

### 2.5 Sensor/BlueZ 运行期自愈

#### `skf_gw/source/include/ble/bt_connection_new.h`

新增一次性读取 proxy recovery 状态的接口：

```text
bt_con_n_take_proxy_recovery_required()
```

#### `skf_gw/source/src/ble/bt_connection_new.c`

保留原有扫描/连接框架，并加入以下最终修改：

- `BT_CONNECTION_NEW_SCANNED_ELE_TTL_S` 最终调整为 **12 秒**；
- 记录白名单 Sensor 最近真实 RSSI/Device1 活动；
- 在目标 Sensor 最近确实活跃、当前无有效连接、但连接阶段持续找不到可用 Device1 proxy 时置位 recovery detector；
- 成功选中 proxy 时清理 recovery 状态；
- 不把正常睡眠、没有近期 RSSI 的 Sensor 当成故障。

#### `skf_gw/source/src/ble/app_pro_ble_new.c`

保留现有 BlueZ/BLE startup 初始化，同时：

- periodic connect 获取 `bt_con_n_connect_to_dev()` 的返回值；
- 当连接失败并命中 proxy recovery detector 时，主进程以非零状态退出；
- `skf-gw.service` 使用 `Restart=on-failure` 自动创建全新的 `skfgw` 进程；
- 普通 Device1/proxy 状态恢复不需要把应用改成 root；
- 现有 `request_privileged_bluetooth_recovery()` 继续作为真正 Bluetooth daemon/full BLE stack recovery 的 request 入口。

---

### 2.6 串口、按键和 LTE USB 使用最小设备权限

`udev/99-skf-gateway.rules` 最终包含：

```udev
# SKF Gateway Modbus UART
SUBSYSTEM=="tty", KERNEL=="ttyS3", GROUP="skfgw", MODE="0660"

# SKF Gateway GPIO physical key
SUBSYSTEM=="input", KERNEL=="event*", ENV{ID_PATH}=="platform-gpio-keys1", GROUP="skfgw", MODE="0640"

# SKF Gateway LTE USB modem
SUBSYSTEM=="usb", ATTR{idVendor}=="2c7c", ATTR{idProduct}=="0125", GROUP="skfgw", MODE="0660"
```

目标权限：

```text
/dev/ttyS3                     root:skfgw 0660
GPIO key event                 root:skfgw 0640
Quectel LTE USB 2c7c:0125      root:skfgw 0660
应用需要的 GPIO value          root:skfgw 0620
```

不再使用 `chmod 777` 解决设备访问。

---

## 3. Bluetooth / HCI 修改点

### 3.1 HCI 与 BlueZ 正式启动链

正式职责：

| 文件 | 最终职责 |
|---|---|
| `scripts/bluetooth-hci-main.sh` | root 初始化 HCI 主流程。 |
| `scripts/bluetooth-hci-up.sh` | root 拉起/检查 HCI。 |
| `scripts/bluetooth-provision.sh` | root 完成 Bluetooth provisioning。 |
| `systemd/skf-gw-bluetooth-init.service` | root HCI 初始化 service。 |
| `systemd/skf-gw-bluetooth-provision.service` | root provisioning service。 |
| `systemd/bluetooth.service.d/10-skf-hci.conf` | 约束 BlueZ 与 HCI 初始化顺序。 |
| `compat/bluetooth` | `/etc/init.d/bluetooth` 兼容入口，仅委托 systemd。 |

正式系统只保留一套 HCI 初始化链和一个 `bluetoothd`。

旧 `restart_ble.sh/start.sh` 不再作为 production 启动链。

---

### 3.2 新增 HCI Connection Update root helper

新增/最终保留：

```text
scripts/hci-conn-update-dispatch.sh
systemd/skf-gw-hci-conn-update.service
systemd/skf-gw-hci-conn-update.path
```

固定运行路径：

```text
skf_gw(skfgw)
  ↓
/run/skf-gateway/hci-conn-update/requests/*.req
  ↓
skf-gw-hci-conn-update.path
  ↓
skf-gw-hci-conn-update.service (root)
  ↓
hci-conn-update-dispatch.sh
  ↓
/opt/skf-gateway/bin/skf_gw --priv-hci-conn-update ...
  ↓
results/*.result
```

约束：

- request 必须由业务侧按固定格式生成；
- dispatcher 校验 token、MAC 和数值参数；
- 结果文件 `root:skfgw 0640`；
- 单次 HCI 请求失败通过 result 返回；
- dispatcher 自身正常退出，避免 `.path` unit 因单次业务请求失败进入 start-limit；
- 不存在第二个 `/opt/skf-gateway/bin/skf_hci_conn_update`。

---

### 3.3 正式 Bluetooth Recovery

临时 BlueZ refresh 方案最终合并进现有正式链，不再增加第二套 `bluez-refresh.*` 文件。

最终对应关系：

```text
原 bluez-refresh-request.sh
  → app_pro_ble_new.c::request_privileged_bluetooth_recovery()

原 skf-gw-bluez-refresh.service
  → skf-gw-bluetooth-recover.service

原 bluez-refresh.sh
  → scripts/bluetooth-recover.sh

原临时 path
  → skf-gw-bluetooth-recover.path
```

正式链：

```text
skf_gw(skfgw)
  ↓ request
/run/skf-gateway/requests/bluetooth-recover
  ↓
skf-gw-bluetooth-recover.path
  ↓
skf-gw-bluetooth-recover.service (root)
  ↓
bluetooth-recover.sh
```

`bluetooth-recover.sh` 最终职责：

```text
记住 Modbus 是否原本 active
→ stop skf-gw.service
→ 确认旧 GW 已停止
→ restart bluetooth.service
→ 等待 bluetooth active
→ settling interval
→ start fresh skf-gw.service
→ 如需要恢复 Modbus
→ 写入 bluetooth-recover.result
```

`skf-gw-bluetooth-recover.service` 使用 root oneshot，`TimeoutStartSec=60`。

不在该 helper 中重启 system D-Bus，也不恢复旧 broad `rfkill/dbus reload/restart_ble.sh` 链。

---

## 4. Network Recovery 修改点

### 4.1 新增 root dispatcher

新增：

```text
scripts/network-recovery-dispatch.sh
```

监听：

```text
/run/skf-gateway/network-recovery/requests
```

结果写入：

```text
/run/skf-gateway/network-recovery/results
```

允许的白名单操作：

```text
probe
gpio_init
lte_power
wwan_managed
lte_connect
dhcp
default_route
```

dispatcher 只接受固定 operation 和参数格式，不提供通用 shell/root 命令执行入口。

### 4.2 新增 Network Recovery units

新增：

```text
systemd/skf-gw-network-recovery.service
systemd/skf-gw-network-recovery.path
```

其中：

- `.path` 在 request 目录非空时触发；
- `.service` 以 `root:root` oneshot 执行 `network-recovery-dispatch.sh`；
- MQTT service 只需要向 request 目录写入请求，本身不获得 `CAP_NET_ADMIN`。

### 4.3 新增 Quectel service

新增：

```text
systemd/skf-gw-quectel.service
```

由 root systemd 管理 `/usr/bin/quectel-CM` 的启动/停止，替代普通业务代码直接 fork/kill root 网络管理进程。

---

## 5. systemd 服务模型修改点

旧架构：

```text
root start.sh / restart_*.sh
        ↓
mqtt/sql/bluetooth/gw/modbus
```

最终正式架构：

```text
skf-gateway.target
 ├─ skf-gw-sql.service
 ├─ skf-gw.service
 ├─ skf-gw-mqtt.service
 ├─ skf-gw-modbus.service
 ├─ skf-gw-bluetooth-recover.path
 ├─ skf-gw-hci-conn-update.path
 └─ skf-gw-network-recovery.path
```

四个 application service 均加入：

```ini
PartOf=skf-gateway.target
```

让 `skf-gateway.target` 成为唯一正式应用 lifecycle owner。

### `skf-gw.service`

核心：

```ini
User=skfgw
Group=skfgw

WorkingDirectory=/var/lib/skf-gateway
Environment=HOME=/var/lib/skf-gateway

Restart=on-failure
RestartSec=5

UMask=0077

AmbientCapabilities=CAP_NET_RAW
CapabilityBoundingSet=CAP_NET_RAW
NoNewPrivileges=true
```

同时依赖/排序到：

```text
GPIO init
Bluetooth
SQL
Bluetooth recovery path
HCI connection update path
```

### `skf-gw-sql.service`

最终：

```text
User=skfgw
Group=skfgw
Before=skf-gw.service
Before=skf-gw-mqtt.service
Restart=always
RestartSec=5
NoNewPrivileges=true
PartOf=skf-gateway.target
```

### `skf-gw-mqtt.service`

最终：

```text
User=skfgw
Group=skfgw
Restart=always
RestartSec=5
NoNewPrivileges=true
PartOf=skf-gateway.target
Wants/After=skf-gw-network-recovery.path
```

不添加 capability。

### `skf-gw-modbus.service`

继续以 `skfgw` 运行，保留对 GW/SQL 的应用依赖，并加入：

```ini
PartOf=skf-gateway.target
```

### `skf-gateway.target`

最终至少包含：

```ini
Wants=skf-gw-sql.service
Wants=skf-gw.service
Wants=skf-gw-mqtt.service
Wants=skf-gw-modbus.service

Wants=skf-gw-bluetooth-recover.path
Wants=skf-gw-hci-conn-update.path
Wants=skf-gw-network-recovery.path
```

并在 Bluetooth/HCI 初始化之后进入正式应用生命周期。

---

## 6. tmpfiles 修改点

`tmpfiles/skf-gateway.conf` 最终至少包含：

```text
d /run/skf-gateway 0755 root root -
d /run/skf-gateway/ipc 0700 skfgw skfgw -
d /run/skf-gateway/requests 0700 skfgw skfgw -
d /run/skf-gateway/state 0750 root skfgw -

d /var/lib/skf-gateway 0700 skfgw skfgw -
d /var/lib/skf-gateway/config 0700 skfgw skfgw -
d /var/lib/skf-gateway/data 0700 skfgw skfgw -
d /var/lib/skf-gateway/update 0700 skfgw skfgw -

d /run/skf-gateway/hci-conn-update 0750 root skfgw -
d /run/skf-gateway/hci-conn-update/requests 0700 skfgw skfgw -
d /run/skf-gateway/hci-conn-update/results 0750 root skfgw -

d /run/skf-gateway/network-recovery 0750 root skfgw -
d /run/skf-gateway/network-recovery/requests 0700 skfgw skfgw -
d /run/skf-gateway/network-recovery/results 0750 root skfgw -
```

这样 `/run` 每次重启被清空后，IPC/helper 所需目录会自动恢复。

---

## 7. 编译与打包系统修改点

| 文件 | 最终修改 |
|---|---|
| `skf_gw/CMakeLists.txt` | 使用正确的 cross toolchain/target rootfs 库路径；现有 `skf_gw` 已包含 `main.c`、`app_pro_general_new.c`、`app_pro_ble_new.c`、`bt_hcitool.c`，不新增第二个 HCI binary target。 |
| `skf_gw/source/include/sys_timer.h` | 在 ELL 使用前补齐 `stdbool.h` 依赖。 |
| MQTT/SQL build 配置 | 修正 libusb/middleware/protobuf/nanopb 等路径。 |
| Modbus Makefile/build | 切换到当前 middleware 路径并确保使用 AArch64 toolchain。 |
| `skf_gw_mqtt/build.sh` | `clean` 不再 `rm -rf deployment/skf_package/*`；只清理构建产物；MQTT/SQL 最终复制到 `deployment/skf_package/bin/`。 |
| `build_skf_package.sh` | 统一从真实 build 产物重建 `bin/`，保留 scripts/systemd/tmpfiles/udev/install 文件，重新生成 manifest 和 tar 包。 |

正式 package runtime binary 只保留：

```text
bin/skf_gw
bin/mqtt_op
bin/sql_op
bin/skf_gw_modbus_rtu
```

不再在 package 根目录保留：

```text
skf_gw
skf_gw_1
mqtt_op
sql_op
```

也不再保留：

```text
start.sh
restart_ble.sh
restart_mqtt.sh
restart_sql.sh
restart_mod.sh
```

作为正式 runtime supervisor。

---

## 8. 一机一密修改点

### `scripts/provision-credentials.sh`

只处理：

```text
root
skfgw
```

逻辑：

1. 从 `/dev/urandom` 分别生成两个随机密码；
2. 分别执行 `chpasswd`；
3. 写入仅 root 可读的首次凭证文件；
4. 创建 marker，后续不重复生成。

典型状态：

```text
/var/lib/skf-gateway/provision/
├── credentials.initialized
├── initial-root-password
└── initial-skfgw-password
```

权限：

```text
root:root 0600
```

**严禁在制作公共 rootfs 时执行此脚本。**

公共 rootfs 中不能存在：

```text
credentials.initialized
initial-root-password
initial-skfgw-password
```

---

## 9. `install.sh` 最终修改点

文件：

```text
skf_gw_mqtt/deployment/skf_package/install.sh
```

最终职责包括：

- 校验 package 基础文件；
- 停止 `skf-gateway.target`，再兼容停止旧版本各 member service；
- `disable --now autorun.service`，退出旧 root supervisor；
- 关闭旧 `skf_gw.service`（下划线旧名称）及历史启动链；
- 只清理旧 supervisor 脚本/进程，不粗暴删除 legacy DB 目录；
- 创建/修正 `skfgw`，home=`/var/lib/skf-gateway`，shell=`/bin/bash`；
- 清理旧 `gwadmin`；
- 验证现有数据库；必要时从健康 legacy DB 迁移到 `/var/lib/skf-gateway/skf.db`；
- 安装四个 root-owned application binary；
- 安装配置文件、Bluetooth per-device env；
- 安装所有 root helper、systemd、tmpfiles、udev、SSH、Reverse SSH 文件；
- 清理旧 root IPC；
- 校验/修正 production MQTT broker；
- 生成/显示首次凭证；
- 只 enable `skf-gateway.target` 和 firstboot credential service；
- 安装完成前必须确认核心 application service 真正处于 active 状态。

### 9.1 最终 helper 安装列表

```text
gpio-init.sh
bluetooth-hci-main.sh
bluetooth-hci-up.sh
bluetooth-provision.sh
bluetooth-recover.sh
provision-credentials.sh
hci-conn-update-dispatch.sh
network-recovery-dispatch.sh
```

### 9.2 最终 systemd 安装列表

```text
skf-gateway.target

skf-gw.service
skf-gw-sql.service
skf-gw-mqtt.service
skf-gw-modbus.service

skf-gw-gpio-init.service
skf-gw-bluetooth-init.service
skf-gw-bluetooth-provision.service

skf-gw-bluetooth-recover.service
skf-gw-bluetooth-recover.path

skf-gateway-firstboot-credential.service

skf-gw-hci-conn-update.service
skf-gw-hci-conn-update.path

skf-gw-network-recovery.service
skf-gw-network-recovery.path

skf-gw-quectel.service
```

### 9.3 安装结束采用两阶段 GW 启动

最终安装后的应用启动顺序固定为：

```text
helper .path ready
        ↓
restart bluetooth.service
        ↓
等待 Bluetooth active
        ↓
sleep 3
        ↓
启动 SQL
        ↓
等待 SQL active
        ↓
启动第一代 skf_gw（warm-up）
        ↓
sleep 5
        ↓
systemctl restart skf-gw.service
        ↓
第二代/最终 skf_gw
        ↓
sleep 3
        ↓
start skf-gateway.target
        ↓
MQTT / Modbus 进入正式生命周期
        ↓
校验 SQL / GW / MQTT
        ↓
安装成功
```

第二次启动后的 `skf_gw` **仍然是**：

```text
User=skfgw
Group=skfgw
CAP_NET_RAW only
NoNewPrivileges=true
```

不通过扩大应用权限实现该启动闭环。

---

## 10. SSH / Reverse SSH 修改点

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

- `skfgw` 不允许远程 SSH；
- `root` 不允许从 LAN/WAN 直接密码 SSH；
- Reverse SSH 从 Gateway 本机 loopback 回到 sshd，因此允许 root 登录；
- `skfgw` 仍可用于本地 console。

### Reverse SSH

`gateway-reverse-ssh-setup-light.sh` 最终规则：

- Gateway 端 Reverse SSH 用户为 `root`；
- 每台 Gateway 自己生成独立 Ed25519 key；
- 私钥不进入公共 rootfs；
- `reverse-ssh-control` 为 `root:root 0700`；
- `skfgw` 无 sudo、不能调用控制器；
- `open` = 72 小时；
- `close` = 正常关闭；
- `always` 仅保留给 root 工程调试；
- 老网关只有在 `Reverse SSH: online` 后，才备份并淘汰旧 `frpc.service`。

最终：

```text
frpc.service -> /dev/null
masked
inactive
```

---

## 11. 当前最终修改/新增文件总览

| 文件 | 类型 | 最终内容摘要 |
|---|---|---|
| `skf_gw/source/src/main.c` | 修改 | IPC/普通用户启动适配；增加唯一 `--priv-hci-conn-update` privileged mode。 |
| `skf_gw/source/src/general/app_pro_general_new.c` | 修改 | HCI Connection Update 改为 request/result root helper；正常采集与 FUOTA 均使用同一入口。 |
| `skf_gw/source/include/ble/bt_connection_new.h` | 修改 | 增加 proxy recovery detector 对外接口。 |
| `skf_gw/source/src/ble/bt_connection_new.c` | 修改 | scanned proxy TTL=12；增加白名单 RSSI/proxy 异常检测和 recovery 状态。 |
| `skf_gw/source/src/ble/app_pro_ble_new.c` | 修改 | persistent proxy 异常时非零退出，由 systemd 重建 GW；保留 privileged Bluetooth recovery request。 |
| `skf_gw/source/src/general/app_io.c` | 修改 | 普通用户不再做 GPIO export/direction。 |
| `skf_gw/source/src/app_common.c` | 修改 | root 硬件初始化从业务路径剥离。 |
| `skf_gw_mqtt/middleware/common/global.h` | 修改 | 数据、配置、DB、IPC 路径迁移。 |
| `skf_gw_mqtt/middleware/common/app_lte.c` | 修改 | 六个网络/LTE root primitive 改为同步 Network Recovery request。 |
| `skf_gw_mqtt/middleware/sh_mem/sh_mem.h` | 修改 | shared-memory/key 路径统一。 |
| `skf_gw_mqtt/middleware/sh_mem/sh_mem.c` | 修改 | 0600 安全创建及旧 IPC 兼容清理。 |
| `modbus_rtu/modbus_slave/app_mslave.c` | 修改 | 配置迁到 `/var/lib/skf-gateway/config`。 |
| `modbus_rtu/modbus_slave/app_mslave_ipc.c` | 修改 | IPC 迁到 `/run/skf-gateway/ipc`。 |
| `scripts/gpio-init.sh` | 新增/修改 | root GPIO 初始化和最小 owner/group/mode。 |
| `scripts/bluetooth-hci-main.sh` | 新增/修改 | root HCI 主初始化。 |
| `scripts/bluetooth-hci-up.sh` | 新增/修改 | root HCI up/check。 |
| `scripts/bluetooth-provision.sh` | 新增/修改 | root Bluetooth provisioning。 |
| `scripts/bluetooth-recover.sh` | 新增/修改 | full BLE recovery：stop GW → restart Bluetooth → fresh GW，并恢复必要的 Modbus 状态。 |
| `scripts/hci-conn-update-dispatch.sh` | 新增 | 校验并执行受控 HCI connection update。 |
| `scripts/network-recovery-dispatch.sh` | 新增 | 执行白名单 Network/LTE root primitive。 |
| `scripts/provision-credentials.sh` | 新增/修改 | root/skfgw 每机随机密码与 marker。 |
| `systemd/skf-gateway.target` | 修改 | 成为唯一正式应用 lifecycle owner，包含三类 privileged watcher。 |
| `systemd/skf-gw.service` | 修改 | `skfgw`、`CAP_NET_RAW` only、`Restart=on-failure`、`PartOf=target`。 |
| `systemd/skf-gw-sql.service` | 修改 | SQL 先于 GW/MQTT，`Restart=always`、`PartOf=target`。 |
| `systemd/skf-gw-mqtt.service` | 修改 | 普通用户、无 capability、`Restart=always`、Network Recovery dependency、`PartOf=target`。 |
| `systemd/skf-gw-modbus.service` | 修改 | 普通用户运行，加入 `PartOf=target`，保留 GW/SQL 依赖。 |
| `systemd/skf-gw-bluetooth-init.service` | 新增/修改 | root HCI/Bluetooth 初始化。 |
| `systemd/skf-gw-bluetooth-provision.service` | 新增/修改 | root provisioning。 |
| `systemd/skf-gw-bluetooth-recover.service` | 新增/修改 | root full BLE recovery oneshot，Timeout=60s。 |
| `systemd/skf-gw-bluetooth-recover.path` | 新增 | 监听 Bluetooth recovery request。 |
| `systemd/skf-gw-hci-conn-update.service` | 新增 | root HCI Connection Update dispatcher。 |
| `systemd/skf-gw-hci-conn-update.path` | 新增 | 监听 HCI request 目录。 |
| `systemd/skf-gw-network-recovery.service` | 新增 | root Network Recovery dispatcher。 |
| `systemd/skf-gw-network-recovery.path` | 新增 | 监听 Network Recovery request 目录。 |
| `systemd/skf-gw-quectel.service` | 新增 | root 管理长期 `quectel-CM` 进程。 |
| `systemd/bluetooth.service.d/10-skf-hci.conf` | 新增/修改 | BlueZ 与 HCI init 顺序。 |
| `tmpfiles/skf-gateway.conf` | 修改 | IPC、Bluetooth、HCI、Network helper runtime 目录及权限。 |
| `udev/99-skf-gateway.rules` | 修改 | ttyS3、GPIO key、Quectel USB 最小权限。 |
| `compat/bluetooth` | 修改 | SysV compatibility entry 只委托 systemd。 |
| `install.sh` | 大幅修改 | 账号/DB/文件安装、旧 lifecycle 退出、helper 安装、两阶段 GW 启动和最终健康校验。 |
| `skf_gw_mqtt/build.sh` | 修改 | 不再清空 deployment package；MQTT/SQL 统一输出到 `bin/`。 |
| `build_skf_package.sh` | 修改 | 统一收集最新 ARM64 产物、manifest、tar；不生成第二 HCI binary。 |
| `ssh/60-skf-gateway.conf` | 修改 | root-only Reverse SSH 边界；禁止 LAN/WAN 密码直连。 |
| `tools/reverse-ssh-light-tools/gateway-reverse-ssh-setup-light.sh` | 修改 | 每机 key、root-only 控制、72h 窗口、确认 online 后淘汰 frpc。 |

---

# 二、如何把最终内容写入 rootfs

## 1. 重新生成最终 package

开发机：

```bash
cd /home/forlinx/GW/GW

./build_skf_package.sh
```

最终 package：

```text
/home/forlinx/GW/GW/skf_gateway_install/skf_package.tar.gz
```

部署目录：

```text
/home/forlinx/GW/GW/skf_gw_mqtt/deployment/skf_package
```

建议确认：

```bash
find skf_gw_mqtt/deployment/skf_package \
    -maxdepth 3 -type f | sort

sha256sum \
    skf_gw_mqtt/deployment/skf_package/bin/*
```

四个正式 binary 必须为：

```text
ELF 64-bit ARM aarch64
```

---

## 2. 设置 rootfs

```bash
export ROOTFS=/home/forlinx/work/OK62xx-linux-sdk/OK62xx-linux-fs/rootfs
export PKG=/home/forlinx/GW/GW/skf_gw_mqtt/deployment/skf_package
```

建立主要目录：

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

目标：

```text
/opt/skf-gateway/bin/*
root:root 0755
```

不要在 rootfs 中再放第二套：

```text
/home/root/.../skf_gw_1
/home/root/.../mqtt_op
/home/root/.../sql_op
```

作为正式 runtime。

---

## 4. 写入 root helper

```bash
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

这里只复制脚本，**不要在公共 rootfs 构建阶段执行凭证或 Reverse SSH provisioning**。

---

## 5. 写入 systemd units

```bash
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
    sudo install -o root -g root -m 0644 \
        "$PKG/systemd/$f" \
        "$ROOTFS/etc/systemd/system/$f"
done
```

Bluetooth drop-in：

```bash
sudo install -o root -g root -m 0644 \
"$PKG/systemd/bluetooth.service.d/10-skf-hci.conf" \
"$ROOTFS/etc/systemd/system/bluetooth.service.d/10-skf-hci.conf"
```

兼容入口：

```bash
sudo install -o root -g root -m 0755 \
"$PKG/compat/bluetooth" \
"$ROOTFS/etc/init.d/bluetooth"
```

---

## 6. 写入 tmpfiles / udev / SSH

```bash
sudo install -o root -g root -m 0644 \
"$PKG/tmpfiles/skf-gateway.conf" \
"$ROOTFS/etc/tmpfiles.d/skf-gateway.conf"

sudo install -o root -g root -m 0644 \
"$PKG/udev/99-skf-gateway.rules" \
"$ROOTFS/etc/udev/rules.d/99-skf-gateway.rules"

sudo install -o root -g root -m 0644 \
"$PKG/ssh/60-skf-gateway.conf" \
"$ROOTFS/etc/ssh/sshd_config.d/60-skf-gateway.conf"
```

---

## 7. rootfs 中只保留 `root + skfgw`

删除旧 `gwadmin`：

```bash
for f in passwd shadow group gshadow passwd- shadow- group- gshadow- subuid subgid; do
    [ -f "$ROOTFS/etc/$f" ] && \
    sudo sed -i '/^gwadmin:/d' "$ROOTFS/etc/$f"
done

sudo rm -rf "$ROOTFS/home/gwadmin"
sudo rm -f "$ROOTFS/etc/sudoers.d/70-gwadmin-reverse-ssh"
```

确保 `skfgw`：

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

## 8. 修正 rootfs 数据目录 owner

不要硬编码 UID/GID：

```bash
SKFGW_UID=$(awk -F: '$1=="skfgw"{print $3}' "$ROOTFS/etc/passwd")
SKFGW_GID=$(awk -F: '$1=="skfgw"{print $3}' "$ROOTFS/etc/group")
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

配置文件按：

```text
skfgw:skfgw 0600
```

处理。

---

## 9. 清理旧 production lifecycle

公共 rootfs 不再启用：

```text
autorun.service
旧 skf_gw.service
start.sh
restart_ble.sh
restart_mqtt.sh
restart_sql.sh
restart_mod.sh
```

全新 rootfs 中可直接移除这些旧 supervisor 文件。

老设备 in-place migration 时只删除 supervisor，不要在数据库迁移完成前粗暴删除整个：

```text
/home/root/skf_gw_mqtt
```

因为该目录可能仍被用作 legacy DB fallback。

---

## 10. rootfs 中默认关闭旧 frpc

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

## 11. 全新设备 Reverse SSH 默认 `closed`

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

## 12. enable 正式 target 与 firstboot

```bash
sudo systemctl --root="$ROOTFS" enable skf-gateway.target
sudo systemctl --root="$ROOTFS" enable skf-gateway-firstboot-credential.service
```

四个业务 service 不需要单独 enable，由 `skf-gateway.target` 统一管理。

---

## 13. rootfs 构建阶段严禁事项

不要执行：

```text
provision-credentials.sh
gateway-reverse-ssh-setup-light.sh
固定 root/skfgw 密码
生成并复制共用 Reverse SSH 私钥
启动业务进程
```

公共 rootfs 应只包含：

```text
程序
systemd units
udev/tmpfiles
helper scripts
SSH policy
```

不包含每台设备的秘密。

制作镜像前强制删除：

```bash
rm -f \
"$ROOTFS/var/lib/skf-gateway/provision/credentials.initialized" \
"$ROOTFS/var/lib/skf-gateway/provision/initial-root-password" \
"$ROOTFS/var/lib/skf-gateway/provision/initial-skfgw-password"
```

并确认：

```bash
find \
"$ROOTFS/var/lib/skf-gateway/provision" \
-maxdepth 1 \
-type f \
-print
```

无输出。

---

# 三、如何在老网关验证最终效果与安全性

## 1. 上传安装包并校验

建议使用：

```text
/home/root/skf-validation/package/
/home/root/skf-validation/extracted/
```

开发机：

```bash
sha256sum skf_gateway_install/skf_package.tar.gz
```

Gateway：

```bash
mkdir -p \
/home/root/skf-validation/package \
/home/root/skf-validation/extracted

cd /home/root/skf-validation/package
sha256sum skf_package.tar.gz
```

两端 hash 必须一致。

解包：

```bash
tar -xzf skf_package.tar.gz \
-C /home/root/skf-validation/extracted
```

如有 manifest：

```bash
cd /home/root/skf-validation/extracted/skf_package
sha256sum -c manifest.sha256
```

必须全部 `OK`。

---

## 2. 安装前保留现有远程入口

老设备仍通过 `frpc` 维护时，不要在 Reverse SSH 尚未建立前手工 stop/mask。

原则：

```text
保留现有远程入口
→ 安装新 package
→ 建立并验证 Reverse SSH
→ 再淘汰 frpc
```

---

## 3. 执行最终安装

```bash
cd /home/root/skf-validation/extracted/skf_package

./install.sh --mac <设备真实MAC>
```

PRO 型：

```bash
./install.sh --mac <设备真实MAC> --pro
```

最终 installer 自己负责：

```text
退出旧 lifecycle
→ 安装新文件
→ 创建 runtime 目录
→ 配置 udev/tmpfiles
→ 启动 helper watcher
→ clean Bluetooth
→ SQL
→ 第一代 GW warm-up
→ 自动 restart GW
→ 最终 GW
→ 启动 target
→ MQTT/Modbus
→ 核心服务校验
```

安装成功后**不应再需要人工执行**：

```bash
systemctl restart skf-gw.service
systemctl restart bluetooth.service
```

才能完成正常首次运行。

---

## 4. 安装后静态检查

### 4.1 用户

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

### 4.2 四个 application service

```bash
for s in \
skf-gw.service \
skf-gw-sql.service \
skf-gw-mqtt.service \
skf-gw-modbus.service
do
    systemctl cat "$s" | \
    grep -E \
    'PartOf=|User=|Group=|ExecStart=|Restart=|NoNewPrivileges|AmbientCapabilities|CapabilityBoundingSet'
done
```

目标：

```text
User=skfgw
Group=skfgw
PartOf=skf-gateway.target
NoNewPrivileges=true
```

仅 `skf-gw.service` 应具有：

```text
AmbientCapabilities=CAP_NET_RAW
CapabilityBoundingSet=CAP_NET_RAW
```

不能出现：

```text
CAP_NET_ADMIN
User=root
```

### 4.3 lifecycle target

```bash
systemctl cat skf-gateway.target
```

应包含：

```text
skf-gw-sql.service
skf-gw.service
skf-gw-mqtt.service
skf-gw-modbus.service

skf-gw-bluetooth-recover.path
skf-gw-hci-conn-update.path
skf-gw-network-recovery.path
```

### 4.4 root helper units

```bash
systemctl status \
skf-gw-bluetooth-recover.path \
skf-gw-hci-conn-update.path \
skf-gw-network-recovery.path \
--no-pager -l
```

正常应为：

```text
active / waiting
```

### 4.5 二进制

```bash
stat -c '%a %U:%G %n' \
/opt/skf-gateway/bin/*
```

目标：

```text
755 root:root
```

---

## 5. 建立 Reverse SSH，再淘汰老 frpc

运行：

```bash
/usr/libexec/skf-gateway/gateway-reverse-ssh-setup-light.sh
```

先确认：

```text
Reverse SSH: online
reverse-ssh.service: active
```

之后再检查：

```bash
systemctl is-enabled frpc.service
systemctl is-active frpc.service
ls -l /etc/systemd/system/frpc.service
```

最终：

```text
masked
inactive
frpc.service -> /dev/null
```

正常维护窗口：

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

关闭：

```bash
/usr/bin/reverse-ssh-control close
```

---

## 6. 重启后验收

```bash
reboot
```

### 6.1 应用生命周期

```bash
systemctl is-active skf-gateway.target

systemctl is-active \
skf-gw.service \
skf-gw-sql.service \
skf-gw-mqtt.service \
skf-gw-modbus.service
```

全部应为：

```text
active
```

业务身份：

```bash
ps -eo user,pid,cmd | \
grep -E '/opt/skf-gateway/bin/(skf_gw|sql_op|mqtt_op|skf_gw_modbus_rtu)' | \
grep -v grep
```

四个业务进程都必须显示：

```text
skfgw
```

### 6.2 实际 capability

```bash
PID="$(systemctl show skf-gw.service -p MainPID --value)"

grep -E \
'^(Uid|Gid|Groups|CapInh|CapPrm|CapEff|CapBnd|CapAmb|NoNewPrivs):' \
"/proc/$PID/status"
```

目标：

- UID/GID 对应 `skfgw`；
- `NoNewPrivs=1`；
- GW 只有 `CAP_NET_RAW`；
- MQTT/SQL/Modbus 无额外 capability。

### 6.3 数据与 IPC

```bash
stat -c '%a %U:%G %n' \
/var/lib/skf-gateway \
/var/lib/skf-gateway/config \
/var/lib/skf-gateway/data \
/run/skf-gateway \
/run/skf-gateway/ipc \
/run/skf-gateway/hci-conn-update/requests \
/run/skf-gateway/hci-conn-update/results \
/run/skf-gateway/network-recovery/requests \
/run/skf-gateway/network-recovery/results

ipcs -q
ipcs -m
```

目标：

```text
应用数据/IPC owner = skfgw
System V IPC perms = 600
helper request 目录只允许 skfgw 写
helper result 目录由 root 管理，skfgw 可读结果
```

### 6.4 设备权限

```bash
stat -c '%a %U:%G %n' /dev/ttyS3
stat -c '%a %U:%G %n' /dev/input/by-path/platform-gpio-keys1-event 2>/dev/null || true
```

Quectel USB：

```bash
lsusb | grep -i '2c7c:0125'
```

目标权限：

```text
ttyS3        root:skfgw 0660
GPIO key     root:skfgw 0640
LTE USB      GROUP=skfgw MODE=0660
```

### 6.5 Bluetooth / HCI

```bash
systemctl status \
skf-gw-bluetooth-init.service \
skf-gw-bluetooth-provision.service \
skf-gw-bluetooth-recover.path \
skf-gw-hci-conn-update.path \
bluetooth.service \
--no-pager -l

hciconfig -a

ps -ef | \
grep -E 'bluetoothd|hciattach' | \
grep -v grep
```

目标：

- `hci0` 为 `UP RUNNING`；
- 只有预期的一套 HCI attach；
- 只有一个 `bluetoothd`；
- HCI/Bluetooth privileged watcher 正常；
- 无旧 `restart_ble.sh/start.sh` 反复拉进程。

### 6.6 Network Recovery

```bash
systemctl status \
skf-gw-network-recovery.path \
skf-gw-quectel.service \
--no-pager -l
```

`skf-gw-network-recovery.path` 应保持 waiting；`skf-gw-quectel.service` 是否 active 由当前 LTE 状态决定。

`mqtt_op` 本身仍应保持：

```text
User=skfgw
NoNewPrivileges=true
无 CAP_NET_ADMIN
```

### 6.7 Sensor 端到端验收

安装/重启后不做人工 GW/Bluetooth restart，正常唤醒真实 Sensor。

验收目标：

```text
Sensor 被扫描
→ Device1 proxy 可用
→ BLE connect
→ service ready
→ privileged HCI connection update
→ Read Version / config exchange
→ collect data
→ SensorData 写入 SQLite
→ mqtt_op 上传
→ 云平台在线
```

数据库确认新数据：

```bash
sqlite3 -header -column \
/var/lib/skf-gateway/skf.db "
SELECT
    SequenceNumber,
    macAddr,
    datetime(ReceivedTimestamp,'unixepoch','localtime') AS received_time,
    DataType,
    Sent
FROM SensorData
WHERE macAddr='<Sensor MAC 无冒号>'
ORDER BY SequenceNumber DESC
LIMIT 20;
"
```

最终验收标准是：

> **重新烧录/安装完成后，无需人工执行 `systemctl restart skf-gw.service` 或 `systemctl restart bluetooth.service`，Sensor 在正常唤醒后即可进入采集/上传链。**

---

## 7. 权限边界验收

### 7.1 `skfgw` 可以写业务数据

```bash
runuser -u skfgw -- sh -c \
'touch /var/lib/skf-gateway/data/write-test && echo DATA_OK'
```

### 7.2 `skfgw` 不能修改程序和系统目录

```bash
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

### 7.3 Reverse SSH root-only

```bash
stat -c '%a %U:%G %n' /usr/bin/reverse-ssh-control

runuser -u skfgw -- \
/usr/bin/reverse-ssh-control open
```

目标：

```text
/usr/bin/reverse-ssh-control = root:root 0700
skfgw 调用失败
```

### 7.4 SSH 边界

最终应同时满足：

```text
LAN/WAN 直接 root SSH       失败
LAN/WAN 直接 skfgw SSH      失败
Reverse SSH 登录 root        成功
本地 console 登录 skfgw      成功
```

---

## 8. 真正验证“一机一密”

至少使用两台设备 A/B：

```text
root(A)   != root(B)
skfgw(A)  != skfgw(B)

root(A)   != skfgw(A)
root(B)   != skfgw(B)
```

marker：

```bash
stat -c '%a %U:%G %n' \
/var/lib/skf-gateway/provision/credentials.initialized
```

marker 存在后再次重启设备，密码不应重新生成。

Reverse SSH 私钥也必须每台设备不同，不能从公共 rootfs 复制同一私钥。
