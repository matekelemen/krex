/// @author Máté Kelemen

// --- Core Includes ---
#include "includes/model_part_io.h"

// --- Utiltiy Includes ---
#include "UtilityApp/ModelPartIO.hpp"

// --- MED Includes ---
#include "custom_io/med_model_part_io.h"

// --- HDF5 Includes ---
#include "custom_io/hdf5_model_part_io.h"
#include "custom_io/hdf5_file.h"

// --- STL Includes ---
#include <filesystem>


namespace Kratos::UtilityApp {


struct MDPAModelPartIO::Impl {
    std::filesystem::path mFilePath;
}; // struct MDPAModelPartIO::Impl


MDPAModelPartIO::MDPAModelPartIO()
    : mpImpl(new Impl)
{
}


MDPAModelPartIO::MDPAModelPartIO(RightRef<std::filesystem::path> rFilePath)
    : mpImpl(new Impl {std::move(rFilePath)})
{
}


MDPAModelPartIO::~MDPAModelPartIO()
{
}


void MDPAModelPartIO::Read(Ref<ModelPart> rTarget) const
{
    Kratos::ModelPartIO(mpImpl->mFilePath, IO::READ).ReadModelPart(rTarget);
}


void MDPAModelPartIO::Write(Ref<const ModelPart> rSource)
{
    Ref<ModelPart> omfg = const_cast<Ref<ModelPart>>(rSource);
    Kratos::ModelPartIO(mpImpl->mFilePath, IO::WRITE | IO::SCIENTIFIC_PRECISION).WriteModelPart(omfg);
}


struct MedModelPartIO::Impl {
    std::filesystem::path mFilePath;
}; // struct MedModelPartIO::Impl


MedModelPartIO::MedModelPartIO()
    : mpImpl(new Impl)
{
}


MedModelPartIO::MedModelPartIO(RightRef<std::filesystem::path> rFilePath)
    : mpImpl(new Impl {std::move(rFilePath)})
{
}


MedModelPartIO::~MedModelPartIO()
{
}


void MedModelPartIO::Read(Ref<ModelPart> rTarget) const
{
    Kratos::MedModelPartIO(mpImpl->mFilePath, IO::READ).ReadModelPart(rTarget);
}


void MedModelPartIO::Write(Ref<const ModelPart> rSource)
{
    Ref<ModelPart> omfg = const_cast<Ref<ModelPart>>(rSource);
    Kratos::ModelPartIO(mpImpl->mFilePath, IO::WRITE).WriteModelPart(omfg);
}


struct HDF5ModelPartIO::Impl {
    std::filesystem::path mFilePath;
}; // HDF5ModelPartIO::Impl


HDF5ModelPartIO::HDF5ModelPartIO()
    : mpImpl(new Impl)
{
}


HDF5ModelPartIO::~HDF5ModelPartIO()
{
}


HDF5ModelPartIO::HDF5ModelPartIO(RightRef<std::filesystem::path> rFilePath)
    : mpImpl(new Impl {std::move(rFilePath)})
{
}


void HDF5ModelPartIO::Read(Ref<ModelPart> rTarget) const
{
    Kratos::Parameters file_parameters(R"({
        "file_name" : "",
        "file_access_mode" : "read_only"
    })");
    file_parameters["file_name"].SetString(mpImpl->mFilePath.string());
    Kratos::HDF5::File::Pointer p_file(new Kratos::HDF5::File(
        rTarget.GetCommunicator().GetDataCommunicator(),
        file_parameters));
    Kratos::HDF5::ModelPartIO(
        Parameters(R"("prefix" : "/ModelData")"),
        p_file).ReadModelPart(rTarget);
}


void HDF5ModelPartIO::Write(Ref<const ModelPart> rSource)
{
    Kratos::Parameters file_parameters(R"({
        "file_name" : "",
        "file_access_mode" : "exclusive"
    })");
    file_parameters["file_name"].SetString(mpImpl->mFilePath.string());
    Kratos::HDF5::File::Pointer p_file(new Kratos::HDF5::File(
        rSource.GetCommunicator().GetDataCommunicator(),
        file_parameters));
    Ref<ModelPart> omfg = const_cast<Ref<ModelPart>>(rSource);
    Kratos::HDF5::ModelPartIO(
        Parameters(R"("prefix" : "/ModelData")"),
        p_file).WriteModelPart(omfg);
}


std::unique_ptr<Kratos::UtilityApp::ModelPartIO> IOFactory(const std::filesystem::path& rFilePath)
{
    std::string suffix = rFilePath.extension().string();
    std::transform(suffix.begin(),
                   suffix.end(),
                   suffix.begin(),
                   [](auto item){return std::tolower(item);});

    using IOPtr = std::unique_ptr<Kratos::UtilityApp::ModelPartIO>;
    if (suffix == ".mdpa") {
        std::filesystem::path path = rFilePath;
        path.replace_extension("");
        return IOPtr(new Kratos::UtilityApp::MDPAModelPartIO(std::move(path)));
    } else if (suffix == ".med") {
        return IOPtr(new Kratos::UtilityApp::MedModelPartIO(std::filesystem::path(rFilePath)));
    } else if (suffix == ".h5" || suffix == ".hdf5") {
        return IOPtr(new Kratos::UtilityApp::HDF5ModelPartIO(std::filesystem::path(rFilePath)));
    }
    KRATOS_ERROR << "Unsupported file format: " << rFilePath.extension();
}


} // namespace Kratos::UtilityApp
