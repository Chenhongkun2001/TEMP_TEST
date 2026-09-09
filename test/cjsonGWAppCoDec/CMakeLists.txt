cmake_minimum_required(VERSION 3.10)
# Project name: cjsonGWAppCoDec
# Version: 1.0.0
# Language: C or CXX (e.g., C++)
# Description: "A test project to verify the json CoDec of gwConfig.json/sensorConfig.json"
project(cjsonGWAppCoDec VERSION 1.0.0 LANGUAGES C CXX DESCRIPTION "A test project to verify the json CoDec of gwConfig.json/sensorConfig.json")

# To generate some files automatically
# Set the project source dir (relative to build/)
set(PROJECT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source")

# To setup the tool chain ...
set(CMAKE_SYSTEM_NAME Linux)
set(TOOLCHAIN_DIR "/home/forlinx/work/OK62xx-linux-sdk/external-toolchain-dir/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu")
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-g++)
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-gcc)

# To assign the language standard and general compiler's flags
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 99)
add_compile_options(-Wall -Wextra -pedantic)

# Set build type
# Ones can use "cmake -DCMAKE_BUILD_TYPE=Debug" or "cmake -DCMAKE_BUILD_TYPE=Release" to switch 
# the compilition mode between release or debug
MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
# Compile options for Debug variant
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
MESSAGE(STATUS "CMAKE_C_FLAGS_DEBUG = ${CMAKE_C_FLAGS_DEBUG}")
MESSAGE(STATUS "CMAKE_CXX_FLAGS_DEBUG = ${CMAKE_CXX_FLAGS_DEBUG}")
# Compile options for Release variant
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O0")
MESSAGE(STATUS "CMAKE_C_FLAGS_RELEASE = ${CMAKE_C_FLAGS_RELEASE}")
MESSAGE(STATUS "CMAKE_CXX_FLAGS_RELEASE = ${CMAKE_CXX_FLAGS_RELEASE}")

# To include the headers' path
include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/../lib/include")

# To add the source code's path
FILE(GLOB SRC_FILES_TMP ${PROJECT_SOURCE_DIR}/src/*.c)
set(SRC_FILES ${SRC_FILES} ${SRC_FILES_TMP})
FILE(GLOB SRC_FILES_TMP ${PROJECT_SOURCE_DIR}/*.c)
set(SRC_FILES ${SRC_FILES} ${SRC_FILES_TMP})
MESSAGE(DEBUG "SRC_FILES = ${SRC_FILES}")

set(CJSON_LIB_DIR "${PROJECT_SOURCE_DIR}/../lib/lib")
find_library(LIB_CJSON cjson ${CJSON_LIB_DIR})

# To add the source code
add_executable(cjsonGWAppCoDec ${SRC_FILES})
target_link_libraries(cjsonGWAppCoDec ${LIB_CJSON})