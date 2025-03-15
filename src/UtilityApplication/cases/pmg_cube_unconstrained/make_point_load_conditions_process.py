# --- Kratos Imports ---
import KratosMultiphysics


class MakePointLoadConditionProcess(KratosMultiphysics.Process):

    def __init__(self,
                 model: KratosMultiphysics.Model,
                 parameters: KratosMultiphysics.Parameters):
        super().__init__()
        parameters.ValidateAndAssignDefaults(self.GetDefaultParameters())
        self.__model_part: KratosMultiphysics.ModelPart = model.GetModelPart(parameters["model_part_name"].GetString())


    def Execute(self) -> None:
        condition_id = self.__GetHighestConstraintId() + 1
        properties = self.__model_part.GetProperties()[0]
        node: KratosMultiphysics.Node
        for node in self.__model_part.Nodes:
            self.__model_part.CreateNewCondition("PointLoadCondition3D1N", condition_id, [node.Id], properties)
            condition_id += 1


    def ExecuteInitialize(self) -> None:
        self.Execute()


    @classmethod
    def GetDefaultParameters(cls) -> KratosMultiphysics.Parameters:
        return KratosMultiphysics.Parameters("""{
            "model_part_name" : ""
        }""")


    def __GetHighestConstraintId(self) -> int:
        output: int = 1
        root_model_part = self.__model_part.GetRootModelPart()
        condition: KratosMultiphysics.Condition
        for condition in root_model_part.Conditions:
            output = max(output, condition.Id)
        return output



def Factory(parameters: KratosMultiphysics.Parameters,
            model: KratosMultiphysics.Model) -> MakePointLoadConditionProcess:
    return MakePointLoadConditionProcess(model, parameters["Parameters"])
