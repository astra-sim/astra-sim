# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

# CMake requirement
cmake_minimum_required(VERSION 3.19)

# C++ requirement
set(CMAKE_CXX_STANDARD 20)

# Setup project
project (AstraSim_Standalone CXX)

# Compile AstraSim library
add_subdirectory("${PROJECT_SOURCE_DIR}/../../" AstraSim)

