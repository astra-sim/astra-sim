# Build the AstraSim static library and protobuf dependencies.
#
# Required variable before include:
#   ASTRA_SIM_ROOT  - absolute path to the astra-sim repository root
#
# Optional variables:
#   ASTRA_SIM_USE_BUNDLED_PROTOBUF  - default ON
#   ASTRA_SIM_STANDALONE_BUILD       - set ON to apply bin/lib output directories
#
# Provides targets: AstraSim, and when bundled, AstraSimProtobufDeps, astra_chakra_proto_gen

if(NOT ASTRA_SIM_ROOT)
    message(FATAL_ERROR "ASTRA_SIM_ROOT must be set before including AstraSimLibrary.cmake")
endif()

if(TARGET AstraSim)
    return()
endif()

if(NOT DEFINED ASTRA_SIM_USE_BUNDLED_PROTOBUF)
    option(ASTRA_SIM_USE_BUNDLED_PROTOBUF "Build protobuf/Abseil from extern/helper submodules" ON)
endif()

# fmt + spdlog (out-of-tree sources need an explicit binary dir)
add_subdirectory("${ASTRA_SIM_ROOT}/extern/helper/fmt" "${CMAKE_BINARY_DIR}/_deps/fmt")
option(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory("${ASTRA_SIM_ROOT}/extern/helper/spdlog" "${CMAKE_BINARY_DIR}/_deps/spdlog")

if(ASTRA_SIM_USE_BUNDLED_PROTOBUF)
    include("${ASTRA_SIM_ROOT}/cmake/AstraBundledProtobuf.cmake")
else()
    find_package(Protobuf REQUIRED)
endif()

file(GLOB _astra_sim_srcs
    "${ASTRA_SIM_ROOT}/astra-sim/system/*.cc"
    "${ASTRA_SIM_ROOT}/astra-sim/workload/*.cc"
    "${ASTRA_SIM_ROOT}/astra-sim/system/astraccl/*.cc"
    "${ASTRA_SIM_ROOT}/astra-sim/system/astraccl/native_collectives/*.cc"
    "${ASTRA_SIM_ROOT}/astra-sim/system/astraccl/native_collectives/logical_topology/*.cc"
    "${ASTRA_SIM_ROOT}/astra-sim/system/astraccl/native_collectives/collective_algorithm/*.cc"
    "${ASTRA_SIM_ROOT}/astra-sim/system/astraccl/custom_collectives/*.cc"
    "${ASTRA_SIM_ROOT}/astra-sim/system/memory/*.cc"
    "${ASTRA_SIM_ROOT}/astra-sim/system/scheduling/*.cc"
    "${ASTRA_SIM_ROOT}/astra-sim/common/*.cc"
    "${ASTRA_SIM_ROOT}/extern/graph_frontend/chakra/src/third_party/utils/*.cc"
    "${ASTRA_SIM_ROOT}/extern/graph_frontend/chakra/src/feeder_v3/*.cpp"
    "${ASTRA_SIM_ROOT}/extern/remote_memory_backend/analytical/*.cc")

if(ASTRA_SIM_USE_BUNDLED_PROTOBUF)
    list(APPEND _astra_sim_srcs "${ASTRA_CHAKRA_PROTO_SRC}")
else()
    file(GLOB _chakra_proto_srcs "${ASTRA_SIM_ROOT}/extern/graph_frontend/chakra/schema/protobuf/*.cc")
    list(APPEND _astra_sim_srcs ${_chakra_proto_srcs})
endif()

add_library(AstraSim STATIC ${_astra_sim_srcs})

if(ASTRA_SIM_USE_BUNDLED_PROTOBUF)
    add_dependencies(AstraSim astra_chakra_proto_gen)
endif()

target_link_libraries(AstraSim PUBLIC fmt::fmt spdlog::spdlog)

if(ASTRA_SIM_USE_BUNDLED_PROTOBUF)
    target_link_libraries(AstraSim PUBLIC AstraSimProtobufDeps)
else()
    target_link_libraries(AstraSim PUBLIC ${Protobuf_LIBRARIES})
endif()

target_include_directories(AstraSim PUBLIC "${ASTRA_SIM_ROOT}")
target_include_directories(AstraSim PUBLIC "${ASTRA_SIM_ROOT}/extern/graph_frontend/chakra")
target_include_directories(AstraSim PUBLIC "${ASTRA_SIM_ROOT}/extern/graph_frontend/chakra/src/third_party/utils")
target_include_directories(AstraSim PRIVATE "${ASTRA_SIM_ROOT}/extern/helper")

if(ASTRA_SIM_USE_BUNDLED_PROTOBUF)
    target_include_directories(AstraSim BEFORE PUBLIC "${ASTRA_CHAKRA_PROTO_OUT_DIR}")
else()
    target_include_directories(AstraSim PUBLIC "${ASTRA_SIM_ROOT}/extern/graph_frontend/chakra/schema/protobuf")
    target_include_directories(AstraSim PUBLIC ${Protobuf_INCLUDE_DIR})
endif()

set_target_properties(AstraSim PROPERTIES
    COMPILE_WARNING_AS_ERROR OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

if(ASTRA_SIM_STANDALONE_BUILD)
    set_target_properties(AstraSim
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/
    )
endif()
