/// @author Máté Kelemen

//#ifdef KRATOS_PYTHON

// --- External Includes ---
#include <pybind11/pybind11.h>

// --- Core Includes ---
#include "includes/define_python.h"

// --- UtilityApp Includes ---
#include "bindings/UtilityApplication.hpp"
#include "UtilityApp/FindElementsByCrossSection.hpp"
#include "UtilityApp/CanonicalizeElementsProcess.hpp"
#include "UtilityApp/AMGCLWrapper.hpp"
#include "UtilityApp/MultifreedomConstraintToElementProcess.hpp"
#include "UtilityApp/FEUtilities.hpp"


namespace Kratos::Python{


PYBIND11_MODULE(KratosUtilityApplication, module) {
    pybind11::class_<UtilityApplication,
                     UtilityApplication::Pointer,
                     KratosApplication>(module, "KratosUtilityApplication")
        .def(pybind11::init<>())
        ;

    pybind11::class_<Kratos::UtilityApp::FindElementsByCrossSectionOperation,
                     std::shared_ptr<Kratos::UtilityApp::FindElementsByCrossSectionOperation>,
                     Operation>(module, "FindElementsByCrossSectionOperation")
        .def(pybind11::init<Model&,Parameters>())
        ;

    pybind11::class_<UtilityApp::FindElementsByCrossSectionProcess,
                     std::shared_ptr<UtilityApp::FindElementsByCrossSectionProcess>,
                     Process>(module, "FindElementsByCrossSectionProcess")
        .def(pybind11::init<Model&,Parameters>())
        ;

    pybind11::class_<UtilityApp::CanonicalizeElementsProcess,
                     std::shared_ptr<UtilityApp::CanonicalizeElementsProcess>,
                     Process>(module, "CanonicalizeElementsProcess")
        .def(pybind11::init<Model&,Parameters>())
        ;

    pybind11::class_<UtilityApp::MultifreedomConstraintToElementProcess,
                     UtilityApp::MultifreedomConstraintToElementProcess::Pointer,
                     Process> (module, "MultifreedomConstraintToElementProcess")
    .def(pybind11::init<Model&, Parameters>())
    ;

    pybind11::class_<UtilityApp::FEUtilities>(module, "FEUtilities")
        .def_static("Interpolate", [](
            UtilityApp::Ref<const Geometry<Node>> g,
            UtilityApp::Ref<const Variable<double>> v,
            array_1d<double,3> c,
            bool IsHistorical) {
                std::span<const double,3> coordinates(&c[0], 3);
                if (IsHistorical) return UtilityApp::FEUtilities::Interpolate<double,true>(g, v, coordinates);
                else return UtilityApp::FEUtilities::Interpolate<double,false>(g, v, coordinates);
            })
        .def_static("Interpolate", [](
            UtilityApp::Ref<const Geometry<Node>> g,
            UtilityApp::Ref<const Variable<array_1d<double,3>>> v,
            array_1d<double,3> c,
            bool IsHistorical) {
                std::span<const double,3> coordinates(&c[0], 3);
                if (IsHistorical) return UtilityApp::FEUtilities::Interpolate<array_1d<double,3>,true>(g, v, coordinates);
                else return UtilityApp::FEUtilities::Interpolate<array_1d<double,3>,false>(g, v, coordinates);
            })
        .def_static("Interpolate", [](
            UtilityApp::Ref<const Geometry<Node>> g,
            UtilityApp::Ref<const Variable<Vector>> v,
            array_1d<double,3> c,
            bool IsHistorical) {
                std::span<const double,3> coordinates(&c[0], 3);
                if (IsHistorical) return UtilityApp::FEUtilities::Interpolate<Vector,true>(g, v, coordinates);
                else return UtilityApp::FEUtilities::Interpolate<Vector,false>(g, v, coordinates);
            })
        .def_static("Interpolate", [](
            UtilityApp::Ref<const Geometry<Node>> g,
            UtilityApp::Ref<const Variable<Matrix>> v,
            array_1d<double,3> c,
            bool IsHistorical) {
                std::span<const double,3> coordinates(&c[0], 3);
                if (IsHistorical) return UtilityApp::FEUtilities::Interpolate<Matrix,true>(g, v, coordinates);
                else return UtilityApp::FEUtilities::Interpolate<Matrix,false>(g, v, coordinates);
            })
        ;
} // PYBIND11_MODULE


} // namespace Kratos::Python

//#endif
