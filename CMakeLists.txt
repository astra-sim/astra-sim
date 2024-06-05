# CMake Requirement
cmake_minimum_required(VERSION 3.15)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the build type to Release if not specified
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# Setup project
project(AstraSim)

# Add libraries.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/extern/helper/fmt")
option(SPDLOG_FMT_EXTERNAL ON)    # override default option for spdlog.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/extern/helper/spdlog")
find_package(Protobuf REQUIRED)

# Files to compile
file(GLOB srcs
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/workload/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/collective/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/topology/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/memory/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/scheduling/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/common/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/third_party/utils/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/et_def/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/et_feeder/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/remote_memory_backend/analytical/*.cc")

# Compile AstraSim Library
add_library(AstraSim STATIC ${srcs})

# Link libraries
target_link_libraries(AstraSim PUBLIC ${Protobuf_LIBRARIES})
target_link_libraries(AstraSim PUBLIC fmt::fmt)
target_link_libraries(AstraSim PUBLIC spdlog::spdlog)

# Include Directories
target_include_directories(AstraSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(AstraSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/)
target_include_directories(AstraSim PUBLIC ${Protobuf_INCLUDE_DIRS})
target_include_directories(AstraSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/extern/helper/)

# Properties
set_target_properties(AstraSim PROPERTIES COMPILE_WARNING_AS_ERROR ON)
set_target_properties(AstraSim
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../bin/
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../lib/
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../lib/
)
