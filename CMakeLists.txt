cmake_minimum_required(VERSION 3.10)
project(PaddleOCR-Library)

set(CMAKE_CXX_STANDARD 11)

# Options
option(WITH_LITE "Build with Paddle Lite (Android)" OFF)

# --- 平台逻辑分支 ---

if(ANDROID OR WITH_LITE)
    message(STATUS "Build for Android with Paddle Lite")
    
    # Android 必须手动指定 OpenCV 路径，否则 cross-compile 会找不到
    # 我们使用命令行传入的 OPENCV_DIR
    find_package(OpenCV REQUIRED PATHS ${OPENCV_DIR} NO_DEFAULT_PATH)
    
    set(PADDLE_LITE_DIR "" CACHE PATH "Path to Paddle Lite directory")
    include_directories(${PADDLE_LITE_DIR}/cxx/include)
    link_directories(${PADDLE_LITE_DIR}/cxx/lib)
    
    set(PADDLE_LIBS paddle_light_api_shared)
    add_definitions(-DWITH_LITE)
else()
    message(STATUS "Build for Windows with Paddle Inference")
    
    # Windows 正常寻找 OpenCV
    find_package(OpenCV REQUIRED PATHS ${OPENCV_DIR})
    
    set(PADDLE_LIB "" CACHE PATH "Path to Paddle Inference directory")
    set(PADDLE_INCLUDE_SEARCH_PATHS 
        "${PADDLE_LIB}/paddle/include"
        "${PADDLE_LIB}/include"
    )
    
    set(PADDLE_FOUND_INCLUDE "")
    foreach(path ${PADDLE_INCLUDE_SEARCH_PATHS})
        if(EXISTS "${path}/paddle_inference_api.h")
            set(PADDLE_FOUND_INCLUDE ${path})
            break()
        endif()
    endforeach()

    if(NOT PADDLE_FOUND_INCLUDE)
        message(FATAL_ERROR "Could not find paddle_inference_api.h")
    endif()

    include_directories(${PADDLE_FOUND_INCLUDE})
    
    if(EXISTS "${PADDLE_LIB}/paddle/lib")
        link_directories(${PADDLE_LIB}/paddle/lib)
    elseif(EXISTS "${PADDLE_LIB}/lib")
        link_directories(${PADDLE_LIB}/lib)
    endif()
    
    set(PADDLE_LIBS paddle_inference)
endif()

# 公共包含路径
include_directories(${OpenCV_INCLUDE_DIRS})

# --- 生成目标 ---

# 建议确保 core 文件夹存在，或者直接把 ocr_engine.cpp 放在根目录
add_library(ocr_engine SHARED core/ocr_engine.cpp)

target_link_libraries(ocr_engine
    ${PADDLE_LIBS}
    ${OpenCV_LIBS}
)

# Android 特有的链接修复
if(ANDROID OR WITH_LITE)
    target_link_options(ocr_engine PRIVATE 
        "-Wl,--allow-shlib-undefined"
        "-Wl,-z,notext"
        "-Wl,--no-fatal-warnings"
        "-Wl,--no-check-sections"
    )
endif()