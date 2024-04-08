/// @author Máté Kelemen

// --- UtilityApp Includes ---
#include "UtilityApp/ModelPartIO.hpp"
#include "UtilityApp/common.hpp"

// --- Core Includes ---
#include "includes/kratos_application.h"
#include "utilities/parallel_utilities.h"

// --- STL Includes ---
#include <filesystem>


namespace Kratos::UtilityApp {


double Distance(Ref<const array_1d<double,3>> rLeft,
                Ref<const array_1d<double,3>> rRight)
{
    double output = 0.0;
    for (int i_component=0; i_component<3; ++i_component) {
        const double diff = rLeft[i_component] - rRight[i_component];
        output += diff * diff;
    }
    return output;
}


void FillGroups(Ref<ModelPart> rRootModelPart,
                Ref<const std::vector<std::pair<Ptr<ModelPart>,Ptr<const std::array<array_1d<double,3>,2>>>>> rPairs,
                const double Tolerance = 1e-8)
{
    std::vector<std::pair<double,array_1d<double,3>>> radius_midpoint_pairs;
    for (const auto& r_pair : rPairs) {
        const auto& points = *r_pair.second;
        array_1d<double,3> midpoint = 0.5 * (points[0] + points[1]);
        midpoint[2] = 0.035;
        radius_midpoint_pairs.emplace_back(0.25 * Distance(points[1], points[0]),
                                           midpoint);
    }

    const std::size_t pair_count = rPairs.size();
    std::size_t max_node_id = 1ul;
    for (auto it_node=rRootModelPart.Nodes().ptr_begin(); it_node!=rRootModelPart.Nodes().ptr_end(); ++it_node) {
        Ref<const Node> r_node = **it_node;
        max_node_id = std::max(max_node_id, r_node.Id());
        for (std::size_t i_pair=0ul; i_pair<pair_count; ++i_pair) {
            const auto& [r_radius, r_midpoint] = radius_midpoint_pairs[i_pair];
            array_1d<double,3> reference_point = r_node;
            reference_point[2] = r_midpoint[2];
            if (std::abs(Distance(reference_point, r_midpoint) - r_radius) < Tolerance) {
                Ref<ModelPart> r_model_part = *rPairs[i_pair].first;
                r_model_part.AddNode(*it_node);
            }
        }
    }

    for (std::size_t i_pair=0ul; i_pair<rPairs.size(); ++i_pair) {
        const auto& r_pair = rPairs[i_pair];
        const double radius = radius_midpoint_pairs[i_pair].first;
        const auto& r_midpoint = radius_midpoint_pairs[i_pair].second;
        rRootModelPart.CreateSubModelPart(r_pair.first->Name() + "_master").CreateNewNode(++max_node_id,
                                                                                          r_midpoint[0],
                                                                                          r_midpoint[1],
                                                                                          r_midpoint[2] + radius);
    }
}


int main([[maybe_unused]] int argc,
         [[maybe_unused]] const char** const argv)
{
    // Load kratos applications
    const auto p_core = std::make_unique<KratosApplication>("KratosCore");
    p_core->Register();

    // Define source IO
    const std::filesystem::path source_path = "trebuchet_arm.med";
    const auto p_source_io = IOFactory(source_path);

    // Load source model
    Model model;
    Ref<ModelPart> r_root_model_part = model.CreateModelPart("root");
    p_source_io->Read(r_root_model_part);

    // Declare groups
    Ref<ModelPart> r_main_shaft = r_root_model_part.CreateSubModelPart("main_shaft");
    Ref<ModelPart> r_trigger_screw_hole = r_root_model_part.CreateSubModelPart("trigger_screw_hole");
    Ref<ModelPart> r_left_screw_hole = r_root_model_part.CreateSubModelPart("left_screw_hole");
    Ref<ModelPart> r_top_screw_hole = r_root_model_part.CreateSubModelPart("top_screw_hole");
    Ref<ModelPart> r_right_screw_hole = r_root_model_part.CreateSubModelPart("right_screw_hole");
    Ref<ModelPart> r_drive_screw_hole = r_root_model_part.CreateSubModelPart("drive_screw_hole");

    const std::array<array_1d<double,3>,2> main_shaft_points {array_1d<double,3> {-0.01905, 0.0, 0.003175},
                                                              array_1d<double,3> {0.01905, 0.0, 0.003175}};
    const std::array<array_1d<double,3>,2> trigger_screw_hole_points {array_1d<double,3> {-0.131, 0.0, 0.003175},
                                                              array_1d<double,3> {-0.123, 0.0, 0.003175}};
    const std::array<array_1d<double,3>,2> left_screw_hole_points {array_1d<double,3> {-0.031594425, 0, 0.003175},
                                                                   array_1d<double,3> {-0.022863175, 0, 0.003175}};
    const std::array<array_1d<double,3>,2> top_screw_hole_points {array_1d<double,3> {-0.004365625, 0.0272288, 0.003175},
                                                                  array_1d<double,3> {0.004365625, 0.0272288, 0.003175}};
    const std::array<array_1d<double,3>,2> right_screw_hole_points {array_1d<double,3> {0.022863175, 0, 0.003175},
                                                                    array_1d<double,3> {0.031594425, 0, 0.003175}};
    const std::array<array_1d<double,3>,2> drive_screw_hole_points {array_1d<double,3> {0.019121771062276, 0.051219984584376, 0.003175},
                                                                    array_1d<double,3> {0.028646771062276, 0.051219984584376, 0.003175}};

    // Fill groups
    FillGroups(r_root_model_part,
               {{&r_main_shaft, &main_shaft_points},
                {&r_trigger_screw_hole, &trigger_screw_hole_points},
                {&r_left_screw_hole, &left_screw_hole_points},
                {&r_top_screw_hole, &top_screw_hole_points},
                {&r_right_screw_hole, &right_screw_hole_points},
                {&r_drive_screw_hole, &drive_screw_hole_points}});

    // Write the modified model
    IOFactory("trebuchet_arm.mdpa")->Write(r_root_model_part);

    return 0;
}


} // namespace Kratos::UtilityApp


int main(int argc, const char** const argv)
{
    return Kratos::UtilityApp::main(argc, argv);
} // int main
