# DDS_P5_V2 使用手册

## 1. 多版本 IDL 支持

### 1.1 概述

DDS_P5_V2 支持同时编译多个 PX4 固件版本的消息定义（IDL），在创建 DDS 连接时
可以选择匹配目标固件版本的配置。这解决了 PX4 v1.16、v1.17 和最新 px4_msgs 之间
的消息布局不兼容问题。

### 1.2 支持的配置

| 配置名 | IDL 版本 | 适用场景 |
|--------|---------|---------|
| PX4 SITL v1.17 | v1（默认） | 软件在环仿真，PX4 v1.17 固件 |
| PX4 SITL v1.16 | px4_v116 | 软件在环仿真，PX4 v1.16 固件 |
| CUAV X7+ (v1.16) | px4_v116 | CUAV X7+ Pro 硬件，v1.16 固件 |
| 自定义 | 用户指定 | 自定义 JSON mapping 文件 |

### 1.3 如何切换版本

1. 打开 QGC → 应用设置 → 通信链路
2. 添加新的 DDS 连接（或编辑现有连接）
3. 在 **DDS Profile** 下拉菜单中选择匹配的配置
4. 设置 Domain ID（默认 0）
5. 保存并连接

选择配置后，QGC 会自动使用对应版本的 IDL 类型描述符来解析 DDS 数据。

### 1.4 各版本消息差异

详见 `PX4_v116_vs_v117_消息差异分析.md`。

主要差异：
- **BatteryStatus**：v1.16 有 `serial_number` 字段（40 字段），v1.17 删除该字段（39 字段）
- **HomePosition**：v1.17 新增 `roll` 和 `pitch` 字段（15 字段 vs v1.16 的 13 字段）
- **AirspeedValidated**：v1.17 新增 `airspeed_source`，删除 `true_ground_minus_wind_m_s`

### 1.5 添加新的版本支持

如果需要支持其他 PX4 版本：

1. 在 `src/DDS/idl/` 下创建新的子目录（如 `px4_v118/`）
2. 放入有差异的消息 IDL 文件（使用独立的 module 名如 `dds_v118_`）
3. 在 `src/Comms/CMakeLists.txt` 中添加编译指令：
   ```cmake
   dds_compile_idl_variant("${CMAKE_SOURCE_DIR}/src/DDS/idl/px4_v118" "_v118")
   ```
4. 在 `DDSTypeRegistry.cc` 中注册新版本的类型和提取函数
5. 在 `DDSSettings.qml` 的 `_profileDefs` 中添加新的配置项

---

## 2. 中文本地化

### 2.1 语言切换

QGC 支持多语言。切换到中文：

1. 打开 QGC → Settings → General
2. 在 Language 选项中选择 **简体中文 (zh-CN)**
3. 重启 QGC 生效

### 2.2 DDS 界面翻译

DDS 连接设置界面的所有文本已翻译为简体中文：
- 域 ID / DDS 配置 / 命名空间前缀 / 自动发现话题
- 配置说明文字

---

## 3. 离线地图

### 3.1 概述

QGC 内置离线地图功能，支持下载地图瓦片供离线使用。中国地区推荐使用
**天地图（TianDiTu）** 或 **OpenStreetMap** 作为地图源。

### 3.2 地图源对比

| 地图源 | 需要 API Key | 中国地区支持 | 说明 |
|--------|-------------|-------------|------|
| OpenStreetMap | 否 | 一般 | 免费，无需配置，直接可用 |
| 天地图 (TianDiTu) | 是 | 最佳 | 中国官方地图，详细度最高 |
| Bing Maps | 否 | 一般 | 微软地图，卫星图清晰 |
| Google Maps | 可能需要 | 受限 | 中国大陆可能无法访问 |

### 3.3 使用 OpenStreetMap（推荐，无需配置）

OpenStreetMap 不需要任何 API Key，开箱即用：

1. 打开 QGC → Settings → Map（地图设置）
2. 选择 **Street Map (OpenStreetMap)** 作为地图提供商
3. 导航到目标区域
4. 进入 Settings → Offline Maps
5. 点击 **Add New Set** 下载当前区域的地图瓦片

### 3.4 使用天地图（中国地区最详细）

天地图需要注册获取 API Token：

1. **获取 Token**：
   - 访问 https://console.tianditu.gov.cn/
   - 注册账号并创建应用
   - 获取个人 API Token（免费，每日 10000 次请求）

2. **配置 QGC**：
   - 打开 QGC → Settings → General
   - 找到 **TianDiTu** 设置项
   - 填入你的 API Token

3. **下载离线地图**：
   - 选择 **TianDiTu Road** 或 **TianDiTu Satellite** 作为地图
   - 导航到目标飞行区域
   - 进入 Settings → Offline Maps → Add New Set
   - 设置缩放级别范围（建议 13-19）
   - 点击 Download 开始下载

### 3.5 离线地图管理

- **导入/导出**：Offline Maps 页面支持导入/导出地图数据库文件（.db）
- **删除**：可以删除不需要的地图集释放存储空间
- **更新**：重新下载同一区域可更新瓦片

### 3.6 存储位置

| 平台 | 地图缓存路径 |
|------|-------------|
| Android | `/sdcard/Android/data/org.mavlink.qgroundcontrol/cache/` |
| Linux | `~/.cache/QGroundControl.org/` |
| Windows | `%LOCALAPPDATA%/cache/QGroundControl.org/` |

---

## 4. 手柄/摇杆 DDS 控制

### 4.1 概述

DDS_P5_V2 支持通过标准 USB/蓝牙手柄（Xbox、PS、北通等）直接通过 DDS 控制无人机，
无需 MAVLink 中转。当 QGC 检测到当前连接类型为 DDS 时，手柄数据会自动路由到
`DDSManualControlPublisher`，通过 DDS `manual_control_input` 话题发送给飞控。

### 4.2 数据流

```
USB/蓝牙手柄 → SDL3 → JoystickManager → Vehicle::sendJoystickDataThreadSafe()
                                              │
                                              ├── DDS 连接 → DDSManualControlPublisher
                                              │               → rt/fmu/in/manual_control_input
                                              │               → PX4 飞控
                                              │
                                              └── MAVLink 连接 → MAVLink MANUAL_CONTROL (#69)
                                                                 → PX4 飞控
```

路由是自动的：QGC 检查当前活动链路类型，DDS 链路走 DDS 通道，MAVLink 链路走原有通道。

### 4.3 使用方法

1. 将 USB 手柄插入手机/电脑（或通过蓝牙配对）
2. 打开 QGC，创建 DDS 连接并连接飞控
3. 进入 Settings → Joystick（摇杆设置）
4. QGC 自动检测到手柄，进行校准
5. 手柄数据自动通过 DDS 发送，无需额外配置

### 4.4 支持的手柄类型

所有被 Android/Linux 系统识别为标准游戏手柄的设备均可使用：

| 手柄 | 连接方式 | 支持情况 |
|------|---------|---------|
| Xbox 手柄 | USB / 蓝牙 | ✓ |
| PS4/PS5 手柄 | USB / 蓝牙 | ✓ |
| 北通手柄 | USB / 蓝牙 | ✓ |
| 罗技手柄 | USB | ✓ |
| 其他标准 HID 手柄 | USB / 蓝牙 | ✓ |

### 4.5 注意事项

- 手柄校准在 QGC 的 Joystick 设置界面完成，与连接类型无关
- DDS 模式下手柄值范围：roll/pitch/yaw ∈ [-1, +1]，thrust ∈ [0, 1]
- 死区和指数曲线设置使用 QGC 的 Virtual Joystick 参数（Settings → General）
- 如果同时存在 DDS 和 MAVLink 连接，手柄数据只发送到主连接

---

## 5. Android 编译

### 5.1 编译步骤

```bash
# 1. 拉取最新代码
cd ~/qgc_dev
git checkout DDS_P5_V2
git pull origin DDS_P5_V2

# 2. 清除旧构建
rm -rf build-android

# 3. 配置（根据你的环境调整路径）
cmake -B build-android -S . \
  -DCMAKE_TOOLCHAIN_FILE=$QT_DIR/lib/cmake/Qt6/qt.toolchain.cmake \
  -DQGC_ENABLE_DDS=ON \
  -DCYCLONEDDS_ROOT=/path/to/cyclonedds-android \
  -G Ninja

# 4. 编译
cmake --build build-android

# 5. 签名 APK（按之前的流程）
```

### 5.2 多版本 IDL 编译说明

CMake 会自动处理多版本 IDL：
- `src/DDS/idl/*.idl` → 基础类型（v1.17 默认）
- `src/DDS/idl/px4_v116/*.idl` → v1.16 覆盖类型
- `src/DDS/idl/v4/*.idl` → V4 VehicleStatus（最新 px4_msgs）

不需要额外的编译参数。

---

## 6. 文件结构

```
src/DDS/idl/
├── *.idl                       # 基础 IDL（PX4 v1.17 布局）
├── px4_v116/                   # PX4 v1.16 覆盖
│   ├── BatteryStatus.idl       # v1.16 电池状态（40 字段）
│   ├── HomePosition.idl        # v1.16 Home 位置（无 roll/pitch）
│   └── AirspeedValidated.idl   # v1.16 空速验证（无 airspeed_source）
└── v4/                         # V4 覆盖（最新 px4_msgs）
    └── VehicleStatus.idl       # V4 载具状态（有 nav_state_display）

resources/dds_mappings/
├── _default.json               # 默认 mapping（v1.17 SITL）
├── cuav_x7pro.json             # CUAV X7+ Pro（v1.16）
├── _default_v4.json            # V4 mapping
└── _vendor_template.json       # 自定义模板

src/DDS/
├── DDSTypeRegistry.cc/.h       # 类型注册（支持 v1/px4_v116/v4）
├── DDSMappingEngine.cc/.h      # JSON mapping 解析
└── DDSDataInjector.cc/.h       # 数据注入到 QGC Fact 系统

src/Comms/DDSLink/
├── DDSConfiguration.cc/.h      # 连接配置（含 idlVersion）
└── DDSLink.cc/.h               # DDS 连接实现

src/Vehicle/
└── Vehicle.cc                  # 手柄→DDS 路由（sendJoystickDataThreadSafe）

src/AppSettings/
└── DDSSettings.qml             # DDS 设置界面（版本选择器）

translations/
└── qgc_source_zh_CN.ts         # 简体中文翻译（含 DDS 翻译）
```
