# CMake Requirement
cmake_minimum_required(VERSION 3.15)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup project
project(AstraSim_Analytical_Congestion_Unaware)

# Include src files to compile
file(GLOB srcs
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hh"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

# Compile Congestion Aware Backend
add_executable(AstraSim_Analytical_Congestion_Unaware ${srcs})
target_sources(AstraSim_Analytical_Congestion_Unaware PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/main.cc")

# Link libraries
target_link_libraries(AstraSim_Analytical_Congestion_Unaware LINK_PUBLIC AstraSim)
target_link_libraries(AstraSim_Analytical_Congestion_Unaware LINK_PUBLIC Analytical_Congestion_Unaware)

# Properties
target_include_directories(AstraSim_Analytical_Congestion_Unaware PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(AstraSim_Analytical_Congestion_Unaware PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../")
target_include_directories(AstraSim_Analytical_Congestion_Unaware PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../extern/network_backend/analytical/congestion_unaware")
set_target_properties(AstraSim_Analytical_Congestion_Unaware
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../bin/"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../lib/"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../lib/"
)
