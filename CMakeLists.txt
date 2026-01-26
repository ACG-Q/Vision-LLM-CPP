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
    message(STATUS "PADDLE_LIB set to: ${PADDLE_LIB}")
    
    # Try multiple common include patterns for Paddle Inference
    set(PADDLE_INCLUDE_SEARCH_PATHS 
        "${PADDLE_LIB}/paddle/include"
        "${PADDLE_LIB}/include"
    )
    
    set(PADDLE_FOUND_INCLUDE "")
    foreach(path ${PADDLE_INCLUDE_SEARCH_PATHS})
        if(EXISTS "${path}/paddle_inference_api.h")
            set(PADDLE_FOUND_INCLUDE ${path})
            message(STATUS "Found Paddle include at: ${PADDLE_FOUND_INCLUDE}")
            break()
        endif()
    endforeach()

    if(NOT PADDLE_FOUND_INCLUDE)
        message(FATAL_ERROR "Could not find paddle_inference_api.h in ${PADDLE_LIB}. Checked: ${PADDLE_INCLUDE_SEARCH_PATHS}")
    endif()

    include_directories(${PADDLE_FOUND_INCLUDE})
    
    # Search for libraries
    if(EXISTS "${PADDLE_LIB}/paddle/lib")
        link_directories(${PADDLE_LIB}/paddle/lib)
    elseif(EXISTS "${PADDLE_LIB}/lib")
        link_directories(${PADDLE_LIB}/lib)
    endif()
    
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

if(ANDROID OR WITH_LITE)
    # Add linker flags to handle Paddle Lite symbol issues on newer NDKs
    # --no-fatal-warnings: Treat ELF symbol violations as warnings
    # --no-check-sections: Skip section checks that might fail on older binaries
    target_link_options(ocr_lib PRIVATE 
        "-Wl,--allow-shlib-undefined"
        "-Wl,-z,notext"
        "-Wl,--no-fatal-warnings"
        "-Wl,--no-check-sections"
    )
endif()
