/// @author Máté Kelemen

// -- UtilityApp Includes ---
#include "bindings/UtilityApplication.hpp"
#include "UtilityApp/CanonicalizeElementsProcess.hpp"
#include "UtilityApp/AMGCLWrapper.hpp"

// --- Core Includes ---
#include "includes/registry_auxiliaries.h"
#include "factories/standard_linear_solver_factory.h"
#include "spaces/ublas_space.h"


namespace Kratos {


template <class TValue, class TSolver>
void RegisterLinearSolver(const std::string& rName)
{
    static auto factory = StandardLinearSolverFactory<
        TUblasSparseSpace<TValue>,
        TUblasDenseSpace<double>,
        TSolver
    >();

    KratosComponents<LinearSolverFactory<
        TUblasSparseSpace<TValue>,
        TUblasDenseSpace<double>>
    >::Add(rName, factory);
}


UtilityApplication::UtilityApplication()
    : KratosApplication("UtilityApplication")
{
}


void UtilityApplication::Register()
{
    KRATOS_TRY

    RegistryAuxiliaries::RegisterProcessWithPrototype("UtilityApplication",
                                                      "CanonicalizeElementsProcess",
                                                      UtilityApp::CanonicalizeElementsProcess());

    RegisterLinearSolver<double,AMGCLWrapper<
        TUblasSparseSpace<double>,
        TUblasDenseSpace<double>
    >>("amgcl_wrapper");

    RegisterLinearSolver<float,AMGCLWrapper<
        TUblasSparseSpace<float>,
        TUblasDenseSpace<double>
    >>("amgcl_wrapper");

    KRATOS_CATCH("")
}


} // namespace Kratos
