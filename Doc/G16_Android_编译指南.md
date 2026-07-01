# G16 遥控器集成 - Android 编译指南

本文档描述如何在 **mavlink_G16** 分支上编译支持 Skydroid G16 遥控器的 QGroundControl Android APK。

## 前置条件

| 依赖 | 最低版本 | 推荐版本 | 验证命令 |
|------|---------|---------|---------|
| Qt | 6.8.0 | 6.8.x | `$QT_DIR/bin/qmake --version` |
| CMake | 3.22 | 3.28+ | `cmake --version` |
| Ninja | 1.10 | 1.12+ | `ninja --version` |
| Android NDK | r25b | r25c / r26b | `ls $ANDROID_NDK_HOME/toolchains/` |
| Android SDK | API 28+ | API 35 | `sdkmanager --list` |
| JDK | 17 | 17 | `java -version` |
| Git | 2.x | 任意 | `git --version` |

## 环境变量

```bash
# Qt Android 安装路径（包含 arm64-v8a 等目录）
export QT_DIR=/path/to/Qt/6.8.x/android_arm64_v8a
export QT_HOST_DIR=/path/to/Qt/6.8.x/gcc_64

# Android SDK/NDK
export ANDROID_SDK_ROOT=/path/to/Android/Sdk
export ANDROID_NDK_HOME=$ANDROID_SDK_ROOT/ndk/25.2.9519653
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
```

## 编译步骤

### 1. 克隆代码

```bash
git clone https://github.com/L-a-L-max/qgc_dev.git
cd qgc_dev
git checkout mavlink_G16
git submodule update --init --recursive
```

### 2. 配置 CMake

```bash
cmake -B build-android -S . \
  -DCMAKE_TOOLCHAIN_FILE=$QT_DIR/lib/cmake/Qt6/qt.toolchain.cmake \
  -DQT_HOST_PATH=$QT_HOST_DIR \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_ABI=arm64-v8a \
  -G Ninja
```

### 3. 编译

```bash
cmake --build build-android --parallel $(nproc)
```

### 4. 生成 APK

```bash
# androiddeployqt 由 Qt CMake 集成自动调用
# APK 输出位置：
ls build-android/android-build/build/outputs/apk/
```

如果需要手动生成签名 APK：

```bash
cd build-android/android-build
./gradlew assembleRelease
```

## 关键文件说明

| 文件 | 作用 |
|------|------|
| `android/libs/rcsdk-v1.9.1.jar` | Skydroid RCSDK 库 |
| `android/src/.../SkydroidRCSDKManager.java` | Java 层：SDK 初始化、通道轮询 |
| `src/G16Joystick/G16JoystickPlugin.h/cc` | C++ 层：JNI 桥接、摇杆映射 |
| `src/G16Joystick/G16JoystickToggle.qml` | UI 开关组件 |

## 常见问题

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
