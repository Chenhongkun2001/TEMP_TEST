cmake_minimum_required(VERSION 3.10)
# Project name: dwthuffman
# Version: 1.0.0
# Language: C or CXX (e.g., C++)
# Description: "Quick verification of data compression based on DWT and Hoffman coding"
project(dwthuffman VERSION 1.0.0 LANGUAGES C CXX DESCRIPTION "Quick verification of data compression based on DWT and Hoffman coding")

# To generate some files automatically
# Set the project source dir (relative to build/)
set(PROJECT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source")
# # Once cmake is called, the helloworld_version.h would be generated and version would be filled.
# # Here, the path of helloworld_version.h.in is related to the path of CMakeLists.txt.
# configure_file("${PROJECT_SOURCE_DIR}/include/helloworld_version.h.in" "${PROJECT_SOURCE_DIR}/include/helloworld_version.h")

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

# Find math library
find_library(MATH_LIB_DIR m)
MESSAGE(STATUS "MATH_LIB_DIR = ${SKF_GW_LIB_DL}")

set(SRCS ${PROJECT_SOURCE_DIR}/main.c
         ${PROJECT_SOURCE_DIR}/dwt.c
         ${PROJECT_SOURCE_DIR}/huffman.c)

# # To include the headers' path
# include_directories("${PROJECT_SOURCE_DIR}/include")
# To add the source code
add_executable(dwthuffman ${SRCS})
target_link_libraries(dwthuffman ${MATH_LIB_DIR})