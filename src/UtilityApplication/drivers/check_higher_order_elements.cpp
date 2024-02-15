/// @author Máté Kelemen
/// @details Load a @ref Kratos::ModelPart and check whether every
///          lagrangian high order element (quadratic or higher)
///          has its nodes in their canonical positions.

// --- UtilityApp Includes ---
#include "UtilityApp/ModelPartIO.hpp"
#include "UtilityApp/common.hpp"

// --- Core Includes ---

// --- Structural Mechanics Includes ---
#include "structural_mechanics_application.h"
#include "utilities/geometry_utilities.h" // GeometryUtils::GetGeometryName

// --- STL Includes ---
#include <iostream> // cerr
#include <filesystem> // path, exists, is_directory
#include <vector> // vector


namespace Kratos::UtilityApp {


int main(int argc, const char** argv)
{
    constexpr double tolerance = 1e-10;

    if (argc != 2) {
        std::cerr << "missing argument for input model part\n";
        return 1;
    }

    const std::filesystem::path source_path(argv[1]);
    if (!std::filesystem::exists(source_path) || std::filesystem::is_directory(source_path)) {
        std::cerr << "File not found: " << source_path << "\n";
        return 1;
    }

    // Load kratos applications
    std::vector<std::unique_ptr<KratosApplication>> applications;
    applications.emplace_back(new KratosApplication("KratosCore"));
    applications.emplace_back(new KratosStructuralMechanicsApplication);
    for (auto& rp_application : applications) rp_application->Register();

    // Load the model part
    const auto p_source_io = UtilityApp::IOFactory(source_path);
    Model model;
    Ref<ModelPart> r_root = model.CreateModelPart("root");
    try {
        p_source_io->Read(r_root);
    } catch (Ref<std::exception> rException) {
        std::cerr << "Error reading " << source_path << ":}n" << rException.what() << "\n";
        return 1;
    }

    // Loop through elements and check node positions
    for (const Element& r_element : r_root.Elements()) {
        Ref<const Geometry<Node>> r_geometry = r_element.GetGeometry();

        using GeoType = GeometryData::KratosGeometryType;
        switch (r_geometry.GetGeometryType()) {
            case GeoType::Kratos_Point2D: break;
            case GeoType::Kratos_Point3D: break;
            case GeoType::Kratos_Tetrahedra3D4: break;
            case GeoType::Kratos_Tetrahedra3D10: {
                for (auto [i_left, i_mid, i_right] : std::vector<std::array<int,3>> {{0, 4, 1},
                                                                                     {1, 5, 2},
                                                                                     {2, 6, 0},
                                                                                     {0, 7, 3},
                                                                                     {1, 8, 3},
                                                                                     {2, 9, 3}}) {
                    const array_1d<double,3> midpoint = 0.5 * (r_geometry[i_left] + r_geometry[i_right]);
                    const array_1d<double,3> diff = midpoint - (const array_1d<double,3>&) r_geometry[i_mid];
                    const double edge_length = std::sqrt(inner_prod(r_geometry[i_left] - r_geometry[i_right], r_geometry[i_left] - r_geometry[i_right]));
                    const double distance = std::sqrt(inner_prod(diff, diff));
                    KRATOS_WARNING_IF(GeometryUtils::GetGeometryName(r_geometry.GetGeometryType()) + ":", tolerance < distance / edge_length)
                        << "high order node " << r_geometry[i_mid].Id() << " in element " << r_element.Id()
                        << " is off by " << distance << " on edge with length " << edge_length << "\n";
                }
                break;
            } // case Tet3D10N
            default: std::cerr << "Unhandled geometry type: " << GeometryUtils::GetGeometryName(r_geometry.GetGeometryType());
        } // switch element geometry type
    } // for r_element in r_root.Elements()

    return 0;
} // int main


}


int main(int argc, const char** argv)
{
    return Kratos::UtilityApp::main(argc, argv);
} // int main

