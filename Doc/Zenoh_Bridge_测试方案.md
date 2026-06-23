# Zenoh Bridge 测试方案

## 测试目标

1. 验证 Zenoh bridge 子进程能正确启动和停止
2. 验证安卓端能通过 Zenoh 接收机载端 DDS 数据
3. 验证安卓端能通过 Zenoh 向机载端发送控制指令
4. 验证不开启 Zenoh bridge 时，原有功能不受影响

## 测试环境

### 硬件

| 设备 | 角色 | 网络 |
|------|------|------|
| Ubuntu 机器 | 机载端模拟（PX4 SITL + zenoh-bridge-dds） | 与安卓同一网段 |
| 安卓手机/遥控器 | 运行 QGC APK | 与机载端同一网段 |

### 软件

| 组件 | 版本 | 说明 |
|------|------|------|
| PX4 SITL | v1.17 | 软件在环仿真 |
| MicroXRCE-DDS Agent | 最新 | PX4 → DDS |
| zenoh-bridge-dds | 1.1.x | 机载端：DDS → Zenoh |
| QGC APK (DDS_zenoh) | 最新 | 安卓端 |

## 测试一：环境搭建

### 1.1 机载端启动

```bash
# 终端 1: 启动 PX4 SITL
cd ~/PX4-Autopilot
make px4_sitl gazebo-classic

# 终端 2: 启动 MicroXRCE-DDS Agent
MicroXRCEAgent udp4 -p 8888

# 终端 3: 启动 zenoh-bridge-dds
zenoh-bridge-dds -m peer -l tcp/0.0.0.0:7447 -d 0

# 终端 4: 验证 DDS 话题
ros2 topic list | grep fmu
ros2 topic hz /fmu/out/vehicle_status_v1
```

### 1.2 网络验证

```bash
# 在安卓设备上（通过 adb shell）
ping <机载端IP>
# 应 100% 可达

# 在机载端
ping <安卓端IP>
# 应 100% 可达
```

## 测试二：Zenoh Bridge 功能测试

### 2.1 基本连接测试

| 步骤 | 操作 | 预期结果 |
|------|------|---------|
| 1 | 打开 QGC，添加 DDS 连接 | 弹出 DDS 设置界面 |
| 2 | 开启 "Enable Zenoh Bridge" | 显示 "Remote Endpoint" 输入框 |
| 3 | 输入 `tcp/<机载端IP>:7447` | 输入框接受格式 |
| 4 | 选择 DDS Profile: PX4 SITL v1.17 | Profile 选中 |
| 5 | 点击连接 | logcat 显示 bridge 启动成功 |
| 6 | 等待 5-10 秒 | QGC 显示飞行器图标，姿态数据更新 |

**logcat 检查点**：
```
[ZenohBridge] Started, endpoint: tcp/x.x.x.x:7447
[DDSLink] CycloneDDS configured for localhost (Zenoh bridge mode)
[DDSLink] DDS link connected on domain 0
[DDSLink] MATCHED: /fmu/out/vehicle_status_v1 → 1 writer(s)
```

### 2.2 数据接收测试

| 测试项 | 验证方法 | 通过标准 |
|--------|---------|---------|
| vehicle_status | QGC 显示 Armed/Disarmed 状态 | 状态与 SITL 一致 |
| vehicle_global_position | QGC 地图上显示飞行器位置 | 经纬度正确 |
| vehicle_attitude | QGC 姿态球更新 | 俯仰/横滚/偏航角正确 |
| battery_status | QGC 显示电池电压/百分比 | 数值合理 |

### 2.3 控制指令测试

| 测试项 | 操作 | 预期结果 |
|--------|------|---------|
| Arm | QGC 中点击 Arm | SITL 飞行器 armed |
| 起飞 | 发送 Takeoff 命令 | SITL 飞行器起飞 |
| 航点规划 | 添加航点并开始任务 | 飞行器按航点飞行 |
| 手柄控制 | 虚拟摇杆/物理手柄 | 飞行器响应控制输入 |
| Disarm | QGC 中 Disarm | SITL 飞行器 disarmed |

### 2.4 断开重连测试

| 步骤 | 操作 | 预期结果 |
|------|------|---------|
| 1 | 正常连接状态 | 数据正常流动 |
| 2 | 点击断开连接 | bridge 子进程停止，logcat 显示 "Bridge stopped" |
| 3 | 重新连接 | bridge 重新启动，数据恢复 |
| 4 | 杀掉机载端 zenoh-bridge-dds | QGC 数据停止更新 |
| 5 | 重启机载端 zenoh-bridge-dds | QGC 数据自动恢复（Zenoh 自动重连） |

## 测试三：回归测试（不开启 Zenoh）

**目的**：验证不开启 Zenoh bridge 时，DDS_P5_V3 的所有功能正常。

### 3.1 直连 DDS 测试

| 步骤 | 操作 | 预期结果 |
|------|------|---------|
| 1 | 添加 DDS 连接，**不开启** Zenoh Bridge | Zenoh 相关 UI 隐藏 |
| 2 | 连接到 DDS Agent（直连模式） | 与 DDS_P5_V3 行为一致 |
| 3 | 验证数据接收 | 正常 |
| 4 | 验证航点规划 | 正常（之前修复的 bug 不回归） |
| 5 | 验证手柄控制 | 正常（Skydroid / USB 手柄） |

### 3.2 多版本 IDL 切换

| 步骤 | 操作 | 预期结果 |
|------|------|---------|
| 1 | 选择 PX4 SITL v1.17 Profile | v1 IDL |
| 2 | 选择 PX4 SITL v1.16 Profile | px4_v116 IDL |
| 3 | 选择 CUAV X7+ Profile | px4_v116 IDL + cuav mapping |

## 测试四：异常场景

| 场景 | 操作 | 预期结果 |
|------|------|---------|
| 无 bridge 二进制 | APK 中未包含 zenoh-bridge-dds | 弹出错误提示 "binary extraction failed" |
| 错误 IP | 输入不存在的 IP 地址 | bridge 启动但连接超时，QGC 最终显示错误 |
| 错误端口 | 输入错误端口 | 同上 |
| 机载端未启动 | 安卓先连接 | bridge 启动成功但无数据，等待机载端上线 |
| Domain ID 不匹配 | 安卓 domain=0，机载端 domain=1 | 无数据匹配 |

## 测试记录模板

```
日期:       _______________
测试人:     _______________
APK 版本:   _______________
机载端 IP:  _______________
PX4 版本:   _______________

测试二 结果:
  [  ] 2.1 基本连接    通过 / 失败
  [  ] 2.2 数据接收    通过 / 失败
  [  ] 2.3 控制指令    通过 / 失败
  [  ] 2.4 断开重连    通过 / 失败

测试三 结果:
  [  ] 3.1 直连 DDS    通过 / 失败
  [  ] 3.2 IDL 切换    通过 / 失败

测试四 结果:
  [  ] 无二进制        通过 / 失败
  [  ] 错误 IP         通过 / 失败
  [  ] 机载端未启动    通过 / 失败

备注:
_______________________________________
```

## 性能指标

| 指标 | 目标值 | 测量方法 |
|------|--------|---------|
| Bridge 启动时间 | < 3 秒 | logcat 时间戳 |
| 首条数据延迟 | < 5 秒 | 连接到首次显示 |
| 数据更新频率 | ≥ 10 Hz | QGC 日志统计 |
| APK 体积增加 | < 25 MB | APK 大小对比 |
| 内存占用增加 | < 50 MB | Android profiler |
