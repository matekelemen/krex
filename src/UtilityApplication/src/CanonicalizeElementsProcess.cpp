/// @author Máté Kelemen

// --- UtilityApp Includes ---
#include "UtilityApp/CanonicalizeElementsProcess.hpp"

// --- Core Includes ---
#include "geometries/geometry.h" // Geometry, GeometryData::KratosGeometryType
#include "utilities/geometry_utilities.h" // GeometryUtils::GetGeometryName

// --- STL Includes ---
#include <optional>


namespace Kratos::UtilityApp {


struct CanonicalizeElementsProcess::Impl
{
    std::optional<Ptr<ModelPart>> mpModelPart;
}; // struct CanonicalizeElementsProcess::Impl


void ProcessEdge(Ref<Geometry<Node>> rEdge)
{
    using GeoType = GeometryData::KratosGeometryType;
    const GeoType edge_type = rEdge.GetGeometryType();
    switch (edge_type) {
        case (GeoType::Kratos_Line2D2): break;
        case (GeoType::Kratos_Line2D3):
        case (GeoType::Kratos_Line3D3): {
            const array_1d<double,3> midpoint = 0.5 * (rEdge[0] + rEdge[1]);
            (Ref<array_1d<double,3>>) rEdge[2] = midpoint;
        }
        //default: KRATOS_ERROR << "unsupported edge type " << GeometryUtils::GetGeometryName(edge_type);
        default: break;
    } // switch edge_type
}


CanonicalizeElementsProcess::CanonicalizeElementsProcess()
    : mpImpl(new Impl)
{
}


CanonicalizeElementsProcess::CanonicalizeElementsProcess(Ref<Model> rModel,
                                                         Parameters parameters)
    : mpImpl(new Impl)
{
    parameters.ValidateAndAssignDefaults(this->GetDefaultParameters());
    mpImpl->mpModelPart = &rModel.GetModelPart(parameters["model_part_name"].Get<std::string>());
}


CanonicalizeElementsProcess::CanonicalizeElementsProcess(const CanonicalizeElementsProcess& rRHS)
    : mpImpl(new Impl(*rRHS.mpImpl))
{
}


CanonicalizeElementsProcess::~CanonicalizeElementsProcess()
{
}


void CanonicalizeElementsProcess::Execute()
{
    KRATOS_INFO(this->Info()) << "begin\n";
    using GeoType = GeometryData::KratosGeometryType;
    for (Ref<Element> r_element : mpImpl->mpModelPart.value()->Elements()) {
        auto& r_geometry = r_element.GetGeometry();
        const GeoType geometry_type = r_geometry.GetGeometryType();

        switch (geometry_type) {
            case (GeoType::Kratos_Point2D):
            case (GeoType::Kratos_Point3D):
            case (GeoType::Kratos_Line2D2):
            case (GeoType::Kratos_Line3D2):
            case (GeoType::Kratos_Triangle2D3):
            case (GeoType::Kratos_Triangle3D3):
            case (GeoType::Kratos_Quadrilateral2D4):
            case (GeoType::Kratos_Quadrilateral3D4):
            case (GeoType::Kratos_Tetrahedra3D4):
            case (GeoType::Kratos_Hexahedra3D8):
            case (GeoType::Kratos_Prism3D6):
            case (GeoType::Kratos_Pyramid3D5): {
                break;
            }
            case (GeoType::Kratos_Triangle2D6):
            case (GeoType::Kratos_Tetrahedra3D10): {
                for (auto edge : r_geometry.GenerateEdges()) {
                    ProcessEdge(edge);
                }
                break;
            }
            default: KRATOS_ERROR << "unsupported geometry type " << GeometryUtils::GetGeometryName(geometry_type);
        }
    } // for r_element in model_part.Elements()
    KRATOS_INFO(this->Info()) << "end\n";
}


void CanonicalizeElementsProcess::ExecuteInitialize()
{
    this->Execute();
}


const Parameters CanonicalizeElementsProcess::GetDefaultParameters() const
{
    return Parameters(R"({
"model_part_name" : ""
})");
}


} // namespace Kratos::UtilityApp
