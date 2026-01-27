cmake_minimum_required(VERSION 3.10)
project(PaddleOCR-Library)

set(CMAKE_CXX_STANDARD 11)

# Options
option(WITH_LITE "Build with Paddle Lite (Android)" OFF)

# 公共包含路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# --- 平台逻辑分支 ---

if(ANDROID OR WITH_LITE)
    message(STATUS "Build for Android with Paddle Lite")
    
    find_package(OpenCV REQUIRED PATHS ${OPENCV_DIR} NO_DEFAULT_PATH)
    
    include_directories(${PADDLE_LITE_DIR}/cxx/include)
    link_directories(${PADDLE_LITE_DIR}/cxx/lib)
    
    set(PADDLE_LIBS paddle_light_api_shared)
    add_definitions(-DWITH_LITE)
else()
    message(STATUS "Build for Windows with Paddle Inference")
    
    find_package(OpenCV REQUIRED PATHS ${OPENCV_DIR})
    
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
        message(FATAL_ERROR "Could not find paddle_inference_api.h in ${PADDLE_LIB}")
    endif()

    include_directories(${PADDLE_FOUND_INCLUDE})
    
    if(EXISTS "${PADDLE_LIB}/paddle/lib")
        link_directories(${PADDLE_LIB}/paddle/lib)
    elseif(EXISTS "${PADDLE_LIB}/lib")
        link_directories(${PADDLE_LIB}/lib)
    endif()
    
    set(PADDLE_LIBS paddle_inference)
endif()

include_directories(${OpenCV_INCLUDE_DIRS})

# --- 生成目标 ---

set(SOURCES
    ocr_engine.cpp
    clipper.cpp
)

add_library(ocr_engine SHARED ${SOURCES})

target_link_libraries(ocr_engine
    ${PADDLE_LIBS}
    ${OpenCV_LIBS}
)

if(ANDROID OR WITH_LITE)
    target_link_options(ocr_engine PRIVATE 
        "-Wl,--allow-shlib-undefined"
        "-Wl,-z,notext"
        "-Wl,--no-fatal-warnings"
        "-Wl,--no-check-sections"
    )
endif()

# --- 安装/打包逻辑 ---

# 1. 安装核心库和头文件
install(TARGETS ocr_engine
        LIBRARY DESTINATION .
        RUNTIME DESTINATION .
        ARCHIVE DESTINATION .)

install(FILES ocr_engine.h DESTINATION .)

# 2. 安装第三方依赖 (二进制运行库)
if(ANDROID OR WITH_LITE)
    # Android: 收集 Paddle Lite 和 OpenCV 的 .so
    install(FILES "${PADDLE_LITE_DIR}/cxx/lib/libpaddle_light_api_shared.so" DESTINATION .)
    
    # 使用 GLOB 获取 OpenCV 所有 .so 文件
    file(GLOB OPENCV_RUNTIME_LIBS "${OPENCV_DIR}/../libs/arm64-v8a/*.so")
    install(FILES ${OPENCV_RUNTIME_LIBS} DESTINATION .)
else()
    # Windows: 递归搜集 Paddle 和 OpenCV 的 .dll
    file(GLOB_RECURSE PADDLE_DLLS "${PADDLE_LIB}/*.dll")
    install(FILES ${PADDLE_DLLS} DESTINATION .)

    # OpenCV 运行库通常在 build/x64/vc16/bin 目录下
    file(GLOB_RECURSE OPENCV_DLLS "${OPENCV_DIR}/*.dll")
    foreach(dll_path ${OPENCV_DLLS})
        # 只收集 bin 目录下的 dll，避免收集不必要的测试工具
        if(dll_path MATCHES "bin")
            install(FILES ${dll_path} DESTINATION .)
        endif()
    endforeach()
endif()
