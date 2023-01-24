// --- External Includes ---
#include "benchmark/benchmark.h"

// --- Core Includes ---
#if __has_include("utilities/profiler.h")
#include "utilities/profiler.h"

// --- STL Includes ---
#include <numeric>
#include <vector>


constexpr std::size_t ARRAY_SIZE = 1e6;


constexpr std::size_t JOB_SIZE = 1e3;


void InitializeArray(int* begin, const int* end)
{
    int i = 0;
    for (; begin != end; ++begin) {
        *begin++ = i % 2 ? i++ : -(i++);
    }
}


void WithoutProfiler(benchmark::State& rState)
{
    std::vector<int> array(ARRAY_SIZE);
    InitializeArray(array.data(), array.data() + array.size());

    const auto job = [&array]() {
        int sum = 0;
        for (int value : array) {
            sum += value;
        }
        benchmark::DoNotOptimize(sum);
    };

    for (auto dummy : rState) {
        for (std::size_t i=0; i<JOB_SIZE; ++i) {
            job();
        }
    }
}


void WithProfiler(benchmark::State& rState)
{
    std::vector<int> array(ARRAY_SIZE);
    InitializeArray(array.data(), array.data() + array.size());

    const auto job = [&array]() {
        KRATOS_PROFILE_SCOPE(KRATOS_CODE_LOCATION);
        int sum = 0;
        for (int value : array) {
            sum += value;
        }
        benchmark::DoNotOptimize(sum);
    };

    for (auto dummy : rState) {
        for (std::size_t i=0; i<JOB_SIZE; ++i) {
            job();
        }
    }
}


BENCHMARK(WithoutProfiler);

BENCHMARK(WithProfiler);

#endif

BENCHMARK_MAIN();
