// --- External Includes ---
#include "benchmark/benchmark.h"

// --- Core Includes ---
#include "containers/model.h"
#include "includes/global_variables.h"
#include "utilities/brute_force_point_locator.h"
#include "geometries/quadrilateral_2d_4.h"
#include "processes/structured_mesh_generator_process.h"
#include "includes/kratos_application.h"

// --- STL Includes ---
#include <memory>


namespace Kratos {


class ModelFactory
{
public:
    static Model& GetModel()
    {
        if (!ModelFactory::mpApplication) {
            ModelFactory::MakeApplication();
            ModelFactory::mpApplication->RegisterKratosCore();
        }

        if (!ModelFactory::mpModel) {
            ModelFactory::MakeModel();
        }

        return *mpModel;
    }

private:
    static void MakeApplication()
    {
        ModelFactory::mpApplication = std::make_unique<KratosApplication>("BenchmarkApplication");
    }

    static void MakeModel()
    {
        ModelFactory::mpModel = std::make_unique<Model>();
        auto& rp_model = ModelFactory::mpModel;

        auto& r_model_part = rp_model->CreateModelPart("main");
        r_model_part.SetBufferSize(1);
        r_model_part.AddNodalSolutionStepVariable(DISPLACEMENT);

        using NodePtr = decltype(std::declval<ModelPart>().CreateNewNode(1, 0.0, 0.0, 0.0));
        std::array<NodePtr, 4> nodes {
            NodePtr(new Node<3>(1, 0.0, 0.0, 0.0)),
            NodePtr(new Node<3>(2, 0.0, 1.0, 0.0)),
            NodePtr(new Node<3>(3, 1.0, 1.0, 0.0)),
            NodePtr(new Node<3>(4, 1.0, 0.0, 0.0))
        };
        Quadrilateral2D4<Node<3>> geometry(nodes[0], nodes[1], nodes[2], nodes[3]);

        Parameters parameters(R"({
            "number_of_divisions" : 500,
            "element_name" : "Element2D3N"
        })");


        StructuredMeshGeneratorProcess(geometry, r_model_part, parameters).Execute();
    }

private:
    static std::unique_ptr<KratosApplication> mpApplication;

    static std::unique_ptr<Model> mpModel;
}; // class ModelFactory


std::unique_ptr<KratosApplication> ModelFactory::mpApplication;


std::unique_ptr<Model> ModelFactory::mpModel;


} // namespace Kratos


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
