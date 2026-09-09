# Gateway Reverse SSH Light 操作手册

## 1. 最终访问模型

Reverse SSH 是 Gateway 的远程维护通道；旧 `frpc.service` 不再作为
fallback。

``` text
Ubuntu:
gwlist  -> 查看在线 Gateway
gwssh   -> 通过 Reverse SSH 登录 Gateway root
gwctl   -> 仅在 Reverse SSH 已在线时远程执行 open / always / close / status
Add
Gateway:
root -> /usr/bin/reverse-ssh-control
skfgw -> 仅运行 Gateway 业务应用，不控制 Reverse SSH
```

`close` 或 `timed` 到期后 Reverse SSH socket 会消失，Ubuntu 端不能再用
`gwctl` 重新唤醒。重新开启必须由 Gateway 本地 root，或后续由具备 root
权限的受控服务接口执行。

------------------------------------------------------------------------

## 2. Root 控制接口

正式控制入口：

``` bash
/usr/bin/reverse-ssh-control open
/usr/bin/reverse-ssh-control status
/usr/bin/reverse-ssh-control close
/usr/bin/reverse-ssh-control always
```

含义：

-   `open`：开启 Reverse SSH，进入 `timed` 模式，默认 72 小时。
-   `status`：查看实际状态。
-   `close`：立即关闭 Reverse SSH，不恢复 Forward SSH。
-   `always`：永久开启，仅保留给 root
    的维护/特殊调试，不作为产品默认模式。

后续手机 APP 不需要直接操作 systemd。APP 对应的 Gateway
侧受控组件只需要以 root 权限调用：

``` bash
/usr/bin/reverse-ssh-control open
```

即可获得新的 72 小时调试窗口。

普通 `skfgw` 不应直接获得该控制权限。

------------------------------------------------------------------------

## 3. 72 小时临时调试

脚本中：

``` bash
DEFAULT_OPEN_SECONDS=259200
```

即：

``` text
259200 秒 = 72 小时 = 3 天
```

root 执行：

``` bash
/usr/bin/reverse-ssh-control open
```

状态应类似：

``` text
Reverse SSH: online
Mode: timed
Expires epoch: ...
```

到期后 health timer 会使状态收敛为：

``` text
Reverse SSH: offline
Mode: closed
Expires epoch: 0
```

再次调试时重新执行 `open`，会重新计算新的 72 小时截止时间。

------------------------------------------------------------------------

## 4. 老 Gateway 与空白新 Gateway 的默认状态

两类设备最终都使用同一个：

``` bash
/usr/bin/reverse-ssh-control
```

区别只在首次部署。

### 4.1 老 Gateway：迁移时首次自动进入 timed

老 Gateway 可能仍依赖 `frpc.service`
作为当前安装连接，因此首次部署不能先关闭 frpc。

推荐首次无 state 时：

``` text
旧 frpc 存在且未 masked
        ↓
初始化 MODE=timed
        ↓
Reverse SSH online
        ↓
备份旧 frpc.service
        ↓
disable / 删除旧 /etc unit / mask
        ↓
最后 stop frpc
        ↓
Reverse SSH 保持最多 72 小时
```

Reverse SSH 未确认 online 时，禁止淘汰 frpc。

迁移完成后不再存在 Forward SSH fallback；72 小时到期后 Reverse SSH
自动关闭。

### 4.2 空白新 Gateway：默认 closed

新 rootfs 中 `frpc.service` 应从镜像开始就是：

``` text
masked
inactive
```

新设备没有迁移需求，因此首次状态应预置为：

``` text
MODE="closed"
EXPIRES_AT="0"
```

烧录后不会因为默认 `always` 自动长期开放 Reverse SSH。

需要调试时，由本地 root 或后续 APP 对应的 root 服务接口执行：

``` bash
/usr/bin/reverse-ssh-control open
```

------------------------------------------------------------------------

## 5. Gateway 部署脚本

正式安装位置：

``` text
/usr/libexec/skf-gateway/gateway-reverse-ssh-setup-light.sh
```

执行必须使用 root：

``` bash
/usr/libexec/skf-gateway/gateway-reverse-ssh-setup-light.sh
```

主要职责：

-   生成/复用本机 Reverse SSH key。
-   安装公钥到指定 Ubuntu。
-   核对 Ubuntu `SERVER_ID`。
-   安装 `reverse-ssh-control`。
-   安装 `reverse-ssh.service`。
-   安装并启用 `reverse-ssh-health.timer`。
-   老 Gateway 仅在 Reverse SSH online 后永久淘汰旧 frpc。
-   不提供 Forward SSH 自动恢复。

老 Gateway 的旧 frpc 备份保存在持久目录：

``` text
/var/lib/reverse-ssh-control/legacy-forward-backup/
```

不要使用 `/tmp` 保存迁移备份。

------------------------------------------------------------------------

## 6. Ubuntu 首次部署

编辑并执行：

``` bash
chmod +x ~/tools/reverse-ssh-light-tools/ubuntu-reverse-ssh-setup-light.sh
sudo ~/tools/reverse-ssh-light-tools/ubuntu-reverse-ssh-setup-light.sh
```

查看 Ubuntu `SERVER_ID`：

``` bash
cat /etc/reverse-ssh-server-id
```

Gateway 脚本中的：

``` bash
UBUNTU_SERVER_ID="..."
```

必须与之对应。

Ubuntu 侧检查：

``` bash
systemctl status frpc-forlinx.service --no-pager
```

应为：

``` text
active (running)
```

------------------------------------------------------------------------

## 7. 在线调试

Ubuntu：

``` bash
gwlist
gwssh <DEVICE_ID>
```

`gwssh` 应登录 Gateway：

``` text
root
```

Reverse SSH 已在线时，也可：

``` bash
gwctl <DEVICE_ID> status
gwctl <DEVICE_ID> open
gwctl <DEVICE_ID> close
gwctl <DEVICE_ID> always
```

注意：`gwctl` 复用当前 Reverse SSH socket。设备已经 offline
后，`gwctl open` 无法重新建立通道。

------------------------------------------------------------------------

## 8. 关闭后由 root 重新开启

如果 Gateway 当前为：

``` text
Reverse SSH: offline
Mode: closed
```

在 Gateway 串口、PuTTY、本地终端或未来的 root 权限服务接口执行：

``` bash
/usr/bin/reverse-ssh-control open
```

即可重新开启 72 小时。

若确实需要永久维护窗口，可由 root 执行：

``` bash
/usr/bin/reverse-ssh-control always
```

产品正常调试流程应优先使用 `open`，不要把 `always` 作为默认状态。

如果当前 shell 本身来自 `gwssh`，执行：

``` bash
/usr/bin/reverse-ssh-control close
```

会切断当前会话，这是预期行为。

------------------------------------------------------------------------

## 9. 状态与 systemd 检查

Gateway：

``` bash
/usr/bin/reverse-ssh-control status
systemctl status reverse-ssh.service --no-pager
systemctl status reverse-ssh-health.timer --no-pager
```

正常 timed：

``` text
Reverse SSH: online
Mode: timed
```

正常 closed：

``` text
Reverse SSH: offline
Mode: closed
```

health timer 应：

``` text
active (waiting)
```

`closed` 时 health timer 不会重新开启 Reverse SSH；`timed` 未到期或
`always` 时，隧道异常会尝试恢复。

故障日志：

``` bash
journalctl -u reverse-ssh.service -n 50 --no-pager
journalctl -u reverse-ssh-health.service -n 50 --no-pager
```

------------------------------------------------------------------------

## 10. Forward SSH 最终状态

### 老 Gateway 完成迁移后

``` bash
systemctl is-enabled frpc.service
systemctl is-active frpc.service
ls -l /etc/systemd/system/frpc.service
```

目标：

``` text
masked
inactive
/etc/systemd/system/frpc.service -> /dev/null
```

旧 unit 已在淘汰前备份。

### 空白新 Gateway

上述状态应直接来自 rootfs，不经历
`frpc active -> Reverse SSH -> frpc stop` 的迁移过程。

禁止通过：

``` bash
systemctl unmask frpc.service
```

恢复 Forward SSH 作为 fallback。

------------------------------------------------------------------------

## 11. 后续 APP 接口边界

当前阶段不实现手机 APP 下发链路，只保留 root 控制入口。

后续实现应遵守：

``` text
手机 APP
   ↓
Gateway 业务/受控 IPC
   ↓
root 权限 helper/service
   ↓
/usr/bin/reverse-ssh-control open
   ↓
Reverse SSH timed 72h
```

不要让 `skfgw` 获得通用 sudo、root shell 或任意 systemctl 权限。

root helper/service
只需要暴露"开启临时调试"这一窄接口；每次合法请求调用一次 `open`
即可刷新为新的 72 小时窗口。

------------------------------------------------------------------------

## 12. 使用 gwscp 传输文件

`gwscp` 通过 Gateway 建立的 Reverse SSH 通道传输文件，无需使用正向 SSH、FRP 公网端口或手动配置 `ProxyCommand`。

### 前提条件

先确认 Gateway 已建立 Reverse SSH 通道：

```bash
gwlist
```

输出示例：

```text
DEVICE_ID                       STATE
bulletgw120120                  online
```

后续命令中的 `DEVICE_ID` 必须使用 `gwlist` 显示的设备名称。

### 上传文件到 Gateway

命令格式：

```bash
gwscp DEVICE_ID LOCAL_FILE REMOTE_PATH
```

示例：

```bash
gwscp \
  bulletgw120120 \
  /home/forlinx/GW/GW/skf_gateway_install/skf_package.tar.gz \
  /home/root/skf-validation/package/
```

其中：

- `bulletgw120120`：Gateway 的 `DEVICE_ID`
- 第二个参数：Ubuntu 上的本地文件路径
- 第三个参数：Gateway 上的目标路径

目标目录必须已经存在。如果目录不存在，先登录 Gateway：

```bash
gwssh bulletgw120120
```

然后在 Gateway 上创建目录：

```bash
mkdir -p /home/root/skf-validation/package/
exit
```

### 从 Gateway 下载文件

命令格式：

```bash
gwscp DEVICE_ID REMOTE_PATH LOCAL_PATH --download
```

示例：

```bash
gwscp \
  bulletgw120120 \
  /home/root/skf-validation/package/skf_package.tar.gz \
  /home/forlinx/ \
  --download
```

### 验证上传结果

登录 Gateway：

```bash
gwssh bulletgw120120
```

检查文件：

```bash
ls -lh /home/root/skf-validation/package/skf_package.tar.gz
```

如需验证文件完整性，可分别在 Ubuntu 和 Gateway 上执行：

```bash
sha256sum skf_package.tar.gz
```

两端输出的 SHA-256 值应保持一致。

### 常见错误

如果提示 Gateway offline 或 Unix Socket 不存在，请执行：

```bash
gwlist
```

确认：

1. Gateway 状态为 `online`
2. 使用的 `DEVICE_ID` 与 `gwlist` 输出完全一致
3. Gateway 已开启 Reverse SSH 通道

------------------------------------------------------------------------
## 13. 最小验收

老 Gateway：

``` bash
/usr/bin/reverse-ssh-control status
systemctl is-enabled frpc.service
systemctl is-active frpc.service
```

迁移后应为 Reverse SSH `timed/online`（或人工关闭后的
`closed/offline`），且 frpc：

``` text
masked
inactive
```

新 Gateway：

``` bash
/usr/bin/reverse-ssh-control status
```

首次默认应：

``` text
Reverse SSH: offline
Mode: closed
Expires epoch: 0
```

root 执行：

``` bash
/usr/bin/reverse-ssh-control open
```

后应：

``` text
Reverse SSH: online
Mode: timed
```

Ubuntu：

``` bash
gwlist
gwssh <DEVICE_ID>
```

能以 root 进入即完成本次 Reverse SSH 功能验证。

