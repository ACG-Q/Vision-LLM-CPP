#!/bin/bash
BUILD_TYPE=$1

# 1. 下载 Paddle Lite 预测库
# 使用 clang 版本的库以获得更好的 NDK 兼容性
echo "Downloading Paddle Lite (Clang version)..."
wget -q https://github.com/PaddlePaddle/Paddle-Lite/releases/download/v2.14-rc/inference_lite_lib.android.armv8.clang.c++_shared.with_extra.with_cv.tar.gz -O paddle_lite.tar.gz
tar -xf paddle_lite.tar.gz

# 重命名以配合 CMakeLists.txt
mv inference_lite_lib.android.armv8.clang.c++_shared.with_extra.with_cv inference_lite_lib

# 2. 修复 Paddle Lite 符号表问题 (解决 LLD 链接器报错)
echo "Fixing Paddle Lite symbol table for LLD compatibility..."
LLVM_OBJCOPY="${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-objcopy"
if [ -f "$LLVM_OBJCOPY" ]; then
    TARGET_SO="inference_lite_lib/cxx/lib/libpaddle_light_api_shared.so"
    $LLVM_OBJCOPY --localize-symbol=_edata $TARGET_SO
    $LLVM_OBJCOPY --localize-symbol=__end__ $TARGET_SO
    $LLVM_OBJCOPY --localize-symbol=__bss_end__ $TARGET_SO
    $LLVM_OBJCOPY --localize-symbol=_bss_end__ $TARGET_SO
    $LLVM_OBJCOPY --localize-symbol=__bss_start__ $TARGET_SO
    $LLVM_OBJCOPY --localize-symbol=_end $TARGET_SO
    $LLVM_OBJCOPY --localize-symbol=__bss_start $TARGET_SO
    echo "Symbol localization completed."
else
    echo "Warning: llvm-objcopy not found at $LLVM_OBJCOPY, skipping fix."
fi

# 3. 执行编译
echo "Configuring and building..."
echo "Using ANDROID_NDK_HOME: $ANDROID_NDK_HOME"
mkdir -p build_android && cd build_android
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI="arm64-v8a" \
    -DANDROID_PLATFORM=android-23 \
    -DPADDLE_LITE_DIR=$(pwd)/../inference_lite_lib \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE

make -j$(nproc)
