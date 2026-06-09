# QGC-DDS 功能对照与开发路线图

> 本文档梳理原版 QGC 的完整功能集，标注 DDS 版本已改造/未改造的状态，
> 并列出后续开发目标，作为 DDS 版 QGC 持续迭代的参考。

---

## 一、原版 QGC 功能总览

原版 QGroundControl 基于 MAVLink 协议实现全部功能，涵盖以下 6 大类：

### 1. 通信与连接

| 功能 | 说明 |
|------|------|
| MAVLink TCP/UDP/Serial | 支持多种物理链路 |
| MAVLink 心跳与链路管理 | 定期心跳检测连接状态 |
| 多机连接 | 同时连接多台无人机 |
| 链路质量监控 | 丢包率、延迟统计 |

### 2. 遥测数据显示

| 功能 | 数据源 (MAVLink) |
|------|----------------|
| GPS 位置（经纬度、高度） | GLOBAL_POSITION_INT |
| 姿态（Roll/Pitch/Yaw） | ATTITUDE |
| 速度（地速、空速） | VFR_HUD |
| 电池状态（电压、电流、剩余） | SYS_STATUS / BATTERY_STATUS |
| 飞行模式 | HEARTBEAT.custom_mode |
| 解锁/上锁状态 | HEARTBEAT.base_mode |
| Home 位置 | HOME_POSITION |
| GPS 信号质量（卫星数、HDOP） | GPS_RAW_INT |
| 着陆/飞行状态 | EXTENDED_SYS_STATE |
| 传感器健康 | SYS_STATUS.sensors |
| 风速/风向 | WIND_COV |
| 云台状态 | GIMBAL_DEVICE_ATTITUDE_STATUS |

### 3. 飞行操控（Guided Mode Actions）

| 功能 | MAVLink 命令 |
|------|-------------|
| 解锁 (Arm) | MAV_CMD_COMPONENT_ARM_DISARM |
| 上锁 (Disarm) | MAV_CMD_COMPONENT_ARM_DISARM |
| 起飞 (Takeoff) | MAV_CMD_NAV_TAKEOFF |
| 降落 (Land) | MAV_CMD_NAV_LAND |
| 返航 (RTL) | MAV_CMD_NAV_RETURN_TO_LAUNCH |
| 暂停 (Pause) | MAV_CMD_DO_PAUSE_CONTINUE |
| 开始任务 (Start Mission) | MAV_CMD_MISSION_START |
| 继续任务 (Continue) | MAV_CMD_MISSION_START |
| 飞往指定位置 (Go To) | MAV_CMD_DO_REPOSITION |
| 改变高度 | MAV_CMD_DO_CHANGE_SPEED + 高度设定 |
| 改变航向 | MAV_CMD_CONDITION_YAW |
| 绕点飞行 (Orbit) | MAV_CMD_DO_ORBIT |
| 兴趣区域 (ROI) | MAV_CMD_DO_SET_ROI_LOCATION |
| 紧急停机 | MAV_CMD_COMPONENT_ARM_DISARM (force) |
| 飞行模式切换 | MAV_CMD_DO_SET_MODE |
| 虚拟摇杆控制 | MANUAL_CONTROL 消息 |
| 物理摇杆控制 | MANUAL_CONTROL / RC_CHANNELS_OVERRIDE |

### 4. 任务规划（Plan View）

| 功能 | 说明 |
|------|------|
| **简单航点 (Waypoint)** | 地图点击添加航点，设置高度/速度/悬停时间 |
| **起飞项 (Takeoff)** | 设定起飞点和起飞高度 |
| **降落项 (Land)** | 固定翼/VTOL 降落模式，含进场航迹 |
| **悬停 (Loiter)** | 定点盘旋：按时间/圈数/无限 |
| **返航项 (RTL)** | 任务中插入返航指令 |
| **样条曲线航点 (Spline)** | 平滑曲线航点 (ArduPilot) |
| **延迟 (Delay)** | 在航点间添加等待时间 |
| **速度变更** | 动态改变飞行速度 |
| **高度变更** | 继续飞行并改变高度 |
| **跳转 (Do Jump)** | 跳转到指定航点（循环航线） |
| **ROI** | 飞行中相机朝向兴趣点 |
| **区域测量 (Survey)** | 多边形区域内自动生成 S 形测绘路线 |
| **走廊扫描 (Corridor Scan)** | 沿线状区域生成覆盖路线 |
| **结构扫描 (Structure Scan)** | 围绕建筑物环绕拍摄 |
| **固定翼降落模式** | 进场航迹 + 着陆点 |
| **VTOL 降落模式** | VTOL 专用过渡 + 降落 |
| **任务统计** | 总距离、预计时间、悬停/巡航时间 |
| **任务上传/下载** | MAVLink Mission Protocol 逐条传输 |
| **任务保存/加载** | 本地 .plan 文件 |
| **KML/SHP 导入** | 从 KML/SHP 文件导入飞行区域 |

### 5. 安全功能

| 功能 | 说明 |
|------|------|
| **地理围栏 (GeoFence)** | 设定飞行边界（圆形/多边形），超出自动触发安全动作 |
| **集结点 (Rally Points)** | 设定多个安全返航点 |
| **失联保护 (Failsafe)** | 失去 GCS 连接时自动 RTL/降落 |
| **低电量保护** | 电池低于阈值自动 RTL |
| **预检检查 (Pre-arm)** | 起飞前传感器/GPS/磁罗盘校验 |
| **降落伞触发** | MAV_CMD_DO_PARACHUTE |
| **电机测试** | MAV_CMD_DO_MOTOR_TEST |

### 6. 参数与校准

| 功能 | 说明 |
|------|------|
| **参数管理** | 读写飞控全部参数（PARAM_REQUEST_LIST / PARAM_SET） |
| **传感器校准** | 加速度计、磁罗盘、陀螺仪校准流程 |
| **遥控器校准** | RC 通道映射与范围设置 |
| **电调校准** | ESC 校准流程 |
| **固件升级** | 通过 USB 刷写固件 |
| **日志下载** | MAVLink FTP 下载飞行日志 |

---

## 二、DDS 版 QGC 已实现功能

DDS 版基于 CycloneDDS 直接与 PX4 的 XRCE-DDS Agent 通信，绕过 MAVLink 协议层。

### P0 — DDS 基础架构（已完成）

| 模块 | 功能 |
|------|------|
| DDSLink | DDS 通信链接，替代 MAVLink TCP/UDP |
| DDSConfiguration | Domain ID、Namespace 等 DDS 连接参数 |
| DDSMappingEngine | JSON 驱动的 DDS topic→Fact 字段映射 |
| DDSTransformRegistry | 数据转换函数注册（弧度→度、微秒→毫秒等） |
| DDSDataInjector | 将 DDS 数据注入 QGC 的 Fact 系统 |
| DDSTypeRegistry | IDL 类型注册与数据提取 |
| idlc 自动编译 | CMake 自动从 .idl 生成 C 类型定义 |

### P1 — CycloneDDS 集成与 SITL 连接（已完成）

| 功能 | DDS Topic |
|------|-----------|
| DDS Participant 管理 | 支持多 Domain |
| Topic 自动发现 | DCPSPublication builtin topic |
| Reader 创建与轮询 | 10ms 间隔 poll |
| 多网卡适配 | CYCLONEDDS_URI 配置 |

### P2 — 遥测数据显示（已完成）

| 功能 | DDS Topic | QGC 显示 |
|------|-----------|---------|
| GPS 位置 | fmu/out/vehicle_global_position | 地图标记 + HUD |
| 姿态 | fmu/out/vehicle_attitude | 姿态球 |
| 速度 | fmu/out/vehicle_global_position | 速度指示器 |
| 电池状态 | fmu/out/battery_status | 电池图标 + 百分比 |
| GPS 质量 | fmu/out/vehicle_gps_position | GPS 图标 + 卫星数 |
| 飞行模式 | fmu/out/vehicle_status (nav_state) | 模式文字 |
| 解锁状态 | fmu/out/vehicle_status (arming_state) | Armed/Disarmed |
| 着陆状态 | fmu/out/vehicle_land_detected | Flying/Landed |
| Home 位置 | fmu/out/home_position | Home 标记 |
| 传感器状态 | fmu/out/estimator_status_flags | 传感器图标 |

### P3 — 飞行操控命令（已完成）

| 功能 | DDS Topic | 状态 |
|------|-----------|------|
| VehicleCommand 发送 | fmu/in/vehicle_command | ✅ |
| Command ACK 接收 | fmu/out/vehicle_command_ack | ✅ |
| GCS 心跳 | fmu/in/telemetry_status | ✅ |
| 解锁/上锁 | MAV_CMD_COMPONENT_ARM_DISARM | ✅ |
| 起飞 | MAV_CMD_NAV_TAKEOFF | ✅ |
| 降落 | MAV_CMD_NAV_LAND | ✅ |
| 返航 | MAV_CMD_NAV_RETURN_TO_LAUNCH | ✅ |
| 飞行模式切换 | MAV_CMD_DO_SET_MODE | ✅ |
| **虚拟摇杆** | **fmu/in/manual_control_input** | **✅ 新增** |

---

## 三、未改造功能（后续开发目标）

按优先级排列，分为 **任务相关** 和 **安全相关** 两大类。

### 优先级 1 — 任务基础功能

| 功能 | 技术方案 | 难度 | 说明 |
|------|---------|------|------|
| **Go To（飞往指定位置）** | 发布 `fmu/in/goto_setpoint` (GotoSetpoint) | ⭐ | PX4 原生支持，NED 坐标+速度约束 |
| **Orbit（绕点飞行）** | MAV_CMD_DO_ORBIT via vehicle_command | ⭐ | 已有 command 通道 |
| **暂停/继续** | MAV_CMD_DO_PAUSE_CONTINUE via vehicle_command | ⭐ | 已有 command 通道 |
| **ROI（兴趣区域）** | MAV_CMD_DO_SET_ROI_LOCATION via vehicle_command | ⭐ | 已有 command 通道 |

### 优先级 2 — 航点任务规划

| 功能 | 技术方案 | 难度 | 说明 |
|------|---------|------|------|
| **简易航点导航** | Offboard 模式 + `fmu/in/goto_setpoint` 逐点发送 | ⭐⭐ | 不需要 Mission Protocol，QGC 端按序发送 goto_setpoint |
| **轨迹跟踪** | Offboard 模式 + `fmu/in/trajectory_setpoint` | ⭐⭐ | 位置/速度/加速度 setpoint |
| **完整 Mission Protocol** | DDS 自定义 topic 或 MAVLink 桥接 | ⭐⭐⭐ | PX4 DDS 无原生 Mission Protocol；需自行实现任务管理器或借助 MAVLink 桥 |

### 优先级 3 — 安全功能

| 功能 | 技术方案 | 难度 | 说明 |
|------|---------|------|------|
| **地理围栏** | 需要 Mission Protocol 支持（围栏项通过 mission item 上传） | ⭐⭐⭐ | 依赖任务上传机制 |
| **集结点** | 同上（rally point 也通过 mission protocol 上传） | ⭐⭐⭐ | 依赖任务上传机制 |
| **失联保护参数配置** | 需要 Parameter Protocol | ⭐⭐⭐ | PX4 DDS 未暴露参数读写接口 |
| **低电量保护配置** | 同上 | ⭐⭐⭐ | 需要参数协议 |
| **紧急停机** | MAV_CMD force arm/disarm via vehicle_command | ⭐ | 已有 command 通道 |

### 优先级 4 — 高级功能

| 功能 | 技术方案 | 难度 | 说明 |
|------|---------|------|------|
| **参数管理** | PX4 DDS 未暴露 PARAM protocol | ⭐⭐⭐⭐ | 需要 PX4 侧扩展或 MAVLink 桥接 |
| **日志下载** | PX4 DDS 未暴露 FTP protocol | ⭐⭐⭐⭐ | 需要 PX4 侧扩展 |
| **传感器校准** | 需要 MAVLink 命令长序列交互 | ⭐⭐⭐⭐ | 复杂交互流程 |
| **固件升级** | USB 直连，与 DDS 无关 | — | 保持 MAVLink 方式 |
| **区域测量 (Survey)** | 依赖 Mission Protocol + 相机触发 | ⭐⭐⭐⭐ | 需完整任务上传+相机控制 |
| **走廊扫描** | 同上 | ⭐⭐⭐⭐ | |
| **结构扫描** | 同上 | ⭐⭐⭐⭐ | |

---

## 四、PX4 DDS 可用 Topic 参考

### 输入 Topic（QGC → PX4）

| Topic | 消息类型 | 用途 |
|-------|---------|------|
| fmu/in/vehicle_command | VehicleCommand | 所有 MAV_CMD 命令 |
| fmu/in/telemetry_status | TelemetryStatus | GCS 心跳 |
| fmu/in/manual_control_input | ManualControlSetpoint | 虚拟摇杆/RC 模拟 |
| fmu/in/offboard_control_mode | OffboardControlMode | Offboard 模式使能 |
| fmu/in/goto_setpoint | GotoSetpoint | 指点飞行（位置+航向+速度约束） |
| fmu/in/trajectory_setpoint | TrajectorySetpoint | 轨迹控制（位置/速度/加速度） |
| fmu/in/vehicle_attitude_setpoint | VehicleAttitudeSetpoint | 姿态控制 |
| fmu/in/vehicle_rates_setpoint | VehicleRatesSetpoint | 角速率控制 |
| fmu/in/vehicle_thrust_setpoint | VehicleThrustSetpoint | 推力控制 |

### 输出 Topic（PX4 → QGC，已订阅）

| Topic | 消息类型 | 用途 |
|-------|---------|------|
| fmu/out/vehicle_global_position | VehicleGlobalPosition | GPS 位置 |
| fmu/out/vehicle_attitude | VehicleAttitude | 姿态 |
| fmu/out/vehicle_status | VehicleStatus | 飞行模式、解锁状态 |
| fmu/out/battery_status | BatteryStatus | 电池 |
| fmu/out/vehicle_gps_position | SensorGps | GPS 质量 |
| fmu/out/vehicle_land_detected | VehicleLandDetected | 着陆状态 |
| fmu/out/home_position | HomePosition | Home 位置 |
| fmu/out/vehicle_command_ack | VehicleCommandAck | 命令应答 |
| fmu/out/estimator_status_flags | EstimatorStatusFlags | 传感器状态 |
| fmu/out/vehicle_control_mode | VehicleControlMode | 控制模式 |
| fmu/out/failsafe_flags | FailsafeFlags | 失效保护标志 |
| fmu/out/vehicle_local_position | VehicleLocalPosition | 本地位置 |

---

## 五、虚拟摇杆 DDS 实现说明

### 架构

```
VirtualJoystick.qml (25Hz Timer)
    ↓ stick values [-1,1]
Vehicle::virtualTabletJoystickValue()
    ↓ DDS publisher available?
    ├── Yes → DDSManualControlPublisher::sendManualControl()
    │         → DDS write ManualControlSetpoint to fmu/in/manual_control_input
    └── No  → sendJoystickDataThreadSafe()
              → MAVLink MANUAL_CONTROL (原路径)
```

### 新增文件

| 文件 | 说明 |
|------|------|
| `src/DDS/DDSManualControlPublisher.h` | 声明 |
| `src/DDS/DDSManualControlPublisher.cc` | 实现：创建 BEST_EFFORT+VOLATILE writer，发送 ManualControlSetpoint |

### 修改文件

| 文件 | 修改 |
|------|------|
| `src/Vehicle/Vehicle.h` | 新增 `_ddsManualControlPublisher` 成员和 setter/getter |
| `src/Vehicle/Vehicle.cc` | `virtualTabletJoystickValue()` 优先走 DDS 路径 |
| `src/Comms/DDSLink/DDSLink.h` | 新增 `_manualControlPublisher` 成员和 accessor |
| `src/Comms/DDSLink/DDSLink.cc` | 连接时初始化、断开时清理 ManualControlPublisher |
| `src/DDS/DDSVehicleManager.cc` | Vehicle 创建时附加 ManualControlPublisher |
| `src/Comms/CMakeLists.txt` | 添加新源文件到构建 |

### 使用方法

1. 在 QGC 设置中启用 **Virtual Joystick**（Application Settings → General → Virtual Joystick）
2. 连接 DDS 无人机
3. 飞行界面底部出现双摇杆
4. 将无人机切换到 **Position 模式**（或其他接受手动输入的模式）
5. 操作摇杆控制无人机

### 注意事项

- PX4 需要处于接受外部手动控制输入的模式（Position/Altitude/Manual）
- DDS 版使用 `data_source=2`（SOURCE_MAVLINK_0），PX4 视为第一个 MAVLink 实例的输入
- 发送频率 25Hz，与真实遥控器一致
- QoS: BEST_EFFORT + VOLATILE，匹配 PX4 XRCE-DDS Agent 的 reader 配置

---

## 六、开发阶段总结

| 阶段 | 内容 | 状态 |
|------|------|------|
| P0 | DDS 基础架构（mapping/transform/inject） | ✅ 完成 |
| P1 | CycloneDDS 集成，SITL 连接 | ✅ 完成 |
| P2 | 遥测数据显示（GPS/姿态/电池/模式等） | ✅ 完成 |
| P3 | 命令发送（arm/takeoff/land/RTL/模式切换）+ 虚拟摇杆 | ✅ 完成 |
| P4 | 多机支持（namespace 发现、多 vehicle）| 📋 封存（命令投递 bug 待验证） |
| P5 | 指点飞行 (Go To) + 简易航点导航 | 🔜 计划中 |
| P6 | 安全功能（紧急停机、失联保护） | 🔜 计划中 |
| P7 | 完整任务规划（Mission Protocol 替代方案） | 🔜 计划中 |
