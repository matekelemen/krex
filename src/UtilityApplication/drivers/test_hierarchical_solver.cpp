/// @author Máté Kelemen

// --- Core Includes ---
#include "containers/model.h"
#include "includes/model_part.h"
#include "includes/kratos_application.h"
#include "geometries/line_2d_3.h"
#include "linear_solvers/poly_hierarchical_solver.h"
#include "spaces/ublas_space.h"

// --- StructuralMechanics Includes ---
#include "structural_mechanics_application.h"

// --- STL Includes ---
#include <vector>
#include <memory>


using SparseSpace = Kratos::TUblasSparseSpace<double>;


using DenseSpace = Kratos::TUblasDenseSpace<double>;


class Context
{
public:
    Context()
    {
        mApplications.emplace_back(new Kratos::KratosApplication("Core"));
        mApplications.emplace_back(new Kratos::KratosStructuralMechanicsApplication);
        for (auto& rp_application : mApplications) {
            rp_application->Register();
        }
    }

private:
    std::vector<std::unique_ptr<Kratos::KratosApplication>> mApplications;
}; // class Context


namespace Kratos {


std::unique_ptr<Model> MakeModel()
{
    auto p_model = std::make_unique<Model>();
    ModelPart& r_model_part = p_model->CreateModelPart("root");
    r_model_part.AddNodalSolutionStepVariable(DISPLACEMENT_X);
    return p_model;
}


void MakeDofs(ModelPart& rModelPart,
              ModelPart::DofsArrayType& rDofs)
{
    rDofs.clear();
    std::size_t equation_id = 0ul;
    for (auto& r_node : rModelPart.Nodes()) {
        r_node.AddDof(DISPLACEMENT_X).SetEquationId(equation_id++);
        for (auto& rp_dof : r_node.GetDofs()) {
            rDofs.insert(rDofs.begin(), rp_dof.get());
        }
    }
}


/** @brief Two elements connected with one MPC.
 *  @details This test models two disconnected quadratic 1D elements that
 *           are constrained at one of their vertices by a single MPC.
 *           @code
 *           +-----+-----+ (master) <- MPC -> (slave) +-----+-----+   <==
 *           0     2     1                            3     5     4    F
 *           | element 0 |                            | element 1 |
 *           @endcode
 */
void OneMasterOneSlave(std::unique_ptr<Model>& rpModel,
                       ModelPart::DofsArrayType& rDofs,
                       SparseSpace::MatrixType& rStiffness,
                       SparseSpace::VectorType& rRHS)
{
    // Construct nodes
    ModelPart& r_model_part = rpModel->GetModelPart("root");
    r_model_part.CreateNewNode(1, 0.0, 0.0, 0.0);
    auto p_master_node = r_model_part.CreateNewNode(2, 1.0, 0.0, 0.0);
    r_model_part.CreateNewNode(3, 0.5, 0.0, 0.0);
    auto p_slave_node = r_model_part.CreateNewNode(4, 1.0, 0.0, 0.0);
    r_model_part.CreateNewNode(5, 2.0, 0.0, 0.0);
    r_model_part.CreateNewNode(6, 1.5, 0.0, 0.0);

    // Construct elements
    auto p_properties = Properties::Pointer(new Properties);
    r_model_part.CreateNewElement("SmallDisplacementTrussElement2D3N",
                                  1,
                                  {1, 2, 3},
                                  p_properties);
    r_model_part.CreateNewElement("SmallDisplacementTrussElement2D3N",
                                  2,
                                  {4, 5, 6},
                                  p_properties);

    // Construct DoFs and MPCs
    MakeDofs(r_model_part, rDofs);
    r_model_part.CreateNewMasterSlaveConstraint("LinearMasterSlaveConstraint",
                                                1,
                                                *p_master_node,
                                                DISPLACEMENT_X,
                                                *p_slave_node,
                                                DISPLACEMENT_X,
                                                1.0,
                                                0.0);

    // Set stiffness matrix
    rStiffness.clear();
    rStiffness.resize(6, 6);
    rStiffness.insert_element(0, 0,  1.1666666666666667);
    rStiffness.insert_element(0, 1,  0.16666666666666666);
    rStiffness.insert_element(0, 2, -1.3333333333333333);
    rStiffness.insert_element(1, 0,  0.16666666666666666);
    rStiffness.insert_element(1, 1,  2.3333333333333335);
    rStiffness.insert_element(1, 2, -1.3333333333333333);
    rStiffness.insert_element(1, 4,  0.16666666666666666);
    rStiffness.insert_element(1, 5, -1.3333333333333333);
    rStiffness.insert_element(2, 0, -1.3333333333333333);
    rStiffness.insert_element(2, 1, -1.3333333333333333);
    rStiffness.insert_element(2, 2,  2.6666666666666665);
    rStiffness.insert_element(4, 1,  0.16666666666666666);
    rStiffness.insert_element(4, 4,  1.1666666666666667);
    rStiffness.insert_element(4, 5, -1.3333333333333333);
    rStiffness.insert_element(5, 1, -1.3333333333333333);
    rStiffness.insert_element(5, 4, -1.3333333333333333);
    rStiffness.insert_element(5, 5,  2.6666666666666665);

    // Set RHS
    rRHS.resize(6);
    SparseSpace::SetToZero(rRHS);
    rRHS[3] = -1.0;
}


void Solve(Model& rModel,
           ModelPart::DofsArrayType& rDofs,
           SparseSpace::MatrixType& rStiffness,
           SparseSpace::VectorType& rRHS)
{
    PolyHierarchicalSolver<SparseSpace,DenseSpace> solver(Parameters(R"({
        "solver_type" : "poly_hierarchical",
        "verbosity" : 4,
        "max_iterations" : 100,
        "tolerance" : 1e-6,
        "coarse_settings" : {
            "solver_type" : "cg",
            "tolerance" : 2e-1,
            "max_iteration" : 50
        },
        "fine_settings" : {
            "solver_type" : "gauss_seidel",
            "max_iterations" : 3
        }
    })"));

    ModelPart& r_model_part = rModel.GetModelPart("root");
    SparseSpace::VectorType solution;
    solution.resize(rStiffness.size1());
    solver.ProvideAdditionalData(rStiffness,
                                 solution,
                                 rRHS,
                                 rDofs,
                                 r_model_part);

    solver.Solve(rStiffness, solution, rRHS);
    KRATOS_INFO("OneMasterOneSlave") << "solution: " << solution << "\n";
}


} // namespace Kratos


int main(int argc, const char* argv[])
{
    Context context;
    std::unique_ptr<Kratos::Model> p_model = Kratos::MakeModel();
    Kratos::ModelPart::DofsArrayType dofs;
    SparseSpace::MatrixType stiffness;
    SparseSpace::VectorType rhs;

    Kratos::OneMasterOneSlave(p_model, dofs, stiffness, rhs);
    Kratos::Solve(*p_model, dofs, stiffness, rhs);

    return 0;
}
