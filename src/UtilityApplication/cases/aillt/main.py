# --- External Imports ---
import scipy
import scipy.sparse
import numpy

# --- Kratos Imports ---
import KratosMultiphysics
from KratosMultiphysics.python_solver import PythonSolver
import KratosMultiphysics.LinearSolversApplication
from KratosMultiphysics.StructuralMechanicsApplication.structural_mechanics_analysis import StructuralMechanicsAnalysis as Analysis

# --- STD Imports ---
import pathlib
import os
import json


class AnalysisContext:
    def __init__(self, parameters: KratosMultiphysics.Parameters) -> None:
        self.__model: KratosMultiphysics.Model = KratosMultiphysics.Model()
        self.__analysis: Analysis = Analysis(self.__model, parameters)

    def __enter__(self) -> Analysis:
        self.__analysis.Initialize()
        self.__analysis._AdvanceTime()
        self.__analysis.InitializeSolutionStep()
        return self.__analysis

    def __exit__(self, *args) -> None:
        self.__analysis.FinalizeSolutionStep()
        self.__analysis.OutputSolutionStep()
        self.__analysis.Finalize()


class TrainingContext:
    def __init__(self) -> None:
        self.__iteration_counter: int = 0
        pass

    def __enter__(self) -> "TrainingContext":
        # Initialize whatever you need for training your network.
        return self

    def __exit__(self, *args) -> None:
        # Output your network here.
        pass

    def Process(self, analysis: Analysis) -> None:
        """Do whatever you need to do with the analysis."""
        self.__iteration_counter += 1

        # Fetch the system matrix.
        lhs: KratosMultiphysics.CompressedMatrix = self.__FetchLHS(analysis)

        # Fetch the individual arrays of the CSR system matrix.
        # This basically constructs views over the
        # - row extents
        # - column indices
        # - nonzeros.
        row_extents: numpy.ndarray = numpy.array(
            lhs.index1_data(),
            copy = False)
        print(f"row extents: {row_extents}")
        column_indices: numpy.ndarray = numpy.array(
            lhs.index2_data(),
            copy = False)
        print(f"column indices: {column_indices}")
        entries: numpy.ndarray = numpy.array(
            lhs.value_data(),
            copy = False)
        print(f"nonzeros: {entries}")

    def __bool__(self) -> bool:
        """Return True while you need more iterations."""
        return 1 < self.__iteration_counter

    @staticmethod
    def __FetchLHS(analysis: Analysis) -> KratosMultiphysics.CompressedMatrix:
        solver: PythonSolver = analysis._GetSolver()
        strategy: KratosMultiphysics.ImplicitSolvingStrategy = solver._mechanical_solution_strategy
        lhs: KratosMultiphysics.CompressedMatrix = strategy.GetSystemMatrix()
        return lhs


if __name__ == "__main__":
    os.chdir(pathlib.Path(__file__).absolute().parent)

    # Load the analysis configuration.
    with open("main.json", "r") as file:
        parameters_map: dict = json.loads(file.read())

    # Manipulate the configuration.
    # We don't actually need to solve the system,
    # just assemble the LHS matrix and yoink it out of Kratos,
    # so the linear solver can be replaced with a dummy.
    parameters_map["solver_settings"]["linear_solver_settings"] = {
        "solver_type"   : "cg",
        "max_iteration" : 0,
        "tolerance"     : 1.0}

    # Run the analysis.
    analysis: Analysis
    with AnalysisContext(KratosMultiphysics.Parameters(json.dumps(parameters_map))) as analysis:
        with TrainingContext() as training_context:
            analysis._GetSolver().SolveSolutionStep()
            training_context.Process(analysis)
