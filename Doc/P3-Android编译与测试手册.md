# P3 - Android 编译与测试手册

本文档描述如何在 Ubuntu 系统上交叉编译 QGC DDS_P3 分支的 Android APK，并在 Android 手机上进行测试。

---

## 目录

1. [环境要求](#1-环境要求)
2. [工具安装](#2-工具安装)
3. [CycloneDDS 交叉编译](#3-cyclonedds-交叉编译)
4. [QGC Android 编译](#4-qgc-android-编译)
5. [生成 APK](#5-生成-apk)
6. [Android 设备测试](#6-android-设备测试)
7. [常见问题排查](#7-常见问题排查)

---

## 1. 环境要求

| 组件 | 版本要求 | 说明 |
|------|---------|------|
| Ubuntu | 22.04+ | 编译主机 |
| Qt | 6.8.x（Desktop + Android arm64_v8a） | 需要 Host 和 Target 两份 |
| Android SDK | Platform 34+ | Command-line tools |
| Android NDK | r27c (27.2.12479018) | 与 Qt 6.8 兼容 |
| Java JDK | 17 | Gradle 编译需要 |
| CMake | 3.25+ | 构建系统 |
| Ninja | 1.10+ | 构建后端 |
| CycloneDDS | 0.10.x | 需要交叉编译 Android ARM64 版本 |
| Python | 3.10+ | aqtinstall 安装 Qt |

### 磁盘空间

- Qt Desktop + Android: ~6 GB
- Android SDK + NDK: ~8 GB
- CycloneDDS 编译: ~200 MB
- QGC 编译: ~2 GB
- **总计建议预留: ≥20 GB**

---

## 2. 工具安装

### 2.1 基础工具

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git curl unzip \
    openjdk-17-jdk python3 python3-pip pkg-config \
    libgl1-mesa-dev libxkbcommon-dev libvulkan-dev
```

### 2.2 设置 JAVA_HOME

```bash
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
echo 'export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64' >> ~/.bashrc
```

### 2.3 安装 aqtinstall（用于下载 Qt）

```bash
pip3 install aqtinstall
```

### 2.4 下载 Qt 6.8（Desktop Host + Android Target）

```bash
# 设定安装目录
export QT_INSTALL_DIR=$HOME/Qt

# 下载 Qt Desktop (Host tools，编译 QML 等需要)
aqt install-qt linux desktop 6.8.0 linux_gcc_64 \
    -m qtlocation qtpositioning qtspeech qtmultimedia qtserialport \
      qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors \
      qtwebsockets qthttpserver \
    -O $QT_INSTALL_DIR

# 下载 Qt Android ARM64 (Target)
aqt install-qt linux android 6.8.0 android_arm64_v8a \
    -m qtlocation qtpositioning qtspeech qtmultimedia qtserialport \
      qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors \
      qtwebsockets qthttpserver \
    -O $QT_INSTALL_DIR
```

> **注意**：如果 6.8.0 不可用，请用 `aqt list-qt linux desktop` 和 `aqt list-qt linux android` 查看可用版本，选择 6.8.x 中最新的版本。

### 2.5 设置 Qt 环境变量

```bash
export QT_HOST_PATH=$QT_INSTALL_DIR/6.8.0/gcc_64
export QT_ROOT_DIR=$QT_INSTALL_DIR/6.8.0/android_arm64_v8a
export PATH=$QT_HOST_PATH/bin:$PATH
```

### 2.6 安装 Android SDK 和 NDK

```bash
# 创建 Android SDK 目录
export ANDROID_HOME=$HOME/Android/Sdk
mkdir -p $ANDROID_HOME

# 下载 command-line tools
cd /tmp
wget https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip commandlinetools-linux-*_latest.zip -d $ANDROID_HOME/cmdline-tools
mv $ANDROID_HOME/cmdline-tools/cmdline-tools $ANDROID_HOME/cmdline-tools/latest

# 添加到 PATH
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH

# 接受许可证
yes | sdkmanager --licenses

# 安装必要组件
sdkmanager "platform-tools" \
           "platforms;android-34" \
           "build-tools;35.0.0" \
           "ndk;27.2.12479018"

# 设置 NDK 路径
export ANDROID_NDK=$ANDROID_HOME/ndk/27.2.12479018
export ANDROID_PLATFORM=android-34
```

### 2.7 保存环境变量

将以下内容添加到 `~/.bashrc`（或 `~/.profile`）：

```bash
cat >> ~/.bashrc << 'EOF'
# Android Development
export ANDROID_HOME=$HOME/Android/Sdk
export ANDROID_NDK=$ANDROID_HOME/ndk/27.2.12479018
export ANDROID_PLATFORM=android-34
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH

# Qt
export QT_INSTALL_DIR=$HOME/Qt
export QT_HOST_PATH=$QT_INSTALL_DIR/6.8.0/gcc_64
export QT_ROOT_DIR=$QT_INSTALL_DIR/6.8.0/android_arm64_v8a
export PATH=$QT_HOST_PATH/bin:$PATH

# Java
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
EOF

source ~/.bashrc
```

---

## 3. CycloneDDS 交叉编译

QGC DDS 功能依赖 CycloneDDS 库，需要为 Android ARM64 交叉编译。

### 3.1 下载 CycloneDDS 源码

```bash
cd $HOME
git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
cd cyclonedds
git checkout 0.10.5  # 使用稳定版本
```

### 3.2 创建 Android 交叉编译工具链文件

```bash
cat > $HOME/cyclonedds/android-arm64.cmake << 'EOF'
# Android ARM64 cross-compilation toolchain for CycloneDDS
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 34)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK})
set(CMAKE_ANDROID_STL_TYPE c++_shared)

# Disable features not needed on Android
set(BUILD_SHARED_LIBS ON)
set(BUILD_IDLC OFF)
set(BUILD_DDSPERF OFF)
set(BUILD_EXAMPLES OFF)
set(BUILD_TESTING OFF)
set(ENABLE_SSL OFF)
set(ENABLE_SECURITY OFF)
set(ENABLE_SHM OFF)
EOF
```

### 3.3 编译 CycloneDDS

```bash
cd $HOME/cyclonedds
mkdir build-android && cd build-android

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=$ANDROID_PLATFORM \
    -DANDROID_STL=c++_shared \
    -DCMAKE_INSTALL_PREFIX=$HOME/cyclonedds-android \
    -DBUILD_IDLC=OFF \
    -DBUILD_DDSPERF=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DENABLE_SSL=OFF \
    -DENABLE_SECURITY=OFF \
    -DENABLE_SHM=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja

ninja -j$(nproc)
ninja install
```

### 3.4 验证编译结果

```bash
# 应该看到 ARM64 的共享库
file $HOME/cyclonedds-android/lib/libddsc.so
# 输出应包含: ELF 64-bit LSB shared object, ARM aarch64

ls $HOME/cyclonedds-android/include/dds/
# 应包含 dds.h 等头文件
```

---

## 4. QGC Android 编译

### 4.1 获取源码

```bash
cd $HOME
git clone https://github.com/L-a-L-max/qgc_dev.git qgc-android
cd qgc-android
git checkout DDS_P3
git submodule update --init --recursive
```

### 4.2 配置 CMake

```bash
cd $HOME/qgc-android
mkdir build-android && cd build-android

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake \
    -DCMAKE_PREFIX_PATH=$QT_ROOT_DIR \
    -DQT_HOST_PATH=$QT_HOST_PATH \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=$ANDROID_PLATFORM \
    -DANDROID_NDK=$ANDROID_NDK \
    -DQT_ANDROID_ABIS=arm64-v8a \
    -DQGC_ENABLE_DDS=ON \
    -DCycloneDDS_INCLUDE_DIR=$HOME/cyclonedds-android/include \
    -DCycloneDDS_LIBRARY=$HOME/cyclonedds-android/lib/libddsc.so \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_ANDROID_SIGN_APK=OFF \
    -G Ninja
```

> **关键参数说明**：
> - `CMAKE_TOOLCHAIN_FILE`: 使用 Qt 提供的 Android 工具链（内部会调用 NDK 工具链）
> - `QT_HOST_PATH`: Host Qt 路径，编译 QML/MOC 等生成工具需要
> - `QGC_ENABLE_DDS=ON`: 启用 DDS 功能
> - `CycloneDDS_*`: 指向交叉编译好的 CycloneDDS

### 4.3 编译

```bash
cmake --build . --parallel $(nproc)
```

编译时间预计 15-30 分钟（取决于 CPU 核心数和内存）。

### 4.4 如果编译报错

**常见问题 1：找不到 CycloneDDS**

确认 `FindCycloneDDS.cmake` 中的搜索路径包含 `$HOME/cyclonedds-android`：

```bash
# 在 cmake 配置时额外添加：
-DCMAKE_PREFIX_PATH="$QT_ROOT_DIR;$HOME/cyclonedds-android"
```

**常见问题 2：Qt QML 模块找不到**

确认 Host Qt 和 Target Qt 版本完全一致（都是 6.8.0）。

**常见问题 3：Gradle 相关错误**

```bash
# 确认 JAVA_HOME 正确
echo $JAVA_HOME
java -version  # 应该显示 openjdk 17
```

---

## 5. 生成 APK

### 5.1 生成 Debug APK（无需签名）

编译成功后，APK 会在以下位置生成：

```bash
ls build-android/android-build/*.apk
# 或
ls build-android/android-build/build/outputs/apk/debug/*.apk
```

### 5.2 如果自动打包没触发

手动触发 androiddeployqt：

```bash
cd build-android

# androiddeployqt 由 Qt 的 CMake 集成自动调用
# 如果需要手动执行：
$QT_HOST_PATH/bin/androiddeployqt \
    --input android-QGroundControl-deployment-settings.json \
    --output android-build \
    --android-platform $ANDROID_PLATFORM \
    --gradle
```

### 5.3 创建 Debug 签名（如果需要）

```bash
keytool -genkey -v \
    -keystore $HOME/debug.keystore \
    -storepass android \
    -alias androiddebugkey \
    -keypass android \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US"

# 重新配置 cmake 添加签名参数：
cmake .. \
    ... (其他参数不变) \
    -DQT_ANDROID_SIGN_APK=ON \
    -DQT_ANDROID_KEYSTORE_PATH=$HOME/debug.keystore \
    -DQT_ANDROID_KEYSTORE_ALIAS=androiddebugkey \
    -DQT_ANDROID_KEYSTORE_STORE_PASS=android \
    -DQT_ANDROID_KEYSTORE_KEY_PASS=android

cmake --build . --parallel $(nproc)
```

---

## 6. Android 设备测试

### 6.1 前提条件

| 条件 | 要求 |
|------|------|
| Android 版本 | 9.0 (API 28) 及以上 |
| CPU 架构 | ARM64 (arm64-v8a) |
| 网络 | 手机与 PX4 电脑在**同一 WiFi 网段** |
| USB 调试 | 开启（设置 → 开发者选项 → USB 调试）|

### 6.2 安装 APK

**方法一：ADB 安装**

```bash
# 连接手机到 Ubuntu（USB 线）
adb devices       # 确认设备已连接
adb install -r build-android/android-build/QGroundControl.apk
```

**方法二：文件传输安装**

1. 将 APK 文件复制到手机存储
2. 手机上通过文件管理器找到 APK 并安装
3. 需要允许"安装未知来源应用"

### 6.3 网络配置

DDS 使用 UDP 多播进行服务发现，需要确保：

1. **手机和 PX4 电脑连接同一 WiFi 路由器**
2. **路由器允许 UDP 多播**（家用路由器通常允许）
3. **防火墙不阻止 UDP 端口 7400-7500**

如果多播不可用，可以在 QGC 中配置 DDS 对端 IP（单播模式）。

### 6.4 PX4 SITL 测试环境搭建

在 Ubuntu 电脑上启动 PX4 + DDS Agent：

```bash
# 终端 1: 启动 PX4 SITL
cd ~/PX4-Autopilot
make px4_sitl gz_x500

# 终端 2: 启动 Micro-XRCE-DDS Agent
MicroXRCEAgent udp4 -p 8888
```

### 6.5 测试流程

#### 测试一：DDS 连接

1. 确保 PX4 + DDS Agent 已启动
2. 手机打开 QGC DDS
3. **预期**：QGC 显示连接状态，能看到遥测数据（GPS 坐标、姿态、电池等）
4. **验证**：飞行仪表盘上显示 PX4 状态

#### 测试二：基本命令（Arm/Disarm/起飞/降落）

1. 确认 QGC 已连接 PX4
2. 点击 "Arm" → PX4 解锁
3. 点击 "Takeoff" → 无人机起飞
4. 等待稳定后点击 "Land" → 降落
5. **预期**：每个命令 PX4 终端有响应日志，仿真无人机执行动作

#### 测试三：飞行模式切换

1. 无人机起飞后
2. 切换到不同飞行模式（Position / Hold / Return）
3. **预期**：PX4 终端显示模式切换成功

#### 测试四：Go To（指点飞行）

1. 无人机起飞并稳定
2. 在地图上点击一个位置 → 选择 "Go to here"
3. **预期**：无人机飞往目标位置

#### 测试五：虚拟摇杆控制

1. 无人机起飞后
2. 在 PX4 终端执行 `commander mode posctl` 切到 Position 模式
3. 触摸虚拟摇杆进行操控
4. **预期**：无人机响应摇杆输入，按方向移动

#### 测试六：RTL（返航）

1. 无人机飞行到一定距离
2. 点击 "Return" 按钮
3. **预期**：无人机自动返回起飞点并降落

### 6.6 测试记录表

| 测试项 | 结果 | 备注 |
|--------|------|------|
| DDS 连接 | ⬜ Pass / ⬜ Fail | |
| Arm/Disarm | ⬜ Pass / ⬜ Fail | |
| 起飞/降落 | ⬜ Pass / ⬜ Fail | |
| 模式切换 | ⬜ Pass / ⬜ Fail | |
| Go To 指点 | ⬜ Pass / ⬜ Fail | |
| 虚拟摇杆 | ⬜ Pass / ⬜ Fail | |
| RTL 返航 | ⬜ Pass / ⬜ Fail | |

---

## 7. 常见问题排查

### 7.1 QGC 无法发现 PX4

**现象**：QGC 启动后无遥测数据

**排查步骤**：
```bash
# 1. 确认 DDS Agent 在运行
ps aux | grep MicroXRCEAgent

# 2. 确认手机和电脑在同一网段
# 电脑端查看 IP
ip addr show wlan0  # 或 eth0

# 手机端查看 IP
# 设置 → 关于手机 → 状态 → IP 地址

# 3. 测试 UDP 连通性（在电脑上）
# 发送测试 UDP 包
echo "test" | nc -u <手机IP> 7400

# 4. 确认防火墙未阻止
sudo ufw status
sudo ufw allow 7400:7500/udp
```

**解决方案**：
- 确保同一网段 + 路由器允许多播
- 如果不行，在 QGC DDS 配置中设置对端 IP 为电脑 IP（单播模式）

### 7.2 QGC 闪退

**排查方法**：
```bash
# 通过 adb 查看 logcat
adb logcat | grep -i "qground\|crash\|fatal\|dds"
```

**常见原因**：
- OpenGL ES 不兼容：尝试在 QGC 设置中切换渲染后端
- 库缺失：确认 libddsc.so 已打包到 APK 中

### 7.3 虚拟摇杆无响应

**排查步骤**：
1. 确认已切到 Position 模式（PX4 终端：`commander mode posctl`）
2. 确认 DDS 数据在发送（看 QGC 日志或 PX4 终端 `listener manual_control_input`）
3. 确认 PX4 参数 `COM_RC_IN_MODE` 不是 4 (Disabled)

### 7.4 编译时找不到头文件

```bash
# 确认环境变量已加载
echo $ANDROID_NDK
echo $QT_HOST_PATH
echo $QT_ROOT_DIR

# 如果为空，重新 source
source ~/.bashrc
```

### 7.5 APK 安装失败

```
INSTALL_FAILED_NO_MATCHING_ABIS
```
→ 手机 CPU 不是 ARM64，需要编译 armeabi-v7a 版本（修改 `-DANDROID_ABI=armeabi-v7a`）

```
INSTALL_FAILED_UPDATE_INCOMPATIBLE
```
→ 先卸载旧版本：`adb uninstall org.mavlink.qgroundcontrol`

---

## 附录 A：一键编译脚本

将以下脚本保存为 `build_android.sh`：

```bash
#!/bin/bash
set -e

# ============ 配置区 ============
QT_VERSION="6.8.0"
QT_INSTALL_DIR=$HOME/Qt
ANDROID_HOME=$HOME/Android/Sdk
ANDROID_NDK=$ANDROID_HOME/ndk/27.2.12479018
ANDROID_PLATFORM=android-34
CYCLONEDDS_ANDROID=$HOME/cyclonedds-android
QGC_SOURCE=$HOME/qgc-android
# ================================

export QT_HOST_PATH=$QT_INSTALL_DIR/$QT_VERSION/gcc_64
export QT_ROOT_DIR=$QT_INSTALL_DIR/$QT_VERSION/android_arm64_v8a
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export PATH=$QT_HOST_PATH/bin:$ANDROID_HOME/cmdline-tools/latest/bin:$PATH

echo "=== QGC Android Build ==="
echo "Qt Host: $QT_HOST_PATH"
echo "Qt Target: $QT_ROOT_DIR"
echo "NDK: $ANDROID_NDK"
echo "CycloneDDS: $CYCLONEDDS_ANDROID"
echo ""

cd $QGC_SOURCE
mkdir -p build-android && cd build-android

echo ">>> Configuring..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake \
    -DCMAKE_PREFIX_PATH=$QT_ROOT_DIR \
    -DQT_HOST_PATH=$QT_HOST_PATH \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=$ANDROID_PLATFORM \
    -DANDROID_NDK=$ANDROID_NDK \
    -DQT_ANDROID_ABIS=arm64-v8a \
    -DQGC_ENABLE_DDS=ON \
    -DCycloneDDS_INCLUDE_DIR=$CYCLONEDDS_ANDROID/include \
    -DCycloneDDS_LIBRARY=$CYCLONEDDS_ANDROID/lib/libddsc.so \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_ANDROID_SIGN_APK=OFF \
    -G Ninja

echo ">>> Building..."
cmake --build . --parallel $(nproc)

echo ""
echo "=== Build Complete ==="
echo "APK location:"
find . -name "*.apk" -type f
```

---

## 附录 B：实机测试网络拓扑

```
┌─────────────────┐         WiFi          ┌─────────────────┐
│  Ubuntu 电脑     │◄──────────────────────►│  Android 手机    │
│                 │    同一局域网           │                 │
│  ┌───────────┐  │                        │  ┌───────────┐  │
│  │ PX4 SITL  │  │                        │  │ QGC DDS   │  │
│  │ (gz_x500) │  │                        │  │ (APK)     │  │
│  └─────┬─────┘  │                        │  └─────┬─────┘  │
│        │ UDP    │                        │        │        │
│  ┌─────▼─────┐  │                        │        │ DDS    │
│  │ DDS Agent │  │     UDP Multicast      │        │ (UDP)  │
│  │ (XRCE)   │◄─┼────────────────────────┼────────┘        │
│  └───────────┘  │    端口 7400-7500      │                 │
└─────────────────┘                        └─────────────────┘
```

### 网络流向

1. PX4 SITL ↔ DDS Agent：UDP 8888（本机）
2. DDS Agent ↔ QGC Android：UDP 多播 239.255.0.1:7400（发现）+ 动态 UDP 端口（数据）

---

## 附录 C：与实际无人机测试

如果用真实 PX4 飞控（而非 SITL）：

1. 飞控通过 WiFi/以太网连接到路由器
2. 飞控上运行 Micro-XRCE-DDS Client（PX4 固件自带）
3. 路由器上或另一台设备运行 DDS Agent
4. 手机连同一 WiFi

```bash
# 在路由器/网关设备上运行 Agent
MicroXRCEAgent udp4 -p 8888
```

飞控 PX4 参数：
```
UXRCE_DDS_CFG = 0 (UDP)
UXRCE_DDS_AG_IP = <Agent设备IP>
UXRCE_DDS_PRT = 8888
```
