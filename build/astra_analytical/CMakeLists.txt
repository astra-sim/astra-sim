# CMake Requirement
cmake_minimum_required(VERSION 3.15)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the build type to Release if not specified
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -Wall -Wextra -Wuninitialized -Wunused-function -Wno-unused-parameter -Wno-empty-body -fsanitize=address,undefined,leak -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Setup project
project(AstraSim_Analytical)

# Compilation target
set(BUILDTARGET "all" CACHE STRING "Compilation target ([all]/congestion_unaware/congestion_aware)")

# Compile AstraSim library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../ AstraSim)

# build appropriate analytical network backend as library
set(NETWORK_BACKEND_BUILD_AS_LIBRARY ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../extern/network_backend/analytical/ Analytical)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../astra-sim/network_frontend/analytical/ AstraSim_Analytical)
