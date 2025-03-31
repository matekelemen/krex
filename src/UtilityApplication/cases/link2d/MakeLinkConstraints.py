# --- Kratos Imports ---
import KratosMultiphysics
import KratosMultiphysics.StructuralMechanicsApplication

# --- STD Imports ---
import typing


class MakeLinkConstraints(KratosMultiphysics.Process):
    def __init__(self,
                 model: KratosMultiphysics.Model,
                 parameters: KratosMultiphysics.Parameters):
        super().__init__()
        parameters.ValidateAndAssignDefaults(self.GetDefaultParameters())

        # Normally you should do some input validation but I'll skip it
        # here for the sake of brevity.
        self.__model_part: KratosMultiphysics.ModelPart = model.GetModelPart(parameters["model_part_name"].GetString())
        self.__node_pairs: "list[tuple[int,int]]" = []
        self.__is_mesh_moved: bool = parameters["move_mesh_flag"].GetBool()

        pair: KratosMultiphysics.Parameters
        for pair in parameters["node_pairs"].values():
            self.__node_pairs.append((pair[0].GetInt(), pair[1].GetInt()))

    @classmethod
    def GetDefaultParameters(cls: "typing.Type[MakeLinkConstraints]") -> KratosMultiphysics.Parameters:
        return KratosMultiphysics.Parameters(R"""{
            "model_part_name" : "",
            "node_pairs" : [],
            "move_mesh_flag" : false
        }""")

    def ExecuteBeforeSolutionLoop(self) -> None:
        id: int = self.__GetLastConstraintId() + 1
        for id_left, id_right in self.__node_pairs:
            left = self.__model_part.GetNode(id_left)
            right = self.__model_part.GetNode(id_right)
            self.__model_part.AddMasterSlaveConstraint(KratosMultiphysics.StructuralMechanicsApplication.LinkConstraint(
                id,
                left,
                right,
                2,
                self.__is_mesh_moved
            ))
            id += 1

    def __GetLastConstraintId(self) -> int:
        constraint_count: int = len(self.__model_part.MasterSlaveConstraints)
        if constraint_count:
            return self.__model_part.MasterSlaveConstraints[constraint_count - 1]
        else:
            return 1


def Factory(parameters: KratosMultiphysics.Parameters,
            model: KratosMultiphysics.Model) -> MakeLinkConstraints:
    return MakeLinkConstraints(model, parameters["Parameters"])