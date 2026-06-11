# P3 - Android 编译与测试手册

本文档描述如何在 Ubuntu 系统上交叉编译 QGC DDS_P3 分支的 Android APK，并在 Android 手机上进行测试。

**前提**：你已在虚拟机上安装好 Qt 6.10.3 PC 版本。

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

| 组件 | 版本 | 说明 |
|------|------|------|
| Ubuntu | 22.04+ | 编译主机 |
| Qt (PC/Host) | **6.10.3** ✅ 已安装 | 提供 moc/rcc/qmlcachegen 等编译工具 |
| Qt (Android Target) | **6.10.3 android_arm64_v8a** | ⚠️ 需额外安装 |
| Android SDK | Platform 35 | Command-line tools |
| Android NDK | r27c (27.2.12479018) | Qt 6.10 推荐版本 |
| Java JDK | 17 | Gradle 编译需要 |
| CMake | 3.25+ | 构建系统 |
| Ninja | 1.10+ | 构建后端 |
| CycloneDDS | 0.10.x | 需要交叉编译 Android ARM64 版本 |

### 磁盘空间

- Qt Android Target: ~3 GB（PC 版已有，无需重复）
- Android SDK + NDK: ~8 GB
- CycloneDDS 编译: ~200 MB
- QGC 编译: ~2 GB
- **总计额外需求: ≥15 GB**

---

## 2. 工具安装

### 2.1 确认已有的 Qt 安装路径

首先确认你的 Qt 6.10.3 PC 版安装路径：

```bash
# 通常是以下路径之一，请确认实际位置
ls ~/Qt/6.10.3/gcc_64/bin/qmake
# 或
ls /opt/Qt/6.10.3/gcc_64/bin/qmake
```

记录下这个路径，后续用作 `QT_HOST_PATH`：

```bash
# 根据你的实际安装路径设置（以下二选一，选你实际的路径）
export QT_HOST_PATH=$HOME/Qt/6.10.3/gcc_64
# 或
# export QT_HOST_PATH=/opt/Qt/6.10.3/gcc_64
```

### 2.2 安装 Qt 6.10.3 Android Target

你需要额外安装 Android ARM64 target 组件。有两种方式：

**方式一：通过 Qt Maintenance Tool（推荐，如果你用 Qt Installer 安装的）**

```bash
# 打开 Qt 维护工具
~/Qt/MaintenanceTool

# 选择 "Add or remove components"
# 勾选: Qt 6.10.3 → Android → Android ARM64-v8a
# 同时勾选以下模块（如果没有的话）：
#   - Qt Location
#   - Qt Positioning
#   - Qt Multimedia
#   - Qt Serial Port
#   - Qt WebSockets
#   - Qt HTTP Server
#   - Qt Connectivity
#   - Qt Sensors
#   - Qt Image Formats
#   - Qt Shader Tools
#   - Qt Quick 3D
```

**方式二：通过 aqtinstall（命令行，无需 Qt 账号）**

```bash
pip3 install aqtinstall

# 查看可用版本
aqt list-qt linux android --archives 6.10.3

# 下载 Android ARM64 target
# 注意: -O 指定安装到你已有的 Qt 目录，这样可以和 PC 版共存
aqt install-qt linux android 6.10.3 android_arm64_v8a \
    -m qtlocation qtpositioning qtspeech qtmultimedia qtserialport \
      qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors \
      qtwebsockets qthttpserver \
    -O $HOME/Qt
```

安装完成后确认：

```bash
ls ~/Qt/6.10.3/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake
# 应该存在此文件
```

### 2.3 基础编译工具

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git curl unzip \
    openjdk-17-jdk python3 python3-pip pkg-config \
    libgl1-mesa-dev libxkbcommon-dev libvulkan-dev
```

### 2.4 设置 JAVA_HOME

```bash
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
```

### 2.5 安装 Android SDK 和 NDK

```bash
# 创建 Android SDK 目录
export ANDROID_HOME=$HOME/Android/Sdk
mkdir -p $ANDROID_HOME

# 下载 command-line tools
cd /tmp
wget https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip -o commandlinetools-linux-*_latest.zip -d $ANDROID_HOME/cmdline-tools
mv $ANDROID_HOME/cmdline-tools/cmdline-tools $ANDROID_HOME/cmdline-tools/latest 2>/dev/null || true

# 添加到 PATH
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH

# 接受许可证
yes | sdkmanager --licenses

# 安装必要组件
sdkmanager "platform-tools" \
           "platforms;android-35" \
           "build-tools;35.0.0" \
           "ndk;27.2.12479018"

# 设置 NDK 路径
export ANDROID_NDK=$ANDROID_HOME/ndk/27.2.12479018
export ANDROID_PLATFORM=android-35
```

### 2.6 保存所有环境变量

将以下内容添加到 `~/.bashrc`：

```bash
cat >> ~/.bashrc << 'EOF'
# === QGC Android Build Environment ===

# Java
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64

# Android SDK/NDK
export ANDROID_HOME=$HOME/Android/Sdk
export ANDROID_NDK=$ANDROID_HOME/ndk/27.2.12479018
export ANDROID_PLATFORM=android-35
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH

# Qt 6.10.3 (修改为你的实际 Qt 安装路径)
export QT_HOST_PATH=$HOME/Qt/6.10.3/gcc_64
export QT_ROOT_DIR=$HOME/Qt/6.10.3/android_arm64_v8a
export PATH=$QT_HOST_PATH/bin:$PATH
EOF

source ~/.bashrc
```

### 2.7 验证环境

```bash
echo "Java: $(java -version 2>&1 | head -1)"
echo "CMake: $(cmake --version | head -1)"
echo "Ninja: $(ninja --version)"
echo "NDK: $ANDROID_NDK"
echo "Qt Host: $QT_HOST_PATH"
echo "Qt Target: $QT_ROOT_DIR"

# 验证关键文件存在
test -f $QT_HOST_PATH/bin/qmake && echo "✓ Qt Host OK" || echo "✗ Qt Host NOT FOUND"
test -f $QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake && echo "✓ Qt Android OK" || echo "✗ Qt Android NOT FOUND"
test -d $ANDROID_NDK && echo "✓ NDK OK" || echo "✗ NDK NOT FOUND"
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

### 3.2 编译 CycloneDDS for Android

```bash
cd $HOME/cyclonedds
mkdir -p build-android && cd build-android

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

### 3.3 验证编译结果

```bash
file $HOME/cyclonedds-android/lib/libddsc.so
# 预期输出: ELF 64-bit LSB shared object, ARM aarch64

ls $HOME/cyclonedds-android/include/dds/dds.h
# 应该存在
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
mkdir -p build-android && cd build-android

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

**参数说明**：

| 参数 | 值 | 说明 |
|------|-----|------|
| `CMAKE_TOOLCHAIN_FILE` | Qt 提供的 toolchain | 自动配置 NDK 交叉编译 |
| `QT_HOST_PATH` | 你已安装的 PC 版 Qt 6.10.3 | 提供编译工具（moc, rcc 等）|
| `QGC_ENABLE_DDS=ON` | 启用 DDS | 编译 CycloneDDS 相关代码 |
| `CycloneDDS_*` | 第 3 步编译的路径 | 交叉编译好的 Android 版 |

### 4.3 编译

```bash
cmake --build . --parallel $(nproc)
```

编译时间预计 15-30 分钟。

---

## 5. 生成 APK

### 5.1 编译成功后查找 APK

```bash
find build-android -name "*.apk" -type f
# 通常在: build-android/android-build/QGroundControl.apk
# 或: build-android/android-build/build/outputs/apk/debug/android-build-debug.apk
```

### 5.2 如果没有自动生成 APK

需要手动触发 androiddeployqt：

```bash
cd $HOME/qgc-android/build-android

$QT_HOST_PATH/bin/androiddeployqt \
    --input android-QGroundControl-deployment-settings.json \
    --output android-build \
    --android-platform $ANDROID_PLATFORM \
    --gradle
```

### 5.3 使用 Debug 签名（适用于本地测试）

如果编译时提示需要签名：

```bash
# 生成 debug keystore
keytool -genkey -v \
    -keystore $HOME/debug.keystore \
    -storepass android \
    -alias androiddebugkey \
    -keypass android \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US"

# 重新 cmake 配置添加签名
cd $HOME/qgc-android/build-android
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
    -DQT_ANDROID_SIGN_APK=ON \
    -DQT_ANDROID_KEYSTORE_PATH=$HOME/debug.keystore \
    -DQT_ANDROID_KEYSTORE_ALIAS=androiddebugkey \
    -DQT_ANDROID_KEYSTORE_STORE_PASS=android \
    -DQT_ANDROID_KEYSTORE_KEY_PASS=android \
    -G Ninja

cmake --build . --parallel $(nproc)
```

---

## 6. Android 设备测试

### 6.1 前提条件

| 条件 | 要求 |
|------|------|
| Android 版本 | 9.0 (API 28) 及以上 |
| CPU 架构 | ARM64 (arm64-v8a)，市面上绝大多数手机都是 |
| 网络 | 手机与 PX4 电脑在**同一 WiFi 网段** |
| USB 调试 | 开启（设置 → 开发者选项 → USB 调试）|

### 6.2 安装 APK

**方法一：ADB 安装（USB 连接）**

```bash
# 确认手机已连接
adb devices

# 安装 APK
adb install -r build-android/android-build/QGroundControl.apk
```

**方法二：文件传输**

1. 将 APK 文件发送到手机（微信/QQ/USB 复制）
2. 手机上用文件管理器打开 APK 安装
3. 需要允许"安装未知来源应用"

### 6.3 网络环境配置

DDS 使用 UDP 多播进行服务发现：

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

**关键检查**：
1. 手机和电脑连接**同一 WiFi**
2. 路由器允许 UDP 多播（家用路由器通常允许）
3. 电脑防火墙开放 UDP 7400-7500：
   ```bash
   sudo ufw allow 7400:7500/udp
   ```

### 6.4 启动 PX4 测试环境

```bash
# 终端 1: PX4 SITL
cd ~/PX4-Autopilot
make px4_sitl gz_x500

# 终端 2: DDS Agent
MicroXRCEAgent udp4 -p 8888
```

### 6.5 测试用例

#### TC-01: DDS 连接

| 步骤 | 操作 | 预期 |
|------|------|------|
| 1 | 确保 PX4 + Agent 运行 | Agent 显示 client connected |
| 2 | 手机打开 QGC | 启动成功 |
| 3 | 等待几秒 | QGC 显示遥测数据（GPS/姿态/电池）|

#### TC-02: Arm / Disarm

| 步骤 | 操作 | 预期 |
|------|------|------|
| 1 | QGC 点击 Arm | PX4 终端显示 Armed |
| 2 | QGC 点击 Disarm | PX4 终端显示 Disarmed |

#### TC-03: 起飞 / 降落

| 步骤 | 操作 | 预期 |
|------|------|------|
| 1 | 点击 Takeoff | 仿真无人机升空到默认高度 |
| 2 | 等待稳定（~10s） | 无人机悬停 |
| 3 | 点击 Land | 无人机降落 |

#### TC-04: 飞行模式切换

| 步骤 | 操作 | 预期 |
|------|------|------|
| 1 | 起飞后点击模式 | 显示可用模式列表 |
| 2 | 切到 Position | PX4 显示模式切换 |
| 3 | 切到 Hold | 无人机原地悬停 |

#### TC-05: Go To 指点飞行

| 步骤 | 操作 | 预期 |
|------|------|------|
| 1 | 起飞并稳定 | 悬停中 |
| 2 | 点击地图某位置 → "Go to here" | 无人机飞往该位置 |
| 3 | 观察到达后 | 无人机在目标位置悬停 |

#### TC-06: 虚拟摇杆

| 步骤 | 操作 | 预期 |
|------|------|------|
| 1 | 起飞后，PX4 终端：`commander mode posctl` | 切到 Position 模式 |
| 2 | 触摸拖动虚拟摇杆 | 无人机按方向移动 |
| 3 | 松开摇杆 | 无人机悬停 |

#### TC-07: RTL 返航

| 步骤 | 操作 | 预期 |
|------|------|------|
| 1 | 飞行到离起飞点一段距离 | — |
| 2 | 点击 Return | 无人机自动返回起飞点 |
| 3 | 等待 | 自动降落 |

### 6.6 测试记录表

| 测试项 | 结果 | 备注 |
|--------|------|------|
| TC-01 DDS 连接 | ⬜ Pass / ⬜ Fail | |
| TC-02 Arm/Disarm | ⬜ Pass / ⬜ Fail | |
| TC-03 起飞/降落 | ⬜ Pass / ⬜ Fail | |
| TC-04 模式切换 | ⬜ Pass / ⬜ Fail | |
| TC-05 Go To 指点 | ⬜ Pass / ⬜ Fail | |
| TC-06 虚拟摇杆 | ⬜ Pass / ⬜ Fail | |
| TC-07 RTL 返航 | ⬜ Pass / ⬜ Fail | |

---

## 7. 常见问题排查

### 7.1 CMake 配置失败: "Qt6 not found"

```
确认 QT_ROOT_DIR 指向 android_arm64_v8a 目录:
ls $QT_ROOT_DIR/lib/cmake/Qt6/Qt6Config.cmake
```

如果文件不存在，说明 Android target 没有安装成功，重新执行 2.2 节。

### 7.2 编译报错: "CycloneDDS not found"

```bash
# 确认交叉编译的 CycloneDDS 存在
ls $HOME/cyclonedds-android/lib/libddsc.so
ls $HOME/cyclonedds-android/include/dds/dds.h

# 如果不存在，重新执行第 3 节
```

### 7.3 QGC 启动后无法发现 PX4

**排查步骤**：
```bash
# 1. 确认 DDS Agent 在运行
ps aux | grep MicroXRCEAgent

# 2. 确认同一网段
ip addr show  # 电脑 IP
# 手机: 设置 → WLAN → 查看 IP

# 3. 开放防火墙
sudo ufw allow 7400:7500/udp

# 4. 测试网络连通
ping <手机IP>  # 从电脑 ping 手机
```

### 7.4 QGC 闪退

```bash
# 查看 Android 系统日志
adb logcat | grep -iE "qground|crash|fatal|dds|signal"
```

常见原因：
- OpenGL ES 兼容性问题 → 重新编译为 Debug 版检查日志
- CycloneDDS 库没有打包到 APK → 检查 APK 内容：
  ```bash
  unzip -l QGroundControl.apk | grep libddsc
  ```

### 7.5 虚拟摇杆无响应

1. 确认已切到 Position 模式
2. PX4 终端验证：`listener manual_control_input`
3. 检查参数：`param show COM_RC_IN_MODE`（不应为 4）

### 7.6 APK 安装失败

| 错误 | 解决 |
|------|------|
| `INSTALL_FAILED_NO_MATCHING_ABIS` | 手机不是 ARM64，需编译 armeabi-v7a |
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | 先卸载: `adb uninstall org.mavlink.qgroundcontrol` |
| `INSTALL_FAILED_OLDER_SDK` | 手机 Android 版本过低（需≥9.0） |

### 7.7 Gradle 编译错误

```bash
# 确认 Java 版本
java -version  # 需要 openjdk 17

# 如果是 Java 版本不对
sudo update-alternatives --config java  # 选择 Java 17
```

---

## 附录 A：一键编译脚本

保存为 `build_android.sh` 并执行：

```bash
#!/bin/bash
set -e

# ============ 配置区（根据实际路径修改）============
QT_HOST_PATH=$HOME/Qt/6.10.3/gcc_64
QT_ROOT_DIR=$HOME/Qt/6.10.3/android_arm64_v8a
ANDROID_HOME=$HOME/Android/Sdk
ANDROID_NDK=$ANDROID_HOME/ndk/27.2.12479018
ANDROID_PLATFORM=android-35
CYCLONEDDS_ANDROID=$HOME/cyclonedds-android
QGC_SOURCE=$HOME/qgc-android
# ==================================================

export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export PATH=$QT_HOST_PATH/bin:$ANDROID_HOME/cmdline-tools/latest/bin:$PATH

echo "========================================="
echo " QGC DDS Android Build (Qt 6.10.3)"
echo "========================================="
echo "Qt Host:    $QT_HOST_PATH"
echo "Qt Target:  $QT_ROOT_DIR"
echo "NDK:        $ANDROID_NDK"
echo "CycloneDDS: $CYCLONEDDS_ANDROID"
echo "Source:     $QGC_SOURCE"
echo ""

# 验证环境
for f in "$QT_HOST_PATH/bin/qmake" \
         "$QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake" \
         "$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
         "$CYCLONEDDS_ANDROID/lib/libddsc.so"; do
    if [ ! -f "$f" ]; then
        echo "ERROR: Missing file: $f"
        exit 1
    fi
done
echo "✓ Environment verified"
echo ""

cd "$QGC_SOURCE"
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

echo ""
echo ">>> Building ($(nproc) threads)..."
cmake --build . --parallel $(nproc)

echo ""
echo "========================================="
echo " Build Complete!"
echo "========================================="
echo "APK location:"
find . -name "*.apk" -type f
echo ""
echo "Install: adb install -r <apk_path>"
```

使用方法：

```bash
chmod +x build_android.sh
./build_android.sh
```

---

## 附录 B：与实际无人机测试

如果用真实 PX4 飞控（而非 SITL）：

1. 飞控通过 WiFi/以太网连接到路由器
2. 飞控上运行 Micro-XRCE-DDS Client（PX4 固件自带）
3. 路由器上或地面站电脑运行 DDS Agent
4. 手机连同一 WiFi

**飞控 PX4 参数**：
```
UXRCE_DDS_CFG = 0 (UDP)
UXRCE_DDS_AG_IP = <Agent设备IP>
UXRCE_DDS_PRT = 8888
```

**Agent 启动**：
```bash
MicroXRCEAgent udp4 -p 8888
```
