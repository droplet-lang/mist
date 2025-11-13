cmake_minimum_required(VERSION 3.18)
project(droplet_android)

set(CMAKE_CXX_STANDARD 20)

# Include Droplet headers
include_directories(
        droplet
        droplet/vm
        droplet/compiler
        droplet/debugger
        droplet/native
        droplet/external/dlfcn
)

# Gather all Droplet source files and Android bridge
file(GLOB_RECURSE DROPLET_SOURCES
        droplet/**/*.cpp
        droplet/**/*.c
        native_bridge.cpp
        droplet_vm_wrapper.cpp
        droplet_platform_api.cpp
)

# Build shared library for Android
add_library(droplet_native SHARED ${DROPLET_SOURCES})

# Link required Android libraries
find_library(log-lib log)
target_link_libraries(droplet_native ${log-lib} android)
