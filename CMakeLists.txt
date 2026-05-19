# # *****************************************************************************
# # This source code is licensed under the MIT license found in the
# # LICENSE file in the root directory of this source tree.
# #
# # Copyright (c) 2024 Georgia Institute of Technology
# # *****************************************************************************

# Standalone build of the AstraSim library target (optional entry point).
cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

project(AstraSim)

set(ASTRA_SIM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(ASTRA_SIM_STANDALONE_BUILD ON)
include("${ASTRA_SIM_ROOT}/cmake/AstraSimLibrary.cmake")
