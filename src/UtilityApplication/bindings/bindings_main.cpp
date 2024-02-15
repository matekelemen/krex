/// @author Máté Kelemen

#ifdef KRATOS_PYTHON

// --- External Includes ---
#include <pybind11/pybind11.h>

// --- Core Includes ---
#include "includes/define_python.h"

// --- UtilityApp Includes ---
#include "bindings/UtilityApplication.hpp"
#include "UtilityApp/CanonicalizeElementsProcess.hpp"


namespace Kratos::Python{


PYBIND11_MODULE(KratosUtilityApplication, module)
{
    pybind11::class_<UtilityApplication,
                     UtilityApplication::Pointer,
                     KratosApplication>(module, "KratosWRApplication")
        .def(pybind11::init<>())
        ;

    pybind11::class_<UtilityApp::CanonicalizeElementsProcess,std::shared_ptr<UtilityApp::CanonicalizeElementsProcess>,Process>(module, "CanonicalizeElementsProcess")
        .def(pybind11::init<Model&,Parameters>())
        ;
} // PYBIND11_MODULE


} // namespace Kratos::Python

#endif
