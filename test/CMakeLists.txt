add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../extern/googletest" "extern/googletest")

file(GLOB astra_test_SRC "*.cc")

add_executable(AstraTest "${astra_test_SRC}")
target_link_libraries(AstraTest gtest gmock gtest_main AstraSim)
gtest_discover_tests(
        AstraTest
        WORKING_DIRECTORY ${PROJECT_DIR}
)
