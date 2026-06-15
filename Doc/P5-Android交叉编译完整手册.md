# P5 - Android 交叉编译完整手册

本文档描述如何在 Ubuntu 系统上交叉编译 QGC DDS_P5 分支的 Android APK，从环境搭建到手机安装的全流程。

**适用分支**：`DDS_P5`（也兼容 `DDS_P3`）

---

## 目录

1. [环境要求](#1-环境要求)
2. [工具安装](#2-工具安装)
3. [CycloneDDS 交叉编译](#3-cyclonedds-交叉编译)
4. [Qt 路径修复（关键步骤）](#4-qt-路径修复关键步骤)
5. [Gradle 镜像与离线配置](#5-gradle-镜像与离线配置)
6. [QGC 编译](#6-qgc-编译)
7. [APK 签名](#7-apk-签名)
8. [安装到手机](#8-安装到手机)
9. [测试验证](#9-测试验证)
10. [ADB 调试指南](#10-adb-调试指南)
11. [常见问题排查](#11-常见问题排查)
12. [附录 A：一键编译脚本](#附录-a一键编译脚本)
13. [附录 B：与真实无人机测试](#附录-b与真实无人机测试)

---

## 1. 环境要求

| 组件 | 版本 | 说明 |
|------|------|------|
| Ubuntu | 22.04+ | 编译主机 |
| Qt (PC/Host) | **6.10.3** | 提供 moc/rcc/qmlcachegen 等编译工具 |
| Qt (Android Target) | **6.10.3 android_arm64_v8a** | Android ARM64 交叉编译目标 |
| Android SDK | Platform 35 | Command-line tools |
| Android NDK | r27c (27.2.12479018) | Qt 6.10.3 推荐版本 |
| Build Tools | **36.0.0** | AGP 9.0.1 要求 ≥36.0.0 |
| Java JDK | 17 | Gradle 编译需要 |
| CMake | 3.25+ | 构建系统 |
| Ninja | 1.10+ | 构建后端 |
| CycloneDDS | 0.10.x | 需要交叉编译 Android ARM64 版本 |

### 磁盘空间

- Qt Android Target: ~3 GB
- Android SDK + NDK: ~8 GB
- CycloneDDS 编译: ~200 MB
- QGC 编译: ~2 GB
- **总计额外需求: ≥15 GB**

---

## 2. 工具安装

### 2.1 基础编译工具

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git curl unzip \
    openjdk-17-jdk python3 python3-pip pkg-config \
    libgl1-mesa-dev libxkbcommon-dev libvulkan-dev
```

### 2.2 设置 JAVA_HOME

```bash
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
```

### 2.3 确认 Qt 安装路径

```bash
# 确认 PC 版 Qt
ls ~/Qt/6.10.3/gcc_64/bin/qmake
# 设置环境变量
export QT_HOST_PATH=$HOME/Qt/6.10.3/gcc_64
```

### 2.4 安装 Qt 6.10.3 Android Target

**方式一：通过 Qt Maintenance Tool（推荐）**

```bash
~/Qt/MaintenanceTool
# 选择 "Add or remove components"
# 勾选: Qt 6.10.3 → Android → Android ARM64-v8a
# 同时勾选: Qt Location, Qt Positioning, Qt Multimedia, Qt Serial Port,
#           Qt WebSockets, Qt HTTP Server, Qt Connectivity, Qt Sensors,
#           Qt Image Formats, Qt Shader Tools, Qt Quick 3D
```

**方式二：通过 aqtinstall（命令行）**

```bash
pip3 install aqtinstall
aqt install-qt linux android 6.10.3 android_arm64_v8a \
    -m qtlocation qtpositioning qtspeech qtmultimedia qtserialport \
      qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors \
      qtwebsockets qthttpserver \
    -O $HOME/Qt
```

验证安装：

```bash
ls ~/Qt/6.10.3/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake
```

### 2.5 安装 Android SDK 和 NDK

```bash
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
           "build-tools;36.0.0" \
           "ndk;27.2.12479018"

export ANDROID_NDK=$ANDROID_HOME/ndk/27.2.12479018
export ANDROID_PLATFORM=android-35
```

> **⚠️ 无法下载 Build Tools 36.0.0？**
> 如果网络环境无法访问 `dl.google.com`（中国大陆常见），可使用仓库中预打包的离线版本：
> ```bash
> tar xzf tools/build-tools-36.0.0.tar.gz -C ~/Android/Sdk/build-tools/
> ls ~/Android/Sdk/build-tools/36.0.0/aapt2  # 验证
> ```

### 2.6 保存环境变量

将以下内容添加到 `~/.bashrc`：

```bash
cat >> ~/.bashrc << 'EOF'
# === QGC Android Build Environment ===
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export ANDROID_HOME=$HOME/Android/Sdk
export ANDROID_NDK=$ANDROID_HOME/ndk/27.2.12479018
export ANDROID_PLATFORM=android-35
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH
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
test -f $QT_HOST_PATH/bin/qmake && echo "OK: Qt Host" || echo "FAIL: Qt Host NOT FOUND"
test -f $QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake && echo "OK: Qt Android" || echo "FAIL: Qt Android NOT FOUND"
test -d $ANDROID_NDK && echo "OK: NDK" || echo "FAIL: NDK NOT FOUND"
test -d $ANDROID_HOME/build-tools/36.0.0 && echo "OK: Build Tools 36.0.0" || echo "FAIL: Build Tools 36.0.0 NOT FOUND"
```

---

## 3. CycloneDDS 交叉编译

QGC DDS 功能依赖 CycloneDDS 库，需要为 Android ARM64 交叉编译。

### 3.1 下载源码

```bash
cd $HOME
git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
cd cyclonedds
git checkout 0.10.5
```

### 3.2 交叉编译

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

### 3.3 验证

```bash
file $HOME/cyclonedds-android/lib/libddsc.so
# 预期: ELF 64-bit LSB shared object, ARM aarch64

ls $HOME/cyclonedds-android/include/dds/dds.h
```

---

## 4. Qt 路径修复（关键步骤）

> **⚠️ 这是最常见的编译失败原因。必须在 CMake 配置之前完成。**

Qt 6.10.3 Android 组件在安装时会把 NDK/SDK 路径**硬编码**到 toolchain 文件中（通常为 `/opt/android/...`）。如果你的 NDK/SDK 不在这个路径，编译会失败。

### 4.1 检查硬编码路径

```bash
grep -r "/opt/android" ~/Qt/6.10.3/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake
```

如果输出包含 `/opt/android/android-ndk-r27c` 或 `/opt/android/sdk` 等路径，需要创建符号链接。

### 4.2 创建符号链接

```bash
sudo mkdir -p /opt/android/

# NDK 符号链接
sudo ln -sf ~/Android/Sdk/ndk/27.2.12479018 /opt/android/android-ndk-r27c

# SDK 符号链接
sudo ln -sf ~/Android/Sdk /opt/android/sdk
```

### 4.3 验证

```bash
ls /opt/android/android-ndk-r27c/build/cmake/android.toolchain.cmake
# 应存在

ls /opt/android/sdk/build-tools/36.0.0/
# 应存在
```

### 4.4 不修复会怎样？

| 缺少的符号链接 | 错误现象 | 连锁反应 |
|---------------|---------|---------|
| NDK 符号链接 | `toolchain.cmake does not exist` | `ANDROID` 变量未设置 → Qt6DBus 找不到 |
| SDK 符号链接 | `Could not locate build tools under /opt/android/sdk/` | CMake 配置失败 |

---

## 5. Gradle 镜像与离线配置

> **⚠️ 中国大陆用户必须配置，否则 Gradle 下载 AGP 组件会失败（SSL 握手被阻断）。**

### 5.1 配置阿里云 Maven 镜像

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

### 5.2 禁止 Gradle 自动下载 SDK 组件

```bash
cat >> ~/.gradle/gradle.properties << 'EOF'
android.builder.sdkDownload=false
EOF
```

---

## 6. QGC 编译

### 6.1 获取源码

```bash
cd $HOME
git clone https://github.com/L-a-L-max/qgc_dev.git qgc-android
cd qgc-android
git checkout DDS_P5
git submodule update --init --recursive
```

### 6.2 CMake 配置

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
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -G Ninja
```

**参数说明**：

| 参数 | 说明 |
|------|------|
| `CMAKE_TOOLCHAIN_FILE` | Qt 提供的 toolchain，自动配置 NDK 交叉编译 |
| `QT_HOST_PATH` | PC 版 Qt 6.10.3，提供编译工具（moc, rcc 等）|
| `QGC_ENABLE_DDS=ON` | 启用 DDS 通信功能 |
| `CycloneDDS_*` | 第 3 步交叉编译好的 Android 版 CycloneDDS |
| `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH` | 允许同时搜索宿主和目标的 CMake 包 |

### 6.3 配置成功标志

输出中应包含：

```
Target system:      Android 1        ← 必须是 Android，不能是 Linux
C++ compiler:       Clang 18.0.3
Qt version:         6.10.3
QGC DDS support enabled
  CycloneDDS: /home/.../cyclonedds-android/lib/libddsc.so
```

> **如果 `Target system` 显示 `Linux` 而非 `Android`**：说明第 4 步的 NDK 符号链接没有正确创建。请回到第 4 步修复。

### 6.4 编译

```bash
cmake --build . --parallel $(nproc)
```

编译时间预计 15-30 分钟。

### 6.5 编译成功标志

```
BUILD SUCCESSFUL in Xm Xs
XX actionable tasks: XX executed
Android package built successfully in XXX.XXX ms.
  -- File: .../android-build/build/outputs/apk/release/android-build-release-unsigned.apk
```

> **注意**：编译过程中会有大量 Java `警告: [deprecation]` 输出（来自 SDL、GStreamer、qtandroidhelpers 等第三方库），这些是过时 API 的警告，**不影响编译和运行**，可以忽略。

---

## 7. APK 签名

生成的 APK 是 `unsigned`（未签名），**不能直接安装到手机**，必须签名。

### 7.1 生成 debug keystore（只需执行一次）

```bash
keytool -genkey -v \
    -keystore ~/debug.keystore \
    -storepass android \
    -alias androiddebugkey \
    -keypass android \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US"
```

> 如果提示 `别名已经存在`，说明之前已经生成过，跳过此步。

### 7.2 签名 APK

```bash
~/Android/Sdk/build-tools/36.0.0/apksigner sign \
    --ks ~/debug.keystore \
    --ks-pass pass:android \
    --key-pass pass:android \
    --out ~/QGroundControl-signed.apk \
    ~/qgc-android/build-android/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
```

### 7.3 验证签名

```bash
~/Android/Sdk/build-tools/36.0.0/apksigner verify ~/QGroundControl-signed.apk
# 无输出 = 签名有效
```

---

## 8. 安装到手机

### 8.1 方法一：ADB 安装（推荐）

```bash
# 确认手机已连接并开启 USB 调试
adb devices
# 应显示设备编号 + "device"

# 安装
adb install ~/QGroundControl-signed.apk

# 覆盖安装（如果之前安装过）
adb install -r ~/QGroundControl-signed.apk
```

### 8.2 方法二：文件传输

1. 将 `~/QGroundControl-signed.apk` 传到手机（微信/QQ/USB 拷贝）
2. 手机上用文件管理器打开 APK
3. 允许"安装未知来源应用"

### 8.3 安装失败排查

| 错误 | 解决 |
|------|------|
| 安装包没有签名 | 回到第 7 步签名，不要用 unsigned APK |
| `INSTALL_FAILED_NO_MATCHING_ABIS` | 手机不是 ARM64 架构 |
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | 先卸载: `adb uninstall org.mavlink.qgroundcontrol` |
| `INSTALL_FAILED_OLDER_SDK` | 手机 Android 版本 < 9.0 |

---

## 9. 测试验证

### 9.1 前提条件

| 条件 | 要求 |
|------|------|
| Android 版本 | 9.0 (API 28) 及以上 |
| CPU 架构 | ARM64 (arm64-v8a) |
| 网络 | 手机与 PX4 电脑在**同一 WiFi 网段** |

### 9.2 启动测试环境

```bash
# 终端 1: PX4 SITL
cd ~/PX4-Autopilot
make px4_sitl gz_x500

# 终端 2: DDS Agent
MicroXRCEAgent udp4 -p 8888
```

### 9.3 测试用例

| 测试项 | 操作 | 预期结果 | 结果 |
|--------|------|---------|------|
| DDS 连接 | 打开 QGC，等待几秒 | 显示遥测数据（GPS/姿态/电池）| ⬜ |
| Arm/Disarm | 点击 Arm → Disarm | PX4 终端显示 Armed/Disarmed | ⬜ |
| 起飞/降落 | 点击 Takeoff → Land | 仿真无人机升空后降落 | ⬜ |
| 模式切换 | 切换到 Position/Hold | PX4 显示模式切换 | ⬜ |
| Go To 指点 | 点击地图 → "Go to here" | 无人机飞往目标位置 | ⬜ |
| 虚拟摇杆 | Position 模式下拖动摇杆 | 无人机按方向移动 | ⬜ |
| RTL 返航 | 飞到远处 → 点击 Return | 自动返回起飞点 | ⬜ |

### 9.4 网络环境

```
┌─────────────────┐         WiFi          ┌─────────────────┐
│  Ubuntu 电脑     │◄──────────────────────►│  Android 手机    │
│  ┌───────────┐  │    同一局域网           │  ┌───────────┐  │
│  │ PX4 SITL  │  │                        │  │ QGC DDS   │  │
│  └─────┬─────┘  │                        │  └─────┬─────┘  │
│  ┌─────▼─────┐  │     UDP Multicast      │        │ DDS    │
│  │ DDS Agent │◄─┼────────────────────────┼────────┘        │
│  └───────────┘  │    端口 7400-7500      │                 │
└─────────────────┘                        └─────────────────┘
```

开放防火墙：
```bash
sudo ufw allow 7400:7500/udp
```

---

## 10. ADB 调试指南

### 10.1 开启 USB 调试

1. 手机：设置 → 关于手机 → 连续点击"版本号" 7 次 → 启用开发者模式
2. 手机：设置 → 系统 → 开发者选项 → 打开"USB 调试"
3. USB 连接手机到电脑，手机弹窗点"允许"

> 不同品牌路径略有不同：
> - 小米：设置 → 我的设备 → 全部参数 → 点击 MIUI 版本 7 次
> - 华为：设置 → 关于手机 → 点击版本号 7 次 → 系统与更新 → 开发者选项
> - OPPO/vivo：设置 → 关于手机 → 点击版本号 7 次 → 其他设置 → 开发者选项

### 10.2 Windows 上使用 ADB

```cmd
:: 下载 Platform Tools 并解压到 D:\platform-tools\

:: 进入目录
D:
cd D:\platform-tools

:: 验证连接
adb.exe devices

:: 如果报版本不匹配错误
adb.exe kill-server
adb.exe start-server
adb.exe devices
```

### 10.3 查看崩溃日志

```bash
# 步骤 1：清除旧日志
adb logcat -c

# 步骤 2：在手机上打开 QGC（等它闪退）

# 步骤 3：导出日志到文件
adb logcat -d > qgc_crash.txt

# 步骤 4：过滤关键信息
grep -iE "qground|crash|fatal|exception|signal|ddsc" qgc_crash.txt
```

Windows CMD 版本：
```cmd
adb.exe logcat -c
:: 在手机上打开 QGC，等闪退
adb.exe logcat -d > D:\qgc_crash.txt
```

---

## 11. 常见问题排查

### 11.1 CMake 配置阶段

| 错误 | 原因 | 解决 |
|------|------|------|
| `toolchain.cmake does not exist` | NDK 路径硬编码不匹配 | 第 4 步创建符号链接 |
| `Could NOT find Qt6DBus` | NDK 路径问题的连锁反应 | 修复 NDK 路径后自动解决 |
| `Could not locate build tools under /opt/android/sdk/` | SDK 路径硬编码不匹配 | `sudo ln -sf ~/Android/Sdk /opt/android/sdk` |
| `CycloneDDS not found` | 未交叉编译 CycloneDDS | 回到第 3 步 |
| `Target system: Linux`（应为 Android）| NDK toolchain 加载失败 | 检查符号链接 |

### 11.2 编译阶段

| 错误 | 原因 | 解决 |
|------|------|------|
| `unused-lambda-capture` error | Android Clang 18 比 GCC 严格 | P5 已修复，确认用最新代码 |
| Gradle TLS/SSL 握手失败 | 无法访问 `dl.google.com` | 第 5 步配置阿里云镜像 |
| `Build Tools revision 36.0.0` not found | Build Tools 版本不够 | `tools/build-tools-36.0.0.tar.gz` 离线安装 |

### 11.3 运行阶段

| 错误 | 原因 | 解决 |
|------|------|------|
| 安装包没有签名 | 使用了 unsigned APK | 第 7 步签名 |
| 打开后闪退 `libddsc.so not found` | CycloneDDS 库未打包进 APK | P5 已修复（`QT_ANDROID_EXTRA_LIBS`），确认用最新代码 |
| 打开后无遥测数据 | 网络不通 | 检查 WiFi 同网段 + 防火墙 |
| 虚拟摇杆无响应 | 未切到 Position 模式 | PX4 终端：`commander mode posctl` |

### 11.4 排查流程图

```
问题分类
├── CMake 配置失败
│   ├── NDK/SDK 路径？ → 第 4 步符号链接
│   ├── Qt6DBus？ → 修复 NDK 路径
│   └── CycloneDDS？ → 第 3 步交叉编译
├── 编译失败（C++）
│   ├── lambda capture？ → git pull 最新代码
│   └── 其他 Clang 错误？ → 检查是否为 Android 特有
├── 编译失败（Gradle/APK）
│   ├── TLS/SSL？ → 第 5 步镜像配置
│   └── Build Tools？ → 离线安装 36.0.0
├── 安装失败
│   ├── 没有签名？ → 第 7 步
│   └── 版本不兼容？ → 先卸载旧版
└── 运行闪退
    ├── libddsc.so？ → git pull 最新代码
    └── 其他？ → adb logcat 查日志（第 10 步）
```

---

## 附录 A：一键编译脚本

保存为 `build_android.sh`：

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
echo " QGC DDS_P5 Android Build (Qt 6.10.3)"
echo "========================================="

# 验证环境
for f in "$QT_HOST_PATH/bin/qmake" \
         "$QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake" \
         "$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
         "$CYCLONEDDS_ANDROID/lib/libddsc.so" \
         "$ANDROID_HOME/build-tools/36.0.0/aapt2"; do
    if [ ! -f "$f" ]; then
        echo "ERROR: Missing: $f"
        exit 1
    fi
done
echo "Environment verified OK"

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
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -G Ninja

echo ">>> Building ($(nproc) threads)..."
cmake --build . --parallel $(nproc)

echo ""
echo "========================================="
echo " Build Complete!"
echo "========================================="
APK_PATH=$(find . -name "*unsigned.apk" -type f | head -1)
echo "Unsigned APK: $APK_PATH"
echo ""
echo "Next steps:"
echo "  1. Sign:  ~/Android/Sdk/build-tools/36.0.0/apksigner sign --ks ~/debug.keystore --ks-pass pass:android --key-pass pass:android --out ~/QGroundControl-signed.apk $APK_PATH"
echo "  2. Install: adb install ~/QGroundControl-signed.apk"
```

使用：

```bash
chmod +x build_android.sh
./build_android.sh
```

---

## 附录 B：与真实无人机测试

如果用真实 PX4 飞控（如 CUAV X7+ Pro）而非 SITL：

### 网络拓扑

```
飞控(PX4) → WiFi/以太网 → 路由器 ← WiFi ← 手机(QGC)
                              ↑
                         DDS Agent(机载电脑)
```

### 飞控 PX4 参数

```
UXRCE_DDS_CFG = 0 (UDP)
UXRCE_DDS_AG_IP = <Agent设备IP>
UXRCE_DDS_PRT = 8888
```

### Agent 启动

```bash
MicroXRCEAgent udp4 -p 8888
```

### DDS Profile 选择

CUAV X7+ Pro 等真实飞控的话题名称可能与 SITL 不同（缺少 `_v1` 后缀）。在 QGC 设置中选择对应的 DDS Profile：

- `PX4 SITL (default)` — 仿真（话题带 `_v1` 后缀）
- `CUAV X7+ Pro` — 真实飞控（话题无 `_v1` 后缀）

### Android 上添加自定义飞控配置

```bash
# 通过 adb 推送自定义 JSON 配置
adb push my_drone.json /storage/emulated/0/Android/data/org.mavlink.qgroundcontrol/files/dds_mappings/
```

QGC 设置中选择 "Custom..." → 输入文件名 → 重连生效。
