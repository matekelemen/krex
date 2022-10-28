project(BenchmarkApplication CXX)

find_package(benchmark)

include_directories("${KRATOS_SOURCE_DIR}/kratos")

add_executable(distance_benchmark drivers/distance_benchmark)
target_link_libraries(distance_benchmark PUBLIC
                      KratosCore
                      benchmark::benchmark)
