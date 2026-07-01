# G16 遥控器集成 - Android 编译指南

本文档描述如何在 **mavlink_G16** 分支上编译支持 Skydroid G16 遥控器的 QGroundControl Android APK。

## 前置条件

| 依赖 | 最低版本 | 推荐版本 | 验证命令 |
|------|---------|---------|---------|
| Python | 3.11 | 3.11+ | `python3 --version` |
| Qt | 6.8.0 | 6.10.x | `qmake --version` |
| CMake | 3.22 | 3.28+ | `cmake --version` |
| Ninja | 1.10 | 1.12+ | `ninja --version` |
| Android NDK | r25b | r26b / r27 | `ls $ANDROID_NDK/toolchains/` |
| Android SDK | API 28+ | API 35 | `sdkmanager --list` |
| JDK | 17 | 17 | `java -version` |
| Git | 2.x | 任意 | `git --version` |

> **重要**：QGC 最新 master 的构建脚本使用 Python `tomllib` 模块（Python 3.11+ 内置）。
> 如果你的系统 Python 是 3.10（如 Ubuntu 22.04），需要先解决此问题（见下方"Python 环境准备"）。

## 环境变量（.bashrc）

以下是适配你实际环境的变量设置（以 Qt 6.10.3 + NDK r27 为例）：

```bash
# Java
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64

# Android SDK/NDK
export ANDROID_HOME=$HOME/Android/Sdk
export ANDROID_NDK=$ANDROID_HOME/ndk/27.2.12479018
export ANDROID_PLATFORM=android-35
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH

# Qt（Android target + Host tools）
export QT_HOST_PATH=$HOME/Qt/6.10.3/gcc_64
export QT_ROOT_DIR=$HOME/Qt/6.10.3/android_arm64_v8a
export PATH=$QT_HOST_PATH/bin:$PATH
```

## Python 环境准备

QGC 构建系统在 CMake 配置阶段会自动创建 `.venv` 并安装依赖。这个过程需要 Python 3.11+（因为使用了 `tomllib` 模块）。

### 检查 Python 版本

```bash
python3 --version
```

### 如果是 Python 3.10（Ubuntu 22.04 默认）

**方法 1：安装 tomli 包（最快）**
```bash
pip3 install tomli
```

**方法 2：安装 Python 3.11+（推荐）**
```bash
sudo add-apt-repository ppa:deadsnakes/ppa
sudo apt update
sudo apt install python3.11 python3.11-venv python3.11-dev
# 设置为默认 python3
sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.11 1
sudo update-alternatives --config python3
```

安装后验证：
```bash
python3 -c "import tomllib; print('OK')"
```

## 编译步骤

### 1. 获取代码

```bash
# 方式一：浅克隆（推荐，速度快）
git clone --depth 1 --branch mavlink_G16 https://github.com/L-a-L-max/qgc_dev.git
cd qgc_dev
git submodule update --init --recursive

# 方式二：如果已有仓库
cd ~/qgc-android
git fetch origin mavlink_G16
git checkout mavlink_G16
git submodule update --init --recursive
```

> 如果 clone 报 `HTTP/2 stream` 错误，先执行：`git config --global http.version HTTP/1.1`

### 2. 配置 CMake

```bash
mkdir -p build-g16 && cd build-g16

cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake \
  -DCMAKE_PREFIX_PATH=$QT_ROOT_DIR \
  -DQT_HOST_PATH=$QT_HOST_PATH \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=$ANDROID_PLATFORM \
  -DANDROID_NDK=$ANDROID_NDK \
  -DQT_ANDROID_ABIS=arm64-v8a \
  -DCMAKE_BUILD_TYPE=Release \
  -DQT_ANDROID_SIGN_APK=OFF \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
  -G Ninja
```

**注意**：mavlink_G16 分支**不需要** DDS 相关参数（`-DQGC_ENABLE_DDS=ON`、`-DCycloneDDS_*`）。
G16 遥控器通过标准 MAVLink MANUAL_CONTROL 消息通信，不依赖 DDS。

### 3. 编译

```bash
cmake --build . --parallel $(nproc)
```

### 4. 生成 APK

```bash
# APK 由 Qt 的 androiddeployqt 自动生成
ls android-build/build/outputs/apk/

# 或手动：
cd android-build
./gradlew assembleRelease
```

## 完整编译命令（一键复制）

适配你的环境（`/home/ros2-3/qgc-android/build-g16`）：

```bash
cd /home/ros2-3/qgc-android

# 1. 确保 Python 有 tomli（如果 Python < 3.11）
pip3 install tomli

# 2. 清除旧构建（如果有）
rm -rf build-g16

# 3. 配置
cmake -B build-g16 -S . \
  -DCMAKE_TOOLCHAIN_FILE=$QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake \
  -DCMAKE_PREFIX_PATH=$QT_ROOT_DIR \
  -DQT_HOST_PATH=$QT_HOST_PATH \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=$ANDROID_PLATFORM \
  -DANDROID_NDK=$ANDROID_NDK \
  -DQT_ANDROID_ABIS=arm64-v8a \
  -DCMAKE_BUILD_TYPE=Release \
  -DQT_ANDROID_SIGN_APK=OFF \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
  -G Ninja

# 4. 编译
cmake --build build-g16 --parallel $(nproc)
```

## 关键文件说明

| 文件 | 作用 |
|------|------|
| `android/libs/rcsdk-v1.9.1.jar` | Skydroid RCSDK 库 |
| `android/src/.../SkydroidRCSDKManager.java` | Java 层：SDK 初始化、通道轮询 |
| `src/G16Joystick/G16JoystickPlugin.h/cc` | C++ 层：JNI 桥接、摇杆映射 |
| `src/G16Joystick/G16JoystickToggle.qml` | UI 开关组件 |

## 常见问题

### Q: CMake 报错 `No module named 'tomllib'`

你的 Python 版本是 3.10，缺少 `tomllib`（3.11+ 内置）。解决：
```bash
pip3 install tomli
```
或安装 Python 3.11+。

### Q: CMake 报错 `No module named 'tomli'`

同上，执行 `pip3 install tomli`。

### Q: 编译报错 `cannot find symbol: SkydroidRCSDKManager`

确保 `android/libs/rcsdk-v1.9.1.jar` 文件存在。这个 JAR 通过 `build.gradle` 中的 `fileTree(dir: 'libs')` 自动包含。

### Q: Release APK 运行时 ClassNotFoundException

确认 `android/proguard-rules.pro` 中包含：
```proguard
-keep class org.mavlink.qgroundcontrol.SkydroidRCSDKManager { *; }
-keep class com.skydroid.rcsdk.** { *; }
```

### Q: G16 面板不显示

G16 开关仅在 Android 平台显示（`Qt.platform.os === "android"`）。桌面端编译正常但不显示该组件。

### Q: 非 Skydroid 遥控器可以用吗？

当前仅支持 Skydroid G 系列（G12/G16/G20/G30）遥控器，需要设备内置 Skydroid SDK 支持。

### Q: git clone 报 `HTTP/2 stream` 错误

网络不稳定导致大仓库 clone 失败：
```bash
git config --global http.version HTTP/1.1
# 或浅克隆
git clone --depth 1 --branch mavlink_G16 https://github.com/L-a-L-max/qgc_dev.git
```
