import KratosMultiphysics
import KratosMultiphysics.UtilityApplication as UtilityApp

MultifreedomConstraintToElementProcess = UtilityApp.MultifreedomConstraintToElementProcess

def Factory(parameters: KratosMultiphysics.Parameters,
            model: KratosMultiphysics.Model) -> MultifreedomConstraintToElementProcess:
    return MultifreedomConstraintToElementProcess(model, parameters["Parameters"])
