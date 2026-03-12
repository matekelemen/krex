// --- Utility Includes ---
#include "UtilityApp/FEUtilities.hpp"

// --- STL Includes ---
#include <algorithm>


namespace Kratos::UtilityApp {


template <class T, bool IsHistorical>
T FEUtilities::Interpolate(
    Ref<const Geometry<Node>> rGeometry,
    Ref<const Variable<T>> rVariable,
    std::span<const double,3> PhysicalCoordinates) {
        T output = rVariable.Zero();

        KRATOS_TRY
        Geometry<Node>::CoordinatesArrayType parametric_coordinates, physical_coordinates;
        physical_coordinates.resize(PhysicalCoordinates.size());
        std::copy(
            PhysicalCoordinates.begin(),
            PhysicalCoordinates.end(),
            physical_coordinates.begin());

        [[maybe_unused]] const bool is_inside = rGeometry.IsInside(
            physical_coordinates,
            parametric_coordinates);

        Vector shape_function_values;
        rGeometry.ShapeFunctionsValues(
            shape_function_values,
            parametric_coordinates);

        for (std::size_t i_node=0ul; i_node<rGeometry.size(); ++i_node) {
            T value;
            if constexpr (IsHistorical) {
                KRATOS_ERROR_IF_NOT(rGeometry[i_node].SolutionStepsDataHas(rVariable));
                value = rGeometry[i_node].GetSolutionStepValue(rVariable);
            } else {
                KRATOS_ERROR_IF_NOT(rGeometry[i_node].Has(rVariable));
                value = rGeometry[i_node].GetValue(rVariable);
            }

            if constexpr (std::is_same_v<T,Vector>) {
                if (output.size() == 0ul) output = ZeroVector(value.size());
                KRATOS_ERROR_IF_NOT(output.size() == value.size());
            } else if constexpr (std::is_same_v<T,Matrix>) {
                if (output.size1() == 0ul) output = ZeroMatrix(value.size1(), value.size2());
                KRATOS_ERROR_IF_NOT(output.size1() == value.size1());
                KRATOS_ERROR_IF_NOT(output.size2() == value.size2());
            }

            output += shape_function_values[i_node] * value;
        } // for i_node in range(rGeometry.size())
        KRATOS_CATCH("")

        return output;
}


#define KRATOS_UTILITY_APP_INSTANTIATE_FEUTILS(T)   \
    template T FEUtilities::Interpolate<T,false>(   \
        Ref<const Geometry<Node>>,                  \
        Ref<const Variable<T>>,                     \
        std::span<const double,3>);                 \
    template T FEUtilities::Interpolate<T,true>(    \
        Ref<const Geometry<Node>>,                  \
        Ref<const Variable<T>>,                     \
        std::span<const double,3>);


using array_1d_d3 = array_1d<double,3>;
KRATOS_UTILITY_APP_INSTANTIATE_FEUTILS(double)
KRATOS_UTILITY_APP_INSTANTIATE_FEUTILS(array_1d_d3)
KRATOS_UTILITY_APP_INSTANTIATE_FEUTILS(Vector)
KRATOS_UTILITY_APP_INSTANTIATE_FEUTILS(Matrix)


#undef KRATOS_UTILITY_APP_INSTANTIATE_FEUTILS


} // namespace Kratos::UtilityApp
