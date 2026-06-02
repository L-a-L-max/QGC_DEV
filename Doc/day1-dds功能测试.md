# Day1 - DDS 功能测试指南

## 一、项目概述

本项目在 QGC 地面站中新增原生 DDS 通信层，通过本地 JSON 映射表适配不同 PX4 厂商的 DDS 话题。本文档描述 Day1 交付物的编译、测试和验证全流程。

### Day1 交付物清单

| 文件 | 说明 |
|------|------|
| `src/Comms/DDSLink/DDSConfiguration.h/cc` | DDS 链接配置类（Domain ID、厂商映射、命名空间） |
| `src/Comms/DDSLink/DDSLink.h/cc` | DDS 链接主类（连接/断开/轮询/注入） |
| `src/DDS/DDSMappingEngine.h/cc` | 映射引擎：JSON 映射表 → O(1) 查找 |
| `src/DDS/DDSTransformRegistry.h/cc` | 转换器注册表（四元数→欧拉角、单位转换等） |
| `src/DDS/DDSDataInjector.h/cc` | 数据注入器：DDS 消息 → Fact::setRawValue() |
| `resources/dds_mappings/_default.json` | 标准 PX4 默认映射表（20 个 topic，70+ 字段） |
| `resources/dds_mappings/_vendor_template.json` | 厂商映射表模板 |
| `cmake/FindCycloneDDS.cmake` | CycloneDDS 查找模块 |
| `cmake/QGC_DDS.cmake` | QGC DDS CMake 集成模块 |
| `test/DDS/test_mapping_engine.cc` | 独立单元测试（不依赖完整 QGC） |
| `test/DDS/CMakeLists.txt` | 测试构建脚本 |

---

## 二、开发环境配置

### 2.1 系统要求

| 组件 | 最低版本 | 推荐版本 |
|------|----------|----------|
| Ubuntu | 22.04 LTS | 24.04 LTS |
| GCC | 12 | 13+ |
| CMake | 3.25 | 3.28+ |
| Qt | 6.10.0 | 6.10.3+ |
| CycloneDDS | 0.10.0 | 最新 release |

### 2.2 安装编译工具链

```bash
# 基础工具
sudo apt update
sudo apt install -y build-essential cmake git ninja-build pkg-config

# GCC 12+（Ubuntu 22.04 默认 GCC 11，需升级）
sudo apt install -y gcc-12 g++-12
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100
```

### 2.3 安装 Qt 6.10+

```bash
# 方法一：Qt Online Installer（推荐）
# 从 https://www.qt.io/download-qt-installer-oss 下载安装器
# 选择 Qt 6.10.x → Desktop gcc_64 组件

# 方法二：从源码编译（适用于 CI 环境）
# 参考 QGC 官方文档：https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/getting_started/

# 验证
qmake6 --version   # 或 ~/Qt/6.10.3/gcc_64/bin/qmake --version
```

### 2.4 安装 CycloneDDS

```bash
# 从源码编译安装
cd /tmp
git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
cd cyclonedds
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local \
         -DBUILD_EXAMPLES=OFF \
         -DBUILD_TESTING=OFF \
         -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install

# 验证安装
ls /usr/local/include/dds/dds.h       # 头文件
ls /usr/local/lib/libddsc.*           # 库文件
```

### 2.5 安装 PX4 SITL（用于端到端测试）

```bash
# 克隆 PX4
git clone https://github.com/PX4/PX4-Autopilot.git --recursive
cd PX4-Autopilot

# 安装依赖
bash ./Tools/setup/ubuntu.sh

# 编译 SITL
make px4_sitl gz_x500

# 安装 Micro XRCE-DDS Agent
sudo snap install micro-xrce-dds-agent --edge
# 或从源码编译:
# git clone https://github.com/eProsima/Micro-XRCE-DDS-Agent.git
# cd Micro-XRCE-DDS-Agent && mkdir build && cd build
# cmake .. && make -j$(nproc) && sudo make install
```

---

## 三、编译

### 3.1 独立测试编译（不需要完整 QGC）

这是验证 Day1 交付物最快的方式，只需要 Qt6 Core 模块：

```bash
cd qgc_dev/test/DDS
mkdir build && cd build

# 设置 Qt 路径（根据你的安装位置调整）
export CMAKE_PREFIX_PATH=~/Qt/6.10.3/gcc_64

# 配置
cmake .. -DQGC_ENABLE_DDS=ON

# 编译
make -j$(nproc)

# 运行测试
./test_dds_mapping
```

**预期输出**：
```
=== QGC DDS Mapping Engine Test Suite ===

=== MappingEngine Tests ===
PASS: Load mapping from JSON
PASS: Vendor name correct
PASS: Topic count = 2
PASS: Attitude topic found
PASS: Attitude has 3 field mappings
PASS: First field maps to roll
PASS: Roll uses quaternion transform
PASS: Battery topic found
PASS: Battery remaining scale = 100
PASS: Non-existing topic returns nullptr
PASS: Namespace-prefixed topic resolved via stripping
PASS: allTopicNames returns 2

=== TransformRegistry Tests ===
PASS: Built-in transforms registered
PASS: Has quaternion_to_euler_roll
PASS: Has rad_to_deg
PASS: Has negate
PASS: Has identity
PASS: Identity quaternion → roll = 0
PASS: Identity quaternion → pitch = 0
PASS: Identity quaternion → yaw = 0
PASS: 45-degree yaw quaternion → heading ≈ 45
PASS: π radians → 180 degrees
PASS: negate(5.5) = -5.5
PASS: ground_speed(3,4) = 5

=== DataInjector Tests ===
PASS: 1 message processed
PASS: 3 facts updated (roll, pitch, heading)
PASS: Injected roll ≈ 0
PASS: Injected pitch ≈ 0
PASS: 2 messages processed
PASS: Battery voltage = 22.4V
PASS: Battery remaining = 75% (0.75 × 100)
PASS: 1 unmapped topic skipped

=== Results ===
Passed: 28
Failed: 0
```

### 3.2 集成到 QGC 完整编译

在 QGC 的 `CMakeLists.txt` 顶层添加：

```cmake
# ===== DDS Support (optional) =====
option(QGC_ENABLE_DDS "Enable native DDS communication support" OFF)
if(QGC_ENABLE_DDS)
    include(cmake/QGC_DDS.cmake)
    target_sources(${PROJECT_NAME} PRIVATE ${QGC_DDS_SOURCES})
    target_link_libraries(${PROJECT_NAME} PRIVATE CycloneDDS::ddsc)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QGC_ENABLE_DDS)
endif()
```

然后编译：

```bash
cd qgc_dev
mkdir build && cd build
cmake .. -DQGC_ENABLE_DDS=ON \
         -DCMAKE_PREFIX_PATH=~/Qt/6.10.3/gcc_64 \
         -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### 3.3 不启用 DDS 编译（验证零侵入）

```bash
cmake .. -DQGC_ENABLE_DDS=OFF
make -j$(nproc)
```

所有 DDS 代码在 `#ifdef QGC_ENABLE_DDS` 保护下，`OFF` 时完全不参与编译。

---

## 四、测试

### 4.1 单元测试

```bash
cd test/DDS/build
ctest --output-on-failure
```

测试覆盖：

| 测试项 | 验证内容 |
|--------|----------|
| MappingEngine JSON 加载 | 从 JSON 对象加载映射表 |
| Topic 查找 | O(1) hash 查找，存在/不存在 |
| 命名空间剥离 | `/drone1/fmu/out/xxx` → `/fmu/out/xxx` |
| 字段映射解析 | dds_field → fact_name + transform + scale |
| 四元数→欧拉角 | 单位四元数、45° yaw 旋转 |
| 弧度→角度 | π → 180° |
| 取反转换 | NED z → 正上方 |
| 地速计算 | sqrt(3²+4²) = 5 |
| 数据注入流程 | DDS 消息 → 映射 → 转换 → 输出 Fact 值 |
| 缩放因子 | remaining 0.75 × 100 = 75% |
| 未映射话题处理 | 未知 topic 被跳过并计数 |

### 4.2 端到端测试（P1 阶段，需要 CycloneDDS + PX4 SITL）

#### 步骤 1：启动 PX4 SITL

```bash
# 终端 1：启动 PX4 SITL
cd PX4-Autopilot
make px4_sitl gz_x500
```

#### 步骤 2：启动 XRCE-DDS Agent

```bash
# 终端 2：启动 DDS Agent
micro-xrce-dds-agent udp4 -p 8888
```

#### 步骤 3：验证 DDS 话题可用

```bash
# 终端 3：检查 DDS 话题（需要安装 ros2 工具或 CycloneDDS 工具）
# 如果安装了 ROS2:
ros2 topic list | grep fmu

# 预期输出:
# /fmu/out/vehicle_attitude
# /fmu/out/vehicle_global_position
# /fmu/out/battery_status
# ... 等 25+ 个话题
```

#### 步骤 4：启动 QGC（带 DDS）

```bash
# 编译后运行
./build/QGroundControl
```

在 QGC 中：
1. 设置 → 通信链路 → 添加 → 选择 "DDS Link"
2. Domain ID: 0
3. 映射表: _default（或留空使用默认）
4. 点击连接

#### 步骤 5：验证数据显示

检查项：
- [ ] HUD 显示 roll/pitch/heading 数值（来自 DDS vehicle_attitude）
- [ ] 地图显示飞机位置（来自 DDS vehicle_global_position）
- [ ] 电池电量显示（来自 DDS battery_status）
- [ ] GPS 状态显示（来自 DDS vehicle_gps_position）
- [ ] 日志输出 `[DDSLink] Connected. Domain: 0 Topics: N`
- [ ] 日志输出 `[DDSDataInjector] vehicle/roll = xxx`

---

## 五、验证清单

### P0 验收标准（Day1 交付）

| # | 验证项 | 验证方法 | 预期结果 |
|---|--------|----------|----------|
| 1 | MappingEngine 加载 _default.json | 单元测试 | 20 topics, 70+ fields |
| 2 | 四元数转欧拉角数学正确 | 单元测试 (identity + 45° yaw) | roll=0, yaw=45 |
| 3 | 缩放因子正确应用 | 单元测试 (battery remaining) | 0.75 → 75% |
| 4 | 命名空间剥离工作 | 单元测试 | /drone1/fmu/... 正确映射 |
| 5 | 未映射话题不崩溃 | 单元测试 | 返回 nullptr，计数+1 |
| 6 | DDSLink 类结构完整 | 代码审查 | 可编译，接口完整 |
| 7 | DDSConfiguration 可序列化 | 代码审查 | load/save settings 实现 |
| 8 | CMake 集成正确 | `cmake -DQGC_ENABLE_DDS=ON` | 编译通过 |
| 9 | QGC_ENABLE_DDS=OFF 零影响 | `cmake -DQGC_ENABLE_DDS=OFF` | 正常编译 |
| 10 | 厂商映射表模板可用 | 检查 _vendor_template.json | 结构完整 |

### P1 验收标准（后续交付）

| # | 验证项 | 验证方法 | 预期结果 |
|---|--------|----------|----------|
| 1 | CycloneDDS 编译通过 | cmake + make | 无编译错误 |
| 2 | DDSLink 连接 PX4 SITL | SITL + Agent + QGC | 日志显示 Connected |
| 3 | 收到 vehicle_attitude | 日志检查 | q=[w,x,y,z] 数据流 |
| 4 | HUD 显示 DDS 数据 | UI 验证 | roll/pitch/heading 更新 |
| 5 | 电池通过 DDS 显示 | UI 验证 | 电压/百分比正确 |
| 6 | MAVLink + DDS 共存 | 同时连接两种链路 | 无冲突 |
| 7 | 数据延迟 < 10ms | QElapsedTimer 测量 | 局域网环境 |

---

## 六、架构说明

### 数据流

```
PX4 飞控
  │ uORB topics
  ▼
XRCE-DDS Agent (UDP:8888)
  │ DDS/RTPS (UDP multicast or unicast)
  ▼
DDSLink._onPollTimer()
  │ CycloneDDS dds_take() 读取样本
  │ 解析字段 → QHash<QString, QVariant>
  ▼
emit ddsMessageReceived(topicName, fields, timestamp)
  │
  ▼
DDSDataInjector.onDDSMessage()
  │ 1. MappingEngine.topicMapping(topicName) → DDSTopicMapping
  │ 2. For each field:
  │    a. TransformRegistry.transform(name) → 转换函数
  │    b. 执行转换 (e.g., quaternion → euler)
  │    c. 应用 scale/offset
  │    d. Fact::setRawValue(value)
  ▼
QGC UI 通过 Qt Property Binding 自动更新
  (HUD, 地图, 仪表盘等)
```

### JSON 映射表结构

```json
{
  "vendor": "px4_standard",     // 厂商标识
  "version": "1.0.0",          // 映射表版本
  "topics": [
    {
      "dds_topic": "/fmu/out/vehicle_attitude",    // DDS 话题名
      "dds_type": "px4_msgs::msg::VehicleAttitude", // DDS 类型（参考用）
      "fact_group": "vehicle",                      // 默认 FactGroup
      "fields": [
        {
          "dds_field": "q[0]",                      // DDS 消息中的字段名
          "fact_name": "roll",                      // QGC Fact 名称
          "transform": "quaternion_to_euler_roll",  // 转换函数（可选）
          "scale": 1.0,                             // 缩放因子（可选，默认 1.0）
          "offset": 0.0                             // 偏移量（可选，默认 0.0）
        }
      ]
    }
  ]
}
```

### 如何添加厂商映射

1. 复制 `resources/dds_mappings/_vendor_template.json` 为 `your_vendor.json`
2. 修改 `vendor` 字段为你的厂商名
3. 修改 `dds_topic` 为厂商实际使用的话题名
4. 修改 `dds_field` 为厂商实际使用的字段名
5. 如需特殊转换，在 `DDSTransformRegistry` 中注册新的转换函数
6. 将文件放入 `~/.config/QGroundControl/dds_mappings/` 或 `resources/dds_mappings/`
7. 在 DDSConfiguration 中选择该映射文件

---

## 七、已知限制与后续计划

### Day1 已知限制

1. **DDSLink 中的 CycloneDDS 调用为 stub**：`_createParticipant()`, `_subscribeToTopics()`, `_readSample()` 等函数目前是占位实现，标记了 `TODO(P1)` 注释。P1 阶段将替换为真实的 CycloneDDS API 调用。

2. **DDSDataInjector._injectField() 未连接 Fact 系统**：目前通过 signal `factUpdated` 输出值，P1 集成到 QGC 时将替换为 `Fact::setRawValue()` 调用。

3. **未包含 QML UI**：DDSSettings.qml（DDS 链接配置界面）将在 P1 集成阶段添加。

4. **默认映射表未覆盖全部 PX4 DDS 话题**：当前覆盖 20/25 个 publication topics，其余 5 个（register_ext_component_reply, collision_constraints, position_setpoint_triplet, timesync_status, message_format_response）为系统级话题，不直接映射到 QGC UI。

### 后续计划

| 阶段 | 目标 | 预计时间 |
|------|------|----------|
| P1 | 替换 CycloneDDS stub → 真实 DDS 通信 → QGC UI 显示 | 3-4 周 |
| P2 | 登录界面 + 权限系统 | 2 周 |
| P3 | DDS 参数与命令协议 | 4 周 |
| P4 | DDS 任务与地理围栏 | 4 周 |
| P5 | DDS 文件传输与日志 | 3 周 |

---

## 八、常见问题

### Q: CycloneDDS 和 Fast-DDS 如何选择？

推荐 CycloneDDS：
- 更轻量（纯 C API，可选 C++ binding）
- 与 C++20 兼容性更好
- PX4 官方 Micro XRCE-DDS Agent 基于它
- Eclipse 基金会维护，许可证友好（EPL-2.0）

### Q: 不安装 CycloneDDS 能运行单元测试吗？

可以！Day1 的单元测试只需要 Qt6 Core，不依赖 CycloneDDS。DDSLink 中的 DDS 调用都是 stub，单元测试直接测试 MappingEngine / TransformRegistry / DataInjector 的纯逻辑。

### Q: 映射表放在哪里？

搜索顺序：
1. `~/.config/QGroundControl/dds_mappings/` — 用户自定义（优先级最高）
2. `:/dds_mappings/` — Qt 资源文件（编译内嵌）
3. `./resources/dds_mappings/` — 可执行文件同级目录

### Q: 如何验证四元数转换是否正确？

单元测试覆盖了两个关键场景：
- 单位四元数 (1,0,0,0) → roll=0, pitch=0, yaw=0
- 45° yaw 旋转 (cos(π/8), 0, 0, sin(π/8)) → yaw≈45°

可以与 QGC 现有的 `VehicleFactGroup::_handleAttitudeQuaternion()` 实现对比验证。
