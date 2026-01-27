#!/bin/bash
set -e
BUILD_TYPE=${1:-Release}

# 1. 下载 Paddle Lite
echo "Downloading Paddle Lite..."
wget -q https://github.com/PaddlePaddle/Paddle-Lite/releases/download/v2.14-rc/inference_lite_lib.android.armv8.clang.c++_shared.with_extra.with_cv.tar.gz -O paddle_lite.tar.gz
tar -xf paddle_lite.tar.gz
mv inference_lite_lib.android.armv8.clang.c++_shared.with_extra.with_cv inference_lite_lib

# 2. 下载 OpenCV Android SDK
echo "Downloading OpenCV Android SDK..."
wget -q https://github.com/opencv/opencv/releases/download/4.5.5/opencv-4.5.5-android-sdk.zip -O opencv_android.zip
unzip -q opencv_android.zip
OPENCV_SDK_DIR="$(pwd)/OpenCV-android-sdk"

# 3. 修复 Paddle Lite 符号表问题
echo "Fixing Paddle Lite symbol table..."
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
fi

# 4. 执行编译
echo "Configuring and building..."
ROOT_DIR=$(pwd)
mkdir -p build_android && cd build_android

PADDLE_PATH="$ROOT_DIR/inference_lite_lib"
OPENCV_JNI_PATH="$OPENCV_SDK_DIR/sdk/native/jni"
INSTALL_DIR="$ROOT_DIR/output_android"

cmake ../ocr_library \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI="arm64-v8a" \
    -DANDROID_PLATFORM=android-23 \
    -DWITH_LITE=ON \
    -DOPENCV_DIR="$OPENCV_JNI_PATH" \
    -DOpenCV_DIR="$OPENCV_JNI_PATH" \
    -DPADDLE_LITE_DIR="$PADDLE_PATH" \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"

make -j$(nproc)
make install

echo "Build complete. Artifacts in $INSTALL_DIR:"
ls -R "$INSTALL_DIR"