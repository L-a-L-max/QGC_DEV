# DDS vs MAVLink 数据差异分析

本文档对比 QGC DDS 版本与 MAVLink 版本在数据覆盖、功能完整性方面的差异，用于指导后续开发优先级。

## 一、当前 DDS 版本已覆盖的功能（15 个订阅 topic）

| 功能 | DDS Topic | 状态 |
|------|-----------|------|
| 姿态（roll/pitch/heading） | vehicle_attitude | 正常 |
| 地图位置（lat/lon） | vehicle_global_position | 正常 |
| 海拔高度（AMSL） | vehicle_global_position.alt | 正常 |
| 相对高度 | vehicle_local_position.z | 需修复（见 P0） |
| 速度（地速/爬升率） | vehicle_local_position vx/vy/vz | 正常 |
| GPS 信息（HDOP/VDOP/卫星数/定位类型） | vehicle_gps_position | 正常 |
| 电池（电压/电流/百分比/温度/已消耗mAh） | battery_status_v1 | 缺 timeRemaining/instantPower |
| 风速风向 | wind | 正常 |
| 空速 | airspeed_validated_v1 | 正常 |
| 陀螺仪角速率 | sensor_combined | 正常 |
| 油门百分比 | manual_control_setpoint | 正常 |
| EKF 状态标志 | estimator_status_flags | 正常 |
| 解锁状态/飞行模式 | vehicle_status_v1 | 正常 |
| Home 位置 | home_position_v1 | 正常 |
| 着陆检测 | vehicle_land_detected | 正常 |
| 故障安全标志 | failsafe_flags | 正常 |
| 命令确认 | vehicle_command_ack | 正常 |

## 二、完全丢失的数据（MAVLink 有，DDS 无）

### 关键丢失（直接影响飞行安全和基本使用）

| MAVLink 消息 | 提供的数据 | 影响 |
|-------------|-----------|------|
| **HEARTBEAT** | 飞控类型、自驾仪类型、系统状态 | DDS 通过 vehicle_status 部分替代，但缺少飞控类型自动识别（PX4 vs ArduPilot），当前硬编码为 PX4 |
| **SYS_STATUS** | 所有传感器健康状态（陀螺/加速计/磁力计/气压计/GPS/光流等）、通信丢包率 | DDS 只有 failsafe_flags 提供部分信息。缺少详细的传感器健康监控面板，无法知道哪个传感器异常 |
| **STATUSTEXT** | 飞控发来的文字消息（警告、错误、信息） | 完全丢失。无法看到 PX4 的 "Preflight check failed"、"GPS accuracy low"、"Battery critical" 等重要提示 |
| **RC_CHANNELS** | 18 个 RC 通道原始值 + RC 信号强度（RSSI） | 完全丢失。无法监控遥控器通道值，无法查看 RC 信号强度 |
| **EXTENDED_SYS_STATE** | 精确着陆状态（地面/起飞中/空中/降落中）、VTOL 状态（固定翼/多旋翼） | DDS 的 vehicle_land_detected 只有 landed=true/false，丢失了"起飞中"和"降落中"的区分。VTOL 前飞/悬停模式切换也丢失 |

### 重要丢失（影响高级功能）

| MAVLink 消息/协议 | 提供的数据 | 影响 |
|------------------|-----------|------|
| **参数协议**（PARAM_VALUE/PARAM_SET） | 读取/修改飞控所有参数 | 完全丢失。无法在 QGC 中查看或修改 PX4 参数（PID、COM_RC_IN_MODE 等） |
| **任务协议**（MISSION_ITEM_INT 等） | 航点上传/下载/执行 | 有自定义 DDSMissionManager，但功能受限 |
| **VFR_HUD** | 综合 HUD 数据：空速、地速、爬升率、油门、高度 | DDS 从多个 topic 分散获取。丢失：timeToHome（到 Home 预计时间）、altitudeTuning（高度调节） |
| **NAV_CONTROLLER_OUTPUT** | 导航控制器输出：高度设定点、航迹偏差、空速设定点、到下一航点距离 | 完全丢失。HUD 上不显示 distanceToNextWP、xTrackError |
| **SERVO_OUTPUT_RAW** | 16 个电机/舵机输出值 | 完全丢失。无法在 QGC 中监控电机输出 |
| **地理围栏协议** | 围栏配置/违规报告 | 完全丢失 |
| **集结点协议** | Rally Point 管理 | 完全丢失 |
| **标定协议** | 传感器标定（加速计/陀螺/磁力计/水平） | DDS 可发命令但无法收到标定进度文字反馈 |

### 次要丢失（特殊场景功能）

| MAVLink 消息 | 提供的数据 | 影响 |
|-------------|-----------|------|
| OBSTACLE_DISTANCE | 避障传感器距离数据 | 无避障显示 |
| ADSB_VEHICLE | ADS-B 交通信息 | 无附近飞机显示 |
| CAMERA_IMAGE_CAPTURED | 相机触发坐标 | 地图上无拍照位置标记 |
| LOG_ENTRY/LOG_DATA | 机载日志下载 | 无法从 QGC 下载飞行日志 |
| SERIAL_CONTROL | MAVLink 控制台 | 无法访问 PX4 NSH Shell |
| GPS2_RAW | 第二 GPS 数据 | 无双 GPS 支持 |
| SCALED_PRESSURE 1/2/3 | 气压计温度（最多3个） | 无气压计温度显示 |
| ESC_INFO/ESC_STATUS | ESC 遥测（RPM/电流/温度） | 无电调监控 |
| RAW_IMU | IMU 温度 | 无 IMU 温度显示 |
| FENCE_STATUS | 围栏违规报告 | 无围栏警报 |
| EVENT | PX4 结构化事件系统 | 无事件消息 |

## 三、HUD/界面上具体缺失的显示项

| 显示项 | MAVLink 来源 | DDS 是否有 | 状态 |
|--------|-------------|-----------|------|
| Roll/Pitch/Heading | ATTITUDE | vehicle_attitude | 正常 |
| 地速 | VFR_HUD.groundspeed | local_position vx/vy | 正常 |
| 空速 | VFR_HUD.airspeed | airspeed_validated | 正常 |
| 爬升率 | VFR_HUD.climb | local_position vz | 正常 |
| 油门% | VFR_HUD.throttle | manual_control_setpoint | 正常 |
| 相对高度 | ALTITUDE.altitude_relative | local_position.z（有偏移） | 需修复 |
| AMSL 高度 | ALTITUDE.altitude_amsl | global_position.alt | 正常 |
| **到 Home 距离** | 计算值 | 有数据但未计算 | **不显示** |
| **到 Home 时间** | VFR_HUD 计算 | 无 | **不显示** |
| **到下一航点距离** | NAV_CONTROLLER_OUTPUT | 无 | **不显示** |
| **航迹偏差** | NAV_CONTROLLER_OUTPUT | 无 | **不显示** |
| RC 信号强度 | RC_CHANNELS.rssi | 无 | **不显示** |
| **飞控消息** | STATUSTEXT | 无 | **不显示** |
| 传感器健康 | SYS_STATUS | 无 | **不显示** |
| 电池剩余时间 | BATTERY_STATUS | 未映射 | **不显示** |
| 电池瞬时功率 | 计算值 | 未计算 | **不显示** |

## 四、按优先级的修复建议

### P0（必须修复，影响安全和基本体验）

1. `altitudeRelative` 改用 `global_position.alt - home_position.alt`（消除 EKF 原点偏移）
2. 添加 `distanceToHome` 计算（当前坐标到 Home 坐标的球面距离）
3. 添加 `timeToHome` 计算（distanceToHome / groundSpeed）
4. 在界面右下角同时显示 AMSL 高度和相对高度

### P1（强烈建议，改善使用体验）

5. 补充电池 `timeRemaining` 映射（DDS 有 `time_remaining_s` 字段未映射）
6. 补充电池 `instantPower` 计算（voltage x current）
7. 添加参数读写协议（通过 DDS service 或自定义 topic）

### P2（后续完善）

8. 添加 STATUSTEXT 等效的飞控消息显示
9. 添加 RC 通道和信号强度监控
10. 添加传感器健康状态面板
11. 补充 VTOL 状态和精确着陆状态

## 五、数据来源对照表

### MAVLink 版本数据流
```
PX4 ──MAVLink──> QGC
  HEARTBEAT         → Vehicle type, armed, flight mode
  ATTITUDE          → roll, pitch, yaw
  GLOBAL_POSITION   → lat, lon, alt, relative_alt, heading
  GPS_RAW_INT       → fix type, hdop, vdop, satellites
  VFR_HUD           → airspeed, groundspeed, climb, throttle
  ALTITUDE          → altitude_relative, altitude_amsl
  BATTERY_STATUS    → voltage (cells), current, remaining, time_remaining
  SYS_STATUS        → sensor health, prearm check
  STATUSTEXT        → text messages from FC
  RC_CHANNELS       → 18 channel values + RSSI
  SERVO_OUTPUT_RAW  → 16 motor/servo outputs
  NAV_CONTROLLER    → distance to WP, xtrack error
  EXTENDED_SYS_STATE→ landed state, VTOL state
  PARAM_VALUE       → parameter values (bidirectional)
  MISSION_ITEM_INT  → mission waypoints (bidirectional)
```

### DDS 版本数据流
```
PX4 ──MicroXRCEAgent──DDS──zenoh-bridge──> QGC
  vehicle_attitude          → roll, pitch, yaw (quaternion)
  vehicle_global_position   → lat, lon, alt (AMSL)
  vehicle_local_position_v1 → x, y, z, vx, vy, vz
  vehicle_gps_position      → fix type, hdop, vdop, satellites
  battery_status_v1         → voltage, current, remaining, temperature
  wind                      → wind speed/direction
  airspeed_validated_v1     → calibrated airspeed
  sensor_combined           → gyro rates
  manual_control_setpoint   → throttle
  estimator_status_flags    → EKF status
  vehicle_status_v1         → arming_state, nav_state
  home_position_v1          → home lat/lon/alt
  vehicle_land_detected     → landed (bool)
  failsafe_flags            → critical failsafe flags
  vehicle_command_ack       → command results
```

### 差距总结

- MAVLink: ~25+ 条消息类型，覆盖遥测、参数、任务、标定、日志等完整功能
- DDS: 15 条订阅 topic + 4 条发布 topic，仅覆盖基础遥测和简单命令
- 缺失比例: 约 40% 的显示数据丢失，约 70% 的交互功能丢失（参数/任务/标定/日志）
