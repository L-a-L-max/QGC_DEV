# P3 阶段：DDS 命令发送与状态就绪

## 概述

P3 阶段将 QGC 的 DDS 模式从**只读监视**升级为**双向控制**，实现通过 DDS 向 PX4 发送命令（解锁/上锁、模式切换、起飞/降落），同时完善飞行就绪状态的显示。

---

## 1. 目标与效果

### 1.1 命令发送

| 功能 | 用户操作 | 预期效果 |
|------|---------|---------|
| 解锁 (Arm) | 点击工具栏 Arm 按钮 | 飞机解锁，电机怠速转动 |
| 上锁 (Disarm) | 点击工具栏 Disarm 按钮 | 飞机上锁，电机停止 |
| 起飞 (Takeoff) | 滑动起飞确认 → 设置高度 | 飞机自动起飞到指定高度 |
| 降落 (Land) | 点击 Land 按钮 | 飞机执行自动降落 |
| 返航 (RTL) | 点击 RTL 按钮 | 飞机返回 Home 点并降落 |
| 模式切换 | 工具栏模式选择器切换 | 飞机切换到指定飞行模式 |

### 1.2 状态就绪

| 功能 | 当前状态 | P3 后状态 |
|------|---------|---------|
| Ready to Fly | 始终显示 "Not Ready" | 根据传感器健康状态正确显示 |
| 飞前检查 | 空白 | 显示关键传感器状态 |
| 命令 ACK 反馈 | 无 | 命令执行成功/失败有 UI 反馈 |

---

## 2. 技术架构

### 2.1 命令发送数据流

```
用户操作 (QML UI)
  → Vehicle::sendMavCommand(compId, MAV_CMD, params...)
    → [P3 Hook] DDSCommandPublisher::sendCommand()
      → 构建 VehicleCommand 结构体
      → dds_write(writer, &vehicleCommand)
        → CycloneDDS 序列化 → UDP 发送
          → PX4 MicroXRCE-DDS Agent 接收
            → uORB vehicle_command
              → PX4 Commander 模块执行
                → vehicle_status 状态变化
                  → DDS 发布 vehicle_command_ack
                    → QGC 订阅接收 ACK
                      → 更新 UI 状态
```

### 2.2 核心组件

| 组件 | 文件 | 职责 |
|------|------|------|
| VehicleCommand IDL | `src/DDS/idl/VehicleCommand.idl` | 定义命令消息结构 |
| DDSCommandPublisher | `src/DDS/DDSCommandPublisher.h/cc` | 创建 DDS writer 并发布命令 |
| DDSLink 扩展 | `src/Comms/DDSLink/DDSLink.h/cc` | 持有 writer，提供发送接口 |
| Vehicle Hook | `src/Vehicle/Vehicle.cc` | 拦截 `sendMavCommand`，转发到 DDS |
| VehicleCommandAck 订阅 | 已有 IDL + TypeRegistry | 接收命令执行结果 |

### 2.3 PX4 DDS 话题

| 方向 | 话题 | 用途 |
|------|------|------|
| QGC → PX4 | `rt/fmu/in/vehicle_command` | 发送命令（解锁、模式切换等） |
| PX4 → QGC | `rt/fmu/out/vehicle_command_ack` | 接收命令执行结果 |
| PX4 → QGC | `rt/fmu/out/vehicle_status` | 已有 — 监视状态变化确认命令生效 |

---

## 3. VehicleCommand 消息结构

基于 PX4 v1.17-alpha `msg/versioned/VehicleCommand.msg`：

```
uint64  timestamp              # 时间戳 (us)
float32 param1                 # 命令参数 1
float32 param2                 # 命令参数 2
float32 param3                 # 命令参数 3
float32 param4                 # 命令参数 4
float64 param5                 # 命令参数 5 (经度等双精度)
float64 param6                 # 命令参数 6 (纬度等双精度)
float32 param7                 # 命令参数 7
uint32  command                # MAV_CMD 命令 ID
uint8   target_system          # 目标系统 ID
uint8   target_component       # 目标组件 ID
uint8   source_system          # 源系统 ID
uint16  source_component       # 源组件 ID
uint8   confirmation           # 确认计数
bool    from_external          # 外部来源标记 (必须为 true)
```

### 关键命令映射

| QGC 操作 | MAV_CMD | param1 | 其他参数 |
|---------|---------|--------|---------|
| 解锁 | 400 (COMPONENT_ARM_DISARM) | 1.0 | — |
| 上锁 | 400 (COMPONENT_ARM_DISARM) | 0.0 | — |
| 起飞 | 22 (NAV_TAKEOFF) | -1 (pitch) | param7=高度 AMSL |
| 降落 | — | — | 通过 DO_SET_MODE 切换到 Land 模式 |
| RTL | — | — | 通过 DO_SET_MODE 切换到 RTL 模式 |
| 模式切换 | 176 (DO_SET_MODE) | MAV_MODE_FLAG_CUSTOM_MODE_ENABLED | param2=custom_mode |

---

## 4. 实现方案

### 4.1 VehicleCommand.idl

```idl
module px4_msgs {
module msg {
module dds_ {

@topic
struct VehicleCommand_ {
    unsigned long long timestamp;
    float param1;
    float param2;
    float param3;
    float param4;
    double param5;
    double param6;
    float param7;
    unsigned long command;
    octet target_system;
    octet target_component;
    octet source_system;
    unsigned short source_component;
    octet confirmation;
    boolean from_external;
};

}; // dds_
}; // msg
}; // px4_msgs
```

### 4.2 DDSCommandPublisher

```cpp
class DDSCommandPublisher : public QObject {
    Q_OBJECT
public:
    explicit DDSCommandPublisher(QObject *parent = nullptr);

    /// 初始化：创建 DDS writer
    bool init(dds_entity_t participant, const QString &namespacePrefix);

    /// 发送命令
    bool sendCommand(uint32_t command,
                     float param1, float param2, float param3, float param4,
                     double param5, double param6, float param7,
                     uint8_t targetSystem = 1,
                     uint8_t targetComponent = 1);

    /// 是否已初始化
    bool isReady() const;

signals:
    void commandSent(uint32_t command);
    void commandFailed(uint32_t command, const QString &reason);

private:
    dds_entity_t _writer = DDS_ENTITY_NIL;
    dds_entity_t _topic  = DDS_ENTITY_NIL;
};
```

### 4.3 Vehicle 命令拦截

在 `Vehicle::sendMavCommand()` 中添加 DDS 路径判断：

```cpp
void Vehicle::sendMavCommand(...) {
    // 如果当前连接是 DDS Link 且 CommandPublisher 就绪
    if (_ddsCommandPublisher && _ddsCommandPublisher->isReady()) {
        _ddsCommandPublisher->sendCommand(command, p1, p2, p3, p4, p5, p6, p7,
                                          id(), _defaultComponentId);
        return;
    }
    // 否则走原来的 MAVLink 路径
    ...
}
```

### 4.4 命令 ACK 处理

已有 `VehicleCommandAck` IDL 和 TypeRegistry。在 `DDSDataInjector` 中添加 ACK 处理：

```cpp
if (topicName.contains("vehicle_command_ack")) {
    uint32_t cmd = fields["command"].toUInt();
    uint8_t result = fields["result"].toUInt();
    // 转换为 mavlink_command_ack_t 格式，触发 Vehicle 的标准 ACK 处理流程
    emit commandAckReceived(cmd, result);
}
```

### 4.5 就绪状态

通过已订阅的 `vehicle_status` 话题中的字段判断飞行就绪：
- `arming_state == 1 (STANDBY)` 且传感器正常 → "Ready to Fly"
- 订阅 `failsafe_flags` 话题（已有 IDL）补充传感器健康信息

---

## 5. 新增/修改文件清单

| 操作 | 文件 | 说明 |
|------|------|------|
| 新建 | `src/DDS/idl/VehicleCommand.idl` | 命令消息 IDL |
| 新建 | `src/DDS/DDSCommandPublisher.h` | 命令发布器头文件 |
| 新建 | `src/DDS/DDSCommandPublisher.cc` | 命令发布器实现 |
| 修改 | `src/DDS/DDSTypeRegistry.cc` | 注册 VehicleCommand 类型 |
| 修改 | `src/Comms/DDSLink/DDSLink.h` | 添加 CommandPublisher 成员 |
| 修改 | `src/Comms/DDSLink/DDSLink.cc` | 初始化 writer |
| 修改 | `src/DDS/DDSDataInjector.cc` | 处理 command_ack |
| 修改 | `src/DDS/DDSDataInjector.h` | 添加 ACK 信号 |
| 修改 | `src/DDS/DDSVehicleManager.cc` | 连接 CommandPublisher 到 Vehicle |
| 修改 | `src/Vehicle/Vehicle.cc` | sendMavCommand DDS 路径 |
| 修改 | `src/Vehicle/Vehicle.h` | 添加 DDS command publisher 指针 |
| 修改 | `src/Comms/CMakeLists.txt` | 添加新源文件 |
| 修改 | `resources/dds_mappings/_default.json` | 添加 failsafe_flags 映射 |

---

## 6. 与 MAVLink 命令流对比

| 特性 | MAVLink | DDS P3 |
|------|---------|--------|
| 传输协议 | MAVLink 编码 → Serial/UDP | CDR 序列化 → DDS UDP |
| 可靠性 | MAVLink 重传 (3次/3s超时) | DDS RELIABLE QoS |
| ACK 机制 | COMMAND_ACK 消息 | vehicle_command_ack 话题 |
| 命令队列 | QGC MavCommandQueue | DDS RELIABLE + 重传 |
| 多 GCS 冲突 | 无原生仲裁 | 同样无原生仲裁 |
| 延迟 | ~10-50ms (取决于链路) | ~5-20ms (局域网 DDS) |

---

## 7. 安全考虑

1. **from_external = true**：所有 QGC 发送的命令必须设置此标志，否则 PX4 Commander 会拒绝
2. **target_system 匹配**：命令的 target_system 必须与 PX4 的 system_id 匹配
3. **source_system 标识**：QGC 使用 system_id=255（标准 GCS ID）
4. **confirmation = 0**：首次发送，不重复确认
5. **ACK 超时**：如果 3 秒内未收到 ACK，向用户显示命令超时

---

## 8. 依赖关系

- P2 全部功能正常运行（数据接收、Vehicle 创建）
- PX4 MicroXRCE-DDS Agent 配置了 `/fmu/in/vehicle_command` 订阅
- PX4 配置了 `/fmu/out/vehicle_command_ack` 发布
- CycloneDDS writer 能够匹配 PX4 Agent 的 reader（同一 Domain）
