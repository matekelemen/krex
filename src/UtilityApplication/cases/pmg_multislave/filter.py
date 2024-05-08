# --- External Imports ---
import scipy

# --- STD Imports ---
import pathlib


script_directory = pathlib.Path(__file__).absolute().parent

for case, active_dofs in zip(("linear_mesh_standalone_amgcl_raw_solver", "consistent_linear_mesh_standalone_amgcl_raw_solver"),
                             ((2, 3, 4, 5, 18, 19), (0, 1, 2, 3, 4, 5))):
    input_matrix = scipy.io.mmread(script_directory / case / "system_matrix_1.mm").todense()
    input_matrix = input_matrix[active_dofs, :]
    input_matrix = input_matrix[:, active_dofs]
    input_matrix = scipy.sparse.csr_matrix(input_matrix)

    scipy.io.mmwrite(script_directory / case / "system_matrix_1_filtered",
                     input_matrix,
                     symmetry = "general")
