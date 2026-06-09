# P4 — DDS 多机支持与 Namespace 发现 — 进展记录

## 1. 概述

P4 在 P3（命令发送 + 状态就绪）的基础上，实现 DDS 多机支持。核心思路是利用 PX4 的 UXRCE_DDS_NS 参数为每架无人机分配不同的 namespace，QGC 在连接设置中自动发现网络中的 namespace 并允许用户选择要连接的无人机。

### 1.1 当前状态：**已封存（待解决 Agent 层 RTPS 路由问题）**

| 功能 | 状态 |
|------|------|
| Namespace 自动发现 | ✅ 完成 |
| 用户选择或手动输入 namespace | ✅ 完成 |
| 按 namespace 隔离连接 | ✅ 完成 |
| 多连接多飞机显示 | ✅ 完成 |
| 无人机名称显示 namespace | ✅ 完成 |
| 切换无人机不断开连接 | ✅ 完成 |
| Vehicle 重连不重复创建 | ✅ 完成 |
| cmd 512 过滤 | ✅ 完成 |
| **多机命令发送** | ❌ px4_1 正常，px4_2/px4_3 失败 |

## 2. 提交历史

| Commit | 内容 |
|--------|------|
| `d850677b8` | P4 初始实现：DDSDiscovery、namespace 选择、多 Vehicle |
| `04a9786fb` | 复用 discovery participant、修复 target_system、Vehicle 显示 namespace |
| `0552639c1` | 修复 participant 转移、namespace 选择同步 |
| `d71cb5960` | 修复重复 Vehicle 创建、cmd 512 过滤、participant 保存复用 |
| `6995b3bfb` | 修复 CycloneDDS participant 复用 bug（断连后始终销毁重建） |
| `1c9e6b263` | 修复 MultiVehicleSelector 字符串解析 crash |
| `680b80155` | disconnect 时重置 vehicleCreated |
| `2331e47bb` | 增加命令发送诊断日志 |
| `36e4a5fa0` | 共享 participant 池（每 domain 一个 participant） |
| `034a3177d` | 尝试 BEST_EFFORT QoS（与 HeartbeatPublisher 一致） |
| `f97798c97` | 尝试 RELIABLE + 单活跃 writer |
| `f99050e0f` | RELIABLE + 2s 匹配超时 + subscription handle 诊断 |
| `12b59597b` | 最终方案：BEST_EFFORT + 创建即初始化 writer + 移除单活跃 writer |

## 3. 已解决的问题

### 3.1 Vehicle 名称显示（commit `1c9e6b263`）
- **问题**：QML 用 `"Vehicle N".split(" ")[1]` 解析 ID，但 customName = "px4_1" 无空格 → undefined → activeVehicle = null → 显示 "Disconnected"
- **修复**：Repeater 直接用 Vehicle 对象引用

### 3.2 Participant 复用失败（commit `0552639c1`）
- **问题**：DDSDiscovery 的 participant 在 subEditConfig 上创建，copyFrom() 没有转移 → DDSLink 永远拿不到
- **修复**：copyFrom() 调用 releaseParticipant() 转移

### 3.3 CycloneDDS participant 重用 bug（commit `6995b3bfb`）
- **问题**：销毁 participant 上所有 reader/writer 后重新创建，RTPS 端点状态损坏
- **修复**：disconnect 时始终销毁 participant，reconnect 时重建

### 3.4 Vehicle 重复创建（commit `d71cb5960`）
- **问题**：重连后 DDSVehicleManager 没有按 namespace 查找已有 Vehicle
- **修复**：按 namespace 查找，存在则复用

### 3.5 cmd 512 无限重试（commit `d71cb5960`）
- **问题**：GimbalController 不停发送 MAV_CMD_REQUEST_MESSAGE(280)，PX4 DDS 不支持
- **修复**：DDSCommandPublisher 过滤 cmd 512

## 4. 未解决的核心问题

### 多机命令投递失败

**现象**：
- px4_1（PX4 instance 1）的命令始终正常收到 ACK
- px4_2、px4_3（instance 2、3）的命令 `matched=1` 但无 ACK → 数据被静默丢弃
- 无论是单独连接还是同时连接 px4_2/px4_3，都无法起飞

**根因分析**：

通过对比 HeartbeatPublisher 和 CommandPublisher：

| 对比项 | HeartbeatPublisher | CommandPublisher |
|--------|-------------------|------------------|
| QoS | BEST_EFFORT + VOLATILE | 多次尝试：RELIABLE/BEST_EFFORT |
| 多机结果 | ✅ 3 台全部正常 | ❌ 只有 px4_1 正常 |

**结论**：XRCE-DDS Agent 的内置 RTPS 栈在处理 RELIABLE writer → BEST_EFFORT reader 匹配时，只能正确路由到第一个 reader。

最终提交 `12b59597b` 将 CommandPublisher 改为 BEST_EFFORT（与 HeartbeatPublisher 完全一致），但**尚未经过用户测试验证**。

### 可能的进一步方案

如果 BEST_EFFORT 仍然不能解决 px4_2/px4_3 的命令投递：

1. **DDS Service 模式**：使用 `rq/{ns}/fmu/VehicleCommandRequest` 服务请求 topic 替代普通 topic
2. **分离 Agent**：每个 PX4 实例使用独立的 XRCE-DDS Agent
3. **升级 Agent**：使用 FastDDS 后端编译的 Agent 替代内置 RTPS

## 5. 架构

### 5.1 新增类

| 类 | 职责 |
|----|------|
| DDSDiscovery | 加入 DDS domain，通过 DCPSPublication builtin topic 发现 namespace |
| DDSVehicleManager | 按 namespace 创建/复用 Vehicle，绑定 DataInjector 和 CommandPublisher |

### 5.2 修改类

| 类 | 修改内容 |
|----|---------|
| DDSConfiguration | 新增 discoveredNamespaces、discovering 属性，participant 管理 |
| DDSLink | 共享 participant 池（acquireSharedParticipant/releaseSharedParticipant） |
| DDSCommandPublisher | QoS 调整、writer 生命周期管理 |
| DDSSettings.qml | Domain ID 输入、扫描按钮、namespace ComboBox |
| MultiVehicleSelector.qml | customName 显示、Vehicle 对象直接引用 |

## 6. 测试记录

共收到用户 10 次测试日志（#15 ~ #24），覆盖了以下场景：

| 日志 | 关键发现 |
|------|---------|
| #15 | 命令无 ACK → participant 未正确转移 |
| #16 | 连接后自动断开 → namespace 为空 |
| #17 | 切换无人机断开 → 字符串解析 crash |
| #18 | 起飞成功但重连后失败 → participant 复用 bug |
| #19 | 首次起飞成功，后续失败 → writer 匹配丢失 |
| #20 | 独立 participant：px4_1 成功，px4_2/3 失败 |
| #21 | 共享 participant：HeartbeatPublisher 全部成功，CommandPublisher 只有 px4_1 |
| #22 | BEST_EFFORT QoS：px4_1 成功（ACK=0） |
| #23 | 单独连接 px4_2/px4_3：均失败 |
| #24 | RELIABLE + 单活跃 writer：仍然只有 px4_1 成功 |
