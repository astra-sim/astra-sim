# Build protobuf 5.x and Abseil from git submodules under extern/helper/.
# Initialize with:
#   git submodule update --init extern/helper/abseil-cpp extern/helper/protobuf
get_filename_component(ASTRA_SIM_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(_ASTRA_ABSL_DIR "${ASTRA_SIM_ROOT}/extern/helper/abseil-cpp")
set(_ASTRA_PROTOBUF_DIR "${ASTRA_SIM_ROOT}/extern/helper/protobuf")

if(NOT EXISTS "${_ASTRA_ABSL_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Missing Abseil submodule at extern/helper/abseil-cpp. "
        "Run: git submodule update --init extern/helper/abseil-cpp")
endif()
if(NOT EXISTS "${_ASTRA_PROTOBUF_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Missing protobuf submodule at extern/helper/protobuf. "
        "Run: git submodule update --init extern/helper/protobuf")
endif()

set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${_ASTRA_ABSL_DIR}" "${CMAKE_BINARY_DIR}/abseil-cpp" EXCLUDE_FROM_ALL)

set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
# absl::strings is already provided above; protobuf skips its bundled third_party/abseil.
add_subdirectory("${_ASTRA_PROTOBUF_DIR}" "${CMAKE_BINARY_DIR}/protobuf" EXCLUDE_FROM_ALL)

add_library(AstraSimProtobufDeps INTERFACE)
target_link_libraries(AstraSimProtobufDeps INTERFACE protobuf::libprotobuf)

# Generate Chakra et_def C++ sources with the bundled protoc.
set(ASTRA_CHAKRA_PROTO_DIR "${ASTRA_SIM_ROOT}/extern/graph_frontend/chakra/schema/protobuf")
set(ASTRA_CHAKRA_PROTO_OUT_DIR "${CMAKE_BINARY_DIR}/generated/chakra_proto")
file(MAKE_DIRECTORY "${ASTRA_CHAKRA_PROTO_OUT_DIR}")

set(ASTRA_CHAKRA_PROTO_HDR "${ASTRA_CHAKRA_PROTO_OUT_DIR}/et_def.pb.h")
set(ASTRA_CHAKRA_PROTO_SRC "${ASTRA_CHAKRA_PROTO_OUT_DIR}/et_def.pb.cc")

add_custom_command(
    OUTPUT "${ASTRA_CHAKRA_PROTO_HDR}" "${ASTRA_CHAKRA_PROTO_SRC}"
    COMMAND $<TARGET_FILE:protobuf::protoc>
    ARGS --cpp_out=${ASTRA_CHAKRA_PROTO_OUT_DIR}
         --proto_path=${ASTRA_CHAKRA_PROTO_DIR}
         ${ASTRA_CHAKRA_PROTO_DIR}/et_def.proto
    DEPENDS ${ASTRA_CHAKRA_PROTO_DIR}/et_def.proto
    COMMENT "Generating Chakra et_def protobuf sources"
    VERBATIM
)

add_custom_target(astra_chakra_proto_gen DEPENDS "${ASTRA_CHAKRA_PROTO_HDR}" "${ASTRA_CHAKRA_PROTO_SRC}")

set(ASTRA_SIM_BUNDLED_PROTOBUF TRUE)
