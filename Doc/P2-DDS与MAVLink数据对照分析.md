# P2 阶段 DDS 与 MAVLink 数据获取对照分析

## 概述

本文档详细对比了 QGC 在 **MAVLink 传统模式** 和 **DDS P2 模式** 下获取的飞行数据，分析当前 DDS 实现的覆盖范围、缺失项以及两种模式的本质差异。

---

## 1. 数据获取架构对比

### MAVLink 模式

```
PX4 → MAVLink 消息 → Serial/UDP → QGC MAVLinkProtocol
   → Vehicle::_mavlinkMessageReceived()
   → switch(msgid) { ... }
   → FactGroup::handleMessage() → Fact::setRawValue()
   → QML UI 绑定自动更新
```

**特点**：
- 双向通信（上行命令 + 下行遥测）
- 消息协议由 MAVLink XML 严格定义
- 支持参数读写、任务上传/下载、固件升级
- 有完善的 ACK/重传/超时机制

### DDS P2 模式

```
PX4 → uORB → MicroXRCE-DDS Agent → CycloneDDS → UDP Multicast
   → QGC DDSLink::_onPollTimer()
   → dds_take() → TypeExtractor → QHash<QString, QVariant>
   → DDSDataInjector::onDDSMessage()
   → MappingEngine 查找映射 → Fact::setRawValue()
   → QML UI 绑定自动更新
```

**特点**：
- 当前为单向接收（只读遥测，不发送命令）
- 直接订阅 PX4 内部 uORB 话题（透过 DDS 中间件）
- 数据更新频率由 PX4 发布频率决定（通常 50-250Hz）
- 无 ACK 机制，BEST_EFFORT QoS

---

## 2. FactGroup 数据对照表

### 2.1 VehicleFactGroup（核心飞行数据）

| Fact 名称 | UI 显示位置 | MAVLink 来源 | DDS P2 来源 | DDS 状态 |
|-----------|------------|-------------|------------|---------|
| `roll` | 姿态仪表 (HUD) | ATTITUDE / ATTITUDE_QUATERNION | vehicle_attitude (四元数→欧拉) | ✅ 已实现 |
| `pitch` | 姿态仪表 (HUD) | ATTITUDE / ATTITUDE_QUATERNION | vehicle_attitude (四元数→欧拉) | ✅ 已实现 |
| `heading` | 指南针 / HUD | ATTITUDE / ATTITUDE_QUATERNION | vehicle_attitude (四元数→欧拉) | ✅ 已实现 |
| `rollRate` | 高级遥测 | ATTITUDE (rollspeed) | sensor_combined (gyro_rad[0]) | ✅ 已实现 |
| `pitchRate` | 高级遥测 | ATTITUDE (pitchspeed) | sensor_combined (gyro_rad[1]) | ✅ 已实现 |
| `yawRate` | 高级遥测 | ATTITUDE (yawspeed) | sensor_combined (gyro_rad[2]) | ✅ 已实现 |
| `groundSpeed` | 工具栏速度条 | VFR_HUD (groundspeed) | vehicle_local_position (sqrt(vx²+vy²)) | ✅ 已实现 |
| `airSpeed` | 工具栏速度条 | VFR_HUD (airspeed) | airspeed_validated (calibrated_airspeed_m_s) | ✅ 已实现 |
| `airSpeedSetpoint` | HUD 速度指标 | VFR_HUD 或 NAV_CONTROLLER_OUTPUT | — | ❌ 未实现 |
| `climbRate` | 工具栏爬升率 | VFR_HUD (climb) | vehicle_local_position (vz, 取反) | ✅ 已实现 |
| `altitudeRelative` | 工具栏高度 | GLOBAL_POSITION_INT (relative_alt) | vehicle_local_position (z, 取反) | ✅ 已实现 |
| `altitudeAMSL` | 工具栏高度 | GLOBAL_POSITION_INT (alt) | vehicle_global_position (alt) | ✅ 已实现 |
| `altitudeAboveTerr` | 地形高度 | ALTITUDE (altitude_terrain) | — | ❌ 未实现 |
| `altitudeTuning` | PID 调参视图 | ALTITUDE (altitude_local) | — | ❌ 未实现 |
| `altitudeTuningSetpoint` | PID 调参视图 | ALTITUDE (altitude_sp) | — | ❌ 未实现 |
| `xTrackError` | NAV 偏航 | NAV_CONTROLLER_OUTPUT (xtrack_error) | — | ❌ 未实现 |
| `rangeFinderDist` | 测距仪 | RANGEFINDER (distance) | — | ❌ 未实现 |
| `flightDistance` | 飞行统计 | QGC 内部计算 | QGC 内部计算（基于坐标） | ✅ 自动工作 |
| `distanceToHome` | 工具栏 | QGC 内部计算 | QGC 内部计算（基于坐标+Home） | ✅ 已实现 |
| `timeToHome` | 工具栏 | QGC 内部计算 | QGC 内部计算 | ✅ 自动工作 |
| `missionItemIndex` | 任务进度 | MISSION_CURRENT (seq) | — | ❌ 未实现 |
| `headingToNextWP` | 任务导航 | QGC 内部计算 | — | ❌ 需要任务数据 |
| `distanceToNextWP` | 任务导航 | QGC 内部计算 | — | ❌ 需要任务数据 |
| `headingToHome` | Home 导航 | QGC 内部计算 | QGC 内部计算 | ✅ 自动工作 |
| `headingFromHome` | Home 导航 | QGC 内部计算 | QGC 内部计算 | ✅ 自动工作 |
| `headingFromGCS` | GCS 导航 | QGC 内部计算 | QGC 内部计算 | ✅ 自动工作 |
| `distanceToGCS` | GCS 导航 | QGC 内部计算 | QGC 内部计算 | ✅ 自动工作 |
| `hobbs` | 飞行时长 | QGC 内部计时 | QGC 内部计时 | ✅ 自动工作 |
| `throttlePct` | 油门百分比 | VFR_HUD (throttle) | manual_control_setpoint (throttle×100) | ✅ 已实现 |
| `imuTemp` | IMU 温度 | RAW_IMU (temperature) | — | ❌ 未实现 |
| `rcRSSI` | RC 信号强度 | RC_CHANNELS (rssi) | — | ❌ 未实现 |

### 2.2 VehicleGPSFactGroup（GPS 数据）

| Fact 名称 | UI 显示位置 | MAVLink 来源 | DDS P2 来源 | DDS 状态 |
|-----------|------------|-------------|------------|---------|
| `lat` | GPS 面板 | GPS_RAW_INT (lat×1e-7) | vehicle_global_position (lat) | ✅ 已实现 |
| `lon` | GPS 面板 | GPS_RAW_INT (lon×1e-7) | vehicle_global_position (lon) | ✅ 已实现 |
| `mgrs` | GPS 面板 | QGC 计算（从 lat/lon） | — | ❌ 未实现 |
| `hdop` | 工具栏 GPS 指示器 | GPS_RAW_INT (eph/100) | vehicle_gps_position (hdop) | ✅ 已实现 |
| `vdop` | GPS 面板 | GPS_RAW_INT (epv/100) | vehicle_gps_position (vdop) | ✅ 已实现 |
| `courseOverGround` | GPS 面板 | GPS_RAW_INT (cog/100) | vehicle_gps_position (cog_rad→deg) | ✅ 已实现 |
| `yaw` | GPS 面板 | GPS_RAW_INT (yaw/100) | — | ❌ 未提取 |
| `count` | 工具栏 GPS 指示器 | GPS_RAW_INT (satellites_visible) | vehicle_gps_position (satellites_used) | ✅ 已实现 |
| `lock` | GPS 面板 | GPS_RAW_INT (fix_type) | vehicle_gps_position (fix_type) | ✅ 已实现 |
| `systemErrors` | GPS 面板 | GNSS_INTEGRITY (system_errors) | — | ❌ 未实现 |
| `spoofingState` | GPS 面板 | GNSS_INTEGRITY (spoofing_state) | — | ❌ 未实现 |
| `jammingState` | GPS 面板 | GNSS_INTEGRITY (jamming_state) | — | ❌ 未实现 |
| `authenticationState` | GPS 面板 | GNSS_INTEGRITY | — | ❌ 未实现 |

### 2.3 BatteryFactGroup（电池数据）

| Fact 名称 | UI 显示位置 | MAVLink 来源 | DDS P2 来源 | DDS 状态 |
|-----------|------------|-------------|------------|---------|
| `voltage` | 工具栏电池图标 | BATTERY_STATUS (voltages[0]/1000) | battery_status (voltage_v) | ✅ 已实现 |
| `current` | 电池面板 | BATTERY_STATUS (current_battery/100) | battery_status (current_a) | ✅ 已实现 |
| `percentRemaining` | 工具栏电池百分比 | BATTERY_STATUS (battery_remaining) | battery_status (remaining×100) | ✅ 已实现 |
| `temperature` | 电池面板 | BATTERY_STATUS (temperature/100) | battery_status (temperature) | ✅ 已实现 |
| `mahConsumed` | 电池面板 | BATTERY_STATUS (current_consumed) | battery_status (discharged_mah) | ✅ 已实现 |
| `chargeState` | 电池面板 | BATTERY_STATUS (charge_state) | — | ❌ 未实现 |
| `timeRemaining` | 电池面板 | BATTERY_STATUS (time_remaining) | — | ❌ 未实现 |
| `cellCount` | 电池面板 | QGC 从电压推算 | — | ❌ 未实现 |
| `instantPower` | 电池面板 | QGC 计算(V×I) | QGC 计算(V×I) | ✅ 自动工作 |

### 2.4 WindFactGroup（风速数据）

| Fact 名称 | UI 显示位置 | MAVLink 来源 | DDS P2 来源 | DDS 状态 |
|-----------|------------|-------------|------------|---------|
| `direction` | 风速指示器 | WIND_COV / WIND | wind (windspeed_north/east→方向) | ✅ 已实现 |
| `speed` | 风速指示器 | WIND_COV / WIND | wind (sqrt(N²+E²)) | ✅ 已实现 |
| `verticalSpeed` | 风速指示器 | WIND_COV (wind_z) | — | ❌ 未实现 |

### 2.5 LocalPositionFactGroup（本地位置）

| Fact 名称 | UI 显示位置 | MAVLink 来源 | DDS P2 来源 | DDS 状态 |
|-----------|------------|-------------|------------|---------|
| `x` | 高级遥测 | LOCAL_POSITION_NED | vehicle_local_position (x) | ✅ 已实现 |
| `y` | 高级遥测 | LOCAL_POSITION_NED | vehicle_local_position (y) | ✅ 已实现 |
| `z` | 高级遥测 | LOCAL_POSITION_NED | vehicle_local_position (z, 取反) | ✅ 已实现 |
| `vx` | 高级遥测 | LOCAL_POSITION_NED | vehicle_local_position (vx) | ✅ 已实现 |
| `vy` | 高级遥测 | LOCAL_POSITION_NED | vehicle_local_position (vy) | ✅ 已实现 |
| `vz` | 高级遥测 | LOCAL_POSITION_NED | vehicle_local_position (vz, 取反) | ✅ 已实现 |

### 2.6 EstimatorStatusFactGroup（EKF 状态）

| Fact 名称 | UI 显示位置 | MAVLink 来源 | DDS P2 来源 | DDS 状态 |
|-----------|------------|-------------|------------|---------|
| `goodHorizPosAbsEstimate` | 飞前检查 | ESTIMATOR_STATUS (flags) | estimator_status_flags | ✅ 已实现 |
| `goodVertPosAbsEstimate` | 飞前检查 | ESTIMATOR_STATUS (flags) | estimator_status_flags | ✅ 已实现 |
| `gpsGlitch` | 飞前检查 | ESTIMATOR_STATUS (flags) | estimator_status_flags | ✅ 已实现 |
| 其他 24 个标志位 | 高级遥测 | ESTIMATOR_STATUS (flags) | — | ❌ 未实现 |

### 2.7 其他 FactGroup（MAVLink 有，DDS 未实现）

| FactGroup | 内容 | MAVLink 来源 | DDS 状态 |
|-----------|------|-------------|---------|
| **VibrationFactGroup** | 振动水平 (x/y/z) + clip 计数 | VIBRATION | ❌ 未实现 |
| **TemperatureFactGroup** | 气压计温度 (1/2/3) | SCALED_PRESSURE 1/2/3 | ❌ 未实现 |
| **DistanceSensorFactGroup** | 下方/前方/侧方测距 | DISTANCE_SENSOR | ❌ 未实现 |
| **RadioStatusFactGroup** | 遥测链路 RSSI/噪声 | RADIO_STATUS | ❌ 不适用（DDS 无此概念） |
| **VehicleSetpointFactGroup** | 期望姿态角 | ATTITUDE_TARGET | ❌ 未实现 |
| **VehicleGPS2FactGroup** | 第二 GPS 模块 | GPS2_RAW | ❌ 未实现 |
| **EscStatusFactGroup** | ESC 转速/电流/温度 | ESC_STATUS | ❌ 未实现 |
| **VehicleGeneratorFactGroup** | 发电机状态 | GENERATOR_STATUS | ❌ 不常用 |
| **VehicleHygrometerFactGroup** | 湿度计 | HYGROMETER_SENSOR | ❌ 不常用 |
| **VehicleRPMFactGroup** | RPM 传感器 | RAW_RPM | ❌ 不常用 |
| **VehicleEFIFactGroup** | 电子燃油喷射 | EFI_STATUS | ❌ 不常用 |

---

## 3. Vehicle 直接属性对照

这些属性不通过 FactGroup 机制，而是由 Vehicle 类直接管理：

| 属性 | UI 影响 | MAVLink 来源 | DDS P2 处理 | DDS 状态 |
|------|---------|-------------|------------|---------|
| `coordinate` | 地图飞机位置 | GLOBAL_POSITION_INT | DDSDataInjector 从 vehicle_global_position 设置 | ✅ 已实现 |
| `homePosition` | Home 点标记 + distanceToHome | HOME_POSITION | DDSDataInjector 从 home_position 设置 | ✅ 已实现 |
| `armed` | 工具栏状态 | HEARTBEAT (base_mode) | DDSDataInjector 从 vehicle_status (arming_state) 设置 | ✅ 已实现 |
| `flightMode` | 工具栏模式选择器 | HEARTBEAT (custom_mode) | DDSVehicleManager 合成心跳（nav_state→custom_mode） | ✅ 已实现 |
| `flying` | 工具栏 "Flying" 状态 | EXTENDED_SYS_STATE (landed_state) | DDSDataInjector 从 vehicle_land_detected 设置 | ✅ 已实现 |
| `landing` | 着陆状态 | EXTENDED_SYS_STATE | DDSDataInjector 从 vehicle_land_detected 设置 | ✅ 已实现 |
| `sensorsPresentBits` | 飞前检查（传感器健康） | SYS_STATUS | — | ❌ 未实现 |
| `sensorsHealthBits` | 飞前检查 | SYS_STATUS | — | ❌ 未实现 |
| `readyToFly` | "Ready to Fly" 状态 | SYS_STATUS (onboard_control_sensors_health) | — | ❌ 未实现 |
| `allSensorsHealthy` | 传感器健康指示 | SYS_STATUS | — | ❌ 未实现 |
| `rcRSSI` | RC 信号强度 | RC_CHANNELS (rssi) | — | ❌ 未实现 |
| `trajectoryPoints` | 地图轨迹线 | QGC 从 coordinate 变化记录 | 自动工作 | ✅ 已实现 |

---

## 4. MAVLink 独有功能（DDS 当前不支持）

### 4.1 双向命令控制

| 功能 | MAVLink 消息 | DDS 替代方案 |
|------|------------|-------------|
| 解锁/上锁 | COMMAND_LONG (ARM/DISARM) | 需实现 DDS 命令发布 |
| 模式切换 | SET_MODE | 需实现 DDS 命令发布 |
| 起飞/降落 | COMMAND_LONG (TAKEOFF/LAND) | 需实现 DDS 命令发布 |
| 参数读写 | PARAM_REQUEST_LIST / PARAM_SET | 需实现 DDS 参数服务 |
| 任务上传/下载 | MISSION_ITEM_INT / MISSION_REQUEST_LIST | 需实现 DDS 任务服务 |
| 校准 | COMMAND_LONG (各种校准命令) | 需实现 DDS 命令发布 |
| 固件升级 | SERIAL_CONTROL + 文件传输 | 不在 DDS 范围内 |
| 日志下载 | LOG_REQUEST_LIST / LOG_REQUEST_DATA | 不在 DDS 范围内 |

### 4.2 协议层功能

| 功能 | MAVLink 机制 | DDS P2 处理 |
|------|------------|------------|
| 心跳保活 | HEARTBEAT (1Hz 双向) | DDSVehicleManager 合成心跳（1Hz） |
| 消息请求 | REQUEST_DATA_STREAM / SET_MESSAGE_INTERVAL | 不需要（PX4 自动发布） |
| ACK 确认 | COMMAND_ACK | 未实现 |
| 超时重传 | MAVLink 协议层 | 不需要（DDS QoS 处理） |
| 多车辆管理 | sysid/compid 区分 | 通过 vehicle_status 中的 system_id |

---

## 5. DDS P2 数据统计总结

### 已实现数据项

| 类别 | 已实现 / 总计 | 覆盖率 |
|------|-------------|--------|
| VehicleFactGroup | 15 / 28 | 54% |
| GPSFactGroup | 6 / 13 | 46% |
| BatteryFactGroup | 5 / 8 | 63% |
| WindFactGroup | 2 / 3 | 67% |
| LocalPositionFactGroup | 6 / 6 | 100% |
| EstimatorStatusFactGroup | 3 / 27 | 11% |
| Vehicle 直接属性 | 7 / 12 | 58% |
| **总计** | **44 / 97** | **45%** |

### 对用户可见的关键差异

| 在 DDS 模式下能看到 | 在 DDS 模式下看不到 |
|-------------------|-------------------|
| 飞机在地图上的实时位置 | 传感器健康状态（飞前检查列表） |
| 姿态仪表（roll/pitch/heading） | 任务进度（当前航点序号） |
| 地速 / 空速 / 爬升率 | RC 信号强度 |
| 高度（相对 / 海拔） | 遥测链路质量 |
| GPS 卫星数 / HDOP / 定位类型 | IMU 温度 |
| 电池电压 / 电流 / 百分比 | 振动水平 |
| 飞行模式 | 测距仪数据 |
| 解锁 / 飞行 / 降落状态 | ESC 转速/温度 |
| 风速 / 风向 | 第二 GPS 数据 |
| Home 点 + 距离 Home | 地形高度 |
| 轨迹线 | 参数读写 |
| | 任务上传/下载 |
| | 解锁/模式切换等命令 |

---

## 6. DDS 话题订阅清单

当前 DDS P2 订阅的 PX4 话题（共 13 个）：

| # | DDS 话题名 | PX4 uORB 话题 | 类型 | 用途 |
|---|-----------|-------------|------|------|
| 1 | rt/fmu/out/vehicle_attitude | vehicle_attitude | VehicleAttitude | 姿态四元数 |
| 2 | rt/fmu/out/vehicle_global_position | vehicle_global_position | VehicleGlobalPosition | 全球位置+地图 |
| 3 | rt/fmu/out/vehicle_local_position_v1 | vehicle_local_position | VehicleLocalPosition | 本地位置+速度+高度 |
| 4 | rt/fmu/out/vehicle_gps_position | vehicle_gps_position | SensorGps | GPS 原始数据 |
| 5 | rt/fmu/out/battery_status_v1 | battery_status | BatteryStatus | 电池状态 |
| 6 | rt/fmu/out/wind | wind | Wind | 风速估计 |
| 7 | rt/fmu/out/airspeed_validated_v1 | airspeed_validated | AirspeedValidated | 校准空速 |
| 8 | rt/fmu/out/sensor_combined | sensor_combined | SensorCombined | 陀螺仪角速率 |
| 9 | rt/fmu/out/manual_control_setpoint | manual_control_setpoint | ManualControlSetpoint | 油门输入 |
| 10 | rt/fmu/out/estimator_status_flags | estimator_status_flags | EstimatorStatusFlags | EKF 标志位 |
| 11 | rt/fmu/out/vehicle_status_v1 | vehicle_status | VehicleStatus | 解锁/模式/类型 |
| 12 | rt/fmu/out/home_position_v1 | home_position | HomePosition | Home 坐标 |
| 13 | rt/fmu/out/vehicle_land_detected | vehicle_land_detected | VehicleLandDetected | 着陆检测 |

---

## 7. 版本绑定与兼容性

### 当前 PX4 版本

- **PX4**: v1.17.0-alpha1-232-g1345b3500a
- **IDL 适配**: 手动编写，已针对上述版本校验

### IDL 与 PX4 的绑定关系

DDS CDR 序列化是**位置敏感**的。每个 `.idl` 文件中的结构体字段必须与 PX4 对应 `.msg` 文件的字段**完全一致**（名称、类型、顺序、数量）。

| 如果 PX4 升级后... | 影响 | 解决方案 |
|-------------------|------|---------|
| 新增了字段（末尾） | 反序列化失败（字节数不匹配） | 更新 IDL，添加新字段 |
| 删除了字段 | 反序列化失败 | 更新 IDL，删除对应字段 |
| 改变了字段顺序 | 数据错乱（但不报错！） | 更新 IDL，调整顺序 |
| 改变了字段类型 | 数据错乱或崩溃 | 更新 IDL + 更新 extractor |

### 已知适配记录

| 话题 | 问题 | 修复 | Commit |
|------|------|------|--------|
| SensorGps | IDL 有 40 字段，PX4 v1.17-alpha 只有 37 字段（无 antenna_offset_x/y/z） | 删除 3 个多余字段 | `9c533729e` |

### 如何验证版本兼容性

```bash
# 1. 查看 PX4 版本
cd ~/PX4-Autopilot && git describe --tags

# 2. 查看任意消息定义
cat ~/PX4-Autopilot/msg/<MsgName>.msg

# 3. 编译后检查是否有反序列化错误
./QGroundControl 2>&1 | grep "deserialization.*failed"
```

---

## 8. 后续扩展建议（P3+）

### 高优先级（直接影响用户体验）

1. **命令发送能力**：实现 DDS 命令话题发布（解锁、模式切换、起飞/降落）
2. **传感器健康状态**：订阅 `sensor_selection` 或类似话题，填充 readyToFly
3. **任务协议**：通过 DDS 实现任务上传/下载

### 中优先级（完善数据覆盖）

4. **振动数据**：订阅 `vehicle_imu_status`
5. **RC 信号**：订阅 `input_rc`（如果通过 DDS 发布）
6. **测距仪**：订阅 `distance_sensor`
7. **ESC 状态**：订阅 `esc_status`

### 低优先级（特殊场景）

8. **第二 GPS**：订阅 `vehicle_gps_position` 实例 2
9. **温度数据**：订阅 `sensor_baro`
10. **发电机/EFI**：仅特殊机型需要

### IDL 自动生成工具

建议开发 PX4 .msg → CycloneDDS .idl 的自动转换工具：
```bash
python3 tools/generate_idl.py ~/PX4-Autopilot/msg/ --output src/DDS/idl/
```

这将消除手动维护 IDL 文件的工作，确保 PX4 升级后快速适配。

---

## 9. 结论

DDS P2 阶段已实现了 **QGC 核心飞行监视功能的完整数据通路**，包括位置、姿态、速度、高度、GPS、电池、风速、飞行模式和状态。用户在 DDS 模式下可以完成正常的飞行监视任务。

**与 MAVLink 的核心差异**在于：
1. **DDS 是单向只读的**（当前阶段），不能发送命令
2. **缺少传感器健康检查**，飞前状态始终显示 "Not Ready"
3. **无任务协议支持**，不能上传/下载航点
4. **无参数系统**，不能读写飞控参数

这些差异符合 P2 的设计目标——**数据监视**。命令控制和参数管理属于 P3+ 阶段的规划范围。
