import KratosMultiphysics
import KratosMultiphysics.UtilityApplication

CanonicalizeElementsProcess = KratosMultiphysics.UtilityApplication.CanonicalizeElementsProcess

def Factory(parameters: KratosMultiphysics.Parameters,
            model: KratosMultiphysics.Model) -> CanonicalizeElementsProcess:
    return CanonicalizeElementsProcess(model, parameters["Parameters"])
