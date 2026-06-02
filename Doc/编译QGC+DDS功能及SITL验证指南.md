# 编译 QGC + DDS 功能及 SITL 验证指南

## 一、概述

本文档描述如何在 QGroundControl 源码中启用 DDS 通信功能进行编译，以及如何使用 PX4 软件在环仿真 (SITL) 验证 DDS 链路。

### 当前阶段 (P0)

P0 阶段的 DDS 功能包含：
- **DDSLink** — 完整的链接管理框架，已集成到 QGC 的 LinkManager
- **DDSConfiguration** — DDS 连接配置（Domain ID、厂商映射、命名空间前缀）
- **DDSMappingEngine** — JSON 映射表加载与 O(1) 查找
- **DDSTransformRegistry** — 12 个内置数据转换器
- **DDSDataInjector** — DDS 消息到 Fact 系统的注入桥接
- **DDSSettings.qml** — QML 设置界面

> **注意**：P0 阶段的 CycloneDDS API 调用为 stub（桩函数），即 QGC 界面可以显示 DDS 链接选项并创建连接，但实际 DDS 数据收发将在 P1 阶段实现。

---

## 二、开发环境要求

| 项目 | 最低版本 | 推荐版本 | 备注 |
|------|---------|---------|------|
| Ubuntu | 22.04 LTS | 24.04 LTS | 目前仅在 Linux 上测试 |
| GCC | 12 | 13+ | 需要 C++20 支持 |
| CMake | 3.25 | 3.28+ | QGC 要求 |
| Qt | 6.8.0 | 6.8.3 | 需包含 Qt Quick、Qt Location 等模块 |
| Python | 3.10 | 3.12 | 构建脚本依赖 |
| CycloneDDS | 0.10.0 | 0.10.5 | P1 阶段需要，P0 可选 |
| PX4-Autopilot | v1.15+ | main 分支 | SITL 测试用 |

---

## 三、编译步骤

### 3.1 安装基础依赖

```bash
# 基础构建工具
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake ninja-build git \
    python3 python3-pip python3-venv \
    pkg-config libfontconfig1-dev libfreetype6-dev \
    libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev \
    libxi-dev libxrender-dev libxcb1-dev libxcb-cursor-dev \
    libxcb-glx0-dev libxcb-keysyms1-dev libxcb-image0-dev \
    libxcb-shm0-dev libxcb-icccm4-dev libxcb-sync-dev \
    libxcb-xfixes0-dev libxcb-shape0-dev libxcb-randr0-dev \
    libxcb-render-util0-dev libxcb-util-dev libxcb-xinerama0-dev \
    libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev \
    libgl1-mesa-dev libglu1-mesa-dev \
    libwayland-dev libwayland-egl1 \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-gl speech-dispatcher-dev \
    libsdl2-dev
```

### 3.2 安装 Qt 6

推荐使用 Qt Online Installer：

```bash
# 下载 Qt Online Installer
wget https://d13lb3tujbc8s0.cloudfront.net/onlineinstallers/qt-online-installer-linux-x64-4.8.1.run
chmod +x qt-online-installer-linux-x64-4.8.1.run
./qt-online-installer-linux-x64-4.8.1.run
```

安装时选择：
- Qt 6.8.3 (或更高版本)
- Desktop gcc_64
- Additional Libraries: Qt Location, Qt Multimedia, Qt Serial Port, Qt Graphs

安装完成后设置环境变量：

```bash
export QT_DIR=~/Qt/6.8.3/gcc_64
export CMAKE_PREFIX_PATH=$QT_DIR
export PATH=$QT_DIR/bin:$PATH
```

### 3.3 克隆代码

```bash
git clone https://github.com/L-a-L-max/qgc_dev.git
cd qgc_dev
git checkout DDS
git submodule update --init --recursive
```

### 3.4 编译 QGC（不启用 DDS）

先验证基础 QGC 可以正常编译：

```bash
mkdir build && cd build
cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=$QT_DIR
ninja -j$(nproc)
```

### 3.5 编译 QGC（启用 DDS）

```bash
mkdir build-dds && cd build-dds
cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=$QT_DIR \
    -DQGC_ENABLE_DDS=ON
ninja -j$(nproc)
```

编译成功后，可执行文件位于 `build-dds/Debug/QGroundControl`。

### 3.6 验证 DDS 编译宏

确认编译时 DDS 宏已正确定义：

```bash
# 在 build-dds 目录下
grep -r "QGC_ENABLE_DDS" compile_commands.json | head -1
# 应该看到 -DQGC_ENABLE_DDS 出现在编译命令中
```

---

## 四、P0 阶段 UI 验证

### 4.1 启动 QGC

```bash
cd build-dds
./Debug/QGroundControl
```

### 4.2 验证 DDS Link 选项

1. **打开通信设置**：点击 QGC 左上角菜单图标 → **Application Settings** → **Comm Links**
2. **添加新链接**：点击 **Add** 按钮
3. **选择链接类型**：在 **Type** 下拉列表中应该能看到 **DDS** 选项
4. **配置 DDS 参数**：
   - **Domain ID**: 设置为 `0`（与 PX4 默认一致）
   - **Vendor Mapping**: 留空使用默认 PX4 映射
   - **Namespace Prefix**: 留空（单飞行器）或填入 `/drone1`（多飞行器）
   - **Auto-Discover Topics**: 勾选
5. **保存并连接**：点击 **OK** 保存，然后点击 **Connect**

### 4.3 预期现象 (P0)

| 操作 | 预期结果 |
|------|---------|
| 下拉列表选择 DDS | 出现 DDS Link Settings 配置面板 |
| 点击 Connect | 连接状态变为 Connected（stub 模式，始终成功） |
| 查看 Application Output / 日志 | 可看到 `DDSLink: DDS link connected on domain 0` 日志 |
| 飞行器显示 | 不会显示飞行器（因为没有真实 DDS 数据） |
| 断开连接 | 连接状态变为 Disconnected |

> **重要**：P0 阶段连接会成功，但由于 CycloneDDS 调用是 stub，不会接收到实际飞行数据。这是预期行为。P1 阶段替换 stub 后将能接收真实遥测数据。

---

## 五、P1 阶段 SITL 验证方案（待实现）

P1 阶段完成后，将可以进行端到端的 SITL 验证。以下是完整的验证流程：

### 5.1 安装 CycloneDDS

```bash
# 从源码编译安装 CycloneDDS
git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
cd cyclonedds
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_EXAMPLES=ON
make -j$(nproc)
sudo make install
sudo ldconfig
```

### 5.2 安装 PX4 SITL

```bash
git clone https://github.com/PX4/PX4-Autopilot.git --recursive
cd PX4-Autopilot
bash ./Tools/setup/ubuntu.sh
```

### 5.3 启动 PX4 SITL + Micro XRCE-DDS Agent

**终端 1：启动 Micro XRCE-DDS Agent**
```bash
# 安装 Agent
git clone https://github.com/eProsima/Micro-XRCE-DDS-Agent.git
cd Micro-XRCE-DDS-Agent
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install

# 启动 Agent（UDP 模式，端口 8888）
MicroXRCEAgent udp4 -p 8888
```

**终端 2：启动 PX4 SITL**
```bash
cd PX4-Autopilot
make px4_sitl gz_x500
```

PX4 启动后会自动通过 Micro XRCE-DDS Client 连接到 Agent，开始发布 DDS 话题。

**终端 3：验证 DDS 话题已发布**
```bash
# 如果安装了 CycloneDDS 工具
ddsspy

# 或者使用 ros2（如果安装了 ROS2）
ros2 topic list
# 应该看到类似以下话题：
# /fmu/out/vehicle_attitude
# /fmu/out/vehicle_global_position
# /fmu/out/battery_status
# /fmu/out/vehicle_status
# ...
```

### 5.4 启动 QGC + DDS

**终端 4：启动 QGC**
```bash
cd qgc_dev/build-dds
./Debug/QGroundControl
```

### 5.5 预期现象 (P1，待实现后)

| 时间 | 预期现象 | 说明 |
|------|---------|------|
| 0-5 秒 | QGC 启动，显示主界面 | 正常启动 |
| 创建 DDS Link | DDS 设置面板显示 | Domain ID = 0 |
| 点击 Connect 后 1-3 秒 | 日志显示 "DDS link connected" | DDS Participant 创建成功 |
| 连接后 1-2 秒 | 日志显示 "Subscribed to N topics" | 映射表中的话题已订阅 |
| 连接后 2-5 秒 | **飞行器图标出现在地图上** | 收到 GPS 数据 |
| 连接后 5 秒 | **仪表盘显示姿态/高度/速度** | 遥测数据通过 Fact 系统注入 |
| 持续运行 | 数据以 ~50Hz 刷新 | 根据 PX4 DDS 发布频率 |
| PX4 解锁起飞 | QGC 显示 ARMED 状态 | vehicle_status 话题 |
| PX4 飞行中 | 姿态球/高度/速度实时更新 | vehicle_attitude + vehicle_global_position |

### 5.6 数据流图

```
┌─────────────────┐     UDP 8888      ┌───────────────────┐
│   PX4 SITL      │ ◄──────────────► │  XRCE-DDS Agent   │
│  (gz_x500)      │   XRCE-DDS       │                   │
│                  │   Protocol       │                   │
└─────────────────┘                   └───────┬───────────┘
                                              │
                                    DDS Domain 0 (UDP multicast)
                                              │
                                    ┌─────────▼───────────┐
                                    │   QGroundControl     │
                                    │   ┌───────────────┐  │
                                    │   │   DDSLink      │  │
                                    │   │  ┌──────────┐  │  │
                                    │   │  │CycloneDDS│  │  │
                                    │   │  │Subscriber│  │  │
                                    │   │  └────┬─────┘  │  │
                                    │   │       │        │  │
                                    │   │  ┌────▼─────┐  │  │
                                    │   │  │ Mapping  │  │  │
                                    │   │  │ Engine   │  │  │
                                    │   │  └────┬─────┘  │  │
                                    │   │       │        │  │
                                    │   │  ┌────▼─────┐  │  │
                                    │   │  │Transform │  │  │
                                    │   │  │Registry  │  │  │
                                    │   │  └────┬─────┘  │  │
                                    │   │       │        │  │
                                    │   │  ┌────▼─────┐  │  │
                                    │   │  │  Data    │  │  │
                                    │   │  │ Injector │  │  │
                                    │   │  └────┬─────┘  │  │
                                    │   └───────┼────────┘  │
                                    │           │           │
                                    │   ┌───────▼────────┐  │
                                    │   │  Fact System   │  │
                                    │   │ (Vehicle Model)│  │
                                    │   └───────┬────────┘  │
                                    │           │           │
                                    │   ┌───────▼────────┐  │
                                    │   │   QML UI       │  │
                                    │   │ (仪表盘/地图)   │  │
                                    │   └────────────────┘  │
                                    └───────────────────────┘
```

---

## 六、DDS 映射表机制

### 6.1 默认映射表位置

启用 DDS 后，QGC 按以下顺序搜索映射表文件：

1. `~/.config/QGroundControl/dds_mappings/<name>.json` — 用户自定义
2. `:/dds_mappings/<name>.json` — Qt 资源文件（编译内嵌）
3. `./resources/dds_mappings/<name>.json` — 源码目录

### 6.2 默认 PX4 映射表内容

`_default.json` 包含 20 个 PX4 DDS 话题的映射：

| PX4 DDS 话题 | QGC FactGroup | 关键字段 |
|-------------|---------------|---------|
| /fmu/out/vehicle_attitude | vehicle.attitude | roll, pitch, heading (四元数自动转换) |
| /fmu/out/vehicle_global_position | vehicle.gps | lat, lon, alt |
| /fmu/out/vehicle_local_position | vehicle.localPosition | x, y, z, vx, vy, vz, groundSpeed |
| /fmu/out/vehicle_gps_position | vehicle.gps | hdop, vdop, satellites, fixType |
| /fmu/out/battery_status | vehicle.battery | voltage, current, percentRemaining |
| /fmu/out/vehicle_status | vehicle | armedState, flightMode |
| /fmu/out/wind | vehicle.wind | direction, speed |
| /fmu/out/vehicle_land_detected | vehicle | landed, freefall |
| /fmu/out/home_position | vehicle.homePosition | lat, lon, alt |
| /fmu/out/airspeed_validated | vehicle | calibratedAirspeed, trueAirspeed |
| ... | ... | （共 70+ 字段映射） |

### 6.3 创建厂商定制映射表

```bash
# 复制模板
cp resources/dds_mappings/_vendor_template.json \
   ~/.config/QGroundControl/dds_mappings/my_vendor.json

# 编辑映射表
# 修改 vendor 字段、topic 名称、字段映射等
vim ~/.config/QGroundControl/dds_mappings/my_vendor.json
```

在 QGC DDS Link 设置中，**Vendor Mapping** 填入 `my_vendor` 即可加载。

---

## 七、常见问题

### Q1: 编译报错 "QGC_ENABLE_DDS is not defined"
**A**: 确保 CMake 配置时加了 `-DQGC_ENABLE_DDS=ON` 参数。

### Q2: 编译 QGC 不加 DDS 开关会受影响吗？
**A**: 不会。所有 DDS 代码都在 `#ifdef QGC_ENABLE_DDS` 条件编译块中，不加开关时完全不参与编译。

### Q3: P0 阶段为什么连接成功但看不到飞行器？
**A**: P0 的 CycloneDDS 调用是 stub，不会收到真实 DDS 数据。DDSLink 的 `_readSample()` 始终返回空，所以没有数据注入 Fact 系统。P1 阶段将替换为真实的 CycloneDDS API 调用。

### Q4: 如何查看 DDS 相关日志？
**A**: QGC 使用 Qt 日志分类系统。启动时添加环境变量：
```bash
export QT_LOGGING_RULES="Comms.DDSLink.debug=true"
./Debug/QGroundControl
```

### Q5: CycloneDDS 是否需要提前安装？
**A**: P0 阶段不需要。P0 使用 stub 函数不实际调用 CycloneDDS 库。P1 阶段需要安装 CycloneDDS 并链接。

### Q6: 多机场景怎么配置？
**A**: 每架飞行器创建一个 DDS Link，设置不同的 **Namespace Prefix**（如 `/drone1`、`/drone2`）。映射引擎会自动剥离命名空间前缀进行匹配。

---

## 八、文件变更清单

以下是相对于原始 QGC 源码的所有变更：

### 新增文件

| 文件路径 | 说明 |
|---------|------|
| `src/Comms/DDSLink/DDSConfiguration.h` | DDS 链接配置类（头文件） |
| `src/Comms/DDSLink/DDSConfiguration.cc` | DDS 链接配置类（实现） |
| `src/Comms/DDSLink/DDSLink.h` | DDS 链接主类（头文件） |
| `src/Comms/DDSLink/DDSLink.cc` | DDS 链接主类（实现） |
| `src/DDS/DDSMappingEngine.h` | 映射引擎（头文件） |
| `src/DDS/DDSMappingEngine.cc` | 映射引擎（实现） |
| `src/DDS/DDSTransformRegistry.h` | 转换注册表（头文件） |
| `src/DDS/DDSTransformRegistry.cc` | 转换注册表（实现） |
| `src/DDS/DDSDataInjector.h` | 数据注入器（头文件） |
| `src/DDS/DDSDataInjector.cc` | 数据注入器（实现） |
| `src/AppSettings/DDSSettings.qml` | DDS 设置 QML 界面 |
| `resources/dds_mappings/_default.json` | PX4 默认映射表 |
| `resources/dds_mappings/_vendor_template.json` | 厂商映射模板 |
| `Doc/day1-dds功能测试.md` | Day1 文档 |
| `Doc/编译QGC+DDS功能及SITL验证指南.md` | 本文档 |

### 修改的 QGC 原始文件

| 文件路径 | 修改内容 |
|---------|---------|
| `cmake/CustomOptions.cmake` | 新增 `QGC_ENABLE_DDS` 选项 |
| `src/Comms/LinkConfiguration.h` | 在 `LinkType` 枚举中新增 `TypeDDS`（`#ifdef` 保护） |
| `src/Comms/LinkConfiguration.cc` | 在 `createSettings()` 和 `duplicateSettings()` 中新增 DDS case |
| `src/Comms/LinkManager.cc` | 在 `createConnectedLink()` 和 `linkTypeStrings()` 中新增 DDS |
| `src/Comms/CMakeLists.txt` | 新增 DDS 源文件、头文件路径、资源文件的条件编译块 |
| `src/AppSettings/CMakeLists.txt` | 新增 `DDSSettings.qml` 到 QML 模块 |

---

## 九、后续计划

| 阶段 | 内容 | 预计时间 |
|------|------|---------|
| **P1** | 替换 CycloneDDS stub 为真实 API，连接 PX4 SITL 端到端 | 第 3-6 周 |
| **P2** | 用户登录 + 权限系统 | 第 7-8 周 |
| **P3** | DDS 参数管理 + 命令发送 | 第 9-12 周 |
| **P4** | 相机/云台/传感器扩展映射 | 第 13-14 周 |
| **P5** | 映射表热加载 + 社区共享 | 第 15-16 周 |
