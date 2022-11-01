#ifndef KRATOS_BENCHMARK_COMMON_HPP
#define KRATOS_BENCHMARK_COMMON_HPP

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
    static Model& GetModel();

private:
    static std::unique_ptr<KratosApplication> mpApplication;

    static std::unique_ptr<Model> mpModel;
}; // class ModelFactory


} // namespace Kratos


#endif
