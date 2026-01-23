param(
    [string]$BuildType = "Release",
    [bool]$WithCuda = $false
)

$ErrorActionPreference = "Stop"

# 1. 克隆 llama.cpp
if (!(Test-Path "llama.cpp")) {
    Write-Host "Cloning llama.cpp..."
    git clone https://github.com/ggerganov/llama.cpp.git --depth 1
}

# 2. 编译
Write-Host "Configuring CMake for llama.cpp (Windows)..."
if (Test-Path "build_qwen_win") { Remove-Item "build_qwen_win" -Recurse -Force }
mkdir build_qwen_win
cd build_qwen_win

$cmake_args = @(
    "..",
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DBUILD_SHARED_LIBS=ON",
    "-DLLAMA_BUILD_EXAMPLES=ON"
)

if ($WithCuda) {
    Write-Host "Enabling CUDA acceleration..."
    $cmake_args += "-DGGML_CUDA=ON"
}

cd ../llama.cpp
# Note: We build inside llama.cpp/build for better compatibility with its structure
if (Test-Path "build") { Remove-Item "build" -Recurse -Force }
mkdir build
cd build

cmake @cmake_args

Write-Host "Building llama.cpp..."
cmake --build . --config $BuildType -j $env:NUMBER_OF_PROCESSORS

# 返回并收集产物
cd ../..
# Workflow will pick up files from llama.cpp/build/bin/
# (This script just facilitates the build process)
mkdir -p build_qwen_win
Copy-Item "llama.cpp/build/bin" "build_qwen_win/" -Recurse
