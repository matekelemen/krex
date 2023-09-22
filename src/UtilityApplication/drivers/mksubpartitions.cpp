/// @author Máté Kelemen

// --- Utility Includes ---
#include "UtilityApp/ModelPartIO.hpp"

// --- Core Includes ---
#include "includes/exception.h"
#include "includes/io.h"
#include "includes/kratos_application.h"
#include "includes/lock_object.h"
#include "utilities/parallel_utilities.h"

// --- Structural Mechanics Includes ---
#include "structural_mechanics_application.h"

// --- STL Includes ---
#include <filesystem>
#include <memory>
#include <algorithm>
#include <mutex>
#include <optional>
#include <string>
#include <cctype>


enum class Partition
{
    Root,   // <== root level partition
    Rear,   // <== x == 0
    Front,  // <== x == 1
    Left,   // <== y == 0
    Right,  // <== y == 1
    Bottom, // <== z == 0
    Top,    // <== z == 1
    TopLoad // <== x in [0.5, 1] && y in [0.5, 1] && z == 1
}; // enum class Partition


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

    try {
        Kratos::ModelPart& r_rear = r_root.CreateSubModelPart("rear");
        Kratos::ModelPart& r_front = r_root.CreateSubModelPart("front");
        Kratos::ModelPart& r_left = r_root.CreateSubModelPart("left");
        Kratos::ModelPart& r_right = r_root.CreateSubModelPart("right");
        Kratos::ModelPart& r_bottom = r_root.CreateSubModelPart("bottom");
        Kratos::ModelPart& r_top = r_root.CreateSubModelPart("top");
        Kratos::ModelPart& r_top_load = r_top.CreateSubModelPart("top_load");
        Kratos::LockObject mutex;

        Kratos::IndexPartition<std::size_t>(r_root.Conditions().size()).for_each([&mutex,
                                                                                  &r_root,
                                                                                  &r_rear,
                                                                                  &r_front,
                                                                                  &r_left,
                                                                                  &r_right,
                                                                                  &r_bottom,
                                                                                  &r_top,
                                                                                  &r_top_load](std::size_t iCondition) -> void {
            auto pCondition = *(r_root.Conditions().ptr_begin() + iCondition);
            const auto& r_geometry = pCondition->GetGeometry();
            constexpr double linear_tolerance = 1e-10;
            Partition partition = Partition::Root;

            if (std::all_of(r_geometry.begin(),
                            r_geometry.end(),
                            [linear_tolerance](const auto& rVertex) -> bool {
                                 return -linear_tolerance < rVertex.X() && rVertex.X() < linear_tolerance;
                            })) {
                partition = Partition::Rear;
            } else if (std::all_of(r_geometry.begin(),
                                   r_geometry.end(),
                                   [linear_tolerance](const auto& rVertex) -> bool {
                                        return 1.0 - linear_tolerance < rVertex.X() && rVertex.X() < 1.0 + linear_tolerance;
                                   })) {
                partition = Partition::Front;
            } else if (std::all_of(r_geometry.begin(),
                                   r_geometry.end(),
                                   [linear_tolerance](const auto& rVertex) -> bool {
                                        return -linear_tolerance < rVertex.Y() && rVertex.Y() < linear_tolerance;
                                   })) {
                partition = Partition::Left;
            } else if (std::all_of(r_geometry.begin(),
                                   r_geometry.end(),
                                   [linear_tolerance](const auto& rVertex) -> bool {
                                        return 1.0 - linear_tolerance < rVertex.Y() && rVertex.Y() < 1.0 + linear_tolerance;
                                   })) {
                partition = Partition::Right;
            } else if (std::all_of(r_geometry.begin(),
                            r_geometry.end(),
                            [linear_tolerance](const auto& rVertex) -> bool {
                                return -linear_tolerance < rVertex.Z() && rVertex.Z() < linear_tolerance;
                            })) {
                partition = Partition::Bottom;
            } else if (std::all_of(r_geometry.begin(),
                                   r_geometry.end(),
                                   [linear_tolerance](const auto& rVertex) -> bool {
                                        return 1.0 - linear_tolerance < rVertex.Z() && rVertex.Z() < 1.0 + linear_tolerance;
                                   })) {
                if (std::all_of(r_geometry.begin(),
                                r_geometry.end(),
                                [linear_tolerance](const auto& rVertex) -> bool {
                                     return 0.5 - linear_tolerance < rVertex.Y() && rVertex.Y() < 1.0 + linear_tolerance
                                         && 0.5 - linear_tolerance < rVertex.X() && rVertex.X() < 1.0 + linear_tolerance;
                                })) {
                    partition = Partition::TopLoad;
                } else {
                    partition = Partition::Top;
                }
            }

            if (partition != Partition::Root) {
                std::vector<Kratos::Node::IndexType> node_ids;
                node_ids.reserve(r_geometry.size());
                std::transform(r_geometry.begin(),
                               r_geometry.end(),
                               std::back_inserter(node_ids),
                               [](const Kratos::Node& rNode){
                                    return rNode.Id();
                               });
                std::optional<Kratos::ModelPart*> p_target_model_part;
                switch (partition) {
                    case Partition::Rear: p_target_model_part = &r_rear; break;
                    case Partition::Front: p_target_model_part = &r_front; break;
                    case Partition::Left: p_target_model_part = &r_left; break;
                    case Partition::Right: p_target_model_part = &r_right; break;
                    case Partition::Bottom: p_target_model_part = &r_bottom; break;
                    case Partition::Top: p_target_model_part = &r_top; break;
                    case Partition::TopLoad: p_target_model_part = &r_top_load; break;
                    default: break; // <== shouldn't happen
                }

                std::scoped_lock<Kratos::LockObject> lock(mutex);
                p_target_model_part.value()->AddCondition(pCondition);
                p_target_model_part.value()->AddNodes(node_ids);
            }
        });
    } catch (std::exception& rException) {
        std::cerr << "Error populating subpartitions:\n" << rException.what() << "\n";
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
