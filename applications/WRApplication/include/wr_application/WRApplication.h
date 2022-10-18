#pragma once

// --- Core Includes ---
#include "includes/kratos_application.h"

// --- STL Includes ---
#include <ostream>


namespace Kratos {

///@name Kratos Classes
///@{

class KRATOS_API(WR_APPLICATION) WRApplication : public KratosApplication {
public:
    ///@name Type Definitions
    ///@{

    KRATOS_CLASS_POINTER_DEFINITION(WRApplication);

    ///@}
    ///@name Life Cycle
    ///@{

    /// Default constructor.
    WRApplication();

    /// Destructor.
    ~WRApplication() override {}

    ///@}
    ///@name Operations
    ///@{

    void Register() override;

    ///@}
    ///@name Input and output
    ///@{

    std::string Info() const override;

    void PrintInfo(std::ostream& rOStream) const override;

    void PrintData(std::ostream& rOStream) const override;

    ///@}
private:
    ///@name Un accessible methods
    ///@{

    WRApplication& operator=(const WRApplication& rOther) = delete;

    WRApplication(const WRApplication& rOther) = delete;

    ///@}

}; // class WRApplication

///@}


} // namespace Kratos
