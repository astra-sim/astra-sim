cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

file(GLOB astra_SRC "astra-sim/**/*.cc")
file(GLOB astra_test_SRC "test/*.cc")

add_library(AstraSim STATIC ${astra_SRC})
target_include_directories(AstraSim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(TARGET AstraSim PROPERTY CXX_STANDARD 11)

enable_testing()
include(GoogleTest)
add_subdirectory(test)
