/// @author Máté Kelemen

// --- Core Includes ---
#if __has_include("custom_solvers/hierarchical_solver.h")
#include "custom_solvers/hierarchical_solver.h"

#include "containers/model.h"
#include "includes/model_part.h"
#include "includes/kratos_application.h"
#include "geometries/line_2d_3.h"
#include "custom_solvers/hierarchical_solver.h"
#include "linear_solvers/skyline_lu_factorization_solver.h"
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


constexpr double SLAVE_DIAGONAL_COMPONENT = 1e3;


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
 *           /
 *           /+-----+-----+  <==  (master) <- MPC -> (slave) +-----+-----+
 *           /0     2     1   F                              3     5     4
 *            | element 0 |                                  | element 1 |
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
    rStiffness.insert_element(0, 0,  1.0);
    rStiffness.insert_element(1, 0,  0.16666666666666666);
    rStiffness.insert_element(1, 1,  2.3333333333333335);
    rStiffness.insert_element(1, 2, -1.3333333333333333);
    rStiffness.insert_element(1, 4,  0.16666666666666666);
    rStiffness.insert_element(1, 5, -1.3333333333333333);
    rStiffness.insert_element(2, 0, -1.3333333333333333);
    rStiffness.insert_element(2, 1, -1.3333333333333333);
    rStiffness.insert_element(2, 2,  2.6666666666666665);
    rStiffness.insert_element(3, 3,  SLAVE_DIAGONAL_COMPONENT); // <== slave
    rStiffness.insert_element(4, 1,  0.16666666666666666);
    rStiffness.insert_element(4, 4,  1.1666666666666667);
    rStiffness.insert_element(4, 5, -1.3333333333333333);
    rStiffness.insert_element(5, 1, -1.3333333333333333);
    rStiffness.insert_element(5, 4, -1.3333333333333333);
    rStiffness.insert_element(5, 5,  2.6666666666666665);

    // Set RHS
    rRHS.resize(rStiffness.size1());
    SparseSpace::SetToZero(rRHS);
    for (std::size_t i_rhs=1ul; i_rhs<rRHS.size(); ++i_rhs) {
        rRHS[i_rhs] = -1e0;
    }
    //rRHS[1] = -1e0;
}


/** @brief Two elements connected with one MPC.
 *  @details This test models two disconnected quadratic 1D elements that
 *           are constrained at one of their vertices by a single MPC.
 *           @code
 *                       + 7   ---
 *                       |      |
 *                       + 8    | element 2
 *                       |      |
 *                       + 6   ---
 *
 *                       ^ (slave)
 *                       | MPC
 *                       v (master)
 *          /
 *          /+-----+-----+  <==  (master) <- MPC -> (slave) +-----+-----+
 *          /0     2     1   F                              3     5     4
 *           | element 0 |                                  | element 1 |
 *           @endcode
 */
void OneMasterTwoSlaves(std::unique_ptr<Model>& rpModel,
                        ModelPart::DofsArrayType& rDofs,
                        SparseSpace::MatrixType& rStiffness,
                        SparseSpace::VectorType& rRHS)
{
    // Construct nodes
    ModelPart& r_model_part = rpModel->GetModelPart("root");
    r_model_part.CreateNewNode(1, 0.0, 0.0, 0.0);
    auto p_master_node = r_model_part.CreateNewNode(2, 1.0, 0.0, 0.0);
    r_model_part.CreateNewNode(3, 0.5, 0.0, 0.0);
    auto p_slave_node_element_1 = r_model_part.CreateNewNode(4, 1.0, 0.0, 0.0);
    r_model_part.CreateNewNode(5, 2.0, 0.0, 0.0);
    r_model_part.CreateNewNode(6, 1.5, 0.0, 0.0);
    auto p_slave_node_element_2 = r_model_part.CreateNewNode(7, 2.0, 0.0, 0.0);
    r_model_part.CreateNewNode(8, 3.0, 0.0, 0.0);
    r_model_part.CreateNewNode(9, 2.5, 0.0, 0.0);

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
    r_model_part.CreateNewElement("SmallDisplacementTrussElement2D3N",
                                  3,
                                  {7, 8, 9},
                                  p_properties);

    // Construct DoFs and MPCs
    MakeDofs(r_model_part, rDofs);
    r_model_part.CreateNewMasterSlaveConstraint("LinearMasterSlaveConstraint",
                                                1,
                                                *p_master_node,
                                                DISPLACEMENT_X,
                                                *p_slave_node_element_1,
                                                DISPLACEMENT_X,
                                                1.0,
                                                0.0);
    r_model_part.CreateNewMasterSlaveConstraint("LinearMasterSlaveConstraint",
                                                2,
                                                *p_master_node,
                                                DISPLACEMENT_X,
                                                *p_slave_node_element_2,
                                                DISPLACEMENT_X,
                                                1.0,
                                                0.0);

    // Set stiffness matrix
    rStiffness.clear();
    rStiffness.resize(9, 9);
    rStiffness.insert_element(0, 0,  1.0);
    rStiffness.insert_element(1, 0,  0.16666666666666666);
    rStiffness.insert_element(1, 1,  3.5);
    rStiffness.insert_element(1, 2, -1.3333333333333333);
    rStiffness.insert_element(1, 4,  0.16666666666666666);
    rStiffness.insert_element(1, 5, -1.3333333333333333);
    rStiffness.insert_element(1, 7,  0.16666666666666666);
    rStiffness.insert_element(1, 8, -1.3333333333333333);
    rStiffness.insert_element(2, 0, -1.3333333333333333);
    rStiffness.insert_element(2, 1, -1.3333333333333333);
    rStiffness.insert_element(2, 2,  2.6666666666666665);
    rStiffness.insert_element(3, 3,  SLAVE_DIAGONAL_COMPONENT); // <== slave
    rStiffness.insert_element(4, 1,  0.16666666666666666);
    rStiffness.insert_element(4, 4,  1.1666666666666667);
    rStiffness.insert_element(4, 5, -1.3333333333333333);
    rStiffness.insert_element(5, 1, -1.3333333333333333);
    rStiffness.insert_element(5, 4, -1.3333333333333333);
    rStiffness.insert_element(5, 5,  2.6666666666666665);
    rStiffness.insert_element(6, 6,  SLAVE_DIAGONAL_COMPONENT); // <== slave
    rStiffness.insert_element(7, 1,  0.16666666666666666);
    rStiffness.insert_element(7, 7,  1.1666666666666667);
    rStiffness.insert_element(7, 8, -1.3333333333333333);
    rStiffness.insert_element(8, 1, -1.3333333333333333);
    rStiffness.insert_element(8, 7, -1.3333333333333333);
    rStiffness.insert_element(8, 8,  2.6666666666666665);

    // Set RHS
    rRHS.resize(rStiffness.size1());
    SparseSpace::SetToZero(rRHS);
    for (std::size_t i_rhs=1ul; i_rhs<rRHS.size(); ++i_rhs) {
        rRHS[i_rhs] = -1e0;
    }
    //rRHS[1] = -1e0;
}


/** @brief Two elements connected with one MPC.
 *  @details This test models two disconnected quadratic 1D elements that
 *           are constrained at one of their vertices by a single MPC.
 *           @code
 *           @endcode
 */
void Triangles(std::unique_ptr<Model>& rpModel,
               ModelPart::DofsArrayType& rDofs,
               SparseSpace::MatrixType& rStiffness,
               SparseSpace::VectorType& rRHS)
{
    // Construct nodes
    ModelPart& r_model_part = rpModel->GetModelPart("root");
    std::vector<Node::Pointer> nodes {
        r_model_part.CreateNewNode( 1, -1.0,  0.0,  0.0),
        r_model_part.CreateNewNode( 2,  0.0,  0.0,  0.0),
        r_model_part.CreateNewNode( 3,  0.0,  1.0,  0.0),
        r_model_part.CreateNewNode( 4,  0.0,  0.0,  0.0),
        r_model_part.CreateNewNode( 5,  1.0,  0.0,  0.0),
        r_model_part.CreateNewNode( 6,  0.0,  1.0,  0.0),
        r_model_part.CreateNewNode( 7, -0.5,  0.0,  0.0),
        r_model_part.CreateNewNode( 8,  0.0,  0.5,  0.0),
        r_model_part.CreateNewNode( 9, -0.5,  0.5,  0.0),
        r_model_part.CreateNewNode(10,  0.5,  0.0,  0.0),
        r_model_part.CreateNewNode(11,  0.5,  0.5,  0.0),
        r_model_part.CreateNewNode(12,  0.0,  0.5,  0.0)
    };

    // Construct elements
    auto p_properties = Properties::Pointer(new Properties);
    r_model_part.CreateNewElement("SmallDisplacementElement2D6N",
                                  1,
                                  {1, 2, 3, 4, 5, 6},
                                  p_properties);
    r_model_part.CreateNewElement("SmallDisplacementElement2D6N",
                                  2,
                                  {7, 8, 9, 10, 11, 12},
                                  p_properties);

    // Construct DoFs and MPCs
    std::size_t equation_id = 0ul;
    for (auto& r_node : r_model_part.Nodes()) {
        const auto id = r_node.Id();
        if (id != 4ul && id != 6ul && id != 12ul) {
            r_node.AddDof(DISPLACEMENT_X).SetEquationId(equation_id++);
            r_node.AddDof(DISPLACEMENT_Y).SetEquationId(equation_id++);
            for (auto& rp_dof : r_node.GetDofs()) {
                rDofs.insert(rDofs.begin(), rp_dof.get());
            }
        } else {
            r_node.AddDof(DISPLACEMENT_X);
            r_node.AddDof(DISPLACEMENT_Y);
        }
    }

    std::size_t i_condition = 0ul;
    for (auto [i_master_node, i_slave_node] : {std::make_pair( 1ul,  3ul),
                                               std::make_pair( 2ul,  5ul),
                                               std::make_pair( 7ul, 11ul)}) {
        r_model_part.CreateNewMasterSlaveConstraint("LinearMasterSlaveConstraint",
                                                    ++i_condition,
                                                    *nodes[i_master_node],
                                                    DISPLACEMENT_X,
                                                    *nodes[i_slave_node],
                                                    DISPLACEMENT_X,
                                                    1.0,
                                                    0.0);
        r_model_part.CreateNewMasterSlaveConstraint("LinearMasterSlaveConstraint",
                                                    ++i_condition,
                                                    *nodes[i_master_node],
                                                    DISPLACEMENT_Y,
                                                    *nodes[i_slave_node],
                                                    DISPLACEMENT_Y,
                                                    1.0,
                                                    0.0);
    }

    // Set stiffness matrix
    rStiffness.clear();
    rStiffness.resize(18, 18);
    rStiffness.insert_element(1 - 1, 1 - 1,     5.494505494505000e-01);
    rStiffness.insert_element(2 - 1, 2 - 1,     1.923076923077000e-01);
    rStiffness.insert_element(3 - 1, 3 - 1,     1.483516483516000e+00);
    rStiffness.insert_element(3 - 1, 4 - 1,     5.551115123126000e-17);
    rStiffness.insert_element(3 - 1, 5 - 1,     1.282051282051000e-01);
    rStiffness.insert_element(3 - 1, 6 - 1,     -1.387778780781000e-17);
    rStiffness.insert_element(3 - 1, 9 - 1,     -7.326007326007000e-01);
    rStiffness.insert_element(3 - 1, 10 - 1,    2.564102564103000e-01);
    rStiffness.insert_element(3 - 1, 11 - 1,    -5.128205128205000e-01);
    rStiffness.insert_element(3 - 1, 12 - 1,    1.110223024625000e-16);
    rStiffness.insert_element(3 - 1, 13 - 1,    5.551115123126000e-17);
    rStiffness.insert_element(3 - 1, 14 - 1,    -4.163336342344000e-17);
    rStiffness.insert_element(3 - 1, 15 - 1,    -7.326007326007000e-01);
    rStiffness.insert_element(3 - 1, 16 - 1,    -2.564102564103000e-01);
    rStiffness.insert_element(3 - 1, 17 - 1,    -2.775557561563000e-16);
    rStiffness.insert_element(3 - 1, 18 - 1,    -1.249000902703000e-16);
    rStiffness.insert_element(4 - 1, 3 - 1,     5.551115123126000e-17);
    rStiffness.insert_element(4 - 1, 4 - 1,     1.483516483516000e+00);
    rStiffness.insert_element(4 - 1, 5 - 1,     -2.775557561563000e-17);
    rStiffness.insert_element(4 - 1, 6 - 1,     3.663003663004000e-01);
    rStiffness.insert_element(4 - 1, 9 - 1,     2.197802197802000e-01);
    rStiffness.insert_element(4 - 1, 10 - 1,    -2.564102564103000e-01);
    rStiffness.insert_element(4 - 1, 11 - 1,    5.551115123126000e-17);
    rStiffness.insert_element(4 - 1, 12 - 1,    -1.465201465201000e+00);
    rStiffness.insert_element(4 - 1, 13 - 1,    -2.775557561563000e-17);
    rStiffness.insert_element(4 - 1, 14 - 1,    8.326672684688999e-17);
    rStiffness.insert_element(4 - 1, 15 - 1,    -2.197802197802000e-01);
    rStiffness.insert_element(4 - 1, 16 - 1,    -2.564102564103000e-01);
    rStiffness.insert_element(4 - 1, 17 - 1,    -1.110223024625000e-16);
    rStiffness.insert_element(4 - 1, 18 - 1,    -2.775557561563000e-16);
    rStiffness.insert_element(5 - 1, 3 - 1,     1.282051282051000e-01);
    rStiffness.insert_element(5 - 1, 4 - 1,     -2.775557561563000e-17);
    rStiffness.insert_element(5 - 1, 5 - 1,     3.846153846154000e-01);
    rStiffness.insert_element(5 - 1, 10 - 1,    1.476738959037000e-17);
    rStiffness.insert_element(5 - 1, 11 - 1,    -5.128205128205000e-01);
    rStiffness.insert_element(5 - 1, 13 - 1,    1.387778780781000e-17);
    rStiffness.insert_element(5 - 1, 14 - 1,    -2.564102564103000e-01);
    rStiffness.insert_element(5 - 1, 15 - 1,    1.387778780781000e-17);
    rStiffness.insert_element(5 - 1, 16 - 1,    7.828495686458999e-18);
    rStiffness.insert_element(5 - 1, 17 - 1,    -1.387778780781000e-17);
    rStiffness.insert_element(5 - 1, 18 - 1,    2.564102564103000e-01);
    rStiffness.insert_element(6 - 1, 3 - 1,     -1.387778780781000e-17);
    rStiffness.insert_element(6 - 1, 4 - 1,     3.663003663004000e-01);
    rStiffness.insert_element(6 - 1, 6 - 1,     1.098901098901000e+00);
    rStiffness.insert_element(6 - 1, 9 - 1,     1.662284473683000e-17);
    rStiffness.insert_element(6 - 1, 12 - 1,    -1.465201465201000e+00);
    rStiffness.insert_element(6 - 1, 13 - 1,    -2.197802197802000e-01);
    rStiffness.insert_element(6 - 1, 14 - 1,    5.551115123126000e-17);
    rStiffness.insert_element(6 - 1, 15 - 1,    9.683950832925001e-18);
    rStiffness.insert_element(6 - 1, 16 - 1,    5.551115123126000e-17);
    rStiffness.insert_element(6 - 1, 17 - 1,    2.197802197802000e-01);
    rStiffness.insert_element(6 - 1, 18 - 1,    -5.551115123126000e-17);
    rStiffness.insert_element(7 - 1, 7 - 1,     5.494505494505000e-01);
    rStiffness.insert_element(8 - 1, 8 - 1,     1.923076923077000e-01);
    rStiffness.insert_element(9 - 1, 3 - 1,     -7.326007326007000e-01);
    rStiffness.insert_element(9 - 1, 4 - 1,     2.197802197802000e-01);
    rStiffness.insert_element(9 - 1, 6 - 1,     1.662284473683000e-17);
    rStiffness.insert_element(9 - 1, 9 - 1,     1.978021978022000e+00);
    rStiffness.insert_element(9 - 1, 10 - 1,    -4.761904761905000e-01);
    rStiffness.insert_element(9 - 1, 11 - 1,    3.053113317719000e-16);
    rStiffness.insert_element(9 - 1, 12 - 1,    -4.761904761905000e-01);
    rStiffness.insert_element(9 - 1, 13 - 1,    -5.128205128205000e-01);
    rStiffness.insert_element(9 - 1, 14 - 1,    4.761904761905000e-01);
    rStiffness.insert_element(10 - 1, 3 - 1,    2.564102564103000e-01);
    rStiffness.insert_element(10 - 1, 4 - 1,    -2.564102564103000e-01);
    rStiffness.insert_element(10 - 1, 5 - 1,    2.170628349427000e-17);
    rStiffness.insert_element(10 - 1, 9 - 1,    -4.761904761905000e-01);
    rStiffness.insert_element(10 - 1, 10 - 1,   1.978021978022000e+00);
    rStiffness.insert_element(10 - 1, 11 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(10 - 1, 12 - 1,   8.326672684688999e-17);
    rStiffness.insert_element(10 - 1, 13 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(10 - 1, 14 - 1,   -1.465201465201000e+00);
    rStiffness.insert_element(11 - 1, 3 - 1,    -5.128205128205000e-01);
    rStiffness.insert_element(11 - 1, 4 - 1,    5.551115123126000e-17);
    rStiffness.insert_element(11 - 1, 5 - 1,    -5.128205128205000e-01);
    rStiffness.insert_element(11 - 1, 9 - 1,    3.330669073875000e-16);
    rStiffness.insert_element(11 - 1, 10 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(11 - 1, 11 - 1,   3.956043956044000e+00);
    rStiffness.insert_element(11 - 1, 12 - 1,   -1.665334536938000e-16);
    rStiffness.insert_element(11 - 1, 13 - 1,   -1.465201465201000e+00);
    rStiffness.insert_element(11 - 1, 14 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(11 - 1, 15 - 1,   -3.330669073875000e-16);
    rStiffness.insert_element(11 - 1, 16 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(11 - 1, 17 - 1,   -1.465201465201000e+00);
    rStiffness.insert_element(11 - 1, 18 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(12 - 1, 3 - 1,    1.110223024625000e-16);
    rStiffness.insert_element(12 - 1, 4 - 1,    -1.465201465201000e+00);
    rStiffness.insert_element(12 - 1, 6 - 1,    -1.465201465201000e+00);
    rStiffness.insert_element(12 - 1, 9 - 1,    -4.761904761905000e-01);
    rStiffness.insert_element(12 - 1, 10 - 1,   1.387778780781000e-16);
    rStiffness.insert_element(12 - 1, 11 - 1,   -2.775557561563000e-16);
    rStiffness.insert_element(12 - 1, 12 - 1,   3.956043956044000e+00);
    rStiffness.insert_element(12 - 1, 13 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(12 - 1, 14 - 1,   -5.128205128205000e-01);
    rStiffness.insert_element(12 - 1, 15 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(12 - 1, 16 - 1,   -3.330669073875000e-16);
    rStiffness.insert_element(12 - 1, 17 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(12 - 1, 18 - 1,   -5.128205128205000e-01);
    rStiffness.insert_element(13 - 1, 3 - 1,    5.551115123126000e-17);
    rStiffness.insert_element(13 - 1, 4 - 1,    -2.775557561563000e-17);
    rStiffness.insert_element(13 - 1, 5 - 1,    1.387778780781000e-17);
    rStiffness.insert_element(13 - 1, 6 - 1,    -2.197802197802000e-01);
    rStiffness.insert_element(13 - 1, 9 - 1,    -5.128205128205000e-01);
    rStiffness.insert_element(13 - 1, 10 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(13 - 1, 11 - 1,   -1.465201465201000e+00);
    rStiffness.insert_element(13 - 1, 12 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(13 - 1, 13 - 1,   1.978021978022000e+00);
    rStiffness.insert_element(13 - 1, 14 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(14 - 1, 3 - 1,    -4.163336342344000e-17);
    rStiffness.insert_element(14 - 1, 4 - 1,    8.326672684688999e-17);
    rStiffness.insert_element(14 - 1, 5 - 1,    -2.564102564103000e-01);
    rStiffness.insert_element(14 - 1, 6 - 1,    5.551115123126000e-17);
    rStiffness.insert_element(14 - 1, 9 - 1,    4.761904761905000e-01);
    rStiffness.insert_element(14 - 1, 10 - 1,   -1.465201465201000e+00);
    rStiffness.insert_element(14 - 1, 11 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(14 - 1, 12 - 1,   -5.128205128205000e-01);
    rStiffness.insert_element(14 - 1, 13 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(14 - 1, 14 - 1,   1.978021978022000e+00);
    rStiffness.insert_element(15 - 1, 3 - 1,    -7.326007326007000e-01);
    rStiffness.insert_element(15 - 1, 4 - 1,    -2.197802197802000e-01);
    rStiffness.insert_element(15 - 1, 5 - 1,    2.775557561563000e-17);
    rStiffness.insert_element(15 - 1, 6 - 1,    9.683950832925001e-18);
    rStiffness.insert_element(15 - 1, 11 - 1,   -3.608224830032000e-16);
    rStiffness.insert_element(15 - 1, 12 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(15 - 1, 15 - 1,   1.978021978022000e+00);
    rStiffness.insert_element(15 - 1, 16 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(15 - 1, 17 - 1,   -5.128205128205000e-01);
    rStiffness.insert_element(15 - 1, 18 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(16 - 1, 3 - 1,    -2.564102564103000e-01);
    rStiffness.insert_element(16 - 1, 4 - 1,    -2.564102564103000e-01);
    rStiffness.insert_element(16 - 1, 5 - 1,    1.476738959037000e-17);
    rStiffness.insert_element(16 - 1, 6 - 1,    5.551115123126000e-17);
    rStiffness.insert_element(16 - 1, 11 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(16 - 1, 12 - 1,   -3.053113317719000e-16);
    rStiffness.insert_element(16 - 1, 15 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(16 - 1, 16 - 1,   1.978021978022000e+00);
    rStiffness.insert_element(16 - 1, 17 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(16 - 1, 18 - 1,   -1.465201465201000e+00);
    rStiffness.insert_element(17 - 1, 3 - 1,    -2.775557561563000e-16);
    rStiffness.insert_element(17 - 1, 4 - 1,    -1.387778780781000e-16);
    rStiffness.insert_element(17 - 1, 5 - 1,    -2.775557561563000e-17);
    rStiffness.insert_element(17 - 1, 6 - 1,    2.197802197802000e-01);
    rStiffness.insert_element(17 - 1, 11 - 1,   -1.465201465201000e+00);
    rStiffness.insert_element(17 - 1, 12 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(17 - 1, 15 - 1,   -5.128205128205000e-01);
    rStiffness.insert_element(17 - 1, 16 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(17 - 1, 17 - 1,   1.978021978022000e+00);
    rStiffness.insert_element(17 - 1, 18 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(18 - 1, 3 - 1,    -1.249000902703000e-16);
    rStiffness.insert_element(18 - 1, 4 - 1,    -2.498001805407000e-16);
    rStiffness.insert_element(18 - 1, 5 - 1,    2.564102564103000e-01);
    rStiffness.insert_element(18 - 1, 6 - 1,    -5.551115123126000e-17);
    rStiffness.insert_element(18 - 1, 11 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(18 - 1, 12 - 1,   -5.128205128205000e-01);
    rStiffness.insert_element(18 - 1, 15 - 1,   -4.761904761905000e-01);
    rStiffness.insert_element(18 - 1, 16 - 1,   -1.465201465201000e+00);
    rStiffness.insert_element(18 - 1, 17 - 1,   4.761904761905000e-01);
    rStiffness.insert_element(18 - 1, 18 - 1,   1.978021978022000e+00);

    // Set RHS
    rRHS.resize(18);
    std::size_t i_rhs = 0ul;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = -1.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
    rRHS[i_rhs++] = 0.0000000000000000e+00;
}


SparseSpace::VectorType Solve(Model& rModel,
                              ModelPart::DofsArrayType& rDofs,
                              SparseSpace::MatrixType& rStiffness,
                              SparseSpace::VectorType& rRHS)
{
    SparseSpace::VectorType solution(rStiffness.size1());

    HierarchicalSolver<SparseSpace,DenseSpace> solver(Parameters(R"({
        "solver_type" : "hierarchical",
        "verbosity" : 4,
        "max_iterations" : 100,
        "tolerance" : 1e-6,
        "coarse_solver_settings" : {
            "solver_type" : "cg",
            "tolerance" : 2e-1,
            "max_iteration" : 50
        },
        "fine_solver_settings" : {
            "solver_type" : "gauss_seidel",
            "max_iterations" : 3
        }
    })"));

    ModelPart& r_model_part = rModel.GetModelPart("root");
    SparseSpace::SetToZero(solution);
    solver.ProvideAdditionalData(rStiffness,
                                 solution,
                                 rRHS,
                                 rDofs,
                                 r_model_part);

    solver.Solve(rStiffness, solution, rRHS);
    return solution;
}


} // namespace Kratos


int main(int argc, const char* argv[])
{
    Context context;
    std::unique_ptr<Kratos::Model> p_model = Kratos::MakeModel();
    Kratos::ModelPart::DofsArrayType dofs;
    SparseSpace::MatrixType stiffness;
    SparseSpace::VectorType rhs;

    std::string configuration_name = "OneMasterOneSlave";
    if (1 < argc) {
        configuration_name = argv[1];
    }

    if (configuration_name == "OneMasterOneSlave") {
        Kratos::OneMasterOneSlave(p_model, dofs, stiffness, rhs);
    } else if (configuration_name == "OneMasterTwoSlaves") {
        Kratos::OneMasterTwoSlaves(p_model, dofs, stiffness, rhs);
    } else if (configuration_name == "Triangles") {
        Kratos::Triangles(p_model, dofs, stiffness, rhs);
    } else {
        std::cerr << "invalid configuration: " << configuration_name << std::endl;
        return 1;
    }

    Kratos::SkylineLUFactorizationSolver<SparseSpace,DenseSpace> reference_solver(Kratos::Parameters(R"({})"));

    KRATOS_INFO(configuration_name) << "solution: " << Kratos::Solve(*p_model, dofs, stiffness, rhs) << std::endl;

    SparseSpace::VectorType reference_solution(rhs.size());
    SparseSpace::SetToZero(reference_solution);
    reference_solver.Solve(stiffness, reference_solution, rhs);
    KRATOS_INFO(configuration_name) << "reference_solution: " << reference_solution << std::endl;

    SparseSpace::VectorType reference_residual(rhs.size());
    SparseSpace::Mult(stiffness, reference_solution, reference_residual);
    reference_residual = rhs - reference_residual;
    KRATOS_INFO(configuration_name) << "reference residual: " << SparseSpace::TwoNorm(reference_residual) / SparseSpace::TwoNorm(rhs) << std::endl;

    return 0;
}

#else
int main() {}
#endif // __has_include("custom_solvers/hierarchical_solver.h")
