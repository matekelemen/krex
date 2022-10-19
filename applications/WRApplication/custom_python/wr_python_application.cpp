#ifdef KRATOS_PYTHON

// --- External Includes ---
#include <pybind11/pybind11.h>

// --- Project Includes ---
#include "includes/define.h"
#include "wr_application/WRApplication.h"
#include "add_custom_utilities_to_python.h"


namespace Kratos::Python
{


PYBIND11_MODULE(WRApplication, module)
{
    pybind11::class_<WRApplication,
                     WRApplication::Pointer,
                     KratosApplication>(module, "WRApplication")
        .def(pybind11::init<>())
        ;

    AddCustomUtilitiesToPython(module);

    // Register custom variables

} // PYBIND11_MODULE


} // namespace Kratos::Python

#endif // KRATOS_PYTHON
