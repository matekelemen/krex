/// @author Máté Kelemen

// --- Utility Includes ---
#include "UtilityApp/ModelPartIO.hpp" // UtilityApp::ModelPartIO

// --- Core Includes ---
#include "geometries/geometry.h" // Geometry
#include "geometries/geometry_data.h" // GeometryData
#include "includes/kratos_application.h" // KratosApplication

// --- STL Includes ---
#include <iostream> // std::cout, std::cerr
#include <vector> // std::vector
#include <filesystem> // std::filesystem::path, std::filesystem::exists, std::filesystem::is_directory
#include <memory> // std::unique_ptr


void CheckRegisteredGeometry(const std::string& rGeometryName)
{
    KRATOS_ERROR_IF_NOT(Kratos::KratosComponents<Kratos::Geometry<Kratos::Node>>::Has(rGeometryName))
        << "No geometry named \"" << rGeometryName << "\" is loaded. "
        << "Did you forget to link the application it's defined in?";
}


void ProcessModelTree(Kratos::ModelPart& rSourceTree,
                      Kratos::ModelPart& rTargetTree)
{
    for (Kratos::ModelPart& r_source_child : rSourceTree.SubModelParts()) {
        const std::string child_name = r_source_child.Name();
        Kratos::ModelPart& r_target_child = rTargetTree.CreateSubModelPart(child_name);
        ProcessModelTree(r_source_child, r_target_child);
    } // for r_source_child in rSourceTree.SubModelParts

    // Copy nodes.
    rTargetTree.AddNodes(rSourceTree.Nodes().begin(),
                         rSourceTree.Nodes().end());

    // Convert geometries to their linear counterparts.
    for (const auto& r_geometry : rSourceTree.Geometries()) {
        switch (r_geometry.GetGeometryFamily()) {

            case Kratos::GeometryData::KratosGeometryFamily::Kratos_Point: {
                const std::string name = r_geometry.GetGeometryType() == Kratos::GeometryData::KratosGeometryType::Kratos_Point2D
                        ? "Point2D"
                        : "Point3D";
                CheckRegisteredGeometry(name);
                Kratos::Element::NodesArrayType nodes(r_geometry.ptr_begin(), r_geometry.ptr_end());
                rTargetTree.CreateNewGeometry(name, r_geometry.Id(), nodes);
                break;
            }

            case Kratos::GeometryData::KratosGeometryFamily::Kratos_Linear: {
                const std::string name = r_geometry.GetGeometryType() == Kratos::GeometryData::KratosGeometryType::Kratos_Line2D2
                                      or r_geometry.GetGeometryType() == Kratos::GeometryData::KratosGeometryType::Kratos_Line2D3
                                      or r_geometry.GetGeometryType() == Kratos::GeometryData::KratosGeometryType::Kratos_Line2D4
                                      or r_geometry.GetGeometryType() == Kratos::GeometryData::KratosGeometryType::Kratos_Line2D5
                        ? "Line2D2"
                        : "Line3D2";
                CheckRegisteredGeometry(name);
                Kratos::Element::NodesArrayType nodes(r_geometry.ptr_begin(), r_geometry.ptr_begin() + 2);
                rTargetTree.CreateNewGeometry(name, r_geometry.Id(), nodes);
                break;
            }

            case Kratos::GeometryData::KratosGeometryFamily::Kratos_Triangle: {
                const std::string name = r_geometry.GetGeometryType() == Kratos::GeometryData::KratosGeometryType::Kratos_Triangle2D3
                        or r_geometry.GetGeometryType() == Kratos::GeometryData::KratosGeometryType::Kratos_Triangle2D6
                        ? "Triangle2D3"
                        : "Triangle3D3";
                CheckRegisteredGeometry(name);
                Kratos::Element::NodesArrayType nodes(r_geometry.ptr_begin(), r_geometry.ptr_begin() + 3);
                rTargetTree.CreateNewGeometry(name, r_geometry.Id(), nodes);
                break;
            }

            case Kratos::GeometryData::KratosGeometryFamily::Kratos_Tetrahedra: {
                const std::string name = "Tetrahedra3D4";
                CheckRegisteredGeometry(name);
                Kratos::Element::NodesArrayType nodes(r_geometry.ptr_begin(), r_geometry.ptr_begin() + 4);
                rTargetTree.CreateNewGeometry(name, r_geometry.Id(), nodes);
                break;
            }

            default: KRATOS_ERROR << "unsupported geometry family of " << r_geometry.Name();

        } // switch r_geometry.GetGeometryFamily
    } // for r_geometry in r_source_model_part.Geometries()
}


int main(int argc, const char** argv)
{
    if (argc != 3) {
        std::cerr << "linearizemesh expects exactly 2 arguments: input file path and output file path\n";
        return 1;
    }
    std::filesystem::path source(argv[1]), target(argv[2]);

    if (!std::filesystem::exists(source) || std::filesystem::is_directory(source)) {
        std::cerr << "File not found: " << source << "\n";
        return 1;
    }

    if (std::filesystem::is_directory(target)) {
        std::cerr << "Output path is a directory: " << target << "\n";
        return 1;
    }

    std::vector<std::unique_ptr<Kratos::KratosApplication>> applications;
    applications.emplace_back(new Kratos::KratosApplication("KratosCore"));
    for (const auto& rp_application : applications) {
        rp_application->Register();
    }

    const auto p_source_io = Kratos::UtilityApp::IOFactory(source);
    const auto p_target_io = Kratos::UtilityApp::IOFactory(target);

    Kratos::Model model;
    Kratos::ModelPart& r_source_model_part = model.CreateModelPart("source");
    Kratos::ModelPart& r_target_model_part = model.CreateModelPart("target");

    try {
        p_source_io->Read(r_source_model_part);
    } catch (std::exception& rException) {
        std::cerr << "Error reading " << source << ":\n" << rException.what() << "\n";
        return 1;
    }

    ProcessModelTree(r_source_model_part, r_target_model_part);

    try {
        p_target_io->Write(r_target_model_part);
    } catch (std::exception& rException) {
        std::cerr << "Error writing " << target << ":\n" << rException.what() << "\n";
        return 1;
    }

    return 0;
}

