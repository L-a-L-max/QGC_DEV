# DDS 版本适配与跨实现兼容性分析

> 文档版本：1.0  
> 适用分支：DDS_P2  
> 最后更新：2026-05-28

---

## 目录

1. [背景与问题描述](#1-背景与问题描述)
2. [DDS 实现对比](#2-dds-实现对比)
3. [PX4 DDS 类型命名规范](#3-px4-dds-类型命名规范)
4. [RTPS 端点匹配机制详解](#4-rtps-端点匹配机制详解)
5. [跨实现兼容性问题](#5-跨实现兼容性问题)
6. [当前解决方案](#6-当前解决方案)
7. [替代方案对比](#7-替代方案对比)
8. [PX4 版本与消息格式变更](#8-px4-版本与消息格式变更)
9. [QoS 兼容性矩阵](#9-qos-兼容性矩阵)
10. [诊断与排错指南](#10-诊断与排错指南)
11. [建议与路线图](#11-建议与路线图)

---

## 1. 背景与问题描述

### 1.1 系统架构

```
┌─────────────────────┐         RTPS/UDP          ┌─────────────────────┐
│   电脑 B (SITL)      │  ◄──────────────────────►  │   电脑 A (GCS)       │
│                     │                           │                     │
│  PX4 Autopilot      │                           │  QGroundControl     │
│       ↕              │                           │       ↕              │
│  MicroXRCE-DDS      │                           │  CycloneDDS         │
│  Client (uORB→DDS)  │                           │  (typed readers)    │
│       ↕              │                           │                     │
│  MicroXRCE-DDS      │                           │                     │
│  Agent (FastDDS)     │                           │                     │
└─────────────────────┘                           └─────────────────────┘
```

### 1.2 核心问题

QGC 使用 **CycloneDDS** 创建 typed reader 订阅 PX4 话题，PX4 的 MicroXRCE-DDS Agent 使用 **FastDDS** 发布数据。两个不同的 DDS 实现虽然都遵循 RTPS 标准协议，但在以下方面存在差异导致数据无法接收：

| 问题层级 | 具体表现 | 影响 |
|---------|---------|------|
| 类型名格式 | PX4 使用 `px4_msgs::msg::dds_::TypeName_`，我们曾使用 `px4_msgs::msg::TypeName` | RTPS 端点完全无法匹配 |
| XTypes 类型对象 | CycloneDDS 和 FastDDS 生成不同的 TypeObject hash | 即使类型名一致仍拒绝匹配 |
| QoS 策略 | PX4 使用 BEST_EFFORT + TRANSIENT_LOCAL | RELIABLE reader 无法从 BEST_EFFORT writer 接收 |

---

## 2. DDS 实现对比

### 2.1 主流 DDS 实现

| 特性 | CycloneDDS | FastDDS (eProsima) | RTI Connext | Gurum DDS |
|------|-----------|-------------------|-------------|-----------|
| 开源许可 | EPL 2.0 / EDL 1.0 | Apache 2.0 | 商业（有免费版） | 商业 |
| 语言 | C（核心）+ C++/Python 绑定 | C++ | C/C++/Java/.NET | C/C++ |
| 库体积 | ~1 MB | ~10 MB | ~50 MB | ~5 MB |
| ROS2 支持 | rmw_cyclonedds | rmw_fastrtps（默认） | rmw_connextdds | rmw_gurumdds |
| PX4 Agent 支持 | ✓（需重编译） | ✓（默认） | ✗ | ✗ |
| IDL 编译器 | idlc | fastddsgen | rtiddsgen | — |
| XTypes 支持 | 完整（v0.10+） | 完整 | 完整 | 部分 |
| CDR 编码 | 标准 CDR | 标准 CDR | 标准 CDR | 标准 CDR |

### 2.2 CycloneDDS 版本历史（与兼容性相关）

| 版本 | 关键变化 | 对我们的影响 |
|------|---------|------------|
| 0.7.x | 基础 RTPS 实现 | 无 XTypes 支持，仅按类型名匹配 |
| 0.8.x | 引入 `dds_create_domain()` API | 可以编程方式配置 domain |
| 0.9.x | 初步 XTypes 支持 | TypeObject 开始参与匹配决策 |
| 0.10.x | 完整 XTypes 支持 | TypeObject hash 不匹配会阻止端点匹配 |
| **11.0.x**（当前） | XTypes 默认启用 | **必须**处理跨实现 TypeObject 差异 |

### 2.3 FastDDS 版本历史（PX4 Agent 使用）

| PX4 版本 | FastDDS 版本 | MicroXRCE-DDS 版本 | 关键变化 |
|---------|-------------|-------------------|---------|
| v1.14 | 2.6.x | 2.2.x | 基础 DDS 支持 |
| v1.15 | 2.10.x | 2.3.x | 改进的 QoS 支持 |
| v1.16 | 2.12.x | 2.4.x | XTypes 增强 |
| **v1.17**（当前） | 2.14.x+ | 2.4.x+ | 完整 XTypes，topic 名称加 `_v1` 后缀 |

---

## 3. PX4 DDS 类型命名规范

### 3.1 类型名生成规则

PX4 通过 `Tools/msg/generate_dds_topics.py` 生成 DDS 话题配置。关键代码（第 100-101 行）：

```python
# ROS2 message type: px4_msgs::msg::VehicleStatus
# DDS wire type:     px4_msgs::msg::dds_::VehicleStatus_
msg_type['dds_type'] = msg_type['type'].replace("::msg::", "::msg::dds_::") + "_"
```

### 3.2 命名格式对照

| 层级 | 格式 | 示例 |
|------|------|------|
| PX4 uORB | snake_case | `vehicle_status` |
| ROS2 消息类型 | `pkg/msg/Type` | `px4_msgs/msg/VehicleStatus` |
| ROS2 内部类型名 | `pkg::msg::Type` | `px4_msgs::msg::VehicleStatus` |
| **DDS 网络类型名** | `pkg::msg::dds_::Type_` | `px4_msgs::msg::dds_::VehicleStatus_` |
| CycloneDDS C 符号 | `pkg_msg_dds__Type_` | `px4_msgs_msg_dds__VehicleStatus_` |
| CycloneDDS 描述符 | `pkg_msg_dds__Type__desc` | `px4_msgs_msg_dds__VehicleStatus__desc` |

### 3.3 话题名格式

| 层级 | 格式 | 示例 |
|------|------|------|
| PX4 uORB | 无前缀 | `vehicle_status` |
| DDS Agent 发布 | `rt/fmu/out/` 前缀 | `rt/fmu/out/vehicle_status` |
| PX4 v1.17+ | 部分话题加版本后缀 | `rt/fmu/out/vehicle_status_v1` |
| `ros2 topic list` 显示 | 不含 `rt/` 前缀 | `/fmu/out/vehicle_status_v1` |

### 3.4 PX4 v1.17 带版本后缀的话题

PX4 v1.17 开始，部分消息格式有破坏性变更的话题添加了 `_v1` 后缀：

| 话题 | 原名 | v1.17 话题名 |
|------|------|------------|
| vehicle_status | `/fmu/out/vehicle_status` | `/fmu/out/vehicle_status_v1` |
| battery_status | `/fmu/out/battery_status` | `/fmu/out/battery_status_v1` |
| vehicle_local_position | `/fmu/out/vehicle_local_position` | `/fmu/out/vehicle_local_position_v1` |
| home_position | `/fmu/out/home_position` | `/fmu/out/home_position_v1` |
| airspeed_validated | `/fmu/out/airspeed_validated` | `/fmu/out/airspeed_validated_v1` |

其余 15 个话题保持原名不变。

---

## 4. RTPS 端点匹配机制详解

### 4.1 匹配流程

RTPS（Real-Time Publish-Subscribe）协议的端点匹配分为三个阶段：

```
阶段 1: 参与者发现 (SPDP)
  ├─ 通过组播 239.255.0.1:7400 发现网络上的其他参与者
  └─ 交换 ParticipantBuiltinTopicData

阶段 2: 端点发现 (SEDP)
  ├─ 通过已发现的参与者交换 WriterBuiltinTopicData / ReaderBuiltinTopicData
  ├─ 包含: topic_name, type_name, QoS, TypeObject/TypeIdentifier
  └─ 双向匹配: reader 发现 writer，writer 发现 reader

阶段 3: 兼容性检查
  ├─ 话题名匹配 (必须完全一致)
  ├─ 类型名匹配 (必须完全一致)
  ├─ QoS 兼容性检查 (见 §9)
  └─ XTypes 类型兼容性检查 (TypeObject hash 比对)
```

### 4.2 匹配失败的三种情况

| 失败原因 | 表现 | 诊断方法 |
|---------|------|---------|
| 话题名不匹配 | `dds_get_matched_publications()` 返回 0 | 检查 `ros2 topic list` vs QGC reader 的话题名 |
| 类型名不匹配 | 同上 | 检查 `ros2 topic info --verbose` 的 Type 字段 |
| QoS 不兼容 | 同上 | 对比 writer/reader 的 Reliability/Durability |
| XTypes 不兼容 | 类型名一致但仍返回 0 | 需要 DDS tracing 日志确认 |

### 4.3 XTypes 类型兼容性

XTypes（Extensible Types）是 DDS 规范的类型系统扩展。核心概念：

- **TypeObject**：完整的类型定义（包含所有字段名、类型、顺序）
- **TypeIdentifier**：TypeObject 的 hash（14 字节）
- **类型兼容性**：两个 TypeIdentifier 相同 → 类型兼容

**跨 DDS 实现的 TypeObject 差异来源：**

```
相同的 IDL:
  struct VehicleStatus_ {
      unsigned long long timestamp;
      octet arming_state;
  };

CycloneDDS idlc 生成:
  TypeObject hash = 0x7A3F...（基于 CycloneDDS 内部算法）

FastDDS fastddsgen 生成:
  TypeObject hash = 0xB2E1...（基于 FastDDS 内部算法）

结果: hash 不同 → XTypes 检查失败 → 端点不匹配
```

即使 IDL 定义**完全相同**，不同 DDS 实现的 TypeObject 序列化方式、hash 算法可能存在差异，导致 hash 不一致。

---

## 5. 跨实现兼容性问题

### 5.1 已遇到的问题及解决方案

| # | 问题 | 根因 | 解决方案 | commit |
|---|------|------|---------|--------|
| 1 | Reader 收不到数据 | QoS 不匹配：RELIABLE reader vs BEST_EFFORT writer | 设置 reader QoS 为 BEST_EFFORT | `37191c18c` |
| 2 | RTPS 端点不匹配 | 类型名格式错误：缺少 `dds_::` 命名空间和 `_` 后缀 | 更新所有 IDL 文件的 module 结构 | `98b4c8d6f` |
| 3 | 类型名一致但仍不匹配 | XTypes TypeObject hash 跨实现不兼容 | `idlc -t` 禁用 TypeObject + domain 兼容配置 | `59788d411` |
| 4 | Durability QoS 不匹配 | PX4 使用 TRANSIENT_LOCAL，默认 VOLATILE | 设置 reader QoS 为 TRANSIENT_LOCAL | `59788d411` |

### 5.2 潜在风险评估

#### 风险 1：序列化兼容性

**风险等级：低（当前）/ 中（未来）**

CycloneDDS 和 FastDDS 都使用标准 CDR（Common Data Representation）序列化。对于基本类型：

| 数据类型 | CDR 大小 | 兼容性 |
|---------|---------|--------|
| `octet` (uint8) | 1 byte | 完全兼容 |
| `unsigned long` (uint32) | 4 bytes | 完全兼容 |
| `unsigned long long` (uint64) | 8 bytes | 完全兼容 |
| `float` | 4 bytes | 完全兼容 |
| `double` | 8 bytes | 完全兼容 |
| `boolean` | 1 byte | 完全兼容 |
| `float[4]`（固定数组） | 16 bytes | 完全兼容 |
| `string`（可变长） | 4+N bytes | **需验证** |
| 嵌套 struct | 递归 CDR | **需验证** |

PX4 当前 20 个订阅话题**全部使用基本类型和固定数组**，不含 string 或嵌套结构体，因此序列化兼容性**完全安全**。

**未来风险场景：** 如果 PX4 新增的消息使用了 `string` 类型（如 `callsign` 字段目前是 `char[9]` 固定数组，未来可能改为 `string`），需要验证跨实现的 string CDR 编码一致性。

#### 风险 2：XTypes 检查被禁用

**风险等级：中**

禁用 XTypes 意味着失去了**编译时类型安全**。如果 PX4 更新后消息字段变化：

```
PX4 v1.17:                      PX4 v1.18 (假设):
struct VehicleStatus_ {          struct VehicleStatus_ {
    uint64 timestamp;                uint64 timestamp;
    uint8 arming_state;              uint8 arming_state;
    ...                              uint8 new_field;    ← 新增
    uint8 vehicle_type;              ...
};                                   uint8 vehicle_type;
                                 };
```

正常情况下 XTypes 会阻止不兼容的 reader 连接。禁用后，reader 会用**旧的字段偏移量**读取**新格式的数据**，导致：
- 字段值错位（如 `vehicle_type` 读到的是 `new_field` 的值）
- 不会报错，静默产生错误数据

**缓解措施：**
- 每次 PX4 版本更新时，对照 `px4_msgs` 更新 IDL 文件
- 在 `_default.json` 中标注 `px4_msgs` 版本号
- 未来可添加 timestamp 合理性检查作为运行时校验

#### 风险 3：发现协议稳定性

**风险等级：低**

CycloneDDS 和 FastDDS 的 SPDP/SEDP 实现都遵循 RTPS 2.2+ 规范，互操作性在以下场景下经过验证：
- ROS2 多 rmw 实现混合使用
- DDS Interoperability Testing（OMG 官方互操作性测试）

可能出现不稳定的极端场景：
- **多网卡环境**：CycloneDDS 默认绑定所有网卡，FastDDS 默认绑定第一个非回环网卡
- **防火墙/NAT**：RTPS 使用 UDP 组播，某些网络环境下组播可能被阻断
- **大量参与者**：超过 120 个 DDS 参与者时，SPDP 端口分配可能冲突

---

## 6. 当前解决方案

### 6.1 方案概述

```
                       QGC (CycloneDDS)
                            │
                  ┌─────────┼─────────┐
                  │         │         │
              idlc -t    domain     QoS
              (无TypeObj) (兼容配置)  (匹配PX4)
                  │         │         │
                  └─────────┼─────────┘
                            │
                    RTPS 端点匹配
                    (仅按类型名)
                            │
                       FastDDS (PX4)
```

### 6.2 三层修复

#### 第一层：类型名修正

IDL 文件使用正确的 module 结构：

```idl
// 修复前
module px4_msgs {
module msg {
struct VehicleStatus { ... };
}; // msg
}; // px4_msgs
// → 生成类型名: px4_msgs::msg::VehicleStatus ✗

// 修复后
module px4_msgs {
module msg {
module dds_ {
struct VehicleStatus_ { ... };
}; // dds_
}; // msg
}; // px4_msgs
// → 生成类型名: px4_msgs::msg::dds_::VehicleStatus_ ✓
```

#### 第二层：禁用 XTypes 类型信息

CMake 编译 IDL 时添加 `-t` 标志：

```cmake
# src/Comms/CMakeLists.txt
COMMAND ${IDLC_EXECUTABLE} -l c -t -o ${DDS_IDL_GEN_DIR} ${idl_file}
#                             ^^
#                        禁止生成 TypeObject/TypeIdentifier
```

创建 CycloneDDS domain 时启用兼容模式：

```cpp
// DDSLink.cc _createParticipant()
static const char *cycloneConfig =
    "<CycloneDDS>"
    "  <Domain id=\"any\">"
    "    <Compatibility>"
    "      <AssumeRtiHasTopicDiscovery>best-effort</AssumeRtiHasTopicDiscovery>"
    "    </Compatibility>"
    "  </Domain>"
    "</CycloneDDS>";
dds_create_domain(domainId, cycloneConfig);
```

#### 第三层：QoS 匹配

```cpp
// DDSLink.cc _subscribeToTopics()
dds_qos_t *qos = dds_create_qos();
dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);    // 匹配 PX4
dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);       // 匹配 PX4
dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);
```

---

## 7. 替代方案对比

### 7.1 方案 A：QGC 改用 FastDDS

**原理：** QGC 使用与 PX4 Agent 相同的 DDS 实现，消除跨实现差异。

**改动范围：**

| 组件 | 改动 |
|------|------|
| CMake | 替换 `FindCycloneDDS.cmake` → `FindFastDDS.cmake` |
| DDSLink | `dds_*()` C API → FastDDS C++ API（`DomainParticipant`, `DataReader`） |
| IDL 编译 | `idlc` → `fastddsgen` |
| DDSTypeRegistry | 重写，使用 FastDDS TypeSupport |
| 依赖 | CycloneDDS (~1MB) → FastDDS (~10MB) + 依赖库 |

**优点：**
- XTypes 完全兼容，无需 workaround
- 序列化 100% 一致
- 可直接使用 ROS2 `px4_msgs` 包的 TypeSupport
- FastDDS 在 PX4/ROS2 生态中测试更充分

**缺点：**
- 依赖体积增大 10 倍
- FastDDS C++ API 比 CycloneDDS C API 复杂
- 交叉编译（嵌入式 GCS）难度增大（依赖 foonathan_memory, tinyxml2, asio 等）
- 需要全量重写 DDS 通信层

**工作量估计：** 3-5 天

### 7.2 方案 B：PX4 Agent 改用 CycloneDDS

**原理：** 在 SITL 机器上重新编译 MicroXRCE-DDS Agent，使用 CycloneDDS 作为后端。

**操作步骤：**

```bash
# 在电脑 B (SITL 机器) 上
git clone https://github.com/eProsima/Micro-XRCE-DDS-Agent.git
cd Micro-XRCE-DDS-Agent
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/usr/local \
         -DUXR_AGENT_MIDDLEWARE=CED      # CycloneDDS 后端
make -j$(nproc)
sudo make install
```

**优点：**
- QGC 代码不需要任何改动
- 类型系统天然一致
- CycloneDDS 轻量，Agent 资源占用更少

**缺点：**
- PX4 社区默认 FastDDS，CycloneDDS 后端测试较少
- ROS2 工具（`ros2 topic echo` 等）仍使用 FastDDS，可能出现同样的兼容性问题
- 每次 PX4 更新需要重新编译自定义 Agent
- 非标准配置，社区支持和文档较少

**工作量估计：** 1 天（编译），持续维护成本中

### 7.3 方案 C：QGC 使用 ROS2 中间件层

**原理：** QGC 通过 `rclcpp`（ROS2 客户端库）+ `rmw`（中间件抽象层）通信，不直接使用任何 DDS API。

**架构变化：**

```
当前:  QGC → CycloneDDS API → RTPS → FastDDS (PX4)
方案C: QGC → rclcpp → rmw_fastrtps → FastDDS → RTPS → FastDDS (PX4)
```

**优点：**
- 完全不关心底层 DDS 实现
- 直接使用 `px4_msgs` ROS2 包，无需手写 IDL
- ROS2 QoS 匹配策略成熟，自动处理兼容性
- ROS2 生态工具链完整（`ros2 topic echo` 直接可用）

**缺点：**
- QGC 必须依赖 ROS2（QGC 社区一直刻意避免此依赖）
- 用户部署环境必须安装 ROS2
- ROS2 版本（Humble/Jazzy/Rolling）与 PX4 版本绑定
- 增大 QGC 构建复杂度和发布包体积
- 与 QGC 上游合并困难（上游不会接受 ROS2 依赖）

**工作量估计：** 5-10 天

### 7.4 方案对比矩阵

| 维度 | 当前方案 | A: QGC→FastDDS | B: Agent→CycloneDDS | C: QGC→ROS2 |
|------|---------|----------------|---------------------|-------------|
| **短期可用性** | ★★★★★ | ★★ | ★★★★ | ★ |
| **长期维护成本** | ★★★ | ★★★★★ | ★★★ | ★★★★ |
| **类型安全性** | ★★ | ★★★★★ | ★★★★★ | ★★★★★ |
| **部署简易度** | ★★★★★ | ★★★★ | ★★★ | ★★ |
| **跨平台兼容** | ★★★★★ | ★★★ | ★★★★ | ★★ |
| **社区支持** | ★★★ | ★★★★★ | ★★ | ★★★★ |
| **改动工作量** | 已完成 | 3-5 天 | 1 天 | 5-10 天 |

---

## 8. PX4 版本与消息格式变更

### 8.1 PX4 消息版本追踪

PX4 的消息定义位于 `PX4-Autopilot/msg/` 和 `px4_msgs` ROS2 包中。版本之间可能的变化：

| 变化类型 | 对 QGC 的影响 | 需要的操作 |
|---------|-------------|-----------|
| 新增话题 | 无影响（未映射的话题被忽略） | 更新 `_default.json` 添加新映射 |
| 删除话题 | reader 创建成功但无数据 | 从 `_default.json` 移除映射 |
| 话题改名/加后缀 | reader 话题名不匹配 | 更新 `_default.json` 的 `dds_topic` |
| 字段新增（末尾） | CDR 解析安全（多余字段被忽略） | 更新 IDL 可获取新字段 |
| 字段删除 | **CDR 偏移错位，数据错误** | **必须更新 IDL** |
| 字段类型修改 | **CDR 大小变化，数据错误** | **必须更新 IDL** |
| 字段重排序 | **CDR 偏移错位，数据错误** | **必须更新 IDL** |

### 8.2 版本适配流程

当需要适配新 PX4 版本时：

```bash
# 1. 获取新版本的消息定义
cd ~/PX4-Autopilot
git checkout v1.18.0  # 目标版本
ls msg/

# 2. 对比消息变化
diff msg/VehicleStatus.msg ~/qgc_dev/src/DDS/idl/VehicleStatus.idl

# 3. 检查话题名变化
cat src/modules/uxrce_dds_client/dds_topics.yaml

# 4. 更新 IDL 文件
#    - 修改字段定义
#    - 保持 module dds_ { struct TypeName_ { ... }; }; 结构

# 5. 更新 _default.json
#    - 修改 dds_topic 字段（如话题名变化）
#    - 修改 fields 映射（如字段名/类型变化）

# 6. 重新编译 QGC
rm -rf build && mkdir build && cd build
cmake .. -G Ninja ... -DQGC_ENABLE_DDS=ON
ninja -j$(nproc)
```

### 8.3 多版本兼容策略

QGC 的配置驱动设计支持多版本：

```
resources/dds_mappings/
├── _default.json              ← PX4 v1.17 默认配置（编译嵌入）
├── px4_v1.16.json             ← PX4 v1.16 配置（预置）
├── px4_v1.18.json             ← PX4 v1.18 配置（未来添加）
└── px4_custom_example.json    ← 用户自定义模板

用户自定义覆盖目录:
~/.config/QGroundControl/dds_mappings/
└── _default.json              ← 用户可用任意版本配置覆盖默认
```

**话题名和字段映射**由配置文件控制，无需改代码。但 **IDL 文件**（字段类型定义）必须与目标 PX4 版本匹配，否则 CDR 反序列化会出错。

---

## 9. QoS 兼容性矩阵

### 9.1 DDS QoS 匹配规则

DDS 的 QoS "请求/提供"（Request/Offered）模型规定了 reader 和 writer 的 QoS 兼容条件：

#### Reliability

| Writer ↓ / Reader → | BEST_EFFORT | RELIABLE |
|---------------------|-------------|----------|
| **BEST_EFFORT** | ✓ 兼容 | ✗ 不兼容 |
| **RELIABLE** | ✓ 兼容 | ✓ 兼容 |

**PX4 使用 BEST_EFFORT → QGC reader 必须使用 BEST_EFFORT**

#### Durability

| Writer ↓ / Reader → | VOLATILE | TRANSIENT_LOCAL |
|---------------------|----------|-----------------|
| **VOLATILE** | ✓ | ✗ |
| **TRANSIENT_LOCAL** | ✓ | ✓ |
| **TRANSIENT** | ✓ | ✓ |
| **PERSISTENT** | ✓ | ✓ |

**PX4 使用 TRANSIENT_LOCAL → QGC reader 可以使用 VOLATILE 或 TRANSIENT_LOCAL**

### 9.2 PX4 v1.17 发布者 QoS

通过 `ros2 topic info --verbose` 确认的 QoS 配置：

```
QoS profile:
  Reliability: BEST_EFFORT
  History (Depth): UNKNOWN
  Durability: TRANSIENT_LOCAL
  Lifespan: Infinite
  Deadline: Infinite
  Liveliness: AUTOMATIC
  Liveliness lease duration: Infinite
```

### 9.3 QGC Reader QoS 配置

```cpp
dds_qos_t *qos = dds_create_qos();
dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);       // 匹配 PX4
dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);          // 匹配 PX4
dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);                  // 只保留最新样本
```

---

## 10. 诊断与排错指南

### 10.1 诊断工具

#### ROS2 命令行

```bash
# 查看所有话题
ros2 topic list

# 查看话题详情（类型名 + QoS）
ros2 topic info /fmu/out/vehicle_status_v1 --verbose

# 实时查看数据
ros2 topic echo /fmu/out/vehicle_status_v1

# 查看话题频率
ros2 topic hz /fmu/out/vehicle_status_v1
```

#### QGC 内置诊断

QGC 在连接后 5 秒和 15 秒自动输出 RTPS 匹配诊断：

```
# 5 秒：汇总
[DDSLink] RTPS match diagnostic (5s): 20 readers matched a writer, 0 unmatched

# 15 秒：逐话题
[DDSLink] MATCHED: "/fmu/out/vehicle_attitude" → 1 writer(s)
[DDSLink] UNMATCHED: "/fmu/out/wind"

# 首次收到数据
[DDSLink] First data received from "/fmu/out/vehicle_attitude" fields: 5
```

#### CycloneDDS Discovery 追踪

通过环境变量启用 CycloneDDS 底层发现追踪：

```bash
# 方法1：环境变量
export CYCLONEDDS_URI='<CycloneDDS><Domain><Tracing><Category>discovery</Category><OutputFile>stderr</OutputFile><Verbosity>finest</Verbosity></Tracing></Domain></CycloneDDS>'
./QGroundControl 2>&1 | tee /tmp/cyclone_trace.log

# 方法2：QGC 内置（已集成 warning 级别追踪到 DDSLink）
./QGroundControl 2>&1 | tee /tmp/qgc_log.txt
```

### 10.2 常见问题排查表

| 现象 | 可能原因 | 排查方法 | 解决方案 |
|------|---------|---------|---------|
| "0 readers matched" | 类型名不匹配 | `ros2 topic info --verbose` 检查 Type 字段 | 更新 IDL module 结构 |
| "0 readers matched" | XTypes 不兼容 | 确认 `idlc -t` 已启用 | `rm -rf build` 重新编译 |
| "0 readers matched" | 网络不可达 | `ping` 对方机器，检查防火墙 | 开放 UDP 7400-7500 端口 |
| "20 matched" 但无数据 | QoS 不兼容 | 对比 writer/reader QoS | 设置 BEST_EFFORT + TRANSIENT_LOCAL |
| "20 matched" 但无数据 | 话题名不匹配 | 比对 `ros2 topic list` 与配置文件 | 更新 `_default.json` 的 `dds_topic` |
| 数据值异常 | IDL 字段不匹配 PX4 版本 | 对比 `px4_msgs` 消息定义 | 更新 IDL 文件字段 |
| 间歇性数据丢失 | BEST_EFFORT 正常行为 | 检查网络丢包率 | 正常现象，非错误 |
| reader 创建失败 | CycloneDDS 版本过低 | `idlc -v` 检查版本 | 升级 CycloneDDS ≥ 0.10 |

### 10.3 网络诊断

```bash
# 检查组播是否可用（在两台机器上分别运行）
# 机器 A（接收端）
python3 -c "
import socket, struct
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('', 7400))
group = socket.inet_aton('239.255.0.1')
mreq = struct.pack('4sL', group, socket.INADDR_ANY)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
print('Listening on 239.255.0.1:7400...')
data, addr = sock.recvfrom(1024)
print(f'Received {len(data)} bytes from {addr}')
"

# 机器 B（发送端）
python3 -c "
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 32)
sock.sendto(b'test', ('239.255.0.1', 7400))
print('Sent multicast packet')
"
```

---

## 11. 建议与路线图

### 11.1 短期（P2 阶段）

**策略：使用当前方案（CycloneDDS + XTypes 禁用）完成功能验证**

- [x] 修复类型名格式（`dds_::TypeName_`）
- [x] 禁用 XTypes 类型信息（`idlc -t`）
- [x] 匹配 PX4 QoS（BEST_EFFORT + TRANSIENT_LOCAL）
- [ ] 验证 20 个话题数据接收
- [ ] 验证 Vehicle 自动创建
- [ ] 验证遥测数据注入到 QGC UI

### 11.2 中期（P3 阶段）

**策略：加固跨实现兼容性**

- 添加运行时类型校验（timestamp 范围检查、字段值合理性检查）
- 添加 PX4 版本自动检测（通过 `vehicle_status` 消息的已知字段模式）
- 支持多个 `_default.json` 预置配置，按 PX4 版本选择
- 编写 IDL 自动生成脚本（从 `px4_msgs/*.msg` → `*.idl`）

### 11.3 长期

**策略：评估是否切换 DDS 实现**

| 条件 | 决策 |
|------|------|
| 当前方案稳定运行 | 继续使用 CycloneDDS，保持轻量 |
| 遇到序列化兼容性问题 | 考虑方案 A（QGC 切换到 FastDDS） |
| PX4 Agent 提供 CycloneDDS 官方支持 | 考虑方案 B（Agent 切换） |
| QGC 社区决定集成 ROS2 | 方案 C（ROS2 层）|

### 11.4 IDL 自动生成（推荐未来实现）

手动维护 IDL 文件容易与 PX4 版本脱节。建议编写自动化脚本：

```bash
# 未来: 从 px4_msgs 自动生成 IDL
python3 tools/generate_idl.py \
    --px4-msgs ~/PX4-Autopilot/msg/ \
    --output src/DDS/idl/ \
    --topics resources/dds_mappings/_default.json
```

脚本逻辑：
1. 读取 `_default.json` 中的话题列表
2. 解析对应的 `.msg` 文件
3. 转换为 CycloneDDS IDL 格式（含 `module dds_` 和 `struct TypeName_`）
4. 写入 `src/DDS/idl/` 目录

这样每次 PX4 版本更新只需重新运行脚本，无需手动编辑 IDL。

---

## 附录 A：参考资料

| 资料 | 链接 |
|------|------|
| DDS 规范 (OMG) | https://www.omg.org/spec/DDS/ |
| RTPS 规范 (OMG) | https://www.omg.org/spec/DDSI-RTPS/ |
| XTypes 规范 (OMG) | https://www.omg.org/spec/DDS-XTypes/ |
| CycloneDDS 文档 | https://cyclonedds.io/docs/ |
| FastDDS 文档 | https://fast-dds.docs.eprosima.com/ |
| PX4 DDS 文档 | https://docs.px4.io/main/en/middleware/uxrce_dds.html |
| PX4 px4_msgs | https://github.com/PX4/px4_msgs |
| MicroXRCE-DDS Agent | https://github.com/eProsima/Micro-XRCE-DDS-Agent |

## 附录 B：术语表

| 术语 | 全称 | 说明 |
|------|------|------|
| CDR | Common Data Representation | DDS 标准序列化格式 |
| RTPS | Real-Time Publish-Subscribe | DDS 的网络传输协议 |
| SPDP | Simple Participant Discovery Protocol | 参与者发现协议 |
| SEDP | Simple Endpoint Discovery Protocol | 端点（reader/writer）发现协议 |
| XTypes | Extensible Types | DDS 类型系统扩展规范 |
| TypeObject | — | 完整的类型定义描述 |
| TypeIdentifier | — | TypeObject 的 hash 标识 |
| QoS | Quality of Service | 服务质量策略 |
| rmw | ROS Middleware | ROS2 中间件抽象层 |
| IDL | Interface Definition Language | 接口定义语言 |
| idlc | IDL Compiler (CycloneDDS) | CycloneDDS 的 IDL 编译器 |
