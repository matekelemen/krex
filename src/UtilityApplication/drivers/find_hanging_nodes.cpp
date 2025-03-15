/// @author Máté Kelemen

// --- Utility Includes ---
#include "UtilityApp/ModelPartIO.hpp" // UtilityApp::ModelPartIO

// --- Core Includes ---
#include "geometries/geometry.h" // Geometry
#include "includes/kratos_application.h" // KratosApplication

// --- STL Includes ---
#include <iostream> // std::cout, std::cerr
#include <memory> // std::unique_ptr
#include <filesystem> // std::filesystem::path, std::filesystem::exists, std::filesystem::is_directory
#include <vector> // std::vector
#include <atomic> // std::atomic
#include <cstdint> // std::uint8_t
#include <algorithm> // std::lower_bound
#include <optional> // std::optional


template <class TEntity>
const Kratos::Geometry<Kratos::Node>& GetGeometry(const TEntity& rEntity)
{
    return rEntity.GetGeometry();
}


template <>
const Kratos::Geometry<Kratos::Node>& GetGeometry<Kratos::Geometry<Kratos::Node>>(const Kratos::Geometry<Kratos::Node>& rEntity)
{
    return rEntity;
}


std::optional<std::size_t> FindNodeIndex(std::size_t NodeId,
                                         Kratos::ModelPart::NodesContainerType::const_iterator itNodeBegin,
                                         Kratos::ModelPart::NodesContainerType::const_iterator itNodeEnd)
{
    const auto it = std::lower_bound(itNodeBegin,
                                     itNodeEnd,
                                     NodeId,
                                     [](const Kratos::Node& r_node, std::size_t target_id){
                                        return r_node.Id() < target_id;
                                     });
    if (it == itNodeEnd or it->Id() != NodeId) {
        return {};
    } else {
        return std::distance(itNodeBegin, it);
    }
}


template <class TItEntity>
void MarkNodes(std::vector<std::atomic<std::uint8_t>>& rHangingNodes,
               TItEntity itEntityBegin,
               TItEntity itEntityEnd,
               Kratos::ModelPart::NodesContainerType::const_iterator itNodeBegin,
               Kratos::ModelPart::NodesContainerType::const_iterator itNodeEnd)
{
    block_for_each(itEntityBegin,
                   itEntityEnd,
                   [&rHangingNodes, itNodeBegin, itNodeEnd](const auto& r_entity){
                        for (const Kratos::Node& r_node : GetGeometry(r_entity)) {
                            const auto i_maybe_node = FindNodeIndex(r_node.Id(), itNodeBegin, itNodeEnd);
                            if (not i_maybe_node) {
                                std::stringstream message;
                                message << "cannot find node " << r_node.Id() << " ";
                                if constexpr (not std::is_same_v<Kratos::Geometry<Kratos::Node>,typename TItEntity::value_type>)
                                    message << " in entity " << r_entity.Id() << " ";
                                message << " in geometry " << GetGeometry(r_entity).Id();
                                throw std::runtime_error(message.str());
                            } else {
                                const auto i_node = *i_maybe_node;
                                rHangingNodes[i_node] = 0;
                            }
                        }
                   });
}


int main(int argc, const char** argv)
{
    if (argc != 2) {
        std::cerr << "linearizemesh expects exactly 1 argument: input file path\n";
        return 1;
    }
    std::filesystem::path source(argv[1]);

    if (!std::filesystem::exists(source) || std::filesystem::is_directory(source)) {
        std::cerr << "File not found: " << source << "\n";
        return 1;
    }

    std::vector<std::unique_ptr<Kratos::KratosApplication>> applications;
    applications.emplace_back(new Kratos::KratosApplication("KratosCore"));
    for (const auto& rp_application : applications) {
        rp_application->Register();
    }

    Kratos::Model model;
    Kratos::ModelPart& r_source_model_part = model.CreateModelPart("source");

    try {
        const auto p_source_io = Kratos::UtilityApp::IOFactory(source);
        p_source_io->Read(r_source_model_part);
    } catch (std::exception& rException) {
        std::cerr << "Error reading " << source << ":\n" << rException.what() << "\n";
        return 1;
    }

    std::vector<std::atomic<std::uint8_t>> hanging_nodes(r_source_model_part.Nodes().size());
    std::fill(hanging_nodes.begin(),
              hanging_nodes.end(),
              static_cast<std::uint8_t>(1));

    MarkNodes(hanging_nodes,
              r_source_model_part.Geometries().begin(),
              r_source_model_part.Geometries().end(),
              r_source_model_part.Nodes().begin(),
              r_source_model_part.Nodes().end());

    MarkNodes(hanging_nodes,
              r_source_model_part.Elements().begin(),
              r_source_model_part.Elements().end(),
              r_source_model_part.Nodes().begin(),
              r_source_model_part.Nodes().end());

    for (std::size_t i_node=0ul; i_node<hanging_nodes.size(); ++i_node) {
        if (hanging_nodes[i_node]) {
            const auto it_node = r_source_model_part.Nodes().begin();
            std::cout << it_node->Id() << '\n';
        } // if hanging_nodes[i_node]
    } // for i_node in range(hanging_nodes.size())

    return 0;
}
