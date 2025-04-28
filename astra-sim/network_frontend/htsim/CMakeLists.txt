# CMake Requirement
cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the build type to Release if not specified
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# Setup project
project(AstraSim_HTSim)

# Compilation target
set(BUILDTARGET "all" CACHE STRING "Compilation target ([all])")

# Include src files to compile
file(GLOB srcs_common
        ${CMAKE_CURRENT_SOURCE_DIR}/../analytical/common/*.cc
)

file(GLOB srcs_htsim
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
)

set (HTSIM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/network_backend/csg-htsim/sim)

set(srcs_htsim_extern   ${HTSIM_DIR}/datacenter/connection_matrix.cpp
                        ${HTSIM_DIR}/datacenter/fat_tree_switch.cpp
                        ${HTSIM_DIR}/datacenter/fat_tree_topology.cpp
                        ${HTSIM_DIR}/datacenter/firstfit.cpp
)

file(GLOB srcs_htsim_proto
        ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.cc
)

add_executable(AstraSim_HTSim ${srcs_htsim} ${srcs_common} ${srcs_htsim_extern} ${srcs_htsim_proto})
target_sources(AstraSim_HTSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/HTSimMain.cc)

# Link libraries
target_link_libraries(AstraSim_HTSim LINK_PRIVATE AstraSim)
target_link_libraries(AstraSim_HTSim LINK_PRIVATE Analytical_Congestion_Unaware)

# Add HTSim library
add_custom_target(htsim COMMAND make WORKING_DIRECTORY ${HTSIM_DIR})
target_link_libraries(AstraSim_HTSim LINK_PRIVATE ${HTSIM_DIR}/libhtsim.a)
add_dependencies(AstraSim_HTSim htsim)

# Include directories
target_include_directories(AstraSim_HTSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(AstraSim_HTSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/proto)
target_include_directories(AstraSim_HTSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../analytical/include/)
target_include_directories(AstraSim_HTSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/)
target_include_directories(AstraSim_HTSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/helper)
target_include_directories(AstraSim_HTSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/network_backend/csg-htsim/sim/)
target_include_directories(AstraSim_HTSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/network_backend/csg-htsim/sim/datacenter/)

# Properties
set_target_properties(AstraSim_HTSim PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
set_target_properties(AstraSim_HTSim
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../bin/
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../lib/
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../lib/
)
