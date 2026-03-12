# --- Kratos Imports ---
import KratosMultiphysics
from KratosMultiphysics import UtilityApplication


def Factory(
        parameters: KratosMultiphysics.Parameters,
        model: KratosMultiphysics.Model) -> KratosMultiphysics.Process:
    return UtilityApplication.FindElementsByCrossSectionProcess(
        model,
        parameters["Parameters"])
