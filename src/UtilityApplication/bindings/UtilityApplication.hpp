/// @author Máté Kelemen
#pragma once

// --- Core Includes ---
#include "includes/kratos_application.h"


namespace Kratos {


class KRATOS_API(UTILITY_APPLICATION) UtilityApplication final : public KratosApplication
{
public:
    KRATOS_CLASS_POINTER_DEFINITION(UtilityApplication);

    UtilityApplication();

    void Register() override;

private:
    UtilityApplication& operator=(const UtilityApplication&) = delete;

    UtilityApplication(const UtilityApplication&) = delete;
}; // class UtilityApplication


} // namespace Kratos
