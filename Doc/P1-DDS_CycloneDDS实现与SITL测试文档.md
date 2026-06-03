# P1阶段：CycloneDDS 真实实现与 PX4 SITL 测试文档

## 1. 本阶段工作概述

P1 阶段将 P0 的所有 stub（桩代码）替换为真实的 CycloneDDS API 调用，实现 QGC 与 PX4 之间通过 DDS 协议的完整数据链路。

| 序号 | 工作项 | 状态 |
|------|--------|------|
| 1 | PX4 消息类型定义（IDL + C 类型生成） | ✅ 完成 |
| 2 | DDSLink 真实 CycloneDDS API 替换 | ✅ 完成 |
| 3 | DDSDataInjector 真实 Fact 注入实现 | ✅ 完成 |
| 4 | CMakeLists.txt CycloneDDS 链接配置 | ✅ 完成 |

## 2. 代码修改详解

### 2.1 PX4 消息类型定义

**文件位置：** `src/DDS/idl/` 和 `src/DDS/generated/`

PX4 通过 XRCE-DDS 协议发布消息，CycloneDDS 需要对应的类型描述符才能正确反序列化。

**实现方式：**
1. 在 `src/DDS/idl/` 下编写 IDL（Interface Definition Language）文件，定义 PX4 消息结构
2. 使用 CycloneDDS 的 `idlc` 编译器生成 C 类型代码到 `src/DDS/generated/`

**已支持的 5 个核心消息类型：**

| IDL 文件 | DDS 类型名 | 用途 |
|----------|-----------|------|
| VehicleAttitude.idl | `px4_msgs::msg::dds_::VehicleAttitude_` | 姿态四元数 (q[0..3]) |
| VehicleGlobalPosition.idl | `px4_msgs::msg::dds_::VehicleGlobalPosition_` | 全局位置 (lat/lon/alt) |
| VehicleLocalPosition.idl | `px4_msgs::msg::dds_::VehicleLocalPosition_` | 本地位置 (x/y/z/vx/vy/vz) |
| BatteryStatus.idl | `px4_msgs::msg::dds_::BatteryStatus_` | 电池 (voltage/current/remaining) |
| VehicleStatus.idl | `px4_msgs::msg::dds_::VehicleStatus_` | 飞行状态 (arming/nav_state) |

**IDL 示例（VehicleAttitude.idl）：**
```idl
module px4_msgs {
module msg {
module dds_ {

struct VehicleAttitude_ {
    unsigned long long timestamp;
    unsigned long long timestamp_sample;
    float q[4];
    float delta_q_reset[4];
    octet quat_reset_counter;
};

}; // dds_
}; // msg
}; // px4_msgs
```

**生成命令（已预生成，无需手动运行）：**
```bash
idlc -l c -o src/DDS/generated src/DDS/idl/VehicleAttitude.idl
```

### 2.2 DDSLink 真实 CycloneDDS 实现

**文件：** `src/Comms/DDSLink/DDSLink.h` 和 `DDSLink.cc`

**P0 → P1 变更对照表：**

| 方法 | P0（stub） | P1（真实实现） |
|------|-----------|---------------|
| `_createParticipant()` | 返回固定值 1 | `dds_create_participant(domainId, nullptr, nullptr)` |
| `_destroyParticipant()` | 只打日志 | `dds_delete(participant)` |
| `_subscribeToTopics()` | 生成假 readerId | 逐 topic 创建 `dds_create_topic` + `dds_create_reader` |
| `_readSample()` | 返回空 QHash | `dds_take()` + 类型特定解析器 |
| `_runDiscovery()` | 返回空列表 | 暂使用映射表 topics（后续可接 builtin discovery） |

**关键实现细节：**

#### a) DDS Participant 创建
```cpp
dds_entity_t DDSLink::_createParticipant(int domainId)
{
    const dds_entity_t participant = dds_create_participant(
        static_cast<dds_domainid_t>(domainId), nullptr, nullptr);
    if (participant < 0) {
        qCWarning(DDSLinkLog) << "dds_create_participant failed:"
                              << dds_strretcode(-participant);
    }
    return participant;
}
```

#### b) Topic 订阅与 Reader 创建
```cpp
dds_entity_t DDSLink::_createTypedReader(dds_entity_t participant,
                                          const QString &topicName,
                                          const dds_topic_descriptor_t *desc)
{
    // 1. 创建 DDS Topic
    dds_entity_t topic = dds_create_topic(participant, desc,
                                           topicName.toUtf8().constData(),
                                           nullptr, nullptr);
    // 2. 设置 QoS（BEST_EFFORT + KEEP_LAST_1 — 遥测适用）
    dds_qos_t *qos = dds_create_qos();
    dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

    // 3. 创建 Reader
    dds_entity_t reader = dds_create_reader(participant, topic, qos, nullptr);
    dds_delete_qos(qos);
    return reader;
}
```

#### c) 样本读取与解析
```cpp
QHash<QString, QVariant> DDSLink::_readSample(dds_entity_t reader,
                                               const QString &topicName)
{
    void *samples[1] = { nullptr };
    dds_sample_info_t infos[1];

    dds_return_t rc = dds_take(reader, samples, infos, 1, 1);
    if (rc <= 0 || !infos[0].valid_data) {
        if (samples[0]) dds_return_loan(reader, samples, rc > 0 ? rc : 0);
        return {};
    }

    // 使用注册的类型特定解析器
    QHash<QString, QVariant> fields = parser(samples[0]);
    dds_return_loan(reader, samples, rc);
    return fields;
}
```

#### d) Topic 名称匹配
PX4 XRCE-DDS Agent 使用 `rt/` 前缀发布 ROS 2 风格的 topic：
- 映射表中：`/fmu/out/vehicle_attitude`
- DDS 网络上：`rt/fmu/out/vehicle_attitude`

代码自动添加可配置的命名空间前缀（默认 `rt`）：
```cpp
QString ddsTopicName = prefix.isEmpty()
    ? QStringLiteral("rt") + topicName
    : prefix + topicName;
```

### 2.3 DDSDataInjector 真实 Fact 注入

**文件：** `src/DDS/DDSDataInjector.h` 和 `DDSDataInjector.cc`

**P0 → P1 变更：**

P0 的 `_injectField()` 只是发射信号，不实际修改 Fact。P1 实现了完整的注入链：

```
DDS sample → QHash<QString,QVariant> → DDSMappingEngine 查表
→ DDSTransformRegistry 变换 → DDSDataInjector._injectField()
→ Vehicle.xxxFactGroup() → FactGroup.getFact(name) → Fact.setRawValue(value)
```

**FactGroup 解析实现：**

由于 Vehicle 没有通用的 `getFactGroup(name)` 方法，需要显式映射：

```cpp
FactGroup *DDSDataInjector::_resolveFactGroup(const QString &path) const
{
    if (path == "vehicle")       return _vehicle->vehicleFactGroup();
    if (path == "gps")           return _vehicle->gpsFactGroup();
    if (path == "localPosition") return _vehicle->localPositionFactGroup();
    if (path == "wind")          return _vehicle->windFactGroup();
    // ... 其他 FactGroup
}
```

### 2.4 CMakeLists.txt 修改

**文件：** `src/Comms/CMakeLists.txt`

新增内容：
```cmake
# Find CycloneDDS
find_package(CycloneDDS REQUIRED)

# 添加生成的 C 类型源文件
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleAttitude.c
    ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleGlobalPosition.c
    ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleLocalPosition.c
    ${CMAKE_SOURCE_DIR}/src/DDS/generated/BatteryStatus.c
    ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleStatus.c
)

# 链接 CycloneDDS
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CycloneDDS::ddsc)

# 抑制生成代码的编译警告
set_source_files_properties(... PROPERTIES COMPILE_FLAGS "-w")
```

## 3. 编译与测试指南

### 3.1 前提条件

确保已安装：
- CycloneDDS（v0.10.5 或更高版本）
- GCC 13（Qt 6.10+ 需要）
- Qt 6.10.x
- PX4-Autopilot 源码
- Gazebo（PX4 SITL 仿真用）

### 3.2 CycloneDDS 安装

QGC 的 `FindCycloneDDS.cmake` 支持三种查找策略（按优先级）：

1. **CMake Config**（`CycloneDDSConfig.cmake`）— 从源码编译安装时自动提供
2. **pkg-config**（`cyclonedds.pc`）— apt 安装或自定义编译可能提供
3. **手动搜索** — 在 `/usr/local/`, `/usr/`, `/opt/cyclonedds/`, ROS 路径下查找

**方式 A：从源码编译安装（推荐）**
```bash
git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
cd cyclonedds
git checkout 0.10.5
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_EXAMPLES=OFF
make -j$(nproc)
sudo make install
sudo ldconfig
```

安装后验证：
```bash
# 检查库文件
ls /usr/local/lib/libddsc.so
# 检查头文件
ls /usr/local/include/dds/dds.h
# 检查 CMake config（find_package CONFIG 模式使用）
ls /usr/local/lib/cmake/CycloneDDS/CycloneDDSConfig.cmake
```

**方式 B：如果 CycloneDDS 安装在自定义路径**
```bash
# 方式1：设置环境变量
export CYCLONEDDS_HOME=/你的安装路径

# 方式2：cmake 时指定
cmake .. -DCYCLONEDDS_ROOT=/你的安装路径 ...
```

### 3.3 编译 QGC

```bash
cd ~/QGC_DDS/qgc_dev
git checkout DDS_P1
git pull origin DDS_P1

# 清理旧的构建（如果之前编译失败）
rm -rf build

mkdir -p build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=$QT_DIR \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
    -DQGC_ENABLE_DDS=ON

ninja -j$(nproc)
```

**CMake 配置阶段预期输出（关于 CycloneDDS）：**
```
-- Found CycloneDDS via CMake config: 0.10.5
```
或：
```
-- CycloneDDS: /usr/local/lib/libddsc.so
-- CycloneDDS include: /usr/local/include
```

### 3.4 常见编译错误排查

#### 错误 1：`The dependency target "CycloneDDS::ddsc" does not exist`

**完整错误信息：**
```
CMake Error in CMakeLists.txt:
  The dependency target "CycloneDDS::ddsc" of target
  "QGroundControl_autogen_timestamp_deps" does not exist.
```

**原因分析（两层原因）：**

1. **CMake CONFIG 作用域问题（CMake 4.x）：** `find_package(CycloneDDS CONFIG)` 在 Find 模块内部调用时，CONFIG 创建的 IMPORTED target 在配置阶段存在，但在 CMake 的生成阶段丢失作用域。这是 CMake 4.x 版本中 Find 模块内嵌套 CONFIG 调用的已知行为。

2. **多版本 CycloneDDS 路径混用：** 如果系统同时安装了手动编译的 CycloneDDS（`/usr/local`）和 ROS 2 自带的 CycloneDDS（`/opt/ros/humble`），CONFIG 模式可能混用两个安装的头文件和库文件（例如头文件来自 `/opt/ros/humble/include`，库来自 `/usr/local/lib`），导致目标创建失败。

**修复方法：** 最新代码的 `FindCycloneDDS.cmake` 已完全移除 `find_package(CycloneDDS CONFIG)` 调用，改用**前缀匹配搜索**：按优先级遍历候选安装前缀（`/usr/local` → `/usr` → ROS 路径），要求头文件和库文件必须来自**同一前缀**，然后自建 GLOBAL IMPORTED target。

```bash
git pull origin DDS_P1
rm -rf build && mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=$QT_DIR \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
    -DQGC_ENABLE_DDS=ON
```

**如果系统存在多版本 CycloneDDS，** 用 `-DCYCLONEDDS_ROOT=` 指定要使用的版本：
```bash
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=$QT_DIR \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
    -DQGC_ENABLE_DDS=ON \
    -DCYCLONEDDS_ROOT=/usr/local
```

**CMake 输出应显示同一前缀：**
```
-- CycloneDDS found in prefix: /usr/local
-- CycloneDDS: /usr/local/lib/libddsc.so
-- CycloneDDS include: /usr/local/include
```

如果看到头文件和库来自不同路径（如 include 来自 `/opt/ros/humble`，lib 来自 `/usr/local`），说明匹配失败，请用 `-DCYCLONEDDS_ROOT=` 显式指定。

#### 错误 2：`Could NOT find CycloneDDS (missing: CycloneDDS_LIBRARY CycloneDDS_INCLUDE_DIR)`

**原因：** 所有搜索策略（前缀匹配、pkg-config、默认路径）都未找到 CycloneDDS。

**排查步骤：**
```bash
# 1. 检查 CycloneDDS 是否安装
find / -name "libddsc.so*" 2>/dev/null
find / -name "dds.h" -path "*/dds/*" 2>/dev/null

# 2. 检查 pkg-config
pkg-config --libs --cflags cyclonedds 2>/dev/null
```

如果以上都没找到，说明 CycloneDDS 未安装，请按 3.2 节指引安装。

如果找到了但路径不在标准位置，用 `-DCYCLONEDDS_ROOT=` 或 `CYCLONEDDS_HOME` 环境变量指定：
```bash
# 例如 CycloneDDS 安装在 /opt/cyclonedds
export CYCLONEDDS_HOME=/opt/cyclonedds
cmake .. -DCYCLONEDDS_ROOT=/opt/cyclonedds ...
```

#### 错误 3：`Cannot open input file ... .qml: No such file or directory`

**示例：**
```
Cannot open input file .../build/qml/QGroundControl/AppSettings/LoggingSettings.qml:No such file or directory
```

**原因：** 这是 Qt QML linter 的警告，不是错误。首次编译前 QML 文件尚未生成到 build 目录，属于 QGC 官方已知行为。**不影响编译和运行，可以忽略。**

#### 错误 4：VehicleComponent metatype / static assertion 错误

**示例：**
```
static assertion failed: Pointer Meta Types must either point to
fully-defined types or be declared with Q_DECLARE_OPAQUE_POINTER(T *)
```

**原因：** GCC 12 与 Qt 6.10+ 不兼容。Qt 6.10 的 constexpr MOC 生成器对类型完整性要求更严格。

**修复：** 升级到 GCC 13：
```bash
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install g++-13 gcc-13
```
然后在 cmake 时指定 `-DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13`。

### 3.5 PX4 SITL 端到端测试

#### 终端 1 — 启动 Micro XRCE-DDS Agent
```bash
MicroXRCEAgent udp4 -p 8888
```

#### 终端 2 — 启动 PX4 SITL 仿真
```bash
cd ~/PX4-Autopilot
make px4_sitl gz_x500
```

等待 PX4 启动完成，你会看到类似输出：
```
INFO  [uxrce_dds_client] synchronized with time offset ...
INFO  [uxrce_dds_client] successfully created topic ...
```

#### 终端 3 — 启动 QGC（带 DDS 调试日志）
```bash
cd ~/QGC_DDS/qgc_dev/build
QT_LOGGING_RULES="Comms.DDSLink.debug=true" ./QGroundControl
```

#### 终端 4 — 验证 DDS 数据流（可选）
```bash
# 使用 CycloneDDS 自带的 ddsperf 工具验证网络上有数据
# 或用 ros2 topic echo 验证（如果安装了 ROS 2）
ros2 topic echo /fmu/out/vehicle_attitude
```

### 3.6 验证步骤

| 步骤 | 验证内容 | 预期结果 |
|------|---------|---------|
| 1 | QGC 启动 | 无崩溃，日志显示 "DDSLink created" |
| 2 | 添加 DDS 链接 | Application Settings → Comm Links → Add → Type: DDS |
| 3 | 连接 | 日志显示 "Created DDS participant" 和 "Subscribed to N topics" |
| 4 | 数据接收 | 日志显示 ddsMessageReceived 信号触发 |
| 5 | 地图显示 | 飞机图标出现在地图上 |
| 6 | 遥测更新 | 姿态、GPS、电池数据实时更新 |
| 7 | 断开连接 | 日志显示 "DDS link disconnected"，无崩溃 |

### 3.7 DDS 调试日志说明

开启 DDS 调试日志：
```bash
export QT_LOGGING_RULES="Comms.DDSLink.debug=true"
```

**日志输出级别：**

| 级别 | 说明 | 示例 |
|------|------|------|
| `qCDebug` | 详细调试信息 | `DDSLink created`、`No type descriptor for topic: ...` |
| `qCInfo` | 正常运行信息 | `Created DDS participant on domain 0`、`Subscribed to 5 of 20 topics` |
| `qCWarning` | 警告/错误 | `dds_create_participant failed: ...`、`Failed to load mapping` |

**预期正常运行日志序列：**
```
[DDSLink] DDSLink created
[DDSLink] Loaded mapping: _default topics: 20 fields: 45
[DDSLink] Created DDS participant on domain 0 entity: 1234
[DDSLink] Subscribed to rt/fmu/out/vehicle_attitude reader: 5678
[DDSLink] Subscribed to rt/fmu/out/vehicle_global_position reader: 5679
[DDSLink] Subscribed to rt/fmu/out/vehicle_local_position reader: 5680
[DDSLink] Subscribed to rt/fmu/out/battery_status reader: 5681
[DDSLink] Subscribed to rt/fmu/out/vehicle_status reader: 5682
[DDSLink] Subscribed to 5 of 20 topics
[DDSLink] DDS link connected on domain 0
```

**注意：** 映射表中有 20 个 topic，但目前只有 5 个有对应的类型描述符，其余 15 个会被跳过（显示 `No type descriptor for topic: ...`）。这是预期行为，后续版本会补充更多消息类型。

## 4. 架构图

```
PX4 Autopilot (SITL)
    │
    │ XRCE-DDS (UDP:8888)
    ▼
Micro XRCE-DDS Agent
    │
    │ Standard DDS (CycloneDDS)
    ▼
┌─────────────────────────────────────────────┐
│ QGroundControl                               │
│                                              │
│  DDSLink (src/Comms/DDSLink/DDSLink.cc)     │
│    ├─ dds_create_participant()               │
│    ├─ dds_create_topic() × N                │
│    ├─ dds_create_reader() × N               │
│    └─ _onPollTimer() [10ms interval]        │
│         │                                    │
│         │ dds_take() → C struct              │
│         │ _parseXxx(sample) → QHash          │
│         │                                    │
│         ▼                                    │
│    DDSMappingEngine                          │
│    (resources/dds_mappings/_default.json)    │
│         │                                    │
│         ▼                                    │
│    DDSTransformRegistry                      │
│    (quaternion_to_euler, radians_to_degrees) │
│         │                                    │
│         ▼                                    │
│    DDSDataInjector                           │
│    _resolveFactGroup(path)                   │
│    FactGroup::getFact(name)                  │
│    Fact::setRawValue(value)                  │
│         │                                    │
│         ▼                                    │
│    QGC UI (地图/遥测/仪表盘)                  │
└─────────────────────────────────────────────┘
```

## 5. 文件清单

### 新增文件

| 文件路径 | 说明 |
|---------|------|
| `src/DDS/idl/VehicleAttitude.idl` | 姿态消息 IDL 定义 |
| `src/DDS/idl/VehicleGlobalPosition.idl` | 全局位置消息 IDL 定义 |
| `src/DDS/idl/VehicleLocalPosition.idl` | 本地位置消息 IDL 定义 |
| `src/DDS/idl/BatteryStatus.idl` | 电池状态消息 IDL 定义 |
| `src/DDS/idl/VehicleStatus.idl` | 飞行状态消息 IDL 定义 |
| `src/DDS/generated/*.c` | idlc 生成的 C 序列化/反序列化代码 |
| `src/DDS/generated/*.h` | idlc 生成的 C 类型头文件 |

### 修改文件

| 文件路径 | 修改内容 |
|---------|---------|
| `src/Comms/DDSLink/DDSLink.h` | 使用 `dds_entity_t` 类型，添加解析器函数声明 |
| `src/Comms/DDSLink/DDSLink.cc` | 全部 5 个 stub 替换为真实 CycloneDDS 调用 |
| `src/DDS/DDSDataInjector.h` | 添加 `_resolveFactGroup` 声明 |
| `src/DDS/DDSDataInjector.cc` | 实现真实 Fact 注入 |
| `src/Comms/CMakeLists.txt` | find_package + link CycloneDDS + 生成源文件 |

## 6. 已知限制与后续计划

### 当前限制
1. **仅支持 5 个消息类型** — 映射表中有 20 个 topic，但目前只有 5 个有 C 类型支持
2. **无 DDS Discovery** — 使用映射表中的 topic 列表，不依赖 DDS builtin discovery
3. **轮询模式** — 10ms 定时器轮询，非事件驱动（CycloneDDS waitset 更高效）
4. **无 DDS 写入** — 仅支持订阅读取，不支持向 PX4 发送命令

### P2 计划
1. 补充更多消息类型（SensorGps, Wind, HomePosition 等）
2. 实现 DDS waitset 事件驱动模式
3. 实现 DDS 写入（向 PX4 发送 offboard 控制等命令）
4. 实现 DDS builtin topic discovery
5. 添加自动重连机制
