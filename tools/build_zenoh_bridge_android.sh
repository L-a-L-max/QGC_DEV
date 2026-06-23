#!/usr/bin/env bash
# =============================================================================
# Build zenoh-bridge-dds for Android arm64-v8a
# =============================================================================
#
# Prerequisites:
#   1. Rust toolchain: curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
#   2. Android NDK r25+ (set ANDROID_NDK_HOME or pass as argument)
#   3. Git, cmake, clang
#
# Usage:
#   ./tools/build_zenoh_bridge_android.sh [ANDROID_NDK_HOME]
#
# Output:
#   android/assets/zenoh-bridge-dds  (arm64-v8a binary)
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# --- Configuration ---
ZENOH_PLUGIN_DDS_VERSION="1.1.1"
ZENOH_PLUGIN_DDS_REPO="https://github.com/eclipse-zenoh/zenoh-plugin-dds.git"
ANDROID_TARGET="aarch64-linux-android"
ANDROID_API_LEVEL=28
BUILD_DIR="${PROJECT_ROOT}/build_zenoh_bridge"

# --- Android NDK ---
NDK_HOME="${1:-${ANDROID_NDK_HOME:-${ANDROID_NDK:-}}}"
if [ -z "$NDK_HOME" ]; then
    echo "ERROR: Android NDK not found."
    echo "Set ANDROID_NDK_HOME or pass as argument:"
    echo "  $0 /path/to/android-ndk-r25c"
    exit 1
fi

if [ ! -d "$NDK_HOME/toolchains/llvm/prebuilt" ]; then
    echo "ERROR: Invalid NDK path: $NDK_HOME"
    echo "Expected toolchains/llvm/prebuilt/ directory"
    exit 1
fi

# Detect host OS
HOST_OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
case "$HOST_OS" in
    linux)  HOST_TAG="linux-x86_64" ;;
    darwin) HOST_TAG="darwin-x86_64" ;;
    *)      echo "ERROR: Unsupported host OS: $HOST_OS"; exit 1 ;;
esac

TOOLCHAIN="${NDK_HOME}/toolchains/llvm/prebuilt/${HOST_TAG}"
if [ ! -d "$TOOLCHAIN" ]; then
    echo "ERROR: Toolchain not found at: $TOOLCHAIN"
    exit 1
fi

echo "=== Build Configuration ==="
echo "NDK:        $NDK_HOME"
echo "Toolchain:  $TOOLCHAIN"
echo "Target:     $ANDROID_TARGET"
echo "API Level:  $ANDROID_API_LEVEL"
echo "Build Dir:  $BUILD_DIR"
echo "==========================="

# --- Install Rust Android target ---
echo ""
echo ">>> Adding Rust target: $ANDROID_TARGET"
rustup target add "$ANDROID_TARGET"

# --- Clone zenoh-plugin-dds ---
echo ""
echo ">>> Cloning zenoh-plugin-dds (v${ZENOH_PLUGIN_DDS_VERSION})"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

git clone --depth 1 --branch "${ZENOH_PLUGIN_DDS_VERSION}" \
    "$ZENOH_PLUGIN_DDS_REPO" zenoh-plugin-dds 2>/dev/null || \
git clone --depth 1 "$ZENOH_PLUGIN_DDS_REPO" zenoh-plugin-dds

cd zenoh-plugin-dds

# --- Configure Cargo for Android cross-compilation ---
echo ""
echo ">>> Configuring Cargo for Android cross-compilation"

export CC_aarch64_linux_android="${TOOLCHAIN}/bin/aarch64-linux-android${ANDROID_API_LEVEL}-clang"
export CXX_aarch64_linux_android="${TOOLCHAIN}/bin/aarch64-linux-android${ANDROID_API_LEVEL}-clang++"
export AR_aarch64_linux_android="${TOOLCHAIN}/bin/llvm-ar"
export RANLIB_aarch64_linux_android="${TOOLCHAIN}/bin/llvm-ranlib"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="${CC_aarch64_linux_android}"

# CycloneDDS (bundled by cyclors crate) needs cmake to find NDK
export CMAKE_TOOLCHAIN_FILE="${NDK_HOME}/build/cmake/android.toolchain.cmake"
export ANDROID_ABI="arm64-v8a"
export ANDROID_PLATFORM="android-${ANDROID_API_LEVEL}"

# Write cargo config
mkdir -p .cargo
cat > .cargo/config.toml << EOF
[target.aarch64-linux-android]
linker = "${CC_aarch64_linux_android}"
ar = "${AR_aarch64_linux_android}"

[env]
CC_aarch64-linux-android = "${CC_aarch64_linux_android}"
CXX_aarch64-linux-android = "${CXX_aarch64_linux_android}"
AR_aarch64-linux-android = "${AR_aarch64_linux_android}"
EOF

# --- Build ---
echo ""
echo ">>> Building zenoh-bridge-dds for $ANDROID_TARGET (this may take 10-20 min)"
cargo build --release --target "$ANDROID_TARGET" \
    --package zenoh-bridge-dds \
    --features zenoh/transport_tcp

BUILD_RC=$?
if [ $BUILD_RC -ne 0 ]; then
    echo ""
    echo "ERROR: Build failed (exit code $BUILD_RC)"
    echo ""
    echo "Common issues:"
    echo "  1. Missing Android NDK clang: check $CC_aarch64_linux_android exists"
    echo "  2. cmake not finding NDK: ensure cmake >= 3.16"
    echo "  3. CycloneDDS build error: may need to set CYCLONEDDS_HOME"
    exit $BUILD_RC
fi

# --- Copy binary to APK assets ---
BINARY="target/${ANDROID_TARGET}/release/zenoh-bridge-dds"
if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary not found at: $BINARY"
    exit 1
fi

DEST="${PROJECT_ROOT}/android/assets/zenoh-bridge-dds"
mkdir -p "$(dirname "$DEST")"
cp "$BINARY" "$DEST"

# Strip debug symbols to reduce size
"${TOOLCHAIN}/bin/llvm-strip" "$DEST" 2>/dev/null || true

BINARY_SIZE=$(du -h "$DEST" | cut -f1)
echo ""
echo "=== Build Complete ==="
echo "Binary: $DEST"
echo "Size:   $BINARY_SIZE"
echo ""
echo "The binary has been placed in android/assets/zenoh-bridge-dds"
echo "Rebuild the QGC APK to include it."
