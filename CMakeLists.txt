cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)

file(GLOB astra_SRC "astra-sim/**/*.cc")

add_library(AstraSim STATIC ${astra_SRC})

target_include_directories(AstraSim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
