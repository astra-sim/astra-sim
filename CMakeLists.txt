file(GLOB astra_SRC 
	"${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/*.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/workload/*.cc")
file(GLOB astra_test_SRC "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cc")

add_library(AstraSim ${astra_SRC})
target_include_directories(AstraSim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(TARGET AstraSim PROPERTY CXX_STANDARD 11)

enable_testing()
include(GoogleTest)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
