project (SimpleAnalytical)

set (CMAKE_CXX_STANDARD 17)

include_directories("${PROJECT_SOURCE_DIR}/../../")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

add_subdirectory ("${PROJECT_SOURCE_DIR}/../../" AtraSim)
add_subdirectory ("${PROJECT_SOURCE_DIR}/../../extern/network_backend/analytical" AnalyticalAstra)

