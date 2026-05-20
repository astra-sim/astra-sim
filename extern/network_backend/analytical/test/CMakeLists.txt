# CMake Requirement
cmake_minimum_required(VERSION 3.15)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup project
project(TestAnalytical)

# enable testing
enable_testing()

# Compilation target
set(BUILDTARGET "" CACHE STRING "Compilation target (congestion_unaware/congestion_aware)")
option(NETWORK_BACKEND_BUILD_AS_LIBRARY "Build as a library" ON)

# Compile Analytical Backend
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. analytical)

# Compile GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 1204d63 # Commit of 2024-10-31
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Compile Congestion Unaware Backend
if (BUILDTARGET STREQUAL "congestion_unaware")
    # compile test target
    add_executable(TestAnalyticalCongestionUnaware ${CMAKE_CURRENT_SOURCE_DIR}/test_congestion_unaware.cpp)
    target_link_libraries(TestAnalyticalCongestionUnaware PRIVATE Analytical_Congestion_Unaware)

    # link with gtest
    target_link_libraries(TestAnalyticalCongestionUnaware PRIVATE gtest_main)
    gtest_discover_tests(TestAnalyticalCongestionUnaware)

elseif (BUILDTARGET STREQUAL "congestion_aware")
    # compile test target
    add_executable(TestAnalyticalCongestionAware ${CMAKE_CURRENT_SOURCE_DIR}/test_congestion_aware.cpp)
    target_link_libraries(TestAnalyticalCongestionAware PRIVATE Analytical_Congestion_Aware)

    # link with gtest
    target_link_libraries(TestAnalyticalCongestionAware PRIVATE gtest_main)
    gtest_discover_tests(TestAnalyticalCongestionAware)
endif ()
