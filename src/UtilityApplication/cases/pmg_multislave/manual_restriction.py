import scipy
import numpy

x = [1.0]
y = []

for sample in x:
    # Read the original operator
    original_restriction_operator = scipy.io.mmread("quadratic_mesh_hierarchical_solver/restriction_operator.mm")

    ## Manipulate its entries
    restriction_operator = original_restriction_operator.todense()

    for i in (0, 1):
        restriction_operator[4 + i, 20 + i] = 0.0
        restriction_operator[4 + i, 24 + i] = 0.0

    # Read the fine system matrix and restrict it
    fine_system_matrix = scipy.io.mmread("quadratic_mesh_hierarchical_solver/system_matrix.mm")
    coarse_system_matrix = restriction_operator.dot(fine_system_matrix.todense()).dot(restriction_operator.transpose())

    # Write the modified operator and the coarsened system
    scipy.io.mmwrite("quadratic_mesh_hierarchical_solver/restriction_operator",
                    scipy.sparse.csr_matrix(restriction_operator),
                    precision = 13,
                    symmetry = "general")
    scipy.io.mmwrite("quadratic_mesh_hierarchical_solver/coarse_system_matrix",
                    scipy.sparse.csr_matrix(coarse_system_matrix),
                    precision = 13,
                    symmetry = "general")

    # Load reference coarse system matrix
    reference_coarse_system_matrix = scipy.io.mmread("linear_mesh_standalone_amgcl_raw_solver/system_matrix_1.mm").todense()
    constrained_dofs = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 16, 17]
    reference_coarse_system_matrix = numpy.delete(reference_coarse_system_matrix, constrained_dofs, axis = 0)
    reference_coarse_system_matrix = numpy.delete(reference_coarse_system_matrix, constrained_dofs, axis = 1)

    diff_matrix = reference_coarse_system_matrix - coarse_system_matrix
    for i_row in range(diff_matrix.shape[0]):
        for i_column in range(diff_matrix.shape[1]):
            if abs(diff_matrix[i_row, i_column]) < 1e-12:
                diff_matrix[i_row, i_column] = 0.0
    scipy.io.mmwrite("quadratic_mesh_hierarchical_solver/diff",
                    scipy.sparse.csr_matrix(diff_matrix),
                    precision = 13,
                    symmetry = "general")

    import os
    os.system("mtx2img quadratic_mesh_hierarchical_solver/diff.mtx quadratic_mesh_hierarchical_solver/diff.png -a sum -c kindlmann")

    print(f"diff: {numpy.linalg.norm(diff_matrix)}")
    y.append(numpy.linalg.norm(diff_matrix))

#from matplotlib import pyplot
#pyplot.plot(x, y, "+-")
#pyplot.show()
