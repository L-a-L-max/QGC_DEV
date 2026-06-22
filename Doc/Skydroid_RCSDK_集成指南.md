# Skydroid RCSDK 集成指南（G16/G20 遥控器摇杆控制）

## 概述

本文档说明如何将 Skydroid RCSDK 集成到 QGC Android APK 中，使 G16/G20 遥控器的物理摇杆能够通过 DDS 控制无人机飞行。

### 架构

```
G16 物理摇杆 → Skydroid MCU → RCSDK (Java)
                                  │ 100ms 轮询
                                  ▼
                        SkydroidRCSDKManager.java
                          (缓存通道值)
                                  │ JNI
                                  ▼
                        SkydroidJoystick (C++)
                          (归一化 + 通道映射)
                                  │
                                  ▼
                      DDSManualControlPublisher
                    (DDS manual_control_setpoint)
                                  │
                                  ▼
                            PX4 飞控
```

### 通道映射（Mode 2 美国手）

| 通道 | 摇杆 | 控制轴 | 归一化范围 |
|------|------|--------|-----------|
| CH1 (index 0) | 右摇杆 左右 | Roll | [-1, +1] |
| CH2 (index 1) | 右摇杆 上下 | Pitch | [-1, +1] |
| CH3 (index 2) | 左摇杆 上下 | Throttle | [0, 1] |
| CH4 (index 3) | 左摇杆 左右 | Yaw | [-1, +1] |
| CH5-CH10 | 开关 SA-SF | 辅助功能 | — |
| CH11-CH12 | 旋钮 | 辅助功能 | — |

原始通道值范围：1000-2000，中心值 1500。

## 编译前准备：拆包 AAR 文件

Qt Android 项目无法直接使用 `.aar` 文件，需要手动拆包。

### 第一步：获取 RCSDK

```bash
# 从 Gitee 克隆 demo 工程
git clone https://gitee.com/skydroid/rcsdk-demo.git
cd rcsdk-demo

# AAR 文件位于
ls app/libs/
# 应该看到: rcsdk-v1.9.1.aar  h16_airlink.aar
```

### 第二步：拆包 AAR

AAR 本质上是 ZIP 文件：

```bash
# 创建临时目录
mkdir -p /tmp/rcsdk_extract /tmp/airlink_extract

# 拆包 rcsdk
cd /tmp/rcsdk_extract
unzip /path/to/rcsdk-v1.9.1.aar

# 拆包 airlink
cd /tmp/airlink_extract
unzip /path/to/h16_airlink.aar
```

拆包后的结构：
```
/tmp/rcsdk_extract/
├── classes.jar          ← SDK Java 代码
├── jni/
│   ├── arm64-v8a/       ← 64位 SO 库
│   │   └── lib*.so
│   └── armeabi-v7a/     ← 32位 SO 库
│       └── lib*.so
├── assets/              ← SDK 资源文件
└── AndroidManifest.xml
```

### 第三步：放到 QGC 项目中

```bash
cd /path/to/qgc_dev

# 创建 libs 目录
mkdir -p android/libs
mkdir -p android/libs/arm64-v8a
mkdir -p android/libs/armeabi-v7a

# 复制 JAR 文件
cp /tmp/rcsdk_extract/classes.jar android/libs/rcsdk-v1.9.1.jar
cp /tmp/airlink_extract/classes.jar android/libs/h16_airlink.jar

# 复制 SO 文件
cp /tmp/rcsdk_extract/jni/arm64-v8a/*.so android/libs/arm64-v8a/
cp /tmp/rcsdk_extract/jni/armeabi-v7a/*.so android/libs/armeabi-v7a/
cp /tmp/airlink_extract/jni/arm64-v8a/*.so android/libs/arm64-v8a/
cp /tmp/airlink_extract/jni/armeabi-v7a/*.so android/libs/armeabi-v7a/

# 复制 assets（如果有）
cp -r /tmp/rcsdk_extract/assets/* android/assets/ 2>/dev/null || true
```

### 第四步：添加 Kotlin 运行时

RCSDK 用 Kotlin 编写，APK 必须包含 Kotlin 运行时 JAR。

从 Maven Central 下载或从 Android Studio 缓存中复制：

```bash
# 从 Maven Central 下载（版本需与 RCSDK 编译时使用的一致，通常 1.3.72 或更高）
cd android/libs

wget https://repo1.maven.org/maven2/org/jetbrains/kotlin/kotlin-stdlib/1.3.72/kotlin-stdlib-1.3.72.jar
wget https://repo1.maven.org/maven2/org/jetbrains/kotlin/kotlin-stdlib-jdk7/1.3.72/kotlin-stdlib-jdk7-1.3.72.jar
wget https://repo1.maven.org/maven2/org/jetbrains/kotlin/kotlin-stdlib-jdk8/1.3.72/kotlin-stdlib-jdk8-1.3.72.jar

# 或者从 Android Studio 缓存
# ~/.gradle/caches/modules-2/files-2.1/org.jetbrains.kotlin/kotlin-stdlib/
```

### 第五步：更新 build.gradle（如使用 Gradle）

如果项目使用 Gradle 构建，在 `android/build.gradle` 中添加：

```groovy
dependencies {
    implementation fileTree(dir: 'libs', include: ['*.jar'])
}

android {
    sourceSets {
        main {
            jniLibs.srcDirs = ['libs']
        }
    }
}
```

### 最终目录结构

```
android/
├── libs/
│   ├── rcsdk-v1.9.1.jar
│   ├── h16_airlink.jar
│   ├── kotlin-stdlib-1.3.72.jar
│   ├── kotlin-stdlib-jdk7-1.3.72.jar
│   ├── kotlin-stdlib-jdk8-1.3.72.jar
│   ├── arm64-v8a/
│   │   └── *.so
│   └── armeabi-v7a/
│       └── *.so
├── assets/
│   └── (RCSDK assets if any)
└── src/org/mavlink/qgroundcontrol/
    ├── QGCActivity.java          (已修改: 初始化/关闭 RCSDK)
    └── SkydroidRCSDKManager.java (新增: RCSDK 管理器)
```

## 使用方法

1. 编译并安装 APK 到 G16 遥控器
2. 打开 QGC → 新建连接 → 选择 DDS
3. 在 DDS 设置中：
   - 设置 Domain ID（默认 0）
   - 选择飞控 Profile
   - **开启 "Skydroid Joystick (G16/G20)"**
4. 连接飞控后，移动 G16 物理摇杆即可控制无人机

## 常见问题

### 1. `NoClassDefFoundError: kotlin/jvm/internal/Intrinsics`

**原因**：缺少 Kotlin 运行时 JAR。
**解决**：确保 `android/libs/` 中有 `kotlin-stdlib-*.jar` 文件。

### 2. SDK 初始化失败

**原因**：AAR 拆包不完整，缺少 SO 库或 assets。
**解决**：确认 `android/libs/arm64-v8a/` 下有 RCSDK 的 `.so` 文件。

### 3. 摇杆无响应

**检查**：
1. DDS 连接已建立（状态栏显示连接成功）
2. "Skydroid Joystick" 开关已开启
3. G16 遥控器已开机且信号灯正常
4. 查看 QGC 日志中的 `[DDS.SkydroidJoystick]` 和 `[SkydroidRCSDK]` 输出

### 4. 通道映射不对

如果使用 Mode 1（日本手）或其他自定义映射，需要修改 `SkydroidJoystick.cc` 中的默认通道分配：
```cpp
int _rollCh     = 0;   // CH1
int _pitchCh    = 1;   // CH2
int _throttleCh = 2;   // CH3
int _yawCh      = 3;   // CH4
```

## 代码文件说明

| 文件 | 层 | 作用 |
|------|---|------|
| `SkydroidRCSDKManager.java` | Java | RCSDK 初始化、100ms 轮询通道值、缓存数据 |
| `SkydroidJoystick.h/cc` | C++ | JNI 桥接，读取 Java 层数据，归一化后发送 DDS |
| `DDSConfiguration.h/cc` | C++ | 新增 `skydroidJoystick` 配置项，持久化存储 |
| `DDSLink.h/cc` | C++ | 连接建立时注入 publisher 到 SkydroidJoystick |
| `DDSSettings.qml` | QML | 新增 "Skydroid Joystick" 开关 |
| `QGCActivity.java` | Java | 生命周期钩子：onCreate 初始化，onDestroy 关闭 |
