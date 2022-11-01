// --- External Includes ---
#include "benchmark/benchmark.h"

// --- Internal Includes ---
#include "custom_utilities/common.hpp"


void BruteForcePointLocator(benchmark::State& rState)
{
    rState.PauseTiming();
    Kratos::Model& r_model = Kratos::ModelFactory::GetModel();
    Kratos::ModelPart& r_model_part = r_model.GetModelPart("main");
    Kratos::BruteForcePointLocator locator(r_model_part);

    const std::size_t number_of_nodes = r_model_part.NumberOfNodes();
    std::cout << "number of nodes: " << number_of_nodes << std::endl;
    std::array<std::size_t, 5> node_indices;
    for (unsigned index=0; index<node_indices.size(); ++index) {
        node_indices[index] = double(index + 1) / double(node_indices.size() + 1) * (number_of_nodes - 1);
    }
    rState.ResumeTiming();

    for (auto dummy : rState) {
        for (auto id_node : node_indices) {
            int tmp = locator.FindNode(r_model_part.GetNode(id_node));
            benchmark::DoNotOptimize(tmp);
        }
    }
}


BENCHMARK(BruteForcePointLocator);


BENCHMARK_MAIN();
