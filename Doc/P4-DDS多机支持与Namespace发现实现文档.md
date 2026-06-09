# P4 — DDS 多机支持与 Namespace 发现

## 1. 概述

P4 在 P3（命令发送 + 就绪状态）基础上，实现 DDS 多机支持。核心思路是利用 PX4 的 UXRCE_DDS_NS 参数为每架无人机分配不同的 namespace，QGC 在连接配置时自动发现网络中的 namespace 并允许用户选择要连接的无人机。

### 1.1 设计目标

| 目标 | 说明 |
|------|------|
| Namespace 自动发现 | QGC 填写 Domain ID 后自动发现网络中所有 DDS topic 的 namespace |
| 用户选择或手动输入 | 下拉列表显示已发现的 namespace，也可手动输入 |
| 按 namespace 隔离连接 | 每个 DDS 连接只订阅指定 namespace 的 topic |
| 多连接多飞机 | 用户添加多个 DDS 连接，每个绑定不同 namespace |
| 向后兼容 | namespace 为空时行为与 P3 完全一致（单机模式） |

### 1.2 与 P3 的关系

P3 已实现的功能在 P4 中完全保留：
- 命令发送（DDSCommandPublisher）
- GCS Heartbeat（DDSHeartbeatPublisher）
- 就绪状态（failsafe_flags）
- 飞行模式同步（nav_state → custom_mode）
- MAVLink 初始化阶段跳过

P4 在此基础上添加：
- DDSDiscovery 类（namespace 自动发现）
- DDSConfiguration 扩展（discoveredNamespaces 属性）
- DDSSettings.qml 改造（Domain ID 确认按钮 + namespace 下拉列表）

## 2. 架构设计

### 2.1 PX4 端 Namespace 配置

每架 PX4 无人机通过 UXRCE_DDS_NS 参数设置不同的命名空间：

```bash
# 飞机 1
param set UXRCE_DDS_NS drone1
# 飞机 2
param set UXRCE_DDS_NS drone2
```

设置后，PX4 发布的 topic 自动带上 namespace 前缀：
```
rt/drone1/fmu/out/vehicle_status_v1
rt/drone1/fmu/out/sensor_gps
rt/drone1/fmu/in/vehicle_command
rt/drone2/fmu/out/vehicle_status_v1
rt/drone2/fmu/out/sensor_gps
rt/drone2/fmu/in/vehicle_command
```

### 2.2 QGC 端连接流程

```
用户操作                          QGC 内部
─────────                        ──────────
点击 Add → 选择 DDS          → 创建 DDSConfiguration
填写 Domain ID                → 存储 domainId
点击 "扫描" 按钮             → DDSDiscovery::startDiscovery(domainId)
                              → 创建临时 DDS participant
                              → 通过 builtin topic 发现网络中的 topic
                              → 正则解析 namespace: rt/{ns}/fmu/...
                              → 发射 namespacesUpdated(QStringList)
下拉列表显示 namespace         → QML ComboBox 绑定 discoveredNamespaces
选择 namespace / 手动输入      → 存储 namespacePrefix
点击 Save                     → DDSConfiguration 保存
Connect                       → DDSLink 使用 namespace 创建 reader/writer
```

### 2.3 类图

```
DDSConfiguration
├── domainId: int
├── namespacePrefix: QString         // 用户选择/输入的 namespace
├── discoveredNamespaces: QStringList // 自动发现的列表
├── startDiscovery()                 // 触发发现
└── stopDiscovery()                  // 停止发现

DDSDiscovery (新增)
├── startDiscovery(domainId)
├── stopDiscovery()
├── discoveredNamespaces(): QStringList
├── signal: namespacesUpdated(QStringList)
└── signal: discoveryError(QString)

DDSLink (已有，微调)
├── _createParticipant(domainId)     // 已有
├── _subscribeToTopics(...)          // 已有，使用 namespacePrefix
├── commandPublisher                 // 已有，使用 namespacePrefix
└── heartbeatPublisher               // 已有，使用 namespacePrefix

DDSSettings.qml (改造)
├── Domain ID 输入框
├── "扫描" 按钮
├── Namespace 下拉列表（可编辑 + 自动填充）
├── Vendor Mapping 输入框
└── 说明文本
```

### 2.4 Namespace 发现原理

CycloneDDS 的 builtin topic 机制允许通过 `DCPSPublication` 发现网络中所有已注册的 topic：

```cpp
// 获取 builtin publication reader
dds_entity_t pubReader = dds_create_reader(participant,
    DDS_BUILTIN_TOPIC_DCPSPUBLICATION, nullptr, nullptr);

// 读取 publication 信息
dds_builtintopic_endpoint_t samples[64];
dds_sample_info_t infos[64];
int n = dds_take(pubReader, samples, infos, 64, 64);

// 从每个 endpoint 的 topic_name 中提取 namespace
// topic_name 格式: "rt/drone1/fmu/out/vehicle_status_v1"
// 正则: ^rt/([^/]+)/fmu/
```

### 2.5 多机 Vehicle ID

| 项目 | 策略 |
|------|------|
| Vehicle ID | 每个 DDSLink 连接使用不同的 vehicleId（1, 2, 3...） |
| 分配方式 | DDSVehicleManager 从 MultiVehicleManager 获取下一个可用 ID |
| target_system | DDSCommandPublisher 使用对应的 vehicleId 作为 target |

## 3. 新增/修改文件清单

| 文件 | 类型 | 说明 |
|------|------|------|
| `src/DDS/DDSDiscovery.h` | 新增 | namespace 发现类头文件 |
| `src/DDS/DDSDiscovery.cc` | 新增 | namespace 发现实现 |
| `src/Comms/DDSLink/DDSConfiguration.h` | 修改 | 添加 discoveredNamespaces 属性和发现方法 |
| `src/Comms/DDSLink/DDSConfiguration.cc` | 修改 | 实现发现触发逻辑 |
| `src/AppSettings/DDSSettings.qml` | 修改 | Domain ID 确认 + namespace 下拉列表 |
| `src/Comms/DDSLink/DDSLink.cc` | 微调 | 确保 namespace 拼接在所有路径一致 |
| `src/DDS/DDSVehicleManager.cc` | 微调 | 支持多 Vehicle ID 分配 |

## 4. 关键实现细节

### 4.1 DDSDiscovery 的生命周期

DDSDiscovery 在用户点击"扫描"时创建临时 participant，发现完成或用户关闭配置页面时销毁。不影响正式的 DDSLink 连接。

```
扫描按钮点击 → startDiscovery(domainId)
  → dds_create_participant(domainId)
  → 启动定时器（每2秒轮询 builtin topic）
  → 发现新 namespace → 更新列表 → 通知 QML

用户关闭配置 / 点击停止 → stopDiscovery()
  → 停止定时器
  → dds_delete(participant)
```

### 4.2 Namespace 解析规则

DDS topic 名称格式约定：
```
无 namespace:  rt/fmu/out/vehicle_status_v1
有 namespace:  rt/{namespace}/fmu/out/vehicle_status_v1
```

解析正则：`^rt/([^/]+)/fmu/`
- 匹配 → 提取 group(1) 作为 namespace
- 不匹配 → 属于无 namespace 的无人机（默认单机）

### 4.3 向后兼容

- namespacePrefix 为空时 → 与 P3 行为完全一致
- 无 namespace 的 PX4 → 发现结果中显示为 "(default)" 或空项
- 旧配置文件中没有 namespacePrefix → 默认为空

### 4.4 Vehicle ID 分配

当前 DDSVehicleManager 硬编码 vehicleId=1。P4 改为：
- 每个 DDSLink 的 DDSVehicleManager 使用不同的 vehicleId
- vehicleId 基于 namespace 名称的 hash 或递增计数器
- 确保与 MAVLink 的 vehicleId 不冲突（MAVLink 范围 1-255）

## 5. 风险与限制

| 风险 | 缓解措施 |
|------|---------|
| builtin topic 发现延迟 | 轮询间隔 2 秒 + 持续扫描至用户关闭 |
| PX4 未配置 namespace | 显示为 "(default)"，向后兼容 |
| 同一 namespace 多次添加 | 配置界面提示重复 |
| CycloneDDS participant 数量限制 | 发现用临时 participant 使用后立即销毁 |
| Domain ID 不一致 | 用户需确保 QGC 与 PX4 使用相同 Domain ID |
