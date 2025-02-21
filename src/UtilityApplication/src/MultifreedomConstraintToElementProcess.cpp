/// @author Máté Kelemen

// Core includes
#include "UtilityApp/MultifreedomConstraintToElementProcess.hpp" // MultifreedomConstraintToElementProcess
#include "containers/model.h" // Model
#include "includes/model_part.h" // ModelPart
#include "includes/master_slave_constraint.h" // MasterSlaveConstraint
#include "includes/element.h" // Element
#include "includes/kratos_components.h" // KratosComponents
#include "includes/key_hash.h" // HashCombine
#include "utilities/interval_utility.h" // IntervalUtility

// System includes
#include <unordered_map> // unordered_map
#include <unordered_set> // unordered_set
#include <optional> // optional


namespace Kratos::UtilityApp {


struct MultifreedomConstraintToElementProcess::Impl
{
    /// @brief Pair of integers with commutative hash and equality comparisons.
    template <class TIndex>
    struct SymmetricIdPair
    {
        using value_type = TIndex;

        using first_type = value_type;

        using second_type = value_type;

        struct hash
        {
            std::size_t operator()(SymmetricIdPair Instance) const noexcept
            {
                value_type min = std::min(Instance.first, Instance.second);
                const value_type max = std::max(Instance.first, Instance.second);
                HashCombine(min, max); // <== mutates min
                return min;
            }
        }; // struct hash

        friend bool operator==(SymmetricIdPair Left, SymmetricIdPair Right) noexcept
        {
            return (Left.first == Right.first && Left.second == Right.second)
                || (Left.first == Right.second && Left.second == Right.first);
        }

        first_type first;

        second_type second;
    };


    /// @brief Pair of symmetric ID pairs.
    /// @note This pair is NOT symmetric, but the subpairs are.
    template <class TFirst, class TSecond>
    struct NestedIdPair
    {
        using first_type = SymmetricIdPair<TFirst>;

        using second_type = SymmetricIdPair<TSecond>;

        struct hash
        {
            std::size_t operator()(NestedIdPair Instance) const noexcept
            {
                auto first_hash = typename first_type::hash()(Instance.first);
                const auto second_hash = typename second_type::hash()(Instance.second);
                HashCombine(first_hash, second_hash); // <== mutates first_hash
                return first_hash;
            }
        }; // struct hash

        friend bool operator==(NestedIdPair Left, NestedIdPair Right) noexcept
        {
            return Left.first == Right.first && Left.second == Right.second;
        }

        first_type first;

        second_type second;
    };

    std::optional<ModelPart*> mpInputModelPart;

    std::optional<ModelPart*> mpOutputModelPart;

    std::optional<const Variable<double>*> mpVariable;

    std::optional<IntervalUtility> mInterval;

    /// Map associating each constrained variable pair with a sub model part
    /// in the output model part.
    std::unordered_map<
        SymmetricIdPair<Variable<double>::KeyType>,
        ModelPart*,
        SymmetricIdPair<Variable<double>::KeyType>::hash
    > mSubModelPartMap;

    /// Map associating an @ref MasterSlaveConstraint "MPC" and an element
    /// with each pair of nodes and variables.
    std::unordered_map<
        NestedIdPair<Node::IndexType,Variable<double>::KeyType>,
        std::pair<
            Element::IndexType,
            MasterSlaveConstraint::IndexType
        >,
        NestedIdPair<Node::IndexType,Variable<double>::KeyType>::hash
    > mConstraintMap;

    /// @brief Find the largest @ref Element ID in a @ref ModelPart.
    static Element::IndexType GetLargestElementId(const ModelPart& rModelPart)
    {
        // IDs are 1-based
        Element::IndexType output = 1;

        // Elements are stored in an ordered container, sorted by their IDs,
        // so the last element in the container should have the largest ID.
        if (!rModelPart.Elements().empty()) {
            output = rModelPart.Elements().back().Id();
        }

        // Max reduce across MPI ranks
        const DataCommunicator& r_mpi = rModelPart.GetCommunicator().GetDataCommunicator();
        return static_cast<Element::IndexType>(r_mpi.MaxAll(static_cast<long unsigned int>(output))); // <== explicit casts for MSVC
    }


    static void EnsureNode(ModelPart& rOutputModelPart, const Node& rInputNode)
    {
        const auto it_node = rOutputModelPart.Nodes().find(rInputNode.Id());
        if (it_node == rOutputModelPart.Nodes().end()) {
            rOutputModelPart.CreateNewNode(rInputNode.Id(),
                                           rInputNode.X0(),
                                           rInputNode.Y0(),
                                           rInputNode.Z0());
        }
    }


    static Element& ConstructMPCElement(ModelPart& rOutputModelPart,
                                        std::pair<const Node*,const Node*> Nodes,
                                        Element::IndexType Id,
                                        const Properties::Pointer& rpProperties)
    {
        // The node pointers might not be in the output model part (usually they aren't)
        // so they must be constructed if they haven't been already.
        EnsureNode(rOutputModelPart, *Nodes.first);
        EnsureNode(rOutputModelPart, *Nodes.second);

        Element::Pointer p_element = rOutputModelPart.CreateNewElement(
            "Element2D2N",
            Id,
            std::vector<Node::IndexType> {Nodes.first->Id(), Nodes.second->Id()},
            rpProperties);
        return *p_element;
    }
}; // struct MultifreedomConstraintToElementProcess::Impl


MultifreedomConstraintToElementProcess::MultifreedomConstraintToElementProcess() noexcept
    : mpImpl(new Impl)
{
}


MultifreedomConstraintToElementProcess::MultifreedomConstraintToElementProcess(Model& rModel,
                                                                               Parameters Settings)
    : MultifreedomConstraintToElementProcess()
{
    KRATOS_TRY

    Settings.ValidateAndAssignDefaults(this->GetDefaultParameters());
    mpImpl->mpInputModelPart = &rModel.GetModelPart(Settings["input_model_part_name"].Get<std::string>());
    mpImpl->mInterval = IntervalUtility(Settings);

    const std::string output_model_part_name = Settings["output_model_part_name"].Get<std::string>();
    if (rModel.HasModelPart(output_model_part_name)) {
        mpImpl->mpOutputModelPart = &rModel.GetModelPart(output_model_part_name);
    } else {
        mpImpl->mpOutputModelPart = &rModel.CreateModelPart(output_model_part_name);

        // Link the input model part's process info. This is necessary to ensure that
        // the interval control works properly (TIME and STEP get updated). This is
        // essential if the user provides an output model part that's outside the tree
        // of the input model part.
        mpImpl->mpOutputModelPart.value()->SetProcessInfo(mpImpl->mpInputModelPart.value()->pGetProcessInfo());

        // Make sure the output model part has properties; it will be used for constructing elements.
        if (mpImpl->mpOutputModelPart.value()->GetRootModelPart().rProperties().empty()) {
            mpImpl->mpOutputModelPart.value()->GetRootModelPart().CreateNewProperties(1);
        }
    }

    const std::string variable_name = Settings["output_variable"].Get<std::string>();
    KRATOS_ERROR_IF_NOT(KratosComponents<Variable<double>>::Has(variable_name))
        << "output variable " << variable_name << " is not registered. "
        << "Check its spelling and import the application it's defined in.";
    mpImpl->mpVariable = &KratosComponents<Variable<double>>::Get(variable_name);

    KRATOS_CATCH("")
}


/// Required by PIMPL
MultifreedomConstraintToElementProcess::~MultifreedomConstraintToElementProcess() = default;


// Make sure every MPC is associated with an element
// AND every element represents at least one MPC.
// Each MPC is represented by a scalar variable in an
// element.
void MultifreedomConstraintToElementProcess::Execute()
{
    KRATOS_TRY

    ModelPart& r_input_model_part = *mpImpl->mpInputModelPart.value();
    const auto& r_constraints = r_input_model_part.MasterSlaveConstraints();
    ModelPart& r_output_model_part = *mpImpl->mpOutputModelPart.value();
    const Variable<double>& r_variable = *mpImpl->mpVariable.value();

    // @todo make sure that issuing new element IDs is MPI-safe and reasonably efficient (@matekelemen)
    Element::IndexType next_element_id = Impl::GetLargestElementId(r_output_model_part.GetRootModelPart()) + 1;
    KRATOS_ERROR_IF(r_output_model_part.GetRootModelPart().rProperties().empty());
    Properties::Pointer p_element_properties = *r_input_model_part.GetRootModelPart().rProperties().ptr_begin();

    // First, remove elements that are no longer related to any MPC.
    for (auto it_entry=mpImpl->mConstraintMap.begin(); it_entry!=mpImpl->mConstraintMap.end(); ++it_entry) {
        const Element::IndexType element_id = it_entry->second.first;
        const MasterSlaveConstraint::IndexType constraint_id = it_entry->second.second;
        if (r_constraints.find(constraint_id) == r_constraints.end()) {
            r_output_model_part.RemoveElement(element_id);
            mpImpl->mConstraintMap.erase(it_entry); // <== don't worry, iterators don't get invalidated in std::unordered_map
        }
    } // for r_entry in mConstraintMap

    // Now create new elements for each MPC that doesn't already have one.
    for (const MasterSlaveConstraint& r_constraint : r_input_model_part.MasterSlaveConstraints()) {
        MasterSlaveConstraint::MatrixType scales;
        MasterSlaveConstraint::VectorType offsets;
        r_constraint.GetLocalSystem(scales, offsets, r_input_model_part.GetProcessInfo());

        const auto& r_slaves = r_constraint.GetSlaveDofsVector();
        const auto& r_masters = r_constraint.GetMasterDofsVector();
        for (std::size_t i_slave=0ul; i_slave<r_slaves.size(); ++i_slave) {
            for (std::size_t i_master=0ul; i_master<r_masters.size(); ++i_master) {
                const double scale = scales(i_slave, i_master);
                if (scale) {

                    const Dof<double>& r_slave = *r_constraint.GetSlaveDofsVector()[i_slave];
                    const Dof<double>& r_master = *r_constraint.GetMasterDofsVector()[i_master];
                    Impl::SymmetricIdPair<Node::IndexType> node_id_pair {r_slave.GetId(), r_master.GetId()};
                    Impl::SymmetricIdPair<Variable<double>::KeyType> variable_id_pair {r_slave.GetVariable().Key(),
                                                                                       r_master.GetVariable().Key()};
                    Impl::NestedIdPair<Node::IndexType,Variable<double>::KeyType> mpc_id {node_id_pair, variable_id_pair};

                    // Get or create the element related to this MPC
                    auto it_mpc = mpImpl->mConstraintMap.find(mpc_id);
                    if (it_mpc == mpImpl->mConstraintMap.end()) {
                        auto it_sub_model_part = mpImpl->mSubModelPartMap.find(variable_id_pair);

                        // Get or create the sub model part related to the pair of constrained variables
                        if (it_sub_model_part == mpImpl->mSubModelPartMap.end()) {
                            const std::string slave_variable_name = r_slave.GetVariable().Name();
                            const std::string master_variable_name = r_master.GetVariable().Name();
                            const std::string sub_model_part_name = std::min(slave_variable_name, master_variable_name)
                                                                    + "_" +
                                                                    std::max(slave_variable_name, master_variable_name);
                            ModelPart& r_sub_model_part = r_output_model_part.CreateSubModelPart(sub_model_part_name);
                            it_sub_model_part = mpImpl->mSubModelPartMap.emplace(variable_id_pair, &r_sub_model_part).first;
                        }

                        const auto it_slave_node = r_input_model_part.Nodes().find(r_slave.Id());
                        const auto it_master_node = r_input_model_part.Nodes().find(r_master.Id());
                        KRATOS_ERROR_IF(it_slave_node == r_input_model_part.Nodes().end());
                        KRATOS_ERROR_IF(it_master_node == r_input_model_part.Nodes().end());
                        ModelPart& r_output_sub_model_part = *it_sub_model_part->second;
                        Element& r_element = Impl::ConstructMPCElement(r_output_sub_model_part,
                                                                       {&*it_slave_node, &*it_master_node},
                                                                       next_element_id++,
                                                                       p_element_properties);
                        it_mpc = mpImpl->mConstraintMap.emplace(mpc_id, std::make_pair(r_element.Id(), r_constraint.Id())).first;
                    } // if constraint is new

                    // Set the scale of the constraint
                    const Element::IndexType element_id = it_mpc->second.first;
                    const auto it_element = r_output_model_part.Elements().find(element_id);
                    KRATOS_ERROR_IF(it_element == r_output_model_part.Elements().end());
                    Element& r_element = *it_element;
                    r_element.SetValue(r_variable, scale);
                } // if scale
            } // for p_slave in constraint.Slaves
        } // for p_master in constraint.Masters
    } // for r_constraint in r_input_model_part.MasterSlaveConstraints

    KRATOS_CATCH("")
}


void MultifreedomConstraintToElementProcess::ExecuteInitialize()
{
    KRATOS_TRY
    this->Execute();
    KRATOS_CATCH("")
}


void MultifreedomConstraintToElementProcess::ExecuteInitializeSolutionStep()
{
    KRATOS_TRY
    const double current_time = mpImpl->mpInputModelPart.value()->GetProcessInfo()[TIME];
    if (mpImpl->mInterval.value().IsInInterval(current_time)) {
        this->Execute();
    }
    KRATOS_CATCH("")
}


const Parameters MultifreedomConstraintToElementProcess::GetDefaultParameters() const
{
    return Parameters(R"({
        "input_model_part_name" : "",
        "output_model_part_name" : "",
        "output_variable" : "CONSTRAINT_SCALE_FACTOR",
        "interval" : [0.0, "End"]
    })");
}


} // namespace Kratos::UtilityApp
