# Zenoh Bridge 编译与集成指南

## 架构概述

```
┌─────────────────┐     Zenoh TCP     ┌─────────────────────────────────┐
│  机载端 (PX4)   │ ←──────────────→ │      安卓 QGC (DDS_zenoh)       │
│                 │                   │                                 │
│  PX4 飞控       │                   │  ┌──────────────────────┐      │
│    ↓ DDS        │                   │  │ zenoh-bridge-dds     │      │
│  zenoh-bridge   │                   │  │ (子进程, 端口7447)   │      │
│  -dds           │                   │  │  ↓↑ DDS (localhost)  │      │
│  (Zenoh Router) │                   │  └──────────────────────┘      │
│                 │                   │    ↓↑                          │
│                 │                   │  ┌──────────────────────┐      │
│                 │                   │  │ DDSLink (CycloneDDS) │      │
│                 │                   │  │ → DDSDataInjector    │      │
│                 │                   │  │ → Vehicle/Fact 系统   │      │
│                 │                   │  └──────────────────────┘      │
└─────────────────┘                   └─────────────────────────────────┘
```

**关键点**：
- 安卓端运行一个 `zenoh-bridge-dds` 子进程
- 子进程通过 Zenoh TCP 连接机载端
- 子进程在本地创建 CycloneDDS participant
- QGC 的 DDSLink 连接本地 CycloneDDS（localhost 回环）
- 所有现有功能（航点规划、手柄控制、心跳等）完全不变

## 前置条件

### 编译 zenoh-bridge-dds 需要

| 工具 | 版本 | 安装方法 |
|------|------|---------|
| Rust | 1.75+ | `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \| sh` |
| Android NDK | r25c+ | https://developer.android.com/ndk/downloads |
| CMake | 3.16+ | `apt install cmake` |
| Git | 2.x | `apt install git` |

### 编译 QGC APK 需要

| 工具 | 说明 |
|------|------|
| Qt 6.x (Android) | 包含 arm64-v8a 工具链 |
| CycloneDDS | 已为 Android arm64 编译 |
| Android SDK | API Level 28+ |

## 第一步：编译 zenoh-bridge-dds

### 方法 A：使用自动脚本（推荐）

```bash
cd ~/qgc-android    # 或你的 QGC 源码目录
git checkout DDS_zenoh
git pull

# 运行编译脚本（传入 NDK 路径）
./tools/build_zenoh_bridge_android.sh /path/to/android-ndk-r25c
```

脚本会自动：
1. 安装 Rust aarch64-linux-android target
2. 克隆 zenoh-plugin-dds 源码
3. 配置 Android 交叉编译工具链
4. 编译 release 版本
5. 复制到 `android/assets/zenoh-bridge-dds`

### 方法 B：手动编译

```bash
# 1. 安装 Rust 和 Android target
rustup target add aarch64-linux-android

# 2. 设置 NDK 环境
export ANDROID_NDK_HOME=/path/to/android-ndk-r25c
export NDK_TOOLCHAIN=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64

# 3. 克隆源码
git clone --depth 1 --branch 1.1.1 \
    https://github.com/eclipse-zenoh/zenoh-plugin-dds.git
cd zenoh-plugin-dds

# 4. 配置交叉编译
export CC_aarch64_linux_android=$NDK_TOOLCHAIN/bin/aarch64-linux-android28-clang
export CXX_aarch64_linux_android=$NDK_TOOLCHAIN/bin/aarch64-linux-android28-clang++
export AR_aarch64_linux_android=$NDK_TOOLCHAIN/bin/llvm-ar
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER=$CC_aarch64_linux_android

mkdir -p .cargo
cat > .cargo/config.toml << 'EOF'
[target.aarch64-linux-android]
linker = "aarch64-linux-android28-clang"
EOF

# 5. 编译
cargo build --release --target aarch64-linux-android \
    --package zenoh-bridge-dds

# 6. 复制到 QGC 项目
cp target/aarch64-linux-android/release/zenoh-bridge-dds \
    ~/qgc-android/android/assets/zenoh-bridge-dds

# 7. 精简体积
$NDK_TOOLCHAIN/bin/llvm-strip ~/qgc-android/android/assets/zenoh-bridge-dds
```

### 验证编译结果

```bash
file android/assets/zenoh-bridge-dds
# 应输出: ELF 64-bit LSB executable, ARM aarch64, ...

ls -lh android/assets/zenoh-bridge-dds
# 预期大小: 10-20 MB (strip 后)
```

## 第二步：编译 QGC APK

```bash
cd ~/qgc-android
git checkout DDS_zenoh
git pull

# 确保 zenoh-bridge-dds 二进制在 android/assets/ 中
ls android/assets/zenoh-bridge-dds

# 正常编译 QGC（和之前一样）
rm -rf build-android
cmake -B build-android -S . \
  -DCMAKE_TOOLCHAIN_FILE=$QT_DIR/lib/cmake/Qt6/qt.toolchain.cmake \
  -DQGC_ENABLE_DDS=ON \
  -DCYCLONEDDS_ROOT=/path/to/cyclonedds-android \
  -G Ninja

cmake --build build-android

# 签名 APK（按之前的步骤）
```

**注意**：`android/assets/zenoh-bridge-dds` 会被自动打包到 APK 中。

## 第三步：使用

### 3.1 机载端配置

确保机载端的 zenoh-bridge-dds 以 **router 或 peer 模式**运行，监听 TCP 端口：

```bash
# 机载端
zenoh-bridge-dds -m peer -l tcp/0.0.0.0:7447 -d 0
```

或使用配置文件：

```json5
{
  mode: "peer",
  listen: {
    endpoints: ["tcp/0.0.0.0:7447"]
  },
  plugins: {
    dds: {
      domain: 0,
      allow: { publishers: [".*"], subscribers: [".*"] }
    }
  }
}
```

### 3.2 安卓端操作

1. 安装编译好的 APK
2. 打开 QGC → 添加新的 DDS 连接
3. 开启 **"Enable Zenoh Bridge"** 开关
4. 在 **"Remote Endpoint"** 中填写机载端 IP：`tcp/192.168.1.100:7447`
5. 其他设置（Domain ID、Profile 等）保持不变
6. 点击连接

### 3.3 连接验证

连接成功后，logcat 中应该看到：

```
[ZenohBridge] Binary extracted to: /data/user/0/.../files/zenoh/zenoh-bridge-dds
[ZenohBridge] Config written to: /data/user/0/.../files/zenoh/bridge_config.json5
[ZenohBridge] Started, endpoint: tcp/192.168.1.100:7447
[DDSLink] CycloneDDS configured for localhost (Zenoh bridge mode)
[DDSLink] Created DDS participant on domain 0
[DDSLink] Zenoh bridge started, endpoint: tcp/192.168.1.100:7447
```

## 常见问题

### Q: Bridge 启动失败 "binary extraction failed"
A: 确认 `zenoh-bridge-dds` 二进制已放在 `android/assets/` 目录中并重新编译了 APK。

### Q: Bridge 启动后立即退出
A: 查看 logcat 中 `[zenoh-bridge]` 的输出。常见原因：
- 机载端 IP/端口不对
- 机载端 zenoh-bridge-dds 未启动
- 网络不通（先用 ping 验证）

### Q: DDS 话题不匹配
A: Zenoh bridge 模式下，CycloneDDS 限制在 localhost。如果 DDS domain ID 不匹配（默认 0），
修改 QGC 中的 Domain ID 使其与机载端一致。

### Q: 与 DDS_P5_V3 的功能对比
A: DDS_zenoh 包含 DDS_P5_V3 的所有功能（多版本 IDL、摇杆、航点规划等）。
唯一的区别是增加了 Zenoh bridge 选项。不开启 Zenoh bridge 时行为完全一致。

### Q: Rust 编译时间太长
A: 首次编译 zenoh-plugin-dds 需要 10-20 分钟（取决于机器性能）。后续增量编译会快很多。
可以使用 `cargo build --release -j4` 限制并行数以降低内存占用。

### Q: NDK 版本兼容性
A: 推荐使用 NDK r25c 或更新版本。r21e 可能缺少某些 LLVM 工具。
