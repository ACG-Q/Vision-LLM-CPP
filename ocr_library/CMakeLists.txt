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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ocr_engine.h")
    install(FILES ocr_engine.h DESTINATION .)
endif()

# 2. 安装第三方依赖 (二进制运行库)
if(ANDROID OR WITH_LITE)
    # Android: 收集 Paddle Lite
    set(PADDLE_SO "${PADDLE_LITE_DIR}/cxx/lib/libpaddle_light_api_shared.so")
    if(EXISTS "${PADDLE_SO}")
        install(FILES "${PADDLE_SO}" DESTINATION .)
    endif()
    
    # 收集 OpenCV .so
    file(GLOB OPENCV_RUNTIME_LIBS "${OPENCV_DIR}/../libs/arm64-v8a/*.so")
    if(OPENCV_RUNTIME_LIBS)
        install(FILES ${OPENCV_RUNTIME_LIBS} DESTINATION .)
    endif()
else()
    # Windows: 收集 Paddle DLLs
    file(GLOB_RECURSE PADDLE_DLLS "${PADDLE_LIB}/*.dll")
    if(PADDLE_DLLS)
        install(FILES ${PADDLE_DLLS} DESTINATION .)
    endif()

    # OpenCV 运行库
    file(GLOB_RECURSE OPENCV_DLLS "${OPENCV_DIR}/*.dll")
    set(OPENCV_BIN_DLLS "")
    foreach(dll_path ${OPENCV_DLLS})
        if(dll_path MATCHES "bin")
            list(APPEND OPENCV_BIN_DLLS ${dll_path})
        endif()
    endforeach()
    
    if(OPENCV_BIN_DLLS)
        install(FILES ${OPENCV_BIN_DLLS} DESTINATION .)
    endif()
endif()
