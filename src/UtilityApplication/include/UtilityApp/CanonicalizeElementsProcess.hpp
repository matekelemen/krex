/// @author Máté Kelemen

// --- UtilityApp Includes ---
#include "UtilityApp/common.hpp" // Ref

// --- Core Includes ---
#include "processes/process.h" // Process
#include "containers/model.h" // Model
#include "includes/kratos_parameters.h" // Parameters

// --- STL Includes ---
#include <memory> // unique_ptr


namespace Kratos::UtilityApp {


class CanonicalizeElementsProcess final : public Process
{
public:
    CanonicalizeElementsProcess();

    CanonicalizeElementsProcess(Ref<Model> rModel,
                                Parameters parameters);

    CanonicalizeElementsProcess(const CanonicalizeElementsProcess& rRHS);

    ~CanonicalizeElementsProcess() override;

    const Parameters GetDefaultParameters() const override;

    void Execute() override;

    void ExecuteInitialize() override;

private:
    struct Impl;
    std::unique_ptr<Impl> mpImpl;
}; // class CanonicalizeElementsProcess


} // namespace Kratos::UtilityApp
