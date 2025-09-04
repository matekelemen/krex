/// @author Máté Kelemen

#pragma once

// --- Utility Includes ---
#include "UtilityApp/common.hpp"
#include "includes/define.h"
#include "utilities/parallel_utilities.h"
#include "utilities/reduction_utilities.h"

// --- Core Includes ---
#include "includes/model_part.h"
#include "utilities/proxies.h"

// --- STL Includes ---
#include <type_traits>


namespace Kratos::UtilityApp {


template <class TPredicate,
          class TFactoryFunctor>
void ConvertGeometries(Ref<const ModelPart::GeometryContainerType> rSourceContainer,
                       ModelPart& rTarget,
                       RightRef<TPredicate> rPredicate,
                       RightRef<TFactoryFunctor> rFactoryFunctor)
{
    KRATOS_TRY

    using FactoryReturnType = decltype(std::declval<std::decay_t<TFactoryFunctor>>()(std::size_t(), std::declval<ModelPart::GeometryType::Pointer>()));
    constexpr Globals::DataLocation location =
        std::is_same_v<FactoryReturnType,Element::Pointer> ?
            Globals::DataLocation::Element :
            std::is_same_v<FactoryReturnType,Condition::Pointer> ?
                Globals::DataLocation::Condition :
                Globals::DataLocation::ProcessInfo;
    static_assert(location != Globals::DataLocation::ProcessInfo, "Invalid factory return type");

    std::size_t id = block_for_each<MaxReduction<std::size_t>>(MakeProxy<location>(rTarget),
                                                               [](const auto& rProxy) {return rProxy.GetEntity().Id();});

    for (auto it_source=rSourceContainer.ptr_begin(); it_source!=rSourceContainer.ptr_end(); ++it_source) {
        if (rPredicate(**it_source)) {
            if constexpr (std::is_same_v<FactoryReturnType,Element::Pointer>) {
                rTarget.AddElement(rFactoryFunctor(++id, *it_source));
            } else if constexpr (std::is_same_v<FactoryReturnType,Condition::Pointer>) {
                rTarget.AddCondition(rFactoryFunctor(++id, *it_source));
            } else {
                static_assert(std::is_same_v<FactoryReturnType,Element::Pointer>, "Invalid factory return type");
            }
        }
    }

    KRATOS_CATCH("")
}



} // namespace Kratos::UtilityApp
