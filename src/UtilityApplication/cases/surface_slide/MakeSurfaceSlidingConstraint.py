# --- Kratos Imports ---
import KratosMultiphysics

# --- STD Imports ---
import typing


class MakeSurfaceSlidingConstraint(KratosMultiphysics.Process):
    def __init__(self,
                 model: KratosMultiphysics.Model,
                 parameters: KratosMultiphysics.Parameters):
        super().__init__()

        # Normally you should do some input validation but I'll skip it
        # here for the sake of brevity.
        self.model_part: KratosMultiphysics.ModelPart = model.GetModelPart(parameters["model_part_name"].GetString())
        self.node_id: int = parameters["node_id"].GetInt()
        self.normal: "KratosMultiphysics.Vector" = parameters["surface_normal"].GetVector()


        @classmethod
        def GetDefaultParameters(cls: "typing.Type[MakeSurfaceSlidingConstraint]") -> KratosMultiphysics.Parameters:
            return KratosMultiphysics.Parameters(R"""{
                "model_part_name" : "",
                "node_id" : 0,
                "surface_normal" : [0.0, 0.0, 1.0],
            }""")


    def ExecuteBeforeSolutionLoop(self) -> None:
        """ I'm putting the logic in ExecuteBeforeSolutionLoop because the Solver is already
            initialized at this point (check AnalysisStage::Initialize) so the Nodes should
            be constructed and could be found by ID. The constructed MasterSlaveConstraints will
            persist across nonlinear iterations and time steps.
        """
        # I'm assuming you don't have any other MasterSlaveConstraints, so I'll
        # set this constraint's ID to 1.
        constraint_id = 1

        # The linear equation n_x * u_x + n_y * u_y + n_z * u_z = 0 is equally valid
        # for any master/slave partitioning, but the Kratos interface demands we choose
        # a slave and masters explicitly.
        # => pick u_x as the slave and the rest (u_y, u_z) as masters.
        #    KEEP IN MIND THAT n_x SHOULD NOT VANISH but I'll ignore that case here.
        node: KratosMultiphysics.Node = self.model_part.GetNode(self.node_id)
        slave_dofs = [node.GetDof(KratosMultiphysics.DISPLACEMENT_X)]
        master_dofs = [node.GetDof(KratosMultiphysics.DISPLACEMENT_Y),
                       node.GetDof(KratosMultiphysics.DISPLACEMENT_Z)]
        relation_matrix = KratosMultiphysics.Matrix([[-self.normal[1] / self.normal[0], -self.normal[2] / self.normal[0]]])
        constraint_gap = KratosMultiphysics.Vector([0.0, 0.0])

        self.model_part.CreateNewMasterSlaveConstraint("LinearMasterSlaveConstraint",
                                                       constraint_id,
                                                       master_dofs,
                                                       slave_dofs,
                                                       relation_matrix,
                                                       constraint_gap)


def Factory(parameters: KratosMultiphysics.Parameters,
            model: KratosMultiphysics.Model) -> MakeSurfaceSlidingConstraint:
    return MakeSurfaceSlidingConstraint(model, parameters["Parameters"])