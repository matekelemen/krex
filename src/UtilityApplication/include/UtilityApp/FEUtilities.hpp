#pragma once

// --- Utility Includes ---
#include "UtilityApp/common.hpp"

// --- Kratos Core Includes ---
#include "geometries/geometry.h"
#include "containers/variable.h"
#include "includes/node.h"

// --- STL Includes ---
#include <span>


namespace Kratos::UtilityApp {


struct FEUtilities {
    template <class T, bool IsHistorical>
    [[nodiscard]] static T Interpolate(
        Ref<const Geometry<Node>> rGeometry,
        Ref<const Variable<T>> rVariable,
        std::span<const double,3> PhysicalCoordinates);
}; // struct FEUtilities


} // namespace Kratos::UtilityApp
