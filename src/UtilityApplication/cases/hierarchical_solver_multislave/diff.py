# --- External Imports ---
import scipy

# --- STD Imports ---
import pathlib


script_directory = pathlib.Path(__file__).absolute().parent

for linear_case, quadratic_case in zip(("linear_mesh_standalone_amgcl_raw_solver", "consistent_linear_mesh_standalone_amgcl_raw_solver"),
                                       ("quadratic_mesh_hierarchical_solver", "consistent_quadratic_mesh_hierarchical_solver")):
    left = scipy.io.mmread(script_directory / linear_case / "system_matrix_1_filtered.mtx")
    right = scipy.io.mmread(script_directory / quadratic_case / "coarse_system_matrix.mm")
    result = left - right
    scipy.io.mmwrite(script_directory / quadratic_case / "diff",
                    result,
                    symmetry = "general")
