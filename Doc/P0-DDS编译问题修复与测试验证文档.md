# P0 DDS 编译问题修复与测试验证文档

> **文档版本**: 1.0  
> **日期**: 2026-06-02  
> **分支**: `DDS`  
> **仓库**: `https://github.com/L-a-L-max/qgc_dev.git`

---

## 一、本阶段主要工作概述

本阶段的核心任务是将 P0 DDS 基础设施代码集成到 QGC 源码中，完成编译，并在 QGC 界面中进行功能验证。工作过程中遇到了以下几类问题并逐一解决：

| 序号 | 问题类别 | 问题描述 | 严重程度 | 状态 |
|------|---------|---------|---------|------|
| 1 | 网络问题 | CMake 配置阶段无法从 GitHub 下载 ArduPilot 参数仓库 | 阻塞 | 已解决 |
| 2 | 系统依赖缺失 | 缺少 `libsecret-1-dev` 库导致 qtkeychain 构建失败 | 阻塞 | 已解决 |
| 3 | DDS 代码缺陷 | `DDSDataInjector` 成员变量缺少构造函数参数传递 | 编译错误 | 已修复并推送 |
| 4 | 上游兼容性 | GCC 12 + Qt 6.10.3 导致 `VehicleComponent*` metatype 断言失败 | 编译错误 | 需升级 GCC |

---

## 二、问题详细分析与修复

### 2.1 问题 1：GitHub 网络连接超时（ArduPilot 参数仓库）

**错误现象**

CMake 配置阶段通过 CPM（CMake 包管理器）从 GitHub 拉取 ArduPilot 参数仓库时失败：

```
fatal: unable to access 'https://github.com/ArduPilot/ParameterRepository.git/':
Failed to connect to github.com port 443 after 133149 ms: Connection timed out
```

**日志定位方法**

在 CMake 输出中搜索关键字 `fatal:` 或 `FAILED:`。该错误出现在 CMake 配置（configure）阶段而非编译（build）阶段，因此看到的是 CMake 输出而不是 Ninja 输出。关键特征：

```
CPM: Adding package ArduPilotParams@0 (037ff6bcfc... to .cache/CPM/ardupilotparams/f29f)
[1/9] Performing download step (git clone) for 'ardupilotparams-populate'
Cloning into 'f29f'...
fatal: unable to access ...
```

**根因分析**

这是纯网络问题——编译机器无法直接访问 GitHub。与 DDS 代码完全无关。

**解决方案（三选一）**

| 方案 | 操作 | 适用场景 |
|------|------|---------|
| A. 配置代理 | `export https_proxy=http://代理地址:端口` | 有代理的网络环境 |
| B. 手动预下载 | 先用能访问 GitHub 的网络 `git clone` 到 `.cache/CPM/ardupilotparams/f29f`，然后 `git checkout 037ff6bc...` | 间歇性网络问题 |
| C. 跳过 APM | cmake 参数加 `-DQGC_DISABLE_APM_PLUGIN=ON` | 只需要 PX4，不需要 ArduPilot |

**本次实际采用方案**: 用户通过代理/重试后成功下载。

---

### 2.2 问题 2：系统库 `libsecret-1` 缺失

**错误现象**

CMake 配置阶段在构建 `qtkeychain` 依赖时报错：

```
-- Checking for module 'libsecret-1'
--   No package 'libsecret-1' found
CMake Error at .../FindPkgConfig.cmake:1093 (message):
  The following required packages were not found:
   - libsecret-1
```

**日志定位方法**

在 CMake 输出中搜索 `No package` 或 `required packages were not found`。`libsecret-1` 是 Linux 系统上的密码/密钥存储库（GNOME Keyring 的接口），`qtkeychain` 依赖它来实现安全的凭证存储。

**解决方案**

安装缺失的系统依赖：

```bash
sudo apt install -y libsecret-1-dev
```

**QGC 常用依赖一键安装（预防后续类似问题）**：

```bash
sudo apt install -y \
    libsecret-1-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    libsdl2-dev \
    libspeechd-dev \
    libmd-dev
```

---

### 2.3 问题 3：`DDSDataInjector` 构造函数参数缺失（DDS 代码缺陷）

**错误现象**

Ninja 编译阶段报错：

```
[253/853] Building CXX object .../DDSLink.cc.o
FAILED: .../DDSLink.cc.o
/home/bjtu/QGC_DDS/qgc_dev/src/Comms/DDSLink/DDSLink.cc:11:35:
error: no matching function for call to 'DDSDataInjector::DDSDataInjector()'
   11 |     : LinkInterface(config, parent)
      |                                   ^
```

**日志定位方法**

编译日志中搜索 `FAILED:` 即可定位。这是 Ninja 编译阶段错误（不同于 CMake 配置错误），格式为：
```
[进度/总数] Building CXX object ...
FAILED: 目标文件
编译命令（很长一行）
源文件:行号: error: 错误描述
```

其中：
- `[253/853]` 表示总共 853 个编译任务，在第 253 个时失败
- `FAILED:` 后面是失败的目标 `.o` 文件
- 最关键的是 `error:` 那一行，告诉你具体哪个源文件的哪一行有什么错误

**根因分析**

`DDSLink` 类声明了 `DDSDataInjector _dataInjector;` 作为成员变量。在 C++ 中，如果构造函数的成员初始化列表（member initializer list）没有显式初始化某个成员，编译器会尝试调用它的默认构造函数。

但 `DDSDataInjector` 类只有一个构造函数，需要三个参数：

```cpp
// src/DDS/DDSDataInjector.h:35-37
explicit DDSDataInjector(DDSMappingEngine *engine,
                         DDSTransformRegistry *transforms,
                         QObject *parent = nullptr);
```

它没有默认构造函数 `DDSDataInjector()`，所以编译器报错 `no matching function for call to 'DDSDataInjector::DDSDataInjector()'`。

**修复方案**

在 `DDSLink` 构造函数的成员初始化列表中显式传递参数：

```cpp
// 修复前（DDSLink.cc:10-11）
DDSLink::DDSLink(SharedLinkConfigurationPtr &config, QObject *parent)
    : LinkInterface(config, parent)
    // _dataInjector 被隐式默认构造 → 报错

// 修复后（DDSLink.cc:10-12）
DDSLink::DDSLink(SharedLinkConfigurationPtr &config, QObject *parent)
    : LinkInterface(config, parent)
    , _dataInjector(&_mappingEngine, &_transformRegistry, this)
```

这里 `&_mappingEngine` 和 `&_transformRegistry` 是 `DDSLink` 自身的成员变量指针，`this` 作为 QObject 父对象传递。成员变量按声明顺序初始化，`_mappingEngine` 和 `_transformRegistry` 在 `_dataInjector` 之前声明（`DDSLink.h:59-61`），因此此时它们已完成初始化。

**关联 Git 提交**

```
commit 4b99d7117
fix(dds): initialize DDSDataInjector with required constructor arguments
```

**涉及文件**:
- `src/Comms/DDSLink/DDSLink.cc` — 修改第 10-12 行，在成员初始化列表中传递参数

---

### 2.4 问题 4：`VehicleComponent*` Metatype 断言失败（上游兼容性问题）

**错误现象**

Ninja 编译阶段的 MOC（Qt 元对象编译器）生成代码报错：

```
[364/853] Building CXX object .../mocs_compilation.cpp.o
FAILED: .../mocs_compilation.cpp.o
...
/home/bjtu/Qt6.10/6.10.3/gcc_64/include/QtCore/qmetatype.h:2665:5:
error: static assertion failed: Pointer Meta Types must either point to
fully-defined types or be declared with Q_DECLARE_OPAQUE_POINTER(T *)
```

**日志定位方法**

搜索 `FAILED:` 找到第二个失败任务。这个错误与第一个 DDSLink 错误不同：
- 目标文件是 `mocs_compilation.cpp.o`（MOC 生成的汇总文件）
- 错误信息中包含 `static assertion failed` 和 `qmetatype.h` — 这是 Qt 框架内部的类型检查
- 关键信息在 `In instantiation of ... [with X = VehicleComponent*]` 一行，告诉你是 `VehicleComponent*` 类型出了问题

向上追溯错误的调用链：
```
qmetatype.h:2665   ← static_assert 触发点
qtmochelpers.h:262 ← MOC 辅助代码
AutoPilotPlugin.h:20 ← Q_OBJECT 宏展开
moc_AutoPilotPlugin.cpp:121 ← MOC 自动生成的代码
```

**根因分析**

这 **不是 DDS 代码引入的问题**，而是 QGC 上游代码在 GCC 12 + Qt 6.10.3 组合下的兼容性问题。

具体原因：

1. `AutoPilotPlugin.h` 第 10 行前向声明了 `class VehicleComponent;`（不完整类型）
2. `AutoPilotPlugin.h` 第 51、57 行在 `Q_INVOKABLE` 方法中使用了 `VehicleComponent*` 指针：
   ```cpp
   Q_INVOKABLE virtual QString prerequisiteSetup(VehicleComponent *component) const = 0;
   Q_INVOKABLE VehicleComponent *findKnownVehicleComponent(KnownVehicleComponent knownVehicleComponent);
   ```
3. Qt 6.10.3 的新 constexpr MOC 生成器在编译期对 `Q_INVOKABLE` 方法的参数/返回值类型做完整性检查
4. GCC 12 在 constexpr 求值时无法推迟类型完整性检查（GCC 13+ 可以）
5. 因此 MOC 在处理 `AutoPilotPlugin.h` 时发现 `VehicleComponent` 只是前向声明而非完整定义，触发 `static_assert`

**解决方案：升级 GCC 到 13**

QGC 官方 CI 使用 GCC 13/14，这个问题在更高版本 GCC 中不会出现：

```bash
# 安装 GCC 13
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install g++-13 gcc-13

# 重新配置 + 编译（指定 GCC 13）
cd ~/QGC_DDS/qgc_dev/build
rm -rf *
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=$QT_DIR \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
    -DQGC_ENABLE_DDS=ON
ninja -j$(nproc)
```

> **注意**: 安装 GCC 13 不会影响系统默认的 GCC 12，PX4 和 Gazebo 等其他项目仍然使用各自的编译器设置。只需在 QGC 编译时通过 `-DCMAKE_CXX_COMPILER=g++-13` 指定即可。

---

## 三、编译日志阅读指南

### 3.1 编译日志的两个阶段

QGC 的编译过程分为两个阶段，对应不同格式的日志：

| 阶段 | 工具 | 日志特征 | 关注点 |
|------|------|---------|--------|
| **CMake 配置** | cmake | `-- xxx` 格式的状态输出 | `CMake Error`、`No package xxx found`、`fatal:` |
| **Ninja 编译** | ninja | `[N/M] Building xxx` 格式的进度 | `FAILED:`、`error:`、`warning:` |

### 3.2 CMake 配置阶段日志解读

CMake 日志从上到下依次：

```
-- BuildConfig: Qt 6.10.3, GStreamer 1.28.1, NDK r27c     ← ① 环境检测
CMake Warning at cmake/modules/Git.cmake:139 ...          ← ② 警告（不影响编译）
-- QGC: Using ccache (/usr/bin/ccache)                     ← ③ 工具链配置
-- CPM: Adding package ArduPilotParams@0 (...)             ← ④ 依赖下载
-- Configuring done (162.2s)                               ← ⑤ 配置完成标志
-- Generating done (2.2s)                                  ← ⑥ 生成构建文件
-- Build files have been written to: .../build             ← ⑦ 成功结束
```

**关键检查点**：
- **看到 `Configuring done` + `Generating done`** = 配置成功，可以进入编译
- **看到 `Configuring incomplete, errors occurred!`** = 配置失败，需要先解决错误
- `Cannot open input file ... .qml: No such file or directory` = **无害警告**，QML linter 在首次编译前找不到生成文件，不影响编译

### 3.3 Ninja 编译阶段日志解读

Ninja 编译日志格式：

```
[进度/总数] 操作描述
```

例如：
```
[253/853] Building CXX object CMakeFiles/QGroundControl.dir/src/Comms/DDSLink/DDSLink.cc.o
FAILED: CMakeFiles/QGroundControl.dir/src/Comms/DDSLink/DDSLink.cc.o
/usr/bin/c++ [很长的编译参数...]
源文件路径:行号:列号: error: 错误描述
```

**如何快速定位编译错误**：

1. **搜索 `FAILED:`** — 每个 `FAILED:` 对应一个编译失败的文件
2. **搜索 `error:`** — 找到具体的错误描述和源码位置
3. **看 `[N/M]` 进度** — `N` 越大说明编译进度越远，如果在很早期就失败，说明是基础组件问题
4. **看目标文件路径** — 判断是哪个模块的问题：
   - `.../DDSLink/DDSLink.cc.o` → DDS 模块代码问题
   - `.../mocs_compilation.cpp.o` → Qt MOC 生成代码问题
   - `.../QGroundControl_autogen/...` → Qt 自动生成代码问题

**区分"我的代码问题"和"上游代码问题"**：

| 特征 | DDS 代码问题 | 上游代码问题 |
|------|-------------|-------------|
| 目标文件路径 | 包含 `DDSLink/` 或 `DDS/` | 不包含 DDS 相关路径 |
| 错误指向源文件 | `src/Comms/DDSLink/` 或 `src/DDS/` | `src/AutoPilotPlugins/`、`src/Vehicle/` 等 |
| 错误类型 | 通常是具体的函数调用/类型错误 | 通常是 `static_assert`、模板实例化错误 |

### 3.4 本次编译日志完整解读

以下是用户最后提供的完整编译日志中的两个 `FAILED:` 块的解读：

#### 第一个 FAILED（DDSLink.cc — DDS 代码错误，已修复）

```
[253/853] Building CXX object .../DDSLink.cc.o        ← 正在编译 DDSLink.cc
FAILED: .../DDSLink.cc.o                               ← 编译失败
/usr/bin/c++ [编译参数...]                              ← 完整编译命令（可忽略）
```
接下来是错误详情：
```
src/Comms/DDSLink/DDSLink.cc:11:35:
error: no matching function for call to 'DDSDataInjector::DDSDataInjector()'
   11 |     : LinkInterface(config, parent)
      |                                   ^
```
- **`DDSLink.cc:11:35`** — 文件 `DDSLink.cc` 第 11 行第 35 列
- **`no matching function`** — 编译器找不到匹配的构造函数
- **`DDSDataInjector::DDSDataInjector()`** — 在尝试调用无参数的默认构造函数

紧接着编译器给出了候选项（`note: candidate`）：
```
DDSDataInjector.h:35:14:
note: candidate: 'DDSDataInjector::DDSDataInjector(DDSMappingEngine*, DDSTransformRegistry*, QObject*)'
note:   candidate expects 3 arguments, 0 provided
```
- 意思是：`DDSDataInjector` 唯一的构造函数需要 3 个参数，但代码提供了 0 个。

#### 第二个 FAILED（mocs_compilation.cpp — 上游兼容性错误，需升级 GCC）

```
[364/853] Building CXX object .../mocs_compilation.cpp.o    ← 正在编译 MOC 汇总文件
FAILED: .../mocs_compilation.cpp.o                           ← 编译失败
```
错误信息较长，关键部分：
```
qmetatype.h: In instantiation of '... [with X = VehicleComponent*]':
```
这告诉你是在实例化 `VehicleComponent*` 类型的 metatype 时出错。

```
AutoPilotPlugin.h:20:5:   required from here
```
这指向 `AutoPilotPlugin.h` 的 `Q_OBJECT` 宏（第 20 行）。

```
error: static assertion failed: Pointer Meta Types must either point to
fully-defined types or be declared with Q_DECLARE_OPAQUE_POINTER(T *)
```
这是 Qt 框架的类型安全检查：使用 `Q_INVOKABLE` 的方法中的指针类型必须是完整定义的类型。

**结论**: 此错误发生在 `AutoPilotPlugin.h`（QGC 核心代码），不在 DDS 目录下，与我们的 DDS 代码无关。

### 3.5 编译日志末尾的统计信息

编译结束时 Ninja 会显示：

```
ninja: build stopped: subcommand failed.
```

这表示有子命令（编译任务）失败导致整体构建停止。如果编译成功，会看到：

```
[853/853] Linking CXX executable QGroundControl
```

---

## 四、P0 功能测试验证方案

### 4.1 前提条件

确保以下步骤已完成：
1. 问题 3（DDSDataInjector）已修复：`git pull` 拉取最新 DDS 分支代码
2. 问题 4（VehicleComponent）已解决：安装 GCC 13 并在 cmake 中指定
3. 编译成功：`ninja -j$(nproc)` 无 FAILED 输出

### 4.2 启动 QGC

```bash
cd ~/QGC_DDS/qgc_dev/build
./QGroundControl
```

### 4.3 测试步骤

#### 测试 1：验证 DDS 链接类型在 UI 中可见

| 步骤 | 操作 | 预期结果 |
|------|------|---------|
| 1 | 启动 QGC | 主界面正常显示 |
| 2 | 点击左上角菜单 → **Application Settings** | 进入设置页面 |
| 3 | 点击左侧 **Comm Links** | 显示通信链接列表 |
| 4 | 点击 **Add** 添加新链接 | 弹出新链接配置面板 |
| 5 | 在 **Type** 下拉列表中查看 | 应包含 **DDS** 选项 |

#### 测试 2：验证 DDS 链接配置

| 步骤 | 操作 | 预期结果 |
|------|------|---------|
| 1 | Type 选择 **DDS** | 显示 DDS 专用配置界面 |
| 2 | 设置 Name = `Test DDS` | 名称输入正常 |
| 3 | 设置 Domain ID = `0` | 数值输入正常 |
| 4 | Vendor Mapping 留空 | 将使用 `_default` 映射表 |
| 5 | Namespace Prefix 留空 | 默认无前缀 |
| 6 | 点击 **OK** 保存 | 链接出现在列表中 |

#### 测试 3：验证连接/断开生命周期

| 步骤 | 操作 | 预期结果 |
|------|------|---------|
| 1 | 选中 `Test DDS` 链接 | 高亮显示 |
| 2 | 点击 **Connect** | 状态变为 Connected |
| 3 | 检查日志（见 4.4 节） | 出现 DDS 连接成功日志 |
| 4 | 点击 **Disconnect** | 状态变为 Disconnected |
| 5 | 检查日志 | 出现 DDS 断开日志 |

#### 测试 4：验证 P0 stub 行为

| 检查项 | 预期结果 | 说明 |
|--------|---------|------|
| 连接是否成功 | 是（stub 模式始终返回成功） | `_createParticipant()` 返回 1 |
| 是否显示飞行器 | 否 | stub 不返回实际 DDS 数据 |
| 是否有 crash | 否 | P0 代码只是无操作 |
| 映射表是否加载 | 是（看日志） | `DDSMappingEngine::loadMapping()` 从 Qt 资源加载 |

### 4.4 QGC 日志查看方法

QGC 使用 Qt 的日志分类系统（`QLoggingCategory`），DDS 相关日志的类别是 `Comms.DDSLink`。

**方法 1：通过环境变量启用 Debug 日志**

在启动 QGC 之前设置环境变量：

```bash
export QT_LOGGING_RULES="Comms.DDSLink.debug=true"
./QGroundControl
```

这会让终端输出所有 DDS 相关的 debug/info/warning 日志。

**方法 2：在终端查看所有输出**

```bash
./QGroundControl 2>&1 | tee qgc_output.log
```

然后在另一个终端实时过滤 DDS 日志：

```bash
tail -f qgc_output.log | grep -i "DDS"
```

**方法 3：QGC 内置 MAVLink Console**

QGC 的 `Analyze → MAVLink Console` 可以查看 Application Output，但 DDS 日志默认只在终端输出。建议使用方法 1 或 2。

**预期日志内容**

连接时应看到（启用 debug 日志后）：

```
[D] Comms.DDSLink: DDSLink created
[D] Comms.DDSLink: Stub: create participant on domain 0
[I] Comms.DDSLink: Loaded mapping: _default topics: 20 fields: 70
[D] Comms.DDSLink: Stub: subscribed to /fmu/out/vehicle_attitude reader: 100
[D] Comms.DDSLink: Stub: subscribed to /fmu/out/vehicle_global_position reader: 101
...（其他 topic 订阅日志）
[I] Comms.DDSLink: DDS link connected on domain 0
```

断开时应看到：

```
[I] Comms.DDSLink: DDS link disconnected
[D] Comms.DDSLink: Stub: destroy participant 1
```

**日志级别说明**：
- `[D]` = Debug：详细的调试信息，默认不显示，需要设置 `QT_LOGGING_RULES` 开启
- `[I]` = Info：关键状态变化（连接/断开），默认显示
- `[W]` = Warning：警告（如映射表加载失败），默认显示

---

## 五、DDS 代码架构回顾

### 5.1 文件清单与职责

```
src/Comms/DDSLink/
├── DDSConfiguration.h/cc    # DDS 链接配置（Domain ID、厂商映射、命名空间前缀）
└── DDSLink.h/cc             # DDS 链接主类（生命周期管理、轮询、CycloneDDS stub）

src/DDS/
├── DDSMappingEngine.h/cc    # JSON 映射表加载与 O(1) 查找
├── DDSTransformRegistry.h/cc # 12 个内置数据转换器（四元数→欧拉角等）
└── DDSDataInjector.h/cc     # DDS 消息 → Fact::setRawValue() 桥接

resources/dds_mappings/
├── _default.json            # 标准 PX4 默认映射表（20 topic，70+ 字段）
└── _vendor_template.json    # 厂商映射表模板

src/Comms/
├── LinkConfiguration.h      # 添加了 TypeDDS 枚举值（#ifdef QGC_ENABLE_DDS 保护）
├── LinkConfiguration.cc     # 添加了 DDS 类型的字符串映射
├── LinkManager.cc           # 添加了 DDSLink 的创建逻辑
└── CMakeLists.txt           # 添加了 DDS 条件编译块

cmake/
└── CustomOptions.cmake      # 添加了 QGC_ENABLE_DDS 选项定义
```

### 5.2 成员初始化顺序（修复的核心知识点）

DDSLink 的成员变量声明顺序决定了初始化顺序：

```cpp
// DDSLink.h:59-67
DDSMappingEngine     _mappingEngine;       // ① 先初始化
DDSTransformRegistry _transformRegistry;    // ② 再初始化
DDSDataInjector      _dataInjector;         // ③ 最后初始化（依赖 ①②）
```

因此在构造函数中：

```cpp
DDSLink::DDSLink(SharedLinkConfigurationPtr &config, QObject *parent)
    : LinkInterface(config, parent)                          // 基类
    , _dataInjector(&_mappingEngine, &_transformRegistry, this)  // ③ 安全引用 ①②
```

C++ 标准保证成员变量按声明顺序初始化，所以 `_dataInjector` 初始化时 `_mappingEngine` 和 `_transformRegistry` 已经构造完毕，传递它们的指针是安全的。

---

## 六、已知限制与后续步骤

### 6.1 P0 已知限制

| 限制 | 原因 | 何时解决 |
|------|------|---------|
| DDS 连接不收发实际数据 | CycloneDDS API 调用为 stub | P1 阶段 |
| 不会显示飞行器 | 没有真实遥测数据注入 | P1 阶段 |
| 需要 GCC 13 编译 | Qt 6.10.3 MOC 与 GCC 12 不兼容 | QGC 上游修复或持续用 GCC 13 |

### 6.2 后续步骤

1. **升级 GCC 到 13** → 解决 VehicleComponent metatype 编译错误
2. **完成编译** → `ninja -j$(nproc)` 无错误
3. **执行第四节测试方案** → 验证 DDS 在 UI 中可见和可操作
4. **P1 开发** → 替换 CycloneDDS stub 为真实 API 调用

---

## 附录：完整编译命令参考

```bash
# 环境变量设置
export QT_DIR=~/Qt6.10/6.10.3/gcc_64
export CMAKE_PREFIX_PATH=$QT_DIR
export PATH=$QT_DIR/bin:$PATH

# 安装 GCC 13（如果还没装）
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install g++-13 gcc-13

# 拉取最新 DDS 分支代码
cd ~/QGC_DDS/qgc_dev
git checkout DDS
git pull

# 清理并重新配置
cd build
rm -rf *
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=$QT_DIR \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
    -DQGC_ENABLE_DDS=ON

# 编译
ninja -j$(nproc)

# 启动（带 DDS debug 日志）
export QT_LOGGING_RULES="Comms.DDSLink.debug=true"
./QGroundControl
```
