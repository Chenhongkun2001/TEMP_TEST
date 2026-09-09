# Flow Release Tools

位于 `tools/flow_tools/`，用于从本地 Ubuntu / VS Code 完成 Git 提交、云效 Flow Sanity、指定 Build 执行和构建产物下载。

## 文件

```text
tools/flow_tools/
├── release.sh
├── release.conf
├── get_version.sh
├── sanity_check.sh
├── yunxiao_token.conf   # 本地私有文件，不提交 Git
└── README.md
```

## 配置

使用前请根据个人信息修改固定配置：

```text
tools/flow_tools/release.conf
```

使用前请申请云效Token（个人设置-个人访问令牌）以及构建yunxiao_token.conf并确保Token私密：

```text
tools/flow_tools/yunxiao_token.conf
```

格式：

```bash
YUNXIAO_TOKEN='pt-xxxxxxxx'
```

该文件必须被 `.gitignore` 忽略。

软件版本从 ./skf_gw_mqtt/middleware/common/global.h的`CONFIG_APPLICATION_VERSION` 读取；修改版本时修改源码中的版本定义，不要修改 `release.sh`。

## 日常使用

有本地代码修改时：

```bash
./tools/flow_tools/release.sh \
    --build GW_ALL_NEW_24_PRODUCTS \
    --message "[bugfix] description"
```

流程：本地 Sanity / Build → Git diff → 用户确认 → commit / push → Flow Sanity → 指定 Build → 下载产物。

本地代码没有修改时，可以直接选择其他 Build：

```bash
./tools/flow_tools/release.sh \
    --build GW_ALLACC_NEW_24_PRODUCTS
```

此时脚本不会为了 Build 人为制造代码修改，也不会执行本地 Sanity、Local Build、`git add`、`git commit` 或 `git push`。如果当前 commit 已被本脚本记录为 Flow Sanity PASS，脚本会向 Flow 传入 `SKIP_SANITY=1`；否则该 commit 仍请求执行一次 Flow Sanity。


## 多个 Build

```bash
./tools/flow_tools/release.sh \
    --build GW_ALL_NEW_24_PRODUCTS \
    --build GW_ALLACC_NEW_24_PRODUCTS \
    --message "[build] multiple products"
```

下载目录按 **commit → Flow Run → Build ID** 隔离，例如：

```text
${DOWNLOAD_ROOT}/
└── 0.2.7_ab12cd34/
    └── 20260817_1603_run123/
        ├── GW_ALL_NEW_24_PRODUCTS/
        └── GW_ALLACC_NEW_24_PRODUCTS/
```

因此不同 Build、同一 Build 的不同 Flow Run 都不会在本地下载目录中互相覆盖。Build 多个模块的过程中有Gateway refuse的风险。

## 只运行 Sanity

```bash
./tools/flow_tools/release.sh \
    --sanity-only \
    --message "[test] sanity only"
```

不会执行正式 Build，也不会下载 Build Artifact。

