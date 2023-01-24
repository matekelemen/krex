project(BenchmarkApplication CXX)

find_package(benchmark)

include_directories("${KRATOS_SOURCE_DIR}/kratos" "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB library_sources custom_utilities/*.cpp)
add_library(BenchmarkCore ${library_sources})

add_executable(distance_benchmark drivers/distance_benchmark)
target_link_libraries(distance_benchmark PUBLIC
                      KratosCore
                      BenchmarkCore
                      benchmark::benchmark)

add_executable(profiler_benchmark drivers/profiler_benchmark)
target_link_libraries(profiler_benchmark PUBLIC
                      KratosCore
                      BenchmarkCore
                      benchmark::benchmark)
