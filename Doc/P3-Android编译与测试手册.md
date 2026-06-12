# P3 - Android 编译与测试手册

本文档描述如何在 Ubuntu 系统上交叉编译 QGC（DDS_P3 / DDS_P5 分支）的 Android APK，并在 Android 手机上进行测试。

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
- [附录 C：常见编译错误与解决方案](#附录-c常见编译错误与解决方案)
- [附录 D：编译成功与 APK 签名安装](#附录-d编译成功与-apk-签名安装)

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


---

## 附录 C：常见编译错误与解决方案

### 错误 1：NDK Toolchain 路径不匹配

**现象**：
```
CMake Warning: The toolchain file to be chainloaded
'/opt/android/android-ndk-r27c/build/cmake/android.toolchain.cmake' does not exist.
```

**原因**：Qt 6.10.3 Android 组件安装时记录了 NDK 路径为 `/opt/android/android-ndk-r27c/`，但实际 NDK 安装在其他位置（如 `~/Android/Sdk/ndk/27.x.xxx/`）。

**解决方案**（二选一）：

**方案 A：创建符号链接（推荐）**
```bash
# 1. 找到实际 NDK 路径
ls ~/Android/Sdk/ndk/
# 输出类似：27.2.12479018

# 2. 创建符号链接
sudo mkdir -p /opt/android/
sudo ln -s ~/Android/Sdk/ndk/27.2.12479018 /opt/android/android-ndk-r27c

# 3. 验证
ls /opt/android/android-ndk-r27c/build/cmake/android.toolchain.cmake
```

**方案 B：CMake 参数覆盖**
```bash
# 在 cmake 配置时显式传入 NDK 路径
cmake -B build-android -S . \
    -DCMAKE_TOOLCHAIN_FILE=~/Qt/6.10.3/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake \
    -DQT_HOST_PATH=~/Qt/6.10.3/gcc_64 \
    -DANDROID_SDK_ROOT=~/Android/Sdk \
    -DANDROID_NDK=~/Android/Sdk/ndk/27.2.12479018 \
    -DANDROID_NDK_ROOT=~/Android/Sdk/ndk/27.2.12479018 \
    -DANDROID_PLATFORM=android-34 \
    -DANDROID_ABI=arm64-v8a \
    -G Ninja
```

### 错误 2：Qt6DBus 找不到（qtkeychain 依赖）

**现象**：
```
Could NOT find Qt6DBus (missing: Qt6DBus_DIR)
CMake Error at .cache/CPM/qtkeychain/aedf/CMakeLists.txt:107
  Failed to find required Qt component "DBus".
```

**原因**：这是错误 1 的**连锁反应**。因为 NDK toolchain 加载失败，CMake 的 `ANDROID` 变量没有被设为 true。`qtkeychain` 库的 CMakeLists.txt 中有如下逻辑：
```cmake
if(UNIX AND NOT APPLE AND NOT ANDROID AND NOT HAIKU)
    find_package(Qt6 COMPONENTS DBus REQUIRED)  # ← Android 时应跳过此行
```
由于 `ANDROID` 未定义，构建系统误认为在给 Linux 编译，去查找 `Qt6DBus`（Android 上不存在此模块）。

**解决方案**：修复错误 1（NDK 路径）后，此错误自动消失。`ANDROID` 变量被正确设置后，`qtkeychain` 会使用 Android Keystore 后端而非 DBus。

**注意**：`qtkeychain` 是 QGC 的必要依赖（用于安全存储凭据），不能跳过。在 Android 上它使用 Android Keystore API，不依赖 DBus。

### 错误排查流程图

```
编译报错
  ├── NDK Warning (toolchain not exist)
  │     └── 修复：创建符号链接 或 传 -DANDROID_NDK 参数
  │           └── 修复后重新 cmake 配置（需删除 build 目录）
  │
  ├── Qt6DBus not found
  │     └── 根因：NDK 路径导致 ANDROID 变量未设置
  │     └── 修复：先修复 NDK 路径问题
  │
  └── 其他 find_package 失败
        └── 检查 CMAKE_PREFIX_PATH 是否包含 Qt Android 路径
        └── 检查 QT_HOST_PATH 是否指向 Desktop Qt
```

### 验证修复成功

```bash
# 修复后重新配置
rm -rf build-android
cmake -B build-android ...  # 使用上述修正后的参数

# 检查输出中应看到：
# -- Android NDK: /path/to/ndk/27.x
# -- Android ABI: arm64-v8a
# -- Android platform: android-34
# -- Configuring done

# 如果看到 "Configuring done" 且无 Error，说明配置成功
cmake --build build-android --parallel
```


---

## 附录 C：常见编译错误与解决方案

### 错误 1：NDK Toolchain 路径不匹配

**现象**：
```
CMake Warning: The toolchain file to be chainloaded
'/opt/android/android-ndk-r27c/build/cmake/android.toolchain.cmake' does not exist.
```

**原因**：Qt 6.10.3 Android 组件安装时记录了 NDK 路径为 `/opt/android/android-ndk-r27c/`，但实际 NDK 安装在其他位置（如 `~/Android/Sdk/ndk/27.x.xxx/`）。

**解决方案**（二选一）：

**方案 A：创建符号链接（推荐）**
```bash
# 1. 找到实际 NDK 路径
ls ~/Android/Sdk/ndk/
# 输出类似：27.2.12479018

# 2. 创建符号链接
sudo mkdir -p /opt/android/
sudo ln -s ~/Android/Sdk/ndk/27.2.12479018 /opt/android/android-ndk-r27c

# 3. 验证
ls /opt/android/android-ndk-r27c/build/cmake/android.toolchain.cmake
```

**方案 B：CMake 参数覆盖**
```bash
# 在 cmake 配置时显式传入 NDK 路径
cmake -B build-android -S . \
    -DCMAKE_TOOLCHAIN_FILE=~/Qt/6.10.3/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake \
    -DQT_HOST_PATH=~/Qt/6.10.3/gcc_64 \
    -DANDROID_SDK_ROOT=~/Android/Sdk \
    -DANDROID_NDK=~/Android/Sdk/ndk/27.2.12479018 \
    -DANDROID_NDK_ROOT=~/Android/Sdk/ndk/27.2.12479018 \
    -DANDROID_PLATFORM=android-34 \
    -DANDROID_ABI=arm64-v8a \
    -G Ninja
```

### 错误 2：Qt6DBus 找不到（qtkeychain 依赖）

**现象**：
```
Could NOT find Qt6DBus (missing: Qt6DBus_DIR)
CMake Error at .cache/CPM/qtkeychain/aedf/CMakeLists.txt:107
  Failed to find required Qt component "DBus".
```

**原因**：这是错误 1 的**连锁反应**。因为 NDK toolchain 加载失败，CMake 的 `ANDROID` 变量没有被设为 true。`qtkeychain` 库的 CMakeLists.txt 中有如下逻辑：
```cmake
if(UNIX AND NOT APPLE AND NOT ANDROID AND NOT HAIKU)
    find_package(Qt6 COMPONENTS DBus REQUIRED)  # ← Android 时应跳过此行
```
由于 `ANDROID` 未定义，构建系统误认为在给 Linux 编译，去查找 `Qt6DBus`（Android 上不存在此模块）。

**解决方案**：修复错误 1（NDK 路径）后，此错误自动消失。`ANDROID` 变量被正确设置后，`qtkeychain` 会使用 Android Keystore 后端而非 DBus。

**注意**：`qtkeychain` 是 QGC 的必要依赖（用于安全存储凭据），不能跳过。在 Android 上它使用 Android Keystore API，不依赖 DBus。

### 错误排查流程图

```
编译报错
  ├── NDK Warning (toolchain not exist)
  │     └── 修复：创建符号链接 或 传 -DANDROID_NDK 参数
  │           └── 修复后重新 cmake 配置（需删除 build 目录）
  │
  ├── Qt6DBus not found
  │     └── 根因：NDK 路径导致 ANDROID 变量未设置
  │     └── 修复：先修复 NDK 路径问题
  │
  └── 其他 find_package 失败
        └── 检查 CMAKE_PREFIX_PATH 是否包含 Qt Android 路径
        └── 检查 QT_HOST_PATH 是否指向 Desktop Qt
```

### 验证修复成功

```bash
# 修复后重新配置
rm -rf build-android
cmake -B build-android ...  # 使用上述修正后的参数

# 检查输出中应看到：
# -- Android NDK: /path/to/ndk/27.x
# -- Android ABI: arm64-v8a
# -- Android platform: android-34
# -- Configuring done

# 如果看到 "Configuring done" 且无 Error，说明配置成功
cmake --build build-android --parallel
```


### 错误 3：Android SDK build-tools 路径不匹配

**现象**：
```
CMake Error at .../Qt6AndroidMacros.cmake:15 (message):
  Could not locate Android SDK build tools under "/opt/android/sdk/build-tools"
```

**原因**：与错误 1 类似，Qt 6.10.3 安装时将 Android SDK 路径硬编码为 `/opt/android/sdk/`，但实际 SDK 安装在 `~/Android/Sdk/`。

**解决方案**：

```bash
# 创建 SDK 符号链接
sudo ln -sf ~/Android/Sdk /opt/android/sdk

# 验证
ls /opt/android/sdk/build-tools/
# 应能看到版本号目录，如 35.0.0
```

### 一次性排查所有硬编码路径

Qt 6.10.3 Android 的 toolchain 文件中可能硬编码了多个 `/opt/android/` 路径。可以一次性排查并全部修复：

```bash
# 查看 Qt toolchain 中所有硬编码的 /opt/android 路径
grep -r "/opt/android" ~/Qt/6.10.3/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake

# 常见需要创建的符号链接：
sudo mkdir -p /opt/android/
sudo ln -sf ~/Android/Sdk/ndk/<你的NDK版本号> /opt/android/android-ndk-r27c
sudo ln -sf ~/Android/Sdk /opt/android/sdk

# 如果还有其他路径（如 /opt/android/openssl 等），按同样方式创建符号链接
```

### 更新后的错误排查流程图

```
Android 交叉编译报错
  │
  ├── NDK toolchain not exist
  │     └── sudo ln -sf ~/Android/Sdk/ndk/<版本> /opt/android/android-ndk-r27c
  │
  ├── Qt6DBus not found (qtkeychain)
  │     └── 根因：NDK 路径导致 ANDROID 变量未设置
  │     └── 修复 NDK 路径后自动解决
  │
  ├── SDK build-tools not found
  │     └── sudo ln -sf ~/Android/Sdk /opt/android/sdk
  │
  └── 其他 /opt/android/xxx not found
        └── grep -r "/opt/android" ~/Qt/6.10.3/.../qt.toolchain.cmake
        └── 对每个缺失路径创建符号链接
```

修复后务必**删除 build 目录重新配置**：
```bash
rm -rf build-android
cmake -B build-android ...  # 重新执行 cmake 配置
```


### 错误 4：Android NDK Clang 编译器警告升级为错误

**现象**：
```
src/DDS/DDSVehicleManager.cc:92:42: error: lambda capture 'vehicleId' is not
required to be captured for this use [-Werror,-Wunused-lambda-capture]
    QTimer::singleShot(100, this, [this, vehicleId]() {
                                         ~~^~~~~~~~~
```

**原因**：`vehicleId` 声明为 `constexpr int`，C++17 标准下 constexpr 变量不需要被 lambda 捕获（编译器直接内联常量值）。桌面版 GCC 不报此警告，但 Android NDK 的 Clang 18 启用了 `-Werror,-Wunused-lambda-capture`，将此警告视为错误。

**解决方案**：

已在 DDS_P3 分支修复（commit `9961215`）。修改 `src/DDS/DDSVehicleManager.cc` 第 92 行：

```cpp
// 修复前（lambda 捕获了 constexpr 变量）：
QTimer::singleShot(100, this, [this, vehicleId]() {

// 修复后（移除不必要的捕获）：
QTimer::singleShot(100, this, [this]() {
```

**拉取修复**：
```bash
cd ~/qgc-android
git pull origin DDS_P3
rm -rf build-android  # 清理旧构建
# 重新 cmake 配置 + 编译
```

**注意**：如果后续遇到类似的 `-Wunused-lambda-capture` 错误，说明代码中有其他不必要的 lambda 捕获。Android NDK Clang 比桌面 GCC 更严格，所有 `-Werror` 警告都会阻止编译。


### 错误 5：Gradle 下载 Android Gradle Plugin 失败（TLS 握手错误）

**现象**：
C++ 编译全部通过（2801/2801），但在最后生成 APK 时失败：
```
FAILURE: Build failed with an exception.
* What went wrong:
Could not resolve com.android.tools.build:gradle:9.0.1.
  > Could not GET '.../gradle-9.0.1.pom'.
    > The server may not support the client's requested TLS protocol versions: (TLSv1.2, TLSv1.3).
      > Remote host terminated the handshake
```

**原因**：Gradle 无法通过 HTTPS 连接到 Google Maven 仓库（`dl.google.com`）下载 Android Gradle Plugin 9.0.1。通常是以下原因之一：
- 企业网络/防火墙阻止了 HTTPS 出站连接
- 网络代理未配置
- Java 的 TLS/SSL 证书链不完整

**解决方案**（按顺序尝试）：

#### 方案 A：检查网络连通性

```bash
# 测试是否能访问 Google Maven
curl -I https://dl.google.com/dl/android/maven2/com/android/tools/build/gradle/9.0.1/gradle-9.0.1.pom

# 如果超时或被拒绝，说明网络有问题
# 如果返回 200/301，说明网络正常，问题在 Java TLS
```

#### 方案 B：配置网络代理（如果在代理环境下）

```bash
# 在 ~/.gradle/gradle.properties 中添加代理设置
mkdir -p ~/.gradle
cat >> ~/.gradle/gradle.properties << 'EOF'
systemProp.http.proxyHost=你的代理IP
systemProp.http.proxyPort=代理端口
systemProp.https.proxyHost=你的代理IP
systemProp.https.proxyPort=代理端口
EOF
```

#### 方案 C：修复 Java TLS 证书（最常见）

```bash
# 检查 Java 版本（需要 JDK 17+）
java -version

# 如果使用的是系统 Java，尝试更新 CA 证书
sudo apt-get update && sudo apt-get install -y ca-certificates-java
sudo update-ca-certificates -f

# 或者显式指定 TLS 版本
echo "org.gradle.jvmargs=-Dhttps.protocols=TLSv1.2,TLSv1.3" >> ~/.gradle/gradle.properties
```

#### 方案 D：使用国内镜像源（中国大陆网络推荐）

修改 Gradle 使用阿里云镜像：

```bash
mkdir -p ~/.gradle
cat > ~/.gradle/init.gradle << 'EOF'
allprojects {
    repositories {
        maven { url 'https://maven.aliyun.com/repository/google' }
        maven { url 'https://maven.aliyun.com/repository/central' }
        maven { url 'https://maven.aliyun.com/repository/gradle-plugin' }
        google()
        mavenCentral()
    }
    buildscript {
        repositories {
            maven { url 'https://maven.aliyun.com/repository/google' }
            maven { url 'https://maven.aliyun.com/repository/central' }
            maven { url 'https://maven.aliyun.com/repository/gradle-plugin' }
            google()
            mavenCentral()
        }
    }
}
EOF
```

#### 方案 E：使用 VPN

如果上述方案都不行，可能需要通过 VPN 访问 `dl.google.com`。

#### 重新编译 APK

修复网络后，不需要重新编译 C++，直接重新打包即可：

```bash
cd ~/qgc-android/build-android
cmake --build . --target QGroundControl_make_apk
# 或者直接重新 ninja
ninja
```


### 错误 6：Build Tools 36.0.0 缺失 + SSL 无法连接 dl.google.com

**现象**：
Gradle 配置成功（AGP 9.0.1 已通过镜像下载），但打包时报错：
```
Failed to find Build Tools revision 36.0.0
IOException: https://dl.google.com/android/repository/addons_list-6.xml
javax.net.ssl.SSLHandshakeException: Remote host terminated the handshake
```

**原因**：
- Android Gradle Plugin 9.0.1 要求 Build Tools ≥ 36.0.0
- 当前只安装了 Build Tools 35.0.0
- Gradle 尝试自动下载 36.0.0 但 `dl.google.com` 的 SSL 连接被阻断

**解决方案**（二选一）：

#### 方案 A：手动安装 Build Tools 36.0.0（推荐）

```bash
# 使用 sdkmanager 安装（需要能访问 dl.google.com）
# 如果直连可以：
~/Android/Sdk/cmdline-tools/latest/bin/sdkmanager "build-tools;36.0.0"

# 如果直连不行，通过代理：
~/Android/Sdk/cmdline-tools/latest/bin/sdkmanager --proxy=http --proxy_host=代理IP --proxy_port=端口 "build-tools;36.0.0"

# 或者使用国内镜像源（腾讯）：
~/Android/Sdk/cmdline-tools/latest/bin/sdkmanager --no_https --channel=0 "build-tools;36.0.0"
```

如果 sdkmanager 也无法下载，可以手动下载：

```bash
# 从可用网络下载 Build Tools 36.0.0
# 方法 1：从其他能联网的电脑上运行 sdkmanager 下载后拷贝过来
# 方法 2：直接下载 zip 包（需要知道具体 URL）

# 下载完成后放到正确位置
ls ~/Android/Sdk/build-tools/
# 应该能看到 36.0.0 目录
```

#### 方案 B：禁用 Gradle 自动下载 SDK 组件 + 降低 Build Tools 要求

在 `~/.gradle/gradle.properties` 中添加：

```bash
echo "android.builder.sdkDownload=false" >> ~/.gradle/gradle.properties
```

然后修改项目的 `build.gradle`（由 Qt 自动生成），但这个方法不可靠因为每次 cmake 配置会重新生成。

**更好的方法**：在 `~/.gradle/init.gradle` 中强制覆盖 Build Tools 版本：

```groovy
// 在已有的 init.gradle 文件末尾追加
allprojects {
    plugins.withType(com.android.build.gradle.BasePlugin) {
        android {
            buildToolsVersion = "35.0.0"
        }
    }
}
```

但注意 AGP 9.0.1 可能因为 35.0.0 太旧而拒绝工作。

#### 方案 C：解决 SSL 根本问题（一劳永逸）

SSL 握手失败通常是因为 Java 的信任证书库过期或被企业防火墙中间人劫持。

```bash
# 1. 检查系统是否能通过 curl 访问（curl 用系统 CA，不受 Java 影响）
curl -I https://dl.google.com/android/repository/addons_list-6.xml

# 2. 如果 curl 也失败 → 网络/防火墙问题，需要 VPN 或代理
# 3. 如果 curl 成功但 Java 失败 → Java 信任库问题

# 修复 Java 信任库：
# 方法 1：重新导入系统 CA 到 Java
sudo apt-get install -y ca-certificates-java
sudo /usr/sbin/update-ca-certificates -f
sudo keytool -importkeystore \
    -srckeystore /etc/ssl/certs/java/cacerts \
    -destkeystore $JAVA_HOME/lib/security/cacerts \
    -srcstorepass changeit -deststorepass changeit -noprompt 2>/dev/null

# 方法 2：让 Gradle 使用系统信任库
echo "org.gradle.jvmargs=-Djavax.net.ssl.trustStore=/etc/ssl/certs/java/cacerts -Djavax.net.ssl.trustStorePassword=changeit" >> ~/.gradle/gradle.properties
```

#### 最简单的完整解决路线

```bash
# 步骤 1：确保能通过代理/VPN 访问 Google
# 步骤 2：安装 Build Tools 36.0.0
~/Android/Sdk/cmdline-tools/latest/bin/sdkmanager "build-tools;36.0.0"

# 步骤 3：验证安装
ls ~/Android/Sdk/build-tools/36.0.0/

# 步骤 4：配置 Gradle 离线模式（避免后续再联网）
echo "android.builder.sdkDownload=false" >> ~/.gradle/gradle.properties

# 步骤 5：重新编译
cd ~/qgc-android/build-android
cmake --build . --parallel $(nproc)
```

#### 方案 D：离线安装 Build Tools 36.0.0（无需联网）

如果远程服务器完全无法访问 Google，可以从仓库 `tools/` 目录中获取预打包的 Build Tools：

```bash
# 解压到 SDK 目录
tar xzf tools/build-tools-36.0.0.tar.gz -C ~/Android/Sdk/build-tools/

# 验证
ls ~/Android/Sdk/build-tools/36.0.0/aapt2

# 禁止 Gradle 自动联网下载 SDK 组件
echo "android.builder.sdkDownload=false" >> ~/.gradle/gradle.properties
```

---

## 附录 D：编译成功与 APK 签名安装

### D.1 编译成功标志

当所有错误修复完成后，`cmake --build . --parallel $(nproc)` 应输出：

```
BUILD SUCCESSFUL in Xm Xs
XX actionable tasks: XX executed
Android package built successfully in XXX.XXX ms.
  -- File: .../android-build/build/outputs/apk/release/android-build-release-unsigned.apk
```

> **注意**：编译过程中会有大量 Java `警告: [deprecation]` 输出（来自 SDL、GStreamer、qtandroidhelpers 等第三方库），这些是过时 API 的警告，**不影响编译和运行**，可以忽略。

### D.2 APK 签名

生成的 APK 是 `unsigned`（未签名），不能直接安装到 Android 设备。需要签名后才能安装。

#### 方法 1：Debug 签名（测试用，最简单）

```bash
# 1. 生成 debug keystore（只需执行一次）
keytool -genkey -v \
    -keystore ~/debug.keystore \
    -storepass android \
    -alias androiddebugkey \
    -keypass android \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US"

# 2. 签名 APK
~/Android/Sdk/build-tools/36.0.0/apksigner sign \
    --ks ~/debug.keystore \
    --ks-pass pass:android \
    --key-pass pass:android \
    --out ~/QGroundControl-signed.apk \
    ~/qgc-android/build-android/android-build/build/outputs/apk/release/android-build-release-unsigned.apk

# 3. 验证签名
~/Android/Sdk/build-tools/36.0.0/apksigner verify ~/QGroundControl-signed.apk
```

#### 方法 2：Release 签名（正式发布用）

```bash
# 1. 生成正式 keystore（妥善保管，丢失后无法更新 APP）
keytool -genkey -v \
    -keystore ~/qgc-release.keystore \
    -alias qgc \
    -keyalg RSA -keysize 2048 -validity 10000

# 2. 签名
~/Android/Sdk/build-tools/36.0.0/apksigner sign \
    --ks ~/qgc-release.keystore \
    --ks-key-alias qgc \
    --out ~/QGroundControl-release.apk \
    ~/qgc-android/build-android/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
```

### D.3 安装到 Android 设备

```bash
# USB 连接手机，确保已开启 USB 调试
adb install ~/QGroundControl-signed.apk

# 如果之前安装过旧版本，使用 -r 覆盖安装
adb install -r ~/QGroundControl-signed.apk
```

### D.4 Android 上的 JSON 配置文件路径

APK 安装后，DDS 配置文件（JSON）通过以下路径加载：

| 类型 | Android 路径 |
|------|-------------|
| 内置默认配置 | APK 内嵌资源（只读） |
| 用户自定义配置 | `/storage/emulated/0/Android/data/org.mavlink.qgroundcontrol/files/dds_mappings/` |

添加新飞控配置：

```bash
# 通过 adb 推送
adb push my_drone.json /storage/emulated/0/Android/data/org.mavlink.qgroundcontrol/files/dds_mappings/

# 或通过手机文件管理器复制到上述目录
```

QGC 设置中选择 "Custom..." → 输入文件名 → 重连生效。

### D.5 完整编译流程总结

```bash
# 1. 环境准备（参见第 2 章）
# 2. 交叉编译 CycloneDDS（参见第 3 章）
# 3. CMake 配置
cmake -B build-android -S . \
    -DCMAKE_TOOLCHAIN_FILE=~/Qt/6.10.3/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake \
    -DQT_HOST_PATH=~/Qt/6.10.3/gcc_64 \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -G Ninja

# 4. 编译 + 打包 APK
cmake --build build-android --parallel $(nproc)

# 5. 签名
~/Android/Sdk/build-tools/36.0.0/apksigner sign \
    --ks ~/debug.keystore --ks-pass pass:android --key-pass pass:android \
    --out ~/QGroundControl-signed.apk \
    build-android/android-build/build/outputs/apk/release/android-build-release-unsigned.apk

# 6. 安装
adb install ~/QGroundControl-signed.apk
```
