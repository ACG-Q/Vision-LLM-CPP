cmake_minimum_required(VERSION 3.10)
project(PaddleOCR-Library)

set(CMAKE_CXX_STANDARD 11)

# Options
option(WITH_LITE "Build with Paddle Lite (Android)" OFF)

if(ANDROID OR WITH_LITE)
    message(STATUS "Build for Android with Paddle Lite")
    set(PADDLE_LITE_DIR "" CACHE PATH "Path to Paddle Lite directory")
    
    include_directories(${PADDLE_LITE_DIR}/cxx/include)
    link_directories(${PADDLE_LITE_DIR}/cxx/lib)
    
    set(PADDLE_LIBS paddle_light_api_shared)
    add_definitions(-DWITH_LITE)
else()
    message(STATUS "Build for Windows with Paddle Inference")
    set(PADDLE_LIB "" CACHE PATH "Path to Paddle Inference directory")
    set(OPENCV_DIR "" CACHE PATH "Path to OpenCV directory")

    # Found OpenCV
    find_package(OpenCV REQUIRED PATHS ${OPENCV_DIR})
    include_directories(${OpenCV_INCLUDE_DIRS})

    # Found Paddle Inference
    include_directories(${PADDLE_LIB}/paddle/include)
    link_directories(${PADDLE_LIB}/paddle/lib)
    
    set(PADDLE_LIBS paddle_inference)
endif()

# Dummy source file to make it a valid project
# In a real project, you would add your source files here
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp "
#include <iostream>
#ifdef WITH_LITE
#include \"paddle_api.h\"
#else
#include \"paddle_inference_api.h\"
#endif

int main() {
    std::cout << \"PaddleOCR Library Initialized\" << std::endl;
    return 0;
}
")
endif()

add_library(ocr_lib SHARED main.cpp)

target_link_libraries(ocr_lib
    ${PADDLE_LIBS}
    ${OpenCV_LIBS}
)
