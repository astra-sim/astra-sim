# CMake Requirement
cmake_minimum_required(VERSION 3.15)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup project
project(AstraSim_Analytical_Congestion_Aware)

# Include src files to compile
file(GLOB srcs
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hh"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

# Compile Congestion Aware Backend
add_executable(AstraSim_Analytical_Congestion_Aware ${srcs})
target_sources(AstraSim_Analytical_Congestion_Aware PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/main.cc")

# Link libraries
target_link_libraries(AstraSim_Analytical_Congestion_Aware LINK_PUBLIC AstraSim)
target_link_libraries(AstraSim_Analytical_Congestion_Aware LINK_PUBLIC Analytical_Congestion_Aware)

# Properties
target_include_directories(AstraSim_Analytical_Congestion_Aware PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(AstraSim_Analytical_Congestion_Aware PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../astra-sim")
target_include_directories(AstraSim_Analytical_Congestion_Aware PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../extern/network_backend/analytical/congestion_aware")
set_target_properties(AstraSim_Analytical_Congestion_Aware
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../bin/"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../lib/"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../lib/"
)
