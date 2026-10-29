# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

# CMake Requirement
cmake_minimum_required(VERSION 3.19)

# C++ requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the build type to Release if not specified
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# Setup project
project(AstraSim)

# Include JSON library
add_subdirectory(extern/helper)

# Include chakra
add_subdirectory(extern/graph_frontend/chakra)

# Fetch yaml-cpp
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchYamlCpp)

# Files to compile
file(GLOB srcs
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/common/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/common/logging/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/common/stats/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/common/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/workload/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/simple/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/simple/astraccl/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/simple/astraccl/native_collectives/collective_algorithm/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/simple/astraccl/native_collectives/logical_topology/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/memory/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/simple/scheduling/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/gpu/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/gpu/msccl/*.cc")

# Compile AstraSim Library
add_library(AstraSim STATIC ${srcs})

# remote_memory_backend/analytical, listed explicitly (source + header, for IDE visibility)
target_sources(AstraSim PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/remote_memory_backend/analytical/src/AnalyticalRemoteMemory.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/remote_memory_backend/analytical/include/astra-memory-analytical/AnalyticalRemoteMemory.hh")

# Link libraries
target_link_libraries(AstraSim PUBLIC ChakraEtFeeder)
target_link_libraries(AstraSim PUBLIC yaml-cpp::yaml-cpp)

# Include Directories
target_include_directories(AstraSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(AstraSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/remote_memory_backend/analytical/include)
target_include_directories(AstraSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/extern/helper/)

# Properties
set_target_properties(AstraSim PROPERTIES COMPILE_WARNING_AS_ERROR ON)
set_target_properties(AstraSim
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/
)
