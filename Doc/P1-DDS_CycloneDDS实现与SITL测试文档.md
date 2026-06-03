# P1阶段：CycloneDDS 集成与 PX4 SITL 测试文档

## 1. 本阶段工作概述

P1 阶段将 QGC 与 CycloneDDS 库链接，建立 DDS 通信基础设施。通过 JSON 映射表驱动的动态话题配置，实现 QGC 对任意 PX4 DDS 话题的支持。

| 序号 | 工作项 | 状态 |
|------|--------|------|
| 1 | CycloneDDS 库查找与链接（FindCycloneDDS.cmake） | ✅ 完成 |
| 2 | DDSLink 继承 LinkInterface 集成到 QGC 链路管理 | ✅ 完成 |
| 3 | DDSMappingEngine JSON 话题映射引擎 | ✅ 完成 |
| 4 | DDSDataInjector 真实 Fact 注入实现 | ✅ 完成 |
| 5 | DDSTransformRegistry 数据变换注册器 | ✅ 完成 |
| 6 | DDSConfiguration 链路配置（domain/namespace/vendor） | ✅ 完成 |
| 7 | DDS participant 生命周期管理 | ✅ 完成 |
| 8 | CMake C/C++ PCH 冲突修复 | ✅ 完成 |
| 9 | PX4 版本后缀话题名兼容（_v1/_v2） | ✅ 完成 |
| 10 | 话题 typed reader（需 IDL 类型生成） | 🔲 P2 |

## 2. 架构说明

### 2.1 核心组件

```
DDSConfiguration           # 链路配置（domain ID, vendor mapping, namespace prefix）
    ↓
DDSLink : LinkInterface    # DDS 通信链路（CycloneDDS participant + reader）
    ↓ ddsMessageReceived signal
DDSDataInjector            # 将 DDS 字段注入 QGC Fact 系统
    ├─ DDSMappingEngine    # JSON 映射表加载与查找
    └─ DDSTransformRegistry # 数据变换（四元数→欧拉角、NED→正上等）
```

### 2.2 数据流

1. `DDSLink::_connect()` → 创建 CycloneDDS participant → 加载映射表 → 注册话题
2. `DDSLink::_onPollTimer()` → 轮询读取 DDS 样本 → 发射 `ddsMessageReceived` 信号
3. `DDSDataInjector::onDDSMessage()` → 查映射表 → 应用变换 → 调用 `Fact::setRawValue()`

### 2.3 文件结构

```
src/Comms/DDSLink/
├── DDSLink.h / .cc           # DDS 通信链路（继承 LinkInterface）
└── DDSConfiguration.h / .cc  # 链路配置（继承 LinkConfiguration）

src/DDS/
├── DDSMappingEngine.h / .cc      # JSON 话题映射引擎
├── DDSTransformRegistry.h / .cc  # 数据变换注册器
└── DDSDataInjector.h / .cc       # Fact 注入器

cmake/
├── FindCycloneDDS.cmake   # CycloneDDS 查找模块（前缀匹配，避免 ROS 冲突）
└── QGC_DDS.cmake          # DDS 模块 CMake 配置

resources/dds_mappings/
├── _default.json           # 默认 PX4 话题映射（20 个话题）
└── _vendor_template.json   # 自定义厂商映射模板
```

## 3. 话题配置系统

### 3.1 配置文件位置

**主配置文件：** `resources/dds_mappings/_default.json`

加载优先级（`DDSMappingEngine::_resolveFilePath()`）：

| 优先级 | 路径 | 说明 |
|--------|------|------|
| 1（最高） | `~/.config/QGroundControl/dds_mappings/<name>.json` | 用户自定义覆盖 |
| 2 | `:/dds_mappings/<name>.json` | Qt 资源（编译时嵌入） |
| 3 | `./resources/dds_mappings/<name>.json` | 可执行文件旁 |

### 3.2 JSON 映射格式

```json
{
  "vendor": "px4_standard",
  "version": "1.0.0",
  "topics": [
    {
      "dds_topic": "/fmu/out/vehicle_attitude",
      "dds_type": "px4_msgs::msg::VehicleAttitude",
      "fact_group": "vehicle",
      "description": "Vehicle attitude quaternion → roll/pitch/heading",
      "fields": [
        {
          "dds_field": "q[0]",
          "fact_name": "roll",
          "transform": "quaternion_to_euler_roll",
          "description": "四元数 → 横滚角（度）"
        },
        {
          "dds_field": "voltage_v",
          "fact_name": "voltage",
          "scale": 1.0,
          "offset": 0.0,
          "description": "电池电压（伏特）"
        }
      ]
    }
  ]
}
```

**字段说明：**

| 字段 | 必填 | 说明 |
|------|------|------|
| `dds_topic` | ✅ | DDS 话题名（如 `/fmu/out/vehicle_attitude`） |
| `dds_type` | 否 | DDS 类型名（供参考） |
| `fact_group` | 否 | 默认 FactGroup 名（可在 field 级别覆盖） |
| `fields[].dds_field` | ✅ | DDS 样本中的字段名 |
| `fields[].fact_name` | ✅ | QGC Fact 名 |
| `fields[].fact_group` | 否 | 覆盖话题级别的 fact_group |
| `fields[].transform` | 否 | 变换函数名 |
| `fields[].scale` | 否 | 线性缩放（默认 1.0） |
| `fields[].offset` | 否 | 线性偏移（默认 0.0） |

### 3.3 支持的变换

| 变换名 | 输入 | 输出 | 说明 |
|--------|------|------|------|
| `quaternion_to_euler_roll` | q[0..3] | 度 | 四元数 → 横滚角 |
| `quaternion_to_euler_pitch` | q[0..3] | 度 | 四元数 → 俯仰角 |
| `quaternion_to_euler_yaw` | q[0..3] | 度 | 四元数 → 航向角 [0, 360) |
| `negate` | 数值 | 数值 | 取反（NED z → 正上） |
| `ground_speed_from_vxy` | vx, vy | m/s | sqrt(vx² + vy²) |
| `course_over_ground_from_vxy` | vx, vy | 度 | atan2(vy, vx) → [0, 360) |
| `rad_to_deg` | 弧度 | 度 | × 180/π |

### 3.4 话题名称兼容性

代码对话题名称**完全通用**，不硬编码任何话题名：

1. **映射表驱动**：所有话题名从 JSON 文件运行时加载
2. **命名空间前缀剥离**：`/ns/fmu/out/xxx` 自动匹配 `/fmu/out/xxx`
3. **PX4 版本后缀兼容**：`/fmu/out/battery_status_v1` 自动匹配 `/fmu/out/battery_status`
4. **Vendor Overlay**：可创建自定义厂商映射覆盖默认配置

### 3.5 当前已映射话题（20个）

| DDS 话题 | FactGroup | 关键字段 |
|----------|-----------|----------|
| `/fmu/out/vehicle_attitude` | vehicle | roll, pitch, heading |
| `/fmu/out/vehicle_global_position` | gps, vehicle | lat, lon, altitudeAMSL |
| `/fmu/out/vehicle_local_position` | localPosition, vehicle | x, y, z, groundSpeed, climbRate |
| `/fmu/out/vehicle_gps_position` | gps | lat, lon, alt, hdop, satellites_used |
| `/fmu/out/battery_status` | battery | voltage, current, percentRemaining |
| `/fmu/out/vehicle_status` | vehicle | armed, flightMode, vehicleType |
| `/fmu/out/wind` | wind | direction, speed |
| `/fmu/out/vehicle_land_detected` | vehicle | landed, freefall |
| `/fmu/out/home_position` | homePosition | lat, lon, alt |
| `/fmu/out/airspeed_validated` | vehicle | airSpeed, airSpeedTrue |
| `/fmu/out/vehicle_odometry` | vehicle | rollRate, pitchRate, yawRate |
| `/fmu/out/estimator_status_flags` | estimatorStatus | goodPosEstimate, gpsGlitch |
| `/fmu/out/failsafe_flags` | vehicle | rcRSSI, gpsInvalid |
| `/fmu/out/vehicle_control_mode` | vehicle | armed, manualControl, autoMode |
| `/fmu/out/vehicle_command_ack` | _system | _commandId, _commandResult |
| `/fmu/out/gimbal_device_attitude_status` | gimbal | gimbalRoll, gimbalPitch, gimbalYaw |
| `/fmu/out/sensor_combined` | vehicle | xacc, yacc, zacc, rollRate |
| `/fmu/out/manual_control_setpoint` | vehicle | throttlePct |
| `/fmu/out/vtol_vehicle_status` | vehicle | vtolState |
| `/fmu/out/transponder_report` | remoteID | lat, lon, alt |

## 4. 编译指南

### 4.1 前置条件

- Qt 6.10+ (含 QtQmlIntegration)
- GCC 13+
- CMake 4.0+, Ninja
- CycloneDDS 0.10+ (安装在 `/usr/local/`，**不要**使用 ROS2 自带版本)

### 4.2 编译步骤

```bash
cd ~/QGC_DDS/qgc_dev
git pull origin DDS_P1

rm -rf build && mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=$QT_DIR \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
    -DQGC_ENABLE_DDS=ON

ninja -j$(nproc)
```

### 4.3 CMake 输出验证

正确输出应包含：
```
-- CycloneDDS found in prefix: /usr/local
-- CycloneDDS: /usr/local/lib/libddsc.so
-- CycloneDDS include: /usr/local/include
```

### 4.4 已修复的编译问题

| 问题 | 原因 | 修复 |
|------|------|------|
| `CycloneDDS::ddsc does not exist` | CMake 4.x CONFIG 导入目标作用域不持久 | FindCycloneDDS.cmake 使用前缀匹配手动搜索 |
| `unknown type name 'namespace'` | .c 文件触发 C PCH 编译 C++ 头文件 | 移除 .c 文件，仅保留 C++ 源文件 |
| `VehicleAttitude.h: 没有那个文件` | DDSLink.cc 引用了未生成的 IDL 类型头 | 移除 IDL 类型依赖 |
| 头文件来自 ROS/库来自 /usr/local | 系统存在多个 CycloneDDS 版本 | FindCycloneDDS 要求头文件和库在同一前缀下 |

## 5. 如何添加新话题

### 5.1 编辑 _default.json

在 `resources/dds_mappings/_default.json` 的 `topics` 数组中添加新条目：

```json
{
  "dds_topic": "/fmu/out/sensor_accel",
  "dds_type": "px4_msgs::msg::SensorAccel",
  "fact_group": "vehicle",
  "description": "加速度计原始数据",
  "fields": [
    {
      "dds_field": "x",
      "fact_name": "xacc",
      "description": "X轴加速度 (m/s²)"
    }
  ]
}
```

### 5.2 用户自定义（不修改源码）

创建 `~/.config/QGroundControl/dds_mappings/_default.json`，该文件会**优先于**编译内嵌的资源加载。

### 5.3 Vendor Overlay（厂商覆盖）

1. 复制 `resources/dds_mappings/_vendor_template.json` 为新文件
2. 修改话题映射
3. 在 DDSConfiguration 中设置 vendorMapping 指向新文件名

## 6. 下一步（P2）

| 工作项 | 说明 |
|--------|------|
| IDL 类型生成 | 从 PX4 msg 定义生成 CycloneDDS 类型描述符 |
| Typed Reader | 使用类型描述符创建 DDS reader 读取真实数据 |
| SITL 端到端测试 | 连接 PX4 SITL + micro-XRCE-DDS Agent |
| 命令发送 | 通过 `/fmu/in/*` 话题发送控制命令 |
