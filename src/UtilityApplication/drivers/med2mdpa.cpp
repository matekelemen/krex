/// @author Máté Kelemen

// --- Utility Includes ---
#include "UtilityApp/ModelPartIO.hpp"
#include "UtilityApp/ElementFactory.hpp"

// --- Core Includes ---
#include "includes/element.h"
#include "includes/exception.h"
#include "includes/io.h"
#include "includes/kratos_application.h"
#include "includes/kratos_components.h"
#include "includes/lock_object.h"
#include "utilities/parallel_utilities.h"

// --- Structural Mechanics Includes ---
#include "structural_mechanics_application.h"
#include "custom_elements/solid_elements/small_displacement.h"

// --- STL Includes ---
#include <filesystem>
#include <memory>
#include <algorithm>
#include <mutex>
#include <string>
#include <cctype>


int main(int argc, const char** argv)
{
    if (argc != 3) {
        std::cerr << "med2mdpa expects exactly 2 arguments: input file path and output file path\n";
        return 1;
    }
    std::filesystem::path source(argv[1]), target(argv[2]);

    if (!std::filesystem::exists(source) || std::filesystem::is_directory(source)) {
        std::cerr << "File not found: " << source << "\n";
        return 1;
    }

    if (std::filesystem::exists(target)) {
        std::cerr << "File exists: " << target << "\n";
        return 1;
    }

    std::vector<std::unique_ptr<Kratos::KratosApplication>> applications;
    applications.emplace_back(new Kratos::KratosApplication("KratosCore"));
    applications.emplace_back(new Kratos::KratosStructuralMechanicsApplication);
    for (const auto& rp_application : applications) {
        rp_application->Register();
    }

    const auto p_source_io = Kratos::UtilityApp::IOFactory(source);
    const auto p_target_io = Kratos::UtilityApp::IOFactory(target);

    Kratos::Model model;
    Kratos::ModelPart& r_root = model.CreateModelPart("root");

    try {
        p_source_io->Read(r_root);
    } catch (std::exception& rException) {
        std::cerr << "Error reading " << source << ":\n" << rException.what() << "\n";
        return 1;
    }

    const auto element_predicate = [](const Kratos::ModelPart::GeometryType& rGeometry) -> bool {
        switch (rGeometry.GetGeometryType()) {
            case Kratos::GeometryData::KratosGeometryType::Kratos_Tetrahedra3D4 : return true;
            case Kratos::GeometryData::KratosGeometryType::Kratos_Tetrahedra3D10 : return true;
            default: return false;
        }
        return false;
    };

    const auto condition_predicate = [](const Kratos::ModelPart::GeometryType& rGeometry) -> bool {
        switch (rGeometry.GetGeometryType()) {
            case Kratos::GeometryData::KratosGeometryType::Kratos_Triangle3D3 : return true;
            case Kratos::GeometryData::KratosGeometryType::Kratos_Triangle3D6 : return true;
            default: return false;
        }
        return false;
    };

    const auto p_properties = Kratos::Properties::Pointer(new Kratos::Properties);
    const auto element_converter = [p_properties](std::size_t id, Kratos::ModelPart::GeometryType::Pointer pGeometry) -> Kratos::Element::Pointer {
        switch (pGeometry->GetGeometryType()) {
            case Kratos::GeometryData::KratosGeometryType::Kratos_Tetrahedra3D4 :
                return Kratos::KratosComponents<Kratos::Element>::Get("SmallDisplacementElement3D4N").Create(id, pGeometry, p_properties);
            case Kratos::GeometryData::KratosGeometryType::Kratos_Tetrahedra3D10 :
                return Kratos::KratosComponents<Kratos::Element>::Get("SmallDisplacementElement3D10N").Create(id, pGeometry, p_properties);
            default:
                KRATOS_ERROR << "Unsupported geometry type: " << pGeometry->Name();
        }
        KRATOS_ERROR << "Unsupported geometry type: " << pGeometry->Name() << "\n";
    };

    const auto condition_converter = [p_properties](std::size_t id, Kratos::ModelPart::GeometryType::Pointer pGeometry) -> Kratos::Condition::Pointer {
        switch (pGeometry->GetGeometryType()) {
            case Kratos::GeometryData::KratosGeometryType::Kratos_Triangle3D3 :
                return Kratos::KratosComponents<Kratos::Condition>::Get("SurfaceCondition3D3N").Create(id, pGeometry, p_properties);
            case Kratos::GeometryData::KratosGeometryType::Kratos_Triangle3D6 :
                return Kratos::KratosComponents<Kratos::Condition>::Get("SurfaceCondition3D6N").Create(id, pGeometry, p_properties);
            default:
                KRATOS_ERROR << "Unsupported geometry type: " << pGeometry->Name();
        }
        KRATOS_ERROR << "Unsupported geometry type: " << pGeometry->Name() << "\n";
    };

    try {
        Kratos::UtilityApp::ConvertGeometries(r_root.Geometries(), r_root, element_predicate, element_converter);
    } catch (std::exception& rException) {
        std::cerr << "Error converting geometries to elements:\n" << rException.what() << "\n";
        return 1;
    }

    try {
        Kratos::UtilityApp::ConvertGeometries(r_root.Geometries(), r_root, condition_predicate, condition_converter);
    } catch (std::exception& rException) {
        std::cerr << "Error converting geometries to conditions:\n" << rException.what() << "\n";
        return 1;
    }

    try {
        p_target_io->Write(r_root);
    } catch (std::exception& rException) {
        std::cerr << "Error writing " << target << ":\n" << rException.what() << "\n";
        return 1;
    }

    return 0;
}
