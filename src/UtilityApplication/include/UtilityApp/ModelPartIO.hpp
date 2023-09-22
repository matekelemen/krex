/// @author Máté Kelemen

#pragma once

// --- Utility Includes ---
#include "UtilityApp/common.hpp"

// --- Core Includes ---
#include "containers/model.h"
#include "includes/model_part.h"

// --- STL Includes ---
#include <filesystem>
#include <memory>


namespace Kratos::UtilityApp {


class ModelPartIO
{
public:
    virtual void Read(Ref<ModelPart> rTarget) const = 0;

    virtual void Write(Ref<const ModelPart> rSource) = 0;

    virtual ~ModelPartIO() = default;
}; // class ModelPartIO


class MDPAModelPartIO final : public ModelPartIO
{
public:
    MDPAModelPartIO();

    explicit MDPAModelPartIO(RightRef<std::filesystem::path> rFilePath);

    ~MDPAModelPartIO() override;

    void Read(Ref<ModelPart> rTarget) const override;

    void Write(Ref<const ModelPart> rSource) override;

private:
    struct Impl;
    std::unique_ptr<Impl> mpImpl;
}; // class MDPAModelPartIO


class MedModelPartIO final : public ModelPartIO
{
public:
    MedModelPartIO();

    explicit MedModelPartIO(RightRef<std::filesystem::path> rFilePath);

    ~MedModelPartIO() override;

    void Read(Ref<ModelPart> rTarget) const override;

    void Write(Ref<const ModelPart> rSource) override;

private:
    struct Impl;
    std::unique_ptr<Impl> mpImpl;
}; // class MedModelPartIO


class HDF5ModelPartIO final : public ModelPartIO
{
public:
    HDF5ModelPartIO();

    HDF5ModelPartIO(RightRef<std::filesystem::path> rFilePath);

    ~HDF5ModelPartIO() override;

    void Read(Ref<ModelPart> rTarget) const override;

    void Write(Ref<const ModelPart> rSource) override;

private:
    struct Impl;
    std::unique_ptr<Impl> mpImpl;
} ; // class HDF5ModelPartIO


std::unique_ptr<Kratos::UtilityApp::ModelPartIO> IOFactory(const std::filesystem::path& rFilePath);


} // namespace Kratos::UtilityApp
