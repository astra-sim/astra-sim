# CMake Requirement
cmake_minimum_required(VERSION 3.15)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the build type to Release if not specified
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# Setup project
project(AstraSim_Analytical)

# Compilation target
set(BUILDTARGET "all" CACHE STRING "Compilation target ([all]/congestion_unaware/congestion_aware)")

# Include src files to compile
file(GLOB srcs_common
        ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cc
)

file(GLOB srcs_congestion_unaware
        ${CMAKE_CURRENT_SOURCE_DIR}/congestion_unaware/*.cc
)

file(GLOB srcs_congestion_aware
        ${CMAKE_CURRENT_SOURCE_DIR}/congestion_aware/*.cc
)

# Compile Congestion Unaware Backend
if (BUILDTARGET STREQUAL "all" OR BUILDTARGET STREQUAL "congestion_unaware")
    add_executable(AstraSim_Analytical_Congestion_Unaware ${srcs_congestion_unaware} ${srcs_common})
    target_sources(AstraSim_Analytical_Congestion_Unaware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/congestion_unaware/main.cc)

    # Link libraries
    target_link_libraries(AstraSim_Analytical_Congestion_Unaware LINK_PRIVATE AstraSim)
    target_link_libraries(AstraSim_Analytical_Congestion_Unaware LINK_PRIVATE Analytical_Congestion_Unaware)

    # Include directories
    target_include_directories(AstraSim_Analytical_Congestion_Unaware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/)
    target_include_directories(AstraSim_Analytical_Congestion_Unaware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/)
    target_include_directories(AstraSim_Analytical_Congestion_Unaware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/helper)

    # Properties
    set_target_properties(AstraSim_Analytical_Congestion_Unaware PROPERTIES COMPILE_WARNING_AS_ERROR ON)
    set_target_properties(AstraSim_Analytical_Congestion_Unaware
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../bin/
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../lib/
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../lib/
    )
endif ()

# Compile Congestion Aware Backend
if (BUILDTARGET STREQUAL "all" OR BUILDTARGET STREQUAL "congestion_aware")
    add_executable(AstraSim_Analytical_Congestion_Aware ${srcs_congestion_aware} ${srcs_common})
    target_sources(AstraSim_Analytical_Congestion_Aware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/congestion_aware/main.cc)

    # Link libraries
    target_link_libraries(AstraSim_Analytical_Congestion_Aware LINK_PRIVATE AstraSim)
    target_link_libraries(AstraSim_Analytical_Congestion_Aware LINK_PRIVATE Analytical_Congestion_Aware)

    # Include directories
    target_include_directories(AstraSim_Analytical_Congestion_Aware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/)
    target_include_directories(AstraSim_Analytical_Congestion_Aware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/)
    target_include_directories(AstraSim_Analytical_Congestion_Aware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/helper)

    # Properties
    set_target_properties(AstraSim_Analytical_Congestion_Aware PROPERTIES COMPILE_WARNING_AS_ERROR ON)
    set_target_properties(AstraSim_Analytical_Congestion_Aware
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../bin/
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../lib/
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../lib/
    )
endif ()
