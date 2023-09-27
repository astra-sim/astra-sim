# CMake Requirement
cmake_minimum_required(VERSION 3.15)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "-O3 -Wall")

# Setup project
project(AstraSim_Analytical)

# Compile AstraSim library
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../" AstraSim)

# Compile congestion_unaware
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../extern/network_backend/analytical/congestion_unaware" Congestion_Unaware)

# Compile congestion_aware
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../extern/network_backend/analytical/congestion_aware" Congestion_Aware)
