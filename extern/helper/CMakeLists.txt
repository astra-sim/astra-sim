# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

cmake_minimum_required(VERSION 3.19)

##
## JSON library
## name and version
##
project(nlohmann_json VERSION 3.10.5 LANGUAGES CXX)

##
## CONFIGURATION
##
set(NLOHMANN_JSON_TARGET_NAME               ${PROJECT_NAME})

message(STATUS "name ${NLOHMANN_JSON_TARGET_NAME}")

##
## TARGET
## create target and add include path
##
add_library(${NLOHMANN_JSON_TARGET_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${NLOHMANN_JSON_TARGET_NAME} ALIAS ${NLOHMANN_JSON_TARGET_NAME})
if (${CMAKE_VERSION} VERSION_LESS "3.8.0")
    target_compile_features(${NLOHMANN_JSON_TARGET_NAME} INTERFACE cxx_range_for)
else()
    target_compile_features(${NLOHMANN_JSON_TARGET_NAME} INTERFACE cxx_std_11)
endif()

set(NLOHMANN_JSON_INCLUDE_BUILD_DIR "${PROJECT_SOURCE_DIR}/")

target_include_directories(
    ${NLOHMANN_JSON_TARGET_NAME}
    INTERFACE
    $<BUILD_INTERFACE:${NLOHMANN_JSON_INCLUDE_BUILD_DIR}>
    $<INSTALL_INTERFACE:include>
)
