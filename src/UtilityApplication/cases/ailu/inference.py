# --- External Imports ---
import scipy
import scipy.sparse
import numpy

# --- Kratos Imports ---
import KratosMultiphysics
import KratosMultiphysics.LinearSolversApplication
from KratosMultiphysics.python_solver import PythonSolver
from KratosMultiphysics.StructuralMechanicsApplication.structural_mechanics_analysis import StructuralMechanicsAnalysis as Analysis
from KratosMultiphysics.StructuralMechanicsApplication.structural_mechanics_solver import MechanicalSolver

# --- STD Imports ---
import pathlib
import os
import argparse


class PreconditionedLinearSolverFactory:

    def __init__(self, solver: MechanicalSolver) -> None:
        self.__original_factory = solver._CreateLinearSolver
        self.__preconditioner: KratosMultiphysics.Preconditioner = KratosMultiphysics.LinearSolversApplication.SubstitutionPreconditioner(
            self.__ReadCSRMatrix(pathlib.Path("lower_triangle.mm")),
            self.__ReadCSRMatrix(pathlib.Path("upper_triangle.mm")))


    def __call__(self) -> KratosMultiphysics.LinearSolver:
        """Replace the preconditioner of the linear solver constructed by PythonSolver."""
        linear_solver: KratosMultiphysics.IterativeSolver = self.__original_factory()
        linear_solver.Preconditioner = self.__preconditioner
        return linear_solver


    @staticmethod
    def __ReadCSRMatrix(matrix_market_path: pathlib.Path) -> KratosMultiphysics.CompressedMatrix:
        scipy_csr: scipy.sparse.csr_matrix = scipy.sparse.csr_matrix(scipy.io.mmread(matrix_market_path))
        row_extents: numpy.ndarray = scipy_csr.indptr
        column_indices: numpy.ndarray = scipy_csr.indices
        entries: numpy.ndarray = scipy_csr.data
        output: KratosMultiphysics.CompressedMatrix = KratosMultiphysics.CompressedMatrix(
            scipy_csr.shape[0],
            scipy_csr.shape[1],
            scipy_csr.nnz)
        output.index1_data().assign(row_extents)
        output.index2_data().assign(column_indices)
        output.value_data().assign(entries)
        return output

        # Of course you can read the matrix market file directly
        # through Kratos (but I wanted to show how to manipulate a matrix in Kratos).
        #output: KratosMultiphysics.CompressedMatrix = KratosMultiphysics.CompressedMatrix()
        #KratosMultiphysics.ReadMatrixMarketMatrix(str(matrix_market_path), output)
        #return output


# We need to hook into the linear solver construction,
# and provide
class CustomAnalysis(Analysis):

    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)


    def _GetSolver(self) -> MechanicalSolver:
        # Fetch the solver from the base class and overwrite its
        # function that constructs linear solvers.
        solver: MechanicalSolver = super()._GetSolver()
        solver._CreateLinearSolver = PreconditionedLinearSolverFactory(solver)
        return solver


if __name__ == "__main__":
    os.chdir(pathlib.Path(__file__).absolute().parent)

    parser: argparse.ArgumentParser = argparse.ArgumentParser(pathlib.Path(__file__).stem)
    parser.add_argument(
        "-p",
        "--preconditioner",
        dest = "preconditioner",
        type = str,
        default = "none",
        choices = ["none", "diagonal", "ilu0", "ilu", "substitution"])
    arguments: argparse.Namespace = parser.parse_args()

    # Load the analysis configuration.
    parameters: KratosMultiphysics.Parameters
    with open("main.json", "r") as file:
        parameters = KratosMultiphysics.Parameters(file.read())

    # Run the analysis.
    model: KratosMultiphysics.Model = KratosMultiphysics.Model()
    analysis: Analysis
    if arguments.preconditioner == "substitution":
        analysis = CustomAnalysis(model, parameters)
    else:
        parameters["solver_settings"]["linear_solver_settings"].AddString("preconditioner_type", arguments.preconditioner)
        analysis = Analysis(model, parameters)

    analysis.Run()

    # Print the number of iterations.
    print(analysis._GetSolver()._GetLinearSolver())
