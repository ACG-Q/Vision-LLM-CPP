param([string]$BuildType = "Release")

$ErrorActionPreference = "Stop"

# 1. 下载并准备 OpenCV
if (!(Test-Path "opencv")) {
    Write-Host "Downloading OpenCV 4.13.0..."
    Invoke-WebRequest -Uri "https://github.com/opencv/opencv/releases/download/4.13.0/opencv-4.13.0-windows.exe" -OutFile "opencv.exe"
    Write-Host "Extracting OpenCV..."
    if (Get-Command "7z" -ErrorAction SilentlyContinue) {
        7z x opencv.exe -o"opencv_tmp" -y
    } else {
        Start-Process -FilePath ".\opencv.exe" -ArgumentList "-o`"opencv_tmp`" -y" -Wait
    }
    Move-Item "opencv_tmp/opencv" "opencv"
    Remove-Item "opencv_tmp" -Recurse -Force
    Remove-Item "opencv.exe" -Force
}

# 2. 下载 Paddle Inference 库
if (!(Test-Path "paddle_inference")) {
    Write-Host "Downloading Paddle Inference 3.0.0..."
    Invoke-WebRequest -Uri "https://paddle-inference-lib.bj.bcebos.com/3.0.0/cxx_c/Windows/CPU/x86-64_avx-mkl-vs2019/paddle_inference.zip" -OutFile "paddle.zip"
    Write-Host "Extracting Paddle Inference..."
    Expand-Archive -Path "paddle.zip" -DestinationPath "paddle_tmp"
    
    $extractedItems = Get-ChildItem "paddle_tmp"
    if ($extractedItems.Count -eq 1 -and $extractedItems[0].Attributes -band [io.fileattributes]::Directory) {
        Move-Item "paddle_tmp/$($extractedItems[0].Name)/*" "paddle_inference"
    } else {
        Move-Item "paddle_tmp/*" "paddle_inference"
    }
    Remove-Item "paddle_tmp" -Recurse -Force
    Remove-Item "paddle.zip" -Force
}

# 3. 编译
Write-Host "Configuring CMake for Windows..."
if (Test-Path "build_win") { Remove-Item "build_win" -Recurse -Force }
mkdir build_win
cd build_win

cmake ..\ocr_library -G "Visual Studio 17 2022" -A x64 `
    -DOPENCV_DIR="$PSScriptRoot/../opencv/build" `
    -DPADDLE_LIB="$PSScriptRoot/../paddle_inference" `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DCMAKE_INSTALL_PREFIX="../output_win"

Write-Host "Building project..."
cmake --build . --config $BuildType
Write-Host "Installing to output_win..."
cmake --install . --config $BuildType