// --- Kratos Includes ---
#include "UtilityApp/FindElementsByCrossSection.hpp"
#include "utilities/parallel_utilities.h"

// --- STL Includes ---
#include <regex>
#include <numeric> // std::inner_product


namespace Kratos::UtilityApp {


bool IsOnPositiveSide(const array_1d<double,3>& rPoint,
                      const array_1d<double,3>& rNormal,
                      const array_1d<double,3>& rOffset) noexcept {
    const array_1d<double,3> transformed = rPoint - rOffset;
    const double projection = std::inner_product(
        transformed.begin(),
        transformed.end(),
        rNormal.begin(),
        0.0);
    return 0.0 < projection;
}

bool Intersects(const Geometry<Node>& rGeometry,
                const array_1d<double,3>& rNormal,
                const array_1d<double,3>& rOffset) noexcept {
    if (!rGeometry.size()) return false;
    bool all_on_positive_side = IsOnPositiveSide(rGeometry.front(), rNormal, rOffset);
    for (std::size_t iNode=1ul; iNode<rGeometry.size(); ++iNode) {
        const bool is_on_positive_side = IsOnPositiveSide(rGeometry[iNode], rNormal, rOffset);
        if (is_on_positive_side != all_on_positive_side) {
            return true;
        }
    }
    return false;
}


FindElementsByCrossSectionOperation::FindElementsByCrossSectionOperation() noexcept
    : mpModelPart(nullptr),
      mCrossSections(),
      mMaybeCSVPath(),
      mModelPartFunctor([](int) -> std::optional<std::string> {return {};})
{}


FindElementsByCrossSectionOperation::FindElementsByCrossSectionOperation(Model& rModel, Parameters Settings)
    : FindElementsByCrossSectionOperation() {
        KRATOS_TRY
        Settings.ValidateAndAssignDefaults(FindElementsByCrossSectionOperation().GetDefaultParameters());

        mpModelPart = &rModel.GetModelPart(Settings["model_part_name"].GetString());

        Parameters normals  = Settings["plane_normals"];
        Parameters offsets  = Settings["plane_offsets"];
        Parameters tags     = Settings["tags"];

        KRATOS_ERROR_IF_NOT(normals.size() == offsets.size());
        KRATOS_ERROR_IF_NOT(normals.size() == tags.size());

        for (std::size_t i_plane=0ul; i_plane<normals.size(); ++i_plane) {
            array_1d<double,3> normal, offset;
            const auto dynamic_normal = normals[i_plane].GetVector();
            KRATOS_ERROR_IF_NOT(dynamic_normal.size() == normal.size());
            std::copy(
                dynamic_normal.begin(),
                dynamic_normal.end(),
                normal.begin());
            const auto dynamic_offset = offsets[i_plane].GetVector();
            KRATOS_ERROR_IF_NOT(dynamic_offset.size() == offset.size());
            std::copy(
                dynamic_offset.begin(),
                dynamic_offset.end(),
                offset.begin());
            mCrossSections.push_back(CrossSectionProperties {
                normal,
                offset,
                tags[i_plane].GetInt()});
        }
        KRATOS_CATCH("")

        KRATOS_TRY
        const std::string csv_file_name = Settings["csv_output_path"].GetString();
        if (!csv_file_name.empty()) {
            std::filesystem::path::string_type path_name;
            std::copy(
                csv_file_name.begin(),
                csv_file_name.end(),
                std::back_inserter(path_name));
            mMaybeCSVPath.emplace(std::move(path_name));
        } // if pattern
        KRATOS_CATCH("")

        KRATOS_TRY
        const std::string pattern = Settings["target_model_part_pattern"].GetString();
        if (!pattern.empty()) {
            mModelPartFunctor = [pattern] (int tag) -> std::optional<std::string> {
                KRATOS_TRY
                std::string output;
                std::regex regex("<tag>");
                std::regex_replace(
                    std::back_inserter(output),
                    pattern.begin(),
                    pattern.end(),
                    regex,
                    std::to_string(tag));
                return output;
                KRATOS_CATCH("")
            }; // mModelPartFunctor
        } // if pattern
        KRATOS_CATCH("")
}


void FindElementsByCrossSectionOperation::Execute() {
    KRATOS_ERROR_IF_NOT(mpModelPart);

    // Check whether postprocessing is requested (not just tagging).
    bool request_postprocessing = false;
    KRATOS_TRY
    const auto maybe_name = mModelPartFunctor(0);
    request_postprocessing = mMaybeCSVPath.has_value() || maybe_name.has_value();
    KRATOS_CATCH("")

    std::vector<std::vector<IndexType>> element_ids(mCrossSections.size());

    KRATOS_TRY
    std::vector<LockObject> mutexes(mCrossSections.size());
    block_for_each(mpModelPart->Elements(), [this, request_postprocessing, &element_ids, &mutexes] (Element& rElement) {
        for (std::size_t i_section=0ul; i_section<mCrossSections.size(); ++i_section) {
            const auto& [r_normal, r_offset, r_tag] = mCrossSections[i_section];
            if (Intersects(rElement.GetGeometry(), r_normal, r_offset)) {
                //auto& r_array = rElement.GetValue(NEIGHBOURS_INDICES);
                //r_array.resize(r_array.size() + 1, true);
                //r_array[r_array.size() - 1] = r_tag;
                rElement.GetValue(RIGID_BODY_ID) = r_tag;
                if (request_postprocessing) {
                    std::scoped_lock<LockObject> lock(mutexes[i_section]);
                    element_ids[i_section].push_back(rElement.Id());
                }
                break;
            }   // if intersect
        } // for i_section in range(mCrossSEctions.size())
    }); // for r_element in mpModelPart->Elements()
    KRATOS_CATCH("")

    KRATOS_TRY
    // If requested,
    // - write results to CSV files, and
    // - copy elements to model parts.
    if (request_postprocessing) {
        std::optional<std::ofstream> maybeCSVFile;
        if (mMaybeCSVPath.has_value()) {
            maybeCSVFile.emplace(mMaybeCSVPath.value());
        }

        for (IndexType i_section=0ul; i_section<mCrossSections.size(); ++i_section) {
            if (maybeCSVFile.has_value()) {
                std::ofstream& rCSVFile = maybeCSVFile.value();
                rCSVFile << mCrossSections[i_section].mTag << ',';
                for (const auto& id : element_ids[i_section]) rCSVFile << id << ",";
                rCSVFile.seekp(rCSVFile.tellp() - std::ofstream::pos_type(1));
                rCSVFile << "\n";
            } // if maybe_file_path

            const auto maybe_model_part_name = mModelPartFunctor(mCrossSections[i_section].mTag);
            if (maybe_model_part_name.has_value()) {
                const std::string& r_model_part_name = maybe_model_part_name.value();
                if (!mpModelPart->HasSubModelPart(r_model_part_name)) {
                    mpModelPart->CreateSubModelPart(r_model_part_name);
                }
                ModelPart& r_model_part = mpModelPart->GetSubModelPart(r_model_part_name);
                r_model_part.AddElements(element_ids[i_section]);

                for (const Element& r_element : r_model_part.Elements()) {
                    std::vector<IndexType> node_ids;
                    std::transform(
                        r_element.GetGeometry().begin(),
                        r_element.GetGeometry().end(),
                        std::back_inserter(node_ids),
                        [] (const Node& r_node) {
                            return r_node.Id();
                        });
                    r_model_part.AddNodes(node_ids);
                }
            } // if maybe_model_part_name
        } // for i_section in range(mCrossSections.size())
    } // if request_postprocessing
    KRATOS_CATCH("")
}


const Parameters FindElementsByCrossSectionOperation::GetDefaultParameters() const {
    return Parameters(R"({
        "model_part_name" : "",
        "plane_normals" : [[1.0, 0.0, 0.0]],
        "plane_offsets" : [[0.0, 0.0, 0.0]],
        "tags" : ["0"],
        "target_model_part_pattern" : "section_<tag>",
        "csv_output_path" : "sections.csv"
    })");
}


} // namespace Kratos::UtilityApp
