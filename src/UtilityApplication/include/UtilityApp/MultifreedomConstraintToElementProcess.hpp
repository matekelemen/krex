/// @author Máté Kelemen

#pragma once

// Core includes
#include "includes/define.h" // KRATOS_API, KRATOS_CLASS_POINTER_DEFINITION
#include "processes/process.h" // Process, Model, Parameters

// System includes
#include <memory> // unique_ptr


namespace Kratos::UtilityApp {


/** @brief Utility process that generates elements from @ref MasterSlaveConstraint "multifreedom constraints" for visualization purposes.
 *  @details Default parameters:
 *           @code
 *           {
 *              "input_model_part_name" : "",
 *              "output_model_part_name" : "",
 *              "output_variable" : "CONSTRAINT_SCALE_FACTOR",
 *              "interval" : [0.0, "End"]
 *           }
 *           @endcode
 *           Multifreedom constraints act on pairs of @ref Dof "DoFs", not on @ref Node "nodes", so elements are further
 *           partitioned into sub model parts, based on the pair of @ref Variable "variables" they constrain. For example,
 *           if @a output_model_part_name is @a root and a multifreedom constraint relates @a DISPLACEMENT_X to @a PRESSURE,
 *           then @ref Element2D2N "Element2D2Ns" will be constructed in the @a root.DISPLACEMENT_X_PRESSURE sub model part.
 *
 *  @param input_model_part_name full name of the model part to scan multifreedom constraints in.
 *  @param output_model_part_name full name of the model part to generate elements in. It is created if it does not exist yet.
 *  @param output_variable name of the element variable to write the scale of the constraints to.
 *  @param interval time interval in which this process is active.
 *
 *  @note This process requires exclusive access to its output @ref ModelPart, which it will manage while active.
 *        Elements are added and deleted to reflect active @ref MasterSlaveConstraint "multifreedom constraints".
 *  @todo This process would be better off generating @ref Geometry "geometries", but no output process writes them yet @matekelemen.
 */
class KRATOS_API(UTILITY_APPLICATION) MultifreedomConstraintToElementProcess final : public Process
{
public:
    KRATOS_CLASS_POINTER_DEFINITION(MultifreedomConstraintToElementProcess);

    MultifreedomConstraintToElementProcess() noexcept;

    MultifreedomConstraintToElementProcess(Model& rModel, Parameters Settings);

    ~MultifreedomConstraintToElementProcess() override;

    void Execute() override;

    void ExecuteInitialize() override;

    void ExecuteInitializeSolutionStep() override;

    const Parameters GetDefaultParameters() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> mpImpl;
}; // class MultifreedomConstraintToElementProcess


} // namespace Kratos::UtilityApp