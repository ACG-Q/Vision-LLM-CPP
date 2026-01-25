#!/bin/bash
BUILD_TYPE=$1

# 1. 下载 Paddle Lite 预测库
# 注意：这里需要根据实际需求选择合适的版本和架构
echo "Downloading Paddle Lite..."
wget -q  https://github.com/PaddlePaddle/Paddle-Lite/releases/download/v2.14-rc/inference_lite_lib.android.armv8.gcc.c++_shared.with_extra.with_cv.tar.gz -O paddle_lite.tar.gz
tar -xf paddle_lite.tar.gz

# 重命名以配合 CMakeLists.txt
mv inference_lite_lib.android.armv8.gcc.c++_shared.with_extra.with_cv inference_lite_lib

# 2. 执行编译
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
