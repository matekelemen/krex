/// @author Máté Kelemen

// -- UtilityApp Includes ---
#include "bindings/UtilityApplication.hpp"
#include "UtilityApp/CanonicalizeElementsProcess.hpp"

// --- Core Includes ---
#include "includes/registry_auxiliaries.h"


namespace Kratos {


UtilityApplication::UtilityApplication()
    : KratosApplication("UtilityApplication")
{
}


void UtilityApplication::Register()
{
    RegistryAuxiliaries::RegisterProcessWithPrototype("UtilityApplication",
                                                      "CanonicalizeElementsProcess",
                                                      UtilityApp::CanonicalizeElementsProcess());
}


} // namespace Kratos
