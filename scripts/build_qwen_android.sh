#!/bin/bash
BUILD_TYPE=$1

# 1. 克隆 llama.cpp
echo "Cloning llama.cpp..."
git clone https://github.com/ggerganov/llama.cpp.git --depth 1
cd llama.cpp

# 2. 执行编译
echo "Configuring and building llama.cpp for Android..."
mkdir -p build_android && cd build_android

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI="arm64-v8a" \
    -DANDROID_PLATFORM=android-23 \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DBUILD_SHARED_LIBS=ON \
    -DLLAMA_BUILD_EXAMPLES=ON

make -j$(nproc)

# 返回主目录并准备产物
cd ../..
mkdir -p build_qwen_android
cp llama.cpp/build_android/bin/llama-cli build_qwen_android/
cp llama.cpp/build_android/src/libllama.so build_qwen_android/
