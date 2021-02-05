## i have cmake 3.5
cmake_minimum_required(VERSION 3.5)
project(jsonperfmon)

enable_testing()

## use this to globally use C11 with in our project
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -m64 -funroll-loops -ffinite-math-only -g -rdynamic -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -rdynamic -pthread")

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE STRING "" FORCE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/modules")

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

set(SOURCE_FILES 
    src/glib_compat.c
    src/jsonperf.c
    src/perflinux.c
    src/proclinux.c)
add_executable(jsonperfmon ${SOURCE_FILES})

target_include_directories(jsonperfmon PUBLIC src)
