// --- Internal Includes ---
#include "custom_utilities/common.hpp"


namespace Kratos {


namespace {


std::unique_ptr<KratosApplication> MakeApplication()
{
    return std::make_unique<KratosApplication>("BenchmarkApplication");
}

std::unique_ptr<Model> MakeModel()
{
    auto p_model = std::make_unique<Model>();

    auto& r_model_part = p_model->CreateModelPart("main");
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
    return p_model;
}


} // unnamed namespace


Model& ModelFactory::GetModel()
{
    if (!ModelFactory::mpApplication) {
        ModelFactory::mpApplication = MakeApplication();
        ModelFactory::mpApplication->RegisterKratosCore();
    }

    if (!ModelFactory::mpModel) {
        ModelFactory::mpModel = MakeModel();
    }

    return *ModelFactory::mpModel;
}


std::unique_ptr<KratosApplication> ModelFactory::mpApplication;


std::unique_ptr<Model> ModelFactory::mpModel;


} // namespace Kratos
