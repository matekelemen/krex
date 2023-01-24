#ifdef KRATOS_PYTHON

// --- External Includes ---
#include <pybind11/pybind11.h>

// --- Core Includes ---
#include "includes/define_python.h"

// --- WRApp Includes ---
#include "wr_application/WRApp.hpp"
#include "wr_application/WRApp_variables.hpp"
#include "add_custom_utilities_to_python.hpp"


namespace Kratos::Python{


PYBIND11_MODULE(WRApp, module)
{
    pybind11::class_<WRApp,
                     WRApp::Pointer,
                     KratosApplication>(module, "WRApp")
        .def(pybind11::init<>())
        ;

    AddCustomUtilitiesToPython(module);

    // Register custom variables
    KRATOS_REGISTER_IN_PYTHON_VARIABLE(module, ANALYSIS_PATH)

} // PYBIND11_MODULE


} // namespace Kratos::Python

#endif
