# Build protobuf 5.x with bundled Abseil (module provider) inside the project tree.
# No PROTOBUF_FROM_SOURCE, absl_DIR, or system protobuf install required.
include(FetchContent)

set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
set(protobuf_ABSL_PROVIDER "module" CACHE STRING "" FORCE)

FetchContent_Declare(
    protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG v29.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(protobuf)

add_library(AstraSimProtobufDeps INTERFACE)
target_link_libraries(AstraSimProtobufDeps INTERFACE protobuf::libprotobuf)

# Generate Chakra et_def C++ sources with the bundled protoc.
set(ASTRA_CHAKRA_PROTO_DIR "${CMAKE_CURRENT_LIST_DIR}/../extern/graph_frontend/chakra/schema/protobuf")
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
