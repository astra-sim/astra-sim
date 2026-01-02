# # *****************************************************************************
# # This source code is licensed under the MIT license found in the
# # LICENSE file in the root directory of this source tree.
# #
# # Copyright (c) 2024 Georgia Institute of Technology
# # *****************************************************************************

# CMake Requirement
cmake_minimum_required(VERSION 3.22)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Setup project
project(AstraSim)

# Add libraries.
include(FetchContent)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/extern/helper/fmt")
option(SPDLOG_FMT_EXTERNAL ON) # override default option for spdlog.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/extern/helper/spdlog")
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG a83cd31 # Current as of 2025-10-01
)
FetchContent_MakeAvailable(yaml-cpp)


# Find Protobuf
# TODO: We want to use the advanced semantics 'protobuf CONFIG REQUIRED'.
# However, this requires protobuf to be compiled from source,
# which is enabled only in Docker containers. (Our documentation makes users install protobuc from apt-get)
# Therefore, enable 'protobuf CONFIG REQUIRED' only when we are building in Docker.
# TODO: Find a better way to detect if protobuf has been compiled by source, OR
# find a way to enable 'protobuf CONFIG REQUIRED' regardless of how protubuf was installed.
if(DEFINED ENV{PROTOBUF_FROM_SOURCE} AND "$ENV{PROTOBUF_FROM_SOURCE}" STREQUAL "True")
    find_package(protobuf CONFIG REQUIRED)
else()
    find_package(Protobuf REQUIRED)
endif()

# Files to compile
file(GLOB srcs
    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/common/*.cc"

    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/basic_system/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/basic_system/astraccl/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/basic_system/astraccl/custom_collectives/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/basic_system/astraccl/native_collectives/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/basic_system/astraccl/native_collectives/collective_algorithm/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/basic_system/astraccl/native_collectives/logical_topology/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/basic_system/basic_model/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/basic_system/basic_model/scheduling/*.cc"

    "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/workload/*.cc"

    "${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/src/third_party/utils/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/schema/protobuf/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/src/feeder_v3/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/remote_memory_backend/analytical/*.cc")

# Compile AstraSim Library
add_library(AstraSim STATIC ${srcs})

# Link libraries
target_link_libraries(AstraSim PUBLIC fmt::fmt)
target_link_libraries(AstraSim PUBLIC spdlog::spdlog)
target_link_libraries(AstraSim PUBLIC yaml-cpp)

# Same as above.
if(DEFINED ENV{PROTOBUF_FROM_SOURCE} AND "$ENV{PROTOBUF_FROM_SOURCE}" STREQUAL "True")
    target_link_libraries(AstraSim PUBLIC protobuf::libprotobuf)
else()
    target_link_libraries(AstraSim PUBLIC ${Protobuf_LIBRARIES})
endif()


# Include Directories
target_include_directories(AstraSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(AstraSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/)
target_include_directories(AstraSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/schema/protobuf)
target_include_directories(AstraSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/src/third_party/utils)
target_include_directories(AstraSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/extern/helper/)
target_include_directories(AstraSim PUBLIC ${Protobuf_INCLUDE_DIR})

# Same as above.
if(DEFINED ENV{PROTOBUF_FROM_SOURCE} AND "$ENV{PROTOBUF_FROM_SOURCE}" STREQUAL "True")
    target_include_directories(AstraSim PUBLIC ${Protobuf_INCLUDE_DIRS})
endif()

# Properties
set_target_properties(AstraSim PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
set_target_properties(AstraSim
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../bin/
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../lib/
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../lib/
)
