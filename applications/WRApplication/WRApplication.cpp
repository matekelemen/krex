// --- WR Includes ---
#include "wr_application/WRApplication.h"


namespace Kratos
{


WRApplication::WRApplication()
    : KratosApplication("WindEngineeringApplication")
{
}


void WRApplication::Register()
{
    KRATOS_INFO("") << "Initializing WRApplication..." << std::endl;
    // Register custom variables
}


std::string WRApplication::Info() const
{
    return "WRApplication";
}


void WRApplication::PrintInfo(std::ostream& rStream) const
{
    rStream << this->Info();
    this->PrintData(rStream);
}


void WRApplication::PrintData(std::ostream& rStream) const
{
    KRATOS_WATCH("in my application");
    KRATOS_WATCH(KratosComponents<VariableData>::GetComponents().size() );

    rStream << "Variables:" << std::endl;
    KratosComponents<VariableData>().PrintData(rStream);
    rStream << std::endl;
    rStream << "Elements:" << std::endl;
    KratosComponents<Element>().PrintData(rStream);
    rStream << std::endl;
    rStream << "Conditions:" << std::endl;
    KratosComponents<Condition>().PrintData(rStream);
}


} // namespace Kratos