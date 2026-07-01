# P5 — QGC Zenoh 集成指南

> QGroundControl DDS_P5 分支的 Zenoh 通信后端文档。
> 日期：2026-06-15

---

## 目录

1. [概述](#1-概述)
2. [架构设计](#2-架构设计)
3. [环境准备](#3-环境准备)
4. [编译 zenoh-pico (Linux x86_64)](#4-编译-zenoh-pico-linux-x86_64)
5. [编译 zenoh-pico (Android ARM64)](#5-编译-zenoh-pico-android-arm64)
6. [编译 QGC (启用 Zenoh)](#6-编译-qgc-启用-zenoh)
7. [Android APK 编译](#7-android-apk-编译)
8. [部署架构](#8-部署架构)
9. [配置说明](#9-配置说明)
10. [测试用例](#10-测试用例)
11. [故障排查](#11-故障排查)

---

## 1. 概述

### 1.1 什么是 Zenoh

[Zenoh](https://zenoh.io/) 是 Eclipse 基金会的零开销 pub/sub/query 协议，支持：
- **按需路由**：数据只发送给有订阅者的节点，节省带宽
- **跨网段通信**：通过 Zenoh Router 穿越 NAT/防火墙
- **多传输协议**：TCP、UDP、TLS、WebSocket、串口
- **轻量实现**：zenoh-pico 是纯 C 实现，适合嵌入式和移动端

### 1.2 为什么在 QGC 中集成 Zenoh

| 对比项 | CycloneDDS (现有) | Zenoh (新增) |
|--------|-------------------|--------------|
| 网络发现 | 组播 SPDP（同一子网） | Router 中继（任意网段） |
| 带宽消耗 | 持续心跳 + 组播流量 | 按需订阅，零无效流量 |
| NAT 穿越 | 不支持 | 原生支持（通过 Router） |
| 移动端适配 | 需要 libddsc.so (≈2MB) | libzenohpico.so (≈300KB) |
| 4G/5G 场景 | 不推荐 | 推荐（低开销） |

### 1.3 共存设计

ZenohLink 与 DDSLink 作为**并列的通信后端**存在于 QGC 中：

```
QGC Link Types:
  ├── SerialLink
  ├── UDPLink
  ├── TCPLink
  ├── BluetoothLink
  ├── DDSLink      ← CycloneDDS 通信（局域网 DDS）
  └── ZenohLink    ← zenoh-pico 通信（跨网段 Zenoh）
```

用户在 QGC 界面中选择创建哪种 Link 类型。两者**可以同时启用**。

---

## 2. 架构设计

### 2.1 数据流

```
PX4 飞控
  │  UXRCE-DDS
  ▼
MicroXRCE-DDS Agent
  │  CycloneDDS (DDS 网络)
  ▼
zenoh-bridge-dds          ← 机载电脑或地面站运行
  │  Zenoh 协议 (TCP/UDP)
  ▼
Zenoh Router (可选)       ← 云端/边缘节点
  │  Zenoh 协议
  ▼
QGC ZenohLink (手机/PC)
  │  CDR 解码
  ▼
DDSDataInjector → Fact System → QGC UI
```

### 2.2 组件复用

ZenohLink 复用以下 DDS 基础组件（不重复开发）：

| 组件 | 作用 | 来源 |
|------|------|------|
| `DDSMappingEngine` | JSON 映射表解析 | `src/DDS/` |
| `DDSTransformRegistry` | 数据变换（单位转换等） | `src/DDS/` |
| `DDSDataInjector` | 数据注入 Fact 系统 | `src/DDS/` |
| `DDSTypeRegistry` | IDL 类型注册 + CDR 解析 | `src/DDS/` |
| IDL 生成代码 | C 结构体定义 | `src/DDS/idl/` |

### 2.3 新增文件

```
cmake/FindZenohPico.cmake              # CMake 查找模块
src/Comms/ZenohLink/ZenohConfiguration.h   # 配置类
src/Comms/ZenohLink/ZenohConfiguration.cc
src/Comms/ZenohLink/ZenohLink.h            # 通信链路类
src/Comms/ZenohLink/ZenohLink.cc
```

### 2.4 CMake 编译选项

```cmake
-DQGC_ENABLE_ZENOH=ON       # 启用 Zenoh 后端
-DZENOHPICO_ROOT=/path/to   # zenoh-pico 安装路径

# 可与 DDS 同时启用
-DQGC_ENABLE_DDS=ON -DQGC_ENABLE_ZENOH=ON

# 也可以单独启用 Zenoh（仍需 CycloneDDS 头文件用于 CDR 解析）
-DQGC_ENABLE_ZENOH=ON
```

---

## 3. 环境准备

### 3.1 基础依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git
sudo apt install -y libssl-dev  # zenoh-pico TLS 支持
```

### 3.2 CycloneDDS（IDL 生成所需）

```bash
# 如果还没有安装 CycloneDDS
git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
cd cyclonedds
mkdir build && cd build
cmake .. -DBUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build . --parallel $(nproc)
sudo cmake --install .
```

### 3.3 Android 交叉编译额外依赖

参考 `Doc/P5-Android交叉编译完整手册.md` 获取完整的 Android 环境搭建步骤。

---

## 4. 编译 zenoh-pico (Linux x86_64)

```bash
# 克隆（如果还没有）
git clone --depth 1 https://github.com/eclipse-zenoh/zenoh-pico.git ~/zenoh-pico

# 编译并安装
cd ~/zenoh-pico
mkdir -p build && cd build
cmake .. \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=~/zenoh-pico-install
cmake --build . --parallel $(nproc)
cmake --install .

# 验证
ls ~/zenoh-pico-install/lib/libzenohpico.so
ls ~/zenoh-pico-install/include/zenoh-pico.h
```

---

## 5. 编译 zenoh-pico (Android ARM64)

```bash
# 确保 NDK 路径正确（根据你的实际路径调整）
export ANDROID_NDK=~/Android/Sdk/ndk/27.2.12479018

cd ~/zenoh-pico
mkdir -p build-android && cd build-android
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-28 \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_TOOLS=OFF \
    -DCMAKE_INSTALL_PREFIX=~/zenoh-pico-android
cmake --build . --parallel $(nproc)
cmake --install .

# 验证
file ~/zenoh-pico-android/lib/libzenohpico.so
# 应显示: ELF 64-bit LSB shared object, ARM aarch64
```

---

## 6. 编译 QGC (启用 Zenoh)

### 6.1 桌面版 (Linux x86_64)

```bash
cd ~/qgc-dev   # QGC 源码目录

cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DQGC_ENABLE_DDS=ON \
    -DQGC_ENABLE_ZENOH=ON \
    -DZENOHPICO_ROOT=~/zenoh-pico-install \
    -G Ninja

cmake --build build --parallel $(nproc)
```

### 6.2 仅 Zenoh（不启用 DDS）

```bash
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DQGC_ENABLE_ZENOH=ON \
    -DZENOHPICO_ROOT=~/zenoh-pico-install \
    -G Ninja

cmake --build build --parallel $(nproc)
```

---

## 7. Android APK 编译

### 7.1 前置条件

- 已完成第 5 步（zenoh-pico Android ARM64 编译）
- 已完成 `Doc/P5-Android交叉编译完整手册.md` 中的环境搭建

### 7.2 编译命令

```bash
cd ~/qgc-android
cmake -B build-android -S . \
    -DCMAKE_TOOLCHAIN_FILE=~/Qt/6.10.3/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake \
    -DQT_HOST_PATH=~/Qt/6.10.3/gcc_64 \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DQGC_ENABLE_DDS=ON \
    -DQGC_ENABLE_ZENOH=ON \
    -DZENOHPICO_ROOT=~/zenoh-pico-android \
    -DCYCLONEDDS_ROOT=~/cyclonedds-android \
    -G Ninja

cmake --build build-android --parallel $(nproc)
```

### 7.3 APK 签名与安装

参考 `Doc/P5-Android交叉编译完整手册.md` 第 7-8 步。

CMake 会自动把 `libzenohpico.so` 打包进 APK（通过 `QT_ANDROID_EXTRA_LIBS`）。

---

## 8. 部署架构

### 8.1 典型部署：机载 Bridge + 手机 Zenoh

```
┌──────────────┐         ┌──────────────────────┐
│  PX4 飞控    │  UART   │  机载电脑             │
│  (UXRCE-DDS) │ ───────→│  MicroXRCE-DDS Agent │
└──────────────┘         │  CycloneDDS          │
                         │  zenoh-bridge-dds    │
                         └──────────┬───────────┘
                                    │ TCP/7447
                         ┌──────────▼───────────┐
                         │  Zenoh Router         │
                         │  (可选，可在云端)      │
                         └──────────┬───────────┘
                                    │ TCP/7447 或 4G
                         ┌──────────▼───────────┐
                         │  QGC 手机端           │
                         │  ZenohLink            │
                         │  (zenoh-pico client)  │
                         └──────────────────────┘
```

### 8.2 安装 zenoh-bridge-dds

```bash
# 在机载电脑上（x86_64 或 aarch64）
# 方法 1：预编译二进制
wget https://github.com/eclipse-zenoh/zenoh-plugin-dds/releases/latest/download/zenoh-bridge-dds-$(uname -m)-unknown-linux-gnu.zip
unzip zenoh-bridge-dds-*.zip
chmod +x zenoh-bridge-dds

# 方法 2：Cargo 编译
cargo install zenoh-bridge-dds

# 启动 Bridge
./zenoh-bridge-dds -d 0 -l tcp/0.0.0.0:7447
# -d 0  = DDS domain ID 0 (与 PX4 Agent 匹配)
# -l    = 监听地址
```

### 8.3 安装 Zenoh Router（可选，用于跨网段）

```bash
# 方法 1：Docker
docker run --init -p 7447:7447/tcp eclipse/zenoh:latest

# 方法 2：预编译二进制
wget https://github.com/eclipse-zenoh/zenoh/releases/latest/download/zenohd-$(uname -m)-unknown-linux-gnu.zip
unzip zenohd-*.zip
./zenohd -l tcp/0.0.0.0:7447
```

---

## 9. 配置说明

### 9.1 ZenohLink 配置项

| 参数 | 说明 | 示例 |
|------|------|------|
| `locator` | Zenoh 连接地址 | `tcp/192.168.1.100:7447` |
| `mode` | 会话模式 | `peer` 或 `client` |
| `vendorMapping` | 映射表 | `_default`、`cuav_x7pro` |
| `namespacePrefix` | Key 前缀 | `rt/` (默认) |

### 9.2 模式选择

- **peer 模式**：适合同一局域网，自动发现其他 peer
- **client 模式**：适合跨网段，需要指定 Router/Bridge 地址

### 9.3 Key-Expression 映射

Zenoh 使用 key-expression 替代 DDS 的 topic name。
zenoh-bridge-dds 自动转换：

| DDS Topic | Zenoh Key-Expression |
|-----------|---------------------|
| `rt/fmu/out/vehicle_status` | `rt/fmu/out/vehicle_status` |
| `rt/fmu/out/vehicle_global_position` | `rt/fmu/out/vehicle_global_position` |
| `rt/fmu/in/vehicle_command` | `rt/fmu/in/vehicle_command` |

名称完全相同，zenoh-bridge-dds 保持 1:1 映射。

---

## 10. 测试用例

### 10.1 基本连接测试

1. 启动 PX4 SITL + MicroXRCE-DDS Agent
2. 启动 zenoh-bridge-dds
3. 在 QGC 中创建 Zenoh Link，填入 bridge 地址
4. 验证：QGC 显示飞机位置、姿态、电池等数据

### 10.2 跨网段测试

1. 在云端启动 Zenoh Router
2. 机载端 bridge 连接 Router
3. QGC 手机端（4G 网络）连接 Router
4. 验证：跨网段数据正常流通

### 10.3 带宽对比测试

1. 同时启用 DDS Link 和 Zenoh Link
2. 使用 `iftop` 或 `nethogs` 监控流量
3. 对比两种链路的带宽消耗

---

## 11. 故障排查

### 11.1 ZenohLink 无法连接

```
排查流程：
1. 确认 Zenoh Router/Bridge 是否运行
   $ zenohd --version     # 或 zenoh-bridge-dds --version

2. 确认网络可达
   $ curl -v telnet://192.168.1.100:7447

3. 确认 zenoh-pico 版本兼容
   QGC 使用 zenoh-pico 1.9.0，Router 版本需 ≥ 1.0.0

4. 检查日志
   在 QGC 启动参数中添加：
   QT_LOGGING_RULES="Comms.ZenohLink=true"
```

### 11.2 接收不到数据

```
排查流程：
1. 确认 zenoh-bridge-dds 连接到了正确的 DDS domain
   $ ./zenoh-bridge-dds -d 0   # domain ID 必须与 Agent 匹配

2. 确认 mapping 表中的 topic 名匹配
   查看 resources/dds_mappings/_default.json

3. 用 zenoh CLI 工具验证数据流
   $ z_sub -k "rt/fmu/out/**" -e tcp/192.168.1.100:7447
```

### 11.3 APK 崩溃：libzenohpico.so not found

与 libddsc.so 相同的问题：确认 CMake 配置中 `QGC_ENABLE_ZENOH=ON` 且
`ZENOHPICO_ROOT` 路径正确。CMake 会自动设置 `QT_ANDROID_EXTRA_LIBS`。

```bash
# 验证 APK 中是否包含 libzenohpico.so
unzip -l build-android/android-build/build/outputs/apk/release/android-build-release-unsigned.apk | grep zenoh
```

---

## 附录 A：zenoh-pico API 快速参考

```c
#include <zenoh-pico.h>

// 创建会话
z_owned_config_t config;
z_config_default(&config);
zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, "tcp/addr:7447");
z_owned_session_t session;
z_open(&session, z_move(config), NULL);
zp_start_read_task(z_loan_mut(session), NULL);
zp_start_lease_task(z_loan_mut(session), NULL);

// 订阅
z_owned_closure_sample_t callback;
z_closure(&callback, handler_fn, NULL, ctx);
z_owned_subscriber_t sub;
z_view_keyexpr_t ke;
z_view_keyexpr_from_str(&ke, "rt/fmu/out/**");
z_declare_subscriber(z_loan(session), &sub, z_loan(ke), z_move(callback), NULL);

// 发布
z_owned_bytes_t payload;
z_bytes_copy_from_buf(&payload, data, len);
z_put(z_loan(session), z_loan(ke), z_move(payload), NULL);

// 清理
z_drop(z_move(sub));
zp_stop_read_task(z_loan_mut(session));
zp_stop_lease_task(z_loan_mut(session));
z_drop(z_move(session));
```

## 附录 B：完整编译脚本

```bash
#!/bin/bash
# build_zenoh_qgc.sh — 一键编译 QGC + Zenoh (桌面版)
set -e

ZENOH_PICO_SRC=~/zenoh-pico
ZENOH_INSTALL=~/zenoh-pico-install
QGC_SRC=~/qgc-dev

echo "=== 步骤 1：编译 zenoh-pico ==="
cd ${ZENOH_PICO_SRC}
mkdir -p build && cd build
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=${ZENOH_INSTALL}
cmake --build . --parallel $(nproc)
cmake --install .

echo "=== 步骤 2：编译 QGC ==="
cd ${QGC_SRC}
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DQGC_ENABLE_DDS=ON \
    -DQGC_ENABLE_ZENOH=ON \
    -DZENOHPICO_ROOT=${ZENOH_INSTALL} \
    -G Ninja
cmake --build build --parallel $(nproc)

echo "=== 完成 ==="
echo "可执行文件: ${QGC_SRC}/build/QGroundControl"
```
