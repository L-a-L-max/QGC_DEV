# DDS-Zenoh 版本 vs 官方 MAVLink QGC 功能对比

> 更新日期：2026-05-28  
> 分支：DDS_zenoh  
> 对比基准：QGC 4.x (MAVLink)

---

## 总览

| 类别 | MAVLink (官方) | DDS-Zenoh (本版) | 状态 |
|------|:---:|:---:|:---:|
| **遥测数据** | 全量 | 部分 | ⚠️ |
| **控制指令** | 全量 | 核心 | ⚠️ |
| **任务规划** | 全量 | 简化版 | ⚠️ |
| **参数系统** | 全量 | ❌ | 未转换 |
| **固件升级** | ✅ | ❌ | 未转换 |
| **日志下载** | ✅ | ❌ | 未转换 |
| **校准** | ✅ | ❌ | 未转换 |

---

## 一、遥测数据接收

### ✅ 已转换

| 功能 | MAVLink 消息 | DDS Topic | 说明 |
|------|-------------|-----------|------|
| 姿态 (Roll/Pitch/Yaw) | `ATTITUDE` | `/fmu/out/vehicle_attitude` | 四元数→欧拉角转换 |
| 全球位置 (Lat/Lon/Alt) | `GLOBAL_POSITION_INT` | `/fmu/out/vehicle_global_position` | 地图坐标 + AMSL高度 |
| 本地位置 (XYZ/速度) | `LOCAL_POSITION_NED` | `/fmu/out/vehicle_local_position_v1` | NED 位移和速度 |
| GPS 原始数据 | `GPS_RAW_INT` | `/fmu/out/vehicle_gps_position` | 星数、定位类型、精度 |
| 电池状态 | `BATTERY_STATUS` | `/fmu/out/battery_status_v1` | 电压、电流、剩余电量 |
| 风速风向 | `WIND_COV` | `/fmu/out/wind` | 风速 XY |
| 空速 | `VFR_HUD.airspeed` | `/fmu/out/airspeed_validated_v1` | 真空速/指示空速 |
| IMU 数据 | `RAW_IMU` | `/fmu/out/sensor_combined` | 加速度计 + 陀螺仪 |
| 飞行状态 | `HEARTBEAT` | `/fmu/out/vehicle_status_v1` | 飞行模式、解锁状态 |
| Home 位置 | `HOME_POSITION` | `/fmu/out/home_position_v1` | 起飞点坐标 |
| 着陆检测 | `EXTENDED_SYS_STATE` | `/fmu/out/vehicle_land_detected` | 在地/飞行中 |
| 安全标志 | — | `/fmu/out/failsafe_flags` | Ready to fly |
| 遥控器输入 | `MANUAL_CONTROL` | `/fmu/out/manual_control_setpoint` | 当前控制输入值 |
| 估计器状态 | `ESTIMATOR_STATUS` | `/fmu/out/estimator_status_flags` | GPS/光流有效性 |
| 指令应答 | `COMMAND_ACK` | `/fmu/out/vehicle_command_ack` | 命令执行结果 |
| 相对高度 | `GLOBAL_POSITION_INT.relative_alt` | 计算: AMSL - homeAlt | 和官方一致 |
| 地速 | `VFR_HUD.groundspeed` | 计算: sqrt(vx²+vy²) | 从本地速度计算 |
| 爬升率 | `VFR_HUD.climb` | local_position.vz (取反) | |
| 航向 | `VFR_HUD.heading` | attitude 四元数→yaw | |
| 到Home距离 | 内部计算 | 内部计算 | |

### ❌ 未转换

| 功能 | MAVLink 消息 | 原因/备注 |
|------|-------------|-----------|
| 伺服输出 | `SERVO_OUTPUT_RAW` | PX4 DDS 未发布此 topic |
| RC 通道值 | `RC_CHANNELS` | PX4 DDS 不发布 RC 通道 |
| HIL 传感器 | `HIL_*` | 仿真专用，DDS 不适用 |
| 光流数据 | `OPTICAL_FLOW_RAD` | 需订阅额外 topic |
| 振动数据 | `VIBRATION` | 需订阅 sensor_accel |
| 距离传感器 | `DISTANCE_SENSOR` | 需订阅 distance_sensor |
| 气压数据 | `SCALED_PRESSURE` | 需订阅 sensor_baro |
| 磁力计数据 | `SCALED_IMU2` | 需订阅额外 sensor topic |
| ADSB 交通 | `ADSB_VEHICLE` | PX4 DDS 未发布 |
| 高精 GPS (RTK) | `GPS_RTK` | 需额外 topic |
| ESC 状态 | `ESC_STATUS` | 需订阅 esc_status |
| 链路质量 | `RADIO_STATUS` | 无 DDS 等价物 |

---

## 二、控制指令发送

### ✅ 已转换

| 功能 | MAVLink 方式 | DDS 方式 | 说明 |
|------|-------------|---------|------|
| 解锁/上锁 | `MAV_CMD_COMPONENT_ARM_DISARM` | `vehicle_command` (DDS) | DDSCommandPublisher |
| 切换飞行模式 | `SET_MODE` / `MAV_CMD_DO_SET_MODE` | `vehicle_command` (DDS) | 通过 command ID |
| 起飞 | `MAV_CMD_NAV_TAKEOFF` | `vehicle_command` (DDS) | |
| 降落 | `MAV_CMD_NAV_LAND` | `vehicle_command` (DDS) | |
| 返航 RTL | `MAV_CMD_NAV_RETURN_TO_LAUNCH` | `vehicle_command` (DDS) | |
| 悬停/定点 | 切换 LOITER 模式 | `vehicle_command` (DDS) | |
| 手动控制 (虚拟摇杆) | `MANUAL_CONTROL` | `manual_control_setpoint` (DDS) | DDSManualControlPublisher |
| 手动控制 (USB摇杆) | `MANUAL_CONTROL` | `manual_control_setpoint` (DDS) | sendManualControlDirect |
| 手动控制 (G16/Skydroid) | N/A | `manual_control_setpoint` (DDS) | SkydroidJoystick |
| Goto 航点 | `MAV_CMD_DO_REPOSITION` | `goto_setpoint` (DDS) | DDSGotoPublisher |
| GCS 心跳 | `HEARTBEAT` (GCS→FC) | `telemetry_status` (DDS) | DDSHeartbeatPublisher |

### ❌ 未转换

| 功能 | MAVLink 方式 | 原因/备注 |
|------|-------------|-----------|
| 参数读写 | `PARAM_REQUEST_LIST` / `PARAM_SET` | PX4 DDS 无参数服务 |
| 任务上传 (标准) | `MISSION_ITEM_INT` + `MISSION_COUNT` | DDS 用 goto 逐点飞行替代 |
| 任务下载 | `MISSION_REQUEST_LIST` | 不适用 |
| 地理围栏上传 | `MISSION_ITEM` (GeoFence) | 无 DDS 等价物 |
| 集结点上传 | `RALLY_POINT` | 无 DDS 等价物 |
| 固件升级 | MAVLink FTP | 无 DDS 等价物 |
| 日志下载 | `LOG_REQUEST_LIST` / MAVLink FTP | 无 DDS 等价物 |
| 校准指令 | `MAV_CMD_PREFLIGHT_CALIBRATION` | 需 DDS command + 参数支持 |
| RC Override | `RC_CHANNELS_OVERRIDE` | 无 DDS 等价物 |
| 导引模式 (Orbit) | `MAV_CMD_DO_ORBIT` | 可通过 command 实现，未做 |
| Follow Me | `FOLLOW_TARGET` | 无 DDS 等价物 |
| 相机控制 | `MAV_CMD_DO_DIGICAM_*` | 无 DDS 等价物 |
| 云台控制 | `MAV_CMD_DO_MOUNT_*` | 无 DDS 等价物 |
| MAVLink 签名 | `SETUP_SIGNING` | 不适用 |

---

## 三、任务规划

| 功能 | MAVLink (官方) | DDS-Zenoh | 状态 |
|------|:---:|:---:|:---:|
| 航点任务上传/下载 | ✅ 完整协议 | ✅ GotoSetpoint 逐点飞 | 简化版 |
| 航线规划 (多点) | ✅ | ✅ | DDSMissionManager |
| 航点编辑 | ✅ | ✅ | DDSPlanPanel UI |
| 地图点击添加航点 | ✅ | ✅ | FlyView 集成 |
| 任务保存/加载 | ✅ (.plan) | ✅ (.ddsmission) | 格式不同 |
| 任务暂停/恢复 | ✅ | ✅ | |
| 到达检测 | ✅ FC 上报 | ✅ 客户端位置判断 (2m) | 实现方式不同 |
| 任务完成动作 | ✅ (RTL/Loiter/Land) | ✅ (Hover/RTL/Land) | |
| Survey (测量) | ✅ 完整 | ❌ | 未转换 |
| Corridor Scan | ✅ | ❌ | 未转换 |
| Structure Scan | ✅ | ❌ | 未转换 |
| 地形跟随 | ✅ | ❌ | 未转换 |
| 任务断点续飞 | ✅ SET_CURRENT | ❌ | 未实现 |
| DO_CHANGE_SPEED | ✅ | ⚠️ goto 自带速度参数 | 等价实现 |
| DO_SET_CAM_TRIGG | ✅ | ❌ | 未转换 |
| 条件命令 (延时等) | ✅ | ❌ | 未转换 |

---

## 四、参数系统

| 功能 | MAVLink (官方) | DDS-Zenoh | 状态 |
|------|:---:|:---:|:---:|
| 参数列表获取 | ✅ `PARAM_REQUEST_LIST` | ❌ | **未转换** |
| 单参数读取 | ✅ `PARAM_REQUEST_READ` | ❌ | **未转换** |
| 参数修改 | ✅ `PARAM_SET` | ❌ | **未转换** |
| 参数保存 | ✅ | ❌ | **未转换** |
| 参数搜索 | ✅ | ❌ | **未转换** |
| 参数导入/导出 | ✅ | ❌ | **未转换** |

> **影响**：无法在 DDS-only 连接下查看/修改 PX4 参数。需要 MAVLink 连接或 PX4 提供 DDS 参数服务。

---

## 五、车辆配置 (Vehicle Setup)

| 功能 | MAVLink (官方) | DDS-Zenoh | 状态 |
|------|:---:|:---:|:---:|
| 摇杆校准 | ✅ | ✅ | 本地操作，已支持 |
| 摇杆启用/禁用 | ✅ | ✅ | |
| 机架选择 | ✅ | ❌ | 依赖参数系统 |
| 传感器校准 | ✅ | ❌ | 依赖参数+command |
| 无线电校准 | ✅ | ❌ | 依赖参数+RC通道 |
| 飞行模式设置 | ✅ | ❌ | 依赖参数系统 |
| 电源设置 | ✅ | ❌ | 依赖参数系统 |
| 安全设置 | ✅ | ❌ | 依赖参数系统 |
| 电调校准 | ✅ | ❌ | 依赖参数+command |
| PID 调参 | ✅ | ❌ | 依赖参数系统 |
| 固件升级 | ✅ | ❌ | MAVLink FTP |
| 组件信息 | ✅ | ❌ | MAVLink FTP |

---

## 六、连接管理

| 功能 | MAVLink (官方) | DDS-Zenoh | 状态 |
|------|:---:|:---:|:---:|
| 连接配置 UI | ✅ | ✅ | DDSSettings.qml |
| 自动发现 | ✅ (heartbeat) | ✅ (DDS discovery + vehicle_status) | |
| Vehicle 创建 | ✅ (heartbeat → MultiVehicleMgr) | ✅ (DDSVehicleManager 合成心跳) | |
| 断连检测 | ✅ (heartbeat 超时 3.5s) | ✅ (数据接收超时 3.5s) | **本次新增** |
| 重连 | ✅ | ✅ (Zenoh bridge 清理+重启) | **本次修复** |
| 多链路冗余 | ✅ | ❌ | 单 DDS 域 |
| 高延迟链路 | ✅ | ❌ | 不适用 |
| Zenoh Bridge 管理 | N/A | ✅ | Android JNI |
| 链路优先级 | ✅ | ✅ (data_source 可配) | |

---

## 七、UI 功能

| 功能 | MAVLink (官方) | DDS-Zenoh | 状态 |
|------|:---:|:---:|:---:|
| 飞行视图 HUD | ✅ | ✅ | 姿态/高度/速度/航向 |
| 地图车辆跟踪 | ✅ | ✅ | |
| 虚拟摇杆 | ✅ | ✅ | DDS 专用面板 |
| 仪表盘 (Instrument) | ✅ | ✅ | Fact 系统驱动 |
| DDS 任务面板 | N/A | ✅ | DDSPlanPanel.qml |
| Plan View (标准) | ✅ 完整 | ❌ | 依赖 MissionManager 协议 |
| 视频流 | ✅ | ✅ (不受影响) | 独立于通信链路 |
| 地图标注 | ✅ | ✅ | |
| 告警消息 | ✅ `STATUSTEXT` | ❌ | PX4 DDS 不发布 |
| 电子围栏显示 | ✅ | ❌ | 无数据源 |
| 集结点显示 | ✅ | ❌ | 无数据源 |
| 飞行日志回放 | ✅ | ❌ | 依赖 MAVLink 日志 |

---

## 八、其他功能

| 功能 | MAVLink (官方) | DDS-Zenoh | 状态 |
|------|:---:|:---:|:---:|
| 多机管理 | ✅ | ⚠️ 理论支持(namespace) | 未充分测试 |
| RemoteID | ✅ | ❌ | 无 DDS topic |
| MAVLink Inspector | ✅ | N/A | |
| MAVLink Console | ✅ | ❌ | 无 DDS 等价物 |
| 地形数据 | ✅ | ❌ | |
| AirMap 空域 | ✅ | ✅ (不受影响) | 独立于通信 |
| NMEA GPS 输出 | ✅ | ❌ | |
| Joystick 按钮映射 | ✅ 完整 | ❌ | 未实现 DDS 版按钮动作 |
| DDS data_source 可配 | N/A | ✅ | 优先级设置 |
| Skydroid G16 SDK | N/A | ✅ | 专属功能 |
| IDL 版本自动适配 | N/A | ✅ | v1/v4/px4_v116 |
| 话题映射热加载 | N/A | ✅ | JSON 配置文件 |

---

## 九、功能转换优先级建议

### 高优先级（影响基本可用性）

| 编号 | 功能 | 难度 | 备注 |
|------|------|------|------|
| 1 | 参数读写 | ★★★★ | PX4 目前无 DDS 参数服务；需等 PX4 支持或走混合 MAVLink |
| 2 | STATUSTEXT 告警 | ★★ | 可订阅 `/fmu/out/log_message` (如 PX4 发布) |
| 3 | Joystick 按钮动作映射 | ★★ | 模式切换、解锁等通过 DDSCommandPublisher |
| 4 | 距离传感器 / 测距 | ★★ | 订阅 `distance_sensor` topic |

### 中优先级（增强体验）

| 编号 | 功能 | 难度 | 备注 |
|------|------|------|------|
| 5 | Survey/Corridor 任务 | ★★★ | 可在 DDSMissionManager 上扩展 |
| 6 | ESC 状态 | ★ | 订阅 `esc_status` 即可 |
| 7 | 振动数据 | ★ | 订阅 `sensor_accel` |
| 8 | 多机支持完善 | ★★★ | DDS namespace 分离 |

### 低优先级（MAVLink 特有/不适用）

| 编号 | 功能 | 原因 |
|------|------|------|
| 9 | 固件升级 | MAVLink FTP 协议特有，DDS 架构不适合 |
| 10 | MAVLink Console | 串口终端，DDS 无等价物 |
| 11 | RC 通道显示 | 通常不通过 DDS 发布 |
| 12 | 链路质量 | MAVLink radio 特有 |

---

## 十、架构差异说明

### 数据流对比

```
MAVLink (官方):
  FC ──MAVLink bytes──→ QGC
  QGC 解析 mavlink_msg_*
  → Vehicle::_handle*() 直接更新属性

DDS-Zenoh (本版):
  FC ──uXRCE-DDS──→ DDS Agent ──→ Zenoh Router ──→ Zenoh Bridge (Android)
                                                      ↓
                                              CycloneDDS localhost
                                                      ↓
  QGC DDSLink::_onPollTimer() 轮询读取 DDS samples
  → DDSDataInjector::onDDSMessage()
  → JSON映射 + Transform → Fact::setRawValue()
```

### 关键设计差异

| 方面 | MAVLink | DDS-Zenoh |
|------|---------|-----------|
| 数据格式 | 固定二进制协议 | IDL 定义 + CDR 序列化 |
| 发现机制 | 心跳广播 | DDS RTPS 发现 |
| 消息映射 | 代码硬编码 | JSON 配置文件 |
| 数据注入 | 直接属性赋值 | Fact 系统注入 |
| 扩展性 | 改代码 | 改 JSON 映射 + Transform |
| 多版本兼容 | MAVLink 版本协商 | IDL 版本选择 (v1/v4/px4_v116) |

---

## 十一、当前版本已解决的问题

| 问题 | 状态 | Commit |
|------|------|--------|
| SDL3 摇杆映射错误 (SILIC RQ10) | ✅ 已修复 | `85b8511` |
| USB 摇杆 DDS 路径 | ✅ 已实现 | `85b8511` + `ef79c36` |
| DDS 连接下摇杆校准 | ✅ 已支持 | `ef79c36` |
| 高度跳变 (GPS vs LocalPos) | ✅ 已修复 | `ef79c36` |
| Zenoh 重连报错 | ✅ 已修复 | `ef79c36` |
| DDS 断连无提示 | ✅ 已实现 (3.5s超时) | `ef79c36` |
| data_source 可配置 | ✅ 已实现 | `7d02c38` |
