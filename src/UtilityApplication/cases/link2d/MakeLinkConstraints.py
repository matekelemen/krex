# --- Kratos Imports ---
import KratosMultiphysics

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
        self.model_part: KratosMultiphysics.ModelPart = model.GetModelPart(parameters["model_part_name"].GetString())
        self.node_pairs: "list[tuple[int,int]]" = []

        pair: KratosMultiphysics.Parameters
        for pair in parameters["node_pairs"].values():
            self.node_pairs.append((pair[0].GetInt(), pair[1].GetInt()))

    @classmethod
    def GetDefaultParameters(cls: "typing.Type[MakeLinkConstraints]") -> KratosMultiphysics.Parameters:
        return KratosMultiphysics.Parameters(R"""{
            "model_part_name" : "",
            "node_pairs" : []
        }""")


    def ExecuteBeforeSolutionLoop(self) -> None:
        for id_left, id_right in self.node_pairs:
            left = self.model_part.GetNode(id_left)
            right = self.model_part.GetNode(id_right)
            self.model_part.AddMasterSlaveConstraint(KratosMultiphysics.LinkConstraint(
                1,
                left,
                right,
                2
            ))


def Factory(parameters: KratosMultiphysics.Parameters,
            model: KratosMultiphysics.Model) -> MakeLinkConstraints:
    return MakeLinkConstraints(model, parameters["Parameters"])