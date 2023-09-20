# CMake Requirement
cmake_minimum_required(VERSION 3.15)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup project
project(AstraSim_Analytical)

# Compilation target
set(BUILDTARGET "all" CACHE STRING "Compilation target ([all]/congestion_unaware/congestion_aware)")

# Compile AstraSim library
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../" AstraSim)

# Compile congestion_unaware
if (BUILDTARGET STREQUAL "all" OR BUILDTARGET STREQUAL "congestion_unaware")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../extern/network_backend/analytical/congestion_unaware" Congestion_Unaware)
endif ()

# Compile congestion_aware
if (BUILDTARGET STREQUAL "all" OR BUILDTARGET STREQUAL "congestion_aware")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../extern/network_backend/analytical/congestion_aware" Congestion_Aware)
endif ()
