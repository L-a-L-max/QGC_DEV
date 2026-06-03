# P2 — DDS Typed Readers 与数据流实现

## 1. 开发目标

P2 阶段的核心目标是 **打通 PX4 DDS 数据到 QGC UI 的完整数据通路**。

P1 建立了 DDS 网络层（CycloneDDS participant）和数据处理管道（映射引擎 + 变换注册器 + 注入器），但缺少实际的数据读取能力。P2 实现了以下关键能力：

1. **IDL 类型生成**：将 PX4 消息定义（px4_msgs .msg）转换为 CycloneDDS 可用的 C 类型和描述符
2. **Typed Reader 创建**：使用 IDL 生成的类型描述符创建真正的 DDS 数据读取器
3. **数据读取与提取**：从 DDS 网络接收 CDR 序列化数据，反序列化为 C 结构体，提取字段
4. **DDS-Only Vehicle 创建**：在纯 DDS 模式下（无 MAVLink），从 DDS 数据创建 QGC Vehicle 对象
5. **CMake 自动化**：`idlc` 编译器集成到构建系统，IDL 生成自动化

## 2. 开发内容

### 2.1 IDL 类型生成管道

**文件位置**: `src/DDS/idl/*.idl`（20 个 IDL 文件）

将 PX4 的 20 个核心话题消息定义从 `.msg` 格式手动转换为 CycloneDDS IDL 格式：

| PX4 消息 | IDL 文件 | 生成的 C 结构体 |
|----------|---------|----------------|
| VehicleAttitude.msg | VehicleAttitude.idl | `px4_msgs_msg_VehicleAttitude` |
| VehicleGlobalPosition.msg | VehicleGlobalPosition.idl | `px4_msgs_msg_VehicleGlobalPosition` |
| VehicleLocalPosition.msg | VehicleLocalPosition.idl | `px4_msgs_msg_VehicleLocalPosition` |
| SensorGps.msg | SensorGps.idl | `px4_msgs_msg_SensorGps` |
| BatteryStatus.msg | BatteryStatus.idl | `px4_msgs_msg_BatteryStatus` |
| VehicleStatus.msg | VehicleStatus.idl | `px4_msgs_msg_VehicleStatus` |
| Wind.msg | Wind.idl | `px4_msgs_msg_Wind` |
| VehicleLandDetected.msg | VehicleLandDetected.idl | `px4_msgs_msg_VehicleLandDetected` |
| HomePosition.msg | HomePosition.idl | `px4_msgs_msg_HomePosition` |
| AirspeedValidated.msg | AirspeedValidated.idl | `px4_msgs_msg_AirspeedValidated` |
| VehicleOdometry.msg | VehicleOdometry.idl | `px4_msgs_msg_VehicleOdometry` |
| EstimatorStatusFlags.msg | EstimatorStatusFlags.idl | `px4_msgs_msg_EstimatorStatusFlags` |
| FailsafeFlags.msg | FailsafeFlags.idl | `px4_msgs_msg_FailsafeFlags` |
| VehicleControlMode.msg | VehicleControlMode.idl | `px4_msgs_msg_VehicleControlMode` |
| VehicleCommandAck.msg | VehicleCommandAck.idl | `px4_msgs_msg_VehicleCommandAck` |
| GimbalDeviceAttitudeStatus.msg | GimbalDeviceAttitudeStatus.idl | `px4_msgs_msg_GimbalDeviceAttitudeStatus` |
| SensorCombined.msg | SensorCombined.idl | `px4_msgs_msg_SensorCombined` |
| ManualControlSetpoint.msg | ManualControlSetpoint.idl | `px4_msgs_msg_ManualControlSetpoint` |
| VtolVehicleStatus.msg | VtolVehicleStatus.idl | `px4_msgs_msg_VtolVehicleStatus` |
| TransponderReport.msg | TransponderReport.idl | `px4_msgs_msg_TransponderReport` |

**类型映射规则**:
- `uint64` → `unsigned long long`
- `float64` / `double` → `double`
- `float32` → `float`
- `uint32` → `unsigned long`
- `uint16` → `unsigned short`
- `uint8` / `int8` → `octet`
- `bool` → `boolean`
- `char[N]` → `char field[N]`
- `float32[N]` → `float field[N]`
- 常量（`= value`）不出现在 IDL 中（仅编译期使用）

### 2.2 DDSTypeRegistry — 类型注册表

**文件**: `src/DDS/DDSTypeRegistry.h`, `src/DDS/DDSTypeRegistry.cc`

将 JSON 映射表中的类型名字符串（如 `"px4_msgs::msg::VehicleAttitude"`）映射到两个东西：
1. `dds_topic_descriptor_t*` — CycloneDDS 创建 topic/reader 所需的类型描述符
2. `DDSFieldExtractorFunc` — 从 C 结构体提取字段到 `QHash<QString, QVariant>` 的函数

每个消息类型都有一个对应的字段提取器函数，负责将 IDL 生成的 C 结构体字段转换为 QGC 映射引擎可识别的键值对。

### 2.3 DDSLink 更新 — 真正的数据读取

**`_subscribeToTopics()` 实现**:
```
对于映射表中的每个话题:
  1. 从 DDSMappingEngine 获取话题的 dds_type 名
  2. 在 DDSTypeRegistry 中查找对应的类型描述符
  3. 调用 dds_create_topic(participant, descriptor, topicName)
  4. 调用 dds_create_reader(participant, topic, QoS, listener)
  5. 存储 reader 和 extractor 到 ReaderInfo
```

**`_readSample()` 实现**:
```
  1. 调用 dds_take(reader, &sample, &info, 1, 1) 获取最新数据
  2. 检查 info.valid_data
  3. 调用注册的 extractor 函数提取字段
  4. 返回 QHash<QString, QVariant>
  5. 归还 loan (dds_return_loan)
```

**数据流：**
```
PX4 SITL → MicroXRCE-DDS Agent → CycloneDDS Network
    → dds_take() 在 DDSLink::_onPollTimer() 中
    → DDSFieldExtractor (C struct → QHash)
    → emit ddsMessageReceived()
    → DDSDataInjector::onDDSMessage()
    → DDSTransformRegistry (四元数→欧拉角 等)
    → Fact::setRawValue()
    → QGC UI 自动更新
```

### 2.4 DDSVehicleManager — 纯 DDS Vehicle 创建

**文件**: `src/DDS/DDSVehicleManager.h`, `src/DDS/DDSVehicleManager.cc`

在纯 DDS 模式（无 MAVLink）下，QGC 没有 MAVLink heartbeat 来触发 Vehicle 创建。DDSVehicleManager 通过监听 `vehicle_status` 话题解决这个问题：

1. 监听 DDSLink 的 `ddsMessageReceived` 信号
2. 等待 `/fmu/out/vehicle_status` 话题的第一条数据
3. 从 `vehicle_type` 字段推断 MAV_TYPE（1→旋翼，2→固定翼，3→车辆 等）
4. 通过 `MAVLinkProtocol::vehicleHeartbeatInfo` 信号触发标准 Vehicle 创建流程
5. 延迟 100ms 后获取新创建的 Vehicle，调用 `DDSDataInjector::setVehicle()` 挂载

这样复用了 QGC 完整的 Vehicle 初始化流程（FactGroups、FirmwarePlugin、参数系统等）。

### 2.5 CMake 集成

**修改文件**: `src/Comms/CMakeLists.txt`

构建流程：
1. `find_program(IDLC_EXECUTABLE idlc REQUIRED)` — 查找 idlc 编译器
2. `add_custom_command` — 对每个 `.idl` 文件运行 `idlc -l c` 生成 `.h/.c`
3. `add_library(dds_idl_types STATIC ...)` — 将生成的 C 文件编译为独立静态库
4. 主目标链接 `dds_idl_types` 和 `CycloneDDS::ddsc`

**关键设计**：生成的 C 文件编译为独立静态库而非加入主目标，避免 C/C++ 预编译头冲突。

## 3. 预期结果

### 3.1 编译输出

```
-- Found idlc: /usr/local/bin/idlc
-- CycloneDDS found in prefix: /usr/local
idlc: Generating VehicleAttitude.h/.c from VehicleAttitude.idl
idlc: Generating BatteryStatus.h/.c from BatteryStatus.idl
... (20 个话题)
[100%] Built target QGroundControl
```

### 3.2 运行日志

启动 QGC 并连接 DDS：
```
[DDSLink] DDSLink created
[DDSTypeRegistry] Registered 20 IDL types
[DDSLink] Loaded mapping: _default topics: 20 fields: 66
[DDSLink] Created DDS participant on domain 0 entity: XXX
[DDSLink] Subscribed: 20 typed readers, 0 stubs, of 20 topics
[DDSLink] DDS link connected on domain 0
```

收到 PX4 数据后：
```
[DDSVehicleManager] Creating DDS vehicle: id= 1 type= 1 MAV_TYPE= 2
[DDSVehicleManager] Attached DDSDataInjector to vehicle 1
```

### 3.3 UI 现象

| 功能 | 预期表现 | 数据来源 |
|------|---------|---------|
| 姿态仪表 | Roll/Pitch/Heading 实时更新 | vehicle_attitude → 四元数变换 |
| 地图位置 | 飞行器图标移动 | vehicle_global_position → lat/lon |
| 电池信息 | 电压/电流/剩余量显示 | battery_status |
| 地速 | 速度数值更新 | vehicle_local_position → vx/vy → ground_speed |
| GPS 信息 | 卫星数/HDOP 显示 | sensor_gps (vehicle_gps_position) |
| 飞行模式 | 模式文字显示 | vehicle_status → nav_state |
| 解锁状态 | 已解锁/已锁定指示 | vehicle_status → arming_state |

### 3.4 数据刷新频率

- **姿态数据**：约 50-100 Hz（PX4 默认发布频率）
- **位置数据**：约 5-10 Hz
- **电池数据**：约 1 Hz
- **GPS 数据**：约 5 Hz

## 4. 文件清单

### 新增文件
| 文件 | 说明 |
|------|------|
| `src/DDS/idl/*.idl` | 20 个 IDL 类型定义文件 |
| `src/DDS/DDSTypeRegistry.h/.cc` | 类型注册表（类型名→描述符+提取器） |
| `src/DDS/DDSVehicleManager.h/.cc` | DDS-only Vehicle 管理器 |
| `Doc/P2-DDS_TypedReaders与数据流实现.md` | 本文档 |
| `Doc/P2-测试手册.md` | P2 测试手册 |

### 修改文件
| 文件 | 修改内容 |
|------|---------|
| `src/Comms/DDSLink/DDSLink.h` | 添加 DDSTypeRegistry/DDSVehicleManager 成员 |
| `src/Comms/DDSLink/DDSLink.cc` | 实现 typed reader 创建和数据读取 |
| `src/DDS/DDSMappingEngine.h` | DDSTopicMapping 添加 ddsTypeName 字段 |
| `src/DDS/DDSMappingEngine.cc` | 解析 JSON 中的 dds_type 字段 |
| `src/Comms/CMakeLists.txt` | 添加 idlc 集成和新源文件 |

## 5. 架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                         PX4 SITL (电脑 B)                       │
│  uORB → MicroXRCE-DDS Agent → CycloneDDS Publisher             │
└──────────────────────────┬──────────────────────────────────────┘
                           │ DDS Network (UDP multicast)
┌──────────────────────────▼──────────────────────────────────────┐
│                    QGroundControl (电脑 A)                       │
│                                                                  │
│  ┌──────────────────────────────────────────────────────┐       │
│  │ DDSLink                                               │       │
│  │  ├─ CycloneDDS Participant (domain 0)                 │       │
│  │  ├─ DDSTypeRegistry (20 types registered)             │       │
│  │  ├─ 20 × dds_create_reader (typed readers)  ◀── NEW  │       │
│  │  ├─ Poll Timer (10ms) → dds_take()           ◀── NEW  │       │
│  │  └─ emit ddsMessageReceived(topic, fields)            │       │
│  └────────────────────┬─────────────────────────────────┘       │
│                       │                                          │
│  ┌────────────────────▼─────────────────────────────────┐       │
│  │ DDSVehicleManager  ◀── NEW                            │       │
│  │  └─ vehicle_status → Create Vehicle via heartbeat     │       │
│  └────────────────────┬─────────────────────────────────┘       │
│                       │                                          │
│  ┌────────────────────▼─────────────────────────────────┐       │
│  │ DDSDataInjector (P1 已完成)                           │       │
│  │  ├─ DDSMappingEngine (topic→FactGroup 映射)           │       │
│  │  ├─ DDSTransformRegistry (12 变换)                    │       │
│  │  └─ Fact::setRawValue() → UI 自动更新                 │       │
│  └──────────────────────────────────────────────────────┘       │
└──────────────────────────────────────────────────────────────────┘
```
