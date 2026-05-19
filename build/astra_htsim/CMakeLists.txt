# CMake Requirement
cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -Wall -Wextra -Wuninitialized -Wunused-function -Wno-unused-parameter -Wno-empty-body -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

project(AstraSim_HTSim)

set(BUILDTARGET "all" CACHE STRING "Compilation target ([all])")

set(ASTRA_SIM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include("${ASTRA_SIM_ROOT}/cmake/AstraSimLibrary.cmake")

set(NETWORK_BACKEND_BUILD_AS_LIBRARY ON)
add_subdirectory("${ASTRA_SIM_ROOT}/extern/network_backend/analytical" Analytical)
add_subdirectory("${ASTRA_SIM_ROOT}/astra-sim/network_frontend/htsim" AstraSim_HTSim)
