# DDS 配置与部署指南

## 一、DDS Profile 配置机制（无需重新编译）

### 问题

每次新增飞控型号都需要添加 JSON 配置文件并重新编译 QGC，这不利于产品化。

### 解决方案

QGC DDS 版本已支持**运行时加载外部 JSON 配置文件**，无需重新编译。

#### 配置文件搜索顺序

`DDSMappingEngine::_resolveFilePath()` 按以下顺序查找配置文件：

| 优先级 | 路径 | 说明 |
|:------:|------|------|
| 1 | `~/.config/QGroundControl.org/dds_mappings/xxx.json` | 用户自定义目录（**推荐**） |
| 2 | 程序内嵌资源 `:/dds_mappings/xxx.json` | 编译时打包的内置配置 |
| 3 | 可执行文件旁 `./resources/dds_mappings/xxx.json` | 便携部署方式 |

**优先级 1 覆盖优先级 2/3**，所以用户自定义的配置会覆盖内置同名文件。

#### 添加新飞控配置步骤

1. 复制模板文件（或现有配置）：
```bash
mkdir -p ~/.config/QGroundControl.org/dds_mappings/
cp cuav_x7pro.json ~/.config/QGroundControl.org/dds_mappings/my_drone.json
```

2. 编辑 `my_drone.json`，修改 `vendor`、`dds_topic` 等字段：
```json
{
  "vendor": "my_drone_v1",
  "version": "1.0.0",
  "topics": [
    {
      "dds_topic": "/my_drone/out/vehicle_status",
      "dds_type": "px4_msgs::msg::dds_::VehicleStatus_",
      "fact_group": "_internal",
      "fields": []
    }
  ]
}
```

3. 在 QGC 的 DDS 链接设置中选择 **"Custom..."**，输入文件名 `my_drone`（不含 .json 后缀）

4. 重新连接 DDS 链接即可生效，**无需重新编译**

#### JSON 配置文件模板

完整字段说明：

```json
{
  "vendor": "飞控厂商名称",
  "version": "1.0.0",
  "description": "说明文字",
  "topics": [
    {
      "dds_topic": "/fmu/out/vehicle_status_v1",  // DDS 话题全名
      "dds_type": "px4_msgs::msg::dds_::VehicleStatus_",  // IDL 类型名
      "fact_group": "_internal",  // QGC FactGroup 名称
      "fields": [
        {
          "dds_field": "lat",           // DDS 消息中的字段名
          "fact_group": "gps",          // 目标 FactGroup（可选，覆盖 topic 级别）
          "fact_name": "lat",           // 目标 Fact 名称
          "transform": "",              // 变换函数（可选）
          "scale": 1.0,                 // 缩放系数（可选，默认 1.0）
          "offset": 0.0                 // 偏移量（可选，默认 0.0）
        }
      ]
    }
  ]
}
```

**可用的 transform 函数**：
- `quaternion_to_euler_roll` / `pitch` / `yaw` — 四元数转欧拉角
- `rad_to_deg` — 弧度转角度
- `negate` — 取反（NED → ENU 转换）
- `ground_speed_from_vxy` — 从 vx/vy 计算地面速度
- `wind_direction_from_ne` / `wind_speed_from_ne` — 风向/风速计算

#### 当前内置配置

| 配置名 | 文件名 | 适用场景 |
|--------|--------|---------|
| PX4 SITL (default) | `_default.json` | PX4 SITL 仿真（话题带 `_v1` 后缀） |
| CUAV X7+ Pro | `cuav_x7pro.json` | CUAV X7+ Pro 真实飞控 |

---

## 二、可执行文件跨机器部署

### 问题

在电脑 A 上编译出的 `build/Debug/QGroundControl`，能否直接拷贝到电脑 B 上运行？

### 答案：可以，但需要满足依赖条件

#### 方案 1：带依赖库一起拷贝（推荐）

```bash
# === 在编译机 A 上 ===

# 1. 收集所有动态库依赖
mkdir -p QGC_Portable/lib
cp build/Debug/QGroundControl QGC_Portable/
ldd build/Debug/QGroundControl | grep "=> /" | awk '{print $3}' | xargs -I {} cp {} QGC_Portable/lib/

# 2. 复制 Qt 插件（必需）
cp -r /path/to/Qt/6.10.3/gcc_64/plugins/platforms QGC_Portable/plugins/
cp -r /path/to/Qt/6.10.3/gcc_64/plugins/imageformats QGC_Portable/plugins/
cp -r /path/to/Qt/6.10.3/gcc_64/plugins/xcbglintegrations QGC_Portable/plugins/

# 3. 复制 QML 模块（必需）
cp -r /path/to/Qt/6.10.3/gcc_64/qml QGC_Portable/qml/

# 4. 复制 DDS 配置文件（可选）
mkdir -p QGC_Portable/resources/dds_mappings
cp resources/dds_mappings/*.json QGC_Portable/resources/dds_mappings/

# 5. 创建启动脚本
cat > QGC_Portable/run_qgc.sh << 'EOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$SCRIPT_DIR/plugins"
export QML2_IMPORT_PATH="$SCRIPT_DIR/qml"
exec "$SCRIPT_DIR/QGroundControl" "$@"
EOF
chmod +x QGC_Portable/run_qgc.sh

# 6. 打包
tar czf QGC_Portable.tar.gz QGC_Portable/
```

```bash
# === 在目标机 B 上 ===
tar xzf QGC_Portable.tar.gz
cd QGC_Portable
./run_qgc.sh
```

#### 方案 2：使用 Qt 的 linuxdeployqt 工具

```bash
# 自动收集所有依赖并创建 AppImage
wget https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage
chmod +x linuxdeployqt-continuous-x86_64.AppImage

./linuxdeployqt-continuous-x86_64.AppImage build/Debug/QGroundControl \
    -qmldir=src \
    -appimage
```

生成的 `.AppImage` 文件可以直接在任何 Linux x86_64 机器上运行。

#### 方案 3：静态编译（一劳永逸）

如果需要完全独立的单文件，可以用 Qt 静态编译版本。但 Qt 静态编译本身需要额外配置，且 LGPL 许可要求会更严格。

#### 前提条件

| 条件 | 说明 |
|------|------|
| CPU 架构 | A 和 B 必须相同（都是 x86_64 或都是 aarch64） |
| Linux 内核 | B 的内核版本 ≥ A 编译时的版本 |
| glibc | B 的 glibc 版本 ≥ A 编译时的版本 |
| GPU 驱动 | B 需要有 OpenGL 支持（Intel/NVIDIA/Mesa） |
| 显示服务 | B 需要 X11 或 Wayland |

#### 快速检查兼容性

```bash
# 在目标机 B 上检查
ldd --version          # glibc 版本
uname -m               # CPU 架构
glxinfo | head -5      # OpenGL 支持
```

---

## 三、Zenoh 通信兼容性分析

### 问题

如果机载端与地面 QGC 端通过 Zenoh 进行通信，已知 Zenoh 的转发方式是按需转发，能否适用于当前 DDS 版本的 QGC？

### 架构分析

```
当前 DDS 直连架构：
[QGC (CycloneDDS)] ←UDP 多播→ [DDS Agent (CycloneDDS)] ←串口→ [PX4 飞控]

Zenoh 桥接架构：
[QGC (CycloneDDS)] ←本地UDP→ [zenoh-bridge-dds] ←Zenoh协议/TCP/QUIC→ [zenoh-bridge-dds] ←本地UDP→ [DDS Agent] ←串口→ [PX4]
     地面端                         路由器/4G                              机载端
```

### zenoh-bridge-dds 工作原理

`zenoh-bridge-dds` 是 Eclipse Zenoh 项目提供的 DDS 透明代理：

1. **本地端**：作为 DDS participant 加入本地 DDS domain，订阅/发布所有（或指定的）DDS topic
2. **远程端**：通过 Zenoh 协议（TCP/QUIC/UDP）将数据转发到对端 bridge
3. **对端 bridge**：接收 Zenoh 数据后，在远端 DDS domain 中重新发布

### 按需转发的影响

Zenoh 的"按需转发"指：**只有当某一端有订阅者时，对端才会转发该 topic 的数据**。

这对 QGC DDS 版本**是兼容的**，原因：

| QGC 行为 | Zenoh 响应 |
|----------|-----------|
| QGC 启动 → 订阅 `vehicle_status`、`global_position` 等 | bridge 检测到订阅 → 自动从远端拉取对应 topic |
| QGC 发布 `vehicle_command`、`manual_control_input` | bridge 检测到发布 → 自动转发到远端 |
| QGC 未订阅的 topic（如 `sensor_combined`） | bridge 不转发 → 节省带宽 ✅ |

**关键结论**：QGC 会在启动时订阅所有 mapping JSON 中定义的 topic，所以所有需要的数据都会触发 Zenoh 转发。按需转发不会遗漏任何 QGC 需要的数据。

### 潜在问题与解决方案

| 问题 | 说明 | 解决方案 |
|------|------|---------|
| **DDS 发现延迟** | bridge 启动后需要时间发现本地 DDS participant | 启动顺序：先 bridge → 再 QGC / DDS Agent |
| **Topic 类型匹配** | bridge 需要知道消息类型才能序列化 | 使用 `--dds-generics` 模式绕过类型检查 |
| **QoS 映射** | Zenoh 和 DDS 的 QoS 策略不完全一致 | bridge 默认做 best-effort 映射，一般够用 |
| **延迟增加** | Zenoh 多一层转发，增加 1-5ms | 对手动控制可接受（50Hz 发送间隔 = 20ms） |
| **带宽** | 4G 网络带宽有限 | 按需转发已自动优化；可进一步限制 topic 白名单 |
| **重发机制** | QGC 每 2s 重发航点命令 | 即使丢包也能恢复 ✅ |

### 测试方案

#### 环境准备

```bash
# 安装 zenoh-bridge-dds（两端都需要）
# 方法 1：cargo 编译
cargo install zenoh-bridge-dds

# 方法 2：下载预编译二进制
wget https://github.com/eclipse-zenoh/zenoh-plugin-dds/releases/latest/download/zenoh-bridge-dds-x86_64-unknown-linux-gnu.zip
```

#### 测试 A：单机回环测试（验证基本功能）

在同一台电脑上模拟 Zenoh 桥接：

```bash
# 终端 1：启动 PX4 SITL
cd PX4-Autopilot && make px4_sitl gz_x500

# 终端 2：启动 DDS Agent
MicroXRCEAgent udp4 -p 8888

# 终端 3：启动 Zenoh bridge A（DDS Agent 侧，domain 0）
zenoh-bridge-dds -d 0 -l tcp/0.0.0.0:7447 --dds-generics

# 终端 4：启动 Zenoh bridge B（QGC 侧，domain 1）
zenoh-bridge-dds -d 1 -e tcp/127.0.0.1:7447 --dds-generics

# 终端 5：启动 QGC（设置 DDS Domain ID = 1）
# QGC 设置中将 Domain ID 改为 1
./build/Debug/QGroundControl
```

**验证项**：
- [ ] QGC 能显示无人机位置和姿态
- [ ] QGC 能发送 Arm/Takeoff 命令
- [ ] 虚拟摇杆能控制无人机
- [ ] Go To 能工作
- [ ] 航点任务能执行

#### 测试 B：双机测试（模拟真实部署）

```
电脑 A（地面站）                    电脑 B（机载端/SITL）
├── QGC (domain 1)                ├── PX4 SITL
├── zenoh-bridge-dds              ├── DDS Agent (domain 0)
│   -d 1                          ├── zenoh-bridge-dds
│   -e tcp/<B的IP>:7447           │   -d 0
│                                 │   -l tcp/0.0.0.0:7447
└── CycloneDDS (domain 1)        └── CycloneDDS (domain 0)
```

```bash
# === 电脑 B（机载端）===
# 启动 PX4 SITL + DDS Agent + Zenoh bridge
MicroXRCEAgent udp4 -p 8888 &
zenoh-bridge-dds -d 0 -l tcp/0.0.0.0:7447 --dds-generics

# === 电脑 A（地面站）===
# 启动 Zenoh bridge + QGC
zenoh-bridge-dds -d 1 -e tcp/192.168.1.100:7447 --dds-generics &
# QGC 中 DDS Domain ID = 1
./QGroundControl
```

#### 测试 C：性能和可靠性

```bash
# 在 bridge 侧查看转发的 topic 列表
zenoh-bridge-dds -d 0 -l tcp/0.0.0.0:7447 --dds-generics -v

# 测量延迟（在两端分别记录 timestamp）
ros2 topic echo /fmu/out/vehicle_global_position --field timestamp
```

**测试清单**：
- [ ] 延迟测量：Zenoh 模式 vs 直连 DDS 模式的操控响应时间
- [ ] 断网恢复：断开网络 5 秒后恢复，QGC 是否自动重连
- [ ] 长时间运行：持续飞行 10 分钟，检查数据是否完整
- [ ] 带宽统计：使用 `iftop` 或 `nethogs` 测量 Zenoh 占用带宽

#### 测试 D：4G 网络测试

```bash
# 如果通过 4G 连接，需要公网 IP 或内网穿透

# 方案 1：机载端有公网 IP
zenoh-bridge-dds -d 0 -l tcp/0.0.0.0:7447  # 机载端
zenoh-bridge-dds -d 1 -e tcp/<公网IP>:7447  # 地面端

# 方案 2：使用 Zenoh 路由器（推荐）
# 在有公网 IP 的服务器上运行 zenoh router
zenohd -l tcp/0.0.0.0:7447

# 两端 bridge 都连接到路由器
zenoh-bridge-dds -d 0 -e tcp/<服务器IP>:7447  # 机载端
zenoh-bridge-dds -d 1 -e tcp/<服务器IP>:7447  # 地面端
```

### 总结

| 方面 | 评估 |
|------|------|
| 兼容性 | ✅ 完全兼容，zenoh-bridge-dds 对 QGC 透明 |
| 按需转发 | ✅ 不影响功能，QGC 已订阅所有需要的 topic |
| 性能 | ✅ 增加 1-5ms 延迟，50Hz 控制频率下可接受 |
| 可靠性 | ✅ QGC 已有 2s 重发 + 10s 超时保护 |
| 部署复杂度 | ⚠️ 需要两端都安装 zenoh-bridge-dds |
| 推荐场景 | 4G/5G 远程控制、跨网段、需要中继的场景 |
