#pragma once

// --- Kratos Includes ---
#include "operations/operation.h"
#include "processes/process.h"

// --- STL Includes ---
#include <functional> // std::function
#include <optional> //std::optional
#include <filesystem> // std::filesystem::path


namespace Kratos::UtilityApp {


class FindElementsByCrossSectionOperation : public Operation {
public:
    FindElementsByCrossSectionOperation() noexcept;

    FindElementsByCrossSectionOperation(Model& rModel, Parameters Settings);

    void Execute() override;

    const Parameters GetDefaultParameters() const override;

private:
    ModelPart* mpModelPart;

    struct CrossSectionProperties {
        array_1d<double,3> mNormal, mOffset;
        int mTag;
    };

    std::vector<CrossSectionProperties> mCrossSections;

    std::optional<std::filesystem::path> mMaybeCSVPath;

    std::function<std::optional<std::string>(int)> mModelPartFunctor;
}; // class FindElementsByCrossSectionOperation


class FindElementsByCrossSectionProcess : public Process {
public:
    FindElementsByCrossSectionProcess() noexcept = default;

    FindElementsByCrossSectionProcess(
        Model& rModel,
        Parameters Settings)
            : mOperation(rModel, Settings)
    {}

    void ExecuteBeforeSolutionLoop() override {
        mOperation.Execute();
    }

    const Parameters GetDefaultParameters() const override {
        return mOperation.GetDefaultParameters();
    }

private:
    FindElementsByCrossSectionOperation mOperation;
}; // FindElementsByCrossSectionProcess


} // namespace Kratos::UtilityApp
