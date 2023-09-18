# CMake Requirement
cmake_minimum_required(VERSION 3.15)

# C++ requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup project
project(AstraSim)

# Find Protobuf
find_package(Protobuf REQUIRED)

# Files to compile
file(GLOB AstraSimSrcFiles
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/workload/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/collective/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/topology/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/memory/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/scheduling/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/third_party/utils/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/et_def/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/et_feeder/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/memory_backend/analytical/*.cc")

# Compile AstraSim Library
add_library(AstraSim ${AstraSimSrcFiles})
target_include_directories(AstraSim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(AstraSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/)
target_include_directories(AstraSim PUBLIC ${Protobuf_INCLUDE_DIRS})
target_link_libraries(AstraSim PUBLIC ${Protobuf_LIBRARIES})
