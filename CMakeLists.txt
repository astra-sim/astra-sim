set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/local/include")

file(GLOB astra_SRC
	"${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/*.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/workload/*.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/collective/*.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/topology/*.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/memory/*.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/astra-sim/system/scheduling/*.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/third_party/utils/*.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/et_def/*.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/et_feeder/*.cpp")

add_library(AstraSim ${astra_SRC})
target_include_directories(AstraSim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/extern/graph_frontend/chakra/)
set_property(TARGET AstraSim PROPERTY CXX_STANDARD 11)
