# Protobuf 5.x (built from source) depends on Abseil. When libprotobuf is static,
# Abseil symbols may not reach the final executable unless they are linked explicitly.
if(DEFINED ENV{PROTOBUF_FROM_SOURCE} AND "$ENV{PROTOBUF_FROM_SOURCE}" STREQUAL "True")
    find_package(protobuf CONFIG REQUIRED)
    find_package(absl CONFIG QUIET)

    add_library(AstraSimProtobufDeps INTERFACE)
    target_link_libraries(AstraSimProtobufDeps INTERFACE protobuf::libprotobuf)

    if(absl_FOUND)
        set(_astra_absl_targets
            absl::absl_check
            absl::absl_log
            absl::log
            absl::log_internal_check_op
            absl::log_internal_message
            absl::strings
            absl::str_format
            absl::cord
            absl::hash
            absl::status
            absl::statusor
            absl::time
            absl::base
            absl::synchronization
            absl::raw_hash_set
            absl::throw_delegate
        )
        foreach(_target IN LISTS _astra_absl_targets)
            if(TARGET ${_target})
                target_link_libraries(AstraSimProtobufDeps INTERFACE ${_target})
            endif()
        endforeach()
    endif()

    set(ASTRA_SIM_PROTOBUF_FROM_SOURCE TRUE)
endif()
